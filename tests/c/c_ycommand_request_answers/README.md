# c_ycommand_request_answers test

Runs `ycommand` (its build dir: `utils/c/ycommand/build/ycommand`) against a
peer written in python (`fake_agent.py`) that plays the agent on
`127.0.0.1:1991`, in namespaces of its own (user, mount, network: a loopback
nobody else sees, a passwd where uid 0 is `yuneta`), with `HOME` in a work
dir, so what ycommand writes in `~/.yuneta/configs/` is seen.

## What it pins

The agent answers its own requests (`read-file`, `read-binary-file`,
`edit-config`...) with an event of its own, and ycommand writes it under
`~/.yuneta/configs/` (and, for `EV_EDIT_CONFIG`, opens the editor and leaves
a `create-config` on its line). It must do so only for a request it made:

1. `ycommand -c help`: the peer sends, before the answer, an
   `EV_READ_BINARY_FILE` (`evil.bin`), an `EV_READ_FILE` (`evil.txt`) and an
   `EV_EDIT_CONFIG` (`evil`) nobody asked. None is written, and ycommand logs
   the WARNING *"answer of a request this ycommand did not make, ignored"*
   three times (run with `-l 1` to see it). Up to 7.30.26 the three were
   written.
2. `ycommand -c 'read-binary-file filename=/x/good.bin'`: the peer answers
   it (`good.bin`, written) and sends an `EV_READ_FILE` more (`evil2.txt`):
   one answer per request, not written. `good.bin` is written `0600` (data,
   not a program; `0700` up to 7.30.26).
3. `ycommand -c "command-agent agent_id=n cmd2agent='read-binary-file ...'"`,
   as through a control center: the answer is written.
4. A request refused at once (`EV_MT_COMMAND_ANSWER` -1, *"Command sent to 0
   nodes"*), then `help`, with which the peer sends an `EV_READ_BINARY_FILE`:
   not written, nothing was left pending.
5. Straight to the agent, a request is answered ONLY by its own event (the
   command is an alias of an event: no `EV_MT_COMMAND_ANSWER`), and the peer
   does so (through a control center, *"Command sent to 1 nodes"* comes
   first). `ycommand -c "read-binary-file filename=/x/good.bin; help"`
   writes `good.bin`, runs `help` and ends by itself; so does the request
   alone. Up to the fix ycommand waited for an answer that never comes: it
   neither ran `help` nor ended (killed at 20 s here).
6. `ycommand -c "help cmd=read-file; help"`: a line that only NAMES a
   request is no request (the request must be the command: the first word,
   or the first word of a `command-agent`'s `cmd2agent`). The peer sends an
   `EV_READ_BINARY_FILE` (`evil6.bin`) with the second `help`: not written.
   Up to 7.30.26 the first line counted as a request, its answer never spent
   it, and `evil6.bin` was written.
7. A link left in `~/.yuneta/configs/` under the name of the file
   (`good.bin` -> a file outside, holding `ORIG`): `read-binary-file` does
   not write through it, and logs *"Cannot write the binary file"*. Up to
   7.30.26 the file outside was overwritten.
8. Interactive (`-i`, in a pty; the Enter key is written alone, as a key is
   matched at the start of what is read): the peer holds the answer of the
   warm-up (`list-gobj-commands`) and refuses `command-agent ...
   cmd2agent='read-binary-file filename=/x/nodata'` with an answer that has
   no `data`; then it releases the warm-up, and sends an
   `EV_READ_BINARY_FILE` (`evil6.bin`) with the next `help`: not written.
   Up to 7.30.26 the refusal was taken for the warm-up's answer and the
   request stayed pending.
9. The link of 7 again: ycommand's exit code is not 0; and with
   `read-binary-file ...; help` the `help` does not run (the command ends
   with what became of the file). Up to 7.30.26 the request ended with the
   event's result (0) before the file was written.
10. Through a control center, `command-agent ...
    cmd2agent='read-binary-file filename=/x/refuse_later'; help`: the peer
    answers *"Command sent to 1 nodes"* and then refuses it (-1, its line in
    the command stack), and sends an `EV_READ_BINARY_FILE` (`evil9.bin`)
    with `help`: not written. Up to 7.30.26 the request stayed pending.
11. A `content64` that is not base64 (`@@@@`), or not a string (`1234`):
    nothing written, a WARNING (*"base64 of a peer not decodable,
    refused"*, *"content64 of a binary file that is not a string, not
    written"*; not the ERROR *"b64_decode() FAILED"* with a stack), exit
    code not 0. Up to 7.30.26: an ERROR with a stack, or nothing, and exit
    code 0.
12. `read-file` and `edit-config`: a text is written `0600`; a link in
    `~/.yuneta/configs/` (`link.txt`, `evil7.json`) is not written through,
    *"Cannot write the file"* is logged, the editor is not opened on the
    file (its content is not printed), and the exit code is not 0; a text
    that is not a string is a WARNING, nothing written. Up to 7.30.26 both
    followed the link, took the umask, and printed what they wrote through
    it.

Mutants killed: no undo of a request refused after "Command sent to 1
nodes" (10), the command ended before the file was written (9), the peer's
base64 read with the ERROR decoder (11), no `O_NOFOLLOW` (7, 9, 12), the
editor opened after a failed write (12).

Skipped (exit 77) without the ycommand binary, unprivileged namespaces or
`ip`.

## Run

```bash
ctest -R test_c_ycommand_request_answers --output-on-failure --test-dir build
```
