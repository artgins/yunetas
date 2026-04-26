# C_YUI_SHELL test app

Standalone harness to exercise `C_YUI_SHELL` + `C_YUI_NAV` in isolation
(no backend, no auth, no treedb).

## Run

```bash
cd kernel/js/lib-yui/test-app
npm install
npm run dev
```

Then open http://localhost:5180 (Vite auto-opens it).

## Presets

The harness ships with two declarative configs. Pick one via URL:

| URL                                 | Preset                | Layouts exercised                          |
|-------------------------------------|-----------------------|--------------------------------------------|
| `http://localhost:5180/`            | default               | `vertical` · `icon-bar` · `tabs` · `submenu` · `drawer` |
| `http://localhost:5180/?preset=accordion` | accordion         | `accordion` · `icon-bar` · `submenu` · `drawer`        |

Between both presets every one of the six supported layouts runs at least once.

## What to look at (default preset)

- **Resize the viewport** across Bulma's breakpoints:
  - `<769` (mobile): primary menu at `bottom` as an icon-bar, submenu as
    tabs at `top-sub`, toolbar hidden.
  - `769–1023` (tablet): same mobile primary/submenu, toolbar appears at `top`.
  - `>=1024` (desktop+): primary menu moves to `left` (vertical `.menu`),
    submenu becomes a Bulma-like heading list in `right` (layout `submenu`).
- **Navigate** via menu clicks or edit the URL hash directly
  (`#/dash/ov`, `#/reports/monthly`, `#/settings`, `#/help`). Back/forward
  buttons work.
- **Lifecycle matrix**:
  - `Overview`, `Devices`  — `keep_alive` (instance number stays the same).
  - `Alerts`               — `lazy_destroy` (new instance on each visit).
  - `Help`                 — `eager` (instance #1 exists before you first
    visit `#/help`; confirm by clicking the `Help` toolbar button *last*).
- **Drawer**: the hamburger icon in the toolbar opens a full-screen
  off-canvas quick menu. Click the backdrop or press `Escape` to close it.
- **Toolbar** is fully declarative — `burger` triggers a drawer, `home`
  navigates, `help` navigates and sits on the right (`align: "end"`).
- **Live i18n**: the `ES/EN` button on the right of the toolbar fires
  `EV_TOGGLE_LANGUAGE` (a custom user event); a tiny side controller
  (`C_TEST_LANG`) listens for it, flips between two trivial
  dictionaries and calls `refresh_language(shell.$container, t)` from
  `@yuneta/gobj-js`. Same flow `c_yui_main.js` uses in its
  `change_language()`. The shell renders every translatable label
  with `data-i18n="<canonical key>"` (via `createElement2`'s `i18n`
  attribute), so a single DOM walk swaps every menu label, the
  secondary-nav heading and every toolbar `<button>` label without
  rebuilding any DOM. The active item highlight is preserved
  trivially because nothing is re-rendered.

## What to look at (accordion preset)

Open `?preset=accordion`:

- The `left` zone on desktop is an `accordion` menu rooted at top-level
  items (`Dashboard`, `Reports`). Click a heading to expand/collapse; the
  heading containing the active leaf auto-expands when you navigate, and
  `aria-expanded` / `aria-controls` update accordingly.
- Keyboard: `Tab` walks the items, `Enter` / `Space` activates them, and
  `:focus-visible` outlines the active control.

## Accessibility spot-checks

- Every nav root is `role="navigation"` with an `aria-label` matching the
  menu id. The drawer wrapper is `role="dialog" aria-modal="true"`, its
  panel is `role="navigation"`.
- Accordion heads are real `<button>` elements with `aria-expanded` /
  `aria-controls`; bodies are `role="region"` labelled by their head.
- Disabled items get `aria-disabled="true"` and `tabindex="-1"`.
- Focus outlines are driven by `:focus-visible`, so clicks don't show an
  outline, only keyboard navigation does.

## Files

- `src/main.js` — registers gclasses, chooses a preset, boots the
  shell.
- `src/app_config.json` — default preset (vertical · icon-bar · tabs ·
  submenu · drawer + toolbar + eager lifecycle).
- `src/app_config_accordion.json` — alternate preset (accordion +
  submenu + drawer).
- `src/c_test_view.js` — minimal view gobj exposing `$container`;
  used as the `target.gclass` for every menu item.
- `src/c_test_lang.js` — small CHILD gobj that subscribes to the
  shell's `EV_TOGGLE_LANGUAGE` and calls `refresh_language` with a
  Spanish or English dictionary. Canonical pattern for any app that
  needs to react to custom toolbar events.
