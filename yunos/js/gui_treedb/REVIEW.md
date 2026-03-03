# Deep Review: `yunos/js/gui_treedb` Webapp

**Date:** 2026-03-03

## Overview

This is a **well-structured, production-grade** single-page application built on the Yuneta GObject/FSM framework. It manages TreeDB graph databases via a WebSocket backend, with role-based UI, OAuth2 login, real-time subscriptions, and multiple visualization modes.

---

## Architecture: Strengths

### 1. Consistent GObject Pattern

Every component follows the same structure: `attrs_table → PRIVATE_DATA → mt_create/start/stop/destroy → local methods → action handlers → FSM states/events → gclass_create`. This makes onboarding and navigation predictable across all ~15 GClasses.

### 2. Clean FSM Design (`c_login.js`)

The login FSM is correct and tight:
- `ST_LOGOUT → ST_WAIT_TOKEN → ST_LOGIN`
- Token refresh is timer-driven, with the timer set to `refresh_token.exp - now - 5s`
- Session is persisted to `localStorage` and restored on startup
- Logout clears session and publishes `EV_LOGOUT_DONE` regardless of server response (fail-safe)

### 3. Loose Coupling via Events

Components communicate exclusively through events (`gobj_publish_event` / `gobj_send_event`). `C_YUI_MAIN` subscribes to both `C_LOGIN` and `C_YUNETA_GUI` to react to open/close without either knowing about the UI. This is correct.

### 4. Hash-Based Routing (`c_yui_routing.js`)

The `hashchange` listener + `select_item()` pattern is clean. The menu stores direct references to gobjs and their `$container` elements, enabling O(1) show/hide without DOM queries per navigation. The `mt_show`/`mt_hide` callback pattern on `$item_a` is a nice extensibility hook.

### 5. Real-Time Node Subscriptions (`c_yui_treedb_topics.js`)

After `descs` are received, subscriptions for `EV_TREEDB_NODE_CREATED/UPDATED/DELETED` are registered per topic with proper `__filter__` keys. This is correct — changes pushed from the backend immediately update the table without polling.

---

## Issues Found

### Bug: `ac_timeout` in `c_login.js` (lines 667–682)

```js
// line 673
if(priv.timeout_refresh > 0) {
    let now = get_now();
    if(now >= priv.timeout_refresh) {  // BUG: comparing timestamp to duration
        do_refresh(gobj);
    }
}
```

`priv.timeout_refresh` is set as a **duration in seconds** (e.g. 1795), but `get_now()` returns a **Unix timestamp** (e.g. 1740000000). The condition `now >= priv.timeout_refresh` is always `true` after year 2026 (or even earlier), so `do_refresh()` is called on **every timer tick**, not just when the token is about to expire. The timer is already set to fire at the right time via `set_timeout(priv.gobj_timer, priv.timeout_refresh*1000)` — the condition check inside `ac_timeout` is redundant and broken. It should either be removed entirely (the timer already fires at the right moment) or compare correctly.

### Bug: Modal removal in `main.js` `manage_bulma_modals()` (lines 319–335)

```js
document.addEventListener('click', (event) => {
    (document.querySelectorAll('.modal') || []).forEach(($element) => {
        if(!$element.contains(event.target)) {
            if($element.classList.contains('is-active')) {
                $element.classList.remove('is-active');
            }
        }
        $element.parentNode.removeChild($element);  // BUG: always removes, even if clicked inside
    });
    ...
});
```

`$element.parentNode.removeChild($element)` is called **outside** the `if(!$element.contains(...))` block, so it runs unconditionally — even when the user clicks **inside** the modal. Every click anywhere removes all modals from the DOM, which breaks any modal that requires interaction (form inputs, buttons). This should be inside the `if` block.

### Bug: `DOMContentLoaded` inside `load` in `main.js` (lines 250–254)

```js
window.addEventListener('load', function() {
    ...
    document.addEventListener('DOMContentLoaded', () => {   // BUG: never fires
        const savedTheme = localStorage.getItem('theme') || 'light';
        document.documentElement.setAttribute("data-theme", savedTheme);
    });
});
```

`DOMContentLoaded` fires **before** `load`. Registering a `DOMContentLoaded` listener inside a `load` handler means it will never fire. The theme initialization code is dead. (Theme is correctly applied elsewhere in `C_YUI_MAIN`, so this is low-impact but still wrong.)

### Bug: Unused variables `gobj_graph_mqtt_broker` / `gobj_graph_authzsdb` in `c_yuneta_gui.js` (lines 413, 476)

```js
let gobj_graph_mqtt_broker = gobj_create_service(...);
// gobj_graph_mqtt_broker is never used after creation
priv.user_gobjs.push(gobj_tabs);  // Only tabs is pushed, not the graph
```

`gobj_graph_mqtt_broker` (line 413) and `gobj_graph_authzsdb` (line 476) are created but the local variable is immediately discarded. The graph gobj **is** a child of `gobj_tabs`, so it won't be garbage collected, but it also won't be in `user_gobjs`, meaning `close_services()` won't clean it up properly on logout (only `gobj_tabs` is pushed). The graph inside tabs would be destroyed when tabs is destroyed, but this asymmetry is fragile.

### Issue: Form URL-encode does not use `encodeURIComponent` in `c_login.js`

```js
for(let k of Object.keys(form_data)) {
    let v = form_data[k];
    if(empty_string(data)) {
        data += sprintf("%s=%s", k, v);   // BUG: no URL encoding
    } else {
        data += sprintf("&%s=%s", k, v);
    }
}
```

`username`, `password`, and `refresh_token` values are directly concatenated without `encodeURIComponent`. A password containing `&`, `=`, `+`, or `%` characters will break the request. This applies to `do_login()`, `do_logout()`, and `do_refresh()`.

### Issue: `offline_access || true` dead branch in `c_login.js` (line 247)

```js
if(offline_access || true) {
    // No uso offline de momento...
}
```

`offline_access || true` is always `true`, making the `if` and the `offline_access` parameter meaningless. This should either be removed or the `offline_access` scope logic completed.

### Issue: `build_menu_r` operator precedence bug in `c_yui_routing.js` (lines 423, 503)

```js
if(html && (html instanceof HTMLElement) || is_string(html) || is_array(html)) {
```

Due to operator precedence, this parses as:
```
(html && (html instanceof HTMLElement)) || is_string(html) || is_array(html)
```
When `html` is a non-empty string, the first `&&` short-circuits to `false`, but `is_string(html)` catches it. When `html` is an array, `is_array(html)` catches it. However the intent appears to be:
```js
if(html && ((html instanceof HTMLElement) || is_string(html) || is_array(html)))
```
The current code actually works by accident (truthy `html` string/array is caught by subsequent OR), but reads incorrectly. Same pattern repeated on line 503 for `container`.

### Issue: `backend_config.js` — identical configs for both hostnames

```js
"localhost": { "realm": "estadodelaire.com", "auth-server-url": "https://auth.artgins.com/", ... },
"treedb.yunetas.com": { "realm": "estadodelaire.com", "auth-server-url": "https://auth.artgins.com/", ... }
```

Both entries are 100% identical. If they're always the same, a single default would suffice. If they're meant to differ per environment, this is a misconfiguration risk. More importantly: production auth credentials (`realm`, `auth-server-url`, `resource`) are hardcoded in the frontend bundle — this is a design constraint of the Keycloak public-client pattern, but worth documenting.

### Issue: Incomplete stub commands in `c_yui_routing.js` (lines 264–295)

```js
function cmd_manage_menu(gobj, command, kw, src) {
    switch(command) {
    case "append-menu": break;   // empty
    case "insert-menu": break;   // empty
    case "remove-menu": break;   // empty
    }
    return null;
}
```

These are declared in `mt_command_parser` and exposed to the command plane, but silently do nothing. Callers would get `null` back with no error, which is confusing. They should either be implemented or removed from the command table.

### Issue: `c_yui_treedb_topics.js` — `remove_tab()` is incomplete (lines 296–310)

```js
function remove_tab(gobj, gobj2, id)
{
    // ...removes <li> from tabs
    // TODO remove sub-container   <== never implemented
}
```

`remove_tab()` removes the tab `<li>` but leaves the sub-container `<div>` in the DOM. Any call to this function (if it ever gets wired up) would leak DOM nodes.

### Issue: `ac_login_refreshed` missing `return 0` in `c_login.js` (lines 582–622)

```js
function ac_login_refreshed(gobj, event, kw, src)
{
    ...
    gobj_publish_event(gobj, "EV_LOGIN_REFRESHED", {...});
    // BUG: no return statement
}
```

All other action handlers return `0`. This one is missing `return 0`. Depending on how the framework handles `undefined` returns from action handlers, this may or may not matter, but it breaks the convention.

### Issue: CSP is incomplete in `index.html`

```html
<meta http-equiv="Content-Security-Policy"
      content="worker-src 'self' blob:; child-src 'self' blob:; img-src 'self' data: blob:;">
```

No `script-src`, `connect-src`, `style-src`, or `default-src` directives. Without `connect-src`, the browser's default-src fallback (or none if missing) doesn't restrict the WebSocket connections to `wss://auth.artgins.com` and `wss://localhost:1800`. Adding `connect-src` would harden the app.

---

## Minor / Code Quality Issues

| Location | Issue |
|---|---|
| `main.js:64–65` | Two commented-out imports for `yuneta-icon-font` with `// TODO parece que no se usa` — should be removed |
| `main.js:219–223` | Dead commented-out code (`gobj_pause`, `gobj_stop`, `gobj_destroy`) — should be removed |
| `c_yuneta_gui.js:653` | `// TODO Get user's home from their config preferences` — hardcoded `"#monitoring"` which doesn't exist in the built menu |
| `c_yuneta_gui.js:319–341` | Large block of commented-out code for `historical_tracks` — should be deleted or tracked in TODO.md |
| `c_yui_routing.js:207–208` | `mt_destroy` docstring says "Framework Method: Destroy" but the function is `mt_command_parser` — copy-paste error in the comment |
| `c_yui_treedb_topics.js:39` | Inline import style: `, gobj_stop_children,` appears tacked onto the previous line — formatting inconsistency |
| `c_yui_treedb_topic_form.js:67–68` | Both `jQuery` and `$` are imported from `jquery` separately — `$` is sufficient |
| `c_yui_treedb_topic_form.js:64,69` | CSS imported twice: `jse-theme-dark.css` is imported here AND in `main.js:56` |
| `vite.config.js:8` | `chunkSizeWarningLimit: 6000` (6MB) suppresses important bundle size warnings rather than fixing them |
| `c_yui_treedb_graph.js` | File comment says "Manage treedb topics with mxgraph" but uses AntV G6 — stale comment from a previous library |

---

## Security Summary

| Risk | Severity | Location |
|---|---|---|
| JWT + OAuth tokens in `localStorage` | Medium | `c_login.js:543–549` — susceptible to XSS |
| No `encodeURIComponent` in form POST | High | `c_login.js:255–262` |
| Incomplete CSP | Medium | `index.html:8–11` |
| Auth config hardcoded in bundle | Low | `backend_config.js` (public-client by design) |

---

## Summary

The codebase is **architecturally sound** and follows the Yuneta framework conventions consistently. The GObject/FSM model is correctly applied throughout. The main actionable bugs are:

1. **`ac_timeout` comparison logic** in `c_login.js` — `now >= priv.timeout_refresh` always true, causes token refresh on every timer tick
2. **Modal removal** in `main.js` `manage_bulma_modals()` — `removeChild` runs unconditionally, breaks interactive modals
3. **Missing `encodeURIComponent`** in OAuth form data — breaks login for passwords containing special characters
4. **Dead `DOMContentLoaded` listener** registered inside `load` — theme init code never runs

The stub commands and incomplete `remove_tab` are technical debt to track. The operator precedence issue in `build_menu_r` works by accident but should be clarified for readability.
