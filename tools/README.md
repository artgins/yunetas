# tools

Shared tooling for the Yunetas project. `tools/` is packaged into the
installation `.deb` and `.rpm` (at `/yuneta/development/yunetas/tools/`), so
anything here is available on a deployed node even when the yunetas source tree
is absent — unlike `scripts/`, which is repo-only.

It holds the CMake build infrastructure (`cmake/project.cmake`, included by
every module to get a consistent compiler configuration, library definitions,
and install paths driven by the Kconfig `.config` file) and operator-facing
utilities meant to run against a live node (`agent/`, `sshd/`, `fail2ban/`,
`treedb/`).

## Files

```
tools/
├── cmake/
│   └── project.cmake           # Master build configuration (included by all modules)
├── agent/
│   └── audit-agents.sh         # Report whether both agents run the binaries that are on disk (read-only)
├── fail2ban/
│   ├── install-probe-ban-escalation.sh # Make the bans of yuneta-nginx-probe grow for an address that returns
│   └── make-fail2ban-log-readable.sh   # Let yuneta read fail2ban.log (root:adm 0640), as Debian ships it
├── treedb/
│   └── check_store_schemas.py  # Say what the schemas of a store make the SDK log at the open (read-only)
└── sshd/
    ├── audit-sshd.sh                  # Report failures and improvements in what sshd runs with (read-only)
    ├── install-sshd-flood-guard.sh    # Keep sshd reachable under a connection flood (drop-in)
    ├── install-fail2ban-sshd-jail.sh  # Make fail2ban watch sshd where the distribution does not
    ├── stop-sshd.sh                   # Stop sshd, with a timer that starts it again
    └── start-sshd.sh                  # Start sshd and cancel that timer
```

## sshd/

**The packages do not touch sshd.** How sshd behaves is the operating-system
policy of whoever runs the node: right for us to set on a node we operate,
wrong on a node of another company. So these are scripts an operator runs by
hand, on purpose — on someone else's node, only with its operator's agreement.
(7.20.0-3 shipped the flood drop-in and, in the `.rpm`, the fail2ban jail
inside the packages; they left them in the next one.)

They are not on the `PATH`. Run them as root, or through `sudo` (each one asks
for it itself):

```bash
T=/yuneta/development/yunetas/tools/sshd

$T/audit-sshd.sh                    # what is wrong or improvable; changes nothing
$T/audit-sshd.sh --all              # also what is already right

$T/install-sshd-flood-guard.sh      # LoginGraceTime 20, MaxStartups 50:30:200, PerSourceMaxStartups 10
$T/install-sshd-flood-guard.sh --check
$T/install-sshd-flood-guard.sh --remove

$T/install-fail2ban-sshd-jail.sh    # [sshd] enabled, backend = systemd
$T/install-fail2ban-sshd-jail.sh --check
$T/install-fail2ban-sshd-jail.sh --remove

$T/stop-sshd.sh                     # stop now, start again in 60 minutes
$T/stop-sshd.sh 15                  # ... in 15 minutes
$T/stop-sshd.sh --no-restart        # arm nothing: only with the provider's console at hand
$T/start-sshd.sh                    # from the provider's console: start it, cancel the timer
```

What each one guarantees:

| Script | Guarantee |
|---|---|
| `audit-sshd.sh` | Reads `sshd -T` — what sshd runs with, not what one file says. Levels FAIL / WARN / INFO; exit status 2 / 1 / 0. Covers login methods, root, weak algorithms and host keys, file permissions, the flood settings, the last hour of the journal and fail2ban. |
| `install-sshd-flood-guard.sh` | `sshd -t` before and after; if sshd rejects the new file the previous state comes back. Reloads, never restarts. Confirms with `sshd -T` that the values are the ones in effect, and warns when password login is on — it never turns it off. |
| `install-fail2ban-sshd-jail.sh` | Does nothing where the distribution already runs an `sshd` jail (Debian). `fail2ban-client -t` before any reload: a jail fail2ban cannot configure takes the whole server down. |
| `stop-sshd.sh` | Refuses when `sshd -t` fails. Arms `yuneta-sshd-autostart.timer` (a transient systemd unit) BEFORE stopping; if it cannot be armed, nothing is stopped. Open sessions survive (`KillMode=process`; it warns when the unit says otherwise). |
| `start-sshd.sh` | Refuses when `sshd -t` fails, showing why. Starts sshd (and its socket unit, if enabled), checks it is up, cancels the timer. |

None of it stops brute force — password login off does, and `audit-sshd.sh`
fails a node where it is on. The flood guard keeps sshd reachable; the fix for
port 22 open to the world is a source allowlist, and in the end a sealed node
with no inbound SSH (`yunos/c/yuno_agent/NODE_SEALING.md`).

## agent/audit-agents.sh

Says whether `yuneta_agent` and `yuneta_agent22` are running **the binaries
that are on disk**. Read-only: it changes nothing and proposes the restart.

```sh
tools/agent/audit-agents.sh          # what is wrong
tools/agent/audit-agents.sh --all    # also what is right
```

Exit status is 2 if an agent is not running, 1 if one is stale, 0 otherwise,
so it drops into a cron or a check script unchanged.

**Why it exists.** Every node runs both agents as a deliberate redundancy —
each can upgrade the other, and the spare is the only way into a node whose
main agent is broken, which is why they are never stopped or upgraded at once.
The consequence is that a spare left behind on an old binary is invisible until
the day it is needed: it happened for five days on four nodes, running the
version-comparison bug 7.12.0 fixed. `install.sh` restarts the spare after a
package upgrade, so the **runtime** nodes are covered; a node that **builds
from source** has no `install.sh` — `yunetas build` replaces `/yuneta/agent/*`
and both agents are restarted by hand.

**Why one line is enough.** It reads the PROCESS and not the file:

```sh
readlink /proc/<pid>/exe    # ends in " (deleted)" when the binary was replaced
```

Linux refuses to write into a binary that is being executed (`ETXTBSY`), so
`cp` over a running agent fails outright, and `install` / `mv` / a package
succeed only by **unlinking first** — which leaves the running process holding
an inode with no name, and the kernel marks it. Every replacement that can
happen while the agent runs is one the kernel marks, so there is no in-place
case to miss.

It matches by process NAME (`pgrep -x`) and never by command line: a `pgrep -f`
pattern matches the shell running the script too, because its own command line
holds the pattern. Several pids per agent are expected — a watcher plus the
process it watches.

## The deploy tools (in the `yunetas` CLI)

`sync_binaries.py`, `sync_configs.py` and `set_start_priorities.py` lived in
`tools/agent/` until SDK 7.8.5. They ship inside the `yunetas` CLI since then
(`yunetas.agent_tools`, on PyPI), so the CLI and the tools it drives are
always the same version. The forwarding shims that stayed here were removed
after SDK 7.27.4. Run the tools through the CLI:

```bash
yunetas sync-binaries [-n|-a|--role R|--no-restart|...]   # built yunos vs the agent
yunetas sync-configs  [-n|-a|-r|--host H|...]              # batches/<host>/ configs vs the agent
yunetas set-start-priorities [-n|-a|--rule P=N|...]        # launch tier of each yuno, by role
yunetas restart-agents [-n|-y]      # agents that run a replaced binary: main, then spare
```

Every one takes `--node <name>` to act on a registered node (except
`restart-agents`, which acts on the node it runs on). The reference is on
doc.yuneta.io: [The `yunetas` CLI](https://doc.yuneta.io/yunetas-cli),
[sync-binaries](https://doc.yuneta.io/sync-binaries),
[sync-configs](https://doc.yuneta.io/sync-configs),
[set-start-priorities](https://doc.yuneta.io/set-start-priorities).

## project.cmake

Central configuration file included by every `CMakeLists.txt` in the project. It performs the following:

### 1. C Standard and CMake Setup

- Requires CMake 3.11+
- Sets C99 with strict enforcement (`CMAKE_C_STANDARD_REQUIRED ON`)
- Includes `CheckIncludeFiles` and `CheckSymbolExists` modules

### 2. Kconfig .config Loading

Reads `${YUNETAS_BASE}/.config` (generated by `menuconfig`) and converts every `KEY=VALUE` pair into a CMake variable:

| Kconfig value | CMake value |
|---------------|-------------|
| `y`           | `1`         |
| `n`           | `0`         |
| `"string"`    | `string`    |

Fails fatally if `YUNETAS_BASE` is unset or `.config` is missing.

### 3. `add_yuno_executable(name ...)`

Custom CMake function that wraps `add_executable()`.

Used by all yuno, test, performance, and stress executables.

### 4. Output Directories

Selects the install prefix based on the build type:

| Build | Install prefix | Description |
|-------|---------------|-------------|
| Default | `${YUNETAS_BASE}/outputs` | Clang/GCC builds |

Install subdirectories:

| Variable | Path | Contents |
|----------|------|----------|
| `INC_DEST_DIR` | `${prefix}/include` | Public headers |
| `LIB_DEST_DIR` | `${prefix}/lib` | Static libraries |
| `BIN_DEST_DIR` | `${prefix}/bin` | CLI tool binaries |
| `YUNOS_DEST_DIR` | `${prefix}/yunos` | Yuno service binaries |

### 5. Include and Link Paths

For non-ESP32 builds:

- Defines `_GNU_SOURCE`
- Adds external library paths (`outputs_ext/`)
- Adds the Yunetas output paths (`outputs/include`, `outputs/lib`)

### 6. Compiler Flags

Applied globally to all targets via `add_compile_options()`:

```
-Wall -Wextra
-Wno-type-limits -Wno-sign-compare -Wno-unused-parameter
-Wmissing-prototypes -Wstrict-prototypes
-funsigned-char
```

Default build type: `RelWithDebInfo` (if not specified).

### 7. Library Variables

Pre-defined CMake variables for linking. Modules pick the ones they need in their `target_link_libraries()` calls.

**Core kernel libraries** (`YUNETAS_KERNEL_LIBS` — always linked by yunos):

| Library | Description |
|---------|-------------|
| `libyunetas-core-linux.a` | Root-linux runtime GClasses (TCP, timers, protocols) |
| `libargp-standalone.a` | Command-line argument parsing |
| `libtimeranger2.a` | Time-series persistence engine |
| `libyev_loop.a` | Async event loop (io_uring) |
| `libytls.a` | TLS abstraction (OpenSSL or mbed-TLS) |
| `libyunetas-gobj.a` | Core GObject framework |

**External libraries** (`YUNETAS_EXTERNAL_LIBS`):

| Library | Description |
|---------|-------------|
| `libjansson.a` | JSON parsing |
| `liburing.a` | Linux io_uring async I/O |

**PCRE** (`YUNETAS_PCRE_LIBS`):

| Library | Description |
|---------|-------------|
| `libpcre2-8.a` | Regular expressions (PCRE2) |

**Conditional libraries** (enabled by `.config` options):

| Config option | Variable | Libraries |
|---------------|----------|-----------|
| `CONFIG_HAVE_OPENSSL` | `OPENSSL_LIBS` | `libjwt-y.a`, `libssl.a`, `libcrypto.a`, `pthread`, `dl` |
| `CONFIG_HAVE_MBEDTLS` | `MBEDTLS_LIBS` | `libjwt-y.a`, `libmbedtls.a`, `libmbedx509.a`, `libmbedcrypto.a` |
| `CONFIG_DEBUG_WITH_BACKTRACE` | `DEBUG_LIBS` | `libbacktrace.a` |
| `CONFIG_MODULE_CONSOLE` | `CONSOLE_LIBS` | `libyunetas-module-console.a` |
| `CONFIG_MODULE_MQTT` | `MQTT_LIBS` | `libyunetas-module-mqtt.a` |
| `CONFIG_MODULE_POSTGRES` | `POSTGRES_LIBS` | `libyunetas-module-postgres.a` |
| `CONFIG_MODULE_TEST` | `TEST_LIBS` | `libyunetas-module-test.a` |

## Usage Pattern

Every `CMakeLists.txt` in the project follows the same boilerplate to include `project.cmake`:

```cmake
cmake_minimum_required(VERSION 3.11)
project(my_module C)

# Resolve YUNETAS_BASE from environment or default paths
if(DEFINED ENV{YUNETAS_BASE} AND IS_DIRECTORY "$ENV{YUNETAS_BASE}")
  set(YUNETAS_BASE "$ENV{YUNETAS_BASE}")
elseif(IS_DIRECTORY "/yuneta/development/yunetas")
  set(YUNETAS_BASE "/yuneta/development/yunetas")
elseif(IS_DIRECTORY "/yuneta/development")
  set(YUNETAS_BASE "/yuneta/development")
else()
  message(FATAL_ERROR "YUNETAS_BASE not found.")
endif()

# Include the master build configuration
include("${YUNETAS_BASE}/tools/cmake/project.cmake")
```

**For libraries** (kernel, modules):

```cmake
add_library(${PROJECT_NAME} ${SRCS} ${HDRS})

install(FILES ${HDRS} DESTINATION ${INC_DEST_DIR})
install(TARGETS ${PROJECT_NAME} DESTINATION ${LIB_DEST_DIR})
```

**For executables** (yunos, utils, tests):

```cmake
add_yuno_executable(${PROJECT_NAME} ${YUNO_SRCS} ${YUNO_HDRS})

target_link_libraries(${PROJECT_NAME}
    ${MODULE_MQTT}                # conditional module libs as needed
    ${YUNETAS_KERNEL_LIBS}      # always
    ${YUNETAS_EXTERNAL_LIBS}    # always
    ${YUNETAS_PCRE_LIBS}        # always
    ${OPENSSL_LIBS}             # conditional on CONFIG_HAVE_OPENSSL
    ${MBEDTLS_LIBS}             # conditional on CONFIG_HAVE_MBEDTLS
    ${DEBUG_LIBS}               # conditional on CONFIG_DEBUG_WITH_BACKTRACE
)

install(TARGETS ${PROJECT_NAME} DESTINATION ${YUNOS_DEST_DIR})
```

## ESP32 Support

`project.cmake` detects the `ESP_PLATFORM` variable (set by ESP-IDF). When building for ESP32, it skips the `_GNU_SOURCE` definition, external library paths, and linker flags that are Linux-specific. The ESP32 port uses its own component-based build system from `kernel/c/root-esp32/`.

## Manual CMake Invocation

While `yunetas init` and `yunetas build` handle this automatically, modules can be built manually:

```bash
# Standard build (uses CC from environment, e.g. clang or gcc)
cmake -DCMAKE_BUILD_TYPE=RelWithDebInfo ..

# Debug build
cmake -DCMAKE_BUILD_TYPE=Debug ..

# Release build
cmake -DCMAKE_BUILD_TYPE=Release ..
```

## fail2ban/

The web jails (`yuneta-nginx-probe`, `nginx-botsearch`) ship in the packages,
disabled. These scripts change the fail2ban **server** or the system log, which
is the policy of whoever runs the node, so like `sshd/` they are run by hand:

```bash
T=/yuneta/development/yunetas/tools/fail2ban

$T/install-probe-ban-escalation.sh          # bans of yuneta-nginx-probe: 1d, 2d, 4d, then 1w
$T/install-probe-ban-escalation.sh --check
$T/install-probe-ban-escalation.sh --remove

$T/make-fail2ban-log-readable.sh            # root:adm 0640, yuneta in adm (a no-op on Debian)
$T/make-fail2ban-log-readable.sh --check
```

**Why escalation and not the stock `recidive` jail.** The scanners return: on
one node in September 2026 an address was banned on the 4th and again on the
21st, another on the 19th and on the 21st. `recidive` reads `fail2ban.log`,
which rotates weekly, so a return after a rotation is a first offence to it --
and both of those returns cross one. `bantime.increment` counts earlier bans in
fail2ban's database, which does not rotate, once `dbpurgeage` (stock: one day)
is raised to 30 days; the script does both. The cap is a week because the jail
catches us too: a hardening check run with `curl` asks for `/.env` like a
scanner.

**Why the log must be readable.** `webstats` reads `fail2ban.log` to say, next
to each top offender of its mail, whether it was banned -- a jail that bans
nobody looks as healthy as one that works. Debian ships the log `root:adm
0640` with `yuneta` in `adm`; RHEL/Rocky ships it `root:root 0600`. After the
script adds `yuneta` to `adm`, restart `yuneta_agent` and then the `webstats`
yuno: a process keeps the groups it started with.

## treedb/check_store_schemas.py

Says what the treedb and msg2db schemas persisted in a store will make the SDK
log when a yuno opens them. **Run it on every store of a node before upgrading
its SDK**: since 7.27.0 a topic already on disk that breaks a rule of a new
topic opens as it was, with an ERROR at every open, and a few faults leave a
topic out or refuse it. Read-only; on a clean store it prints only the count
of what it read (on stderr).

```bash
T=/yuneta/development/yunetas/tools/treedb

python3 $T/check_store_schemas.py                       # every store under /yuneta/store
python3 $T/check_store_schemas.py /yuneta/store/myappdb     # one store

# a node still on an older package: stream it over ssh
ssh <node> python3 - /yuneta/store < tools/treedb/check_store_schemas.py
```

One line per fault, with the level of what the SDK does and the message it
logs:

```
/yuneta/store/x/.../treedb_x.treedb_schema.json: ERROR items.id: a pkey2 cannot be the pkey 'id' -- "Topic on disk opened as it was, with bad pkey2s"
/yuneta/store/x/.../treedb_x.treedb_schema.json: CRITICAL depts.users: a column both 'hook' and 'fkey' -- "A column cannot be both 'hook' and 'fkey'"
```

| Level | What the SDK does at the open |
|---|---|
| `NO-OPEN` | The treedb is not opened (*"No topics found"*). |
| `LEFT-OUT` | The topic is not opened: its records stay on disk, unreachable. |
| `CRITICAL` | *"Cannot create topic of the schema"*: a yuno whose `on_critical_error` exits, exits. |
| `ERROR` | The topic opens as it was, and the ERROR is logged at every open. |
| `PROJECTION` | The `__system__` projection (`C_TREEDB`, on the master) refuses the node. |
| `CUT` | The `__assets__` hook of a `file` column is cut to `NAME_MAX`, with no log. |
| `LIST` | The id of a list of an index is cut to `NAME_MAX` (ESTIMATED: it holds the name of the gobj that owns the tranger; `--list-ids-gobj` names it). |

Exit status 0 when nothing is found, 1 when a fault is, 2 when a file cannot be
read. It does not check the fields of a column against the column meta-schema
(*"a column is not valid (see the log)"*): that one is derived from the
`__system__` schema at run time.

