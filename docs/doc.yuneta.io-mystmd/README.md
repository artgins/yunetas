# mystmd migration — `doc.yuneta.io-mystmd/`

Full migration of `docs/doc.yuneta.io/` (the original Sphinx site) to
[mystmd / Jupyter Book 2](https://mystmd.org). This directory is
intended to replace the Sphinx site once validated.

## Why mystmd

- **Zero conversion cost**: mystmd reads MyST Markdown natively — the
  same format the old Sphinx site already uses.
- **Client-side navigation**: mystmd is a React/Remix SPA, so sidebar
  clicks no longer trigger full page reloads. No inter-page flash.
- **Modern theme family**: `book-theme` is the successor to
  `sphinx-book-theme` from the same Executable Books project, so the
  visual identity is preserved.
- **Native notebook support** via `{code-cell}` directive — a clean
  replacement for the old `myst_nb` Sphinx extension.
- **Single Node binary** (`mystmd`) instead of a Python environment
  with 15+ pip packages.
- **Fast incremental builds**: ~79 pages built in ~3s clean, thanks
  to the module-per-page layout (one landing page with every function
  of the module anchored as `(funcname)=`).

## How to build

```bash
npm install -g mystmd
cd docs/doc.yuneta.io-mystmd

# Live-reloading dev server (usually http://localhost:3000)
myst start

# Static HTML build
myst build --html
```

For any executable `{code-cell}` pages you additionally need a Python
kernel and whatever libraries those cells import.

## Migration scope and status

### Done — Phase 1: structural migration
- MyST Markdown source files copied verbatim from `docs/doc.yuneta.io/`.
- `_static/` assets copied.
- `myst.yml` written with a complete TOC mirroring the old Sphinx
  `{toctree}` structure.
- `index.md` hand-adapted: `{toctree}` blocks stripped (they now live
  in `myst.yml`).

### Done — Phase 2a: C module landing pages reconciled
- Each `api/*/api_*.md` module landing page rebuilt from the
  corresponding `kernel/c/*/README.md` (the authoritative up-to-date
  source).

### Done — Phase 2b: JavaScript API section (new)
- `api/js/` is **new content**. The old Sphinx site did not document
  the JavaScript framework at all. This section is generated from
  `kernel/js/gobj-js/README.md` and `kernel/js/lib-yui/README.md`.

### Done — Phase 3: warnings + strict-mode errors
- `myst build` and `myst build --strict` both produce zero warnings
  and zero errors. Fixes ranged from the `myst.yml` author schema to
  hundreds of anchor/link target mismatches (`(func())=` → `(func)=`),
  empty-target C type references (`hgobj`, `json_t`, …), legacy
  unresolved cross-refs, broken external URLs, and the glossary being
  rewritten as plain Markdown headings (the `{glossary}` directive
  rejected embedded code fences).

### Done — Phase 4: consolidation
- The ~740 one-function-per-file pages (`api/helpers/istream/istream_create.md`,
  etc.) were merged into 42 module landing pages. Every function
  keeps its `(funcname)=` anchor, so every internal reference resolves
  identically; only the URL changes from
  `api/helpers/istream/istream_create.html` to
  `api/helpers/istream.html#istream_create`.
- `list-table` parameter blocks → Markdown pipe tables (cheaper parse,
  easier to edit).
- Empty JS/Python tab-set stubs and `// TODO` Examples dropdowns
  dropped wholesale.
- Build time dropped from ~32s to ~3s (clean) on 829 → 79 pages.

### Done — Phase 5: API coverage verifier
- `tools/docs-migration/verify_api_coverage.py` compares every
  `PUBLIC` function declared in the kernel C headers against the
  `(funcname)=` anchors present in the mystmd landing pages. It
  reports per-header MISSING (exported but not documented) and EXTRA
  (documented but not exported) symbols.
- The initial run found 9 real gaps (7 `ydaemon.h` functions, 2
  `yev_loop.h` measure-time helpers); they are now fixed.
- Current state: **23 headers, 784 PUBLIC functions, 784 documented,
  0 gaps, 0 stale entries.**
- CI enforcement: `.github/workflows/docs-api-coverage.yml` runs the
  verifier on every push / PR that touches kernel headers, mystmd
  sources or the script itself, and fails the build on any issue.
- Local run: `python3 tools/docs-migration/verify_api_coverage.py`.

### Pending — function-by-function C API signature/description reconciliation
- The verifier only checks presence. It does **not** validate that
  parameter types, return types or descriptions actually match the
  current headers. Expect stale signatures on some pages.
- A stronger check would parse the headers with tree-sitter or
  libclang and compare parameter lists. Not yet implemented.
- The reconciliation itself is cheaper now: it is a per-module edit
  against one landing page, not 700 individual files.

## Coexistence with the old Sphinx site

The original Sphinx site at `docs/doc.yuneta.io/` is **not deleted**
by this migration. Both stacks coexist until the mystmd build has
been visually verified and the C API reconciliation is complete.
Deletion of the Sphinx site is a separate commit, explicitly
requested.

## Trade-offs lost vs. the Sphinx site

- `sphinx_thebe` "launch on Binder/Colab" buttons — no direct
  equivalent. Dropped.
- `ablog` blog integration — mystmd has basic "posts" support, but
  it's less mature. Not used in this repo.
- `intersphinx` cross-references to external Python docs — mystmd has
  its own cross-project reference system, but it's not a 1:1 drop-in.
- Some custom Sphinx roles used in a handful of pages
  (`:danger:`, `:warning:`) — they render as plain text in mystmd.
  Can be replaced with standard admonitions if needed.
