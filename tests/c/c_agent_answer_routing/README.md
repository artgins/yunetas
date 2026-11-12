# c_agent_answer_routing test

Runs the **real agent** (`yunos/c/yuno_agent/build/yuneta_agent`) in the
sandbox of `c_agent_sandbox` (namespaces of its own: a tmpfs on `/yuneta`, a
`/etc/passwd` where uid 0 is `yuneta`, a loopback nobody else sees), with its
memory bounded (`ulimit -v 4000000`; none for an agent built with ASan, whose
shadow memory fits no bound) and no core, and sends it answers on its `1991`.
It takes `outputs/yunos/logcenter` as the binary of two yunos that are
registered and never run: a websocket client written by hand (`client.py`)
sends the card of each and answers what the agent asks them.

## What it pins

An `EV_MT_COMMAND_ANSWER` or `EV_MT_STATS_ANSWER` can come from any session
of the agent. The agent takes one only from where it sent the order.

1. A session watches the yuno (`watch-yuno-stats ids=<id> period=1000`, with
   `"__relays__": ["EV_YUNO_STATS"]`): its first readings come from the yuno
   (`"data": {"reading": "of the yuno"}`), and the agent asks the yuno to
   `publish-stats`, which it takes. Then another session sends the agent a
   reading of that yuno (`__md_iev__.agent_watch`, `"data": {"forged":
   true}`) and an answer that refuses the publication
   (`__md_iev__.agent_publish`, result -1). The watcher must get no forged
   reading, the log must hold no *"publish-stats refused, the agent asks for
   these stats"*, and ONE WARNING *"answer for the agent, not from the yuno
   it names: dropped"* (a brake: one per kind and 10 s). The link of the
   OTHER yuno sends the same two, naming the first one: dropped too. Up to
   7.30.26 the watcher got the reading, and the agent took the yuno as
   refusing.

   Then a session sends where the agent keeps a pointer: a reading and a
   publication with a `__temp__` of its own (`[1094795585]`,
   `{"channel_gobj": 1094795585}`), and `EV_PLAY_YUNO_ACK` /
   `EV_PAUSE_YUNO_ACK` with those and with none. The agent must live (a
   `help` is answered), and the acks are dropped with ONE WARNING *"ack not
   from a yuno: dropped"*, no *"hs_yuno NULL"* and no *"gobj NULL"*. Up to
   7.30.26 the agent took the number for a gobj and died (SIGSEGV), and an
   ack from a session was read as a yuno's (ERRORs with a stack).

   The same session sends the events of a console (`EV_TTY_OPEN`,
   `EV_TTY_DATA`, `EV_TTY_CLOSE`, 60 frames, with a kw of any shape) and
   the agent's own requests as events (`EV_EDIT_CONFIG`, `EV_READ_FILE`
   and the other seven that its commands `edit-config`, `read-file`...
   send, 18 frames). They are dropped with ONE WARNING *"console event not
   from a console: dropped"* and ONE *"request of the agent not by its
   command: dropped"* (brakes), with no *"named dict not found"*, no
   *"path MUST BE a json str"* and no *"Event NOT DEFINED in state"*. Up
   to 7.30.26: an ERROR *"named dict not found"* per console frame, and the
   agent answered each request to the gate of its channels, an ERROR
   *"Event NOT DEFINED in state"* with a stack per frame.
2. A session sends `command-yuno id=<id> command=help` and closes before the
   yuno answers; another session takes its channel (the same `input-N`, as
   the yuno sees in the `src_service` of the two orders) and sends it too.
   The yuno answers both: the second session must get its own answer only,
   and the log holds *"requester gone before its answer, answer dropped"*.
   The same with `stats-yuno id=<id>` and `authzs-yuno id=<id>`. Up to
   7.30.26 the second session got both answers.
3. The yuno answers to `authz`, a service of the agent that is no
   requester: ONE WARNING *"requester of the answer not found: dropped"*.
   Up to 7.30.26 the answer was sent to that service.

4. What those drops must let through. The agent's requests asked by their
   commands are answered, each with its own event: `read-file` and
   `read-json` of `/yuneta/agent/yuneta_agent.json`, `running-keys` and
   `running-bin` of the yuno. A real console (`open-console name=t1
   process=cat cwd=/yuneta`) opened by a session and joined by another
   (`open-console name=t1`): both get its `EV_TTY_OPEN`, what it writes
   after a `write-tty` of the second one (`EV_TTY_DATA`, the text written
   echoed back), and its `EV_TTY_CLOSE` after a `close-console`. Up to
   7.30.26 neither got any of them: the agent sent them to the session's
   channel, which takes no such event (an ERROR *"Event NOT DEFINED in
   state"* with a stack per frame), and the second one got only its own
   `EV_TTY_OPEN` again. A console goes with the session that opened it:
   `list-consoles` lists it while the session is there, and not after it
   left. Up to 7.30.26 the agent kept the console of a session in the
   session's `C_IEVENT_SRV` and looked for it in the channel: it was never
   closed, and it wrote to the next session of that channel.

Last, a session watches the yuno again and the agent asks it to publish;
with the watch still armed the client sends the agent a SIGINT. The yuno
must get its `publish-stats` with `stop`, and the log must hold no ERROR
*"hgobj NULL or DESTROYED"*. Up to 7.30.26 the stop ended the watch after
the pause had closed the treedb: that ERROR with a stack, and the yuno was
told nothing.

The log must hold no *"Event NOT DEFINED in state"*. The agent must end
in order (exit 0), with no *"system memory not free"*.

The agent's boot runs its enabled yunos some 3 s after it starts
(`timerStBoot`, 0.1 s in `agent.json`): the yuno is created after that, so
that nobody but the test's card says it runs.

Skipped (exit 77) without the agent or logcenter binary, unprivileged
namespaces or `ip`.

## Run

```bash
ctest -R test_c_agent_answer_routing --output-on-failure --test-dir build
```
