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

## 3. Escape priority chain (modal > drawer > popup) — DONE

`priv.escape_stack` is a LIFO of `{ layer, handler }` records
pushed by every overlay when it opens and popped when it closes.
The window-level `keydown` listener calls only the top entry and
consumes the event (preventDefault + stopPropagation).

- `open_drawer` / `close_drawer` / `toggle_drawer` /
  `close_all_drawers` go through `open_drawer_one` /
  `close_drawer_one`, which keep DOM (`is-active`), focus-trap,
  and escape-stack in sync.
- The backdrop click in `c_yui_nav.js` no longer mutates the DOM
  directly; it publishes the new `EV_DRAWER_CLOSE_REQUESTED`
  output event with `{menu_id}`. The shell's
  `ac_drawer_close_requested` calls the canonical close path —
  closing the focus-trap leak that was open since the drawer
  landed.
- Public API for non-drawer overlays (used by #4 modals and any
  future popup): `yui_shell_push_escape(shell, layer, handler)` /
  `yui_shell_pop_escape(shell, handler)`. Both re-exported from
  `@yuneta/lib-yui`.
- `SHELL.md` §11 documents the LIFO ordering, the z-index
  rationale (loading > modal > popup > overlay), and the public
  API.
- `tests-e2e/drawer.spec.mjs` covers: burger opens / Escape
  closes / focus restored; backdrop click closes via the
  canonical path; Escape with the stack empty is a no-op.

The cross-overlay test (modal over drawer → first Escape closes
the modal, second Escape closes the drawer) lands with #4 once
modals exist.

---

## 4. Modal / notification API on top of the shell layers — DONE

Two new modules, one inline refactor, seven public helpers.

### Generic focus-trap → `src/shell_focus_trap.js`
- `FOCUSABLE_SELECTOR` constant + `activate_focus_trap_on($panel,
  doc?)` returning a `release()` function. Pure module, no Yuneta
  deps. The optional `doc` arg lets unit tests inject a stub.
- The drawer's inline focus-trap in `c_yui_shell.js` is gone;
  `open_drawer_one` now calls `activate_focus_trap_on(panel)` and
  parks the returned release fn on `$drawer.__yui_focus_release__`.
  `close_drawer_one` invokes it. The `priv.drawer_focus_states` Map
  is gone too.
- `tests/shell_focus_trap.test.mjs`: 10 unit tests with hand-rolled
  DOM stubs (no extra devDep). Brings the parser+focus-trap unit
  total to 23/23.

### Shell-level helpers → `src/shell_modals.js`
Naming convention: `yui_shell_show_*` for non-blocking,
`yui_shell_confirm_*` for blocking dialogs that resolve a Promise.
- **Non-blocking**: `yui_shell_show_info`, `yui_shell_show_warning`,
  `yui_shell_show_error`, `yui_shell_show_modal`. Toasts auto-
  dismiss after `opts.timeout` ms (default 5000; `0` disables).
  Each returns a `{ close() }` handle.
- **Blocking**: `yui_shell_confirm_ok`, `yui_shell_confirm_yesno`,
  `yui_shell_confirm_yesnocancel`. Each returns a Promise that
  resolves with the user's choice (`undefined` / boolean /
  `"yes"|"no"|"cancel"` respectively). Escape, the close button
  and click on the background all resolve with the LAST button's
  value (cancel/no/ok — safe-default convention).
- Every modal/dialog automatically pushes a close handler onto the
  Escape priority chain (§11) and installs a focus-trap on the
  modal card. `Tab`/`Shift+Tab` cycle inside, focus is restored on
  close.
- All seven re-exported from `@yuneta/lib-yui`.

### Smoke entries in the test-app
`app_config.json` toolbar grew two items: `Hello` (fires
`EV_SHOW_HELLO` → `yui_shell_show_info`) and `Ask` (fires
`EV_ASK_QUESTION` → `yui_shell_confirm_yesno` + a follow-up info
toast). The `c_test_lang` controller picked up the two new
actions/events. No new gobj.

### e2e (`tests-e2e/modals.spec.mjs`)
Four tests × 2 browsers = 8 new e2e tests:
- Hello toolbar item paints a `.notification` on the notification
  layer; clicking the `.delete` button dismisses.
- Ask toolbar item opens a yes/no dialog; clicking *Yes* resolves
  the Promise and shows the follow-up toast.
- Escape on an open dialog dismisses with the last button's value
  (= the *No* answer in this case).
- **Modal over drawer**: open the drawer, then open a modal on top
  of it (synthetic click bypasses the drawer's pointer occlusion);
  first Escape closes the modal, the drawer remains open; second
  Escape closes the drawer. This is the cross-overlay test that
  was deferred from #3.

### Drift policy
Documented in `SHELL.md §10`: legacy `display_*` / `get_yes*` /
`get_ok` bug fixes are NOT back-ported to `yui_shell_show_*` /
`yui_shell_confirm_*` and vice versa. The two stacks are
parallel.

Tests: parser + focus-trap 23/23, e2e 26/26 (chromium + firefox).

---

## 5. Generalise the secondary-nav loop beyond `menu.primary` — DONE

`instantiate_menus` now walks every menu mounted via a `"menu.<id>"`
host whose items declare a `submenu` with its own `render`. The
synthesised secondary `menu_id` is
`secondary.<owning_menu_id>.<item.id>`, scoped to the owning menu so
two primary-style menus can share item ids without colliding.

`update_secondary_nav_visibility` reads `entry.menu_id` (the route
owner) and computes `target_secondary_id =
secondary.<entry.menu_id>.<active_primary_id>`. The matching nav
shows; every other secondary hides. Cross-menu duplicates of the
same item id are independent.

The previous limitation in `SHELL.md` §11.1 has been removed; §6
gained a small "Secondary nav `menu_id`" subsection documenting the
synthesised id.

A new preset `app_config_multimenu.json` declares two primary-style
menus side-by-side (`menu.primary` in `left/bottom`, `menu.admin` in
`right/bottom-sub`) with the same item id (`dash`) in each; the e2e
test `tests-e2e/multimenu.spec.mjs` boots that preset and verifies:

- Both primary navs are rendered, each with its own `aria-label`.
- The shell auto-instantiates one secondary nav per (menu × item
  with submenu.render).
- On the default route (`/dash/ov`, owned by `menu.primary`),
  `menu.primary`'s secondary nav is visible; `menu.admin`'s is
  hidden.
- Navigating to `/admin/dash/users` flips the visibility — strictly
  by `menu_id`, not by the duplicated `dash` item id.
- The active secondary's items belong to the right tree and the
  other tree's routes are absent.

Tests: parser + focus-trap 23/23, e2e 28/28 (chromium + firefox).

---

## 6. End-to-end smoke with Playwright

Today the only automated coverage is the 13 `shell_show_on`
parser tests — Playwright wraps the actual UI.

### Done in the first pass
- `playwright.config.js` boots the test-app via `vite preview`
  against the built bundle on port 5181 (chromium + firefox,
  `webServer` auto-starts before tests).
- `npm run test:e2e` / `npm run test:e2e:ui` / `npm run test:all`
  wired up.
- `.github/workflows/lib-yui.yml` runs unit + e2e on PRs and on
  `main` touching `kernel/js/lib-yui/**` or `kernel/js/gobj-js/**`.
  Uploads the Playwright HTML report on failure.
- `tests-e2e/boot.spec.mjs`:
    * default preset lands on `/dash/ov` with the Overview card,
      the URL hash mirrors the route, the primary nav has its
      active item, and the boot is console-error-clean.
    * accordion preset auto-expands the Dashboard branch
      (`aria-expanded="true"`) and lands on Overview.
- `tests-e2e/navigation.spec.mjs`:
    * clicks each primary item, asserts the URL hash and the
      centre-stage card title.
    * typed-URL hash navigation lands on the right card.
    * back/forward replays the route history.
    * submenu container `/dash` redirects to `/dash/ov`.

### Pending — to be added as the corresponding feature lands
- **breakpoint**: viewport at 800 px shows the bottom icon-bar;
  at 1280 px shows the left vertical menu.
- **drawer**: hamburger opens drawer, focus moves into the panel,
  `Tab` cycles inside, `Escape` closes and restores focus to the
  burger button.
- **escape stack** (after #3): modal over drawer, Escape closes
  the modal first, then the drawer.
- **modal** (after #4): toolbar item triggers
  `yui_shell_show_info`; the toast is in the notification layer.
- **lifecycle**: visiting `/dash/alerts` (`lazy_destroy`) twice
  increments the `instance #` counter; `/dash/ov` (`keep_alive`)
  does not.
- **live-translate**: clicking the ES/EN button flips labels
  everywhere; the active highlight is preserved.

**Done when:** CI green on the branch with all bullets above.

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
