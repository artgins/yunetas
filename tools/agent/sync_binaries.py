#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
sync_binaries.py — compare what the local yuneta_agent already has installed
against the freshly built binaries, and (optionally) push the differences.

It DRIVES FROM THE AGENT's installed binaries, not from outputs/yunos: only
roles the agent already manages on this node are candidates. Not every binary
in outputs/yunos is meant to be installed here, so iterating the agent avoids
proposing roles this node doesn't run.

The agent side is read from ``ycommand -c '*list-binaries'`` (the leading ``*``
makes ycommand emit raw JSON instead of the table). That command reports only the
PRIMARY (active) slot per role; the full set of installed versions is read from
``ycommand -c '*list-binaries-instances'``. The instance list is what decides
"is this version already installed?": with a snap active the primary can be an
OLD version while the freshly built one is already installed as a non-primary
slot — comparing only against the primary would mis-classify that as a BUMP and
then fail ``install-binary`` with "Node already exists". For each installed id
its freshly built counterpart is looked up in ``$YUNETAS_BASE/outputs/yunos/`` —
the exact directory the agent's ``$$(<role>)`` macro reads from when it base64-
encodes a binary for upload — and queried with ``--print-role`` for its version.

For every installed binary it decides:

  * BUMP       — local version not installed, > primary       -> install-binary
  * INSTALLED  — local version already an installed slot, but  -> skipped (promote
                 not the primary (snap active / pending promote)   with upgrade-yunos)
  * DOWNGRADE  — local version  <  agent version             -> install-binary (flagged)
  * REBUILD    — same version, content changed: size differs,  -> update-binary
                 OR the local file is newer than the agent
                 slot (a relink/edit that left the byte count
                 unchanged still bumps the file time)
  * UP-TO-DATE — same version, same size AND not newer         -> skipped
  * NO-BUILD   — agent has it, but no build in outputs/yunos  -> skipped (informational)

Size alone misses a rebuild that keeps the byte count identical (e.g. a one-char
log-string edit, or a relink against a changed static lib). The file time is the
second signal: ``*list-binaries[-instances]`` reports each binary's on-disk
mtime as ``time`` (epoch) / ``time_str`` next to ``size``; a LOCAL file mtime
newer than the agent's installed slot is offered as a REBUILD even when the size
matches. When the agent predates the ``time`` field it falls back to the
embedded build datetime (``date``, the C ``__DATE__ " " __TIME__``, reported by
both ``--print-role`` and ``*list-binaries``).

Then it shows the candidates, asks what to apply, and runs the corresponding
``install-binary`` / ``update-binary`` for each chosen role.

For a REBUILD (``update-binary``) the agent overwrites the slot whose version
equals the uploaded binary's. The copy fails with text-file-busy only if a LIVE
process is mapped to that exact file — an instance whose ``role_version`` is the
version being written. Once the operator has cleared the two confirmation gates
(Apply-all / Proceed) the intent to deploy is explicit, so this script runs the
documented per-role hot-patch cycle itself, scoped by ``yuno_role`` (never
node-wide):

    kill-yuno yuno_role=R   (only if running THAT version) -> poll until it exits
    update-binary id=R
    run-yuno yuno_role=R play=0   (only if it had been running)
    play-yuno yuno_role=R         (only if it had been playing)

If the running instances are on a DIFFERENT version (e.g. a snap pins an older
release), the slot being overwritten is not the one executing, so the
kill/restart is skipped and ``update-binary`` writes in place. Prior run/play
state is read from ``*list-yunos`` (the ``role_version`` per instance) and
restored per role, so a deliberately stopped or paused yuno is left as it was.
Pass ``--no-restart`` to
keep the old print-only behaviour. A role with several instances across realms
is handled in one shot: the role-scoped commands act on every instance, which
all share the one slot.

Roles are deployed in ascending ``start_priority`` (read from the agent via
``*list-yunos``, lowest among a role's instances), so a REBUILD brings
infrastructure (logcenter/emailsender/auth_bff) back before gates and dba
instead of in alphabetical order.

It still does NOT automate the version-bump path (find-new-yunos +
deactivate-snap after an install-binary) — that is a node-wide bounce with
broader side effects. It prints the reminder instead, pointing at
``yunetas upgrade-yunos`` (the wrapper that bundles those steps with a
rollback snap).
See yunos/c/yuno_agent/YUNO_LIFECYCLE.md §6.5/§6.6.
"""

import argparse
import json
import os
import re
import shutil
import subprocess
import sys
import time
import urllib.error
import urllib.parse
import urllib.request
from datetime import datetime

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
#   Version comparison
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


def version_tuple(v):
    """
    Turn '7.4.5' or '1.4.4.0' into a comparable tuple of ints. Non-numeric
    chunks fall back to 0 so a malformed version never crashes the compare.
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


def parse_build_date(s):
    """
    Parse the build datetime embedded in a binary and reported identically by
    '--print-role' and '*list-binaries' as the `date` field ('Mmm D YYYY
    HH:MM:SS' — the C `__DATE__ " " __TIME__`). __DATE__ space-pads a single
    digit day ('Jun  5 2026'), so collapse runs of whitespace before parsing.
    Returns a datetime, or None when absent/malformed so the caller falls back
    to the size-only compare.
    """
    if not s or s == "?":
        return None
    norm = re.sub(r"\s+", " ", str(s)).strip()
    for fmt in ("%b %d %Y %H:%M:%S", "%b %d %Y"):
        try:
            return datetime.strptime(norm, fmt)
        except ValueError:
            continue
    return None


def is_newer_build(local_date, ref_date):
    """
    True iff `local_date` parses to a strictly later build datetime than
    `ref_date`. If either side is missing/unparseable returns False (the caller
    then relies on size alone), so a malformed date never forces a spurious
    REBUILD.
    """
    ld = parse_build_date(local_date)
    rd = parse_build_date(ref_date)
    return ld is not None and rd is not None and ld > rd


def local_is_newer(lb, ref_time, ref_date):
    """
    True iff the freshly built local binary is newer than the reference slot.

    Prefers the numeric file mtime ('time', epoch seconds) the agent reports
    next to 'size' in *list-binaries[-instances]: local file mtime > the
    installed slot's. This is the robust signal — it catches a relink that left
    the byte count unchanged AND a recompile, on the same node clock.

    Falls back to the embedded build-date string ('date', the C
    __DATE__ " " __TIME__) when the agent predates the 'time' field. Either
    side missing -> False (size alone decides), so it never forces a spurious
    REBUILD.
    """
    lt = lb.get("mtime")
    if lt is not None and ref_time is not None:
        return lt > ref_time
    return is_newer_build(lb.get("date"), ref_date)


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
def find_yunetas_base():
    base = os.environ.get("YUNETAS_BASE")
    if base and os.path.isdir(base):
        return base
    # Fall back: walk up from this script (tools/agent/) until we find outputs/yunos.
    here = os.path.dirname(os.path.abspath(__file__))
    cur = here
    while True:
        if os.path.isdir(os.path.join(cur, "outputs", "yunos")):
            return cur
        parent = os.path.dirname(cur)
        if parent == cur:
            break
        cur = parent
    return os.path.dirname(os.path.dirname(here))


def local_binaries(yunos_dir):
    """
    Return {role: {version, date, size, path, file}} for every executable
    regular file in outputs/yunos, queried via --print-role.
    """
    out = {}
    for name in sorted(os.listdir(yunos_dir)):
        path = os.path.join(yunos_dir, name)
        if not os.path.isfile(path) or not os.access(path, os.X_OK):
            continue
        try:
            res = subprocess.run(
                [path, "--print-role"],
                capture_output=True, text=True, timeout=20,
            )
        except (OSError, subprocess.SubprocessError) as e:
            print(red("  ! %s: cannot run --print-role (%s)" % (name, e)))
            continue
        info = None
        try:
            info = parse_leading_json(res.stdout)
        except json.JSONDecodeError:
            print(red("  ! %s: --print-role did not return JSON, skipped" % name))
            continue
        role = info.get("role") or name
        if role != name:
            # $$(<file>) uploads by filename; update-binary id=<role> targets by
            # the agent id. They normally coincide; warn if they don't.
            print(yellow("  ! %s: role '%s' != filename, using filename for upload" % (name, role)))
        try:
            mtime = int(os.path.getmtime(path))
        except OSError:
            mtime = None
        out[name] = {
            "role": name,            # what $$(...) and id= will use
            "reported_role": role,
            "version": info.get("version", "?"),
            "date": info.get("date", "?"),
            "mtime": mtime,          # local file mtime, vs the agent's `time`
            "size": os.path.getsize(path),
            "path": path,
        }
    return out


def agent_binaries(ycommand, url, jwt):
    """
    Return {id: {version, size, date, binary}} from the agent via
    'ycommand -c *list-binaries' (the '*' forces raw JSON).
    """
    cmd = ycmd_base(ycommand, url, jwt) + ["-c", "*list-binaries"]
    try:
        res = subprocess.run(cmd, capture_output=True, text=True, timeout=30)
    except (OSError, subprocess.SubprocessError) as e:
        print(red("ERROR: cannot run ycommand: %s" % e))
        sys.exit(2)
    try:
        data = parse_leading_json(res.stdout)
    except json.JSONDecodeError:
        print(red("ERROR: '*list-binaries' did not return JSON. Is the agent up?"))
        print(dim(res.stdout[:500]))
        print(dim(res.stderr[:500]))
        sys.exit(2)
    if not isinstance(data, list):
        print(red("ERROR: unexpected '*list-binaries' payload (not a list)."))
        sys.exit(2)
    out = {}
    for rec in data:
        bid = rec.get("id")
        if not bid:
            continue
        out[bid] = {
            "version": rec.get("version", "?"),
            "size": rec.get("size", 0),
            "date": rec.get("date", "?"),
            "time": rec.get("time"),   # agent-reported file mtime (epoch), or None
            "binary": rec.get("binary", ""),
        }
    return out


def agent_binary_instances(ycommand, url, jwt):
    """
    Return {role: {version: size}} for EVERY installed slot, via
    'ycommand -c *list-binaries-instances' (the '*' forces raw JSON).

    '*list-binaries' reports only the PRIMARY (active) version per role. When a
    snap is active the primary can be an OLD version while the freshly built one
    is already installed as a non-primary slot; classifying against the primary
    alone would call that a BUMP and then fail install-binary with "Node already
    exists". This map is the authoritative "is this version already installed?".

    On any error returns {} so classify() degrades to the primary-only compare.
    """
    cmd = ycmd_base(ycommand, url, jwt) + ["-c", "*list-binaries-instances"]
    try:
        res = subprocess.run(cmd, capture_output=True, text=True, timeout=30)
        data = parse_leading_json(res.stdout)
    except (OSError, subprocess.SubprocessError, json.JSONDecodeError):
        return {}
    if not isinstance(data, list):
        return {}
    out = {}
    for rec in data:
        bid = rec.get("id")
        ver = rec.get("version")
        if not bid or ver is None:
            continue
        out.setdefault(bid, {})[str(ver)] = {
            "size": rec.get("size", 0),
            "date": rec.get("date", "?"),
            "time": rec.get("time"),   # agent-reported file mtime (epoch), or None
        }
    return out


# ----------------------------------------------------------------------------
#   Classification
# ----------------------------------------------------------------------------
def classify(local, agent, instances):
    """
    Drive from the AGENT's installed binaries (not from outputs/yunos): only
    roles the agent already manages on this node are candidates. For each one
    look up its freshly built counterpart and compare.

    `instances` is {role: {version: size}} from '*list-binaries-instances'; it
    guards the BUMP path so a version already installed as a non-primary slot
    (snap active / pending promote) is not offered to a doomed install-binary.

    kind in {bump, installed, downgrade, rebuild, uptodate, nolocal}.
    """
    rows = []
    for role, ab in sorted(agent.items()):
        lb = local.get(role)
        if lb is None:
            # Agent has it installed but there is no build in outputs/yunos.
            rows.append({
                "role": role, "kind": "nolocal", "action": None,
                "local": None, "agent": ab,
            })
            continue
        inst = instances.get(role, {})
        cmp = cmp_versions(lb["version"], ab["version"])
        if cmp > 0:
            if lb["version"] in inst:
                # The freshly built version is ALREADY installed as a slot that
                # is not the active primary (a snap is pinning an older one).
                # install-binary would fail "Node already exists"; nothing to
                # push — it only needs promotion (yunetas upgrade-yunos). If its
                # content drifted (size differs, OR the local build is newer-
                # dated than that slot) it is a same-version REBUILD of that slot
                # instead, which update-binary overwrites in place.
                slot = inst[lb["version"]]
                if lb["size"] != slot["size"]:
                    rows.append({
                        "role": role, "kind": "rebuild", "action": "update-binary",
                        "local": lb, "agent": ab, "reason": "size",
                    })
                elif local_is_newer(lb, slot.get("time"), slot.get("date")):
                    rows.append({
                        "role": role, "kind": "rebuild", "action": "update-binary",
                        "local": lb, "agent": ab, "reason": "newer build",
                    })
                else:
                    rows.append({
                        "role": role, "kind": "installed", "action": None,
                        "local": lb, "agent": ab,
                    })
                continue
            rows.append({
                "role": role, "kind": "bump", "action": "install-binary",
                "local": lb, "agent": ab,
            })
        elif cmp < 0:
            rows.append({
                "role": role, "kind": "downgrade", "action": "install-binary",
                "local": lb, "agent": ab,
            })
        else:
            # Same version. Worth an update-binary if the content changed. Size
            # is the cheap signal; a rebuild that doesn't move the byte count
            # (a one-char log edit, or a relink) is caught by comparing the file
            # mtime — the agent's `time` vs the local file's (or the embedded
            # build `date` when the agent predates `time`).
            if lb["size"] != ab["size"]:
                rows.append({
                    "role": role, "kind": "rebuild", "action": "update-binary",
                    "local": lb, "agent": ab, "reason": "size",
                })
            elif local_is_newer(lb, ab.get("time"), ab.get("date")):
                rows.append({
                    "role": role, "kind": "rebuild", "action": "update-binary",
                    "local": lb, "agent": ab, "reason": "newer build",
                })
            else:
                rows.append({
                    "role": role, "kind": "uptodate", "action": None,
                    "local": lb, "agent": ab,
                })
    return rows


KIND_LABEL = {
    "bump":      (lambda s: green(s),  "BUMP",       "install-binary"),
    "installed": (lambda s: cyan(s),   "INSTALLED",  "-"),
    "downgrade": (lambda s: red(s),    "DOWNGRADE",  "install-binary"),
    "rebuild":   (lambda s: yellow(s), "REBUILD",    "update-binary"),
    "uptodate":  (lambda s: dim(s),    "up-to-date", "-"),
    "nolocal":   (lambda s: dim(s),    "no-build",   "-"),
}


def _short_mtime(epoch):
    """epoch -> 'YYYY-MM-DD HH:MM' for the table note, or '' if unavailable."""
    if not epoch:
        return ""
    try:
        return datetime.fromtimestamp(epoch).strftime("%Y-%m-%d %H:%M")
    except (OverflowError, OSError, ValueError):
        return ""


def print_table(rows, show_uptodate):
    print()
    print(bold("%-18s %-11s %-12s %-12s %-9s %-15s %s" % (
        "role", "status", "local ver", "agent ver", "Δsize", "command", "note")))
    print(dim("-" * 92))
    for r in rows:
        kind = r["kind"]
        if kind in ("uptodate", "nolocal") and not show_uptodate:
            continue
        colour, label, _ = KIND_LABEL[kind]
        lv = r["local"]["version"] if r["local"] else "-"
        av = r["agent"]["version"] if r["agent"] else "-"
        if r["local"] and r["agent"]:
            dsize = r["local"]["size"] - r["agent"]["size"]
            dtxt = ("+%d" % dsize) if dsize >= 0 else ("%d" % dsize)
        else:
            dtxt = "-"
        action = r["action"] or "-"
        # A date-triggered REBUILD shows Δsize 0; spell out why so it doesn't
        # read as a no-op. Show the local file's build time as the evidence.
        note = ""
        if kind == "rebuild" and r.get("reason") == "newer build":
            stamp = _short_mtime(r["local"].get("mtime")) if r["local"] else ""
            note = dim("newer build" + (" (%s)" % stamp if stamp else ""))
        print("%-18s %s %-12s %-12s %-9s %-15s %s" % (
            r["role"],
            colour("%-11s" % label),
            lv, av, dtxt, action, note,
        ))
    print()


# ----------------------------------------------------------------------------
#   Execution
# ----------------------------------------------------------------------------
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
    # A resumed upgrade re-runs install-binary against a slot that a prior run
    # already created; the agent answers "... already exists" (treedb create_node
    # / "Binary already exists"). That is idempotent, not a failure: the new slot
    # is already there, pending promotion via upgrade-yunos. Treat it as ok so the
    # tally and exit code stay clean and the operator isn't scared by a red FAILED.
    already = "already exists" in out
    ok = res.returncode == 0 and ("ERROR" not in out or already)
    if already:
        print(yellow("   ALREADY PRESENT (idempotent, pending promote)"))
    else:
        print(green("   OK") if ok else red("   FAILED"))
    return ok, out


def yuno_states(ycommand, url, jwt, role):
    """
    Return the list of instance records for `role` via '*list-yunos
    yuno_role=R'. Each record carries yuno_running / yuno_playing. A role may
    have several instances (one per realm); they all share the one slot.
    Returns [] on any error — the caller treats "unknown" as "not running".
    """
    cmd = ycmd_base(ycommand, url, jwt) + ["-c", "*list-yunos yuno_role=%s" % role]
    try:
        res = subprocess.run(cmd, capture_output=True, text=True, timeout=30)
        data = parse_leading_json(res.stdout)
    except (OSError, subprocess.SubprocessError, json.JSONDecodeError):
        return []
    if not isinstance(data, list):
        return []
    return [r for r in data if isinstance(r, dict)]


def agent_start_priorities(ycommand, url, jwt):
    """
    Return {role: start_priority} from '*list-yunos', taking the LOWEST
    start_priority among a role's instances (its most-infrastructure instance
    sets the role's restart rank). The agent owns this number; we order
    restarts by it instead of alphabetically. Instances that predate the column
    default to 5. {} on any error — the caller then defaults every role to 5.
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
        if not isinstance(rec, dict):
            continue
        role = rec.get("yuno_role")
        if not role:
            continue
        try:
            prio = int(rec.get("start_priority", 5))
        except (TypeError, ValueError):
            prio = 5
        if role not in out or prio < out[role]:
            out[role] = prio
    return out


def wait_until_stopped(ycommand, url, jwt, role, timeout_s=15.0, poll_s=0.3):
    """
    Poll '*list-yunos yuno_role=R' until no instance reports yuno_running, so
    the executable is unmapped before update-binary overwrites its slot
    (otherwise the copy hits text-file-busy again). Returns True if stopped.
    """
    deadline = time.monotonic() + timeout_s
    while time.monotonic() < deadline:
        states = yuno_states(ycommand, url, jwt, role)
        if not any(s.get("yuno_running") for s in states):
            return True
        time.sleep(poll_s)
    return False


def deploy_install(ycommand, url, jwt, action, role, dry_run):
    """install-binary / update-binary with NO lifecycle (--no-restart, or bump)."""
    ok, _ = run_ycmd(
        ycommand, url, jwt,
        "%s id=%s content64=$$(%s)" % (action, role, role),
        dry_run,
    )
    return ok


def deploy_update_with_restart(ycommand, url, jwt, role, local_version, dry_run):
    """
    Same-version REBUILD hot-patch, scoped to `role`: stop the running
    instance(s) so the slot is free, overwrite it, then restore each
    instance's prior run/play state. Reads state up front; if nothing is
    running it degrades to a plain update-binary.

    update-binary overwrites the slot whose version equals the uploaded
    binary's (`local_version`). text-file-busy only bites if a LIVE process is
    mapped to that exact file — an instance whose role_version is the version
    being written. If the running instances are on a different version (e.g. a
    snap pins an older release), the slot we overwrite is not the one
    executing, so the kill/restart cycle is skipped and update-binary writes
    in place. An instance with no role_version (predates the column) is treated
    as on-target, so we kill on the safe side rather than risk text-file-busy.
    """
    states = yuno_states(ycommand, url, jwt, role)

    def on_target(s):
        rv = str(s.get("role_version", "")).strip()
        if not rv:
            return True  # unknown running version -> assume ours (kill, safe side)
        return cmp_versions(rv, local_version) == 0

    was_running = any(s.get("yuno_running") and on_target(s) for s in states)
    was_playing = any(s.get("yuno_playing") and on_target(s) for s in states)

    if not was_running and any(s.get("yuno_running") for s in states):
        # Something IS running, but on another version; overwriting our
        # (non-running) slot can't hit text-file-busy. No kill/restart needed.
        print(dim(
            "   running version differs from the rebuilt %s; update-binary in "
            "place (no kill/restart)" % local_version))

    if was_running:
        # Orderly shutdown (SIGQUIT, not force) so the gbmem audit runs.
        run_ycmd(ycommand, url, jwt, "kill-yuno yuno_role=%s" % role, dry_run)
        if not dry_run and not wait_until_stopped(ycommand, url, jwt, role):
            print(yellow(
                "   ! %s still running after kill; update-binary may hit "
                "text-file-busy" % role))

    ok, _ = run_ycmd(
        ycommand, url, jwt,
        "update-binary id=%s content64=$$(%s)" % (role, role),
        dry_run,
    )

    # Restore prior state even if the update failed, so we never leave a yuno
    # we stopped lying dead (it comes back on the old binary in that case).
    if was_running:
        run_ycmd(ycommand, url, jwt, "run-yuno yuno_role=%s play=0" % role, dry_run)
        if was_playing:
            run_ycmd(ycommand, url, jwt, "play-yuno yuno_role=%s" % role, dry_run)

    return ok


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
        description="Compare outputs/yunos binaries with the agent and push updates.")
    ap.add_argument("-u", "--url", default=None,
                    help="ycommand url (default: ws://127.0.0.1:1991).")
    ap.add_argument("--ycommand", default=None,
                    help="path to ycommand (default: search PATH).")
    ap.add_argument("--yunos-dir", default=None,
                    help="override outputs/yunos directory.")
    ap.add_argument("-a", "--all", action="store_true",
                    help="apply every candidate without asking per-role.")
    ap.add_argument("-n", "--dry-run", action="store_true",
                    help="show what would run, execute nothing.")
    ap.add_argument("--show-uptodate", action="store_true",
                    help="also list binaries already in sync.")
    ap.add_argument("--no-restart", action="store_true",
                    help="for same-version REBUILDs, do NOT kill/restart the "
                         "running yuno; run update-binary only and print the "
                         "manual reminder (old behaviour).")
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

    base = find_yunetas_base()
    yunos_dir = args.yunos_dir or os.path.join(base, "outputs", "yunos")
    if not os.path.isdir(yunos_dir):
        print(red("ERROR: yunos dir not found: %s" % yunos_dir))
        sys.exit(2)

    jwt = obtain_jwt(args)

    print(dim("yunetas base : %s" % base))
    print(dim("yunos dir    : %s" % yunos_dir))
    print(dim("ycommand     : %s%s%s" % (
        ycommand,
        ("  url=" + args.url) if args.url else "",
        "  oauth2=on" if jwt else "")))

    print(dim("\nreading agent binaries (*list-binaries)..."))
    agent = agent_binaries(ycommand, args.url, jwt)
    print(dim("reading installed slots (*list-binaries-instances)..."))
    instances = agent_binary_instances(ycommand, args.url, jwt)
    print(dim("reading local binaries (--print-role)..."))
    local = local_binaries(yunos_dir)

    rows = classify(local, agent, instances)
    print_table(rows, show_uptodate=args.show_uptodate or args.dry_run)

    installed = [r for r in rows if r["kind"] == "installed"]
    if installed:
        print(yellow(
            "%d binary(ies) already installed but NOT primary (snap active / "
            "pending promote)." % len(installed)))
        print(dim("Promote the staged version(s) with:  yunetas upgrade-yunos"))

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

    downgrades = [r for r in candidates if r["kind"] == "downgrade"]
    if downgrades:
        print(red("WARNING: %d downgrade(s) — local version is OLDER than the agent's:" %
                  len(downgrades)))
        for r in downgrades:
            print(red("   %s  local %s < agent %s" %
                      (r["role"], r["local"]["version"], r["agent"]["version"])))

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
                    r["role"], colour(label), r["action"]))
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

    # Deploy in ascending start_priority order so a same-version REBUILD brings
    # infrastructure (logcenter/emailsender/auth_bff) back before gates and dba.
    # Harmless for installs (no restart). Single source of truth: the agent.
    prio_map = agent_start_priorities(ycommand, args.url, jwt)
    chosen.sort(key=lambda r: (prio_map.get(r["role"], 5), r["role"]))

    print()
    ok, fail = 0, 0
    for r in chosen:
        if r["action"] == "update-binary" and not args.no_restart:
            success = deploy_update_with_restart(
                ycommand, args.url, jwt, r["role"], r["local"]["version"], args.dry_run)
        else:
            success = deploy_install(
                ycommand, args.url, jwt, r["action"], r["role"], args.dry_run)
        if success:
            ok += 1
        else:
            fail += 1
    print()
    print(bold("Done: %s, %s." % (green("%d ok" % ok), red("%d failed" % fail) if fail else dim("0 failed"))))

    # Lifecycle reminders / notes.
    did_update = any(r["action"] == "update-binary" for r in chosen)
    did_install = any(r["action"] == "install-binary" for r in chosen)
    if did_update and args.no_restart:
        print(dim("\nNote: update-binary overwrites the SAME-version slot. If a yuno was running"))
        print(dim("from that slot it must be kill-yuno'd first (text-file-busy), then run-yuno."))
    elif did_update:
        print(dim("\nNote: REBUILD roles were kill-yuno'd, overwritten and restored to their"))
        print(dim("prior run/play state, scoped by yuno_role."))
    if did_install:
        print(dim("\nNote: install-binary created NEW slot(s). To make them primary run:"))
        print(dim("   yunetas upgrade-yunos              (shoot-snap -> find-new-yunos -> deactivate-snap)"))
        print(dim("or the raw equivalent:"))
        print(dim("   ycommand -c 'find-new-yunos create=1'"))
        print(dim("   ycommand -c 'deactivate-snap'      (node-wide bounce; shoot-snap first to keep a rollback)"))


if __name__ == "__main__":
    main()
