# Publish / Subscribe

Publish events from a gobj and subscribe other gobjs to them. Subscriptions are stored on the publisher and are automatically cleaned up on destroy.

Source code:

- [`gobj.h`](https://github.com/artgins/yunetas/blob/7.25.7/kernel/c/gobj-c/src/gobj.h)
- [`gobj.c`](https://github.com/artgins/yunetas/blob/7.25.7/kernel/c/gobj-c/src/gobj.c)

(gobj_find_subscribings)=
## [`gobj_find_subscribings()`](https://github.com/artgins/yunetas/blob/7.25.7/kernel/c/gobj-c/src/gobj.c#L9164)

Returns a list of subscriptions where the given `subscriber` is subscribed to events from various publishers.

```C
json_t *gobj_find_subscribings(
    hgobj gobj,
    gobj_event_t event,
    json_t *kw,
    hgobj publisher
);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `subscriber` | `hgobj` | The subscriber gobj whose subscriptions are queried. |
| `event` | `gobj_event_t` | The event name to filter subscriptions. If NULL, all events are considered. |
| `kw` | `json_t *` | A JSON object containing additional filtering criteria, such as `__config__`, `__global__`, `__local__`, and `__filter__`. Owned by the function. |
| `publisher` | `hgobj` | The publisher gobj to filter subscriptions. If NULL, all publishers are considered. |

**Returns**

A JSON array containing the matching subscriptions. The caller owns the returned JSON object and must free it using `json_decref()`.

**Notes**

This function searches for subscriptions where `subscriber` is subscribed to events from `publisher`. The filtering criteria in `kw` allow for fine-grained selection of subscriptions.

---

(gobj_find_subscriptions)=
## [`gobj_find_subscriptions()`](https://github.com/artgins/yunetas/blob/7.25.7/kernel/c/gobj-c/src/gobj.c#L9138)

Retrieves a list of event subscriptions for a given publisher, filtering by event, keyword parameters, and subscriber.

```C
json_t *gobj_find_subscriptions(
    hgobj gobj,
    gobj_event_t event,
    json_t *kw,
    hgobj subscriber
);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `publisher` | `hgobj` | The publisher object whose subscriptions are queried. |
| `event` | `gobj_event_t` | The event name to filter subscriptions. If NULL, all events are considered. |
| `kw` | `json_t *` | A JSON object containing filtering parameters such as `__config__`, `__global__`, `__local__`, and `__filter__`. If NULL, no additional filtering is applied. |
| `subscriber` | `hgobj` | The subscriber object to filter subscriptions. If NULL, all subscribers are considered. |

**Returns**

A JSON array containing the matching subscriptions. Each subscription is represented as a JSON object. The caller is responsible for freeing the returned JSON object.

**Notes**

This function is useful for inspecting active subscriptions and can be used in conjunction with [`gobj_unsubscribe_list()`](#gobj_unsubscribe_list) to remove subscriptions.

---

(gobj_list_subscriptions)=
## [`gobj_list_subscriptions()`](https://github.com/artgins/yunetas/blob/7.25.7/kernel/c/gobj-c/src/gobj.c#L9221)

Retrieves a list of event subscriptions for a given `hgobj`. The function returns details about events the object is subscribed to and the objects that have subscribed to its events.

```C
json_t *gobj_list_subscriptions(
    hgobj gobj,
    gobj_event_t event,
    json_t *kw,
    hgobj subscriber
);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `gobj2view` | `hgobj` | The `hgobj` whose subscriptions are to be listed. |

**Returns**

A `json_t *` object containing two lists: `subscriptions` (events published by `gobj2view` and their subscribers) and `subscribings` (events `gobj2view` is subscribed to). Each entry includes event names, publisher, and subscriber details.

**Notes**

The returned JSON object must be managed by the caller. The function internally calls [`gobj_find_subscriptions()`](#gobj_find_subscriptions) and [`gobj_find_subscribings()`](#gobj_find_subscribings) to gather the relevant data.

---

(gobj_publish_event)=
## [`gobj_publish_event()`](https://github.com/artgins/yunetas/blob/7.25.7/kernel/c/gobj-c/src/gobj.c#L9275)

The `gobj_publish_event` function publishes an event from a given publisher to all its subscribers, applying optional filters and transformations before dispatching the event.

```C
int gobj_publish_event(
    hgobj        publisher,
    gobj_event_t event,
    json_t       *kw  // this kw extends kw_request.
);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `publisher` | `hgobj` | The gobj (generic object) that is publishing the event. |
| `event` | `gobj_event_t` | The event to be published. |
| `kw` | `json_t *` | A JSON object containing additional data for the event. This object is extended with the subscription's global parameters. |

**Returns**

Returns the sum of the return values from [`gobj_send_event()`](<#gobj_send_event>) calls to all subscribers. A return value of -1 indicates that an event was owned and must not be further published.

**Notes**

If the publisher has a `mt_publish_event` method, it is called first. If it returns <= 0, the function returns immediately.
Each subscriber's `mt_publication_pre_filter` method is called before dispatching the event. This allows for filtering or modification of the event data.
If a subscriber has a `mt_publication_filter` method, it is used to determine whether the event must be sent to that subscriber.
If the event is a system event, it is only sent to subscribers that support system events.

**One kw, or a twin.** Every subscriber gets the SAME kw (`kw_incref()`), which
costs nothing however many subscribers there are, unless its subscription
rewrites it: a subscription with a `__local__` (keys removed) or a
`__global__` (keys added) gets a twin of its own ([`kw_twin()`](#kw_twin): a
new top level, the values and the binary fields shared and increfed, so its
cost does not grow with the size of the event), and only the twin is changed.
The `__filter__` of each subscription is evaluated on the publisher's kw as it
came. So what one subscription changes reaches no other subscriber, and the
publisher gets its kw back as it gave it. A receiver that changes the kw it
got (as `C_IEVENT_SRV` does to send it on) must change a twin of its own when
anybody else holds it, and a nested value it changes in place (the
`__md_iev__` stack, for `C_IEVENT_SRV`) a copy of that value: a twin shares
them with the publisher, and the values of a `__global__` with the
subscription. Up to 7.25.4 the kw was shared always, and the `__local__` and
`__global__` of one subscription changed the event of every subscriber after
it -- including those of a remote peer, see
[What a peer may put in a subscription](#gclass-c-ievent-srv).

```C
/*  Two subscribers: `a` gets {"x":1,"tag":"a"}, `b` gets {"x":1}  */
gobj_subscribe_event(publisher, EV_X, json_pack("{s:{s:s}}", "__global__", "tag", "a"), a);
gobj_subscribe_event(publisher, EV_X, 0, b);
gobj_publish_event(publisher, EV_X, json_pack("{s:i}", "x", 1));
```

---

(gobj_subscribe_event)=
## [`gobj_subscribe_event()`](https://github.com/artgins/yunetas/blob/7.25.7/kernel/c/gobj-c/src/gobj.c#L8725)

The `gobj_subscribe_event` function subscribes a `subscriber` GObj to an `event` emitted by a `publisher` GObj, with optional configuration parameters.

```C
json_t *gobj_subscribe_event(
    hgobj         publisher,
    gobj_event_t  event,
    json_t       *kw,
    hgobj         subscriber
);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `publisher` | `hgobj` | The GObj that emits the event. |
| `event` | `gobj_event_t` | The event to subscribe to. |
| `kw` | `json_t *` | A JSON object containing subscription options, including `__config__`, `__global__`, `__local__`, and `__filter__`. |
| `subscriber` | `hgobj` | The GObj that will receive the event notifications. |

**Returns**

Returns a JSON object representing the subscription if successful, or `NULL` on failure.

**Notes**

The `event` must be in the publisher's output event list unless the `gcflag_no_check_output_events` flag is set.
If a subscription with the same parameters already exists, it will be overridden: the function logs a warning (*"subscription(s) REPEATED, will be deleted and override"*, with the kw capped to 256 bytes and no stack trace: a repeat is the caller's, not a broken invariant), removes it with [`gobj_unsubscribe_list()`](#gobj_unsubscribe_list) without `force`, and adds the new one.
The `__config__` field in `kw` can include options such as `__hard_subscription__` (permanent subscription), `__own_event__` (prevents further propagation if the subscriber handles the event) and `__rename_event_name__` (the event is delivered under another name, and the kw gets `__original_event_name__`).
These three keys are taken out of the `__config__` that the subscription stores (they become its flags), and a renamed event adds `__original_event_name__` to the stored `__global__`. The `kw` of a repeat, and of [`gobj_unsubscribe_event()`](#gobj_unsubscribe_event), is compared as it would be stored, so the same `kw` always finds the subscription it made. Up to 7.25.4 the `kw` was compared as it came: a repeated `__hard_subscription__`, `__own_event__` or `__rename_event_name__` subscription was made a second time (each event arrived twice), and the withdrawal of an `__own_event__` or `__rename_event_name__` subscription with the same `kw` found nothing.
A HARD subscription is not overridden, because only `gobj_unsubscribe_list()` with `force` removes it. When one matches, no new subscription is made: the function logs a warning (*"Hard subscription REPEATED, the one there is kept and returned"*) and returns the hard subscription that is there. `__hard_subscription__` itself is not compared: subscribing hard twice with the same `kw` gives one subscription. A hard subscription over a PLAIN one with the same parameters replaces it, as any override does. Up to 7.25.4 a repeated hard subscription was made a second time with no log, and the subscriber got each event twice; a hard one over a plain one did not replace it either.

```C
json_t *subs1 = gobj_subscribe_event(publisher, EV_ON_MESSAGE,
    json_pack("{s:{s:b}}", "__config__", "__hard_subscription__", 1), subscriber);
json_t *subs2 = gobj_subscribe_event(publisher, EV_ON_MESSAGE,
    json_pack("{s:{s:b}}", "__config__", "__hard_subscription__", 1), subscriber);  // a WARNING
// subs2 == subs1: one subscription, each event arrives once

json_t *kw_own = json_pack("{s:{s:b}}", "__config__", "__own_event__", 1);
gobj_subscribe_event(publisher, EV_ON_MESSAGE, json_incref(kw_own), subscriber);
gobj_subscribe_event(publisher, EV_ON_MESSAGE, json_incref(kw_own), subscriber); // overridden, a WARNING
gobj_unsubscribe_event(publisher, EV_ON_MESSAGE, kw_own, subscriber);           // the same kw removes it
```

---

(gobj_unsubscribe_event)=
## [`gobj_unsubscribe_event()`](https://github.com/artgins/yunetas/blob/7.25.7/kernel/c/gobj-c/src/gobj.c#L8976)

Removes a subscription from a publisher to a subscriber for a specific event in the GObj system.

```C
int gobj_unsubscribe_event(
    hgobj         publisher,
    gobj_event_t  event,
    json_t       *kw,
    hgobj         subscriber
);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `publisher` | `hgobj` | The GObj acting as the publisher from which the subscription must be removed. |
| `event` | `gobj_event_t` | The event name for which the subscription must be removed. |
| `kw` | `json_t *` | A JSON object containing additional parameters for filtering the subscription removal. Owned by the function. |
| `subscriber` | `hgobj` | The GObj acting as the subscriber that must be unsubscribed from the event. |

**Returns**

Returns -1 when `publisher` or `subscriber` is NULL (logged); otherwise 0, also when nothing was removed (the log says why).

**Notes**

If the `event` is not found in the publisher's output event list, an error is logged and nothing is removed, unless the publisher has the `gcflag_no_check_output_events` flag set.
If multiple subscriptions match the given parameters, all of them will be removed.
If no matching subscription is found, an error is logged (*"No subscription found"*).
A HARD subscription (`__hard_subscription__` in its `__config__`) is never removed here: only [`gobj_unsubscribe_list()`](#gobj_unsubscribe_list) with `force` removes it. Since 7.25.5 a hard subscription that matches is logged as a warning (*"Hard subscription not removed, only gobj_unsubscribe_list() with force removes it"*, with the count in `hard`); up to 7.25.4 it was counted as removed, and nothing was logged.
The function decrements the reference count of `kw` before returning.

```C
gobj_subscribe_event(publisher, EV_ON_MESSAGE,
    json_pack("{s:{s:b}}", "__config__", "__hard_subscription__", 1), subscriber);
gobj_unsubscribe_event(publisher, EV_ON_MESSAGE, 0, subscriber);   // kept, a WARNING

json_t *dl_subs = gobj_find_subscriptions(publisher, EV_ON_MESSAGE, 0, subscriber);
gobj_unsubscribe_list(publisher, dl_subs, TRUE);                   // removed
```

---

(gobj_unsubscribe_list)=
## [`gobj_unsubscribe_list()`](https://github.com/artgins/yunetas/blob/7.25.7/kernel/c/gobj-c/src/gobj.c#L9114)

Removes a list of event subscriptions from their respective publishers, optionally forcing the removal of hard subscriptions.

```C
int gobj_unsubscribe_list(
    hgobj gobj,
    json_t *dl_subs,
    BOOL force
);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `dl_subs` | `json_t *` | A JSON array containing the subscriptions to be removed. Each element represents a subscription. |
| `force` | `BOOL` | If set to `TRUE`, hard subscriptions will also be removed. |

**Returns**

Returns `0` upon successful removal of the subscriptions.

**Notes**

Each subscription in `dl_subs` is checked and removed from both the publisher's and subscriber's subscription lists.

---

(gobj_list_subscribings)=
## [`gobj_list_subscribings()`](https://github.com/artgins/yunetas/blob/7.25.7/kernel/c/gobj-c/src/gobj.c#L9248)

Returns a JSON array describing the subscriptions where the given gobj is acting as a subscriber. Each element in the returned array contains human-readable information about a matching subscription (publisher name, event, subscriber name, flags and more.). The results can be filtered by event, kw sub-dictionaries, and subscriber.

```C
json_t *gobj_list_subscribings(
    hgobj gobj,
    gobj_event_t event,
    json_t *kw,
    hgobj subscriber
);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `gobj` | `hgobj` | The GObj whose outgoing subscriptions (subscribings) will be listed. |
| `event` | `gobj_event_t` | Filter by this event. Pass `NULL` to match all events. |
| `kw` | `json_t *` | A JSON object with optional sub-dictionaries (`__config__`, `__global__`, `__local__`) used to filter subscriptions. Pass `NULL` to match all. |
| `subscriber` | `hgobj` | Filter by this subscriber gobj. Pass `NULL` to match all subscribers. |

**Returns**

A new JSON array (owned by the caller) containing one JSON object per matching subscription. Each object includes details such as publisher name, subscriber name, event, flags, and kw sub-dictionaries.

**Notes**

Internally calls `gobj_find_subscribings()` to locate matching subscriptions and then converts each one to a human-readable JSON representation via `get_subs_info()`.

---

(gobj_subs_desc)=
## [`gobj_subs_desc()`](https://github.com/artgins/yunetas/blob/7.25.7/kernel/c/gobj-c/src/gobj.c#L8297)

Returns a pointer to the internal subscription schema descriptor (`sdata_desc_t` array). This schema defines the structure of a subscription record, including fields such as `publisher`, `subscriber`, `event`, `renamed_event`, `subs_flag`, `__config__`, `__global__`, `__local__`, `__filter__`, and `__service__`.

```C
const sdata_desc_t *gobj_subs_desc(void);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `-` | `-` | This function does not take any parameters. |

**Returns**

A pointer to the static `sdata_desc_t` array that describes the subscription data structure. The returned pointer references internal static data and must not be freed or modified.

**Notes**

This is useful for introspection or for building subscription records programmatically using the same schema the framework uses internally.

---

