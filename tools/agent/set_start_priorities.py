#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
set_start_priorities.py — assign each managed yuno's ``start_priority`` (the
agent's per-yuno launch-order band 0..9) by role, in one shot.

The agent launches yunos in ascending ``start_priority`` (utilities first) and
stops them in descending order (utilities last, so logcenter captures everyone
else's shutdown). A fresh agent leaves every yuno at the default 5, so the order
is effectively insertion order until the tiers are assigned. This tool reads the
yunos from ``*list-yunos``, maps each role to a tier, and writes the differences
with ``update-node`` (scoped per yuno id, never node-wide).

Default tiers (lower starts earlier):

    1  utilities   logcenter, emailsender, auth_bff
    4  gates       gate_*
    7  dba         db_*
    5  (default)   everything else — left untouched unless a rule matches

Rules are matched in order; the first match wins. ``--rule 'PATTERN=PRIO'`` adds
or overrides a rule (PATTERN is an exact role or a ``prefix_*`` glob), e.g.
``--rule 'notifier_wz=2' --rule 'scheduler_*=6'``. Built-in rules are applied
after explicit ones, so an explicit rule always takes precedence.

It shows the plan, asks for confirmation, then writes only the yunos whose
start_priority actually changes. ``--dry-run`` writes nothing; ``--all`` skips
the prompt. The write goes through ``command-agent service=treedb_yuneta_agent
command=update-node`` with the record base64-encoded into ``content64`` (the
inline ``record={...}`` form is not coerced by the ycommand CLI).

Like its siblings sync_binaries.py / sync_configs.py it can drive a remote
wss:// agent: it logs in ONCE (OAuth2 password grant) and threads the jwt
through ``-j`` on every call. Stdlib only.
"""

import argparse
import base64
import fnmatch
import json
import re
import shutil
import subprocess
import sys
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
#   Default role -> tier rules (first match wins; explicit --rule comes first)
# ----------------------------------------------------------------------------
BUILTIN_RULES = [
    ("logcenter", 1),
    ("emailsender", 1),
    ("auth_bff", 1),
    ("gate_*", 4),
    ("db_*", 7),
]
DEFAULT_PRIORITY = 5


def tier_for_role(role, rules):
    """First matching rule wins; PATTERN is an exact role or an fnmatch glob."""
    for pattern, prio in rules:
        if pattern == role or fnmatch.fnmatchcase(role, pattern):
            return prio
    return None  # no rule -> leave as-is


# ----------------------------------------------------------------------------
#   JSON helper
# ----------------------------------------------------------------------------
_ANSI_RE = re.compile(r"\033\[[0-9;]*m")


def parse_leading_json(text):
    """Strip ANSI, find the first JSON value, decode it, ignore any trailer."""
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


# ----------------------------------------------------------------------------
#   OAuth2 — log in ONCE, reuse the token on every ycommand call
# ----------------------------------------------------------------------------
def _http_json(url, data=None, timeout=30):
    body = None
    headers = {}
    if data is not None:
        body = urllib.parse.urlencode(data).encode("utf-8")
        headers["Content-Type"] = "application/x-www-form-urlencoded"
    req = urllib.request.Request(url, data=body, headers=headers)
    with urllib.request.urlopen(req, timeout=timeout) as resp:
        return json.loads(resp.read().decode("utf-8"))


def discover_token_endpoint(issuer, timeout=30):
    doc = _http_json(issuer.rstrip("/") + "/.well-known/openid-configuration",
                     timeout=timeout)
    ep = doc.get("token_endpoint")
    if not ep:
        raise RuntimeError("OIDC discovery document has no token_endpoint")
    return ep


def obtain_jwt(args):
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
    cmd = [ycommand]
    if url:
        cmd += ["-u", url]
    if jwt:
        cmd += ["-j", jwt]
    return cmd


# ----------------------------------------------------------------------------
#   Agent I/O
# ----------------------------------------------------------------------------
def agent_yunos(ycommand, url, jwt):
    """Return the list of yuno records via '*list-yunos' (raw JSON)."""
    cmd = ycmd_base(ycommand, url, jwt) + ["-c", "*list-yunos"]
    try:
        res = subprocess.run(cmd, capture_output=True, text=True, timeout=30)
        data = parse_leading_json(res.stdout)
    except (OSError, subprocess.SubprocessError) as e:
        print(red("ERROR: cannot run ycommand: %s" % e))
        sys.exit(2)
    except json.JSONDecodeError:
        print(red("ERROR: '*list-yunos' did not return JSON. Is the agent up?"))
        print(dim((res.stdout or "")[:400]))
        sys.exit(2)
    if not isinstance(data, list):
        print(red("ERROR: unexpected '*list-yunos' payload (not a list)."))
        sys.exit(2)
    return [r for r in data if isinstance(r, dict)]


def set_start_priority(ycommand, url, jwt, yid, prio, dry_run):
    """update-node the yuno's start_priority, record base64'd into content64."""
    record = json.dumps({"id": str(yid), "start_priority": int(prio)})
    content64 = base64.b64encode(record.encode("utf-8")).decode("ascii")
    cmd_str = ("command-agent service=treedb_yuneta_agent command=update-node "
               "topic_name=yunos content64=%s" % content64)
    print(cyan(">> set %s start_priority=%d" % (yid, prio)))
    if dry_run:
        print(dim("   (dry-run, not executed)"))
        return True
    cmd = ycmd_base(ycommand, url, jwt) + ["-c", cmd_str]
    try:
        res = subprocess.run(cmd, capture_output=True, text=True, timeout=60)
    except (OSError, subprocess.SubprocessError) as e:
        print(red("   ERROR: %s" % e))
        return False
    out = (res.stdout or "").strip()
    ok = res.returncode == 0 and "ERROR" not in out
    print(green("   OK") if ok else red("   FAILED: %s" % out[:200]))
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
        description="Assign each managed yuno's start_priority by role.")
    ap.add_argument("-u", "--url", default=None,
                    help="ycommand url (default: ws://127.0.0.1:1991).")
    ap.add_argument("--ycommand", default=None,
                    help="path to ycommand (default: search PATH).")
    ap.add_argument("-a", "--all", action="store_true",
                    help="apply without asking for confirmation.")
    ap.add_argument("-n", "--dry-run", action="store_true",
                    help="show the plan, write nothing.")
    ap.add_argument("--rule", action="append", default=[], metavar="PATTERN=PRIO",
                    help="add/override a role->priority rule (PATTERN is an exact "
                         "role or a glob like 'gate_*'); repeatable, matched "
                         "before the built-in rules.")
    ap.add_argument("--show-all", action="store_true",
                    help="also list yunos already at their target priority.")
    auth = ap.add_argument_group(
        "OAuth2 (remote wss:// agent; logs in ONCE, reuses the token via -j)")
    auth.add_argument("-I", "--issuer", default=None,
                      help="OIDC issuer for discovery.")
    auth.add_argument("-T", "--token-endpoint", default=None,
                      help="explicit token endpoint (skips discovery).")
    auth.add_argument("-Z", "--client-id", default=None, help="OAuth2 client_id.")
    auth.add_argument("--client-secret", default=None, help="OAuth2 client_secret.")
    auth.add_argument("-x", "--user-id", default=None, help="OAuth2 username.")
    auth.add_argument("-X", "--user-passw", default=None, help="OAuth2 password.")
    auth.add_argument("-j", "--jwt", default=None, help="reuse this jwt directly.")
    args = ap.parse_args()

    ycommand = args.ycommand or shutil.which("ycommand")
    if not ycommand:
        print(red("ERROR: ycommand not found in PATH (use --ycommand)."))
        sys.exit(2)

    # Build the rule list: explicit --rule first, then built-ins.
    rules = []
    for spec in args.rule:
        if "=" not in spec:
            print(red("ERROR: bad --rule '%s' (want PATTERN=PRIO)." % spec))
            sys.exit(2)
        pat, _, pv = spec.rpartition("=")
        try:
            pv = int(pv)
        except ValueError:
            print(red("ERROR: bad priority in --rule '%s'." % spec))
            sys.exit(2)
        if not 0 <= pv <= 9:
            print(red("ERROR: priority in --rule '%s' must be 0..9." % spec))
            sys.exit(2)
        rules.append((pat.strip(), pv))
    rules += BUILTIN_RULES

    jwt = obtain_jwt(args)

    print(dim("ycommand : %s%s%s" % (
        ycommand,
        ("  url=" + args.url) if args.url else "",
        "  oauth2=on" if jwt else "")))
    print(dim("reading yunos (*list-yunos)..."))
    yunos = agent_yunos(ycommand, args.url, jwt)
    if not yunos:
        print(yellow("No yunos returned by the agent."))
        return

    # Build the plan.
    plan = []
    for y in yunos:
        yid = y.get("id")
        role = y.get("yuno_role", "")
        try:
            cur = int(y.get("start_priority", DEFAULT_PRIORITY))
        except (TypeError, ValueError):
            cur = DEFAULT_PRIORITY
        target = tier_for_role(role, rules)
        plan.append({"id": yid, "role": role, "cur": cur, "target": target})

    # Table.
    print()
    print(bold("%-18s %-8s %-9s %-9s %s" % ("role", "id", "current", "target", "")))
    print(dim("-" * 56))
    changes = []
    for p in sorted(plan, key=lambda r: (r["target"] if r["target"] is not None else 99, r["role"])):
        if p["target"] is None:
            if args.show_all:
                print("%-18s %-8s %-9s %-9s %s" % (
                    p["role"], p["id"], p["cur"], "-", dim("no rule, left as-is")))
            continue
        if p["target"] == p["cur"]:
            if args.show_all:
                print("%-18s %-8s %-9s %-9s %s" % (
                    p["role"], p["id"], p["cur"], p["target"], dim("already")))
            continue
        print("%-18s %-8s %-9s %s %s" % (
            p["role"], p["id"], p["cur"], green("%-9s" % p["target"]), yellow("change")))
        changes.append(p)
    print()

    if not changes:
        print(green("Nothing to change — every yuno already at its target priority."))
        return

    print(bold("%d yuno(s) to update." % len(changes)))
    if not args.all and not args.dry_run:
        if ask("\nApply? [y/N] ") not in ("y", "yes"):
            print("Cancelled - no changes made.")
            return

    print()
    ok = fail = 0
    for p in changes:
        if set_start_priority(ycommand, args.url, jwt, p["id"], p["target"], args.dry_run):
            ok += 1
        else:
            fail += 1
    print()
    print(bold("Done: %s, %s." % (
        green("%d ok" % ok), red("%d failed" % fail) if fail else dim("0 failed"))))
    print(dim("\nEffect: next run-yuno launches ascending (utilities first); "
              "kill/pause descending (utilities last)."))


if __name__ == "__main__":
    main()
