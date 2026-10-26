---
title: 'JS: DOM and i18n helpers'
description: >-
  Building DOM elements, escaping untrusted values, and translating a tree
  again after a change of language.
---

# DOM and i18n helpers

The browser is an operating system, and its DOM is where a GUI gclass draws.
These helpers build the elements and keep the untrusted values safe.

**Source code:** [`src/helpers.js`](https://github.com/artgins/gobj-js/blob/7.28.1/src/helpers.js)

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
## [`createElement2()`](https://github.com/artgins/gobj-js/blob/7.28.1/src/helpers.js#L5470)

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

:::{danger}
A string in the third slot is **markup** when it starts with `<`: it is parsed
as HTML. Never pass data there (a value from a backend, a name that a user
typed) as a bare string, or `<img src=x onerror=...>` runs. Pass a Text node,
which is always text. In a **list** of children, a node (element, svg, Text) is
appended as it is, and a string is always text (since gobj-js 7.26.2; before, a
Text node or a string in a list was dropped). A list whose first item is a
string is read as one description, so put the text in a Text node there.
Markup that you mean is said with [`html_markup()`](#js_html_markup), and with
[`set_strings_are_text(true)`](#js_set_strings_are_text) a plain string is
always text.
:::

```javascript
// Data in the third slot: a Text node
['span', {class: 'HOST_NAME'}, document.createTextNode(host)]

// Data in a list of children (gobj-js 7.26.2+): a string is text
['p', {}, [['b', {}, 'Host: '], host]]
```

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

(js_html_markup)=
### [`html_markup(html)`](https://github.com/artgins/gobj-js/blob/7.28.1/src/helpers.js#L5201)

Marks a string as **markup** for `createElement2()`, said out loud: the string
is the caller's own (an icon, an svg), never data. It works in the third slot
and in a list of children, in any mode, and keeps every node of the string
(since gobj-js 7.26.3).

```javascript
['span', {class: 'icon'}, html_markup('<i class="yi-copy"></i>')]
['div', {}, [['b', {}, 'Saved'], html_markup('<i class="yi-check"></i>')]]
```

Turned into a string (`String(m)`, a template, `+`), the value gives its html
back, so code that still builds `innerHTML` keeps working. The string that comes
out is a plain string again, and it is not marked: never add data to it. In
`createElement2()` with `set_strings_are_text(true)` that string is text, and
it is logged when it starts with `<`.

```javascript
$el.innerHTML = `${html_markup('<i class="yi-check"></i>')}`;  // works
['span', {}, `${html_markup('<b>')}${name}`]                   // text, and logged
```

(js_is_html_markup)=
### [`is_html_markup(v)`](https://github.com/artgins/gobj-js/blob/7.28.1/src/helpers.js#L5210)

Tells whether `v` is a value that `html_markup()` returned.

```javascript
is_html_markup(html_markup('<b>x</b>'));   // true
is_html_markup('<b>x</b>');                // false
```

(js_set_strings_are_text)=
### [`set_strings_are_text(on)`](https://github.com/artgins/gobj-js/blob/7.28.1/src/helpers.js#L5237)

Sets how `createElement2()` reads a plain string that starts with `<`. With
`false` (the default) the string is markup, as it always was. With `true` it is
**text**, and `html_markup()` is the only way to insert markup. In that mode a
string that starts with `<` is logged once with a warning: it is a deliberate
markup that was not migrated, or a datum that used to run as HTML.

An app turns the mode on at startup, after its own code and the libraries it
mounts pass all their markup through `html_markup()`. Since 7.26.10 gobj-ui
passes its markup that way, and the check
`node node_modules/@yuneta/gobj-ui/scripts/check-markup-strings.mjs src` (published with gobj-ui 7.26.11+) finds the literal cases in an app.

```javascript
import {set_strings_are_text} from "@yuneta/gobj-js";
set_strings_are_text(true);
createElement2(['span', {}, '<img src=x onerror=alert(1)>']);  // text, logged once
```

(js_strings_are_text)=
### [`strings_are_text()`](https://github.com/artgins/gobj-js/blob/7.28.1/src/helpers.js#L5242)

Returns the mode that `set_strings_are_text()` set.

```javascript
if(!strings_are_text()) {
    log_warning("this app still reads '<' strings as markup");
}
```

---

(js_createOneHtml)=
### [`createOneHtml(htmlString)`](https://github.com/artgins/gobj-js/blob/7.28.1/src/helpers.js#L5268)

Builds one element from a string of HTML.

(js_parseSVG)=
### [`parseSVG(string)`](https://github.com/artgins/gobj-js/blob/7.28.1/src/helpers.js#L5624)

Builds an SVG element from a string of SVG code.

(js_getPositionRelativeToBody)=
### [`getPositionRelativeToBody(element)`](https://github.com/artgins/gobj-js/blob/7.28.1/src/helpers.js#L5596)

Gives the position of an element against the body of the document. A popover or
a menu that must stay next to its button uses it.

---

## Untrusted values

(js_escapeHtml)=
### [`escapeHtml(str)`](https://github.com/artgins/gobj-js/blob/7.28.1/src/helpers.js#L5144)

Escapes a value for an HTML context. Use it every time that a string from a
user or from a server goes into a template that becomes `innerHTML`.

(js_safeSrc)=
### [`safeSrc(url)`](https://github.com/artgins/gobj-js/blob/7.28.1/src/helpers.js#L5162)

Validates a URL for the `src` attribute of an image or a media element. It
refuses the `javascript:` and `data:` schemes, which carry a cross-site script.
It gives an empty string back for every scheme that it refuses.

---

## Language

(js_refresh_language)=
## [`refresh_language()`](https://github.com/artgins/gobj-js/blob/7.28.1/src/helpers.js#L5632)

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

:::{note}
Since `7.16.6` the function translates the element that you give to it, and
also the elements below it. Before that version it read the four attributes
with `querySelectorAll`, which finds the elements below only. A caller that
gave the function the element with the key got the children in the new
language, and that element in the old one.

The problem is not visible in English, because there the key is the text. It
appears where a widget is built late and is translated as one piece. On a live
shell the menu of the language opened with `aria-label="select language"`, next
to the button that opened it, which said *"Elegir idioma"*.
:::

:::{warning}
A key that no locale file holds is invisible. i18next answers an unknown key
with the key itself, so the text renders in lower case and never changes
language. A key that a locale file holds two times is invisible too, because an
object literal keeps the last one. Check both with `scripts/validate-locales.mjs`
in the application.
:::
