#
#   Peers of the agent's 1991: a peer that closes while an order goes to it
#
#   Run by run_test.sh inside the sandbox, with the pid of the agent. The
#   agent is stopped (SIGSTOP) while the frames are written and continued
#   (SIGCONT) after, so it reads all of them in ONE turn of its loop. A
#   WebSocket Close puts the peer's C_WEBSOCKET out of its session at once,
#   and its C_IEVENT_SRV leaves it only when the TCP connection ends, on a
#   later turn: in between, up to 7.30.26, what was sent to the peer was
#   "Event NOT DEFINED in state" (an ERROR with a stack) and lost. The order
#   in which the agent reads its sockets in that turn is the kernel's, so
#   each case writes from two peers, connected before and after the one
#   that closes, one before its Close and one after: one of them always
#   lands in the window.
#
#   1. A yuno (a card of a logcenter registered and not run) closes in four
#      ways while two operators send stats-yuno and command-yuno to it: a
#      Close, a Close and the end of the connection, a continuation frame
#      with no message begun and a Ping of 126 bytes (both a protocol
#      error, which the agent's websocket answers with its Close). The pair
#      that reaches the yuno closing is refused at once by the yuno's
#      channel (-1, "not in session", a WARNING each); the other went to
#      the yuno, which never answers.
#   2. Two operators close while the answers to their command-yuno come
#      back from the yuno: the answer to the one that closed first has
#      nobody to go to, and is dropped.
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
AGENT_PID = int(sys.argv[1])
YUNO_BINARY = sys.argv[2]
CYCLES = 3
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


def answer_of(iev, answer_event):
    """What a yuno's C_IEVENT_CLI answers to a request: its routing reversed"""
    md = json.loads(json.dumps(iev["kw"].get("__md_iev__", {})))
    stack = md.get("ievent_gate_stack") or [{}]
    top = stack[0]
    top["dst_yuno"], top["dst_role"], top["dst_service"] = (
        top.get("src_yuno", ""), top.get("src_role", ""), top.get("src_service", ""))
    top["src_yuno"], top["src_role"], top["src_service"] = "x", "logcenter", "logcenter"
    md["__msg_type__"] = "__answer__"
    return {"event": answer_event, "kw": {
        "result": 0, "comment": "fake yuno", "schema": None, "data": None, "__md_iev__": md}}


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
    def frame(opcode, payload, fin=True):
        n = len(payload)
        b0 = (0x80 if fin else 0) | opcode
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

    def write(self, data):
        self.w.write(data)

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

    async def until(self, event, secs):
        end = time.monotonic() + secs
        while time.monotonic() < end:
            iev = await self.recv(end - time.monotonic())
            if iev is None:
                return None
            if iev.get("event") == event:
                return iev
        return None

    async def ask(self, cmd, kw_cmd, secs=10):
        self.send(command(cmd, kw_cmd))
        iev = await self.until("EV_MT_COMMAND_ANSWER", secs)
        return iev["kw"] if iev else {}

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


CLOSES = (
    ("a Close", Peer.frame(0x8, struct.pack(">H", 1000)), False),
    ("a Close and the end of the connection", Peer.frame(0x8, struct.pack(">H", 1000)), True),
    ("a continuation frame with no message begun", Peer.frame(0x0, b"x"), False),
    ("a Ping of 126 bytes", Peer.frame(0x9, b"p" * 126), False),
)


async def in_one_turn(*writes):
    """The agent reads all of `writes` in one turn of its loop"""
    os.kill(AGENT_PID, signal.SIGSTOP)
    try:
        for w in writes:
            w()
        await asyncio.sleep(0.2)    # loopback: in the agent's sockets by now
    finally:
        os.kill(AGENT_PID, signal.SIGCONT)


async def yuno_session(yuno, realm_id):
    p = await Peer().open()
    p.send(card("logcenter", -1, yuno_id=yuno["id"], yuno_name="x",
        yuno_release=yuno["yuno_release"], realm_id=realm_id, watcher_pid=-1))
    ack = await p.until("EV_IDENTITY_CARD_ACK", 10)
    return p, (ack or {}).get("kw", {}).get("result") == 0


async def operator():
    o = await Peer().open()
    o.send(card("ycommand", os.getpid()))
    ack = await o.until("EV_IDENTITY_CARD_ACK", 10)
    return o if ack and ack["kw"].get("result") == 0 else None


async def answers_of(o, secs):
    got = []
    end = time.monotonic() + secs
    while time.monotonic() < end:
        iev = await o.until("EV_MT_COMMAND_ANSWER", end - time.monotonic())
        if iev is None:
            break
        got.append(iev["kw"])
    return got


async def not_running(op, yid, secs=10):
    end = time.monotonic() + secs
    r = {}
    while time.monotonic() < end:
        r = await op.ask("list-yunos", {"id": yid})
        rows = r.get("data") or []
        if rows and not rows[0].get("yuno_running"):
            return True
        await asyncio.sleep(0.05)
    print("list-yunos id=%s: %s" % (yid, json.dumps(r.get("data"))[:900]))
    return False


async def main():
    op = await Peer().open()
    op.send(card("ycommand", os.getpid()))
    ack = await op.until("EV_IDENTITY_CARD_ACK", 10)
    check("a session of ycommand on 1991", bool(ack) and ack["kw"].get("result") == 0)

    #   The agent runs its enabled yunos once, some 3 s after it starts
    #   (timerStBoot is 0.1 s in agent.json): the yuno below is created
    #   after that, so that nobody but the cards here says it is running
    await asyncio.sleep(6)

    #   A yuno registered and not run: the cards below say they are it
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
    check("the logcenter is registered and not running", len(rows) == 1 and not rows[0].get("yuno_running"))
    if len(rows) != 1:
        return
    yuno = rows[0]
    yid = yuno["id"]
    realm_id = yuno["realm_id"][0] if isinstance(yuno["realm_id"], list) else yuno["realm_id"]

    #   1. The yuno closes while the operator's orders go to it.
    #   Two operators send them, one connected before the yuno and one
    #   after, their orders written before and after the Close: whether
    #   the agent reads its sockets in the order they were written, in the
    #   reverse one, or by descriptor, one pair of orders goes to the yuno
    #   (which never answers it) and the other to the yuno closing, which
    #   must be answered
    for cycle in range(CYCLES):
        for how, close_frame, end_too in CLOSES:
            p, accepted = await yuno_session(yuno, realm_id)
            op_after = await operator()
            if not accepted or op_after is None:
                check("cycle %d, %s: the yuno and the second operator in session" % (cycle, how), False)
                return

            def close_it():
                p.write(close_frame)
                if end_too:
                    p.w.write_eof()
            await in_one_turn(
                lambda: op.send(command("stats-yuno", {"id": yid})),
                lambda: op.send(command("command-yuno", {"id": yid, "command": "help"})),
                close_it,
                lambda: op_after.send(command("stats-yuno", {"id": yid})),
                lambda: op_after.send(command("command-yuno", {"id": yid, "command": "help"})),
            )
            a, b = await asyncio.gather(answers_of(op, 1.5), answers_of(op_after, 1.5))
            got = a + b
            check("cycle %d, %s: the orders to the yuno closing refused: %s" % (
                    cycle, how, [(a.get("result"), a.get("comment")) for a in got]),
                all(a.get("result") == -1 for a in got))
            check("cycle %d, %s: the agent ends the connection" % (cycle, how), await p.closed(10))
            p.close()
            op_after.close()
            if not await not_running(op, yid):
                check("cycle %d, %s: the yuno is not running for the agent" % (cycle, how), False)
                return

    #   2. An operator closes while the answer to its command comes back:
    #   two of them, connected before and after the yuno, their Closes
    #   written before and after the answers, for the same reason
    for cycle in range(CYCLES):
        op_before = await operator()
        p, accepted = await yuno_session(yuno, realm_id)
        op_after = await operator()
        if not accepted or op_before is None or op_after is None:
            check("cycle %d: the yuno and two operators in session" % cycle, False)
            return
        op_before.send(command("command-yuno", {"id": yid, "command": "help"}))
        op_after.send(command("command-yuno", {"id": yid, "command": "help"}))
        reqs = [await p.until("EV_MT_COMMAND", 10), await p.until("EV_MT_COMMAND", 10)]
        check("cycle %d: the commands of the two operators reach the yuno" % cycle, None not in reqs)
        if None in reqs:
            return
        await in_one_turn(
            lambda: op_before.write(Peer.frame(0x8, struct.pack(">H", 1000))),
            lambda: p.send(answer_of(reqs[0], "EV_MT_COMMAND_ANSWER")),
            lambda: p.send(answer_of(reqs[1], "EV_MT_COMMAND_ANSWER")),
            lambda: op_after.write(Peer.frame(0x8, struct.pack(">H", 1000))),
        )
        check("cycle %d: the agent ends the connections of the two operators" % cycle,
            await op_before.closed(10) and await op_after.closed(10))
        op_before.close()
        op_after.close()
        p.close()
        if not await not_running(op, yid):
            check("cycle %d: the yuno is not running for the agent" % cycle, False)
            return

    r = await op.ask("help", {"cmd": "stats-yuno"})
    check("the agent still answers", r.get("result") == 0)
    op.close()


asyncio.run(main())
if failures:
    print("%d check(s) failed" % len(failures))
    sys.exit(1)
sys.exit(0)
