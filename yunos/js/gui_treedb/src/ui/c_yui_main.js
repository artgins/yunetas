/***********************************************************************
 *          yui_main.js
 *
 *          Yuneta UI Main
 *
 *          Use EV_ON_OPEN/EV_ON_CLOSE (==> show_app_content())
 *              to show/hide the app ("content-layer", "iframe-layer")
 *
 *          Define the top toolbar
 *              - login
 *              - themes
 *              - languages
 *              - app menu
 *              - user menu
 *
 *          In the yui_main.css are the sizes.
 *
 *          Define layers:  top-layer
 *                          content-layer
 *                          bottom-layer
 *                          popup-layer
 *                          modal-layer
 *                          iframe-layer
 *
 *          The content-layer is managed in yui_routing.js
 *
 *          The tabs in a content-layer to manage sub-containers can be managed by yui_tabs.js
 *          or be integrated in each gobj.
 *
 *          Copyright (c) 2025, ArtGins.
 *          All Rights Reserved.
 ***********************************************************************/
/* global ResizeObserver, window, document */

import {
    __yuno__,
    SDATA,
    SDATA_END,
    data_type_t,
    event_flag_t,
    log_error,
    gclass_create,
    gobj_subscribe_event,
    gobj_read_pointer_attr,
    kw_get_local_storage_value,
    kw_set_local_storage_value,
    createElement2,
    empty_string,
    is_array,
    is_string,
    createOneHtml,
    kw_get_str,
    getPositionRelativeToBody,
    gobj_find_service,
    gobj_read_attr,
    gobj_send_event,
    gobj_write_attr,
    gobj_publish_event,
    gobj_default_service,
    refresh_language,
    debounce,
} from "yunetas";

import {flags_of_world} from "../locales/flags.js";
import {logo_wide_svg} from "./logos_svg.js";
import {setup_dev} from "./yui_dev.js";
import {yui_toolbar} from "./yui_toolbar.js";

import i18next, {t} from 'i18next';

import "./c_yui_main.css"; // Must be in index.js ?

/***************************************************************
 *              Constants
 ***************************************************************/
const GCLASS_NAME = "C_YUI_MAIN";

/***************************************************************
 *              Data
 ***************************************************************/
const attrs_table = [
SDATA(data_type_t.DTP_POINTER,  "subscriber",           0,  null,           "Subscriber of output events"),
SDATA(data_type_t.DTP_STRING,   "username",             0,  "",             "username logged"),
SDATA(data_type_t.DTP_STRING,   "publi_page",           0,  "publi_page",   "Publicity page shown when user is not logged in"),
SDATA(data_type_t.DTP_STRING,   "theme",                0,  "light",        "Theme: light or dark"),
SDATA(data_type_t.DTP_STRING,   "logo_wide",            0,  "logo-wide.png", "Logo for the app"),
SDATA(data_type_t.DTP_REAL,     "width",                0,  "-1",           "Main Window size. -1 -> set full size of the device"),
SDATA(data_type_t.DTP_REAL,     "height",               0,  "-1",           "Main Window size"),
SDATA(data_type_t.DTP_STRING,   "color_yuneta_connected",0, "#32CD32",      "LimeGreen - connected"),
SDATA(data_type_t.DTP_STRING,   "color_yuneta_disconnected",0, "#FF4500",   "OrangeRed - disconnected"),
SDATA(data_type_t.DTP_STRING,   "color_user_login",     0,  "#32CD32",      "LimeGreen - user logged in"),
SDATA(data_type_t.DTP_STRING,   "color_user_logout",    0,  "#FF4500",      "OrangeRed - user logged out"),
SDATA_END()
];

let PRIVATE_DATA = {};

let __gclass__ = null;




                    /******************************
                     *      Framework Methods
                     ******************************/




/***************************************************************
 *          Framework Method: Create
 ***************************************************************/
function mt_create(gobj)
{
    build_ui(gobj);

    let current_theme = kw_get_local_storage_value("theme", "light", true);
    set_theme(gobj, current_theme);

    /*
     *  SERVICE subscription model
     */
    const subscriber = gobj_read_pointer_attr(gobj, "subscriber");
    if(subscriber) {
        gobj_subscribe_event(gobj, null, {}, subscriber);
    }

}

/***************************************************************
 *          Framework Method: Start
 ***************************************************************/
function mt_start(gobj)
{
    resize_and_subscribe_to_system(gobj);
}

/***************************************************************
 *          Framework Method: Stop
 ***************************************************************/
function mt_stop(gobj)
{
    // TODO gobj.clear_timeout();
}

/***************************************************************
 *          Framework Method: Destroy
 ***************************************************************/
function mt_destroy(gobj)
{
}




                    /***************************
                     *      Local Methods
                     ***************************/




/************************************************************
 *   Build UI
 ************************************************************/
function build_ui(gobj)
{
    /*----------------------------------------------*
     *  Layout Schema
     *----------------------------------------------*/
    const $layout = createElement2(
        ['div', {id: 'root', class: 'root'}, [
            ['div', {id: 'top-layer', class: 'top-layer'}],
            ['div', {id: 'content-layer', class: 'content-layer is-hidden'}],
            ['div', {id: 'bottom-layer', class: 'bottom-layer'}],
            ['div', {id: 'popup-layer', class: 'popup-layer'}],
            ['div', {id: 'modal-layer', class: 'modal-layer'}],
            ['div', {id: 'iframe-layer', class: 'iframe-layer'},
                `<iframe id="iframe-publi-page" src="./${gobj_read_attr(gobj, "publi_page")}/index.html" width="100%" height="100%"></iframe>`]
        ]]
    );
    document.body.appendChild($layout);

    const iframe = document.getElementById('iframe-publi-page');
    if(iframe) {
        iframe.onload = () => {
            const theme = kw_get_local_storage_value("theme", "light", true);
            let iframeDocument = iframe.contentDocument || iframe.contentWindow.document;
            iframeDocument.documentElement.setAttribute("data-theme", theme);
            refresh_language(iframeDocument, t);
        };
    }

    /*---------------------------------------*
     *      Top Header toolbar
     *---------------------------------------*/
    /*
     *  Left
     */
    let left_items = [
        ['button', {
            class: 'button',
            style: {
                height: `var(--top-layer-size-inside)`,
                width: `2.5em`
            }
        }, [
            'i', {
                id: 'icon_yuneta_state',
                style: `font-size:1.5em; color:${gobj_read_attr(gobj, "color_yuneta_disconnected")};`,
                class: 'fa-solid fa-bars'
            }
        ], {
            click: (evt) => {
                evt.stopPropagation();
                gobj_send_event(gobj, "EV_APP_MENU", {evt: evt}, gobj);
            },
            contextmenu: (evt) => {
                evt.stopPropagation();
                evt.preventDefault();
                gobj_send_event(gobj, "EV_APP_MENU", {evt: evt}, gobj);
            }
        }]
    ];

    /*
     *  Center
     */
    let center_items = [
        ['img',
            {
                src: `${gobj_read_attr(gobj, "logo_wide")}`,
                alt: `${gobj_read_attr(gobj, "logo_wide")}`,
                height: "36",
                width: "72",
                style: "cursor: pointer;"
            },
            '',
            {
                click: (evt) => {
                    evt.stopPropagation();
                    gobj_send_event(gobj, "EV_HOME", {evt: evt}, gobj);
                }
            }
        ]

        // ['img', {
        //     style: {
        //         height: `var(--top-layer-size-inside)`,
        //         width: `var(2 * --top-layer-size-inside)`
        //     }
        // },
        //     `${logo_wide_svg}`,
        //     {
        //         click: (evt) => {
        //             evt.stopPropagation();
        //             gobj_send_event(gobj, "EV_HOME", {evt: evt}, gobj);
        //         }
        //     }]
    ];

    /*
     *  Right
     */
    let right_items = [
        [
            'button', {
            id: "EV_SELECT_LANGUAGE",
            class: 'button has-text-link',
            style: {
                'margin-left': '4px',
                height: `var(--top-layer-size-inside)`,
                width: `2.5em`,
            }
        }, [
            ['i', {style: 'font-size:1.5em;', class: 'fa-solid fa-language'}]
        ], {
            click: (evt) => {
                evt.stopPropagation();
                gobj_send_event(gobj, "EV_SELECT_LANGUAGE", {evt: evt}, gobj);
            }
        }
        ], [
            'button', {
                id: 'theme-change',
                class: 'button',
                style: {
                    'margin-left': '4px',
                    height: `var(--top-layer-size-inside)`,
                    width: `2.5em`,
                }
            }, [
                ['i', {
                    style: 'font-size:1.5em; color:#8156F5; display:none;',
                    class: 'theme-dark fa-solid fa-moon'
                }],
                ['i', {
                    style: 'font-size:1.5em; color:#FFB70F; display:none;',
                    class: 'theme-light fa-solid fa-sun'
                }]
            ], {
                click: (evt) => {
                    evt.stopPropagation();
                    gobj_send_event(gobj, "EV_SELECT_THEME", {evt: evt}, gobj);
                }
            }
        ], [
            'button',
            {
                id: "EV_USER_MENU",
                class: 'button',
                style: {
                    'margin-left': '4px',
                    display: 'flex',
                    height: `var(--top-layer-size-inside)`
                }
            }, [
                ['span', {id: 'tag_username', class: 'tag is-hidden-mobile', i18n: 'login'}, 'login'],
                ['i', {
                    id: 'icon_username',
                    style: `font-size:1.5em; color:${gobj_read_attr(gobj, "color_user_logout")};`,
                    class: 'fa-solid fa-user'
                }]
            ], {
                click: (evt) => {
                    evt.stopPropagation();
                    gobj_send_event(gobj, "EV_USER_MENU", {evt: evt}, gobj);
                }
            }
        ]
    ];

    const $toolbar_header = yui_toolbar({}, [
        ['div', {class: 'yui-horizontal-toolbar-section left'}, left_items],
        ['div', {class: 'yui-horizontal-toolbar-section center is-hidden-mobile'}, center_items],
        ['div', {class: 'yui-horizontal-toolbar-section right'}, right_items]
    ]);

    document.getElementById('top-layer').appendChild($toolbar_header);

    refresh_language($toolbar_header, t);

    /*---------------------------------------*
     *      Top Breadcrumb toolbar
     *---------------------------------------*/

    /*---------------------------------------*
     *      Work area
     *---------------------------------------*/
    // build_work_area(gobj);

    /*---------------------------------------*
     *      Add menu items
     *---------------------------------------*/
    // add_menu_item("EV_HOME", t("Home"), "fas fa-home", false);
    // add_menu_item("EV_USER", t("Profile"), "fas fa-user", false);
    // add_menu_item("EV_MESSAGE", t("Messages"), "fas fa-envelope", false);
    // add_menu_item("EV_SETTING", t("Settings"), "fas fa-cog", false);
    // add_menu_item("EV_XXX", t("Home"), "fas fa-home", false);
    //
    // set_active_menu_item("EV_USER", true);

    /*
     *  Important to adjust some elements
     */
    window.dispatchEvent(new Event('resize'));
}

/************************************************************
 *   Set theme
 ************************************************************/
function set_theme(gobj, theme)
{
    gobj_write_attr(gobj, "theme", theme);

    let elm = document.querySelectorAll("#theme-change i");
    elm.forEach(function (element) {
        element.style.display = 'none';
    });

    switch (theme) {
        case 'dark':
            elm = document.querySelector("#theme-change i.theme-dark");
            break;
        case 'light':
        default:
            elm = document.querySelector("#theme-change i.theme-light");
            break;
    }
    document.documentElement.setAttribute("data-theme", theme);
    if(elm) {
        elm.style.display = 'flex';
    }

    const iframe = document.getElementById('iframe-publi-page');
    if(iframe && iframe.contentWindow) {
        iframe.contentWindow.document.documentElement.setAttribute("data-theme", theme);
    } else {
        log_error("WTF!");
    }
    gobj_publish_event(gobj, "EV_THEME", {theme: theme});
}

/************************************************************
 *
 ************************************************************/
function change_language(new_language)
{
    kw_set_local_storage_value("locale", new_language?new_language:"es");
    i18next.changeLanguage(new_language);
    refresh_language(null, t);

    const iframe = document.getElementById('iframe-publi-page');
    let iframeDocument = iframe.contentDocument || iframe.contentWindow.document;
    refresh_language(iframeDocument, t);

    window.dispatchEvent(new Event('resize'));
}

/************************************************************
 *
 ************************************************************/
function show_select_language(gobj, show)
{
    let $element = document.getElementById('popup-select-language');
    if(!$element) {
        $element = createElement2([
            'div', { id: 'popup-select-language', class: 'dropdown is-right popup' }, [
                ['div', { class: 'dropdown-menu', role: 'menu', style: 'min-width:4rem;' }, [
                    ['div', { class: 'dropdown-content', style: 'padding: 0;' }, [
                        ['a', {class: 'dropdown-item flex-horizontal-section', 'data-value':'es'}, [
                            ['span', {style: 'width:1.5em; height:1.5em;'}, flags_of_world['es']],
                            ['span', {i18n: 'es'}, 'Español']

                        ]],
                        ['a', {class: 'dropdown-item flex-horizontal-section', 'data-value':'en'}, [
                            ['span', {style: 'width:1.5em; height:1.5em;'}, flags_of_world['en']],
                            ['span', {i18n: 'en'}, 'Inglés']
                        ]]
                    ]]
                ]]
            ], {
                'click': (event) => {
                    //////////////////////////////////////////////////////////////////////
                    //  WARNING
                    //  if do not stop propagation, the event will arrive until body,
                    //  and an auto-closing function will remove is-active class
                    //  and this popup will not open
                    //  (Bulma components that it opens with toggling "is-active" class)
                    //////////////////////////////////////////////////////////////////////
                    event.stopPropagation();
                    event.currentTarget.classList.toggle("is-active");
                    const target = event.target.closest('.dropdown-item');
                    change_language(target.dataset.value);
                }
            }
        ]);
        document.getElementById("popup-layer").appendChild($element);
    }

    let pos = getPositionRelativeToBody(document.getElementById("EV_SELECT_LANGUAGE"));
    $element.style.position = "absolute";
    $element.style.top = pos.bottom + "px";
    $element.style.left = pos.right + "px";
    if(show) {
        let locale = kw_get_local_storage_value("locale", "es", true);

        let $items = $element.querySelectorAll('a[data-value]');
        $items.forEach(function($item) {
            $item.classList.remove('is-active');
        });

        let $item = $element.querySelector(`a[data-value="${locale}"]`);
        $item.classList.add('is-active');

        $element.classList.add('is-active');
    } else {
        $element.classList.remove('is-active');
    }
}

/************************************************************
 *
 ************************************************************/
// <!-- aqui va forgot password -->
// <div class="field">
//     <a class="button is-text is-size-7 forgot-password" data-i18n="forgot password?">Forgot Password?</a>
// </div>

let login_html = `
<div id="modal-login" class="modal">
    <div class="modal-background">
    </div>
    <div class="modal-content" style="max-width:400px;">
        <div class="box">
            <div class = "is-flex is-flex-direction-row is-justify-content-center mb-5">
                <figure class="image" style="width: 60%;">
                    ${logo_wide_svg}
                </figure>
            </div>
            <!-- Form -->
            <form>
                <div class="field">
                    <label class="label" data-i18n="email">Email</label>
                    <div class="control has-icons-left">
                        <input class="input with-focus input-email" type="email" placeholder="name@example.com" required>
                        <span class="icon is-small is-left">
                            <i class="fa fa-envelope"></i>
                        </span>
                    </div>
                </div>

                <div class="field">
                    <label class="label"  data-i18n="password">Password</label>
                    <div class="control has-icons-left has-icons-right">
                        <input class="input input-password" type="password" placeholder="********" required>
                        <span class="icon is-small is-left">
                            <i class="fa fa-lock"></i>
                        </span>
                        <span class="icon is-small is-right is-clickable toggle-password">
                            <i class="fa fa-eye"></i>
                        </span>
                    </div>
                    <!-- aqui va forgot password -->
                </div>

               <!-- Warning Message is-hidden -->
                <article class="message is-danger is-hidden invalid-username-password">
                    <div style="padding: 0.5em 1em;" class="message-body" data-i18n="invalid-username-password">
                        Invalid username or password.
                    </div>
                </article>

                <div class="pt-4"></div>
                <div class="control">
                    <button type="submit" class="button is-link full-width-button process-form" data-i18n="login">Login</button>
                </div>
                <div class="pb-4"></div>
            </form>
        </div>
    </div>
    <button class="modal-close is-large" aria-label="close"></button>
</div>
`;

/************************************************************
 *
 ************************************************************/
function show_login_form(gobj)
{
    let $element = document.getElementById("modal-login");
    if(!$element) {
        $element = createOneHtml(login_html);
        refresh_language($element, t);

        /*------------------------*
         *      Add events
         *------------------------*/
        /*
         *  Don't propagate click
         */
        $element.addEventListener('click', function(event) {
            event.stopPropagation();
        });

        /*
         *  Button close
         */
        $element.querySelector('.modal-close').addEventListener('click', function(event) {
            event.stopPropagation();
            $element.classList.remove('is-active');
        });

        /*
         *  Forgot password
         */
        // $element.querySelector('.forgot-password').addEventListener('click', function(event) {
        //     event.stopPropagation();
        //     $element.classList.remove('is-active');
        //     show_recovery_password(gobj, true);
        // });

        /*
         *  Button password visibility
         */
        let $input_password = $element.querySelector('.input-password');
        let $toggle_password = $element.querySelector('.toggle-password');
        $toggle_password.addEventListener('click', function(event) {
            event.stopPropagation();
            let icon = $toggle_password.querySelector('i');
            if ($input_password.type === 'password') {
                $input_password.type = 'text';
                icon.classList.replace('fa-eye', 'fa-eye-slash');
            } else {
                $input_password.type = 'password';
                icon.classList.replace('fa-eye-slash', 'fa-eye');
            }
            $input_password.focus();
        });

        /*
         *  Submit
         */
        $element.addEventListener('submit', function(event) {
            const email = $element.querySelector('.input-email').value;
            const password = $element.querySelector('.input-password').value;

            event.stopPropagation();
            event.preventDefault(); // if not set in mobiles refresh the page
            let kw = {
                 username: email,
                 password: password
            };
            gobj_send_event(gobj, "EV_DO_LOGIN", kw, gobj);
        });

        /*
         *  Add to popup layer
         */
        document.getElementById("popup-layer").appendChild($element);
    }

    $element.querySelector('.invalid-username-password').classList.add('is-hidden');
    $element.classList.add('is-active');
    $element.querySelector('.with-focus').focus();
}

/************************************************************
 *
 ************************************************************/
function hide_login_form(gobj)
{
    let $element = document.getElementById("modal-login");
    if($element) {
        $element.classList.remove('is-active');
    }
}

/************************************************************
 *
 ************************************************************/
function set_error_login_message(gobj, message)
{
    let $element = document.querySelector("#modal-login .invalid-username-password");

    let $content = $element.querySelector(".message-body");
    if($content) {
        $content.textContent = `${message}`;
    }

    if(empty_string(message)) {
        $element.classList.add('is-hidden');
    } else {
        $element.classList.remove('is-hidden');
    }
}

/************************************************************
 *
 ************************************************************/
let recovery_password_html = `
<div id="modal-recovery-password" class="modal">
    <div class="modal-background">
    </div>
    <div class="modal-content" style="max-width:600px;">
        <div class="box" >
            <div class = "is-flex is-flex-direction-row is-justify-content-center mb-5">
                <figure class="image" style="width: 60%;">
                    ${logo_wide_svg}
                </figure>
            </div>
            <!-- Form -->
            <div class="mb-5" data-i18n="instructions password recovery">instructions password recovery</div>
            <form>
                <div class="field">
                    <label class="label" for="recovery-email" data-i18n="email">Email</label>
                    <div class="control has-icons-left">
                        <input id="recovery-email" class="input with-focus" type="email" placeholder="name@example.com" required aria-required="true">
                        <span class="icon is-small is-left">
                            <i class="fas fa-envelope"></i>
                        </span>
                    </div>
                </div>
                <div class="field">
                    <div class="control has-text-centered">
                        <button type="submit" class="button is-info" data-i18n="submit">Submit</button>
                    </div>
                </div>
            </form>
        </div>
    </div>
    <button class="modal-close is-large" aria-label="close"></button>
</div>
`;

/************************************************************
 *
 ************************************************************/
function show_recovery_password(gobj, show)
{
    let $element = document.getElementById("modal-recovery-password");
    if(!$element) {
        $element = createOneHtml(recovery_password_html);
        refresh_language($element, t);

        /*
         *  Don't propagate click
         */
        $element.addEventListener('click', function(event) {
            event.stopPropagation();
        });

        /*------------------------*
         *      Add events
         *------------------------*/
        /*
         *  Button close
         */
        $element.querySelector('.modal-close').addEventListener('click', function(event){
            event.stopPropagation();
            $element.classList.remove('is-active');
        });

        /*
         *  Submit
         */
        $element.addEventListener('submit', function(event) {
            event.stopPropagation();
            event.preventDefault(); // if not set in mobiles refresh the page
            $element.classList.remove('is-active');
            let kw = {
                 username: "username"
            };
            gobj_send_event(gobj, "EV_DO_RECOVERY_PASSWORD", kw, gobj);
        });

        /*
         *  Add to popup layer
         */
        document.getElementById("popup-layer").appendChild($element);
    }

    if(show) {
        $element.classList.add('is-active');
        $element.querySelector('.with-focus').focus();
    } else {
        $element.classList.remove('is-active');
    }
}

/************************************************************
 *
 ************************************************************/
function show_user_menu(gobj, show, pos)
{
    let $element = document.getElementById('popup-user-menu');
    if(!$element) {
        $element = createElement2([
            'div', { id: 'popup-user-menu', style: 'position=absolute;', class: 'dropdown is-right popup' }, [
                ['div', { class: 'dropdown-menu', role: 'menu', style: 'min-width:4rem;' }, [
                    ['div', { class: 'dropdown-content', style: 'padding: 0;' }, [
                        // ['hr', {class: 'dropdown-divider'}],
                        ['a', {class: 'dropdown-item flex-horizontal-section'}, [
                            ['span', {class:'pr-2', i18n: 'logout'}, 'logout'],
                            ['i', {style: `color: ${gobj_read_attr(gobj, "color_user_logout")}`, class: 'fas fa-sign-out-alt'}]
                        ],
                        {
                            click: (evt) => {
                                evt.stopPropagation();
                                evt.currentTarget.closest('.popup').classList.toggle("is-active");
                                gobj_send_event(gobj, "EV_DO_LOGOUT", {evt: evt}, gobj);
                            }
                        }]
                    ]]
                ]]
            ]
        ]);
        document.getElementById("popup-layer").appendChild($element);
    }

    // $element.style.position = "absolute";
    $element.style.top = pos.bottom + "px";
    $element.style.left = pos.right + "px";
    if(show) {
        $element.classList.add('is-active');
    } else {
        $element.classList.remove('is-active');
    }
}

/************************************************
 *
 ************************************************/
function set_username(gobj, username)
{
    gobj_write_attr(gobj, "username", username);

    /*
     *  Change color user icon
     */
    document.getElementById("icon_username").style.color = username?
        gobj_read_attr(gobj, "color_user_login") : gobj_read_attr(gobj, "color_user_logout");

    /*
     *  Change text user label
     */
    let $element = document.getElementById("tag_username");
    $element.textContent = username?username:t('login');
    if(username) {
        $element.setAttribute('data-i18n', '');
    } else {
        $element.setAttribute('data-i18n', 'login');
    }
}

/************************************************************
 *
 ************************************************************/
let modal_message_html = `
<div id="modal-message" class="modal is-active">
    <div class="modal-background"></div>
    <div class="modal-content">
        <!-- Any other Bulma elements you want can go here -->
        <div class="box notification is-warning">
            <h1 class="title has-text-centered"></h1>
            <div class="content"></div>
            <button class="button is-large delete"></button>
        </div>
    </div>
</div>
`;

/************************************************************
 *  type: 'info', 'success', 'warning', 'danger'
 *  buttons: [{content, callback}]
 *      content can be:
 *          string  -> createOneHtml()
 *          []      -> createElement2()
 *
 *      callback:
 *          function()
 *
 *  Add class 'with-focus' to the element you want the focus
 *  Add class 'to-close' to the buttons INSIDE msg, that you want to close the modal

Example:
    display_volatil_modal({
        title: "Info",
        type: "info",
        x_close: true,
        msg: `<div class="">
            <p class="title">Are you sure you want to proceed?</p>
            <div class="buttons is-centered">
                <button class="button to-close is-success" id="yesButton">Yes</button>
                <button class="button to-close is-danger" id="noButton">No</button>
                <button class="button to-close" id="cancelButton">Cancel</button>
            </div>
        </div>`,
        buttons: [
            {
                content:
                `<button class="button is-success px-6 with-focus">${t('accept')}</button>`,
                callback: function(evt) {
                    console.dir(evt)
                    console.log("BUTTON callback");
                }
            }
        ],
        callback: function(evt) {
            console.log("GLOBAL callback");
        }
    });

 ************************************************************/
function display_volatil_modal(
    {title, msg, callback, type, x_close, buttons}
) {
    const callback_ = callback;  // ✅ Assign to a constant
    let $element = createElement2([
        'div', {class: 'modal'}, [
            ['div', {class: 'modal-background'}, ''],
            ['div', {class: 'modal-card'}, []]
        ]
    ]);

    const destroyModal = ()=> {
        $element.classList.remove('is-active');
        $element.parentNode.removeChild($element);
    };

    let $modal_card = $element.querySelector('.modal-card');
    if(title || x_close) {
        if(!type) {
            type = "";
        }
        let background_color = "";
        switch(type.toLowerCase()) {
            case "info":
                background_color = 'has-background-info';
                break;
            case "success":
                background_color = 'has-background-success';
                break;
            case "warning":
                background_color = 'has-background-warning';
                break;
            case "error":
            case "danger":
                background_color = 'has-background-danger';
                break;
        }

        let title_ = !title ? [] : ['p', {class: 'modal-card-title has-text-centered'}, title];
        let x_close_ = !x_close ? [] : ['button', {class: 'delete is-large', 'aria-label': 'close'}];

        let $header = createElement2(
            ['header', {class: `modal-card-head ${background_color} p-3`}, [
                title_,
                x_close_
            ]]
        );
        $modal_card.appendChild($header);
    }

    let $section = createElement2(
        ['section', {class: 'modal-card-body has-text-centered'}, createOneHtml(msg)]
    );
    $modal_card.appendChild($section);

    if(buttons && buttons.length) {
        let $footer = createElement2(
            ['footer', {class: 'modal-card-foot p-3'}, [
                ['div', {class: 'buttons', style: 'justify-content:center; width:100%;'}]
            ]]
        );
        $modal_card.appendChild($footer);

        let $buttons = $footer.querySelector('.buttons');

        for(let button of buttons) {
            let content = button.content;
            const btn_callback = button.callback;
            let $button = null;

            if(is_string(content)) {
                $button = createOneHtml(content);
            } else if(is_array(content)) {
                $button = createElement2(content);
            }

            if($button) {
                const clickHandler = (evt)=> {
                    evt.stopPropagation();
                    destroyModal();
                    if(btn_callback) {
                        btn_callback(evt);
                    } else if(callback_) {
                        callback_(evt);
                    }
                };

                $button.addEventListener('click', clickHandler);
                $buttons.appendChild($button);
            }
        }
    }

    refresh_language($element, t);

    /*------------------------*
     *      Add events
     *------------------------*/
    $element.querySelectorAll('.delete, .to-close').forEach((element)=> {
        element.addEventListener('click', (evt)=> {
            evt.stopPropagation();
            destroyModal();
            if(callback_) {
                callback_(evt);
            }
        });
    });

    /*
     *  Add to popup layer
     */
    document.getElementById("popup-layer").appendChild($element);
    $element.classList.add('is-active');

    /*
     *  Set focus
     */
    let $with_focus = $element.querySelector('.with-focus');
    if($with_focus) {
        $with_focus.focus();
    }
}

/************************************************************
 *
 ************************************************************/
function display_info_message(title, msg, callback)
{
    if(!title) {
        title = t("info");
    }
    display_volatil_modal({
        title: title,
        msg: msg,
        callback: callback,
        type: 'info',
        buttons: [{
            content:
            `<button class="button is-success px-6 with-focus">${t('accept')}</button>`,
        }]
    });
}

/************************************************************
 *
 ************************************************************/
function display_warning_message(title, msg, callback)
{
    if(!title) {
        title = t("warning");
    }
    display_volatil_modal({
        title: title,
        msg: msg,
        callback: callback,
        type: 'warning',
        buttons: [{
            content:
                `<button class="button is-success px-6 with-focus">${t('accept')}</button>`,
        }]
    });
}

/************************************************************
 *
 ************************************************************/
function display_error_message(title, msg, callback)
{
    if(!title) {
        title = t("error");
    }
    display_volatil_modal({
        title: title,
        msg: msg,
        callback: callback,
        type: 'error',
        buttons: [{
            content:
                `<button class="button is-success px-6 with-focus">${t('accept')}</button>`,
        }]
    });
}

/************************************************************
 *
 ************************************************************/
function get_yesnocancel(msg, callback)
{
    let content =
        `<div class="">
        <p class="title">${t(msg)}</p>
        <div class="buttons is-centered">
            <button class="button to-close is-success px-6 with-focus yesButton">${t('yes')}</button>
            <button class="button to-close is-danger px-6 noButton">${t('no')}</button>
            <button class="button to-close px-5 cancelButton">${t('cancel')}</button>
        </div>
    </div>`;

    const callback_ = (evt) => {
        if (evt.currentTarget.classList.contains('yesButton')) {
            callback("yes", evt);
        } else if (evt.currentTarget.classList.contains('noButton')) {
            callback("no", evt);
        } else if (evt.currentTarget.classList.contains('cancelButton')) {
            callback("cancel", evt);
        }
    };

    display_volatil_modal({
        title: null,
        msg: content,
        callback: callback_,
        type: 'info'
    });
}

/************************************************************
 *
 ************************************************************/
function get_yesno(msg, callback)
{
    let content =
    `<div class="">
        <p class="title">${msg}</p>
        <div class="buttons is-centered">
            <button class="button to-close is-success px-6 with-focus yesButton">${t('yes')}</button>
            <button class="button to-close is-danger px-6 noButton">${t('no')}</button>
        </div>
    </div>`;

    const callback_ = (evt) => {
        if (evt.currentTarget.classList.contains('yesButton')) {
            callback("yes", evt);
        } else if (evt.currentTarget.classList.contains('noButton')) {
            callback("no", evt);
        } else if (evt.currentTarget.classList.contains('cancelButton')) {
            callback("cancel", evt);
        }
    };

    display_volatil_modal({
        title: null,
        msg: content,
        callback: callback_,
        type: 'info'
    });
}

/************************************************************
 *
 ************************************************************/
function get_ok(msg, callback)
{
    let content =
        `<div class="">
        <p class="title">${msg}</p>
        <div class="buttons is-centered">
            <button class="button to-close is-success px-6 with-focus yesButton">${t('accept')}</button>
        </div>
    </div>`;

    const callback_ = (evt) => {
        if (evt.currentTarget.classList.contains('yesButton')) {
            callback("yes", evt);
        } else if (evt.currentTarget.classList.contains('noButton')) {
            callback("no", evt);
        } else if (evt.currentTarget.classList.contains('cancelButton')) {
            callback("cancel", evt);
        }
    };

    display_volatil_modal({
        title: null,
        msg: content,
        callback: callback_,
        type: 'info'
    });
}

/************************************************************
 *      Switch between app and publi
 ************************************************************/
function show_app_content(gobj, show)
{
    /*
     *  Activate layers
     */
    let $content_layer = document.getElementById("content-layer");
    let $iframe_layer = document.getElementById("iframe-layer");
    if(show) {
        // Activate own content
        $content_layer.classList.remove('is-hidden');
        $iframe_layer.classList.add('is-hidden');
    } else {
        // Activate publi content
        $content_layer.classList.add('is-hidden');
        $iframe_layer.classList.remove('is-hidden');
    }
}

/************************************************************
 *
 ************************************************************/
function resize_and_subscribe_to_system(gobj)
{
    function resize() {
        const width = document.body.offsetWidth;
        const height = document.body.offsetHeight;

        gobj_write_attr(gobj, "width", width);
        gobj_write_attr(gobj, "height", height);

        gobj_publish_event(gobj, "EV_RESIZE", {
            width,
            height
        });
    }

    const debouncedResize = debounce(resize, 300);

    const observer = new ResizeObserver(() => {
        debouncedResize();
    });

    observer.observe(document.body);

    resize();
}


                    /***************************
                     *      Actions
                     ***************************/




/************************************************
 *  {
 *      username:
 *      password:
 *  }
 ************************************************/
function ac_do_login(gobj, event, kw, src)
{
    set_error_login_message(gobj, "");

    let __login__ = gobj_find_service("__login__", true);
    gobj_send_event(__login__, "EV_DO_LOGIN", kw, gobj);
    return 0;
}

/************************************************
 *
 ************************************************/
function ac_do_logout(gobj, event, kw, src)
{
    let __login__ = gobj_find_service("__login__", true);
    gobj_send_event(__login__, "EV_DO_LOGOUT", kw, gobj);
    return 0;
}

function ac_do_recovery_password(gobj, event, kw, src)
{
    // TODO
    return 0;
}

/************************************************
 *  {
 *      username:
 *      jwt:
 *  }
 ************************************************/
function ac_login_accepted(gobj, event, kw, src)
{
    hide_login_form(gobj);
    set_username(gobj, kw.username);
    return 0;
}

/************************************************
 *
 ************************************************/
function ac_login_refreshed(gobj, event, kw, src)
{
    return 0;
}

/************************************************
 *
 ************************************************/
function ac_login_denied(gobj, event, kw, src)
{
    set_username(gobj, "");
    let error = kw_get_str(gobj, kw, "error", "login denied", 0);
    set_error_login_message(gobj, t(error));
    return 0;
}

/************************************************
 *
 ************************************************/
function ac_logout_done(gobj, event, kw, src)
{
    set_username(gobj, "");
    return 0;
}

/************************************************
 *
 ************************************************/
function ac_select_language(gobj, event, kw, src)
{
    show_select_language(gobj, true);
    return 0;
}

/************************************************
 *
 ************************************************/
function ac_app_menu(gobj, event, kw, src)
{
    if(kw.evt.type === 'click') {
        gobj_publish_event(gobj, event, {});
    } else if(kw.evt.type === 'contextmenu') {
        if(!gobj_find_service("Developer-Window")) {
            setup_dev(gobj, true);
        }
    }
    return 0;
}

/************************************************
 *
 ************************************************/
function ac_home(gobj, event, kw, src)
{
    let home = gobj_read_attr(gobj_default_service(), "home");
    if(home) {
        gobj_send_event(__yuno__.__yui_routing__,
            "EV_SELECT",
            {
                id: home
            },
            gobj
        );
    }

    return 0;
}

/************************************************
 *
 ************************************************/
function ac_select_theme(gobj, event, kw, src)
{
    let current_theme = kw_get_local_storage_value("theme", "light", true);

    let new_theme;
    switch (current_theme) {
        case 'dark':
            new_theme = 'light';
            break;
        case 'light':
        default:
            new_theme = 'dark';
            break;
    }

    set_theme(gobj, new_theme);
    kw_set_local_storage_value("theme", new_theme);

    return 0;
}

/************************************************
 *
 ************************************************/
function ac_user_menu(gobj, event, kw, src)
{
    if (empty_string(gobj_read_attr(gobj, "username"))) {
        show_login_form(gobj);
    } else {
        let element = kw.evt.currentTarget; // button clicked
        let pos = getPositionRelativeToBody(element);
        show_user_menu(gobj, true, pos);
    }

    return 0;
}

/********************************************
 *  Connected to yuneta
 ********************************************/
function ac_on_open(gobj, event, kw, src)
{
    /*
     *  Change color icon
     */
    document.getElementById("icon_yuneta_state").style.color = gobj_read_attr(gobj, "color_yuneta_connected");

    show_app_content(gobj, true);
    return 0;
}

/********************************************
 *  Disconnected from yuneta
 ********************************************/
function ac_on_close(gobj, event, kw, src)
{
    /*
     *  Change color icon
     */
    document.getElementById("icon_yuneta_state").style.color = gobj_read_attr(gobj, "color_yuneta_disconnected");

    show_app_content(gobj, false);
    return 0;
}

/************************************************
 *
 ************************************************/
function ac_timeout(gobj, event, kw, src)
{
    if (empty_string(gobj_read_attr(gobj, "username"))) {
        // show_login_form(gobj);
    }
    return 0;
}




                    /***************************
                     *          FSM
                     ***************************/




/*---------------------------------------------*
 *          Global methods table
 *---------------------------------------------*/
const gmt = {
    mt_create:  mt_create,
    mt_start:   mt_start,
    mt_stop:    mt_stop,
    mt_destroy: mt_destroy
};

/***************************************************************
 *          Create the GClass
 ***************************************************************/
function create_gclass(gclass_name)
{
    if (__gclass__) {
        log_error(`GClass ALREADY created: ${gclass_name}`);
        return -1;
    }

    /***************************************************************
     *          FSM
     ***************************************************************/
    /*---------------------------------------------*
     *          States
     *---------------------------------------------*/
    const states = [
        ["ST_IDLE", [
            ["EV_LOGIN_ACCEPTED",       ac_login_accepted,      null],
            ["EV_LOGIN_DENIED",         ac_login_denied,        null],
            ["EV_LOGIN_REFRESHED",      ac_login_refreshed,     null],
            ["EV_LOGOUT_DONE",          ac_logout_done,         null],
            ["EV_DO_LOGIN",             ac_do_login,            null],
            ["EV_DO_LOGOUT",            ac_do_logout,           null],
            ["EV_SELECT_LANGUAGE",      ac_select_language,     null],
            ["EV_APP_MENU",             ac_app_menu,            null],
            ["EV_HOME",                 ac_home,                null],
            ["EV_SELECT_THEME",         ac_select_theme,        null],
            ["EV_USER_MENU",            ac_user_menu,           null],
            ["EV_ON_OPEN",              ac_on_open,             null],
            ["EV_ON_CLOSE",             ac_on_close,            null],
            ["EV_DO_RECOVERY_PASSWORD", ac_do_recovery_password,null],
            ["EV_TIMEOUT",              ac_timeout,             null]
        ]]
    ];

    /*---------------------------------------------*
     *          Events
     *---------------------------------------------*/
    const event_types = [
        ["EV_LOGIN_ACCEPTED",       0],
        ["EV_LOGIN_DENIED",         0],
        ["EV_LOGIN_REFRESHED",      0],
        ["EV_LOGOUT_DONE",          0],
        ["EV_RESIZE",               event_flag_t.EVF_OUTPUT_EVENT|event_flag_t.EVF_NO_WARN_SUBS],
        ["EV_THEME",                event_flag_t.EVF_OUTPUT_EVENT|event_flag_t.EVF_NO_WARN_SUBS],
        ["EV_DO_LOGIN",             0],
        ["EV_DO_LOGOUT",            0],
        ["EV_SELECT_LANGUAGE",      0],
        ["EV_APP_MENU",             event_flag_t.EVF_OUTPUT_EVENT],
        ["EV_HOME",                 0],
        ["EV_SELECT_THEME",         0],
        ["EV_USER_MENU",            0],
        ["EV_ON_OPEN",              0],
        ["EV_ON_CLOSE",             0],
        ["EV_DO_RECOVERY_PASSWORD", 0],
        ["EV_TIMEOUT",              0]
    ];

    __gclass__ = gclass_create(
        gclass_name,
        event_types,
        states,
        gmt,
        0,  // lmt,
        attrs_table,
        PRIVATE_DATA,
        0,  // authz_table,
        0,  // command_table,
        0,  // s_user_trace_level
        0   // gclass_flag
    );
    if(!__gclass__) {
        // Error already logged
        return -1;
    }

    return 0;
}

/***************************************************************
 *          Register GClass
 ***************************************************************/
function register_c_yui_main()
{
    return create_gclass(GCLASS_NAME);
}

export {
    register_c_yui_main,
    display_volatil_modal,
    display_info_message,
    display_warning_message,
    display_error_message,
    get_yesnocancel,
    get_yesno,
    get_ok,
};
