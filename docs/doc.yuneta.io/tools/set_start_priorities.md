# set_start_priorities.py

Assigns each managed yuno's **`start_priority`** — the agent's per-yuno
launch-order band `0..9` — by role, in one shot.

The agent launches yunos in **ascending** `start_priority` (utilities first) and
stops them **descending** (utilities last, so logcenter captures everyone else's
shutdown). A fresh agent leaves every yuno at the default `5`, so the order is
effectively insertion order until the tiers are assigned. This tool reads the
yunos from `*list-yunos`, maps each role to a tier, and writes only the
differences with `update-node`.

Run it **once per node**: a version-bump deploy (`find-new-yunos create=1`) now
inherits `start_priority` / `sched_priority` / `cpu_core` from the prior release
of each id, so the tiers survive bumps — re-running after every deploy is no
longer needed (it remains idempotent if you do).

It is the third agent helper alongside
[`sync_binaries.py`](sync_binaries.md) and [`sync_configs.py`](sync_configs.md),
and shares their OAuth2-once + `-j` plumbing, so it can drive a remote `wss://`
agent.

## Tiers

Rules are matched in order; the **first match wins**, and explicit `--rule`
entries are matched **before** the built-ins. Lower priority starts earlier.

| Tier | Roles (built-in) |
|------|------------------|
| `1` | `logcenter`, `emailsender`, `auth_bff` (framework utilities) |
| `4` | `gate_*` |
| `7` | `db_*` |
| `5` | everything else — **left untouched** unless a rule matches |

A `PATTERN` is an exact role name or an `fnmatch` glob (`gate_*`, `*_wz`). App
roles (which are deployment-specific) are **not** built in — assign them with
`--rule`:

```bash
set_start_priorities.py --rule 'agregador_wz=8' --rule 'scheduler_wz=8'
```

## How it writes

The agent's `update-node` reads its record from `content64` (base64 JSON); the
inline `record={...}` form is **not** coerced by the ycommand CLI. The tool
therefore base64-encodes a strict-JSON `{ "id": ..., "start_priority": ... }`
per changed yuno and sends:

```
command-agent service=treedb_yuneta_agent command=update-node topic_name=yunos content64=<base64>
```

Only yunos whose `start_priority` actually changes are written. New yunos born
with the `util` tag already default to tier `1` at `create-yuno`, so they
usually need no action here.

## Usage

```bash
tools/agent/set_start_priorities.py             # interactive: show plan, ask, apply
tools/agent/set_start_priorities.py -n          # dry-run: show the plan, write nothing
tools/agent/set_start_priorities.py -a          # apply without asking
tools/agent/set_start_priorities.py --show-all  # also list yunos already at target / with no rule
tools/agent/set_start_priorities.py --rule 'gate_*=3' --rule 'reporter=2'
tools/agent/set_start_priorities.py -u ws://127.0.0.1:1991   # target a specific agent
```

## See also

- [Yuno lifecycle](../../../yunos/c/yuno_agent/YUNO_LIFECYCLE.md) §4.8 — the three
  "priority" planes and the launch/shutdown ordering.
- [`sync_binaries.py`](sync_binaries.md) / [`sync_configs.py`](sync_configs.md) —
  the sibling agent helpers (their `--restart` honours `start_priority` too).
- [Tools](../tools.md) — overview of `tools/` (build infrastructure + agent scripts).
- [`tools/README.md`](https://github.com/artgins/yunetas/blob/7.7.1/tools/README.md).
