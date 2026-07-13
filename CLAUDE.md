# CLAUDE.md

Guidance for Claude Code (claude.ai/code) when working with this repository.

## ⚠️ Yunetas is a consolidated project

Before proposing or applying any change:

1. **Read in depth** the affected code and its context — not just the fragment.
2. **Identify invariants and conventions** of the module (gclass patterns, FSM, `gbmem_*`, banner layout, etc.).
3. **Analyze side effects**: callers, tests, static builds, TLS backends.
4. **Justify the change** and wait for approval before touching anything.

"Obvious" or "small" changes have caused regressions. Speed does not compensate for breaking invariants.

### Restore, don't redesign

Dead branches, `if(0)` fences and TODO stubs that trace back to the pre-v7
codebase are usually designs validated by years of production use — the wiring
was cut during the v7 port, not the design. The default question is *"what
wiring is missing to restore the original behaviour?"*, never *"how do I
redesign this?"*. Treat "this looks odd" as a flag to investigate harder;
redesign only after citing the concrete scenario the original design breaks.
(This applies to prod-hardened legacy designs, not to new/experimental code
such as the gobj-ui v2 declarative shell.)

### `kernel/js/gobj-js/`, `kernel/js/gobj-ui/` and `yunos/js/` are git submodules

The C kernel and the runtime gclasses are consolidated: treat them as described
above.

The JavaScript layer is **not** consolidated — it lives in its **own
repositories** and is embedded here as **git submodules** (the same model as
`utils/python/tui_yunetas`), each at the same path it used to occupy:

- `kernel/js/gobj-js/` → `github.com/artgins/gobj-js` (tracks `main`) — the
  GObject-JS runtime.
- `kernel/js/gobj-ui/` → `github.com/artgins/gobj-ui.js` (tracks `main`/v2) — the
  UI library.
- `yunos/js/` → `github.com/artgins/yunos-js` (tracks `main`) — the JS **yunos**
  (browser SPAs: `gui_agent`, `gui_treedb`). Extracted from `yunetas/yunos/js`
  at 7.6.8 as a fresh snapshot (history stays in yunetas); it is the most
  active-changing JS layer, so it evolves on its own line with its own
  `CHANGELOG.md`.

Clone yunetas with `--recurse-submodules` (or run `git submodule update --init`).
Because each submodule sits at its original path, local `file:` consumers resolve
unchanged: the yunos' `@yuneta/gobj-js` / `@yuneta/gobj-ui` deps
(`../../../kernel/js/…` from `yunos/js/<yuno>`) and wattyzer's `file:` deps still
point at the checked-out `kernel/js/*` submodules. To edit a JS yuno: work in
`yunos/js` directly, commit on `main` in the `yunos-js` repo, then **bump this
submodule pointer in yunetas** (same flow as gobj-js/gobj-ui).

The standalone repo carries **two maintained lines**, and they are consumed in
**two different ways** (since 2026-06-16):

- **`main` branch** (the v2 line, tag `2.0.0`+) — **active development**: the
  declarative shell (`C_YUI_SHELL/NAV/PAGER/WIZARD`; the legacy stack
  `C_YUI_MAIN/TABS/ROUTING` was removed from this line in `3.0.0`).
  **This submodule tracks `main`/v2.** It is consumed **locally** by **wattyzer**
  and the in-repo yunos **`yunos/js/gui_agent`** and **`yunos/js/gui_treedb`** as
  a `file:` dependency (`@yuneta/gobj-ui` → `../../../kernel/js/gobj-ui`, exactly
  like `@yuneta/gobj-js`); they import it by package specifier
  (`@yuneta/gobj-ui/src/*.js`, resolved by v2's exports map `"./src/*"`; the
  source lives under `src/` mirroring v1, with `index.js` and the vite plugin at
  the package root). **All new gobj-ui work lands on `main`/v2**: edit
  `kernel/js/gobj-ui` directly, commit on `main` in the standalone repo, then
  bump this submodule pointer in yunetas.
- **`v1` branch** (tag `1.0.1`, npm dist-tag `legacy`) — the **frozen** legacy
  GClass GUI stack (`C_YUI_MAIN/WINDOW/TABS/ROUTING` + TreeDB editors +
  charts/maps). `v1` is **maintenance-only** — do not land feature work here.
  estadodelaire and hidraulia consume it from the **npm registry** as
  `@yuneta/gobj-ui@^1.0.1` (the **published** package, **not** this local
  checkout). The local `kernel/js/gobj-ui` checkout is **no longer `v1`** — do
  not point a `file:` dependency at it for a v1 consumer. (The in-repo
  `yunos/js/gui_treedb` was migrated to **v2** and consumes this local checkout
  by `file:`, like gui_agent and wattyzer — it is **not** a v1/npm consumer.)

`@yuneta/gobj-js` now lives in its **own repository** `github.com/artgins/gobj-js`
(public, snapshot start — history not preserved; single line on `main`, symmetric
with gobj-ui) and is embedded here as the `kernel/js/gobj-js` submodule. It is
versioned to track `YUNETA_VERSION` (SDK `7.7.2`; the gobj-js package is at
`7.7.2` on npm) and **published to npm**.
To ship a new version: edit `kernel/js/gobj-js` directly, bump its `package.json`
in lockstep with `YUNETA_VERSION`, commit on `main` in the standalone repo +
`npm publish`, then **bump this submodule pointer in yunetas**. (A gobj-js-only
patch may move ahead of `YUNETA_VERSION` between SDK releases — e.g. `7.6.7`'s
`EV_ON_CLOSE`-on-deliberate-stop fix shipped on gobj-js first; the SDK then
caught up at the `7.6.7` release.) estadodelaire/
hidraulia consume it from the registry (estadodelaire on `@yuneta/gobj-js@^7.6.7`,
hidraulia on `^7.6.6`); **wattyzer**
and the in-repo `yunos/js/gui_treedb` keep a local `file:` dep on this checkout
(the path is unchanged, so the `file:` deps still resolve).

The JS GUI scaffold (declarative-shell yuno template) lives under
`wattyzer/templates/js_gui/` (see the JS GUI scaffold note below).

Operational notes:
- The vitest suite + active source live here now (v2/`main`):
  `cd kernel/js/gobj-ui && npm install && npm test` (and `npm run build`). Run
  this when touching `kernel/js/gobj-ui/**`.
- `dist/` is no longer on any consumer's critical path: wattyzer (v2) imports
  source files by specifier, and estadodelaire/hidraulia (v1) pull `dist/` from
  the **published** npm tarball. Still run `npm run build` to validate, and
  before publishing.
- To ship a new **v1** release for estadodelaire/hidraulia: from a **v1**
  checkout of the standalone repo, `npm run build` + `npm publish`, then bump
  their `@yuneta/gobj-ui` `^1.x` range. (This local submodule stays on `main`.)
- gobj-ui installs its own copies of shared third-party libs (i18next,
  @antv/g6, maplibre-gl, tabulator-tables, tom-select, uplot,
  vanilla-jsoneditor). Any consumer that also depends on one of them must add
  `resolve.dedupe` for the full list in its `vite.config.js` (mirror
  wattyzer's, the reference v2 consumer) — otherwise components importing a
  module-level singleton (e.g. i18next's `t`) bind an uninitialized second
  copy and render blank.
- Preserve backwards compatibility for existing consumers (estadodelaire +
  hidraulia on published v1, wattyzer on local v2) and update the repo's
  `README.md` when the contract changes.

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

## ⚠️ CRITICAL: No silent errors

**Every error path must leave a trace** — either `gobj_log_error(...)` at the
failure point, or the annotation `// Error already logged` when the failing
helper already logged. Applies to every failure exit: `return -1/NULL/FALSE`,
`continue`/`break`/`goto` after a failed validation, swallowed truncation,
void-function early returns, `default:` on an unexpected enum. Never "fix" a
compiler warning (`-Wformat-truncation`, `-Wunused-result`) with a silent
early-out.

**Why:** the log is the only window into runtime failures; a silent error path
defeats the framework's observability.

Corollaries:

- **Never add a no-op FSM action to swallow *"Event NOT DEFINED in state"*.**
  The error is deliberately loud — it flags a badly built FSM or a sender
  emitting in the wrong situation. Trace who sends the event in that state and
  fix that path.
- **Never debug a running yuno with `fprintf(stderr, …)`** or ad-hoc `/tmp` log
  files. Use the trace machinery (`set-global-trace` / `set-gclass-trace`,
  `gobj_trace_msg/json/dump`) — it has scoping, standard JSON format and
  cross-yuno correlation. If no existing category fits, add a per-gclass trace
  level (16 slots) rather than bypassing the framework. Playbook:
  [`DEBUGGING.md`](yunos/c/yuno_agent/DEBUGGING.md). Pair every trace enable
  with its disable in the same session.
- **Protocol decoder severity** (`c_prot_*`, `c_websocket`, mqtt, …): a failure
  caused by a malformed/malicious **peer packet** ⇒ `gobj_log_warning` with its
  assigned category (`MSGSET_PROTOCOL`, `MSGSET_MQTT`, …), **no**
  `LOG_OPT_TRACE_STACK`, plus a length-capped dump of the offending bytes. A
  broken **internal invariant** ⇒ `gobj_log_error` with stack trace. Ask:
  "could a remote peer trigger this with bad bytes?" → warning; "our own
  contract is broken" → error.

## C coding rules

- **Paths:** assemble filesystem paths with `build_path(bf, sizeof(bf), seg1,
  ..., NULL)` (`kernel/c/gobj-c/src/helpers.h`), never `snprintf("%s/%s")`.
  `build_path` strips duplicate `/`, always null-terminates and logs on
  overflow. Applies to new/modified code; don't refactor pre-existing calls
  unprompted.
- **Buffer sizes:** use the `<limits.h>` system constants — `NAME_MAX`
  (filenames), `PATH_MAX` (full paths), `HOST_NAME_MAX`, `LOGIN_NAME_MAX`,
  `PIPE_BUF` — never hand-picked numbers. The constant documents the contract;
  `char buf[64]` lies about it.
- **Naming:** name functions by intent — verb + object + direction
  (`get_token_from_idp`, `send_token_to_browser`) — not by framework role; the
  framework keys (`mt_*`, `ac_*`, `exec_action`) already label the role. Prefer
  generic domain words over vendor names (`idp`, not `keycloak`). If a
  function's honest name is uninformative, the function is probably wrong —
  remove it, don't rename it.
- **Comments:** asymmetric by location. File headers may carry ample prose
  (gclass role, FSM, design decisions). Inside function bodies default to *no
  comment* — one short line only for a genuinely non-obvious why (hidden
  constraint, workaround, invariant). Never narrate what the next line does.
- **Section titles** in scripts (bash, Python) use the established 3-line
  comment block (`#` / `#   Title` / `#`); never invent one-liner formats
  (`#---`, `#===`) — they are invisible when scanning.
- **Copyright:** on non-trivial edits bump the ArtGins year as a **range**
  (`Copyright (c) 2024-2026, ArtGins.`), keeping the original year. Brand-new
  files use a single year. `Niyamaka` copyrights are historical — never change
  them.
- **Persistent attrs:** always name what you save —
  `gobj_save_persistent_attrs(gobj, "attr_name")` (string / list / dict of
  names). The bare call saves every `SDF_PERSIST` attr, which is wasteful and
  can clobber attrs the caller didn't touch. Same API shape in C and JS.
- **Command responses:** every non-empty `jn_comment` passed to
  `build_command_response()` in a `cmd_*` handler starts with the yuno
  identity: `json_sprintf("%s: ...", gobj_yuno_role_plus_name(), ...)`. Keep it
  on one line if the whole expression fits in 80 bytes, else break with the
  format string first. Skip when `jn_comment == 0` or when the message already
  names a specific target gobj.
- **`SDF_RSTATS` attrs backed by a priv field need `mt_reading`** in the
  GMETHODS — `priv->counter++` alone leaves the attr at zero forever (typed
  readers consult `mt_reading` first; the stored attr is never written). Keep
  attr name and priv field name identical.
- **Commands callable via `ycommand command-yuno`** hit two parser quirks:
  (1) `id=` is reserved by `command-yuno` as the yuno filter — name inner
  parameters `<thing>_id` (keep `id` as a legacy alias read as fallback in the
  handler); (2) `SDF_REQUIRED` + `DTP_JSON`/`DTP_INTEGER` parameters are
  forwarded from kw **without** type coercion — drop `SDF_REQUIRED` and
  validate in the handler, or defensively accept the string form
  (`anystring2json`).
- **`gobj_log_last_message()`** is a process-global 256-byte buffer written
  only by `priority <= LOG_ERR` logs. Prefer explicit `json_sprintf` messages
  in `cmd_*` failure paths; never rely on it across function boundaries. A
  kernel function that fails via `gobj_log_info` should call
  `gobj_log_set_last_message()` itself if callers need the cause.

### C API footguns

- **`jwt_checker_verify2()` returns claims even on FAILED verification**
  (`kernel/c/libjwt`). Only `jwt_checker_error(checker)` is authoritative —
  never treat a non-NULL payload as "verified" (that accepts forged tokens),
  and always `json_decref` the returned payload regardless of outcome.
  Canonical consumer: `c_authz.c`.
- **`kw["gbuffer"]` is auto-decref'd** by gobj's serializer table when the kw
  is decref'd. Reading the pointer with `extract=FALSE` and then calling
  `GBUFFER_DECREF` before `KW_DECREF` is a double-free (*"BAD gbuf_decref()"*).
  Decref manually only after `extract=TRUE` (ownership transferred) or when
  the gbuffer never entered a kw. Match `c_prot_raw` / `c_prot_tcp4h` in new
  protocol gclasses.

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

### Documentation conventions

- **Reviewing or debugging a yuno/gclass = improve its doc page in the same
  pass.** The verified mental model just built (config attrs, commands,
  persistence, failure semantics, trace levels) is exactly what its page under
  `docs/doc.yuneta.io/` should capture — many pages are stubs.
- **Topology diagrams are left-to-right following data flow** (sources →
  gateways → DBs → consumers; agent/utilities as a bottom band), realms as
  horizontal background bands, `role^name` node labels. No top-down layouts.

### Documentation site (doc.yuneta.io)

The site is built with **mystmd** (MyST / Jupyter Book 2), deployed via
`docs/doc.yuneta.io/deploy.sh`. Tooling quirks:

- `myst build --html` does not exit — it boots a dev server. Validate edits
  non-interactively with
  `timeout 30 myst build --html 2>&1 | grep -iE 'warning|error|⚠|fail'`, then
  `pkill -f "myst build" || true`. MyST warning lines start with `⚠`, not with
  the word "warning" — include `⚠` in any grep of build logs.
- After any **bulk** edit of `docs/doc.yuneta.io/**.md` (e.g. a release-link
  repin), run `rm -rf _build/site _build/html` before `deploy.sh` — myst reuses
  a stale AST cache and ships old content (keep `_build/templates`, the theme
  cache). Verify with a grep of `_build/html` for the old pattern (also catches
  the `build/*.md` source-copy artifacts).

## Onboarding docs (read these before touching code)

If you are a Claude instance landing in this repo for the first time, read
these in order. They are ~5800 lines total but they replace ~weeks of
spelunking through `c_agent.c` and `gobj.c`. The README is an index.

| # | Doc                                                                     | Covers                                                            |
|---|-------------------------------------------------------------------------|-------------------------------------------------------------------|
| 0 | [`yunos/c/yuno_agent/README.md`](yunos/c/yuno_agent/README.md)          | Index of the chapters below.                                      |
| 1 | [`ENTRY_POINT.md`](yunos/c/yuno_agent/ENTRY_POINT.md)                   | What `main()` does. `yuneta_entry_point` + `ydaemon.c` watcher (the autonomous-survival kernel). `/var/crash/core.%e`. |
| 2 | [`YUNO_LIFECYCLE.md`](yunos/c/yuno_agent/YUNO_LIFECYCLE.md)             | How the agent manages yunos (create/run/kill/update/delete).      |
| 3 | [`DEBUGGING.md`](yunos/c/yuno_agent/DEBUGGING.md)                       | Trace levels, log infrastructure, logcenter, SPA dev panel.       |
| 4 | [`IPC.md`](yunos/c/yuno_agent/IPC.md)                                   | Events, ievents, gates, the SPA case.                             |
| 5 | [`REALMS.md`](yunos/c/yuno_agent/REALMS.md)                             | Multi-tenancy, on-disk layout, CRUD, what is NOT realm-scoped.    |
| 6 | [`SCAFFOLDING.md`](yunos/c/yuno_agent/SCAFFOLDING.md)                   | `yuno-skeleton` templates, banner conventions.                    |
| 7 | [`YUNO_AUTH.md`](yunos/c/yuno_agent/YUNO_AUTH.md)                       | OIDC/auth_bff, `C_AUTHZ`, cert-sync. **Read §4.5**: the per-command authz check is re-armed but gated off by default (`enable_command_authz`). |
| 8 | [`GOBJ.md`](yunos/c/yuno_agent/GOBJ.md)                                 | Framework crash course (gclass, mt_*, SData, runtime tree).       |
| 9 | [`YUNO_TREEDB.md`](yunos/c/yuno_agent/YUNO_TREEDB.md)                   | timeranger2 + treedb (link-saves-child rule, `topic_version` trap). |

Each one is published as a chapter under **Operating Yuneta** in
[`docs/doc.yuneta.io`](docs/doc.yuneta.io/) (slug `/entry-point`,
`/yuno-lifecycle`, `/debugging`, `/ipc`, `/realms`, `/scaffolding`,
`/yuno-auth`, `/gobj`, `/yuno-treedb`).

**Deploying/updating yunos on a node?** Read
[`docs/doc.yuneta.io/deploying-yunos.md`](docs/doc.yuneta.io/deploying-yunos.md)
(published at `/deploying-yunos`) — the scenario-driven guide (hot-patch,
version bump, config-only, new yuno, rollback) tying together the `yunetas`
CLI, `sync_binaries.py` and `sync_configs.py`. Start there, not in
`c_agent.c`. Keep that guide updated when deploy flows change.

## System Prerequisites

See [`docs/doc.yuneta.io/installation.md`](docs/doc.yuneta.io/installation.md) for
the full apt dependency list and one-time environment setup.

Avoid **snap-packaged CLI tools** for anything the build or docs need (typst,
pandoc, formatters): snap confinement only grants `$HOME`, so they cannot read
`/yuneta/...` (symptom: *"input file not found"* for a file that exists, with
`which <tool>` → `/snap/bin/...`). Install via apt or a static binary in
`/usr/local/bin` (which shadows `/snap/bin` in `PATH`).

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

**Build via the `yunetas` CLI, not per-module make.** For any kernel/lib change
that must reach consumers, and for ANY version bump or release deploy, do a
CLEAN rebuild: `yunetas clean && yunetas build`. Incremental builds have left
stale binaries in `outputs/yunos/` — always verify the staged binary
(`outputs/yunos/<role> --print-role`) before deploying it.

`ctest --test-dir build` only **runs** tests, it never rebuilds them — the
unified root `build/` tree is built by `yunetas test` (or `cmake --build
build`), not by `yunetas build`. A raw ctest after per-module builds executes
stale binaries.

### External projects (registered in the CLI)

Projects (wattyzer, estadodelaire, …) can be registered so the `yunetas`
CLI builds them right after the SDK. Registry: `~/.yuneta/projects.json`
(machine-local user state, outside the source tree).

```bash
yunetas register-project /yuneta/development/projects/wattyzer
yunetas list-projects / unregister-project <name>
yunetas init|build|clean                  # SDK + every registered project
yunetas init|build|clean <name>...        # only those projects (SDK skipped)
yunetas init|build|clean --sdk-only       # only the SDK
yunetas sync-binaries [args]              # wrapper over tools/agent/sync_binaries.py
yunetas sync-configs [args]               # auto-match batches/<host>/ to the agent's realm_ids (*list-realms)
yunetas sync-configs --host <h> [args]    # or target one batches dir explicitly
yunetas upgrade-yunos [--no-snap|--snap-name N|-y|-n]  # shoot-snap -> find-new-yunos -> deactivate-snap
```

### Building a single module

For a quick single-module compile sanity check only — never the way to produce
deployable binaries (see the CLI rule above):

```bash
cd kernel/c/gobj-c/build && make install
cd tests/c/c_timer/build && make && ctest --output-on-failure
```

### JavaScript framework (kernel/js/gobj-js/, kernel/js/gobj-ui/)

```bash
cd kernel/js/gobj-js && npm install && npm run build && npm test
# gobj-ui submodule now tracks main/v2 — full vitest suite + build live here:
cd kernel/js/gobj-ui && npm install && npm run build && npm test
```

Run this matrix locally when touching `kernel/js/gobj-js/**` or
`kernel/js/gobj-ui/**` (the submodule now tracks main/v2 — see the submodule
note above). There is no GitHub Actions workflow for it — by design,
the only workflow in `.github/` is `release-packages.yml` (builds the AMD64
`.deb` and the x86_64 `.rpm` on a published release).

### When to re-run `yunetas init`

- First time after cloning.
- Any time `.config` changes (compiler, build type, enabled modules).
- After toggling the TLS backend in `.config`, also wipe `CMakeCache.txt` +
  `CMakeFiles/` in the affected `tests/c/*/build/` dirs — their cached
  `MBEDTLS_LIBS`/`OPENSSL_LIBS` go stale and test links fail with undefined
  TLS symbols.

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

Changing a flag in `kernel/c/linux-ext-libs/configure-libs.sh` that alters
installed paths, artifact names or exported symbols must land in the **same
commit** as the migration of every consumer (includes +
`target_link_libraries`). Grep all consumers first, list them as companion
changes, and verify with `nm`/`strings` on rebuilt binaries.

### Fully Static Binaries (`CONFIG_FULLY_STATIC`)

Produces fully static glibc binaries (GCC or Clang) that run on any Linux of the
same arch with no shared libraries. Works with both TLS backends. Since 7.4.3
the entire yuno set is static (emailsender included — libcurl was dropped in
favour of native SMTPS on top of `ytls` / `c_tcp`).

`configure-libs.sh` already passes the OpenSSL flags needed for clean static
builds (`no-dso`, `no-sock`).

**Static linking is per-target opt-in:** every yuno/util `CMakeLists.txt`
needs BOTH blocks — (1) the linker flags after `include(project.cmake)`
(`CMAKE_EXE_LINKER_FLAGS "-static ..."`, `CMAKE_FIND_LIBRARY_SUFFIXES ".a"`,
`BUILD_SHARED_LIBS OFF`) and (2) `LINK_SEARCH_START_STATIC` /
`LINK_SEARCH_END_STATIC TRUE` after `add_yuno_executable`. With only the
second, the link succeeds but glibc stays dynamic. Verify:
`ldd outputs/yunos/<name>` → *"not a dynamic executable"*.

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

### GObject design rules

- **The OS boundary is where events are born: EVERY asynchronous notification
  from the system becomes a Yuneta event.** That is the whole point of the
  framework — the FSM is the only place where things happen, so the runtime can
  be audited (the `machine` trace *is* the execution log). In C this is already
  the contract: `yev_loop` turns every io_uring completion (fd readable/
  writable, timer, signal) into an event delivered to a gobj. **The rule is
  general, and the browser is just another operating system**: a DOM click, an
  `onmessage`, a `setTimeout`, a `resize`, a resolved promise are OS
  notifications and must enter the machine as events (`gobj_send_event`), not
  run application logic inside the callback. The callback's only job is to
  translate the OS notification into an event; anything that bypasses that path
  is invisible to the trace and, by construction, undebuggable.
- **Fix the cause at its layer.** A high-level gclass must not compensate for a
  lower layer: reconnection/backoff belongs to the transport (`c_tcp`) and the
  session gclass, never to the application on top. Don't scatter `C_TIMER`s to
  "kick" the layer below, and never mask a crash with a defensive NULL check —
  find why the pointer is NULL. A semantic guard
  (`if(!gobj_is_playing(gobj))`) is fine; a pointer-existence test papering
  over the cause is not.
- **Explicit lifecycle — Yuneta is not a JVM.** Prefer, in order: (1) static
  child gobjs whose create/start/stop/destroy pair with the parent's
  lifecycle; (2) self-destroy at a clear end-of-work point
  (`if(gobj_is_volatil(gobj)) gobj_destroy(gobj)`); (3) last resort, a
  gclass-local deferred destroy via `set_timeout0(timer, 0)`.
- **No polling.** A timer re-issuing the same query is a discarded pattern in
  Yuneta — remove it when found. Alternatives, in order: on-demand refresh
  from a user action, or have the producer **publish an event** the consumer
  subscribes to. Deliberate, user-approved exceptions are commented as such in
  the code — don't "fix" those.
- **Never synchronously stop the publisher's tree from a subscriber callback.**
  `gobj_publish_event` dispatches synchronously — the callback runs inside the
  publisher's stack, and `gobj_stop_tree` there corrupts mid-iteration state.
  Defer: set a `dying` flag + a short timeout and stop from the timer action.
  Applies to C and JS alike.
- **Main services start their work in `mt_play`, not `mt_create`/`mt_start`.**
  `mt_create` creates children; `mt_play` opens queues and starts what
  connects. This is why `run-yuno play=0` leaves a yuno with
  `running=true, playing=false` until `play-yuno`.
- **Inter-yuno communication happens only between named SERVICES** with public
  events/commands. A routed view, a pure child, or any unnamed gobj can never
  be the `src` or destination of an inter-yuno message — answers route back
  via `gobj_find_service(src_service)`. Symptom of a violation: *"gobj service
  not found"* — fix the sender, don't silence the log.
- **Declarative JSON is sugar over a runtime API.** A JSON config that
  describes objects to build must call a runtime API at startup, and that API
  stays available at runtime. If a feature needs to mutate a config-described
  structure and "there's no API, it's static" — implement the missing API;
  don't hack around the static config.

### Multi-Language Implementations

- C is primary.
- `kernel/js/gobj-js/` — JavaScript framework (Vite).
- `kernel/js/gobj-ui/` — Yuneta UI Library (reusable GUI components).

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
| `kernel/js/` | JS implementations (gobj-js, gobj-ui) |
| `yunos/c/mqtt_broker/` | MQTT v3.1.1 + v5.0 broker |
| `yunos/c/yuno_agent/` | Yuno lifecycle manager (start/stop/update) |
| `utils/c/ycommand/` | Control-plane CLI |
| `tests/c/` | Test suites (ctest) |
| `performance/c/` | Benchmarks (TCP, TLS, ping-pong) |
| `tools/cmake/` | CMake toolchain files |
| `packages/deb`, `packages/rpm` | Debian/RPM packaging (used by `release-packages.yml`) |
| `scripts/` | Utility scripts (added to `PATH` by `yunetas-env.sh`) |
| `docs/doc.yuneta.io/` | MyST (mystmd) documentation site |
| `outputs/` | Compiled libs, headers, yuno binaries (created by `yunetas init`) |
| `outputs_ext/` | Built external libraries |

**`tools/` ships in the install packages; `scripts/` is repo-only.**
Node-usable / operator-facing utilities go under `tools/` (grouped by
subsystem, e.g. `tools/agent/`); repo-dev helpers stay in `scripts/`. Test:
"must this run on a bare installed node?" — yes → `tools/`.

### Module notes

- **HTTP parsing:** llhttp is vendored (`kernel/c/gobj-c/src/llhttp*`;
  http-parser was dropped as unmaintained) and bridged to the gobj event model
  by `kernel/c/root-linux/src/ghttp_parser.c` (used by `c_prot_http_cl` /
  `c_prot_http_sr`). The wrapper has caused repeated bugs — when investigating
  an HTTP parsing issue, suspect `ghttp_parser` first, not llhttp upstream.
- **MQTT:** `C_PROT_MQTT2` (`c_prot_mqtt2.c`) is the complete implementation
  and the only target for protocol-level work; `C_PROT_MQTT` is **deprecated**,
  kept only until the remaining gates migrate. Neither implements
  publish/subscribe ACL (open design item, see `TODO.md`) — don't assume
  topic-level permissions exist.

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

The **JS GUI yuno** scaffold (Vite + declarative shell on gobj-ui v2) lives in
`wattyzer/templates/js_gui/` (a private repo) since 2026-05-21. Use that copy
when starting a new declarative-shell GUI yuno.

**Every banner from the skeleton must be present, even when its section is
empty.** Don't add extra banners outside the skeleton set. Don't reorder
sections in legacy gclasses (e.g. `c_tcp_s.c`, `c_udp_s.c`) — merge new code
into the existing layout to keep `git blame` clean. Greenfield gclasses follow
the skeleton order.

`c_yuno.c` and `c_agent.c` are canonical large-gclass examples; `c_timer.c` is
the minimal example.

### GClass public interface

A gclass `.h` exposes exactly two things: `GOBJ_DECLARE_GCLASS(C_FOO);` and
`PUBLIC int register_c_foo(void);`. Any other interaction goes through the five
public mechanisms: attributes (`SDATA` + `gobj_read/write_*_attr`), commands
(`SDATACM` + `gobj_command`, ycommand-discoverable), events (`gobj_send_event`
/ `gobj_publish_event`), local methods (`gobj_local_method`), and statistics
(`SDF_STATS/RSTATS/PSTATS`, or `mt_stats` for hot paths). Adding raw `PUBLIC`
C functions to a gclass header is a layering violation — treat existing ones
as defects.

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

## JS framework and GUI rules

### gobj-js gotchas

- `createElement2(["tag", attrs, children])`: the 2nd slot MUST be an attrs
  object; text/children go in slot 3 (`["p", {}, "msg"]`,
  `["ul", {}, [li1, li2]]`).
- `gobj_create_default_service()` does NOT register in `__jn_services__` —
  `gobj_find_service()` returns null for it; capture the return value instead.
- `pure_child` gobjs are not auto-started by `c_yuno.mt_play` — call
  `gobj_start(child)` explicitly.
- `c_ievent_cli` contract (C and JS): a **deliberate** `gobj_stop` /
  `gobj_stop_tree` must still deliver the transport close to the FSM so
  `EV_ON_CLOSE` reaches subscribers when a session was open. Guard the handler
  with `gobj_is_destroying(gobj)`; never detach `onclose` to silence a late
  event.
- SPA teardown order: `set_remote_log_functions(null)` FIRST, before dropping
  the websocket or destroying the shell, in every teardown path (logout,
  open-error, restore-failed) — otherwise every teardown log recurses through
  the dead socket into *"too much recursion"*.

### JS GUI DOM: logical class names on important blocks

**Mandatory in every JS GUI (gobj-ui and the `yunos/js/*` yunos alike).** When
a gclass builds DOM, tag its elements so the tree is self-describing in the
browser Inspector:

- **Root of the view:** the `GCLASS_NAME` class **plus** a logical card name,
  e.g. `class="C_AGENT_CONSOLE CONSOLE_CARD view-card"`.
- **Every meaningful child** (status line, response panel, input row, input,
  button, list…) gets a logical class, **prefixed by the view/feature name**:
  `CONSOLE_STATUS`, `CONSOLE_COMMENT`, `CONSOLE_RESPONSE`, `CONSOLE_INPUT_ROW`,
  `CONSOLE_INPUT`, `CONSOLE_EXEC`, …

**Casing: `UPPER_SNAKE`, exactly like the gclass names** — `CONSOLE_COMMENT`,
never `console-comment`. CSS/styling classes stay lowercase (`view-card`,
`is-size-7`), so in a `class` attribute the case alone tells the two
namespaces apart: **uppercase = logical block name, lowercase = styling**.
Keep the existing Bulma/utility classes; **prepend** the logical name(s).

**Logical names are independent of whatever CSS class names each app uses.**
They form their own namespace: they identify blocks, they don't style them,
and they are tied to no CSS framework or app stylesheet (Bulma today, anything
else tomorrow). Each app keeps its own styling classes alongside them —
restyling or swapping the CSS layer never renames a logical class, and adding
a logical class never requires a CSS rule.

**Why:** a bare `<pre class="is-size-7 mb-2">` is unidentifiable in devtools —
you can't tell it's "the comment line". `CONSOLE_COMMENT` makes the DOM
self-describing. These are primarily debug aids, but they **may** double as
real CSS hooks — styling them is fine when useful (e.g. the `.CONSOLE_INPUT_ROW`
rules in `gui_agent/src/app.css`).

Canonical example: `yunos/js/gui_agent/src/c_agent_console.js` (the full
`CONSOLE_*` family).

### JS GUI conventions

- **EVERY action goes through the FSM — a button click IS an action.** This is
  the GUI face of the general rule above (*the OS boundary is where events are
  born*): in a browser the DOM **is** the operating system, so its callbacks —
  click, `onmessage`, `setTimeout`, `resize`, a resolved promise — are OS
  notifications and must be turned into Yuneta events. A DOM handler's only job
  is to **send an event** (`gobj_send_event(gobj,
  "EV_OPEN_CARD", {...}, gobj)`); the work belongs in the FSM action. Same for
  anything the view decides to do on its own (re-arm on reconnect, refresh,
  close). A view whose whole life happens in `ST_IDLE`, with DOM callbacks
  calling functions directly, throws away Yuneta's main debugging asset: the
  `machine` trace shows nothing, so failures can only be chased through
  WebSocket traffic and screenshots. Model the lifecycle as **states** too
  (e.g. "no topic loaded yet" is a state, not an `if(!priv.x) return` that
  silently no-ops a button), so an action arriving in the wrong state fails
  LOUDLY and names its sender. Cards/items do **not** need to be child gobjs —
  keeping them in the same gclass is fine; what matters is that every action
  crosses the automaton. Canonical example: `C_TRANGER_VIEW`
  (`yunos/js/gui_treedb`) — `ST_DISCONNECTED` → `ST_LOADING_TOPICS` →
  `ST_TOPIC_SELECTED`, with every click/`on_close`/dialog-confirm sent as an
  event. Widget plumbing that is not an action stays a plain call (Tabulator's
  `ajaxRequestFunc` must RETURN a Promise — it is a data source, not an event).
- **A `kw` must be plain JSON — never put a gobj, a widget or a DOM node in
  it.** The machine trace dumps the kw (`trace_json(kw)`), and those objects
  are circular: serializing one throws, so the first thing to break is the
  trace the FSM exists to feed. Pass an IDENTITY instead (`{key, mode}`, an
  id) and resolve it to the object inside the action — it also makes the trace
  line readable.
- **No transitions/animations.** Menus, popovers, tooltips and state changes
  appear instantly — no fade/slide/glide. If a third-party lib injects
  transition CSS, override it (`transition: none !important`). Don't add
  decorative animation unless explicitly requested.
- **Buttons in rows: icon required; icon-only on mobile.** Every button in a
  row that gets narrow on mobile carries an icon; on mobile hide the text
  label (`<span class="is-hidden-mobile">Label</span>`). Always set `title` +
  `aria-label` (mobile users see only the icon).
- **Bulma helpers carry `!important`** (`.is-flex`, `.is-hidden`): a bare
  inline `style.display = 'none'` loses to them. Toggle `is-hidden`, or use
  `style.setProperty('display', 'none', 'important')` /
  `removeProperty('display')`. jsdom tests don't load Bulma CSS and can't
  catch this — verify display toggles against the real stylesheet.
- **`yui_icons.css` (gobj-ui) is a small CSS-mask set, not FontAwesome** — an
  undefined `yi-*` class renders as a solid black square. Grep the real set
  before using an icon
  (`grep -oE '^\.yi-[a-z0-9-]+::before' src/yui_icons.css`); a missing glyph
  is added as a deliberate mask rule, never referenced on hope.

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

- **`set_expected_results()` / any `json_pack` that `test_json` will DECREF
  must run inside `register_yuno_and_more`**, not in `main()` before
  `yuneta_entry_point` — jansson is routed through `gbmem_*` only after the
  entry point calls `json_set_alloc_funcs`. Earlier allocations are
  libc-backed and die at cleanup (`dl_delete: DIFFERENT dl_list_t`,
  `free(): invalid pointer`). Canonical: `tests/c/c_task_authenticate/`.

- **No `static json_t *` caches in library helpers** — anything alive past
  `gobj_end()` is reported as a leak under `CONFIG_DEBUG_TRACK_MEMORY` and
  fails ctest. Parse/build per call for rare helpers; for hot paths hand the
  cache lifetime to the caller via incref/decref (see `topic_cols_desc` in
  `tr_treedb.c`). Trust ctest over a standalone run — ctest captures the
  `print_track_mem` errors.

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

**Every treedb is a graph**: topics = nodes, hook→fkey columns = links
(a self-referent hook = a tree). Docs, schema references and SPA views lead
with the graph; column tables are the per-node detail. Derive graph diagrams
from the schema source of truth, never hand-draw them.

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
- **Changing a topic's cols requires bumping its `topic_version`** (and
  `schema_version` for structural changes) — otherwise the persisted
  `topic_cols.json` masks the new in-memory schema and validation fires stale
  errors. When reproducing locally, wipe the treedb `store/` first or the fix
  won't take. Details:
  [`YUNO_TREEDB.md`](yunos/c/yuno_agent/YUNO_TREEDB.md).

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

### Verifying flows against a running system

Verify message flows against the **backend**, not a SPA (the SPA only
visualizes). Loop: mutate treedb from the CLI
(`command-yuno … service=<treedb_svc> command=update-node …` — treedb replaces
object fields wholesale, resend the full sub-object), inject stimulus over a
persistent connection, and assert on the timeranger2 topic with
`tr2list <store>/<treedb>/<topic> -l3 [--follow]`. Inspect live nodes via
commands (`get-node`, …), never by editing the append-only store on disk — the
running yuno caches treedb in memory and out-of-band writes corrupt the log.

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

Every yuno `main.c` carries a **commented-out block of trace toggles**
(`gobj_set_gclass_trace(...)`, `gobj_set_global_trace(...)` for the common
levels) after the `arguments.verbose` setup, so tracing is one uncomment away.
Keep/add it when touching a `main.c`; it is not dead code. Canonical:
`utils/c/ycommand/src/main.c`.

### 2. Monitor logs

```bash
ls -lt /yuneta/realms/<realm>/<yuno>/logs/                                # find active log
tail -f /yuneta/realms/<realm>/<yuno>/logs/<N>.log | grep -a "keyword"    # follow + filter
```

### Inspecting configuration

**A yuno's effective config is a merge**: `fixed_config` + `variable_config`
compiled into its `main.c`, deep-merged with the external JSON files (the
overrides layer). Services, `public_services` and gclass trees live only in
`main.c` and never appear in the external JSON. Verify any claim about a
running yuno with
`ycommand -c 'command-yuno id=<id> service=__yuno__ command=view-config'`,
never from the external JSON alone.

Two related gotchas:

- The agent's own configs (`/yuneta/agent/yuneta_agent{,22}.json`) are created
  only by the deb/rpm packagers from `packages/templates/*.json.sample`
  (conffiles — never overwritten on upgrade), and are merged on top of the
  agent's `main.c` defaults the same way.
- A `crypto` JSON attr supplied in a yuno/gate config **replaces** the gclass
  default wholesale (attr-level, no merge). Since TLS clients verify by
  default, any crypto override must repeat the verifying keys
  (`ssl_use_system_ca`, `ssl_verify_mode`, …) or the client silently loses
  them.

### Memory bugs

Debug memory bugs (bad decref, leaks, refcounts) **locally first** when
reproducible — faster cycle, `CONFIG_DEBUG_TRACK_MEMORY` is on, low blast
radius. The stack-trace block printed after a `gobj_log_error` carries line
numbers from the binary *as built* — map frames by **function symbol**, not
line number. Promote to remote nodes only after the local cycle is clean.

After deploying a modified yuno, **cycle it orderly at least once**
(`kill-yuno` + `run-yuno`): the gbmem audit only fires on clean shutdown
(`deactivate-snap` SIGKILLs and skips it). A leak logs *"system memory not
free"* at the end of the yuno's log; a clean shutdown emits nothing.

### 3. Deploy an updated binary

**Always go through the agent — never kill processes manually or overwrite a
running binary directly.**

#### Same-version hot-patch (debug rebuild, `APP_VERSION` unchanged)

```bash
# 1. Build
cd /yuneta/development/yunetas/yunos/c/<yuno>/build && make clean && make install

# 2. Kill the running yuno(s)
ycommand -c 'kill-yuno yuno_role=<role>'

# 3. Overwrite the same-version slot
ycommand -c 'update-binary id=<role> content64=$$(<role>)'

# 4. Verify size/date
ycommand -c 'list-binaries'

# 5. Start the yuno(s), then play them (two steps = one response each)
ycommand -c 'run-yuno play=0'
ycommand -c 'play-yuno'
```

`run-yuno play=0` launches the process(es) without the implicit auto-play, so
it returns a single aggregated response; `play-yuno` then starts the services
(also a single response). In a script this keeps each command in sync. Plain
`run-yuno` (auto-play) still works for interactive use but emits one extra
async answer per `must_play` yuno.

#### Version bump (`APP_VERSION` in `main.c` changed)

`kill-yuno` + `run-yuno` will re-launch the OLD release: the agent's
in-memory primary index for the `yunos` topic does not move when a
new `pkey2` row is appended. The canonical sequence is:

```bash
# 1. Build (APP_VERSION bumped in main.c)
cd /yuneta/development/yunetas/yunos/c/<yuno>/build && make clean && make install

# 2. Install into a NEW slot
ycommand -c 'install-binary id=<role> content64=$$(<role>)'

# 3. Register a yuno-instance row at the new role_version
ycommand -c 'find-new-yunos create=1'

# 4. Trigger restart_nodes() so the new pkey2 becomes primary
ycommand -c 'deactivate-snap'
```

`deactivate-snap` (no args, no active snap) is the supported way to
force the agent to SIGKILL every running yuno, promote the highest
`yuno_release` per id, reload the treedb, and bring everything back
on the new release. (The treedb primary is the highest-**ROWID**
record, not the highest version; `restart_nodes()` re-appends the
newest release first — `promote_highest_release_yunos()` — so the
reload promotes it reliably.) It is a node-wide bounce — use
`shoot-snap name=<tag>` first if you want a rollback point. Full
flow incl. rollback in
[`yunos/c/yuno_agent/YUNO_LIFECYCLE.md`](yunos/c/yuno_agent/YUNO_LIFECYCLE.md)
§6.5 / §6.6.

**CLI shortcut.** Steps 3–4 (plus the optional rollback snapshot) are
bundled by `yunetas upgrade-yunos`: it shoots a rollback snap (idempotent by
name, `--no-snap` to skip), previews `find-new-yunos` and asks before
`create=1`, then runs `deactivate-snap`. Steps 1–2 (build + `install-binary`,
or a `yunetas sync-binaries` push) still come first. Use it after the new
binaries are installed; the raw `ycommand` sequence above is the manual
equivalent.

#### Deploy conventions

- **Describe steps by what the agent does**, with the command in parens:
  `kill-yuno` is an **orderly shutdown** (drain + deregister), not a SIGKILL —
  "build → orderly shutdown (kill-yuno) → upload binary (update-binary) →
  verify (list-binaries) → start (run-yuno)".
- **Scope redeploys to actual consumers.** A change in a shared kernel lib
  does not mean bouncing every yuno that links it — identify which running
  yunos actually use the changed feature and propose that narrow set; a
  node-wide bounce of live services has real cost.
- **Security defaults cut over hard fail-closed** — no warn-then-enforce or
  migration windows; outdated peers are allowed to break loudly. On a noisy
  deploy, **snap-rollback first** (`shoot-snap`/`activate-snap`), analyze
  offline, redeploy.
- **Binary and config deploys travel together:** run `sync-configs` right
  after `sync-binaries` — new runtime + stale config has caused a real
  incident.
- **Batch configs** (`yunos/batches/<host>/<yuno>.json`): every edit bumps
  `__version__` and rewrites `__description__` as **that version's changelog
  entry** (what it changes + rollback caveat) — operators read it in
  `list-configs`; rollback is config-row reassignment, not file reverts.
- **The agent itself is a standalone daemon, not a managed yuno** —
  `kill-yuno` / `update-binary` / `run-yuno` do nothing to it. Deploy it with
  `yuneta_agent --config-file=<its json> --stop` then `--start` (it
  re-daemonizes on the new binary). SIGTERM is ignored by design. Every node
  runs `yuneta_agent` + `yuneta_agent22` (minimal escape hatch) as deliberate
  redundancy — each can upgrade the other; **never stop or upgrade both at
  once**.

### 4. Deactivate traces when done

Same commands with `set=0`. Response should be `[]` or only the permanently
configured ones.

```bash
ycommand -c 'command-yuno id=<id> service=__yuno__ command=set-gclass-trace gclass=<GClass> set=0 level=<level>'
ycommand -c 'command-yuno id=<id> service=__yuno__ command=set-global-trace level=<level> set=0'
```

## Release checklist

- **Tags have no `v` prefix.** Before creating any tag, run
  `git tag -l | grep <version>` (catches both prefixed and unprefixed forms);
  if anything matches, stop and ask — duplicate `7.x.y`/`v7.x.y` tags pointing
  at different commits is a serious error.
- **Pre-tag audit** (also on any "save" / "save and quit" request), checked
  against `git log <last-tag>..HEAD`:
  1. `CHANGELOG.md ## Unreleased` lists every non-docs/test change.
  2. READMEs of touched modules reflect behavior changes (simple isolated bug
     fixes are exempt; BREAKING changes never are).
  3. `docs/doc.yuneta.io/` mirrors them.
  4. `TODO.md` is pruned of items the commits resolved.
  5. READMEs/docs scanned for stale content (old versions, removed features,
     renamed APIs).
  Surface gaps as a punch list before committing/tagging.
- **Every release includes the live docs:** repin `blob/<old>/` / `tree/<old>/`
  deep links to the new tag across `docs/doc.yuneta.io/**` and
  `yunos/c/yuno_agent/*.md` (URL segment only — leave "since X" prose alone);
  clear the myst cache (a repin is a bulk edit); run
  `docs/doc.yuneta.io/deploy.sh` and curl-verify the live site. The release is
  not done until the live site reflects it.
- **GitHub release body from the CHANGELOG:** strip the 4-space bullet indent
  from the `## vX.Y.Z` section first (otherwise GitHub renders it as one code
  block). When backfilling an older release, restore
  `gh release edit <newest-tag> --latest` afterwards.

## Environment Variables

| Variable | Purpose |
|----------|---------|
| `YUNETAS_BASE` | Root of this repo (auto-set by `yunetas-env.sh`). `/yuneta/development/yunetas` is the same base on every node: `.deb`/`.rpm` runtime-only nodes get a sparse SDK there (`outputs/`, `outputs_ext/`, `tools/`, `.config` — no sources) |
| `YUNETAS_OUTPUTS` | `$YUNETAS_BASE/outputs` — build artefacts (include/lib/bin/yunos) |
| `YUNETAS_OUTPUTS_EXT` | `$YUNETAS_BASE/outputs_ext` — built external libraries |
| `YUNETAS_YUNOS` | `$YUNETAS_OUTPUTS/yunos/` — deployed yuno binaries |

## Useful Files

| File | Purpose |
|------|---------|
| `YUNETA_VERSION` | Current version (7.7.2) — used to generate `yuneta_version.h` |
| `Kconfig` | Root Kconfig definition |
| `TODO.md` | API renames/removals/additions between versions |
| `CHANGELOG.md` | Release history |
| `ctest-loop.sh` | Runs `ctest` in a loop until first failure |
