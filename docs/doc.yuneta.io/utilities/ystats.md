(util-ystats)=
# `ystats`

`ystats` subscribes to the **live statistics** of a running yuno and redraws
them on a fixed interval. Where [`ycommand`](ycommand.md) sends one request and
prints one response, `ystats` keeps an open subscription and refreshes — use it
to watch counters move (throughput, queue depth, connection counts) while a
system is under load.

## Usage

```bash
ystats --url ws://host:port [options]
```

```bash
# Watch the logcenter's stats, refreshing every 2 seconds
ystats --url ws://127.0.0.1:1991 --yuno_role logcenter -t 2

# Watch a specific statistic and a gobj attribute
ystats -s txMsgs -a connected -g my_tcp_server
```

Run `ystats --help` for the full flag list.

## Options

| Option | Short | Purpose |
|--------|-------|---------|
| `--refresh_time=<n>` | `-t` | Refresh interval in seconds (default `1`). |
| `--stats=<names>` | `-s` | Statistic(s) to request. |
| `--attribute=<names>` | `-a` | Attribute(s) to request. |
| `--command=<cmd>` | `-c` | Command to execute. |
| `--gobj_name=<name>` | `-g` | Target gobj for the attribute (named gobj or full path). |
| `--with-metadata` | `-m` | Include metadata in the output. |
| `--url=<url>` | `-u` | Yuno to connect to. Default `ws://127.0.0.1:1991`. |
| `--yuno_role=<role>` | `-O` | Remote yuno role. |
| `--yuno_name=<name>` | `-o` | Remote yuno name. |
| `--yuno_service=<svc>` | `-S` | Target service. Default `__default_service__`. |
| `--config-file=<file>` | `-f` | Load settings from a JSON config file (or files). |
| `--print` | `-p` | Print the resulting configuration and exit. |

Authentication uses the same OAuth2 / OIDC flags as [`ycommand`](#util-ycommand)
(`--issuer`/`-I`, `--token-endpoint`/`-T`, `--end-session-endpoint`/`-E`,
`--client-id`/`-Z`, `--user_id`/`-x`, `--user_passw`/`-X`, `--jwt`/`-j`) — see
[Authentication (OAuth2 / OIDC)](#ycommand-auth).

## See also

- [`ycommand`](ycommand.md) — one-shot `stats` / `stats-yuno` requests when you
  do not need a live refresh.
- Repository README (for code navigators):
  [`utils/c/ystats/README.md`](https://github.com/artgins/yunetas/blob/7.5.6/utils/c/ystats/README.md).
