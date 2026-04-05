# Quarto pilot — `doc.yuneta.io-quarto/`

This directory is a **pilot migration** of the Sphinx documentation at
`docs/doc.yuneta.io/` to [Quarto](https://quarto.org). It lives in parallel
with the original Sphinx site — **nothing in `doc.yuneta.io/` has been
modified**. The purpose is to evaluate Quarto as a possible successor to
`sphinx` + `sphinx-book-theme` + `myst_nb`, not to replace anything yet.

## What is included

| Area | Source | Notes |
|---|---|---|
| `index.qmd` | `doc.yuneta.io/index.md` | Hand-ported (MyST `{grid}` → Quarto `.grid`). `{toctree}` stripped — navigation now lives in `_quarto.yml`. |
| `philosophy/*.qmd` | `doc.yuneta.io/philosophy/*.md` | Pure prose, converted mechanically (no directives to translate). |
| `api/helpers/istream/*.qmd` | `doc.yuneta.io/api/helpers/istream/*.md` | 17 function reference pages. Converted mechanically by `tools/docs-migration/myst_to_quarto.py` — this directory is the **stress test for the script**. |
| `api/helpers/istream/index.qmd` | *new* | Section landing page written from scratch — Sphinx relied on `{toctree}` autogeneration which Quarto does not have. |
| `notebooks/demo_timeseries.qmd` | *new* | Executable Jupyter-backed page — validates the "notebooks are a future requirement" criterion. |

## What is **not** included (and why)

- **~800 other API reference pages**. The `istream` slice already exercises
  every MyST directive used throughout the codebase
  (`{tab-set}`, `{tab-item}`, `{list-table}`, `{dropdown}`, anchors,
  cross-refs). Once the pilot is approved, `myst_to_quarto.py` can be run
  over the whole tree.
- **`sphinx_thebe` interactive execution**. Quarto has no direct equivalent
  (it uses OJS / Shiny for live interactivity, a different model).
- **`ablog` blog posts**. Would need rewriting to Quarto's `listing:` format.
- **`intersphinx` cross-references** to Python stdlib. No equivalent in
  Quarto — would become regular external links.
- **`sphinx_design` cards/dropdowns/grids outside the index page**. The
  migration script handles `{dropdown}` but not `{grid}`/`{card}`; those
  appear in a handful of index pages and need manual review.

## How to build it

You need Quarto ≥ 1.4. Install from <https://quarto.org/docs/get-started/>.

```bash
cd docs/doc.yuneta.io-quarto
quarto render                  # generates HTML into _build/
quarto preview                 # live-reloading dev server
```

For the executable notebook you additionally need:

```bash
pip install jupyter matplotlib numpy
```

## Re-running the migration script

If you edit the source Sphinx docs and want to refresh the pilot:

```bash
# From repo root
python3 tools/docs-migration/myst_to_quarto.py --dir \
    docs/doc.yuneta.io/api/helpers/istream \
    docs/doc.yuneta.io-quarto/api/helpers/istream

python3 tools/docs-migration/myst_to_quarto.py --dir \
    docs/doc.yuneta.io/philosophy \
    docs/doc.yuneta.io-quarto/philosophy
```

The script is documented at `tools/docs-migration/myst_to_quarto.py` —
see the module docstring for the list of MyST constructs it handles.

## Findings from the pilot

1. **The repetitive API pages convert cleanly and mechanically.** All 17
   `istream` pages converted on a second attempt after two small regex
   fixes. The pattern is uniform across the ~800 API pages, so the same
   script should handle the rest with minimal extra work.
2. **Prose pages (`philosophy/`) are essentially free** — they are plain
   Markdown with no Sphinx-specific directives.
3. **Hand-written landing pages are unavoidable.** Sphinx generates section
   indexes from `{toctree}` directives; Quarto expects an explicit
   `_quarto.yml` sidebar and a manually written page per section.
   Estimated cost: ~20 index pages across the full site, each ~30 lines.
4. **`{grid}` / `{card}` constructs** need manual porting to Quarto's
   `.grid` / `.g-col-*` CSS classes. The migration script deliberately
   does **not** try to handle these — they are too variable.
5. **Notebooks are the strongest argument in favour of Quarto.** The
   `.qmd` format is simpler than the current `myst_nb` + `thebe`
   combination, has proper caching, and produces figures with
   `fig-cap` / `label` metadata out of the box.
6. **The visible loss** is `sphinx_thebe`'s "launch on Binder/Colab"
   buttons. If that feature is actively used, Quarto is not a drop-in
   replacement. If it's nice-to-have, Quarto is strictly simpler.

## Not a recommendation yet

This pilot is a **technical dry run**, not a recommendation. Look at the
rendered output, compare it visually and semantically with the live
`doc.yuneta.io` site, and decide whether the trade-offs make sense for
Yuneta's documentation needs.
