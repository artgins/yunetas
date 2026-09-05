---
title: 'gobj-ui: DOM helpers'
description: >-
  The helpers that change the classes of a set of elements, the icons,
  the clear button of an input and the toolbar that scrolls.
---

# DOM helpers

**Source code:** [`src/lib_graph.js`](https://github.com/artgins/gobj-ui.js/blob/7.23.62/src/lib_graph.js),
[`src/lib_icons.js`](https://github.com/artgins/gobj-ui.js/blob/7.23.62/src/lib_icons.js),
[`src/yui_inputs.js`](https://github.com/artgins/gobj-ui.js/blob/7.23.62/src/yui_inputs.js),
[`src/yui_toolbar.js`](https://github.com/artgins/gobj-ui.js/blob/7.23.62/src/yui_toolbar.js)

---

## Classes of a set of elements

Each function takes a container and a selector, and it works on every element
that matches inside the container.

(js_addClasses)=
### [`addClasses($container, selector, ...classNames)`](https://github.com/artgins/gobj-ui.js/blob/7.23.62/src/lib_graph.js#L20)

Adds the classes.

(js_removeClasses)=
### [`removeClasses($container, selector, ...classNames)`](https://github.com/artgins/gobj-ui.js/blob/7.23.62/src/lib_graph.js#L30)

Removes the classes.

(js_toggleClasses)=
### [`toggleClasses($container, selector, ...classNames)`](https://github.com/artgins/gobj-ui.js/blob/7.23.62/src/lib_graph.js#L40)

Adds the classes that are absent, and removes the classes that are present.

(js_removeChildElements)=
### [`removeChildElements($element)`](https://github.com/artgins/gobj-ui.js/blob/7.23.62/src/lib_graph.js#L50)

Removes every child of an element.

(js_disableElements)=
### [`disableElements($container, selector)`](https://github.com/artgins/gobj-ui.js/blob/7.23.62/src/lib_graph.js#L58)

Disables the elements that match.

(js_enableElements)=
### [`enableElements($container, selector)`](https://github.com/artgins/gobj-ui.js/blob/7.23.62/src/lib_graph.js#L65)

Enables the elements that match.

---

## States of an action

The functions below write the state of a set of elements. The first three
write a color; the fourth writes the look of a control that is pressed.

(js_set_submit_state)=
### [`set_submit_state($container, selector, set)`](https://github.com/artgins/gobj-ui.js/blob/7.23.62/src/lib_graph.js#L78)

Writes the state of *"there are changes to save"*, in orange.

(js_set_cancel_state)=
### [`set_cancel_state($container, selector, set)`](https://github.com/artgins/gobj-ui.js/blob/7.23.62/src/lib_graph.js#L91)

Writes the state of *"the cancel is possible"*, in red.

(js_set_active_state)=
### [`set_active_state($container, selector, set)`](https://github.com/artgins/gobj-ui.js/blob/7.23.62/src/lib_graph.js#L104)

Writes the state of *"active"*, in violet. The library uses it for the buttons
of the history, undo and redo.

(js_set_pressed_state)=
### [`set_pressed_state($container, selector, set)`](https://github.com/artgins/gobj-ui.js/blob/7.23.62/src/lib_graph.js#L122)

Writes the state of *"this toggle is on"*. It does not use a color: it gives
the control the look of a button that is pressed, an inverted neutral. A color
of the group above tells which KIND of action a control does, and a toggle that
is on is not an action — it is a state. Two controls of the same toolbar with
the same color, for two different reasons, are more difficult to read than one
control that looks pressed.

---

## Color

(js_getStrokeColor)=
### [`getStrokeColor(fillColor, theme, factor)`](https://github.com/artgins/gobj-ui.js/blob/7.23.62/src/lib_graph.js#L142)

Gives a color of line for a color of fill, in the form `rgba()`. It makes the
color lighter or darker by `factor`, which is 0.2 by default, and the direction
depends on the theme. It accepts the hexadecimal form and the `rgb()` form.

---

## Icons

(js_inject_svg_icons)=
### [`inject_svg_icons()`](https://github.com/artgins/gobj-ui.js/blob/7.23.62/src/lib_icons.js#L160)

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

---

## Inputs

(js_attach_clear)=
### [`attach_clear($control, $input, on_clear)`](https://github.com/artgins/gobj-ui.js/blob/7.23.62/src/yui_inputs.js#L48)

Adds a button of clear to a control that holds an input, and gives the button
back. The button appears only when the input holds a value and accepts a write.

(js_refresh_clear)=
### [`refresh_clear($input)`](https://github.com/artgins/gobj-ui.js/blob/7.23.62/src/yui_inputs.js#L98)

Looks at the button of clear again, after a change that the code made: a value
that a form loaded, or a change of the read-only state. Neither of those sends
an event of input, so nothing else looks. It does nothing on an input that has
no button of clear.

---

## Toolbar

(js_yui_toolbar)=
### [`yui_toolbar(attrs, items)`](https://github.com/artgins/gobj-ui.js/blob/7.23.62/src/yui_toolbar.js#L19)

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
