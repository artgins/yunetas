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
   <role>/<version>/<role>                        <realm>/<yuno>/{bin,logs}
```

The agent is the **only** thing that should touch those directories. Manual
edits create drift with treedb that won't show up until the next start fails.

---

## 2. Data model

### 2.1 The `binaries` topic

Defined in `treedb_schema_yuneta_agent.c:499-604`. Composite key:

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

**On-disk path**, built by `yuneta_repos_yuno_dir()` (c_agent.c:7088) and
`yuneta_repos_yuno_file()` (c_agent.c:7109):

```
<yuneta_root_dir>/repos/<tags>/<role>/<version>/<role>
```

The file inside is the executable; the filename is the role again.

### 2.2 The `configurations` topic

Defined in `treedb_schema_yuneta_agent.c:607-674`. Composite key:

| Column      | Role                                                                |
|-------------|---------------------------------------------------------------------|
| `id`        | pkey, format `<role>.<name>` (e.g. `mqtt_broker.broker_01`)         |
| `version`   | pkey2                                                               |
| `zcontent`  | compressed JSON payload — the actual config                         |

The blob lives in treedb. At start time the agent **materialises** it to disk
under the yuno's `bin/` directory and hands the path to the binary via
`--config-file='[<paths>]'`. The materialisation site is c_agent.c:7722-7735
and 7857-7868; the launcher line is c_agent.c:8025.

### 2.3 The `yunos` topic

Defined in `treedb_schema_yuneta_agent.c:275-495`. Per-yuno record. Important
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
| `configurations`  | hook — N:M against `configurations` for multi-file config sets     |

A yuno record without a matching `binaries` row or `configurations` row will
fail at create time, not at start time (c_agent.c:4466, 4487-4492).

### 2.4 Realm and per-yuno layout

Built by `build_yuno_private_domain()` (c_agent.c:7135):

```
<yuneta_root_dir>/realms/<realm_owner>/<realm_name>.<realm_role>.<realm_env>/<yuno_role>_<yuno_name>/
  ├── bin/        ← N-<role>_<name>.json (materialised configs)
  └── logs/       ← N.log files
```

The `bin/` directory is **not** the binary. It is the working dir the yuno gets
config files from. The actual binary lives in `/yuneta/repos/` (see §2.1).

---

## 3. Command inventory

Registered in the agent's command table at c_agent.c:806-900. Yuno + binary +
config commands only (admin, realm, certs and console commands omitted):

### Binaries

| Command           | At          | Effect                                                                  |
|-------------------|-------------|-------------------------------------------------------------------------|
| `install-binary`  | c_agent.c:3005 | Decode `content64`, introspect role+version, refuse if `(role, version)` already exists, write file, create treedb row. |
| `update-binary`   | c_agent.c:3234 | Same as install but **overwrites** existing `(role, version)` row and file in place. Description literally says *"WARNING: Don't use in production!"*. |
| `delete-binary`   | c_agent.c:3446 | Refuse if any yuno still references it, then `gobj_delete_node` + `rmrdir`. |
| `list-binaries`   | c_agent.c:2917 | `gobj_list_nodes("binaries", filter)`, returns all rows.                |

### Configurations

| Command         | At          | Effect                                                          |
|-----------------|-------------|-----------------------------------------------------------------|
| `update-config` | c_agent.c:3744 | Upsert a row in `configurations`. Used both for creation and updates (no separate `install-config`). |
| `delete-config` | c_agent.c:3860 | Remove a config row. Fails if yunos reference it.               |
| `view-config`   | c_agent.c:9143 | Read the **stored** zcontent for a given `(role.name, version)`. Does not return the merged effective config that the running yuno actually sees — for that, ask the yuno itself with `command-yuno service=__yuno__ command=view-config`. |

### Yunos

| Command            | At          | Effect                                                                      |
|--------------------|-------------|-----------------------------------------------------------------------------|
| `create-yuno`      | c_agent.c:4427 | Create row in `yunos`. Validates realm + binary + config existence.       |
| `delete-yuno`      | c_agent.c:4686 | Refuse if `yuno_running=true` or `tagged` (unless `force=1`); delete row. |
| `enable-yuno`      | c_agent.c:5530 | `yuno_disabled := false`.                                                 |
| `disable-yuno`     | c_agent.c:5601 | `yuno_disabled := true`. Does **not** stop a running yuno.                |
| `run-yuno`         | c_agent.c:4805 | Spawn matching `(disabled=false, running=false)` yunos. See §4.            |
| `kill-yuno`        | c_agent.c:4998 | Orderly shutdown of matching running yunos. Sends `signal2kill` (SIGQUIT by default). See §4. |
| `play-yuno`        | c_agent.c:5172 | Send `EV_PLAY_YUNO` event over the yuno's channel; flip `must_play=true`. |
| `pause-yuno`       | c_agent.c:5360 | Send `EV_PAUSE_YUNO` event over the yuno's channel; flip `must_play=false`. |
| `command-yuno`     | c_agent.c:896  | Wildcard: forward an arbitrary command to a running yuno's service.       |
| `list-yunos`       | c_agent.c:871  | All `yunos` rows with current pid + state.                                |
| `view-yuno-config` | c_agent.c:865  | The stored configs attached to a yuno (still not the effective merged one). |
| `stats-yuno`       | c_agent.c:897  | Forward a stats request to the running yuno.                              |

Permission gating is per-command via `pm_<name>` schemas (c_agent.c:849-898).

---

## 4. Lifecycle, step by step

### 4.1 State machine of a single yuno (as the agent sees it)

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

Note: the agent itself stays in `ST_IDLE` always (c_agent.c:11015). It is the
**event types** (c_agent.c:11022-11054) that drive transitions on the per-yuno
record fields (`yuno_running`, `yuno_playing`, `yuno_pid`).

### 4.2 Registration: `create-yuno`

c_agent.c:4427. Requires realm + binary `(yuno_role, role_version)` + config
`(yuno_role.yuno_name, name_version)` to already exist. Defaults missing
versions to *latest*. Writes the row with `yuno_running=false`,
`yuno_disabled=false`, `yuno_pid=0` (c_agent.c:4813-4814).

### 4.3 Start: `run-yuno`

c_agent.c:4805 → `run_yuno()` at c_agent.c:7943.

1. Select yunos: `disabled=false ∧ running=false` (c_agent.c:8362-8363).
2. Resolve binary via `get_yuno_binary()` (c_agent.c:7275): prefer active
   snapshot (`gobj_list_snaps()`, c_agent.c:7279-7284), fallback to direct
   `(role, role_version)` lookup (c_agent.c:7291-7297).
3. Materialise each `configurations.zcontent` blob to a JSON file under the
   yuno's `bin/` directory (c_agent.c:7722-7735, 7857-7868).
4. Build a launcher shell script that runs the binary with:
   ```
   <binary> --config-file='["bin/1-role_name.json", "bin/2-role_name.json", …]' "$@"
   ```
   (c_agent.c:8025).
5. `run_process2(bfbinary, argv)` (c_agent.c:7995) — `fork`/`exec` via a
   wrapper.
6. The yuno opens a channel back to the agent and emits **`EV_ON_OPEN`**
   carrying its `pid` and `watcher_pid` (handler `ac_on_open()`, c_agent.c:10233).
   Only after this handshake is `yuno_running` set to `true` (c_agent.c:10408)
   and `yuno_pid` / `watcher_pid` stored (c_agent.c:10410-10411).
7. If `must_play=true`, the agent fires `play-yuno` automatically right after
   the open (c_agent.c:10476).

**Implication**: a yuno that forks fine but never opens the channel never
becomes `running` from the agent's point of view, even if `ps` shows the
process alive. See §5 *Stale pid*.

### 4.4 Pause / Play

These are **not** process signals. They are gobj events delivered through the
yuno's open channel:

- `play-yuno` → `EV_PLAY_YUNO` → yuno does whatever "playing" means for it →
  `EV_PLAY_YUNO_ACK` → agent sets `yuno_playing=true` (c_agent.c:8156).
- `pause-yuno` → `EV_PAUSE_YUNO` → … → `EV_PAUSE_YUNO_ACK` → agent sets
  `yuno_playing=false` (c_agent.c:8192).

Most yunos use the play/paused gate to enable/disable I/O processing without
exiting. The process never stops; only its inputs are gated.

### 4.5 Stop: `kill-yuno`

c_agent.c:4998 → `kill_yuno()` at c_agent.c:8040. **Orderly shutdown**, not a
SIGKILL.

1. Read the `signal2kill` attribute (default `SIGQUIT`, c_agent.c:8045-8048).
2. `kill(yuno_pid, signal2kill)` (c_agent.c:8084).
3. If the chosen signal is `SIGKILL`, the watcher is killed too
   (c_agent.c:8104-8127).
4. **No timer-based escalation in code**. The agent trusts the yuno's signal
   handler to actually shut down. If it doesn't, the yuno stays "running"
   from the agent's record forever (see §5).
5. When the channel closes, `ac_on_close()` (c_agent.c:10501) flips
   `yuno_running=false`, `yuno_playing=false`, `yuno_pid=0`
   (c_agent.c:10563-10565).

### 4.6 Crash detection and reconciliation

**From the agent's point of view**, a crash is indistinguishable from a
clean kill: `EV_ON_CLOSE` fires, `ac_on_close()` runs, the treedb row
flips `yuno_running=false`. The agent has no `SIGCHLD` handler and does
not poll pids. It does not know "exited normally" from "segfaulted".

**The restart, however, does not depend on the agent.** Every yuno
started with `--start` runs under a per-yuno watcher process (the
first-fork survivor in `ydaemon.c`). When the yuno child dies abnormally
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
- At agent boot the timer `ac_timeout()` (c_agent.c:10902) runs:
    1. `run_util_yunos()` (c_agent.c:8389) — yunos tagged `util`,
       ignoring `disabled`.
    2. `run_enabled_yunos()` (c_agent.c:8331) — every row with
       `disabled=false ∧ running=false`.

   This reconciliation matters only for yunos that **don't** have a live
   watcher (e.g. the agent itself was killed and brought back; any yunos
   whose own watchers happened to die too).

Forensics: a crashed yuno also dumps a core at `/var/crash/core.<role>`
(sysctl + PAM limits configured by the `.deb`, see
[`ENTRY_POINT.md`](ENTRY_POINT.md#entry-point-crash-forensics) §8).
The watcher emits a `Daemon relaunched` log line on every relaunch
(`MSGSET_SYSTEM`) — grep for it to spot silent crash loops.

### 4.7 Deletion: `delete-yuno`

c_agent.c:4686. Refuses if `yuno_running=true` (c_agent.c:4721-4735). Optional
refusal on tagged yunos unless `force=1` (c_agent.c:4738-4752). Removes the
treedb row; the on-disk `bin/` directory is cleaned by treedb cascade.

---

## 5. Sharp edges (read these before touching production)

### 5.1 `update-binary` does not check for running yunos

c_agent.c:3234 overwrites the file at `/yuneta/repos/<tags>/<role>/<version>/<role>`
unconditionally (c_agent.c:3370). If a process is currently running that
binary, you are overwriting the file backing its `mmap`. This **can** corrupt
the live process. The command's own description in the table says
*"WARNING: Don't use in production!"* (c_agent.c:850).

**Use `install-binary` with a new version, or `kill-yuno` first.**

### 5.2 Stale `yuno_running=true` after a hard crash

If the yuno dies in a way that leaves the channel open (rare, but
`SIGKILL`-from-outside is one of them) `ac_on_close()` is never invoked. The
record keeps `yuno_running=true` and `yuno_pid=<old-pid>`.

On the next `run-yuno`, `ac_on_open()` checks `getpgid(_pid) >= 0`
(c_agent.c:10304). If the old pid happens to have been reused, the agent
**kills the new occupant** (c_agent.c:10318). This is the worst flavour of
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
(`c_agent.c:8104-8127`, and [`ENTRY_POINT.md §7`](ENTRY_POINT.md#entry-point-kill-yuno)).

### 5.4 `pause` ≠ `SIGSTOP`, `play` ≠ `SIGCONT`

Pause/Play are channel events. The process is never frozen at the kernel
level. If you actually want the process suspended (e.g. for `gdb attach`),
the agent gives you nothing — use the shell.

### 5.5 `update-config` does not hot-reload

c_agent.c:3744 updates the treedb blob. The launcher script is rebuilt only
at `run-yuno`. So **changes take effect on next start**. There is no
re-materialisation of the JSON files on disk for a running yuno.

### 5.6 `disable-yuno` does not stop a running yuno

c_agent.c:5601 only flips the flag. A `disabled=true` yuno that was already
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

All examples assume `ycommand` is talking to the local agent.

### 6.1 Onboard a brand-new yuno

```bash
# 1. install the binary (new role or new version)
ycommand -c 'install-binary content64=$$(my_role)'

# 2. install/upsert its configuration
ycommand -c 'update-config id=my_role.my_name version=1 zcontent=$$(my_role_my_name.json)'

# 3. create the yuno record (links binary + config to a realm)
ycommand -c 'create-yuno realm_id=<realm> yuno_role=my_role yuno_name=my_name'

# 4. enable and start
ycommand -c 'enable-yuno id=<yuno_id>'
ycommand -c 'run-yuno id=<yuno_id>'

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

# 5. start back
ycommand -c 'run-yuno'
```

For a real version bump (`1.3.1.0` → `1.3.1.1`, `7.3.4` → `7.4.0`,
…) use the upgrade flow in §6.5 instead — `update-binary` is the
wrong tool, and the `WARNING: Don't use in production!` description
in `command-yuno` help applies precisely to that misuse.

### 6.3 Change a yuno's configuration

`update-config` does not hot-reload — the yuno must be restarted.

```bash
ycommand -c 'update-config id=<role>.<name> version=<v> zcontent=$$(<file>.json)'
ycommand -c 'kill-yuno id=<yuno_id>'
ycommand -c 'run-yuno  id=<yuno_id>'

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

# 4. Force the agent to promote the newest pkey2 to primary and restart
#    deactivate-snap (no args, no active snap) is the only command that
#    triggers `restart_nodes()` (c_agent.c:8816): SIGKILL every running
#    yuno, gobj_stop/start the treedb resource (which rebuilds the
#    primary index from disk — newest pkey2 wins), then run every
#    must_play yuno. Equivalent to `yshutdown` + `restart-yuneta` but
#    without restarting the agent process itself.
ycommand -c 'deactivate-snap'

# 5. Verify
ycommand -c 'list-yunos yuno_role=<role> yuno_running=true'
# release column should now read the new version. The OLD pkey2 row
# stays in treedb for rollback (see §6.6 below).
```

#### Caveats

- **Node-wide bounce.** `restart_nodes()` SIGKILLs every running
  yuno on the node, not just the one being upgraded. Tolerable for
  kernel-yuno rotations (`auth_bff`, `emailsender`, `logcenter`);
  worth flagging before doing it during a busy window on a realm
  with many citizen yunos. A per-role rolling upgrade would require
  `cmd_run_yuno` to be made snap-aware the same way
  `get_yuno_binary` already is (TODO in `c_agent.c:4898-4900` is
  the same observation).

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

---

## 7. Code pointers (one-pager)

| What                                  | Where                                                                 |
|---------------------------------------|-----------------------------------------------------------------------|
| Agent gclass                          | `src/c_agent.c`                                                       |
| Treedb schema (yunos / binaries / configurations) | `src/treedb_schema_yuneta_agent.c`                          |
| Command table                         | `src/c_agent.c:806-900`                                               |
| Agent attributes                      | `src/c_agent.c:914-948`                                               |
| FSM (`ST_IDLE`, event types)          | `src/c_agent.c:10984-11054`                                           |
| Subscription model (SERVICE)          | `src/c_agent.c:1149-1157`                                             |
| Per-yuno on-disk path                 | `src/c_agent.c:7135` (`build_yuno_private_domain`)                    |
| Repos binary path                     | `src/c_agent.c:7088-7140`                                             |
| Binary resolution at start            | `src/c_agent.c:7275-7335` (`get_yuno_binary`)                         |
| Launcher script + `--config-file`     | `src/c_agent.c:7708-8030`                                             |
| `EV_ON_OPEN` handshake                | `src/c_agent.c:10233` (`ac_on_open`)                                  |
| `EV_ON_CLOSE` (death detection)       | `src/c_agent.c:10501` (`ac_on_close`)                                 |
| Boot-time reconciliation              | `src/c_agent.c:8331-8441, 10902` (`run_enabled_yunos`, `ac_timeout`)  |
