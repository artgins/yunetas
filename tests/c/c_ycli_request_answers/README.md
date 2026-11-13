# c_ycli_request_answers test

Runs `ycli` (its build dir: `utils/c/ycli/build/ycli`) against a peer written
in python (`fake_agent.py`) that plays the agent on `127.0.0.1:1991`. `ycli`
reads a terminal, so the peer runs it in a pty of its own, without ncurses
(`"global": {"ycli.use_ncurses": 0}`, editor `cat`), and types its lines.
Everything runs in namespaces of its own (user, mount, network: a loopback
nobody else sees, a passwd where uid 0 is `yuneta`), with `HOME` in a work dir,
so what ycli writes in `~/.yuneta/configs/` is seen. The twin of
`c_ycommand_request_answers`.

## What it pins

The agent answers its own requests (`read-file`, `read-binary-file`,
`edit-config`...) with an event of its own, and ycli writes it under
`~/.yuneta/configs/` (and, for `EV_EDIT_CONFIG`, opens the editor and leaves
a `create-config` on its line). It must do so only for a request it made on
that connection. One ycli session:

1. `!connect url=ws://127.0.0.1:1991`, then `help`: the peer sends, before the
   answer, an `EV_READ_BINARY_FILE` (`evil.bin`), an `EV_READ_FILE`
   (`evil.txt`) and an `EV_EDIT_CONFIG` (`evil`) nobody asked. None is
   written, and ycli logs the WARNING *"answer of a request this ycli did not
   make, ignored"* three times. Up to 7.30.26 the three were written.
2. `read-binary-file filename=/x/good.bin`: the peer answers it (`good.bin`,
   written) and sends an `EV_READ_FILE` more (`evil2.txt`): one answer per
   request, not written. `good.bin` is written `0600` (`0700` up to 7.30.26).
3. `command-agent agent_id=n cmd2agent='read-binary-file ...'`, as through a
   control center: the answer is written.
4. A request refused at once (`EV_MT_COMMAND_ANSWER` -1, *"Command sent to 0
   nodes"*), then `help`, with which the peer sends an `EV_READ_BINARY_FILE`:
   not written, nothing was left pending.
5. `read-file filename=/x/hold`: the peer closes the connection instead of
   answering; ycli connects again by itself, and an `EV_READ_BINARY_FILE` that
   comes with `help` is not written: the end of the connection cleared what
   was pending on it.
6. On that second connection the peer holds the answer of ycli's warm-up
   (`list-gobj-commands`) and refuses a request meanwhile with an answer that
   has no `data`: the refusal must still spend its pending slot (an
   `EV_READ_BINARY_FILE` with the next `help` is not written). The warm-up's
   intercept took such an answer for its own; it now asks the answer's command
   stack (red before: `evil5.bin` written).
7. `help cmd=read-file`, then `help`, with which the peer sends an
   `EV_READ_BINARY_FILE` (`evil6.bin`): not written. A line that only NAMES
   a request is no request: the request must be the command (the first
   word, or the first word of a `command-agent`'s `cmd2agent`). Up to
   7.30.26 the first line counted, its answer never spent it, and
   `evil6.bin` was written.
8. A link left in `~/.yuneta/configs/` under the name of the file
   (`good.bin` -> a file outside, holding `ORIG`): `read-binary-file` does
   not write through it, and logs *"Cannot write the binary file"*. Up to
   7.30.26 the file outside was overwritten.
9. A `content64` that is not base64 (`@@@@`), or not a string (`1234`):
   nothing written, a WARNING (*"base64 of a peer not decodable, refused"*,
   *"content64 of a binary file that is not a string, not written"*; not
   the ERROR *"b64_decode() FAILED"* with a stack). Up to 7.30.26: an ERROR
   with a stack, or nothing, and *"Binary file save in"*.
10. `read-file` and `edit-config`, with an editor that notes what it is
    given (`editor.sh`): a text is written `0600` and the editor opened on
    it; a link in `~/.yuneta/configs/` (`link.txt`, `evil7.json`) is not
    written through, *"Cannot write the file"* is logged, and the editor is
    not opened; a text that is not a string is a WARNING, nothing written.
    Up to 7.30.26 both followed the link, took the umask, and opened the
    editor on what they wrote through it.

Mutants killed: no guard in one answer's action (1), no decrement at a
refusal (4), the pending count kept at a disconnection (5), the file written
`0700` (2), an answer taken without spending its request (2, 4); the
`command-agent` branch of the request rule (3), no `O_NOFOLLOW` (8), a
failed open taken for a success (8); the peer's base64 read with the
ERROR decoder (9), no `O_NOFOLLOW` for a text and a json (10), the editor
opened after a failed write (10).

Skipped (exit 77) without the ycli binary, unprivileged namespaces or `ip`.

## Run

```bash
ctest -R test_c_ycli_request_answers --output-on-failure --test-dir build
```
