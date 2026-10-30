---
title: 'JS: gobj-ui API'
description: >-
  The reference of @yuneta/gobj-ui, the UI library of Yuneta: the
  declarative shell, the component gclasses and the helpers.
---

# gobj-ui API

`@yuneta/gobj-ui` is the UI library. It gives a **declarative shell** that draws
the frame of an application from one JSON file, a set of component gclasses, and
the helpers that go with them.

It sits on top of [`@yuneta/gobj-js`](../js/js_api.md), and it holds the same
rules: a DOM callback translates a notification of the browser into an event,
and the work happens in an action of a state machine.

**Source code:** [github.com/artgins/gobj-ui.js](https://github.com/artgins/gobj-ui.js/tree/7.29.2) —
**version:** `7.29.2`

---

## Install

```bash
npm install @yuneta/gobj-ui
```

An application imports by the name of the package. The exports map of the
package gives both the barrel and the modules of the source:

```javascript
import { register_c_yui_shell } from "@yuneta/gobj-ui";
import { yui_shell_show_modal } from "@yuneta/gobj-ui/src/shell_modals.js";
```

:::{important}
A local change under `kernel/js/gobj-ui` reaches no application until somebody
publishes the package. Every consumer takes the library from the registry.
Publish the library, then raise the range in the application.
:::

:::{warning}
The library installs its own copy of the third-party libraries that it shares
with an application: i18next, `@antv/g6`, maplibre-gl, tabulator-tables,
tom-select and uplot. An application that depends on one of them too must add
`resolve.dedupe` for the whole list in its `vite.config.js`. Without it, a
component that imports a value of a module, such as the `t` of i18next, takes a
second copy that nobody started, and the component draws nothing.
:::

---

## The two lines

The repository carries two lines, and each one has its own consumers.

| Line | Tag | npm | State |
|---|---|---|---|
| `main` | `2.0.0` and later | `latest` (`7.x`, the first two numbers are the SDK's) | The v2 line, with the declarative shell. Every new work lands here. |
| `v1` | `1.0.4` | `legacy` | The old GUI stack. Maintenance only. |

This reference covers the **v2** line.

---

## The pages

| Page | What it holds |
|---|---|
| [The shell](shell_api.md) | The API of `C_YUI_SHELL`: navigation, drawers, overlays, avatars and the toolbar. |
| [Dialogs and notifications](modals.md) | The notifications, the modal and the four dialogs of confirmation. |
| [Component gclasses](gclasses.md) | The registration function of each component. |
| [Time and periods](time.md) | The algebra of the periods, the rolling windows and the formatting. |
| [Theme](theme.md) | The theme that is active, and how to follow a change of it. |
| [DOM helpers](dom.md) | The classes of an element, the icons, the inputs and the toolbar. |
| [Development panel](dev.md) | The panel of traces and traffic. |
| [Map controls](maplibre.md) | The controls of edition and of markers for maplibre. |

Two chapters describe the design, and they are not a reference of the API:
[The declarative shell](../../../../kernel/js/gobj-ui/SHELL.md) and
[Routing](../../../../kernel/js/gobj-ui/ROUTING.md).

Every symbol of the package is in the
[JS API index](../appendix_js_api_index.md), and the index carries the symbols
that a deep import reaches too.

---

## The rules of a GUI gclass

These rules apply to every gclass that draws.

**Every action goes through the state machine.** A click is an action. The DOM
handler sends an event, and the work happens in the action. A view whose whole
life happens in one state gives a `machine` trace that shows nothing.

**A `kw` is plain JSON.** Never put a gobj, a widget or a DOM node in it. Pass
an identity, and find the object inside the action.

**Data is text, and markup is said out loud.** A value from a backend, or a
value that a user typed, goes into the DOM as text: a Text node in the third
slot of a description, or a string in a list of children. Markup that you mean
(an icon, an svg) goes in [`html_markup()`](../js/helpers_dom.md#js_html_markup).
gobj-ui passes all its markup that way since 7.26.10. So an app can start with
[`set_strings_are_text(true)`](../js/helpers_dom.md#js_set_strings_are_text),
and then a plain string is never markup. Run the check of gobj-ui on the app
in its `prebuild`; it finds a literal markup or markup that is built with
values:

```javascript
['span', {class: 'icon'}, html_markup('<i class="yi-copy"></i>')]  // markup
['span', {class: 'HOST'}, document.createTextNode(host)]          // data
```

```bash
# gobj-ui publishes the check (7.26.11+); its acorn is a dependency of gobj-ui
# since 7.26.29, so the app declares nothing for it
node node_modules/@yuneta/gobj-ui/scripts/check-markup-strings.mjs src
```

**Give a logical name to each block of the DOM.** The root of a view carries the
name of its gclass and a name of its own, such as
`class="C_AGENT_CONSOLE CONSOLE_CARD view-card"`. Each meaningful child carries a
logical name with the prefix of the view. A logical name is `UPPER_SNAKE`, and a
name of style is lower case, so the case alone separates the two.

**Every text goes through i18n, and it must change language.** A text that goes
through the translator one time does not change language again. See
[`refresh_language()`](../js/helpers_dom.md#js_refresh_language).

**No transitions and no animations.** A menu, a popover and a change of state
appear at once.

**Four spaces of indentation**, everywhere that a structure appears as an
indentation, and a rendered tree indents in `ch` and not in `rem`.
