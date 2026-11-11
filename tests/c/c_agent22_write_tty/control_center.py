#
#   A control center for yuneta_agent22, as little as it takes: TLS (the
#   agent pins this certificate; no client certificate is asked), tcp4h
#   frames (a 4-byte length in network order, the header included) holding
#   ievents. It acks the identity card, sends each line of argv as a command
#   to agent22, and prints what came back for each, and whether the link
#   stayed up.
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
