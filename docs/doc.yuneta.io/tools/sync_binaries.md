(tool-sync_binaries)=
# [`sync_binaries.py`](https://github.com/artgins/yunetas/blob/7.5.3/tools/agent/sync_binaries.py)

Operator utility ([`tools/agent/sync_binaries.py`](https://github.com/artgins/yunetas/blob/7.5.3/tools/agent/sync_binaries.py)) that reconciles the freshly
built yuno binaries with what the local `yuneta_agent` already has installed,
and — after confirmation — pushes the differences via `install-binary` /
`update-binary`.

It **drives from the agent's installed binaries**, not from `outputs/yunos`:
only roles the agent already manages on this node are candidates, so it never
proposes installing a role this node doesn't run.

- **Agent side:** `ycommand -c '*list-binaries'` (the leading `*` makes ycommand
  emit raw JSON instead of the table).
- **Local side:** each installed id is looked up in `$YUNETAS_BASE/outputs/yunos/`
  — the exact directory the agent's `$$(<role>)` macro reads from on upload —
  and queried with `--print-role` for its version.

## Classification

For each binary the agent has installed:

| Status       | Condition                                  | Action           |
|--------------|--------------------------------------------|------------------|
| `BUMP`       | local version > agent version              | `install-binary` |
| `DOWNGRADE`  | local version < agent version              | `install-binary` (flagged) |
| `REBUILD`    | same version, size changed                 | `update-binary`  |
| `UP-TO-DATE` | same version, same size                    | skipped          |
| `NO-BUILD`   | agent has it, no build in `outputs/yunos`  | skipped (informational) |

It prints the candidate table, asks what to apply (all / one-by-one / quit),
then runs `install-binary` / `update-binary id=<role> content64=$$(<role>)` for
each chosen role.

## REBUILD lifecycle is automated; the bump path is not

A same-version `REBUILD` overwrites the very slot the running yuno is executing
from, so `update-binary` fails with `text-file-busy` unless the running instance
is stopped first. Once both confirmation gates (Apply-all / Proceed) are
cleared the deploy intent is explicit, so for `REBUILD` roles the script runs
the documented per-role hot-patch cycle itself, **scoped by `yuno_role`** (never
node-wide):

```{figure} ../_static/deploy_sync.svg
:alt: build → diff (*list-binaries) → classify NEW/BUMP/REBUILD/UP-TO-DATE. For REBUILD roles, per role in ascending start_priority: kill-yuno (SIGQUIT, poll until exit), update-binary, run-yuno play=0, play-yuno. Config push always succeeds; a version bump is a separate, non-automated path.
:width: 100%

The REBUILD hot-patch, scoped by `yuno_role`. Kill first — the running binary
is mapped, so `update-binary` would hit `text-file-busy` otherwise.
```

```
kill-yuno yuno_role=<role>     # only if running; SIGQUIT (orderly), so the
                               # gbmem shutdown audit runs
   ↳ poll *list-yunos until the process exits (else text-file-busy again)
update-binary id=<role> content64=$$(<role>)
run-yuno yuno_role=<role> play=0   # only if it had been running
play-yuno yuno_role=<role>         # only if it had been playing
```

Prior run/play state is read from `*list-yunos` and restored per role, so a
deliberately stopped or paused yuno is left as it was, and a role with several
instances across realms is handled in one shot (the role-scoped commands act on
every instance — they all share the one slot). Pass `--no-restart` to keep the
old print-only-reminder behaviour.

When several roles are pushed at once the restarts run in **ascending
`start_priority`** order (read from the agent via `*list-yunos`), so a `REBUILD`
brings infrastructure (logcenter/emailsender/auth_bff) back before gates and dba
instead of alphabetically; it degrades to the previous order when the agent has
no `start_priority` yet. See [`set_start_priorities.py`](set_start_priorities.md).

The **version-bump** path is still **not** automated: after an `install-binary`
the script prints the `find-new-yunos create=1` + `deactivate-snap` reminder,
because that is a node-wide bounce with broader side effects.

See [Yuno lifecycle](../../../yunos/c/yuno_agent/YUNO_LIFECYCLE.md) §6.

## Usage

```bash
tools/agent/sync_binaries.py            # interactive: show table, ask, apply
tools/agent/sync_binaries.py -n         # dry-run: print the commands, run nothing
tools/agent/sync_binaries.py -a         # apply every candidate without asking
tools/agent/sync_binaries.py --no-restart   # REBUILD: update-binary only, no kill/restart
tools/agent/sync_binaries.py -u ws://127.0.0.1:1991   # target a specific agent
tools/agent/sync_binaries.py --yunos-dir /path/to/yunos   # override the build dir
```

## See also

- [Tools](../tools.md) — overview of `tools/` (build infrastructure + agent scripts).
- [`tools/README.md`](https://github.com/artgins/yunetas/blob/7.5.3/tools/README.md).
