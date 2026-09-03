---
title: 'JS: DOM and i18n helpers'
description: >-
  Building DOM elements, escaping untrusted values, and translating a tree
  again after a change of language.
---

# DOM and i18n helpers

The browser is an operating system, and its DOM is where a GUI gclass draws.
These helpers build the elements and keep the untrusted values safe.

**Source code:** [`src/helpers.js`](https://github.com/artgins/gobj-js/blob/7.16.2/src/helpers.js)

:::{important}
A DOM callback does the work of a translator, and nothing more. A click, an
`onmessage`, a `setTimeout` and a `resize` are notifications of the operating
system, so they enter the machine as events with
[`gobj_send_event()`](events.md#js_gobj_send_event). Application logic that runs
inside the callback is invisible to the `machine` trace.
:::

---

## Build an element

(js_createElement2)=
## [`createElement2()`](https://github.com/artgins/gobj-js/blob/7.16.2/src/helpers.js#L3060)

Builds an HTML element from a description that is a plain array. It is the
function that a GUI gclass uses to draw.

```javascript
createElement2(description, translate_fn)
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `description` | `array` | The element, as `[tag, attrs, content, events]`. |
| `translate_fn` | `function` | The translation function. The attribute `i18n` uses it. |

**Returns**

The element that the function built.

**The four slots**

| Slot | Type | Description |
|---|---|---|
| `tag` | `string` | The name of the element. |
| `attrs` | `object` | The attributes. `style` accepts a string or an object. `i18n` gives the content through `translate_fn`. |
| `content` | `string` or `array` | The text, one child, or a list of children. |
| `events` | `object` | The listeners, as `{event: function}`. |

:::{warning}
The second slot must be the attributes object. Text and children go in the
**third** slot. `["p", "message"]` builds an element with a broken attribute
list. Write `["p", {}, "message"]`.
:::

```javascript
['div', { class: 'window-top', style: 'border-bottom: 1px solid black;' },
    [
        ['span', {}, 'title'],
        ['button', {}, 'Close', {
            click: (e) => {
                e.target.closest('.window').remove();
            }
        }]
    ]
]
```

The `style` attribute accepts both forms:

```javascript
['span', {style: {position: 'absolute'} }, 'title']
```

:::{note}
The function **removes the spaces** at the two ends of a text node. A string
that is built from parts loses the space between them, so give the separator to
the CSS and not to the text.
:::

---

(js_createOneHtml)=
### [`createOneHtml(htmlString)`](https://github.com/artgins/gobj-js/blob/7.16.2/src/helpers.js#L2871)

Builds one element from a string of HTML.

(js_parseSVG)=
### [`parseSVG(string)`](https://github.com/artgins/gobj-js/blob/7.16.2/src/helpers.js#L3196)

Builds an SVG element from a string of SVG code.

(js_getPositionRelativeToBody)=
### [`getPositionRelativeToBody(element)`](https://github.com/artgins/gobj-js/blob/7.16.2/src/helpers.js#L3168)

Gives the position of an element against the body of the document. A popover or
a menu that must stay next to its button uses it.

---

## Untrusted values

(js_escapeHtml)=
### [`escapeHtml(str)`](https://github.com/artgins/gobj-js/blob/7.16.2/src/helpers.js#L2838)

Escapes a value for an HTML context. Use it every time that a string from a
user or from a server goes into a template that becomes `innerHTML`.

(js_safeSrc)=
### [`safeSrc(url)`](https://github.com/artgins/gobj-js/blob/7.16.2/src/helpers.js#L2856)

Validates a URL for the `src` attribute of an image or a media element. It
refuses the `javascript:` and `data:` schemes, which carry a cross-site script.
It gives an empty string back for every scheme that it refuses.

---

## Language

(js_refresh_language)=
## [`refresh_language()`](https://github.com/artgins/gobj-js/blob/7.16.2/src/helpers.js#L3204)

Translates a DOM tree again, after the application changes its language.

```javascript
refresh_language(element, t)
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `element` | `Element` | The root of the tree. With an empty value the function takes the whole document. |
| `t` | `function` | The translation function, such as the `t` of i18next. |

**What the function translates**

It reads four attributes, and each one carries the key of its own text.

| Attribute | What it translates |
|---|---|
| `data-i18n` | The first text node of the element. |
| `data-i18n-title` | The `title` attribute, which is the tooltip. |
| `data-i18n-aria-label` | The `aria-label` attribute, which a screen reader reads. |
| `data-i18n-placeholder` | The `placeholder` attribute of an input or a text area. |

:::{important}
A text that goes through `t()` one time does **not** change language again. The
function reaches only a node that carries its key. A string that the code builds
at the time of the render keeps the old language for the life of the view.
Give each half of a composed string its own key.

A `title` or an `aria-label` needs its own attribute from the table above,
because the walk of `data-i18n` reaches text nodes only.

What a **widget** draws reaches none of these attributes. The view subscribes to
`EV_LANGUAGE_CHANGED` of the shell, and it draws again inside the action.
:::

:::{warning}
A key that no locale file holds is invisible. i18next answers an unknown key
with the key itself, so the text renders in lower case and never changes
language. A key that a locale file holds two times is invisible too, because an
object literal keeps the last one. Check both with `scripts/validate-locales.mjs`
in the application.
:::
