# c_agent_answer_routing test

Runs the **real agent** (`yunos/c/yuno_agent/build/yuneta_agent`) in the
sandbox of `c_agent_sandbox` (namespaces of its own: a tmpfs on `/yuneta`, a
`/etc/passwd` where uid 0 is `yuneta`, a loopback nobody else sees), with its
memory bounded (`ulimit -v 4000000`; none for an agent built with ASan, whose
shadow memory fits no bound) and no core, and sends it answers on its `1991`.
It takes `outputs/yunos/logcenter` as the binary of a yuno that is registered
and never run: a websocket client written by hand (`client.py`) sends its
card and answers what the agent asks it.

## What it pins

An `EV_MT_COMMAND_ANSWER` or `EV_MT_STATS_ANSWER` can come from any session
of the agent. The agent takes one only from where it sent the order.

1. A session watches the yuno (`watch-yuno-stats ids=<id> period=1000`, with
   `"__relays__": ["EV_YUNO_STATS"]`): its first readings come from the yuno
   (`"data": {"reading": "of the yuno"}`), and the agent asks the yuno to
   `publish-stats`, which it takes. Then another session sends the agent a
   reading of that yuno (`__md_iev__.agent_watch`, `"data": {"forged":
   true}`) and an answer that refuses the publication
   (`__md_iev__.agent_publish`, result -1). The watcher must get no forged
   reading, the log must hold no *"publish-stats refused, the agent asks for
   these stats"*, and ONE WARNING *"answer for the agent, not from the yuno
   it names: dropped"* (a brake: one per kind and 10 s). Up to 7.30.26 the
   watcher got the reading, and the agent took the yuno as refusing.
2. A session sends `command-yuno id=<id> command=help` and closes before the
   yuno answers; another session takes its channel (the same `input-N`, as
   the yuno sees in the `src_service` of the two orders) and sends it too.
   The yuno answers both: the second session must get its own answer only,
   and the log holds *"requester gone before its answer, answer dropped"*.
   The same with `stats-yuno id=<id>`. Up to 7.30.26 the second session got
   both answers.
3. The yuno answers to `authz`, a service of the agent that is no
   requester: ONE WARNING *"requester of the answer not found: dropped"*.
   Up to 7.30.26 the answer was sent to that service.

The log must hold no *"Event NOT DEFINED in state"*. The agent is then
stopped in order (SIGINT): it must end with 0, with no *"system memory not
free"*.

The agent's boot runs its enabled yunos some 3 s after it starts
(`timerStBoot`, 0.1 s in `agent.json`): the yuno is created after that, so
that nobody but the test's card says it runs.

Skipped (exit 77) without the agent or logcenter binary, unprivileged
namespaces or `ip`.

## Run

```bash
ctest -R test_c_agent_answer_routing --output-on-failure --test-dir build
```
