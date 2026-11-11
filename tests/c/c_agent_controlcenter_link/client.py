#
#   The agent's link to the control center IN session: a control center
#   written in python on 127.0.0.1:19898, and an operator on the agent's
#   1991 that sends command-agent and stats-agent to service=controlcenter.
#
#   The control center is as little as it takes: TLS (the agent pins this
#   certificate; no client certificate is asked), tcp4h frames (a 4-byte
#   length in network order, the header included) holding ievents. It acks
#   the identity card of the agent's link and answers each request with its
#   routing reversed, as a C_IEVENT_SRV does: the answer goes to the
#   `src_service` the request came from.
#
#   Up to 7.30.26 that `src_service` was the operator's channel of the agent
#   ("input-0"), which is no service: the link dropped the answer ("service
#   of the event not found, event dropped") and the operator waited for its
#   timeout. Each line must be answered with what the control center said.
#
#   Usage: client.py <cert> <key> <port>
#   Exit 0 when both lines were answered.
#
#   Copyright (c) 2026, ArtGins.
#   All Rights Reserved.
#
import asyncio
import json
import os
import ssl
import struct
import sys
import time

import websockets

URL = "ws://127.0.0.1:1991"
CC_ANSWER = "the control center answers"
failures = []
requests = []


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


async def control_center(r, w, in_session):
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
            requests.append((event, top.get("src_service")))
            answer = "EV_MT_COMMAND_ANSWER" if event == "EV_MT_COMMAND" else "EV_MT_STATS_ANSWER"
            w.write(tcp4h({"event": answer, "kw": {"result": 0, "comment": CC_ANSWER,
                "schema": None, "data": None, "__md_iev__": reverse(md)}}))


def routing(src_role):
    return {
        "dst_yuno": "", "dst_role": "yuneta_agent", "dst_service": "agent",
        "src_yuno": "", "src_role": src_role, "src_service": "cli",
        "user": "yuneta", "host": "sandbox",
    }


def card():
    kw = {
        "yuno_role": "ycommand", "yuno_id": "", "yuno_name": "x", "yuno_tag": "",
        "yuno_version": "1", "yuno_release": "1", "yuneta_version": "",
        "playing": False, "pid": os.getpid(), "watcher_pid": 0,
        "jwt": "", "username": "yuneta", "launch_id": 0, "yuno_startdate": "",
        "id": "sandbox", "realm_id": "", "required_services": [],
        "__md_iev__": {
            "ievent_gate_stack": [routing("ycommand")],
            "__msg_type__": "__identity__",
        },
    }
    return json.dumps({"event": "EV_IDENTITY_CARD", "kw": kw})


def command(line):
    return json.dumps({"event": "EV_MT_COMMAND", "kw": {
        "__command__": line,
        "__md_iev__": {
            "ievent_gate_stack": [routing("ycommand")],
            "__msg_type__": "__command__",
            "command_stack": [{"command": line, "kw": {}}],
        },
    }})


async def answer_of(ws, secs):
    end = time.monotonic() + secs
    while time.monotonic() < end:
        try:
            iev = json.loads(await asyncio.wait_for(ws.recv(), end - time.monotonic()))
        except (asyncio.TimeoutError, websockets.ConnectionClosed):
            return None
        if iev.get("event") in ("EV_MT_COMMAND_ANSWER", "EV_MT_STATS_ANSWER"):
            return iev["kw"]
    return None


async def main():
    ctx = ssl.SSLContext(ssl.PROTOCOL_TLS_SERVER)
    ctx.load_cert_chain(sys.argv[1], sys.argv[2])
    in_session = asyncio.Event()
    server = await asyncio.start_server(
        lambda r, w: control_center(r, w, in_session), "127.0.0.1", int(sys.argv[3]), ssl=ctx)

    ws = None
    for i in range(300):    # the agent is starting: up to 30 s
        try:
            ws = await websockets.connect(URL, max_size=None)
            break
        except OSError:
            await asyncio.sleep(0.1)
    if ws is None:
        check("the agent listens on " + URL, False)
        return
    await ws.send(card())
    ack = None
    end = time.monotonic() + 10
    while time.monotonic() < end and ack is None:
        iev = json.loads(await asyncio.wait_for(ws.recv(), end - time.monotonic()))
        if iev.get("event") == "EV_IDENTITY_CARD_ACK":
            ack = iev["kw"]
    check("a session of ycommand on 1991", bool(ack) and ack.get("result") == 0)

    #   The link connects at once, or after its timeout_between_connections
    try:
        await asyncio.wait_for(in_session.wait(), 30)
    except asyncio.TimeoutError:
        pass
    check("the agent's link to the control center in session", in_session.is_set())
    if not in_session.is_set():
        return
    await asyncio.sleep(0.5)    # the agent took the ack

    for line in ("command-agent service=controlcenter command=list-yunos",
                 "stats-agent service=controlcenter"):
        await ws.send(command(line))
        r = await answer_of(ws, 5)
        check("%s: answered with what the control center said: %s" % (
                line, (r or {}).get("comment")),
            bool(r) and r.get("result") == 0 and r.get("comment") == CC_ANSWER)

    print("requests the control center got (event, src_service): %s" % requests, flush=True)
    check("the control center got both requests", len(requests) == 2)
    await ws.send(command("help cmd=command-agent"))
    r = await answer_of(ws, 5)
    check("the agent still answers", bool(r) and r.get("result") == 0)
    await ws.close()
    server.close()


asyncio.run(main())
if failures:
    print("%d check(s) failed" % len(failures))
    sys.exit(1)
sys.exit(0)
