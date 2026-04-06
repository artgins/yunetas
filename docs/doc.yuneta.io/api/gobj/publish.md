# Publish / Subscribe

Publish events from a gobj and subscribe other gobjs to them. Subscriptions are stored on the publisher and are automatically cleaned up on destroy.

Source code:

- [`gobj.h`](https://github.com/artgins/yunetas/blob/main/kernel/c/gobj-c/src/gobj.h)
- [`gobj.c`](https://github.com/artgins/yunetas/blob/main/kernel/c/gobj-c/src/gobj.c)

(gobj_find_subscribings)=
## `gobj_find_subscribings()`

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
| `subscriber` | `hgobj` | The subscriber gobj whose subscriptions are being queried. |
| `event` | `gobj_event_t` | The event name to filter subscriptions. If NULL, all events are considered. |
| `kw` | `json_t *` | A JSON object containing additional filtering criteria, such as `__config__`, `__global__`, `__local__`, and `__filter__`. Owned by the function. |
| `publisher` | `hgobj` | The publisher gobj to filter subscriptions. If NULL, all publishers are considered. |

**Returns**

A JSON array containing the matching subscriptions. The caller owns the returned JSON object and must free it using `json_decref()`.

**Notes**

This function searches for subscriptions where `subscriber` is subscribed to events from `publisher`. The filtering criteria in `kw` allow for fine-grained selection of subscriptions.

---

(gobj_find_subscriptions)=
## `gobj_find_subscriptions()`

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
| `publisher` | `hgobj` | The publisher object whose subscriptions are being queried. |
| `event` | `gobj_event_t` | The event name to filter subscriptions. If NULL, all events are considered. |
| `kw` | `json_t *` | A JSON object containing filtering parameters such as `__config__`, `__global__`, `__local__`, and `__filter__`. If NULL, no additional filtering is applied. |
| `subscriber` | `hgobj` | The subscriber object to filter subscriptions. If NULL, all subscribers are considered. |

**Returns**

A JSON array containing the matching subscriptions. Each subscription is represented as a JSON object. The caller is responsible for freeing the returned JSON object.

**Notes**

This function is useful for inspecting active subscriptions and can be used in conjunction with [`gobj_unsubscribe_list()`](#gobj_unsubscribe_list) to remove subscriptions.

---

(gobj_list_subscriptions)=
## `gobj_list_subscriptions()`

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
## `gobj_publish_event()`

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

Returns the sum of the return values from [`gobj_send_event()`](<#gobj_send_event>) calls to all subscribers. A return value of -1 indicates that an event was owned and should not be further published.

**Notes**

If the publisher has a `mt_publish_event` method, it is called first. If it returns <= 0, the function returns immediately.
Each subscriber's `mt_publication_pre_filter` method is called before dispatching the event, allowing for filtering or modification of the event data.
If a subscriber has a `mt_publication_filter` method, it is used to determine whether the event should be sent to that subscriber.
Local and global keyword modifications are applied before sending the event.
If the event is a system event, it is only sent to subscribers that support system events.

---

(gobj_subscribe_event)=
## `gobj_subscribe_event()`

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
If a subscription with the same parameters already exists, it will be overridden.
The `__config__` field in `kw` can include options such as `__hard_subscription__` (permanent subscription) and `__own_event__` (prevents further propagation if the subscriber handles the event).
The function calls [`gobj_unsubscribe_event()`](#gobj_unsubscribe_event) to remove duplicate subscriptions before adding a new one.

---

(gobj_unsubscribe_event)=
## `gobj_unsubscribe_event()`

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
| `publisher` | `hgobj` | The GObj acting as the publisher from which the subscription should be removed. |
| `event` | `gobj_event_t` | The event name for which the subscription should be removed. |
| `kw` | `json_t *` | A JSON object containing additional parameters for filtering the subscription removal. Owned by the function. |
| `subscriber` | `hgobj` | The GObj acting as the subscriber that should be unsubscribed from the event. |

**Returns**

Returns 0 on success, or -1 if the subscription was not found or an error occurred.

**Notes**

If the `event` is not found in the publisher's output event list, the function will return an error unless the publisher has the `gcflag_no_check_output_events` flag set.
If multiple subscriptions match the given parameters, all of them will be removed.
If no matching subscription is found, an error is logged.
The function decrements the reference count of `kw` before returning.

---

(gobj_unsubscribe_list)=
## `gobj_unsubscribe_list()`

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
## `gobj_list_subscribings()`

*Description pending — signature extracted from header.*

```C
json_t *gobj_list_subscribings(
    hgobj gobj,
    gobj_event_t event,
    json_t *kw,
    hgobj subscriber
);
```

---

(gobj_subs_desc)=
## `gobj_subs_desc()`

*Description pending — signature extracted from header.*

```C
const sdata_desc_t *gobj_subs_desc(void);
```

---

