/***********************************************************************
 *          shell_modals.js
 *
 *      Modal / notification API on top of the layers C_YUI_SHELL
 *      already creates (`priv.layers.notification`,
 *      `priv.layers.modal`).  Naming convention:
 *
 *          yui_shell_show_*     — non-blocking notifications/modal
 *          yui_shell_confirm_*  — blocking dialog, returns Promise
 *
 *      Bulma `.notification` / `.modal-card` markup is reused
 *      verbatim so apps importing Bulma get the visual styling for
 *      free.
 *
 *      Every blocking dialog and every modal pushes a close handler
 *      onto the shell's Escape priority chain (see
 *      yui_shell_push_escape / yui_shell_pop_escape) so Escape
 *      closes the topmost overlay only.
 *
 *      The legacy display_* / get_yes* helpers in c_yui_main.js are
 *      NOT changed — apps that ride on the legacy shell keep using
 *      them (see SHELL.md §10 drift policy).
 *
 *          Copyright (c) 2026, ArtGins.
 *          All Rights Reserved.
 ***********************************************************************/
/* global document */

import {
    gobj_read_attr,
    createElement2,
    refresh_language,
    empty_string,
    log_warning,
} from "@yuneta/gobj-js";

import {
    activate_focus_trap_on,
} from "./shell_focus_trap.js";

import {
    yui_shell_push_escape,
    yui_shell_pop_escape,
} from "./c_yui_shell.js";


/***************************************************************
 *              Layer accessors
 ***************************************************************/
function notification_layer(shell)
{
    let priv = gobj_read_attr(shell, "priv");
    return priv && priv.layers && priv.layers.notification;
}

function modal_layer(shell)
{
    let priv = gobj_read_attr(shell, "priv");
    return priv && priv.layers && priv.layers.modal;
}


/***************************************************************
 *  i18n bridge:
 *      - `text` is rendered as the canonical English key.
 *      - The hosting element is tagged with `data-i18n="<key>"`,
 *        so a later `refresh_language(shell.$container, t)` call
 *        retranslates the modal even if it was created BEFORE the
 *        language was switched.
 *      - If `opts.t` is a function, the helper translates the
 *        node right after creating it — so a modal opened AFTER
 *        the user has already toggled to ES renders in ES on the
 *        first frame, not the canonical English key.
 ***************************************************************/
function maybe_apply_translator($node, opts)
{
    if(opts && typeof opts.t === "function") {
        refresh_language($node, opts.t);
    }
}


/***************************************************************
 *              Notifications (Bulma .notification)
 *
 *      Non-blocking, auto-dismiss after `opts.timeout` ms (default
 *      5000).  `opts.timeout = 0` disables auto-dismiss.
 *      Returns `{ close() }` so callers can dismiss programmatically.
 ***************************************************************/
function show_notification(shell, kind, message, opts)
{
    let $layer = notification_layer(shell);
    if(!$layer) {
        log_warning("yui_shell_show_*: shell has no notification layer");
        return { close: () => {} };
    }

    let p_attrs = (typeof message === "string")
        ? {i18n: message}
        : {};
    let $note = createElement2(
        ["div", {class: `notification yui-notification is-${kind} is-light`,
                 role: kind === "danger" ? "alert" : "status"},
            [
                ["button", {class: "delete", "aria-label": "close"}],
                ["p", p_attrs, message]
            ]
        ]
    );
    $layer.appendChild($note);
    maybe_apply_translator($note, opts);

    let timeout_id = null;
    let closed = false;

    let close = function() {
        if(closed) {
            return;
        }
        closed = true;
        if(timeout_id) {
            clearTimeout(timeout_id);
            timeout_id = null;
        }
        if($note.parentNode) {
            $note.parentNode.removeChild($note);
        }
    };

    let $del = $note.querySelector(".delete");
    if($del) {
        $del.addEventListener("click", close);
    }

    let timeout = (opts && opts.timeout != null) ? opts.timeout : 5000;
    if(timeout > 0) {
        timeout_id = setTimeout(close, timeout);
    }

    return { close };
}


export function yui_shell_show_info(shell, message, opts)
{
    return show_notification(shell, "info", message, opts);
}
export function yui_shell_show_warning(shell, message, opts)
{
    return show_notification(shell, "warning", message, opts);
}
export function yui_shell_show_error(shell, message, opts)
{
    return show_notification(shell, "danger", message, opts);
}


/***************************************************************
 *              Modal (Bulma .modal — non-blocking)
 *
 *      Drops a Bulma `.modal-content` overlay into the modal layer.
 *      `content` may be a string (rendered inside a Bulma .box) or
 *      an HTMLElement (rendered as-is).  Returns `{ close() }`.
 *      The caller decides when to dismiss; click on background,
 *      the `.modal-close` button, or `Escape` also close it.
 ***************************************************************/
export function yui_shell_show_modal(shell, content, opts)
{
    let $layer = modal_layer(shell);
    if(!$layer) {
        log_warning("yui_shell_show_modal: shell has no modal layer");
        return { close: () => {} };
    }

    let inner = (typeof content === "string")
        ? ["div", {class: "box"}, [["p", {i18n: content}, content]]]
        : null;

    let $modal = createElement2(
        ["div", {class: "modal yui-modal is-active",
                 role: "dialog", "aria-modal": "true"},
            [
                ["div", {class: "modal-background"}],
                ["div", {class: "modal-content"},
                    inner ? [inner] : []
                ],
                ["button", {class: "modal-close is-large",
                            "aria-label": "close"}]
            ]
        ]
    );
    $layer.appendChild($modal);

    if(!inner && content && typeof content.appendChild !== "undefined") {
        let $content = $modal.querySelector(".modal-content");
        $content.appendChild(content);
    }

    maybe_apply_translator($modal, opts);

    let closed = false;
    let close_fn = null;
    let release_focus = null;

    let close = function() {
        if(closed) {
            return;
        }
        closed = true;
        if(release_focus) {
            release_focus();
            release_focus = null;
        }
        if(close_fn) {
            yui_shell_pop_escape(shell, close_fn);
            close_fn = null;
        }
        if($modal.parentNode) {
            $modal.parentNode.removeChild($modal);
        }
    };
    close_fn = close;

    yui_shell_push_escape(shell, "modal", close);
    release_focus = activate_focus_trap_on(
        $modal.querySelector(".modal-content")
    );

    if((opts == null || opts.dismiss_on_background !== false)) {
        $modal.querySelector(".modal-background").addEventListener("click", close);
    }
    $modal.querySelector(".modal-close").addEventListener("click", close);

    return { close };
}


/***************************************************************
 *              Blocking dialogs (Bulma .modal-card)
 *
 *      build_dialog returns a Promise that resolves with the
 *      clicked button's `value`.  Escape, the close button and
 *      the dismiss action all resolve with the LAST button's value
 *      (cancel/no/ok by convention — the safe-default action).
 ***************************************************************/
function build_dialog(shell, message, buttons, opts)
{
    let $layer = modal_layer(shell);
    if(!$layer) {
        log_warning("yui_shell_confirm_*: shell has no modal layer");
        return Promise.resolve(buttons[buttons.length - 1].value);
    }

    let title = (opts && opts.title) || "";
    let dismiss_value = buttons[buttons.length - 1].value;

    let $body_children = (typeof message === "string")
        ? [["p", {i18n: message}, message]]
        : [message];

    let title_attrs = !empty_string(title)
        ? {class: "modal-card-title", i18n: title}
        : {class: "modal-card-title"};

    let $footer_children = buttons.map(b => {
        let cls = "button";
        if(b.kind === "primary") {
            cls += " is-link";
        } else if(b.kind === "danger") {
            cls += " is-danger";
        }
        let btn_attrs = {class: cls, type: "button",
                         "data-modal-button-value": b.value};
        if(typeof b.label === "string") {
            btn_attrs.i18n = b.label;
        }
        return ["button", btn_attrs, b.label];
    });

    let $modal = createElement2(
        ["div", {class: "modal yui-modal yui-confirm is-active",
                 role: "dialog", "aria-modal": "true"},
            [
                ["div", {class: "modal-background"}],
                ["div", {class: "modal-card"},
                    [
                        ["header", {class: "modal-card-head"},
                            [
                                ["p", title_attrs, title],
                                ["button", {class: "delete",
                                            "aria-label": "close"}]
                            ]
                        ],
                        ["section", {class: "modal-card-body"}, $body_children],
                        ["footer", {class: "modal-card-foot"}, $footer_children]
                    ]
                ]
            ]
        ]
    );
    $layer.appendChild($modal);
    maybe_apply_translator($modal, opts);

    return new Promise(resolve => {
        let resolved = false;
        let close_fn = null;
        let release_focus = null;

        let close = function(value) {
            if(resolved) {
                return;
            }
            resolved = true;
            if(release_focus) {
                release_focus();
                release_focus = null;
            }
            if(close_fn) {
                yui_shell_pop_escape(shell, close_fn);
                close_fn = null;
            }
            if($modal.parentNode) {
                $modal.parentNode.removeChild($modal);
            }
            resolve(value);
        };
        close_fn = () => close(dismiss_value);

        yui_shell_push_escape(shell, "modal", close_fn);
        release_focus = activate_focus_trap_on(
            $modal.querySelector(".modal-card")
        );

        $modal.querySelector(".modal-background").addEventListener(
            "click", () => close(dismiss_value)
        );
        $modal.querySelector(".modal-card-head .delete").addEventListener(
            "click", () => close(dismiss_value)
        );
        let footer_buttons = $modal.querySelectorAll(
            ".modal-card-foot button"
        );
        footer_buttons.forEach($btn => {
            $btn.addEventListener("click", () => {
                close($btn.getAttribute("data-modal-button-value"));
            });
        });
    });
}


export function yui_shell_confirm_ok(shell, message, opts)
{
    let label = (opts && opts.ok_label) || "OK";
    return build_dialog(shell, message, [
        {label: label, value: "ok", kind: "primary"}
    ], opts).then(() => undefined);
}

export function yui_shell_confirm_yesno(shell, message, opts)
{
    let yes_label = (opts && opts.yes_label) || "Yes";
    let no_label  = (opts && opts.no_label)  || "No";
    return build_dialog(shell, message, [
        {label: yes_label, value: "yes", kind: "primary"},
        {label: no_label,  value: "no"}
    ], opts).then(v => v === "yes");
}

export function yui_shell_confirm_yesnocancel(shell, message, opts)
{
    let yes_label    = (opts && opts.yes_label)    || "Yes";
    let no_label     = (opts && opts.no_label)     || "No";
    let cancel_label = (opts && opts.cancel_label) || "Cancel";
    return build_dialog(shell, message, [
        {label: yes_label,    value: "yes", kind: "primary"},
        {label: no_label,     value: "no"},
        {label: cancel_label, value: "cancel"}
    ], opts);
}
