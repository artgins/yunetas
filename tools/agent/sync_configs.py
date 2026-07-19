#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
sync_configs.py — compare the yuno configs in the CURRENT DIRECTORY against what
the local yuneta_agent already has installed, and (optionally) push the
differences.

This is the config-side sibling of ``sync_binaries.py``. The two differ in how
they DISCOVER work, because configs are not centralized the way binaries are:

  * ``sync_binaries.py`` drives from ``$YUNETAS_BASE/outputs/yunos/`` — every
    built binary lives in one well-known directory.
  * Configs live scattered under each yuno's ``batches/<host>/`` directory, so
    this script drives from the CURRENT DIRECTORY. You ``cd`` into the batches
    directory holding the ``*.json`` configs you want to sync, then run it.

Identity / matching
-------------------
A config in the agent is keyed by ``(id, version)``:

  * ``id``      is supplied as a command parameter; by convention it equals the
                config FILENAME without the ``.json`` extension
                (``auth_bff.1801.json`` -> id ``auth_bff.1801``, mirroring the
                live agent ids ``scheduler_wz.5004``, ``gate_auraair.4502`` …).
  * ``version`` is NOT a parameter: the agent reads it from the ``__version__``
                field INSIDE the file content. ``__description__`` is read the
                same way. (See ``cmd_create_config`` / ``cmd_update_config`` in
                ``yunos/c/yuno_agent/src/c_agent.c``.)

So this script, for every ``*.json`` in the directory (skipping ``_*.json``
batch/deploy helpers):

  1. derives the config id from the filename,
  2. reads ``__version__`` from the file (a file without it is not a deployable
     config under the current agent contract -> skipped),
  3. looks the id up in the agent via ``ycommand -c '*list-configs'`` (the
     leading ``*`` makes ycommand emit raw JSON instead of the table) for the
     PRIMARY version, and reads every installed version from
     ``ycommand -c '*list-configs-instances'``,
  4. classifies it.

Classification
--------------
  * NEW        — id not in the agent                       -> create-config
  * BUMP       — local version not installed, > primary    -> create-config
  * INSTALLED  — local version already an installed record  -> skipped (pending
                 but not the primary (pending promote)         promote)
  * UPDATE     — same version, content changed             -> update-config
  * UP-TO-DATE — same version, identical content           -> skipped
  * DOWNGRADE  — local version  <  agent version           -> reported only, NOT pushed
  * agent-only — agent has a config absent here            -> skipped (informational)

A DOWNGRADE is never offered for install: seeding a stale version with
create-config would break the version logic, so it is reported and left alone.

Like ``*list-binaries``, ``*list-configs`` reports only the primary (id,version);
if a freshly built config's version is already created as a non-primary record,
create-config would fail "already exists". The instance list from
``*list-configs-instances`` is the authoritative "is this version already
installed?" check that guards the BUMP path (it carries no content, so a content
change to an already-installed non-primary version must be pushed manually with
update-config).

The new-version install verb is ``create-config`` (it refuses to overwrite an
existing ``(id, version)``); ``update-config`` overwrites the content of an
existing same-version record. ``create-config`` also answers to the alias
``install-config``, by analogy with ``install-binary``.

Content upload uses the same ``content64=$$(<path>)`` macro as binaries; ``$$()``
base64-encodes the file. We pass the file's absolute path so it resolves
regardless of ycommand's working directory.

Restart handling
----------------
Installing a config does NOT require stopping the yuno. This is the key
difference from a binary: ``update-binary`` fails with text-file-busy while the
yuno is running from that slot, so ``sync_binaries.py`` MUST kill first and then
must bring it back. A config push has no such constraint — it always succeeds on
a running yuno; it simply does not take effect until that yuno next (re)starts.

So by default this script only pushes, then prints the affected yuno ids (from
the agent record's ``yunos`` field; a config id is ``<role>.<yuno_id>``, and the
field lists the using yuno instance ids) as a ``kill-yuno`` + ``run-yuno``
reminder. Restarting to apply the change is a separate, optional step.

Pass ``--restart`` to also bounce the using yunos right away, scoped by yuno
``id`` (never node-wide), preserving each one's prior run/play state:

    kill-yuno id=<yuno_id>   (only if running) -> poll *list-yunos until it exits
    run-yuno id=<yuno_id> play=0   (it was running)
    play-yuno id=<yuno_id>         (only if it had been playing)

A yuno that is not running is left stopped (it reads the new config on its next
start). NEW configs have no agent record yet (typically a yuno not created here)
and are never auto-restarted — their ids are printed as a reminder.

The affected yunos are restarted in ascending ``start_priority`` order (read
from the agent's ``*list-yunos`` record), so infrastructure comes back before
its dependents instead of in alphabetical id order.
"""

import argparse
import json
import os
import re
import shutil
import signal
import stat
import subprocess
import tempfile
import sys
import time
import urllib.error
import urllib.parse
import urllib.request

# ----------------------------------------------------------------------------
#   ANSI colours (only when stdout is a tty)
# ----------------------------------------------------------------------------
_TTY = sys.stdout.isatty()


def c(code, s):
    if not _TTY:
        return s
    return "\033[%sm%s\033[0m" % (code, s)


def bold(s):
    return c("1", s)


def green(s):
    return c("32", s)


def yellow(s):
    return c("33", s)


def red(s):
    return c("31", s)


def cyan(s):
    return c("36", s)


def dim(s):
    return c("90", s)


# ----------------------------------------------------------------------------
#   JSON helpers
# ----------------------------------------------------------------------------
_ANSI_RE = re.compile(r"\033\[[0-9;]*m")


def parse_leading_json(text):
    """
    ycommand wraps the JSON payload with a leading blank line and a trailing
    coloured footer (the command name). Strip ANSI, locate the first '[' or '{'
    and decode just that value, ignoring whatever trails it.
    """
    clean = _ANSI_RE.sub("", text or "")
    start = None
    for i, ch in enumerate(clean):
        if ch in "[{":
            start = i
            break
    if start is None:
        raise json.JSONDecodeError("no JSON value found", clean, 0)
    obj, _ = json.JSONDecoder().raw_decode(clean[start:])
    return obj


def strip_jsonc(text):
    """
    Remove ``//`` line comments and ``/* */`` block comments that live OUTSIDE
    of strings. Yuneta loads its configs with a non-strict parser
    (``config_gbuffer2json``) that tolerates comments, so a plain ``json.loads``
    would choke on otherwise-valid config files.
    """
    out = []
    i, n = 0, len(text)
    in_str = False
    esc = False
    while i < n:
        ch = text[i]
        if in_str:
            out.append(ch)
            if esc:
                esc = False
            elif ch == "\\":
                esc = True
            elif ch == '"':
                in_str = False
            i += 1
            continue
        if ch == '"':
            in_str = True
            out.append(ch)
            i += 1
            continue
        if ch == "/" and i + 1 < n and text[i + 1] == "/":
            while i < n and text[i] != "\n":
                i += 1
            continue
        if ch == "/" and i + 1 < n and text[i + 1] == "*":
            i += 2
            while i + 1 < n and not (text[i] == "*" and text[i + 1] == "/"):
                i += 1
            i += 2
            continue
        out.append(ch)
        i += 1
    return "".join(out)


def load_jsonc(path):
    """Load a (possibly comment-bearing) JSON config file into a dict."""
    with open(path, "r", encoding="utf-8") as f:
        text = f.read()
    try:
        return json.loads(text)
    except json.JSONDecodeError:
        return json.loads(strip_jsonc(text))


def version_tuple(v):
    """
    Turn '7.4.5' or '2' into a comparable tuple of ints. Non-numeric chunks
    fall back to 0 so a malformed version never crashes the compare.
    """
    parts = []
    for chunk in str(v).split("."):
        m = re.match(r"\d+", chunk)
        parts.append(int(m.group(0)) if m else 0)
    return tuple(parts)


def cmp_versions(a, b):
    ta, tb = version_tuple(a), version_tuple(b)
    if ta < tb:
        return -1
    if ta > tb:
        return 1
    return 0


# ----------------------------------------------------------------------------
#   OAuth2 — log in ONCE, reuse the token on every ycommand call
# ----------------------------------------------------------------------------
#   ycommand reads its credentials only from argp and does NOT cache a token
#   between one-shot `-c` invocations, so passing -x/-X would re-run the
#   password grant on every call. Instead we log in once here (Keycloak
#   password grant), then thread the resulting jwt through `-j` to every
#   invocation. This is what lets these scripts drive a REMOTE wss:// agent
#   with SSH disabled. Stdlib only — no external deps.
def _http_json(url, data=None, timeout=30):
    """GET (data=None) or form-urlencoded POST, returning parsed JSON."""
    body = None
    headers = {}
    if data is not None:
        body = urllib.parse.urlencode(data).encode("utf-8")
        headers["Content-Type"] = "application/x-www-form-urlencoded"
    req = urllib.request.Request(url, data=body, headers=headers)
    with urllib.request.urlopen(req, timeout=timeout) as resp:
        return json.loads(resp.read().decode("utf-8"))


def discover_token_endpoint(issuer, timeout=30):
    """Read token_endpoint from the issuer's OIDC discovery document."""
    doc = _http_json(issuer.rstrip("/") + "/.well-known/openid-configuration",
                     timeout=timeout)
    ep = doc.get("token_endpoint")
    if not ep:
        raise RuntimeError("OIDC discovery document has no token_endpoint")
    return ep


def obtain_jwt(args):
    """
    Return one jwt to reuse on every ycommand call, or None for a local/
    unauthenticated run:
      * --jwt given                                 -> used verbatim.
      * user-id + passw + (issuer | token-endpoint) -> password grant, once.
      * otherwise                                   -> None (local ws:// needs none).
    """
    if args.jwt:
        return args.jwt
    if not (args.user_id and args.user_passw and (args.issuer or args.token_endpoint)):
        return None
    token_endpoint = args.token_endpoint or discover_token_endpoint(args.issuer)
    form = {
        "grant_type": "password",
        "client_id": args.client_id or "",
        "username": args.user_id,
        "password": args.user_passw,
    }
    if args.client_secret:
        form["client_secret"] = args.client_secret
    print(dim("authenticating once at %s ..." % token_endpoint))
    try:
        tok = _http_json(token_endpoint, data=form)
    except urllib.error.HTTPError as e:
        detail = ""
        try:
            detail = e.read().decode("utf-8", "replace")
        except Exception:
            pass
        print(red("ERROR: OAuth2 login failed (HTTP %s): %s" % (e.code, detail or e.reason)))
        sys.exit(2)
    except Exception as e:
        print(red("ERROR: OAuth2 login failed: %s" % e))
        sys.exit(2)
    jwt = tok.get("access_token")
    if not jwt:
        print(red("ERROR: token endpoint returned no access_token."))
        sys.exit(2)
    return jwt


def ycmd_base(ycommand, url, jwt):
    """Base ycommand argv: binary + optional -u url + optional -j jwt."""
    cmd = [ycommand]
    if url:
        cmd += ["-u", url]
    if jwt:
        cmd += ["-j", jwt]
    return cmd


# ----------------------------------------------------------------------------
#   Discovery
# ----------------------------------------------------------------------------
#
#   Secret overlays
#
SECRET_SENTINEL = "__SECRET__"
SECRET_WORKDIR_PREFIX = "yuneta-cfg-"


def sweep_stale_secret_workdirs():
    """
    Remove leftover merged-config directories from earlier runs.

    The normal path deletes its workdir in a ``finally``, but that does not run
    if the process is SIGKILLed (a timeout, an OOM kill, a hard ^C on some
    shells) — and what survives is a config with a real credential in it. The
    files are 0600 inside a 0700 directory, so only this user can read them,
    but "only readable by me, forever" is not where a password should live.

    Sweeping at startup bounds that to "until the next sync" instead. Only
    directories owned by this uid are touched.
    """
    tmp = tempfile.gettempdir()
    try:
        names = os.listdir(tmp)
    except OSError:
        return
    for name in names:
        if not name.startswith(SECRET_WORKDIR_PREFIX):
            continue
        path = os.path.join(tmp, name)
        try:
            st = os.lstat(path)
            if not stat.S_ISDIR(st.st_mode) or st.st_uid != os.getuid():
                continue
            shutil.rmtree(path, ignore_errors=True)
            print(dim("  (removed stale secret workdir %s)" % path))
        except OSError:
            continue


def deep_merge(base, overlay):
    """
    Recursively overlay `overlay` onto `base`, returning a new dict. Matches
    how a yuno's effective config is already assembled (fixed_config +
    variable_config + external JSON), so an overlay is one more layer of
    something the framework understands, not a new mechanism.
    """
    out = dict(base)
    for key, value in overlay.items():
        if isinstance(value, dict) and isinstance(out.get(key), dict):
            out[key] = deep_merge(out[key], value)
        else:
            out[key] = value
    return out


def find_sentinels(node, path=""):
    """
    Paths still holding the SECRET_SENTINEL after merging, so a config can be
    refused instead of shipped with a placeholder where a credential belongs.
    """
    found = []
    if isinstance(node, dict):
        for key, value in node.items():
            found += find_sentinels(value, "%s.%s" % (path, key) if path else key)
    elif isinstance(node, list):
        for i, value in enumerate(node):
            found += find_sentinels(value, "%s[%d]" % (path, i))
    elif node == SECRET_SENTINEL:
        found.append(path)
    return found


def leaf_values(node, path=""):
    """
    {path: value} for every leaf, built by the SAME walk as find_sentinels().

    Deliberately not a "resolve this dotted path" helper: yuneta configs use
    flat dotted key names ("Emailsender.password" as a single key inside
    "global"), so splitting a path on '.' and descending would look for a
    nested object that does not exist and report a perfectly good value as
    missing. Sharing the walk keeps the two in step whatever the key shape.
    """
    out = {}
    if isinstance(node, dict):
        for key, value in node.items():
            out.update(leaf_values(value, "%s.%s" % (path, key) if path else key))
    elif isinstance(node, list):
        for i, value in enumerate(node):
            out.update(leaf_values(value, "%s[%d]" % (path, i)))
    else:
        out[path] = node
    return out


def apply_secret_overlays(local, secrets_dir, workdir):
    """
    Merge ``<secrets_dir>/<id>.json`` over each config that needs it and
    repoint the config at the merged copy, written 0600 under `workdir`.

    Secrets stay out of the project repo: the committed config carries
    ``"field": "__SECRET__"`` to declare that a credential belongs there, and
    the value lives only on the deploy machine.

    FAILS CLOSED. A config whose sentinel survives the merge is not pushed:
    shipping an empty SMTP password means either an auth failure or, worse, an
    unauthenticated send attempt, and silently is the wrong way to find out.

    Rotation note: the overlay does NOT carry a version. Rotating a credential
    means bumping ``__version__`` in the committed config, which is the
    existing batches convention (every edit bumps the version and writes what
    changed in ``__description__``). That keeps the *fact* of the rotation
    auditable in git while the *value* never touches it.

    Returns the number of configs that had an overlay applied.
    """
    applied = 0
    problems = []

    for cid, lc in sorted(local.items()):
        overlay_path = os.path.join(secrets_dir, "%s.json" % cid)
        content = lc["content"]

        # Which paths the committed config declares as credentials. Captured
        # BEFORE the merge, because afterwards the sentinel is gone and an
        # overlay that supplied "" would be indistinguishable from a config
        # that never wanted a secret -- shipping the empty password this whole
        # mechanism exists to prevent.
        declared = find_sentinels(content)

        if os.path.isfile(overlay_path):
            try:
                overlay = load_jsonc(overlay_path)
            except (OSError, json.JSONDecodeError) as e:
                problems.append("%s: cannot parse overlay %s (%s)" % (cid, overlay_path, e))
                continue
            if not isinstance(overlay, dict):
                problems.append("%s: overlay %s is not a JSON object" % (cid, overlay_path))
                continue
            content = deep_merge(content, overlay)
            applied += 1

        missing = find_sentinels(content)
        if missing:
            problems.append(
                "%s: no value for %s\n      expected in %s"
                % (cid, ", ".join(missing), overlay_path)
            )
            continue

        merged_leaves = leaf_values(content)
        blank = [p for p in declared if not str(merged_leaves.get(p, "")).strip()]
        if blank:
            problems.append(
                "%s: overlay supplies an EMPTY value for %s\n      in %s"
                % (cid, ", ".join(blank), overlay_path)
            )
            continue

        if content is not lc["content"]:
            merged_path = os.path.join(workdir, "%s.json" % cid)
            # 0600 before any content lands in it.
            fd = os.open(merged_path, os.O_WRONLY | os.O_CREAT | os.O_TRUNC, 0o600)
            with os.fdopen(fd, "w") as f:
                json.dump(content, f, indent=4)
            lc["content"] = content
            lc["path"] = merged_path
            lc["version"] = str(content.get("__version__", lc["version"]))

    if problems:
        print(red("  ! secret overlay unresolved:"))
        for p in problems:
            print(red("    - %s" % p))
        print(yellow("    Create the overlay with ONLY the secret fields, e.g.:"))
        print(yellow('      {"global": {"smtp_password": "…"}}'))
        raise SystemExit(2)

    return applied


def local_configs(config_dir):
    """
    Return {id: {id, version, description, path, content}} for every config
    ``*.json`` in config_dir. Files starting with '_' (batch/deploy helpers such
    as ``_deploy_*.json`` / ``_update-configs.json``) are skipped, as are files
    without a ``__version__`` field (not deployable under the agent contract).

    The config id is the filename without the ``.json`` extension.
    """
    out = {}
    for name in sorted(os.listdir(config_dir)):
        if name.startswith("_") or not name.endswith(".json"):
            continue
        path = os.path.join(config_dir, name)
        if not os.path.isfile(path):
            continue
        try:
            content = load_jsonc(path)
        except (OSError, json.JSONDecodeError) as e:
            print(red("  ! %s: cannot parse as JSON (%s), skipped" % (name, e)))
            continue
        if not isinstance(content, dict):
            print(red("  ! %s: top-level JSON is not an object, skipped" % name))
            continue
        version = content.get("__version__", "")
        if version == "" or version is None:
            print(yellow("  ! %s: no __version__ field, skipped (not a deployable config)" % name))
            continue
        cid = name[:-len(".json")]
        out[cid] = {
            "id": cid,
            "version": str(version),
            "description": content.get("__description__", ""),
            "path": os.path.abspath(path),
            "content": content,
        }
    return out


def agent_configs(ycommand, url, jwt):
    """
    Return {id: {version, description, date, zcontent, yunos}} from the agent via
    'ycommand -c *list-configs' (the '*' forces raw JSON). One record per id
    (the current primary version).
    """
    cmd = ycmd_base(ycommand, url, jwt) + ["-c", "*list-configs"]
    try:
        res = subprocess.run(cmd, capture_output=True, text=True, timeout=30)
    except (OSError, subprocess.SubprocessError) as e:
        print(red("ERROR: cannot run ycommand: %s" % e))
        sys.exit(2)
    try:
        data = parse_leading_json(res.stdout)
    except json.JSONDecodeError:
        print(red("ERROR: '*list-configs' did not return JSON. Is the agent up?"))
        print(dim(res.stdout[:500]))
        print(dim(res.stderr[:500]))
        sys.exit(2)
    if not isinstance(data, list):
        print(red("ERROR: unexpected '*list-configs' payload (not a list)."))
        sys.exit(2)
    out = {}
    for rec in data:
        cid = rec.get("id")
        if not cid:
            continue
        out[cid] = {
            "version": str(rec.get("version", "?")),
            "description": rec.get("description", ""),
            "date": rec.get("date", "?"),
            "zcontent": rec.get("zcontent", None),
            "yunos": rec.get("yunos", []),
        }
    return out


def agent_config_instances(ycommand, url, jwt):
    """
    Return {id: set(version, ...)} for EVERY installed config record, via
    'ycommand -c *list-configs-instances' (the '*' forces raw JSON).

    '*list-configs' reports only the primary (id,version); a freshly built
    config whose version is already created as a non-primary record must not be
    offered to create-config (which refuses to overwrite an existing
    (id,version)). This set is the authoritative "is this version already
    installed?". Returns {} on any error so classify() degrades gracefully.
    """
    cmd = ycmd_base(ycommand, url, jwt) + ["-c", "*list-configs-instances"]
    try:
        res = subprocess.run(cmd, capture_output=True, text=True, timeout=30)
        data = parse_leading_json(res.stdout)
    except (OSError, subprocess.SubprocessError, json.JSONDecodeError):
        return {}
    if not isinstance(data, list):
        return {}
    out = {}
    for rec in data:
        cid = rec.get("id")
        ver = rec.get("version")
        if not cid or ver is None:
            continue
        out.setdefault(cid, set()).add(str(ver))
    return out


# ----------------------------------------------------------------------------
#   Classification
# ----------------------------------------------------------------------------
def classify(local, agent, instances):
    """
    Drive from the LOCAL configs found in the directory. For each one look it up
    in the agent and compare. Agent-only configs (present in the agent, absent
    here) are appended as informational 'orphan' rows.

    `instances` is {id: set(version)} from '*list-configs-instances'; it guards
    the NEW/BUMP paths so a version already created as a non-primary record
    (pending promote) is not offered to a doomed create-config.

    kind in {new, bump, installed, update, uptodate, downgrade, orphan}.
    """
    rows = []
    for cid, lc in sorted(local.items()):
        ac = agent.get(cid)
        already_installed = lc["version"] in instances.get(cid, set())
        if ac is None:
            # No primary at all. If the version is somehow already a record,
            # create-config would still fail; flag it as installed not new.
            rows.append({
                "id": cid,
                "kind": "installed" if already_installed else "new",
                "action": None if already_installed else "create-config",
                "local": lc, "agent": None,
            })
            continue
        cmp = cmp_versions(lc["version"], ac["version"])
        if cmp > 0:
            if already_installed:
                # Version already created as a non-primary record (primary is an
                # older one, e.g. pending promote). create-config would fail
                # "already exists"; nothing to push — it only needs promotion.
                rows.append({
                    "id": cid, "kind": "installed", "action": None,
                    "local": lc, "agent": ac,
                })
            else:
                rows.append({
                    "id": cid, "kind": "bump", "action": "create-config",
                    "local": lc, "agent": ac,
                })
        elif cmp < 0:
            # Local is OLDER than the agent. Installing it would break the
            # version logic (create-config would seed a stale version), so we
            # only report it — never offer to push it.
            rows.append({
                "id": cid, "kind": "downgrade", "action": None,
                "local": lc, "agent": ac,
            })
        else:
            # Same version. Only worth an update-config if content changed.
            if lc["content"] == ac["zcontent"]:
                rows.append({
                    "id": cid, "kind": "uptodate", "action": None,
                    "local": lc, "agent": ac,
                })
            else:
                rows.append({
                    "id": cid, "kind": "update", "action": "update-config",
                    "local": lc, "agent": ac,
                })

    for cid, ac in sorted(agent.items()):
        if cid not in local:
            rows.append({
                "id": cid, "kind": "orphan", "action": None,
                "local": None, "agent": ac,
            })
    return rows


KIND_LABEL = {
    "new":       (lambda s: green(s),  "NEW",        "create-config"),
    "bump":      (lambda s: green(s),  "BUMP",       "create-config"),
    "installed": (lambda s: cyan(s),   "INSTALLED",  "-"),
    "update":    (lambda s: yellow(s), "UPDATE",     "update-config"),
    "uptodate":  (lambda s: dim(s),    "up-to-date", "-"),
    "downgrade": (lambda s: red(s),    "DOWNGRADE",  "-"),
    "orphan":    (lambda s: dim(s),    "agent-only", "-"),
}


def print_table(rows, show_uptodate):
    print()
    print(bold("%-30s %-11s %-10s %-10s %-14s" % (
        "config id", "status", "local ver", "agent ver", "command")))
    print(dim("-" * 78))
    for r in rows:
        kind = r["kind"]
        if kind in ("uptodate", "orphan") and not show_uptodate:
            continue
        colour, label, _ = KIND_LABEL[kind]
        lv = r["local"]["version"] if r["local"] else "-"
        av = r["agent"]["version"] if r["agent"] else "-"
        action = r["action"] or "-"
        print("%-30s %s %-10s %-10s %-14s" % (
            r["id"],
            colour("%-11s" % label),
            lv, av, action,
        ))
    print()


# ----------------------------------------------------------------------------
#   Execution
# ----------------------------------------------------------------------------
def run_one(ycommand, url, jwt, action, cid, path, dry_run):
    cmd_str = "%s id='%s' content64=$$(%s)" % (action, cid, path)
    cmd = ycmd_base(ycommand, url, jwt) + ["-c", cmd_str]
    print(cyan(">> ycommand -c '%s'" % cmd_str))
    if dry_run:
        print(dim("   (dry-run, not executed)"))
        return True
    try:
        res = subprocess.run(cmd, capture_output=True, text=True, timeout=120)
    except (OSError, subprocess.SubprocessError) as e:
        print(red("   ERROR: %s" % e))
        return False
    out = (res.stdout or "").strip()
    err = (res.stderr or "").strip()
    if out:
        print(out)
    if err:
        print(dim(err))
    # A resumed sync re-runs create-config for a (id, version) the prior run
    # already stored; the agent answers "... already exists". That is idempotent,
    # not a failure: the config row is already there. Treat it as ok.
    already = "already exists" in out
    ok = res.returncode == 0 and ("ERROR" not in out or already)
    if already:
        print(yellow("   ALREADY PRESENT (idempotent)"))
    else:
        print(green("   OK") if ok else red("   FAILED"))
    return ok


def run_ycmd(ycommand, url, jwt, cmd_str, dry_run, timeout=120):
    """Run one `ycommand -c '<cmd_str>'`, echoing it. Returns (ok, stdout)."""
    cmd = ycmd_base(ycommand, url, jwt) + ["-c", cmd_str]
    print(cyan(">> ycommand -c '%s'" % cmd_str))
    if dry_run:
        print(dim("   (dry-run, not executed)"))
        return True, ""
    try:
        res = subprocess.run(cmd, capture_output=True, text=True, timeout=timeout)
    except (OSError, subprocess.SubprocessError) as e:
        print(red("   ERROR: %s" % e))
        return False, ""
    out = (res.stdout or "").strip()
    err = (res.stderr or "").strip()
    if out:
        print(out)
    if err:
        print(dim(err))
    # Idempotent "... already exists" (resumed sync) is not a failure.
    already = "already exists" in out
    ok = res.returncode == 0 and ("ERROR" not in out or already)
    if already:
        print(yellow("   ALREADY PRESENT (idempotent)"))
    else:
        print(green("   OK") if ok else red("   FAILED"))
    return ok, out


def yuno_states_by_id(ycommand, url, jwt):
    """
    Return {yuno_id: record} for every yuno the agent manages, via '*list-yunos'.
    Each record carries yuno_running / yuno_playing. {} on any error.
    """
    cmd = ycmd_base(ycommand, url, jwt) + ["-c", "*list-yunos"]
    try:
        res = subprocess.run(cmd, capture_output=True, text=True, timeout=30)
        data = parse_leading_json(res.stdout)
    except (OSError, subprocess.SubprocessError, json.JSONDecodeError):
        return {}
    if not isinstance(data, list):
        return {}
    out = {}
    for rec in data:
        if isinstance(rec, dict) and rec.get("id") is not None:
            out[str(rec["id"])] = rec
    return out


def wait_until_stopped(ycommand, url, jwt, yid, timeout_s=15.0, poll_s=0.3):
    """
    Poll '*list-yunos id=<yid>' until the yuno is no longer running, so a later
    run-yuno relaunches a fully-exited process. Returns True if it stopped.
    """
    deadline = time.monotonic() + timeout_s
    while time.monotonic() < deadline:
        cmd = ycmd_base(ycommand, url, jwt) + ["-c", "*list-yunos id=%s" % yid]
        try:
            res = subprocess.run(cmd, capture_output=True, text=True, timeout=30)
            data = parse_leading_json(res.stdout)
        except (OSError, subprocess.SubprocessError, json.JSONDecodeError):
            data = []
        records = data if isinstance(data, list) else []
        if not any(isinstance(r, dict) and r.get("yuno_running") for r in records):
            return True
        time.sleep(poll_s)
    return False


def restart_yuno(ycommand, url, jwt, yid, was_running, was_playing, dry_run):
    """
    Bounce one yuno by id so it re-reads its config, restoring prior run/play
    state. A yuno that was not running is left stopped — it reads the new config
    on its next start. kill-yuno is orderly (SIGQUIT), so the gbmem audit runs.
    """
    if not was_running:
        print(dim("   %s not running — left stopped (reads new config on next start)" % yid))
        return
    run_ycmd(ycommand, url, jwt, "kill-yuno id=%s" % yid, dry_run)
    if not dry_run and not wait_until_stopped(ycommand, url, jwt, yid):
        print(yellow("   ! %s still running after kill" % yid))
    run_ycmd(ycommand, url, jwt, "run-yuno id=%s play=0" % yid, dry_run)
    if was_playing:
        run_ycmd(ycommand, url, jwt, "play-yuno id=%s" % yid, dry_run)


def ask(prompt):
    try:
        return input(prompt).strip().lower()
    except EOFError:
        return "q"


# ----------------------------------------------------------------------------
#   Main
# ----------------------------------------------------------------------------
def main():
    ap = argparse.ArgumentParser(
        description="Compare the configs in a directory with the agent and push updates.")
    ap.add_argument("config_dir", nargs="?", default=".",
                    help="directory holding the *.json configs (default: current dir).")
    ap.add_argument("-u", "--url", default=None,
                    help="ycommand url (default: ws://127.0.0.1:1991).")
    ap.add_argument("--ycommand", default=None,
                    help="path to ycommand (default: search PATH).")
    ap.add_argument("-a", "--all", action="store_true",
                    help="apply every candidate without asking per-config.")
    ap.add_argument("-n", "--dry-run", action="store_true",
                    help="show what would run, execute nothing.")
    ap.add_argument("--show-uptodate", action="store_true",
                    help="also list configs already in sync and agent-only ones.")
    ap.add_argument("--secrets-dir", default=None,
                    help="directory of secret overlays (<config-id>.json), deep-merged "
                         "over each config before pushing. A committed config declares "
                         "a credential with the value \"__SECRET__\"; if the overlay "
                         "does not supply it, the push is REFUSED.")
    ap.add_argument("-r", "--restart", action="store_true",
                    help="after pushing a changed config, also restart the using "
                         "yunos to apply it (default: push only + print a "
                         "reminder). Installing a config never needs a kill; the "
                         "restart is optional.")
    auth = ap.add_argument_group(
        "OAuth2 (remote wss:// agent; logs in ONCE, reuses the token via -j)")
    auth.add_argument("-I", "--issuer", default=None,
                      help="OIDC issuer for discovery, e.g. "
                           "https://auth.artgins.com/realms/artgins")
    auth.add_argument("-T", "--token-endpoint", default=None,
                      help="explicit token endpoint (skips discovery).")
    auth.add_argument("-Z", "--client-id", default=None, help="OAuth2 client_id.")
    auth.add_argument("--client-secret", default=None,
                      help="OAuth2 client_secret (confidential client only).")
    auth.add_argument("-x", "--user-id", default=None, help="OAuth2 username.")
    auth.add_argument("-X", "--user-passw", default=None, help="OAuth2 password.")
    auth.add_argument("-j", "--jwt", default=None,
                      help="reuse this jwt directly (skip the login).")
    args = ap.parse_args()

    ycommand = args.ycommand or shutil.which("ycommand")
    if not ycommand:
        print(red("ERROR: ycommand not found in PATH (use --ycommand)."))
        sys.exit(2)

    config_dir = os.path.abspath(args.config_dir)
    if not os.path.isdir(config_dir):
        print(red("ERROR: config dir not found: %s" % config_dir))
        sys.exit(2)

    jwt = obtain_jwt(args)

    print(dim("config dir : %s" % config_dir))
    print(dim("ycommand   : %s%s%s" % (
        ycommand,
        ("  url=" + args.url) if args.url else "",
        "  oauth2=on" if jwt else "")))

    print(dim("\nreading agent configs (*list-configs)..."))
    agent = agent_configs(ycommand, args.url, jwt)
    print(dim("reading installed records (*list-configs-instances)..."))
    instances = agent_config_instances(ycommand, args.url, jwt)
    print(dim("reading local configs (*.json in dir)..."))
    local = local_configs(config_dir)

    if not local:
        print(yellow("\nNo deployable *.json configs found in %s." % config_dir))
        return

    #
    #   Secret overlays
    #
    #   The merged copies hold real credentials, so they live in a 0700
    #   directory for the length of the run and are removed afterwards --
    #   including on failure, which is exactly when a stray plaintext copy
    #   would be easiest to forget.
    #
    secrets_workdir = None
    if args.secrets_dir:
        if not os.path.isdir(args.secrets_dir):
            print(red("Error: --secrets-dir '%s' is not a directory." % args.secrets_dir))
            raise SystemExit(2)
        sweep_stale_secret_workdirs()
        secrets_workdir = tempfile.mkdtemp(prefix=SECRET_WORKDIR_PREFIX)
        os.chmod(secrets_workdir, 0o700)

        # finally: does not run on a signal. Catch the ones we can so an
        # interrupted sync still takes its plaintext with it.
        def _on_signal(signum, frame):
            shutil.rmtree(secrets_workdir, ignore_errors=True)
            raise SystemExit(128 + signum)

        for _sig in (signal.SIGINT, signal.SIGTERM, signal.SIGHUP):
            try:
                signal.signal(_sig, _on_signal)
            except (OSError, ValueError):
                pass

    try:
        if secrets_workdir:
            applied = apply_secret_overlays(local, args.secrets_dir, secrets_workdir)
            print(dim("secret overlays applied: %d (from %s)" % (applied, args.secrets_dir)))
        _sync_body(args, ycommand, jwt, local, agent, instances)
    finally:
        if secrets_workdir:
            shutil.rmtree(secrets_workdir, ignore_errors=True)


def _sync_body(args, ycommand, jwt, local, agent, instances):
    """
    The compare/confirm/push part, split out so the caller can guarantee the
    merged-secret workdir is destroyed however this returns.
    """

    rows = classify(local, agent, instances)
    print_table(rows, show_uptodate=args.show_uptodate or args.dry_run)

    installed = [r for r in rows if r["kind"] == "installed"]
    if installed:
        print(yellow(
            "%d config(s) already installed but NOT primary (pending "
            "promote)." % len(installed)))

    candidates = [r for r in rows if r["action"]]
    if not candidates:
        if not installed:
            print(green("Everything is up to date. Nothing to do."))
        return

    # Summary line.
    by_kind = {}
    for r in candidates:
        by_kind[r["kind"]] = by_kind.get(r["kind"], 0) + 1
    summary = ", ".join("%d %s" % (n, k) for k, n in sorted(by_kind.items()))
    print(bold("%d candidate(s): %s" % (len(candidates), summary)))

    downgrades = [r for r in rows if r["kind"] == "downgrade"]
    if downgrades:
        print(red("Note: %d config(s) are OLDER locally than in the agent — reported, NOT pushed:" %
                  len(downgrades)))
        for r in downgrades:
            print(red("   %s  local %s < agent %s" %
                      (r["id"], r["local"]["version"], r["agent"]["version"])))

    # Decide which to apply.
    chosen = []
    if args.all:
        chosen = candidates
    else:
        mode = ask("\nApply [a]ll, [s]elect one by one, or [q]uit? ")
        if mode in ("q", "quit", ""):
            print("Cancelled - no changes made.")
            return
        if mode in ("a", "all"):
            chosen = candidates
        else:
            for r in candidates:
                colour, label, _ = KIND_LABEL[r["kind"]]
                ans = ask("  %s  [%s -> %s]  apply? [y/N] " % (
                    r["id"], colour(label), r["action"]))
                if ans in ("y", "yes"):
                    chosen.append(r)

    if not chosen:
        print("Nothing selected.")
        return

    if not args.all and not args.dry_run:
        confirm = ask("\nAbout to run %d command(s) against the agent. Proceed? [y/N] "
                      % len(chosen))
        if confirm not in ("y", "yes"):
            print("Cancelled - no changes made.")
            return

    print()
    ok, fail = 0, 0
    pushed = []
    for r in chosen:
        if run_one(ycommand, args.url, jwt, r["action"], r["id"], r["local"]["path"], args.dry_run):
            ok += 1
            pushed.append(r)
        else:
            fail += 1
    print()
    print(bold("Done: %s, %s." % (green("%d ok" % ok), red("%d failed" % fail) if fail else dim("0 failed"))))

    # A yuno reads its config only at (re)start. Restart the yunos that use a
    # successfully pushed config so the change takes effect — scoped by yuno id,
    # preserving prior run/play state. NEW configs have no agent record yet
    # (typically a yuno not created here) -> reminder only.
    affected = set()
    new_only = set()
    for r in pushed:
        if r["agent"]:
            for y in r["agent"].get("yunos", []) or []:
                affected.add(str(y))
        else:
            new_only.add(r["id"])

    if affected and args.restart:
        print(dim("\nRestarting affected yuno(s) to apply the new config(s)..."))
        states = yuno_states_by_id(ycommand, args.url, jwt)

        # Restart in ascending start_priority order (the agent owns this number)
        # so infrastructure comes back before its dependents. Default 5 for
        # yunos that predate the column.
        def _start_priority(yid):
            try:
                return int(states.get(yid, {}).get("start_priority", 5))
            except (TypeError, ValueError):
                return 5

        for yid in sorted(affected, key=lambda y: (_start_priority(y), y)):
            st = states.get(yid, {})
            restart_yuno(
                ycommand, args.url, jwt, yid,
                bool(st.get("yuno_running")), bool(st.get("yuno_playing")),
                args.dry_run,
            )
    elif affected:
        print(dim("\nNote: a yuno reads its config when it (re)starts. For the change to take"))
        print(dim("effect, restart the yunos that use it: kill-yuno then run-yuno."))
        print(dim("   affected yuno id(s): %s" % ", ".join(sorted(affected))))

    if new_only:
        print(dim("\nNote: NEW config(s) %s — start their yuno(s) when ready." %
                  ", ".join(sorted(new_only))))


if __name__ == "__main__":
    main()
