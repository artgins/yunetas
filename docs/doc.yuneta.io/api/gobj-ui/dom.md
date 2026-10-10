---
title: 'gobj-ui: DOM helpers'
description: >-
  The helpers that change the classes of a set of elements, the icons,
  the clear button of an input and the toolbar that scrolls.
---

# DOM helpers

**Source code:** [`src/lib_graph.js`](https://github.com/artgins/gobj-ui.js/blob/7.26.6/src/lib_graph.js),
[`src/lib_icons.js`](https://github.com/artgins/gobj-ui.js/blob/7.26.6/src/lib_icons.js),
[`src/yui_inputs.js`](https://github.com/artgins/gobj-ui.js/blob/7.26.6/src/yui_inputs.js),
[`src/yui_toolbar.js`](https://github.com/artgins/gobj-ui.js/blob/7.26.6/src/yui_toolbar.js)

---

## Classes of a set of elements

Each function takes a container and a selector, and it works on every element
that matches inside the container.

(js_addClasses)=
### [`addClasses($container, selector, ...classNames)`](https://github.com/artgins/gobj-ui.js/blob/7.26.6/src/lib_graph.js#L21)

Adds the classes.

(js_removeClasses)=
### [`removeClasses($container, selector, ...classNames)`](https://github.com/artgins/gobj-ui.js/blob/7.26.6/src/lib_graph.js#L31)

Removes the classes.

(js_toggleClasses)=
### [`toggleClasses($container, selector, ...classNames)`](https://github.com/artgins/gobj-ui.js/blob/7.26.6/src/lib_graph.js#L41)

Adds the classes that are absent, and removes the classes that are present.

(js_removeChildElements)=
### [`removeChildElements($element)`](https://github.com/artgins/gobj-ui.js/blob/7.26.6/src/lib_graph.js#L51)

Removes every child of an element.

(js_disableElements)=
### [`disableElements($container, selector)`](https://github.com/artgins/gobj-ui.js/blob/7.26.6/src/lib_graph.js#L59)

Disables the elements that match.

(js_enableElements)=
### [`enableElements($container, selector)`](https://github.com/artgins/gobj-ui.js/blob/7.26.6/src/lib_graph.js#L66)

Enables the elements that match.

---

## States of an action

The functions below write the state of a set of elements. The first three
write a color; the fourth writes the look of a control that is pressed.

(js_set_submit_state)=
### [`set_submit_state($container, selector, set)`](https://github.com/artgins/gobj-ui.js/blob/7.26.6/src/lib_graph.js#L79)

Writes the state of *"there are changes to save"*, in orange.

(js_set_cancel_state)=
### [`set_cancel_state($container, selector, set)`](https://github.com/artgins/gobj-ui.js/blob/7.26.6/src/lib_graph.js#L92)

Writes the state of *"the cancel is possible"*, in red.

(js_set_active_state)=
### [`set_active_state($container, selector, set)`](https://github.com/artgins/gobj-ui.js/blob/7.26.6/src/lib_graph.js#L105)

Writes the state of *"active"*, in violet. The library uses it for the buttons
of the history, undo and redo.

(js_set_pressed_state)=
### [`set_pressed_state($container, selector, set)`](https://github.com/artgins/gobj-ui.js/blob/7.26.6/src/yui_toolbar.js#L189)

Writes the state of *"this toggle is on"*. It does not use a color: it gives
the control the look of a button that is pressed, an inverted neutral. A color
of the group above tells which KIND of action a control does, and a toggle that
is on is not an action — it is a state. Two controls of the same toolbar with
the same color, for two different reasons, are more difficult to read than one
control that looks pressed.

---

## Color

(js_getStrokeColor)=
### [`getStrokeColor(fillColor, theme, factor)`](https://github.com/artgins/gobj-ui.js/blob/7.26.6/src/lib_graph.js#L132)

Gives a color of line for a color of fill, in the form `rgba()`. It makes the
color lighter or darker by `factor`, which is 0.2 by default, and the direction
depends on the theme. It accepts the hexadecimal form and the `rgb()` form.

---

## Icons

(js_inject_svg_icons)=
### [`inject_svg_icons()`](https://github.com/artgins/gobj-ui.js/blob/7.26.6/src/lib_icons.js#L170)

Puts the set of SVG symbols in the document. A second call does nothing, because
a guard on the identifier of the element stops it.

:::{warning}
`yui_icons.css` is a small set of CSS masks, and it is not FontAwesome. A class
`yi-*` that the set does not hold draws a black square. Read the set before you
use an icon:

```bash
grep -oE '^\.yi-[a-z0-9-]+::before' src/yui_icons.css
```

Add a missing icon as a rule of mask. Never name one on hope.
:::

(js_yui_icon_is_defined)=
### [`yui_icon_is_defined(name)`](https://github.com/artgins/gobj-ui.js/blob/7.26.6/src/lib_icons.js#L201)

Tells if `name` is a class that draws an icon in the loaded stylesheets. It asks
the real stylesheet, so an icon that an app adds is seen too. The answer is kept
for each name; a change of the user icons clears the answers about `yi-u-` names.

```js
if(!yui_icon_is_defined("yi-bolt")) {
    show_the_name_instead();
}
```

### User icons

A user adds icons as data. Each treedb has the system topic `__icons__`: one
node for each icon, with the name in `id` and the drawing in `svg`. A column with
the flag `icon` names such an icon as `yi-u-<id>`. The prefix `yi-u-` is not used
by the icons of the library, so a user icon cannot replace one of them.
`C_YUI_TREEDB_TOPICS` loads `__icons__` and keeps the registry current. An app
that does not mount that view calls the functions below itself.

(js_ICONS_TOPIC)=
#### [`ICONS_TOPIC`](https://github.com/artgins/gobj-ui.js/blob/7.26.6/src/lib_icons.js#L244)

The name of the system topic: `"__icons__"`.

(js_USER_ICON_PREFIX)=
#### [`USER_ICON_PREFIX`](https://github.com/artgins/gobj-ui.js/blob/7.26.6/src/lib_icons.js#L252)

The prefix of the class of a user icon: `"yi-u-"`.

(js_yui_user_icon_class)=
#### [`yui_user_icon_class(id)`](https://github.com/artgins/gobj-ui.js/blob/7.26.6/src/lib_icons.js#L301)

Gives the class of the user icon `id`.

```js
yui_user_icon_class("transformer");     // "yi-u-transformer"
```

(js_yui_svg_sanitize)=
#### [`yui_svg_sanitize(svg_text)`](https://github.com/artgins/gobj-ui.js/blob/7.26.6/src/lib_icons.js#L317)

Parses an svg and writes a new one from its shapes only (`path`, `circle`,
`rect`, `ellipse`, `line`, `polyline`, `polygon`, `g`) and their geometric and
stroke attributes. It gives `{svg, dropped}`, where `dropped` names what it
removed, or `{error}`. The error is one of `"empty svg"`, `"svg too big"`,
`"not an svg document"`, `"svg without size"`, `"svg without shapes"`.

```js
yui_svg_sanitize("<svg viewBox='0 0 10 10' onload='x()'><circle cx='5' cy='5' r='5'/></svg>");
// {svg: '<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 10 10"><circle cx="5" cy="5" r="5"></circle></svg>',
//  dropped: ["@onload"]}
```

(js_yui_svg_icon_element)=
#### [`yui_svg_icon_element(svg_text)`](https://github.com/artgins/gobj-ui.js/blob/7.26.6/src/lib_icons.js#L464)

Gives `{element}`, a `span` that shows the svg as an icon (the same box and the
same mask as a `yi-` icon), or `{error}`. Use it for a preview, before the icon
is registered.

```js
let r = yui_svg_icon_element(textarea.value);
if(r.error) {
    note.textContent = r.error;
} else {
    box.replaceChildren(r.element);
}
```

(js_yui_icons_set_user)=
#### [`yui_icons_set_user(nodes)`](https://github.com/artgins/gobj-ui.js/blob/7.26.6/src/lib_icons.js#L532)

Replaces all the user icons with the nodes of `__icons__`, and gives the number
of icons registered. A node with a bad name or an svg that cannot be drawn is
not registered, and a warning is logged. Tell the views after it:

```js
yui_icons_set_user(nodes);          // the answer of `nodes` of __icons__
yui_shell_icons_changed(shell);     // publishes EV_ICONS_CHANGED
```

(js_yui_icons_put_user)=
#### [`yui_icons_put_user(node)`](https://github.com/artgins/gobj-ui.js/blob/7.26.6/src/lib_icons.js#L553)

Registers one node that was created or updated, and gives `true` if the icon is
drawn. If the new svg cannot be drawn, the old icon is removed.

```js
yui_icons_put_user({id: "transformer", svg: "<svg viewBox='0 0 24 24'><path d='M4 4h16v16H4z'/></svg>"});
```

(js_yui_icons_remove_user)=
#### [`yui_icons_remove_user(id)`](https://github.com/artgins/gobj-ui.js/blob/7.26.6/src/lib_icons.js#L571)

Removes the user icon `id`, after its node was deleted.

```js
yui_icons_remove_user("transformer");
```

(js_yui_icons_list)=
#### [`yui_icons_list()`](https://github.com/artgins/gobj-ui.js/blob/7.26.6/src/lib_icons.js#L583)

Gives every icon that can be chosen, as `[{name, user}]`: the icons of the
library, read from the loaded stylesheets, then the user icons. Each part is
sorted by name. The form uses it for the picker of an `icon` column.

```js
yui_icons_list().filter(icon => icon.user).map(icon => icon.name);
// ["yi-u-transformer"]
```

---

## Inputs

(js_attach_clear)=
### [`attach_clear($control, $input, on_clear)`](https://github.com/artgins/gobj-ui.js/blob/7.26.6/src/yui_inputs.js#L48)

Adds a button of clear to a control that holds an input, and gives the button
back. The button appears only when the input holds a value and accepts a write.

(js_refresh_clear)=
### [`refresh_clear($input)`](https://github.com/artgins/gobj-ui.js/blob/7.26.6/src/yui_inputs.js#L98)

Looks at the button of clear again, after a change that the code made: a value
that a form loaded, or a change of the read-only state. Neither of those sends
an event of input, so nothing else looks. It does nothing on an input that has
no button of clear.

---

## Toolbar

(js_yui_toolbar)=
### [`yui_toolbar(attrs, items)`](https://github.com/artgins/gobj-ui.js/blob/7.26.6/src/yui_toolbar.js#L56)

Builds a toolbar that scrolls, with an arrow at each end when the items do not
fit. The arrows take the icons of the set of the library, and they take their
color from `currentColor`, so the two themes work.

:::{note}
Every button of a row carries an icon. Keep the text of the label when the row
still fits at the narrowest width that the application supports, in the longest
language that it supports. Decide it one time, at the time of the design, and
never by a measurement at run time: the width depends on the language, so a
measurement shows text in one language and icons in another, in the same
toolbar. Always write `title` and `aria-label`.
:::
