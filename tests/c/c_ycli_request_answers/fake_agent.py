#
#   A peer of ycli that sends it the answers of the agent's own requests
#   (EV_READ_BINARY_FILE, EV_READ_FILE...), asked or not.
#
#   Run by run_test.sh inside a network namespace of its own. A websocket
#   server written by hand on 127.0.0.1:1991 acks ycli's identity card and
#   answers its commands. ycli runs in a pty (it reads a terminal), without
#   ncurses, with HOME in the work dir, so what it writes in
#   ~/.yuneta/configs/ is seen here; this peer types its lines.
#
#   1. ycli sends `help`: the peer answers it, and before the answer sends
#      an EV_READ_BINARY_FILE, an EV_READ_FILE and an EV_EDIT_CONFIG ycli
#      never asked. ycli must write none of them, and say it in three
#      WARNINGs (up to 7.30.26 it wrote each in ~/.yuneta/configs/, and an
#      EV_EDIT_CONFIG opened the editor and left a create-config on its line).
#   2. ycli sends `read-binary-file filename=/x/good.bin`: the peer answers
#      it with an EV_READ_BINARY_FILE, written, and then with an EV_READ_FILE
#      more: one answer per request, not written. The file is written 0600,
#      data (0700 up to 7.30.26).
#   3. ycli sends it through a control center (`command-agent
#      cmd2agent='read-binary-file ...'`): the same.
#   4. A request refused at once (EV_MT_COMMAND_ANSWER -1, "Command sent to
#      0 nodes") leaves nothing pending: an EV_READ_BINARY_FILE that comes
#      with the next command (`help`) is not written.
#   5. A request whose connection ends before its answer leaves nothing
#      pending: the peer closes the connection at `read-file`, ycli connects
#      again, and an EV_READ_BINARY_FILE that comes with `help` is not written.
#   6. On that second connection the peer holds the answer of ycli's warm-up
#      (`list-gobj-commands`): a request refused meanwhile, with an answer
#      that has no "data", must still leave nothing pending -- it is not the
#      warm-up's answer, its command stack says so -- so an
#      EV_READ_BINARY_FILE that comes with the next `help` is not written.
#   7. A line that only NAMES a request is no request: `help cmd=read-file`
#      is answered as a command, and an EV_READ_BINARY_FILE that comes with
#      the next `help` is not written (up to 7.30.26 the name anywhere in
#      the line counted, and its answer never spent it).
#   8. A link left in ~/.yuneta/configs/ under the name of the file is not
#      followed: the file it points to keeps its content, and ycli says it
#      (up to 7.30.26 it was written through the link, and a failure to
#      open was taken for a success).
#   9. A content64 the peer sends that is not base64, or not a string, is
#      a WARNING (not an ERROR with a stack, nor nothing), and nothing is
#      written.
#  10. A text (read-file) and a json (edit-config) are written as the
#      binary file is: 0600, never through a link, a failure said; and a
#      write that failed does not open the editor (here a script that
#      notes what it was given) on what the file held before (up to
#      7.30.26 both followed a link, took the umask, and a failure was
#      taken for a success). A text that is not a string is a WARNING.
#
#   Usage: fake_agent.py <ycli binary> <work dir>
#   Exit 0 when every check passed.
#
#   Copyright (c) 2026, ArtGins.
#   All Rights Reserved.
#
import asyncio
import base64
import fcntl
import hashlib
import json
import os
import pty
import struct
import sys
import termios

HOST = "127.0.0.1"
PORT = 1991
YCLI = sys.argv[1]
WORK = sys.argv[2]
HOME = os.path.join(WORK, "home")
CONFIGS = os.path.join(HOME, ".yuneta", "configs")
WARNING = "answer of a request this ycli did not make, ignored"
failures = []
scenario = {"name": ""}
seen = {"commands": [], "connections": 0}
held = {}   # the warm-up of the second connection, answered when the test says
output = []
EDITOR_LOG = os.path.join(WORK, "editor.log")


def editor_opened(name):
    if not os.path.exists(EDITOR_LOG):
        return False
    with open(EDITOR_LOG) as f:
        return name in f.read()


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


def evil_binary(md, name):
    return answer("EV_READ_BINARY_FILE", md,
        {"name": name, "content64": base64.b64encode(b"EVIL").decode()})


async def agent(r, w):
    head = await r.readuntil(b"\r\n\r\n")
    key = [l.split(b":", 1)[1].strip() for l in head.split(b"\r\n")
        if l.lower().startswith(b"sec-websocket-key:")][0]
    accept = base64.b64encode(hashlib.sha1(
        key + b"258EAFA5-E914-47DA-95CA-C5AB0DC85B11").digest()).decode()
    w.write(("HTTP/1.1 101 Switching Protocols\r\nUpgrade: websocket\r\n"
        "Connection: Upgrade\r\nSec-WebSocket-Accept: %s\r\n\r\n" % accept).encode())
    seen["connections"] += 1
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
            continue
        if event != "EV_MT_COMMAND":
            continue
        command = kw.get("__command__", "")
        if command == "list-gobj-commands":
            if seen["connections"] == 2:
                held["warmup"] = (w, md)
            else:
                w.write(answer("EV_MT_COMMAND_ANSWER", md, None, "answered"))
            continue
        if scenario["name"] == "unasked":
            w.write(evil_binary(md, "evil.bin"))
            w.write(answer("EV_READ_FILE", md, {"name": "evil.txt", "zcontent": "EVIL"}))
            w.write(answer("EV_EDIT_CONFIG", md, [{"id": "evil", "zcontent": {"evil": True}}]))
        elif "nodata" in command:
            # a refusal with no "data": the shape the warm-up's intercept took for its own
            w.write(frame({"event": "EV_MT_COMMAND_ANSWER", "kw": {"result": -1,
                "comment": "Command sent to 0 nodes", "schema": None,
                "__md_iev__": reverse(md)}}))
            seen["commands"].append(command)
            continue
        elif "nope" in command:
            w.write(frame({"event": "EV_MT_COMMAND_ANSWER", "kw": {"result": -1,
                "comment": "Command sent to 0 nodes", "schema": None, "data": None,
                "__md_iev__": reverse(md)}}))
            seen["commands"].append(command)
            continue
        elif "hold" in command:
            seen["commands"].append(command)
            w.close()
            return
        elif scenario["name"] == "after-refused":
            w.write(evil_binary(md, "evil3.bin"))
        elif scenario["name"] == "after-close":
            w.write(evil_binary(md, "evil4.bin"))
        elif scenario["name"] == "after-warmup":
            w.write(evil_binary(md, "evil5.bin"))
        elif scenario["name"] == "after-named" and command == "help":
            w.write(evil_binary(md, "evil6.bin"))
        elif "badb64" in command:
            w.write(answer("EV_READ_BINARY_FILE", md, {"name": "bad64.bin", "content64": "@@@@"}))
        elif "num64" in command:
            w.write(answer("EV_READ_BINARY_FILE", md, {"name": "num64.bin", "content64": 1234}))
        elif command.startswith("read-file"):
            fname = command.split("filename=/x/", 1)[-1].split()[0]
            content = 123 if fname == "numtext.txt" else "NEWTEXT"
            w.write(answer("EV_READ_FILE", md, {"name": fname, "zcontent": content}))
        elif command.startswith("edit-config"):
            w.write(answer("EV_EDIT_CONFIG", md, [{"id": "evil7", "zcontent": {"marker": "NEWJSON"}}]))
        elif "read-binary-file" in command:
            w.write(answer("EV_READ_BINARY_FILE", md,
                {"name": "good.bin", "content64": base64.b64encode(b"GOOD").decode()}))
            w.write(answer("EV_READ_FILE", md, {"name": "evil2.txt", "zcontent": "EVIL"}))
        w.write(answer("EV_MT_COMMAND_ANSWER", md, None, "answered"))
        seen["commands"].append(command)


def written(name):
    path = os.path.join(CONFIGS, name)
    if not os.path.exists(path):
        return None
    with open(path, "rb") as f:
        return f.read()


async def wait_for(what, cond, timeout=10):
    for _ in range(int(timeout * 20)):
        if cond():
            return True
        await asyncio.sleep(0.05)
    check("the wait for %s ended in time" % what, False)
    return False


async def main():
    os.makedirs(HOME, exist_ok=True)
    server = await asyncio.start_server(agent, HOST, PORT)

    master, slave = pty.openpty()

    def controlling_tty():
        os.setsid()
        fcntl.ioctl(0, termios.TIOCSCTTY, 0)

    editor = os.path.join(WORK, "editor.sh")
    with open(editor, "w") as f:
        f.write('echo "$1" >> "%s"\ncat "$1"\n' % EDITOR_LOG)
    config = {
        "global": {"ycli.use_ncurses": 0, "ycli.editor": "sh " + editor},
        "environment": {"work_dir": WORK, "console_log_handlers": {
            "to_file": {"handler_type": "stdout"}}}     # its logs in the pty, once
    }
    p = await asyncio.create_subprocess_exec(
        YCLI, json.dumps(config),
        stdin=slave, stdout=slave, stderr=slave,
        env=dict(os.environ, HOME=HOME), preexec_fn=controlling_tty)
    os.close(slave)
    loop = asyncio.get_running_loop()
    os.set_blocking(master, False)

    def on_read():
        try:
            data = os.read(master, 65536)
        except OSError:
            data = b""
        if not data:
            loop.remove_reader(master)
            return
        output.append(data.decode(errors="replace"))

    loop.add_reader(master, on_read)

    def warnings():
        return "".join(output).count(WARNING)

    async def type_line(line, sent=True):
        n = len(seen["commands"])
        os.write(master, (line + "\n").encode())
        if sent:
            await wait_for("the command '%s'" % line, lambda: len(seen["commands"]) > n)
        await asyncio.sleep(0.5)    # its answers handled

    try:
        await asyncio.sleep(1)
        await type_line("!connect url=ws://%s:%d" % (HOST, PORT), sent=False)
        await wait_for("the connection", lambda: seen["connections"] == 1)
        await asyncio.sleep(1)

        scenario["name"] = "unasked"
        await type_line("help")
        check("unasked: an EV_READ_BINARY_FILE it did not ask is not written: %r" %
            written("evil.bin"), written("evil.bin") is None)
        check("unasked: nor an EV_READ_FILE: %r" % written("evil.txt"),
            written("evil.txt") is None)
        check("unasked: nor an EV_EDIT_CONFIG: %r" % written("evil.json"),
            written("evil.json") is None)
        check("unasked: and it says so, three warnings: %d" % warnings(), warnings() == 3)

        scenario["name"] = "asked"
        await type_line("read-binary-file filename=/x/good.bin")
        check("asked: the answer of read-binary-file is written: %r" % written("good.bin"),
            written("good.bin") == b"GOOD")
        mode = os.stat(os.path.join(CONFIGS, "good.bin")).st_mode & 0o777 \
            if written("good.bin") else 0
        check("asked: written as data, not executable: %o" % mode, mode == 0o600)
        if written("good.bin") is not None:
            os.unlink(os.path.join(CONFIGS, "good.bin"))
        await type_line("command-agent agent_id=n cmd2agent='read-binary-file filename=/x/good.bin'")
        check("asked through a control center: written: %r" % written("good.bin"),
            written("good.bin") == b"GOOD")
        check("asked: one answer per request: a second one is not written: %r" %
            written("evil2.txt"), written("evil2.txt") is None)

        scenario["name"] = "after-refused"
        await type_line("command-agent agent_id=n cmd2agent='read-binary-file filename=/x/nope'")
        await type_line("help")
        check("refused: the request refused at once leaves nothing pending: %r" %
            written("evil3.bin"), written("evil3.bin") is None)

        scenario["name"] = "after-close"
        await type_line("read-file filename=/x/hold")
        await wait_for("the connection again", lambda: seen["connections"] == 2, 15)
        await asyncio.sleep(1)
        await type_line("help")
        check("closed: a request whose connection ended leaves nothing pending: %r" %
            written("evil4.bin"), written("evil4.bin") is None)

        scenario["name"] = "in-warmup"
        await type_line("command-agent agent_id=n cmd2agent='read-binary-file filename=/x/nodata'")
        check("in warm-up: the warm-up of the connection is still pending", "warmup" in held)
        if "warmup" in held:
            w, md = held.pop("warmup")
            w.write(answer("EV_MT_COMMAND_ANSWER", md, [{"gclass": "C_AGENT", "commands": []}]))
            await asyncio.sleep(0.5)
        scenario["name"] = "after-warmup"
        await type_line("help")
        check("in warm-up: a request refused while the warm-up was pending leaves nothing pending: %r" %
            written("evil5.bin"), written("evil5.bin") is None)

        scenario["name"] = "after-named"
        await type_line("help cmd=read-file")
        await type_line("help")
        check("named: a line that only names a request leaves nothing pending: %r" %
            written("evil6.bin"), written("evil6.bin") is None)

        scenario["name"] = "asked"
        if written("good.bin") is not None:
            os.unlink(os.path.join(CONFIGS, "good.bin"))
        outside = os.path.join(WORK, "outside.bin")
        with open(outside, "wb") as f:
            f.write(b"ORIG")
        os.symlink(outside, os.path.join(CONFIGS, "good.bin"))
        await type_line("read-binary-file filename=/x/good.bin")
        with open(outside, "rb") as f:
            kept = f.read()
        check("a link: the file it points to is not written: %r" % kept, kept == b"ORIG")
        check("a link: and it says so", "Cannot write the binary file" in "".join(output))
        os.unlink(os.path.join(CONFIGS, "good.bin"))

        #   9. A content64 that is not base64, or not a string
        await type_line("read-binary-file filename=/x/badb64")
        check("bad base64: not written: %r" % written("bad64.bin"), written("bad64.bin") is None)
        check("bad base64: a WARNING, not an ERROR",
            "base64 of a peer not decodable" in "".join(output) and
            "b64_decode() FAILED" not in "".join(output))
        await type_line("read-binary-file filename=/x/num64")
        check("content64 not a string: not written, and said",
            written("num64.bin") is None and
            "content64 of a binary file that is not a string" in "".join(output))

        #   10. A text and a json: 0600, never through a link, no editor after a failure
        await type_line("read-file filename=/x/plain.txt")
        mode = os.stat(os.path.join(CONFIGS, "plain.txt")).st_mode & 0o777 \
            if written("plain.txt") is not None else 0
        check("a text: written 0600: %o" % mode, written("plain.txt") == b"NEWTEXT" and mode == 0o600)
        check("a text: the editor opened on it", editor_opened("plain.txt"))
        outside_txt = os.path.join(WORK, "outside.txt")
        with open(outside_txt, "wb") as f:
            f.write(b"ORIG")
        os.symlink(outside_txt, os.path.join(CONFIGS, "link.txt"))
        await type_line("read-file filename=/x/link.txt")
        with open(outside_txt, "rb") as f:
            kept = f.read()
        check("a text, a link: the file it points to is not written: %r" % kept, kept == b"ORIG")
        check("a text, a link: said", "Cannot write the file" in "".join(output))
        check("a text, a link: the editor not opened", not editor_opened("link.txt"))
        os.unlink(os.path.join(CONFIGS, "link.txt"))
        await type_line("read-file filename=/x/numtext.txt")
        check("a text that is not a string: said, nothing written",
            "content of a file that is not a string" in "".join(output) and
            written("numtext.txt") is None)
        outside_json = os.path.join(WORK, "outside.json")
        with open(outside_json, "wb") as f:
            f.write(b"ORIGJSON")
        os.symlink(outside_json, os.path.join(CONFIGS, "evil7.json"))
        await type_line("edit-config id=evil7")
        with open(outside_json, "rb") as f:
            kept = f.read()
        check("a json, a link: the file it points to is not written: %r" % kept, kept == b"ORIGJSON")
        check("a json, a link: said", "Cannot write the file" in "".join(output))
        check("a json, a link: the editor not opened on what it held", not editor_opened("evil7.json"))
        os.unlink(os.path.join(CONFIGS, "evil7.json"))
    finally:
        p.kill()
        await p.wait()
        server.close()
        if failures:
            print("---- ycli said ----")
            print("".join(output)[-6000:])


asyncio.run(main())
if failures:
    print("%d check(s) failed" % len(failures))
    sys.exit(1)
sys.exit(0)
