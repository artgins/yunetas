(deploying-yunos)=
# Deploying yunos step by step

This is the **hands-on deploy guide**. You built or edited something, and you
want it to run on a node. This guide does not repeat the theory. It ties
together the three tools that do the work:

| Tool | What it is | Where it lives |
|------|------------|----------------|
| [`yunetas`](yunetas-cli.md) | The CLI that manages and builds (`pipx install yunetas`). It is the front end for everything below. | [`utils/python/tui_yunetas`](https://github.com/artgins/yunetas/tree/7.12.0/utils/python/tui_yunetas) (git submodule, published on PyPI) |
| [`sync_binaries.py`](tools/sync_binaries.md) | Compares the built binaries with the agent's installed set, then pushes the differences. | ships **inside the CLI** (`yunetas.agent_tools`) since 0.17.0 |
| [`sync_configs.py`](tools/sync_configs.md) | Compares a directory of `*.json` yuno configs with the agent's installed set, then pushes the differences. | ships **inside the CLI** (`yunetas.agent_tools`) since 0.17.0 |

You almost never call the two scripts directly. `yunetas sync`,
`yunetas sync-binaries` and `yunetas sync-configs` wrap them, and they add the
discovery of projects and realms. Every command goes to the local
`yuneta_agent` through [`ycommand`](utilities/ycommand.md).

If you do not know a term below (yuno, role, realm, slot, snap), read the
[mental model](#dy-mental-model) first. It is seven paragraphs.

## TL;DR

```bash
# The whole deploy, 99% of the time:
yunetas build                 # compile SDK + registered projects
yunetas sync -n               # DRY-RUN: see what would be pushed (binaries + configs)
yunetas sync                  # push it for real
yunetas upgrade-yunos         # ONLY if a version was bumped: promote + restart
```

Those commands target the **local** agent. If you deploy to another machine,
register it one time and add `--node`:

```bash
# Agent reachable from here (wss:// on 1993, OAuth2):
yunetas register-node prod --url wss://myhost:1993 \
    --issuer https://auth.example.com/realms/r --client-id myhost --user-id me
# Or: agent listening only on loopback (the default), reached over SSH:
yunetas register-node prod --ssh yuneta@myhost

yunetas sync -n --node prod   # same flow, remote target
```

The registry (`~/.yuneta/nodes.json`) stores **where** a node is and **which
identity** you present. It never stores a password. Give the credential at call
time in `$YUNETA_OAUTH_PASSW`. To use a token again, give it in
`$YUNETA_OAUTH_JWT`.

  - **Same-version rebuild (hot-patch)?** `yunetas sync` is enough. It stops and
  restarts the affected yunos itself. Do not run `upgrade-yunos`.
- **Version bump (`APP_VERSION` changed)?** Run `yunetas sync`, then
  `yunetas upgrade-yunos`. Without the second command, **the node keeps the old
  version**. The push installs the new release next to the old one. It does not
  activate it.
- **Config-only change?** Run `yunetas sync-configs -r`. The `-r` flag restarts
  the yunos that use the config. Without `-r`, the push succeeds, but the yuno
  keeps its old config until its next restart.
- **Brand-new yuno on this node?** The sync tools do not propose it. See
  [Recipe D](#dy-recipe-new-yuno).

(dy-mental-model)=
## The mental model in seven paragraphs

**A node runs one `yuneta_agent`**, and the agent owns every yuno on that node.
It stores their binaries and configs. It launches them, watches them, and
restarts them. You never copy files into place, and you never kill processes by
hand. You send commands to the agent with [`ycommand`](utilities/ycommand.md),
whose default url is `ws://127.0.0.1:1991`. The deploy tools in this guide are
automation on top of exactly those commands.

**A yuno is a binary, a config and a realm, joined by a registration row.** Two
things identify the binary: its **role** (the filename of the executable, for
example `auth_bff`) and its **version** (the `APP_VERSION` compiled into it).
Each `(role, version)` pair is a separate **slot** in the agent's repository.
If you install version 1.2.1, the 1.2.0 slot does not change. The config is a
JSON file. An **id** identifies it (by convention `<role>.<name>`, for example
`auth_bff.1801`), and the `__version__` field *inside* the file gives its
version. A **realm** is the tenant that the yuno instance runs in.

**Deploy is two phases: push, then promote.** The push phase (`yunetas sync`)
uploads new binaries and configs into the agent's store. Nothing that runs
changes yet. The promote phase (`yunetas upgrade-yunos`) makes the newly
installed versions the **primary** ones, and restarts the yunos onto them. Only
same-version changes skip the promote phase, because they overwrite the slot
that is already primary. A rebuilt binary and an edited config are such
changes.

**Where the artifacts are.** Binaries: `make install` and `yunetas build` put
every built yuno in `$YUNETAS_BASE/outputs/yunos/`. The agent's `$$(<role>)`
upload macro reads that same directory. Configs: each project keeps one config
set per node under `yunos/batches/<host>/*.json`. The `<host>` part is the
**realm_id** of the target node, which is its deploy FQDN, for example
`batches/app.wattyzer.com/`.

**CAUTION: build where the SDK was built, not on the target node.** Yunos are
linked fully static, and the prebuilt archives in the `.deb` and the `.rpm`
(`outputs/lib`, `outputs_ext/lib`) are tied to the glibc that produced them. A
node that has a compiler and your project sources *can* compile its own yunos
against those archives, and the link succeeds. But the link mixes the archives
with the **node's** glibc, and the binary then corrupts its heap at run time.
It dies inside `malloc` seconds after start. It writes no Yuneta error, and its
stack trace points at unrelated code.

Since 7.8.4 the build refuses this at `cmake` time instead
(`glibc mismatch. REFUSING to build.`). If you see that message, build on the
machine that built the SDK. Then push the binaries from there, which is what
this guide does anyway. A build on a node is safe in two cases only: that node
built the whole SDK from source, or it runs a package made for its own
distribution.

**Safety nets.** Every push tool shows you a classification table, and it asks
you before it changes anything. `-n` does a dry run, and `-a` skips the
questions. `upgrade-yunos` shoots a rollback **snap** first, so one
`activate-snap` command undoes a bad release
([Recipe E](#dy-recipe-rollback)). The whole flow is idempotent. If a deploy
stops in the middle, run `sync` and `upgrade-yunos` again, and they finish the
job instead of failing. The tools read "already exists" answers as already-done,
not as errors.

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
| Yuno never installed on this node before | [D — new yuno](#dy-recipe-new-yuno) | manual `ycommand` sequence |
| New release is not correct, go back | [E — rollback](#dy-recipe-rollback) | `ycommand -c 'activate-snap ...'` |

If both the binaries and the configs changed, push them **together** with
`yunetas sync`. This is the normal case for a release. A new binary with a
stale config is a known cause of incidents: a fail-closed runtime that reads an
old config breaks in ways that neither artifact breaks alone. `sync` pushes the
binaries first. If the binary push fails, `sync` does not continue to the
configs, so a half deploy cannot happen without a message.

(dy-recipe-hotpatch)=
## Recipe A — hot-patch (same version)

If the code changed but `APP_VERSION` did not, use this recipe. It covers a
quick fix, a rebuild with more debug info, and a relink.

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

The tool restores the prior run and play state of each role. A yuno that you
stopped on purpose stays stopped. Multiple roles restart in ascending
`start_priority`, so the infrastructure (logcenter, auth_bff…) starts before
its dependents. If you want the push only, pass `--no-restart`. The tool then
prints a reminder instead of the restart.

**Do not run `upgrade-yunos` after a hot-patch.** There is no new version to
promote, so the command finds nothing to do. It does no damage, but it adds
noise.

(dy-recipe-bump)=
## Recipe B — version bump

If `APP_VERSION` changed in `main.c`, use this recipe. Also use it when the SDK
version moved and your yunos took a new version with it.

CAUTION: Step 3 restarts every yuno on the node, not only the yunos that you
changed. On a busy production node, tell the team before you start.

```bash
# 1. Build
yunetas build

# 2. Push binaries + configs (the changed roles show as BUMP -> install-binary)
yunetas sync -n
yunetas sync

# 3. Promote the new releases and restart onto them
yunetas upgrade-yunos
```

To make sure that the node runs the new release:

```bash
ycommand -c 'list-yunos'      # the release column shows the new version, running=true
```

### Why step 3 is not optional

`install-binary` makes a **new** `(role, version)` slot next to the old one. It
does not touch the registration of the yunos that run now. The agent keeps each
one registered against the OLD release, and it starts them again on that old
release, even after `kill-yuno` and `run-yuno`. `upgrade-yunos` is the command
that moves the node to the new release.

`upgrade-yunos` does four operations, in this order:

1. **Rollback snap** — `shoot-snap name=pre-upgrade-<YYYYMMDD>`. The name makes
   the operation idempotent, and the tool uses an already-active snap again.
   `--no-snap` skips this operation. `--snap-name N` gives the snap another
   name.
2. **Preview** — `find-new-yunos` lists the new yuno rows that the tool can
   register. Then the tool asks you for confirmation. `-y` skips the question.
3. **Register** — `find-new-yunos create=1` writes the new yuno-instance rows.
4. **Promote and restart** — `deactivate-snap` starts the agent's
   `restart_nodes()`.

Operation 4 sends **SIGKILL to every yuno that runs on the node**. Then the
agent loads the treedb again. The newest release becomes primary, and all the
yunos start again.

The restart is node-wide, not per-role. During an infrastructure window this is
acceptable. On a production node it is not, and the CAUTION above applies.

SIGKILL gives no orderly shutdown, because `mt_stop` does not run. If a yuno
must write its state to disk on exit, stop it first with
`ycommand -c 'kill-yuno id=<id>'`. Then run `upgrade-yunos`.

(dy-recipe-config)=
## Recipe C — config-only change

If you edited a config under `<project>/yunos/batches/<host>/` and no binary
changed, use this recipe.

```bash
# Preview, then push AND restart the yunos that use the changed configs
yunetas sync-configs -n
yunetas sync-configs -r
```

Key facts, because configs behave differently from binaries:

  - **A config push never needs a kill.** The push always succeeds, even while
  the yuno runs. But the yuno reads its config only at start or restart.
  Without `-r`, the new content stays in the agent until the next restart of
  the yuno. Then you can believe that a change is live when it is not.
  `-r`/`--restart` restarts the affected yunos, scoped by id, in
  `start_priority` order, and keeps their run and play state. By default the
  tool prints their ids as a reminder instead.
- **The file name is the config id**: `auth_bff.1801.json` → id
  `auth_bff.1801`. **The version lives inside the file**, in its
  `__version__` field. A file without `__version__` is not deployable, and the
  tool skips it. The tool also skips the files that start with `_`, which are
  batch helpers.
- Same version and changed content give `UPDATE` (`update-config`), which
  overwrites in place. A higher `__version__` gives `BUMP` (`create-config`),
  which writes a new record. If the yuno row pins config versions, a `BUMP`
  needs the same promote step as a binary bump. Normally you raise the config
  versions together with the binary versions, and
  [Recipe B](#dy-recipe-bump) covers both.
- **The tool never pushes a local version that is older than the version on
  the agent.** It reports the difference as `DOWNGRADE` and does nothing else.
  A stale version corrupts the version logic.

**Which batches directory does the tool sync?** Without `--host`, the CLI asks
the local agent for the realm_ids that it manages (`*list-realms`). Then it
syncs the `batches/<host>/` directory of every registered project with a name
that matches one realm_id. A node with several realms deploys all of them in
one pass. `--host <h>` targets one directory explicitly. If the agent does not
answer, the CLI uses the hostname of the machine instead. The message
`Skipping <project>: no batches for host ...` almost always means that the
batches directory does not have the name of the node's realm_id.

To make sure that the *effective* config is correct (the `main.c` defaults
merged with the stored JSON):

```bash
ycommand -c 'command-yuno id=<yuno_id> service=__yuno__ command=view-config'
```

(dy-recipe-new-yuno)=
## Recipe D — brand-new yuno on this node

**The sync tools will not propose a role the agent does not already manage.**
`sync_binaries.py` drives from the agent's installed set, not from
`outputs/yunos`. This is deliberate. The tool therefore never offers to
install the 30 other binaries in your build tree. The first installation is
therefore a short manual sequence (full detail in
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

`upgrade-yunos` shot a snap before it changed anything. The default name is
`pre-upgrade-<YYYYMMDD>`.

CAUTION: `activate-snap` restarts every yuno on the node, like
`upgrade-yunos`. On a busy production node, tell the team before you start.

If the new release is not correct, run these commands:

```bash
ycommand -c 'snaps'                              # find the snap name
ycommand -c 'activate-snap name=pre-upgrade-<YYYYMMDD>'
```

`activate-snap` runs the same node-wide restart cycle. But with the snap
active, the OLD releases become primary. The node runs again what it ran
before the upgrade. When you deploy a corrected release, or when you decide to
stay on the old one, remove the pin:

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
| `BUMP` | local version > agent's | `install-binary` or `create-config`. It makes a new slot or record, and it needs a promote |
| `REBUILD` / `UPDATE` | same version, content differs | `update-binary` (kill→write→restore) / `update-config` (in place) |
| `INSTALLED` | new version already pushed, not yet primary | nothing — run `yunetas upgrade-yunos` |
| `UP-TO-DATE` | nothing to do | skipped |
| `DOWNGRADE` | local is OLDER than the agent's | binaries: offered but marked red. Configs: never pushed |
| `no-build` / `agent-only` | exists on one side only | skipped (informational). A missing local build is correct. A missing agent role means [Recipe D](#dy-recipe-new-yuno) |

`REBUILD` with `Δsize 0` and note `newer build` is real: a rebuild can keep
the byte count identical (one-char string edit, relink), so file times are
compared too.

## Troubleshooting

| Symptom | Cause | Fix |
|---|---|---|
| After `sync`, the yuno still runs the OLD version | You pushed a version bump but never promoted | `yunetas upgrade-yunos` ([Recipe B](#dy-recipe-bump), step 3) |
| `update-binary` fails with *text-file-busy* | The running process is mapped to the slot being overwritten | Let the tool do the kill and the restart. Do not pass `--no-restart`. Or `kill-yuno` first and wait until `list-yunos` shows `yuno_running=false` |
| `... already exists` answers during a re-run | A prior interrupted run already pushed that artifact | Nothing. It prints `ALREADY PRESENT (idempotent)` and continues. Finish with `upgrade-yunos` |
| `INSTALLED` rows, "pending promote" | New versions staged but a snap / missing promote pins the old primary | `yunetas upgrade-yunos` |
| `sync-configs`: *Skipping \<project\>: no batches for host* | `batches/<dir>` name does not match any realm_id of the local agent | Name the directory after the realm_id (deploy FQDN), or pass `--host <dir>` |
| `sync-configs`: *no `__version__` field, skipped* | The JSON is not a deployable config under the agent contract | Add `"__version__": "<n>"` (and `__description__`) to the file |
| Config pushed but behavior unchanged | A yuno reads config only at (re)start | Re-run with `-r`, or `kill-yuno` + `run-yuno` + `play-yuno` the affected ids |
| *ERROR: '\*list-binaries' did not return JSON. Is the agent up?* | No agent listening at the url | Check the agent, or pass `-u` (remote: see the wss/OAuth2 section) |
| `run-yuno` fails *primary binary not found* | Yuno rows registered without their binary, for example `find-new-yunos create=1` before the push | Push the binary (`yunetas sync-binaries`), then `upgrade-yunos` again |
| Sync proposes nothing for a yuno you just built | The agent does not manage that role on this node | [Recipe D](#dy-recipe-new-yuno) — first-time onboarding is manual |

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
