---
title: 'JS: Local database and transport helpers'
description: >-
  The jdb in-memory database and the file and HTTP helpers of
  @yuneta/gobj-js.
---

# Local database and transport helpers

**Source code:** [`src/helpers.js`](https://github.com/artgins/gobj-js/blob/7.29.1/src/helpers.js)

---

## `jdb`, a small database in memory

A `jdb` is a JSON object with a schema, a set of topics and a hook name. It
holds the records of a view while the page is open. It is not
[TreeDB](treedb_helpers.md), which is the graph database of the backend.

```
{
    hook:   "data",
    type:   [],
    schema: { app_menu: [], account_menu: [], … },
    topics: { app_menu: [ {id: …}, … ] }
}
```

A record is written under another one by the path of ids that leads to it, and
it goes in the hook list of its parent. Writing an `id` that is already there
updates it:

```javascript
const jdb = jdb_init({schema: {menu: []}});      // topics: {menu: []}, hook "data"
jdb_update(jdb, "menu", "", {id: "home", label: "Home"});
jdb_update(jdb, "menu", "home", {id: "users", label: "Users"});
jdb_update(jdb, "menu", "home", {id: "users", label: "People"});   // updated
// jdb.topics.menu:
//     [{id: "home", label: "Home", data: [{id: "users", label: "People"}]}]
jdb_delete(jdb, "menu", "home", {id: "users"});
jdb_get(jdb, "menu", "home");    // {id: "home", label: "Home", data: []}
```

Before gobj-js 7.26.9, `jdb_init()` created no topic and no hook.

(js_jdb_init)=
### [`jdb_init(jdb, prefix, duplicate)`](https://github.com/artgins/gobj-js/blob/7.29.1/src/helpers.js#L4865)

Prepares a `jdb`, and builds one topic for each LEAF of the schema: a key whose
value is a dict groups topics, it is not one (`{a: {b: [], c: []}}` gives the
topics ``a`b`` and ``a`c``; since 7.26.10). An empty dict is an empty group:
`{a: {}}` gives no topic at all. With
`duplicate` set to `true` the function works on a deep copy, and the caller
keeps its own object. It gives the `jdb` back.

A key of the schema that begins with two underscores is metadata, and the
function builds no topic for it.

(js_jdb_update)=
### [`jdb_update(jdb, topic_name, path, kw)`](https://github.com/artgins/gobj-js/blob/7.29.1/src/helpers.js#L4897)

Writes a record in a topic, and gives the topic back. A record whose `id` is
already in the list it goes in is updated, at the root too (since gobj-js
7.26.11; at the root it was added a second time). The function returns `null`
and writes a log error when the topic does not exist, when the record has no
`id`, when `path` is not a string (since 7.26.8; it threw before), when a
record of `path` is not there (since 7.26.10; it gave the topic back as if it
had written), or when the hook of the record `path` ends on is not a list
(since 7.26.11; it threw), when the record is not an object, or when `path`
has an empty segment (``"a`"``; since 7.26.12, it threw or was taken as
`"a"`). An update that fails leaves the jdb as it was.

```javascript
jdb_update(jdb, "menu", "home`nope", {id: "x"});   // null, logged: no "nope" under "home"
```

(js_jdb_delete)=
### [`jdb_delete(jdb, topic_name, path, kw)`](https://github.com/artgins/gobj-js/blob/7.29.1/src/helpers.js#L4976)

Deletes a record from a topic, and gives the topic back. It returns `null` with
a log error in the same cases as `jdb_update()`, and when the record to delete
is not there (since 7.26.10).

```javascript
jdb_delete(jdb, "menu", "home", {id: "nope"});     // null, logged: nothing to delete
```

(js_jdb_get)=
### [`jdb_get(jdb, topic_name, id, recursive)`](https://github.com/artgins/gobj-js/blob/7.29.1/src/helpers.js#L5053)

Gives the record with an identifier. The search goes down into the children
through the hook. With `recursive` not given the function goes down.

(js_jdb_get_by_idx)=
### [`jdb_get_by_idx(jdb, topic_name, idx)`](https://github.com/artgins/gobj-js/blob/7.29.1/src/helpers.js#L5070)

Gives the record at an index of a topic.

(js_jdb_get_topic)=
### [`jdb_get_topic(jdb, topic_name)`](https://github.com/artgins/gobj-js/blob/7.29.1/src/helpers.js#L5033)

Gives a whole topic. Returns `null` and writes a log error when the topic does
not exist.

---

## Files and HTTP

:::{important}
These two helpers are for a load at start up and for a request that is not part
of a session, such as a configuration file. They are not the transport of the
framework. A gclass speaks to a backend through `C_IEVENT_CLI`, which carries
the events, the subscriptions and the identity. See
[Built-in GClasses](builtin_gclasses.md).
:::

(js_load_json_file)=
### [`load_json_file(url, on_success, on_error)`](https://github.com/artgins/gobj-js/blob/7.29.1/src/helpers.js#L4733)

Reads a JSON file from a URL, and calls `on_success` with the object. It calls
`on_error` when the read fails.

(js_send_http_json_post)=
### [`send_http_json_post(url, data, on_response)`](https://github.com/artgins/gobj-js/blob/7.29.1/src/helpers.js#L4766)

Sends a POST request with a JSON body. It calls `on_response(status,
response_data)`.
