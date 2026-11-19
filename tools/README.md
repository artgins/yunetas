# tools

The shared tools of the Yunetas project. The installation `.deb` and `.rpm`
contain `tools/` (at `/yuneta/development/yunetas/tools/`). Thus everything here
is available on a deployed node, also when the yunetas source tree is not there.
`scripts/` is different: it is only in the repository.

`tools/` holds the CMake build infrastructure (`cmake/project.cmake`) and the
utilities that an operator runs against a live node (`agent/`, `sshd/`,
`fail2ban/`, `treedb/`). Every module includes `cmake/project.cmake` to get the
same compiler configuration, library definitions, and install paths, from the
Kconfig `.config` file. Each utility has a section below, with its commands
first.

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

The packages do not touch sshd. How sshd behaves is the operating-system
policy of the operator of the node. It is correct for us to set it on a node
that we operate, and wrong on a node of another company. Thus these are scripts
that an operator runs by hand, on purpose. On the node of another company, run
them only with the agreement of its operator. (7.20.0-3 shipped the flood
drop-in and, in the `.rpm`, the fail2ban jail inside the packages. The next
package removed them.)

They are not on the `PATH`. Run them as root, or through `sudo` (each script
asks for it itself):

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
| `audit-sshd.sh` | Reads `sshd -T`: what sshd runs with, not what one file says. Levels FAIL / WARN / INFO, with exit status 2 / 1 / 0. Covers login methods, root, weak algorithms and host keys, file permissions, the flood settings, the last hour of the journal, and fail2ban. |
| `install-sshd-flood-guard.sh` | `sshd -t` before and after. If sshd rejects the new file, the script restores the previous state. Reloads, never restarts. Makes sure with `sshd -T` that the values are the ones in effect. Warns when password login is on, but never turns it off. |
| `install-fail2ban-sshd-jail.sh` | Does nothing where the distribution already runs an `sshd` jail (Debian). `fail2ban-client -t` before any reload, because a jail that fail2ban cannot configure stops the whole server. |
| `stop-sshd.sh` | Refuses when `sshd -t` fails. Arms `yuneta-sshd-autostart.timer` (a transient systemd unit) BEFORE it stops sshd. If it cannot arm the timer, it stops nothing. Open sessions survive (`KillMode=process`). It warns when the unit says otherwise. |
| `start-sshd.sh` | Refuses when `sshd -t` fails, and shows why. Starts sshd (and its socket unit, if enabled), makes sure that it is up, and cancels the timer. |

None of these scripts stops brute force. Password login off stops it, and
`audit-sshd.sh` fails a node where password login is on. The flood guard keeps
sshd reachable. The fix for port 22 open to the world is a source allowlist,
and at the end a sealed node with no inbound SSH
(`yunos/c/yuno_agent/NODE_SEALING.md`).

## agent/audit-agents.sh

Tells whether `yuneta_agent` and `yuneta_agent22` run the binaries that are on
disk. It is read-only: it changes nothing, and it proposes the restart.

```sh
tools/agent/audit-agents.sh          # what is wrong
tools/agent/audit-agents.sh --all    # also what is right
```

The exit status is 2 if an agent is not running, 1 if one is stale, and 0
otherwise. Thus you can put it in a cron job or a monitoring script with no
change.

Why it exists: every node runs both agents as a deliberate redundancy. Each
one can upgrade the other. The spare is the only way into a node whose main
agent is broken. That is why nobody stops or upgrades the two at the same time.
As a result, a spare left behind on an old binary is invisible until the day
that you need it. This happened for five days on four nodes, which ran the
version-comparison bug that 7.12.0 fixed. `install.sh` restarts the spare after
a package upgrade, so the runtime nodes are covered. A node that builds from
source has no `install.sh`. On that node, `yunetas build` replaces
`/yuneta/agent/*`, and you restart both agents by hand.

Why one line is enough: the script reads the PROCESS and not the file.

```sh
readlink /proc/<pid>/exe    # ends in " (deleted)" when the binary was replaced
```

Linux refuses to write into a binary that runs (`ETXTBSY`). Thus `cp` over a
running agent fails. `install` / `mv` / a package succeed only because they
unlink the file first. The running process then holds an inode with no name,
and the kernel marks it. The kernel marks every replacement that can occur
while the agent runs, so the script cannot miss an in-place case.

It matches by process NAME (`pgrep -x`) and never by command line. A `pgrep -f`
pattern also matches the shell that runs the script, because the command line
of that shell holds the pattern. Several pids for each agent are normal: a
watcher and the process that it watches.

## The deploy tools (in the `yunetas` CLI)

`sync_binaries.py`, `sync_configs.py` and `set_start_priorities.py` were in
`tools/agent/` until SDK 7.8.5. Since then, they ship inside the `yunetas` CLI
(`yunetas.agent_tools`, on PyPI). Thus the CLI and the tools that it drives
always have the same version. The forwarding shims that stayed here were
removed after SDK 7.27.4. Run the tools through the CLI:

```bash
yunetas sync-binaries [-n|-a|--role R|--no-restart|...]   # built yunos vs the agent
yunetas sync-configs  [-n|-a|-r|--host H|...]              # batches/<host>/ configs vs the agent
yunetas set-start-priorities [-n|-a|--rule P=N|...]        # launch tier of each yuno, by role
yunetas restart-agents [-n|-y]      # agents that run a replaced binary: main, then spare
```

Each one takes `--node <name>` to act on a registered node, but
`restart-agents` acts on the node that it runs on. The reference is on
doc.yuneta.io: [The `yunetas` CLI](https://doc.yuneta.io/yunetas-cli),
[sync-binaries](https://doc.yuneta.io/sync-binaries),
[sync-configs](https://doc.yuneta.io/sync-configs),
[set-start-priorities](https://doc.yuneta.io/set-start-priorities).

## project.cmake

The central configuration file that every `CMakeLists.txt` in the project includes. It does these steps:

### 1. C Standard and CMake Setup

- Requires CMake 3.11+
- Sets C99 with strict enforcement (`CMAKE_C_STANDARD_REQUIRED ON`)
- Includes `CheckIncludeFiles` and `CheckSymbolExists` modules

### 2. Kconfig .config Loading

Reads `${YUNETAS_BASE}/.config` (made by `menuconfig`) and changes every `KEY=VALUE` pair into a CMake variable:

| Kconfig value | CMake value |
|---------------|-------------|
| `y`           | `1`         |
| `n`           | `0`         |
| `"string"`    | `string`    |

If `YUNETAS_BASE` is not set or `.config` is missing, it stops with a fatal error.

### 3. `add_yuno_executable(name ...)`

A custom CMake function that wraps `add_executable()`. All yuno, test,
performance, and stress executables use it.

### 4. Output Directories

Selects the install prefix from the build type:

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

`add_compile_options()` applies these flags to all targets:

```
-Wall -Wextra
-Wno-type-limits -Wno-sign-compare -Wno-unused-parameter
-Wmissing-prototypes -Wstrict-prototypes
-funsigned-char
```

Default build type: `RelWithDebInfo` (if no build type is given).

### 7. Library Variables

Predefined CMake variables for linking. Each module picks the ones that it needs in its `target_link_libraries()` calls.

Core kernel libraries (`YUNETAS_KERNEL_LIBS`, always linked by yunos):

| Library | Description |
|---------|-------------|
| `libyunetas-core-linux.a` | Root-linux runtime GClasses (TCP, timers, protocols) |
| `libargp-standalone.a` | Command-line argument parsing |
| `libtimeranger2.a` | Time-series persistence engine |
| `libyev_loop.a` | Async event loop (io_uring) |
| `libytls.a` | TLS abstraction (OpenSSL or mbed-TLS) |
| `libyunetas-gobj.a` | Core GObject framework |

External libraries (`YUNETAS_EXTERNAL_LIBS`):

| Library | Description |
|---------|-------------|
| `libjansson.a` | JSON parsing |
| `liburing.a` | Linux io_uring async I/O |

PCRE (`YUNETAS_PCRE_LIBS`):

| Library | Description |
|---------|-------------|
| `libpcre2-8.a` | Regular expressions (PCRE2) |

Conditional libraries (enabled by `.config` options):

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

Every `CMakeLists.txt` in the project uses the same boilerplate to include `project.cmake`:

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

For libraries (kernel, modules):

```cmake
add_library(${PROJECT_NAME} ${SRCS} ${HDRS})

install(FILES ${HDRS} DESTINATION ${INC_DEST_DIR})
install(TARGETS ${PROJECT_NAME} DESTINATION ${LIB_DEST_DIR})
```

For executables (yunos, utils, tests):

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

`project.cmake` detects the `ESP_PLATFORM` variable (set by ESP-IDF). For an ESP32 build, it skips the `_GNU_SOURCE` definition, the external library paths, and the Linux-specific linker flags. The ESP32 port uses its own component-based build system from `kernel/c/root-esp32/`.

## Manual CMake Invocation

`yunetas init` and `yunetas build` do this automatically. You can also build a module manually:

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
disabled. These scripts change the fail2ban server or the system log. That is
the policy of the operator of the node, so you run them by hand, like the
`sshd/` scripts:

```bash
T=/yuneta/development/yunetas/tools/fail2ban

$T/install-probe-ban-escalation.sh          # bans of yuneta-nginx-probe: 1d, 2d, 4d, then 1w
$T/install-probe-ban-escalation.sh --check
$T/install-probe-ban-escalation.sh --remove

$T/make-fail2ban-log-readable.sh            # root:adm 0640, yuneta in adm (a no-op on Debian)
$T/make-fail2ban-log-readable.sh --check
```

Why escalation and not the stock `recidive` jail: the scanners return. On one
node in September 2026, one address was banned on the 4th and again on the
21st, and another on the 19th and on the 21st. `recidive` reads
`fail2ban.log`, which rotates each week. Thus a return after a rotation is a
first offense to it, and both of those returns cross a rotation.
`bantime.increment` counts earlier bans in the database of fail2ban, which
does not rotate, after you raise `dbpurgeage` (stock: one day) to 30 days. The
script does both. The cap is one week because the jail also catches us: a
hardening test that runs `curl` asks for `/.env` like a scanner.

Why the log must be readable: `webstats` reads `fail2ban.log`. Next to each
top offender in its mail, it tells whether that address was banned. A jail that
bans nobody looks as healthy as a jail that works. Debian ships the log
`root:adm 0640`, with `yuneta` in `adm`. RHEL/Rocky ships it `root:root 0600`.
After the script adds `yuneta` to `adm`, restart `yuneta_agent` and then the
`webstats` yuno, because a process keeps the groups that it started with.

## treedb/check_store_schemas.py

Tells what the SDK will log about the treedb and msg2db schemas of a store when
a yuno opens them. Run it on every store of a node before you upgrade its SDK.
Since 7.27.0, a topic already on disk that breaks a rule of a new topic opens
as it was, with an ERROR at every open. A few faults leave a topic out, or
refuse it. The script is read-only. On a clean store, it prints only the count
of what it read (on stderr).

```bash
T=/yuneta/development/yunetas/tools/treedb

python3 $T/check_store_schemas.py                       # every store under /yuneta/store
python3 $T/check_store_schemas.py /yuneta/store/myappdb     # one store

# a node still on an older package: stream it over ssh
ssh <node> python3 - /yuneta/store < tools/treedb/check_store_schemas.py
```

One line for each fault, with the level of what the SDK does and the message
that it logs:

```
/yuneta/store/x/.../treedb_x.treedb_schema.json: ERROR items.id: a pkey2 cannot be the pkey 'id' -- "Topic on disk opened as it was, with bad pkey2s"
/yuneta/store/x/.../treedb_x.treedb_schema.json: CRITICAL depts.users: a column both 'hook' and 'fkey' -- "A column cannot be both 'hook' and 'fkey'"
```

| Level | What the SDK does at the open |
|---|---|
| `NO-OPEN` | The treedb is not opened (*"No topics found"*). |
| `LEFT-OUT` | The topic is not opened: its records stay on disk, unreachable. |
| `CRITICAL` | *"Cannot create topic of the schema"*: if the `on_critical_error` of the yuno exits, the yuno exits. |
| `ERROR` | The topic opens as it was, and the ERROR is logged at every open. |
| `PROJECTION` | The `__system__` projection (`C_TREEDB`, on the master) refuses the node. |
| `CUT` | The `__assets__` hook of a `file` column is cut to `NAME_MAX`, with no log. |
| `LIST` | The id of a list of an index is cut to `NAME_MAX` (ESTIMATED: it holds the name of the gobj that owns the tranger. `--list-ids-gobj` names it). |

The exit status is 0 when it finds nothing, 1 when it finds a fault, and 2 when
it cannot read a file. It does not compare the fields of a column with the
column meta-schema (*"a column is not valid (see the log)"*). That meta-schema
is derived from the `__system__` schema at run time.

