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

## What to look at

- **Resize the viewport** across Bulma's breakpoints:
  - `<769` (mobile): primary menu at `bottom` as an icon-bar, submenu as
    tabs at `top-sub`.
  - `769-1023` (tablet): same mobile layout (top toolbar appears).
  - `>=1024` (desktop+): primary menu moves to `left` as vertical .menu,
    submenu becomes a vertical list in `right`.
- **Navigate** via menu clicks or edit the URL hash directly
  (`#/dash/ov`, `#/reports/monthly`, `#/settings`). Back/forward buttons
  work.
- **Lifecycle demo**: the `Alerts` item under Dashboard is declared
  `lazy_destroy` - its instance number in the card changes each time you
  visit it.  `Overview` and `Devices` are `keep_alive` - their instance
  numbers stay the same across visits.

## Files

- `src/main.js` - registers gclasses, boots the shell.
- `src/app_config.json` - the declarative config (layers aren't declared
  explicitly; default set is used).
- `src/c_test_view.js` - a minimal view gobj exposing `$container`;
  used as the `target.gclass` for every menu item.
