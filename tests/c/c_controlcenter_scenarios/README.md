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
   itself: dropped with a warning. Its answer is a frame whose bytes the test
   writes by hand (`iev_create_to_gbuffer()` leaves `__temp__` out of every
   frame, so it cannot write this one), decoded as `__top_side__`'s
   `C_IEVENT_SRV` decodes it (`iev_create_from_gbuffer()`), that carries a
   `__temp__` of its own, `[0x41414141]`: the warning names the client's
   channel. Without the deletion of a peer's `__temp__` in
   `iev_create_from_gbuffer()` the test dies here with SIGSEGV. The agent's answer ends the run. Up to 7.25.20 the answer
   through `command-agent` ended the run; up to 7.30.26 the peer's `__temp__`
   was kept, C_IOGATE could not write the channel into a list, and the warning
   read `0x41414141` as the channel: the control center died with SIGSEGV.
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
    last tick's value). It is the last test but case 44, which runs after
    its stop.
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
    published to the control center by the store (a service
    `treedb_authzs` of the test) as C_NODE publishes them: a role created,
    updated or deleted, and a link or unlink whose parent is a role, publish
    `EV_PERMISSIONS_CHANGED` once each; an update of a user (its sessions
    change at every login) and a link of another topic publish nothing. The
    same `EV_TREEDB_NODE_UPDATED` of `roles` sent three times by another gobj
    publishes nothing, with ONE warning (*"event of the users store not sent
    by the store, ignored"*). Up to 7.30.26 each told every client to ask
    `my-permissions` again. Mutant killed: the check of the source removed.

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
    (command-yuno id=1235 command=pause-load): the yuno refused"*, the
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

21. The operator assertion (7.30.9). The agent's identity card says
    `knows_operator_assertion`, and the control center has an
    `operator_assertion_key` (a test key that `main.c` writes to
    `/tmp/test_c_controlcenter_scenarios_assertion.jwk` for the run). A
    `command-agent` from *alice@example.com* reaches the agent with an
    `__operator_assertion__`. The test verifies it with the public key, as the
    agent's `C_AUTHZ` does: `iss` `controlcenter`, `aud` the agent's node
    uuid, `sub` the user, and 60 s of life. A `command-agent` with no user
    reaches the agent with none. Every step of a parallel run carries one
    too. This yuno has no `C_AUTHZ`, so the test sets libjwt's allocators
    itself. Without them, the control center signs nothing.

22. The key of a node (7.30.10), run first. Agent sessions present a key
    (`peer_cert.spki_sha256` in the `EV_ON_OPEN`) or none:
    - Only recording (`enforce_node_keys` off): a node never seen is recorded
      `pending` with its key and let in. `accept-node` pins the key it
      presented. Another key is let in, with a WARNING.
    - With `enforce_node_keys`:
      - another key is dropped (the channel gets `EV_DROP`), and no
        `command-agent` reaches that node;
      - the accepted key is let in;
      - a node never seen is dropped;
      - an agent with no key is let in (`allow_nodes_without_key`);
      - `refuse-node` drops the agent in session (its wire gets `EV_DROP`).
    - `accept-node` of a node that presented no key is refused, and so is a
      user without `manage-nodes`.
    - The control center's `mt_authenticate` (7.30.11): an agent with the
      accepted key and no token is authenticated as `agent:<uuid>`, reaching
      `controlcenter`. Another key, with no token, is refused (a WARNING). A
      node never seen, with no token, is refused and kept in memory only
      (`knocking` in `list-nodes`); a second key for it does not replace the
      first; `accept-node` with the key creates its record, and it logs in.
      `accept-node` without `spki_sha256` is refused.
    - `refuse-node` drops the agent in session with an INFO (*"A session of
      the node dropped"*).

23. A node is reached by its uuid, and by a hostname only when one node has
    it. Nodes A and B are accepted with their keys, `enforce_node_keys` on:
    - A gives B's hostname, its channel first: two nodes have that name (a
      WARNING when the second opens), so `command-agent`, `stats-agent` and
      `write-tty` by it are refused and reach neither; `write-tty` without
      `agent_id` reaches no agent (a WARNING each); B's uuid reaches B;
    - A gives B's uuid as its hostname, its channel first: B's uuid reaches B,
      for `command-agent` and `stats-agent`;
    - A back with its own name: B's hostname reaches B;
    - a session with no key (`allow_nodes_without_key`) that gives B's
      hostname, its channel first, takes nothing either.

    Up to 7.30.14 the first channel whose hostname or uuid matched took it,
    and `write-tty` went to every match (to all agents without `agent_id`).
24. One key, one node: `accept-node` of a key accepted for another node is
    refused (a WARNING), and nothing is written. Up to 7.30.14 it was
    accepted.
25. The card of an agent not authenticated yet: an `id` that is not a string
    or not shaped as a node uuid (a backtick, 300 characters), and a `host`
    that is not a string or is longer than a host name, are refused (a
    WARNING each), and none of them is kept knocking. Up to 7.30.14 a value
    that was not a string gave an ERROR with a stack, and the rest was kept.
26. A refused node whose agent presents its key: the WARNING's `refusal` says
    *"The node is refused"*. Up to 7.30.14 it said *"another key"*.
27. An open with nothing changed (hostname, address, key) writes nothing in
    `nodes` (its `g_rowid` stays), though its link comes from a new port: the
    port is no change. A new hostname is written, and so is a new address.
    Up to 7.30.14 every open wrote the record; in 7.30.15 every open from a
    new port did.
28. A decision drops the sessions of the node that it no longer lets in (an
    INFO each). `enforce_node_keys` off: `accept-node` with the key a session
    logged in by (`agent:<uuid>`) keeps it; with a new key it drops it, and
    keeps a session of a token; `refuse-node` drops a session logged in by
    the key. `enforce_node_keys` on: a new key drops a session that presents
    the old one. Up to 7.30.14 such sessions stayed until they reconnected.
29. `logout-user` asks the permission `logout-user` ("viewer" has not: the
    `authz` service, played by the peer, gets nothing), and passes `disabled`
    only when given: absent, `true` or `false` as given. Up to 7.30.14
    anybody logged anybody out, and a missing `disabled` went as `false` (a
    disabled user was enabled again).

30. A hostname cannot be planted, nor taken by a uuid. Node C is accepted
    with its key, `enforce_node_keys` on, its agent logged in by the key with
    `host-c`:
    - a session of a token that claims C's uuid with another hostname is
      refused, and C's record keeps `host-c` (only a session that logged in
      by an accepted node's key writes its hostname and address);
    - a session of a token whose card `id` IS `host-c` takes nothing by that
      name (the uuid of a session and the hostname of another node), and C's
      uuid still reaches C;
    - a session of a token with another uuid that gives `host-c` takes
      nothing either (two records have the name);
    - the record of a refused node (`keyed`, case 22) names nobody: the one
      node that is not refused and has `keyed` gets it.

    Before this, the refused open renamed C's record, so the last session was
    the only `host-c` and got C's commands; and a card `id` of `host-c` was
    matched as a uuid, first.
31. `drop-agent` names a node as `command-agent` does. `enforce_node_keys`
    off: A, by a token, gives the hostname that B logged in with by its key;
    `drop-agent` by that name drops B's session and not A's. Up to 7.30.14 it
    dropped every session whose uuid or card host matched.

32. The hostname of a node without a key is not taken through its record.
    `enforce_node_keys` on, `allow_nodes_without_key` (production): node P
    is in session with no key, its card `host-np`.
    - A token with a key that claims P's uuid is refused, and P's record
      keeps `host-np`.
    - A token with no key that claims P's uuid (let in), and another uuid
      that gives `host-np`: `command-agent` and `write-tty` by `host-np`
      reach nobody.
    - Both gone, `host-np` reaches P.

    In 7.30.15 either open renamed P's record, and the other uuid got P's
    commands and console keys; gone, P was not reached by its name.
33. An open leaves no name behind. Node B is accepted, its hostname
    `host-p13`, `enforce_node_keys` on. A token with a key and a new uuid
    that gives `host-p13` (`allow_nodes_without_key` off) is refused and kept
    `knocking`, not written; a token with no key and a new uuid that gives
    it (`allow_nodes_without_key` on) is let in, and goes. Then `host-p13`
    reaches B. In 7.30.15 each left a record with the name, and B's name was
    two nodes' until a `refuse-node` of the other uuid.
34. `enforce_node_keys` off: a session of a token that claims an accepted
    node's uuid, first, does not get what goes to the node: the session that
    logged in by the key gets it. And a session of a token whose card has an
    `id` or a `host` that is not a string is refused (a WARNING each);
    `command-agent`, `list-agents` and another agent's open after it log
    nothing. A `knows_operator_assertion` that is not a boolean (`1`,
    `"yes"`) is no: `command-agent` reaches that agent with no
    `__operator_assertion__`, and nothing logs. In 7.30.15 the first session
    got it, the bad card was let in, with an ERROR from each of those, and
    the non-boolean gave an ERROR with a stack at each command.
35. A scenario ("amb") with a yuno on a name that two nodes without a key
    give: its start is refused, saying so (*"is the hostname of more than
    one node"*), and no step is sent; its stop stops the yuno on the agent
    and names the other step. In 7.30.15 the start ran its first step, and
    both said *"not connected"*.
36. The stop of a `max_run` on such a name. The scenario "mra" (`max_run`
    1 s) starts on the one node that gives `host-mra`; a second node gives
    it then. Past the stop, the stop ran once, its step naming the name of
    more than one node, and it is tried again in a minute (*"... tried again
    in a minute"*, no `stop_waits_node`). The second node goes, and a stop by
    the user stops it. In 7.30.15 it waited for a node to connect, for ever.
    It runs in phases 2-3, between case 17 and case 19.
37. `accept-node` does not keep the name that a session of a token wrote.
    `enforce_node_keys` off. Node B is accepted with its key, its hostname
    `host-q12b`. Node P presented its key once (a `pending` record). Then a
    session of a token with no key claimed P's uuid and gave `host-q12b`.
    `accept-node` of P with its key empties its hostname and address, and
    `host-q12b` reaches B. Node R, whose last session presented the key
    accepted, keeps its hostname. In 7.30.16 P was accepted with the name
    `host-q12b`, and the name named two nodes until P logged in by its key.
38. The stop of a `max_run` that does some steps only. The scenario "mrp"
    (`max_run` 1 s) has a yuno on the agent and one on `host-mrp`, a node in
    session by a token, which goes after the start. Past the stop, the run
    did 1 of 2 steps, the warning says *"... did not do every step, the rest
    tried again when a node of it connects"*, and `scenarios` says
    `stop_waits_node`. The node comes back: the stop goes to it only (the
    agent gets nothing), and when it is answered nothing is scheduled. Then a
    start and a stop by the user, whose second step fails: nothing is
    scheduled (the rest of a user's stop is not the `max_run`'s). In 7.30.16
    the stop that did some steps ended the schedule with no log, and the
    yuno on `host-mrp` continued to run. It runs in phases 6-8, after case 20
    and before case 11.
39. A request that carries a gbuffer, as `kw_deserialize()` leaves one of a
    peer's `"__gbuffer___"` (`test_commands_with_gbuffer`): `command-agent`
    and `stats-agent` that reach the agent (which gets the bytes),
    `drop-agent` of a node that is not there, `watch-scenario` of no
    scenario, and `command-agent`, `stats-agent` and `drop-agent` of a user
    who may not. After each, the gbuffer holds only the test's reference.
    In 7.30.16 the handlers copied the kw with `json_deep_copy()` (the
    integer without its reference) and released the copy with `KW_DECREF`:
    the gbuffer was freed while the test still held it, refused requests
    included.
40. `write-tty` (`test_write_tty_refused`). Of a user who may not ("viewer"):
    its kw is released once (the test holds a reference and reads it back),
    the client gets one `EV_MT_COMMAND_ANSWER` (*"No permission to
    'write-tty'"*), and nothing reaches the agent. Of a user who may, with a
    gbuffer: the agent gets the bytes, and the gbuffer holds only the
    test's reference. In 7.30.17 the refusal released the kw twice (the
    poison read back as its refcount) and answered `EV_WRITE_TTY`, which no
    client takes (*"Event NOT DEFINED"*); the kw given to the agent was the
    request itself, whose gbuffer the agent's gate serialized away, so one
    reference was never released.
41. Who may write a scenario but not run it does not make the control
    center run what it writes. The scenario "mrw" (`max_run` 1 s; the
    "writer" of `main.c` may write it) has a yuno on the agent and one on
    `host-mrw`, a node in session by a token that goes after the start.
    Past the stop, the stop did the agent's step only, and the rest waits
    for `host-mrw`. The writer saves "mrw" with another stop (`command-yuno
    id=9999 service=__yuno__ command=shutdown`), and deletes it: both
    refused (*"saving it needs 'run-scenarios' too"*, *"deleting it needs
    'run-scenarios' too"*), the stop still pending. The step of `host-mrw`
    is changed in the treedb itself (not by `save-scenario`, which schedules
    again), and the node comes back: the rest is due and none of its steps
    is in the scenario, so the stop does not run (*"... cannot run, tried
    again in a minute"*), and nothing reaches the agent nor the node. A user
    who may run it saves that stop: the rest is dropped, and no stop is
    scheduled. In 7.30.17 the stop that did some
    steps made the scenario look stopped, so the writer's save and delete
    went through; and a rest none of whose steps was in the scenario ran the
    whole new stop. It runs in phases 8-10, after case 38.
42. A `max_run` stop whose step is answered *"Yuno not found"* is not done:
    the agent answers it also for a yuno that lives and has not reached it
    yet (an agent just restarted). The run is a failed stop, the warning
    *"... tried again in a minute"*, and `scenarios` says a `stop_at` a
    minute away, no `stop_waits_node`. A stop by the user then stops it:
    nothing is scheduled. In 7.30.18 such a stop was done, and the
    `max_run` of a yuno not connected again yet was lost. The scenario
    "mrg", in phases 10-11.
43. The `max_run` stop is not tried again after 10 tries in a row that did
    nothing (`MAX_RUN_STOP_TRIES`), and that end is said once (*"the stop of
    a scenario's max_run did nothing in its last tries: not tried again"*)
    and shown: `scenarios` says `stop_given_up` and no `stop_at`. The
    scenario runs, so the "writer" (who may not run it) cannot save it; a
    user who may run it saves it as it is, and the stop is tried again (a
    `stop_at`, no `stop_given_up`); a stop by the user then stops it. The
    scenario "mrc": the 9 tries before the last are written into the
    treedb by the test, as the control center writes them (a minute apart,
    they would take 9 minutes); the last, answered -1, ends it. In 7.30.17
    a step that answered an error for ever was tried every minute, for
    ever; in 7.30.18 the stop given up was shown nowhere, and a save did not
    try it again. In phases 11-12.
44. A control center with no `operator_assertion_key` (after case 12's
    stop, which drops the key and closes the treedb, so the agent is named
    by its uuid): `command-agent` and `stats-agent` with a gbuffer reach the
    agent with the bytes, and the gbuffer holds only the test's reference.
    In 7.30.17 the kw for the agent was the request itself (see case 40).
45. A gate does not change a kw that somebody else holds. A caller keeps a
    kw with a gbuffer, a `__md_iev__` route and a `__md_command__` /
    `__md_stats__`, and hands `gobj_command()` and `gobj_stats()` a
    `kw_incref()` of it, on the agent's `C_IEVENT_SRV` and on a
    `C_IEVENT_CLI` in session over a test peer: the frame carries the bytes,
    the caller's kw is equal to a copy taken before, its refcount is 1 and
    the gbuffer's 1 (released with it; the memory audit at the end finds no
    leak). In 7.30.17 `mt_command()` and `mt_stats()` of both gates wrote
    their stacks in the caller's kw and serialized its gbuffer away.
46. A `max_run` stop that cannot even start writes no run: its attempts
    count towards the same 10. The scenario "mrb": started, 9 failed tries
    written into the treedb as in case 43 and counted by a save of "mrb" as
    it is; then its `stop` is taken away in the treedb itself. Past the
    stop: nothing reaches the agent, no stop is scheduled, and one warning
    (*"the stop of a scenario's max_run cannot run: not tried again"*). In
    7.30.17 it was tried every minute, for ever. In phases 12-13.
47. The agent's link is closing: the transport below its `C_IEVENT_SRV` is
    put in `ST_DISCONNECTED` (`C_TEST_CC_PEER`'s, as a peer's Close leaves
    `C_WEBSOCKET` before `EV_ON_CLOSE`), and the link refuses every order at
    once, -1 *"... not in session"*, with one WARNING *"Not in session"* each
    (six, in the expected logs). `command-agent` and `stats-agent` answer -1
    *"...: service 'agent-1' not in session: Command sent to 0 nodes"* (and
    *"Stats ..."*); the start of the scenario "cls" (two yunos, its steps in
    order) ends at once, *"start failed, 0 of 2 steps done: a step failed"*,
    and its stop (both steps at once) too, *"stop failed, 0 of 2 steps done:
    2 of 2 steps failed"*, each step written with -1 and the link's reason;
    a `write-tty` is answered `EV_MT_COMMAND_ANSWER` -1 with it; nothing
    reaches the agent. Up to 7.30.26 the control center dropped the link's
    answer: *"Command sent to 1 nodes"*, a run that waited for its whole
    `run_step_timeout` (and every later run refused, *"a run is in flight"*),
    and keystrokes lost with no answer. In case 0, after case 45.

Case 22 also checks that `accept-node` of a node that knocked under another
key does not take the hostname it knocked with. Its refused opens of nodes
never seen are kept knocking, not written (7.30.15 wrote them `pending`).

The channel of a session has a `peername` (an attribute of `C_TEST_CC_PEER`,
as `C_TCP`'s), with a new port at every session, as a link that connects
again.

Sessions are opened as a token's (no `__username__`) or as a login by the key
(`agent:<uuid>` in the session and in its `EV_ON_OPEN`, as `C_IEVENT_SRV`
gives it); cases 23, 27 and 28 use the key logins.

The agent of `node1` opens its session as in production (`EV_ON_OPEN`), so its
node has a record in `nodes`, whose hostname names it. Case 0's copy, from
another host, renames that record, so the agent opens its session again after
it. Case 0's report is a WARNING (an ERROR up to 7.30.14), and the expected
logs check the priority (`msg|when|dropped|refusal|priority` in `main.c`).

## Run

```bash
ctest -R '^test_c_controlcenter_scenarios$' --output-on-failure --test-dir build
```
