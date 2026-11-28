# c_agent22_write_tty test

Runs the real agent22 (`yunos/c/yuno_agent22/build/yuneta_agent22`)
linked to a control center written in python (`control_center.py`), in
namespaces of its own: a tmpfs on `/yuneta` with throwaway certificates, a
`/etc/passwd` where uid 0 is `yuneta`, an `/etc/hosts` that sends the control
center's name to a loopback nobody else sees. Its memory is bounded
(`ulimit -v 4000000`, but none for an agent22 built with ASan, whose shadow memory
fits no bound) and it dumps no core.

agent22 has no listener of its own. Everything reaches it through its link
to the control center: a `C_IEVENT_CLI` over tcp4h and TLS, pinned to
`certs/controlcenter.crt`. So the test plays the control center. It takes the
TLS connection, acks the identity card, and sends commands to `agent22`.

## What it pins

These commands arrive from the control center, in this order:

```
write-tty name=nosuch content64=YQ==
write-tty name=__yuno__ content64=YQ==
write-tty name=agent22 content64=YQ==
write-tty name=nosuch
help
```

- The first three must each be answered with `EV_TTY_CLOSE` naming the
  console, and log one WARNING (*"console not found"*, then *"not a
  console"* twice).
- The fourth (no `content64`) must log one WARNING *"content64 required"*.
- The link must stay up through all of them: the `help` after them is
  answered.
- At the end the agent is stopped in order (SIGINT). It must end with 0, and
  its log must have no *"system memory not free"*.

Then an operator whose only role is `agent22-install-package`
(`deployer@sandbox.test`). The control center signs its operator
assertions as a real one does: ES256, with a key that the test makes, whose
public half is in `Authz.assertion_jwks` of the agent22's json with the
`kid` `test-controlcenter`. `control_center.py` signs with the `openssl`
command, so the test needs no python package. The principal of the link
opens a console (`open-console name=c1 process=cat`), and then:

```
AS deployer@sandbox.test write-tty name=c1 content64=YQ==
write-tty name=c1 content64=Ygo=
AS deployer@sandbox.test open-console name=c2 process=cat cwd=/yuneta
AS deployer@sandbox.test upgrade-agent content64=YQ==
AS deployer@sandbox.test install-package-status
close-console name=c1
```

- The key of the operator must be answered with `EV_TTY_CLOSE` naming `c1`,
  and log one WARNING *"write-tty refused: no permission to open-console"*
  with its `username`.
- The console must go on for the principal: the console echoes its key in
  `EV_TTY_DATA`.
- `open-console` and `upgrade-agent` of the operator must be refused with
  *"No permission to '<permission>'"*, and its `install-package-status` must
  run: its role grants it.

Up to 7.32.4 `write-tty` asked no permission, and the key of that operator
reached the console.

Then the control center opens a console that floods (`open-console name=f1
process=yes`, and the principal of the link, `controlcenter`, is given the role
`root` in the agent22's json) and does not read its link for 8 s:
`agent22`'s memory (`VmRSS`) must grow less than 160 MB, and when the link is
read again the console must go on (5 MB more) and `close-console` close it.
The console pauses while the link has `console_pause_tx_queue` messages
waiting, and waits for it (the link is its only route). Up to 7.30.26
`agent22` grew some 50 MB/s until it aborted. The run stops at 300 MB of
growth. Then the same twice with the request padded with 1 MB (`FLOODPAD
md`: a key of the control center's own in `__md_iev__`, and `FLOODPAD hop`: the
`host` of its hop), the link not read for 6 s: `agent22` must grow less than
25 MB, and with the key of its own every frame of output must be under 16 KB
(the route keeps only the stack of hops and the `__msg_type__`, and a padded hop
pauses the console on `console_pause_tx_bytes`). Before the fix both grew
`agent22` some 250 MB and every frame carried the padding. Another `help`
after them is answered.

Up to 7.30.26 a write-tty for a console that is not there (a key typed right
after the console closed) and one without `content64` logged an ERROR and
sent `EV_DROP` to the link. That took down the whole control channel of the
node for one stray key. A write-tty to a service that is not a console was
sent to it: `__yuno__` answered *"Event NOT DEFINED in state"* with a stack,
and agent22 wrote to itself.

Skipped (exit 77) without the agent22 binary, unprivileged namespaces, `ip`
or `openssl`.

## Run

```bash
ctest -R test_c_agent22_write_tty --output-on-failure --test-dir build
```
