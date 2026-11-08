#
#   A peer of the agent's 1991, its frames written by hand
#
#   Run by run_test.sh inside the sandbox. In order:
#
#   1. write-tty of a console that is not there, the kw with a gbuffer
#      (a serialized `__gbuffer___`, which the gate turns into a gbuffer of
#      the kw): the agent answers EV_TTY_CLOSE, and released that gbuffer
#      twice up to 7.30.17;
#   2. check-realm, dir-logs, dir-local-data and update-binary of an id
#      that is not there, with a gbuffer: released twice too;
#   3. list-yunos and command-yuno with a gbuffer of 200 KB: the filter
#      kept a reference of the gbuffer that nobody released (the gbmem audit
#      at the end of the agent, read by run_test.sh);
#   4. a yuno installed and run by the agent (logcenter), then identity
#      cards that the agent refuses, each naming a pid:
#        - an unknown yuno, the pid of a decoy process: the decoy lives;
#        - a second instance of the running yuno, the pid of a decoy: lives;
#        - a second instance of the running yuno, the pid of that yuno: it
#          is killed (a process of its own: what the SIGKILL is for);
#        - an unknown yuno, pid -1: nothing is killed. Up to 7.30.17 the
#          agent sent SIGKILL to the pid of every card it refused, and -1 is
#          every process of the user: this peer too (run_test.sh fails).
#
#   Copyright (c) 2026, ArtGins.
#   All Rights Reserved.
#
import asyncio
import base64
import json
import os
import subprocess
import sys
import time

import websockets

URL = "ws://127.0.0.1:1991"
YUNO_BINARY = sys.argv[1]
failures = []


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


def card(role, pid, yuno_id="", yuno_name="x", yuno_release="1", realm_id=""):
    kw = {
        "yuno_role": role, "yuno_id": yuno_id, "yuno_name": yuno_name, "yuno_tag": "",
        "yuno_version": "1", "yuno_release": yuno_release, "yuneta_version": "",
        "playing": False, "pid": pid, "watcher_pid": 0,
        "jwt": "", "username": "yuneta", "launch_id": 0, "yuno_startdate": "",
        "id": "sandbox", "realm_id": realm_id, "required_services": [],
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


def gbuffer(size):
    return {"label": "", "data": base64.b64encode(b"g" * size).decode()}


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


async def session(role="ycommand", **kw):
    ws = await connect()
    await ws.send(card(role, os.getpid(), **kw))
    got = await answers(ws, 10, lambda g: g[-1].get("event") == "EV_IDENTITY_CARD_ACK")
    ack = got[-1]["kw"] if got else {}
    return ws, ack


def alive(pid):
    try:
        with open("/proc/%d/stat" % pid) as f:
            return f.read().split(") ")[1].split()[0] != "Z"
    except OSError:
        return False


async def refused_card(role, pid, **kw):
    ws = await connect()
    await ws.send(card(role, pid, **kw))
    await answers(ws, 2)
    await ws.close()
    await asyncio.sleep(0.5)


async def main():
    ws, ack = await session()
    check("a session of ycommand on 1991", ack.get("result") == 0)

    #   1. write-tty of a console that is not there
    for i in range(3):
        await ws.send(command("write-tty", {
            "name": "no_such_console", "content64": "aGk=", "__gbuffer___": gbuffer(10)
        }))
    got = await answers(ws, 10, lambda g: len(g) >= 3)
    check("write-tty of an absent console: 3 x EV_TTY_CLOSE",
        [x.get("event") for x in got] == ["EV_TTY_CLOSE"] * 3)

    #   2. the commands that read a node by its id
    for cmd in ("check-realm", "dir-logs", "dir-local-data", "update-binary"):
        r = await ask(ws, cmd, {"id": "nope", "content64": "aGk=", "__gbuffer___": gbuffer(10)})
        check(cmd + " of an id that is not there: refused", r.get("result") == -1)

    #   3. the filters
    for i in range(3):
        r = await ask(ws, "list-yunos", {"__gbuffer___": gbuffer(200*1024)})
        check("list-yunos with a gbuffer of 200 KB", r.get("result") == 0)
        r = await ask(ws, "command-yuno", {"command": "help", "__gbuffer___": gbuffer(200*1024)})
        check("command-yuno with a gbuffer of 200 KB: no yuno", r.get("result") == -1)

    #   4. a yuno of the agent
    with open(YUNO_BINARY, "rb") as f:
        content64 = base64.b64encode(f.read()).decode()
    config64 = base64.b64encode(json.dumps(
        {"__version__": "1", "__description__": "sandbox"}).encode()).decode()
    for cmd, kw in (
        ("create-realm", {"id": "w.prod.test", "realm_owner": "t", "realm_role": "prod",
            "realm_name": "w", "realm_env": "test", "bind_ip": "127.0.0.1"}),
        ("install-binary", {"id": "logcenter", "content64": content64}),
        ("create-config", {"id": "logcenter.x", "content64": config64}),
        ("create-yuno", {"realm_id": "w.prod.test", "yuno_role": "logcenter", "yuno_name": "x"}),
        ("run-yuno", {"yuno_role": "logcenter"}),
    ):
        r = await ask(ws, cmd, kw, secs=30)
        check(cmd, r.get("result") == 0)

    yuno = None
    for i in range(300):    # the yuno is starting: up to 30 s
        r = await ask(ws, "list-yunos", {"yuno_role": "logcenter"})
        rows = r.get("data") or []
        if rows and rows[0].get("yuno_running") and rows[0].get("yuno_pid"):
            yuno = rows[0]
            break
        await asyncio.sleep(0.1)
    check("logcenter runs, its pid known to the agent", yuno is not None)
    if yuno is None:
        return
    realm_id = yuno["realm_id"][0] if isinstance(yuno["realm_id"], list) else yuno["realm_id"]
    same_yuno = {"yuno_id": yuno["id"], "yuno_name": yuno["yuno_name"],
        "yuno_release": yuno["yuno_release"], "realm_id": realm_id}

    decoy = subprocess.Popen(["sleep", "600"])
    await refused_card("evil_role", decoy.pid, yuno_id="4242")
    check("a card of an unknown yuno: the decoy of its pid lives", alive(decoy.pid))

    await refused_card("logcenter", decoy.pid, **same_yuno)
    check("a second card of the running yuno: the decoy of its pid lives", alive(decoy.pid))
    decoy.kill()
    decoy.wait()

    await refused_card("logcenter", yuno["yuno_pid"], **same_yuno)
    check("a second card of the running yuno, its own pid: killed", not alive(yuno["yuno_pid"]))

    await refused_card("evil_role", -1, yuno_id="4243")
    check("a card of pid -1: this peer lives", True)

    await ws.close()


asyncio.run(main())
if failures:
    print("%d check(s) failed" % len(failures))
    sys.exit(1)
sys.exit(0)
