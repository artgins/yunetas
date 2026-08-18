---
title: 'JS: Local database, transport and token helpers'
description: >-
  The jdb in-memory database, the file and HTTP helpers, and the JWT
  readers of @yuneta/gobj-js.
---

# Local database, transport and token helpers

**Source code:** [`src/helpers.js`](https://github.com/artgins/gobj-js/blob/7.12.0/src/helpers.js)

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

(js_jdb_init)=
### [`jdb_init(jdb, prefix, duplicate)`](https://github.com/artgins/gobj-js/blob/7.12.0/src/helpers.js#L2470)

Prepares a `jdb`, and builds one topic for each entry of the schema. With
`duplicate` set to `true` the function works on a deep copy, and the caller
keeps its own object. It gives the `jdb` back.

A key of the schema that begins with two underscores is metadata, and the
function builds no topic for it.

(js_jdb_update)=
### [`jdb_update(jdb, topic_name, path, kw)`](https://github.com/artgins/gobj-js/blob/7.12.0/src/helpers.js#L2495)

Writes a record in a topic. The function writes a log error when the topic does
not exist.

(js_jdb_delete)=
### [`jdb_delete(jdb, topic_name, path, kw)`](https://github.com/artgins/gobj-js/blob/7.12.0/src/helpers.js#L2549)

Deletes a record from a topic.

(js_jdb_get)=
### [`jdb_get(jdb, topic_name, id, recursive)`](https://github.com/artgins/gobj-js/blob/7.12.0/src/helpers.js#L2620)

Gives the record with an identifier. The search goes down into the children
through the hook. With `recursive` not given the function goes down.

(js_jdb_get_by_idx)=
### [`jdb_get_by_idx(jdb, topic_name, idx)`](https://github.com/artgins/gobj-js/blob/7.12.0/src/helpers.js#L2637)

Gives the record at an index of a topic.

(js_jdb_get_topic)=
### [`jdb_get_topic(jdb, topic_name)`](https://github.com/artgins/gobj-js/blob/7.12.0/src/helpers.js#L2600)

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
### [`load_json_file(url, on_success, on_error)`](https://github.com/artgins/gobj-js/blob/7.12.0/src/helpers.js#L2338)

Reads a JSON file from a URL, and calls `on_success` with the object. It calls
`on_error` when the read fails.

(js_send_http_json_post)=
### [`send_http_json_post(url, data, on_response)`](https://github.com/artgins/gobj-js/blob/7.12.0/src/helpers.js#L2371)

Sends a POST request with a JSON body. It calls `on_response(status,
response_data)`.

---

## Tokens

(js_jwtDecode)=
### [`jwtDecode(jwt)`](https://github.com/artgins/gobj-js/blob/7.12.0/src/helpers.js#L2709)

Reads a JWT, and gives its three parts back.

(js_jwt2json)=
### [`jwt2json(jwt, what)`](https://github.com/artgins/gobj-js/blob/7.12.0/src/helpers.js#L2757)

Reads one part of a JWT as JSON. `what` accepts `"header"`, `"payload"` and
`"signature"`, and the default is `"payload"`.

:::{warning}
Both functions **read** a token. Neither one verifies it. The signature, the
issuer and the expiry are checked by the backend, and a browser cannot check
them: it holds no key, and a token that it reads is a token that somebody can
write. Use the payload to draw a name on the screen. Never use it to decide a
permission.
:::
