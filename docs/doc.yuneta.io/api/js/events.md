---
title: 'JS: Event System'
description: >-
  Sending, posting, publishing and subscribing events in @yuneta/gobj-js,
  with the command and stats entry points.
---

# Event System

GObjects communicate only with events that carry a JSON key-value payload
(`kw`). There is no direct method call. Every interaction goes through the
event dispatcher of the finite state machine.

**Source code:** [`src/gobj.js`](https://github.com/artgins/gobj-js/blob/7.13.8/src/gobj.js),
[`src/command_parser.js`](https://github.com/artgins/gobj-js/blob/7.13.8/src/command_parser.js),
[`src/stats_parser.js`](https://github.com/artgins/gobj-js/blob/7.13.8/src/stats_parser.js)

---

(js_gobj_send_event)=
## [`gobj_send_event()`](https://github.com/artgins/gobj-js/blob/7.13.8/src/gobj.js#L3935)

Sends an event to `dst` and runs the action of the current state immediately.

```javascript
gobj_send_event(dst, event, kw, src)
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `dst` | `GObj` | The destination gobj. |
| `event` | `string` | The event name. The GClass must declare it in the state that is active. |
| `kw` | `object` | The JSON payload of the event. |
| `src` | `GObj` | The gobj that sends the event. |

**Returns**

The value that the action function returns. It returns `-1` when the event is
not defined in the current state.

**Notes**

Delivery is **synchronous**. When the call returns, the action ran to the end,
together with every cascade that the action started. To send an event that must
run after the current action returns, use [`gobj_post_event()`](#js_gobj_post_event).

An event that the current state does not declare is an error, and the framework
logs it. Do not add an empty action to make the message quiet. The message shows
a bad state machine, or a sender that emits in the wrong situation.

---

(js_gobj_post_event)=
## [`gobj_post_event()`](https://github.com/artgins/gobj-js/blob/7.13.8/src/gobj.js#L3818)

Puts an event in a queue for delivery on the next turn of the browser task
queue. It is [`gobj_send_event()`](#js_gobj_send_event) with the delivery
delayed.

```javascript
gobj_post_event(dst, event, kw, src)
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `dst` | `GObj` | The destination gobj. |
| `event` | `string` | The event name. The GClass of `dst` must declare it. |
| `kw` | `object` | The JSON payload of the event. |
| `src` | `GObj` | The gobj that posts the message. |

**Returns**

Returns `0` when the message goes into the queue. Returns `-1` when the
framework refuses it, and logs the cause: `dst` is not a gobj, `dst` is under
destruction, the event name is empty, the GClass does not declare the event, or
the queue holds `MAX_POSTED_EVENTS` (10000) messages already. The queue is not
a work queue, and the limit protects that rule.

**Notes**

Use it to leave the stack that you stand on. [`gobj_publish_event()`](#js_gobj_publish_event)
dispatches synchronously, so the action of a subscriber runs inside the stack of
the publisher. To destroy or to stop that publisher there dismantles what is
still in an iteration. Post the event instead, and do the work on the next turn.

The queue drains with `setTimeout(…, 0)`, which is a **macrotask**. It does not
drain with `queueMicrotask()`. A microtask runs before the browser paints or
handles an input, so a chain of posted events holds the page. A macrotask gives
the browser its turn between one event and the next.

:::{important}
An older version of this page said "next microtask". That description is wrong.
The queue moved to `setTimeout(…, 0)` when the JS side aligned with the C
contract in `@yuneta/gobj-js` 7.10.0.
:::

Delivery is **a snapshot per turn**. The messages that are in the queue when a
drain begins are the messages that the drain delivers. An event that an action
posts while the drain runs waits for the next turn. A chain of messages that
each post the next one advances one step per turn, and the browser keeps its
turns.

[`gobj_destroy()`](lifecycle.md#js_gobj_destroy) takes the gobj out of the
queue, and the two halves are different. As **destination** the framework drops
the message. As **source** the framework only clears the pointer, because the
destination still wants its event. That event arrives with `src` set to `null`,
which every action must accept.

**Do not use a `C_TIMER` of one millisecond for this.** A deferral is not a
time. Written as a time it costs a timer, a child gobj with its own start and
stop, and the name of the event: every continuation arrives as `EV_TIMEOUT`, so
the `machine` trace says "timeout" instead of what occurs. Use a timer when
there is a real time to measure: a schedule, an inactivity window, or a backoff.

The C function takes three parameters, because there the gobj that posts is
also the destination and the source. The JS function takes four, and the
destination and the source are independent.

---

(js_gobj_posted_events_size)=
## [`gobj_posted_events_size()`](https://github.com/artgins/gobj-js/blob/7.13.8/src/gobj.js#L3876)

Gives the quantity of posted events that wait for delivery.

```javascript
gobj_posted_events_size()
```

**Returns**

The quantity of messages in the queue, as a number.

**Notes**

Use it in a test to show that a queue is empty, or in a diagnostic. Application
code must not make a decision from this value, because the value changes on
every turn of the browser task queue.

---

(js_gobj_deliver_posted_events)=
## [`gobj_deliver_posted_events()`](https://github.com/artgins/gobj-js/blob/7.13.8/src/gobj.js#L3884)

Delivers the messages that are in the queue. One turn, one snapshot.

```javascript
gobj_deliver_posted_events()
```

**Returns**

The quantity of messages that the function delivered. It does not count the
messages of a gobj that went into destruction after the message went into the
queue.

**Notes**

`gobj_post_event()` schedules this function automatically, so application code
does not call it. Call it in a **test**, to deliver a queue immediately and to
keep the test synchronous.

The function takes a snapshot of the queue, and it replaces the queue with an
empty one before the first delivery. If the actions post new messages, the
function schedules one more turn.

---

(js_gobj_publish_event)=
## [`gobj_publish_event()`](https://github.com/artgins/gobj-js/blob/7.13.8/src/gobj.js#L4752)

Sends an event to every gobj that has a subscription to it.

```javascript
gobj_publish_event(publisher, event, kw)
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `publisher` | `GObj` | The gobj that publishes the event. |
| `event` | `string` | The event name. The GClass must declare it as an output event. |
| `kw` | `object` | The JSON payload of the event. |

**Returns**

The sum of the values that the actions of the subscribers returned. It is not
the quantity of subscribers. To count the subscribers, use
[`gobj_find_subscriptions()`](#js_gobj_find_subscriptions).

**Notes**

Dispatch is **synchronous**, and it goes to one subscriber after the other.
Never stop or destroy the tree of the publisher inside the action of a
subscriber. Use [`gobj_post_event()`](#js_gobj_post_event), or set a flag and do
the work from a timeout action.

When a subscriber is optional, mark the event with `EVF_NO_WARN_SUBS` in the
event table. If you do not, the framework logs *"Publish event WITHOUT
subscribers"* on every call.

---

(js_gobj_subscribe_event)=
## [`gobj_subscribe_event()`](https://github.com/artgins/gobj-js/blob/7.13.8/src/gobj.js#L4443)

Makes a subscription of `subscriber` to an event of `publisher`.

```javascript
gobj_subscribe_event(publisher, event, kw, subscriber)
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `publisher` | `GObj` | The gobj that publishes. |
| `event` | `string` | The event name. Use `null` to subscribe to every output event. |
| `kw` | `object` | The filter and the configuration of the subscription. Use `{}` to accept every payload. |
| `subscriber` | `GObj` | The gobj that receives the event. |

**Returns**

The subscription, as a JSON object.

**Notes**

A partial object in `kw` subscribes only to the events with a payload that
matches those keys.

The `kw` accepts four configuration keys: `__config__`, `__global__`,
`__local__` and `__filter__`. See [Subscription options](#subscription-options).

Each GClass takes one of two subscription models, and writes the block in
`mt_create`. A **child** gobj uses its parent when the `subscriber`
attribute is empty. A **service** gobj subscribes only when the attribute holds
a gobj.

---

(js_gobj_unsubscribe_event)=
## [`gobj_unsubscribe_event()`](https://github.com/artgins/gobj-js/blob/7.13.8/src/gobj.js#L4565)

Deletes a subscription.

```javascript
gobj_unsubscribe_event(publisher, event, kw, subscriber)
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `publisher` | `GObj` | The gobj that publishes. |
| `event` | `string` | The event name of the subscription. |
| `kw` | `object` | The filter that the subscription used. |
| `subscriber` | `GObj` | The gobj that receives the event. |

**Returns**

Returns `0` on success, or `-1` when there is no subscription that matches.

---

(js_gobj_unsubscribe_list)=
## [`gobj_unsubscribe_list()`](https://github.com/artgins/gobj-js/blob/7.13.8/src/gobj.js#L4640)

Deletes every subscription of a list.

```javascript
gobj_unsubscribe_list(gobj, dl_subs, force)
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `gobj` | `GObj` | The gobj that owns the operation. |
| `dl_subs` | `array` | The list of subscriptions, from `gobj_find_subscriptions()`. |
| `force` | `boolean` | When it is `true`, the function also deletes the hard subscriptions. |

**Returns**

Returns `0`.

---

(js_gobj_find_subscriptions)=
## [`gobj_find_subscriptions()`](https://github.com/artgins/gobj-js/blob/7.13.8/src/gobj.js#L4661)

Finds the subscriptions that a publisher holds.

```javascript
gobj_find_subscriptions(publisher, event, kw, subscriber)
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `publisher` | `GObj` | The gobj that publishes. |
| `event` | `string` | The event name to match. Use `null` to match every event. |
| `kw` | `object` | The filter to match. Use `{}` to match every subscription. |
| `subscriber` | `GObj` | The subscriber to match. Use `null` to match every subscriber. |

**Returns**

A list of the subscriptions that match.

---

(js_gobj_find_subscribings)=
## [`gobj_find_subscribings()`](https://github.com/artgins/gobj-js/blob/7.13.8/src/gobj.js#L4733)

Finds the subscriptions that a subscriber holds. It is the opposite direction of
[`gobj_find_subscriptions()`](#js_gobj_find_subscriptions).

```javascript
gobj_find_subscribings(subscriber, event, kw, publisher)
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `subscriber` | `GObj` | The gobj that receives the events. |
| `event` | `string` | The event name to match. Use `null` to match every event. |
| `kw` | `object` | The filter to match. |
| `publisher` | `GObj` | The publisher to match. Use `null` to match every publisher. |

**Returns**

A list of the subscriptions that match.

---

(js_gobj_list_subscriptions)=
## [`gobj_list_subscriptions()`](https://github.com/artgins/gobj-js/blob/7.13.8/src/gobj.js#L4682)

Gives the subscriptions of a gobj in a form that a human reads.

```javascript
gobj_list_subscriptions(gobj2view)
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `gobj2view` | `GObj` | The gobj to examine. |

**Returns**

A list of JSON objects. Each object names the publisher, the subscriber and the
event.

---

(subscription-options)=
## Subscription options

The `kw` of [`gobj_subscribe_event()`](#js_gobj_subscribe_event) accepts four
keys that change the delivery. Every other key is a filter on the payload.

| Key | Description |
|---|---|
| `__config__` | The options of the subscription. Put the two booleans below inside this object. |
| `__config__.__hard_subscription__` | Keeps the subscription when a stop deletes the others. |
| `__config__.__own_event__` | Stops the publication when the action of this subscriber returns a negative value, and gives that value to the publisher. The subscriber owns the event. |
| `__global__` | A JSON object that the framework adds to the payload of every event of this subscription. |
| `__local__` | A list of key names that the framework removes from the payload before the delivery. |
| `__filter__` | A JSON object that the payload must match. The framework does not deliver the event when the payload does not match. |

---

## Commands and stats

(js_gobj_command)=
## [`gobj_command()`](https://github.com/artgins/gobj-js/blob/7.13.8/src/gobj.js#L5064)

Runs a command of a gobj.

```javascript
gobj_command(gobj, command, kw, src)
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `gobj` | `GObj` | The gobj that holds the command. |
| `command` | `string` | The command name, as the command table declares it. |
| `kw` | `object` | The parameters of the command. |
| `src` | `GObj` | The gobj that asks for the command. |

**Returns**

The response, as a JSON object with the keys `result`, `comment`, `schema` and
`data`. Build it with [`build_command_response()`](#js_build_command_response).

**Notes**

The GClass declares its commands with the `SDATACM` macro, and gives them to
[`command_parser()`](#js_command_parser) from the `mt_command_parser` method.
See [Commands and statistics](commands.md).

---

(js_gobj_stats)=
## [`gobj_stats()`](https://github.com/artgins/gobj-js/blob/7.13.8/src/gobj.js#L5124)

Reads the statistics of a gobj.

```javascript
gobj_stats(gobj, stats, kw, src)
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `gobj` | `GObj` | The gobj to examine. |
| `stats` | `string` | The name of the statistic. An empty string asks for every statistic. |
| `kw` | `object` | The options of the operation. |
| `src` | `GObj` | The gobj that asks. |

**Returns**

The response, as a JSON object. Build it with
[`build_stats_response()`](#js_build_stats_response).

---

(js_command_parser)=
## [`command_parser()`](https://github.com/artgins/gobj-js/blob/7.13.8/src/command_parser.js#L12)

The default parser of commands. A GClass gives it as its `mt_command_parser`
method.

```javascript
command_parser(gobj, command, kw, src)
```

**Returns**

The response of the command, as a JSON object.

---

(js_stats_parser)=
## [`stats_parser()`](https://github.com/artgins/gobj-js/blob/7.13.8/src/stats_parser.js#L12)

The default parser of statistics. A GClass gives it as its `mt_stats` method.

```javascript
stats_parser(gobj, stats, kw, src)
```

**Returns**

The response, as a JSON object.

---

(js_build_command_response)=
## [`build_command_response()`](https://github.com/artgins/gobj-js/blob/7.13.8/src/command_parser.js#L24)

Builds the JSON response of a command.

```javascript
build_command_response(gobj, result, comment, schema, data)
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `gobj` | `GObj` | The gobj that answers. |
| `result` | `number` | `0` on success, a negative number on failure. |
| `comment` | `string` | A message for the operator. |
| `schema` | `object` | The description of the columns of `data`. Use `null` when there is none. |
| `data` | `object` | The payload of the response. |

**Returns**

A JSON object with the four keys.

---

(js_build_stats_response)=
## [`build_stats_response()`](https://github.com/artgins/gobj-js/blob/7.13.8/src/stats_parser.js#L24)

Builds the JSON response of a statistics request. It takes the same parameters
as [`build_command_response()`](#js_build_command_response).

```javascript
build_stats_response(gobj, result, comment, schema, data)
```

**Returns**

A JSON object with the four keys.
