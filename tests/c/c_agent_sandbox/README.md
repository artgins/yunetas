# c_agent_sandbox test

Runs the **real agent** (`yunos/c/yuno_agent/build/yuneta_agent`) and talks to
its `1991` as a peer, with frames written by hand (`client.py`).

The agent's `1991` and `1993` and its pid file are fixed in the binary, so it
cannot run beside the agent of the machine. `run_test.sh` runs it in
namespaces of its own (`unshare -rmn --pid --fork --mount-proc`): a tmpfs on
`/yuneta`, a `/etc/passwd` where uid 0 is `yuneta`, a loopback nobody else
sees, and `/proc` of its own processes only. It builds nothing: it takes the
agent of its build dir and `outputs/yunos/logcenter`, both built by
`yunetas build`. It is skipped (exit 77, with the reason) when one of them is
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

At the end the agent is stopped in order (SIGINT) and its log must have no
*"BAD gbuf_decref()"*, no *"system memory not free"* (the gbmem audit of
`CONFIG_DEBUG_TRACK_MEMORY`), three WARNINGs *"identity card refused: its pid
is not a process of that yuno, not killed"*, and one *"identity card accepted:
its pid is not a process of that yuno, not recorded"*.

## Run

```bash
ctest -R test_c_agent_sandbox --output-on-failure --test-dir build
```
