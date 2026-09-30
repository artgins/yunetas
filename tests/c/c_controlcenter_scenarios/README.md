# c_controlcenter_scenarios test

Tests the scenarios of the control center, and the routing of what its agents
send back, on the control center's own source
(`yunos/c/controlcenter/src/c_controlcenter.c`, compiled into the test).

The control center runs as in production, between two sides played by
`C_TEST_CC_PEER`: `__top_side__`, whose channels are web clients, and
`__input_side__`, with one agent connected -- a real `C_IEVENT_SRV` in session,
whose frames are decoded by the peer below it and answered by the test as the
agent does (`msg_iev_build_response()` on the request).

Freed memory is poisoned (`0x5A`) and held in a quarantine before it is given
back (`main.c`), so a read after a free reads the poison instead of what was
there: without it the block is handed out again at once, often to a copy of
the very same string, and the read looks right.

## What it pins

1. `save-scenario` with the scenario as a string answers with its id. The
   command parser already parses a `DTP_JSON` parameter that arrives as a
   string, so the handler sees a string only when the json is itself a string
   holding the scenario (encoded twice). Up to 7.25.20 the id was read from the
   parsed json after it was freed: the answer said
   *"scenario created: ZZZZ..."*.
2. A step parameter named like a framework key (`__md_iev__`, `__username__`,
   `__md_command__`) is refused by `save-scenario`; a plain one is saved.
3. A scenario saved before those checks (written straight into the treedb by
   the test, as 7.25.14 saved it) is checked again by `run-scenario`: the run
   is refused naming the step (`step 0: command: ...`) and nothing is sent to
   the agent. Up to 7.25.20 it ran.
4. A step's answer counts only from the agent the step went to. Another client
   sends `command-agent` with the marks of the run's step in its `__md_iev__`
   (`cc_run`, `cc_step`): they do not reach the agent, and the agent's answer
   goes to that client, not to the run. A client injects the step's answer
   itself: dropped with a warning. The agent's answer ends the run. Up to
   7.25.20 the answer through `command-agent` ended the run.
5. What the agent sends back reaches a channel only while it is the connection
   that asked. A client sends `command-agent`, `stats-agent` and
   `open-console`; the console's `EV_TTY_OPEN` reaches it, and when the agent's
   connection closes the client of the mirror is dropped (up to 7.25.20 the
   agent's channel named it from a freed frame, and with the poison nobody was
   dropped). The client leaves and the next one takes its channel: the late
   answers, `EV_TTY_DATA` (two frames, one warning), `EV_TTY_OPEN` and
   `EV_TTY_CLOSE` are dropped with warnings, the agent's close drops nobody,
   and the new connection gets its own answer. Up to 7.25.20 all of them
   reached the new client.

## Run

```bash
ctest -R '^test_c_controlcenter_scenarios$' --output-on-failure --test-dir build
```
