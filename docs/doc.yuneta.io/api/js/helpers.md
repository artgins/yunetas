---
title: 'JS: Helpers'
description: >-
  The index of the helper families of @yuneta/gobj-js, and where each
  one lives.
---

# Helpers

`@yuneta/gobj-js` ships the helpers that a gclass needs, and it gives them the
names that the C side uses. A gclass that moves between the two languages keeps
its calls.

**Source code:** [`src/helpers.js`](https://github.com/artgins/gobj-js/blob/7.16.2/src/helpers.js),
[`src/sprintf.js`](https://github.com/artgins/gobj-js/blob/7.16.2/src/sprintf.js)

| Family | Page |
|---|---|
| The JSON operations of jansson, and the type predicates | [JSON and type helpers](helpers_json.md) |
| Reading and writing a `kw`, the `kwid` records, and the inter-event metadata | [`kw`, `kwid` and inter-event helpers](helpers_kw.md) |
| The comparisons of strings, the lists, the time and the general utilities | [String, list and general helpers](helpers_str.md) |
| Building elements, escaping untrusted values, and the language | [DOM and i18n helpers](helpers_dom.md) |
| The database in memory, the files, the HTTP and the tokens | [Local database, transport and token helpers](helpers_data.md) |
| The writers of logs and traces, and the formatting | [Logging](logging.md) |

Every symbol of the package is in the
[JS API index](../appendix_js_api_index.md).

---

## Two rules that a caller trips on

**A path uses the back-tick, and not the point.** ``kw_get_str(gobj, kw,
"__md_iev__`__msg_type__", "", 0)`` goes down two levels. A path with a point in
it is one key that holds a point in its name.

**Most helpers take `gobj` first, and a few do not.**
[`kw_has_key()`](helpers_kw.md#js_kw_has_key),
[`kw_pop()`](helpers_kw.md#js_kw_pop),
[`kw_match_simple()`](helpers_kw.md#js_kw_match_simple),
[`msg_iev_read_key()`](helpers_kw.md#js_msg_iev_read_key) and
[`msg_iev_write_key()`](helpers_kw.md#js_msg_iev_write_key) take none. The
`gobj` names the gobj in a log message, and it changes nothing else.
