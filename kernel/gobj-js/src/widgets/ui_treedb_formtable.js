/***********************************************************************
 *          ui_treedb_formtable.js
 *
 *          Mix "Container Panel" & "Pinhold Window"
 *
 *          Manage table and form
 *
 *  Version
 *  -------
 *  1.0     Initial release
 *  1.1     Convert to Mix Panel/Window
 *  1.2     Public name of datable in self.config.webix_datatable_id
 *  1.3     Add user_data attribute
 *          Set initial mode
 *  1.4     Paging optional
 *  1.5     Select row only if it has update/create mode
 *
 *          Copyright (c) 2020-2021 Niyamaka.
 *          All Rights Reserved.
 ***********************************************************************/
(function (exports) {
    "use strict";

    /********************************************
     *      Configuration (C attributes)
     ********************************************/
    var CONFIG = {
        //////////////// Common Attributes //////////////////
        is_pinhold_window:true, // CONF: Select default: window or container panel
        panel_properties: {},   // CONF: creator can set "Container Panel" properties
        window_properties: {},  // CONF: creator can set "Pinhold Window" properties
        ui_properties: null,    // CONF: creator can set webix properties
        window_image: "",       // CONF: Used by pinhold_window_top_toolbar "Pinhold Window"
        window_title: "",       // CONF: Used by pinhold_window_top_toolbar "Pinhold Window"
        left: 0,                // CONF: Used by pinhold_window_top_toolbar "Pinhold Window"
        top: 0,                 // CONF: Used by pinhold_window_top_toolbar "Pinhold Window"
        width: 600,             // CONF: Used by pinhold_window_top_toolbar "Pinhold Window"
        height: 500,            // CONF: Used by pinhold_window_top_toolbar "Pinhold Window"

        $ui: null,
        subscriber: null,       // Subscriber of published events, by default the parent.
        $ui_fullscreen: null,   // Which part of window will be fullscreened "Pinhold Window"
        resizing_event_id: null,// Used for automatic_resizing by window
        pinpushed: false,       // Used by pinhold_window_top_toolbar "Pinhold Window"

        //////////////// Particular Attributes //////////////////
        user_data: null,
        treedb_name: null,
        topic_name: null,
        schema: null,   // TODO change all users creating this to use cols, then remove schema
        cols: null,
        form_label_width: 140,
        webix_datatable_id: null,   // webix public id of datatable
        list_mode_enabled: true,
        update_mode_enabled: false,
        create_mode_enabled: false,
        delete_mode_enabled: false,
        current_mode: null,
        fields_enabled: null,
        hide_private_fields: false,
        with_drag: false,
        with_checkbox: false,
        with_radio: false,
        with_webix_id: false,   // Set true when your id is repeated
        with_textfilter: false,
        with_sort: false,
        with_navigation_toolbar: false,
        with_paging: false,
        with_tooltip: true,
        with_footer: true,
        with_select: true,
        with_multiselect: false,
        with_table_editable: false, // FUTURE de momento datatable no editable (solo en form)
        with_keyboard_navigation: true,
        with_resizeColumn: true,
        with_resizeRow: false,
        with_fixedRowHeight: true,
        with_refresh: true,
        with_json_viewers: true,
        with_trash_button: true,
        with_clone_button: true,
        with_rowid_field: false,
        publish_row_selected: false,

        clone_record: null,

        _writable_fields: null, // automatic built

        current_id: null,
        last_selected_id: null,
        page_size: 0,
        page: 0,            // current page
        record_idx: 0,      // current idx of selected record
        total: 0,           // total of record in datatable


        //////////////////////////////////
        __writable_attrs__: [
            ////// Common /////
            "window_title",
            "window_image",
            "left",
            "top",
            "width",
            "height",

            ////// Particular /////
            "page_size"
        ]
    };




            /***************************
             *      Local Methods
             ***************************/




    /************************************************************
     *   Rebuild
     ************************************************************/
    function rebuild(self)
    {
        if(self.config.$ui) {
            self.config.$ui.destructor();
            self.config.$ui = 0;
        }
        build_webix(self);
        build_table(self);
    }

    /************************************************************
     *   Webix UI
     ************************************************************/
    function build_webix(self)
    {
        /*
         *  Set initial mode
         */
        if(!self.config.current_mode) {
            if(self.config.list_mode_enabled) {
                self.config.current_mode = "list";
            } else if(self.config.update_mode_enabled) {
                self.config.current_mode = "update";
            } else if(self.config.create_mode_enabled) {
                self.config.current_mode = "create";
            } else {
                log_error("No mode in " + self.gobj_short_name());
                self.config.current_mode = "list";
            }
        }

        /*---------------------------------------*
         *      Particular UI code
         *---------------------------------------*/
        var segmented_tooltip = "";

        if(self.config.list_mode_enabled) {
            if(segmented_tooltip) {
                segmented_tooltip += " | ";
            }
            segmented_tooltip += t("list");
        }
        if(self.config.update_mode_enabled) {
            if(segmented_tooltip) {
                segmented_tooltip += " | ";
            }
            segmented_tooltip += t("update");
        }
        if(self.config.create_mode_enabled) {
            if(segmented_tooltip) {
                segmented_tooltip += " | ";
            }
            segmented_tooltip += t("create");
        }

        var segmented_size = 60;
        var show_segmented = false;
        var segmented_options = [];

        if(self.config.list_mode_enabled) {
            show_segmented = true;
            segmented_size += 60;
            segmented_options.push(
                {
                    id: "list",
                    value: "<span style='font-size:22px;' class='fal fa-table'>&nbsp;</span>"
                }
            );
        }
        if(self.config.update_mode_enabled) {
            show_segmented = true;
            segmented_size += 60;
            segmented_options.push(
                {
                    id: "update",
                    value: "<span style='font-size:22px;' class='far fa-money-check'>&nbsp;</span>"
                }
            );
        }
        if(self.config.create_mode_enabled) {
            show_segmented = true;
            segmented_size += 60;
            segmented_options.push(
                {
                    id: "create",
                    value: "<span style='font-size:22px;' class='far fa-rectangle-landscape'>&nbsp;</span>"
                }
            );
        }

        var navigation_toolbar = {
            view: "scrollview",
            height: 50,
            scroll: "x",
            hidden: self.config.with_navigation_toolbar?false:true,
            body: {
                view: "toolbar",
                css: "toolbar2color",
                elementsConfig: {
                    height: 40
                },
                cols: [
                    {
                        view: "segmented",
                        id: build_name(self, "segmented"),
                        width: segmented_size,
                        value: self.config.current_mode,
                        hidden: show_segmented?false:true,
                        tooltip: segmented_tooltip,
                        options: segmented_options,
                        click: function(id, ev) {
                            var view_mode = this.getValue();
                            if(view_mode == "list") {
                                self.gobj_send_event("EV_LIST_MODE", {}, self);
                            } else if(view_mode == "update") {
                                self.gobj_send_event("EV_UPDATE_MODE", {}, self);
                            } else if(view_mode == "create") {
                                self.gobj_send_event("EV_CREATE_MODE", {}, self);
                            }
                        }
                    },
                    {
                        view: "button",
                        type: "icon",
                        icon: "far fa-arrow-to-left",
                        css: "webix_transparent icon_toolbar_16",
                        tooltip: t("first_record"),
                        width: 50,
                        click: function() {
                            self.gobj_send_event("EV_FIRST_RECORD", {}, self);
                        }
                    },
                    {
                        view: "button",
                        type: "icon",
                        icon: "far fa-angle-double-left",
                        css: "webix_transparent icon_toolbar_16",
                        tooltip: t("prev_page"),
                        width: 50,
                        click: function() {
                            self.gobj_send_event("EV_PREVIOUS_PAGE", {}, self);
                        }
                    },
                    {
                        view: "button",
                        type: "icon",
                        icon: "far fa-angle-left",
                        css: "webix_transparent icon_toolbar_16",
                        tooltip: t("prev_record"),
                        width: 50,
                        click: function() {
                            self.gobj_send_event("EV_PREVIOUS_RECORD", {}, self);
                        }
                    },
                    {
                        view: "button",
                        type: "icon",
                        icon: "far fa-angle-right",
                        css: "webix_transparent icon_toolbar_16",
                        tooltip: t("next_record"),
                        width: 50,
                        click: function() {
                            self.gobj_send_event("EV_NEXT_RECORD", {}, self);
                        }
                    },
                    {
                        view: "button",
                        type: "icon",
                        icon: "far fa-angle-double-right",
                        css: "webix_transparent icon_toolbar_16",
                        tooltip: t("next_page"),
                        width: 50,
                        click: function() {
                            self.gobj_send_event("EV_NEXT_PAGE", {}, self);
                        }
                    },
                    {
                        view: "button",
                        type: "icon",
                        icon: "far fa-arrow-to-right",
                        css: "webix_transparent icon_toolbar_16",
                        tooltip: t("last_record"),
                        width: 50,
                        click: function() {
                            self.gobj_send_event("EV_LAST_RECORD", {}, self);
                        }
                    },
                    {
                        view: "text",
                        id: build_name(self, "record_idx"),
                        minWidth: 90,
                        maxWidth: 140,
                        format: "1" + webix.i18n.groupDelimiter + "111",
                        label: t("record_idx"),
                        tooltip: t("record_idx"),
                        value: self.config.record_idx,
                        labelPosition: "top",
                        css: "small_input small_label",
                        on: {
                            onChange: function(newVal, oldVal) {
                                newVal = Number(newVal);
                                if(self.config.record_idx != newVal) {
                                    self.gobj_send_event(
                                        "EV_RECORD_BY_INDEX", {record_idx:newVal}, self
                                    );
                                }
                            }
                        }
                    },
                    {
                        view: "text",
                        id: build_name(self, "page"),
                        hidden: self.config.with_paging?false:true,
                        minWidth: 70,
                        maxWidth: 140,
                        format: "1" + webix.i18n.groupDelimiter + "111",
                        label: t("page"),
                        tooltip: t("page"),
                        value: self.config.page,
                        labelPosition: "top",
                        css: "small_input small_label",
                        on: {
                            onChange: function(newVal, oldVal) {
                                newVal = Number(newVal);
                                if(self.config.page != newVal) {
                                    self.gobj_send_event("EV_PAGE", {page:newVal}, self);
                                }
                            }
                        }
                    },
                    {
                        view: "text",
                        id: build_name(self, "total"),
                        hidden: self.config.with_paging?false:true,
                        minWidth: 90,
                        maxWidth: 140,
                        format: "1" + webix.i18n.groupDelimiter + "111",
                        label: t("total"),
                        readonly: true,
                        tooltip: t("total"),
                        value: self.config.total,
                        labelPosition: "top",
                        css: "small_input small_label"
                    },
                    {
                        view: "text",
                        id: build_name(self, "page_size"),
                        hidden: self.config.with_paging?false:true,
                        minWidth: 70,
                        maxWidth: 140,
                        format: "1" + webix.i18n.groupDelimiter + "111",
                        label: t("page_size"),
                        tooltip: t("page_size"),
                        value: self.config.page_size,
                        labelPosition: "top",
                        css: "small_input small_label",
                        on: {
                            onChange: function(newVal, oldVal) {
                                newVal = Number(newVal);
                                if(self.config.page_size != newVal) {
                                    self.config.page_size = newVal;
                                    self.gobj_save_persistent_attrs();
                                }
                            }
                        }
                    },
                    {
                        view: "button",
                        hidden: self.config.with_refresh?false:true,
                        type: "icon",
                        icon: "far fa-sync",
                        css: "webix_transparent icon_toolbar_16",
                        tooltip: t("refresh"),
                        width: 50,
                        click: function() {
                            var $table = $$(build_name(self, "list_table"));
                            self.config.last_selected_id = $table.getSelectedId();
                            $table.clearAll();

                            self.gobj_send_event("EV_UNDO_RECORD", {}, self);
                            self.gobj_send_event("EV_DISCARD_RECORD", {}, self);

                            self.gobj_publish_event(
                                "EV_REFRESH_TABLE",
                                {
                                    topic_name: self.config.topic_name
                                }
                            );
                        }
                    },
                    {
                        view:"button",
                        type: "icon",
                        icon: "fas fa-folder-tree",
                        css: "webix_transparent icon_toolbar_16",
                        hidden: self.config.with_json_viewers?false:true,
                        maxWidth: 80,
                        label: t("Cols"),
                        click: function() {
                            var n = "Json Cols: " + self.name;
                            var gobj_je = __yuno__.gobj_find_unique_gobj(n);
                            if(!gobj_je) {
                                gobj_je = __yuno__.gobj_create_unique(
                                    n,
                                    Je_viewer,
                                    {
                                        window_title: n,
                                        width: 900,
                                        height: 600
                                    },
                                    self
                                );
                                gobj_je.gobj_start();
                            }
                            gobj_je.gobj_send_event(
                                "EV_SHOW",
                                {},
                                self
                            );
                            gobj_je.gobj_send_event(
                                "EV_CLEAR_DATA",
                                {},
                                self
                            );
                            var $table = $$(build_name(self, "list_table"));
                            gobj_je.gobj_send_event(
                                "EV_LOAD_DATA",
                                {
                                    data: self.config.cols
                                },
                                self
                            );
                        }
                    },
                    {
                        view:"button",
                        type: "icon",
                        icon: "fas fa-folder-tree",
                        css: "webix_transparent icon_toolbar_16",
                        hidden: self.config.with_json_viewers?false:true,
                        maxWidth: 80,
                        label: t("Data"),
                        click: function() {
                            var n = "Json Data: " + self.name;
                            var gobj_je = __yuno__.gobj_find_unique_gobj(n);
                            if(!gobj_je) {
                                gobj_je = __yuno__.gobj_create_unique(
                                    n,
                                    Je_viewer,
                                    {
                                        window_title: n,
                                        width: 900,
                                        height: 600
                                    },
                                    self
                                );
                                gobj_je.gobj_start();
                            }
                            gobj_je.gobj_send_event(
                                "EV_SHOW",
                                {},
                                self
                            );
                            gobj_je.gobj_send_event(
                                "EV_CLEAR_DATA",
                                {},
                                self
                            );
                            var $table = $$(build_name(self, "list_table"));
                            gobj_je.gobj_send_event(
                                "EV_LOAD_DATA",
                                {
                                    data: $table.serialize(true)
                                },
                                self
                            );
                        }
                    }
                ]
            }
        };

        var update_toolbar = {
            view: "toolbar",
            id: build_name(self, "update_toolbar"),
            width: 40,
            css: "toolbar2color",
            rows: [
                {
                    view: "icon",
                    id: build_name(self, "update_record"),
                    icon: "far fa-save",
                    css: "webix_transparent icon_toolbar_24",
                    tooltip: t("update record"),
                    click: function() {
                        var $form = $$(build_name(self, "update_form"));
                        self.gobj_send_event("EV_UPDATE_RECORD", $form.getValues(), self);
                    }
                },
                {
                    view: "icon",
                    id: build_name(self, "undo_record"),
                    icon: "far fa-undo",
                    css: "webix_transparent icon_toolbar_24",
                    tooltip: t("undo"),
                    click: function() {
                        self.gobj_send_event("EV_UNDO_RECORD", {}, self);
                    }
                },
                {
                    view: "icon",
                    id: build_name(self, "delete_record"),
                    hidden: self.config.with_trash_button?false:true,
                    icon: "fas fa-trash-alt",
                    css: "webix_transparent icon_toolbar_24",
                    tooltip: t("delete record"),
                    click: function() {
                        webix.confirm(
                            {
                                title: t("delete record"),
                                text: t("are you sure"),
                                type:"confirm-warning"
                            }).then(function(result) {
                                var $form = $$(build_name(self, "update_form"));
                                self.gobj_send_event("EV_DELETE_RECORD", $form.getValues(), self);
                            }
                        );
                    }
                },
                {},
                {
                    view: "icon",
                    id: build_name(self, "copy_record"),
                    hidden: self.config.with_clone_button?false:true,
                    icon: "far fa-copy",
                    css: "webix_transparent icon_toolbar_24",
                    tooltip: t("copy"),
                    click: function() {
                        var $form = $$(build_name(self, "update_form"));
                        self.config.clone_record = __duplicate__($form.getValues());
                        delete self.config.clone_record.id;

                        if(is_string(self.config.clone_record._geometry)) {
                            self.config.clone_record._geometry = JSON.parse(
                                self.config.clone_record._geometry
                            );
                        }
                    }
                }
            ]
        };

        var create_toolbar = {
            view: "toolbar",
            id: build_name(self, "create_toolbar"),
            width: 40,
            css: "toolbar2color",
            rows: [
                {
                    view: "icon",
                    id: build_name(self, "create_record"),
                    icon: "far fa-save",
                    css: "webix_transparent icon_toolbar_24",
                    tooltip: t("save record"),
                    click: function() {
                        var $new = $$(build_name(self, "create_form"));
                        self.gobj_send_event("EV_CREATE_RECORD", $new.getValues(), self);
                    }
                },
                {
                    view: "icon",
                    id: build_name(self, "discard_record"),
                    icon: "far fa-rectangle-landscape",
                    css: "webix_transparent icon_toolbar_24",
                    tooltip: t("discard record"),
                    click: function() {
                        self.gobj_send_event("EV_DISCARD_RECORD", {}, self);
                    }
                },
                {},
                {
                    view: "icon",
                    id: build_name(self, "paste_record"),
                    hidden: self.config.with_clone_button?false:true,
                    icon: "far fa-paste",
                    css: "webix_transparent icon_toolbar_24",
                    tooltip: t("paste"),
                    click: function() {
                        var $form = $$(build_name(self, "create_form"));
                        if(self.config.clone_record) {
                            self.config.clone_record._geometry.x +=
                                self.config.clone_record._geometry.width;
                            self.config.clone_record._geometry.y +=
                                self.config.clone_record._geometry.height;

                            $form.setValues(self.config.clone_record);
                        }
                    }
                }
            ]
        };

        var list_table = {
            view: "datatable",
            id: build_name(self, "list_table"),
            tooltip: self.config.with_tooltip,
            footer: self.config.with_footer,
            select: self.config.with_select,
            multiselect: self.config.with_multiselect,
            editable: self.config.with_table_editable,
            navigation: self.config.with_keyboard_navigation,
            resizeColumn: self.config.with_resizeColumn,
            resizeRow: self.config.with_resizeRow,
            fixedRowHeight: self.config.with_fixedRowHeight,
            drag: self.config.with_drag? "source":false,
            gobj: self, // HACK needed for factory. Available in config.gobj

            onClick: {
                "hook-class": function(e, id, node) {
                    var record = this.getItem(id);
                    if(record[id.column]) {
                        var kw_hook = {
                            click_x: e.x,
                            click_y: e.y,
                            id: id.row,
                            hook_name: id.column
                        };
                        self.gobj_send_event("EV_SHOW_HOOK_DATA", kw_hook, self);
                    }
                }
            },

            on: {
                onCheck: function(rowId, colId, state){
                    var $table = $$(build_name(self, "list_table"));
                    var record = $table.getItem(rowId);
                    self.gobj_publish_event(
                        "EV_ROW_CHECKED",
                        {
                            topic_name: self.config.topic_name,
                            record: record,
                            checked: state
                        }
                    );
                },
                onItemDblClick: function(id) {
                    if(self.config.update_mode_enabled) {
                        $$(build_name(self, "segmented")).setValue("update");
                        self.gobj_send_event("EV_UPDATE_MODE", {}, self);
                    }
                },
                onAfterSelect: function(selection, preserve) {
                    set_current_table_record(self);
                }
            }
        };

        self.config.webix_datatable_id = build_name(self, "list_table");

        var update_form_window = {
            view: "scrollview",
            id: build_name(self, "update_form_window"),
            scroll: "auto",
            hidden: true,
            body: {
                cols: [
                    {
                        view: "form",
                        id: build_name(self, "update_form"),
                        scroll:true,
                        elementsConfig: {
                            labelAlign:"left",
                            labelWidth: self.config.form_label_width
                        },
                        elements: [],
                        on: {
                            onChange: function(new_v, old_v) {
                                update_form_onChange(self, new_v, old_v);
                            },
                            onValidationError: function(key, obj) {
                                log_warning(t("check field") + ": '" + key + "'");
                            }
                        }
                    },
                    update_toolbar
                ]
            }
        };

        var create_form_window = {
            view: "scrollview",
            id: build_name(self, "create_form_window"),
            scroll: "auto",
            hidden: true,
            body: {
                cols: [
                    {
                        view: "form",
                        id: build_name(self, "create_form"),
                        scroll:true,
                        elementsConfig: {
                            labelAlign:"left",
                            labelWidth: self.config.form_label_width
                        },
                        elements: [],
                        on: {
                            onChange: function(new_v, old_v) {
                                create_form_onChange(self, new_v, old_v);
                            },
                            onValidationError: function(key, obj) {
                                log_warning(t("check field") + ": '" + key + "'");
                            }
                        }
                    },
                    create_toolbar
                ]
            }
        };

        /*----------------------------------------------------*
         *                      UI
         *  Common UI of Pinhold Window and Container Panel
         *----------------------------------------------------*/
        if(self.config.is_pinhold_window) {
            /*-------------------------*
             *      Pinhold Window
             *-------------------------*/
            self.config.$ui = webix.ui({
                view: "window",
                id: self.gobj_escaped_short_name(), // HACK can be a global gobj, use gclass_name+name
                top: self.config.top,
                left: self.config.left,
                width: self.config.width,
                height: self.config.height,
                hidden: self.config.pinpushed?true:false,
                move: true,
                resize: true,
                position: (self.config.left==0 && self.config.top==0)?"center":null,
                head: get_window_top_toolbar(self),
                body: {
                    id: build_name(self, "fullscreen"),
                    ////////////////// REPEATED webix code /////////////////
                    rows: [
                        {
                            animate: false,
                            cells: [
                                list_table,
                                update_form_window,
                                create_form_window
                            ]
                        },
                        navigation_toolbar
                    ]
                    ////////////////// webix code /////////////////
                },
                on: {
                    "onViewResize": function() {
                        self.config.left = this.gobj.config.$ui.config.left;
                        self.config.top = this.gobj.config.$ui.config.top;
                        self.config.width = this.gobj.config.$ui.config.width;
                        self.config.height = this.gobj.config.$ui.config.height;
                        self.gobj_save_persistent_attrs();
                    },
                    "onViewMoveEnd": function() {
                        self.config.left = this.gobj.config.$ui.config.left;
                        self.config.top = this.gobj.config.$ui.config.top;
                        self.config.width = this.gobj.config.$ui.config.width;
                        self.config.height = this.gobj.config.$ui.config.height;
                        self.gobj_save_persistent_attrs();
                    }
                }
            });
            self.config.$ui_fullscreen = $$(build_name(self, "fullscreen"));

        } else {
            /*-------------------------*
             *      Container Panel
             *-------------------------*/
            self.config.$ui = webix.ui({
                id: self.gobj_name(),
                ////////////////// REPEATED webix code /////////////////
                rows: [
                    // HACK "Container Panel" toolbar suministrada por ui_container
                    get_container_panel_top_toolbar(self),
                    {
                        animate: false,
                        cells: [
                            list_table,
                            update_form_window,
                            create_form_window
                        ]
                    },
                    navigation_toolbar
                ]
                ////////////////// webix code /////////////////
            });
        }
        self.config.$ui.gobj = self;

        if(self.config.ui_properties) {
            self.config.$ui.define(self.config.ui_properties);
            if(self.config.$ui.refresh) {
                self.config.$ui.refresh();
            }
        }

        /*----------------------------------------------*
         *  Inform of panel viewed to "Container Panel"
         *----------------------------------------------*/
        if(!self.config.is_pinhold_window) {
            self.config.$ui.attachEvent("onViewShow", function() {
                self.parent.gobj_send_event("EV_ON_VIEW_SHOW", self, self);
            });
        }

        /*----------------------------------------------*
         *  Set fullscreen ui in "Pinhold Window"
         *----------------------------------------------*/
        if(self.config.is_pinhold_window) {
            self.config.$ui_fullscreen = $$(build_name(self, "fullscreen"));
            automatic_resizing_cb(); // Adapt window size to device
        }

        /*---------------------------------------*
         *   Automatic Resizing in "Pinhold Window"
         *---------------------------------------*/
        function automatic_resizing(gadget, window_width, window_height)
        {
            var $gadget = $$(gadget);
            var new_width = -1;
            var new_height = -1;
            var new_x = $gadget.config.left;
            var new_y = $gadget.config.top;

            if($gadget.$width + new_x > window_width) {
                new_width = window_width;
                new_x = 0;
            }
            if($gadget.$height + new_y > window_height) {
                new_height = window_height;
                new_y = 0;
            }

            if(new_width < 0 && new_height < 0) {
                return;
            }

            $gadget.config.width = new_width<0? $gadget.$width:new_width;
            $gadget.config.height = new_height<0? $gadget.$height:new_height;
            $gadget.resize();
            $gadget.setPosition(new_x, new_y);
        }

        function automatic_resizing_cb()
        {
            var window_width = window.innerWidth-8;
            var window_height = window.innerHeight-8;
            automatic_resizing(self.gobj_escaped_short_name(), window_width, window_height);
        }

        if(self.config.is_pinhold_window) {
            if(self.config.resizing_event_id) {
                webix.eventRemove(self.config.resizing_event_id);
                self.config.resizing_event_id = 0;
            }
            self.config.resizing_event_id = webix.event(window, "resize", automatic_resizing_cb);
        }
    }

    /********************************************
     *
     ********************************************/
    function cols2webix_table_cols(self, cols)
    {
        var webix_schema = [];
        if(self.config.with_checkbox) {
            webix_schema.push(
                {
                    id: "row",
                    header: { content:"masterCheckbox", css: "doublecheckbox" },
                    checkValue: true,
                    uncheckValue: false,
                    template: "{common.checkbox()}",
                    width: 50,
                    css: "doublecheckbox"
                }
            );
        } else if(self.config.with_radio) {
            webix_schema.push(
                {
                    id: "row",
                    header: "",
                    checkValue: true,
                    uncheckValue: false,
                    template: "{common.radio()}",
                    width: 50,
                    css: "doubleradio"
                }
            );
        }

        for(var i=0,j=0; cols && i<cols.length; i++) {
            var webix_col = null;
            var tranger_col = cols[i];
            if(self.config.hide_private_fields) {
                if(tranger_col.id.charAt(0)=='_') {
                    continue;
                }
            }

            webix_col = {
                id: self.config.with_webix_id?
                    (tranger_col.id==="id"?"id_":tranger_col.id):tranger_col.id,
                header: self.config.with_textfilter?
                    [t(tranger_col.header), {content:"textFilter"}]:t(tranger_col.header),

                sort: self.config.with_sort?
                    tranger_col.type === 'integer'? "int":"string":false,
                minWidth: tranger_col.fillspace * 10,
                fillspace: tranger_col.fillspace
            };

            var flag = tranger_col.flag;
            var is_hook = elm_in_list("hook", flag);
            var is_fkey = elm_in_list("fkey", flag);
            var is_enum = elm_in_list("enum", flag);
            var is_time = elm_in_list("time", flag);
            var is_color = elm_in_list("color", flag);
            var is_rowid = elm_in_list("rowid", flag);

            if(is_rowid) {
                if(!self.config.with_rowid_field) {
                    continue;
                }
            }

            var real_type;
            var enum_list;
            var type = tranger_col.type; // By default is basic type
            if(is_hook) {
                type = "hook";
            } else if(is_fkey) {
                type = "fkey";
            } else if(is_enum) {
                type = "enum";
            } else if(is_time) {
                type = "time";
            } else if(is_color) {
                type = "color";
            }

            switch(type) {
                case "string":
                    webix_col["editor"] = "text";
                    break;
                case "integer":
                    webix_col["editor"] = "text";
                    break;
                case "real":
                    webix_col["editor"] = "text";
                    break;
                case "boolean":
                    if(self.config.with_table_editable) {
                        webix_col["template"] = "{common.checkbox()}";
                        webix_col["checkValue"] = true;
                        webix_col["uncheckValue"] = false;
                    } else {
                        webix_col["editor"] = "inline-checkbox";
                    }
                    break;
                case "object":
                case "dict":
                    webix_col["editor"] = "text";
                    break;
                case "array":
                case "list":
                    webix_col["editor"] = "text";
                    break;
                case "blob":
                    webix_col["editor"] = "text";
                    break;

                case "enum":
                    real_type = tranger_col.type;
                    enum_list = tranger_col.enum;
                    switch(real_type) {
                        case "string":
                            webix_col["editor"] = "combo";
                            //webix_col["optionslist"] = true;
                            webix_col["options"] = list2options(enum_list);
                            break;
                        case "object":
                        case "dict":
                        case "array":
                        case "list":
                            webix_col["optionslist"] = true;
                            webix_col["options"] = list2options(enum_list);
                            webix_col["editor"] = "multiselect";
                            webix_col["suggest"] = {
                               view:"checksuggest"
                            };
                            break;
                        default:
                            log_error("table '" + self.config.topic_name +
                                "' enum type of '" + tranger_col.id +
                                "' is invalid: " + real_type
                            );
                            webix_col["optionslist"] = true;
                            webix_col["options"] = list2options(enum_list);
                            webix_col["editor"] = "multiselect";
                            webix_col["suggest"] = {
                               view:"checksuggest"
                            };
                            break;
                    }
                    break;

                case "time":
                    real_type = tranger_col.type;
                    switch(real_type) {
                        case "string":
                            webix_col["editor"] = "date";
                            break;
                        case "integer":
                            webix_col["editor"] = "date";
                            break;
                        default:
                            log_error("table '" + self.config.topic_name +
                                "' enum type of '" + tranger_col.id +
                                "' is invalid: " + real_type
                            );
                            webix_col["editor"] = "date";
                            break;
                    }
                    break;

                case "color":
                    real_type = tranger_col.type;
                    switch(real_type) {
                        case "string":
                            webix_col["editor"] = "color";
                            break;
                        case "integer":
                            webix_col["editor"] = "color";
                            break;
                        default:
                            log_error("table '" + self.config.topic_name +
                                "' enum type of '" + tranger_col.id +
                                "' is invalid: " + real_type
                            );
                            webix_col["editor"] = "color";
                            break;
                    }
                    break;

                case "hook":    // definition of webix DATATABLE col
                    break;

                case "fkey":    // definition of webix DATATABLE col
                    webix_col["optionslist"] = true;
                    webix_col["editor"] = "multiselect";
                    webix_col["options"] = get_fkey_options(self, tranger_col);
                    break;

                default:
                    log_error("col type unknown 1: " + type);
                    break;
            }

            if(webix_col) {
                j++;
                if(self.config.with_footer) {
                    if(j==1) {
                        webix_col.footer = {
                            content: "countColumn",
                            height: 25
                        };
                    } else if(j==2) {
                        webix_col.footer = {
                            text: t("records"),
                            height: 25
                        };
                    }
                }

                if(tranger_col.template) {
                    webix_col["template"] = tranger_col.template;
                }
                if(!self.config.fields_enabled ||
                        elm_in_list(webix_col.id, self.config.fields_enabled)) {
                    webix_schema.push(webix_col);
                }
            }
        }
        return webix_schema;
    }

    /********************************************
     *  General onChange for create form
     ********************************************/
    function create_form_onChange(self, new_v, old_v)
    {
        var $form = $$(build_name(self, "create_form"));
        var changed = $form.isDirty();
        if(changed) {
            var btn = $$(build_name(self, "create_record"));
            webix.html.addCss(btn.getNode(), "icon_color_submmit");
            btn = $$(build_name(self, "discard_record"));
            webix.html.addCss(btn.getNode(), "icon_color_cancel");
        }
    }

    /********************************************
     *  General onChange for create form
     ********************************************/
    function update_form_onChange(self, new_v, old_v)
    {
        var $form = $$(build_name(self, "update_form"));
        var changed = $form.isDirty();
        if(changed) {
            var btn = $$(build_name(self, "update_record"));
            webix.html.addCss(btn.getNode(), "icon_color_submmit");
            btn = $$(build_name(self, "undo_record"));
            webix.html.addCss(btn.getNode(), "icon_color_cancel");
        }
    }

    /********************************************
     *  Own rules to form
     ********************************************/
    function rule_json(value, fields, name)
    {
        if(empty_string(value)) {
            return true;
        }
        try {
            JSON.parse(value);
        } catch (e) {
            return false;
        }
        return true;
    }

    /********************************************
     *
     ********************************************/
    function build_list_table(self, cols)
    {
        var $table = $$(build_name(self, "list_table"));
        $table.clearAll();

        // redefine columns
        $table.config.columns = cols2webix_table_cols(self, cols);
        $table.refreshColumns();

        return $table;
    }

    /********************************************
     *
     ********************************************/
    function cols2webix_form_elements(self, cols, mode)
    {
        var _writable_fields = [];
        var webix_elements = [];

        var system_topic = false;
        var topic_name = self.config.topic_name;
        if(empty_string(topic_name) || topic_name.substring(0, 2) == "__") {
            system_topic = true;
        }

        if(self.config.with_webix_id) {
            webix_elements.push({
                view: "text",
                name: "id",
                label: "Webix Id",
                css: "input_font_fijo",
                readonly: true,
                type: "string"
            });
        }

        for(var i=0; cols && i<cols.length; i++) {
            var webix_element = null;
            var tranger_col = cols[i];
            if(self.config.hide_private_fields) {
                if(tranger_col.id.charAt(0)=='_') {
                    continue;
                }
            }

            var id = self.config.with_webix_id?
                    (tranger_col.id==="id"?"id_":tranger_col.id):tranger_col.id;
            var flag = tranger_col.flag;
            var is_required = elm_in_list("required", flag);
            var is_persistent = elm_in_list("persistent", flag);
            var is_password = elm_in_list("password", flag);
            var is_email = elm_in_list("email", flag);
            var is_url = elm_in_list("url", flag);
            var is_writable = elm_in_list("writable", flag);
            var is_notnull = elm_in_list("notnull", flag);
            var is_hook = elm_in_list("hook", flag);
            var is_fkey = elm_in_list("fkey", flag);
            var is_enum = elm_in_list("enum", flag);
            var is_time = elm_in_list("time", flag);
            var is_color = elm_in_list("color", flag);
            var is_rowid = elm_in_list("rowid", flag);

            if(is_rowid) {
                if(!self.config.with_rowid_field) {
                    continue;
                }
            }

            if(is_hook || is_fkey) {
                is_writable = true;
            }

            switch(mode) {
                case "create":
                    if(!system_topic && (is_required || is_persistent || is_writable)) {
                        is_writable = true;
                        _writable_fields.push(id);
                    } else {
                        is_writable = false;
                    }
                    break;
                case "update":
                default:
                    if(!system_topic && (is_writable)) {
                        _writable_fields.push(id);
                    } else {
                        is_writable = false;
                    }
                    break;
            }

            var real_type;
            var enum_list;
            var type = tranger_col.type; // By default is basic type
            if(is_hook) {
                type = "hook";
            } else if(is_fkey) {
                type = "fkey";
            } else if(is_enum) {
                type = "enum";
            } else if(is_time) {
                type = "time";
            } else if(is_color) {
                type = "color";
            }

            switch(type) {
                case "string":
                    var types = "text";
                    if(tranger_col.id.indexOf("email")>=0) {
                        types = "email";
                    }
                    if(is_email) {
                        types = "email";
                    }
                    if(is_url) {
                        types = "url";
                    }
                    if(is_password) {
                        types = "password";
                    }
                    webix_element = {
                        view: "text",
                        name: id,
                        label: t(tranger_col.header),
                        css: "input_font_fijo",
                        readonly: is_writable?false:true,
                        type: types
                    };

                    if(is_required) {
                        webix_element["required"] = true;
                    }
                    if(is_email) {
                        webix_element["validate"] = webix.rules.isEmail;
                        webix_element["invalidMessage"] = t("incorrect email address");
                    }
                    break;
                case "integer":
                    webix_element = {
                        view: "text",
                        name: id,
                        label: t(tranger_col.header),
                        css: "input_font_fijo",
                        readonly: is_writable?false:true,
                        type: "number"
                    };
                    if(is_required) {
                        webix_element["required"] = true;
                    }
                    if(is_writable) {
                        // No lo uses, da por inválidos campos vacios
                        //webix_element["validate"] = webix.rules.isNumber;
                        //webix_element["invalidMessage"] = t("invalid number");
                    }
                    break;
                case "real":
                    webix_element = {
                        view: "text",
                        name: id,
                        label: t(tranger_col.header),
                        css: "input_font_fijo",
                        readonly: is_writable?false:true,
                        type: "number"
                    };
                    if(is_required) {
                        webix_element["required"] = true;
                    }
                    if(is_writable) {
                        // No lo uses, da por inválidos campos vacios
                        //webix_element["validate"] = webix.rules.isNumber;
                        //webix_element["invalidMessage"] = t("invalid number");
                    }
                    break;
                case "boolean":
                    webix_element = {
                        view: "checkbox",
                        name: id,
                        label: t(tranger_col.header),
                        css: "input_font_fijo",
                        readonly: is_writable?false:true
                    };
                    if(is_required) {
                        webix_element["required"] = true;
                    }
                    break;
                case "object":
                case "dict":
                    webix_element = {
                        view: "text",
                        name: id,
                        label: t(tranger_col.header),
                        css: "input_font_fijo",
                        readonly: is_writable?false:true,
                        type: "text"
                    };
                    if(is_required) {
                        webix_element["required"] = true;
                    }
                    webix_element["validate"] = rule_json;
                    webix_element["invalidMessage"] = t("invalid json");
                    break;
                case "array":
                case "list":
                    webix_element = {
                        view: "text",
                        name: id,
                        label: t(tranger_col.header),
                        css: "input_font_fijo",
                        readonly: is_writable?false:true,
                        type: "text"
                    };
                    if(is_required) {
                        webix_element["required"] = true;
                    }
                    webix_element["validate"] = rule_json;
                    webix_element["invalidMessage"] = t("invalid json");
                    break;
                case "blob":
                    // TODO botón en campo del form para abrir js_editor
                    webix_element = {
                        view: "text",
                        name: id,
                        label: t(tranger_col.header),
                        css: "input_font_fijo",
                        readonly: is_writable?false:true,
                        type: "text"
                    };
                    if(is_required) {
                        webix_element["required"] = true;
                    }
                    webix_element["validate"] = rule_json;
                    webix_element["invalidMessage"] = t("invalid json");
                    break;
                case "enum":
                    real_type = tranger_col.type;
                    enum_list = tranger_col.enum;
                    switch(real_type) {
                        case "string":
                            webix_element = {
                                view: "combo2",
                                name: id,
                                label: t(tranger_col.header),
                                css: "input_font_fijo",
                                readonly: is_writable?false:true,
                                options: list2options(enum_list)
                            };
                            break;
                        case "object":
                        case "dict":
                        case "array":
                        case "list":
                            webix_element = {
                                view: "multicombo2",
                                name: id,
                                label: t(tranger_col.header),
                                css: "input_font_fijo",
                                readonly: is_writable?false:true,
                                options: list2options(enum_list)
                            };
                            break;
                        default:
                            log_error("form '" + self.config.topic_name +
                                "' enum type of '" + tranger_col.id +
                                "' is invalid: " + real_type
                            );
                            webix_element = {
                                view: "multicombo2",
                                name: id,
                                label: t(tranger_col.header),
                                css: "input_font_fijo",
                                readonly: is_writable?false:true,
                                options: list2options(enum_list)
                            };
                            break;
                    }
                    if(is_required) {
                        webix_element["required"] = true;
                    }
                    break;

                case "time":
                    real_type = tranger_col.type;
                    switch(real_type) {
                        case "string":
                        case "integer":
                            webix_element = {
                                view: "datepicker",
                                name: id,
                                timepicker: true,
                                editable: true,
                                label: t(tranger_col.header),
                                css: "input_font_fijo",
                                readonly: is_writable?false:true
                            };
                            break;
                        default:
                            log_error("form '" + self.config.topic_name +
                                "' enum type of '" + tranger_col.id +
                                "' is invalid: " + real_type
                            );
                            webix_element = {
                                view: "datepicker",
                                name: id,
                                timepicker: true,
                                editable: true,
                                label: t(tranger_col.header),
                                css: "input_font_fijo",
                                readonly: is_writable?false:true
                            };
                            break;
                    }
                    if(is_required) {
                        webix_element["required"] = true;
                    }
                    break;

                case "color":
                    real_type = tranger_col.type;
                    switch(real_type) {
                        case "string":
                        case "integer":
                            webix_element = {
                                view: "colorpicker",
                                name: id,
                                label: t(tranger_col.header),
                                css: "input_font_fijo",
                                readonly: is_writable?false:true
                            };
                            break;
                        default:
                            log_error("form '" + self.config.topic_name +
                                "' enum type of '" + tranger_col.id +
                                "' is invalid: " + real_type
                            );
                            webix_element = {
                                view: "colorpicker",
                                name: id,
                                label: t(tranger_col.header),
                                css: "input_font_fijo",
                                readonly: is_writable?false:true
                            };
                            break;
                    }
                    if(is_required) {
                        webix_element["required"] = true;
                    }
                    break;

                case "fkey":    // Definition of webix FORM element
                    webix_element = {
                        view: "multicombo2",
                        name: id,
                        label: t(tranger_col.header),
                        css: "input_font_fijo",
                        readonly: is_writable?false:true,
                        options: get_fkey_options(self, tranger_col)
                    };
                    if(is_required) {
                        webix_element["required"] = true;
                    }
                    break;

                case "hook":
                    // set below, only in update form
                    break;
                default:
                    log_error("col type unknown 2: " + type);
                    break;
            }

            if(mode == "update") {
                switch(type) {
                    case "hook":    // Definition of webix FORM element
                        webix_element = {
                            cols: [
                                {
                                    view: "label",
                                    width: self.config.form_label_width,
                                    label: t(tranger_col.header)
                                },
                                {
                                    view: "label",
                                    name: id,
                                    width: 120,
                                    label: "",
                                    on: {
                                        onItemClick: function(id, e) {
                                            if(this.data.value) {
                                                var kw_hook = {
                                                    click_x: e.x,
                                                    click_y: e.y,
                                                    id: self.config.current_id,
                                                    hook_name: this.data.name
                                                };
                                                self.gobj_send_event(
                                                    "EV_SHOW_HOOK_DATA", kw_hook, self
                                                );
                                            }
                                        }
                                    }
                                },
                                {}
                            ]
                        };
                        break;
                }
            }

            if(webix_element) {
                webix_elements.push(webix_element);
            }
        }

        return [webix_elements, _writable_fields];
    }

    /********************************************
     *
     ********************************************/
    function build_update_form(self, cols)
    {
        var $form = $$(build_name(self, "update_form"));
        $form.clear();

        var eles = cols2webix_form_elements(self, cols, "update");

        // redefine elements
        $form.define("elements", eles[0]);

        self.config._writable_fields = eles[1];

        $form.reconstruct();
        return $form;
    }

    /********************************************
     *
     ********************************************/
    function build_create_form(self, cols)
    {
        var $form = $$(build_name(self, "create_form"));
        $form.clear();

        var eles = cols2webix_form_elements(self, cols, "create");

        // redefine elements
        $form.define("elements", eles[0]);

        self.config._writable_fields = eles[1];

        $form.reconstruct();
        return $form;
    }

    /********************************************
     *
     ********************************************/
    function set_current_table_record(self)
    {
        var $table = $$(build_name(self, "list_table"));

        var record = $table.getSelectedItem();
        var id = $table.getSelectedId();
        var idx = $table.getIndexById(id);
        self.config.record_idx = idx + 1;
        self.config.current_id = id.id;
        $$(build_name(self, "record_idx")).setValue(self.config.record_idx);

        if(self.config.page_size>0) {
            self.config.page = Math.floor(idx / self.config.page_size) + 1;
            $$(build_name(self, "page")).setValue(self.config.page);
        }

        if(self.config.publish_row_selected) {
            self.gobj_publish_event(
                "EV_ROW_SELECTED",
                {
                    topic_name: self.config.topic_name,
                    record: record,
                    id: id.id
                }
            );
        }
    }

    /********************************************
     *
     ********************************************/
    function build_table(self)
    {
        var cols = self.config.cols;
        if(!cols) {
            cols = self.config.schema;
        }

        /*
         *  Tabla
         */
        var $table = build_list_table(self, cols);

        /*
         *  Form update
         */
        var $update = build_update_form(self, cols);

        /*
         *  Bind form to table
         */
        $update.bind($table);

        /*
         *  Form create
         */
        var $create = build_create_form(self, cols);

        /*
         *  Set initial mode
         */
        switch(self.config.current_mode) {
            case "update":
                self.gobj_send_event("EV_UPDATE_MODE", {}, self);
                break;
            case "create":
                self.gobj_send_event("EV_CREATE_MODE", {}, self);
                break;
            default:
                self.gobj_send_event("EV_LIST_MODE", {}, self);
                break;
        }
    }

    /********************************************
     *
     ********************************************/
    function get_fkey_options(self, col)
    {
        for(var topic_name in col.fkey) {
            // HACK we work only with one fkey
            var current_list = treedb_get_topic_data(self.config.treedb_name, topic_name);
            return list2options(current_list, "id", "id");
        }
        log_error("No fkey options found" + STRING.stringify(col));
        return null;
    }

    /********************************************
     *
     ********************************************/
    function get_hook_options(self, col)
    {
        for(var topic_name in col.hook) {
            // HACK we work with only one hook
            var current_list = treedb_get_topic_data(self.config.treedb_name, topic_name);
            return list2options(current_list, "id", "id");
        }
        log_error("No hook options found" + STRING.stringify(col));
        return null;
    }

    /********************************************
     *
     ********************************************/
    function webix_dt_collect($table, kw)
    {
        var collection = [];
        $table.eachRow(function(row) {
            var record = $table.getItem(row);
            if(kw_match_simple(record, kw)) {
                collection.push(row);
            }
        });
        return collection;
    }

    /********************************************
     *
     ********************************************/
    function get_schema_col(self, field_name)
    {
        let col = null;
        let cols = self.config.cols;
        if(!cols) {
            cols = self.config.schema;
        }

        for(let i=0; i<cols.length; i++) {
            let col = cols[i];
            if(col.id == field_name) {
                return col;
            }
        }
        return null;
    }

    /********************************************
     *  Convert from backend to frontend
     ********************************************/
    function record2frontend(self, record)
    {
        var new_record = {};
        for(var field_name in record) {
            var value = record[field_name];
            var col = get_schema_col(self, field_name);
            if(col) {
                new_record[field_name] = col2frontend(col, value);
            }
        }
        return new_record;
    }

    /********************************************
     *  Convert from backend to frontend
     ********************************************/
    function col2frontend(col, value)
    {
        var flag = col.flag;
        var is_hook = elm_in_list("hook", flag);
        var is_fkey = elm_in_list("fkey", flag);
        var is_enum = elm_in_list("enum", flag);
        var is_time = elm_in_list("time", flag);
        var is_color = elm_in_list("color", flag);

        var real_type;
        var enum_list;
        var type = col.type; // By default is basic type
        if(is_hook) {
            type = "hook";
        } else if(is_fkey) {
            type = "fkey";
        } else if(is_enum) {
            type = "enum";
        } else if(is_time) {
            type = "time";
        } else if(is_color) {
            type = "color";
        }

        switch(type) {
            case "string":
                break;
            case "integer":
                break;
            case "real":
                break;
            case "boolean":
                break;
            case "object":
            case "dict":
                value = JSON.stringify(value);
                break;
            case "array":
            case "list":
                value = JSON.stringify(value);
                break;
            case "blob":
                value = JSON.stringify(value);
                break;

            case "enum":
                real_type = col.type;
                enum_list = col.enum;
                switch(real_type) {
                    case "string":
                        break;
                    case "object":
                    case "dict":
                    case "array":
                    case "list":
                        break;
                    default:
                        log_error("col type unknown 3: " + real_type);
                        log_error(col);
                        log_error(value);
                        break;
                }
                break;

            case "time":
                real_type = col.type;
                switch(real_type) {
                    case "string":
                        if(value) {
                            value = new Date(value);
                        } else {
                            value = null;
                        }
                        break;
                    case "integer":
                        if(value) {
                            value = new Date(value*1000);
                        } else {
                            value = null;
                        }
                        break;
                    default:
                        log_error("col type unknown 4: " + real_type);
                        log_error(col);
                        log_error(value);
                        break;
                }
                break;

            case "color":
                real_type = col.type;
                switch(real_type) {
                    case "string":
                        // TODO
                        break;
                    case "integer":
                        // TODO
                        break;
                    default:
                        log_error("col type unknown 4: " + real_type);
                        log_error(col);
                        log_error(value);
                        break;
                }
                break;

            case "hook":    // Convert data from backend to frontend
                let items = treedb_hook_data_size(value);
                if(items > 0) {
                    value = "<span class='webix_icon fas fa-eye hook-class'></span>";
                    value += "&nbsp;&nbsp;[&nbsp;<u>" + items + "</u>&nbsp;]";
                } else {
                    value = "";
                }
                break;

            case "fkey":    // Convert data from backend to frontend
                var new_value = [];
                if(value) {
                    if(is_string(value)) {
                        let fkey = decoder_fkey(col, value);
                        if(fkey) {
                            new_value.push(fkey.id);
                        }
                    } else if(is_array(value)) {
                        for(let i=0; i<value.length; i++) {
                            let fkey = decoder_fkey(col, value[i]);
                            if(fkey) {
                                new_value.push(fkey.id);
                            }
                        }
                    } else {
                        log_error("fkey type unsupported: " + JSON.stringify(value));
                    }
                }

                value = new_value;

                break;

            default:
                log_error("col type unknown 5: " + type);
                log_error(col);
                log_error(value);
                break;
        }

        return value;
    }

    /********************************************
     *  Convert from frontend to backend
     ********************************************/
    function record2backend(self, kw, operation)
    {
        for(var field_name in kw) {
            var value = kw[field_name];
            var col = get_schema_col(self, field_name);
            if(col) {
                kw[field_name] = col2backend(col, value, operation);
            } else {
                log_error("No col def for " + field_name);
            }
        }
        return kw;
    }

    /********************************************
     *  Convert from frontend to backend
     ********************************************/
    function col2backend(col, value, operation)
    {
        var flag = col.flag;
        var is_hook = elm_in_list("hook", flag);
        var is_fkey = elm_in_list("fkey", flag);
        var is_rowid = elm_in_list("rowid", flag);
        var is_enum = elm_in_list("enum", flag);
        var is_time = elm_in_list("time", flag);
        var is_color = elm_in_list("color", flag);

        var real_type;
        var enum_list;
        var type = col.type; // By default is basic type
        if(is_enum) {
            type = "enum";
        } else if(is_hook) {
            type = "hook";
        } else if(is_fkey) {
            type = "fkey";
        } else if(is_time) {
            type = "time";
        } else if(is_color) {
            type = "color";
        }

        switch(type) {
            case "string":
                if(is_rowid && operation=="create") {
                    value = "";
                }
                break;
            case "integer":
                value = parseInt(value) || 0;
                break;
            case "real":
                value = parseFloat(value)  || 0.0;
                break;
            case "boolean":
                value = parseBoolean(value);
                break;
            case "object":
            case "dict":
                if(!empty_string(value)) {
                    value = JSON.parse(value);
                } else {
                    value = {};
                }
                break;
            case "array":
            case "list":
                if(!empty_string(value)) {
                    value = JSON.parse(value);
                } else {
                    value = [];
                }
                break;
            case "blob":
                if(!empty_string(value)) {
                    value = JSON.parse(value);
                } else {
                    value = {};
                }
                break;

            case "enum":
                real_type = col.type;
                enum_list = col.enum;
                switch(real_type) {
                    case "string":
                        break;
                    case "object":
                    case "dict":
                    case "array":
                    case "list":
                        break;
                    default:
                        log_error("col type unknown 6: " + real_type);
                        break;
                }
                break;

            case "time":
                real_type = col.type;
                switch(real_type) {
                    case "string":
                        if(value && is_date(value)) {
                            value = value.toISOString();
                        } else {
                            value = "";
                        }
                        break;
                    case "integer":
                        if(value && is_date(value)) {
                            value = (value.getTime())/1000;
                        } else {
                            value = 0;
                        }
                        break;
                    default:
                        log_error("col type unknown 7: " + real_type);
                        break;
                }
                break;

            case "color":
                real_type = col.type;
                switch(real_type) {
                    case "string":
                        // TODO
                        break;
                    case "integer":
                        // TODO
                        break;
                    default:
                        log_error("col type unknown 7: " + real_type);
                        break;
                }
                break;

            case "hook":    // Convert data from frontend to backend
                value = null;
                break;

            case "fkey":    // Convert data from frontend to backend
                value = build_fkey_ref(self, col, value);
                break;

            default:
                log_error("col type unknown 8: " + type);
                break;
        }

        return value;
    }

    /********************************************
     *
     ********************************************/
    function build_fkey_ref(self, col, value)
    {
        // HACK we work only with one fkey
        var topic_name = Object.keys(col.fkey)[0]; // Get the first key
        var hook = col.fkey[topic_name];

        var refs = null;

        switch(col.type) {
            case "string":
                // TODO check if widget is not multi select
                refs = "";
                if(value.length > 0) {
                    let v = value[0];
                    if(!empty_string(v)) {
                        refs = topic_name + "^" + v + "^" + hook;
                    }
                }
                break;
            case "object":
            case "dict":
                refs = {};
                if(value.length > 0) {
                    for(let i=0; i<value.length; i++) {
                        let v = value[i];
                        if(!empty_string(v)) {
                            refs[topic_name + "^" + v + "^" + hook] = true;
                        }
                    }
                }
                break;
            case "array":
            case "list":
                refs = [];
                if(value.length > 0) {
                    for(let i=0; i<value.length; i++) {
                        let v = value[i];
                        if(!empty_string(v)) {
                            refs.push(topic_name + "^" + v + "^" + hook);
                        }
                    }
                }
                break;
            default:
                log_error("Merde type: " + col.type);
                break;
        }

        return refs;
    }




            /***************************
             *      Actions
             ***************************/




    /********************************************
     *  From external
     ********************************************/
    function ac_load_data(self, event, kw, src)
    {
        var data = kw;
        if(!is_array(data)) {
            log_error("FormTable, data MUST be an array");
            trace_msg(data);
            return -1;
        }

        var $table = $$(build_name(self, "list_table"));

        if(self.config.with_webix_id) {
            // HACK change id by id_, in webix id is key primary, in yuneta id can be repeated.
            for(var i=0; i<data.length; i++) {
                data[i]["id_"] = data[i].id;
                delete data[i]["id"];
            }
        }

        var new_data = [];
        for(let i=0; i<data.length; i++) {
            let record = data[i];
            new_data.push(record2frontend(self, record));
        }
        $table.parse(new_data);

        self.config.total = $table.count();
        $$(build_name(self, "total")).setValue(self.config.total);

        $table.unselectAll(); // HACK important, to update the form in the select below.

        if(self.config.update_mode_enabled || self.config.create_mode_enabled) {
            /*
             *  Select only if it has update/create mode
             */
            if(data.length == 1) {
                if(!self.config.with_webix_id) {
                    self.gobj_send_event("EV_RECORD_BY_ID", {id:data[0].id}, self);
                } else {
                    self.gobj_send_event("EV_FIRST_RECORD", {}, self);
                }
            } else if(data.length > 1) {
                if(self.config.last_selected_id) {
                    self.gobj_send_event("EV_RECORD_BY_ID", {id:self.config.last_selected_id.id}, self);
                } else {
                    self.gobj_send_event("EV_FIRST_RECORD", {}, self);
                }
            }
        }

        return 0;
    }

    /********************************************
     *  From external
     ********************************************/
    function ac_delete_data(self, event, kw, src)
    {
        var $table = $$(build_name(self, "list_table"));

        $table.unselectAll(); // HACK important, to update the form in the select below.

        var idx = $table.getIndexById(kw.id);
        if(idx >= 0) {
            $table.remove(kw.id);

            self.config.total = $table.count();
            $$(build_name(self, "total")).setValue(self.config.total);

            if(idx >= self.config.total) {
                idx = self.config.total - 1;
            }
            var id = $table.getIdByIndex(idx);
            $table.select(id);
            if($table.isVisible()) {
                $table.showItem(id);
            }
        } else {
            log_error("delete_data: record not found " + kw.id);
        }

        return 0;
    }

    /********************************************
     *  External wants data
     ********************************************/
    function ac_get_data(self, event, kw, src)
    {
        var data = [];
        var $table = $$(build_name(self, "list_table"));

        // TODO pongo filtros?
        var ids = kwid_get_ids(kw.id);
        for(var i=0; i<ids.length; i++) {
            var id = ids[i];
            var record = $table.getItem(id);
            data.push(record);
        }
        return data;
    }

    /********************************************
     *  External wants data
     ********************************************/
    function ac_get_checked_data(self, event, kw, src)
    {
        var data = [];
        var $table = $$(build_name(self, "list_table"));

        if(self.config.with_checkbox) {
            $table.data.each(function(obj){
                if(obj.row) {
                    data.push(obj);
                }
            });
        }
        return data;
    }

    /********************************************
     *  Update options of combo,etc, fields
     ********************************************/
    function ac_update_options(self, event, kw, src)
    {
        var cols = self.config.cols;
        if(!cols) {
            cols = self.config.schema;
        }

        // redefine columns
        var $table = $$(build_name(self, "list_table"));
        if($table) {
            $table.config.columns = cols2webix_table_cols(self, cols);
            $table.refreshColumns();
        } else {
            log_error("$table not found: " + build_name(self, "list_table"));
        }

        // redefine elements
        var eles = cols2webix_form_elements(self, cols, "create");
        var $create_form = $$(build_name(self, "create_form"));
        if($create_form) {
            $create_form.define("elements", eles[0]);
            $create_form.reconstruct();
        } else {
            log_error("$form create not found: " + build_name(self, "create_form"));
        }

        eles = cols2webix_form_elements(self, cols, "update");
        var $update_form = $$(build_name(self, "update_form"));
        if($update_form) {
            $update_form.define("elements", eles[0]);
            $update_form.reconstruct();
        } else {
            log_error("$form update not found: " + build_name(self, "update_form"));
        }

        // Hemos destruido esquema y por lo tanto los datos, restáuralos
        self.gobj_send_event("EV_UNDO_RECORD", {}, self);

        return 0;
    }

    /********************************************
     *
     ********************************************/
    function ac_show_hook_data(self, event, kw, src)
    {
        var click_x = kw.click_x;
        var click_y = kw.click_y;
        var hook_name = kw.hook_name;
        var id = kw.id;

        var col = get_schema_col(self, hook_name);
        var hook_desc = col.hook;
        var child_topic_name = Object.keys(hook_desc)[0];
        var child_field_name = hook_desc[child_topic_name];

        var kw_hook = {
            treedb_name: self.config.treedb_name,
            parent_topic_name: self.config.topic_name,
            child_topic_name: child_topic_name,
            child_field_name: child_field_name,
            child_field_value: id,
            click_x: click_x,
            click_y: click_y
        };

        return self.gobj_publish_event("EV_SHOW_HOOK_DATA", kw_hook, self);
    }

    /********************************************
     *  From internal toolbar
     ********************************************/
    var update_check_invalid_fields = false;
    function ac_update_record(self, event, kw, src)
    {
        var $form = $$(build_name(self, "update_form"));
        if($form.validate()) {
            if(update_check_invalid_fields) {
                log_info("");
                update_check_invalid_fields = false;
            }
            var btn = $$(build_name(self, "update_record"));
            webix.html.removeCss(btn.getNode(), "icon_color_submmit");
            btn = $$(build_name(self, "undo_record"));
            webix.html.removeCss(btn.getNode(), "icon_color_cancel");
            var new_kw = filter_dict(kw, self.config._writable_fields);

            if(self.config.with_webix_id) {
                new_kw["id"] = kw["id_"];
                delete new_kw["id_"];
            }

            try {
                new_kw = record2backend(self, new_kw, "update");
            } catch (e) {
                log_warning(e);
                throw e;
            }

            self.gobj_publish_event(
                "EV_UPDATE_RECORD",
                {
                    topic_name: self.config.topic_name,
                    record: new_kw
                }
            );
            //$form.save(); update asynchronously by backend
        } else {
            update_check_invalid_fields = true;
            log_warning(t("check invalid fields"));
            return -1;
        }
        return 0;
    }

    /********************************************
     *  From internal toolbar
     ********************************************/
    function ac_delete_record(self, event, kw, src)
    {
        var $form = $$(build_name(self, "update_form"));
        var btn = $$(build_name(self, "update_record"));
        webix.html.removeCss(btn.getNode(), "icon_color_submmit");
        btn = $$(build_name(self, "undo_record"));
        webix.html.removeCss(btn.getNode(), "icon_color_cancel");
        var new_kw = filter_dict(kw, self.config._writable_fields);

        if(self.config.with_webix_id) {
            new_kw["id"] = kw["id_"];
            delete new_kw["id_"];
        }

        try {
            new_kw = record2backend(self, new_kw, "delete");
        } catch (e) {
            log_warning(e);
            throw e;
        }

        self.gobj_publish_event(
            "EV_DELETE_RECORD",
            {
                topic_name: self.config.topic_name,
                record: new_kw
            }
        );
        return 0;
    }

    /********************************************
     *  From internal toolbar
     ********************************************/
    function ac_undo_record(self, event, kw, src)
    {
        var $form = $$(build_name(self, "update_form"));
        if(update_check_invalid_fields) {
            log_info("");
            update_check_invalid_fields = false;
        }

        $form.clearValidation();

        var $table = $$(build_name(self, "list_table"));
        var id = $table.getSelectedId();
        if(id) {
            $table.unselectAll();
            $table.select(id);
            if($table.isVisible()) {
                $table.showItem(id);
            }
        }
        var btn = $$(build_name(self, "update_record"));
        webix.html.removeCss(btn.getNode(), "icon_color_submmit");
        btn = $$(build_name(self, "undo_record"));
        webix.html.removeCss(btn.getNode(), "icon_color_cancel");

        return 0;
    }

    /********************************************
     *  From internal toolbar
     ********************************************/
    var create_check_invalid_fields = false;
    function ac_create_record(self, event, kw, src)
    {
        var $form = $$(build_name(self, "create_form"));
        if($form.validate()) {
            if(create_check_invalid_fields) {
                log_info("");
                create_check_invalid_fields = false;
            }
            var new_kw = filter_dict(kw, self.config._writable_fields);

            if(self.config.with_webix_id) {
                new_kw["id"] = kw["id_"];
                delete new_kw["id_"];
            }

            try {
                new_kw = record2backend(self, new_kw, "create");
            } catch (e) {
                log_warning(e);
                throw e;
            }

            self.gobj_publish_event(
                "EV_CREATE_RECORD",
                {
                    topic_name: self.config.topic_name,
                    record: new_kw
                }
            );

            var btn = $$(build_name(self, "create_record"));
            webix.html.removeCss(btn.getNode(), "icon_color_submmit");
            btn = $$(build_name(self, "discard_record"));
            webix.html.removeCss(btn.getNode(), "icon_color_cancel");
            $form.clearValidation();
            //$form.clear(); // Don't clear the form, user can use it to create new record

            //$form.save(); new asynchronously by backend
        } else {
            create_check_invalid_fields = true;
            log_warning(t("check invalid fields"));
        }
        return 0;
    }

    /********************************************
     *  From internal toolbar
     ********************************************/
    function ac_discard_record(self, event, kw, src)
    {
        var $form = $$(build_name(self, "create_form"));
        if(create_check_invalid_fields) {
            log_info("");
            create_check_invalid_fields = false;
        }

        $form.clearValidation();
        $form.clear();

        var btn = $$(build_name(self, "create_record"));
        webix.html.removeCss(btn.getNode(), "icon_color_submmit");
        btn = $$(build_name(self, "discard_record"));
        webix.html.removeCss(btn.getNode(), "icon_color_cancel");

        // need refresh?
        return 0;
    }

    /********************************************
     *
     ********************************************/
    function ac_list_mode(self, event, kw, src)
    {
        self.config.current_mode = "list";

        $$(build_name(self, "list_table")).show();

        return 0;
    }

    /********************************************
     *
     ********************************************/
    function ac_update_mode(self, event, kw, src)
    {
        self.config.current_mode = "update";

        $$(build_name(self, "update_form_window")).show();

        return 0;
    }

    /********************************************
     *
     ********************************************/
    function ac_create_mode(self, event, kw, src)
    {
        self.config.current_mode = "create";

        $$(build_name(self, "create_form_window")).show();

        return 0;
    }

    /********************************************
     *
     ********************************************/
    function ac_first_record(self, event, kw, src)
    {
        var $table = $$(build_name(self, "list_table"));
        var id = $table.getFirstId();
        if(id) {
            $table.select(id);
            if($table.isVisible()) {
                $table.showItem(id);
            }
        }

        return 0;
    }

    /********************************************
     *
     ********************************************/
    function ac_previous_page(self, event, kw, src)
    {
        if(self.config.page_size>0) {
            var page = self.config.page;
            page--;
            if(page > 0) {
                self.gobj_send_event("EV_PAGE", {page:page}, self);
            }
        }
        return 0;
    }

    /********************************************
     *
     ********************************************/
    function ac_previous_record(self, event, kw, src)
    {
        var $table = $$(build_name(self, "list_table"));
        var id = $table.getPrevId($table.getSelectedId());
        if(id) {
            $table.select(id);
            if($table.isVisible()) {
                $table.showItem(id);
            }
        }
        return 0;
    }

    /********************************************
     *
     ********************************************/
    function ac_next_record(self, event, kw, src)
    {
        var $table = $$(build_name(self, "list_table"));
        var id = $table.getNextId($table.getSelectedId());
        if(id) {
            $table.select(id);
            if($table.isVisible()) {
                $table.showItem(id);
            }
        }
        return 0;
    }

    /********************************************
     *
     ********************************************/
    function ac_next_page(self, event, kw, src)
    {
        if(self.config.page_size>0) {
            var page = self.config.page;
            page++;
            if(page <= self.config.total/self.config.page_size) {
                self.gobj_send_event("EV_PAGE", {page:page}, self);
            }
        }
        return 0;
    }

    /********************************************
     *
     ********************************************/
    function ac_last_record(self, event, kw, src)
    {
        var $table = $$(build_name(self, "list_table"));
        var id = $table.getLastId();
        if(id) {
            $table.select(id);
            if($table.isVisible()) {
                $table.showItem(id);
            }
        }
        return 0;
    }

    /********************************************
     *
     ********************************************/
    function ac_record_by_index(self, event, kw, src)
    {
        var $table = $$(build_name(self, "list_table"));
        var idx = kw.record_idx -1;

        if(idx >= 0 && idx < self.config.total) {
            let id = $table.getIdByIndex(idx);
            $table.select(id);
        } else {
            let id = $table.getSelectedId();
            $table.unselectAll();
            $table.select(id);
        }
        if($table.isVisible()) {
            $table.showItem(id);
        }
        return 0;
    }

    /********************************************
     *
     ********************************************/
    function ac_record_by_id(self, event, kw, src)
    {
        var id =  kw.id;

        var $table = $$(build_name(self, "list_table"));

        var idx = $table.getIndexById(id);
        if(idx < 0) {
            id = $table.getFirstId();
            $table.select(id);
        } else {
            $table.select(id);
        }
        if($table.isVisible()) {
            $table.showItem(id);
        }
        return 0;
    }

    /********************************************
     *
     ********************************************/
    function ac_page(self, event, kw, src)
    {
        var $table = $$(build_name(self, "list_table"));
        var page = kw.page;

        var idx = ((page-1) * self.config.page_size);

        if(idx >= 0 && idx < self.config.total) {
            let id = $table.getIdByIndex(idx);
            self.config.page = page;
            $table.select(id);
        } else {
            let id = $table.getSelectedId();
            $table.unselectAll();
            $table.select(id);
        }
        if($table.isVisible()) {
            $table.showItem(id);
        }
        return 0;
    }

    /********************************************
     *
     ********************************************/
    function ac_rebuild_table(self, event, kw, src)
    {
        /*
         *  Parameters can be passed here as event's attrs
         *  or with previously attributes writing
         */
        if(kw_has_key(kw, "title")) {
            self.config.title = kw_get_str(kw, "title", "", 0);
        }
        if(kw_has_key(kw, "topic_name")) {
            self.config.topic_name = kw_get_str(kw, "topic_name", "", 0);
        }
        if(kw_has_key(kw, "cols")) {
            self.config.cols = self.config.schema = kw_get_dict_value(kw, "cols", [], 0);
        } else if(kw_has_key(kw, "schema")) {
            self.config.cols = self.config.schema = kw_get_dict_value(kw, "schema", [], 0);
        }
        build_table(self);
        return 0;
    }

    /********************************************
     *
     ********************************************/
    function ac_clear_data(self, event, kw, src)
    {
        var $table = $$(build_name(self, "list_table"));
        if(json_object_size(kw)==0) {
            $table.clearAll();
        } else {
            var collection = webix_dt_collect($table, kw);
            $table.remove(collection);
        }

        self.gobj_send_event("EV_UNDO_RECORD", {}, self);
        self.gobj_send_event("EV_DISCARD_RECORD", {}, self);

        return 0;
    }

    /********************************************
     *  Top toolbar informing of window close
     *  kw
     *      {destroying: true}   Window destroying
     *      {destroying: false}  Window minifying
     ********************************************/
    function ac_close_window(self, event, kw, src)
    {
        kw.treedb_name = self.config.treedb_name;
        kw.topic_name = self.config.topic_name;

        return self.gobj_publish_event(event, kw, self);
    }

    /********************************************
     *
     ********************************************/
    function ac_toggle(self, event, kw, src)
    {
        if(self.config.$ui.isVisible()) {
            self.config.$ui.hide();
        } else {
            self.config.$ui.show();
        }
        return self.config.$ui.isVisible();
    }

    /********************************************
     *
     ********************************************/
    function ac_show(self, event, kw, src)
    {
        self.config.$ui.show();
        return self.config.$ui.isVisible();
    }

    /********************************************
     *
     ********************************************/
    function ac_hide(self, event, kw, src)
    {
        self.config.$ui.hide();
        return self.config.$ui.isVisible();
    }

    /********************************************
     *
     ********************************************/
    function ac_select(self, event, kw, src)
    {
        return 0;
    }

    /*************************************************************
     *  Refresh, order from container
     *  provocado por entry/exit de fullscreen
     *  o por redimensionamiento del panel, propio o de hermanos
     *************************************************************/
    function ac_refresh(self, event, kw, src)
    {
        return 0;
    }

    /********************************************
     *  "Container Panel"
     *  Order from container (parent): re-create
     ********************************************/
    function ac_rebuild_panel(self, event, kw, src)
    {
        rebuild(self);
        return 0;
    }




            /***************************
             *      GClass/Machine
             ***************************/




    var FSM = {
        "event_list": [
            "EV_LOAD_DATA",
            "EV_DELETE_DATA",
            "EV_CLEAR_DATA",
            "EV_GET_DATA",
            "EV_GET_CHECKED_DATA",
            "EV_UPDATE_OPTIONS",

            "EV_CREATE_RECORD: output",
            "EV_UPDATE_RECORD: output",
            "EV_DELETE_RECORD: output",
            "EV_REFRESH_TABLE: output",
            "EV_ROW_CHECKED: output",
            "EV_ROW_SELECTED: output",
            "EV_CLOSE_WINDOW: output",
            "EV_SHOW_HOOK_DATA: output",

            "EV_UNDO_RECORD",
            "EV_DISCARD_RECORD",
            "EV_LIST_MODE",
            "EV_UPDATE_MODE",
            "EV_CREATE_MODE",
            "EV_FIRST_RECORD",
            "EV_PREVIOUS_PAGE",
            "EV_PREVIOUS_RECORD",
            "EV_NEXT_RECORD",
            "EV_NEXT_PAGE",
            "EV_LAST_RECORD",
            "EV_RECORD_BY_INDEX",
            "EV_RECORD_BY_ID",
            "EV_PAGE",
            "EV_REBUILD_TABLE",
            "EV_TOGGLE",
            "EV_SHOW",
            "EV_HIDE",
            "EV_SELECT",
            "EV_REFRESH",
            "EV_REBUILD_PANEL"
        ],
        "state_list": [
            "ST_IDLE"
        ],
        "machine": {
            "ST_IDLE":
            [
                ["EV_LOAD_DATA",                ac_load_data,               undefined],
                ["EV_DELETE_DATA",              ac_delete_data,             undefined],
                ["EV_CLEAR_DATA",               ac_clear_data,              undefined],
                ["EV_GET_DATA",                 ac_get_data,                undefined],
                ["EV_GET_CHECKED_DATA",         ac_get_checked_data,        undefined],
                ["EV_UPDATE_OPTIONS",           ac_update_options,          undefined],
                ["EV_SHOW_HOOK_DATA",           ac_show_hook_data,          undefined],
                ["EV_UPDATE_RECORD",            ac_update_record,           undefined],
                ["EV_DELETE_RECORD",            ac_delete_record,           undefined],
                ["EV_UNDO_RECORD",              ac_undo_record,             undefined],
                ["EV_CREATE_RECORD",            ac_create_record,           undefined],
                ["EV_DISCARD_RECORD",           ac_discard_record,          undefined],
                ["EV_LIST_MODE",                ac_list_mode,               undefined],
                ["EV_UPDATE_MODE",              ac_update_mode,             undefined],
                ["EV_CREATE_MODE",              ac_create_mode,             undefined],
                ["EV_FIRST_RECORD",             ac_first_record,            undefined],
                ["EV_PREVIOUS_PAGE",            ac_previous_page,           undefined],
                ["EV_PREVIOUS_RECORD",          ac_previous_record,         undefined],
                ["EV_NEXT_RECORD",              ac_next_record,             undefined],
                ["EV_NEXT_PAGE",                ac_next_page,               undefined],
                ["EV_LAST_RECORD",              ac_last_record,             undefined],
                ["EV_RECORD_BY_INDEX",          ac_record_by_index,         undefined],
                ["EV_RECORD_BY_ID",             ac_record_by_id,            undefined],
                ["EV_PAGE",                     ac_page,                    undefined],
                ["EV_REBUILD_TABLE",            ac_rebuild_table,           undefined],
                ["EV_CLOSE_WINDOW",             ac_close_window,            undefined],
                ["EV_TOGGLE",                   ac_toggle,                  undefined],
                ["EV_SHOW",                     ac_show,                    undefined],
                ["EV_HIDE",                     ac_hide,                    undefined],
                ["EV_SELECT",                   ac_select,                  undefined],
                ["EV_REFRESH",                  ac_refresh,                 undefined],
                ["EV_REBUILD_PANEL",            ac_rebuild_panel,           undefined]
            ]
        }
    };

    var Ui_treedb_formtable = GObj.__makeSubclass__();
    var proto = Ui_treedb_formtable.prototype; // Easy access to the prototype
    proto.__init__= function(name, kw) {
        GObj.prototype.__init__.call(
            this,
            FSM,
            CONFIG,
            name,
            "Ui_treedb_formtable",
            kw,
            0
        );
        return this;
    };
    gobj_register_gclass(Ui_treedb_formtable, "Ui_treedb_formtable");




            /***************************
             *      Framework Methods
             ***************************/




    /************************************************
     *      Framework Method create
     ************************************************/
    proto.mt_create = function(kw)
    {
        var self = this;

        var subscriber = self.gobj_read_attr("subscriber");
        if(!subscriber)
            subscriber = self.gobj_parent();
        self.gobj_subscribe_event(null, null, subscriber);

        rebuild(self);

        if(!empty_string(self.config.treedb_name)) {
            treedb_register_formtable(
                self.config.treedb_name,
                self.config.topic_name,
                self
            );
        }
    };

    /************************************************
     *      Framework Method destroy
     *      In this point, all childs
     *      and subscriptions are already deleted.
     ************************************************/
    proto.mt_destroy = function()
    {
        var self = this;

        if(!empty_string(self.config.treedb_name)) {
            treedb_unregister_formtable(
                self.config.treedb_name,
                self.config.topic_name,
                self
            );
        }
        if(self.config.$ui) {
            self.config.$ui.destructor();
            self.config.$ui = 0;
        }
        if(self.config.resizing_event_id) {
            webix.eventRemove(self.config.resizing_event_id);
            self.config.resizing_event_id = 0;
        }
    };

    /************************************************
     *      Framework Method start
     ************************************************/
    proto.mt_start = function(kw)
    {
        var self = this;

    };

    /************************************************
     *      Framework Method stop
     ************************************************/
    proto.mt_stop = function(kw)
    {
        var self = this;

    };

    //=======================================================================
    //      Expose the class via the global object
    //=======================================================================
    exports.Ui_treedb_formtable = Ui_treedb_formtable;

})(this);
