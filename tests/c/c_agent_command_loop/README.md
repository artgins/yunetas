# c_agent_command_loop test

Runs the **real agent** (`yunos/c/yuno_agent/build/yuneta_agent`) in the
sandbox of `c_agent_sandbox` (namespaces of its own: a tmpfs on `/yuneta`, a
`/etc/passwd` where uid 0 is `yuneta`, a loopback nobody else sees), with its
memory bounded (`ulimit -v 4000000`) and no core, and sends it on its `1991`
`command-agent` lines whose forwarded command is `command-agent` again.

## What it pins

`command-agent` forwards its `command` to a service of the agent with the
same kw, which still carries that `command`. A line that names
`command-agent` therefore runs `command-agent` again, for ever. The agent
refuses it with *"Loop command"*: the command the line names is found as the
parser finds it (its first word, in any case), in the table of the service
it goes to.

These lines must answer -1 *"…: Loop command"*:

```
command-agent command="'command-agent'"
command-agent command="command-agent service=agent"
command-agent command="command-agent "
command-agent command="COMMAND-AGENT service=agent"
command-agent service=agent command='"command-agent" x=1'
command-agent command=command-agent
command-agent          (kw: {"command": "'command-agent' service=agent"})
```

Up to 7.30.26 the guard compared the WHOLE text of `command` with the name,
so every line but `command=command-agent` passed it: the agent called itself until its
stack overflowed (SIGSEGV), writing one audit record per call (about 35 000).

Then four lines in the kw with two levels of quotes. The guard and the
parser read the first word once each (the parser's own lookup,
`command_get_cmd_desc()`), so both see a word in quotes, which is no
command: each must answer -1 *"…: command not available: '<...>'"*.

```
command-agent          (kw: {"command": "\"'command-agent'\""})
command-agent          (kw: {"command": "'\"COMMAND-AGENT\" x=1'"})
command-agent          (kw: {"command": " \t\"'command-agent'\" service=agent"})
command-agent          (kw: {"command": "'\"'command-agent'\"'"})
```

Up to 7.30.26 the parser read the first word TWICE (each read takes one
level of quotes off) and the guard once: the guard saw `'command-agent'`,
the parser `command-agent`, and the first of these lines killed the agent
the same way. Quotes cannot nest a third level (the next quote of its kind
closes a quoted word), so the last line names no command either.

Then `command-agent service=controlcenter command=list-yunos`: that service
is the agent's link to the control center (`C_IEVENT_CLI`), down in the
sandbox. It must answer -1 *"…: service 'controlcenter' not in session"* at
once, and the agent log must hold one WARNING *"Not in session"* and no
ERROR. Up to 7.30.26 the link returned NULL (*"the answer will come"*) after
an ERROR with a stack, and the peer waited for ever.

Then `command-agent command="help cmd=list-yunos"` is still forwarded and
answered, and the agent answers a `help`.

At the end the agent is stopped in order (SIGINT): it must end with 0, its
log must have no *"system memory not free"*, and its audit file must hold one
record per command sent (8 to 20 records of `command-agent`), not one per
call to itself.

Skipped (exit 77) without the agent binary, unprivileged namespaces, `ip`,
or python `websockets`.

## Run

```bash
ctest -R test_c_agent_command_loop --output-on-failure --test-dir build
```
