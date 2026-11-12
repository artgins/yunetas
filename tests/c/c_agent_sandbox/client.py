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
#      The yuno killed with SIGKILL stays down (a watcher does not relaunch
#      a yuno killed that way), and a run-yuno runs it again.
#   5. a card the agent ACCEPTS, of a yuno registered and not running, with
#      pid -1 and watcher_pid -1: neither is recorded (a WARNING), and a
#      kill-yuno of it is refused (no pid). Up to 7.30.17 the agent recorded
#      the card's pids and kill-yuno sent SIGQUIT to pid -1.
#   6. trace-on-yuno and trace-off-yuno of the running logcenter: without
#      a level, or with one that is not global, trace-on-yuno is refused,
#      lists the global levels and sets nothing; with levels it sets them in
#      the yuno (its get-global-trace says them, a second call adds to them)
#      and `traced` is true; trace-off-yuno resets them all and `traced` is
#      false. Three times. Each answer of the yuno goes to the agent
#      (run_test.sh: no "a command of the agent to a yuno failed", and no
#      memory left). Up to 7.30.21 trace-on-yuno set the user levels of the
#      yuno's root gobj, which has none: nothing was traced. Up to 7.30.18
#      the command named no gobj (the yuno answered "gobj not found: ''"),
#      trace-off-yuno's answer reached this peer as a second answer, and
#      each call left the yunos found in the agent.
#
#   7. a crash of the running logcenter (SIGSEGV): its watcher relaunches
#      it, a run-yuno asked while the watcher waits launches nothing (the
#      watcher is a process of the yuno), and the agent registers the
#      relaunched one: one instance, and no "ALREADY living" beyond the two
#      cards of 4 (run_test.sh). It is the order of events of a relaunch by
#      the watcher that meets a launch by the agent.
#   8. trace-on-yuno and trace-off-yuno of a yuno not connected (killed):
#      refused, naming it, and `traced` not touched; run again it has its
#      saved trace, and a trace-off then resets it. Up to 7.30.22
#      trace-off-yuno answered 0 and set traced false, and the yuno came
#      back traced.
#   9. delete-yuno whole=1 removes the trace the yuno saved (an emailsender:
#      logcenter's domain_dir is fixed in its binary): created again with
#      the same id, it traces nothing. Up to 7.30.22 the saved trace came
#      back in the new yuno.
#  10. delete-yuno yuno_release=<its only release> takes the whole yuno:
#      it removes the saved trace too. Up to 7.30.23 only whole=1 did, and
#      the trace came back in the yuno created again.
#  11. a run-yuno that plays keeps its requester for the answer of the play
#      of that launch only: a play-yuno, then a relaunch by the watcher
#      after a crash, answer nothing more to the session that ran it, nor to
#      one that took its channel. Up to 7.30.26 the relaunch answered that
#      run-yuno again, to whoever held the channel.
#
#   Copyright (c) 2026, ArtGins.
#   All Rights Reserved.
#
import asyncio
import base64
import json
import os
import signal
import subprocess
import sys
import time

import websockets

URL = "ws://127.0.0.1:1991"
YUNO_BINARY = sys.argv[1]
OTHER_BINARY = sys.argv[2] if len(sys.argv) > 2 else ""  # a yuno whose domain_dir the agent gives
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


def card(role, pid, yuno_id="", yuno_name="x", yuno_release="1", realm_id="", watcher_pid=0):
    kw = {
        "yuno_role": role, "yuno_id": yuno_id, "yuno_name": yuno_name, "yuno_tag": "",
        "yuno_version": "1", "yuno_release": yuno_release, "yuneta_version": "",
        "playing": False, "pid": pid, "watcher_pid": watcher_pid,
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


def processes_named(name):
    pids = []
    for d in os.listdir("/proc"):
        if not d.isdigit():
            continue
        try:
            with open("/proc/%s/cmdline" % d, "rb") as f:
                argv0 = f.read().split(b"\0")[0].decode(errors="replace")
        except OSError:
            continue
        if os.path.basename(argv0) == name and alive(int(d)):
            pids.append(int(d))
    return pids


def alive(pid):
    try:
        with open("/proc/%d/stat" % pid) as f:
            return f.read().split(") ")[1].split()[0] != "Z"
    except OSError:
        return False


async def run_and_wait(ws, yid):
    """run-yuno, then wait until the yuno answers a command (connected)"""
    r = await ask(ws, "run-yuno", {"id": yid}, secs=30)
    if r.get("result") != 0:
        print("run-yuno %s: %s" % (yid, r.get("comment")))
        return None
    for i in range(300):    # up to 30 s
        r = await ask(ws, "command-yuno id=%s service=__root__ command=get-global-trace" % yid, {})
        if r.get("result") == 0:
            r = await ask(ws, "list-yunos", {"id": yid})
            rows = r.get("data") or []
            return rows[0] if rows else None
        await asyncio.sleep(0.1)
    return None


async def kill_and_wait(ws, yid, pid):
    r = await ask(ws, "kill-yuno", {"id": yid}, secs=30)
    check("kill-yuno %s: %s" % (yid, r.get("comment")), r.get("result") == 0)
    for i in range(100):
        r = await ask(ws, "list-yunos", {"id": yid})
        rows = r.get("data") or []
        if not alive(pid) and rows and not rows[0].get("yuno_running"):
            return
        await asyncio.sleep(0.1)
    check("kill-yuno %s: it ended" % yid, False)


async def refused_card(role, pid, **kw):
    ws = await connect()
    await ws.send(card(role, pid, **kw))
    await answers(ws, 2)
    await ws.close()
    await asyncio.sleep(0.5)


async def main():
    ws, ack = await session()
    check("a session of ycommand on 1991", ack.get("result") == 0)
    booted = time.monotonic() + 6

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

    #   4. a yuno of the agent. The agent runs its enabled yunos once, some
    #   3 s after it starts (timerStBoot is 0.1 s in agent.json): the yuno is
    #   created after that, so that only this peer runs it
    await asyncio.sleep(max(0, booted - time.monotonic()))
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

    #   The agent killed it with SIGKILL, and a watcher does not relaunch a
    #   yuno killed that way (ydaemon.c): the yuno is down until it is run
    #   again. Then run-yuno: its new pid is recorded
    for i in range(100):    # up to 10 s
        r = await ask(ws, "list-yunos", {"yuno_role": "logcenter", "yuno_name": "x"})
        rows = r.get("data") or []
        if rows and not rows[0].get("yuno_running"):
            break
        await asyncio.sleep(0.1)
    check("the killed logcenter is down", bool(rows) and not rows[0].get("yuno_running"))
    await asyncio.sleep(3)  # a watcher relaunches a crashed yuno 2 s after its end
    check("its watcher does not relaunch a yuno killed with SIGKILL",
        not processes_named("logcenter"))
    r = await ask(ws, "list-yunos", {"yuno_role": "logcenter", "yuno_name": "x"})
    rows = r.get("data") or []
    check("nor the agent", bool(rows) and not rows[0].get("yuno_running"))
    relaunched = await run_and_wait(ws, yuno["id"])
    check("logcenter run again, its new pid recorded",
        relaunched is not None and relaunched.get("yuno_pid") not in (0, yuno["yuno_pid"]))
    if relaunched:
        #   6. trace-on-yuno and trace-off-yuno of the running logcenter
        yid = relaunched["id"]

        async def global_traces():
            #   in the command line: a `service` in the kw would route it to that service of the agent
            r = await ask(ws, "command-yuno id=%s service=__root__ command=get-global-trace" % yid, {})
            data = r.get("data")
            return sorted(data) if isinstance(data, list) else data

        async def traced():
            r = await ask(ws, "list-yunos", {"id": yid})
            rows = r.get("data") or []
            return rows[0].get("traced") if rows else None

        before = await global_traces()
        check("the global traces of the logcenter before: %s" % before, before == [])
        r = await ask(ws, "trace-on-yuno", {"id": yid})
        check("trace-on-yuno without level: refused, the global levels listed: %s" % r.get("comment"),
            r.get("result") == -1 and '"machine"' in json.dumps(r.get("data")))
        r = await ask(ws, "trace-on-yuno", {"id": yid, "level": "machine,no_such_level"})
        check("trace-on-yuno of a level that is not global: refused: %s" % r.get("comment"),
            r.get("result") == -1 and '"ev_kw"' in json.dumps(r.get("data")))
        check("nothing was set by the refused ones", await global_traces() == [] and await traced() is False)
        for i in range(3):
            r = await ask(ws, "trace-on-yuno", {"id": yid, "level": "machine,ev_kw"})
            check("trace-on-yuno level=machine,ev_kw: %s" % r.get("comment"), r.get("result") == 0)
            r = await ask(ws, "trace-on-yuno", {"id": yid, "level": "commands"})
            check("trace-on-yuno level=commands, on a traced yuno: %s" % r.get("comment"), r.get("result") == 0)
            on = await global_traces()
            check("the yuno traces machine, ev_kw and commands: %s" % on,
                on == ["commands", "ev_kw", "machine"])
            check("traced is true", await traced() is True)
            r = await ask(ws, "trace-off-yuno", {"id": yid})
            check("trace-off-yuno: %s" % r.get("comment"), r.get("result") == 0)
            off = await global_traces()
            check("the yuno traces nothing: %s" % off, off == [])
            check("traced is false", await traced() is False)

        #   7. a crash: its watcher relaunches it, and a run-yuno that comes
        #      while the watcher waits launches nothing; the agent registers
        #      the relaunched one, with no "ALREADY living" (run_test.sh)
        old_pid = relaunched["yuno_pid"]
        os.kill(old_pid, signal.SIGSEGV)
        await asyncio.sleep(1.0)    # the watcher waits 2 s before its first relaunch
        r = await ask(ws, "run-yuno", {"id": yid}, secs=35)
        comment = r.get("comment") or ""
        check("run-yuno while the watcher relaunches: nothing launched: %s" % comment,
            r.get("result") == -1 and "alive but not connected" in comment)
        crashed = relaunched
        relaunched = None
        for i in range(150):    # up to 15 s
            r = await ask(ws, "list-yunos", {"id": yid})
            rows = r.get("data") or []
            if rows and rows[0].get("yuno_running") and rows[0].get("yuno_pid") not in (0, old_pid):
                relaunched = rows[0]
                break
            await asyncio.sleep(0.1)
        check("relaunched by the same watcher, its new pid recorded",
            relaunched is not None and relaunched.get("watcher_pid") == crashed.get("watcher_pid"))
        if relaunched:
            check("one instance: its watcher and its child",
                sorted(processes_named("logcenter")) ==
                    sorted([relaunched["watcher_pid"], relaunched["yuno_pid"]]))
        r = await ask(ws, "kill-yuno", {"id": relaunched["id"]}, secs=30)
        check("kill-yuno of the running logcenter", r.get("result") == 0)
        for i in range(100):
            if not alive(relaunched["yuno_pid"]):
                break
            await asyncio.sleep(0.1)
        check("kill-yuno of the running logcenter: it ended", not alive(relaunched["yuno_pid"]))

        #   8. trace-on-yuno and trace-off-yuno of a yuno not connected to
        #      the agent: nothing reaches it, the answer says so, and
        #      `traced` is not touched. The yuno keeps its saved trace and
        #      has it again at its next start; a trace-off then resets it.
        #      Up to 7.30.22 trace-off-yuno answered 0 and set traced false,
        #      and the trace came back with the yuno.
        running = await run_and_wait(ws, yid)
        check("logcenter run again, connected", running is not None)
        if running:
            r = await ask(ws, "trace-on-yuno", {"id": yid, "level": "machine,ev_kw"})
            check("trace-on-yuno level=machine,ev_kw: %s" % r.get("comment"), r.get("result") == 0)
            check("traced is true", await traced() is True)
            await kill_and_wait(ws, yid, running["yuno_pid"])
            r = await ask(ws, "trace-off-yuno", {"id": yid})
            comment = r.get("comment") or ""
            check("trace-off-yuno of the stopped yuno: not reached: %s" % comment,
                r.get("result") == -1 and "not connected" in comment and yid in comment)
            check("traced is still true", await traced() is True)
            r = await ask(ws, "trace-on-yuno", {"id": yid, "level": "commands"})
            comment = r.get("comment") or ""
            check("trace-on-yuno of the stopped yuno: not reached: %s" % comment,
                r.get("result") == -1 and "not connected" in comment)
            check("traced is still true, after the refused trace-on", await traced() is True)
            running = await run_and_wait(ws, yid)
            check("logcenter run again, connected", running is not None)
            if running:
                on = await global_traces()
                check("the yuno has its saved trace again: %s" % on, on == ["ev_kw", "machine"])
                r = await ask(ws, "trace-off-yuno", {"id": yid})
                check("trace-off-yuno of the running yuno: %s" % r.get("comment"), r.get("result") == 0)
                check("traced is false", await traced() is False)
                off = await global_traces()
                check("the yuno traces nothing: %s" % off, off == [])
                await kill_and_wait(ws, yid, running["yuno_pid"])

        #   11. A run-yuno that plays (play=1, the default) keeps its
        #       requester for the answer of the play: it goes with the first
        #       open of that launch, and only then. The yuno does not play
        #       by itself (must_play false), a play-yuno plays it, and its
        #       watcher relaunches it after a crash: neither the session that
        #       ran it, nor one that took its channel, gets an answer then.
        #       Up to 7.30.26 the relaunch answered that run-yuno again, to
        #       whoever held the channel.
        for leaves in (False, True):
            how = "the session that ran it leaves" if leaves else "the session that ran it stays"
            a, ack = await session()
            check("%s: a session" % how, ack.get("result") == 0)
            r = await ask(a, "run-yuno", {"id": yid}, secs=30)
            check("%s: run-yuno: %s" % (how, r.get("comment")), r.get("result") == 0)
            played = None
            for i in range(100):
                r = await ask(ws, "list-yunos", {"id": yid})
                rows = r.get("data") or []
                if rows and rows[0].get("yuno_running") and rows[0].get("yuno_pid"):
                    played = rows[0]
                    break
                await asyncio.sleep(0.1)
            check("%s: the yuno runs, not playing (must_play false)" % how,
                played is not None and not played.get("yuno_playing") and not played.get("must_play"))
            if played is None:
                await a.close()
                continue
            r = await ask(a, "play-yuno", {"id": yid}, secs=30)
            check("%s: play-yuno: %s" % (how, r.get("comment")), r.get("result") == 0)
            listener = a
            if leaves:
                await a.close()
                await asyncio.sleep(0.5)
                listener, ack = await session()     # it takes the channel of the one that left
                check("%s: another session" % how, ack.get("result") == 0)
            os.kill(played["yuno_pid"], signal.SIGSEGV)
            again = None
            for i in range(150):    # up to 15 s: its watcher waits 2 s
                r = await ask(ws, "list-yunos", {"id": yid})
                rows = r.get("data") or []
                if rows and rows[0].get("yuno_playing") and rows[0].get("yuno_pid") not in (0, played["yuno_pid"]):
                    again = rows[0]
                    break
                await asyncio.sleep(0.1)
            check("%s: relaunched by its watcher, and played (must_play true now)" % how, again is not None)
            got = await answers(listener, 3)
            unasked = [g["kw"].get("comment") for g in got if g.get("event") == "EV_MT_COMMAND_ANSWER"]
            check("%s: no answer to a session that asked nothing: %s" % (how, unasked), not unasked)
            r = await ask(ws, "pause-yuno", {"id": yid}, secs=30)
            check("%s: pause-yuno (must_play false again): %s" % (how, r.get("comment")), r.get("result") == 0)
            if again:
                await kill_and_wait(ws, yid, again["yuno_pid"])
            await listener.close()

    #   9. delete-yuno whole=1 removes the trace the yuno saved: a yuno
    #      created again with the same id starts with none. Up to 7.30.22
    #      the saved trace survived the delete and came back in the new
    #      yuno, with `traced` false. (Not logcenter: its domain_dir is
    #      fixed in its binary, the agent does not know where it saves.)
    if OTHER_BINARY:
        with open(OTHER_BINARY, "rb") as f:
            other64 = base64.b64encode(f.read()).decode()
        oid = "sandbox_other"
        orole = os.path.basename(OTHER_BINARY)
        for cmd, kw in (
            ("install-binary", {"id": orole, "content64": other64}),
            ("create-config", {"id": orole + ".z", "content64": config64}),
            ("create-yuno", {"id": oid, "realm_id": "w.prod.test", "yuno_role": orole, "yuno_name": "z"}),
        ):
            r = await ask(ws, cmd, kw, secs=30)
            check(cmd + " " + orole + ": %s" % r.get("comment"), r.get("result") == 0)
        saved = "/yuneta/realms/t/w.prod.test/%s^%s/data/C_YUNO-z-persistent-attrs.json" % (orole, oid)
        running = await run_and_wait(ws, oid)
        check(orole + " runs, connected", running is not None)
        if running:
            r = await ask(ws, "trace-on-yuno", {"id": oid, "level": "machine"})
            check("trace-on-yuno of " + orole + ": %s" % r.get("comment"), r.get("result") == 0)
            check("its trace is saved in " + saved, os.path.exists(saved))
            await kill_and_wait(ws, oid, running["yuno_pid"])
            r = await ask(ws, "delete-yuno", {"id": oid, "whole": True})
            check("delete-yuno whole=1: %s" % r.get("comment"), r.get("result") == 0)
            check("its saved trace is removed", not os.path.exists(saved))
            r = await ask(ws, "create-yuno",
                {"id": oid, "realm_id": "w.prod.test", "yuno_role": orole, "yuno_name": "z"})
            check("create-yuno again, the same id: %s" % r.get("comment"), r.get("result") == 0)
            running = await run_and_wait(ws, oid)
            check(orole + " created again runs, connected", running is not None)
            if running:
                r = await ask(ws, "command-yuno id=%s service=__root__ command=get-global-trace" % oid, {})
                check("the yuno created again traces nothing: %s" % r.get("data"), r.get("data") == [])
                r = await ask(ws, "list-yunos", {"id": oid})
                rows = r.get("data") or []
                check("and traced is false", bool(rows) and rows[0].get("traced") is False)
                await kill_and_wait(ws, oid, running["yuno_pid"])

        #   10. delete-yuno yuno_release=<its only release> takes the whole
        #       yuno, so it removes the trace the yuno saved too. Up to
        #       7.30.23 only whole=1 removed it, and the saved trace came
        #       back in a yuno created again with the same id.
        running = await run_and_wait(ws, oid)
        check(orole + " runs again, connected", running is not None)
        if running:
            r = await ask(ws, "trace-on-yuno", {"id": oid, "level": "machine"})
            check("trace-on-yuno of " + orole + " again: %s" % r.get("comment"), r.get("result") == 0)
            check("its trace is saved again in " + saved, os.path.exists(saved))
            await kill_and_wait(ws, oid, running["yuno_pid"])
            release = running.get("yuno_release") or ""
            r = await ask(ws, "list-yunos", {"id": oid})
            check("it has one release only: %s" % release,
                len(r.get("data") or []) == 1 and release != "")
            r = await ask(ws, "delete-yuno", {"id": oid, "yuno_release": release})
            check("delete-yuno yuno_release=%s, its only one: %s" % (release, r.get("comment")),
                r.get("result") == 0)
            r = await ask(ws, "list-yunos", {"id": oid})
            check("the yuno is gone", not (r.get("data") or []))
            check("its saved trace is removed, deleted by its only release", not os.path.exists(saved))
            r = await ask(ws, "create-yuno",
                {"id": oid, "realm_id": "w.prod.test", "yuno_role": orole, "yuno_name": "z"})
            check("create-yuno again after the delete by release: %s" % r.get("comment"), r.get("result") == 0)
            running = await run_and_wait(ws, oid)
            check(orole + " created again runs, connected", running is not None)
            if running:
                r = await ask(ws, "command-yuno id=%s service=__root__ command=get-global-trace" % oid, {})
                check("the yuno created again after the delete by release traces nothing: %s" % r.get("data"),
                    r.get("data") == [])
                await kill_and_wait(ws, oid, running["yuno_pid"])

    #   5. a card the agent accepts, of a yuno registered and not running,
    #      that says pid -1: the pid is not recorded, and the kill-yuno that
    #      follows signals nothing. Up to 7.30.17 the agent kept the pid of
    #      the card, and kill-yuno sent SIGQUIT to every process of the user.
    r = await ask(ws, "create-config", {"id": "logcenter.y", "content64": config64})
    check("create-config of a second logcenter", r.get("result") == 0)
    r = await ask(ws, "create-yuno", {"realm_id": "w.prod.test", "yuno_role": "logcenter", "yuno_name": "y"})
    check("create-yuno of a second logcenter, not run", r.get("result") == 0)
    r = await ask(ws, "list-yunos", {"yuno_role": "logcenter", "yuno_name": "y"})
    rows = r.get("data") or []
    check("the second logcenter is registered and not running", len(rows) == 1 and not rows[0].get("yuno_running"))
    if len(rows) == 1:
        other = rows[0]
        ws2 = await connect()
        await ws2.send(card("logcenter", -1, yuno_id=other["id"], yuno_name="y",
            yuno_release=other["yuno_release"], realm_id=realm_id, watcher_pid=-1))
        got = await answers(ws2, 10, lambda g: g[-1].get("event") == "EV_IDENTITY_CARD_ACK")
        check("the card of the second logcenter, pid -1: accepted",
            bool(got) and got[-1]["kw"].get("result") == 0)
        await asyncio.sleep(0.5)
        r = await ask(ws, "list-yunos", {"yuno_role": "logcenter", "yuno_name": "y"})
        rows = r.get("data") or []
        check("its pid -1 is not recorded",
            len(rows) == 1 and rows[0].get("yuno_pid") == 0 and rows[0].get("watcher_pid") == 0)
        r = await ask(ws, "kill-yuno", {"id": other["id"]})
        check("kill-yuno of it: refused, no process to signal", r.get("result") == -1)
        await asyncio.sleep(0.5)
        check("kill-yuno of it: this peer lives", True)
        await ws2.close()

    await refused_card("evil_role", -1, yuno_id="4243")
    check("a card of pid -1: this peer lives", True)

    await ws.close()


asyncio.run(main())
if failures:
    print("%d check(s) failed" % len(failures))
    sys.exit(1)
sys.exit(0)
