# c_agent_controlcenter_link test

Runs the **real agent** (`yunos/c/yuno_agent/build/yuneta_agent`) in the
sandbox of `c_agent_sandbox` (namespaces of its own: a tmpfs on `/yuneta`, a
`/etc/passwd` where uid 0 is `yuneta`, a loopback nobody else sees), with its
memory bounded (`ulimit -v 4000000`; none for an agent built with ASan, whose
shadow memory fits no bound) and no core, and links it to a control center
written in python (`client.py`). The operators are websocket clients written
by hand: the test needs no python package.

The agent's link to the control center is a `C_IEVENT_CLI` over tcp4h and TLS,
pinned to `certs/controlcenter.crt`, to
`tcps://<machine>.<node_owner>.<__output_url__>`. The test makes throwaway
certificates, sets `node_owner` to `sandbox` and `__output_url__` to
`cc.test:19898` (`agent.json`), and sends that name to the loopback with an
`/etc/hosts` of the sandbox. The control center acks the card of the link and
answers each request with its routing reversed, as a `C_IEVENT_SRV` does: the
answer goes to the `src_service` the request came from.

## What it pins

An operator on the agent's `1991` sends, with the link in session:

```
command-agent service=controlcenter command=list-yunos
stats-agent service=controlcenter
```

Each must be answered with what the control center said (result 0, its
comment). The request goes up with `src_service` `agent`, and the agent sends
the answer on to the operator by the requester it put in
`__md_iev__.agent_requester`. The agent log must hold no *"service of the
event not found, event dropped"*.

Up to 7.30.26 the request went up with the operator's channel of the agent
(`input-0`) as its `src_service`. That is no service: the link dropped the
answer (*"service of the event not found, event dropped"*) and the operator
waited for its own timeout.

Then answers that a session sends to the agent:

- `bob`, a user with a password and a role for the service `agent` only
  (`create-user` through `command-agent service=authz`), sends an
  `EV_MT_COMMAND_ANSWER` and an `EV_MT_STATS_ANSWER` to `agent` with
  `__md_iev__.agent_requester` `controlcenter`. The control center must get
  neither (up to 7.30.26 it got both, with the agent's identity), and the log
  holds ONE WARNING *"answer naming a requester of a link, not from a link:
  dropped"* (a brake: one per kind and 10 s).
- `bob` sends three answers to `dst_service` `nosuch` and two to an empty
  `dst_service`, and the control center one to `agent_requester` `nosuch`:
  ONE WARNING *"requester of the answer not found: dropped"*, and no
  *"child not found"* or *"name NULL"* (up to 7.30.26 an ERROR for each).
- The first of each kind of bob's answers carries `"__temp__":
  [1094795585]`, a number where the agent keeps a pointer; the control
  center sends `EV_PLAY_YUNO_ACK` / `EV_PAUSE_YUNO_ACK` with no `__temp__`
  (its link has no channel), with a failed result and with that `__temp__`,
  and a reading (`agent_watch`) and a publication (`agent_publish`) with a
  `__temp__` of its own. The agent must live (a `help` is answered), with
  ONE WARNING *"ack not from a yuno: dropped"* and no *"gobj NULL"* or
  *"hs_yuno NULL"*. Up to 7.30.26 the agent took the number for a gobj and
  died (SIGSEGV), and an ack of the control center gave ERRORs with a stack.
- `bob` answers to its own channel (its `input-N`, seen by the control center
  in `agent_requester`) with a single hop in `ievent_gate_stack`: the agent
  pops its own and has no route back. ONE WARNING *"answer with no route back
  to its requester: dropped"*, and no *"__md_iev__ stack NOT FOUND"* (up to
  7.30.26 a WARNING and six ERRORs per answer).

And an answer to a session that left: an operator sends `command-agent
service=controlcenter command=hold-me`, and the control center holds its
answer; the operator closes, and another session takes its channel (the same
`input-N`, which the control center sees in `agent_requester`) and asks too.
The control center then sends both answers: the second session must get its
own, not the first one's (up to 7.30.26 it got both), and the log holds
*"requester gone before its answer, answer dropped"*. The same with
`stats-agent service=controlcenter stats=hold-me`.

Then a `help` is still answered. The control center opens a console through
the link (`open-console name=t3 process=cat cwd=/yuneta`; the principal of a
command of the link, `controlcenter`, is a user with the role `root` in
`agent.json`), writes to it (`write-tty`) and closes it (`close-console`):
its `EV_TTY_OPEN`, its output (`EV_TTY_DATA`) and its `EV_TTY_CLOSE` go up the
link. A `read-file` by the link is answered up it with `EV_READ_FILE`. They
did already up to 7.30.26: the agent now sends the events of a console with
`EV_SEND_IEV`, as its answers, and this is the route that must stay.

The control center opens a console that floods (`open-console name=t4
process=yes`) and stops reading the link. The console pauses for the link;
the agent's memory (`VmRSS`) must grow less than 160 MB, and the console must
be closed: the link took nothing within `console_pause_timeout` (3 s in
`agent.json`), and it opened the console (ONE WARNING *"console output not
taken by its session, console closed for it"*). When the control
center reads again, what it had waiting ends with the `EV_TTY_CLOSE` of `t4`.
Up to 7.30.26 the agent grew until it aborted. The run stops at 300 MB of
growth.

The control center then plays and pauses
the agent through its link: it sends `EV_PLAY_YUNO`, and, in ONE write, an
order (`list-yunos`), `EV_PAUSE_YUNO` and the first two bytes of the next
order, then the rest of it and 40 orders more. Only a yuno's own agent may
play or pause it, so the link refuses both: each ack has result -1, every
one of the 42 orders is answered with 0, the operator is still answered,
the `1991` still takes a session, and the link stays (no new connection).
Up to 7.30.26 the pause reached the agent's `mt_pause`, which stops that
same link inside its own action and closes the treedbs: the agent died
(SIGSEGV in `C_PROT_TCP4H`, which read on after its own stop), or, without
the extra bytes, it lived with its `1991` and its link closed for good.

The control center opens a console (`bash`) that writes `TICK` every 10 ms,
and drops its connection: the link is out of session while the console
writes. The agent says it ONCE, an INFO *"requester gone before its answer,
answer dropped"* for `EV_TTY_DATA` (up to 7.30.26 one per chunk), the link
connects again (its `timeout_between_connections`, 10 s) and the console
writes up the new connection; `close-console` closes it.

Last, the control center sends 300 orders
(`list-yunos`) to the agent: the first 150 one by one, each answered with 0;
then the client sends the agent a SIGINT and the other 150 at once. The agent
must end in order, with 0 and no *"system memory not free"*, and no answer
may name an order never sent. Up to 7.30.26 the pause of the stop closed the
treedbs with the link still in session, and an order read the treedb freed
(SIGSEGV).

Skipped (exit 77) without the agent binary, `openssl`, `ip` or unprivileged
namespaces.

## Run

```bash
ctest -R test_c_agent_controlcenter_link --output-on-failure --test-dir build
```
