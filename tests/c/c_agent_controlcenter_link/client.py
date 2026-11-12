#
#   The agent's link to the control center IN session: a control center
#   written in python on 127.0.0.1:19898, and operators on the agent's 1991
#   that send command-agent and stats-agent to service=controlcenter.
#
#   The control center is as little as it takes: TLS (the agent pins this
#   certificate; no client certificate is asked), tcp4h frames (a 4-byte
#   length in network order, the header included) holding ievents. It acks
#   the identity card of the agent's link and answers each request with its
#   routing reversed, as a C_IEVENT_SRV does: the answer goes to the
#   `src_service` the request came from. The operators are websocket
#   clients written by hand, so the test needs no package beyond python.
#
#   1. Each line is answered with what the control center said. Up to
#      7.30.26 the request went up with the operator's channel of the agent
#      ("input-0") as its `src_service`, which is no service: the link
#      dropped the answer ("service of the event not found, event dropped")
#      and the operator waited for its timeout.
#   2. A user with a password and a role for the service `agent` only
#      (bob) sends an EV_MT_COMMAND_ANSWER and an EV_MT_STATS_ANSWER to
#      `agent` naming the link in `__md_iev__.agent_requester`: the control
#      center gets neither (after 58647f485 the agent sent both on, with its
#      own identity). Then three answers to a `dst_service` that is no
#      requester, and the control center one to an `agent_requester` that
#      is none: each is dropped with a WARNING of the agent (with a brake),
#      not an ERROR "child not found".
#   3. An operator asks the control center and leaves before the answer;
#      another session takes its channel of the agent (the same `input-N`)
#      and asks too: it gets its own answer, never the first one's.
#
#   Usage: client.py <cert> <key> <port>
#   Exit 0 when every check passed.
#
#   Copyright (c) 2026, ArtGins.
#   All Rights Reserved.
#
import asyncio
import base64
import json
import os
import ssl
import struct
import sys
import time

HOST = "127.0.0.1"
PORT = 1991
CC_ANSWER = "the control center answers"
HELD_ANSWER = "the answer of the session that left"
FORGED = "forged by a session"
failures = []
requests = []           # (event, src_service, agent_requester) the control center got
answers_up = []         # answers that reached the control center from the agent
held = []               # the routing of a request whose answer waits
cc_writer = []


def check(what, ok):
    print(("ok    " if ok else "FAIL  ") + what, flush=True)
    if not ok:
        failures.append(what)


def reverse(md):
    """The routing of an answer: the request's, its source made its destination"""
    md = json.loads(json.dumps(md))
    top = md["ievent_gate_stack"][0]
    top["dst_yuno"], top["dst_role"], top["dst_service"] = (
        top.get("src_yuno", ""), top.get("src_role", ""), top.get("src_service", ""))
    top["src_yuno"], top["src_role"], top["src_service"] = "cc", "controlcenter", "controlcenter"
    md["__msg_type__"] = "__answer__"
    return md


def tcp4h(iev):
    b = json.dumps(iev).encode()
    return struct.pack("!I", len(b) + 4) + b


def cc_answer(event, md, comment):
    answer = "EV_MT_COMMAND_ANSWER" if event == "EV_MT_COMMAND" else "EV_MT_STATS_ANSWER"
    return tcp4h({"event": answer, "kw": {"result": 0, "comment": comment,
        "schema": None, "data": None, "__md_iev__": reverse(md)}})


async def control_center(r, w, in_session):
    cc_writer.append(w)
    while True:
        try:
            n = struct.unpack("!I", await r.readexactly(4))[0] - 4
            iev = json.loads(await r.readexactly(n))
        except (asyncio.IncompleteReadError, ConnectionError, ssl.SSLError):
            return
        event = iev.get("event")
        md = iev.get("kw", {}).get("__md_iev__", {})
        if event == "EV_IDENTITY_CARD":
            w.write(tcp4h({"event": "EV_IDENTITY_CARD_ACK",
                "kw": {"result": 0, "comment": "", "data": {}, "__md_iev__": reverse(md)}}))
            in_session.set()
        elif event in ("EV_MT_COMMAND", "EV_MT_STATS"):
            top = md.get("ievent_gate_stack", [{}])[0]
            requests.append((event, top.get("src_service"), md.get("agent_requester")))
            if "hold-me" in iev["kw"].get("__command__", ""):
                held.append((event, md))
                continue
            while held:
                h_event, h_md = held.pop(0)
                w.write(cc_answer(h_event, h_md, HELD_ANSWER))
            w.write(cc_answer(event, md, CC_ANSWER))
        else:
            answers_up.append((event, iev.get("kw", {}).get("comment")))


def routing(src_role, dst_service="agent"):
    return {
        "dst_yuno": "", "dst_role": "yuneta_agent", "dst_service": dst_service,
        "src_yuno": "", "src_role": src_role, "src_service": "cli",
        "user": "yuneta", "host": "sandbox",
    }


def card(username="yuneta", password=""):
    kw = {
        "yuno_role": "ycommand", "yuno_id": "", "yuno_name": "x", "yuno_tag": "",
        "yuno_version": "1", "yuno_release": "1", "yuneta_version": "",
        "playing": False, "pid": os.getpid(), "watcher_pid": 0,
        "jwt": "", "username": username, "launch_id": 0, "yuno_startdate": "",
        "id": "sandbox", "realm_id": "", "required_services": [],
        "__md_iev__": {
            "ievent_gate_stack": [routing("ycommand")],
            "__msg_type__": "__identity__",
        },
    }
    if password:
        kw["password"] = password
    return {"event": "EV_IDENTITY_CARD", "kw": kw}


def command(line, dst_service="agent"):
    return {"event": "EV_MT_COMMAND", "kw": {
        "__command__": line,
        "__md_iev__": {
            "ievent_gate_stack": [routing("ycommand", dst_service)],
            "__msg_type__": "__command__",
            "command_stack": [{"command": line, "kw": {}}],
        },
    }}


def forged_answer(event, dst_service, agent_requester=None):
    md = {
        "ievent_gate_stack": [routing("ycommand", dst_service)],
        "__msg_type__": "__answer__",
    }
    if agent_requester is not None:
        md["agent_requester"] = agent_requester
    return {"event": event, "kw": {"result": 0, "comment": FORGED,
        "schema": None, "data": None, "__md_iev__": md}}


class Peer:
    """A websocket client written by hand: no third-party package"""

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

    async def answers(self, secs):
        """Every answer (command or stats) that comes within `secs`"""
        got = []
        end = time.monotonic() + secs
        while time.monotonic() < end:
            iev = await self.recv(end - time.monotonic())
            if iev is None:
                break
            if iev.get("event") in ("EV_MT_COMMAND_ANSWER", "EV_MT_STATS_ANSWER"):
                got.append(iev["kw"])
        return got

    async def ask(self, line, secs=5, dst_service="agent"):
        self.send(command(line, dst_service))
        end = time.monotonic() + secs
        while time.monotonic() < end:
            iev = await self.recv(end - time.monotonic())
            if iev is None:
                return None
            if iev.get("event") in ("EV_MT_COMMAND_ANSWER", "EV_MT_STATS_ANSWER"):
                return iev["kw"]
        return None

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


async def session(username="yuneta", password=""):
    p = await Peer().open()
    p.send(card(username, password))
    end = time.monotonic() + 10
    while time.monotonic() < end:
        iev = await p.recv(end - time.monotonic())
        if iev is None:
            break
        if iev.get("event") == "EV_IDENTITY_CARD_ACK":
            return p if iev["kw"].get("result") == 0 else None
    return None


async def main():
    ctx = ssl.SSLContext(ssl.PROTOCOL_TLS_SERVER)
    ctx.load_cert_chain(sys.argv[1], sys.argv[2])
    in_session = asyncio.Event()
    server = await asyncio.start_server(
        lambda r, w: control_center(r, w, in_session), "127.0.0.1", int(sys.argv[3]), ssl=ctx)

    op = await session()
    check("a session of ycommand on 1991", op is not None)
    if op is None:
        return

    #   The link connects at once, or after its timeout_between_connections
    try:
        await asyncio.wait_for(in_session.wait(), 30)
    except asyncio.TimeoutError:
        pass
    check("the agent's link to the control center in session", in_session.is_set())
    if not in_session.is_set():
        return
    await asyncio.sleep(0.5)    # the agent took the ack

    #   1. Answered with what the control center said
    for line in ("command-agent service=controlcenter command=list-yunos",
                 "stats-agent service=controlcenter"):
        r = await op.ask(line)
        check("%s: answered with what the control center said: %s" % (
                line, (r or {}).get("comment")),
            bool(r) and r.get("result") == 0 and r.get("comment") == CC_ANSWER)
    print("requests the control center got (event, src_service, agent_requester): %s" % requests,
        flush=True)
    check("the control center got both requests", len(requests) == 2)

    #   2. Answers a session sends to the agent
    r = await op.ask("command-agent service=authz command=create-user"
        " username=bob role=roles^operator^users password=bob-secret")
    check("create-user bob: %s" % (r or {}).get("comment"), bool(r) and r.get("result") == 0)
    bob = await session("bob", "bob-secret")
    check("a session of bob (a password, a role for the service agent only)", bob is not None)
    if bob is None:
        return
    bob.send(forged_answer("EV_MT_COMMAND_ANSWER", "agent", "controlcenter"))
    bob.send(forged_answer("EV_MT_STATS_ANSWER", "agent", "controlcenter"))
    bob.send(forged_answer("EV_MT_COMMAND_ANSWER", "nosuch"))
    bob.send(forged_answer("EV_MT_COMMAND_ANSWER", "nosuch"))
    bob.send(forged_answer("EV_MT_STATS_ANSWER", "nosuch"))
    cc_writer[-1].write(tcp4h({"event": "EV_MT_COMMAND_ANSWER", "kw": {
        "result": 0, "comment": "to nobody", "schema": None, "data": None,
        "__md_iev__": {
            "ievent_gate_stack": [{
                "dst_yuno": "", "dst_role": "yuneta_agent", "dst_service": "agent",
                "src_yuno": "cc", "src_role": "controlcenter", "src_service": "controlcenter"}],
            "__msg_type__": "__answer__",
            "agent_requester": "nosuch",
        }}}))
    r = await bob.ask("help cmd=list-yunos")
    check("the agent still answers bob", bool(r) and r.get("result") == 0)
    await asyncio.sleep(0.5)
    check("the control center got none of bob's answers: %s" % answers_up,
        not any(c == FORGED for e, c in answers_up))

    #   3. A session leaves before its answer; another takes its channel
    a = await session()
    check("a session that will leave", a is not None)
    if a is None:
        return
    n = len(requests)
    a.send(command("command-agent service=controlcenter command=hold-me"))
    end = time.monotonic() + 5
    while len(requests) == n and time.monotonic() < end:
        await asyncio.sleep(0.05)
    check("the control center holds the answer of the session that leaves", len(held) == 1)
    a.w.write(Peer.frame(0x8, struct.pack(">H", 1000)))
    check("the agent ends the connection of the session that left", await a.closed(10))
    a.close()
    b = await session()
    check("a session after it", b is not None)
    if b is None:
        return
    b.send(command("command-agent service=controlcenter command=list-yunos"))
    got = await b.answers(2)
    first, second = requests[-2][2], requests[-1][2]
    check("the second session took the channel of the first: %s, %s" % (first, second),
        bool(first) and first == second)
    check("the second session gets its own answer, not the first one's: %s" % (
            [g.get("comment") for g in got]),
        len(got) == 1 and got[0].get("comment") == CC_ANSWER)

    r = await op.ask("help cmd=command-agent")
    check("the agent still answers", bool(r) and r.get("result") == 0)
    for p in (op, bob, b):
        p.close()
    server.close()


asyncio.run(main())
if failures:
    print("%d check(s) failed" % len(failures))
    sys.exit(1)
sys.exit(0)
