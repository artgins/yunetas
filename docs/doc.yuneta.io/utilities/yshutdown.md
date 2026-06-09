(util-yshutdown)=
# `yshutdown`

`yshutdown` stops Yuneta processes on the **local** machine, including the
agent. Selective modes let you bounce only part of the stack.

## Usage

```bash
yshutdown                 # stop everything (yunos + agent)
yshutdown --agent-only    # only the agent
yshutdown --keep-agent    # stop the yunos, leave the agent running
```

Run `yshutdown --help` for the full flag list (verbose logging, etc.).

```{note}
For a controlled redeploy of a single yuno, prefer the agent flow
(`kill-yuno` / `run-yuno` via [`ycommand`](ycommand.md)) over a blanket
`yshutdown`. The two-agent design also means a node normally keeps
`yuneta_agent22` alive as an escape hatch — see
[`yuneta_agent22`](../yunos/yuneta_agent22.md).
```

## See also

- [`utils/c/yshutdown/README.md`](https://github.com/artgins/yunetas/blob/7.5.7/utils/c/yshutdown/README.md).
