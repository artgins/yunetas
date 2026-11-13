#
#   A control center for yuneta_agent22, as little as it takes: TLS (the
#   agent pins this certificate; no client certificate is asked), tcp4h
#   frames (a 4-byte length in network order, the header included) holding
#   ievents. It acks the identity card, sends each line of argv as a command
#   to agent22, and prints what came back for each, and whether the link
#   stayed up. The line FLOOD is a console that floods (`yes`) while the
#   control center does not read its link for 8 s (flood()): "FLOOD OK"
#   when agent22's memory stayed bounded and the console went on. The lines
#   FLOODPAD md / FLOODPAD hop open one with the request padded with 1 MB
#   (flood_pad()): "FLOODPAD <where> OK" when agent22's memory stayed
#   bounded, and the padding of its own was not in the frames of output.
#
#   Usage: control_center.py <cert> <key> <port> <line>...
#   Exit 0 when the link lived through every line, 2 when the agent closed
#   it.
#
#   Copyright (c) 2026, ArtGins.
#   All Rights Reserved.
#
import json
import socket
import ssl
import struct
import sys
import time


def send(s, event, kw):
    b = json.dumps({"event": event, "kw": kw}).encode()
    s.sendall(struct.pack("!I", len(b) + 4) + b)


def recv_exact(s, n):
    b = b""
    while len(b) < n:
        c = s.recv(n - len(b))
        if not c:
            raise ConnectionError("closed")
        b += c
    return b


def recv(s, secs):
    s.settimeout(secs)
    n = struct.unpack("!I", recv_exact(s, 4))[0] - 4
    return json.loads(recv_exact(s, n))


def routing(dst_yuno, dst_role, dst_service):
    return {
        "dst_yuno": dst_yuno, "dst_role": dst_role, "dst_service": dst_service,
        "src_yuno": "cc", "src_role": "controlcenter", "src_service": "controlcenter",
        "user": "yuneta", "host": "sandbox",
    }


def command(line):
    return {
        "__command__": line,
        "__md_iev__": {
            "ievent_gate_stack": [routing("", "", "agent22")],
            "__msg_type__": "__command__",
            "command_stack": [{"command": line, "kw": {}}],
        },
    }


def agent_pid():
    """agent22 of the sandbox (a pid namespace of its own)"""
    import os
    for d in os.listdir("/proc"):
        if d.isdigit():
            try:
                with open("/proc/%s/comm" % d) as f:
                    if f.read().strip() == "yuneta_agent22":
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


def tty_bytes(s, secs, name, until=0):
    """What the console `name` writes up the link within `secs` (or until
    `until` bytes), and whether its EV_TTY_CLOSE came"""
    import base64
    n = 0
    closed = False
    end = time.time() + secs
    try:
        while time.time() < end and (not until or n < until):
            g = recv(s, max(0.1, end - time.time()))
            data = g.get("kw", {}).get("data") or {}
            if not isinstance(data, dict) or data.get("name") != name:
                continue
            if g.get("event") == "EV_TTY_DATA":
                n += len(base64.b64decode(data.get("content64") or ""))
            elif g.get("event") == "EV_TTY_CLOSE":
                closed = True
                break
    except socket.timeout:
        pass
    return n, closed


def flood(s):
    """The control center opens a console that floods (`yes`) and does not
    read its link for 8 s: agent22's memory stays bounded and the console
    waits for it (its flow control); then it reads, and the console goes
    on. Up to 7.30.26 agent22 grew until it aborted."""
    pid = agent_pid()
    rss0 = rss_mb(pid)
    send(s, "EV_MT_COMMAND", command("open-console name=f1 process=yes cwd=/yuneta"))
    n, closed = tty_bytes(s, 2, "f1", 100000)
    if n == 0:
        print("FLOOD FAIL: the console wrote nothing up the link")
        return
    ok = True
    peak = rss0
    t0 = time.time()
    while time.time() - t0 < 8:
        time.sleep(0.25)
        rss = rss_mb(pid)
        if rss < 0:
            break
        peak = max(peak, rss)
        if peak - rss0 > 300:
            break
    print("FLOOD: agent22 from %d to %d MB (peak)" % (rss0, peak))
    if rss_mb(pid) < 0:
        print("FLOOD FAIL: agent22 died")
        return
    if peak - rss0 >= 160:
        print("FLOOD FAIL: agent22 grew %d MB while the link was not read" % (peak - rss0))
        ok = False
    n, closed = tty_bytes(s, 15, "f1", 5000000)
    if closed or n < 5000000:
        print("FLOOD FAIL: when the link is read again the console does not go on: %d bytes%s" % (
            n, ", closed" if closed else ""))
        ok = False
    send(s, "EV_MT_COMMAND", command("close-console name=f1"))
    n, closed = tty_bytes(s, 15, "f1")
    if not closed:
        print("FLOOD FAIL: close-console did not close it")
        return
    if ok:
        print("FLOOD OK")


PAD = 1000000       # bytes the control center pads its request with


def flood_pad(s, where):
    """The control center opens a console that floods (`yes`) with its
    request padded with 1 MB -- a key of its own in __md_iev__ ("md"), or
    the `host` of its hop ("hop") -- and does not read its link for 6 s:
    agent22's memory stays bounded. The key of its own is not kept by the
    console's route, so the output comes in small frames; a padded hop
    makes every frame 1 MB, and the console pauses on the bytes waiting
    (console_pause_tx_bytes). Before the fix agent22 kept the whole
    __md_iev__, copied it into every chunk, and paused at 256 messages
    whatever their size."""
    name = "pad" + where
    kw = command("open-console name=%s process=yes cwd=/yuneta" % name)
    if where == "md":
        kw["__md_iev__"]["padding"] = "x" * PAD
    else:
        kw["__md_iev__"]["ievent_gate_stack"][0]["host"] = "x" * PAD
    pid = agent_pid()
    rss0 = rss_mb(pid)
    send(s, "EV_MT_COMMAND", kw)
    peak = rss0
    t0 = time.time()
    while time.time() - t0 < 6:
        time.sleep(0.25)
        rss = rss_mb(pid)
        if rss < 0:
            break
        peak = max(peak, rss)
        if peak - rss0 > 300:
            break
    print("FLOODPAD %s: agent22 from %d to %d MB (peak)" % (where, rss0, peak))
    if rss_mb(pid) < 0:
        print("FLOODPAD %s FAIL: agent22 died" % where)
        return
    ok = True
    if peak - rss0 >= 25:
        print("FLOODPAD %s FAIL: agent22 grew %d MB while the link was not read" % (where, peak - rss0))
        ok = False
    sizes = []
    end = time.time() + 20
    try:
        while time.time() < end and len(sizes) < 30:
            s.settimeout(max(0.1, end - time.time()))
            n = struct.unpack("!I", recv_exact(s, 4))[0] - 4
            g = json.loads(recv_exact(s, n))
            data = g.get("kw", {}).get("data") or {}
            if g.get("event") == "EV_TTY_DATA" and isinstance(data, dict) and data.get("name") == name:
                sizes.append(n)
    except socket.timeout:
        pass
    print("FLOODPAD %s: %d frames of output, the biggest %d bytes" % (where, len(sizes), max(sizes or [0])))
    if len(sizes) < 30:
        print("FLOODPAD %s FAIL: when the link is read the console does not go on" % where)
        ok = False
    if where == "md" and max(sizes or [0]) >= 16384:
        print("FLOODPAD md FAIL: the padding of the request travels in every frame")
        ok = False
    send(s, "EV_MT_COMMAND", command("close-console name=%s" % name))
    n, closed = tty_bytes(s, 15, name)
    if not closed:
        print("FLOODPAD %s FAIL: close-console did not close it" % where)
        return
    if ok:
        print("FLOODPAD %s OK" % where)


def main():
    ctx = ssl.SSLContext(ssl.PROTOCOL_TLS_SERVER)
    ctx.load_cert_chain(sys.argv[1], sys.argv[2])
    ls = socket.socket()
    ls.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
    ls.bind(("127.0.0.1", int(sys.argv[3])))
    ls.listen(1)
    ls.settimeout(40)
    s = ctx.wrap_socket(ls.accept()[0], server_side=True)

    card = recv(s, 10)
    print("got", card["event"])
    hop = card["kw"]["__md_iev__"]["ievent_gate_stack"][0]
    send(s, "EV_IDENTITY_CARD_ACK", {
        "result": 0,
        "data": {},
        "__md_iev__": {
            "ievent_gate_stack": [
                routing(hop.get("src_yuno", ""), hop.get("src_role", ""), hop.get("src_service", ""))
            ],
            "__msg_type__": "__answer__",
        },
    })
    time.sleep(0.5)

    for line in sys.argv[4:]:
        if line == "FLOOD":
            flood(s)
            continue
        if line.startswith("FLOODPAD "):
            flood_pad(s, line.split()[1])
            continue
        send(s, "EV_MT_COMMAND", command(line))
        got = []
        end = time.time() + 2
        try:
            while time.time() < end:
                got.append(recv(s, max(0.1, end - time.time())))
        except socket.timeout:
            pass
        except (ConnectionError, ssl.SSLError, OSError) as e:
            print("LINE %r: the link was closed (%s)" % (line, e))
            return 2
        shown = []
        for g in got:
            kw = g.get("kw", {})
            shown.append([g.get("event"), kw.get("result"),
                (kw.get("data") or {}).get("name") if g.get("event") == "EV_TTY_CLOSE" else None])
        print("LINE %r: %s" % (line, json.dumps(shown)))
    print("LINK ALIVE")
    s.close()
    return 0


sys.exit(main())
