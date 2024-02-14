/***********************************************************************
 *          Ui_header.js
 *
 *          GUI Header
 *
 *          Copyright (c) 2022 Niyamaka.
 *          All Rights Reserved.
 ***********************************************************************/
(function (exports) {
    'use strict';

    /********************************************
     *      Configuration (C attributes)
     ********************************************/
    let CONFIG = {
        layer: null,    // Konva layer

        username: "",   // username with login done
        height: 50,     // height of header
        width: 0,       // width of header, update by parent
        form_padding: 30,
        header_background_color: "#eeeeee",
        header_color: "black",
        fontFamily: "sans-serif", // "DejaVu Sans Mono, monospace", //"sans-serif";
        items_color: "black",   // Text color

        color_yuneta_connected: "#32CD32",      // LimeGreen
        color_yuneta_disconnected: "#FF4500",   // OrangeRed
        // color_user_logout: "#708090",        // SlateGray
        // color_user_login: "#F58E25",         // Carrot Orange
        color_user_login: "#32CD32",            // LimeGreen
        color_user_logout: "#FF4500",           // OrangeRed

        icon_size: 30,  // Wanted size, but change by checking pixels in browser (_icon_size will be used)
        text_size: 18,  // it's different in mobile with text size larger (_text_size will be used)

        draw_dimensions: false, // To debug: draw dimensions

        // Private data
        _gobj_app_menu_popup: null,
        _gobj_app_setup_popup: null,
        _gobj_user_setup_popup: null,
        _gobj_user_menu_popup: null,

        _connex_info: null,
        _connex_info_window: null,

        _icon_size: 0,      // Calculated by checking browser
        _text_size: 0,      // Calculated by checking browser
        _login_form: null,           // html form element
        _wrapper_login_form: null,   // html form element
        _login_form_height: 0,   // to save original form dimension
        _login_form_width: 0,    // to save original form dimension
        _ka_app_menu_button: null,
        _ka_app_name_label: null,
        _ka_user_name_label: null,
        _ka_user_menu_button: null,
        _ka_background_rect: null,
        _ka_container: null
    };




            /***************************
             *      Local Methods
             ***************************/



    /********************************************
     *
     ********************************************/
    function show_login_form(self)
    {
        self.config._wrapper_login_form.style.visibility = "visible";
        self.config._login_form.style.visibility = "visible";
        self.config._wrapper_login_form.focus();
    }

    /********************************************
     *
     ********************************************/
    function hide_login_form(self)
    {
        self.config._wrapper_login_form.style.visibility = "hidden";
        self.config._login_form.style.visibility = "hidden";
        self.config.layer.getStage().getContainer().focus();
    }

    /************************************************
     *
     ************************************************/
    function adjust_text_and_icon_size(self)
    {
        self.private._text_size = adjust_font_size(self.config.text_size, self.config.fontFamily);
        self.private._icon_size = adjust_font_size(self.config.icon_size, self.config.fontFamily);
    }

    /********************************************
     *  WARNING:
     *      z-index 2 and 3 busy by login form
     ********************************************/
    function create_login_form(self, add_forgot_password, add_register)
    {
        let template_forgot_password = sprintf(`
            <div style="margin-top: 15px;">
                <a tabindex="4" id="forgot_password" style="padding:10px 0px 0px 0px;color: blue; cursor:pointer;">%s</a>
            </div>
        `, t("forgot password?"));

        let template_register = sprintf(`
            <div style="margin-top: 15px;">
                <span >%s<a tabindex="5" id="register" style="padding:10px 0px 0px 10px;color: blue; cursor:pointer;">%s</a></span>
            </div>
        `, t("not a member?"), t("register"));

        let height = 240;
        let width = 300;
        let padding = self.config.form_padding;
        if(add_forgot_password) {
            height += self.private._text_size + 10;
        }
        if(add_register) {
            height += self.private._text_size + 10;
        }

        let template_data = {
            fontfamily: self.config.fontFamily,
            fontsize: self.private._text_size-2,
            form_height: height,
            form_width: width,
            form_top: 60,
            form_left: (window.innerWidth - width)/2 - padding,
            padding: padding,
            login_size: self.private._text_size + self.private._text_size/4,
            background_opacity: 0.1,
            border_opacity: 0.125,
            user: t("user"),
            password: t("password"),
            login: t("login"),
            forgot_password: add_forgot_password?template_forgot_password:"",
            template_register: add_register?template_register:""
        };

        let login_template = sprintf(`
            <div tabindex="1" id="_wrapper_login_form" style="overflow: hidden;color: black; z-index: 2; position: absolute; left: 0px; top: 0px; width: 100%%; height: 100%%;visibility: hidden; background-color: rgba(0,0,0,%(background_opacity)f);">

            <span style="visibility: hidden;font-family: 'FontAwesome', 'Font Awesome 6 Brands Regular', 'Font Awesome 6 Free Regular', 'Font Awesome 6 Free Solid', sans-serif;">\u{f039} \u{f2b9} \u{e080}</span> <!-- hack to quick load of fontawesome? --> 
            <form action="" method="dialog" id="_login_form" style="overflow: hidden;background: rgb(238, 238, 238);color: black;font-family: %(fontfamily)s !important; font-size: %(fontsize)dpx; padding: %(padding)dpx; border-radius: .50rem; border: 1px solid rgba(0,0,0,%(border_opacity)f); z-index: 3; position: absolute; left: %(form_left)dpx; top: %(form_top)dpx; width: %(form_width)dpx; height: %(form_height)dpx;visibility: hidden;">
                <div>
                    <div>
                        <label style="">%(user)s</label>
                    </div>
                    <div style="padding-top: 5px; width: 100%%;">
                        <input tabindex="2" name="username" style="width: 100%%;box-sizing: border-box;outline: none;height: 40px;line-height: 40px;border-radius: 4px;border: 1px solid #c2c0ca;color: black;background: white;padding: 0 0 0 10px;" type="email" placeholder="" required autofocus autocomplete="username"/>
                    </div>
                </div>
                <div style="margin-top: 20px;">
                    <div style="width: 100%%;">
                        <label style="width:100%%;">%(password)s</label>
                    </div>
                    <div style="padding-top: 5px;">
                        <input tabindex="2" name="password"  style="width: 100%%;box-sizing: border-box;outline: none;height: 40px;line-height: 40px;border-radius: 4px;border: 1px solid #c2c0ca;color: black;background: white;padding: 0 0 0 10px;" type="password" placeholder="" required autocomplete="current-password"/>
                    </div>
                </div>
                <div style="margin-top: 30px;">
                    <button tabindex="3" name="accept" style="border: none;border-radius: 4px;width: 100%%;background-color: #0088ce;color: white;font-weight: bold;cursor: pointer;text-align: center;font-size: %(login_size)dpx;height: 50px;line-height: 50px;vertical-align:top;padding:0;" type="submit">%(login)s</button>
                </div>
                %(forgot_password)s
                %(template_register)s
            </form>
            </div>
        `, template_data
        );

        const dialog = htmlToElement(login_template);
        document.body.appendChild(dialog);
        let _login_form = self.config._login_form = document.getElementById("_login_form");
        self.config._login_form_height = height;
        self.config._login_form_width = parseInt(_login_form.style.width);
        let _wrapper = self.config._wrapper_login_form = document.getElementById("_wrapper_login_form");
        const forgot_password_field = document.getElementById("forgot_password");
        const register_field = document.getElementById("register");

        _login_form.onsubmit = function(ev) {
            // hide_login_form(self);

            let elements = _login_form.elements;
            let kw_login = {};
            for(let i = 0 ; i < elements.length ; i++){
                let item = elements.item(i);
                if(!empty_string(item.name)) {
                    kw_login[item.name] = item.value;
                }
            }

            try {
                self.gobj_send_event("EV_DO_LOGIN", kw_login, self);
            } catch(error) {
            }

            ev.preventDefault();
            return false;
        };

        _wrapper.addEventListener("focus", function (e) {
            if(self.is_tracing()) {
                log_warning("====> _wrapper_login_form.onfocus");
            }
            if(!_wrapper.contains(document.activeElement) || _wrapper === document.activeElement) {
                if(empty_string(_login_form.elements["username"].value)) {
                    _login_form.elements["username"].focus();
                } else if(empty_string(_login_form.elements["password"].value)) {
                    _login_form.elements["password"].focus();
                } else {
                    _login_form.elements["accept"].focus();
                }
            }
        });

        if(forgot_password_field) {
            forgot_password_field.onclick = function(ev) {
                let elements = _login_form.elements;
                let kw_login = {};
                for(let i = 0 ; i < elements.length ; i++){
                    let item = elements.item(i);
                    if(!empty_string(item.name)) {
                        kw_login[item.name] = item.value;
                    }
                }

                try {
                    self.gobj_send_event("EV_FORGOT_PASSWORD", kw_login, self);
                } catch(error) {
                }
                ev.preventDefault();
                return false;
            };
        }
        if(register_field) {
            register_field.onclick = function(ev) {
                let elements = _login_form.elements;
                let kw_login = {};
                for(let i = 0 ; i < elements.length ; i++){
                    let item = elements.item(i);
                    if(!empty_string(item.name)) {
                        kw_login[item.name] = item.value;
                    }
                }
                try {
                    self.gobj_send_event("EV_REGISTER_USER", kw_login, self);
                } catch(error) {
                }
                ev.preventDefault();
                return false;
            };
        }
    }

    /********************************************
     *  TODO crea el form login con este estilo
     *  o directamente con w2ui
     ********************************************/
    function __create_login_form(self, add_forgot_password, add_register)
    {
        let left = 100;
        let top = 100;
        let width = 600;
        let height = 500;

        let styles = `
            position: absolute;
            left: ${left}px;
            top: ${top}px;
            width: ${parseInt(width)}px;
            height: ${parseInt(height)}px;
        `;
        let msg = `<div id="toolbar" style="${w2utils.stripSpaces(styles)}"></div>`;
        query('body').append(msg);

        let tb = new w2toolbar({
            box: '#toolbar',
            name: 'toolbar',
            items: [
                { type: 'button', id: 'item1', text: 'Button', icon: 'w2ui-icon-colors' },
                { type: 'break' },
                { type: 'check', id: 'item2', text: 'Check 1', icon: 'w2ui-icon-check' },
                { type: 'check', id: 'item3', text: 'Check 2', icon: 'w2ui-icon-check' },
                { type: 'break' },
                { type: 'radio', id: 'item4', group: '1', text: 'Radio 1', icon: 'w2ui-icon-info', checked: true },
                { type: 'radio', id: 'item5', group: '1', text: 'Radio 2', icon: 'w2ui-icon-paste' },
                { type: 'spacer' },
                { type: 'button', id: 'item6', text: 'Button', icon: 'w2ui-icon-cross' }
            ],
            onClick(event) {
                console.log('Target: '+ event.target, event);
                setTimeout(function() {
                    tb.destroy();
                    // or w2ui.toolbar.destroy();
                    query('#toolbar').remove();

                }, 1);

            }
        });
    }

    function __todo_crea_contenido_en_popup()
    {
        // widget configuration
        let config = {
            layout: {
                name: 'layout',
                padding: 4,
                panels: [
                    { type: 'left', size: '50%', resizable: true, minSize: 300 },
                    { type: 'main', minSize: 300, style: 'overflow: hidden' }
                ]
            },
            grid: {
                name: 'grid',
                style: 'border: 1px solid #efefef',
                columns: [
                    { field: 'fname', text: 'First Name', size: '33%', sortable: true, searchable: true },
                    { field: 'lname', text: 'Last Name', size: '33%', sortable: true, searchable: true },
                    { field: 'email', text: 'Email', size: '33%' },
                    { field: 'sdate', text: 'Start Date', size: '120px', render: 'date' }
                ],
                records: [
                    { recid: 1, fname: 'John', lname: 'Doe', email: 'jdoe@gmail.com', sdate: '4/3/2012' },
                    { recid: 2, fname: 'Stuart', lname: 'Motzart', email: 'jdoe@gmail.com', sdate: '4/3/2012' },
                    { recid: 3, fname: 'Jin', lname: 'Franson', email: 'jdoe@gmail.com', sdate: '4/3/2012' },
                    { recid: 4, fname: 'Susan', lname: 'Ottie', email: 'jdoe@gmail.com', sdate: '4/3/2012' },
                    { recid: 5, fname: 'Kelly', lname: 'Silver', email: 'jdoe@gmail.com', sdate: '4/3/2012' },
                    { recid: 6, fname: 'Francis', lname: 'Gatos', email: 'jdoe@gmail.com', sdate: '4/3/2012' },
                    { recid: 7, fname: 'Mark', lname: 'Welldo', email: 'jdoe@gmail.com', sdate: '4/3/2012' },
                    { recid: 8, fname: 'Thomas', lname: 'Bahh', email: 'jdoe@gmail.com', sdate: '4/3/2012' },
                    { recid: 9, fname: 'Sergei', lname: 'Rachmaninov', email: 'jdoe@gmail.com', sdate: '4/3/2012' }
                ],
                async onClick(event) {
                    await event.complete; // needs to wait for evnet complete cycle, so selection is right
                    let sel = grid.getSelection();
                    if (sel.length == 1) {
                        form.recid  = sel[0];
                        form.record = w2utils.clone(grid.get(sel[0]));
                        form.refresh();
                    } else {
                        form.clear();
                    }
                }
            },
            form: {
                header: 'Edit Record',
                name: 'form',
                style: 'border: 1px solid #efefef',
                fields: [
                    { field: 'recid', type: 'text', html: { label: 'ID', attr: 'size="10" readonly' } },
                    { field: 'fname', type: 'text', required: true, html: { label: 'First Name', attr: 'size="40" maxlength="40"' } },
                    { field: 'lname', type: 'text', required: true, html: { label: 'Last Name', attr: 'size="40" maxlength="40"' } },
                    { field: 'email', type: 'email', html: { label: 'Email', attr: 'size="30"' } },
                    { field: 'sdate', type: 'date', html: { label: 'Date', attr: 'size="10"' } }
                ],
                actions: {
                    Reset() {
                        this.clear();
                    },
                    Save() {
                        let errors = this.validate();
                        if (errors.length > 0) return;
                        if (this.recid == 0) {
                            grid.add(w2utils.extend({ recid: grid.records.length + 1 }, this.record));
                            grid.selectNone();
                            this.clear();
                        } else {
                            grid.set(this.recid, this.record);
                            grid.selectNone();
                            this.clear();
                        }
                    }
                }
            }
        };

        // initialization in memory
        let layout = new w2layout(config.layout);
        let grid = new w2grid(config.grid);
        let form = new w2form(config.form);

        window.openPopup = function() {
            w2popup.open({
                title: 'Popup',
                width: 900,
                height: 600,
                showMax: true,
                body: '<div id="main" style="position: absolute; left: 2px; right: 2px; top: 0px; bottom: 3px;"></div>'
            })
            .then(e => {
                layout.render('#w2ui-popup #main');
                layout.html('left', grid);
                layout.html('main', form);
            });
        };
    }

    /********************************************
     *
     ********************************************/
    function create_label(self, ka_parent, event, kw)
    {
        let stage = self.config.layer.getStage();

        let kw_element = { // Common fields
            id: event,
            fill: self.config.items_color,
            opacity: 1,
            fontFamily: self.config.fontFamily,
            height: self.config.height,
            listening: true
        };
        json_object_update(kw_element, kw);

        let element = new Konva.Text(kw_element);

        if(!empty_string(event)) {
            //element.filters([Konva.Filters.RGBA]);

            element.on("mouseenter", function (e) {
                stage.container().style.cursor = "pointer";
            });

            element.on("mouseleave", function (e) {
                element.opacity(1);
                stage.container().style.cursor = "default";
            });

            element.on("pointerdown", function (e) {
                if(self.is_tracing()) {
                    log_warning(sprintf("%s.%s ==> (%s), cancelBubble: %s, gobj: %s, ka_id: %s, ka_name: %s",
                        "Ui_header", "element",
                        e.type,
                        (e.cancelBubble)?"Y":"N",
                        self.gobj_short_name(),
                        kw_get_str(e.target.attrs, "id", ""),
                        kw_get_str(e.target.attrs, "name", "")
                    ));
                }
                element.opacity(0.5);
            });
            element.on("pointerup", function (e) {
                e.cancelBubble = true;
                e.gobj = self;
                if(self.is_tracing()) {
                    log_warning(sprintf("%s.%s ==> (%s), cancelBubble: %s, gobj: %s, ka_id: %s, ka_name: %s",
                        "Ui_header", "element",
                        e.type,
                        (e.cancelBubble)?"Y":"N",
                        self.gobj_short_name(),
                        kw_get_str(e.target.attrs, "id", ""),
                        kw_get_str(e.target.attrs, "name", "")
                    ));
                }
                element.opacity(1);
                /*
                 *  WARNING If action provoke deleting the konva item then the event is not bubbled!
                 *  Don't worry, if the konva item is closed, and the event don't arrive to stage listener,
                 *  the window will send a EV_DEACTIVATE and the window will be deactivated,
                 *  so for the activation service will work well.
                 *  BE CAREFUL with service needing bubbling.
                 */
                self.gobj_send_event(event, {element: element}, self);
            });
        }

        ka_parent.add(element);

        return element;
    }

    /********************************************
     *
     ********************************************/
    function update_positions(self)
    {
        let parte = self.config.width
            - self.config._ka_app_menu_button.width()
            - self.config._ka_user_menu_button.width()
            - 20;
        parte = Math.floor(parte/2);
        self.config._ka_app_name_label.width(parte);
        self.config._ka_user_name_label.width(parte);

        let pos = {
            x: 10,
            y: 0

        };
        self.config._ka_app_menu_button.position(pos);

        pos.x += self.config._ka_app_menu_button.width();

        self.config._ka_app_name_label.position(pos);

        pos.x = self.config.width;
        pos.x -= 10;
        pos.x -= self.config._ka_user_menu_button.width();
        self.config._ka_user_menu_button.position(pos);

        pos.x -= self.config._ka_user_name_label.width();
        self.config._ka_user_name_label.position(pos);

        //self.private._ka_container.cache();
    }

    /********************************************
     *
     ********************************************/
    function create_header(self)
    {
        let layer = self.config.layer;
        let width = self.config.width;

        /*
         *  Container (Group)
         */
        let ka_container = self.private._ka_container = new Konva.Group({
            id: self.gobj_short_name(),
            name: "ka_container",
            x: 0,
            y: 0,
            listening: true
        });
        layer.add(ka_container);
        ka_container.gobj = self; // cross-link

        /*
         *  Background
         */
        self.config._ka_background_rect = new Konva.Rect({
            x: 0,
            y: 0,
            width: width,
            height: self.config.height,
            // stroke: 'red',
            // strokeWidth: 1,
            fill: self.config.header_background_color,
            shadowBlur: 10,
            listening: false
        });
        ka_container.add(self.config._ka_background_rect);

        /*
         *  App menu
         */
        self.config._ka_app_menu_button = create_label(self,
            ka_container,
            "EV_APP_MENU",
            {
                text: icono("yuneta"),
                fontSize: self.private._icon_size,
                fontFamily: "yuneta-icon-font",
                fontStyle: "normal",
                width: 40,
                height: self.config.height,
                align: "left",
                verticalAlign: "middle",
                fill: self.config.color_yuneta_disconnected
            }
        );

        /*
         *  Label app name
         */
        self.config._ka_app_name_label = create_label(self,
            ka_container,
            "EV_APP_SETUP",
            {
                text: self.yuno.yuno_name + " " + self.yuno.yuno_version,
                fontSize: self.private._text_size,
                fontStyle: "normal",
                width: 150,
                height: self.config.height,
                align: "left",
                verticalAlign: "middle"
            }
        );

        /*
         *  User name
         */
        self.config._ka_user_name_label = create_label(self,
            ka_container,
            "EV_USER_MENU",
            {
                text: "Login",
                fontSize: self.private._text_size,
                fontStyle: "normal",
                width: 150,
                align: "right",
                verticalAlign: "middle"
            }
        );

        /*
         *  User menu
         */
        self.config._ka_user_menu_button = create_label(self,
            ka_container,
            "EV_USER_MENU",
            {
                text: icono("user"),
                fontSize: self.private._icon_size,
                fontFamily: "yuneta-icon-font",
                fontStyle: "normal",
                width: 40,
                align: "right",
                verticalAlign: "middle",
                fill: self.config.color_user_logout
            }
        );

        /*
         *  Bottom
         */

    }

    /********************************************
     *
     ********************************************/
    function create_menus_popups(self)
    {
        self.private._gobj_user_menu_popup = self.yuno.gobj_create(
            'user_menu',
            Sw_menu,
            {
                layer: self.config.layer,

                modal: false,       // Activation SERVICE: (webix) Outside disabled but Esc or clicking out will close
                super_modal: false, // Activation SERVICE: Outside disabled and only inside action will close
                autoclose: true,    // Activation SERVICE: Close window on pointerup bubbling to stage or Esc key

                x: 200,
                y: 200,
                width: 200,
                height: 200,
                visible: false,
                draggable: false,
                autosize: true,
                fix_dimension_to_screen: true,

                kw_text: {
                    fontFamily: self.config.fontFamily,
                    fontSize: self.private._text_size
                },
                items: [
                    {
                        id: "language",
                        label: t("language"),
                        icon: "far fa-globe",
                        action: "EV_SELECT_LANGUAGE"
                    },
                    {
                        id: "logout",
                        disabled: true,
                        label: t("logout"),
                        icon: "fas fa-user-alt-slash",
                        action: "EV_DO_LOGOUT"
                    }
                ]
            },
            self
        );
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
    function ac_do_login(self, event, kw, src)
    {
        let __login__ = self.yuno.gobj_find_service("__login__", true);
        __login__.gobj_send_event("EV_DO_LOGIN", kw, self);

        return 0;
    }

    /************************************************
     *
     ************************************************/
    function ac_do_logout(self, event, kw, src)
    {
        let __login__ = self.yuno.gobj_find_service("__login__", true);
        __login__.gobj_send_event("EV_DO_LOGOUT", kw, self);
        return 0;
    }

    /************************************************
     *  {
     *      username:
     *      jwt:
     *  }
     ************************************************/
    function ac_login_accepted(self, event, kw, src)
    {
        hide_login_form(self);

        self.config._ka_user_menu_button.fill(self.config.color_user_login);
        self.config.username = kw.username;
        self.config._ka_user_name_label.setText(kw.username);
        self.private._gobj_user_menu_popup.gobj_send_event("EV_ENABLE_ITEMS", ["logout"], self);
        self.config.layer.draw();

        return 0;
    }

    /************************************************
     *
     ************************************************/
    function ac_login_refreshed(self, event, kw, src)
    {
        return 0;
    }

    /************************************************
     *
     ************************************************/
    function ac_login_denied(self, event, kw, src)
    {
        hide_login_form(self);

        self.config._ka_user_menu_button.fill(self.config.color_user_logout);
        self.config.username = "";
        self.config._ka_user_name_label.setText("Login");
        self.private._gobj_user_menu_popup.gobj_send_event("EV_DISABLE_ITEMS", ["logout"], self);
        self.config.layer.draw();

        let error = kw_get_str(kw, "error", "login denied", false, false);
        display_error_message(
            t(error),
            "Error",
            function() {
                if(empty_string(self.config.username)) {
                    show_login_form(self);
                }
            }
        );

        return 0;
    }

    /************************************************
     *
     ************************************************/
    function ac_logout_done(self, event, kw, src)
    {
        self.config._ka_user_menu_button.fill(self.config.color_user_logout);
        self.config.username = "";
        self.config._ka_user_name_label.setText("Login");
        self.private._gobj_user_menu_popup.gobj_send_event("EV_DISABLE_ITEMS", ["logout"], self);
        self.config.layer.draw();

        show_login_form(self);

        return 0;
    }

    /************************************************
     *
     ************************************************/
    function ac_select_language(self, event, kw, src)
     {
        // src.gobj_send_event("EV_HIDE", {}, self); // TODO
        return 0;
    }

    /************************************************
     *
     ************************************************/
    function ac_app_menu(self, event, kw, src)
    {
        let element = kw.element; // button clicked

        let position = {
            x: element.absolutePosition().x + 0, //element.width()/4,
            y: element.absolutePosition().y + element.height(),
        };

        if(self.private._connex_info_window) {
            self.private._connex_info_window.destroy();
            self.private._connex_info_window = null;
        } else {
            let editor = null;
            self.private._connex_info_window = new w2window({
                name: "connection_info",
                title: t("connection info"),
                x: 40,
                y: 2,
                width: 400,
                height: 350,
                onResized(ev) {
                    editor && editor.setSize && editor.setSize(
                        ev.detail.body_rect.width, ev.detail.body_rect.height
                    );
                },
                onClose(ev) {
                    editor && editor.destroy && editor.destroy();
                    self.private._connex_info_window = null;
                }
            });
            let target = self.private._connex_info_window.get_container();

            if(target.length > 0) {
                let props = {
                    content: {
                        json: self.private._connex_info
                    },
                    readOnly: true,
                    navigationBar: false
                };
                editor = new JSONEditor({
                    target: target[0],
                    props: props
                });
                target.addClass("jse-theme-dark");

                let font_family = "DejaVu Sans Mono, monospace";
                let sz = adjust_font_size(16, font_family);
                const root = document.querySelector(':root');

                root.style.setProperty('--jse-font-size-mono', sz + 'px');
                root.style.setProperty('--jse-font-family-mono', font_family);

                query(self.private._connex_info_window.box).find(".w2ui-window-title").css({
                    'font-size': self.private._text_size + 'px'
                });
                query(self.private._connex_info_window.box).find("span.w2ui-xicon").css({
                    'font-size': sz + 'px'
                });
            }
        }

        if(self.private._gobj_app_menu_popup) {
            self.private._gobj_app_menu_popup.gobj_send_event("EV_TOGGLE", position, self);
        }

        return 0;
    }

    /************************************************
     *
     ************************************************/
    function ac_app_setup(self, event, kw, src)
    {
        if(self.private._gobj_app_setup_popup) {
            self.private._gobj_app_setup_popup.gobj_send_event("EV_TOGGLE", {}, self);
        }


        return 0;
    }

    /************************************************
     *
     ************************************************/
    function ac_user_menu(self, event, kw, src)
    {
        if(empty_string(self.config.username)) {
            show_login_form(self);
        } else {
            let element = kw.element; // button clicked

            let position = {
                x: element.absolutePosition().x + element.width(),
                y: element.absolutePosition().y + element.height(),
            };
            if(self.private._gobj_user_menu_popup) {
                self.private._gobj_user_menu_popup.gobj_send_event("EV_TOGGLE", position, self);
            }
        }

        return 0;
    }

    /********************************************
     *  Connected to yuneta
     ********************************************/
    function ac_info_connected(self, event, kw, src)
    {
        self.private._connex_info = kw;
        self.config._ka_app_menu_button.fill(self.config.color_yuneta_connected);
        self.config.layer.draw();
    }

    /********************************************
     *  Disconnected from yuneta
     ********************************************/
    function ac_info_disconnected(self, event, kw, src)
    {
        self.config._ka_app_menu_button.fill(self.config.color_yuneta_disconnected);
        self.config.layer.draw();
    }

    /************************************************
     *
     ************************************************/
    function ac_timeout(self, event, kw, src)
    {
        // self.gobj_send_event("EV_USER_MENU", {element: self.config._ka_user_menu_button}, self);
        // self.set_timeout(100);

        if(empty_string(self.config.username)) {
            show_login_form(self);
        }
        return 0;
    }

    /************************************************
     *  stage.draw() is done automatically
     ************************************************/
    function ac_resize(self, event, kw, src)
    {
        self.config.width = kw.width;
        // self.config._wrapper_login_form.style.width = kw.width + "px";
        // self.config._wrapper_login_form.style.height = kw.height + "px";

        /*
         *  Resize container
         */
        let ka_container = self.private._ka_container;
        ka_container.size({
            width: kw.width,
            height: self.config.height
        });

        /*
         *  Resize background rect
         */
        let _ka_background_rect = self.config._ka_background_rect;
        _ka_background_rect.size({
            width: kw.width,
            height: self.config.height
        });

        /*
         *  Resize Login Form
         */
        let _login_form = self.config._login_form;
        let rec = _login_form.getBoundingClientRect();
        if(window.innerHeight < rec.height) {
            _login_form.style.top = 0;
            _login_form.style.height = window.innerHeight - self.config.form_padding*2 + "px";
        } else {
            _login_form.style.top = 60 + "px";
            _login_form.style.height = self.config._login_form_height + "px";
        }

        if(window.innerWidth < rec.width) {
            _login_form.style.left = 0;
            _login_form.style.width = window.innerWidth - self.config.form_padding*2 + "px";
        } else {
            _login_form.style.left = (window.innerWidth - rec.width)/2 + "px";
            _login_form.style.width = self.config._login_form_width + "px";
        }

        /*
         *  Align to left/right the labels
         */
        update_positions(self);

        /*
         *  Resize dimensions rect (to debug)
         */
        if(self.config.draw_dimensions) {
            let elements = ka_container.getChildren();
            elements.forEach(function(element) {
                if(element.background_rect) {
                    let dim = element.getClientRect({relativeTo:element.getParent()});
                    element.background_rect.position(dim);
                    element.background_rect.size(dim);
                }
            });
        }

        /*
         *  Re-position popup menus
         */
        let element = self.config._ka_app_menu_button;
        let position = {
            x: element.absolutePosition().x + element.width()/2,
            y: element.absolutePosition().y + element.height(),
        };
        if(self.private._gobj_app_menu_popup) {
            self.private._gobj_app_menu_popup.gobj_send_event("EV_RESIZE", position, self);
        }

        element = self.config._ka_user_menu_button;
        position = {
            x: element.absolutePosition().x + element.width()/2,
            y: element.absolutePosition().y + element.height(),
        };
        if(self.private._gobj_user_menu_popup) {
            self.private._gobj_user_menu_popup.gobj_send_event("EV_RESIZE", position, self);
        }

        return 0;
    }




            /***************************
             *      GClass/Machine
             ***************************/




    let FSM = {
        "event_list": [
            "EV_LOGIN_ACCEPTED: input",
            "EV_LOGIN_DENIED: input",
            "EV_LOGIN_REFRESHED: input",
            "EV_LOGOUT_DONE: input",
            "EV_DO_LOGIN",
            "EV_DO_LOGOUT",
            "EV_SELECT_LANGUAGE",
            "EV_APP_MENU",
            "EV_APP_SETUP",
            "EV_USER_MENU",
            "EV_USER_SETUP",
            "EV_INFO_CONNECTED",
            "EV_INFO_DISCONNECTED",
            "EV_TIMEOUT",
            "EV_SHOWED",    // debugging
            "EV_HIDDEN",    // debugging
            "EV_RESIZE"
        ],
        "state_list": [
            "ST_IDLE"
        ],
        "machine": {
            "ST_IDLE":
            [
                ["EV_LOGIN_ACCEPTED",       ac_login_accepted,      undefined],
                ["EV_LOGIN_DENIED",         ac_login_denied,        undefined],
                ["EV_LOGIN_REFRESHED",      ac_login_refreshed,     undefined],
                ["EV_LOGOUT_DONE",          ac_logout_done,         undefined],
                ["EV_DO_LOGIN",             ac_do_login,            undefined],
                ["EV_DO_LOGOUT",            ac_do_logout,           undefined],
                ["EV_SELECT_LANGUAGE",      ac_select_language,     undefined],
                ["EV_APP_MENU",             ac_app_menu,            undefined],
                ["EV_APP_SETUP",            ac_app_setup,           undefined],
                ["EV_USER_MENU",            ac_user_menu,           undefined],
                ["EV_USER_SETUP",           ac_user_menu,           undefined], // No user_setup by now
                ["EV_INFO_CONNECTED",       ac_info_connected,      undefined],
                ["EV_INFO_DISCONNECTED",    ac_info_disconnected,   undefined],
                ["EV_TIMEOUT",              ac_timeout,             undefined],
                ["EV_SHOWED",               undefined,              undefined],
                ["EV_HIDDEN",               undefined,              undefined],
                ["EV_RESIZE",               ac_resize,              undefined]
            ]
        }
    };

    let Ui_header = GObj.__makeSubclass__();
    let proto = Ui_header.prototype; // Easy access to the prototype
    proto.__init__= function(name, kw) {
        GObj.prototype.__init__.call(
            this,
            FSM,
            CONFIG,
            name,
            "Ui_header",
            kw,
            0
        );
        return this;
    };
    gobj_register_gclass(Ui_header, "Ui_header");




            /***************************
             *      Framework Methods
             ***************************/




    /************************************************
     *      Framework Method create
     ************************************************/
    proto.mt_create = function(kw)
    {
        let self = this;

        if(!self.config.layer) {
            log_error("What layer?"); // TODO refactor with a toolbar?
        }

        adjust_text_and_icon_size(self);

        create_header(self);
        create_login_form(self, false, false);
        create_menus_popups(self);
        update_positions(self);

        let __login__ = self.yuno.gobj_find_service("__login__", true);
        __login__.gobj_subscribe_event(null, {}, self);
    };

    /************************************************
     *      Framework Method destroy
     *      In this point, all childs
     *      and subscriptions are already deleted.
     ************************************************/
    proto.mt_destroy = function()
    {
    };

    /************************************************
     *      Framework Method start
     ************************************************/
    proto.mt_start = function(kw)
    {
        let self = this;
        self.set_timeout(500);

        if(self.config.draw_dimensions) {
            let elements = self.private._ka_container.getChildren();
            elements.forEach(function(element) {
                let dim = element.getClientRect({relativeTo:element.getParent()});
                let rect = element.background_rect = new Konva.Rect({
                    draggable: true,
                    name: "xxx",
                    x: dim.x,
                    y: dim.y,
                    width: dim.width,
                    height: dim.height,
                    stroke: "red",
                    strokeWidth: 1,
                    fill: undefined
                });
                self.private._ka_container.add(rect);
            });
        }

        if(0) { // DEBUGGING check background colors: opacity and shadowColor affects.
            var rect = new Konva.Rect({
                draggable: true,
                name: "xxx",
                x: 800,
                y: 100,
                width: 200,
                height: 200,
                stroke: "red",
                strokeWidth: 20,
                fill: "white",
                opacity: 0.5,
                shadowBlur: 20,
                shadowColor: "gray"
            });
            self.config.layer.add(rect);

            let rect2 = new Konva.Rect({
                draggable: true,
                name: "xxx",
                x: 119,
                y: 119,
                width: 222,
                height: 222,
                stroke: "red",
                strokeWidth: 8,
                fill: "white",
                opacity: 0.5, // HACK with opacity the shadowColor is used!!!
                shadowBlur: 5,
                shadowColor: "green",
            });
            self.config.layer.add(rect2);
            self.config.layer.draw();
        }
    };

    /************************************************
     *      Framework Method stop
     ************************************************/
    proto.mt_stop = function(kw)
    {
        let self = this;
    };

    /************************************************
     *      Local Method
     ************************************************/
    proto.get_konva_container = function()
    {
        let self = this;
        return self.private._ka_container;
    };


    //=======================================================================
    //      Expose the class via the global object
    //=======================================================================
    exports.Ui_header = Ui_header;

})(this);
