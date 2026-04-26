# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## ⚠️ CRITICAL: Memory allocation in C code

**NEVER use raw `malloc` / `calloc` / `realloc` / `free` / `strdup` / `strndup` in Yuneta C code.**
**ALWAYS use the `gbmem_*` wrappers declared in `kernel/c/gobj-c/src/gbmem.h`.**

This applies to every file under `kernel/c/`, `modules/c/`, `utils/c/`, `yunos/c/`, `tests/c/`,
`performance/c/`, and `stress/c/` — i.e. any C code that links against the gobj-c framework.

| Forbidden          | Use instead                  |
|--------------------|------------------------------|
| `malloc(n)`        | `gbmem_malloc(n)`            |
| `calloc(n, sz)`    | `gbmem_calloc(n, sz)`        |
| `realloc(p, n)`    | `gbmem_realloc(p, n)`        |
| `free(p)`          | `gbmem_free(p)`              |
| `strdup(s)`        | `gbmem_strdup(s)`            |
| `strndup(s, n)`    | `gbmem_strndup(s, n)`        |

**Why it matters:**
- `gbmem_*` routes through Yuneta's allocator, which is swapped at startup via
  `gbmem_set_allocators()` and `json_set_alloc_funcs()`. Mixing raw libc calls
  with `gbmem_*` causes pointers allocated by one to be freed by the other →
  heap corruption or leaks that bypass the `CONFIG_DEBUG_TRACK_MEMORY` audit.
- Memory-tracking, size limits (`MEM_MAX_BLOCK`, `MEM_MAX_SYSTEM_MEMORY`) and
  the superblock pool (`USE_OWN_SYSTEM_MEMORY`) only work if every allocation
  goes through `gbmem_*`.
- Jansson is routed through `gbmem_*` via `json_set_alloc_funcs()`, so all
  `json_*` APIs are already safe — no special handling needed there.

**Rule of thumb:** if the file links against `yunetas.h` or the gobj-c framework,
use `gbmem_*`. The only exception is third-party code isolated in
`kernel/c/linux-ext-libs/` (OpenSSL, liburing, …) which uses its own allocator.

When reviewing diffs or writing new code, grep for `\bmalloc\s*\(`, `\bfree\s*\(`,
`\brealloc\s*\(`, `\bstrdup\s*\(` and replace every hit with the `gbmem_*`
equivalent.

## ⚠️ CRITICAL: Always braces, never single-line bodies

**Every `if` / `else` / `else if` / `for` / `while` / `do` body must
be wrapped in `{ ... }` and laid out across multiple lines.** Applies
to **every language** in the repo — C, JS, Python utility scripts,
shell — and to every block, no matter how short.

| Forbidden                           | Required                                  |
|-------------------------------------|-------------------------------------------|
| `if(!ptr) return;`                  | `if(!ptr) {`<br>`    return;`<br>`}`      |
| `if(x) doIt();`                     | `if(x) {`<br>`    doIt();`<br>`}`         |
| `for(...) continue;`                | `for(...) {`<br>`    continue;`<br>`}`    |
| `if(x) y(); else z();`              | `if(x) {`<br>`    y();`<br>`} else {`<br>`    z();`<br>`}` |
| `if(x) { y(); }` (one-liner)        | `if(x) {`<br>`    y();`<br>`}`            |

**Why it matters (visual reasoning):** the user reviews code by
*looking* at it. A `return;` or `continue;` hidden at the end of a
single-line `if` is invisible at a glance — it disappears into the
condition. Important control-flow statements (`return`, `continue`,
`break`, `throw`) deserve a braced body so the eye lands on them
immediately when scanning the file.

**Bad — control-flow disappears at the right edge:**

```js
for(let nav of priv.navs) {
    let level = gobj_read_attr(nav, "level");
    if(level !== "secondary") continue;
    let menu_id = gobj_read_attr(nav, "menu_id") || "";
    let m = /^secondary\.(.+)$/.exec(menu_id);
    if(!m) continue;
    let $c = gobj_read_attr(nav, "$container");
    if(!$c) continue;
    ...
}
```

**Good — every guard is visible:**

```js
for(let nav of priv.navs) {
    let level = gobj_read_attr(nav, "level");
    if(level !== "secondary") {
        continue;
    }
    let menu_id = gobj_read_attr(nav, "menu_id") || "";
    let m = /^secondary\.(.+)$/.exec(menu_id);
    if(!m) {
        continue;
    }
    let $c = gobj_read_attr(nav, "$container");
    if(!$c) {
        continue;
    }
    ...
}
```

When reviewing or writing code, search for these patterns and fix
each occurrence:

- `\)\s*(return|continue|break|throw)` followed by anything other
  than `{` on the next character.
- `\)\s*[a-zA-Z_$].*[^{]\s*$` — a non-brace statement after a
  control-flow `)` on the same line.
- `\}\s*else\s+(?!if|\{)` — an `else` whose body starts with code,
  not a brace.
- `\)\s*\{[^}]*\}\s*$` — a one-liner braced body (the body and the
  braces all collapsed onto a single line). Expand it.

Yes, this is a verbose style. It is intentional. Visibility wins.

## System Prerequisites

Install system dependencies (from doc.yuneta.io):

```bash
sudo apt -y install --no-install-recommends \
  git mercurial make cmake ninja-build \
  gcc clang g++ \
  python3-dev python3-pip python3-setuptools \
  python3-tk python3-wheel python3-venv \
  libjansson-dev libpcre2-dev liburing-dev libcurl4-openssl-dev \
  libpcre3-dev zlib1g-dev libssl-dev \
  perl dos2unix tree curl \
  postgresql-server-dev-all libpq-dev \
  kconfig-frontends telnet pipx \
  patch gettext fail2ban rsync \
  build-essential pkg-config ca-certificates linux-libc-dev

pipx install kconfiglib
```

## Quick Start (clone to build)

```bash
# 1. Clone and enter the repo
git clone <repo-url> && cd yunetas

# 2. Install the yunetas CLI tool
pipx install yunetas

# 3. Generate .config with your build preferences
menuconfig

# 4. Set up environment (must be sourced from within the repo directory)
source yunetas-env.sh

# 5. Apply compiler selection to external libs (requires sudo for update-alternatives)
./set_compiler.sh

# 6. Build external dependency libraries
cd kernel/c/linux-ext-libs
./extrae.sh             # clone libraries
./configure-libs.sh     # configure, build and install into outputs_ext/
cd ../../..

# 7. Initialize build directories and generate headers
yunetas init

# 8. Build everything
yunetas build

# 9. Run tests
yunetas test
```

## Build Commands

### Install the `yunetas` CLI tool

```bash
pipx install yunetas
```

### Build workflow

```bash
# Set up environment (must be sourced from within the repo directory)
source yunetas-env.sh

# FIRST TIME (or after any .config change): initialize build directories and generate headers
yunetas init

# Build and install (preferred — respects dependency order)
yunetas build

# Run all tests
yunetas test

# Run a single test by name (from repo root)
ctest -R test_c_timer --output-on-failure --test-dir build

# Run ctest in a loop until first failure (useful for flaky test detection)
./ctest-loop.sh

# Clean build artifacts
yunetas clean
```

### Building a single module

For faster iteration when working on a specific module, build just that module directly:

```bash
cd kernel/c/gobj-c/build && make install    # build and install one module
cd tests/c/c_timer/build && make && ctest --output-on-failure   # build and test one test
```

### JavaScript framework (kernel/js/gobj-js/)

```bash
cd kernel/js/gobj-js
npm install
npm run build          # vite build → dist/ (CJS, ES, UMD)
npm test               # vitest
npm run test:watch     # vitest --watch
npm run test:coverage  # vitest --coverage
```

### When to re-run `yunetas init`

`yunetas init` must be run:
- The **first time** after cloning/setting up the repo
- Any time `.config` changes (compiler, build type, enabled modules)

`yunetas init` does the following:
- Reads compiler (`CONFIG_USE_COMPILER_CLANG/GCC`) and build type (`CONFIG_BUILD_TYPE_*`) from `.config`
- Recreates the `outputs/` directory and subdirs (`include/`, `lib/`, `bin/`, `yunos/`)
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
./extrae.sh             # clone libraries
./configure-libs.sh     # configure, build and install into outputs_ext/

```

For **fully static glibc** builds (`CONFIG_FULLY_STATIC`) use the standard `configure-libs.sh` — it already includes the necessary `no-dso` and `no-sock` OpenSSL flags.

### Build configuration

Configuration is controlled by `.config` (Kconfig format). This file is **not tracked by git** (it's in `.gitignore`). Generate it by running:

```bash
menuconfig    # interactive TUI configurator — select compiler, build type, modules, etc.
```

Key knobs:
- Compiler: `CONFIG_USE_COMPILER_GCC=y` (default) or CLANG
- Build type: `CONFIG_BUILD_TYPE_RELWITHDEBINFO=y` (default), Debug, Release, MinSizeRel
- TLS: `CONFIG_HAVE_OPENSSL=y` (default) and/or `CONFIG_HAVE_MBEDTLS=y` — both can be enabled for runtime selection
- Static linking: `CONFIG_FULLY_STATIC=y` — fully static glibc binaries (GCC or Clang)
- Debug extras: `CONFIG_DEBUG_WITH_BACKTRACE`, `CONFIG_DEBUG_TRACK_MEMORY`, `CONFIG_DEBUG_PRINT_YEV_LOOP_TIMES`
- Optional modules: `CONFIG_MODULE_CONSOLE`, `CONFIG_MODULE_MQTT`, `CONFIG_MODULE_POSTGRES`, `CONFIG_MODULE_TEST`

### Fully Static Binaries (`CONFIG_FULLY_STATIC`)

Yuneta can produce **fully static glibc binaries** using GCC or Clang — no shared libraries, no dynamic linker, runs on any Linux system of the same architecture without installing anything.  It uses standard glibc statically linked.

> Works with both TLS backends: `CONFIG_HAVE_OPENSSL` and `CONFIG_HAVE_MBEDTLS`.
> Exception: `emailsender` utility cannot be static (depends on libcurl).

#### How to build static binaries

```bash
# 1. Enable in menuconfig
menuconfig   # set CONFIG_FULLY_STATIC=y (and keep GCC or Clang as compiler)

# 2. Rebuild external libs with the static-friendly OpenSSL flags
cd kernel/c/linux-ext-libs
./configure-libs.sh     # already includes no-dso + no-sock for OpenSSL (see below)
cd ../../..

# 3. Re-initialise (outputs/ is recreated, yuneta_config.h regenerated)
yunetas init

# 4. Build everything
yunetas build
```

#### OpenSSL flags required for clean static builds (`configure-libs.sh`)

Two extra flags are added to OpenSSL's `./config` line:

| Flag | What it removes | Why needed |
|------|-----------------|------------|
| `no-dso` | DSO/dlopen engine loader | Eliminates `dlopen`/`dlsym` linker warnings in static binaries |
| `no-sock` | `bio_addr.c` / `bio_sock.c` (socket BIO layer) | Eliminates `BIO_lookup_ex`→`getaddrinfo` and `BIO_gethostbyname` warnings; safe because Yuneta never uses OpenSSL socket BIOs — all I/O goes through `ytls` + `yev_loop` |

Both flags have been verified working: rebuilt OpenSSL + full Yuneta suite with no errors.

#### TLS backend size impact on static binaries

Measured on AMD64, `RelWithDebInfo`, fully static (`CONFIG_FULLY_STATIC`):

| Binary  | OpenSSL | mbedTLS |
|---------|---------|---------|
| watchfs | 29.1 MB | 10.2 MB |
| ybatch  | 29.1 MB | 10.3 MB |
| ycli    | 30.8 MB | 11.9 MB |

**mbedTLS produces binaries ~3× smaller than OpenSSL** when linking statically.
The difference is almost entirely the OpenSSL crypto/TLS library footprint.
Choose `CONFIG_HAVE_MBEDTLS` when binary size matters (embedded, edge deployment).

**Speed trade-off**: mbedTLS is slower than OpenSSL for TLS round-trips —
roughly ~40% lower throughput on the `perf_c_tcps` benchmarks (see
`performance/c/README.md`). So the backend choice is a size-vs-speed
trade-off: OpenSSL for hot-path servers, mbedTLS when the binary has
to stay small.

#### What was changed in the Yuneta source to support `CONFIG_FULLY_STATIC`

Static glibc binaries cannot call NSS (Name Service Switch) or the system resolver at runtime.  The following changes were made, all guarded by `#ifdef CONFIG_FULLY_STATIC`:

| File | Change |
|------|--------|
| `kernel/c/gobj-c/src/helpers.h/.c` | Added `static_getpwuid()`, `static_getpwnam()`, `static_getgrnam()`, `static_getgrouplist()` — read `/etc/passwd` and `/etc/group` directly, bypassing NSS |
| `kernel/c/gobj-c/src/gobj.c` | `getpwuid()` → `static_getpwuid()` |
| `kernel/c/root-linux/src/c_yuno.c` | `getpwuid()` → `static_getpwuid()` |
| `kernel/c/root-linux/src/entry_point.c` | `getpwuid()` → `static_getpwuid()` |
| `kernel/c/yev_loop/src/static_resolv.c/.h` | Added `yuneta_getaddrinfo()` / `yuneta_freeaddrinfo()`: a full UDP DNS resolver (reads `/etc/resolv.conf` + `/etc/hosts`, handles numeric addresses). `#define` macros in `static_resolv.h` redirect all `getaddrinfo`/`freeaddrinfo` call sites in files that include it |
| `kernel/c/root-linux/src/c_cli.c` | `getpwuid()->pw_dir` → `static_getpwuid()` + NULL guard |
| `utils/c/ycommand/src/main.c` | `getpwuid()->pw_dir` → `static_getpwuid()` + NULL guard |

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
- `kernel/js/gobj-js/` — JavaScript framework (uses Vite)
- `kernel/js/lib-yui/` — Yuneta UI Library (reusable GUI components)

### Layered Build Dependencies

Build order as processed by `yunetas` (each layer depends on all above it):

```
kernel/c/gobj-c       ← core GObject framework, logging, buffers, JSON config
kernel/c/libjwt       ← JWT authentication
kernel/c/ytls         ← TLS abstraction (OpenSSL and/or mbed-TLS, runtime selectable)
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
| `kernel/js/` | JavaScript implementations (gobj-js framework, lib-yui UI library) |
| `yunos/c/mqtt_broker/` | MQTT v3.1.1 + v5.0 broker with persistence |
| `yunos/c/yuno_agent/` | Yuno lifecycle manager (start/stop/update) |
| `utils/c/ycommand/` | Control-plane CLI — sends commands to running yunos |
| `tests/c/` | Test suites (run via `ctest`) |
| `performance/c/` | Performance benchmarks (TCP, TLS, ping-pong) |
| `tools/cmake/` | CMake toolchain files (`project.cmake`) |
| `tools/packages/` | Debian packaging scripts (AMD64, ARM32, ARMhf, RISCV64) |
| `scripts/` | Utility scripts (added to `PATH` by `yunetas-env.sh`) |
| `docs/doc.yuneta.io/` | Sphinx documentation site (API docs, guides) |
| `outputs/` | Compiled libs, headers, and yuno binaries (created by `yunetas init`) |
| `outputs_ext/` | Built external libraries |

### GClass Implementation Pattern

Every GClass follows this structure in its `.c` file:

```c
// 1. Private data struct
typedef struct { ... } priv_t;

// 2. Attribute schema (SData)
PRIVATE sdata_desc_t attrs_table[] = {
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

### GClass section layout (authoritative)

Every gclass file — in any language — must mirror the layout of the
matching template under
[`utils/c/yuno-skeleton/skeletons/`](utils/c/yuno-skeleton/skeletons).
That directory is the **single source of truth** for the minimum
structure of a gclass; the templates live there:

| Flavour                   | Skeleton                                                     |
|---------------------------|--------------------------------------------------------------|
| C service-level gclass    | `gclass_service/c_+rootname+.c_tmpl` (+`.h_tmpl`)            |
| C child-level gclass      | `gclass_child/c_+rootname+.c_tmpl` (+`.h_tmpl`)              |
| JS gclass                 | `js_gclass/+rootname+.js_tmpl`                               |
| Standalone yuno (C)       | `yuno_standalone/src/c_+rootname+.c_tmpl` (+`.h_tmpl`)       |
| Citizen yuno (C)          | `yuno_citizen/src/c_+rootname+.c_tmpl` (+`.h_tmpl`)          |

Future languages add new sub-directories here; the layout rule is the
same.

**Every banner from the skeleton must be present in the gclass, even
when its section is empty.** The banner is the visual anchor used to
*scan* the file — it documents intent and keeps the layout uniform
across the whole codebase, regardless of how full each section is on
a given day. Adding extra banners outside the skeleton set is also
forbidden, for the same reason.

Canonical order — **always in this order**:

1. **Framework Methods** — `mt_create`, `mt_destroy`, `mt_start`,
   `mt_stop`, `mt_writing`, `mt_play`, `mt_pause`, etc.
2. **Commands** — ONLY the `cmd_*` entry points registered in the
   `command_table`. No helpers.
3. **Local Methods** — every helper function used by commands, actions
   or framework methods (e.g. `cb_walk_*`, `reload_*_from_attrs`,
   callbacks like `yev_callback`).
4. **Actions** — `ac_*` functions referenced by `ev_action_t` state
   tables.
5. **FSM** — global methods table (`gmt`), `GOBJ_DEFINE_GCLASS`, state
   tables, event types, and `create_gclass()` / `register_*()`.

Exact banner format (note the indentation and the spacing between the
banner and adjacent code — four blank lines before and after):

```c
                    /***************************
                     *      Commands
                     ***************************/
```

The FSM banner uses a slightly longer form:

```c
                    /***************************
                     *                          FSM
                     ***************************/
```

**Rules that follow from this layout** (learned the hard way while
reviewing the cert-reload feature):

- **All five banners must be present**, even when a section is empty.
  An empty Commands section is still valid — the banner documents
  intent and keeps the layout uniform.
- **Helpers used by commands belong in Local Methods**, never in
  Commands. The Commands section holds only the `cmd_*` functions
  that appear in `command_table`. For example, `reload_ytls_from_attrs`
  (used by `cmd_reload_certs`) goes under Local Methods.
- **Walk callbacks (`cb_walk_*`), tree helpers, parsing helpers,
  conversion helpers** — Local Methods.
- Do **not** introduce a duplicate banner when the file already has
  the target section. Merge new functions into the existing section
  instead.
- A few historic gclasses predate the skeleton and have Local Methods
  placed **before** Commands (e.g. `c_tcp_s.c`, `c_udp_s.c`). When
  adding new code to such files, merge into the existing sections
  rather than reordering them — reordering pollutes `git blame` for
  unrelated code. Greenfield gclasses should always follow the
  skeleton order above.

`c_yuno.c` and `c_agent.c` are the canonical large-gclass examples of
this layout; `c_timer.c` is the minimal example.

### JS GClass — banner set per the skeleton

The JS skeleton
([`js_gclass/+rootname+.js_tmpl`](utils/c/yuno-skeleton/skeletons/js_gclass/%2Brootname%2B.js_tmpl))
defines four banners, **all four mandatory even when empty**:

1. `Framework Methods` — indented, 30-star wrapper.
2. `Local Methods` — indented, 27-star wrapper.
3. `Actions` — indented, 27-star wrapper.
4. `FSM` — file-level (NOT indented), 64-star wrapper.

Every `mt_*` / `ac_*` function takes its own block header on top
(`Framework Method: Create`, etc.) — same skeleton.

The C skeletons have one extra section (`Commands`) between
Framework Methods and Local Methods; the FSM banner in C is also a
file-level wrapper. See `gclass_service/c_+rootname+.c_tmpl` for
the canonical 5-section C layout.

**The user reads gclass files visually**, by scanning for these
banners. A missing or non-canonical banner makes the file unreadable
for the reviewer even when the code is correct.

### GClass blank-line spacing rules (apply to every language)

The skeleton encodes a **fixed spacing grammar** that the eye uses to
locate sections. It is not optional and is the most common drift in
generated code, so the rules are spelled out here in full.

**Rule A — Indented section banners (Framework Methods, Commands,
Local Methods, Actions) need exactly 4 blank lines above AND 4 blank
lines below.** No more, no fewer. The 4 blanks are how the banner
reads as a separator at a glance. The FSM banner in JS and C is the
file-level wrapper form (not indented) and rides directly on top of
its first block (`gmt`/states/etc.) with at most 1 blank above —
match the skeleton exactly.

**Rule B — Everywhere else, the separator is exactly 1 blank line.**
Two blank lines between regular code is forbidden. This includes:

- between two functions inside the same section,
- between a function's closing `}` and the next per-function block
  header `/*** Framework Method: Start ***/`,
- between the import block and the first file-level block header,
- between a top-level `const`/`let` and the next file-level block
  header.

**Rule C — Per-function block headers (`/*** Framework Method:
Create ***/`, `/*** Action: foo ***/`) sit directly above their
function with no blank line between header and `function`.** The
1-blank separator from Rule B goes *above* the header.

Canonical layout, copy-paste reference:

```js
let __gclass__ = null;
                            ← rule A: blank #1
                            ← rule A: blank #2
                            ← rule A: blank #3
                            ← rule A: blank #4
                    /******************************
                     *      Framework Methods
                     ******************************/
                            ← rule A: blank #1
                            ← rule A: blank #2
                            ← rule A: blank #3
                            ← rule A: blank #4
/***************************************************************
 *          Framework Method: Create
 ***************************************************************/
function mt_create(gobj) {
    ...
}
                            ← rule B: blank #1 (only one!)
/***************************************************************
 *          Framework Method: Start
 ***************************************************************/
function mt_start(gobj) {
    ...
}
```

**Common mistakes to NOT reproduce:**

```js
let __gclass__ = null;          /* WRONG — rule A wants 4 blanks above the banner */


                    /******************************
                     *      Framework Methods
                     ******************************/
```

```js
    return 0;
}                               /* WRONG — rule B says 1 blank, never 2 */


/***************************************************************
 *          Framework Method: Start
 ***************************************************************/
```

When in doubt, open
`utils/c/yuno-skeleton/skeletons/js_gclass/+rootname+.js_tmpl` (or
the matching C template) and **count blank lines**. The skeleton is
the answer.

### GClass subscription model (CHILD vs SERVICE) — applies to every language

Every gclass picks **exactly one** of two canonical subscription
patterns and writes the block verbatim, with the canonical comment,
inside `mt_create`. Do not invent a third path or weaken either
pattern to silence a runtime error.

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

How to choose:

- **CHILD** — the gobj is created with a parent gobj
  (`gobj_create(name, GCLASS, kw, parent)`); the parent is the
  natural audience for its output events. The parent's FSM **must**
  declare every event the child publishes.
- **SERVICE** — the gobj is created via
  `gobj_create_default_service` / `gobj_create_service`; it is a
  top-level node, so subscribers must opt in explicitly via the
  `subscriber` attr.

When `gobj_publish_event` from a CHILD raises *"Event NOT DEFINED in
state: <parent>, <state>, <event>"*, the fix is one of:

1. Declare the event/action in the parent gclass's FSM, **or**
2. Confirm the publishing gobj is actually a SERVICE and switch its
   `mt_create` block accordingly.

**Never** strip the parent fallback from a CHILD gclass to silence
the error. That breaks the convention every other gclass relies on.

Reference: `kernel/js/lib-yui/src/c_yui_form.js` (CHILD),
`kernel/js/lib-yui/src/c_yui_main.js` (SERVICE),
`kernel/c/root-linux/src/c_timer.c` (CHILD in C).

### Writing tests against the gobj framework

Two gotchas that break tests at runtime (not at compile time) — learned
from reviewer fixes on the cert-reload test suite:

- **`gobj_end()` must run BEFORE any `get_cur_system_memory()` check.**
  `gobj_start_up()` allocates a small baseline (measured ~104 bytes)
  that is only freed by `gobj_end()`. If the leak check happens first,
  it always reports that baseline as a leak in debug builds with
  `CONFIG_DEBUG_TRACK_MEMORY`, and the test fails regardless of
  whether the code under test is clean. Pattern:

  ```c
  do_test();
  gobj_end();                      // free baseline FIRST
  size_t leaked = get_cur_system_memory();
  if(leaked != 0) { /* real leak */ }
  ```

  To tell a real leak from the baseline, probe the same check across
  0 / 1 / N iterations of the code under test: a constant delta means
  the extra allocation belongs to startup, not to the loop.

- **`set_expected_results()` + `test_json(NULL)` uses a strict FIFO.**
  Every `gobj_log_info` / `gobj_info_msg` emitted during the test must
  appear in the `error_list` **once per emission, in order**. If the
  test runs a phase twice (e.g. two message exchanges sandwiching a
  reload), every recurring log line must appear twice in the list. A
  shorter list causes the extra emissions to be flagged as unexpected
  and the test fails even when the code is correct.

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

### TreeDB (tr_treedb) — Graph Memory Database on timeranger2

TreeDB is a **graph memory database** that uses timeranger2 for persistence. Nodes belong to **topics** and are linked via **hook/fkey relationships**.

#### Key Concepts

- **Topics**: collections of nodes (e.g., `departments`, `users`). Each topic has a schema defining its columns.
- **Nodes**: JSON objects identified by a primary key (`id`). Each node has a `__md_treedb__` metadata section containing `g_rowid`, `i_rowid`, `t`, `tm`, `tag`, `pure_node`, `topic_name`, `treedb_name`.
- **Hooks**: fields on **parent** nodes that reference children. Hooks are in-memory references (not persisted directly). Defined with `'flag': ['hook']` and a `'hook'` mapping (e.g., `{'departments': 'department_id'}` means "child departments link back via their `department_id` fkey").
- **Fkeys**: fields on **child** nodes that store persistent references to parents. Defined with `'flag': ['fkey']`. Fkey values are strings like `"departments^direction^departments"` (topic^parent_id^hook_name).
- **Hook+Fkey**: a field can be both hook and fkey (e.g., `departments.users` has `'flag': ['hook', 'fkey']`). As a hook it links child users; as a fkey it stores references when linked as a child via another hook.

#### Persistence Rules (CRITICAL)

- Every call to `tranger2_append_record()` writes a new record to disk. This increments `g_rowid` and `i_rowid` for that key within its topic.
- `g_rowid`: cumulative total of records appended for a given key in a topic (monotonically increasing, never resets).
- `i_rowid`: row index within the key's md2 file (also monotonically increasing).
- **Only `g_rowid` and `i_rowid` are set by timeranger2.c** — never modify these values directly in test code.
- **`treedb_create_node()`**: calls `tranger2_append_record()` → g_rowid=1 for new node.
- **`treedb_link_nodes(hook, parent, child)`**: updates the **child's fkey field** and saves the **child** to disk → child's g_rowid increments. The parent (hook owner) is NOT saved.
- **`treedb_unlink_nodes(hook, parent, child)`**: clears the **child's fkey field** and saves the **child** to disk → child's g_rowid increments. The parent is NOT saved.
- **Key rule**: link/unlink operations save ONLY the child node (the one with the fkey), NEVER the parent node (the one with the hook).

#### Hook Mappings Example (schema_sample)

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

#### g_rowid Tracing Example

For a department "administration" through the test sequence:
1. `treedb_create_node("administration")` → g_rowid=1
2. `treedb_link_nodes("departments", direction, administration)` → administration's `department_id` fkey updated → g_rowid=2
3. `treedb_link_nodes("managers", operation, administration)` → administration's `users` fkey updated → g_rowid=3
4. `treedb_unlink_nodes("managers", operation, administration)` → administration's `users` fkey cleared → g_rowid=4
5. `treedb_link_nodes("managers", operation, administration)` → administration's `users` fkey updated → g_rowid=5

#### Test Fotos (Expected State Snapshots)

Test "foto" files (e.g., `foto_final1.c`, `foto_final2.c`, `foto_final3.c`) contain the expected full treedb state as a JSON string at specific points in the test sequence. When updating fotos:
- Nodes appear **multiple times** in the JSON: once in the `id` index and again nested inside parent hooks. All occurrences of the same node share the same `g_rowid`/`i_rowid`.
- Different test checkpoints may need **different foto files** if g_rowid values differ (e.g., `foto_final1` for pre-compound state, `foto_final3` for post-compound-unlink state).

### Control Plane (ycommand)

Running yunos expose commands and stats over a local socket. Use `ycommand` to interact:
```bash
ycommand -c 'help'                        # list commands of the default yuno
ycommand -c 'stats'                       # get stats
ycommand -c 'list-yunos'                  # list all managed yunos with pid/status
ycommand -c 'list-binaries'               # list stored binaries with size and date
ycommand -c 'kill-yuno id=<id>'           # stop a yuno
ycommand -c 'update-binary id=X content64=$$(X)'   # upload new binary (same version)
ycommand -c 'run-yuno'                    # start all enabled yunos
```

### Debugging a running yuno

#### 1. Activate traces

Traces depend on what is being investigated. Each GClass defines its own trace levels;
there are also global traces that apply across all GClasses.

```bash
# Discover available trace levels for a GClass
ycommand -c 'command-yuno id=<id> service=__yuno__ command=get-gclass-trace gclass=<GClass>'

# Enable a GClass trace level
ycommand -c 'command-yuno id=<id> service=__yuno__ command=set-gclass-trace gclass=<GClass> set=1 level=<level>'

# Enable a global trace level
ycommand -c 'command-yuno id=<id> service=__yuno__ command=set-global-trace level=<level> set=1'
```

Global trace levels include `machine` (FSM events), `connections`, `traffic`, among others.
GClass trace levels are specific to each class — e.g. `C_TCP` has `tls`, `traffic`, `connect`, etc.

#### 2. Monitor logs

```bash
# Find the active log (most recent)
ls -lt /yuneta/realms/<realm>/<yuno>/logs/

# Follow in real time, filtering by keyword
tail -f /yuneta/realms/<realm>/<yuno>/logs/<N>.log | grep -a "keyword"
```

#### 3. Deploy an updated binary

**Always go through the agent — never kill processes manually or overwrite a running binary directly.**

```bash
# 1. Build
cd /yuneta/development/yunetas/yunos/c/<yuno>/build && make clean && make install

# 2. Kill the running yuno(s)
ycommand -c 'kill-yuno yuno_role=<role>'

# 3. Upload the new binary (version string stays the same)
ycommand -c 'update-binary id=<role> content64=$$(<role>)'

# 4. Verify the new size/date
ycommand -c 'list-binaries'

# 5. Start the yuno(s)
ycommand -c 'run-yuno'
```

#### 4. Deactivate traces when done

Use the same commands with `set=0`. The response shows remaining active traces;
it should return `[]` or only the permanently configured ones.

```bash
ycommand -c 'command-yuno id=<id> service=__yuno__ command=set-gclass-trace gclass=<GClass> set=0 level=<level>'
ycommand -c 'command-yuno id=<id> service=__yuno__ command=set-global-trace level=<level> set=0'
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
| `YUNETA_VERSION` | Current version (7.3.0) — used to generate `yuneta_version.h` |
| `Kconfig` | Root Kconfig definition (compiler, build type, TLS, modules, debug) |
| `TODO.md` | Tracks API renames, removals, and additions between versions |
| `CHANGELOG.md` | Release history and change log |
| `ctest-loop.sh` | Utility: runs `ctest` in a loop until first failure |
