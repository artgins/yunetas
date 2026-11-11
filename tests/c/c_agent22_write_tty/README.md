# c_agent22_write_tty test

Runs the **real agent22** (`yunos/c/yuno_agent22/build/yuneta_agent22`)
linked to a control center written in python (`control_center.py`), in
namespaces of its own: a tmpfs on `/yuneta` with throwaway certificates, a
`/etc/passwd` where uid 0 is `yuneta`, an `/etc/hosts` that sends the control
center's name to a loopback nobody else sees. Its memory is bounded
(`ulimit -v 4000000`) and it dumps no core.

agent22 has no listener of its own. Everything reaches it through its link
to the control center: a `C_IEVENT_CLI` over tcp4h and TLS, pinned to
`certs/controlcenter.crt`. So the test plays the control center. It takes the
TLS connection, acks the identity card, and sends commands to `agent22`.

## What it pins

These commands come from the control center, in this order:

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
