# Yuno lifecycle under the agent

This document describes what the Yuneta agent (`yuno_agent`) actually does when
a yuno is created, started, paused, killed, updated or deleted on a host. It is
the on-disk + in-memory + on-the-wire picture, not the marketing one.

> **Yuno = binary + configuration.** Those two halves are stored independently,
> versioned independently, and linked by the yuno record. Understanding that is
> the prerequisite to understanding everything else here.

The agent's authoritative source is
[`src/c_agent.c`](src/c_agent.c) (~11k lines) and its treedb schema
[`src/treedb_schema_yuneta_agent.c`](src/treedb_schema_yuneta_agent.c). Every
claim below cites the file:line it comes from.

---

## 1. Mental model

```
                         ┌───────────────────────┐
                         │       yuno_agent      │
                         │   (one per host)      │
                         └───────────┬───────────┘
                                     │ owns
                                     ▼
                         ┌───────────────────────┐
                         │  treedb (timeranger2) │
                         └───┬───────┬───────┬───┘
                             │       │       │
              ┌──────────────┘       │       └───────────────┐
              ▼                      ▼                       ▼
       ┌─────────────┐       ┌─────────────────┐      ┌─────────────┐
       │  binaries   │       │ configurations  │      │    yunos    │
       │  (per role  │       │ (per role+name  │      │ (registered │
       │  + version) │       │   + version,    │      │  instances) │
       │             │       │  blob zcontent) │      │             │
       └──────┬──────┘       └────────┬────────┘      └──────┬──────┘
              │                       │                      │
   resolves   │            materialised at start             │  references:
   yuno_role  │            into JSON files inside            │   role+role_version
   + role_    │            <realm>/<yuno>/bin/                │   role+name+name_version
   version    │                                              │
              ▼                                              ▼
   /yuneta/repos/<tags>/                          /yuneta/realms/<owner>/
   <role>/<version>/<role>                        <realm>/<yuno>/{bin,data,logs}
```

The agent is the **only** thing that should touch those directories. Manual
edits create drift with treedb that won't show up until the next start fails.

---

## 2. Data model

### 2.1 The `binaries` topic

Defined in [`treedb_schema_yuneta_agent.c`](https://github.com/artgins/yunetas/blob/7.8.3/yunos/c/yuno_agent/src/treedb_schema_yuneta_agent.c). Composite key:

| Column     | Role                                                    |
|------------|---------------------------------------------------------|
| `id`       | pkey, equals the yuno **role** (e.g. `mqtt_broker`)     |
| `version`  | pkey2, semver string extracted from the binary itself   |
| `size`     | file size, computed at install time                     |
| `date`     | timestamp                                               |
| `binary`   | absolute filesystem path                                |
| `yunos`    | fkey → which `yunos` rows currently use this binary     |
| `tags`     | classification (used in the on-disk path)               |

Multiple versions per role coexist. The yuno record decides which version is
used (see §2.3).

**On-disk path**, built by [`yuneta_repos_yuno_dir()`](https://github.com/artgins/yunetas/blob/7.8.3/yunos/c/yuno_agent/src/c_agent.c#L7468) ([c_agent.c:7313](https://github.com/artgins/yunetas/blob/7.8.3/yunos/c/yuno_agent/src/c_agent.c#L7313)) and
[`yuneta_repos_yuno_file()`](https://github.com/artgins/yunetas/blob/7.8.3/yunos/c/yuno_agent/src/c_agent.c#L7497) ([c_agent.c:7342](https://github.com/artgins/yunetas/blob/7.8.3/yunos/c/yuno_agent/src/c_agent.c#L7342)):

```
<yuneta_root_dir>/repos/<tags>/<role>/<version>/<role>
```

The file inside is the executable; the filename is the role again.

### 2.2 The `configurations` topic

Defined in [`treedb_schema_yuneta_agent.c`](https://github.com/artgins/yunetas/blob/7.8.3/yunos/c/yuno_agent/src/treedb_schema_yuneta_agent.c). Composite key:

| Column      | Role                                                                |
|-------------|---------------------------------------------------------------------|
| `id`        | pkey, format `<role>.<name>` (e.g. `mqtt_broker.broker_01`)         |
| `version`   | pkey2                                                               |
| `zcontent`  | compressed JSON payload — the actual config                         |

The blob lives in treedb. At start time the agent **materialises** it to a
`.json` file under the yuno's `bin/` directory and hands that path to the binary
via `--config-file='[<paths>]'` when it launches the process. Both steps run
inside [`run_yuno()`](https://github.com/artgins/yunetas/blob/7.8.3/yunos/c/yuno_agent/src/c_agent.c#L8451):
it writes the config through
[`build_yuno_running_script()`](https://github.com/artgins/yunetas/blob/7.8.3/yunos/c/yuno_agent/src/c_agent.c#L8058),
then exec's the binary with the materialised paths.

### 2.3 The `yunos` topic

Defined in [`treedb_schema_yuneta_agent.c`](https://github.com/artgins/yunetas/blob/7.8.3/yunos/c/yuno_agent/src/treedb_schema_yuneta_agent.c). Per-yuno record. Important
columns:

| Column            | Meaning                                                            |
|-------------------|--------------------------------------------------------------------|
| `id`              | pkey, the yuno's unique id                                         |
| `realm_id`        | fkey → the realm this yuno belongs to                              |
| `yuno_role`       | which binary (joins to `binaries.id`)                              |
| `role_version`    | which version of that binary                                       |
| `yuno_name`       | which configuration (joins to `configurations.id = role.name`)     |
| `name_version`    | which version of that configuration                                |
| `yuno_release`    | additional identity                                                |
| `yuno_disabled`   | bool — disabled yunos are skipped at boot                          |
| `yuno_running`    | bool — true while the agent holds an open channel to the yuno      |
| `yuno_playing`    | bool — true after a successful `EV_PLAY_YUNO_ACK`                  |
| `yuno_pid`        | last known pid (0 when not running)                                |
| `must_play`       | bool — auto-play after `EV_ON_OPEN` handshake                      |
| `start_priority`  | int 0..9 (default 5) — node-local launch order. See §4.8.          |
| `sched_priority`  | int (default 20) — injected as the yuno's `sched_priority` attr. See §4.8.|
| `cpu_core`        | int (default 0) — injected as the yuno's `cpu_core` attr. See §4.8. |
| `configurations`  | hook — N:M against `configurations` for multi-file config sets     |

A yuno record without a matching `binaries` row or `configurations` row fails at
create time, not at start time:
[`cmd_create_yuno()`](https://github.com/artgins/yunetas/blob/7.8.3/yunos/c/yuno_agent/src/c_agent.c#L4665)
rejects the request as soon as either lookup comes back empty.

`start_priority` / `sched_priority` / `cpu_core` are node placement decisions:
they live with the agent (this node), not in the binary or its config that
travel across nodes. They were added with `topic_version` 19→20 +
`schema_version` 22→23; the bump only refreshes the col schema (it does **not**
touch record data), so existing yunos keep their data and read the defaults
until set with `update-node`.

### 2.4 Realm and per-yuno layout

Built by [`build_yuno_private_domain()`](https://github.com/artgins/yunetas/blob/7.8.3/yunos/c/yuno_agent/src/c_agent.c#L7523) ([c_agent.c:7368](https://github.com/artgins/yunetas/blob/7.8.3/yunos/c/yuno_agent/src/c_agent.c#L7368)):

```
<yuneta_root_dir>/realms/<realm_owner>/<realm_name>.<realm_role>.<realm_env>/<yuno_role>_<yuno_name>/
  ├── bin/        ← N-<role>_<name>.json (materialised configs)
  ├── data/       ← <GClass>-<name>-persistent-attrs.json (only once a service saves one)
  └── logs/       ← N.log files
```

The `bin/` directory is **not** the binary. It is the working dir the yuno gets
config files from. The actual binary lives in `/yuneta/repos/` (see §2.1).

The `data/` directory holds the [**persistent attributes**](#persistent_attrs)
of the yuno's services — `SDF_PERSIST` attrs that a service changed and saved at run-time,
one `<GClass>-<name>-persistent-attrs.json` per service
([`db_save_persistent_attrs()`](#db_save_persistent_attrs)). It is created
**lazily**: it does not exist until the first attr is actually saved — a load
never creates it ([`db_load_persistent_attrs()`](#db_load_persistent_attrs)
reports "nothing saved" when the file is absent). At startup each service is
seeded from its merged config and **then** these saved values are written on
top — so **a persisted attr takes precedence over the same key in any
configuration file**. Only services (and `__root__`) load them; pure children
do not.

---

## 3. Command inventory

Registered in the agent's command table. Yuno + binary + config commands only
(admin, realm, certs and console commands omitted):

### Binaries

| Command           | Effect                                                                  |
|-------------------|-------------------------------------------------------------------------|
| `install-binary`  | Decode `content64`, introspect role+version, refuse if `(role, version)` already exists, write file, create treedb row. |
| `update-binary`   | Same as install but **overwrites** the existing `(role, version)` row and file in place. Description literally says *"WARNING: Don't use in production!"*. |
| `delete-binary`   | Pass `version=` to durably prune one installed version (per-instance delete); else the primary. Refuses if a yuno on **that** version still references it (validated per-yuno via [`gobj_get_node`](#gobj_get_node), so stale hook refs don't block) **or a snap tags it** (`__md_treedb__.tag`); `force=1` overrides. Then [`gobj_delete_node`](#gobj_delete_node) + [`rmrdir`](#rmrdir). |
| `list-binaries`   | `gobj_list_nodes("binaries", filter)`, returns one node per role — the binary **in use** (primary per `id`). |
| `list-binaries-instances` | `gobj_list_instances("binaries", "", filter)`, returns one row per installed `(role, version)` so every version is visible. |

### Configurations

| Command         | Effect                                                          |
|-----------------|-----------------------------------------------------------------|
| `create-config` (alias `install-config`) | Decode `content64`, read `version` from the `__version__` field **inside** it, refuse if `(id, version)` already exists, create the row in `configurations`. The `install-config` alias mirrors `install-binary`. |
| `update-config` | **Overwrite** the `zcontent` of an EXISTING `(id, version)` row (version again read from `__version__`). Fails *"Configuration not found"* if the row does not exist — it does **not** create. |
| `delete-config` | Pass `version=` to durably prune one config version (per-instance delete); else the primary. Fails if a yuno on **that** version references it (validated per-yuno via `gobj_get_node`, so an unused version prunes even while another is in use, and stale hook refs don't block); `force=1` overrides. |
| `list-configs`  | `gobj_list_nodes("configurations", filter)`, one node per `id` (the primary version). |
| `list-configs-instances` | `gobj_list_instances(...)`, one row per `(id, version)` so every version is visible. |
| `view-config`   | A ycommand console helper (not an agent command): reads the **stored** zcontent for a given `(id, version)`. Does not return the merged effective config that the running yuno actually sees — for that, ask the yuno itself with `command-yuno service=__yuno__ command=view-config`. |

### Yunos

| Command            | Effect                                                                      |
|--------------------|-----------------------------------------------------------------------------|
| `create-yuno`      | Create a row in `yunos`. Validates realm + binary + config existence.       |
| `delete-yuno`      | Pass `yuno_release=` to durably prune one release instance (e.g. a superseded/higher release), else the primary. Refuse if `yuno_running=true` (checked against the **primary**, since instance rows carry a stale flag) or `tagged` (unless `force=1`); delete row. |
| `enable-yuno`      | `yuno_disabled := false`.                                                 |
| `disable-yuno`     | `yuno_disabled := true`. Does **not** stop a running yuno.                |
| `run-yuno`         | Spawn matching `(disabled=false, running=false)` yunos. See §4.            |
| `kill-yuno`        | Orderly shutdown of matching running yunos. Sends `signal2kill` (SIGQUIT by default). See §4. |
| `play-yuno`        | Send `EV_PLAY_YUNO` event over the yuno's channel; flip `must_play=true`. |
| `pause-yuno`       | Send `EV_PAUSE_YUNO` event over the yuno's channel; flip `must_play=false`. |
| `command-yuno`     | Wildcard: forward an arbitrary command to a running yuno's service.       |
| `list-yunos`       | All `yunos` rows with current pid + state.                                |
| `view-yuno-config` | The stored configs attached to a yuno (still not the effective merged one). |
| `stats-yuno`       | Forward a stats request to the running yuno.                              |

Permission gating is per-command via `pm_<name>` schemas.

---

## 4. Lifecycle, step by step

### 4.1 State machine of a single yuno (as the agent sees it)

![Yuno lifecycle state machine: create-yuno registers a STOPPED yuno; run-yuno forks it to STARTING; EV_ON_OPEN reaches RUNNING (paused); play-yuno and pause-yuno toggle paused and playing; kill-yuno (SIGQUIT) returns it to STOPPED; delete-yuno (only if stopped) reaches DELETED.](../../../docs/doc.yuneta.io/_static/yuno_lifecycle_fsm.svg)

The same machine in text (the precise transitions the agent drives on the
per-yuno record fields):

```
                                  delete-yuno
                              ┌─────────────────────┐
                              │  (only if stopped)  │
                              ▼                     │
                          ┌────────┐               ┌┴────────────┐
   create-yuno  ─────────►│ STOPPED│◄──────────────┤  DELETED    │
                          └────┬───┘  EV_ON_CLOSE  └─────────────┘
                               │                        ▲
                               │                        │
                  run-yuno     │                        │ kill-yuno
                  (fork+exec   │                        │ (SIGQUIT)
                   + handshake)│                        │
                               ▼                        │
                          ┌────────┐                    │
                          │STARTING│                    │
                          │(pid    │                    │
                          │ alive, │                    │
                          │ no chan│                    │
                          │ yet)   │                    │
                          └────┬───┘                    │
                               │  EV_ON_OPEN            │
                               │  (yuno reports pid +   │
                               │   watcher_pid)         │
                               ▼                        │
                          ┌────────┐    EV_PAUSE_YUNO   │
                          │RUNNING ├────────────────┐   │
                          │playing │                │   │
                          │=true   │◄──────┐        │   │
                          └───┬────┘       │ ACK    ▼   │
                              │            │   ┌────────┴┐
                              └────────────┘   │ RUNNING │
                                               │ paused  │
                       EV_PLAY_YUNO_ACK        │ =true   │
                                               └─────────┘
```

Note: the agent itself stays in `ST_IDLE` always. The diagram above is **not**
the agent's own FSM — it is the lifecycle of a *managed yuno* as the agent tracks
it through the per-yuno record fields (`yuno_running`, `yuno_playing`,
`yuno_pid`), driven by the event types it receives over each yuno's channel.

### 4.2 Registration: `create-yuno`

Requires realm + binary `(yuno_role, role_version)` + config
`(yuno_role.yuno_name, name_version)` to already exist. Defaults missing
versions to *latest*. Writes the row with `yuno_running=false`,
`yuno_disabled=false`, `yuno_pid=0`.

### 4.3 Start: `run-yuno`

Driven by [`run_yuno()`](https://github.com/artgins/yunetas/blob/7.8.3/yunos/c/yuno_agent/src/c_agent.c#L8451):

1. Select yunos: `disabled=false ∧ running=false`.
2. Resolve the binary via [`get_yuno_binary()`](https://github.com/artgins/yunetas/blob/7.8.3/yunos/c/yuno_agent/src/c_agent.c#L7663):
   prefer the active snapshot ([`gobj_list_snaps()`](#gobj_list_snaps)), falling back to a direct
   `(role, role_version)` lookup.
3. Materialise each `configurations.zcontent` blob to a JSON file under the
   yuno's `bin/` directory ([`build_yuno_running_script()`](https://github.com/artgins/yunetas/blob/7.8.3/yunos/c/yuno_agent/src/c_agent.c#L8058)).
4. Build a launcher shell script that runs the binary with:
   ```
   <binary> --config-file='["bin/1-role_name.json", "bin/2-role_name.json", …]' "$@"
   ```
5. `run_process2(bfbinary, argv)` — `fork`/`exec` via a wrapper.
6. The yuno opens a channel back to the agent and emits **`EV_ON_OPEN`**
   carrying its `pid` and `watcher_pid` (handler
   [`ac_on_open()`](https://github.com/artgins/yunetas/blob/7.8.3/yunos/c/yuno_agent/src/c_agent.c#L11000)).
   Only after this handshake does the agent set `yuno_running` to `true` and
   store `yuno_pid` / `watcher_pid`.
7. If `must_play=true`, the agent fires `play-yuno` automatically right after
   the open (`ac_on_open()`), **unless** the command was issued with `play=0`
   (see below).

**Implication**: a yuno that forks fine but never opens the channel never
becomes `running` from the agent's point of view, even if `ps` shows the
process alive. See §5 *Stale pid*.

#### Command response: one answer per command, and the `play=0` knob

`run-yuno` returns a **single** command answer once all launched yunos have
connected back — like `kill-yuno`/`pause-yuno`/`play-yuno`. It aggregates the
per-yuno `EV_ON_OPEN` ACKs into one [`C_COUNTER`](#gclass-c-counter) (`max_count=total`) created
after the launch loop, exactly mirroring the other three commands.

The implicit auto-play of step 7, however, is inherently per-yuno and async
(each yuno connects at its own time), so in the default `play=1` mode the
caller also sees one extra `play-yuno` answer per `must_play` yuno. Scripts
that need exactly one answer per command should split the two phases:

```bash
ycommand -c 'run-yuno play=0'   # launch only → 1 answer ("N yunos found to run")
ycommand -c 'play-yuno'         # play already-running yunos → 1 aggregated answer
```

`play=0` (default `1`, backward-compatible) suppresses the auto-play for that
launch only. The agent records the `launch_id` in an in-memory set
(`priv->no_play_launches`); `ac_on_open()` consumes it by matching the
connecting yuno's `identity_card`launch_id` and deletes it on first connect.
It is not a treedb column and does not touch `must_play`, so a watcher crash
relaunch (which reuses the same `launch_id`, now absent) still reconciles
`must_play` per §4.6.

### 4.4 Pause / Play

These are **not** process signals. They are gobj events delivered through the
yuno's open channel:

- `play-yuno` → `EV_PLAY_YUNO` → yuno does whatever "playing" means for it →
  `EV_PLAY_YUNO_ACK` → agent sets `yuno_playing=true`.
- `pause-yuno` → `EV_PAUSE_YUNO` → … → `EV_PAUSE_YUNO_ACK` → agent sets
  `yuno_playing=false`.

Most yunos use the play/paused gate to enable/disable I/O processing without
exiting. The process never stops; only its inputs are gated.

### 4.5 Stop: `kill-yuno`

An **orderly shutdown**, not a SIGKILL, performed by
[`kill_yuno()`](https://github.com/artgins/yunetas/blob/7.8.3/yunos/c/yuno_agent/src/c_agent.c#L8565):

1. Read the `signal2kill` attribute (default `SIGQUIT`).
2. `kill(yuno_pid, signal2kill)`.
3. If the chosen signal is `SIGKILL`, the watcher is killed too.
4. **No timer-based escalation in code**. The agent trusts the yuno's signal
   handler to actually shut down. If it doesn't, the yuno stays "running"
   from the agent's record forever (see §5).
5. When the channel closes, [`ac_on_close()`](https://github.com/artgins/yunetas/blob/7.8.3/yunos/c/yuno_agent/src/c_agent.c#L11290)
   flips `yuno_running=false`, `yuno_playing=false`, `yuno_pid=0`.

### 4.6 Crash detection and reconciliation

**From the agent's point of view**, a crash is indistinguishable from a
clean kill: `EV_ON_CLOSE` fires, `ac_on_close()` runs, the treedb row
flips `yuno_running=false`. The agent has no `SIGCHLD` handler and does
not poll pids. It does not know "exited normally" from "segfaulted".

**The restart, however, does not depend on the agent.** Every yuno
started with `--start` runs under a per-yuno watcher process (the
first-fork survivor in [`ydaemon.c`](https://github.com/artgins/yunetas/blob/7.8.3/kernel/c/root-linux/src/ydaemon.c)). When the yuno child dies abnormally
(any signal other than `SIGKILL`, or any non-zero exit code), the watcher
sleeps 2s and re-execs the same binary — and it does so **regardless of
whether the agent is running**. A yuno is an autonomous machine; the
watcher is what makes that concrete. See
[`ENTRY_POINT.md`](ENTRY_POINT.md#entry-point-watcher) §4
for the full decision matrix.

What the agent contributes on top:

- It logs the closed channel and clears `yuno_pid`/`yuno_playing`. A
  fresh `run-yuno` is **not** needed; the watcher has already spawned
  a new child with a new pid that will reconnect to the agent on its own
  (it goes through the normal `EV_ON_OPEN` handshake).
- At agent boot the timer `ac_timeout()` ([c_agent.c:11393](https://github.com/artgins/yunetas/blob/7.8.3/yunos/c/yuno_agent/src/c_agent.c#L11393)) runs:
    1. [`run_util_yunos()`](https://github.com/artgins/yunetas/blob/7.8.3/yunos/c/yuno_agent/src/c_agent.c#L9018) ([c_agent.c:9018](https://github.com/artgins/yunetas/blob/7.8.3/yunos/c/yuno_agent/src/c_agent.c#L9018)) — yunos tagged `util`,
       ignoring `disabled`.
    2. [`run_enabled_yunos()`](https://github.com/artgins/yunetas/blob/7.8.3/yunos/c/yuno_agent/src/c_agent.c#L8954) ([c_agent.c:8954](https://github.com/artgins/yunetas/blob/7.8.3/yunos/c/yuno_agent/src/c_agent.c#L8954)) — every row with
       `disabled=false ∧ running=false`.

   This reconciliation matters only for yunos that **don't** have a live
   watcher (e.g. the agent itself was killed and brought back; any yunos
   whose own watchers happened to die too).

   **The agent launches a yuno once, and there its responsibility ends.**
   `running` comes from `ac_on_open()`, so between the two sweeps a yuno is
   launched but not yet registered — `run_yuno()` marks it as launching and
   both sweeps skip a marked yuno, `ac_on_open()` clearing the mark. Without
   that, anything slower than `timerStBoot` to open (a yuno loading a treedb,
   on the cold machine a real boot gives you) was launched a second time. The
   `run-yuno` command honors the same mark, so an operator can't land a second
   instance on a yuno that is still coming up either. The agent must not retry
   a yuno that died before opening: an abnormal death is the watcher's job, and
   a clean `exit 0` is the yuno deciding to stay down.

   The mark carries its launch time and **expires after `timeout_expiration`**
   (30s). A yuno that dies before opening never clears its mark, and the agent
   watches no pids: without the expiry, one failed launch would block `run-yuno`
   for that yuno until the agent restarted.

Forensics: a crashed yuno also dumps a core at `/var/crash/core.<role>`
(sysctl + PAM limits configured by the `.deb`, see
[`ENTRY_POINT.md`](ENTRY_POINT.md#entry-point-crash-forensics) §8).
The watcher emits a `Daemon relaunched` log line on every relaunch
(`MSGSET_SYSTEM`) — grep for it to spot silent crash loops.

### 4.7 Deletion: `delete-yuno`

Refuses if `yuno_running=true`. Optionally refuses on tagged yunos unless
`force=1`. Removes the treedb row; the on-disk `bin/` directory is cleaned by
treedb cascade.

**Pruning one release instance.** Without `yuno_release=` the command targets the
in-memory primary (historic behaviour). With `yuno_release=<rel>` it durably
prunes that one instance — useful to drop a superseded or mistakenly-created
**higher** release without a snap rollback. This rides the treedb per-instance
delete ([`treedb_delete_instance`](#treedb_delete_instance)), which tombstones **every** md2 row of the
`(id, yuno_release)` — a treedb instance spans several rows (create + each
link/save re-appends one), so a partial tombstone would let an earlier row
resurrect the release on reload. The running-guard consults the primary (the
per-instance row carries a stale `yuno_running`). `delete-config`/`delete-binary`
gained the symmetric `version=` form for config/binary versions.

### 4.8 Start order and CPU placement

Three planes share the word "priority" — keep them apart:

| Plane | Where | What it controls |
|-------|-------|------------------|
| OS scheduling | yuno attr `sched_priority` + `cpu_core` ([`c_yuno.c`](https://github.com/artgins/yunetas/blob/7.8.3/kernel/c/root-linux/src/c_yuno.c), [`boost_process_performance`](https://github.com/artgins/yunetas/blob/7.8.3/kernel/c/root-linux/src/c_yuno.c#L4561)) | `sched_setscheduler` + CPU affinity of the process |
| Intra-yuno | each service's `priority` 0..9 ([`manage_services.c`](https://github.com/artgins/yunetas/blob/7.8.3/kernel/c/root-linux/src/manage_services.c)) | order services start **within** a yuno |
| Inter-yuno | agent col `start_priority` 0..9 (this section) | order yunos start **on this node** |

**Launch order.** [`cmd_run_yuno`](https://github.com/artgins/yunetas/blob/7.8.3/yunos/c/yuno_agent/src/c_agent.c#L5087) sorts the matched yunos by `start_priority`
**ascending** before spawning ([`sort_yunos_by_start_priority`](https://github.com/artgins/yunetas/blob/7.8.3/yunos/c/yuno_agent/src/c_agent.c#L8366), c_agent.c). Lower
goes first: utilities (logcenter / emailsender / auth_bff) → gates → dba. Use a
low number for infrastructure a node can't work without. The same ascending sort
is applied by `run_enabled_yunos`, so a node bounce ([`restart_nodes`](https://github.com/artgins/yunetas/blob/7.8.3/yunos/c/yuno_agent/src/c_agent.c#L9430) /
`deactivate-snap`) and the at-startup relaunch honour the tiers too. (The force
SIGKILL pass inside `restart_nodes` is left unordered on purpose: SIGKILL has no
graceful drain to sequence.)

**Shutdown order.** `kill-yuno` and `pause-yuno` sort **descending**, so the
utilities die **last** — e.g. logcenter stays up long enough to capture
everyone else's shutdown logs. Within one priority, treedb order is preserved
(stable). Single-target commands (by `id`) are unaffected.

**Default on creation.** `create-yuno` seeds `start_priority = 1` for a yuno
carrying the `util` tag — the same set `run_util_yunos` starts first — so
framework utilities are born at the top tier without operator action. A
genuinely new yuno otherwise takes the column default (5). No app role names are
hard-coded in the agent; assign app tiers per node with
[`tools/agent/set_start_priorities.py`](https://github.com/artgins/yunetas/blob/7.8.3/tools/agent/set_start_priorities.py).

**Inherited across version bumps.** A version-bump deploy (`find-new-yunos
create=1`) does NOT reset placement: [`cmd_find_new_yunos`](https://github.com/artgins/yunetas/blob/7.8.3/yunos/c/yuno_agent/src/c_agent.c#L4486) copies
`start_priority` / `sched_priority` / `cpu_core` from the prior primary row of
the same id into the emitted `create-yuno`, so the operator-set tiers survive the
bump. [`set_start_priorities.py`](https://github.com/artgins/yunetas/blob/7.8.3/tools/agent/set_start_priorities.py) is therefore a first-time-only step per node, not
a per-deploy chore. (Same-version `update-binary` hot-patches keep the existing
row and were never affected.)

**CPU placement.** `sched_priority` and `cpu_core` are injected into the
agent-built config file #1 as the yuno's `sched_priority` / `cpu_core` attrs
(`build_yuno_running_script`). They are **defaults only**: the user config file
is merged after #1, so an explicit value in the yuno's own config still wins
(precedence stays with the deployer). `cpu_core=0` (the default) means no
affinity boost, i.e. unchanged behaviour.

Set any of the three live, no redeploy. The `record=` inline form is NOT
coerced from text by the ycommand CLI ("What record?"), so pass the node as a
**strict-JSON** file via `content64=$$(<file>)` (single quotes / unquoted keys
fail to decode):

```bash
printf '{"id":"<yuno_id>","start_priority":1,"cpu_core":2,"sched_priority":10}' > /tmp/rec.json
ycommand -c "command-agent service=treedb_yuneta_agent command=update-node topic_name=yunos content64=\$\$(/tmp/rec.json)"
```

(Over the websocket/JSON API the `record` field is a real dict and works
directly; the file form is only needed for the text CLI.)

---

## 5. Sharp edges (read these before touching production)

### 5.1 `update-binary` fails while the yuno is running (text-file-busy)

`update-binary` base64-decodes `content64` to
`/yuneta/realms/agent/agent/temp/<role>`, then copies it over
`/yuneta/repos/<tags>/<role>/<version>/<role>`. The agent **execs running
yunos directly from that repos path**, so if a process is running that exact
file the copy is refused by the kernel (`ETXTBSY`) and the command returns:

```
ERROR -1: Cannot copy '/yuneta/realms/agent/agent/temp/<role>'
          to '/yuneta/repos/.../<version>/<role>'
```

It does **not** corrupt the live process — Linux simply won't let you overwrite
a busy executable. The command's description still says *"WARNING: Don't use in
production!"*.

**So the same-version hot-patch order is mandatory: `kill-yuno <role>` FIRST,
then `update-binary`, then `run-yuno`.** (`$$(<role>)` in ycommand reads the
freshly-built binary from `$YUNETAS_YUNOS` = `outputs/yunos/<role>`, so a plain
`make install` is enough to stage the new build.) For a real release, prefer
`install-binary` with a bumped version.

> **Note — `list-binaries` shows the binary IN USE (the primary node per role).**
> A runtime `update-binary` (same version) mutates that primary node in place, so
> the new size/date appear immediately. An `install-binary` of a NEW version does
> **not** change what `list-binaries` shows until `deactivate-snap` promotes and
> reloads it — which is correct: the new binary is not in use until then. To see
> every installed `(role, version)` from the moment it lands, use
> `list-binaries-instances` ([`gobj_list_instances`](#gobj_list_instances), the pkey2 iterator refreshed
> at runtime by dbf532ec9).

### 5.2 Stale `yuno_running=true` after a hard crash

If the yuno dies in a way that leaves the channel open (rare, but
`SIGKILL`-from-outside is one of them) `ac_on_close()` is never invoked. The
record keeps `yuno_running=true` and `yuno_pid=<old-pid>`.

On the next `run-yuno`, `ac_on_open()` checks `getpgid(_pid) >= 0`.
If the old pid happens to have been reused, the agent
**kills the new occupant**. This is the worst flavour of
flapping. If you suspect a stale pid, manually clear `yuno_running` and
`yuno_pid` in treedb before retrying.

### 5.3 No SIGKILL escalation, and the watcher gotcha

`kill-yuno` sends one signal and waits. There's no "after 30 seconds, try
SIGKILL". A yuno that swallows `SIGQUIT` (or whose signalfd handler is
wedged) stays alive and the agent's record stays stuck in `running`.

Worse, a naive `kill -9 <yuno_pid>` from the shell **does not** kill the
yuno permanently: the watcher classifies SIGKILL on the child as
"abnormal" and relaunches after 2s. You need to kill **both** the child
and its watcher, or — better — use `kill-yuno force=1` /
`set-quick-kill`, which sends SIGKILL to both pids and is the only way
the agent can actually take down an uncooperative yuno
([`c_agent.c`](https://github.com/artgins/yunetas/blob/7.8.3/yunos/c/yuno_agent/src/c_agent.c), and [`ENTRY_POINT.md §7`](ENTRY_POINT.md#entry-point-kill-yuno)).

### 5.4 `pause` ≠ `SIGSTOP`, `play` ≠ `SIGCONT`

Pause/Play are channel events. The process is never frozen at the kernel
level. If you actually want the process suspended (e.g. for `gdb attach`),
the agent gives you nothing — use the shell.

### 5.5 `update-config` does not hot-reload

c_agent.c updates the treedb blob. The launcher script is rebuilt only
at `run-yuno`. So **changes take effect on next start**. There is no
re-materialisation of the JSON files on disk for a running yuno.

### 5.6 `disable-yuno` does not stop a running yuno

c_agent.c only flips the flag. A `disabled=true` yuno that was already
running keeps running until you `kill-yuno`. The flag only prevents the next
`run-yuno` from picking it up.

### 5.7 `install-binary` vs `update-binary`

| Same `(role, version)` exists?    | `install-binary`  | `update-binary`        |
|-----------------------------------|-------------------|------------------------|
| Yes                               | refuses           | overwrites file + row  |
| No                                | creates           | creates                |

Pick `install-binary` for new versions. Pick `update-binary` only when you
truly want to overwrite — and then only if no yuno is running that version.

---

## 6. Operational recipes

All examples assume [`ycommand`](#util-ycommand) is talking to the local agent.

> **Bulk reconciliation.** To compare every binary the agent has installed
> against the freshly built ones in `outputs/yunos` and push the differences in
> one pass, use [`tools/agent/sync_binaries.py`](https://github.com/artgins/yunetas/blob/7.8.3/tools/agent/sync_binaries.py) (drives from the agent's
> installed set; proposes `install-binary` for version bumps and `update-binary`
> for same-version rebuilds; `-n` for a dry run). For a same-version rebuild it
> also runs the per-role hot-patch cycle below (kill → poll → update → restore
> run/play state, scoped by `yuno_role`); `--no-restart` keeps it print-only.
> The recipes below are the manual, per-yuno equivalents.

### 6.1 Onboard a brand-new yuno

```bash
# 1. install the binary (new role or new version)
ycommand -c 'install-binary content64=$$(my_role)'

# 2. install its configuration (create-config — the row does not exist yet;
#    version is read from the __version__ field inside the file)
ycommand -c 'create-config id=my_role.my_name content64=$$(my_role_my_name.json)'

# 3. create the yuno record (links binary + config to a realm)
ycommand -c 'create-yuno realm_id=<realm> yuno_role=my_role yuno_name=my_name'

# 4. enable, launch, then play (two steps = one response each)
ycommand -c 'enable-yuno id=<yuno_id>'
ycommand -c 'run-yuno play=0 id=<yuno_id>'
ycommand -c 'play-yuno id=<yuno_id>'

# 5. verify
ycommand -c 'list-yunos'
```

### 6.2 Hot-patch the binary at the same version (`update-binary`)

Use this when the version number in `main.c` (`APP_VERSION`) is
**unchanged** — e.g. a `RelWithDebInfo` rebuild for a quick fix
during a debugging session. `update-binary` overwrites the existing
`{role}/{version}/` slot in `/yuneta/repos/...` and the matching
treedb row. There is no rollback path; the previous bytes are gone.

Always **orderly shutdown first**; never `update-binary` over a live
mmap. The command itself does NOT refuse if a yuno using the binary
is running (see §5.1).

```bash
# 1. build (APP_VERSION unchanged in main.c)
cd /yuneta/development/yunetas/yunos/c/<yuno>/build && make clean && make install

# 2. orderly shutdown via the agent (NOT a manual kill)
ycommand -c 'kill-yuno yuno_role=<role>'

# 3. wait until it really left
ycommand -c 'list-yunos'    # expect yuno_running=false

# 4. overwrite the same-version slot in the agent repo
ycommand -c 'update-binary id=<role> content64=$$(<role>)'
ycommand -c 'list-binaries' # verify size/date

# 5. launch back, then play (two steps = one response each)
ycommand -c 'run-yuno play=0'
ycommand -c 'play-yuno'
```

For a real version bump (`1.3.1.0` → `1.3.1.1`, `7.3.4` → `7.4.0`,
…) use the upgrade flow in §6.5 instead — `update-binary` is the
wrong tool, and the `WARNING: Don't use in production!` description
in `command-yuno` help applies precisely to that misuse.

### 6.3 Change a yuno's configuration

`update-config` does not hot-reload — the yuno must be restarted. It overwrites
an existing config (version is read from the `__version__` field in the file);
to install a NEW version use `create-config` (alias `install-config`) instead.

```bash
ycommand -c 'update-config id=<role>.<name> content64=$$(<file>.json)'
ycommand -c 'kill-yuno id=<yuno_id>'
ycommand -c 'run-yuno  play=0 id=<yuno_id>'
ycommand -c 'play-yuno id=<yuno_id>'

# verify the EFFECTIVE merged config (not the stored one)
ycommand -c 'command-yuno id=<yuno_id> service=__yuno__ command=view-config'
```

### 6.4 Retire a yuno

```bash
ycommand -c 'kill-yuno    id=<yuno_id>'    # orderly shutdown
ycommand -c 'list-yunos'                   # confirm running=false
ycommand -c 'disable-yuno id=<yuno_id>'    # belt and braces
ycommand -c 'delete-yuno  id=<yuno_id>'    # refuses if running
# binary stays in /yuneta/repos until you delete-binary it
```

### 6.5 Upgrade a yuno to a new release (version bump)

When `APP_VERSION` in `main.c` changes (`1.3.1.0` → `1.3.1.1`, or a
yunetas-side bump like `7.3.4` → `7.4.0`), the canonical flow is
**not** `kill-yuno` + `run-yuno`. Both the old and the new
yuno-instance rows exist after registration, and the agent's
in-memory primary index for the `yunos` topic still points at the
old `pkey2` (`yuno_release`). Plain `run-yuno` will re-launch the
older release.

```bash
# 1. Build the new version
cd /yuneta/development/yunetas/yunos/c/<yuno>/build && make clean && make install

# 2. Push the new binary to the agent
#    install-binary creates a new <role>/<version>/ slot; refuses if
#    (role, version) already exists — that is the safety vs update-binary.
ycommand -c 'install-binary id=<role> content64=$$(<role>)'

# 3. Register a yuno-instance row at the new role_version
#    create=1 actually persists; without it, find-new-yunos just lists
#    the commands it would run. Both pkey2 rows now coexist:
#       <id> <realm> <role>  <yuno_release=OLD>
#       <id> <realm> <role>  <yuno_release=NEW>
#    Visible via `list-yunos-instances`; `list-yunos` still shows OLD
#    as primary.
ycommand -c 'find-new-yunos create=1'

# 4. Force the agent to promote the newest release to primary and restart
#    deactivate-snap (no args, no active snap) is the only command that
#    triggers `restart_nodes()`: SIGKILL every running yuno, then —
#    BEFORE the reload — `promote_highest_release_yunos()` re-appends the
#    highest non-disabled `yuno_release` per id so it becomes the highest
#    rowid. (The treedb primary is the highest-ROWID record, not the
#    highest version; lifecycle/snap writes can leave an older release on
#    top, which is why a plain reload alone is not enough — the old
#    "force volatil" TODO.) gobj_stop/start then rebuilds the primary
#    index from disk with the promoted release on top, and every
#    must_play yuno runs on it. Equivalent to `yshutdown` +
#    `restart-yuneta` but without restarting the agent process itself.
ycommand -c 'deactivate-snap'

# 5. Verify
ycommand -c 'list-yunos yuno_role=<role> yuno_running=true'
# release column should now read the new version. The OLD pkey2 row
# stays in treedb for rollback (see §6.6 below).
```

> **CLI shortcut.** Steps 3–4 (plus an optional rollback snapshot before them)
> are bundled by `yunetas upgrade-yunos` (`tui_yunetas` ≥ 0.10.0): it shoots a
> rollback snap (idempotent by name, default `pre-upgrade-<YYYYMMDD>`,
> `--no-snap` to skip), runs `find-new-yunos` as a preview and asks before
> `create=1`, then `deactivate-snap`. Steps 1–2 (build + `install-binary`, or a
> `yunetas sync-binaries` push) still run first; `--dry-run` prints the agent
> commands without executing them. The raw `ycommand` sequence above remains the
> manual equivalent.

#### Caveats

- **Node-wide bounce.** `restart_nodes()` SIGKILLs every running
  yuno on the node, not just the one being upgraded. Tolerable for
  kernel-yuno rotations (`auth_bff`, `emailsender`, `logcenter`);
  worth flagging before doing it during a busy window on a realm
  with many citizen yunos. (The version-promotion half of the old
  "force volatil" TODO is now handled by
  [`promote_highest_release_yunos()`](https://github.com/artgins/yunetas/blob/7.8.3/yunos/c/yuno_agent/src/c_agent.c#L8882); what remains is making the
  bounce per-role instead of node-wide.)

- **No orderly shutdown.** The SIGKILL means yunos do NOT run their
  `mt_stop` / FSM stop callbacks. For protocols that flush state on
  exit (mqtt close packets, treedb final saves), prefer manually
  `kill-yuno`-ing each role before `deactivate-snap`.

- **`find-new-yunos create=1` without an actual binary will not
  break anything**, but the row it creates will have no
  corresponding binary file and `run-yuno` will then fail with
  *"primary binary not found"* once `deactivate-snap` promotes it.
  Always do step 2 first.

### 6.6 Rollback after an upgrade

The OLD `pkey2` row stays in `yunos`, and the OLD binary stays in
`/yuneta/repos/<role>/<old_version>/`. To revert:

```bash
# Take a snap of the current state first (optional but recommended)
ycommand -c 'shoot-snap name=<rollback-tag> description="pre-upgrade"'

# … later, if the new release misbehaves, activate the snap
ycommand -c 'activate-snap name=<rollback-tag>'
# This calls the same restart_nodes() cycle, but with the snap
# active get_yuno_binary refuses to fall back to list_instances
# and primary-only lookup is enforced — the OLD row wins again.

# To remove the pin once you've decided:
ycommand -c 'deactivate-snap'
```

> **Snaps pin the binaries they reference — `delete-binary` respects that.**
> `shoot-snap` stamps the snap's id on every topic's current-primary record
> (its md2 `user_flag`, surfaced as `__md_treedb__.tag`), `binaries` included.
> A binary the snap tagged must survive for `activate-snap` to roll back to it —
> otherwise the treedb pointer is restored but the file is gone and `run-yuno`
> fails with *"primary binary not found"*. So `delete-binary` refuses to remove
> a snap-tagged binary (the kernel's `treedb_delete_node` enforces it; the agent
> also reports it clearly and never reaches the `rmrdir`). Pass `force=1` to
> delete anyway — that **breaks** the rollback the snap was protecting.

### 6.7 Inspecting a snap (`snaps` / `snap-content`)

A snap is a point-in-time tag (a numeric `user_flag`, `1..65534`) applied
to the treedb records that were current when it was shot. `snaps` lists
them; `snap-content` shows what a given snap captured — useful to see what
a rollback snap (§6.6) would restore *before* you `activate-snap` it.

```bash
# List the snaps (id, name, date, active, description)
ycommand -c 'snaps'

# Overview: WHERE the snap points — every topic it tags and how many
# records each. Select the snap by name, id, or the legacy snap_id.
ycommand -c 'snap-content name=<tag>'
#   → realms:3  yunos:16  binaries:15  configurations:16  public_services:2
#     "snap 2 spans 5 topic(s); add topic_name=<topic> to see the records"

# Drill into one topic's foto (the records as captured in that snap)
ycommand -c 'snap-content name=<tag> topic_name=yunos'
```

`snap-content` selects the snap by any of:

- `name=<tag>` — the friendly name, resolved against `__snaps__`,
- `id=<n>` / `snap_id=<n>` — the numeric id shown by `snaps` (`snap_id` is
  the legacy form, kept for backward compatibility).

`topic_name` is optional: omit it for the per-topic overview (a cheap
count-only walk that does not load records), or pass `topic_name=<topic>`
for the full record foto of that topic. With neither a valid snap nor a
name it answers `What snap? give snap_id/id (1..65534) or name`.

---

## 7. Code pointers (one-pager)

| What                                  | Where                                                                 |
|---------------------------------------|-----------------------------------------------------------------------|
| Agent gclass                          | [`src/c_agent.c`](https://github.com/artgins/yunetas/blob/7.8.3/yunos/c/yuno_agent/src/c_agent.c)                                                       |
| Treedb schema (yunos / binaries / configurations) | [`src/treedb_schema_yuneta_agent.c`](https://github.com/artgins/yunetas/blob/7.8.3/yunos/c/yuno_agent/src/treedb_schema_yuneta_agent.c)                          |
| Command table                         | [`src/c_agent.c`](https://github.com/artgins/yunetas/blob/7.8.3/yunos/c/yuno_agent/src/c_agent.c)                                               |
| Agent attributes                      | [`src/c_agent.c`](https://github.com/artgins/yunetas/blob/7.8.3/yunos/c/yuno_agent/src/c_agent.c)                                               |
| FSM (`ST_IDLE`, event types)          | [`src/c_agent.c`](https://github.com/artgins/yunetas/blob/7.8.3/yunos/c/yuno_agent/src/c_agent.c)                                           |
| Subscription model (SERVICE)          | [`src/c_agent.c`](https://github.com/artgins/yunetas/blob/7.8.3/yunos/c/yuno_agent/src/c_agent.c)                                             |
| Per-yuno on-disk path                 | [`src/c_agent.c`](https://github.com/artgins/yunetas/blob/7.8.3/yunos/c/yuno_agent/src/c_agent.c) (`build_yuno_private_domain`)                    |
| Repos binary path                     | [`src/c_agent.c`](https://github.com/artgins/yunetas/blob/7.8.3/yunos/c/yuno_agent/src/c_agent.c)                                             |
| Binary resolution at start            | [`src/c_agent.c`](https://github.com/artgins/yunetas/blob/7.8.3/yunos/c/yuno_agent/src/c_agent.c) (`get_yuno_binary`)                         |
| Launcher script + `--config-file`     | [`src/c_agent.c`](https://github.com/artgins/yunetas/blob/7.8.3/yunos/c/yuno_agent/src/c_agent.c)                                             |
| `EV_ON_OPEN` handshake                | [`src/c_agent.c`](https://github.com/artgins/yunetas/blob/7.8.3/yunos/c/yuno_agent/src/c_agent.c) (`ac_on_open`)                                  |
| `EV_ON_CLOSE` (death detection)       | [`src/c_agent.c`](https://github.com/artgins/yunetas/blob/7.8.3/yunos/c/yuno_agent/src/c_agent.c) (`ac_on_close`)                                 |
| Boot-time reconciliation              | [`src/c_agent.c`](https://github.com/artgins/yunetas/blob/7.8.3/yunos/c/yuno_agent/src/c_agent.c) (`run_enabled_yunos`, `ac_timeout`)  |
