#
#   A peer of ycommand that sends it the answers of the agent's own
#   requests (EV_READ_BINARY_FILE, EV_READ_FILE...), asked or not.
#
#   Run by run_test.sh inside a network namespace of its own. A websocket
#   server written by hand on 127.0.0.1:1991 acks ycommand's identity card
#   and answers its command; ycommand runs with HOME in the work dir, so
#   what it writes in ~/.yuneta/configs/ is seen here.
#
#   1. ycommand sends `help`: the peer answers it, and before the answer
#      sends an EV_READ_BINARY_FILE, an EV_READ_FILE and an EV_EDIT_CONFIG
#      ycommand never asked. ycommand must write none of them (up to
#      7.30.26 it wrote each in ~/.yuneta/configs/, and an EV_EDIT_CONFIG
#      opened the editor and left a create-config on its line).
#   2. ycommand sends `read-binary-file filename=/x/good.bin`: the peer
#      answers it with an EV_READ_BINARY_FILE, written as before, and then
#      with an EV_READ_FILE more: one answer per request, not written.
#   3. ycommand sends it through a control center (`command-agent
#      cmd2agent='read-binary-file ...'`): the same.
#
#   Usage: fake_agent.py <ycommand binary> <work dir>
#   Exit 0 when every check passed.
#
#   Copyright (c) 2026, ArtGins.
#   All Rights Reserved.
#
import asyncio
import base64
import hashlib
import json
import os
import struct
import sys

HOST = "127.0.0.1"
PORT = 1991
YCOMMAND = sys.argv[1]
WORK = sys.argv[2]
HOME = os.path.join(WORK, "home")
CONFIGS = os.path.join(HOME, ".yuneta", "configs")
failures = []
scenario = {"name": ""}


def check(what, ok):
    print(("ok    " if ok else "FAIL  ") + what, flush=True)
    if not ok:
        failures.append(what)


def reverse(md):
    md = json.loads(json.dumps(md))
    top = md["ievent_gate_stack"][0]
    top["dst_yuno"], top["dst_role"], top["dst_service"] = (
        top.get("src_yuno", ""), top.get("src_role", ""), top.get("src_service", ""))
    top["src_yuno"], top["src_role"], top["src_service"] = "", "yuneta_agent", "agent"
    md["__msg_type__"] = "__answer__"
    return md


def frame(payload):
    b = json.dumps(payload).encode()
    n = len(b)
    if n < 126:
        hdr = bytes([0x81, n])
    elif n < 65536:
        hdr = bytes([0x81, 126]) + struct.pack(">H", n)
    else:
        hdr = bytes([0x81, 127]) + struct.pack(">Q", n)
    return hdr + b


async def recv(r):
    b = await r.readexactly(2)
    n = b[1] & 0x7f
    if n == 126:
        n = struct.unpack(">H", await r.readexactly(2))[0]
    elif n == 127:
        n = struct.unpack(">Q", await r.readexactly(8))[0]
    mask = await r.readexactly(4) if b[1] & 0x80 else b"\0\0\0\0"
    data = bytes(c ^ mask[i % 4] for i, c in enumerate(await r.readexactly(n)))
    if (b[0] & 0x0f) != 0x1:
        return None
    return json.loads(data)


def answer(event, md, data, comment=""):
    return frame({"event": event, "kw": {"result": 0, "comment": comment, "schema": None,
        "data": data, "__md_iev__": reverse(md)}})


async def agent(r, w):
    head = await r.readuntil(b"\r\n\r\n")
    key = [l.split(b":", 1)[1].strip() for l in head.split(b"\r\n")
        if l.lower().startswith(b"sec-websocket-key:")][0]
    accept = base64.b64encode(hashlib.sha1(
        key + b"258EAFA5-E914-47DA-95CA-C5AB0DC85B11").digest()).decode()
    w.write(("HTTP/1.1 101 Switching Protocols\r\nUpgrade: websocket\r\n"
        "Connection: Upgrade\r\nSec-WebSocket-Accept: %s\r\n\r\n" % accept).encode())
    while True:
        try:
            iev = await recv(r)
        except (asyncio.IncompleteReadError, ConnectionError):
            return
        if iev is None:
            continue
        event = iev.get("event")
        kw = iev.get("kw", {})
        md = kw.get("__md_iev__", {})
        if event == "EV_IDENTITY_CARD":
            w.write(frame({"event": "EV_IDENTITY_CARD_ACK", "kw": {"result": 0, "comment": "",
                "data": {}, "__md_iev__": reverse(md)}}))
        elif event == "EV_MT_COMMAND":
            command = kw.get("__command__", "")
            if scenario["name"] == "unasked":
                w.write(answer("EV_READ_BINARY_FILE", md,
                    {"name": "evil.bin", "content64": base64.b64encode(b"EVIL").decode()}))
                w.write(answer("EV_READ_FILE", md, {"name": "evil.txt", "zcontent": "EVIL"}))
                w.write(answer("EV_EDIT_CONFIG", md,
                    [{"id": "evil", "zcontent": {"evil": True}}]))
            elif "read-binary-file" in command:
                w.write(answer("EV_READ_BINARY_FILE", md,
                    {"name": "good.bin", "content64": base64.b64encode(b"GOOD").decode()}))
                w.write(answer("EV_READ_FILE", md, {"name": "evil2.txt", "zcontent": "EVIL"}))
            w.write(answer("EV_MT_COMMAND_ANSWER", md, None, "answered"))


async def ycommand(command, verbose="0"):
    """ycommand -c `command`; with `verbose` 1 its warnings are printed too"""
    env = dict(os.environ, HOME=HOME)
    p = await asyncio.create_subprocess_exec(
        YCOMMAND, "-u", "ws://%s:%d" % (HOST, PORT), "-w", "1", "--editor=cat", "-l", verbose,
        "-c", command,
        stdin=asyncio.subprocess.DEVNULL, stdout=asyncio.subprocess.PIPE,
        stderr=asyncio.subprocess.STDOUT, env=env)
    try:
        out, _ = await asyncio.wait_for(p.communicate(), 20)
    except asyncio.TimeoutError:
        p.kill()
        out, _ = await p.communicate()
    return out.decode(errors="replace")


def written(name):
    path = os.path.join(CONFIGS, name)
    if not os.path.exists(path):
        return None
    with open(path, "rb") as f:
        return f.read()


async def main():
    os.makedirs(HOME, exist_ok=True)
    server = await asyncio.start_server(agent, HOST, PORT)

    scenario["name"] = "unasked"
    out = await ycommand("help", "1")
    check("unasked: ycommand ran its command", "answered" in out)
    check("unasked: an EV_READ_BINARY_FILE it did not ask is not written: %r" % written("evil.bin"),
        written("evil.bin") is None)
    check("unasked: nor an EV_READ_FILE: %r" % written("evil.txt"), written("evil.txt") is None)
    check("unasked: nor an EV_EDIT_CONFIG: %r" % written("evil.json"), written("evil.json") is None)
    check("unasked: and it says so",
        out.count("answer of a request this ycommand did not make, ignored") == 3)

    scenario["name"] = "asked"
    out = await ycommand("read-binary-file filename=/x/good.bin")
    check("asked: the answer of read-binary-file is written: %r" % written("good.bin"),
        written("good.bin") == b"GOOD")
    if written("good.bin") is not None:
        os.unlink(os.path.join(CONFIGS, "good.bin"))
    out = await ycommand("command-agent agent_id=n cmd2agent='read-binary-file filename=/x/good.bin'")
    check("asked through a control center: written: %r" % written("good.bin"),
        written("good.bin") == b"GOOD")
    check("asked: one answer per request: a second one is not written: %r" % written("evil2.txt"),
        written("evil2.txt") is None)

    server.close()


asyncio.run(main())
if failures:
    print("%d check(s) failed" % len(failures))
    sys.exit(1)
sys.exit(0)
