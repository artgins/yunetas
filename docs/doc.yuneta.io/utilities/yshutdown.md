(util-yshutdown)=
# `yshutdown`

`yshutdown` stops Yuneta processes on the **local** machine, including the
agent. Selective modes let you bounce only part of the stack.

## Usage

```bash
yshutdown                     # stop everything (yunos + agent): SIGKILL by their pid files
yshutdown --kill-only-agent   # only the agent (-a)
yshutdown --no-kill-agent     # stop the yunos, leave the agent running (-n)
yshutdown --no-kill-system    # leave logcenter running too (-s)
```

`yuneta_agent22` is never stopped: it writes no `yuno.pid`, and the agent
is looked for by its exact name. On a node with the agents' systemd units,
`yshutdown` followed by `yuneta_agent --start` starts the agent OUTSIDE its
unit: restart it with `/yuneta/bin/restart-yuneta` (or `sudo systemctl
restart yuneta_agent`), which does `yshutdown --no-kill-agent` and restarts
the unit.

Run `yshutdown --help` for the full flag list (verbose logging and more.).

```{note}
For a controlled redeploy of a single yuno, prefer the agent flow
(`kill-yuno` / `run-yuno` via [`ycommand`](ycommand.md)) over a blanket
`yshutdown`. The two-agent design also means a node normally keeps
`yuneta_agent22` alive as an escape hatch — see
[`yuneta_agent22`](../yunos/yuneta_agent22.md).
```

## See also

- [`utils/c/yshutdown/README.md`](https://github.com/artgins/yunetas/blob/7.25.22/utils/c/yshutdown/README.md).
