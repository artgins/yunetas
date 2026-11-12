#
#   Answers that come to the agent from its 1991: the readings of a watch,
#   the answer to a publish-stats, and the answer of a yuno to a session
#   that left
#
#   Run by run_test.sh inside the sandbox. A yuno (a card of a logcenter
#   registered and not run, written here) answers what the agent asks it.
#
#   1. A session watches the yuno (watch-yuno-stats): the first readings
#      come from the yuno. Then another session sends the agent a reading
#      of that yuno (an EV_MT_STATS_ANSWER to `agent` with
#      `__md_iev__.agent_watch`), and an answer to a publish-stats that
#      refuses it (`agent_publish`): neither came from the yuno, and
#      neither is taken (up to 7.30.26 the watcher got the forged reading,
#      and the agent took the yuno as refusing its publication).
#   2. A session sends command-yuno and leaves before the yuno answers;
#      another session takes its channel of the agent (the same `input-N`)
#      and sends command-yuno too: it gets its own answer, never the first
#      one's (up to 7.30.26 it got both). The same with stats-yuno.
#   3. The yuno answers to a service of the agent that is no requester (no
#      channel, no link): dropped with a WARNING.
#   6. What the drops of 4 must let through: the agent's requests asked by
#      their commands (read-file, read-json, running-keys, running-bin) are
#      answered, each with its own event; a real console opened by a
#      session and joined by another: both get its open, what it writes and
#      its close (up to 7.30.26 neither did: an ERROR "Event NOT DEFINED in
#      state" per frame); and a console goes with the session that opened
#      it (up to 7.30.26 it was never closed).
#   5. The agent stops (SIGINT) with a watch armed and the yuno publishing:
#      the yuno is told to stop publishing, with no ERROR (up to 7.30.26 the
#      stop read the treedb its pause had closed: "hgobj NULL or DESTROYED"
#      with a stack, and the yuno was told nothing).
#
#   Copyright (c) 2026, ArtGins.
#   All Rights Reserved.
#
import asyncio
import base64
import json
import os
import signal
import struct
import sys
import time

HOST = "127.0.0.1"
PORT = 1991
YUNO_BINARY = sys.argv[1]
YUNO_READING = "of the yuno"
failures = []


def check(what, ok):
    print(("ok    " if ok else "FAIL  ") + what, flush=True)
    if not ok:
        failures.append(what)


def routing(src_role, dst_service="agent"):
    return {
        "dst_yuno": "", "dst_role": "yuneta_agent", "dst_service": dst_service,
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
    return {"event": "EV_IDENTITY_CARD", "kw": kw}


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
    return {"event": "EV_MT_COMMAND", "kw": kw}


def answer_of(iev, answer_event, result, comment, data=None):
    """What a yuno's C_IEVENT_CLI answers to a request: its routing reversed"""
    md = json.loads(json.dumps(iev["kw"].get("__md_iev__", {})))
    stack = md.get("ievent_gate_stack") or [{}]
    top = stack[0]
    top["dst_yuno"], top["dst_role"], top["dst_service"] = (
        top.get("src_yuno", ""), top.get("src_role", ""), top.get("src_service", ""))
    top["src_yuno"], top["src_role"], top["src_service"] = "x", "logcenter", "logcenter"
    md["__msg_type__"] = "__answer__"
    return {"event": answer_event, "kw": {
        "result": result, "comment": comment, "schema": None, "data": data, "__md_iev__": md}}


def forged(event, md_key, md_value, result, data):
    return {"event": event, "kw": {
        "result": result, "comment": "forged by a session", "schema": None, "data": data,
        "__md_iev__": {
            "ievent_gate_stack": [routing("ycommand")],
            "__msg_type__": "__answer__",
            md_key: md_value,
        }}}


PEER_POINTER = 1094795585     # 0x41414141: what a peer writes where the agent keeps a pointer


def ack(event, result, temp=None):
    """A play / pause ack, as a yuno sends it; `temp`: a `__temp__` of the peer"""
    kw = {"result": result, "comment": "an ack of a session", "data": None,
        "__md_iev__": {
            "ievent_gate_stack": [routing("ycommand")],
            "__msg_type__": "__answer__",
        }}
    if temp is not None:
        kw["__temp__"] = temp
    return {"event": event, "kw": kw}


def peer_event(event, kw):
    """An event a session sends, with the kw it chooses"""
    kw = dict(kw)
    kw["__md_iev__"] = {"ievent_gate_stack": [routing("ycommand")]}
    return {"event": event, "kw": kw}


class Peer:
    """A websocket client written by hand: its frames go out as they are made"""

    async def open(self):
        for i in range(300):    # the agent is starting: up to 30 s
            try:
                self.r, self.w = await asyncio.open_connection(HOST, PORT)
                break
            except OSError:
                await asyncio.sleep(0.1)
        else:
            raise SystemExit("the agent never listened on %s:%d" % (HOST, PORT))
        key = base64.b64encode(os.urandom(16)).decode()
        self.w.write((
            "GET / HTTP/1.1\r\nHost: %s:%d\r\nUpgrade: websocket\r\nConnection: Upgrade\r\n"
            "Sec-WebSocket-Key: %s\r\nSec-WebSocket-Version: 13\r\n\r\n" % (HOST, PORT, key)
        ).encode())
        head = await asyncio.wait_for(self.r.readuntil(b"\r\n\r\n"), 10)
        if b" 101 " not in head.split(b"\r\n")[0]:
            raise SystemExit("no websocket upgrade: %r" % head[:80])
        return self

    @staticmethod
    def frame(opcode, payload):
        n = len(payload)
        b0 = 0x80 | opcode
        if n < 126:
            hdr = bytes([b0, 0x80 | n])
        elif n < 65536:
            hdr = bytes([b0, 0x80 | 126]) + struct.pack(">H", n)
        else:
            hdr = bytes([b0, 0x80 | 127]) + struct.pack(">Q", n)
        mask = os.urandom(4)
        return hdr + mask + bytes(c ^ mask[i % 4] for i, c in enumerate(payload))

    def send(self, iev):
        self.w.write(self.frame(0x1, json.dumps(iev).encode()))

    async def recv(self, secs):
        """The next text frame as json; None at a Close, at the end or at the time"""
        try:
            b = await asyncio.wait_for(self.r.readexactly(2), secs)
            n = b[1] & 0x7f
            if n == 126:
                n = struct.unpack(">H", await self.r.readexactly(2))[0]
            elif n == 127:
                n = struct.unpack(">Q", await self.r.readexactly(8))[0]
            payload = await self.r.readexactly(n)
        except (asyncio.TimeoutError, asyncio.IncompleteReadError, ConnectionError):
            return None
        if (b[0] & 0x0f) != 0x1:
            return None
        return json.loads(payload)

    async def until(self, events, secs):
        end = time.monotonic() + secs
        while time.monotonic() < end:
            iev = await self.recv(end - time.monotonic())
            if iev is None:
                return None
            if iev.get("event") in events:
                return iev
        return None

    async def ask(self, cmd, kw_cmd, secs=10):
        self.send(command(cmd, kw_cmd))
        iev = await self.until(("EV_MT_COMMAND_ANSWER",), secs)
        return iev["kw"] if iev else {}

    async def frames(self, secs, done=None):
        """Every frame within `secs`, or until done(frames)"""
        got = []
        end = time.monotonic() + secs
        while time.monotonic() < end:
            iev = await self.recv(end - time.monotonic())
            if iev is None:
                break
            got.append(iev)
            if done and done(got):
                break
        return got

    async def answers(self, secs):
        got = []
        end = time.monotonic() + secs
        while time.monotonic() < end:
            iev = await self.until(("EV_MT_COMMAND_ANSWER", "EV_MT_STATS_ANSWER"), end - time.monotonic())
            if iev is None:
                break
            got.append(iev["kw"])
        return got

    async def closed(self, secs):
        """TRUE when the agent ends the connection within `secs`"""
        end = time.monotonic() + secs
        while time.monotonic() < end:
            try:
                data = await asyncio.wait_for(self.r.read(65536), end - time.monotonic())
            except asyncio.TimeoutError:
                return False
            except ConnectionError:
                return True
            if not data:
                return True
        return False

    def close(self):
        self.w.close()


class Yuno(Peer):
    """The yuno: answers the readings and the publish-stats of the agent,
    holds the orders of the sessions until told"""

    def __init__(self):
        self.publish_asked = 0
        self.publish_stopped = 0
        self.held = []

    async def serve(self):
        while True:
            iev = await self.recv(3600)
            if iev is None:
                return
            event = iev.get("event")
            kw = iev.get("kw", {})
            md = kw.get("__md_iev__", {})
            if event == "EV_MT_STATS" and "agent_watch" in md:
                self.send(answer_of(iev, "EV_MT_STATS_ANSWER", 0, "",
                    {"reading": YUNO_READING}))
            elif event == "EV_MT_COMMAND" and kw.get("__command__") == "publish-stats":
                self.publish_asked += 1
                if kw.get("stop") in (True, 1, "1", "true"):
                    self.publish_stopped += 1
                self.send(answer_of(iev, "EV_MT_COMMAND_ANSWER", 0, "publishing"))
            elif event in ("EV_MT_COMMAND", "EV_MT_STATS"):
                self.held.append(iev)


async def session():
    o = await Peer().open()
    o.send(card("ycommand", os.getpid()))
    ack = await o.until(("EV_IDENTITY_CARD_ACK",), 10)
    return o if ack and ack["kw"].get("result") == 0 else None


async def readings(w, secs):
    got = []
    end = time.monotonic() + secs
    while time.monotonic() < end:
        iev = await w.until(("EV_YUNO_STATS",), end - time.monotonic())
        if iev is None:
            break
        got.append(iev["kw"])
    return got


async def wait_for(cond, secs):
    end = time.monotonic() + secs
    while time.monotonic() < end:
        if cond():
            return True
        await asyncio.sleep(0.05)
    return cond()


async def left_before_its_answer(op_keep, yuno, yid, order, kw_order, answer_event):
    """A session sends `order` and leaves; the next one takes its channel"""
    a = await session()
    if a is None:
        check("%s: a session that will leave" % order, False)
        return
    n = len(yuno.held)
    a.send(command(order, dict(kw_order, id=yid)))
    check("%s: the yuno got the order of the session that leaves" % order,
        await wait_for(lambda: len(yuno.held) == n + 1, 10))
    a.w.write(Peer.frame(0x8, struct.pack(">H", 1000)))
    check("%s: the agent ends the connection of the session that left" % order, await a.closed(10))
    a.close()
    b = await session()
    if b is None:
        check("%s: a session after it" % order, False)
        return
    b.send(command(order, dict(kw_order, id=yid)))
    check("%s: the yuno got the order of the second session" % order,
        await wait_for(lambda: len(yuno.held) == n + 2, 10))
    first, second = yuno.held[n], yuno.held[n + 1]
    ch1 = first["kw"]["__md_iev__"]["ievent_gate_stack"][0].get("src_service")
    ch2 = second["kw"]["__md_iev__"]["ievent_gate_stack"][0].get("src_service")
    check("%s: the second session took the channel of the first: %s, %s" % (order, ch1, ch2),
        bool(ch1) and ch1 == ch2)
    yuno.send(answer_of(first, answer_event, 0, "to the session that left"))
    yuno.send(answer_of(second, answer_event, 0, "to the second session"))
    got = await b.answers(2)
    check("%s: the second session gets its own answer, not the first one's: %s" % (
            order, [g.get("comment") for g in got]),
        len(got) == 1 and got[0].get("comment") == "to the second session")
    b.w.write(Peer.frame(0x8, struct.pack(">H", 1000)))
    check("%s: the agent ends the connection of the second session" % order, await b.closed(10))
    b.close()


def events_of(frames, event):
    return [f["kw"] for f in frames if f.get("event") == event]


def tty_text(frames):
    """What a console wrote, in the EV_TTY_DATA of `frames`"""
    text = b""
    for kw in events_of(frames, "EV_TTY_DATA"):
        content64 = (kw.get("data") or {}).get("content64") or ""
        text += base64.b64decode(content64)
    return text.decode(errors="replace")


async def requests_by_command(op, yid):
    """The agent's requests asked by their commands are answered, each with
    its own event (the WARNING of 4 is for the events a session sends)"""
    for cmd, kw, event in (
        ("read-file", {"filename": "/yuneta/agent/yuneta_agent.json"}, "EV_READ_FILE"),
        ("read-json", {"filename": "/yuneta/agent/yuneta_agent.json"}, "EV_READ_JSON"),
        ("running-keys", {"id": yid}, "EV_READ_FILE"),
        ("running-bin", {"id": yid}, "EV_READ_FILE"),
    ):
        op.send(command(cmd, kw))
        got = await op.frames(10, lambda g: g[-1].get("event") == event)
        answers = events_of(got, event)
        check("%s by its command: answered with %s: %s" % (cmd, event,
                [a.get("comment") for a in answers] or [g.get("event") for g in got]),
            len(answers) == 1 and answers[0].get("result") == 0 and answers[0].get("data"))


async def open_console(peer, name):
    """open-console of `name` (cat, so what is written comes back): its
    answer and the frames that came with it"""
    peer.send(command("open-console", {"name": name, "process": "cat", "cwd": "/yuneta"}))
    got = await peer.frames(5, lambda g: events_of(g, "EV_MT_COMMAND_ANSWER") and
        events_of(g, "EV_TTY_OPEN"))
    answer = (events_of(got, "EV_MT_COMMAND_ANSWER") or [{}])[0]
    return answer, got


async def leave(peer):
    peer.w.write(Peer.frame(0x8, struct.pack(">H", 1000)))
    await peer.closed(10)
    peer.close()


async def console_of_two_sessions():
    """A console opened by a session and joined by another: both get its
    open, what it writes, and its close. Up to 7.30.26 neither got any of
    them: the agent sent them to the channel, which takes no such event (an
    ERROR "Event NOT DEFINED in state" with a stack per frame), and the
    second one only got its own open again"""
    a = await session()
    b = await session()
    if a is None or b is None:
        check("console: two sessions", False)
        return
    answer, got_a = await open_console(a, "t1")
    check("console: open-console: %s" % answer.get("comment"), answer.get("result") == 0)
    check("console: its open reaches the session that opened it",
        [(k.get("data") or {}).get("name") for k in events_of(got_a, "EV_TTY_OPEN")] == ["t1"])
    answer, got_b = await open_console(b, "t1")
    check("console: open-console of it by another session: %s" % answer.get("comment"),
        answer.get("result") == 0)
    check("console: its open reaches the other session",
        [(k.get("data") or {}).get("name") for k in events_of(got_b, "EV_TTY_OPEN")] == ["t1"])

    b.send(command("write-tty", {"name": "t1",
        "content64": base64.b64encode(b"hello console\n").decode()}))
    got_a, got_b = await asyncio.gather(
        a.frames(5, lambda g: "hello console" in tty_text(g)),
        b.frames(5, lambda g: "hello console" in tty_text(g)),
    )
    check("console: what it writes reaches the session that opened it", "hello console" in tty_text(got_a))
    check("console: what it writes reaches the other session", "hello console" in tty_text(got_b))

    a.send(command("close-console", {"name": "t1"}))
    got_a, got_b = await asyncio.gather(
        a.frames(5, lambda g: events_of(g, "EV_TTY_CLOSE") and events_of(g, "EV_MT_COMMAND_ANSWER")),
        b.frames(5, lambda g: events_of(g, "EV_TTY_CLOSE")),
    )
    check("console: close-console: %s" % [k.get("comment") for k in events_of(got_a, "EV_MT_COMMAND_ANSWER")],
        [k.get("result") for k in events_of(got_a, "EV_MT_COMMAND_ANSWER")] == [0])
    check("console: its close reaches the session that opened it",
        [(k.get("data") or {}).get("name") for k in events_of(got_a, "EV_TTY_CLOSE")] == ["t1"])
    check("console: its close reaches the other session",
        [(k.get("data") or {}).get("name") for k in events_of(got_b, "EV_TTY_CLOSE")] == ["t1"])
    await leave(a)
    await leave(b)


async def console_of_a_session_that_leaves():
    """A console goes with the session that opened it: the channel is a
    pool, and its next session must not get what the console writes. Up to
    7.30.26 the agent kept the console of a session in the C_IEVENT_SRV of
    its channel and looked for it in the channel: it was never closed"""
    c = await session()
    w = await session()
    if c is None or w is None:
        check("console of a session that leaves: two sessions", False)
        return
    answer, got = await open_console(c, "t2")
    check("console of a session that leaves: open-console: %s" % answer.get("comment"),
        answer.get("result") == 0)
    r = await w.ask("list-consoles", {})
    check("console of a session that leaves: listed while its session is there",
        "t2" in (r.get("data") or {}))
    await leave(c)
    consoles = None
    for i in range(50):
        r = await w.ask("list-consoles", {})
        consoles = r.get("data") or {}
        if "t2" not in consoles:
            break
        await asyncio.sleep(0.1)
    check("console of a session that leaves: closed with it: %s" % list(consoles),
        "t2" not in consoles)
    await leave(w)


def agent_pid():
    """The agent of the sandbox (a pid namespace of its own)"""
    for d in os.listdir("/proc"):
        if d.isdigit():
            try:
                with open("/proc/%s/comm" % d) as f:
                    if f.read().strip() == "yuneta_agent":
                        return int(d)
            except OSError:
                pass
    return 0


async def main():
    op = await session()
    check("a session of ycommand on 1991", op is not None)
    if op is None:
        return

    #   The agent runs its enabled yunos once, some 3 s after it starts
    #   (timerStBoot is 0.1 s in agent.json): the yuno below is created
    #   after that, so that nobody but the cards here says it is running
    await asyncio.sleep(6)

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
    ):
        r = await op.ask(cmd, kw, secs=30)
        check(cmd, r.get("result") == 0)
    r = await op.ask("list-yunos", {"yuno_role": "logcenter"})
    rows = r.get("data") or []
    check("the logcenter is registered", len(rows) == 1)
    if len(rows) != 1:
        return
    yid = rows[0]["id"]
    realm_id = rows[0]["realm_id"][0] if isinstance(rows[0]["realm_id"], list) else rows[0]["realm_id"]

    #   Another yuno, whose link sends what only the first one may
    config64_y = base64.b64encode(json.dumps(
        {"__version__": "1", "__description__": "sandbox"}).encode()).decode()
    for cmd, kw in (
        ("create-config", {"id": "logcenter.y", "content64": config64_y}),
        ("create-yuno", {"realm_id": "w.prod.test", "yuno_role": "logcenter", "yuno_name": "y"}),
    ):
        r = await op.ask(cmd, kw, secs=30)
        check("%s of another yuno" % cmd, r.get("result") == 0)
    r = await op.ask("list-yunos", {"yuno_role": "logcenter", "yuno_name": "y"})
    rows_y = r.get("data") or []
    check("another logcenter is registered", len(rows_y) == 1)
    if len(rows_y) != 1:
        return

    yuno = await Yuno().open()
    yuno.send(card("logcenter", -1, yuno_id=yid, yuno_name="x",
        yuno_release=rows[0]["yuno_release"], realm_id=realm_id, watcher_pid=-1))
    card_ack = await yuno.until(("EV_IDENTITY_CARD_ACK",), 10)
    check("the yuno in session", bool(card_ack) and card_ack["kw"].get("result") == 0)
    if not card_ack:
        return
    other = await Yuno().open()
    other.send(card("logcenter", -1, yuno_id=rows_y[0]["id"], yuno_name="y",
        yuno_release=rows_y[0]["yuno_release"], realm_id=realm_id, watcher_pid=-1))
    card_ack = await other.until(("EV_IDENTITY_CARD_ACK",), 10)
    check("the other yuno in session", bool(card_ack) and card_ack["kw"].get("result") == 0)
    if not card_ack:
        return
    serving = asyncio.ensure_future(yuno.serve())
    serving_other = asyncio.ensure_future(other.serve())
    r = await op.ask("list-yunos", {"id": yid})
    pid = ((r.get("data") or [{}])[0]).get("yuno_pid", 0)

    #   1. Readings of a watch, and the answer to a publish-stats
    w = await session()
    r = await w.ask("watch-yuno-stats", {"ids": yid, "period": 1000, "__relays__": ["EV_YUNO_STATS"]})
    check("watch-yuno-stats: %s" % r.get("comment"), r.get("result") == 0)
    got = await readings(w, 3)
    check("the watcher gets the readings of the yuno",
        any((g.get("data") or {}).get("data", {}) == {"reading": YUNO_READING} for g in got))
    check("the agent asks the yuno to publish its stats",
        await wait_for(lambda: yuno.publish_asked > 0, 10))
    await asyncio.sleep(0.5)    # the agent took its answer: the publication is on

    f = await session()
    f.send(forged("EV_MT_STATS_ANSWER", "agent_watch",
        {"yuno_id": yid, "kind": "app", "service": "", "route": ""}, 0, {"forged": True}))
    f.send(forged("EV_MT_COMMAND_ANSWER", "agent_publish",
        {"yuno_id": yid, "service": "__yuno__", "pid": pid, "stop": False}, -1, None))
    #   The link of ANOTHER yuno names the first one: not its channel either
    other.send(forged("EV_MT_STATS_ANSWER", "agent_watch",
        {"yuno_id": yid, "kind": "app", "service": "", "route": ""}, 0, {"forged": "by another yuno"}))
    other.send(forged("EV_MT_COMMAND_ANSWER", "agent_publish",
        {"yuno_id": yid, "service": "__yuno__", "pid": pid, "stop": False}, -1, None))
    got = await readings(w, 2)
    check("the watcher gets no reading forged by a session",
        not any((g.get("data") or {}).get("data", {}) == {"forged": True} for g in got))
    check("the watcher gets no reading forged by another yuno",
        not any((g.get("data") or {}).get("data", {}) == {"forged": "by another yuno"} for g in got))

    #   4. A session sends where the agent keeps a pointer (`__temp__`, a
    #   key of the agent's own, which a peer's frame never carries): a
    #   reading, a publication and the acks of play / pause, with a
    #   `__temp__` of its own; and acks with none, which came from no yuno
    #   (up to 7.30.26 the agent took the peer's number for a gobj and died;
    #   an ack read its channel as a yuno's, ERRORs with a stack)
    g = await session()
    g.send({"event": "EV_MT_STATS_ANSWER", "kw": dict(forged("EV_MT_STATS_ANSWER", "agent_watch",
        {"yuno_id": yid, "kind": "app", "service": "", "route": ""}, 0, {"forged": True})["kw"],
        __temp__=[PEER_POINTER])})
    g.send({"event": "EV_MT_COMMAND_ANSWER", "kw": dict(forged("EV_MT_COMMAND_ANSWER", "agent_publish",
        {"yuno_id": yid, "service": "__yuno__", "pid": pid, "stop": False}, -1, None)["kw"],
        __temp__={"channel_gobj": PEER_POINTER})})
    for event in ("EV_PLAY_YUNO_ACK", "EV_PAUSE_YUNO_ACK"):
        g.send(ack(event, 0, temp=[PEER_POINTER]))
        g.send(ack(event, 0, temp={"channel_gobj": PEER_POINTER}))
        g.send(ack(event, 0))
        g.send(ack(event, -1))
    #   The events of a console (the agent takes them from its consoles)
    #   sent by a session, with a kw of any shape: dropped with ONE WARNING
    #   (a brake), no ERROR (up to 7.30.26 an ERROR "named dict not found"
    #   per frame)
    for i in range(10):
        for event in ("EV_TTY_OPEN", "EV_TTY_DATA", "EV_TTY_CLOSE"):
            g.send(peer_event(event, {"name": ["x"], "content64": 5}))
            g.send(peer_event(event, {}))
    #   And the requests of the agent that come by its commands (edit-config,
    #   read-file...), sent as events: dropped with ONE WARNING, no ERROR (up
    #   to 7.30.26 the agent answered them to the gate of its channels: an
    #   ERROR "Event NOT DEFINED in state" with a stack per frame)
    for event in ("EV_EDIT_CONFIG", "EV_VIEW_CONFIG", "EV_EDIT_YUNO_CONFIG", "EV_VIEW_YUNO_CONFIG",
            "EV_READ_JSON", "EV_READ_FILE", "EV_READ_BINARY_FILE", "EV_READ_RUNNING_KEYS",
            "EV_READ_RUNNING_BIN"):
        g.send(peer_event(event, {"id": ["x"], "filename": 5}))
        g.send(peer_event(event, {}))
    got = await readings(w, 1)
    check("the watcher gets no reading a session sent with a __temp__",
        not any((g_.get("data") or {}).get("data", {}) == {"forged": True} for g_ in got))
    r = await g.ask("help", {"cmd": "stats-yuno"})
    check("the agent lives after the frames with a __temp__ of a session, and the events of others",
        r.get("result") == 0)
    r = await w.ask("watch-yuno-stats", {"stop": 1})
    check("the watch stops", r.get("result") == 0)

    #   2. A session leaves before the answer of the yuno
    await left_before_its_answer(op, yuno, yid, "command-yuno", {"command": "help"},
        "EV_MT_COMMAND_ANSWER")
    await left_before_its_answer(op, yuno, yid, "stats-yuno", {},
        "EV_MT_STATS_ANSWER")
    await left_before_its_answer(op, yuno, yid, "authzs-yuno", {},
        "EV_MT_COMMAND_ANSWER")

    #   3. An answer of the yuno to a service of the agent that is no
    #   requester (neither a channel nor a link): dropped with a WARNING (up
    #   to 7.30.26 the service was sent the answer)
    yuno.send({"event": "EV_MT_COMMAND_ANSWER", "kw": {
        "result": 0, "comment": "to a service", "schema": None, "data": None,
        "__md_iev__": {
            "ievent_gate_stack": [{
                "dst_yuno": "", "dst_role": "yuneta_agent", "dst_service": "authz",
                "src_yuno": "x", "src_role": "logcenter", "src_service": "logcenter"}],
            "__msg_type__": "__answer__",
        }}})

    r = await op.ask("help", {"cmd": "stats-yuno"})
    check("the agent still answers", r.get("result") == 0)

    #   6. What the guards of 4 must let through: the agent's requests by
    #   their commands, and the events of a real console
    await requests_by_command(op, yid)
    await console_of_two_sessions()
    await console_of_a_session_that_leaves()

    #   5. The agent stops (SIGINT) with a watch still armed and the yuno
    #   publishing: the yuno is told to stop publishing, and the log holds
    #   no ERROR "hgobj NULL or DESTROYED" (up to 7.30.26 the stop asked for
    #   the yuno in the treedb its pause had closed: an ERROR with a stack,
    #   and the yuno was told nothing)
    w2 = await session()
    asked = yuno.publish_asked
    r = await w2.ask("watch-yuno-stats", {"ids": yid, "period": 1000, "__relays__": ["EV_YUNO_STATS"]})
    check("watch-yuno-stats before the stop: %s" % r.get("comment"), r.get("result") == 0)
    check("the agent asks the yuno to publish its stats again",
        await wait_for(lambda: yuno.publish_asked > asked, 10))
    await asyncio.sleep(0.5)    # the agent took its answer: the publication is on
    stopped = yuno.publish_stopped
    pid = agent_pid()
    check("the agent's pid: %d" % pid, pid > 0)
    if pid > 0:
        os.kill(pid, signal.SIGINT)
    check("the yuno is told to stop publishing when the agent stops",
        await wait_for(lambda: yuno.publish_stopped > stopped, 10))
    await wait_for(lambda: pid <= 0 or not os.path.exists("/proc/%d" % pid), 15)

    serving.cancel()
    serving_other.cancel()
    for p in (op, f, g, w, yuno, other):
        p.close()


asyncio.run(main())
if failures:
    print("%d check(s) failed" % len(failures))
    sys.exit(1)
sys.exit(0)
