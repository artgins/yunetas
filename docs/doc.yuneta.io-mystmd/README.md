# mystmd migration — `doc.yuneta.io-mystmd/`

Full migration of `docs/doc.yuneta.io/` (the original Sphinx site) to
[mystmd / Jupyter Book 2](https://mystmd.org). This directory is
intended to replace the Sphinx site once validated.

## Why mystmd

- **Zero conversion cost**: mystmd reads MyST Markdown natively — the
  same format the old Sphinx site already uses. All `{tab-set}`,
  `{tab-item}`, `{list-table}`, `{dropdown}`, `{grid}`, `{card}`
  directives work as-is. The `.md` source files were copied verbatim.
- **Client-side navigation**: mystmd is a React/Remix SPA, so sidebar
  clicks no longer trigger full page reloads. No inter-page flash.
- **Modern theme family**: `book-theme` is the successor to
  `sphinx-book-theme` from the same Executable Books project, so the
  visual identity is preserved.
- **Native notebook support** via `{code-cell}` directive — a clean
  replacement for the old `myst_nb` Sphinx extension.
- **Single Node binary** (`mystmd`) instead of a Python environment
  with 15+ pip packages.

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
- All 818 MyST Markdown source files copied verbatim from
  `docs/doc.yuneta.io/` (verified with `diff -rq`).
- `_static/` assets copied.
- `myst.yml` written with a complete TOC mirroring the old Sphinx
  `{toctree}` structure 1:1.
- `index.md` hand-adapted: `{toctree}` blocks stripped (they now live
  in `myst.yml`).

### Done — Phase 2a: C module landing pages reconciled
- Each `api/*/api_*.md` module landing page rebuilt from the
  corresponding `kernel/c/*/README.md` (the authoritative up-to-date
  source). Obsolete pointers in the old landings are removed, new
  concepts from the READMEs are added.

### Done — Phase 2b: JavaScript API section (new)
- `api/js/` is **new content**. The old Sphinx site did not document
  the JavaScript framework at all. This section is generated from
  `kernel/js/gobj-js/README.md` (764 lines of authoritative API
  reference) and `kernel/js/lib-yui/README.md`.

### Pending — function-by-function C API reconciliation
- The ~700 individual C function reference pages (e.g.
  `api/helpers/istream/istream_create.md`) are copied verbatim from
  the old Sphinx site and have **not** been reconciled against the
  actual headers in `kernel/c/*/src/`. The C READMEs are overview
  documents, not function references — they cannot drive that
  reconciliation.
- Expect stale signatures, removed functions, and missing new
  functions. Each needs to be checked against the corresponding
  header (`gobj.h`, `yev_loop.h`, `timeranger2.h`, `ytls.h`, …).
- This is the large remaining task. It is deliberately out of scope
  for the migration commit so the structural work can be reviewed in
  isolation.

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
