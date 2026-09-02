---
title: 'gobj-ui: assets'
description: >-
  The bytes a treedb node owns but cannot hold: reading the link, taking
  either shape the backend answers with, and saying so when one is missing.
---

# Assets

**Source code:** [`src/yui_asset.js`](https://github.com/artgins/gobj-ui.js/blob/7.23.47/src/yui_asset.js)

A treedb node often owns something that is not JSON: a photo, a plan, a clip.
Those bytes cannot live in the treedb, so the SDK's
[`C_ASSETS`](../gclass/data.md) keeps them in a directory it owns and the node
names one with an **fkey**.

`get-asset` answers in one of two shapes, and the **backend** decides which:

```json
{"mode": "url",    "url": "/media/ab/cd/<id>.jpg?e=<expires>&s=<token>"}
{"mode": "inline", "content_type": "image/jpeg", "content64": "..."}
```

It signs a URL when a web server sits in front of the store and hands over the
bytes when there is none, so a consumer has **one** code path and a node with
no web server still shows its images instead of showing nothing.

**These helpers do not talk to the backend.** Asking is an action and belongs
in the view's own state machine; they are the two ends of it — read the id out
of the link before asking, and turn the answer into an element afterwards.

---

## Reading the link

A column that holds a link comes back in one of three shapes, and **which one
is the reader's choice, not the schema's**: the stored `"assets^<id>^as_foto"`,
the bare `"<id>"` that `fkey_only_id` collapses it to, or an expanded
`{id}`. Each can come alone or in a list — and an **unset** single-valued fkey
is still an empty list. All of them are read.

(js_yui_asset_ids)=
### [`yui_asset_ids(ref)`](https://github.com/artgins/gobj-ui.js/blob/7.23.47/src/yui_asset.js#L63)

Every asset id the column names, in order. Always an array, possibly empty.

(js_yui_asset_id)=
### [`yui_asset_id(ref)`](https://github.com/artgins/gobj-ui.js/blob/7.23.47/src/yui_asset.js#L75)

The first id the column names, or `null`. An empty column answers nothing
rather than throwing.

---

## Showing it

(js_yui_asset_src)=
### [`yui_asset_src(answer)`](https://github.com/artgins/gobj-ui.js/blob/7.23.47/src/yui_asset.js#L88)

Turns either shape of a `get-asset` answer into something an element can
load: the signed URL as it comes, or a `data:` URL built from the inline
bytes.

Answers `null` — **never an empty string** — when the answer carries neither.
An `<img src="">` reloads the page in some browsers, which is a worse failure
than the one being reported.

(js_yui_asset_element)=
### [`yui_asset_element(answer, opts)`](https://github.com/artgins/gobj-ui.js/blob/7.23.47/src/yui_asset.js#L152)

The element for one asset, built from the **content type the backend stored**
and not from the name a person typed: `<img>`, `<video>` or `<audio>`. Video
and audio are assets too, and an `<img>` whose src is a film shows the broken
box this exists to remove.

It wires `onerror`, so a dead element is **replaced** by the marker below
whatever the reason — an expired signature, a blob gone from the store, an
unsupported codec.

`opts.detail` is what the marker shows: pass the original name or the source
path, the thing a person can act on. `opts.alt` sets the alt text of an image,
`opts.key` overrides the i18n key of the marker, and `opts.class` is added to
the element.

(js_yui_asset_missing)=
### [`yui_asset_missing(detail, opts)`](https://github.com/artgins/gobj-ui.js/blob/7.23.47/src/yui_asset.js#L112)

The marker for an asset that is not there, on its own.

A missing image used to leave a broken box and no word about it, which is
indistinguishable from a slow one and from a bug — 47 such holes in one day on
one deployment before anybody noticed. The marker takes the space the image
would have taken, so a card does not jump, and it says **which** asset is
missing.

The label carries its i18n key (`asset not available`), so it follows a
language change; `detail` is DATA and is never translated. Import
`src/yui_asset.css` for it to have a shape.

---

## Example

```js
import {yui_asset_id, yui_asset_element} from "@yuneta/gobj-ui/src/yui_asset.js";
import "@yuneta/gobj-ui/src/yui_asset.css";

//  in the view's state machine, not in a DOM callback:
const id = yui_asset_id(device.foto);
if(id) {
    gobj_send_event(gobj, "EV_ASK_ASSET", {asset_id: id, slot: "foto"}, gobj);
}

//  ...and when the answer arrives, in the action:
$box.appendChild(yui_asset_element(answer, {detail: device.foto_name}));
```
