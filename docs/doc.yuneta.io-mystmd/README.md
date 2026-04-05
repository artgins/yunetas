# mystmd pilot — `doc.yuneta.io-mystmd/`

This directory is a **second pilot migration** of the Sphinx documentation
at `docs/doc.yuneta.io/`, this time to
[mystmd (Jupyter Book 2)](https://mystmd.org). It lives in parallel with:

- the original Sphinx site at `docs/doc.yuneta.io/`
- the earlier Quarto pilot at `docs/doc.yuneta.io-quarto/`

Nothing in either of those is modified. All three can coexist so their
rendered output can be compared side by side.

## Why mystmd is different from the Quarto pilot

**mystmd reads MyST Markdown natively — the exact format already used by
the existing Sphinx site.** That means:

| Concern | Quarto pilot | mystmd pilot |
|---|---|---|
| `.md` source files | Converted to `.qmd` by a script | **Copied verbatim** |
| `{tab-set}` / `{tab-item}` | Rewritten to `::: {.panel-tabset}` | **Works as-is** |
| `{list-table}` | Rewritten to pipe tables | **Works as-is** |
| `{dropdown}` | Rewritten to `.callout-note collapse` | **Works as-is** |
| `(label())=` anchors | Rewritten to `{#sec-label}` | **Works as-is** |
| `{toctree}` | Stripped; replaced by `_quarto.yml` sidebar | Stripped; replaced by `myst.yml` TOC |
| `{grid}` / `{card}` | Rewritten to Quarto `.grid/.g-col-*` | **Works as-is** |
| Migration script needed | Yes (`tools/docs-migration/myst_to_quarto.py`) | **No** |
| Inter-page navigation | Full page reload → flash | **Client-side SPA → no flash** |

The two files inside `philosophy/` and the seventeen files inside
`api/helpers/istream/` in this directory are **byte-identical** to the
Sphinx sources they came from. You can verify with `diff -r`.

## What is included

- `myst.yml` — project config (title, authors, TOC, theme).
- `index.md` — hand-written landing page (uses `{grid}` + `{grid-item-card}` natively).
- `philosophy/*.md` — copied verbatim from the Sphinx site.
- `api/helpers/istream/*.md` — 17 reference pages copied verbatim from the Sphinx site.
- `api/helpers/istream/index.md` — section landing page (new, ~40 lines).
- `notebooks/demo_timeseries.md` — executable page using the `{code-cell}` directive.
- `_static/` — logos copied from the Sphinx site.

## What is **not** included

Same omissions as the Quarto pilot: the ~800 other API pages are not
copied (but since the copy is verbatim, adding them is a trivial
`cp -r` away — no conversion script to run).

## How to build it

You need Node.js ≥ 18 and `mystmd`:

```bash
npm install -g mystmd
cd docs/doc.yuneta.io-mystmd

# Live-reloading dev server (usually at http://localhost:3000)
myst start

# Static HTML build
myst build --html
```

For the executable notebook you additionally need:

```bash
pip install jupyter matplotlib numpy
```

## Expected findings

1. **No migration cost** for the MyST Markdown files. The whole
   `doc.yuneta.io/` tree should render under mystmd with no changes;
   only the navigation needs to be expressed in `myst.yml` instead of
   per-page `{toctree}` directives.
2. **No inter-page flash.** mystmd is a single-page-application built on
   React/Remix: clicking a sidebar link triggers a client-side route
   change, not a full document reload. This is the feature that
   motivated the second pilot.
3. **Same visual family** as the current Sphinx site (`book-theme` is
   the successor of `sphinx-book-theme`), so the "look & feel" transition
   cost is near zero.
4. **Executable notebooks** work via the `{code-cell}` directive — same
   syntax as `myst_nb`, which is what the Sphinx site uses today.

## Trade-offs vs. Sphinx

What you lose moving to mystmd:

- `sphinx.ext.autodoc` / Python autodoc (unused in this repo anyway).
- `ablog` blog integration (mystmd has basic "posts" support but it's
  less mature).
- `intersphinx` cross-references to Python stdlib (mystmd has its own
  cross-project references; not a 1:1 drop-in).
- `sphinxcontrib.bibtex` — mystmd has native bibtex support, but the
  extension surface is smaller.
- Some custom sphinx extensions (`sphinx_thebe`, `sphinxext.opengraph`,
  `sphinxcontrib.youtube`) — mystmd has equivalents for most of these
  or they're not needed.

What you gain:

- **Single Node binary** (`mystmd`) instead of a Python env with
  15+ pip packages.
- **Client-side navigation** (no inter-page flash).
- **Modern tooling**: hot reload, live preview, cross-reference validation.
- **Same source files work in both stacks during the transition** —
  you can migrate incrementally.
- **PDF / JATS / LaTeX** output built in.

## Not a recommendation yet

Like the Quarto pilot, this is a technical dry run. Render both with
`myst start` / `quarto preview`, compare the UX and the look, and
decide which stack (if any) wins.
