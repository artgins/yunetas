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
- `bob` sends three answers to `dst_service` `nosuch`, and the control center
  one to `agent_requester` `nosuch`: ONE WARNING *"requester of the answer
  not found: dropped"*, and no *"child not found"* (up to 7.30.26 an ERROR for
  each).

And an answer to a session that left: an operator sends `command-agent
service=controlcenter command=hold-me`, and the control center holds its
answer; the operator closes, and another session takes its channel (the same
`input-N`, which the control center sees in `agent_requester`) and asks too.
The control center then sends both answers: the second session must get its
own, not the first one's (up to 7.30.26 it got both), and the log holds
*"requester gone before its answer, answer dropped"*.

Then a `help` is still answered, and the agent is stopped in order (SIGINT):
it must end with 0, with no *"system memory not free"*.

Skipped (exit 77) without the agent binary, `openssl`, `ip` or unprivileged
namespaces.

## Run

```bash
ctest -R test_c_agent_controlcenter_link --output-on-failure --test-dir build
```
