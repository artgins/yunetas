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
     leading ``*`` makes ycommand emit raw JSON instead of the table),
  4. classifies it.

Classification
--------------
  * NEW        — id not in the agent                       -> create-config
  * BUMP       — local version  >  agent version           -> create-config
  * UPDATE     — same version, content changed             -> update-config
  * UP-TO-DATE — same version, identical content           -> skipped
  * DOWNGRADE  — local version  <  agent version           -> reported only, NOT pushed
  * agent-only — agent has a config absent here            -> skipped (informational)

A DOWNGRADE is never offered for install: seeding a stale version with
create-config would break the version logic, so it is reported and left alone.

The new-version install verb is ``create-config`` (it refuses to overwrite an
existing ``(id, version)``); ``update-config`` overwrites the content of an
existing same-version record. ``create-config`` also answers to the alias
``install-config``, by analogy with ``install-binary``.

Content upload uses the same ``content64=$$(<path>)`` macro as binaries; ``$$()``
base64-encodes the file. We pass the file's absolute path so it resolves
regardless of ycommand's working directory.

Like ``sync_binaries.py`` it deliberately does NOT run the surrounding lifecycle
steps. A yuno reads its config when it (re)starts, so after pushing a changed
config the yunos that use it must be restarted (kill-yuno + run-yuno) for it to
take effect. The affected yuno ids (from the agent's ``yunos`` field) are printed
as a reminder.
"""

import argparse
import json
import os
import re
import shutil
import subprocess
import sys

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
#   Discovery
# ----------------------------------------------------------------------------
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


def agent_configs(ycommand, url):
    """
    Return {id: {version, description, date, zcontent, yunos}} from the agent via
    'ycommand -c *list-configs' (the '*' forces raw JSON). One record per id
    (the current primary version).
    """
    cmd = [ycommand]
    if url:
        cmd += ["-u", url]
    cmd += ["-c", "*list-configs"]
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


# ----------------------------------------------------------------------------
#   Classification
# ----------------------------------------------------------------------------
def classify(local, agent):
    """
    Drive from the LOCAL configs found in the directory. For each one look it up
    in the agent and compare. Agent-only configs (present in the agent, absent
    here) are appended as informational 'orphan' rows.

    kind in {new, bump, update, uptodate, downgrade, orphan}.
    """
    rows = []
    for cid, lc in sorted(local.items()):
        ac = agent.get(cid)
        if ac is None:
            rows.append({
                "id": cid, "kind": "new", "action": "create-config",
                "local": lc, "agent": None,
            })
            continue
        cmp = cmp_versions(lc["version"], ac["version"])
        if cmp > 0:
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
def run_one(ycommand, url, action, cid, path, dry_run):
    cmd_str = "%s id='%s' content64=$$(%s)" % (action, cid, path)
    cmd = [ycommand]
    if url:
        cmd += ["-u", url]
    cmd += ["-c", cmd_str]
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
    ok = res.returncode == 0 and "ERROR" not in out
    print(green("   OK") if ok else red("   FAILED"))
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
    args = ap.parse_args()

    ycommand = args.ycommand or shutil.which("ycommand")
    if not ycommand:
        print(red("ERROR: ycommand not found in PATH (use --ycommand)."))
        sys.exit(2)

    config_dir = os.path.abspath(args.config_dir)
    if not os.path.isdir(config_dir):
        print(red("ERROR: config dir not found: %s" % config_dir))
        sys.exit(2)

    print(dim("config dir : %s" % config_dir))
    print(dim("ycommand   : %s%s" % (ycommand, ("  url=" + args.url) if args.url else "")))

    print(dim("\nreading agent configs (*list-configs)..."))
    agent = agent_configs(ycommand, args.url)
    print(dim("reading local configs (*.json in dir)..."))
    local = local_configs(config_dir)

    if not local:
        print(yellow("\nNo deployable *.json configs found in %s." % config_dir))
        return

    rows = classify(local, agent)
    print_table(rows, show_uptodate=args.show_uptodate or args.dry_run)

    candidates = [r for r in rows if r["action"]]
    if not candidates:
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
            print("Aborted.")
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
            print("Aborted.")
            return

    print()
    ok, fail = 0, 0
    for r in chosen:
        if run_one(ycommand, args.url, r["action"], r["id"], r["local"]["path"], args.dry_run):
            ok += 1
        else:
            fail += 1
    print()
    print(bold("Done: %s, %s." % (green("%d ok" % ok), red("%d failed" % fail) if fail else dim("0 failed"))))

    # Lifecycle reminder (we intentionally do not automate this).
    affected = set()
    for r in chosen:
        if r["agent"]:
            for y in r["agent"].get("yunos", []) or []:
                affected.add(str(y))
    print(dim("\nNote: a yuno reads its config when it (re)starts. For a changed config to"))
    print(dim("take effect, restart the yunos that use it: kill-yuno then run-yuno."))
    if affected:
        print(dim("   affected yuno id(s): %s" % ", ".join(sorted(affected))))


if __name__ == "__main__":
    main()
