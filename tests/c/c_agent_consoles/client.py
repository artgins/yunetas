#
#   The consoles of the agent and the sessions that see them
#
#   Run by run_test.sh inside the sandbox. Every peer is a websocket client
#   written by hand on the agent's 1991.
#
#   1. A session opens a console that floods (`yes`) and does not read: the
#      agent's memory stays bounded and it lives. The console pauses its
#      reading for that session (`console_pause_tx_queue`), and waits for
#      it past `console_pause_timeout` (5 s in agent.json): the session is
#      alone on it and freezes nobody else. When it reads, the console goes
#      on; when it leaves, the console goes with it. Up to 7.30.26 the
#      agent grew some 50 MB/s until it aborted ("REACHED
#      MAX_SYSTEM_MEMORY"); before the fix the console was closed at the
#      deadline.
#   2. A session opens a console and reads; another joins it and does not
#      read. The console prints 30 MB at a pace: the reading session gets
#      ALL of it, in order (the console is paused for the other one, which
#      is taken off within its deadline, ONE WARNING: it does not freeze
#      the console for ever), and when the other one reads at last, its
#      backlog ends with the console's EV_TTY_CLOSE.
#   5. A session that reads at about 1 MB/s with a small receive window,
#      slower than the console writes (`seq` of 11 MB at full speed), gets
#      ALL of it, in order, and is not taken off: the console waits for it.
#      With no flow control it was taken off once it fell
#      console_max_tx_queue messages behind (2048 in agent.json).
#   3. A session that only JOINS a console leaves: the console lives, and
#      the one that opened it still gets its output. The session that
#      opened it leaves: the console is closed, and the session still
#      joined gets its EV_TTY_CLOSE. Up to 7.30.26 the joiner's leaving
#      closed the console for everybody, and nobody was told.
#   4. A session that presents the card of a registered yuno opens a
#      console that writes, and leaves: the console goes with it, and the
#      next session of that channel gets nothing of it. Up to 7.30.26 the
#      console of such a session outlived it, the next session of the
#      channel got its output, and an INFO "requester gone" was logged per
#      chunk in between. The same with a watch-yuno-stats of that session:
#      up to 7.30.26 the next session of the channel got its readings.
#
#   6. The session that opened a console does not read, while a session
#      that joined it does: the one that opened it stops the other, so at
#      its deadline it is taken off (ONE WARNING) and the console is closed
#      for everybody; the joined session gets the EV_TTY_CLOSE.
#
#   7. The session that opened a console does not read and waits alone on
#      it; another session joins it 2.5 to 3 s before the deadline of the
#      first one's pause: the console lives past that deadline, and the
#      first one is taken off (ONE WARNING, the console closed) a whole
#      console_pause_timeout after the join, while the other one would
#      read. Before the fix the join closed the console, and killed its
#      process, at the deadline the first one had.
#
#   8. A session opens a console of `yes` with its request padded with
#      1 MB (a key of its own in __md_iev__, then the `host` of its hop),
#      and does not read: the agent's memory stays bounded (+25 MB), the
#      padding of its own is not in the frames of the output (each under
#      16 KB), and a padded hop pauses the console on the bytes waiting
#      (console_pause_tx_bytes). Before the fix the agent kept the whole
#      __md_iev__, copied it into every chunk of 1 KB, and paused at 256
#      messages whatever their size: +160 MB or more.
#
#   Usage: client.py <logcenter binary>
#   Exit 0 when every check passed.
#
#   Copyright (c) 2026, ArtGins.
#   All Rights Reserved.
#
import asyncio
import base64
import json
import os
import re
import socket
import struct
import sys
import time

HOST = "127.0.0.1"
PORT = 1991
YUNO_BINARY = sys.argv[1]
RSS_CAP_MB = 300        # a run that grows more is a failure, and stops there
RSS_BOUND_MB = 160      # what the agent may grow while a session does not read
THROTTLE = 1000000      # bytes of output per second the slow reader takes
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


def command(cmd, kw_cmd, role="ycommand"):
    kw = {
        "__command__": cmd,
        "__md_iev__": {
            "ievent_gate_stack": [routing(role)],
            "__msg_type__": "__command__",
            "command_stack": [{"command": cmd, "kw": {}}],
        },
    }
    kw.update(kw_cmd)
    return {"event": "EV_MT_COMMAND", "kw": kw}


class Peer:
    """A websocket client written by hand: its frames go out as they are made"""

    async def open(self, rcvbuf=0):
        for i in range(300):    # the agent is starting: up to 30 s
            try:
                sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
                if rcvbuf:
                    sock.setsockopt(socket.SOL_SOCKET, socket.SO_RCVBUF, rcvbuf)
                sock.setblocking(False)
                await asyncio.get_running_loop().sock_connect(sock, (HOST, PORT))
                self.r, self.w = await asyncio.open_connection(sock=sock)
                break
            except OSError:
                sock.close()
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


async def session(card_iev=None, rcvbuf=0):
    o = await Peer().open(rcvbuf)
    o.send(card_iev or card("ycommand", os.getpid()))
    ack = await o.until(("EV_IDENTITY_CARD_ACK",), 10)
    return o if ack and ack["kw"].get("result") == 0 else None


async def leave(peer):
    peer.w.write(Peer.frame(0x8, struct.pack(">H", 1000)))
    await peer.closed(10)
    peer.close()


def events_of(frames, event):
    return [f["kw"] for f in frames if f.get("event") == event]


def names_of(frames, event):
    return [(k.get("data") or {}).get("name") for k in events_of(frames, event)]


def tty_bytes(frames, name=None):
    """What a console wrote, in the EV_TTY_DATA of `frames`"""
    text = b""
    for kw in events_of(frames, "EV_TTY_DATA"):
        data = kw.get("data") or {}
        if name is None or data.get("name") == name:
            text += base64.b64decode(data.get("content64") or "")
    return text


def tty_text(frames, name=None):
    return tty_bytes(frames, name).decode(errors="replace")


def write_tty(peer, name, text):
    peer.send(command("write-tty", {"name": name,
        "content64": base64.b64encode(text.encode()).decode()}))


async def open_console(peer, name, process="cat"):
    """open-console of `name`: its answer and the frames that came with it"""
    peer.send(command("open-console", {"name": name, "process": process, "cwd": "/yuneta"}))
    got = await peer.frames(5, lambda g: events_of(g, "EV_MT_COMMAND_ANSWER") and
        events_of(g, "EV_TTY_OPEN"))
    answer = (events_of(got, "EV_MT_COMMAND_ANSWER") or [{}])[0]
    return answer, got


async def consoles(peer):
    r = await peer.ask("list-consoles", {})
    return r.get("data") or {}


async def wait_for(cond, secs):
    end = time.monotonic() + secs
    while time.monotonic() < end:
        if await cond():
            return True
        await asyncio.sleep(0.1)
    return await cond()


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


def rss_mb(pid):
    try:
        with open("/proc/%d/status" % pid) as f:
            for line in f:
                if line.startswith("VmRSS:"):
                    return int(line.split()[1]) // 1024
    except OSError:
        pass
    return -1


async def drain(peer, secs):
    """Read everything that waits for `peer`, up to its EV_TTY_CLOSE"""
    got = []
    end = time.monotonic() + secs
    while time.monotonic() < end:
        # The whole window, not a gap of 2 s: on a loaded machine the backlog
        # can pause longer than that before its EV_TTY_CLOSE
        iev = await peer.recv(max(0.1, end - time.monotonic()))
        if iev is None:
            break
        got.append(iev)
        if iev.get("event") == "EV_TTY_CLOSE":
            break
    return got


async def flood_not_read(op, pid):
    """1. A session opens a console that floods, and does not read"""
    n = await session()
    if n is None:
        check("flood: a session", False)
        return
    rss0 = rss_mb(pid)
    n.send(command("open-console", {"name": "flood", "process": "yes", "cwd": "/yuneta"}))
    peak = rss0
    t0 = time.monotonic()
    while time.monotonic() - t0 < 12:   # past two console_pause_timeout
        await asyncio.sleep(0.25)
        rss = rss_mb(pid)
        if rss < 0:
            break
        peak = max(peak, rss)
        if peak - rss0 > RSS_CAP_MB:
            break
    print("flood: the agent from %d to %d MB (peak)" % (rss0, peak), flush=True)
    check("flood: the agent lives", rss_mb(pid) > 0 and (await op.ask("help", {})).get("result") == 0)
    check("flood: the agent's memory stays bounded: +%d MB (bound %d MB)" % (peak - rss0, RSS_BOUND_MB),
        peak - rss0 < RSS_BOUND_MB)
    check("flood: the console of the session alone on it waits for it, it is not closed",
        "flood" in await consoles(op))
    n_bytes = 0
    closed = False
    end = time.monotonic() + 60     # a budget, not a bound: 12 MB in 20 s on a riscv64 board
    while time.monotonic() < end and n_bytes <= 20000000 and not closed:
        iev = await n.recv(end - time.monotonic())
        if iev is None:
            break
        closed = iev.get("event") == "EV_TTY_CLOSE"
        if iev.get("event") == "EV_TTY_DATA":
            n_bytes += len((iev["kw"].get("data") or {}).get("content64") or "") * 3 // 4
    check("flood: when the session reads, the console goes on: %d bytes" % n_bytes,
        n_bytes > 20000000 and not closed)
    n.close()
    check("flood: the console goes with the session", await wait_for(
        lambda: _absent(op, "flood"), 5))


async def opener_not_read(op):
    """6. The session that opened a console does not read, while a session
    that joined it does: the one that opened it stops the other, so it is
    taken off, and the console is closed (it opened it)"""
    o = await session()
    j = await session()
    if o is None or j is None:
        check("opener does not read: two sessions", False)
        return
    answer, got = await open_console(o, "oflood", "yes")
    check("opener does not read: open-console: %s" % answer.get("comment"), answer.get("result") == 0)
    answer, got_j = await open_console(j, "oflood")
    check("opener does not read: another session joins: %s" % answer.get("comment"),
        answer.get("result") == 0)
    closes = []
    end = time.monotonic() + 20     # the console waits for the opener: nothing comes for a while
    while time.monotonic() < end and not closes:
        iev = await j.recv(end - time.monotonic())
        if iev is None:
            break
        if iev.get("event") == "EV_TTY_CLOSE":
            closes.append((iev["kw"].get("data") or {}).get("name"))
    check("opener does not read: the session that joined and reads gets the close: %s" % closes,
        closes == ["oflood"])
    check("opener does not read: and the console is closed", await wait_for(
        lambda: _absent(op, "oflood"), 5))
    o.close()
    await leave(j)


async def joiner_near_the_deadline(op):
    """7. A session that opened a console of `yes` and does not read waits
    alone on it; another session joins it 2.5 to 3 s before the deadline of
    the first one's pause. The join is no reason to close the console at
    that deadline: the first one stops the other only from the join on,
    and is taken off a whole console_pause_timeout after it (5 s), not at
    the deadline it had (before the fix the console was closed, and its
    process killed, at that deadline). The agent's console timer has a
    grain of 1 s: a deadline fires up to 1 s late, so the console is
    looked for 4.3 s after the join (the old deadline fired by then), and
    the close is expected from 5 s to 7.2 s after it (a period restarted at
    the old deadline, not at the join, ends 7.5 s after it or later)"""
    o = await session()
    j = await session()
    if o is None or j is None:
        check("joiner near the deadline: two sessions", False)
        return
    o.send(command("open-console", {"name": "near", "process": "yes", "cwd": "/yuneta"}))
    left = None
    end = time.monotonic() + 20
    while time.monotonic() < end:
        routes = ((await consoles(op)).get("near") or {}).get("routes") or {}
        deadlines = [r.get("pause_deadline") for r in routes.values() if r.get("pause")]
        if len(routes) == 1 and deadlines and deadlines[0]:
            left = deadlines[0] / 1000.0 - time.monotonic()
            if 2.5 <= left <= 3.0:
                break
        left = None
        await asyncio.sleep(0.05)
    check("joiner near the deadline: the opener holds the console paused, %s s to its deadline" % (
            "%.2f" % left if left is not None else "?"),
        left is not None)
    if left is None:
        o.close()
        await leave(j)
        return
    t_join = time.monotonic()
    j.send(command("open-console", {"name": "near", "process": "yes", "cwd": "/yuneta"}))
    t_close = []

    async def joined():
        while True:
            iev = await j.recv(30)
            if iev is None:
                return
            if iev.get("event") == "EV_TTY_CLOSE" and data_name_of(iev) == "near":
                t_close.append(time.monotonic())
                return

    task = asyncio.ensure_future(joined())
    await asyncio.sleep(4.3)
    routes = ((await consoles(op)).get("near") or {}).get("routes") or {}
    check("joiner near the deadline: %.1f s after the join (%.1f s past the deadline the opener had)"
            " the console lives, with both sessions: %s" % (
            time.monotonic() - t_join, time.monotonic() - t_join - left, list(routes)),
        len(routes) == 2 and not t_close)
    try:
        await asyncio.wait_for(task, 10)
    except asyncio.TimeoutError:
        pass
    after = t_close[0] - t_join if t_close else -1
    check("joiner near the deadline: the opener that does not read is taken off a whole"
            " console_pause_timeout (5 s) after the join: %.1f s" % after,
        4.6 <= after <= 7.2)
    check("joiner near the deadline: and the console is closed", await wait_for(
        lambda: _absent(op, "near"), 5))
    o.close()
    await leave(j)


PAD = 1000000           # bytes a peer pads its request with
PAD_BOUND_MB = 25       # what the agent may grow while a padded session does not read (the hard bound alone, console_max_tx_bytes, lets it hold 32 MB)
SMALL_FRAME = 16384     # a frame of a console's output is about 2 KB


def padded_command(cmd, kw_cmd, where):
    """A request padded with PAD bytes: a key of its own in __md_iev__
    ("md"), or the `host` of its hop ("hop")"""
    iev = command(cmd, kw_cmd)
    md = iev["kw"]["__md_iev__"]
    if where == "md":
        md["padding"] = "x" * PAD
    else:
        md["ievent_gate_stack"][0]["host"] = "x" * PAD
    return iev


async def padded_request(op, pid):
    """8. A session opens a console of `yes` with its request padded with
    1 MB, and does not read: the agent's memory stays bounded. A key of
    the peer's own in __md_iev__ is not kept by the route, so the output
    comes in small frames; a hop padded (what routes the answer back) makes
    every frame 1 MB, and the console pauses on the bytes waiting
    (console_pause_tx_bytes), not on 256 messages of 1 MB. Before the fix
    the agent kept the whole __md_iev__ and copied it into every chunk, and
    paused at 256 messages whatever their size"""
    for name, where in (("padmd", "md"), ("padhop", "hop")):
        p = await session()
        if p is None:
            check("padded %s: a session" % where, False)
            return
        rss0 = rss_mb(pid)
        p.send(padded_command("open-console", {"name": name, "process": "yes", "cwd": "/yuneta"}, where))
        peak = rss0
        t0 = time.monotonic()
        while time.monotonic() - t0 < 6:
            await asyncio.sleep(0.25)
            rss = rss_mb(pid)
            if rss < 0:
                break
            peak = max(peak, rss)
            if peak - rss0 > RSS_CAP_MB:
                break
        print("padded %s: the agent from %d to %d MB (peak)" % (where, rss0, peak), flush=True)
        check("padded %s: the agent's memory stays bounded: +%d MB (bound %d MB)" % (
                where, peak - rss0, PAD_BOUND_MB),
            rss_mb(pid) > 0 and peak - rss0 < PAD_BOUND_MB)
        sizes = []
        end = time.monotonic() + 20
        while time.monotonic() < end and len(sizes) < 50:
            iev = await p.recv(end - time.monotonic())
            if iev is None:
                break
            if iev.get("event") == "EV_TTY_DATA" and data_name_of(iev) == name:
                sizes.append(len(json.dumps(iev)))
        print("padded %s: %d frames of output, the biggest %d bytes" % (
            where, len(sizes), max(sizes or [0])), flush=True)
        check("padded %s: when the session reads, the console goes on: %d frames" % (where, len(sizes)),
            len(sizes) == 50)
        if where == "md":
            check("padded md: its output comes in small frames: the biggest %d bytes" % max(sizes or [0]),
                bool(sizes) and max(sizes) < SMALL_FRAME)
        p.close()
        check("padded %s: the console goes with the session" % where, await wait_for(
            lambda: _absent(op, name), 10))


def data_name_of(iev):
    data = (iev.get("kw") or {}).get("data")
    return data.get("name") if isinstance(data, dict) else None


SEQ_LINES = 3000000
SEQ_COMMAND = ("for i in $(seq 1 300); do seq -f %08.0f $((i*10000)) $((i*10000+9999)); "
    "sleep 0.02; done; echo END_OF_OUTPUT\n")


async def joiner_not_read(op):
    """2. A reading session and a joined one that does not read"""
    r = await session()
    n = await session()
    if r is None or n is None:
        check("slow joiner: two sessions", False)
        return
    answer, got = await open_console(r, "paced", "bash")
    check("slow joiner: open-console: %s" % answer.get("comment"), answer.get("result") == 0)
    answer, got_n = await open_console(n, "paced")
    check("slow joiner: the other session joins: %s" % answer.get("comment"), answer.get("result") == 0)
    await asyncio.sleep(0.5)
    await r.frames(1)
    write_tty(r, "paced", SEQ_COMMAND)
    data = bytearray()
    end = time.monotonic() + 180    # a budget, not a bound: 2.3 M of 3 M lines in 60 s on a riscv64 board
    while time.monotonic() < end:
        iev = await r.recv(10)
        if iev is None:
            break
        if iev.get("event") == "EV_TTY_DATA":
            data += base64.b64decode((iev["kw"].get("data") or {}).get("content64") or "")
            if b"\nEND_OF_OUTPUT" in data[-1024:]:
                break
    numbers = seq_numbers(data)
    check("slow joiner: the reading session gets all the output, in order: %d of %d lines" % (
            len(numbers), SEQ_LINES),
        numbers == list(range(10000, 10000 + SEQ_LINES)))
    routes = ((await consoles(op)).get("paced") or {}).get("routes") or {}
    check("slow joiner: the session that does not read is off the console: %s" % list(routes),
        len(routes) == 1)
    got_n = await drain(n, 90)     # a budget too: 30 s did not read its whole backlog on a riscv64 board
    check("slow joiner: its backlog ends with the close of the console for it: %s" % (
            [f.get("event") for f in got_n[-1:]]),
        bool(got_n) and got_n[-1].get("event") == "EV_TTY_CLOSE" and
        names_of(got_n[-1:], "EV_TTY_CLOSE") == ["paced"])
    write_tty(r, "paced", "echo STILL_HERE\n")
    got = await r.frames(5, lambda g: "STILL_HERE\r\n" in tty_text(g))
    check("slow joiner: the console goes on for the reading session", "STILL_HERE\r\n" in tty_text(got))
    r.send(command("close-console", {"name": "paced"}))
    await r.frames(5, lambda g: events_of(g, "EV_TTY_CLOSE"))
    await leave(r)
    await leave(n)


def seq_numbers(data):
    numbers = []
    for line in bytes(data).split(b"\n"):
        line = line.rstrip(b"\r").split(b"\r")[-1]    # what the terminal printed before it, overwritten
        if re.fullmatch(rb"\d{8}", line):
            numbers.append(int(line))
    return numbers


async def slow_reader():
    """5. A session that reads at about 1 MB/s gets all of a fast output"""
    r = await session(rcvbuf=65536)     # a small window, as a reader far away
    if r is None:
        check("slow reader: a session", False)
        return
    answer, got = await open_console(r, "slow", "bash")
    check("slow reader: open-console: %s" % answer.get("comment"), answer.get("result") == 0)
    await asyncio.sleep(0.5)
    await r.frames(1)
    write_tty(r, "slow", "seq -f %08.0f 1 1100000; echo END_OF_OUTPUT\n")
    data = bytearray()
    t0 = time.monotonic()
    closed = False
    while time.monotonic() - t0 < 60:
        iev = await r.recv(10)
        if iev is None:
            break
        if iev.get("event") == "EV_TTY_CLOSE":
            closed = True
            break
        if iev.get("event") == "EV_TTY_DATA":
            data += base64.b64decode((iev["kw"].get("data") or {}).get("content64") or "")
            if b"\nEND_OF_OUTPUT" in data[-1024:]:
                break
            ahead = len(data) / THROTTLE - (time.monotonic() - t0)
            if ahead > 0:
                await asyncio.sleep(ahead)
    elapsed = time.monotonic() - t0
    numbers = seq_numbers(data)
    print("slow reader: %d bytes in %.1f s" % (len(data), elapsed), flush=True)
    check("slow reader: it is not taken off the console", not closed)
    check("slow reader: it gets ALL the output, in order: %d of 1100000 lines" % len(numbers),
        numbers == list(range(1, 1100001)))
    check("slow reader: it read slower than the console writes (%.1f s)" % elapsed, elapsed > 5)
    r.send(command("close-console", {"name": "slow"}))
    await r.frames(5, lambda g: events_of(g, "EV_TTY_CLOSE"))
    await leave(r)


async def joiner_leaves(op):
    """3. The session that joins leaves; then the one that opened it"""
    a = await session()
    b = await session()
    c = await session()
    if a is None or b is None or c is None:
        check("joiner leaves: three sessions", False)
        return
    answer, got = await open_console(a, "j1")
    check("joiner leaves: open-console: %s" % answer.get("comment"), answer.get("result") == 0)
    answer, got = await open_console(b, "j1")
    check("joiner leaves: another session joins: %s" % answer.get("comment"), answer.get("result") == 0)
    await leave(b)
    await asyncio.sleep(0.5)
    check("joiner leaves: the console lives after the joiner left", "j1" in await consoles(op))
    write_tty(a, "j1", "after the joiner left\n")
    got = await a.frames(5, lambda g: "after the joiner left" in tty_text(g))
    check("joiner leaves: the session that opened it still gets its output",
        "after the joiner left" in tty_text(got))
    check("joiner leaves: and no close", names_of(got, "EV_TTY_CLOSE") == [])

    answer, got = await open_console(c, "j1")
    check("joiner leaves: a third session joins: %s" % answer.get("comment"), answer.get("result") == 0)
    await leave(a)
    got = await c.frames(5, lambda g: events_of(g, "EV_TTY_CLOSE"))
    check("joiner leaves: the session that opened it leaves: the one joined gets the close: %s" % (
            names_of(got, "EV_TTY_CLOSE")),
        names_of(got, "EV_TTY_CLOSE") == ["j1"])
    check("joiner leaves: and the console is closed", await wait_for(
        lambda: _absent(op, "j1"), 5))
    await leave(c)


async def _absent(op, name):
    return name not in await consoles(op)


async def register_yuno(op):
    """A logcenter registered and never run: its card can be presented"""
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
    return rows[0] if len(rows) == 1 else None


async def console_of_a_yuno_card(op, row):
    """4. The console of a session that presents the card of a yuno"""
    realm_id = row["realm_id"][0] if isinstance(row["realm_id"], list) else row["realm_id"]
    y = await session(card("logcenter", 999999, yuno_id=row["id"], yuno_name="x",
        yuno_release=row["yuno_release"], realm_id=realm_id, watcher_pid=999999))
    if y is None:
        check("yuno card: a session with the card of the yuno", False)
        return
    y.send(command("open-console", {"name": "fy", "process": "bash", "cwd": "/yuneta"}, role="logcenter"))
    got = await y.frames(5, lambda g: events_of(g, "EV_MT_COMMAND_ANSWER") and events_of(g, "EV_TTY_OPEN"))
    answer = (events_of(got, "EV_MT_COMMAND_ANSWER") or [{}])[0]
    check("yuno card: open-console: %s" % answer.get("comment"), answer.get("result") == 0)
    routes = ((await consoles(op)).get("fy") or {}).get("routes") or {}
    y.send(command("write-tty", {"name": "fy", "content64": base64.b64encode(
        b"while true; do echo TICK; sleep 0.01; done\n").decode()}, role="logcenter"))
    got = await y.frames(3, lambda g: "TICK\r\n" in tty_text(g))
    check("yuno card: its console writes", "TICK\r\n" in tty_text(got))
    y.send(command("watch-yuno-stats", {"ids": row["id"], "period": 1000,
        "__relays__": ["EV_YUNO_STATS"]}, role="logcenter"))
    got = await y.frames(3, lambda g: events_of(g, "EV_YUNO_STATS"))
    check("yuno card: its watch reads", bool(events_of(got, "EV_YUNO_STATS")))
    y.close()      # abrupt, as a crash
    await asyncio.sleep(1)
    check("yuno card: the console goes with the session: %s" % list(await consoles(op)),
        await wait_for(lambda: _absent(op, "fy"), 5))
    z = await session()
    if z is None:
        check("yuno card: the next session", False)
        return
    got = await z.frames(2.5)
    answer, probe = await open_console(z, "zprobe")
    z_routes = ((await consoles(op)).get("zprobe") or {}).get("routes") or {}
    check("yuno card: the next session took its channel: %s, %s" % (list(routes), list(z_routes)),
        bool(routes) and list(routes) == list(z_routes))
    z.send(command("close-console", {"name": "zprobe"}))
    await z.frames(2, lambda g: events_of(g, "EV_TTY_CLOSE"))
    check("yuno card: the next session gets nothing of the console: %d bytes" % len(tty_bytes(got)),
        tty_bytes(got) == b"")
    check("yuno card: nor of its watch: %d readings" % len(events_of(got, "EV_YUNO_STATS")),
        events_of(got, "EV_YUNO_STATS") == [])
    print("yuno card: its route was %s" % list(routes), flush=True)
    await leave(z)


async def main():
    op = await session()
    check("a session of ycommand on 1991", op is not None)
    if op is None:
        return
    pid = agent_pid()
    check("the agent's pid: %d" % pid, pid > 0)

    #   The agent runs its enabled yunos once, some 3 s after it starts
    #   (timerStBoot is 0.1 s in agent.json): the yuno is created after it
    await asyncio.sleep(4)
    row = await register_yuno(op)

    await flood_not_read(op, pid)
    await joiner_not_read(op)
    await joiner_leaves(op)
    await slow_reader()
    await opener_not_read(op)
    await joiner_near_the_deadline(op)
    await padded_request(op, pid)
    if row:
        await console_of_a_yuno_card(op, row)

    r = await op.ask("help", {})
    check("the agent still answers", r.get("result") == 0)
    print("the agent at the end: %d MB" % rss_mb(pid), flush=True)
    op.close()


asyncio.run(main())
if failures:
    print("%d check(s) failed" % len(failures))
    sys.exit(1)
sys.exit(0)
