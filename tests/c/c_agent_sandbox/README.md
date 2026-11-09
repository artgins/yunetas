# c_agent_sandbox test

Runs the **real agent** (`yunos/c/yuno_agent/build/yuneta_agent`) and talks to
its `1991` as a peer, with frames written by hand (`client.py`).

The agent's `1991` and `1993` and its pid file are fixed in the binary, so it
cannot run beside the agent of the machine. `run_test.sh` runs it in
namespaces of its own (`unshare -rmn --pid --fork --mount-proc`): a tmpfs on
`/yuneta`, a `/etc/passwd` where uid 0 is `yuneta`, a loopback nobody else
sees, and `/proc` of its own processes only. It builds nothing: it takes the
agent of its build dir, `outputs/yunos/logcenter` and
`outputs/yunos/emailsender`, all built by `yunetas build`. It is skipped (exit 77, with the reason) when one of them is
missing, or the machine does not allow unprivileged namespaces, or `python3`
has no `websockets`.

## What it pins

1. A `write-tty` for a console that is not there, its kw with a gbuffer (a
   serialized `__gbuffer___`, which the gate makes a gbuffer of the kw): the
   agent answers `EV_TTY_CLOSE` and releases the gbuffer once. Up to 7.30.17
   it handed `json_incref(kw)` as the request of the answer, which releases
   it with `KW_DECREF`: one *"BAD gbuf_decref()"* per write.
2. `check-realm`, `dir-logs`, `dir-local-data` and `update-binary` of an id
   that is not there, with a gbuffer: the same double release, through
   `gobj_get_node()` (`C_NODE` releases its kw with `KW_DECREF`).
3. `list-yunos` and `command-yuno` with a gbuffer of 200 KB: the filter that
   `gobj_list_nodes()` releases with `JSON_DECREF` went in with
   `kw_incref()`, and every call leaked the gbuffer (18 blocks at the end of
   this test, up to 7.30.17).
4. A yuno installed and run by the agent (`logcenter`), then identity cards
   that the agent refuses, each naming a pid:
   - an unknown yuno, the pid of a decoy process: the decoy lives;
   - a second instance of the running yuno, the pid of a decoy: it lives;
   - a second instance of the running yuno, the pid of that yuno: killed (a
     process of its own);
   - an unknown yuno, pid `-1`: nothing is killed.

   Up to 7.30.17 the agent sent SIGKILL to the pid of every card it refused,
   whatever process that was. `-1` is every process of the user: the peer
   was killed too, and the test fails there.
5. `kill-yuno` of the running `logcenter` (relaunched by its watcher, its
   new pid recorded): it ends. Then a second `logcenter`, registered and not
   run, and a card the agent ACCEPTS for it with `pid: -1` and
   `watcher_pid: -1`: neither is recorded (`yuno_pid` and `watcher_pid` stay
   0, one WARNING), and a `kill-yuno` of it is refused (no pid). Up to
   7.30.17 the agent recorded the card's pids, and that `kill-yuno` sent
   SIGQUIT to every process of the user (the peer dumped a core).

6. `trace-on-yuno` and `trace-off-yuno` of the relaunched `logcenter`:
   - without `level`, or with `level=machine,no_such_level`: refused (-1),
     the global levels listed in the `data`, nothing set, `traced` false;
   - `level=machine,ev_kw`, then `level=commands` on the traced yuno: the
     yuno's `get-global-trace` (asked with `command-yuno`) says the three,
     and `traced` is true;
   - `trace-off-yuno`: `get-global-trace` says none, `traced` is false.

   Three times. Each answer of the yuno goes to the agent, which logs no
   *"a command of the agent to a yuno failed"*. Up to 7.30.21 the pair set
   the user levels of the yuno's root gobj, which has none: nothing was
   traced. Up to 7.30.18 the command named no gobj (the yuno answered
   *"gobj not found: ''"*), the answer of `trace-off-yuno` reached the peer
   as a second answer (its next question read it), and every call left the
   yunos it found in the agent (421 blocks at the end of this test).
7. A crash of the running `logcenter` (SIGSEGV; the sandbox runs with
   `ulimit -c 0`, so no core reaches the host's `/var/crash`): its watcher
   relaunches it 2 s later, and a `run-yuno` asked 1 s after the crash
   launches nothing (*"alive but not connected to the agent: not launched
   again"*: the watcher is a process of the yuno). The relaunched one is
   registered with its new pid and the same watcher, and only two processes
   of it live. It is the order of events of a relaunch by the watcher that
   meets a launch by the agent; the agent log has no *"yuno ALREADY
   living"* for it (two in all, from the cards of 4).
8. `trace-on-yuno` and `trace-off-yuno` of a yuno not connected: the
   `logcenter` is run again and traced (`machine,ev_kw`), then killed.
   `trace-off-yuno` answers -1, *"…: yuno 1 not connected to the agent
   (stopped, starting or stopping): its trace not changed"*, and `traced`
   stays true; a `trace-on-yuno` of it is refused the same way. Run again,
   the yuno has its saved levels; `trace-off-yuno` then answers 0, `traced`
   is false and the yuno traces nothing. Up to 7.30.22 `trace-off-yuno` of
   the stopped yuno answered 0 and set `traced` false, and the yuno came
   back with `ev_kw` and `machine`.
9. `delete-yuno whole=1` removes the trace the yuno saved: an
   `emailsender` (`logcenter` fixes its `domain_dir` in its binary, so the
   agent does not know where it saves) traced with `machine`, its
   `data/C_YUNO-z-persistent-attrs.json` there; killed, deleted whole: the
   file is gone; created again with the same id and run: it traces nothing,
   `traced` false. Up to 7.30.22 the file stayed and the new yuno traced
   `machine`.
10. `delete-yuno yuno_release=<its only release>` takes the whole yuno, so
    it removes the saved trace too: the same `emailsender`, run and traced
    with `machine` again, killed, deleted by its only release (the answer
    is 0 and `list-yunos` shows no row): the file is gone; created again
    with the same id and run, it traces nothing. Up to 7.30.23 only
    `whole=1` removed the file, and the new yuno traced `machine`.

The test does not have a process of another user (the sandbox maps one uid
only), so it cannot show that `find_living_yuno_pids()` skips one: it shows
that the agent still takes its own processes (the card with the yuno's own
pid is killed, `kill-yuno` ends the yuno).

At the end the agent is stopped in order (SIGINT) and its log must have no
*"BAD gbuf_decref()"*, no *"system memory not free"* (the gbmem audit of
`CONFIG_DEBUG_TRACK_MEMORY`), three WARNINGs *"identity card refused: its pid
is not a process of that yuno, not killed"*, one *"identity card accepted:
its pid is not a process of that yuno, not recorded"*, and two *"yuno
ALREADY living, killing new yuno"* (the second cards of 4).

## Run

```bash
ctest -R test_c_agent_sandbox --output-on-failure --test-dir build
```
