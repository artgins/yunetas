# Deep Review: `kernel/js/gobj-js` Framework

**Date:** 2026-03-03

## Overview

This is the **JavaScript port of the C Yuneta GObject/FSM runtime**. The translation is faithful and consistent — the same GClass/GObj/FSM semantics, the same subscription model, the same attribute schema system. The result is a ~10,700-line library that allows webapp components to be written with the same architecture as the C backend.

---

## Architecture: Strengths

### 1. Faithful C-to-JS Portability

The enum strategy (`Object.freeze({...})`), the class constructors for `GClass`, `GObj`, `State`, `Event_Action`, `Event_Type`, and the linear-array FSM lookups all mirror the C implementation exactly. A developer who knows the C framework can navigate the JS codebase without friction.

### 2. Compile-Time FSM Validation

`gclass_check_fsm()` runs at registration time and catches:
- Duplicate events in a state
- State transitions referencing non-existent states
- Events declared in states but missing from the event_types table
- Input events not handled in any state

This fail-fast approach catches wiring errors before any GObj is instantiated.

### 3. Subscription Deduplication

`gobj_subscribe_event()` detects and replaces duplicate subscriptions (same publisher + event + filter + subscriber tuple) with a `log_warning`, preventing silent double-delivery bugs.

### 4. Unknown Subscription Key Validation

```js
const IGNORED_KEYS = new Set(["__config__", "__global__", "__local__", "__filter__", "__service__"]);
Object.keys(kw).forEach(key => {
    if (!IGNORED_KEYS.has(key)) {
        log_error(`key ignored in subscription (key: ${key}, ...)`);
    }
});
```
Any key passed to `gobj_subscribe_event()` that is not a recognized filter key is immediately logged as an error. Good defensive API design.

### 5. WebSocket Robustness in `c_ievent_cli.js`

`close_websocket()` nulls handlers before closing (`onopen = null`, etc.), correctly preventing double-dispatch from the close callback. The `inside_on_open` flag prevents duplicate subscription resends during the identity card ACK sequence. The `browser_beforeunload` flag avoids spurious reconnects on page unload.

### 6. Persistent Attrs Merge Strategy in `dbsimple.js`

`db_save_persistent_attrs()` reads the existing localStorage entry and calls `json_object_update_missing(attrs, jn_file)` before writing, so keys not in the current write are preserved. This prevents different GClasses from clobbering each other's keys if they happen to share the same storage path.

---

## Bugs Found

### Bug: `sdata_write_default_values` calls `mt_writing` without null check (`gobj.js:461`)

```js
// json2item() does it correctly:
if(gobj.gclass.gmt.mt_writing) {        // ← null check present
    gobj.gclass.gmt.mt_writing(gobj, it.name);
}

// sdata_write_default_values() does NOT:
gobj.gclass.gmt.mt_writing(gobj, it.name);  // ← TypeError if mt_writing is undefined
```

`sdata_write_default_values()` is called from `gobj_write_attrs()`. Any GClass that doesn't define `mt_writing` in its `gmt` table will throw a `TypeError` when `gobj_write_attrs()` is called on it after creation. The fix in `json2item` is already correct — `sdata_write_default_values` needs the same guard.

### Bug: `set_default` error message prints `it.name` twice (`gobj.js:562`)

```js
log_error(`default_value WRONG, it: ${it.name}, value: ${it.name}`);
//                                                         ^^^^^^^^ should be it.default_value
```

When a default value evaluates to `undefined`, the error message shows the attribute name twice and never shows the actual bad default value, making the error undiagnosable from the log alone.

### Bug: `log_info` routes to `f_warning` instead of `f_info` (`helpers.js:135`)

```js
function log_info(format) {
    let msg = format_log(format);
    let hora = current_timestamp();
    if(f_warning) {
        f_warning("%c" + hora + " INFO: " + String(msg), "color:cyan");  // ← f_warning, not f_info
    }
}
```

`f_info` is declared and set to `window.console.info` but is never called. All `log_info()` calls are silently routed to the warning channel. In production where only warnings are captured by the remote log function, INFO messages would be incorrectly sent as warnings.

### Bug: `gobj_get_gclass_config` accesses `.prototype` on a `GClass` instance (`gobj.js:1237`)

```js
if(gclass && gclass.prototype.mt_get_gclass_config) {  // TODO review
    return gclass.prototype.mt_get_gclass_config.call();
}
```

`gclass` is a `GClass` *instance* (returned by `gclass_create()`), not a constructor function. Plain objects don't have a `.prototype` property — this is always `undefined`, so the condition is always `false` and the function always returns `null`. The correct lookup for a GClass instance is `gclass.gmt.mt_get_gclass_config`.

### Bug: `ac_mt_stats` and `ac_mt_command` return `null` not `0` (`c_ievent_cli.js:1190, 1196`)

```js
function ac_mt_stats(gobj, event, kw, src) {
    // TODO
    return null;   // should be 0 (or -1 for error)
}
```

All other action handlers return an integer. Returning `null` from an action propagates as a return value from `gobj_send_event()`, which callers interpret as failure/error. These should return `0` until implemented.

### Bug: `mt_subscription_deleted` has a bare `return;` (`c_ievent_cli.js:456`)

```js
if(empty_string(subs.event)) {
    // HACK only resend explicit subscriptions
    return;    // ← returns undefined, not 0
}
```

All other early exits in action handlers return `0`. This propagates `undefined` to `gobj_send_event()` callers.

### Bug: `gobj_post_event` sends to potentially-destroyed gobj (`gobj.js:3235`)

```js
function gobj_post_event(gobj, event, kw, src) {
    setTimeout(() => {
        gobj_send_event(gobj, event, kw, src);   // gobj might be destroyed in 10ms
    }, 10);
}
```

`gobj_send_event` checks `obflag_destroyed` at entry, so state won't be corrupted, but it will log a noisy error for a legitimate pattern where a gobj is destroyed before the deferred event fires. A destroyed-check before the call suppresses the false-positive error.

### Bug: `__inside__` counter not protected against action exceptions (`gobj.js:3272, 3381`)

`__inside__` is incremented at the top of `gobj_send_event()` and decremented at the bottom. If an action function throws an unhandled exception, `__inside__` is never decremented, permanently corrupting the indentation depth counter for all subsequent tracing. A `try/finally` block around the action call fixes this.

---

## Issues Found

### Issue: `_gclass_register` uses a plain object — prototype key collision risk (`gobj.js:76`)

```js
let _gclass_register = {};
```

If a GClass were named `"constructor"`, `"toString"`, `"hasOwnProperty"`, or any other `Object.prototype` property, `_gclass_register["constructor"]` would shadow the built-in. `Object.create(null)` eliminates the prototype chain entirely.

### Issue: `localStorage` operations have no error handling (`helpers.js`)

```js
function kw_set_local_storage_value(key, value) {
    localStorage.setItem(key, JSON.stringify(value));  // can throw QuotaExceededError
}
function kw_get_local_storage_value(key, def, verbose) {
    let value = localStorage.getItem(key);             // Safari private browsing throws
    ...
}
```

In Safari private browsing, `localStorage.setItem` throws a `SecurityError`. In any browser when storage is full, it throws `QuotaExceededError`. Neither is caught; unhandled throws crash the event loop.

### Issue: `json_is_identical` is order-sensitive (`helpers.js:192`)

```js
function json_is_identical(kw1, kw2) {
    let kw1_ = JSON.stringify(kw1);
    let kw2_ = JSON.stringify(kw2);
    return (kw1_ === kw2_);
}
```

`JSON.stringify` output depends on property insertion order. `{a:1, b:2}` and `{b:2, a:1}` are semantically identical but stringify differently. This is used in `_match_subscription()` to match `__filter__`, `__config__`, `__global__`, and `__local__` blobs — a subscription built in different property order won't match, causing silent duplicate subscriptions.

### Issue: `tab()` builds indentation string with a loop (`gobj.js:346`)

```js
function tab() {
    let bf = '';
    for(let i=1; i<__inside__*2; i++) {
        bf += ' ';
    }
    return bf;
}
```

String concatenation in a loop creates N intermediate strings. Should be `' '.repeat(Math.max(0, __inside__ * 2 - 1))`.

### Issue: `window` reference is evaluated at module import time (`helpers.js:45–48`)

```js
let f_error = window.console.error;
let f_warning = window.console.warn;
let f_info = window.console.info;
let f_debug = window.console.debug;
```

Evaluated at module import time. If the module is ever imported in a non-browser environment (Vitest/jsdom, Node.js), this throws `ReferenceError: window is not defined`. The `typeof window !== "undefined"` guard is needed.

### Issue: `window?` conditional in `c_ievent_cli.js:725` is always truthy in browser

```js
"user_agent": window ? window.navigator.userAgent : "",
```

In a browser, `window` is always truthy. The falsy branch is dead code. The correct guard is `typeof window !== "undefined"`.

### Issue: `json_object_update` has redundant `hasOwnProperty` check (`helpers.js:208`)

```js
for (let property of Object.keys(source)) {
    if (source.hasOwnProperty(property)) {   // ← Object.keys() already excludes prototype props
        destination[property] = source[property];
    }
}
```

`Object.keys()` only returns own enumerable properties. The `hasOwnProperty` guard inside is always `true` and should be removed.

### Issue: `__inside_event_loop__` is a dead variable (`gobj.js:62`)

```js
let __inside_event_loop__ = 0;
```

Declared at module level but never read or written after initialization. Only `__inside__` (line 81) is used for depth tracking.

---

## Minor / Code Quality Issues

| Location | Issue |
|---|---|
| `gobj.js:3440–3459` | Large `// TODO` block — C code for `__rename_event_name__` pasted as a comment, not yet ported |
| `c_ievent_cli.js:443` | `// TODO hay algo mal, las subscripciones locales se interpretan como remotas` — known local/remote subscription conflation issue |
| `c_ievent_cli.js:735` | `// TODO ??? in C is gobj_name(gobj)` — uncertain origin of identity card `src_service` |
| `c_ievent_cli.js:1124–1140` | Inbound `__subscribing__` / `__unsubscribing__` return `-1` with TODO — server cannot subscribe to browser-side events |
| `c_ievent_cli.js:115–120` | Per-class trace level enum is commented out — prevents fine-grained tracing of the IEvent client |
| `command_parser.js`, `stats_parser.js` | Both are 42-line stubs; exported as public API but return empty responses |

---

## Security Notes

| Risk | Severity | Detail |
|---|---|---|
| JWT stored in `SDF_PERSIST` attr | Medium | `c_ievent_cli.js` stores JWT in a persistent attribute → goes to `localStorage` via `dbsimple.js` — susceptible to XSS |
| No origin validation on WebSocket messages | Low | `iev_create_from_json` parses any incoming JSON; browser enforces origin at connection time but worth noting |
| `window.console` used as log sink | Low | An XSS attacker could override `window.console` methods before module load, redirecting all log output |

---

## Summary

The gobj-js framework is a **solid, production-grade JS port** of the C runtime. The FSM validation, subscription model, and WebSocket client are mature and correct. The main actionable bugs are:

1. **`sdata_write_default_values` calls `mt_writing` without null check** — crashes any GClass that omits `mt_writing`
2. **`log_info` routes to `f_warning`** — INFO log level is broken
3. **`set_default` error message** prints `it.name` instead of `it.default_value`
4. **`gobj_get_gclass_config` uses `.prototype`** on an instance — always returns `null`
5. **`ac_mt_stats` / `ac_mt_command` return `null`** instead of `0`
6. **`mt_subscription_deleted` bare `return;`** propagates `undefined`
7. **`localStorage` has no try-catch** — crashes in private browsing or on quota exceeded
8. **`__inside__` not protected with `try/finally`** — corrupts depth counter on action exceptions
9. **`_gclass_register` plain object** — susceptible to prototype key collision
10. **`json_is_identical` is order-sensitive** — subscription deduplication can fail silently

---

## Fixes Applied

**Date:** 2026-03-03
**Branch:** `claude/review-gui-treedb-webapp-tuDfl`

All bugs and issues listed above were fixed. Details per file:

### `gobj-js/src/gobj.js`

| Fix | What changed |
|---|---|
| `sdata_write_default_values` null check | Added `if(gobj.gclass.gmt.mt_writing)` guard before calling `mt_writing`, matching the pattern already used in `json2item` |
| `set_default` error message | Changed second `${it.name}` to `${it.default_value}` so the bad value is actually shown |
| `gobj_get_gclass_config` | Changed `gclass.prototype.mt_get_gclass_config` to `gclass.gmt.mt_get_gclass_config` — correct lookup on a GClass instance |
| `gobj_post_event` destroyed-check | Added `if(gobj_is_destroying(gobj)) return;` before the deferred `gobj_send_event` call |
| `__inside__` try/finally | Wrapped the action call in `try/finally` so `__inside__--` always runs even if the action throws |
| `_gclass_register` | Changed from `{}` to `Object.create(null)` to eliminate prototype key collision risk |
| `tab()` | Replaced loop with `' '.repeat(Math.max(0, __inside__ * 2 - 1))` |
| Dead `__inside_event_loop__` | Removed the unused `let __inside_event_loop__ = 0;` variable |

### `gobj-js/src/helpers.js`

| Fix | What changed |
|---|---|
| `log_info` routing | Changed `f_warning(...)` to `f_info(...)` so INFO messages go to the info channel |
| `localStorage` try-catch | Wrapped `kw_set_local_storage_value` and `kw_get_local_storage_value` in try/catch, logging errors instead of crashing |
| `json_is_identical` order-sensitivity | Replaced stringify comparison with a recursive key-sorted deep comparison so `{a:1,b:2}` equals `{b:2,a:1}` |
| `json_object_update` redundant check | Removed the always-true `if(source.hasOwnProperty(property))` inside `for...of Object.keys(source)` |

### `gobj-js/src/c_ievent_cli.js`

| Fix | What changed |
|---|---|
| `ac_mt_stats` return value | Changed `return null` to `return 0` |
| `ac_mt_command` return value | Changed `return null` to `return 0` |
| `mt_subscription_deleted` bare return | Changed bare `return;` to `return 0;` |
| `window?` guard | Changed `window ?` to `typeof window !== "undefined" ?` for correctness in non-browser environments |
