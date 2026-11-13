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

Mutants killed: no guard in one answer's action (1), no decrement at a
refusal (4), the pending count kept at a disconnection (5), the file written
`0700` (2), an answer taken without spending its request (2, 4).

Skipped (exit 77) without the ycli binary, unprivileged namespaces or `ip`.

## Run

```bash
ctest -R test_c_ycli_request_answers --output-on-failure --test-dir build
```
