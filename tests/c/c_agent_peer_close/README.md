# c_agent_peer_close test

Runs the **real agent** (`yunos/c/yuno_agent/build/yuneta_agent`) in the
sandbox of `c_agent_sandbox` (namespaces of its own: a tmpfs on `/yuneta`, a
`/etc/passwd` where uid 0 is `yuneta`, a loopback nobody else sees), with its
memory bounded (`ulimit -v 4000000`; none for an agent built with ASan, whose
shadow memory fits no bound) and no core, and closes its peers while
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

To hit the window, the peer stops the agent (`SIGSTOP`), writes the frames,
and continues it (`SIGCONT`): the agent reads them all in one turn of its
loop. The order of the sockets in that turn is the kernel's, so each case
writes from two peers, one connected before the peer that closes and one
after, one written before its Close and one after.

1. A yuno closes in four ways while two operators send `stats-yuno id=<id>`
   and `command-yuno id=<id> command=help` to it:

   - a Close frame;
   - a Close frame and the end of the connection;
   - a continuation frame with no message begun (a protocol error);
   - a Ping of 126 bytes (a protocol error).

   An order read after the Close must be answered -1 at once:

   ```
   yuneta_agent^<host>: stats-yuno: yuno 1 runs but is not connected to the agent (starting or stopping): nothing sent
   yuneta_agent^<host>: help: yuno 1 runs but is not connected to the agent (starting or stopping): nothing sent
   ```

   One read after the end of the yuno's connection is answered -1 too
   (*"Yuno not found"*); one read before the Close reached the yuno, which
   never answers it.
2. Two operators send `command-yuno` to the yuno, and close (a Close frame)
   while the yuno answers both: an answer after the Close of its operator is
   dropped, one before it reaches the operator.

Three cycles of each. How many orders land in each place depends on the
order the agent reads its sockets in (the kernel's: both Closes can come
before the answers, an order after the end of a connection), so the counts
are not fixed. Every answer an operator gets must be -1; the client counts
the answers *"not connected to the agent"* (an order of `help` after them on
the same operator is answered after them), and the agent log must hold as
many WARNINGs *"Not in session"*, at least one (the window was hit); and no
more than 6 answers to an operator dropped, by *"Transport closing, nothing
sent"* (one per session) or *"requester gone before its answer"*. The log
must hold no *"Event NOT DEFINED in state"*. The agent is then stopped in
order (SIGINT): it must end with 0, with no *"system memory not free"*.
What the yuno got cannot be counted: the end of its connection can take with
it what it had not read yet.

The first version of this test fixed the counts (24 *"Not in session"*, 3
*"Transport closing"*, two answers per close), and failed a correct agent
when the kernel read the sockets in another order.

The agent's boot runs its enabled yunos some 3 s after it starts
(`timerStBoot`, 0.1 s in `agent.json`): the yuno is created after that, so
that nobody but the test's cards says it runs.

Skipped (exit 77) without the agent or logcenter binary, unprivileged
namespaces or `ip`.

## Run

```bash
ctest -R test_c_agent_peer_close --output-on-failure --test-dir build
```
