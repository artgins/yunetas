(deploying-yunos)=
# Deploying yunos step by step

This is the **hands-on deploy guide**. You built or edited something, and you
want it to run on a node. This guide does not repeat the theory. It ties
together the three tools that do the work:

| Tool | What it is | Where it lives |
|------|------------|----------------|
| [`yunetas`](yunetas-cli.md) | The CLI that manages and builds (`pipx install yunetas`). It is the front end for everything below. | [`utils/python/tui_yunetas`](https://github.com/artgins/yunetas/tree/7.26.7/utils/python/tui_yunetas) (git submodule, published on PyPI) |
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
questions. `upgrade-yunos` shoots a rollback **snap** before it changes anything, so one
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

If the node runs SDK 7.25.4 or earlier, read
[Before an upgrade from 7.25.4 or earlier: the SDK and the agent](#dy-upgrade-7255)
and look for md2 files that are not whole rows before step 1: see
[Before an upgrade from 7.25.4 or earlier](#dy-md2-scan).

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

A node coming from 7.25.5..7.25.22 keeps the tm markers it wrote; they are
ignored, and there is nothing to migrate: see
[The tm markers of 7.25.5..7.25.22](#dy-mark-tm-order). An upgrade to 7.26.0
has three more things to check: see [Upgrading to 7.26.0](#dy-upgrade-726).

### Why step 3 is not optional

`install-binary` makes a **new** `(role, version)` slot next to the old one. It
does not touch the registration of the yunos that run now. The agent keeps each
one registered against the OLD release, and it starts them again on that old
release, even after `kill-yuno` and `run-yuno`. `upgrade-yunos` is the command
that moves the node to the new release.

`upgrade-yunos` does five operations, in this order:

1. **Preview** — `find-new-yunos` lists the new yuno rows that the tool can
   register. If the list is empty, the tool stops: it shoots no snap and
   restarts nothing.
2. **Confirm** — the tool asks you for confirmation. `-y` skips the question.
   If you answer no, the tool stops and leaves no snap.
3. **Rollback snap** — `shoot-snap name=pre-upgrade-<YYYYMMDD>`. The name makes
   the operation idempotent, and the tool uses an already-active snap again.
   `--no-snap` skips this operation. `--snap-name N` gives the snap another
   name.
4. **Register** — `find-new-yunos create=1` writes the new yuno-instance rows,
   and the tool prints `N created, M already registered`. When every row of
   the preview is already registered, the tool skips `create=1`.
5. **Promote and restart** — `deactivate-snap` starts the agent's
   `restart_nodes()`.

The preview has one line for each yuno that has a newer binary or
configuration. A line is the `create-yuno` command that registers the yuno at
the new release. When an earlier run already registered that release and
nothing promoted it (the old release stays primary until `deactivate-snap`),
the line says so, and `create=1` does not run it again:

```bash
ycommand -c 'find-new-yunos'
#   → comment: "yuneta_agent^<node>: 1 yuno(s) already registered at the new release,
#               pending promotion: run deactivate-snap"
#   → data:
#     "create-yuno id=mqtt_broker realm_id=... yuno_role=mqtt_broker role_version=7.25.5 ..."
#     "already registered, pending promotion (deactivate-snap): create-yuno id=emailsender ..."
```

In 7.25.4 and before, the preview listed the second row as a row to create,
and `find-new-yunos create=1` failed on it with *"Yuno already exists"*.

`yunetas upgrade-yunos` (CLI 0.19.4 and later) shows the two kinds of rows
apart, and counts them apart after `create=1`:

```text
1 new yuno row(s) would be created:
  create-yuno id=mqtt_broker realm_id=... yuno_role=mqtt_broker role_version=7.25.5 ...
1 yuno row(s) already registered, pending promotion:
  already registered, pending promotion (deactivate-snap): create-yuno id=emailsender ...
...
1 created, 1 already registered.
```

CLI 0.19.3 and before counted every row of the preview as created; they still
work with this agent. Get 0.19.4 with `pipx upgrade yunetas`.

The snap comes after the preview, and this order is intentional. A snap tags
every current record, and it clones each record that another snap tagged
before. When there is nothing to upgrade, a snap does that work for no reason.
Before CLI 0.19.2 the snap was the first operation.

Operation 5 sends **SIGKILL to every yuno that runs on the node**. Then the
agent loads the treedb again. The newest release becomes primary, and all the
yunos start again.

The restart is node-wide, not per-role. During an infrastructure window this is
acceptable. On a production node it is not, and the CAUTION above applies.

SIGKILL gives no orderly shutdown, because `mt_stop` does not run. If a yuno
must write its state to disk on exit, stop it first with
`ycommand -c 'kill-yuno id=<id>'`. Then run `upgrade-yunos`.

(dy-upgrade-7255)=
### Before an upgrade from 7.25.4 or earlier: the SDK and the agent

**A node that builds from source rebuilds the external libraries first.**
linux-ext-libs is 1.22 (a patched jansson), and the build refuses a stale
`outputs_ext` until it is rebuilt:

```bash
cd kernel/c/linux-ext-libs && ./extrae.sh && ./configure-libs.sh && cd ../../..
yunetas clean && yunetas build
```

**The agent removes old audit files at its first start.** The audit files in
`/yuneta/realms/agent/agent/audit/` older than `audit_keep_days` (default 7)
are removed, with one INFO *"Old audit files removed"* that names them. Up to
7.25.4 nothing was removed. To keep more, set the retention in the `global`
section of `/yuneta/agent/yuneta_agent.json` BEFORE you start the new agent
(`0` keeps all):

```json
{
    "global": {
        "agent.audit_keep_days": 30
    }
}
```

See [The agent's audit files](#agent-audit-files).

**If the main agent does not come back after the upgrade**, its treedb did not
open: the agent now exits 0 when the schema of its treedb is refused, and it is
not relaunched. Its log has:

```text
treedb 'treedb_yuneta_agent' did not open, its schema was refused (see the log): close-treedb it before opening it again
```

and syslog has *"Cannot start agent treedb: ..."*. Reach the node through
`yuneta_agent22`, which opens no treedb.

**The first open of each treedb reads what 7.25.4 left in `__system__`.** It
writes a record, `saved_schemas/<treedb>.upgrade.json` under the `__system__`
tranger; do not delete it. Expect, once per treedb:

- the WARNING *"Restored from the schema from C: ..."*, where a first
  projection of 7.23.0-7.25.4 died after it wrote its stamp;
- when the literal of that open is newer, the WARNING *"Removed from __system__
  what an older release left there: ..."* and, in `withdrawn_at_open`, the
  kind `left_by_older_release`, for example
  `{"topics": {"departments": "left_by_older_release"}}`.

Neither is work of an operator.

**Before you upgrade, run `save-schema` for each draft that you want to
keep.** The first open cannot tell an unsaved draft from what 7.25.4 left: a
topic or column that an unsaved draft added is taken as left by 7.25.4, and a
topic that an unsaved draft deleted is restored when the file in use is the
literal:

```bash
ycommand -c 'command-yuno id=<id> service=treedbs command=save-schema treedb_name=<treedb>'
```

(dy-md2-scan)=
### Before an upgrade from 7.25.4 or earlier: md2 files that are not whole rows

An md2 file is a sequence of 32-byte rows. A power cut during the write of a
row leaves a part of a row at the end of the file, and 7.25.4 kept appending
after it. 7.25.5 does not cut such a file: every load of its key fails, with
this CRITICAL, until the file is repaired:

```text
CRITICAL check_torn_md2_rows: md2 file of the key ends in a whole row that is not on a row
      boundary: written by 7.25.4 after a torn row; not cut, repair it by hand
```

Before you upgrade the node, look for such files:

```bash
find /yuneta/store /yuneta/realms -name '*.md2' -printf '%s %p\n' | awk '$1 % 32'
```

No output means there is nothing to repair. Each line is the size and the path
of a file that is not a whole number of rows. For each file, follow
[A topic that did not load whole](#treedb-topic-not-loaded-whole): it tells a
torn last row (the master cuts it back at the open, and the key loads) from
the shapes that must be repaired by hand, and it gives the repair.

(dy-mark-tm-order)=
### The tm markers of 7.25.5..7.25.22: nothing to do

From 7.25.5 to 7.25.22 timeranger2 marked the md2 files whose `__tm__` went
back (`<file>.tm_unordered`, and `"marks_tm_unordered": true` in a topic's
`topic_desc.json`), and the `mark-tm-order` command of `C_TRANGER` migrated an
older topic. They are gone: a tm query (`from_tm` / `to_tm`) is a filter on
every row of the key, whatever the topic. A store written by those releases
keeps its markers, ignored; nothing has to be run after the upgrade.

CAUTION: a **rollback to 7.25.5..7.25.22** meets topics whose
`topic_desc.json` still says `"marks_tm_unordered": true`, and files the new
binary appended to without tm markers. That binary trusts their tm ranges, so a
tm query can MISS rows. After such a rollback, run that release's own
`mark-tm-order all=1` on each tranger service, as its documentation says:

```bash
ycommand -c 'command-yuno id=<id> service=tranger_treedb_x command=mark-tm-order all=1'
```

(dy-upgrade-726)=
### Upgrading to 7.26.0: followers with their master, link events, write-attr

**1. The rt_disk followers of a topic upgrade with its master.** 7.26.0 tells
a key delete to the followers with the master's delete sequence (a signal
`disks/<rt_id>/.d<seq>.<key>`, and the record `<topic>/delete_seq.json`). A
follower of 7.25.22 under a 7.26.0 master takes each signal for a key: its
`key_deleted` callback gets a key that does not exist, and the real delete is
not heard. A 7.26.0 follower under a 7.25.22 master hears no delete, and logs
it when it opens a feed:

```text
The master of the topic does not signal key deletes as this follower hears
them: it is older than 7.26.0, or has not opened the topic since its upgrade.
```

The followers are the readers of another yuno's store: replica yunos,
`tr2list --follow`, the lists of a `C_TRANGER` that is not the master. Upgrade
them in the same window as the master. On one node `yunetas upgrade-yunos`
restarts every yuno at once, and a follower that starts before its master
opened the topic logs the line above as an ERROR, once per feed, on that first
boot only (the master makes the record when it opens the topic, and it stays):
that ERROR is expected there. When the feed hears its first delete after all,
the follower says so (*"The master signals key deletes as this follower hears
them"*). On another node, upgrade it right after the master's.

A key delete on a topic with rt_disk followers writes `delete_seq.json` before
it removes anything, and is refused when it cannot: on a full disk, a
read-only filesystem or with no file descriptor left, a treedb `delete-node`
answers `-1` with *"Cannot delete key: the delete sequence of the topic cannot
be read or recorded"* in the log. Free the disk and repeat the delete.

**2. `with_link_events` is on by default** (`C_NODE`, `C_TREEDB`, `C_AUTHZ`):
a link or an unlink publishes `EV_TREEDB_NODE_LINKED` / `UNLINKED`, not the
parent's `EV_TREEDB_NODE_UPDATED`. A yuno served by a v1 SPA (it reads the
parent's update) turns it off, in its `C_TREEDB` and in its `C_AUTHZ`:

```C
json_t *kw_treedbs = json_pack("{s:s, s:s, s:b, s:i, s:i, s:i, s:b}",
    "path", path,
    "filename_mask", "%Y",
    "master", 1,
    "xpermission", 02770,
    "rpermission", 0660,
    "exit_on_error", LOG_OPT_EXIT_ZERO,
    "with_link_events", 0   // the v1 SPA reads the parent's EV_TREEDB_NODE_UPDATED
);
priv->gobj_treedbs = gobj_create_service("treedbs", C_TREEDB, kw_treedbs, gobj);
```

and in the services of its `main.c`:

```text
{
    'name': 'authz',
    'gclass': 'C_AUTHZ',
    'kw': {
        'with_link_events': false
    }
}
```

(estadodelaire's `db_history` does exactly this.)

A gobj that subscribes to EVERY event of a treedb service now gets the two
events too: declare them in its FSM, or subscribe only the ones it handles. A
GUI that shows the hooks of a parent needs gobj-ui 7.25.26 or later.

**3. A negative `from_t` selects rows again.** Since v7 a negative bound
matched nothing. A yuno that opens its history with `from_t=-86400` (the
`db_history` yunos of hidraulia, estadodelaire and wattyzer) now LOADS the last
day of each key at its start: expect that memory and that start time on the
first boot.

**4. `write-attr` writes only `SDF_WR` attributes.** A persistent attribute
without `SDF_WR` answers *"attr not writable"*. A script that set one at run
time puts it in the yuno's config (`yunos/batches/<host>/<yuno>.json`), or uses
the attribute's own command (`set-max-sessions`, `set-email-user`, ...).

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
before the upgrade.

The snap is a pin, and how you remove it depends on where you want to end up.
`deactivate-snap` does NOT mean "stay here": before its reload it re-promotes
the **highest** release of every yuno (`promote_highest_release_yunos()`), and
while the bad release is still installed, the highest is the bad one.

**To go forward to a corrected release**, install it, then deactivate. The
corrected release is now the highest, and it is the one promoted:

```bash
yunetas sync-binaries ...          # push the corrected binary
yunetas upgrade-yunos              # it reuses the active snap, then deactivates
```

**To stay on the old release**, first remove the bad one, so that the old one
is the highest again. Only then deactivate. Do it with the snap active: the bad
release is not running, and it is not held by the snap (it was installed after
the shot), so both deletes go through.

```bash
ycommand -c 'list-yunos-instances yuno_role=<role>'           # find the bad release
ycommand -c 'delete-yuno id=<yuno_id> yuno_release=<bad_release>'
ycommand -c 'delete-binary id=<role> version=<bad_version>'
ycommand -c 'deactivate-snap'                                 # promotes the old one
ycommand -c 'list-yunos yuno_role=<role>'                     # release = the old one
```

Delete the binary too. If it stays installed, the next `find-new-yunos
create=1` (or `upgrade-yunos`) registers the bad release again, and the next
`deactivate-snap` starts it.

Do not `deactivate-snap` while the bad release is still installed. Doing so
starts the node again on the release you rolled back from.

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
