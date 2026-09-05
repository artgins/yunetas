---
title: 'gobj-ui: assets'
description: >-
  The bytes a treedb node owns but cannot hold: reading the link, taking
  either shape the backend answers with, and saying so when one is missing.
---

# Assets

**Source code:** [`src/yui_asset.js`](https://github.com/artgins/gobj-ui.js/blob/7.23.63/src/yui_asset.js)

A treedb node often owns something that is not JSON: a photo, a plan, a clip.
Those bytes cannot live in the treedb's records, so the treedb keeps them on
disk under its own directory and the node names one with an **fkey** into the
system topic `__assets__` — a column flagged `['fkey', 'file']`, see
[File columns](../gclass/data.md#treedb-file-columns). The SDK's
[`C_ASSETS`](../gclass/data.md#gclass-c-assets) is the way out: it publishes
the bytes to a browser.

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
is the reader's choice, not the schema's**: the stored `"__assets__^<id>^as_devices_foto"`,
the bare `"<id>"` that `fkey_only_id` collapses it to, or an expanded
`{id}`. Each can come alone or in a list — and an **unset** single-valued fkey
is still an empty list. All of them are read.

(js_yui_asset_ids)=
### [`yui_asset_ids(ref)`](https://github.com/artgins/gobj-ui.js/blob/7.23.63/src/yui_asset.js#L63)

Every asset id the column names, in order. Always an array, possibly empty.

(js_yui_asset_id)=
### [`yui_asset_id(ref)`](https://github.com/artgins/gobj-ui.js/blob/7.23.63/src/yui_asset.js#L75)

The first id the column names, or `null`. An empty column answers nothing
rather than throwing.

---

## Showing it

(js_yui_asset_src)=
### [`yui_asset_src(answer)`](https://github.com/artgins/gobj-ui.js/blob/7.23.63/src/yui_asset.js#L88)

Turns either shape of a `get-asset` answer into something an element can
load: the signed URL as it comes, or a `data:` URL built from the inline
bytes.

Answers `null` — **never an empty string** — when the answer carries neither.
An `<img src="">` reloads the page in some browsers, which is a worse failure
than the one being reported.

(js_yui_asset_element)=
### [`yui_asset_element(answer, opts)`](https://github.com/artgins/gobj-ui.js/blob/7.23.63/src/yui_asset.js#L152)

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
### [`yui_asset_missing(detail, opts)`](https://github.com/artgins/gobj-ui.js/blob/7.23.63/src/yui_asset.js#L112)

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

---

(gobj-ui-file-column)=
## Filling one: the `file` column control

Reading an asset is the half above. **Writing one** is a column flagged
`['fkey','file']` and the control the form draws for it
([`src/yui_file_field.js`](https://github.com/artgins/gobj-ui.js/blob/7.23.63/src/yui_file_field.js)). The form and the treedb topic table use it
themselves — a consumer needs these only to build a file UI of its own.

Three things about the shape are not obvious, and each one is a bug it
avoids:

- **The file is read at SAVE, not at pick.** An `<input type=file>` hands over
  a `File`, which is a *reference* and not the bytes. Picking shows the name
  and the size the `File` already carries and reads nothing, so cancelling a
  form does not mean a 40 MB video was read for nothing.
- **A `File` cannot travel in a kw.** A kw is plain JSON — the machine trace
  serialises it — and a `File` is a host object. The form **keeps** it and the
  host asks for it at save, with `gobj_command(form, "get_picked_files")`.
- **Reading is a promise, so saving stops being synchronous.** A resolved
  promise is an OS notification and enters the machine as an event
  (`EV_FILES_READ` / `EV_FILES_FAILED`), never as a chain of callbacks the
  trace cannot see.

(js_YUI_FILE_ACCEPT)=
### [`YUI_FILE_ACCEPT`](https://github.com/artgins/gobj-ui.js/blob/7.23.63/src/yui_file_field.js#L50)

What the treedb's default ceiling holds, as an `accept` attribute: a hint to
the file dialog and **never a check**. The check is treedb's, on the bytes, at
the door — a browser filter is a convenience, and a client that means to lie
walks past it.

(js_yui_file_control)=
### [`yui_file_control(gobj, {name, value, readonly, accept, on_pick})`](https://github.com/artgins/gobj-ui.js/blob/7.23.63/src/yui_file_field.js#L237)

The control: a button, what it is holding, and the way to clear it. It hangs
its state on the returned element — `yui_file` (the picked `File`, or null),
`yui_file_value` (the id the column keeps) and `yui_file_render()`, because
writing a property fires nothing and the host has to be able to say *"now draw
what you are holding"*.

The `<input type="file">` is hidden and driven by the button: a bare file
input cannot be styled and says *"No file chosen"* in the **browser's**
language, not the app's.

(js_yui_file_read)=
### [`yui_file_read(file)`](https://github.com/artgins/gobj-ui.js/blob/7.23.63/src/yui_file_field.js#L145)

Reads one picked file into what the manifest carries —
`{content64, content_type, original_name, size, id}`. Async: this is the
promise the host awaits.

(js_yui_files_manifest)=
### [`yui_files_manifest(picks, record)`](https://github.com/artgins/gobj-ui.js/blob/7.23.63/src/yui_file_field.js#L172)

Turns what was read into the write: the record takes the **id** in each picked
column, and the bytes go beside it in `__files__`. That key is not a column —
it is an instruction to the treedb write path, consumed and dropped at the
door. A field that *carries* the bytes and a field that *keeps* them are one
word apart and 460 MB of RAM apart.

(js_yui_file_sha256)=
### [`yui_file_sha256(buffer)`](https://github.com/artgins/gobj-ui.js/blob/7.23.63/src/yui_file_field.js#L96)

The sha256 as lowercase hex, **or `null`** — and null is a legal answer, not a
failure: `crypto.subtle` exists only in a secure context, so a dev server on
plain `http` has none. Then the column goes empty and treedb fills the id from
what arrives. What the hash buys is an integrity check the backend can make (a
wrong id with good bytes is refused), never the identity itself.

(js_yui_array_buffer_to_base64)=
### [`yui_array_buffer_to_base64(buffer)`](https://github.com/artgins/gobj-ui.js/blob/7.23.63/src/yui_file_field.js#L125)

Base64 in chunks, because `String.fromCharCode(...bytes)` spreads the whole
array onto the call stack and a few hundred KB is already a `RangeError` —
which, for a file picker, is every file that matters.

(js_yui_file_size_label)=
### [`yui_file_size_label(bytes)`](https://github.com/artgins/gobj-ui.js/blob/7.23.63/src/yui_file_field.js#L68)

A size a person reads: bytes up to 1 KB, then one decimal.

(js_yui_file_id_label)=
### [`yui_file_id_label(id)`](https://github.com/artgins/gobj-ui.js/blob/7.23.63/src/yui_file_field.js#L211)

An id a person can look at: a sha256 in full is 64 characters of noise in a
form, so it is shortened, and the whole of it belongs in the `title`.
