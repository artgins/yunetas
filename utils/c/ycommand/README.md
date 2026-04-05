# ycommand

Control-plane CLI for Yuneta services. Sends commands and stats requests to a running yuno over its local socket or a remote WebSocket URL. Supports single-command mode and an interactive shell.

## Usage

```bash
ycommand -c '<command>'                     # single command on the default yuno
ycommand --url ws://host:port -c 'help'     # remote yuno
ycommand                                    # interactive shell
```

Authentication options: `--jwt`, `--user`/`--password`, or a config file. Run `ycommand --help` for all flags.
