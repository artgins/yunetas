# yunos

Deployable Yuneta services (yunos). Each sub-directory is a self-contained process built on top of the kernel in `kernel/`.

## Layout

- **`c/`** — C yunos (agents, brokers, gateways, utilities). See `yunos/c/README.md` for the catalogue.
- **`js/`** — JavaScript yunos and web applications. Currently: `gui_treedb` — the browser UI for the TreeDB graph database.

## Building

All C yunos are built by `yunetas build` and installed into `$YUNETAS_YUNOS/` (i.e. `$YUNETAS_OUTPUTS/yunos/`). JS yunos are built with their own `npm run build` inside each sub-directory.

See the top-level `CLAUDE.md` for the full build workflow.
