# ybatch

`ybatch` runs a file of commands against a running yuno. Use it for
automation, provisioning and smoke tests. The authentication is the same as
in `ycommand` (JWT, OAuth2 user/password, or a configuration file).

## Usage

```bash
ybatch [options] FILE
```

`FILE` is a positional argument. It is a file of relaxed-JSON command objects.
Each object has a `command` key. If a command fails, the batch stops, with two
exceptions. The batch continues if the command starts with `-`, or if the
object has `ignore_fail: true`.

The connection and authentication flags are the same as in `ycommand`
(`--url`/`-u`, `--jwt`/`-j`, `--user_id`/`-x` + `--user_passw`/`-X`,
`--issuer`/`-I`, `--client-id`/`-Z`, `--config-file`/`-f`, …).

```text
{command: 'list-yunos'}
{command: '-stats-yuno yuno_role=logcenter'}   # leading - => continue on failure
{command: 'reset-counters', ignore_fail: true}
```

Run `ybatch --help` to see all the options.
