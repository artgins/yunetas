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
#   A console the control center opens through the link (open-console,
#   write-tty, close-console), and a read-file: their open, output, close
#   and answer go up the link, as they did before the agent sent its
#   console events with EV_SEND_IEV.
#   A console the control center opens that floods (`yes`) while the
#   control center does not read: the console pauses for the link, the
#   agent's memory stays bounded, and the link, which takes nothing within
#   `console_pause_timeout` (3 s in agent.json), is taken off: the console
#   is closed, the link opened it (ONE WARNING). When the control center
#   reads again, what it had waiting ends with the console's EV_TTY_CLOSE.
#   Up to 7.30.26 the agent grew until it aborted.
#   4. The control center sends EV_PLAY_YUNO, then an order, EV_PAUSE_YUNO
#      and two bytes of the next order in one write, then 40 orders more:
#      both are refused (ack -1), every order is answered, the link and the
#      1991 stay. Up to 7.30.26 the pause stopped the link inside its own
#      action: SIGSEGV, or an agent nobody could reach any more.
#   A console the control center opens writes while the link is down: the
#   agent says it once (an INFO "requester gone"), not per chunk as up to
#   7.30.26, and the console writes up the link again when it reconnects.
#   5. The control center sends orders while the agent stops (SIGINT).
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
import signal
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
cc_reading = asyncio.Event()    # cleared: the control center does not read
RSS_CAP_MB = 300        # a run that grows more is a failure, and stops there
RSS_BOUND_MB = 160      # what the agent may grow while the control center does not read
flood_answers = {}      # cc_order -> result of the orders the control center sends
pause_answers = {}      # cc_pause_order -> result of the orders sent around a pause
play_pause_acks = []    # (event, result, comment) of the agent's acks of a play / pause
link_frames = []        # (event, kw) of what comes back of a console or a request of the control center
FLOOD = 300             # orders the control center sends while the agent stops
PEER_POINTER = 1094795585     # 0x41414141: what a peer writes where the agent keeps a pointer


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
        await cc_reading.wait()
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
            if "hold-me" in (iev["kw"].get("__command__") or iev["kw"].get("__stats__") or ""):
                held.append((event, md))
                continue
            while held:
                h_event, h_md = held.pop(0)
                w.write(cc_answer(h_event, h_md, HELD_ANSWER))
            w.write(cc_answer(event, md, CC_ANSWER))
        elif "cc_order" in md:
            flood_answers[md["cc_order"]] = iev.get("kw", {}).get("result")
        elif "cc_pause_order" in md:
            pause_answers[md["cc_pause_order"]] = iev.get("kw", {}).get("result")
        elif "cc_link_order" in md:
            link_frames.append((event, iev.get("kw", {})))
        elif event in ("EV_PLAY_YUNO_ACK", "EV_PAUSE_YUNO_ACK"):
            kw = iev.get("kw", {})
            play_pause_acks.append((event, kw.get("result"), kw.get("comment")))
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


def forged_answer(event, dst_service, agent_requester=None, temp=None):
    md = {
        "ievent_gate_stack": [routing("ycommand", dst_service)],
        "__msg_type__": "__answer__",
    }
    if agent_requester is not None:
        md["agent_requester"] = agent_requester
    kw = {"result": 0, "comment": FORGED, "schema": None, "data": None, "__md_iev__": md}
    if temp is not None:
        kw["__temp__"] = temp
    return {"event": event, "kw": kw}


def cc_frame(event, md_extra=None, temp=None, result=0):
    """What the control center sends to the agent (dst_service `agent`)"""
    md = {
        "ievent_gate_stack": [{
            "dst_yuno": "", "dst_role": "yuneta_agent", "dst_service": "agent",
            "src_yuno": "cc", "src_role": "controlcenter", "src_service": "controlcenter"}],
        "__msg_type__": "__answer__",
    }
    md.update(md_extra or {})
    kw = {"result": result, "comment": "of the control center", "schema": None, "data": None,
        "__md_iev__": md}
    if temp is not None:
        kw["__temp__"] = temp
    return tcp4h({"event": event, "kw": kw})


def cc_order(n, mark="cc_order"):
    """An order of the control center to the agent"""
    return tcp4h({"event": "EV_MT_COMMAND", "kw": {
        "__command__": "list-yunos",
        "__md_iev__": {
            "ievent_gate_stack": [{
                "dst_yuno": "", "dst_role": "yuneta_agent", "dst_service": "agent",
                "src_yuno": "cc", "src_role": "controlcenter", "src_service": "controlcenter",
                "user": "yuneta", "host": "cc"}],
            "__msg_type__": "__command__",
            mark: n,
        }}})


def cc_command(cmd, kw_cmd):
    """A command of the control center to the agent, with its parameters
    in the kw; what comes back of it carries `cc_link_order`"""
    kw = {
        "__command__": cmd,
        "__md_iev__": {
            "ievent_gate_stack": [{
                "dst_yuno": "", "dst_role": "yuneta_agent", "dst_service": "agent",
                "src_yuno": "cc", "src_role": "controlcenter", "src_service": "controlcenter",
                "user": "yuneta", "host": "cc"}],
            "__msg_type__": "__command__",
            "cc_link_order": cmd,
        }}
    kw.update(kw_cmd)
    return tcp4h({"event": "EV_MT_COMMAND", "kw": kw})


async def link_frames_until(done, secs):
    end = time.monotonic() + secs
    while not done() and time.monotonic() < end:
        await asyncio.sleep(0.02)
    return done()


def link_events(event):
    return [kw for e, kw in link_frames if e == event]


def cc_play_pause(event):
    """EV_PLAY_YUNO / EV_PAUSE_YUNO of the control center: an event the
    link takes itself (a public event of every C_IEVENT_CLI)"""
    return tcp4h({"event": event, "kw": {
        "__md_iev__": {
            "ievent_gate_stack": [{
                "dst_yuno": "", "dst_role": "yuneta_agent", "dst_service": "agent",
                "src_yuno": "cc", "src_role": "controlcenter", "src_service": "controlcenter",
                "user": "yuneta", "host": "cc"}],
            "__msg_type__": "__message__",
        }}})


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


def rss_mb(pid):
    try:
        with open("/proc/%d/status" % pid) as f:
            for line in f:
                if line.startswith("VmRSS:"):
                    return int(line.split()[1]) // 1024
    except OSError:
        pass
    return -1


def data_name(kw):
    data = kw.get("data")
    return data.get("name") if isinstance(data, dict) else None


def tty_events(event, name):
    return [kw for kw in link_events(event) if data_name(kw) == name]


async def flood_not_read_by_the_link(op):
    """A console of the control center floods (`yes`) and the control
    center does not read"""
    pid = agent_pid()
    w = cc_writer[-1]
    rss0 = rss_mb(pid)
    w.write(cc_command("open-console", {"name": "t4", "process": "yes", "cwd": "/yuneta"}))
    await w.drain()
    cc_reading.clear()
    peak = rss0
    gone = False
    t0 = time.monotonic()
    while time.monotonic() - t0 < 12:
        await asyncio.sleep(0.25)
        rss = rss_mb(pid)
        if rss < 0:
            break
        peak = max(peak, rss)
        if peak - rss0 > RSS_CAP_MB:
            break
        if time.monotonic() - t0 > 3 and not gone:
            r = (await op.ask("list-consoles")) or {}
            gone = "t4" not in (r.get("data") or {})
        if gone and time.monotonic() - t0 > 5:
            break
    print("flood by the link: the agent from %d to %d MB (peak)" % (rss0, peak), flush=True)
    if not gone:
        w.write(cc_command("close-console", {"name": "t4"}))     # the run must end
        await w.drain()
    cc_reading.set()
    check("flood by the link: the agent lives",
        rss_mb(pid) > 0 and ((await op.ask("help cmd=list-yunos")) or {}).get("result") == 0)
    check("flood by the link: the agent's memory stays bounded: +%d MB (bound %d MB)" % (
            peak - rss0, RSS_BOUND_MB),
        peak - rss0 < RSS_BOUND_MB)
    check("flood by the link: the console is closed for the link that opened it", gone)
    await link_frames_until(lambda: tty_events("EV_TTY_CLOSE", "t4"), 30)
    t4 = [(e, kw) for e, kw in link_frames if data_name(kw) == "t4"]
    check("flood by the link: what the link had waiting ends with the close of the console: %s" % (
            [e for e, kw in t4[-1:]]),
        bool(t4) and t4[-1][0] == "EV_TTY_CLOSE" and len(tty_events("EV_TTY_CLOSE", "t4")) == 1)


async def console_while_the_link_is_down(in_session):
    """A console of the control center writes while its link is down"""
    w = cc_writer[-1]
    w.write(cc_command("open-console", {"name": "t5", "process": "bash", "cwd": "/yuneta"}))
    w.write(cc_command("write-tty", {"name": "t5", "content64": base64.b64encode(
        b"while true; do echo TICK; sleep 0.01; done\n").decode()}))
    await link_frames_until(lambda: "TICK" in b"".join(
        base64.b64decode((kw.get("data") or {}).get("content64") or "")
        for kw in tty_events("EV_TTY_DATA", "t5")).decode(errors="replace"), 5)
    in_session.clear()
    n_links = len(cc_writer)
    w.close()
    await asyncio.sleep(1.5)
    #   The link connects again after its timeout_between_connections (10 s)
    try:
        await asyncio.wait_for(in_session.wait(), 30)
    except asyncio.TimeoutError:
        pass
    check("console while the link is down: the link connects again",
        in_session.is_set() and len(cc_writer) == n_links + 1)
    n_data = len(tty_events("EV_TTY_DATA", "t5"))
    check("console while the link is down: it writes up the link again",
        await link_frames_until(lambda: len(tty_events("EV_TTY_DATA", "t5")) > n_data, 5))
    w = cc_writer[-1]
    w.write(cc_command("close-console", {"name": "t5"}))
    check("console while the link is down: its close goes up the link",
        await link_frames_until(lambda: tty_events("EV_TTY_CLOSE", "t5"), 5))


async def main():
    ctx = ssl.SSLContext(ssl.PROTOCOL_TLS_SERVER)
    ctx.load_cert_chain(sys.argv[1], sys.argv[2])
    cc_reading.set()
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
    #   The first of each kind carries a `__temp__` of bob's own, where the
    #   agent keeps a pointer: the WARNING of the first reads it (up to
    #   7.30.26 the agent took bob's number for a gobj and died)
    bob.send(forged_answer("EV_MT_COMMAND_ANSWER", "agent", "controlcenter", temp=[PEER_POINTER]))
    bob.send(forged_answer("EV_MT_STATS_ANSWER", "agent", "controlcenter"))
    bob.send(forged_answer("EV_MT_COMMAND_ANSWER", "nosuch", temp=[PEER_POINTER]))
    bob.send(forged_answer("EV_MT_COMMAND_ANSWER", "nosuch"))
    bob.send(forged_answer("EV_MT_STATS_ANSWER", "nosuch"))
    #   An empty dst_service names no requester either (up to 7.30.26 an
    #   ERROR "name NULL" with a stack per answer)
    bob.send(forged_answer("EV_MT_COMMAND_ANSWER", ""))
    bob.send(forged_answer("EV_MT_STATS_ANSWER", ""))
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

    #   The control center sends what only a yuno sends (the acks of play /
    #   pause: its link has no channel, so they came from no yuno), and
    #   readings and publications with a `__temp__` of its own (up to
    #   7.30.26 the agent took its number for a gobj and died; an ack with
    #   no `__temp__` gave ERRORs with a stack)
    for event in ("EV_PLAY_YUNO_ACK", "EV_PAUSE_YUNO_ACK"):
        cc_writer[-1].write(cc_frame(event))
        cc_writer[-1].write(cc_frame(event, result=-1))
        cc_writer[-1].write(cc_frame(event, temp=[PEER_POINTER]))
    cc_writer[-1].write(cc_frame("EV_MT_STATS_ANSWER",
        {"agent_watch": {"yuno_id": "1", "kind": "app", "service": "", "route": ""}},
        temp=[PEER_POINTER]))
    cc_writer[-1].write(cc_frame("EV_MT_COMMAND_ANSWER",
        {"agent_publish": {"yuno_id": "1", "service": "__yuno__", "pid": 1, "stop": False}},
        temp={"channel_gobj": PEER_POINTER}))
    r = await bob.ask("help cmd=list-yunos")
    check("the agent lives after the frames with a __temp__ of the control center",
        bool(r) and r.get("result") == 0)

    #   Bob answers to its own channel with a single hop: the agent pops its
    #   own, and has no route back (up to 7.30.26 a WARNING and six ERRORs
    #   per answer, and the answer sent with no route)
    n = len(requests)
    r = await bob.ask("command-agent service=controlcenter command=list-yunos")
    check("command-agent of bob answered", bool(r) and r.get("result") == 0)
    bob_channel = requests[-1][2] if len(requests) > n else None
    check("the channel of bob: %s" % bob_channel, bool(bob_channel))
    bob.send(forged_answer("EV_MT_COMMAND_ANSWER", bob_channel or "nosuch"))
    bob.send(forged_answer("EV_MT_STATS_ANSWER", bob_channel or "nosuch"))
    r = await bob.ask("help cmd=list-yunos")
    check("the agent still answers bob after its one-hop answers", bool(r) and r.get("result") == 0)

    #   3. A session leaves before its answer; another takes its channel:
    #   command-agent, and stats-agent
    for held_line, line in (
        ("command-agent service=controlcenter command=hold-me",
            "command-agent service=controlcenter command=list-yunos"),
        ("stats-agent service=controlcenter stats=hold-me",
            "stats-agent service=controlcenter"),
    ):
        a = await session()
        check("%s: a session that will leave" % held_line, a is not None)
        if a is None:
            return
        n = len(requests)
        n_held = len(held)
        a.send(command(held_line))
        end = time.monotonic() + 5
        while len(requests) == n and time.monotonic() < end:
            await asyncio.sleep(0.05)
        check("%s: the control center holds the answer of the session that leaves" % held_line,
            len(held) == n_held + 1)
        a.w.write(Peer.frame(0x8, struct.pack(">H", 1000)))
        check("%s: the agent ends the connection of the session that left" % held_line,
            await a.closed(10))
        a.close()
        b = await session()
        check("%s: a session after it" % held_line, b is not None)
        if b is None:
            return
        b.send(command(line))
        got = await b.answers(2)
        first, second = requests[-2][2], requests[-1][2]
        check("%s: the second session took the channel of the first: %s, %s" % (
                held_line, first, second),
            bool(first) and first == second)
        check("%s: the second session gets its own answer, not the first one's: %s" % (
                held_line, [g.get("comment") for g in got]),
            len(got) == 1 and got[0].get("comment") == CC_ANSWER)
        b.w.write(Peer.frame(0x8, struct.pack(">H", 1000)))
        check("%s: the agent ends the connection of the second session" % held_line,
            await b.closed(10))
        b.close()

    r = await op.ask("help cmd=command-agent")
    check("the agent still answers", bool(r) and r.get("result") == 0)

    #   A console that the control center opens, and a request of the
    #   agent's: what they send back goes up the link, as before
    w = cc_writer[-1]
    w.write(cc_command("open-console", {"name": "t3", "process": "cat", "cwd": "/yuneta"}))
    check("console by the link: its open goes up the link",
        await link_frames_until(lambda: link_events("EV_TTY_OPEN"), 5))
    w.write(cc_command("write-tty", {"name": "t3",
        "content64": base64.b64encode(b"hello link\n").decode()}))

    def tty_text():
        return b"".join(base64.b64decode((kw.get("data") or {}).get("content64") or "")
            for kw in link_events("EV_TTY_DATA")).decode(errors="replace")

    check("console by the link: what it writes goes up the link",
        await link_frames_until(lambda: "hello link" in tty_text(), 5))
    w.write(cc_command("close-console", {"name": "t3"}))
    check("console by the link: its close goes up the link",
        await link_frames_until(lambda: link_events("EV_TTY_CLOSE"), 5))
    w.write(cc_command("read-file", {"filename": "/yuneta/agent/yuneta_agent.json"}))
    check("read-file by the link: answered up the link with EV_READ_FILE",
        await link_frames_until(lambda: [kw for kw in link_events("EV_READ_FILE")
            if kw.get("result") == 0 and kw.get("data")], 5))
    print("what came back of the control center's console and request: %s" % [
        (e, kw.get("result"), kw.get("comment"), (kw.get("data") or {}).get("name") if isinstance(kw.get("data"), dict) else None)
        for e, kw in link_frames], flush=True)

    #   A console of the control center that floods while it does not read
    await flood_not_read_by_the_link(op)

    #   4. The control center plays and pauses the agent through its link.
    #   Only a yuno's agent may play or pause it: the link to the control
    #   center refuses both (ack -1) and the agent goes on as it was. Up to
    #   7.30.26 the pause reached the agent's mt_pause, which stops that same
    #   link inside its own action and closes the treedbs: with an order, the
    #   pause and two more bytes in one write the agent died (SIGSEGV in
    #   C_PROT_TCP4H); otherwise its 1991 and the link were closed for good
    w = cc_writer[-1]
    n_links = len(cc_writer)
    w.write(cc_play_pause("EV_PLAY_YUNO"))
    end = time.monotonic() + 5
    while not play_pause_acks and time.monotonic() < end:
        await asyncio.sleep(0.01)
    tail = cc_order(1, "cc_pause_order")
    try:
        w.write(cc_order(0, "cc_pause_order") + cc_play_pause("EV_PAUSE_YUNO") + tail[:2])
        await w.drain()
        await asyncio.sleep(0.2)
        w.write(tail[2:] + b"".join(cc_order(i, "cc_pause_order") for i in range(2, 42)))
        await w.drain()
    except (ConnectionError, ssl.SSLError):
        pass
    end = time.monotonic() + 10
    while (len(pause_answers) < 42 or len(play_pause_acks) < 2) and time.monotonic() < end:
        await asyncio.sleep(0.01)
    print("acks of play / pause: %s" % play_pause_acks, flush=True)
    check("the play and the pause of the control center refused (ack -1): %s" % (
            [(e, r_) for e, r_, c in play_pause_acks]),
        [(e, r_) for e, r_, c in play_pause_acks] ==
            [("EV_PLAY_YUNO_ACK", -1), ("EV_PAUSE_YUNO_ACK", -1)])
    check("the orders around the pause are answered: %d of 42" % len(pause_answers),
        len(pause_answers) == 42 and all(v == 0 for v in pause_answers.values()))
    r = await op.ask("help cmd=list-yunos")
    check("the agent still answers its operator after the pause", bool(r) and r.get("result") == 0)
    o2 = await session()
    check("the agent's 1991 still takes a session after the pause", o2 is not None)
    if o2 is not None:
        o2.close()
    check("the link to the control center stays (no new connection): %d" % len(cc_writer),
        len(cc_writer) == n_links)

    #   A console of the control center that writes while the link is down
    await console_while_the_link_is_down(in_session)

    #   5. The control center sends orders while the agent stops: the first
    #   half one by one, each answered; then a SIGINT to the agent, and the
    #   rest at once. The agent pauses (its treedbs closed) before it stops,
    #   and the loop goes on: up to 7.30.26 the link stayed in session, and
    #   an order read the treedb freed (SIGSEGV).
    w = cc_writer[-1]
    for i in range(FLOOD // 2):
        w.write(cc_order(i))
        end = time.monotonic() + 5
        while i not in flood_answers and time.monotonic() < end:
            await asyncio.sleep(0.005)
    check("the orders before the stop are answered: %d of %d" % (
            len(flood_answers), FLOOD // 2),
        len(flood_answers) == FLOOD // 2 and all(v == 0 for v in flood_answers.values()))
    pid = agent_pid()
    check("the agent's pid: %d" % pid, pid > 0)
    if pid > 0:
        os.kill(pid, signal.SIGINT)
    for i in range(FLOOD // 2, FLOOD):
        try:
            w.write(cc_order(i))
            await w.drain()
        except (ConnectionError, ssl.SSLError):
            break
        await asyncio.sleep(0.001)
    end = time.monotonic() + 15
    while pid > 0 and time.monotonic() < end and os.path.exists("/proc/%d" % pid):
        await asyncio.sleep(0.05)
    print("orders answered: %d of %d" % (len(flood_answers), FLOOD), flush=True)
    check("no answer to an order never sent",
        all(0 <= k < FLOOD for k in flood_answers))

    for p in (op, bob):
        p.close()
    server.close()


asyncio.run(main())
if failures:
    print("%d check(s) failed" % len(failures))
    sys.exit(1)
sys.exit(0)
