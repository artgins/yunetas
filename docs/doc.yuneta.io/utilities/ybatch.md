(util-ybatch)=
# `ybatch`

`ybatch` runs a **file of commands** against a running yuno, one after another,
each waiting for the previous response. It is the unattended, file-driven member
of the control-plane family — use it for automation, provisioning, and smoke
tests. For interactive use or one-off commands reach for [`ycommand`](ycommand.md)
instead; `ybatch` is what you point at a checked-in script.

## Usage

```bash
ybatch [options] FILE
```

`FILE` is a positional argument: the path to the commands file (see
[Commands file format](#ybatch-file-format) below). The connection and
authentication options match [`ycommand`](ycommand.md).

```bash
# Run a provisioning script against the local agent
ybatch provision.ybatch

# Against a remote agent, authenticating via OIDC discovery
ybatch --url wss://host:1993 \
       --issuer https://auth.example.com/realms/yuneta/ \
       --client-id yuneta --user_id alice --user_passw '••••••' \
       provision.ybatch
```

Run `ybatch --help` for the full flag list.

(ybatch-file-format)=
## Commands file format

The file is a sequence of relaxed-JSON objects (single quotes and unquoted keys
are accepted), each with a `command` key. Objects are read by brace matching, so
they may span multiple lines and any text outside a `{ ... }` block is ignored
(handy for free-form comments between commands).

```text
# A ybatch script (text outside { } is ignored)

{command: 'list-yunos'}

{command: 'stats-yuno yuno_role=logcenter'}

# Keep going even if this one fails:
{command: '-command-yuno yuno_role=logcenter command=reset-counters'}

# Equivalent, explicit form:
{command: 'reset-counters', ignore_fail: true}
```

| Field | Meaning |
|-------|---------|
| `command` | The command string, exactly as you would type it in [`ycommand`](#util-ycommand) (with `arg=val`, `service=…`, etc.). Required. |
| `ignore_fail` | If `true`, a failure of this command does not abort the batch. |
| `-` prefix | A leading `-` on the command value is shorthand for `ignore_fail: true` (the same convention `ycommand` uses). |

By default the first failing command stops the batch; mark the commands that are
allowed to fail with the `-` prefix or `ignore_fail`.

## Options

| Option | Short | Purpose |
|--------|-------|---------|
| `--url=<url>` | `-u` | Yuno to connect to. Default `ws://127.0.0.1:1991`. |
| `--yuno_role=<role>` | `-O` | Remote yuno role. |
| `--yuno_name=<name>` | `-o` | Remote yuno name. |
| `--yuno_service=<svc>` | `-S` | Target service. Default `__default_service__`. |
| `--config-file=<file>` | `-f` | Load settings from a JSON config file (or files). |
| `--print` | `-p` | Print the resulting configuration and exit. |
| `--verbose=<level>` | `-l` | Verbose level. |

Authentication uses the same OAuth2 / OIDC flags as `ycommand`
(`--issuer`/`-I`, `--token-endpoint`/`-T`, `--end-session-endpoint`/`-E`,
`--client-id`/`-Z`, `--user_id`/`-x`, `--user_passw`/`-X`, `--jwt`/`-j`) — see
[Authentication (OAuth2 / OIDC)](#ycommand-auth).

## See also

- [`ycommand`](ycommand.md) — interactive / single-command control plane; it can
  also run inline batches with `;` chaining, `!source`, or stdin piping.
- Repository README (for code navigators):
  [`utils/c/ybatch/README.md`](https://github.com/artgins/yunetas/blob/7.7.0/utils/c/ybatch/README.md).
