# `lib-yui` shell — pending work

Living TODO for the declarative shell refactor on branch
`claude/refactor-gui-system-qptVn`.  Numbered roughly by dependency
order; do not skip ahead without reading the rationale on each item.

**Scope decision (2026-04):** existing GUIs will **not** be migrated
to the new shell.  `C_YUI_MAIN` and `C_YUI_ROUTING` stay shipped and
supported so legacy apps keep working unchanged.  The new shell
(`C_YUI_SHELL` + `C_YUI_NAV`) targets **new** GUIs only.  The
migration of legacy apps is captured as an optional, gated track at
the end of this file (#8) for whoever decides to take it on later.

The arc this list closes:

> The original review scope (10 findings, 4 blockers) is **done**.
> What remains is the polish needed to make the new shell a
> first-class option for new GUIs (modals, escape stack, secondary-
> nav generalisation, e2e smoke), plus the release cut on top of
> which new apps can ride.  Legacy `C_YUI_MAIN` / `C_YUI_ROUTING`
> are explicitly out of scope here.

Order rationale (revised after the "no legacy migration" decision):

- #1 + #2 close the original review scope and let us cut a `7.4.0`
  release that new GUIs can adopt.
- #3 (Escape stack) lands **before** #4 (modals) on purpose: any
  modal/popup component will register itself on the stack, retro-
  fitting it after the fact is much more painful.
- #4 (Modal/notification API) makes the new shell self-sufficient
  for new GUIs — they don't need to import `c_yui_main.js` for
  toasts/dialogs.
- #5 (secondary-nav generalisation) removes the `menu.primary`-only
  limitation so new GUIs with multiple primary-style menus work.
- #6 (Playwright e2e) gates regressions on the shell itself.
- #7 closes the branch.
- #8 is the **optional** legacy-migration track, kept here as a
  reference checklist if the decision is ever revisited.

---

## 1. Test-app: language-switch button (smoke for `refresh_language`)

**Status: DONE** (canonical pattern).

The shell tags every translatable text node with
`data-i18n="<canonical key>"` (via `createElement2`'s `i18n` attr).
Apps switch language by calling
`refresh_language(shell.$container, t)` from `@yuneta/gobj-js` —
the same flow `c_yui_main.js` uses in its `change_language()`.
There is no shell-level translate hook and no DOM rebuild.

The test-app exercises this with:
- A `lang` toolbar item (`align: "end"`) that fires the user-defined
  `EV_TOGGLE_LANGUAGE` via `action.type:"event"`.  The shell is
  registered with `gcflag_no_check_output_events` so it forwards the
  event without the app having to extend its `event_types` table.
- A small CHILD gobj `C_TEST_LANG` that subscribes to that event and
  calls `refresh_language(shell.$container, t)` with one of two
  trivial dictionaries (EN identity / ES map).

---

## 2. Bump `lib-yui` to `7.4.0` + top-level CHANGELOG entry

`kernel/js/lib-yui/package.json` is still on `7.3.0`.  Before any
publish:

- Bump to `7.4.0`.
- Add a section to top-level `CHANGELOG.md`:
    - **Added**: `C_YUI_SHELL`, `C_YUI_NAV`, `shell_show_on` parser
      with 13 `node --test` tests, declarative toolbar, all 6 nav
      layouts, drawer overlay with focus-trap, canonical i18n
      (every translatable label rendered with
      `data-i18n="<canonical key>"`; apps switch language by calling
      `refresh_language(shell.$container, t)` from
      `@yuneta/gobj-js`), public helpers
      `yui_shell_navigate / yui_shell_open_drawer /
      yui_shell_close_drawer / yui_shell_toggle_drawer`.
    - **Fixed (review pass)**: `build_item_index` route collision,
      drawer staying open after navigation, vite alias matching
      sub-paths, missing drawer helper re-exports, ugly
      `secondary.<id>` heading via the new `nav_label` attribute.
    - **Note**: `C_YUI_MAIN` and `C_YUI_ROUTING` remain shipped and
      fully supported.  The new shell is the recommended path for
      **new** GUIs only; legacy GUIs keep using the old classes.
      Optional migration tasks tracked in `TODO.md` §8.

**Done when:** version bumped, CHANGELOG entry merged.

---

## 3. Escape priority chain (modal > drawer > popup)

The shell's global `keydown` listener unconditionally closes any
open drawer on `Escape`.  Once modals (#4) and any future popup
component land, this becomes wrong — closing the modal first,
*then* the drawer, *then* the popup is the universal expectation.

**Land this BEFORE #4** so modals are born already aware of the
stack.

- Refactor `priv.keydown_handler` into a stack `priv.escape_stack`:
  an array of close handlers.  Each interactive overlay pushes its
  handler when it opens and pops it when it closes.  Escape calls
  the **top** handler only and consumes the event.
- Order of priority by `z-index` of the layer the overlay lives in:
  `loading` (no Escape) > `modal` > `popup` > `overlay` (drawer).
- Refactor `open_drawer` / `close_drawer` / `toggle_drawer` to use
  the stack instead of calling `close_all_drawers` directly.
- Document the order in `SHELL.md` §11 and add a regression test in
  the e2e suite once #6 is up.

**Done when:** with a modal open over a drawer, Escape closes the
modal first; second Escape closes the drawer.

---

## 4. Modal / notification API on top of the shell layers

Today the shell creates `priv.layers.modal`, `priv.layers.notification`
and `priv.layers.loading` in `build_ui` but exposes no API for them.
New GUIs built on the shell should be able to render modals and
toasts **without** importing `c_yui_main.js`.

The legacy entry points (`display_volatil_modal`, `display_info_message`,
`display_warning_message`, `display_error_message`, `get_yesnocancel`,
`get_yesno`, `get_ok`) stay in `c_yui_main.js` for legacy apps and
are **not** removed.  The new shell exposes a parallel API instead.

- **Generalise** the drawer focus-trap (`activate_focus_trap` /
  `release_focus_trap` / `FOCUSABLE_SELECTOR`) into a generic
  `activate_focus_trap_on($element)` and move it to
  `src/shell_focus_trap.js` for unit-testability alongside
  `shell_show_on.js`.  Add a small `tests/shell_focus_trap.test.mjs`
  using happy-dom (or a tiny stub) so the runner gets its second
  client.
- Add shell-level helpers — distinct names from the legacy ones to
  avoid ambiguity for consumers that might import both.  Naming
  convention: `yui_shell_show_*` for non-blocking notifications
  (no answer expected from the user), `yui_shell_confirm_*` for
  blocking dialogs (the helper resolves with the user's choice).
    - Non-blocking: `yui_shell_show_modal`, `yui_shell_show_info`,
      `yui_shell_show_warning`, `yui_shell_show_error`.
    - Blocking: `yui_shell_confirm_yesnocancel`,
      `yui_shell_confirm_yesno`, `yui_shell_confirm_ok` (single OK
      button — still a dialog the user has to dismiss, hence
      `confirm_*`, not `show_*`).
  Re-export from `index.js`.
- Reuse Bulma's `.modal` / `.notification` markup verbatim for visual
  parity with the legacy implementation.
- Each modal/popup that grabs focus must register a close handler on
  the Escape stack from #3.
- Add a smoke entry in `test-app/app_config.json`: a toolbar item
  that fires `yui_shell_show_info(shell, "Hello")` so the toast
  renders without any view doing it.

**Drift policy** between the new helpers and the legacy `display_*`
/ `get_yes*` / `get_ok` is documented in
[`SHELL.md` §10](./SHELL.md): legacy bug fixes are not back-ported,
shell improvements stay on the shell.

**Done when:** new GUIs can render every modal/toast variant through
shell helpers without importing `c_yui_main.js`; the legacy helpers
remain untouched.

---

## 5. Generalise the secondary-nav loop beyond `menu.primary`

`instantiate_menus()` in `c_yui_shell.js` walks only
`menus.primary.items[*].submenu` to build level-2 navs.  See
[`SHELL.md` §11.1](./SHELL.md).

- Walk every menu mounted via a zone host of the form `"menu.<id>"`
  whose items declare a `submenu`, not just `primary`.  The
  `secondary.<item.id>` synthesised id stays unique because item
  ids are scoped to their parent menu — but if collisions across
  menus become a concern, switch to
  `secondary.<menu_id>.<item.id>`.
- Update `SHELL.md` §11 to drop limitation #1 (and renumber).
- Add a regression case to the test-app: a second primary-style
  menu (e.g. `menu.admin`) with its own submenus, and verify the
  right secondary nav surfaces under the right primary.

**Done when:** the test-app has at least two primary-style menus
with submenus and both render their secondary navs correctly.

---

## 6. End-to-end smoke with Playwright

Today there is **no automated smoke** of the shell — only the 13
`shell_show_on` parser tests run in CI.  Add a thin Playwright
suite.

- Boot the test-app via `vite preview` against the built bundle.
- Two presets, two browsers (chromium + firefox at minimum).
- Tests, in order of priority:
    1. **boot**: `/dash/ov` renders, the card title is "Overview".
    2. **navigation**: clicking each primary item changes the
       active highlight and the centre stage.
    3. **breakpoint**: viewport at 800 px shows the bottom
       icon-bar; at 1280 px shows the left vertical menu.
    4. **drawer**: hamburger opens drawer, focus moves into the
       panel, `Tab` cycles inside, `Escape` closes and restores
       focus to the burger button.
    5. **escape stack** (after #3): modal over drawer, Escape
       closes the modal first, then the drawer.
    6. **lifecycle**: visiting `/dash/alerts` (`lazy_destroy`)
       twice increments the `instance #` counter; `/dash/ov`
       (`keep_alive`) does not.
    7. **live-translate**: clicking the ES/EN button (#1) flips
       labels everywhere.
- Wire `npm run test:e2e` from `kernel/js/lib-yui/`.
- Add a CI workflow stub (`.github/workflows/lib-yui.yml`) that
  runs `npm test` (parser) + `npm run test:e2e` on PRs touching
  `kernel/js/lib-yui/**`.

**Done when:** CI green on the branch and the workflow gates the
PR.

---

## 7. Open PR + run `/ultrareview` before merging to `main`

Convention: feature branches are independently reviewed before
they land.

- Open the PR `claude/refactor-gui-system-qptVn → main` once tasks
  #1–#6 are at a place where merging is desirable (does not
  require every task to be done — at minimum #1 + #2 close the
  original review scope).
- The user runs `/ultrareview <PR#>` for a multi-agent independent
  review.
- Triage findings into either: (a) immediate fixes on the same PR;
  (b) follow-ups appended to this TODO.
- Squash-merge with the PR description summarising the entire
  thread.
- Delete the feature branch after merge.

**Done when:** the PR is merged and the branch is deleted.

---

## 8. (Optional, deferred) Migrate legacy GUIs off `C_YUI_MAIN` / `C_YUI_ROUTING`

> **Status: not planned.**  Captured here as a reference checklist
> for whoever decides to take it on later.  Until that decision is
> made, `C_YUI_MAIN` and `C_YUI_ROUTING` stay shipped and supported.
> Do **not** start any sub-task below without an explicit go-ahead.

The work needed if the decision is reversed:

### 8.1. Migrate `C_YUI_TABS` from `EV_ROUTING_CHANGED` to `EV_ROUTE_CHANGED`

`C_YUI_TABS.ac_show` / `ac_hide` listen to `EV_ROUTING_CHANGED` of
`C_YUI_ROUTING`.  Move that subscription to the shell's
`EV_ROUTE_CHANGED`:

- Add an attr `shell` (DTP_POINTER, optional) on `C_YUI_TABS`.
- In `mt_start`, if `shell` is set: `gobj_subscribe_event(shell,
  "EV_ROUTE_CHANGED", {}, gobj)`.  In `mt_stop`: unsubscribe.
- Keep the legacy `C_YUI_ROUTING` path as a fallback when `shell` is
  not set, so the migration can land before consumers are converted.
- Add a small test-app stage that uses tabs to confirm.

### 8.2. Inventory the remaining `C_YUI_ROUTING` / `C_YUI_MAIN` consumers

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
  via the shell (see 8.1).
- Replace `display_*` / `get_yes*` calls with the shell-helper
  version (#4: `yui_shell_show_*` / `yui_shell_confirm_*`).
- Drop the dependency on the old gclasses from imports.

### 8.3. Migrate `estadodelaire/gui` to the shell — first real-world test

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

### 8.4. Delete `C_YUI_MAIN` and `C_YUI_ROUTING` from `lib-yui`

Gated on 8.1, 8.2, 8.3.  All four greps above must return empty.

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

**Done when (8 as a whole):** `lib-yui` no longer ships either
gclass and no consumer inside the org references them.
