# ybatch

Batch command runner for Yuneta services. Connects to a running yuno and
executes a file of commands — useful for automation, provisioning and smoke
tests. Authentication matches `ycommand` (JWT, OAuth2 user/password, or a config
file).

## Usage

```bash
ybatch [options] FILE
```

`FILE` is a positional argument: a file of relaxed-JSON command objects, each
with a `command` key (a leading `-` on the command, or `ignore_fail: true`, lets
the batch continue past a failure). Connection/auth flags match `ycommand`
(`--url`/`-u`, `--jwt`/`-j`, `--user_id`/`-x` + `--user_passw`/`-X`,
`--issuer`/`-I`, `--client-id`/`-Z`, `--config-file`/`-f`, …).

```text
{command: 'list-yunos'}
{command: '-stats-yuno yuno_role=logcenter'}   # leading - => continue on failure
{command: 'reset-counters', ignore_fail: true}
```

Run `ybatch --help` for all options.
