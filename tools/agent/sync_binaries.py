#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
sync_binaries.py — compare freshly built yuno binaries against the ones the
local yuneta_agent already has, and (optionally) push the differences.

Source of truth for the local side is ``$YUNETAS_BASE/outputs/yunos/`` — the
exact directory the agent's ``$$(<role>)`` macro reads from when it base64-
encodes a binary for upload. Each file is queried with ``--print-role`` to get
its role/version/date; the agent side is read from ``ycommand -c '*list-binaries'``
(the leading ``*`` makes ycommand emit raw JSON instead of the table).

For every local binary it decides:

  * NEW        — the agent has no binary with this id        -> install-binary
  * BUMP       — local version  >  agent version             -> install-binary
  * DOWNGRADE  — local version  <  agent version             -> install-binary (flagged)
  * REBUILD    — same version, content changed (size/date)   -> update-binary
  * UP-TO-DATE — same version, same size                     -> skipped

Then it shows the candidates, asks what to apply, and runs the corresponding
``install-binary`` / ``update-binary`` for each chosen role.

It deliberately does NOT run the surrounding lifecycle steps (kill-yuno before a
same-version overwrite, or find-new-yunos + deactivate-snap after a version
bump) — those have side effects across the whole node. It prints the reminder
instead. See yunos/c/yuno_agent/YUNO_LIFECYCLE.md §6.5/§6.6.
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
        out[name] = {
            "role": name,            # what $$(...) and id= will use
            "reported_role": role,
            "version": info.get("version", "?"),
            "date": info.get("date", "?"),
            "size": os.path.getsize(path),
            "path": path,
        }
    return out


def agent_binaries(ycommand, url):
    """
    Return {id: {version, size, date, binary}} from the agent via
    'ycommand -c *list-binaries' (the '*' forces raw JSON).
    """
    cmd = [ycommand]
    if url:
        cmd += ["-u", url]
    cmd += ["-c", "*list-binaries"]
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
            "binary": rec.get("binary", ""),
        }
    return out


# ----------------------------------------------------------------------------
#   Classification
# ----------------------------------------------------------------------------
def classify(local, agent):
    """
    Yield dicts describing each local binary's relationship to the agent's.
    kind in {new, bump, downgrade, rebuild, uptodate}.
    """
    rows = []
    for role, lb in sorted(local.items()):
        ab = agent.get(role)
        if ab is None:
            rows.append({
                "role": role, "kind": "new", "action": "install-binary",
                "local": lb, "agent": None,
            })
            continue
        cmp = cmp_versions(lb["version"], ab["version"])
        if cmp > 0:
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
            # Same version. Only worth an update-binary if content changed.
            if lb["size"] != ab["size"]:
                rows.append({
                    "role": role, "kind": "rebuild", "action": "update-binary",
                    "local": lb, "agent": ab,
                })
            else:
                rows.append({
                    "role": role, "kind": "uptodate", "action": None,
                    "local": lb, "agent": ab,
                })
    return rows


KIND_LABEL = {
    "new":       (lambda s: green(s),  "NEW",        "install-binary"),
    "bump":      (lambda s: green(s),  "BUMP",       "install-binary"),
    "downgrade": (lambda s: red(s),    "DOWNGRADE",  "install-binary"),
    "rebuild":   (lambda s: yellow(s), "REBUILD",    "update-binary"),
    "uptodate":  (lambda s: dim(s),    "up-to-date", "-"),
}


def print_table(rows, show_uptodate):
    print()
    print(bold("%-18s %-11s %-12s %-12s %-9s %-9s" % (
        "role", "status", "local ver", "agent ver", "Δsize", "command")))
    print(dim("-" * 78))
    for r in rows:
        kind = r["kind"]
        if kind == "uptodate" and not show_uptodate:
            continue
        colour, label, _ = KIND_LABEL[kind]
        lv = r["local"]["version"]
        av = r["agent"]["version"] if r["agent"] else "-"
        if r["agent"]:
            dsize = r["local"]["size"] - r["agent"]["size"]
            dtxt = ("+%d" % dsize) if dsize >= 0 else ("%d" % dsize)
        else:
            dtxt = "-"
        action = r["action"] or "-"
        print("%-18s %s %-12s %-12s %-9s %-9s" % (
            r["role"],
            colour("%-11s" % label),
            lv, av, dtxt, action,
        ))
    print()


# ----------------------------------------------------------------------------
#   Execution
# ----------------------------------------------------------------------------
def run_one(ycommand, url, action, role, dry_run):
    cmd_str = "%s id=%s content64=$$(%s)" % (action, role, role)
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

    print(dim("yunetas base : %s" % base))
    print(dim("yunos dir    : %s" % yunos_dir))
    print(dim("ycommand     : %s%s" % (ycommand, ("  url=" + args.url) if args.url else "")))

    print(dim("\nreading agent binaries (*list-binaries)..."))
    agent = agent_binaries(ycommand, args.url)
    print(dim("reading local binaries (--print-role)..."))
    local = local_binaries(yunos_dir)

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
            print("Aborted.")
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
            print("Aborted.")
            return

    print()
    ok, fail = 0, 0
    for r in chosen:
        if run_one(ycommand, args.url, r["action"], r["role"], args.dry_run):
            ok += 1
        else:
            fail += 1
    print()
    print(bold("Done: %s, %s." % (green("%d ok" % ok), red("%d failed" % fail) if fail else dim("0 failed"))))

    # Lifecycle reminders (we intentionally do not automate these).
    did_update = any(r["action"] == "update-binary" for r in chosen)
    did_install = any(r["action"] == "install-binary" for r in chosen)
    if did_update:
        print(dim("\nNote: update-binary overwrites the SAME-version slot. If a yuno was running"))
        print(dim("from that slot it must be kill-yuno'd first (text-file-busy), then run-yuno."))
    if did_install:
        print(dim("\nNote: install-binary created NEW slot(s). To make them primary run:"))
        print(dim("   ycommand -c 'find-new-yunos create=1'"))
        print(dim("   ycommand -c 'deactivate-snap'      (node-wide bounce; shoot-snap first to keep a rollback)"))


if __name__ == "__main__":
    main()
