# tools

Shared tooling for the Yunetas project. `tools/` is packaged into the
installation `.deb`, so anything here is available on a deployed node even when
the yunetas source tree is absent — unlike `scripts/`, which is repo-only.

It holds the CMake build infrastructure (`cmake/project.cmake`, included by
every module to get a consistent compiler configuration, library definitions,
and install paths driven by the Kconfig `.config` file) and operator-facing
utilities meant to run against a live node (`agent/`).

## Files

```
tools/
├── cmake/
│   └── project.cmake           # Master build configuration (included by all modules)
└── agent/
    ├── sync_binaries.py        # Compare built yunos vs the agent's installed set, push updates
    └── sync_configs.py         # Compare a directory's configs vs the agent's installed set, push updates
```

## agent/sync_binaries.py

Operator utility that reconciles the freshly built yuno binaries with what the
local `yuneta_agent` already has installed, and (with confirmation) pushes the
differences via `install-binary` / `update-binary`.

It **drives from the agent's installed binaries**, not from `outputs/yunos`:
only roles the agent already manages on this node are candidates, so it never
proposes installing a role this node doesn't run.

- Agent side: `ycommand -c '*list-binaries'` (the leading `*` makes ycommand
  emit raw JSON instead of the table).
- Local side: each installed id is looked up in `$YUNETAS_BASE/outputs/yunos/`
  (the exact directory the agent's `$$(<role>)` macro reads from on upload) and
  queried with `--print-role` for its version.

Classification per installed binary:

| Status | Condition | Action |
|--------|-----------|--------|
| `BUMP` | local version > agent version | `install-binary` |
| `DOWNGRADE` | local version < agent version | `install-binary` (flagged red) |
| `REBUILD` | same version, size changed | `update-binary` |
| `UP-TO-DATE` | same version, same size | skipped |
| `NO-BUILD` | agent has it, no build in `outputs/yunos` | skipped (informational) |

It shows the candidate table, asks what to apply (all / one-by-one / quit),
then runs `install-binary` / `update-binary id=<role> content64=$$(<role>)` for
each chosen role.

For a same-version `REBUILD` the agent overwrites the slot the running yuno is
executing from, so `update-binary` would hit `text-file-busy`. Once both
confirmation gates are cleared the script runs the documented per-role hot-patch
cycle itself, scoped by `yuno_role` (never node-wide): `kill-yuno` (only if
running, orderly SIGQUIT) → poll `*list-yunos` until the process exits →
`update-binary` → `run-yuno play=0` (if it was running) → `play-yuno` (if it was
playing). Prior run/play state is read from `*list-yunos` and restored per role;
`--no-restart` keeps the old print-only behaviour. The version-bump path
(`find-new-yunos create=1` + `deactivate-snap`) is still **not** automated — that
is a node-wide bounce, printed as a reminder. See
`yunos/c/yuno_agent/YUNO_LIFECYCLE.md` §6.5/§6.6.

```bash
tools/agent/sync_binaries.py            # interactive: show table, ask, apply
tools/agent/sync_binaries.py -n         # dry-run: print the commands, run nothing
tools/agent/sync_binaries.py -a         # apply every candidate without asking
tools/agent/sync_binaries.py --no-restart   # REBUILD: update-binary only, no kill/restart
tools/agent/sync_binaries.py -u ws://127.0.0.1:1991   # target a specific agent url
tools/agent/sync_binaries.py --yunos-dir /path/to/yunos   # override the build dir
```

## agent/sync_configs.py

Config-side sibling of `sync_binaries.py`. Reconciles the yuno **configs in a
directory** with what the local `yuneta_agent` already has installed, and (with
confirmation) pushes the differences via `create-config` (alias `install-config`)
/ `update-config`.

Configs are **not centralized** like binaries — they live under each yuno's
`batches/<host>/` directory — so this script **drives from the current
directory**: you `cd` into the batches directory and run it.

- A config's `id` is the **filename without `.json`** (`auth_bff.1801.json` → id
  `auth_bff.1801`), matching the agent's config ids.
- Its `version` is read from the `__version__` field **inside the file** (a file
  without it is skipped); `_*.json` batch/deploy helpers are skipped too.
- Agent side: `ycommand -c '*list-configs'`. Content upload uses the same
  `content64=$$(<path>)` macro (absolute path, resolves regardless of cwd).

Classification per local config:

| Status | Condition | Action |
|--------|-----------|--------|
| `NEW` | id not in the agent | `create-config` |
| `BUMP` | local version > agent version | `create-config` |
| `UPDATE` | same version, content changed | `update-config` |
| `UP-TO-DATE` | same version, identical content | skipped |
| `DOWNGRADE` | local version < agent version | reported only, **not** pushed |
| `agent-only` | agent has a config absent in the directory | skipped (informational) |

A `DOWNGRADE` is never offered for install (seeding a stale version would break
the version logic). `create-config` is the new-version install (it refuses to
overwrite an existing `(id, version)`); `update-config` overwrites a same-version
record. A yuno reads its config only at (re)start, so after pushing the chosen
configs the script restarts the using yunos itself — ids come from the agent
record's `yunos` field — scoped by yuno `id` (never node-wide): `kill-yuno`
(only if running, orderly SIGQUIT) → poll `*list-yunos` until it exits →
`run-yuno play=0` → `play-yuno` (if it was playing). A stopped yuno is left
stopped; NEW configs (no agent record) are printed as a reminder; `--no-restart`
keeps the old print-only behaviour. Config pushes never hit `text-file-busy`, so
there is no pre-kill.

```bash
cd yunos/c/auth_bff/batches/localhost          # stand in the configs directory
tools/agent/sync_configs.py                    # interactive: show table, ask, apply
tools/agent/sync_configs.py -n                 # dry-run: print the commands, run nothing
tools/agent/sync_configs.py -a                 # apply every candidate without asking
tools/agent/sync_configs.py --show-uptodate    # also list in-sync and agent-only configs
tools/agent/sync_configs.py --no-restart       # push only, don't restart the using yunos
tools/agent/sync_configs.py -u ws://127.0.0.1:1991   # target a specific agent url
```

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
