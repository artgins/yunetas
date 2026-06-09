(util-ytests)=
# `ytests`

`ytests` runs a JSON-described **test suite** against a live yuno, over the same
control-plane connection (and the same authentication) as
[`ycommand`](ycommand.md) / [`ybatch`](ybatch.md). The `--repeat` / `--interval`
options make it a simple soak-tester: replay the suite N times (or forever) at a
fixed cadence.

## Usage

```bash
ytests --url ws://host:port [options] <test.json>
```

```bash
# Run a suite once against the local agent
ytests --url ws://127.0.0.1:1991 mytests.json

# Soak: repeat forever
ytests --url ws://127.0.0.1:1991 -t -1 mytests.json
```

Run `ytests --help` for the full flag list.

## Options

| Option | Short | Purpose |
|--------|-------|---------|
| `<FILE>` | — | The test-suite JSON (positional) |
| `--repeat=<n>` | `-t` | Repeat the run `n` times; `-1` = infinite loop (default `1`) |
| `--url=<url>` | `-u` | Yuno to connect to. Default `ws://127.0.0.1:1991` |
| `--yuno_role=<role>` | `-O` | Remote yuno role |
| `--yuno_name=<name>` | `-o` | Remote yuno name |
| `--yuno_service=<svc>` | `-S` | Target service. Default `__default_service__` |
| `--config-file=<file>` | `-f` | Load settings from a JSON config file |
| `--print` | `-p` | Print the resulting configuration and exit |
| `--print-role` | `-r` | Print the basic yuno information |

Authentication uses the same OAuth2 / OIDC flags as [`ycommand`](#util-ycommand) (`--issuer`/`-I`,
`--token-endpoint`/`-T`, `--end-session-endpoint`/`-E`, `--client-id`/`-Z`,
`--jwt`/`-j`) — see [Authentication (OAuth2 / OIDC)](#ycommand-auth).

## See also

- [`ycommand`](ycommand.md) — one-shot commands; [`ybatch`](ybatch.md) — scripted batches.
- [`utils/c/ytests/README.md`](https://github.com/artgins/yunetas/blob/7.5.9/utils/c/ytests/README.md).
