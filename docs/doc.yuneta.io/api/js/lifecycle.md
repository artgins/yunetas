---
title: 'JS: GObject Lifecycle'
description: >-
  Creating, starting, playing, stopping and destroying a gobj, and the
  flags that give each kind of gobj its behavior.
---

# GObject Lifecycle

**Source code:** [`src/gobj.js`](https://github.com/artgins/gobj-js/blob/7.16.4/src/gobj.js)

:::{important}
The lifecycle is explicit. Yuneta has no garbage collector for gobjs. Prefer, in
this order: first, a static child whose creation and destruction go with those
of its parent. Second, a destruction of itself at a clear end of work. Last, a
destruction that a posted event carries. Never a timer of one millisecond.
:::

---

## Flags

(js_gobj_flag_t)=
### [`gobj_flag_t`](https://github.com/artgins/gobj-js/blob/7.16.4/src/gobj.js#L277)

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
### [`gobj_create(gobj_name, gclass_name, kw, parent)`](https://github.com/artgins/gobj-js/blob/7.16.4/src/gobj.js#L2048)

Builds a child gobj. It is the function that a gclass uses in `mt_create`.

(js_gobj_create_yuno)=
### [`gobj_create_yuno(gobj_name, gclass_name, kw)`](https://github.com/artgins/gobj-js/blob/7.16.4/src/gobj.js#L1986)

Builds the root of the tree. There is one for each process.

(js_gobj_create_service)=
### [`gobj_create_service(gobj_name, gclass_name, kw, parent)`](https://github.com/artgins/gobj-js/blob/7.16.4/src/gobj.js#L1994)

Builds a service with a name under the yuno. Only a service can be the source or
the destination of a message between yunos.

(js_gobj_create_default_service)=
### [`gobj_create_default_service(gobj_name, gclass_name, kw, parent)`](https://github.com/artgins/gobj-js/blob/7.16.4/src/gobj.js#L2003)

Builds the default service, which receives
[`gobj_play()`](#js_gobj_play) and [`gobj_pause()`](#js_gobj_pause).

:::{warning}
This function does **not** put the gobj in the register of the services, so
[`gobj_find_service()`](hierarchy.md#js_gobj_find_service) gives `null` for it.
Keep the value that the function returns.
:::

(js_gobj_create_volatil)=
### [`gobj_create_volatil(gobj_name, gclass_name, kw, parent)`](https://github.com/artgins/gobj-js/blob/7.16.4/src/gobj.js#L2018)

Builds a child that the parent destroys with itself.

(js_gobj_create_pure_child)=
### [`gobj_create_pure_child(gobj_name, gclass_name, kw, parent)`](https://github.com/artgins/gobj-js/blob/7.16.4/src/gobj.js#L2033)

Builds a pure child. It sends its events to its parent, and the parent must
declare each one of them in its own state machine.

:::{note}
A pure child does **not** start with `mt_play` of the yuno. Start it with
[`gobj_start()`](#js_gobj_start).
:::

(js_gobj_create2)=
### [`gobj_create2(gobj_name, gclass_name, kw, parent, gobj_flag)`](https://github.com/artgins/gobj-js/blob/7.16.4/src/gobj.js#L1804)

Builds a gobj with the flags that the caller gives. Every function above calls
it with one combination of [`gobj_flag_t`](#js_gobj_flag_t). Use it only for a
combination that no other function gives.

(js_gobj_destroy)=
### [`gobj_destroy(gobj)`](https://github.com/artgins/gobj-js/blob/7.16.4/src/gobj.js#L2066)

Destroys a gobj. The destruction goes down: it destroys the children first. It
deletes the subscriptions, and it takes the gobj out of the queue of the posted
events.

---

## Start and stop

(js_gobj_start)=
### [`gobj_start(gobj)`](https://github.com/artgins/gobj-js/blob/7.16.4/src/gobj.js#L2215)

Starts a gobj, and calls its `mt_start` method.

(js_gobj_stop)=
### [`gobj_stop(gobj)`](https://github.com/artgins/gobj-js/blob/7.16.4/src/gobj.js#L2330)

Stops a gobj, and calls its `mt_stop` method.

(js_gobj_start_children)=
### [`gobj_start_children(gobj)`](https://github.com/artgins/gobj-js/blob/7.16.4/src/gobj.js#L2275)

Starts the children of a gobj, and not the gobj.

(js_gobj_stop_children)=
### [`gobj_stop_children(gobj)`](https://github.com/artgins/gobj-js/blob/7.16.4/src/gobj.js#L2373)

Stops the children of a gobj.

(js_gobj_start_tree)=
### [`gobj_start_tree(gobj)`](https://github.com/artgins/gobj-js/blob/7.16.4/src/gobj.js#L2301)

Starts a gobj and each one of the gobjs below it. A gclass with the flag
`gcflag_manual_start` does not start here.

(js_gobj_stop_tree)=
### [`gobj_stop_tree(gobj)`](https://github.com/artgins/gobj-js/blob/7.16.4/src/gobj.js#L2386)

Stops a gobj and each one of the gobjs below it.

:::{warning}
Never stop the tree of a publisher inside the action of a subscriber.
[`gobj_publish_event()`](events.md#js_gobj_publish_event) dispatches
synchronously, so the action runs inside the stack of the publisher, and the
stop dismantles what is still in an iteration. Post an event instead.
:::

---

## Play and pause

(js_gobj_play)=
### [`gobj_play(gobj)`](https://github.com/artgins/gobj-js/blob/7.16.4/src/gobj.js#L2409)

Plays a gobj, and calls its `mt_play` method.

(js_gobj_pause)=
### [`gobj_pause(gobj)`](https://github.com/artgins/gobj-js/blob/7.16.4/src/gobj.js#L2460)

Pauses a gobj, and calls its `mt_pause` method.

:::{note}
A service opens its queues and its connections in `mt_play`, and it builds its
children in `mt_create`. That is the reason why a yuno that runs without a play
stays with `running` true and `playing` false.
:::

---

## Status

(js_gobj_is_running)=
### [`gobj_is_running(gobj)`](https://github.com/artgins/gobj-js/blob/7.16.4/src/gobj.js#L2490)

Tells if a gobj is running.

(js_gobj_is_playing)=
### [`gobj_is_playing(gobj)`](https://github.com/artgins/gobj-js/blob/7.16.4/src/gobj.js#L2524)

Tells if a gobj is playing.

(js_gobj_is_service)=
### [`gobj_is_service(gobj)`](https://github.com/artgins/gobj-js/blob/7.16.4/src/gobj.js#L2512)

Tells if a gobj is a service.

(js_gobj_is_volatil)=
### [`gobj_is_volatil(gobj)`](https://github.com/artgins/gobj-js/blob/7.16.4/src/gobj.js#L2654)

Tells if the parent destroys this gobj with itself.

(js_gobj_is_pure_child)=
### [`gobj_is_pure_child(gobj)`](https://github.com/artgins/gobj-js/blob/7.16.4/src/gobj.js#L2670)

Tells if a gobj is a pure child.

(js_gobj_is_destroying)=
### [`gobj_is_destroying(gobj)`](https://github.com/artgins/gobj-js/blob/7.16.4/src/gobj.js#L2686)

Tells if a gobj is under destruction. Use it in a handler of a transport that
can arrive late, such as the close of a websocket.
