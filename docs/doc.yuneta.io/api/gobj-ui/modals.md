---
title: 'gobj-ui: Dialogs and notifications'
description: >-
  The notifications that go away on their own, the modal, and the four
  dialogs of confirmation.
---

# Dialogs and notifications

**Source code:** [`src/shell_modals.js`](https://github.com/artgins/gobj-ui.js/blob/7.23.4/src/shell_modals.js)

Every function takes the shell as its first parameter.

---

## Notifications

A notification goes into the layer of the shell and goes away on its own after
`opts.timeout` milliseconds. The default is 5000, and a value of `0` keeps it
until somebody closes it. Each one gives `{close}` back.

(js_yui_shell_show_info)=
### [`yui_shell_show_info(shell, message, opts)`](https://github.com/artgins/gobj-ui.js/blob/7.23.4/src/shell_modals.js#L166)

Shows a message of information.

(js_yui_shell_show_warning)=
### [`yui_shell_show_warning(shell, message, opts)`](https://github.com/artgins/gobj-ui.js/blob/7.23.4/src/shell_modals.js#L170)

Shows a warning.

(js_yui_shell_show_error)=
### [`yui_shell_show_error(shell, message, opts)`](https://github.com/artgins/gobj-ui.js/blob/7.23.4/src/shell_modals.js#L174)

Shows an error.

---

## Modal

(js_yui_shell_show_modal)=
## [`yui_shell_show_modal(shell, content, opts)`](https://github.com/artgins/gobj-ui.js/blob/7.23.4/src/shell_modals.js#L189)

Puts an overlay in the layer of the modals. `content` accepts a string, which
the function draws inside a box, or an element, which it draws as it is.

**Returns** `{close}`. The caller decides when to close it. A click on the
background, the button of close and the Escape key close it too.

:::{note}
Give `opts.dialog: true` for a dialog that adapts itself to the size of the
screen. It is the standard form for a dialog in the v2 line.
:::

---

## Confirmation

Each one gives a promise back. The buttons carry a label that the caller can
change.

(js_yui_shell_confirm_ok)=
### [`yui_shell_confirm_ok(shell, message, opts)`](https://github.com/artgins/gobj-ui.js/blob/7.23.4/src/shell_modals.js#L544)

Shows a message with one button. The promise gives `undefined` back.
`opts.ok_label` changes the label.

(js_yui_shell_confirm_yesno)=
### [`yui_shell_confirm_yesno(shell, message, opts)`](https://github.com/artgins/gobj-ui.js/blob/7.23.4/src/shell_modals.js#L555)

Asks a question with two answers. The promise gives `true` for yes.
`opts.yes_label` and `opts.no_label` change the labels.

(js_yui_shell_confirm_yesnocancel)=
### [`yui_shell_confirm_yesnocancel(shell, message, opts)`](https://github.com/artgins/gobj-ui.js/blob/7.23.4/src/shell_modals.js#L592)

Asks a question with three answers. The promise gives `"yes"`, `"no"` or
`"cancel"`.

Use it when the third answer is a real one. A question such as *"play all"* has
three answers and not two: add, replace, or cancel.

(js_yui_shell_confirm_danger)=
### [`yui_shell_confirm_danger(shell, message, opts)`](https://github.com/artgins/gobj-ui.js/blob/7.23.4/src/shell_modals.js#L579)

Asks a **destructive** question. The promise gives `true` only when the user
presses the red button. `opts.confirm_label` and `opts.cancel_label` change the
labels, and the defaults are `"Delete"` and `"Cancel"`.

---

:::{important}
A destructive question takes
[`yui_shell_confirm_danger()`](#js_yui_shell_confirm_danger), and not
[`yui_shell_confirm_yesno()`](#js_yui_shell_confirm_yesno). The yes of the
second one is blue, which is the right color for *"do you want to continue"*
and the wrong one for *"this deletes an account"*: the two read the same at one
look, and the destructive one is the one that nobody must press by reflex. In
the destructive dialog the button that does the damage is red, and the safe
answer is the **last** button, so the Escape key, the background and the button
of close all give the safe answer.
:::

:::{note}
This function reached the barrel of the package in **5.11.1**. Before that
release, an import of it from `@yuneta/gobj-ui` gave `undefined`, and the only
way in was its module. A consumer that still writes the deep import keeps
working, because the `./src/*` map does not change.
:::
