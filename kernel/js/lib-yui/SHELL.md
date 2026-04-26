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

### Internationalisation

The shell does **not** own i18n. Every translatable text node it
renders (menu labels, secondary-nav heading, toolbar buttons) is
emitted with `data-i18n="<canonical English key>"` via
`createElement2`'s `i18n` attribute. To switch languages, use the
canonical helper from `@yuneta/gobj-js`:

```js
import { refresh_language } from "@yuneta/gobj-js";
import i18next, { t } from "i18next";

i18next.changeLanguage("es");
refresh_language(shell.$container, t);
```

This is the same flow `c_yui_main.js` uses in its
`change_language()`. Apps that already configure i18next get the
shell labels switched for free; apps without i18next can pass any
`(key) => string` function as `t`.

### `C_YUI_NAV`

Instantiated by the shell (one per *menu, zone* pair). End users do
not normally create it directly. The nav **does not** navigate: it
publishes the intent and the shell routes.

Published events:
- `EV_NAV_CLICKED` — `{ route, item_id, zone, level }`. The shell is
  subscribed and decides whether to change the hash or call
  `navigate_to` directly.

Notable attributes: `menu_items`, `zone`, `layout`, `icon_pos`,
`show_label`, `level` (`primary` | `secondary`), `shell`,
`nav_label` (human-readable label used by the secondary navs as
their heading and `aria-label`; the shell fills it from the parent
item's `name`).

The nav has no `translate` attr — it does not translate text
itself. Labels are emitted with `data-i18n` and a single
`refresh_language` call on the shell's `$container` re-translates
every nav at once.

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

## 10. Coexistence with `C_YUI_MAIN` / `C_YUI_ROUTING` — no migration planned

This pass closes **every** promise of the original design.
`C_YUI_MAIN` and `C_YUI_ROUTING` are **not** scheduled for removal:
they keep being shipped and supported alongside the new shell.

### Which one to use

- **New GUIs → `C_YUI_SHELL` + `C_YUI_NAV`.** Declarative config,
  routed stages, drawer overlay, focus-trap, `data-i18n` labels
  driven by `refresh_language()` (the canonical Yuneta i18n flow),
  the new modal/notification helpers (`yui_shell_show_*` /
  `yui_shell_confirm_*`, see `TODO.md` §4).
- **Existing GUIs → keep `C_YUI_MAIN` + `C_YUI_ROUTING`.** Switching
  is opt-in, not mandated. If an app already works on top of the
  legacy stack, leave it alone.

The two stacks coexist and **do not interoperate** in the same DOM —
do not import `c_yui_main.css` and `c_yui_shell.css` together (see
§11 below). One app picks one stack and stays there.

### Drift policy between the two stacks

Bug reports against `display_*`, `get_yes*`, `get_ok` (legacy) are
fixed on the legacy implementation only. The new shell helpers
(`yui_shell_show_*`, `yui_shell_confirm_*`) **do not back-port**
those fixes unless the original behaviour was a real feature, not a
quirk. Conversely, improvements landed on the new shell stay on the
shell. Treat the two APIs as parallel — same intent, separate code.

### Implemented ✓ in this pass

- Zones + `show_on` with the operators `>=`, `<=`, `<`, `>`,
  enumeration, and `|`. Pure parser, unit-tested (`npm test`).
- Automatic inference of the `main` stage from `"host":
  "stage.<name>"`.
- All 6 menu layouts (`vertical`, `icon-bar`, `tabs`, `drawer`,
  `submenu`, `accordion`), with auto-expansion of the active branch
  on accordion when the route changes.
- Off-canvas drawer: mounted on the `overlay` layer (not inside the
  zone grid), closed on backdrop click and on `Escape`, public API
  `yui_shell_{open,close,toggle}_drawer`, focus-trap with
  Tab/Shift-Tab cycling and focus restoration.
- `lifecycle: eager | keep_alive | lazy_destroy`, with the first one
  preinstantiating the views at startup.
- **Declarative toolbar** (`toolbar.items[]` with `navigate`,
  `drawer`, and `event` actions).
- Single router: the nav publishes `EV_NAV_CLICKED`, the shell
  routes.
- Canonical i18n: every translatable text node carries
  `data-i18n="<canonical key>"`; apps switch language by calling
  `refresh_language(shell.$container, t)` from `@yuneta/gobj-js`,
  the same helper `c_yui_main.js` uses.
- Accessibility: `role="navigation"` on navs, `role="dialog"` +
  `aria-modal` on drawers, `aria-expanded` / `aria-controls` on the
  accordion, `aria-disabled` + `tabindex="-1"` on disabled items,
  `:focus-visible` on every interactive control.
- Loud failure when no route is available: `log_error` + a placeholder
  visible in the stage instead of a blank screen.
- Hard contract: when a view does not expose `$container`, the shell
  logs an error and destroys the half-built gobj.

### If the migration decision is ever revisited

The work needed to retire `C_YUI_MAIN` / `C_YUI_ROUTING` is captured
as an optional, deferred track in `TODO.md` §8 (sub-tasks 8.1 → 8.4).
Until that decision is made, do **not** start any of those sub-tasks.

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
