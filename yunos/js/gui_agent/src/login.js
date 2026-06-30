/***********************************************************************
 *          login.js
 *
 *          Pre-shell login screen. A full-page centered card with the
 *          brand, the tenant/plane it is signing into, a username +
 *          password form (with show/hide), a busy state and an error
 *          line. Mounted by C_APP when there is no session, unmounted
 *          on EV_LOGIN_ACCEPTED.
 *
 *          mount_login({on_submit}) -> { unmount, set_busy, set_error }
 *
 *          Copyright (c) 2026, ArtGins.
 *          All Rights Reserved.
 ***********************************************************************/
import {createElement2, refresh_language} from "@yuneta/gobj-js";
import {t} from "i18next";

import {deploy_info} from "./conf/deploy.js";


/***************************************************************
 *  Mount the login screen on document.body.
 ***************************************************************/
function mount_login(opts)
{
    opts = opts || {};
    let dep = deploy_info();
    let plane_label = (dep.plane === "agent22") ? "agent22" : "agents";

    /*  password input + show/hide eye  */
    let $pwd = createElement2(["input", {
        class: "input", type: "password", name: "password",
        placeholder: "", autocomplete: "current-password"
    }]);
    let $eye = createElement2(["i", {class: "yi-eye"}]);
    let $eyebtn = createElement2(
        ["button", {class: "button", type: "button", "aria-label": "show password",
                    tabindex: "-1"}, [["span", {class: "icon"}, [$eye]]]]
    );
    $eyebtn.addEventListener("click", () => {
        let show = ($pwd.type === "password");
        $pwd.type = show ? "text" : "password";
        $eye.className = show ? "yi-eye-slash" : "yi-eye";
        $pwd.focus();
    });

    let $user = createElement2(["input", {
        class: "input", type: "text", name: "username",
        placeholder: "", autocomplete: "username", autofocus: "true"
    }]);

    let $error = createElement2(["p", {class: "help is-danger", style: "min-height:1.25em;"}, ""]);

    let $btn = createElement2(
        ["button", {class: "button is-primary is-fullwidth", type: "submit", i18n: "login"}, "Sign In"]
    );

    let submit = () => {
        let username = $user.value.trim();
        let password = $pwd.value;
        if(!username || !password) {
            set_error(t("username and password are required"));
            return;
        }
        set_error("");
        set_busy(true);
        if(typeof opts.on_submit === "function") {
            opts.on_submit({username, password});
        }
    };

    let $form = createElement2(
        ["form", {class: "box", style: "width:100%; max-width:400px;"},
            [
                /*  brand  */
                ["div", {style: "text-align:center; margin-bottom:1.25rem;"}, [
                    ["img", {src: "/agent-mark.svg", alt: "",
                             style: "width:56px; height:56px;"}],
                    ["h1", {class: "title is-4 mt-3 mb-1", style: "color:#2E7CD6;"}, "Agent Console"],
                    ["p", {class: "is-size-7 has-text-grey"}, `${dep.tenant} · ${plane_label}`]
                ]],
                ["div", {class: "field"}, [
                    ["label", {class: "label is-small", i18n: "username"}, "Username"],
                    ["div", {class: "control"}, [$user]]
                ]],
                ["div", {class: "field"}, [
                    ["label", {class: "label is-small", i18n: "password"}, "Password"],
                    ["div", {class: "field has-addons mb-0"}, [
                        ["div", {class: "control is-expanded"}, [$pwd]],
                        ["div", {class: "control"}, [$eyebtn]]
                    ]]
                ]],
                $error,
                ["div", {class: "field mt-4"}, [["div", {class: "control"}, [$btn]]]]
            ]
        ]
    );
    $form.addEventListener("submit", (ev) => { ev.preventDefault(); submit(); });

    let $overlay = createElement2(
        ["div", {class: "yagent-login",
                 style: "position:fixed; inset:0; z-index:1000; display:flex; " +
                        "align-items:center; justify-content:center; padding:1.5rem; " +
                        "background:linear-gradient(135deg,#1F3A5F 0%,#2E7CD6 100%);"},
            [$form]]
    );
    document.body.appendChild($overlay);
    refresh_language($overlay, t);
    setTimeout(() => { $user.focus(); }, 0);

    function set_busy(busy)
    {
        $btn.classList.toggle("is-loading", !!busy);
        $user.disabled = !!busy;
        $pwd.disabled = !!busy;
    }

    function set_error(text)
    {
        $error.textContent = text || "";
    }

    function unmount()
    {
        if($overlay && $overlay.parentNode) {
            $overlay.parentNode.removeChild($overlay);
        }
        $overlay = null;
    }

    return {unmount, set_busy, set_error};
}

export {mount_login};
