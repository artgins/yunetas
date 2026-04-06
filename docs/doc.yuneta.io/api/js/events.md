# Event System

GObjects communicate exclusively via events carrying JSON key-value
payloads (`kw`). There is no direct method calling — every interaction
goes through the FSM event dispatcher.

## Sending events

```javascript
gobj_send_event(gobj, event, kw, src)        // direct send, runs synchronously
gobj_publish_event(gobj, event, kw)          // publish to all subscribers
gobj_post_event(gobj, event, kw, src)        // deferred (microtask)
```

`gobj_send_event` runs the action function immediately and returns its
result. `gobj_publish_event` dispatches to every GObject that has
subscribed via `gobj_subscribe_event`. `gobj_post_event` queues the
event to run on the next microtask — useful to avoid deep recursion
or to break up long work.

## Subscriptions

```javascript
gobj_subscribe_event  (publisher, event, kw, subscriber)
gobj_unsubscribe_event(publisher, event, kw, subscriber)
gobj_unsubscribe_list (publisher, dl_subs, force)     // force=true also removes hard subs

gobj_find_subscriptions (publisher,  event, kw, subscriber)
gobj_list_subscriptions (gobj)
gobj_find_subscribings  (subscriber, event, kw, publisher)
```

Pass `{}` as the filter `kw` to match all payloads. Pass a partial
object to subscribe only to events whose payload matches those keys.

## Commands and stats

```javascript
gobj_command(gobj, command, kw, src)         // → response JSON
gobj_stats(gobj, stats, kw, src)             // → response JSON

build_command_response(gobj, result, comment, schema, data)
build_stats_response(gobj, result, comment, schema, data)
```

Commands and stats are declared in the GClass's `command_table` and
look a lot like events, but the response is a synchronously returned
JSON object (suitable for wiring up to a REST endpoint or a CLI tool).
