# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Build Commands

```bash
# Set up environment (must be sourced from within the repo directory)
source yunetas-env.sh

# Build and install (preferred — respects dependency order)
yunetas build

# Run tests
yunetas test

# Or manually with CMake + Ninja:
mkdir build && cd build
cmake -GNinja -DCMAKE_BUILD_TYPE=Debug ..
ninja && ninja install
ctest         # run all tests
ctest -V      # verbose output
ctest -R kw   # run a single test suite by name
```

Build variants:
```bash
cmake -GNinja -DCMAKE_BUILD_TYPE=Release ..               # optimized
cmake -GNinja -DCMAKE_BUILD_TYPE=RelWithDebInfo ..         # profiling
cmake -GNinja -DCMAKE_BUILD_TYPE=Debug -DENABLE_TESTS=OFF ..  # skip tests
cmake -GNinja -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_TOOLCHAIN_FILE=tools/cmake/musl-toolchain.cmake ..  # static binary
```

Build configuration is controlled by `.config` (Kconfig format). Key knobs:
- Compiler: `CONFIG_USE_COMPILER_CLANG=y` (default), GCC, or Musl
- TLS: `CONFIG_HAVE_OPENSSL=y` (default) or mbed-TLS
- Debug extras: `CONFIG_DEBUG_WITH_BACKTRACE`, `CONFIG_DEBUG_TRACK_MEMORY`
- Optional modules: `CONFIG_C_POSTGRES`, `CONFIG_C_PROT`, `CONFIG_C_CONSOLE`

## Architecture

### Core Paradigm: GObject + Finite State Machines

Every component in Yuneta is a **GObject** (`gobj`) — an instance of a **GClass**. GClasses define:
- **States** and valid **events** per state
- **Attributes** (typed schema via `sdata_desc_t`, declared with `SDATA()` macros)
- **Commands** exposed to the control plane
- **Action functions** called when an event fires in a given state

GObjects are organized into a hierarchical tree forming a **Yuno** (a deployable process). Communication between gobjs happens exclusively via events carrying JSON key-value payloads (`json_t *kw`).

### Layered Build Dependencies

```
gobj-c          ← core GObject framework, logging, buffers, JSON config
ytls            ← TLS abstraction (OpenSSL or mbed-TLS)
yev_loop        ← async event loop (epoll-based, non-blocking I/O)
libjwt          ← JWT authentication
timeranger2     ← append-only time-series persistence
root-linux      ← runtime GClasses: TCP/UDP, timers, channels, protocols, DB
modules/c       ← optional: c_console, c_postgres, c_prot
utils/c         ← CLI tools (ycommand, ytests, …)
yunos/c         ← deployable services (mqtt_broker, yuno_agent, …)
```

### Key Source Locations

| Path | What's there |
|------|-------------|
| `kernel/c/gobj-c/src/gobj.h` | GClass definition macros, SData types, FSM API |
| `kernel/c/gobj-c/src/gobj.c` | Core ~12K-line GObject runtime |
| `kernel/c/yev_loop/` | Non-blocking event loop engine |
| `kernel/c/timeranger2/` | Time-series DB (append-only, key-indexed) |
| `kernel/c/root-linux/src/` | All runtime GClasses (`c_tcp`, `c_timer`, `c_prot_*`, `c_treedb`, …) |
| `yunos/c/mqtt_broker/` | MQTT v3.1.1 + v5.0 broker with persistence |
| `yunos/c/yuno_agent/` | Yuno lifecycle manager (start/stop/update) |
| `utils/c/ycommand/` | Control-plane CLI — sends commands to running yunos |
| `tests/c/` | Test suites (run via `ctest`) |
| `outputs/` | Compiled libs, headers, and yuno binaries |

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

`yev_loop` drives everything. GClasses attach `yev_event_t` handles for:
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
See `MEMORY.md` for `content64` expansion syntax and deployment workflow.

## Environment Variables

| Variable | Purpose |
|----------|---------|
| `YUNETAS_BASE` | Root of this repo (auto-set by `yunetas-env.sh`) |
| `YUNETAS_OUTPUTS` | `../outputs` — where compiled artefacts land |
| `YUNETAS_YUNOS` | `../outputs/yunos` — deployed yuno binaries |
