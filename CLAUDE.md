# CLAUDE.md

Guidance for Claude Code (claude.ai/code) when working with this repository.

## ⚠️ Yunetas is a consolidated project

Before proposing or applying any change:

1. **Read in depth** the affected code and its context — not just the fragment.
2. **Identify invariants and conventions** of the module (gclass patterns, FSM, `gbmem_*`, banner layout, etc.).
3. **Analyze side effects**: callers, tests, static builds, TLS backends.
4. **Justify the change** and wait for approval before touching anything.

"Obvious" or "small" changes have caused regressions. Speed does not compensate for breaking invariants.

### Exception: `kernel/js/lib-yui/` is in active growth

The C kernel, `kernel/js/gobj-js/` and the runtime gclasses are consolidated:
treat them as described above.

`kernel/js/lib-yui/` is different. It is still maturing as more apps adopt it
(currently wattyzer, hidraulia, estadodelaire) and the declarative shell, nav,
window and form contracts are expected to grow. **Improvements are welcome**:
new toolbar item types, new `action.type` values, finer-grained `show_on`,
nav layouts, CSS hooks, etc. The bar is correctness, not conservatism.

Still applies to lib-yui:
- read the code in depth before editing,
- preserve backwards compatibility for existing consumers (wattyzer +
  hidraulia + estadodelaire) — additive changes preferred,
- run the lib-yui CI matrix (`.github/workflows/lib-yui.yml`),
- update `kernel/js/lib-yui/SHELL.md` and the corresponding skeleton when
  the contract changes.

What no longer applies:
- the "wait for approval before touching anything" gate. Propose, implement,
  test, then surface the diff for review.

## ⚠️ CRITICAL: Memory allocation in C code

**NEVER use raw `malloc` / `calloc` / `realloc` / `free` / `strdup` / `strndup` in Yuneta C code.**
**ALWAYS use the `gbmem_*` wrappers declared in `kernel/c/gobj-c/src/gbmem.h`.**

Applies to every file under `kernel/c/`, `modules/c/`, `utils/c/`, `yunos/c/`, `tests/c/`,
`performance/c/`, `stress/c/` — any C code linking against gobj-c.

| Forbidden          | Use instead                  |
|--------------------|------------------------------|
| `malloc(n)`        | `gbmem_malloc(n)`            |
| `calloc(n, sz)`    | `gbmem_calloc(n, sz)`        |
| `realloc(p, n)`    | `gbmem_realloc(p, n)`        |
| `free(p)`          | `gbmem_free(p)`              |
| `strdup(s)`        | `gbmem_strdup(s)`            |
| `strndup(s, n)`    | `gbmem_strndup(s, n)`        |

**Why:** Yuneta's allocator is swapped at startup via `gbmem_set_allocators()` and
`json_set_alloc_funcs()`. Mixing libc with `gbmem_*` causes pointers allocated by one
to be freed by the other — heap corruption or leaks that bypass `CONFIG_DEBUG_TRACK_MEMORY`.
Memory tracking, size limits (`MEM_MAX_BLOCK`, `MEM_MAX_SYSTEM_MEMORY`) and the
superblock pool only work if every allocation goes through `gbmem_*`. Jansson is
already routed through `gbmem_*`, so all `json_*` APIs are safe.

**Exception:** third-party code in `kernel/c/linux-ext-libs/` (OpenSSL, liburing, …)
uses its own allocator.

When reviewing diffs or writing code, grep for `\bmalloc\s*\(`, `\bfree\s*\(`,
`\brealloc\s*\(`, `\bstrdup\s*\(` and replace each hit.

## ⚠️ CRITICAL: Always braces, never single-line bodies

**Every `if` / `else` / `else if` / `for` / `while` / `do` body must be wrapped in
`{ ... }` across multiple lines.** Applies to every language in the repo (C, JS,
Python, shell), every block, no matter how short.

| Forbidden                           | Required                                  |
|-------------------------------------|-------------------------------------------|
| `if(!ptr) return;`                  | `if(!ptr) {`<br>`    return;`<br>`}`      |
| `if(x) doIt();`                     | `if(x) {`<br>`    doIt();`<br>`}`         |
| `for(...) continue;`                | `for(...) {`<br>`    continue;`<br>`}`    |
| `if(x) y(); else z();`              | `if(x) {`<br>`    y();`<br>`} else {`<br>`    z();`<br>`}` |
| `if(x) { y(); }` (one-liner)        | `if(x) {`<br>`    y();`<br>`}`            |

**Why:** code is reviewed visually. A `return` / `continue` / `break` / `throw`
hidden at the end of a single-line `if` disappears into the condition. Important
control-flow deserves a braced body so the eye lands on it when scanning.

When reviewing or writing code, search for and fix:

- `\)\s*(return|continue|break|throw)` followed by anything other than `{`.
- `\)\s*[a-zA-Z_$].*[^{]\s*$` — non-brace statement after a control-flow `)`.
- `\}\s*else\s+(?!if|\{)` — `else` whose body starts with code.
- `\)\s*\{[^}]*\}\s*$` — one-liner braced body. Expand it.

Verbose by design. Visibility wins.

## ⚠️ Documentation language: English only

**Every text artifact committed to this repo must be in English**, regardless of the
chat language (often Spanish). Applies equally to claudia console (Claude Code CLI)
and claudia gui (claude.ai web).

In English (always): `.md` files (READMEs, design docs, this `CLAUDE.md`,
in-tree guides, skill outputs), section headings inside those files, code comments,
commit messages, PR titles/descriptions, log messages, error strings.

What stays in the user's language: the live chat between user and assistant.
Only artifacts that get committed must be in English.

When editing a doc with Spanish content, translate it directly when the user asks
for any cleanup of that file; otherwise propose the translation first.

## System Prerequisites

See [`docs/doc.yuneta.io/installation.md`](docs/doc.yuneta.io/installation.md) for
the full apt dependency list and one-time environment setup.

## Quick Start

```bash
# 1. Install the yunetas CLI tool
pipx install yunetas

# 2. Generate .config (compiler, build type, modules)
menuconfig

# 3. Set up environment (source from within the repo)
source yunetas-env.sh

# 4. Apply compiler selection to external libs (sudo: update-alternatives)
./set_compiler.sh

# 5. Build external dependency libraries
cd kernel/c/linux-ext-libs && ./extrae.sh && ./configure-libs.sh && cd ../../..

# 6. Initialize build dirs and generate headers
yunetas init

# 7. Build everything
yunetas build

# 8. Run tests
yunetas test
```

## Build Commands

```bash
source yunetas-env.sh                                  # set env (from repo dir)
yunetas init                                           # FIRST TIME or after .config change
yunetas build                                          # build + install in dependency order
yunetas test                                           # run all tests
yunetas clean                                          # clean build artifacts

ctest -R test_c_timer --output-on-failure --test-dir build   # run a single test
./ctest-loop.sh                                              # run ctest in a loop until first failure
```

### Building a single module

```bash
cd kernel/c/gobj-c/build && make install
cd tests/c/c_timer/build && make && ctest --output-on-failure
```

### JavaScript framework (kernel/js/gobj-js/, kernel/js/lib-yui/)

```bash
cd kernel/js/gobj-js && npm install && npm run build && npm test
cd kernel/js/lib-yui && npm install && npm test
# lib-yui e2e (Playwright, optional):
./install-e2e-deps.sh && npm run test:e2e
```

CI runs the lib-yui matrix on every PR touching `kernel/js/lib-yui/**` or
`kernel/js/gobj-js/**` via `.github/workflows/lib-yui.yml`.

### When to re-run `yunetas init`

- First time after cloning.
- Any time `.config` changes (compiler, build type, enabled modules).

`yunetas init` recreates `outputs/`, regenerates `yuneta_version.h` and
`yuneta_config.h`, and runs `cmake` in each module's `build/`.
`yunetas build` runs `make install` in each module in dependency order.

### Build configuration (`.config`, Kconfig format, gitignored)

Generate via `menuconfig`. Key knobs:

- Compiler: `CONFIG_USE_COMPILER_GCC=y` (default) or CLANG.
- Build type: `CONFIG_BUILD_TYPE_RELWITHDEBINFO=y` (default), Debug, Release, MinSizeRel.
- TLS: `CONFIG_HAVE_OPENSSL=y` and/or `CONFIG_HAVE_MBEDTLS=y` (runtime selectable).
- Static linking: `CONFIG_FULLY_STATIC=y` — fully static glibc binaries.
- Debug: `CONFIG_DEBUG_WITH_BACKTRACE`, `CONFIG_DEBUG_TRACK_MEMORY`, `CONFIG_DEBUG_PRINT_YEV_LOOP_TIMES`.
- Modules: `CONFIG_MODULE_CONSOLE`, `CONFIG_MODULE_MQTT`, `CONFIG_MODULE_POSTGRES`, `CONFIG_MODULE_TEST`.

### Fully Static Binaries (`CONFIG_FULLY_STATIC`)

Produces fully static glibc binaries (GCC or Clang) that run on any Linux of the
same arch with no shared libraries. Works with both TLS backends.
Exception: `emailsender` (depends on libcurl) cannot be static.

`configure-libs.sh` already passes the OpenSSL flags needed for clean static
builds (`no-dso`, `no-sock`).

**Size vs speed trade-off (AMD64, RelWithDebInfo, static):**
- mbedTLS produces binaries ~3× smaller than OpenSSL (~10 MB vs ~30 MB).
- mbedTLS is ~40% slower for TLS round-trips (`perf_c_tcps`).
- OpenSSL for hot-path servers; mbedTLS when binary size matters (embedded/edge).

**Source-code adjustments for static (all under `#ifdef CONFIG_FULLY_STATIC`):**
NSS and the system resolver are not available in static glibc, so the following
helpers replace them — use these instead of the libc calls anywhere new code is
added:

- `static_getpwuid()` / `static_getpwnam()` / `static_getgrnam()` /
  `static_getgrouplist()` in `kernel/c/gobj-c/src/helpers.{h,c}` — read
  `/etc/passwd` and `/etc/group` directly.
- `yuneta_getaddrinfo()` / `yuneta_freeaddrinfo()` in
  `kernel/c/yev_loop/src/static_resolv.{h,c}` — UDP DNS resolver reading
  `/etc/resolv.conf` + `/etc/hosts`. The header `#define`s redirect
  `getaddrinfo`/`freeaddrinfo` for files that include it.

## Architecture

### Core Paradigm: GObject + Finite State Machines

Every component is a **GObject** (`gobj`), instance of a **GClass**. GClasses define:
- **States** and valid **events** per state.
- **Attributes** (typed schema via `sdata_desc_t`, `SDATA()` macros).
- **Commands** exposed to the control plane.
- **Action functions** invoked when an event fires in a state.

GObjects form a hierarchical tree — a **Yuno** (deployable process). Communication
happens exclusively via events carrying JSON payloads (`json_t *kw`).

### Multi-Language Implementations

- C is primary.
- `kernel/js/gobj-js/` — JavaScript framework (Vite).
- `kernel/js/lib-yui/` — Yuneta UI Library (reusable GUI components).

### Layered Build Dependencies

Build order (each layer depends on all above):

```
kernel/c/gobj-c       ← core GObject framework, logging, buffers, JSON config
kernel/c/libjwt       ← JWT authentication
kernel/c/ytls         ← TLS abstraction (OpenSSL and/or mbedTLS, runtime selectable)
kernel/c/yev_loop     ← async event loop (io_uring-based)
kernel/c/timeranger2  ← append-only time-series persistence
kernel/c/root-linux   ← runtime GClasses: TCP/UDP, timers, channels, protocols, DB
kernel/c/root-esp32   ← ESP32 port
modules/c/*           ← optional: console, mqtt, postgres, test
utils/c/*             ← CLI tools (ycommand, ytests, ylist, …)
yunos/c/*             ← deployable services (mqtt_broker, yuno_agent, …)
tests/c/*             ← test suites (run via ctest)
performance/c/*       ← benchmarks
stress/c/*            ← stress test programs
```

### Key Source Locations

| Path | What's there |
|------|-------------|
| `kernel/c/gobj-c/src/gobj.h` | GClass macros, SData types, FSM API |
| `kernel/c/gobj-c/src/gobj.c` | Core ~12K-line GObject runtime |
| `kernel/c/yev_loop/` | Non-blocking event loop (io_uring) |
| `kernel/c/timeranger2/` | Time-series DB (append-only, key-indexed) |
| `kernel/c/linux-ext-libs/` | External dep libs (OpenSSL, liburing, …) |
| `kernel/c/root-linux/src/` | Runtime GClasses (`c_tcp`, `c_timer`, `c_prot_*`, `c_treedb`, …) |
| `kernel/js/` | JS implementations (gobj-js, lib-yui) |
| `yunos/c/mqtt_broker/` | MQTT v3.1.1 + v5.0 broker |
| `yunos/c/yuno_agent/` | Yuno lifecycle manager (start/stop/update) |
| `utils/c/ycommand/` | Control-plane CLI |
| `tests/c/` | Test suites (ctest) |
| `performance/c/` | Benchmarks (TCP, TLS, ping-pong) |
| `tools/cmake/` | CMake toolchain files |
| `tools/packages/` | Debian packaging (AMD64, ARM32, ARMhf, RISCV64) |
| `scripts/` | Utility scripts (added to `PATH` by `yunetas-env.sh`) |
| `docs/doc.yuneta.io/` | Sphinx documentation site |
| `outputs/` | Compiled libs, headers, yuno binaries (created by `yunetas init`) |
| `outputs_ext/` | Built external libraries |

## ⚠️ GClass templates and skeletons

**Mandatory: every gclass — C or JS, child or service — and every standalone or
citizen yuno must follow the structure of the matching template under
[`utils/c/yuno-skeleton/skeletons/`](utils/c/yuno-skeleton/skeletons).**

The skeletons are the single source of truth for: section banners, banner order,
banner indentation/format, blank-line spacing, function block headers, and the
canonical `mt_create` subscription block. Open the matching template and copy
the structure exactly — count blank lines if in doubt.

| Flavour                   | Skeleton                                                     |
|---------------------------|--------------------------------------------------------------|
| C service-level gclass    | `gclass_service/c_+rootname+.c_tmpl` (+`.h_tmpl`)            |
| C child-level gclass      | `gclass_child/c_+rootname+.c_tmpl` (+`.h_tmpl`)              |
| JS gclass                 | `js_gclass/+rootname+.js_tmpl`                               |
| Standalone yuno (C)       | `yuno_standalone/src/c_+rootname+.c_tmpl` (+`.h_tmpl`)       |
| Citizen yuno (C)          | `yuno_citizen/src/c_+rootname+.c_tmpl` (+`.h_tmpl`)          |

**Every banner from the skeleton must be present, even when its section is
empty.** Don't add extra banners outside the skeleton set. Don't reorder
sections in legacy gclasses (e.g. `c_tcp_s.c`, `c_udp_s.c`) — merge new code
into the existing layout to keep `git blame` clean. Greenfield gclasses follow
the skeleton order.

`c_yuno.c` and `c_agent.c` are canonical large-gclass examples; `c_timer.c` is
the minimal example.

### GClass subscription model (CHILD vs SERVICE)

Every gclass picks **exactly one** pattern and writes the block verbatim inside
`mt_create`. Do not invent a third path or weaken either pattern to silence a
runtime error.

```js
/*
 *  CHILD subscription model
 */
let subscriber = gobj_read_pointer_attr(gobj, "subscriber");
if(!subscriber) {
    subscriber = gobj_parent(gobj);
}
gobj_subscribe_event(gobj, null, {}, subscriber);
```

```js
/*
 *  SERVICE subscription model
 */
const subscriber = gobj_read_pointer_attr(gobj, "subscriber");
if(subscriber) {
    gobj_subscribe_event(gobj, null, {}, subscriber);
}
```

- **CHILD** — gobj created with a parent (`gobj_create(name, GCLASS, kw, parent)`);
  the parent is the natural audience. The parent's FSM **must** declare every
  event the child publishes.
- **SERVICE** — gobj created via `gobj_create_default_service` /
  `gobj_create_service`; subscribers opt in explicitly via the `subscriber` attr.

When `gobj_publish_event` from a CHILD raises *"Event NOT DEFINED in state"*,
the fix is one of: declare the event/action in the parent's FSM, or confirm the
publishing gobj is a SERVICE and switch its `mt_create` accordingly. **Never**
strip the parent fallback from a CHILD to silence the error.

### Optional-subscriber events: `EVF_NO_WARN_SUBS`

When a SERVICE publishes an event whose subscribers are optional, tag it with
`EVF_NO_WARN_SUBS` in `event_types`. Otherwise `gobj_publish_event` logs
*"Publish event WITHOUT subscribers"* every time — noisy and not actionable.

```c
event_type_t event_types[] = {
    {EV_TIMEOUT_PERIODIC, EVF_OUTPUT_EVENT|EVF_NO_WARN_SUBS},
    {NULL, 0}
};
```

The flag is the explicit *"missing subscriber is not a bug"* annotation; don't
use it as a generic noise suppressor. Keep the warning when the gclass really
requires someone to react (e.g. a CHILD whose parent must handle the event).

Examples: `c_yui_main.js` (`EV_RESIZE`/`EV_THEME`), `c_yui_window.js`
(`EV_WINDOW_*`), `c_yuno.c` / `c_esp_yuno.c` (`EV_TIMEOUT_PERIODIC`),
`c_agent.c` / `c_agent22.c` (`EV_PLAY_YUNO_ACK`/`EV_PAUSE_YUNO_ACK`).

### Writing tests against the gobj framework

- **`gobj_end()` must run BEFORE any `get_cur_system_memory()` check.**
  `gobj_start_up()` allocates a small baseline (~104 bytes) only freed by
  `gobj_end()`. With `CONFIG_DEBUG_TRACK_MEMORY` a leak check before
  `gobj_end()` always reports that baseline as a leak. Pattern:

  ```c
  do_test();
  gobj_end();                      // free baseline FIRST
  size_t leaked = get_cur_system_memory();
  if(leaked != 0) { /* real leak */ }
  ```

  To distinguish a real leak from the baseline, probe across 0 / 1 / N
  iterations: a constant delta means it belongs to startup, not the loop.

- **`set_expected_results()` + `test_json(NULL)` is strict FIFO.** Every
  `gobj_log_info` / `gobj_info_msg` emitted must appear in the `error_list`
  once per emission, in order. If a phase runs twice, every recurring log line
  must appear twice.

## Event Loop & Async I/O

`yev_loop` drives everything via **Linux io_uring** (not epoll). GClasses attach
`yev_event_t` handles for fd I/O (TCP/UDP, serial), timers, signals.

All callbacks return without blocking. **No threading** — scaling is one yuno
per CPU core, with inter-yuno messaging.

## Persistence (timeranger2)

- Append-only log files (`tranger2_open_topic`, `tranger2_append_record`).
- Key-value index for fast retrieval by primary key or time range.
- Used by MQTT broker (sessions/subscriptions/queues) and treedb.

## TreeDB (tr_treedb) — Graph Memory Database on timeranger2

Graph memory database using timeranger2 for persistence. Nodes belong to
**topics** and link via **hook/fkey** relationships.

### Key Concepts

- **Topics**: collections of nodes (e.g. `departments`, `users`) with a schema.
- **Nodes**: JSON objects identified by `id`. Each has `__md_treedb__` metadata
  containing `g_rowid`, `i_rowid`, `t`, `tm`, `tag`, `pure_node`, `topic_name`,
  `treedb_name`.
- **Hooks**: fields on **parent** nodes referencing children. In-memory only.
  Defined with `'flag': ['hook']` and a `'hook'` mapping
  (`{'departments': 'department_id'}` = "child departments link back via their
  `department_id` fkey").
- **Fkeys**: fields on **child** nodes storing persistent references to parents.
  Defined with `'flag': ['fkey']`. Values like `"departments^direction^departments"`
  (topic^parent_id^hook_name).
- **Hook+Fkey**: a field can be both (e.g. `departments.users` is `['hook', 'fkey']`).

### Persistence Rules (CRITICAL)

- Every `tranger2_append_record()` writes a new record, incrementing `g_rowid`
  (cumulative) and `i_rowid` (md2 row index) for that key.
- **Only `g_rowid` and `i_rowid` are set by timeranger2.c** — never modify them
  directly in test code.
- `treedb_create_node()`: appends → g_rowid=1.
- `treedb_link_nodes(hook, parent, child)`: updates the **child's fkey** and
  saves the **child** → child's g_rowid increments. Parent NOT saved.
- `treedb_unlink_nodes(hook, parent, child)`: clears the **child's fkey** and
  saves the **child** → child's g_rowid increments. Parent NOT saved.
- **Key rule:** link/unlink saves ONLY the child (the one with the fkey),
  NEVER the parent (the one with the hook).

### Hook Mappings Example (schema_sample)

```
departments.departments: hook → {'departments': 'department_id'}
    (child department's department_id fkey is updated)

departments.users: hook+fkey → {'users': 'departments'}
    (as hook: child user's departments fkey is updated)
    (as fkey: updated when this department is linked as child via managers hook)

departments.managers: hook → {'users': 'manager', 'departments': 'users'}
    (linking a user child: user's manager fkey is updated → user saved)
    (linking a department child: department's users fkey is updated → department saved)
```

### Test Fotos (Expected State Snapshots)

Test "foto" files (`foto_final1.c`, `foto_final2.c`, `foto_final3.c`) contain
the expected full treedb state as JSON at specific points. When updating fotos:

- Nodes appear **multiple times** in the JSON (in `id` index and nested inside
  parent hooks). All occurrences share the same `g_rowid`/`i_rowid`.
- Different checkpoints may need different foto files if g_rowid values differ.

## Control Plane (ycommand)

Running yunos expose commands and stats over a local socket:

```bash
ycommand -c 'help'                                       # list commands of the default yuno
ycommand -c 'stats'                                      # get stats
ycommand -c 'list-yunos'                                 # list managed yunos with pid/status
ycommand -c 'list-binaries'                              # list stored binaries
ycommand -c 'kill-yuno id=<id>'                          # stop a yuno
ycommand -c 'update-binary id=X content64=$$(X)'         # upload new binary (same version)
ycommand -c 'run-yuno'                                   # start all enabled yunos
```

## Debugging a running yuno

### 1. Activate traces

Each GClass defines its own trace levels; global traces apply across all GClasses.

```bash
# Discover available trace levels for a GClass
ycommand -c 'command-yuno id=<id> service=__yuno__ command=get-gclass-trace gclass=<GClass>'

# Enable a GClass trace level
ycommand -c 'command-yuno id=<id> service=__yuno__ command=set-gclass-trace gclass=<GClass> set=1 level=<level>'

# Enable a global trace level
ycommand -c 'command-yuno id=<id> service=__yuno__ command=set-global-trace level=<level> set=1'
```

Global levels include `machine` (FSM events), `connections`, `traffic`. GClass
levels are class-specific (e.g. `C_TCP` has `tls`, `traffic`, `connect`).

### 2. Monitor logs

```bash
ls -lt /yuneta/realms/<realm>/<yuno>/logs/                                # find active log
tail -f /yuneta/realms/<realm>/<yuno>/logs/<N>.log | grep -a "keyword"    # follow + filter
```

### 3. Deploy an updated binary

**Always go through the agent — never kill processes manually or overwrite a
running binary directly.**

```bash
# 1. Build
cd /yuneta/development/yunetas/yunos/c/<yuno>/build && make clean && make install

# 2. Kill the running yuno(s)
ycommand -c 'kill-yuno yuno_role=<role>'

# 3. Upload the new binary (version stays the same)
ycommand -c 'update-binary id=<role> content64=$$(<role>)'

# 4. Verify size/date
ycommand -c 'list-binaries'

# 5. Start the yuno(s)
ycommand -c 'run-yuno'
```

### 4. Deactivate traces when done

Same commands with `set=0`. Response should be `[]` or only the permanently
configured ones.

```bash
ycommand -c 'command-yuno id=<id> service=__yuno__ command=set-gclass-trace gclass=<GClass> set=0 level=<level>'
ycommand -c 'command-yuno id=<id> service=__yuno__ command=set-global-trace level=<level> set=0'
```

## Environment Variables

| Variable | Purpose |
|----------|---------|
| `YUNETAS_BASE` | Root of this repo (auto-set by `yunetas-env.sh`) |
| `YUNETAS_OUTPUTS` | `$(dirname $YUNETAS_BASE)/outputs` — artefacts in **parent** dir of repo |
| `YUNETAS_YUNOS` | `$YUNETAS_OUTPUTS/yunos/` — deployed yuno binaries |

## Useful Files

| File | Purpose |
|------|---------|
| `YUNETA_VERSION` | Current version (7.3.1) — used to generate `yuneta_version.h` |
| `Kconfig` | Root Kconfig definition |
| `TODO.md` | API renames/removals/additions between versions |
| `CHANGELOG.md` | Release history |
| `ctest-loop.sh` | Runs `ctest` in a loop until first failure |
