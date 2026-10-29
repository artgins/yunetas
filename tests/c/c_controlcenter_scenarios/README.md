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
   when their window ends (case 11), with the five of case 10
   (`dropped=11`); likewise the second late `EV_TTY_DATA` of case 5 and the
   second PTY frame routed to nobody of case 9. Before, a count with no later
   event was never said.
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
    six of case 6 and said once by the drops timer in case 11 (`dropped=11`). A flush on every connection's
    close bounded the warnings only by the client's reconnect rate.
11. The drops timer, with a short window. Once every window the cases above
    opened is over 400 ms old, `drops_warning_window` is set to 300 ms: the
    timer says the counts left (`when="its window ended"`: case 5's frame,
    case 9's PTY frame, the 11 injected), before the PASSED and not at the
    stop, and then `drops_timer_armed` is false. Four readings for a client
    that is gone: the first is said at once (`dropped=1`), 3 are counted,
    the timer is armed, and after the window it says `dropped=3` and is not
    armed. Each step waits for the drops timer to have fired
    (`drops_timer_armed` false), checked every 100 ms for at most 5 s, so a
    stall of the loop delays the steps but cannot reorder them; a timer that
    never fires fails (*"the drops timer did not fire in time"*), and its
    counts come at the stop. (C_TIMER ticks every 100 ms here:
    `timeout_periodic` of the test's yuno.)
12. The control center paused and stopped while the yuno runs, not at the
    shutdown: no log. Before, `mt_stop()` stopped again the timers that
    `clear_timeout()` had already stopped (and the rate timer, never armed,
    that `mt_pause()` had cleared): three *"GObj NOT RUNNING"* errors with a
    stack. It runs a tick after the 3 messages of test 13, so the rates are
    not 0 before the pause; paused, they read 0 (up to 7.25.21 they kept the
    last tick's value). It is the last test.
13. The message counters and rates. The rate tick (`timeout`) is set to
    1500 ms and the stats reset; 100 `command-agent` round trips add exactly
    200 to `rxMsgs` and to `txMsgs`. Over two ticks later, the maxima hold
    the rate of the tick that saw the burst, over its exact interval:
    between 200*1000/2250 and 200*1000/1500 (a rate divided by whole seconds
    gives 200 or, read 3.5 s later, 66: both out). `stats=__reset__` zeroes
    counters, rates and maxima; then a `write-tty` and a run (its request,
    its step, the step's answer, its answer) count 3 in and 3 out. Up to
    7.25.20 all of them read 0.

14. A user who may read ONE scenario ("viewer": the test's checker grants
    it `read-scenarios` with `scenario_id` "scn" only, the scope a role's
    `parameters` gives in C_AUTHZ). `scenarios` lists "scn" alone; another
    scenario asked by id, `command-agent` and `save-scenario` of "scn" are
    refused; `scenario-runs` of "scn" answers. `watch-scenario` sends the
    agent the line the control center builds from the scenario
    (`watch-yuno-stats ids=1234 period=1000`, none of its own parameters
    along); `keys=` keeps the keys asked, keys or a node of no yuno of the
    scenario are refused (*"has no yuno to watch on node"*), `stop=1` sends
    `watch-yuno-stats stop=1`, and another scenario is refused.
    `my-permissions` says `read-scenarios: ["scn"]` and false for the rest
    to the viewer, true for everything to a user who may do everything.

15. What a user may do changed in the users store. The store's events are
    sent to the control center as C_NODE publishes them: a role created,
    updated or deleted, and a link or unlink whose parent is a role, publish
    `EV_PERMISSIONS_CHANGED` once each; an update of a user (its sessions
    change at every login) and a link of another topic publish nothing.

16. A scenario's `max_run`: its start schedules its stop, which the control
    center runs itself when it is due. The scenario "mr" (`max_run` 1 s) is
    saved and started; `scenarios` says when its stop is due (`stop_at`); a
    `max_run` out of range is refused, and while it runs a user who may
    write it but not run it cannot save it, nor delete it (*"deleting it
    needs 'run-scenarios' too"*; up to 7.29.1 the delete went through and
    took the stop with it). In the next phase the control
    center has sent the stop step by itself (*"stopping a scenario: its
    max_run is reached"*); the run is written as the control center's, with
    the reason, and the stop is no longer scheduled. Every run that ends is
    told (`EV_SCENARIO_RUN_ENDED`).
17. A stop, a pause or a report asks every yuno at once and waits for them
    all; a start or a resume goes in order. The scenario "par" has three
    yunos on the agent, one step each. Its **start**, with the second step
    answered -1: the third step is not sent, and the run ends there
    (*"start failed, 1 of 3 steps done: a step failed"*). Its **stop**: the
    agent gets the three steps at once; while they wait, `scenarios` gives
    the run in `run_in_flight` (its id, action, user, start); the steps are
    answered out of order and the second one -1, and the run still waits
    for the last one. Then it ends with -1 and *"1 of 3 steps failed; step 1
    (command-yuno id=1235 command=pause-generation): the yuno refused"*, the
    other steps keep their result 0 in `scenario-runs`, and `scenarios`
    gives no `run_in_flight`. Its **pause**, in the phase after case 16's,
    with `run_step_timeout` set to 300 ms and the second step never
    answered: in flight while it waits; once it is not (checked every
    100 ms, for at most 5 s), it names the step *"not answered in 300 ms"*,
    with one warning (*"a step of a scenario run was not answered"*), and
    the other two keep their result 0. `run_step_timeout` goes back to its
    default after. It runs before case 16, which counts the runs that end,
    and its pause runs before the drops timer of case 11 has a short
    window, so its warning has one place in the expected logs. Up to 7.28.3
    a stop ran its steps in order and ended at the first one that failed or
    did not answer: one dead yuno left the yunos after it running.
18. The SWITCH of a yuno (`run-scenario action=on|off scenario_yuno=<key>`).
    The scenario "sw": a generator "sim" whose switch has two steps in `on`
    (the second names no yuno, so it goes to "sim") and two in `off`, and a
    gate whose start and stop are one step each. `save-scenario` refuses a
    switch without its `off`, and a switch step that names no yuno of the
    scenario. `on` while the scenario is stopped is refused (*"a switch is
    turned on only while its scenario runs"*), and so are `on` without a
    `scenario_yuno` and a `scenario_yuno` with `start`; nothing reaches the
    agent. Started: `on` of the gate is refused (*"has no switch"*); `on` of
    "sim" sends its steps in order, each the line of "sim" with its service,
    and its answer names the yuno (*"on sim done, 2 steps"*). `off` sends
    both steps at once, as a stop does, and another run is refused while it
    waits. The switch runs leave the scenario running: `on` goes again.
    Stopped: `on` is refused again.
19. The stop of a `max_run` whose node is not connected. The scenario "mrx"
    (`max_run` 1 s) starts a yuno on the agent and stops one on `ghost`,
    whose agent is not connected. Past its stop, the control center has run
    it once, it did nothing (one run, failed, and the warning *"the stop of
    a scenario's max_run did nothing, tried again when a node of it
    connects"*), and `scenarios` says `stop_waits_node`. Saved again as it
    is, it still waits (in 7.29.2 the save rebuilt the stop without the
    wait, due at once, and one more failed run was written). The agent of
    `ghost` connects (`EV_ON_OPEN` from `__input_side__`): the stop goes to
    it a second later; answered, it is the second stop run, done, and no
    stop is scheduled. Up to 7.29.1 the stop was tried every minute, a run
    written each time, for as long as the node was down. It runs in phases
    3-5, between case 17 and case 11.
20. The stop of a `max_run` whose node IS connected, and refuses it. The
    scenario "mrn" (`max_run` 1 s) starts and stops a yuno on the agent.
    Past its stop, the control center has sent the stop step, answered -1
    (*"the yuno is restarting"*): one failed run, the warning *"the stop of
    a scenario's max_run did nothing, tried again in a minute"*, no
    `stop_waits_node`, and a `stop_at` a minute away. A stop by the user then
    stops it, and no stop is scheduled. In 7.29.2 it waited for an agent to
    connect, and its own was already connected: the safety stop was never
    tried again. It runs in phases 5-6, after case 19.

## Run

```bash
ctest -R '^test_c_controlcenter_scenarios$' --output-on-failure --test-dir build
```
