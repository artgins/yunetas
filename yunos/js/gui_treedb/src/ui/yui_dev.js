/***********************************************************************
 *          ui_dev.js
 *
 *          Development Tools
 *
 *          Copyright (c) 2024, ArtGins.
 *          All Rights Reserved.
 ***********************************************************************/
import {
    __yuno__,
    gobj_find_service,
    log_error,
    is_string,
    createElement2,
    kw_get_local_storage_value,
    kw_set_local_storage_value,
    gobj_write_attr,
    gobj_create_service,
    trace_json,
} from "yunetas";

import i18next from 'i18next';

import { JSONEditor } from 'vanilla-jsoneditor';
import "vanilla-jsoneditor/themes/jse-theme-dark.css";

/************************************************************
 *
 ************************************************************/
function info_traffic(title, msg, direction, size)
{
    if(!gobj_find_service("Developer-Window")) {
        trace_json(msg);
        return;
    }

    if(!size) {
        size = 0;
    }

    let jn_msg;
    try {
        if(is_string(msg)) {
            jn_msg = JSON.parse(msg);
        } else {
            jn_msg = JSON.parse(JSON.stringify(msg));
        }
    } catch (e) {
        return;
    }

    let content = {
        text: undefined,
        json: jn_msg
    };

    function formatCurrentTime() {
        let now = new Date();

        // Pad single digit numbers with a leading zero
        let pad = (num, size) => ('000' + num).slice(size * -1);

        let hours = pad(now.getHours(), 2);
        let minutes = pad(now.getMinutes(), 2);
        let seconds = pad(now.getSeconds(), 2);
        let milliseconds = pad(now.getMilliseconds(), 4);

        // Format to hh:mm:ss .SSSS
        return `${hours}:${minutes}:${seconds} .${milliseconds}`;
    }

    let element = document.getElementById('developer-traffic-logger');
    if(element) {
        let style = "background-color:#3883FA;";
        if(direction === 2) {
            style += "color:yellow;";
        } else if(direction === 3) {
            style += "color:red;";
        } else {
            style += "color:white;";
        }

        let $item = createElement2(
            ['div', {class: 'mt-4'}, [
                ['div', {class: 'is-flex with-border is-justify-content-space-between', style: style}, [
                    ['div', {class: 'p-1'}, title],
                    ['div', {class: 'p-1'}, `(${size} bytes)`],
                    ['div', {class: 'p-1'}, formatCurrentTime()]
                ]],
                ['div', {class: 'x-jsoneditor jse-theme-dark'}, []],
            ]]
        );
        let $target = $item.querySelector('.x-jsoneditor');
        let font_family = "DejaVu Sans Mono, monospace, consolas, monaco";
        let sz = 15;
        $target.style.setProperty('--jse-font-size-mono', sz + 'px');
        $target.style.setProperty('--jse-font-family-mono', font_family);

        document.getElementById("developer-traffic-logger").appendChild($item);

        let editor = new JSONEditor({
            target: $target,
            props: {
                content: content,
                readOnly: true,
                timestampTag: function ({field, value, path}) {
                    if (field === '__t__' || field === '__tm__' || field === 'tm' ||
                        field === 'from_t' || field === 'to_t' || field === 't' ||
                        field === 'from_tm' || field === 'to_tm' || field === 'time'
                    ) {
                        return true;
                    }
                    return false;
                },
                timestampFormat: function ({field, value, path}) {
                    if (field === '__t__' || field === '__tm__' || field === 'tm' ||
                        field === 'from_t' || field === 'to_t' || field === 't' ||
                        field === 'from_tm' || field === 'to_tm' || field === 'time'
                    ) {
                        return new Date(value * 1000).toISOString();
                    }
                    return null;
                },
            }
        });
        editor.expand(path => path.length < 2);

        element.scrollIntoView({block: "end"});
    }
}

/************************************************************
 *
 ************************************************************/
function trace_traffic()
{
    let v = kw_get_local_storage_value("trace_traffic");
    v = Number(v);
    if(v) {
        gobj_write_attr(__yuno__, "trace_inter_event", false);
        v = 0;
    } else {
        gobj_write_attr(__yuno__, "trace_inter_event", true);
        gobj_write_attr(__yuno__, "trace_ievent_callback", info_traffic);
        v = 1;
    }
    kw_set_local_storage_value("trace_traffic", v);
    info_user();
}

/************************************************************
 *
 ************************************************************/
function trace_automata()
{
    let v = kw_get_local_storage_value("trace_automata");
    v = Number(v);
    if(v===0) {
        v = 1;
    } else if(v===1) {
        v = 2;
    } else {
        v = 0;
    }
    gobj_write_attr(__yuno__, "tracing", v);
    kw_set_local_storage_value("trace_automata", v);
    info_user();
}

/************************************************************
 *
 ************************************************************/
function trace_creation()
{
    let v = kw_get_local_storage_value("trace_creation");
    v = Number(v);
    if(v===0) {
        v = 1;
    } else {
        v = 0;
    }
    gobj_write_attr(__yuno__, "trace_creation", v);
    kw_set_local_storage_value("trace_creation", v);
    info_user();
}

/************************************************************
 *
 ************************************************************/
function trace_i18n()
{
    let v = kw_get_local_storage_value("trace_i18n");
    v = Number(v);
    if(v===0) {
        v = 1;
    } else {
        v = 0;
    }
    i18next.options.debug = v?true:false;
    kw_set_local_storage_value("trace_i18n", v);
    info_user();
}

/************************************************************
 *
 ************************************************************/
function set_no_poll()
{
    let v = kw_get_local_storage_value("no_poll");
    v = Number(v);
    if(v) {
        v = 0;
    } else {
        v = 1;
    }
    gobj_write_attr(__yuno__, "no_poll", v);
    kw_set_local_storage_value("no_poll", v);
    info_user();
}

/************************************************************
 *
 ************************************************************/
function info_user()
{
    let $info = document.getElementById("developer-window-info");

    let traffic = Number(kw_get_local_storage_value("trace_traffic", 0, false));
    let trace = Number(kw_get_local_storage_value("trace_automata", 0, false));
    let creation = Number(kw_get_local_storage_value("trace_creation", 0, false));
    let i18n = Number(kw_get_local_storage_value("trace_i18n", 0, false));
    let no_poll = Number(kw_get_local_storage_value("no_poll", 0, false));

    // Code repeated
    let estados = `
        <div>tracing automata: ${trace}</div>
        <div>tracing creation: ${creation}</div>
        <div>tracing i18n: ${i18n}</div>
        <div>traffic: ${traffic}</div>
        <div>no_poll: ${no_poll}</div>`;

    $info.innerHTML = estados;
}

/************************************************************
 *
 ************************************************************/
function setup_dev(self, show)
{
    let traffic = Number(kw_get_local_storage_value("trace_traffic", 0, false));
    let trace = Number(kw_get_local_storage_value("trace_automata", 0, false));
    let creation = Number(kw_get_local_storage_value("trace_creation", 0, false));
    let i18n = Number(kw_get_local_storage_value("trace_i18n", 0, false));
    let no_poll = Number(kw_get_local_storage_value("no_poll", 0, false));

    if(show) {
        const $dev_toolbar = createElement2(
            ['div', {class: 'buttons'}, [
                ['button', {
                    class: 'button',
                }, 'Tracing automata', {
                    click: (evt) => {
                        evt.stopPropagation();
                        trace_automata();
                    }
                }],
                ['button', {
                    class: 'button',
                }, 'Tracing creation', {
                    click: (evt) => {
                        evt.stopPropagation();
                        trace_creation();
                    }
                }],
                ['button', {
                    class: 'button',
                }, 'Tracing i18n', {
                    click: (evt) => {
                        evt.stopPropagation();
                        trace_i18n();
                    }
                }],
                ['button', {
                    class: 'button',
                }, 'Traffic', {
                    click: (evt) => {
                        evt.stopPropagation();
                        trace_traffic();
                    }
                }],
                ['button', {
                    class: 'button',
                }, 'No Poll', {
                    click: (evt) => {
                        evt.stopPropagation();
                        set_no_poll();
                    }
                }],
                ['button', {
                    class: 'button',
                }, 'Clear Traffic', {
                    click: (evt) => {
                        evt.stopPropagation();
                        document.getElementById("developer-traffic-logger").innerHTML = "";
                    }
                }],
            ]]
        );

        // TODO repon la position
        // onViewResize: function() {
        //     var record = filter_dict(this.config, self.config.traffic_window_position);
        //     gobj_update_writable_attrs({traffic_window_position: record});
        //     gobj_save_persistent_attrs();
        // },
        // onViewMoveEnd: function() {
        //     var record = filter_dict(this.config, self.config.traffic_window_position);
        //     gobj_update_writable_attrs({traffic_window_position: record});
        //     gobj_save_persistent_attrs();
        // }

        // Code repeated
        let estados = `
        <div>tracing automata: ${trace}</div>
        <div>tracing creation: ${creation}</div>
        <div>tracing i18n: ${i18n}</div>
        <div>traffic: ${traffic}</div>
        <div>no_poll: ${no_poll}</div>`;

        gobj_create_service(
            "Developer-Window",
            "C_YUI_WINDOW",
            {
                $parent: document.getElementById('top-layer'),
                subscriber: null,
                header: $dev_toolbar,
                auto_save_size_and_position: true,
                center: false,
                // resizable: false,
                body: '<div style="overflow:scroll;height:100%;"><div id="developer-traffic-logger" style="margin-left:10px;margin-right:10px;"/></div>',
                footer: `<div id="developer-window-info" class="is-flex is-justify-content-space-between">${estados}</div>`,
                on_close: function() {
                    kw_set_local_storage_value("open_developer_window", 0);
                }
            },
            self
        );

        kw_set_local_storage_value("open_developer_window", 1);

    }

    if(traffic) {
        gobj_write_attr(__yuno__, "trace_inter_event", true);
        gobj_write_attr(__yuno__, "trace_ievent_callback", info_traffic);
    }
    gobj_write_attr(__yuno__, "tracing", trace);
    gobj_write_attr(__yuno__, "no_poll", no_poll);
}

export {info_traffic, setup_dev};
