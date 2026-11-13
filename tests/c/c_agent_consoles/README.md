# c_agent_consoles test

Runs the **real agent** (`yunos/c/yuno_agent/build/yuneta_agent`) in the
sandbox of `c_agent_sandbox` (namespaces of its own: a tmpfs on `/yuneta`, a
`/etc/passwd` where uid 0 is `yuneta`, a loopback nobody else sees), with its
memory bounded (`ulimit -v 4000000`; none for an agent built with ASan) and no
core, and opens consoles from sessions on its `1991`, websocket clients written
by hand (`client.py`). It takes `outputs/yunos/logcenter` as the binary of a
yuno that is registered and never run, whose card a session presents.

## What it pins

`agent.json` sets `console_pause_timeout` to 5000 ms (the default is 30 s)
and `console_max_tx_queue` to 2048 (8192), so the cases are quick and a route
with no flow control would be taken off soon.

1. **A session that floods its console and does not read.** The session
   opens `open-console name=flood process=yes` and reads nothing. The
   console pauses for it (`console_pause_tx_queue`), the agent's memory (its
   `VmRSS`, read in `/proc`) must grow less than 160 MB in 12 s, the agent
   must live (`help` is answered), and the console must still be there:
   the session opened it and is alone on it, so it freezes nobody else and
   the console waits for it (its process waits), past two
   `console_pause_timeout`. When the session reads, it gets more than 20 MB
   and no close; when it leaves, the console goes with it. Up to 7.30.26 the
   agent grew some 50 MB/s until it aborted (*"REACHED
   MAX_SYSTEM_MEMORY"*); before the fix the console was closed at the
   deadline. The run stops at 300 MB of growth, so a broken agent cannot
   eat the machine.
2. **A joined session that does not read, beside one that reads.** A session
   opens `paced` (`bash`) and reads; another joins it and reads nothing. The
   console prints 3,000,000 numbered lines (30 MB) at a pace. The reading
   session must get ALL of them, in order: the console pauses for the other
   one, which is taken off within its deadline, so it does not freeze the
   console of the opener for ever. `list-consoles` then shows one route,
   and when the other one reads at last, what it had waiting must end with
   the console's `EV_TTY_CLOSE`. The console goes on for the reading
   session.
3. **A session that only joins a console leaves.** A opens `j1`, B joins and
   leaves: `j1` lives, A still gets its output and no close. C joins, and A
   leaves: C gets the `EV_TTY_CLOSE` of `j1`, and `j1` is gone. Up to 7.30.26
   B's leaving closed `j1` (and killed its process), and nobody was told.
4. **The console of a session that presents the card of a yuno.** A session
   presents the card of the registered logcenter (a pid that is none: the
   agent says so with a WARNING and accepts it) and opens `fy`, which writes
   `TICK` every 10 ms, and the session drops its connection. `fy` must be gone
   (`list-consoles`), and the next session, which takes the same channel,
   must get none of its output. The session also watches the yuno
   (`watch-yuno-stats`): the next session gets none of its readings. Up to
   7.30.26 the console and the watch outlived the session, the next session
   of the channel got their output, and an INFO *"requester gone"* was logged
   per chunk in between.
5. **A slow reader.** A session with a 64 KB receive window that reads at
   about 1 MB/s opens a console that prints 11 MB at full speed (`seq`). It
   must get ALL of it, in order, and not be taken off: the console waits for
   it. With no flow control it was taken off once it fell
   `console_max_tx_queue` messages behind.
6. **An opener that does not read, beside a session that does.** A session
   opens `oflood` (`yes`) and reads nothing; another joins it and reads.
   The opener stops the other one, so at its deadline it is taken off (ONE
   WARNING, `"opener": "yes, the console is closed"`) and the console is
   closed: the joined session gets the `EV_TTY_CLOSE` of `oflood`.
7. **A session joins the console of a reader away, near its deadline.** A
   session opens `near` (`yes`) and reads nothing: it waits alone, its
   pause renewed each period. Another session joins it 2.5 to 3 s before
   the deadline of that pause (read from `list-consoles`). 4.3 s after the
   join the console must still be there with both routes, and the opener is
   taken off (ONE WARNING, the console closed) 4.6 to 7.2 s after the join:
   a whole `console_pause_timeout` (5 s) from the join, with the 1 s grain
   of the agent's timer. Before the fix the console was closed, and its
   process killed, at the deadline the opener had before the join; a period
   restarted at that deadline instead of at the join ends 7.5 s after it or
   later.
8. **A request padded with 1 MB.** A session opens `padmd` (`yes`) with a
   key of its own of 1 MB in `__md_iev__`, and reads nothing for 6 s: the
   agent grows less than 25 MB, and when it reads, the console goes on in
   frames under 16 KB (about 2 KB: the route keeps only the stack of hops and
   the `__msg_type__`). Then `padhop`, with the `host` of its hop padded: its
   frames are 1 MB, and the agent grows less than 25 MB too, the console
   paused on `console_pause_tx_bytes`. Before the fix both grew the agent
   120 MB or more and every frame carried the padding.

`run_test.sh` checks the agent's log: ONE WARNING *"console output not taken
by its session, console closed for it"* per session that did not read and
stopped another one (3: the joiner of case 2, the openers of cases 6 and
7, and two of them say `"opener": "yes, the console is closed"`), at
most 2 INFO *"requester gone before its answer"* for `EV_TTY_DATA`, no
*"Event NOT DEFINED in state"*, and the agent ends in order (exit 0) with no
*"system memory not free"*.

Skipped (exit 77) without the agent or logcenter binary, unprivileged
namespaces or `ip`. `KEEP_AGENT_LOG=<file>` keeps the agent's log.

## Run

```bash
ctest -R test_c_agent_consoles --output-on-failure --test-dir build
```
