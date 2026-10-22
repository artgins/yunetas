# `yunetas set-start-priorities`

Operator tool of the [`yunetas` CLI](../yunetas-cli.md) (module
`yunetas.agent_tools.set_start_priorities`,
[source](https://github.com/artgins/tui_yunetas.py/blob/0.23.4/yunetas/agent_tools/set_start_priorities.py)).

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

It is the third agent tool alongside
[`sync-binaries`](sync_binaries.md) and [`sync-configs`](sync_configs.md),
and shares their OAuth2-once + `-j` plumbing, so it can drive a remote `wss://`
agent.

## Tiers

Rules are matched in order. The **first match wins**, and explicit `--rule`
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
yunetas set-start-priorities --rule 'agregador_wz=8' --rule 'scheduler_wz=8'
```

## How it writes

The agent's `update-node` reads its record from `content64` (base64 JSON). The
inline `record={...}` form is **not** coerced by the ycommand CLI. The tool
therefore base64-encodes a strict-JSON `{ "id": ..., "start_priority": ... }`
per changed yuno and sends:

```
command-agent service=treedb_yuneta_agent command=update-node topic_name=yunos content64=<base64>
```

Only yunos whose `start_priority` changes are written. New yunos born
with the `util` tag already default to tier `1` at `create-yuno`, so they
usually need no action here.

## Usage

Run it with the `yunetas` CLI (since CLI 0.22.0). The CLI forwards every
argument to the tool:

```bash
yunetas set-start-priorities             # interactive: show plan, ask, apply
yunetas set-start-priorities -n          # dry-run: show the plan, write nothing
yunetas set-start-priorities -a          # apply without asking
yunetas set-start-priorities --show-all  # also list yunos already at target / with no rule
yunetas set-start-priorities --rule 'gate_*=3' --rule 'reporter=2'
yunetas set-start-priorities -u ws://127.0.0.1:1991   # target a specific agent
yunetas set-start-priorities -n --node prod           # a registered node
```

The exit code is `1` when a write failed.

## See also

- [Yuno lifecycle](../../../yunos/c/yuno_agent/YUNO_LIFECYCLE.md) §4.8 — the three
  "priority" planes and the launch/shutdown ordering.
- [`sync-binaries`](sync_binaries.md) / [`sync-configs`](sync_configs.md) —
  the sibling agent helpers (their `--restart` honours `start_priority` too).
- [Tools](../tools.md) — overview of `tools/` (build infrastructure + agent scripts).
- [`tools/README.md`](https://github.com/artgins/yunetas/blob/7.27.6/tools/README.md).
