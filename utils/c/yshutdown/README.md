# yshutdown

Shut down all Yuneta processes on the local machine, including the agent. Supports selective modes (agent-only, system-only, preserve agent) and verbose logging.

## Usage

```bash
yshutdown                 # stop everything
yshutdown --agent-only    # only the agent
yshutdown --keep-agent    # stop yunos, leave agent running
```

Run `yshutdown --help` for the full list of flags.
