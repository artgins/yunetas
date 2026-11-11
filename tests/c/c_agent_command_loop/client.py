#
#   A peer of the agent's 1991: command-agent lines that name command-agent
#
#   Run by run_test.sh inside the sandbox. Each line names command-agent as
#   the command to forward, in a text that is not the bare name: in quotes,
#   with a blank after it, with more words, in capitals. The agent must
#   answer "Loop command" and live. Up to 7.30.26 it compared the whole text
#   with the name, so these lines passed the guard, and the agent forwarded
#   the line to itself with the same kw (which still carried the line) until
#   its stack overflowed: SIGSEGV, with one audit record per call.
#
#   Lines with two levels of quotes are no command for the guard nor for
#   the parser (each reads the first word once): "command not available".
#
#   A command-agent to a service that is a link to another yuno (the
#   control center's), with the link down: answered at once with -1, and
#   so is a stats-agent of it.
#
#   A content64 that is not base64 (create-config, update-config,
#   install-binary): answered -1 naming it. A config content64 that is
#   empty or not json: answered -1 too.
#
#   Lines whose secret's key the shell's quotes split, for the audit file;
#   and write-tty to services that are no console: EV_TTY_CLOSE.
#
#   Then a command-agent of another command (help) is still forwarded and
#   answered, and the session still works.
#
#   Copyright (c) 2026, ArtGins.
#   All Rights Reserved.
#
import asyncio
import base64
import json
import os
import sys
import time

import websockets

URL = "ws://127.0.0.1:1991"
failures = []

#   A config that is not json, with a private key under a name that no mask
#   knows: none of it may reach the agent's log
NOT_JSON64 = base64.b64encode(
    b'{"crypto": {"ssl_certificate_key": "-----BEGIN PRIVATE KEY-----'
    b'MIIEvQIBADANBgkqhkiG9w0BAQEFAASCBKcwggSjAgEAAoIBAQC7"} oops'
).decode()

LOOP_LINES = (
    "command-agent command=\"'command-agent'\"",
    "command-agent command=\"command-agent service=agent\"",
    "command-agent command=\"command-agent \"",
    "command-agent command=\"COMMAND-AGENT service=agent\"",
    "command-agent service=agent command='\"command-agent\" x=1'",
    "command-agent command=command-agent",
)


def check(what, ok):
    print(("ok    " if ok else "FAIL  ") + what)
    if not ok:
        failures.append(what)


def routing(src_role):
    return {
        "dst_yuno": "", "dst_role": "yuneta_agent", "dst_service": "agent",
        "src_yuno": "", "src_role": src_role, "src_service": "cli",
        "user": "yuneta", "host": "sandbox",
    }


def card(role, pid):
    kw = {
        "yuno_role": role, "yuno_id": "", "yuno_name": "x", "yuno_tag": "",
        "yuno_version": "1", "yuno_release": "1", "yuneta_version": "",
        "playing": False, "pid": pid, "watcher_pid": 0,
        "jwt": "", "username": "yuneta", "launch_id": 0, "yuno_startdate": "",
        "id": "sandbox", "realm_id": "", "required_services": [],
        "__md_iev__": {
            "ievent_gate_stack": [routing(role)],
            "__msg_type__": "__identity__",
        },
    }
    return json.dumps({"event": "EV_IDENTITY_CARD", "kw": kw})


def command(cmd, kw_cmd):
    kw = {
        "__command__": cmd,
        "__md_iev__": {
            "ievent_gate_stack": [routing("ycommand")],
            "__msg_type__": "__command__",
            "command_stack": [{"command": cmd, "kw": {}}],
        },
    }
    kw.update(kw_cmd)
    return json.dumps({"event": "EV_MT_COMMAND", "kw": kw})


async def answers(ws, secs, until=None):
    got = []
    end = time.monotonic() + secs
    while time.monotonic() < end:
        try:
            frame = await asyncio.wait_for(ws.recv(), end - time.monotonic())
        except (asyncio.TimeoutError, websockets.ConnectionClosed):
            break
        iev = json.loads(frame)
        got.append(iev)
        if until and until(got):
            break
    return got


async def ask(ws, cmd, kw_cmd, secs=10):
    await ws.send(command(cmd, kw_cmd))
    got = await answers(ws, secs, lambda g: g[-1].get("event") == "EV_MT_COMMAND_ANSWER")
    return got[-1]["kw"] if got and got[-1].get("event") == "EV_MT_COMMAND_ANSWER" else {}


async def connect():
    for i in range(300):    # the agent is starting: up to 30 s
        try:
            return await websockets.connect(URL, max_size=None)
        except OSError:
            await asyncio.sleep(0.1)
    raise SystemExit("the agent never listened on " + URL)


async def session():
    ws = await connect()
    await ws.send(card("ycommand", os.getpid()))
    got = await answers(ws, 10, lambda g: g[-1].get("event") == "EV_IDENTITY_CARD_ACK")
    ack = got[-1]["kw"] if got else {}
    return ws, ack


async def main():
    ws, ack = await session()
    check("a session of ycommand on 1991", ack.get("result") == 0)

    for line in LOOP_LINES:
        try:
            r = await ask(ws, line, {})
        except websockets.ConnectionClosed:
            r = {}
        if not r:
            check("%s: no answer (the agent died)" % line, False)
            return
        check("%s: %s" % (line, r.get("comment")),
            r.get("result") == -1 and "Loop command" in (r.get("comment") or ""))

    #   The same, the line to forward given in the kw
    r = await ask(ws, "command-agent", {"command": "'command-agent' service=agent"})
    check("command-agent, kw command='command-agent' service=agent: %s" % r.get("comment"),
        r.get("result") == -1 and "Loop command" in (r.get("comment") or ""))

    #   Two levels of quotes, the line in the kw: the guard and the parser
    #   read its first word once each, so both see `'command-agent'` (no
    #   command). Up to 7.30.26 the parser read it twice, saw command-agent
    #   behind the guard's back, and the agent called itself for ever
    #   (SIGSEGV). Quotes cannot nest a third level: the next quote of its
    #   kind closes a quoted word, so the last line names no command either.
    for kw_line in (
        "\"'command-agent'\"",
        "'\"COMMAND-AGENT\" x=1'",
        " \t\"'command-agent'\" service=agent",
        "'\"'command-agent'\"'",
    ):
        try:
            r = await ask(ws, "command-agent", {"command": kw_line})
        except websockets.ConnectionClosed:
            r = {}
        if not r:
            check("command-agent, kw command=%r: no answer (the agent died)" % kw_line, False)
            return
        check("command-agent, kw command=%r: %s" % (kw_line, r.get("comment")),
            r.get("result") == -1 and "command not available" in (r.get("comment") or ""))

    #   A service of the agent that is a link to another yuno, with the link
    #   down (the sandbox has no control center): an answer at once, -1.
    #   Up to 7.30.26 the link answered nothing (NULL is "the answer will
    #   come") and the peer waited for ever
    r = await ask(ws, "command-agent service=controlcenter command=list-yunos", {}, secs=5)
    check("command-agent service=controlcenter with the link down: %s" % r.get("comment"),
        r.get("result") == -1 and "not in session" in (r.get("comment") or ""))

    #   The same for the stats of that link: up to 7.30.26 no answer either
    r = await ask(ws, "stats-agent service=controlcenter", {}, secs=5)
    check("stats-agent service=controlcenter with the link down: %s" % r.get("comment"),
        r.get("result") == -1 and "not in session" in (r.get("comment") or ""))

    #   A content64 that is not base64: the operator's bytes, answered -1
    #   naming the problem, and a WARNING in the log (up to 7.30.26 an ERROR
    #   with a stack, "b64_decode() FAILED")
    for line, says in (
        ("create-config id=bad64 content64=@@@notbase64@@@", "Invalid base64 in content64"),
        ("update-config id=bad64 content64=@@@notbase64@@@", "Invalid base64 in content64"),
        ("install-binary id=bad64 content64=@@@notbase64@@@", "Bad data in content64"),
    ):
        r = await ask(ws, line, {}, secs=5)
        check("%s: %s" % (line, r.get("comment")),
            r.get("result") == -1 and says in (r.get("comment") or ""))

    #   A content64 that is base64 of nothing, or of a config that is not
    #   json: answered -1, a WARNING with no stack and no dump of the
    #   content (up to 7.30.26 "len is ZERO" and "json_loads() FAILED", ERRORs
    #   with a stack, and a dump of the WHOLE config: a private key under a
    #   name no mask knows among it)
    for line, says in (
        ("create-config id=empty64 content64='  '", "Bad json in content64"),
        ("update-config id=empty64 content64='  '", "Bad json in content64"),
        ("create-config id=notjson content64=" + NOT_JSON64, "Bad json in content64"),
        ("update-config id=notjson content64=" + NOT_JSON64, "Bad json in content64"),
    ):
        r = await ask(ws, line, {}, secs=5)
        check("%s: %s" % (line[:48], r.get("comment")),
            r.get("result") == -1 and says in (r.get("comment") or ""))

    #   A secret's key split by quotes, which the shell reads without them:
    #   the audit file must not hold the secret (run_test.sh checks it; up
    #   to 7.30.26 each of these lines was written with it)
    for line in (
        "create-realm p'a'ssword=AUDITSEKRET1",
        "create-realm 'p'assword=AUDITSEKRET2",
        "update-node p\"a\"ssword=AUDITSEKRET3",
        "update-realm id=x record='{\"p'a'ssword\":\"AUDITSEKRET4\"}'",
        "update-realm id=x record='{\"'p'assword\":\"'AUDITSEKRET5'\"}'",
    ):
        r = await ask(ws, line, {}, secs=5)
        check("%s: answered" % line, bool(r))

    #   A write-tty to a service that is not a console: refused with a
    #   WARNING, the requester told there is no such console (EV_TTY_CLOSE).
    #   Up to 7.30.26 __yuno__ answered "Event NOT DEFINED in state" (an
    #   ERROR with a stack), and the agent sent EV_WRITE_TTY to itself
    for name in ("__yuno__", "agent"):
        line = "write-tty name=%s content64=YQ==" % name
        await ws.send(command(line, {}))
        got = await answers(ws, 3, lambda g: g[-1].get("event") == "EV_TTY_CLOSE")
        closed = [g for g in got if g.get("event") == "EV_TTY_CLOSE"]
        check("%s: EV_TTY_CLOSE" % line,
            bool(closed) and (closed[-1].get("kw", {}).get("data") or {}).get("name") == name)

    #   A write-tty without content64: a WARNING, this message dropped (up
    #   to 7.30.26 an ERROR); run_test.sh counts it
    await ws.send(command("write-tty name=nosuch", {}))
    await answers(ws, 1)

    #   Another command is still forwarded
    r = await ask(ws, "command-agent command=\"help cmd=list-yunos\"", {})
    check("command-agent command=\"help cmd=list-yunos\": forwarded and answered",
        r.get("result") == 0)
    r = await ask(ws, "help cmd=command-agent", {})
    check("the agent still answers", r.get("result") == 0)

    await ws.close()


asyncio.run(main())
if failures:
    print("%d check(s) failed" % len(failures))
    sys.exit(1)
sys.exit(0)
