---
title: 'JS: GObject Lifecycle'
description: >-
  Creating, starting, playing, stopping and destroying a gobj, and the
  flags that give each kind of gobj its behavior.
---

# GObject Lifecycle

**Source code:** [`src/gobj.js`](https://github.com/artgins/gobj-js/blob/7.29.2/src/gobj.js)

:::{important}
The lifecycle is explicit. Yuneta has no garbage collector for gobjs. Prefer, in
this order: first, a static child whose creation and destruction go with those
of its parent. Second, a destruction of itself at a clear end of work. Last, a
destruction that a posted event carries. Never a timer of one millisecond.
:::

---

## Flags

(js_gobj_flag_t)=
### [`gobj_flag_t`](https://github.com/artgins/gobj-js/blob/7.29.2/src/gobj.js#L303)

The features of a gobj. Each creation function below writes one combination of
these, so application code rarely names them.

| Flag | Description |
|---|---|
| `gobj_flag_yuno` | The root of the tree. |
| `gobj_flag_default_service` | The service that receives `gobj_play()` and `gobj_pause()`. |
| `gobj_flag_service` | A service, with a public interface of events, attributes, commands and statistics. |
| `gobj_flag_volatil` | The parent destroys it when the parent goes. |
| `gobj_flag_pure_child` | It sends its events to its parent, and nobody outside reaches it. |
| `gobj_flag_autostart` | The framework starts it. |
| `gobj_flag_autoplay` | The framework plays it. |

---

## Creation

(js_gobj_create)=
### [`gobj_create(gobj_name, gclass_name, kw, parent)`](https://github.com/artgins/gobj-js/blob/7.29.2/src/gobj.js#L2699)

Builds a child gobj. It is the function that a gclass uses in `mt_create`.

(js_gobj_create_yuno)=
### [`gobj_create_yuno(gobj_name, gclass_name, kw)`](https://github.com/artgins/gobj-js/blob/7.29.2/src/gobj.js#L2637)

Builds the root of the tree. There is one for each process.

(js_gobj_create_service)=
### [`gobj_create_service(gobj_name, gclass_name, kw, parent)`](https://github.com/artgins/gobj-js/blob/7.29.2/src/gobj.js#L2645)

Builds a service with a name under the yuno. Only a service can be the source or
the destination of a message between yunos.

(js_gobj_create_default_service)=
### [`gobj_create_default_service(gobj_name, gclass_name, kw, parent)`](https://github.com/artgins/gobj-js/blob/7.29.2/src/gobj.js#L2654)

Builds the default service, which receives
[`gobj_play()`](#js_gobj_play) and [`gobj_pause()`](#js_gobj_pause).

:::{warning}
This function does **not** put the gobj in the register of the services, so
[`gobj_find_service()`](hierarchy.md#js_gobj_find_service) gives `null` for it.
Keep the value that the function returns.
:::

(js_gobj_create_volatil)=
### [`gobj_create_volatil(gobj_name, gclass_name, kw, parent)`](https://github.com/artgins/gobj-js/blob/7.29.2/src/gobj.js#L2669)

Builds a child that the parent destroys with itself.

(js_gobj_create_pure_child)=
### [`gobj_create_pure_child(gobj_name, gclass_name, kw, parent)`](https://github.com/artgins/gobj-js/blob/7.29.2/src/gobj.js#L2684)

Builds a pure child. It sends its events to its parent, and the parent must
declare each one of them in its own state machine.

:::{note}
A pure child does **not** start with `mt_play` of the yuno. Start it with
[`gobj_start()`](#js_gobj_start).
:::

(js_gobj_create2)=
### [`gobj_create2(gobj_name, gclass_name, kw, parent, gobj_flag)`](https://github.com/artgins/gobj-js/blob/7.29.2/src/gobj.js#L2454)

Builds a gobj with the flags that the caller gives. Every function above calls
it with one combination of [`gobj_flag_t`](#js_gobj_flag_t). Use it only for a
combination that no other function gives.

(js_gobj_destroy)=
### [`gobj_destroy(gobj)`](https://github.com/artgins/gobj-js/blob/7.29.2/src/gobj.js#L2717)

Destroys a gobj. The destruction goes down: it destroys the children first. It
deletes the subscriptions, and it takes the gobj out of the queue of the posted
events.

**Not under code that still runs on it** (gobj-js 7.26.22, the same check as
the C kernel's [`gobj_destroy()`](../gobj/creation.md#gobj_destroy)). The
runtime keeps a frame for each action, publication and framework method in
progress. When `gobj_destroy()` finds a live frame of the gobj that it
destroys, or of a gobj below it, it logs an ERROR with the stack and destroys
all the same:

- *"Destroying a gobj while its tree is publishing"*: a subscriber destroys
  the publisher, or an ancestor of the publisher.
- *"Destroying a gobj while one of its actions is still running"*: an inner
  dispatch destroys a gobj whose outer action did not return.

The line names `destroying`, `publisher` (or `running`), `event`, `destroyer`
and `inner_event`. It is not an error when the innermost frame destroys its own
gobj (a gobj that destroys itself in its action), or when a host destroys its
transport in the transport's `EV_STOPPED`.

The fix is to post an event to yourself and destroy in its action. The event is
an input event of the gclass, in its FSM and in its `event_types`:

```javascript
// A host ends the C_TIMER child that it armed. EV_TIMEOUT is PUBLISHED by
// the timer: destroying it here destroys the timer under its publication.
function ac_timeout(gobj, event, kw, src)
{
    gobj_post_event(gobj, "EV_TIMER_DONE", {timer: gobj_name(src)}, gobj);
    return 0;
}

function ac_timer_done(gobj, event, kw, src)
{
    let timer = gobj.priv.timer;
    if(timer && gobj_name(timer) === kw.timer) {
        gobj_stop(timer);
        gobj_destroy(timer);
        gobj.priv.timer = null;
    }
    return 0;
}
```

---

## Start and stop

(js_gobj_start)=
### [`gobj_start(gobj)`](https://github.com/artgins/gobj-js/blob/7.29.2/src/gobj.js#L2927)

Starts a gobj, and calls its `mt_start` method.

The function refuses to start a gobj that has an empty `SDF_REQUIRED` attribute,
as `gobj_start()` in C does (gobj-js 7.26.28, BREAKING). It logs one ERROR with
the names of the empty attributes, gives `-1`, and does not call `mt_start`.
These values are empty:

- `DTP_STRING`: `""`.
- `DTP_LIST`, `DTP_DICT`, `DTP_JSON`: a value with nothing in it, as
  [`empty_json()`](helpers_json.md#js_empty_json) tells: `null`, `{}`, `[]`,
  `""`, or a scalar.
- `DTP_POINTER`: `null`.

A `DTP_BOOLEAN`, `DTP_INTEGER` or `DTP_REAL` is never empty.

```javascript
const attrs_table = [
SDATA(data_type_t.DTP_STRING, "url", sdata_flag_t.SDF_RD|sdata_flag_t.SDF_REQUIRED, "", "Backend url"),
SDATA_END()
];
// ...
const gobj = gobj_create("backend", "C_MY_LINK", {}, parent);
gobj_start(gobj);   // -1, ERROR: "Cannot start without all required attributes: ["url"]"

const gobj2 = gobj_create("backend2", "C_MY_LINK", {url: "wss://example.com:1991"}, parent);
gobj_start(gobj2);  // 0
```

[`gobj_start_tree()`](#js_gobj_start_tree) does not start such a gobj, and it
starts its children, as in C. Before 7.26.28 the check was not in JS, and the
gobj started without the attribute.

(js_gobj_stop)=
### [`gobj_stop(gobj)`](https://github.com/artgins/gobj-js/blob/7.29.2/src/gobj.js#L3076)

Stops a gobj, and calls its `mt_stop` method.

(js_gobj_start_children)=
### [`gobj_start_children(gobj)`](https://github.com/artgins/gobj-js/blob/7.29.2/src/gobj.js#L3017)

Starts the children of a gobj, and not the gobj.

(js_gobj_stop_children)=
### [`gobj_stop_children(gobj)`](https://github.com/artgins/gobj-js/blob/7.29.2/src/gobj.js#L3126)

Stops the children of a gobj.

(js_gobj_start_tree)=
### [`gobj_start_tree(gobj)`](https://github.com/artgins/gobj-js/blob/7.29.2/src/gobj.js#L3045)

Starts a gobj and each one of the gobjs below it. A gclass with the flag
`gcflag_manual_start` does not start here.

(js_gobj_stop_tree)=
### [`gobj_stop_tree(gobj)`](https://github.com/artgins/gobj-js/blob/7.29.2/src/gobj.js#L3139)

Stops a gobj and each one of the gobjs below it.

:::{warning}
Never stop or destroy the tree of a publisher inside the action of a
subscriber. [`gobj_publish_event()`](events.md#js_gobj_publish_event)
dispatches synchronously, so the action runs inside the stack of the
publisher, and the stop dismantles what is still in an iteration. Post an event
instead. A destroy there is an ERROR (see
[`gobj_destroy()`](#js_gobj_destroy)).
:::

---

## Play and pause

(js_gobj_play)=
### [`gobj_play(gobj)`](https://github.com/artgins/gobj-js/blob/7.29.2/src/gobj.js#L3164)

Plays a gobj, and calls its `mt_play` method.

If the gobj is not running, `gobj_play()` starts it first, with a WARNING. If
that start is refused (an empty `SDF_REQUIRED` attribute), destroys the gobj,
or leaves it not running, `gobj_play()` gives `-1` and does not call `mt_play`
(gobj-js 7.26.29, as in C). A `mt_start` that returns `-1` and leaves the gobj
running does not stop the play.

```javascript
// "url" is SDF_REQUIRED and empty: the start is refused, so the play is too.
const ws = gobj_create("ws", "C_MY_CLIENT", {}, yuno);
gobj_play(ws);  // -1; mt_play is not called, gobj_is_playing(ws) is false
```

(js_gobj_pause)=
### [`gobj_pause(gobj)`](https://github.com/artgins/gobj-js/blob/7.29.2/src/gobj.js#L3225)

Pauses a gobj, and calls its `mt_pause` method.

:::{note}
A service opens its queues and its connections in `mt_play`, and it builds its
children in `mt_create`. That is the reason why a yuno that runs without a play
stays with `running` true and `playing` false.
:::

---

## Status

(js_gobj_is_running)=
### [`gobj_is_running(gobj)`](https://github.com/artgins/gobj-js/blob/7.29.2/src/gobj.js#L3260)

Tells if a gobj is running.

(js_gobj_is_playing)=
### [`gobj_is_playing(gobj)`](https://github.com/artgins/gobj-js/blob/7.29.2/src/gobj.js#L3294)

Tells if a gobj is playing.

(js_gobj_is_service)=
### [`gobj_is_service(gobj)`](https://github.com/artgins/gobj-js/blob/7.29.2/src/gobj.js#L3282)

Tells if a gobj is a service.

(js_gobj_is_volatil)=
### [`gobj_is_volatil(gobj)`](https://github.com/artgins/gobj-js/blob/7.29.2/src/gobj.js#L3424)

Tells if the parent destroys this gobj with itself.

(js_gobj_is_pure_child)=
### [`gobj_is_pure_child(gobj)`](https://github.com/artgins/gobj-js/blob/7.29.2/src/gobj.js#L3440)

Tells if a gobj is a pure child.

(js_gobj_is_destroying)=
### [`gobj_is_destroying(gobj)`](https://github.com/artgins/gobj-js/blob/7.29.2/src/gobj.js#L3456)

Tells if a gobj is under destruction. Use it in a handler of a transport that
can arrive late, such as the close of a websocket.
