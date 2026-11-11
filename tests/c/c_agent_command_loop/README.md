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
