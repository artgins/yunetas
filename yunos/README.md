# yunos

The deployable Yuneta services (yunos). Each subdirectory is a separate process. It runs on the kernel in `kernel/`.

## Layout

- `c/`: the C yunos (agents, brokers, gateways, utilities). For the catalog, see `yunos/c/README.md`.
- `js/`: the JavaScript yunos and web applications, a git submodule (`github.com/artgins/yunos-js`). It holds `gui_agent`, the browser UI of the agent, and `gui_treedb`, the browser UI for the TreeDB graph database.

## Building

`yunetas build` builds all the C yunos. It installs them into `$YUNETAS_YUNOS/` (that is, `$YUNETAS_OUTPUTS/yunos/`). To build a JS yuno, run `npm run build` in its subdirectory.

For the full build workflow, see the top-level `CLAUDE.md`.
