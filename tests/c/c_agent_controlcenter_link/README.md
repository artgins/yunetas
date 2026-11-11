# c_agent_controlcenter_link test

Runs the **real agent** (`yunos/c/yuno_agent/build/yuneta_agent`) in the
sandbox of `c_agent_sandbox` (namespaces of its own: a tmpfs on `/yuneta`, a
`/etc/passwd` where uid 0 is `yuneta`, a loopback nobody else sees), with its
memory bounded (`ulimit -v 4000000`) and no core, and links it to a control
center written in python (`client.py`).

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

Then a `help` is still answered, and the agent is stopped in order (SIGINT):
it must end with 0, with no *"system memory not free"*.

Skipped (exit 77) without the agent binary, python `websockets`, `openssl`,
`ip` or unprivileged namespaces.

## Run

```bash
ctest -R test_c_agent_controlcenter_link --output-on-failure --test-dir build
```
