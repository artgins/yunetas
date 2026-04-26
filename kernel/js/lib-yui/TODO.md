Listo. 11 tareas en el TODO, ordenadas por dependencia (lo primero cierra el círculo de live-translate visualmente; lo último abre el PR). Cada una con criterio de "hecho" claro:

1. **Botón ES/EN en la test-app toolbar** — smoke visual de `yui_shell_set_translate`.
2. **Bump 7.4.0 + CHANGELOG** — empaquetar la entrega.
3. **Modales/notificaciones sobre los layers del shell** — porting de `display_*` y `get_yes*` desde `C_YUI_MAIN`, generalizando el focus-trap de drawer a cualquier overlay.
4. **`C_YUI_TABS` a `EV_ROUTE_CHANGED`** — primer consumidor migrado del routing antiguo.
5. **Inventario + migración del resto de consumidores** — `treedb_*`, `g6_nodes_tree`.
6. **`estadodelaire/gui` al shell** — primer test real con datos.
7. **Borrar `C_YUI_MAIN` y `C_YUI_ROUTING`** — cuando los `grep` estén vacíos; bump 7.5.0 (breaking).
8. **Generalizar el loop de secundarios** más allá de `menu.primary`.
9. **Cadena de prioridad de `Escape`** — pila modal > drawer > popup.
10. **e2e con Playwright** — booteo + dos presets + lifecycle + live-translate + focus-trap.
11. **PR + `/ultrareview`** — antes de mergear.

Si confirmas que el orden te encaja, puedo empezar por la #1 (botón ES/EN en la toolbar) — es el más visible y cierra el último work item del review original. Avísame.


# `lib-yui` shell — pending work

Living TODO for the declarative shell refactor on branch
`claude/refactor-gui-system-qptVn`.  Numbered roughly by dependency
order; do not skip ahead without reading the rationale on each item.

The arc this list closes:

> The original review scope (10 findings, 4 blockers) is **done**.  What
> remains is the migration plan documented in [`SHELL.md` §10](./SHELL.md)
> ("Status and retirement plan") plus the polish items spilled out of
> the iteration.  When this list is empty, `C_YUI_MAIN` and
> `C_YUI_ROUTING` can be deleted from `lib-yui`.

---

## 1. Test-app: language-switch button (smoke for `yui_shell_set_translate`)

The hot-swap helper is implemented and exported, but no consumer
exercises it.  Add a tiny visible loop:

- Add a 4th item to `app_config.json → toolbar.items` (`align: "end"`),
  e.g. `{ "id": "lang", "icon": "icon-translate", "name": "ES/EN",
  "action": { "type": "event", "event": "EV_TOGGLE_LANGUAGE" } }`.
- In `test-app/src/main.js` subscribe to `EV_TOGGLE_LANGUAGE` of the
  shell; on each fire, flip an in-memory boolean and call
  `yui_shell_set_translate(shell, fn)` with one of two trivial
  dictionaries (`"Dashboard" ↔ "Panel"`, `"Reports" ↔ "Informes"`,
  `"Settings" ↔ "Ajustes"`, etc.).
- After the click, the menu in `left`/`bottom`, the submenu in
  `top-sub`/`right`, the heading of the secondary nav (uses
  `nav_label`) and every toolbar `<button>` label should swap.
- Update `test-app/README.md` with a "Live i18n" section.

**Done when:** clicking the ES/EN button visibly relabels every menu,
heading and toolbar button, and the active item highlight survives the
re-publish of `EV_ROUTE_CHANGED`.

---

## 2. Bump `lib-yui` to `7.4.0` + top-level CHANGELOG entry

`kernel/js/lib-yui/package.json` is still on `7.3.0`.  Before any
publish:

- Bump to `7.4.0`.
- Add a section to top-level `CHANGELOG.md`:
    - **Added**: `C_YUI_SHELL`, `C_YUI_NAV`, `shell_show_on` parser with
      13 `node --test` tests, declarative toolbar, all 6 nav layouts,
      drawer overlay with focus-trap, hot-swap i18n via
      `yui_shell_set_translate`, public helpers
      `yui_shell_navigate / yui_shell_open_drawer /
      yui_shell_close_drawer / yui_shell_toggle_drawer /
      yui_shell_set_translate / yui_nav_rebuild`.
    - **Fixed (review pass)**: `build_item_index` route collision,
      drawer staying open after navigation, vite alias matching
      sub-paths, missing drawer helper re-exports, ugly
      `secondary.<id>` heading via the new `nav_label` attribute.
    - **Note**: `C_YUI_MAIN` and `C_YUI_ROUTING` are still shipped and
      not deprecated yet.  Migration is tracked in
      `kernel/js/lib-yui/TODO.md` and `kernel/js/lib-yui/SHELL.md` §10.

**Done when:** version bumped, CHANGELOG entry merged.

---

## 3. Modal / notification API on top of the shell layers

Today the shell creates `priv.layers.modal`, `priv.layers.notification`
and `priv.layers.loading` in `build_ui` but exposes no API for them.
The legacy entry points still live in `c_yui_main.js`:
`display_volatil_modal`, `display_info_message`, `display_warning_message`,
`display_error_message`, `get_yesnocancel`, `get_yesno`, `get_ok`.

- Port each helper to a shell-level function that paints into the
  corresponding layer.  Keep the same exported names (drop-in
  replacement) and re-export them from `index.js`.
- Reuse Bulma's `.modal` / `.notification` markup verbatim for visual
  parity.
- **Generalise** the drawer focus-trap (`activate_focus_trap` /
  `release_focus_trap` / `FOCUSABLE_SELECTOR`) into a generic
  `activate_focus_trap_on($element)` that any modal / drawer / popup
  can use.  Move it into `src/shell_focus_trap.js` for unit-testability
  alongside `shell_show_on.js`.
- Add a smoke entry in `test-app/app_config.json`: a toolbar item that
  fires `display_info_message("Hello")` so the toast renders without
  any view doing it.

**Done when:** every existing call to `display_*` from `c_yui_main.js`
can be served from the shell, and `c_yui_main.js` is no longer the
authority for modals/toasts.

---

## 4. Migrate `C_YUI_TABS` from `EV_ROUTING_CHANGED` to `EV_ROUTE_CHANGED`

`C_YUI_TABS.ac_show` / `ac_hide` listen to `EV_ROUTING_CHANGED` of
`C_YUI_ROUTING`.  Move that subscription to the shell's
`EV_ROUTE_CHANGED`:

- Add an attr `shell` (DTP_POINTER, optional) on `C_YUI_TABS`.
- In `mt_start`, if `shell` is set: `gobj_subscribe_event(shell,
  "EV_ROUTE_CHANGED", {}, gobj)`.  In `mt_stop`: unsubscribe.
- Keep the legacy `C_YUI_ROUTING` path as a fallback when `shell` is
  not set, so the migration can land before consumers are converted.
  Mark the fallback `// transitional` and link to this TODO.
- Add a small test-app stage that uses tabs to confirm.

**Done when:** the test-app + `estadodelaire/gui` (after task #6) work
without registering `C_YUI_ROUTING`.

---

## 5. Inventory and migrate the remaining `C_YUI_ROUTING` / `C_YUI_MAIN` consumers

```
grep -r register_c_yui_main      kernel/ utils/ yunos/
grep -r register_c_yui_routing   kernel/ utils/ yunos/
grep -rl EV_ROUTING_CHANGED      kernel/ utils/ yunos/
grep -rl display_(error|info|warning|volatil) kernel/ utils/ yunos/
```

For every hit that is not the migration itself, open a sub-task here.
Likely consumers inside `lib-yui`:

- `c_yui_treedb_topics.js`
- `c_yui_treedb_topic_with_form.js`
- `c_yui_treedb_graph.js`
- `c_g6_nodes_tree.js`
- `c_yui_form.js` (uses `display_*`)
- `yui_dev.js` (developer panel)

Each consumer:

- Replace `EV_ROUTING_CHANGED` subscription with `EV_ROUTE_CHANGED`
  (via the shell, see task #4).
- Replace `display_*` calls with the shell-helper version (task #3).
- Drop the dependency on the old gclasses from its imports.

**Done when:** the two `grep`s above return empty inside `lib-yui`
itself.


## 6. Migrate `estadodelaire/gui` to the shell — first real-world test

The companion repo `artgins/estadodelaire` is the canonical app on top
of `lib-yui`.  Replace its bootstrap:

- `gui/src/main.js`: drop `register_c_yui_main / register_c_yui_routing`,
  add `register_c_yui_shell / register_c_yui_nav` and
  `register_c_test_view`-style registrations for every `c_ui_*` gclass.
- `gui/src/c_yuneta_gui.js`: replace its custom shell wiring with a
  declarative `app_config.json` next to it.  Each `c_ui_*` gclass
  becomes a `target.gclass` with the appropriate `lifecycle`:
    - `c_ui_alarms` → `keep_alive` (subscribes to live data).
    - `c_ui_device_sonda` / `c_ui_device_termod` → `keep_alive` per
      device id.
    - `c_ui_historical_chart` → `lazy_destroy` (heavy, infrequent).
    - `c_ui_monitoring` / `c_ui_monitoring_group` → `keep_alive`.
    - `c_ui_todo` / `c_yui_gobj_tree_js` → `lazy_destroy`.
- Verify on real screen sizes:
    - Mobile (<769): primary as `icon-bar` at `bottom`, submenu as `tabs`
      at `top-sub`, drawer for the user/account menu.
    - Tablet (769-1023): same primary placement + toolbar appears at
      `top`.
    - Desktop (≥1024): primary at `left` (vertical), submenu at `right`
      (vertical), full toolbar.
- Confirm the live language switch using existing locales/flags
  (`gui/src/locales/`).
- Capture any feature gap (a layout the current GUI does that the new
  shell doesn't) as a follow-up here before continuing.

**Done when:** `cd gui && npm run dev` boots EstadoDelAire on the new
shell and the smoke flows (alarms, monitoring, history) work.

---

## 7. Delete `C_YUI_MAIN` and `C_YUI_ROUTING` from `lib-yui`

Gated on tasks #3, #4, #5 and #6.  The retirement checklist
documented in `SHELL.md` §10 must be empty:

```
grep -r register_c_yui_main   kernel/ utils/ yunos/   # empty
grep -r register_c_yui_routing kernel/ utils/ yunos/   # empty
```

Then:

- Delete `src/c_yui_main.js`, `src/c_yui_main.css`, `src/c_yui_routing.js`,
  `src/c_yui_routing.css`.
- Remove their exports from `index.js`.
- Drop `import "@yuneta/lib-yui/src/c_yui_main.css"` /
  `c_yui_routing.css` from any remaining `main.js` (test-app should
  not need them).
- Update `SHELL.md` §11 to remove the "Do not import `c_yui_main.css`
  and `c_yui_shell.css` together" item (and renumber).  Update
  `SHELL.md` §10 to mark the retirement plan as completed.
- Update `README.md` of `lib-yui` to drop the `C_YUI_MAIN` /
  `C_YUI_ROUTING` mentions.
- Bump `lib-yui` to `7.5.0` (breaking change).  Add CHANGELOG entry
  with the removal and link to this TODO.

**Done when:** `lib-yui` no longer ships either gclass and no consumer
inside the org references them.

## 8. Generalise the secondary-nav loop beyond `menu.primary`

`instantiate_menus()` in `c_yui_shell.js` walks only
`menus.primary.items[*].submenu` to build level-2 navs.  See
[`SHELL.md` §11.1](./SHELL.md).

- Walk every menu mounted via a zone host of the form `"menu.<id>"`
  whose items declare a `submenu`, not just `primary`.  The
  `secondary.<item.id>` synthesised id stays unique because item ids
  are scoped to their parent menu — but if collisions across menus
  become a concern, switch to `secondary.<menu_id>.<item.id>`.
- Update `SHELL.md` §11 to drop limitation #1 (and renumber).
- Add a regression case to the test-app: a second primary-style menu
  (e.g. `menu.admin`) with its own submenus, and verify the right
  secondary nav surfaces under the right primary.

**Done when:** the test-app has at least two primary-style menus with
submenus and both render their secondary navs correctly.

## 9. Escape priority chain (modal > drawer > popup)

The shell's global `keydown` listener unconditionally closes any open
drawer on `Escape`.  Once modals (task #3) and any future popup
component land, this becomes wrong.

- Refactor into a stack `priv.escape_stack`: an array of close
  handlers.  Each interactive overlay pushes its handler when it
  opens and pops it when it closes.  Escape calls the top handler
  only.
- Order of priority by `z-index` of the layer the overlay lives in:
  `loading` (no Escape) > `modal` > `popup` > `overlay` (drawer).
- Document the order in `SHELL.md`.

**Done when:** with a modal open over a drawer, Escape closes the
modal first; second Escape closes the drawer.

## 10. End-to-end smoke with Playwright

Today there is **no automated smoke** of the shell — only the 13
`shell_show_on` parser tests run in CI.  Add a thin Playwright suite:

- Boot the test-app via `vite preview` against the built bundle.
- Two presets, two browsers (chromium + firefox at minimum).
- Tests:
    - **boot**: `/dash/ov` renders, the card title is "Overview".
    - **navigation**: clicking each primary item changes the active
      highlight and the centre stage.
    - **breakpoint**: viewport at 800px shows the bottom icon-bar; at
      1280px shows the left vertical menu.
    - **drawer**: hamburger opens drawer, focus moves into the panel,
      `Tab` cycles inside, `Escape` closes and restores focus to the
      burger button.
    - **lifecycle**: visiting `/dash/alerts` (`lazy_destroy`) twice
      increments the `instance #` counter; `/dash/ov` (`keep_alive`)
      does not.
    - **live-translate**: clicking the ES/EN button (task #1) flips
      labels everywhere.
- Wire `npm run test:e2e` from `kernel/js/lib-yui/`.
- Add a CI workflow stub (`.github/workflows/lib-yui.yml`) that runs
  `npm test` (parser) + `npm run test:e2e` on PRs touching
  `kernel/js/lib-yui/**`.

**Done when:** CI green on the branch and the workflow gates the PR.

## 11. Open PR + run `/ultrareview` before merging to `main`

Convention: feature branches are independently reviewed before they
land.

- Open the PR `claude/refactor-gui-system-qptVn → main` once tasks
  #1–#10 are at a place where merging is desirable (does not require
  every task to be done — at minimum #1 + #2 close the original
  review scope).
- The user runs `/ultrareview <PR#>` for a multi-agent independent
  review.
- Triage findings into either: (a) immediate fixes on the same PR;
  (b) follow-ups appended to this TODO.
- Squash-merge with the PR description summarising the entire
  thread.
- Delete the feature branch after merge.

**Done when:** the PR is merged and the branch is deleted.
