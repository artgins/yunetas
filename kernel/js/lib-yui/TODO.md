# `lib-yui` shell — pending work

Living TODO for the declarative shell.  Everything originally on
this list (the new shell + nav, escape stack, modal/notification
API, generalised secondary-nav loop, validator, Playwright e2e on
three browsers) is **done** and currently riding on PR #105.

`CHANGELOG.md` carries the full feature list under `## Unreleased`.

---

## 1. Cut the `7.4.0` release

`kernel/js/lib-yui/package.json` is still at `7.3.0`.  Once PR #105
merges and we want to publish to npm:

- Bump `kernel/js/lib-yui/package.json` to `7.4.0`.
- Move the `## Unreleased` block in the top-level `CHANGELOG.md` to
  `## v7.4.0 -- <date>`.
- `npm publish --access public` from `kernel/js/lib-yui/` (the
  `prepublishOnly` script runs `vite build` automatically).

**Done when:** `@yuneta/lib-yui@7.4.0` is on npmjs.com and the
CHANGELOG has the dated section instead of `## Unreleased`.

---

## 2. (Optional, deferred) Migrate legacy GUIs off `C_YUI_MAIN` / `C_YUI_ROUTING`

> **Status: not planned.**  Captured here as a reference checklist
> for whoever decides to take it on later.  Until that decision is
> made, `C_YUI_MAIN` and `C_YUI_ROUTING` stay shipped and supported.
> Do **not** start any sub-task below without an explicit go-ahead.

The work needed if the decision is reversed:

### 2.1. Migrate `C_YUI_TABS` from `EV_ROUTING_CHANGED` to `EV_ROUTE_CHANGED`

`C_YUI_TABS.ac_show` / `ac_hide` listen to `EV_ROUTING_CHANGED` of
`C_YUI_ROUTING`.  Move that subscription to the shell's
`EV_ROUTE_CHANGED`:

- Add an attr `shell` (DTP_POINTER, optional) on `C_YUI_TABS`.
- In `mt_start`, if `shell` is set: `gobj_subscribe_event(shell,
  "EV_ROUTE_CHANGED", {}, gobj)`.  In `mt_stop`: unsubscribe.
- Keep the legacy `C_YUI_ROUTING` path as a fallback when `shell` is
  not set, so the migration can land before consumers are converted.
- Add a small test-app stage that uses tabs to confirm.

### 2.2. Inventory the remaining `C_YUI_ROUTING` / `C_YUI_MAIN` consumers

```
grep -r register_c_yui_main      kernel/ utils/ yunos/
grep -r register_c_yui_routing   kernel/ utils/ yunos/
grep -rl EV_ROUTING_CHANGED      kernel/ utils/ yunos/
grep -rlE 'display_(error|info|warning|volatil)|get_yes|get_ok' kernel/ utils/ yunos/
```

Likely consumers inside `lib-yui`:

- `c_yui_treedb_topics.js`
- `c_yui_treedb_topic_with_form.js`
- `c_yui_treedb_graph.js`
- `c_g6_nodes_tree.js`
- `c_yui_form.js` (uses `display_*`)
- `yui_dev.js` (developer panel)

For each consumer:

- Replace `EV_ROUTING_CHANGED` subscription with `EV_ROUTE_CHANGED`
  via the shell (see 2.1).
- Replace `display_*` / `get_yes*` calls with the shell-helper
  version (`yui_shell_show_*` / `yui_shell_confirm_*`).
- Drop the dependency on the old gclasses from imports.

### 2.3. Migrate `estadodelaire/gui` to the shell — first real-world test

The companion repo `artgins/estadodelaire` is the canonical app on
top of `lib-yui`.  Replace its bootstrap:

- `gui/src/main.js`: drop `register_c_yui_main / register_c_yui_routing`,
  add `register_c_yui_shell / register_c_yui_nav` and registrations
  for every `c_ui_*` gclass.
- `gui/src/c_yuneta_gui.js`: replace its custom shell wiring with a
  declarative `app_config.json` next to it.  Each `c_ui_*` gclass
  becomes a `target.gclass` with the appropriate `lifecycle`:
    - `c_ui_alarms` → `keep_alive`.
    - `c_ui_device_sonda` / `c_ui_device_termod` → `keep_alive` per
      device id.
    - `c_ui_historical_chart` → `lazy_destroy`.
    - `c_ui_monitoring` / `c_ui_monitoring_group` → `keep_alive`.
    - `c_ui_todo` / `c_yui_gobj_tree_js` → `lazy_destroy`.
- **Verify CSS class scope.**  `c_yui_shell.css` defines generic
  selectors (`.yui-toolbar`, `.yui-stage > *`, `.yui-zone-center > *`,
  `.yui-nav-iconbar .yui-nav-item.is-active`, etc.).  Confirm none
  collide with classes used by the `c_ui_*` views.  If they do, add
  the `.yui-shell` ancestor selector to scope shell rules.
- Verify on real screen sizes (mobile / tablet / desktop) per the
  layout matrix in `SHELL.md`.
- Confirm the live language switch using existing locales/flags
  (`gui/src/locales/`).
- **If a feature gap appears**, capture it as a follow-up here
  **before continuing**.  Do not patch around it ad-hoc inside
  `gui/`.

### 2.4. Delete `C_YUI_MAIN` and `C_YUI_ROUTING` from `lib-yui`

Gated on 2.1, 2.2, 2.3.  All four greps above must return empty.

- Delete `src/c_yui_main.js`, `src/c_yui_main.css`,
  `src/c_yui_routing.js`, `src/c_yui_routing.css`.
- Remove their exports from `index.js`.
- Drop `import "@yuneta/lib-yui/src/c_yui_main.css"` /
  `c_yui_routing.css` from any remaining `main.js`.
- Update `SHELL.md` §11 to remove the "Do not import `c_yui_main.css`
  and `c_yui_shell.css` together" item (and renumber).
- Update `README.md` of `lib-yui` to drop the `C_YUI_MAIN` /
  `C_YUI_ROUTING` mentions.
- Bump `lib-yui` to `8.0.0` (breaking change).  Add CHANGELOG entry
  with the removal and link to this TODO.

**Done when (2 as a whole):** `lib-yui` no longer ships either
gclass and no consumer inside the org references them.

---

## Acknowledged debt (not blocking anything)

- **Focus-trap unit tests use hand-rolled DOM stubs.**  Stubs cover
  every branch of the trap (Tab / Shift+Tab / non-Tab / focus-from-
  outside / release idempotency / missing panel / empty panel / LIFO
  stacking).  Switch to `jsdom` or `happy-dom` only if a real edge
  case appears (Shadow DOM, `inert`, native `<dialog>`, `tabindex`
  derived from CSS, etc.).
