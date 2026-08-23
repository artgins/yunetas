---
title: 'gobj-ui: Map controls'
description: >-
  The controls of edition and of markers that C_YUI_MAP puts on a
  maplibre map.
---

# Map controls

Two controls for a map of maplibre. Each one takes the gobj that builds it, so
every action of the user arrives at the state machine of that gobj as an event.

**Source code:** [`src/lib_maplibre.js`](https://github.com/artgins/gobj-ui.js/blob/7.14.3/src/lib_maplibre.js)

---

(js_EditControl)=
## [`EditControl`](https://github.com/artgins/gobj-ui.js/blob/7.14.3/src/lib_maplibre.js#L18)

The control of edition of the map.

```javascript
new EditControl(gobj, config)
```

| Option | Default | Description |
|---|---|---|
| `showMarkerDrag` | `false` | Shows the button that lets the user move the markers. |

(js_MarkerControl)=
## [`MarkerControl`](https://github.com/artgins/gobj-ui.js/blob/7.14.3/src/lib_maplibre.js#L99)

The control of the markers.

```javascript
new MarkerControl(gobj, config)
```

| Option | Default | Description |
|---|---|---|
| `showCenterMap` | `true` | Shows the button that puts the map in its centre. |
| `showUserLocation` | `false` | Shows the button that goes to the position of the user. |

---

:::{note}
maplibre 6 is ESM only. An application emits the worker as a file with the name
`.js`, and gives that URL to `setWorkerUrl`. A file with the name `.mjs` arrives
with a type of content that the browser refuses, and the map draws nothing.
:::
