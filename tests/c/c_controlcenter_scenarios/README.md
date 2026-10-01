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
The quarantine is emptied, and closed (a block freed after is freed at once),
in `cleaning()`, before the entry point's memory check: the blocks it held are
freed memory, and counted as not free they printed a false report of ~4000
`mem-not-free` blocks on every run. A clean run prints none; a real leak is
still reported and fails the test.

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
6. A web client sends, from `__top_side__`, each event only an agent sends
   (`EV_MT_COMMAND_ANSWER`, `EV_MT_STATS_ANSWER`, `EV_TTY_OPEN`/`DATA`/`CLOSE`,
   `EV_YUNO_STATS`), routed to another client's channel with that client's
   connection: nothing reaches it, one warning each, and the sender's channel
   takes no mirror. Up to 7.25.20 all six were delivered. The warning is
   said once a minute, with `dropped=` counting the others: the one injected
   in case 4 is said, these six are counted (one warning per injected frame
   let an authenticated client flood the log). The six counted are said
   when their minute ends or, as here, when the control center stops, with
   the five of case 10 (`dropped=11`); likewise the second late `EV_TTY_DATA`
   of case 5 and the second PTY frame routed to nobody of case 9. Before, a
   count with no later event was never said.
7. Several consoles mirrored through one agent's channel. Two clients, two
   consoles: the agent's close drops both. One console opened again by another
   client: only that client is dropped (the agent routes it to the last
   opener). One client with two consoles, one closed: dropped once. Up to
   7.25.20 the channel kept one client, the last opened, cleared by the close
   of any console.
8. `command-agent` tells the agent the client takes `EV_YUNO_STATS`
   (`__relays__: ["EV_YUNO_STATS"]`) only when the client's own `__relays__`
   names it: not without one, not for another entry, not for a string; and
   only that entry is passed on. Up to 7.25.20 it said so for every client,
   and a `ycommand` watch through the control center got readings it cannot
   handle.
9. What the agent sends back for no web client goes only to the
   `C_IEVENT_CLI` link the request came in by (the control center's link to
   its own agent), named in the control center's own hop. A route whose hop
   names nobody and whose next hop (the client's) names a local service that
   listens: nothing reaches that service, a warning; the same for two PTY
   frames, one warning. A hop naming a local service that is not a link:
   dropped, a warning. A hop naming a `C_IEVENT_CLI` in session that never
   sent a request to that agent: dropped, a warning. A `command-agent` from
   that `C_IEVENT_CLI`: its answer goes back by that link; after the agent's
   connection closes, the same answer is dropped, a warning. Up to 7.25.20 the answer went to the
   service of the next hop -- the local service in the forged case, the
   control center itself (nobody) through the local agent.

10. A client connects, sends one event only an agent sends, and leaves, five
    times: no warning per loop (the cap held), the five are counted with the
    six of case 6 and said once at the stop. A flush on every connection's
    close bounded the warnings only by the client's reconnect rate.

## Run

```bash
ctest -R '^test_c_controlcenter_scenarios$' --output-on-failure --test-dir build
```
