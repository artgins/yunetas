# c_agent_peer_close test

Runs the **real agent** (`yunos/c/yuno_agent/build/yuneta_agent`) in the
sandbox of `c_agent_sandbox` (namespaces of its own: a tmpfs on `/yuneta`, a
`/etc/passwd` where uid 0 is `yuneta`, a loopback nobody else sees), with its
memory bounded (`ulimit -v 4000000`) and no core, and closes its peers while
orders go to them. It takes `outputs/yunos/logcenter` as the binary of a yuno
that is registered and never run: the cards of the test say they are it.

## What it pins

A WebSocket Close of a peer, or a protocol error of its frames, puts the
peer's `C_WEBSOCKET` in `ST_DISCONNECTED` at once. Its `C_IEVENT_SRV` leaves
`ST_SESSION` only at `EV_ON_CLOSE`, when the TCP connection ends, on a later
turn of the agent's loop. What goes to the peer in between must not reach
the transport:

- a command or a stats (`mt_command`, `mt_stats`) is answered at once, -1
  *"`<yuno>`: service '`<name>`' not in session"*, with one WARNING *"Not in
  session"*;
- anything else (an answer, an event) is dropped, with one WARNING
  *"Transport closing, nothing sent"* per session.

Up to 7.30.26 each of them was *"Event NOT DEFINED in state"* in
`C_WEBSOCKET` (an ERROR with a stack), and the command was never answered.

To make the window certain, the peer stops the agent (`SIGSTOP`), writes the
frames, and continues it (`SIGCONT`): the agent reads them all in one turn of
its loop. The order of the sockets in that turn is the kernel's, so each case
writes from two peers, one connected before the peer that closes and one
after, one written before its Close and one after: one of them is always
read after the Close.

1. A yuno closes in four ways while two operators send `stats-yuno id=<id>`
   and `command-yuno id=<id> command=help` to it:

   - a Close frame;
   - a Close frame and the end of the connection;
   - a continuation frame with no message begun (a protocol error);
   - a Ping of 126 bytes (a protocol error).

   The pair read after the Close must be answered -1 at once:

   ```
   yuneta_agent^<host>: stats-yuno: yuno 1 runs but is not connected to the agent (starting or stopping): nothing sent
   yuneta_agent^<host>: help: yuno 1 runs but is not connected to the agent (starting or stopping): nothing sent
   ```

   The other pair reached the yuno, which never answers it.
2. Two operators send `command-yuno` to the yuno, and close (a Close frame)
   while the yuno answers both: the answer to the one that closed first is
   dropped.

Three cycles of each. The agent log must hold no *"Event NOT DEFINED in
state"*, 24 WARNINGs *"Not in session"* (two per close of case 1) and 3
*"Transport closing, nothing sent"* (one per cycle of case 2). The agent is
then stopped in order (SIGINT): it must end with 0, with no *"system memory
not free"*.

The agent's boot runs its enabled yunos some 3 s after it starts
(`timerStBoot`, 0.1 s in `agent.json`): the yuno is created after that, so
that nobody but the test's cards says it runs.

Skipped (exit 77) without the agent or logcenter binary, unprivileged
namespaces or `ip`.

## Run

```bash
ctest -R test_c_agent_peer_close --output-on-failure --test-dir build
```
