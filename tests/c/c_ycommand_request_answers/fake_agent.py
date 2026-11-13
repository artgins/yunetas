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
#      with an EV_READ_FILE more: one answer per request, not written. The
#      file is written 0600, data (0700 up to 7.30.26).
#   3. ycommand sends it through a control center (`command-agent
#      cmd2agent='read-binary-file ...'`): the same.
#   4. A request refused at once (EV_MT_COMMAND_ANSWER -1, "Command sent to
#      0 nodes") leaves nothing pending: an EV_READ_BINARY_FILE that comes
#      with the next command (`help`) is not written.
#   5. Straight to the agent, `read-binary-file` is answered only by its
#      EV_READ_BINARY_FILE (an alias of an event: no EV_MT_COMMAND_ANSWER),
#      as the peer does here; through a control center "Command sent to 1
#      nodes" comes first. `-c "read-binary-file ...; help"` writes the
#      file, runs `help` and ends by itself (up to the fix it waited for
#      ever), and so does the request alone.
#
#   6. A line that only NAMES a request is no request: `help cmd=read-file`
#      is answered as a command, and an EV_READ_FILE that comes with the
#      next `help` is not written (up to 7.30.26 the name anywhere in the
#      line counted, and its answer never spent it).
#   7. A link left in ~/.yuneta/configs/ under the name of the file is not
#      followed: the file it points to keeps its content (up to 7.30.26 it
#      was written through the link).
#   8. Interactive (`-i`, in a pty): the peer holds the answer of ycommand's
#      warm-up (`list-gobj-commands`) and refuses a request meanwhile, with
#      an answer that has no "data": it is not the warm-up's answer (its
#      command stack says so), so the request leaves nothing pending and an
#      EV_READ_BINARY_FILE that comes with the next `help` is not written.
#   9. A binary file that cannot be written (the link of 7) ends ycommand
#      with an exit code that says it, and stops the rest of its line (up
#      to 7.30.26 the request ended with the event's result, 0, before the
#      file was written).
#  10. Through a control center, a request the agent refuses AFTER the
#      "Command sent to 1 nodes" leaves nothing pending: the refusal carries
#      the line of its request in its command stack, as ycli reads it (up
#      to 7.30.26 the next answer nobody asked was taken).
#  11. A content64 the peer sends that is not base64, or not a string, is
#      a WARNING (not an ERROR with a stack, nor nothing), nothing is
#      written, and the exit code says it.
#  12. A text (read-file) and a json (edit-config) are written as the
#      binary file is: 0600, never through a link, a failure said; and a
#      write that failed does not open the editor on what the file held
#      before (up to 7.30.26 both followed a link, took the umask, and a
#      failure was taken for a success). A text that is not a string is a
#      WARNING, and nothing written.
#
#   Usage: fake_agent.py <ycommand binary> <work dir>
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
YCOMMAND = sys.argv[1]
WORK = sys.argv[2]
HOME = os.path.join(WORK, "home")
CONFIGS = os.path.join(HOME, ".yuneta", "configs")
failures = []
scenario = {"name": ""}
held = {}       # the warm-up of an interactive ycommand, answered when the test says
seen = {"commands": []}


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
            seen["commands"].append(command)
            if command == "list-gobj-commands":
                if scenario["name"] == "interactive":
                    held["warmup"] = (w, md)
                else:
                    w.write(answer("EV_MT_COMMAND_ANSWER", md, None, "answered"))
                continue
            if "nodata" in command:
                # a refusal with no "data": the shape the warm-up's intercept took for its own
                w.write(frame({"event": "EV_MT_COMMAND_ANSWER", "kw": {"result": -1,
                    "comment": "Command sent to 0 nodes", "schema": None,
                    "__md_iev__": reverse(md)}}))
                continue
            if scenario["name"] in ("after-named", "after-warmup") and command == "help":
                w.write(answer("EV_READ_BINARY_FILE", md,
                    {"name": "evil6.bin", "content64": base64.b64encode(b"EVIL").decode()}))
            if scenario["name"] == "unasked":
                w.write(answer("EV_READ_BINARY_FILE", md,
                    {"name": "evil.bin", "content64": base64.b64encode(b"EVIL").decode()}))
                w.write(answer("EV_READ_FILE", md, {"name": "evil.txt", "zcontent": "EVIL"}))
                w.write(answer("EV_EDIT_CONFIG", md,
                    [{"id": "evil", "zcontent": {"evil": True}}]))
            elif "nope" in command:
                w.write(frame({"event": "EV_MT_COMMAND_ANSWER", "kw": {"result": -1,
                    "comment": "Command sent to 0 nodes", "schema": None, "data": None,
                    "__md_iev__": reverse(md)}}))
                continue
            elif "refuse_later" in command:
                # through a control center: sent, and then the agent refuses it
                w.write(answer("EV_MT_COMMAND_ANSWER", md, None, "Command sent to 1 nodes"))
                w.write(frame({"event": "EV_MT_COMMAND_ANSWER", "kw": {"result": -1,
                    "comment": "User not found", "schema": None, "data": None,
                    "__md_iev__": reverse(md)}}))
                continue
            elif scenario["name"] == "after-refused-later" and command == "help":
                w.write(answer("EV_READ_BINARY_FILE", md,
                    {"name": "evil9.bin", "content64": base64.b64encode(b"EVIL").decode()}))
            elif "badb64" in command:
                w.write(answer("EV_READ_BINARY_FILE", md, {"name": "bad64.bin", "content64": "@@@@"}))
                continue
            elif "num64" in command:
                w.write(answer("EV_READ_BINARY_FILE", md, {"name": "num64.bin", "content64": 1234}))
                continue
            elif command.startswith("read-file"):
                fname = command.split("filename=/x/", 1)[-1].split()[0]
                content = 123 if fname == "numtext.txt" else "NEWTEXT"
                w.write(answer("EV_READ_FILE", md, {"name": fname, "zcontent": content}))
                continue
            elif command.startswith("edit-config"):
                w.write(answer("EV_EDIT_CONFIG", md,
                    [{"id": "evil7", "zcontent": {"marker": "NEWJSON"}}]))
                continue
            elif scenario["name"] == "after-refused":
                w.write(answer("EV_READ_BINARY_FILE", md,
                    {"name": "evil3.bin", "content64": base64.b64encode(b"EVIL").decode()}))
            elif "read-binary-file" in command:
                #   Through a control center its "Command sent to 1 nodes"
                #   comes first; straight to the agent the request's own
                #   event is its only answer (a command that is an alias of
                #   an event answers nothing else)
                through_cc = command.startswith("command-agent")
                if through_cc:
                    w.write(answer("EV_MT_COMMAND_ANSWER", md, None, "Command sent to 1 nodes"))
                w.write(answer("EV_READ_BINARY_FILE", md,
                    {"name": "good.bin", "content64": base64.b64encode(b"GOOD").decode()}))
                w.write(answer("EV_READ_FILE", md, {"name": "evil2.txt", "zcontent": "EVIL"}))
                continue
            w.write(answer("EV_MT_COMMAND_ANSWER", md, None, "answered"))


ended = {"by_itself": True}


async def ycommand(command, verbose="0"):
    """ycommand -c `command`; with `verbose` 1 its warnings are printed too.
    ended["by_itself"]: it ended on its own, not killed at 20 s"""
    env = dict(os.environ, HOME=HOME)
    p = await asyncio.create_subprocess_exec(
        YCOMMAND, "-u", "ws://%s:%d" % (HOST, PORT), "-w", "1", "--editor=cat", "-l", verbose,
        "-c", command,
        stdin=asyncio.subprocess.DEVNULL, stdout=asyncio.subprocess.PIPE,
        stderr=asyncio.subprocess.STDOUT, env=env)
    ended["by_itself"] = True
    try:
        out, _ = await asyncio.wait_for(p.communicate(), 20)
    except asyncio.TimeoutError:
        ended["by_itself"] = False
        p.kill()
        out, _ = await p.communicate()
    ended["rc"] = p.returncode
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
    mode = os.stat(os.path.join(CONFIGS, "good.bin")).st_mode & 0o777 if written("good.bin") else 0
    check("asked: written as data, not executable: %o" % mode, mode == 0o600)
    if written("good.bin") is not None:
        os.unlink(os.path.join(CONFIGS, "good.bin"))
    out = await ycommand("command-agent agent_id=n cmd2agent='read-binary-file filename=/x/good.bin'")
    check("asked through a control center: written: %r" % written("good.bin"),
        written("good.bin") == b"GOOD")
    check("asked: one answer per request: a second one is not written: %r" % written("evil2.txt"),
        written("evil2.txt") is None)

    #   5. Straight to the agent, the request's own event is its answer:
    #   the next command of the line runs, and ycommand ends. Up to the fix
    #   it waited for an EV_MT_COMMAND_ANSWER that never comes
    if written("good.bin") is not None:
        os.unlink(os.path.join(CONFIGS, "good.bin"))
    out = await ycommand("read-binary-file filename=/x/good.bin; help")
    check("a line of a request and help: the request is written: %r" % written("good.bin"),
        written("good.bin") == b"GOOD")
    check("a line of a request and help: help runs after it", "answered" in out)
    check("a line of a request and help: ycommand ends by itself", ended["by_itself"])
    out = await ycommand("read-binary-file filename=/x/good.bin")
    check("a request alone: ycommand ends by itself", ended["by_itself"])

    scenario["name"] = "after-refused"
    out = await ycommand("-command-agent agent_id=n cmd2agent='read-binary-file filename=/x/nope'; help")
    check("refused: the request refused at once leaves nothing pending: %r" % written("evil3.bin"),
        written("evil3.bin") is None)

    #   6. A line that only names a request
    scenario["name"] = "after-named"
    out = await ycommand("help cmd=read-file; help", "1")
    check("named: a line that only names a request ran, and ycommand ended",
        "answered" in out and ended["by_itself"])
    check("named: it leaves nothing pending: %r" % written("evil6.bin"),
        written("evil6.bin") is None)
    if written("evil6.bin") is not None:
        os.unlink(os.path.join(CONFIGS, "evil6.bin"))

    #   7. A link in the directory is not followed
    scenario["name"] = "asked"
    if written("good.bin") is not None:
        os.unlink(os.path.join(CONFIGS, "good.bin"))
    outside = os.path.join(WORK, "outside.bin")
    with open(outside, "wb") as f:
        f.write(b"ORIG")
    os.symlink(outside, os.path.join(CONFIGS, "good.bin"))
    out = await ycommand("read-binary-file filename=/x/good.bin", "1")
    with open(outside, "rb") as f:
        kept = f.read()
    check("a link: the file it points to is not written: %r" % kept, kept == b"ORIG")
    check("a link: and it says so", "Cannot write the binary file" in out)

    #   9. ... and the exit code says it, and the rest of the line stops
    check("a link: the exit code says the file was not written: %r" % ended["rc"],
        ended["rc"] != 0)
    seen["commands"].clear()
    out = await ycommand("read-binary-file filename=/x/good.bin; help", "1")
    check("a link, then help: help does not run: %r" % seen["commands"],
        "help" not in seen["commands"])
    check("a link, then help: the exit code says the file was not written: %r" % ended["rc"],
        ended["rc"] != 0 and ended["by_itself"])
    os.unlink(os.path.join(CONFIGS, "good.bin"))

    #   10. Refused by the agent after the control center sent it
    scenario["name"] = "after-refused-later"
    out = await ycommand(
        "-command-agent agent_id=n cmd2agent='read-binary-file filename=/x/refuse_later'; help")
    check("refused later: a request the agent refuses after it was sent leaves nothing "
        "pending: %r" % written("evil9.bin"), written("evil9.bin") is None)

    #   11. A content64 that is not base64, or not a string
    scenario["name"] = "asked"
    out = await ycommand("read-binary-file filename=/x/badb64", "1")
    check("bad base64: not written: %r" % written("bad64.bin"), written("bad64.bin") is None)
    check("bad base64: a WARNING, not an ERROR",
        "base64 of a peer not decodable" in out and "b64_decode() FAILED" not in out)
    check("bad base64: the exit code says it: %r" % ended["rc"], ended["rc"] != 0)
    out = await ycommand("read-binary-file filename=/x/num64", "1")
    check("content64 not a string: not written: %r" % written("num64.bin"),
        written("num64.bin") is None)
    check("content64 not a string: said", "content64 of a binary file that is not a string" in out)
    check("content64 not a string: the exit code says it: %r" % ended["rc"], ended["rc"] != 0)

    #   12. A text and a json: 0600, never through a link, no editor after a failure
    out = await ycommand("read-file filename=/x/plain.txt")
    mode = os.stat(os.path.join(CONFIGS, "plain.txt")).st_mode & 0o777 \
        if written("plain.txt") is not None else 0
    check("a text: written 0600: %o" % mode, written("plain.txt") == b"NEWTEXT" and mode == 0o600)
    outside_txt = os.path.join(WORK, "outside.txt")
    with open(outside_txt, "wb") as f:
        f.write(b"ORIG")
    os.symlink(outside_txt, os.path.join(CONFIGS, "link.txt"))
    out = await ycommand("read-file filename=/x/link.txt", "1")
    with open(outside_txt, "rb") as f:
        kept = f.read()
    check("a text, a link: the file it points to is not written: %r" % kept, kept == b"ORIG")
    check("a text, a link: said", "Cannot write the file" in out)
    check("a text, a link: the exit code says it: %r" % ended["rc"], ended["rc"] != 0)
    out = await ycommand("read-file filename=/x/link.txt")     # without the log, which dumps the kw
    check("a text, a link: the editor not opened on the file",
        "File NOT saved" in out and "NEWTEXT" not in out and "ORIG" not in out)
    os.unlink(os.path.join(CONFIGS, "link.txt"))
    out = await ycommand("read-file filename=/x/numtext.txt", "1")
    check("a text that is not a string: said, nothing written",
        "content of a file that is not a string" in out and written("numtext.txt") is None)
    outside_json = os.path.join(WORK, "outside.json")
    with open(outside_json, "wb") as f:
        f.write(b"ORIGJSON")
    os.symlink(outside_json, os.path.join(CONFIGS, "evil7.json"))
    out = await ycommand("edit-config id=evil7", "1")
    with open(outside_json, "rb") as f:
        kept = f.read()
    check("a json, a link: the file it points to is not written: %r" % kept, kept == b"ORIGJSON")
    check("a json, a link: said", "Cannot write the file" in out)
    check("a json, a link: the exit code says it: %r" % ended["rc"], ended["rc"] != 0)
    out = await ycommand("edit-config id=evil7")
    check("a json, a link: the editor not opened on what the file held",
        "File NOT saved" in out and "NEWJSON" not in out and "ORIGJSON" not in out)
    os.unlink(os.path.join(CONFIGS, "evil7.json"))

    #   8. Interactive: a refusal while the warm-up is pending
    scenario["name"] = "interactive"
    await interactive_warmup()

    server.close()


async def interactive_warmup():
    master, slave = pty.openpty()

    def controlling_tty():
        os.setsid()
        fcntl.ioctl(0, termios.TIOCSCTTY, 0)

    p = await asyncio.create_subprocess_exec(
        YCOMMAND, "-i", "-u", "ws://%s:%d" % (HOST, PORT), "-w", "1", "--editor=cat", "-l", "1",
        stdin=slave, stdout=slave, stderr=slave,
        env=dict(os.environ, HOME=HOME), preexec_fn=controlling_tty)
    os.close(slave)
    loop = asyncio.get_running_loop()
    os.set_blocking(master, False)
    output = []

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

    async def wait_for(what, cond, timeout=10):
        for _ in range(int(timeout * 20)):
            if cond():
                return True
            await asyncio.sleep(0.05)
        check("interactive: the wait for %s ended in time" % what, False)
        return False

    async def type_line(line):
        n = len(seen["commands"])
        os.write(master, line.encode())
        await asyncio.sleep(0.3)
        os.write(master, b"\r")     # alone: a key is matched at the start of what is read
        await wait_for("the command '%s'" % line, lambda: len(seen["commands"]) > n)
        await asyncio.sleep(0.5)    # its answers handled

    try:
        await wait_for("the warm-up", lambda: "warmup" in held)
        await type_line("command-agent agent_id=n cmd2agent='read-binary-file filename=/x/nodata'")
        if "warmup" in held:
            w, md = held.pop("warmup")
            w.write(answer("EV_MT_COMMAND_ANSWER", md, [{"gclass": "C_AGENT", "commands": []}]))
            await asyncio.sleep(0.5)
        scenario["name"] = "after-warmup"
        await type_line("help")
        check("interactive: a request refused while the warm-up was pending leaves nothing "
            "pending: %r" % written("evil6.bin"), written("evil6.bin") is None)
    finally:
        p.kill()
        await p.wait()
        if failures:
            print("---- ycommand -i said ----")
            print("".join(output)[-4000:])


asyncio.run(main())
if failures:
    print("%d check(s) failed" % len(failures))
    sys.exit(1)
sys.exit(0)
