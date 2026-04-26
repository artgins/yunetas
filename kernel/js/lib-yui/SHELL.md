# `C_YUI_SHELL` + `C_YUI_NAV` — Declarative shell

Application-level presentation and navigation system for Yuneta GUIs.
Replaces the `C_YUI_MAIN` + `C_YUI_ROUTING` pair (both still exist and
can coexist) with a couple of GClasses driven by a JSON document in the
Yuneta configuration style.

- Built with **Vite** (same as the rest of `lib-yui`).
- Backed by **Bulma** (`.menu`, `.tabs`, `.level`, `.navbar`,
  `is-hidden-*` helpers). No JS framework is introduced; everything is
  DOM + GObj.
- Designed to be folded into `libyui` once validation is complete.

---

## 1. Goals

1. **Declarative**: the screen split and the menu tree are described in
   a JSON document, not in imperative JavaScript.
2. **Per-zone responsive**: be able to say *"the primary menu lives in
   `left` on desktop and in `bottom` on mobile"* without duplicating the
   menu definition or breaking the panels' internal state.
3. **Two-level navigation**: primary options + sub-options, mapped to
   hash routes (`#/primary/secondary`).
4. **Pluggable per-zone rendering**: the same menu option must be able
   to render differently depending on where it lands (vertical icon +
   label in `left`; icon-over-label in `bottom`; horizontal tabs in
   `top-sub`; vertical list in `right`; etc.).
5. **Each view is a gobj with its own `$container`**: the shell does
   not know what is inside — it just mounts it, shows it, and hides
   it. Navigating means *"show a gobj in its zone and hide the one
   that was there"*.
6. **Per-option lifecycle**: `eager` / `keep_alive` / `lazy_destroy`,
   to balance the cost of rebuilding against RAM usage.
7. **No regressions on `lib-yui`**: existing components (`C_YUI_MAIN`,
   `C_YUI_ROUTING`, `C_YUI_TABS`, etc.) are left untouched.

---

## 2. Model

Three orthogonal concepts:

- **Layer** — Z-stacked plane (full-screen). Defined planes:
  - `base` — main layout (zones).
  - `overlay` — off-canvas drawer, large dropdowns.
  - `popup` — tooltips / context menus.
  - `modal` — blocking dialogs.
  - `notification` — toasts.
  - `loading` — global spinner.
- **Zone** — region inside `base`. There are 7 fixed zones laid out on
  a CSS grid:
  ```
  +------------------- top --------------------+
  +----------------- top-sub ------------------+
  | left | ............ center .......| right |
  +---------------- bottom-sub ----------------+
  +------------------ bottom ------------------+
  ```
  A zone can *host* (`host`) a menu, a toolbar, or a *stage*. It is
  hidden per breakpoint via the `show_on` operator.
- **Stage** — zone marked as a container of routed gobjs. The most
  common one is `main` mounted on `center`. There can be several
  (e.g. a `right` configured as a stage for an independent detail
  panel).

### What goes in each zone

| Zone         | Typical use                                            |
|--------------|--------------------------------------------------------|
| `top`        | fixed toolbar (logo, theme, language, user)            |
| `top-sub`    | submenu rendered as `tabs` on mobile                   |
| `left`       | primary menu rendered as `vertical` on desktop         |
| `center`     | primary **stage**: active gobj                         |
| `right`      | submenu as `vertical` on desktop, or a secondary stage |
| `bottom-sub` | secondary toolbar or tabs on mobile                    |
| `bottom`     | primary menu rendered as `icon-bar` on mobile          |

---

## 3. Configuration JSON

Minimal schema:

```json
{
  "shell": {
    "zones":  { ... how zones are used ...   },
    "stages": { ... which stages exist ...   }
  },
  "menu": {
    "primary": { "render": { ... }, "items": [ ... ] }
  },
  "toolbar": { "zone": "top", "items": [ ... ] }
}
```

### 3.1 `shell.zones`

Each zone may declare:

- `host`: which content it receives. Values:
  - `"menu.<id>"` — render that menu in this zone.
  - `"stage.<name>"` — the zone is a stage for routed gobjs.
  - `"toolbar"` — the zone hosts the toolbar defined under `toolbar`.
- `show_on`: Bulma breakpoint expression. Accepted forms:
  - `"mobile"`, `"tablet"`, `"desktop"`, `"widescreen"`, `"fullhd"`
  - `">=desktop"`, `"<tablet"`, `"<=tablet"`, `">mobile"`, `">fullhd"` (→ ∅)
  - Combinable with `|`: `"mobile|tablet"`, `">=desktop|mobile"`

The shell translates the expression into a set of **custom CSS classes**
that hide the zone per breakpoint. Bulma only ships "up-to" helpers
(`is-hidden-tablet`, `is-hidden-desktop`) — to be able to say *"hidden
only on tablet"* `lib-yui` adds these classes in `c_yui_shell.css`:

| Class                          | Hidden when                 |
|--------------------------------|-----------------------------|
| `.yui-hidden-mobile`           | `<769 px`                   |
| `.yui-hidden-tablet-only`      | `769–1023 px`               |
| `.yui-hidden-desktop-only`     | `1024–1215 px`              |
| `.yui-hidden-widescreen-only`  | `1216–1407 px`              |
| `.yui-hidden-fullhd`           | `≥1408 px`                  |

No classes outside this table are expected in `show_on`. The parser is
pure and is covered by `tests/shell_show_on.test.mjs` (`npm test` in
`lib-yui/`).

Example:

```json
"zones": {
  "top":     { "host": "toolbar",        "show_on": ">=tablet" },
  "left":    { "host": "menu.primary",   "show_on": ">=desktop" },
  "bottom":  { "host": "menu.primary",   "show_on": "<desktop"  },
  "top-sub": { "host": "menu.secondary", "show_on": "<desktop"  },
  "right":   { "host": "menu.secondary", "show_on": ">=desktop" },
  "center":  { "host": "stage.main" }
}
```

> The same primary-menu option is instantiated twice (in `left` and in
> `bottom`) and shown/hidden via CSS. This avoids moving DOM nodes when
> crossing breakpoints, which would break their internal state.

### 3.2 `shell.stages`

```json
"stages": {
  "main": { "zone": "center", "default_route": "/dash/ov" }
}
```

- `zone` — host zone (must exist).
- `default_route` — initial route when the hash is empty.

A stage is inferred automatically when a zone declares
`"host": "stage.<name>"`, so `shell.stages` may be omitted when there
is only one main stage named `main` in `center`.

### 3.3 `menu.<id>`

```json
"primary": {
  "render": {
    "left":   { "layout": "vertical", "icon_pos": "left", "show_label": true },
    "bottom": { "layout": "icon-bar", "icon_pos": "top",  "show_label": true }
  },
  "items": [ ... ]
}
```

- `render[zone]` — how the menu is rendered when it appears in `zone`.
  - `layout`: `vertical` | `icon-bar` | `tabs` | `drawer` | `submenu` |
    `accordion`.
  - `icon_pos`: `left` | `right` | `top` | `bottom`.
  - `show_label`: boolean.
  - Shortcut: in `submenu.render` the bare layout string (`"tabs"`) is
    accepted instead of an object.
- `items[]` — options.

### 3.4 `toolbar`

```json
"toolbar": {
  "zone": "top",
  "aria_label": "App toolbar",
  "items": [
    { "id": "burger", "icon": "icon-menu",
      "aria_label": "Open menu",
      "action": { "type": "drawer", "op": "toggle", "menu_id": "quick" } },
    { "id": "home",   "icon": "icon-home",  "name": "Home",
      "action": { "type": "navigate", "route": "/dash/ov" } },
    { "id": "user",   "icon": "icon-user",  "align": "end",
      "action": { "type": "event", "event": "EV_OPEN_USER_MENU" } }
  ]
}
```

- `zone` — host zone. Defaults to the first zone in `shell.zones` that
  declares `"host": "toolbar"`.
- `items[].action.type`:
  - `"navigate"` → `{ route }` — delegated to the shell (respects
    `use_hash`).
  - `"drawer"`   → `{ op: "toggle" | "open" | "close", menu_id? }` —
    opens/closes the nav with `layout:"drawer"` whose `menu_id`
    matches.
  - `"event"`    → `{ event, kw? }` — `gobj_publish_event` from the
    shell.
- `items[].align`: `"start"` (default) or `"end"` (right-align).
- `aria_label` per item is used as the `<button>`'s `aria-label`.

### 3.5 `items[]` — option structure

```json
{
  "id": "ov",
  "name": "Overview",
  "icon": "icon-eye",
  "route": "/dash/ov",
  "disabled": false,
  "badge": "3",
  "target": {
    "stage": "main",
    "gclass": "C_DASHBOARD_OVERVIEW",
    "kw": { "refresh_ms": 5000 },
    "lifecycle": "keep_alive"
  },
  "submenu": {
    "render": { "top-sub": "tabs", "right": "vertical" },
    "default": "/dash/ov/overview",
    "items": [ ... ]
  }
}
```

- `route` — hash route to associate.
- `target` — what to show when the route is activated:
  - `gclass` — GClass to instantiate.
  - `kw` — initial attributes.
  - `name` — gobj name (optional, derived from the route).
  - `gobj` — alternative: reuse a preexisting gobj by name.
  - `stage` — where to mount it (defaults to `main`).
  - `lifecycle`: `eager` | `keep_alive` | `lazy_destroy`.
- `submenu` — when declared, the item becomes a *container*: its bare
  route redirects to the first sub-item (or to `submenu.default`).
  Sub-items declare their own `target`.

---

## 4. Lifecycle

When a route is activated, the shell:

1. Looks the entry up in its `route → { item, parent_item, target,
   stage }` index.
2. Hides the previous gobj's `$container` in that stage (`is-hidden`).
3. If the previous item had `lifecycle: "lazy_destroy"`, destroys it.
4. If the new one does not yet exist, creates it with `target.gclass`
   + `target.kw`, appends it to the stage's DOM, and starts it.
5. Removes `is-hidden` from the new one, publishes `EV_ROUTE_CHANGED`,
   and synchronises the hash.

`lifecycle` modes:

| Mode            | Created     | On exit                      | Use case                                       |
|-----------------|-------------|------------------------------|------------------------------------------------|
| `keep_alive`    | 1st visit   | hidden (state preserved)     | heavy views or views with half-filled forms    |
| `lazy_destroy`  | 1st visit   | destroyed                    | occasional views, avoids accumulating RAM      |
| `eager`         | at startup  | hidden                       | views that must "be there" from the beginning  |

Recommended default: `keep_alive`.

---

## 5. Navigation

- **Two-level hash routing**: the shell installs a `hashchange`
  listener. Any click on `C_YUI_NAV` changes `window.location.hash`
  and the shell reacts.
- **Programmatic**: `yui_shell_navigate(shell_gobj, "/dash/ov")`.
- **Outgoing event**: `EV_ROUTE_CHANGED` (with `route`, `item`,
  `parent_item`, `stage`). Navs consume it to mark the active item;
  views may subscribe to react.

---

## 6. GClasses

### `C_YUI_SHELL`

| Attribute        | Type        | Description                                              |
|------------------|-------------|----------------------------------------------------------|
| `config`         | JSON        | The JSON document described above                        |
| `default_route`  | string      | Fallback when the hash is empty and no `stages.*.default_route` is set |
| `current_route`  | string      | Read-only: active route                                  |
| `use_hash`       | bool        | If `true`, syncs `window.location.hash`                  |
| `mount_element`  | HTMLElement | Where to mount the shell (default `document.body`)       |
| `translate`      | function    | `(key) => string` — optional i18n; applied to `item.name`, toolbar labels, and `aria-label` |
| `$container`     | HTMLElement | Shell root                                               |

Published events:
- `EV_ROUTE_CHANGED` — `{ route, item, parent_item, stage }`.

Public helpers (import from `@yuneta/lib-yui`):
- `yui_shell_navigate(shell, route)` — programmatic navigation.
- `yui_shell_open_drawer(shell, menu_id?)`,
  `yui_shell_close_drawer(shell, menu_id?)`,
  `yui_shell_toggle_drawer(shell, menu_id?)` — act on the
  `C_YUI_NAV` with `layout:"drawer"` whose `menu_id` matches (all of
  them when `menu_id` is omitted). The shell also closes them on
  `Escape` by default. While a drawer is open the shell installs a
  focus-trap on its panel: `Tab` / `Shift+Tab` cycle inside, and on
  close the focus is restored to whichever element triggered the
  open.
- `yui_shell_set_translate(shell, fn)` — hot-swap the i18n resolver.
  Replaces the `translate` attr on the shell and on every nav,
  rebuilds the nav DOMs and the toolbar, then re-publishes
  `EV_ROUTE_CHANGED` so navs re-mark the active item under the new
  labels. Use this to switch languages without destroying the
  shell.

### `C_YUI_NAV`

Instantiated by the shell (one per *menu, zone* pair). End users do
not normally create it directly. The nav **does not** navigate: it
publishes the intent and the shell routes.

Published events:
- `EV_NAV_CLICKED` — `{ route, item_id, zone, level }`. The shell is
  subscribed and decides whether to change the hash or call
  `navigate_to` directly.

Notable attributes: `menu_items`, `zone`, `layout`, `icon_pos`,
`show_label`, `level` (`primary` | `secondary`), `shell`, `translate`,
`nav_label` (human-readable label used by the secondary navs as their
heading and `aria-label`; the shell fills it from the parent item's
`name`).

Public helpers:
- `yui_nav_rebuild(nav)` — tear the nav's DOM down and re-render it
  in place, preserving the host parent and the visibility state.
  Called by `yui_shell_set_translate`; you typically don't need it
  directly.

---

## 7. Integration in an app

```js
import {
    register_c_yui_shell,
    register_c_yui_nav,
} from "@yuneta/lib-yui";
import "@yuneta/lib-yui/src/c_yui_shell.css";
import "bulma/css/bulma.css";
import app_config from "./app_config.json";

register_c_yui_shell();
register_c_yui_nav();
/*  also: register_c_<your_view>() for every gclass referenced in target.gclass */

gobj_create_default_service(
    "shell",
    "C_YUI_SHELL",
    { config: app_config, use_hash: true },
    yuno
);
```

Every view GClass must expose a `$container` attribute holding a root
`HTMLElement`; the shell takes care of appending it to the stage and
managing its visibility.

---

## 8. Solution vs. the original requirements

| Requirement                                              | How it is covered                                             |
|----------------------------------------------------------|---------------------------------------------------------------|
| Declarative, Yuneta-style JSON                           | `config` attribute holding a JSON of `shell`/`menu`/`toolbar` |
| Layers + working zones                                   | 6 fixed layers + 7 zones in a grid                            |
| Two-level primary menu                                   | `menu.primary.items[].submenu.items[]`                        |
| Primary in `left` on desktop and `bottom` on mobile      | `"host": "menu.primary"` in both, with opposite `show_on`     |
| Icon + label; different per zone                         | `render[zone]` with `layout` + `icon_pos` + `show_label`      |
| Submenu as tabs **or** as a side submenu                 | `submenu.render[zone]` set to `"tabs"` / `"vertical"` / etc.  |
| Fixed toolbar at top or bottom                           | `shell.zones.top.host = "toolbar"` (or `bottom`)              |
| Built on top of Bulma helpers                            | `.menu`, `.tabs`, `.level`, `is-hidden-*`; our own CSS only for `icon-bar`, `drawer`, `accordion`, and the per-breakpoint hiders |
| Vite-compatible                                          | Same flow as the rest of `lib-yui`                            |
| Drop-in for libyui later                                 | Re-exported from `index.js`                                   |

---

## 9. Test app

See `test-app/README.md`. Quick start:

```
cd kernel/js/lib-yui/test-app
npm install
npm run dev
```

Exercises every path: breakpoints, click and hash navigation, and the
`keep_alive` vs `lazy_destroy` contrast (visible in the `instance #`
counter painted by `C_TEST_VIEW`).

---

## 10. Status and retirement plan

This pass closes **every** promise of the original design. What remains
are the two historical consumers (`C_YUI_MAIN` and `C_YUI_ROUTING`)
still used elsewhere inside `lib-yui`:

### Implemented ✓

- Zones + `show_on` with the operators `>=`, `<=`, `<`, `>`,
  enumeration, and `|`. Pure parser, unit-tested (`npm test`).
- Automatic inference of the `main` stage from `"host":
  "stage.<name>"`.
- All 6 menu layouts (`vertical`, `icon-bar`, `tabs`, `drawer`,
  `submenu`, `accordion`), with auto-expansion of the active branch on
  accordion when the route changes.
- Off-canvas drawer: mounted on the `overlay` layer (not inside the
  zone grid), closed on backdrop click and on `Escape`, public API
  `yui_shell_{open,close,toggle}_drawer`.
- `lifecycle: eager | keep_alive | lazy_destroy`, with the first one
  preinstantiating the views at startup.
- **Declarative toolbar** (`toolbar.items[]` with `navigate`,
  `drawer`, and `event` actions).
- Single router: the nav publishes `EV_NAV_CLICKED`, the shell
  routes.
- i18n hook `translate: (key) => string` applied to labels and
  `aria-label`.
- Accessibility: `role="navigation"` on navs, `role="dialog"` +
  `aria-modal` on drawers, `aria-expanded` / `aria-controls` on the
  accordion, `aria-disabled` + `tabindex="-1"` on disabled items,
  `:focus-visible` on every interactive control.
- Loud failure when no route is available: `log_error` + a placeholder
  visible in the stage instead of a blank screen.
- Hard contract: when a view does not expose `$container`, the shell
  logs an error and destroys the half-built gobj.

### Retirement plan for `C_YUI_MAIN` / `C_YUI_ROUTING`

These are the blockers before either of them can be deleted:

1. **Modals and notifications** — `C_YUI_MAIN` exposes `display_*`,
   the volatile-dialog manager and toasts. The shell already creates
   the `modal`, `notification`, and `loading` layers in `build_ui`,
   but there is still no declarative API on top of them. First step:
   move `display_error`, `display_info`, `display_confirm` to shell
   helpers that paint into `layers.notification` / `layers.modal`.
2. **Widget grid / `C_YUI_MAIN.layout`** — review which `lib-yui`
   consumers use it (`grep register_c_yui_main`). Each one switches
   to the shell by declaring its zones.
3. **`C_YUI_ROUTING`** — the hash routing already lives in the shell,
   but `C_YUI_TABS` and a few other components still use it. It can
   be retired once each one is rewritten to subscribe to the shell's
   `EV_ROUTE_CHANGED` instead of `C_YUI_ROUTING`'s `EV_ROUTING_CHANGED`.

Checklist before deleting either gclass:

- [ ] `grep -r register_c_yui_main  kernel/ utils/ yunos/` is empty.
- [ ] `grep -r register_c_yui_routing kernel/ utils/ yunos/` is empty.
- [ ] Helpers equivalent to `display_*` are available on top of the
      shell.
- [ ] Consumers of `EV_ROUTING_CHANGED` migrated to the shell's
      `EV_ROUTE_CHANGED`.

---

## 11. Known limitations

These are intentional gaps, documented so they don't surface as
review nits.  Each one has a clear path forward when it becomes
worth the work.

1. **Secondary navs are auto-instantiated only from `menu.primary`.**
   `instantiate_menus` walks `menus.primary.items[*].submenu` to
   build the level-2 navs.  If you declare additional "primary-style"
   menus elsewhere with their own `submenu`, those submenus will
   not get a nav unless they are mounted manually.  Drawer overlays
   (`render[zone].layout === "drawer"`) are detected for any menu
   id and are not subject to this limitation.
2. **Do not import `c_yui_main.css` and `c_yui_shell.css` together.**
   `c_yui_main.css` defines its own `.top-layer` / `.content-layer` /
   `.bottom-layer` with `position:fixed` and CSS variables that
   collide with the shell's full-screen grid.  Pick one: the new
   shell while the migration is in progress, or the legacy main while
   you have not switched.  Apps mixing both will see double layout.
