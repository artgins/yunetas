(deploying-yunos)=
# Deploying yunos step by step

This is the **hands-on deploy guide**: you built (or edited) something and you
want it running on a node, without re-deriving the theory. It ties together the
three tools that do the work —

| Tool | What it is | Where it lives |
|------|------------|----------------|
| [`yunetas`](yunetas-cli.md) | The management/build CLI (`pipx install yunetas`). Front-end for everything below. | [`utils/python/tui_yunetas`](https://github.com/artgins/yunetas/tree/main/utils/python/tui_yunetas) (git submodule, published on PyPI) |
| [`sync_binaries.py`](tools/sync_binaries.md) | Diffs the built binaries against the agent's installed set and pushes the differences. | [`tools/agent/`](tools.md) (shipped in the `.deb`, available on every node) |
| [`sync_configs.py`](tools/sync_configs.md) | Diffs a directory of `*.json` yuno configs against the agent's installed set and pushes the differences. | [`tools/agent/`](tools.md) |

You almost never call the two scripts directly: `yunetas sync`,
`yunetas sync-binaries` and `yunetas sync-configs` wrap them and add the
project/realm discovery. Everything ultimately talks to the local
`yuneta_agent` through [`ycommand`](utilities/ycommand.md).

If a term below is unfamiliar (yuno, role, realm, slot, snap), read the
[mental model](#dy-mental-model) first — it is five paragraphs.

## TL;DR

```bash
# The whole deploy, 99% of the time:
yunetas build                 # compile SDK + registered projects
yunetas sync -n               # DRY-RUN: see what would be pushed (binaries + configs)
yunetas sync                  # push it for real
yunetas upgrade-yunos         # ONLY if a version was bumped: promote + restart
```

- **Same-version rebuild (hot-patch)?** `yunetas sync` is enough — it
  kill/restarts the affected yunos itself. Skip `upgrade-yunos`.
- **Version bump (`APP_VERSION` changed)?** `yunetas sync` then
  `yunetas upgrade-yunos`. Without the second step **the old version keeps
  running** — pushing installs the new release next to the old one, it does not
  activate it.
- **Config-only change?** `yunetas sync-configs -r` (the `-r` restarts the
  yunos that use it; without it the push lands but the running yuno keeps its
  old config until its next restart).
- **Brand-new yuno on this node?** The sync tools will NOT propose it — see
  [Recipe D](#dy-recipe-new-yuno).

(dy-mental-model)=
## The mental model in five paragraphs

**A node runs one `yuneta_agent`**, and the agent owns every yuno on that node:
it stores their binaries and configs, launches them, watches them, restarts
them. You never copy files into place or kill processes by hand — you send
commands to the agent with [`ycommand`](utilities/ycommand.md) (default url
`ws://127.0.0.1:1991`), and the deploy tools in this guide are automation on
top of exactly those commands.

**A yuno is (binary, config, realm) glued by a registration row.** The binary
is identified by its **role** (the executable's filename, e.g. `auth_bff`) and
a **version** (the `APP_VERSION` compiled into it). Each `(role, version)` pair
is a separate **slot** in the agent's repository — installing version 1.2.1
does not touch the 1.2.0 slot. The config is a JSON file identified by an
**id** (by convention `<role>.<name>`, e.g. `auth_bff.1801`) and a version read
from the `__version__` field *inside* the file. A **realm** is the tenant the
yuno instance runs in.

**Deploy is two phases: push, then promote.** Pushing (`yunetas sync`) uploads
new binaries and configs into the agent's store; nothing running changes yet.
Promoting (`yunetas upgrade-yunos`) makes the freshly installed versions the
**primary** ones and restarts the yunos onto them. Only same-version changes
(a rebuilt binary, an edited config) skip the promote phase, because they
overwrite the slot that is already primary.

**Where the artifacts come from.** Binaries: every built yuno lands in
`$YUNETAS_BASE/outputs/yunos/` (that is what `make install` / `yunetas build`
does), and that directory is exactly where the agent's `$$(<role>)` upload
macro reads from. Configs: each project keeps per-node config sets under
`yunos/batches/<host>/*.json`, where `<host>` is the **realm_id** of the target
node (its deploy FQDN, e.g. `batches/app.wattyzer.com/`).

**Safety nets.** Every push tool shows you a classification table and asks
before touching anything (`-n` dry-runs, `-a` skips the questions);
`upgrade-yunos` shoots a rollback **snap** first, so a bad release is one
`activate-snap` away from being undone ([Recipe E](#dy-recipe-rollback)). The
whole flow is idempotent: re-running `sync` + `upgrade-yunos` after an
interrupted deploy finishes the job instead of failing ("already exists"
answers are treated as already-done, not as errors).

```
 build                    push                      promote                verify
────────►  outputs/yunos ───────►  agent store  ──────────────►  running ────────►
yunetas    batches/<host>  yunetas   (slots per     yunetas         yunos    ycommand -c
 build                      sync    role+version)  upgrade-yunos            'list-yunos'
                                                  (skip if no
                                                   version bump)
```

## One-time setup (per machine)

Skip anything already true. Full detail in [Installation](installation.md).

```bash
# 1. The CLI itself
pipx install yunetas

# 2. Build environment (from the yunetas repo dir; needed for building/pushing)
source yunetas-env.sh

# 3. Register the app project(s) whose yunos you deploy
yunetas register-project /yuneta/development/projects/<myproject>
yunetas list-projects

# 4. Sanity: the local agent answers
ycommand -c 'list-yunos'
```

Registering a project (its root must contain `yunos/CMakeLists.txt`) is what
lets `yunetas build` compile it after the SDK and lets `yunetas sync-configs`
find its `yunos/batches/<host>/` directories. The registry is machine-local
(`~/.yuneta/projects.json`), never committed.

## Which recipe do I need?

| What changed | Recipe | Commands |
|---|---|---|
| Code edit, **same** `APP_VERSION` (debug fix, rebuild) | [A — hot-patch](#dy-recipe-hotpatch) | `yunetas build` + `yunetas sync` |
| `APP_VERSION` **bumped** in `main.c` | [B — version bump](#dy-recipe-bump) | `yunetas build` + `yunetas sync` + `yunetas upgrade-yunos` |
| Only a config `*.json` changed | [C — config-only](#dy-recipe-config) | `yunetas sync-configs -r` |
| Yuno never installed on this node before | [D — new yuno](#dy-recipe-new-yuno) | manual `ycommand` onboarding |
| New release misbehaves, go back | [E — rollback](#dy-recipe-rollback) | `ycommand -c 'activate-snap ...'` |

If both binaries and configs changed (the normal case for a release), always
push them **together** with `yunetas sync` — a new binary against a stale
config is the classic footgun (a fail-closed runtime reading an old config
breaks in ways neither artifact would alone). `sync` runs binaries first and
refuses to continue to configs if the binary push failed, so a half-deploy
cannot happen silently.

(dy-recipe-hotpatch)=
## Recipe A — hot-patch (same version)

Use when the code changed but `APP_VERSION` did not: a quick fix, a rebuild
with more debug info, a relink.

```bash
# 1. Build
yunetas build                 # or scoped: yunetas build <project>

# 2. Preview, then push
yunetas sync -n
yunetas sync
```

That is the whole recipe. In the table, the changed roles show as **`REBUILD`**
(`update-binary`): same version, different content. For each one the tool runs
the full hot-patch cycle itself, scoped to that role (never node-wide):

```
kill-yuno yuno_role=<role>          # orderly shutdown (only if it was running)
   └─ poll until the process exits  # else update-binary hits text-file-busy
update-binary id=<role> content64=$$(<role>)
run-yuno  yuno_role=<role> play=0   # only if it had been running
play-yuno yuno_role=<role>          # only if it had been playing
```

Prior run/play state is restored per role — a deliberately stopped yuno stays
stopped. Multiple roles restart in ascending `start_priority`, so
infrastructure (logcenter, auth_bff…) comes back before its dependents. Pass
`--no-restart` if you want the push only, with a printed reminder instead.

**Do NOT run `upgrade-yunos` after a hot-patch** — there is no new version to
promote; it will find nothing to do (harmless, but noise).

(dy-recipe-bump)=
## Recipe B — version bump

Use when `APP_VERSION` changed in `main.c` (or the SDK version moved and your
yunos re-versioned with it).

```bash
# 1. Build
yunetas build

# 2. Push binaries + configs (the changed roles show as BUMP -> install-binary)
yunetas sync -n
yunetas sync

# 3. Promote the new releases and restart onto them
yunetas upgrade-yunos
```

**Step 3 is not optional.** `install-binary` creates a *new* `(role, version)`
slot next to the old one; the running yunos are still registered against — and
will be relaunched on — the OLD release, even across `kill-yuno`/`run-yuno`.
`upgrade-yunos` is what flips the node to the new release. It does, in order:

1. **Rollback snap** — `shoot-snap name=pre-upgrade-<YYYYMMDD>` (idempotent by
   name; reuses an already-active snap; `--no-snap` skips, `--snap-name N`
   renames).
2. **Preview** — runs `find-new-yunos`, lists the new yuno rows it would
   register, and asks for confirmation (`-y` skips the prompt).
3. **Register** — `find-new-yunos create=1` writes the new yuno-instance rows.
4. **Promote + restart** — `deactivate-snap`, which triggers the agent's
   `restart_nodes()`: **SIGKILL every running yuno on the node**, reload the
   treedb with the newest release promoted to primary, and bring everything
   back up.

Two things to know about step 4:

- It is a **node-wide bounce**, not per-role. Fine for infrastructure windows;
  flag it before running it on a busy production node.
- SIGKILL means no orderly shutdown (`mt_stop` does not run). If some yuno
  must flush state on exit, `ycommand -c 'kill-yuno id=<id>'` it manually
  before `upgrade-yunos`.

Verify:

```bash
ycommand -c 'list-yunos'      # release column shows the new version, running=true
```

(dy-recipe-config)=
## Recipe C — config-only change

You edited a config under `<project>/yunos/batches/<host>/`, no binary change.

```bash
# Preview, then push AND restart the yunos that use the changed configs
yunetas sync-configs -n
yunetas sync-configs -r
```

Key facts, because configs behave differently from binaries:

- **A config push never needs a kill.** It always succeeds against a running
  yuno — but the yuno only reads its config at **(re)start**. Without `-r` the
  new content sits in the agent until the yuno's next restart, which is a great
  way to believe a change is live when it isn't. `-r`/`--restart` bounces the
  affected yunos (scoped by id, run/play state preserved, `start_priority`
  order); the default prints their ids as a reminder instead.
- **The file name is the config id**: `auth_bff.1801.json` → id
  `auth_bff.1801`. **The version lives inside the file**, in its
  `__version__` field. A file without `__version__` is skipped (not
  deployable); files starting with `_` (batch helpers) are skipped too.
- Same version + changed content → `UPDATE` (`update-config`, overwrite in
  place). Higher `__version__` → `BUMP` (`create-config`, new record — needs a
  promote just like a binary bump if the yuno row pins config versions;
  normally you bump config versions together with binary versions and
  [Recipe B](#dy-recipe-bump) covers both).
- A **local version older than the agent's is never pushed** (`DOWNGRADE`,
  reported only) — seeding a stale version would corrupt the version logic.

**Which batches directory gets synced?** Without `--host`, the CLI asks the
local agent (`*list-realms`) for the realm_ids it manages and syncs every
`batches/<host>/` of every registered project whose name matches one — a node
running several realms deploys all of them in one pass. `--host <h>` targets
one directory explicitly; if the agent is unreachable it falls back to matching
the machine's hostname. `Skipping <project>: no batches for host ...` almost
always means the batches directory is not named after the node's realm_id.

Verify the *effective* config (main.c defaults merged with the stored JSON):

```bash
ycommand -c 'command-yuno id=<yuno_id> service=__yuno__ command=view-config'
```

(dy-recipe-new-yuno)=
## Recipe D — brand-new yuno on this node

**The sync tools will not propose a role the agent does not already manage.**
`sync_binaries.py` deliberately drives from the agent's installed set — not
from `outputs/yunos` — so it never offers to install the 30 other binaries
your build tree happens to contain. First-time provisioning is therefore a
short manual sequence (full detail in
[Yuno lifecycle §6.1](../../yunos/c/yuno_agent/YUNO_LIFECYCLE.md)):

```bash
# 1. Install the binary (reads $YUNETAS_BASE/outputs/yunos/<role>)
ycommand -c 'install-binary content64=$$(<role>)'

# 2. Install its config (id = filename minus .json; version read from __version__)
ycommand -c "create-config id=<role>.<name> content64=\$\$(/path/to/batches/<host>/<role>.<name>.json)"

# 3. Register the yuno (links binary + config into a realm)
ycommand -c 'create-yuno realm_id=<realm> yuno_role=<role> yuno_name=<name>'

# 4. Enable, launch, play
ycommand -c 'enable-yuno id=<yuno_id>'
ycommand -c 'run-yuno play=0 id=<yuno_id>'
ycommand -c 'play-yuno id=<yuno_id>'

# 5. Verify
ycommand -c 'list-yunos'
```

From then on the yuno is part of the agent's set and every later update flows
through Recipes A–C.

(dy-recipe-rollback)=
## Recipe E — rollback

`upgrade-yunos` shot a snap before changing anything (default name
`pre-upgrade-<YYYYMMDD>`). If the new release misbehaves:

```bash
ycommand -c 'snaps'                              # find the snap name
ycommand -c 'activate-snap name=pre-upgrade-<YYYYMMDD>'
```

`activate-snap` runs the same node-wide restart cycle, but with the snap
active the OLD releases win — the node is back on what it ran before the
upgrade. When the situation is resolved (either a fixed release was deployed,
or you decided to stay), remove the pin:

```bash
ycommand -c 'deactivate-snap'
```

Details, including why snap-tagged binaries refuse deletion, in
[Yuno lifecycle §6.6](../../yunos/c/yuno_agent/YUNO_LIFECYCLE.md).

## Remote nodes (wss + OAuth2)

Everything above targets the **local** agent over `ws://127.0.0.1:1991`. To
drive a remote agent (TLS port, OAuth2-gated) pass the url and credentials —
the sync tools log in **once** and reuse the token on every underlying
`ycommand` call:

```bash
yunetas sync -n \
    -u wss://<node>:1993 \
    -I https://auth.example.com/realms/<realm> \
    -Z <client_id> -x <user> -X '<password>'

# or with a token you already have:
yunetas sync -n -u wss://<node>:1993 -j "$JWT"
```

The same flags work on `sync-binaries`, `sync-configs` and `upgrade-yunos`
(they are forwarded to the wrapped scripts). This is how a node with SSH
disabled is still deployable.

## Reading the sync tables

Both tools print a classification table and only act on rows with a command.
Quick decoder:

| Status | Meaning | What happens |
|---|---|---|
| `BUMP` | local version > agent's | `install-binary` / `create-config` (new slot/record; needs promote) |
| `REBUILD` / `UPDATE` | same version, content differs | `update-binary` (kill→write→restore) / `update-config` (in place) |
| `INSTALLED` | new version already pushed, not yet primary | nothing — run `yunetas upgrade-yunos` |
| `UP-TO-DATE` | nothing to do | skipped |
| `DOWNGRADE` | local is OLDER than the agent's | binaries: offered but flagged red; configs: never pushed |
| `no-build` / `agent-only` | exists on one side only | skipped (informational). A missing local build is fine; a missing agent role means [Recipe D](#dy-recipe-new-yuno) |

`REBUILD` with `Δsize 0` and note `newer build` is real: a rebuild can keep
the byte count identical (one-char string edit, relink), so file times are
compared too.

## Troubleshooting

| Symptom | Cause | Fix |
|---|---|---|
| After `sync`, the yuno still runs the OLD version | You pushed a version bump but never promoted | `yunetas upgrade-yunos` ([Recipe B](#dy-recipe-bump), step 3) |
| `update-binary` fails with *text-file-busy* | The running process is mapped to the slot being overwritten | Let the tool do the kill/restart (don't pass `--no-restart`), or `kill-yuno` first and wait until `list-yunos` shows `yuno_running=false` |
| `... already exists` answers during a re-run | A prior interrupted run already pushed that artifact | Nothing — it prints `ALREADY PRESENT (idempotent)` and continues; just finish with `upgrade-yunos` |
| `INSTALLED` rows, "pending promote" | New versions staged but a snap / missing promote pins the old primary | `yunetas upgrade-yunos` |
| `sync-configs`: *Skipping \<project\>: no batches for host* | `batches/<dir>` name doesn't match any realm_id of the local agent | Name the directory after the realm_id (deploy FQDN), or pass `--host <dir>` |
| `sync-configs`: *no `__version__` field, skipped* | The JSON is not a deployable config under the agent contract | Add `"__version__": "<n>"` (and `__description__`) to the file |
| Config pushed but behaviour unchanged | A yuno reads config only at (re)start | Re-run with `-r`, or `kill-yuno` + `run-yuno` + `play-yuno` the affected ids |
| *ERROR: '\*list-binaries' did not return JSON. Is the agent up?* | No agent listening at the url | Check the agent, or pass `-u` (remote: see the wss/OAuth2 section) |
| `run-yuno` fails *primary binary not found* | Yuno rows registered without their binary (e.g. `find-new-yunos create=1` before the push) | Push the binary (`yunetas sync-binaries`), then `upgrade-yunos` again |
| Sync proposes nothing for a yuno you just built | The agent doesn't manage that role on this node | [Recipe D](#dy-recipe-new-yuno) — first-time onboarding is manual |

## Where the full detail lives

- [The `yunetas` CLI](yunetas-cli.md) — every command and flag of the CLI.
- [`sync_binaries.py`](tools/sync_binaries.md) /
  [`sync_configs.py`](tools/sync_configs.md) — classification internals of the
  wrapped scripts.
- [Yuno lifecycle](../../yunos/c/yuno_agent/YUNO_LIFECYCLE.md) — the agent's
  data model, the raw `ycommand` recipes (§6), and the sharp edges (§5) this
  guide's tooling exists to protect you from.
- [`ycommand`](utilities/ycommand.md) — the control-plane client everything
  here is built on.
