(tool-sync_configs)=
# `yunetas sync-configs`

Operator tool of the [`yunetas` CLI](../yunetas-cli.md) (module
`yunetas.agent_tools.sync_configs`, [source](https://github.com/artgins/tui_yunetas.py/blob/0.23.5/yunetas/agent_tools/sync_configs.py)) that
reconciles the yuno
**configs in a directory** with what the local `yuneta_agent` already has
installed, and — after confirmation — pushes the differences via `create-config`
(alias `install-config`) / `update-config`.

It is the config-side sibling of [`sync-binaries`](sync_binaries.md). The two
differ in how they discover work, because configs are **not centralized** the way
binaries are:

- `sync-binaries` drives from `$YUNETAS_BASE/outputs/yunos/` — every built
  binary lives in one well-known directory.
- Configs live scattered under each project's `yunos/batches/<host>/`
  directory, so the tool **drives from one directory**. The CLI walks the
  registered projects and runs it once for each `batches/<host>/` directory
  whose name is a realm of the agent (or the one given with `--host`).

## Identity and matching

A config in the agent is keyed by `(id, version)`:

- **`id`** is supplied as a command parameter. By convention it equals the config
  **filename without the `.json` extension** (`auth_bff.1801.json` → id
  `auth_bff.1801`, mirroring the live agent ids `scheduler_wz.5004`,
  `gate_auraair.4502`, …).
- **`version`** is *not* a parameter: the agent reads it from the `__version__`
  field **inside the file content**. `__description__` is read the same way.
  (See [`cmd_create_config`](https://github.com/artgins/yunetas/blob/7.27.10/yunos/c/yuno_agent/src/c_agent.c#L3807) / [`cmd_update_config`](https://github.com/artgins/yunetas/blob/7.27.10/yunos/c/yuno_agent/src/c_agent.c#L3980) in
  [`c_agent.c`](https://github.com/artgins/yunetas/blob/7.27.10/yunos/c/yuno_agent/src/c_agent.c).)

So for every `*.json` in the directory (skipping `_*.json` batch/deploy helpers
such as `_deploy_*.json` / `_update-configs.json`), the script derives the id
from the filename, reads `__version__` from the file (a file without it is not a
deployable config under the current agent contract and is skipped), looks the id
up in the agent, and classifies it.

- **Agent side:** `ycommand -c '*list-configs'` (the leading `*` makes ycommand
  emit raw JSON instead of the table).
- **Content upload:** the same `content64=$$(<path>)` macro as binaries — `$$()`
  base64-encodes the file. The script passes the file's absolute path so it
  resolves regardless of ycommand's working directory.

## Classification

| Status       | Condition                                  | Action                       |
|--------------|--------------------------------------------|------------------------------|
| `NEW`        | id not in the agent                        | `create-config`              |
| `BUMP`       | local version > agent version              | `create-config`              |
| `UPDATE`     | same version, content changed              | `update-config`              |
| `UP-TO-DATE` | same version, identical content            | skipped                      |
| `DOWNGRADE`  | local version < agent version              | reported only, **not** pushed |
| `agent-only` | agent has a config absent in the directory | skipped (informational)      |

A `DOWNGRADE` is never offered for install: seeding a stale version with
`create-config` will break the version logic, so it is reported and left alone.

There is no separate `install-config` command — it is an **alias of
`create-config`**, added by analogy with `install-binary`. `create-config`
refuses to overwrite an existing `(id, version)` (so it is the new-version
install). `update-config` overwrites the content of an existing same-version
record.

It prints the candidate table, asks what to apply (all / one-by-one / quit),
then runs `create-config` / `update-config id='<id>' content64=$$(<path>)` for
each chosen config.

## Restart is optional (opt-in `--restart`)

Installing a config does **not** require stopping the yuno. This is the key
difference from a binary: `update-binary` fails with `text-file-busy` while the
yuno runs from that slot, so [`sync-binaries`](sync_binaries.md) must kill
first. A config push has no such constraint — it always succeeds on a running
yuno, it just does not take effect until that yuno next **(re)starts**.

So by default this script only pushes, then prints the affected yuno ids (from
the agent record's `yunos` field. A config id is `<role>.<yuno_id>`, and the
field lists the using yuno instance ids) as a `kill-yuno` + `run-yuno` reminder.
Restarting to apply the change is a separate, optional step.

Pass `--restart` to also bounce the using yunos right away, **scoped by yuno
`id`** (never node-wide), preserving each one's prior run/play state:

```
kill-yuno id=<yuno_id>     # only if running; SIGQUIT (orderly), gbmem audit runs
   ↳ poll *list-yunos until the process exits
run-yuno id=<yuno_id> play=0   # it was running
play-yuno id=<yuno_id>         # only if it had been playing
```

A yuno that is not running is left stopped (it reads the new config on its next
start). NEW configs have no agent record yet (typically a yuno not created here),
so they are never auto-restarted — their ids are printed as a reminder.

**`--restart` restarts the yunos of an `UPDATE` only** (since CLI 0.22.0). A
yuno runs the config version of its own row (`name_version`, read by
`get_yuno_config()` in `c_agent.c`), and only `find-new-yunos create=1` +
`deactivate-snap` change that row. So a restart after a `BUMP` brings the yuno
back on the old version. For a `BUMP`, the tool prints a reminder to promote
it instead:

```
Note: create-config stored a NEW version of auth_bff.1801.
A yuno runs the config version of its own row, so a restart changes nothing.
Promote it with:
   yunetas upgrade-yunos              (shoot-snap -> find-new-yunos -> deactivate-snap)
```

The exit code is `1` when a push or a restart failed.

The affected yunos are restarted in **ascending `start_priority`** order (read
from their `*list-yunos` record), so the infrastructure starts before its
dependents instead of in alphabetical id order. See
[`set-start-priorities`](set_start_priorities.md).

See [Yuno lifecycle](../../../yunos/c/yuno_agent/YUNO_LIFECYCLE.md).

## Usage

```bash
yunetas sync-configs                       # interactive: show table, ask, apply
yunetas sync-configs -n                    # dry-run: print the commands, run nothing, ask nothing
yunetas sync-configs -a                    # apply every candidate without asking
yunetas sync-configs --show-uptodate       # also list in-sync and agent-only configs
yunetas sync-configs -r                    # push AND restart the yunos of an UPDATE
yunetas sync-configs --host my.host.com    # only that batches/<host>/ directory
yunetas sync-configs -p myproject          # only that registered project
yunetas sync-configs -n --node prod        # a registered node
```

## See also

- [Deploying yunos step by step](../deploying-yunos.md) — the scenario-driven
  deploy guide (this script is its push engine for configs).
- [`sync-binaries`](sync_binaries.md) — the binary-side sibling.
- [Tools](../tools.md) — overview of `tools/` (build infrastructure + agent scripts).
- [`tools/README.md`](https://github.com/artgins/yunetas/blob/7.27.10/tools/README.md).
