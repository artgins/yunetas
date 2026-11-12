# c_agent_consoles test

Runs the **real agent** (`yunos/c/yuno_agent/build/yuneta_agent`) in the
sandbox of `c_agent_sandbox` (namespaces of its own: a tmpfs on `/yuneta`, a
`/etc/passwd` where uid 0 is `yuneta`, a loopback nobody else sees), with its
memory bounded (`ulimit -v 4000000`; none for an agent built with ASan) and no
core, and opens consoles from sessions on its `1991`, websocket clients written
by hand (`client.py`). It takes `outputs/yunos/logcenter` as the binary of a
yuno that is registered and never run, whose card a session presents.

## What it pins

1. **A session that floods its console and does not read.** The session
   opens `open-console name=flood process=yes` and reads nothing. The agent's
   memory (its `VmRSS`, read in `/proc`) must grow less than 160 MB, the agent
   must live (`help` is answered), and the console must be closed: the
   session is the one that opened it, and it fell behind by
   `console_max_tx_queue` messages. Up to 7.30.26 the agent grew some
   50 MB/s until it aborted (*"REACHED MAX_SYSTEM_MEMORY"*). The run stops
   at 300 MB of growth, so a broken agent cannot eat the machine.
2. **A joined session that does not read, beside one that reads.** A session
   opens `paced` (`bash`) and reads; another joins it and reads nothing. The
   console prints 3,000,000 numbered lines (30 MB) at a pace a reader on the
   same machine follows. The reading session must get ALL of them, in order.
   The other one must be taken off the console (`list-consoles` shows one
   route), and when it reads at last, what it had waiting must end with the
   console's `EV_TTY_CLOSE`. The console goes on for the reading session.
3. **A session that only joins a console leaves.** A opens `j1`, B joins and
   leaves: `j1` lives, A still gets its output and no close. C joins, and A
   leaves: C gets the `EV_TTY_CLOSE` of `j1`, and `j1` is gone. Up to 7.30.26
   B's leaving closed `j1` (and killed its process), and nobody was told.
4. **The console of a session that presents the card of a yuno.** A session
   presents the card of the registered logcenter (a pid that is none: the
   agent says so with a WARNING and accepts it) and opens `fy`, which writes
   `TICK` every 10 ms, and the session drops its connection. `fy` must be gone
   (`list-consoles`), and the next session, which takes the same channel,
   must get none of its output. Up to 7.30.26 the console outlived the session,
   the next session of the channel got its output, and an INFO *"requester
   gone"* was logged per chunk in between.

`run_test.sh` checks the agent's log: ONE WARNING *"console output not taken
by its session, console closed for it"* per session that did not read (2), at
most 2 INFO *"requester gone before its answer"* for `EV_TTY_DATA`, no
*"Event NOT DEFINED in state"*, and the agent ends in order (exit 0) with no
*"system memory not free"*.

Skipped (exit 77) without the agent or logcenter binary, unprivileged
namespaces or `ip`. `KEEP_AGENT_LOG=<file>` keeps the agent's log.

## Run

```bash
ctest -R test_c_agent_consoles --output-on-failure --test-dir build
```
