# yshutdown

`yshutdown` stops all Yuneta processes on the local machine, and the agent too. It has modes that stop only part of them: only the agent, all but the system yunos, or all but the agent. It also has a verbose log mode.

## Usage

```bash
yshutdown                    # stop everything
yshutdown --kill-only-agent  # only the agent (-a)
yshutdown --no-kill-agent    # stop yunos, leave agent running (-n)
yshutdown --no-kill-system   # do not stop the system yunos, e.g. logcenter (-s)
yshutdown --verbose          # verbose log (-l)
```

Run `yshutdown --help` for the full list of flags.
