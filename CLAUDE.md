# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## System Prerequisites

Install system dependencies (from doc.yuneta.io):

```bash
sudo apt -y install --no-install-recommends \
  git mercurial make cmake ninja-build \
  gcc musl musl-dev musl-tools clang g++ \
  python3-dev python3-pip python3-setuptools \
  python3-tk python3-wheel python3-venv \
  libjansson-dev libpcre2-dev liburing-dev libcurl4-openssl-dev \
  libpcre3-dev zlib1g-dev libssl-dev \
  perl dos2unix tree curl \
  postgresql-server-dev-all libpq-dev \
  kconfig-frontends telnet pipx \
  patch gettext fail2ban rsync

pipx install kconfiglib
```

## Quick Start (clone to build)

```bash
# 1. Clone and enter the repo
git clone <repo-url> && cd yunetas

# 2. Initialize the git submodule for the yunetas CLI tool
git submodule update --init

# 3. Install the yunetas CLI tool
pipx install utils/python/tui_yunetas

# 4. Create .config from the default configuration
cp defconfig .config
# Or use the interactive TUI configurator:
#   menuconfig

# 5. Set up environment (must be sourced from within the repo directory)
source yunetas-env.sh

# 6. Apply compiler selection to external libs (requires sudo for update-alternatives)
./set_compiler.sh

# 7. Build external dependency libraries
cd kernel/c/linux-ext-libs
./extrae.sh          # extract sources
./install-libs.sh    # build and install into outputs_ext/
cd ../../..

# 8. Initialize build directories and generate headers
yunetas init

# 9. Build everything
yunetas build

# 10. Run tests
yunetas test
```

## Build Commands

### Install the `yunetas` CLI tool

The `yunetas` CLI lives in a git submodule. Initialize it first:

```bash
git submodule update --init
pipx install utils/python/tui_yunetas
```

### Build workflow

```bash
# Set up environment (must be sourced from within the repo directory)
source yunetas-env.sh

# FIRST TIME (or after any .config change): initialize build directories and generate headers
yunetas init

# Build and install (preferred — respects dependency order)
yunetas build

# Run tests
yunetas test

# Clean build artifacts
yunetas clean
```

`yunetas init` must be run:
- The **first time** after cloning/setting up the repo
- Any time `.config` changes (compiler, build type, enabled modules)

`yunetas init` does the following:
- Reads compiler (`CONFIG_USE_COMPILER_CLANG/GCC/MUSL`) and build type (`CONFIG_BUILD_TYPE_*`) from `.config`
- Recreates the `outputs/` directory (or `outputs_static/` for musl builds) and subdirs (`include/`, `lib/`, `bin/`, `yunos/`)
- Generates `outputs/include/yuneta_version.h` from `YUNETA_VERSION`
- Generates `outputs/include/yuneta_config.h` from `.config` (Kconfig → C `#define`)
- Creates `build/` directories under each module and runs `cmake` with the selected compiler and build type

`yunetas build` runs `make install` in each module's `build/` directory in dependency order.

### External dependency libraries

External libraries live in `kernel/c/linux-ext-libs/`. They must be extracted and built separately, **before** building yunetas:

```bash
# After selecting a compiler in .config, apply it to external libs:
# NOTE: requires sudo (runs update-alternatives and apt reinstall)
./set_compiler.sh

# Then extract, configure, and build them:
cd kernel/c/linux-ext-libs
./extrae.sh          # extract sources
./install-libs.sh    # build and install into outputs_ext/
```

For musl/static builds use `extrae-static.sh` / `install-libs-static.sh`.

### Build configuration

Configuration is controlled by `.config` (Kconfig format). This file is **not tracked by git** (it's in `.gitignore`). To create it:

```bash
cp defconfig .config   # start from default configuration (musl, debug, c_prot)
# Or use the interactive TUI configurator:
menuconfig
```

Key knobs:
- Compiler: `CONFIG_USE_COMPILER_CLANG=y`, GCC, or Musl (static). Kconfig default: Clang; `defconfig` default: Musl
- Build type: `CONFIG_BUILD_TYPE_RELWITHDEBINFO=y` (Kconfig default), Debug, Release, MinSizeRel. `defconfig` uses Debug
- TLS: `CONFIG_HAVE_OPENSSL=y` (default) or mbed-TLS
- Debug extras: `CONFIG_DEBUG_WITH_BACKTRACE`, `CONFIG_DEBUG_TRACK_MEMORY`, `CONFIG_DEBUG_PRINT_YEV_LOOP_TIMES`
- Optional modules: `CONFIG_C_POSTGRES`, `CONFIG_C_PROT`, `CONFIG_C_CONSOLE`

## Architecture

### Core Paradigm: GObject + Finite State Machines

Every component in Yuneta is a **GObject** (`gobj`) — an instance of a **GClass**. GClasses define:
- **States** and valid **events** per state
- **Attributes** (typed schema via `sdata_desc_t`, declared with `SDATA()` macros)
- **Commands** exposed to the control plane
- **Action functions** called when an event fires in a given state

GObjects are organized into a hierarchical tree forming a **Yuno** (a deployable process). Communication between gobjs happens exclusively via events carrying JSON key-value payloads (`json_t *kw`).

### Multi-Language Implementations

The primary implementation is in **C**, but parallel implementations exist:
- `kernel/js/gobj-js/` — JavaScript GObj implementation
- `kernel/js/yunetas-js7/` — Modern ES7+ JavaScript framework (uses Vite)
- `kernel/py/yunetas/` — Python SData implementation

### Layered Build Dependencies

Build order as processed by `yunetas` (each layer depends on all above it):

```
kernel/c/gobj-c       ← core GObject framework, logging, buffers, JSON config
kernel/c/libjwt       ← JWT authentication
kernel/c/ytls         ← TLS abstraction (OpenSSL or mbed-TLS)
kernel/c/yev_loop     ← async event loop (io_uring-based, non-blocking I/O)
kernel/c/timeranger2  ← append-only time-series persistence
kernel/c/root-linux   ← runtime GClasses: TCP/UDP, timers, channels, protocols, DB
kernel/c/root-esp32   ← ESP32 port of runtime GClasses
modules/c/*           ← optional: console, mqtt, postgres, test
utils/c/*             ← CLI tools (ycommand, ytests, ylist, …)
yunos/c/*             ← deployable services (mqtt_broker, yuno_agent, …)
tests/c/*             ← test suites (run via ctest)
performance/c/*       ← performance benchmarks (perf_c_tcp, perf_yev_ping_pong, …)
stress/c/*            ← stress test programs
```

### Key Source Locations

| Path | What's there |
|------|-------------|
| `kernel/c/gobj-c/src/gobj.h` | GClass definition macros, SData types, FSM API |
| `kernel/c/gobj-c/src/gobj.c` | Core ~12K-line GObject runtime |
| `kernel/c/yev_loop/` | Non-blocking event loop engine (io_uring) |
| `kernel/c/timeranger2/` | Time-series DB (append-only, key-indexed) |
| `kernel/c/linux-ext-libs/` | External dependency libraries (OpenSSL, liburing, …) |
| `kernel/c/root-linux/src/` | All runtime GClasses (`c_tcp`, `c_timer`, `c_prot_*`, `c_treedb`, …) |
| `kernel/js/` | JavaScript implementations (gobj-js, yunetas-js7) |
| `kernel/py/` | Python SData implementation |
| `yunos/c/mqtt_broker/` | MQTT v3.1.1 + v5.0 broker with persistence |
| `yunos/c/yuno_agent/` | Yuno lifecycle manager (start/stop/update) |
| `utils/c/ycommand/` | Control-plane CLI — sends commands to running yunos |
| `tests/c/` | Test suites (run via `ctest`) |
| `performance/c/` | Performance benchmarks (TCP, TLS, ping-pong) |
| `tools/cmake/` | CMake toolchain files (`musl-toolchain.cmake`, `project.cmake`) |
| `tools/packages/` | Debian packaging scripts (AMD64, ARM32, ARMhf, RISCV64) |
| `scripts/` | Utility scripts (added to `PATH` by `yunetas-env.sh`) |
| `docs/doc.yuneta.io/` | Sphinx documentation site (API docs, guides) |
| `outputs/` | Compiled libs, headers, and yuno binaries (created by `yunetas init`) |
| `outputs_static/` | Same for musl/static builds |
| `outputs_ext/` | Built external libraries |

### GClass Implementation Pattern

Every GClass follows this structure in its `.c` file:

```c
// 1. Private data struct
typedef struct { ... } priv_t;

// 2. Attribute schema (SData)
PRIVATE sdata_desc_t tattr_desc[] = {
    SDATA(DTP_STRING, "name", SDF_RD, "default", "description"),
    SDATA_END()
};

// 3. FSM states and event table
PRIVATE EV_ACTION ST_IDLE[] = { {EV_FOO, ac_foo, 0}, {0,0,0} };
PRIVATE const char *st_names[] = { ST_IDLE, NULL };
PRIVATE ev_action_t *states[] = { ST_IDLE, NULL };
PRIVATE event_type_t event_types[] = { {EV_FOO, 0}, {0,0} };

// 4. Action callbacks: json_t *ac_foo(hgobj gobj, gobj_event_t event, json_t *kw, hgobj src)

// 5. GClass descriptor
PRIVATE GOBJ_DEFINE_GCLASS(MY_CLASS);

// 6. register_my_class() populates and registers the descriptor
```

### Event Loop & Async I/O

`yev_loop` drives everything using **Linux io_uring** (not epoll). GClasses attach `yev_event_t` handles for:
- File descriptor I/O (TCP/UDP sockets, serial ports)
- Timers
- Signals

All callbacks return without blocking. There is **no threading** — scaling is achieved by running one yuno per CPU core, with inter-yuno messaging.

### Persistence (timeranger2)

- Append-only log files (`tranger2_open_topic`, `tranger2_append_record`)
- Key-value index for fast retrieval by primary key or time range
- Used by MQTT broker for session/subscription persistence, queues, and treedb

### Control Plane (ycommand)

Running yunos expose commands and stats over a local socket. Use `ycommand` to interact:
```bash
ycommand -c 'help'                        # list commands of the default yuno
ycommand -c 'stats'                       # get stats
ycommand -c 'kill-yuno id=<id>'           # stop a yuno
ycommand -c 'update-binary id=X content64=$$(X)'   # update binary (dev only)
```

## Environment Variables

| Variable | Purpose |
|----------|---------|
| `YUNETAS_BASE` | Root of this repo (auto-set by `yunetas-env.sh`) |
| `YUNETAS_OUTPUTS` | `$(dirname $YUNETAS_BASE)/outputs` — compiled artefacts land in the **parent** directory of the repo |
| `YUNETAS_YUNOS` | `$YUNETAS_OUTPUTS/yunos/` — deployed yuno binaries |

## Useful Files

| File | Purpose |
|------|---------|
| `YUNETA_VERSION` | Current version (7.0.0) — used to generate `yuneta_version.h` |
| `defconfig` | Default Kconfig — copy to `.config` for first-time setup |
| `Kconfig` | Root Kconfig definition (compiler, build type, TLS, modules, debug) |
| `TODO.md` | Tracks API renames, removals, and additions between versions |
| `CHANGELOG.md` | Release history and change log |
| `ctest-loop.sh` | Utility: runs `ctest` in a loop until first failure |
