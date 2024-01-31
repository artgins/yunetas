/***********************************************************************
 *          w2_jsoneditor.js
 *
 *          w2window with a JSONEditor
 *
 *          Copyright (c) 2022 Niyamaka.
 *          All Rights Reserved.
 ***********************************************************************/

/********************************************
 *  Create Json editor
 ********************************************/
function create_json_editor_window(kw)
{
    let name = kw_get_str(kw, "name", kw_get_str(kw, "id", undefined));
    let title = kw_get_str(kw, "title", "JSON Editor");
    let props = kw_get_dict(kw, "props", null, false, true);
    if(!props) {
        return;
    }

    let editor = null;
    let target = null;


    // let new_container = null;
    // if(container) {
    //     target = document.getElementById(container);
    // } else {
    //     let dict = {
    //         id: "pp",
    //         x: 150,
    //         y: 50,
    //         width: 600,
    //         height: 400
    //     }
    //     let template = sprintf(`
    //         <div id="%(id)s" class="my-json-editor jse-theme-dark" style="z-index: 99; position: absolute; left: %(x)dpx; top: %(y)dpx; width: %(width)dpx; height: %(height)dpx;" ></div>
    //     `, dict);
    //     target = new_container = htmlToElement(template);
    //     document.body.appendChild(target);
    // }

    let w2 = new w2window({
        name: name,
        title: title,
        showMax: true,
        center: true,
        modal: false,
        width: 600,
        height: 500,
        // text: "", // this has preference over body, it's a simple centered text
        // body: `<div id="jse-${name}"  class="my-json-editor jse-theme-dark" style="position: absolute; left: 0px; top: 0px; right: 0px; bottom: 0px;"></div>`,

        onClose(ev) {
            if(editor && editor.destroy) {
                editor.destroy();
            }
        },
        onMoving(ev) {
            // console.log("moving")
            // console.dir(ev.detail.window_rect);
        },
        onMoved(ev) {
            // console.log("moved")
            // console.dir(ev.detail.window_rect);
        },
        onResizing(ev) {
            // console.log("resizing")
            // console.dir(ev.detail.window_rect);
        },
        onResized(ev) {
            // console.log("resized")
            // console.dir(ev.detail.window_rect);
            // console.dir(ev.detail.window_body);
        },

        // buttons: "<button> Pepe </button>", // Other option, preference over actions
        actions: {
            // function name is used for button text
            Ok(ev) {
                // do something
                this.close();
            },
            Cancel(ev) {
                this.close();
            },
            // custom button, when you can define text and class
            custom: {
                text: "Other Button",
                class: "w2ui-btn-blue",
                style: "color: yellow",
                onClick(ev) {
                    console.log("button clicked");
                }
            }
        }
    });

    // target = query(`.my-json-editor`); // other option
    // target = query(`#jse-${name}`);
    target = w2.get_container();
    if(target.length > 0) {
        target.addClass("jse-theme-dark");

        let font_family = "DejaVu Sans Mono, monospace";
        let sz = adjust_font_size(16, font_family);
        // const root = document.querySelector(':root');
        const root = target[0];
        root.style.setProperty('--jse-font-size-mono', sz + 'px');
        root.style.setProperty('--jse-font-family-mono', font_family);

        editor = new JSONEditor({
            target: target[0],
            props: props
        });
    }

//
//     timestampTag: function({field, value, path}) {
//         if (field === '__t__' || field === '__tm__' || field === 'tm' ||
//             field === 'from_t' || field === 'to_t' || field === 't' ||
//             field === 't_input' || field === 't_output' ||
//             field === 'from_tm' || field === 'to_tm' || field === 'time'
//         ) {
//             return true;
//         }
//         return false;
//     },
//     timestampFormat: function({field, value, path}) {
//         if (field === '__t__' || field === '__tm__' || field === 'tm' ||
//             field === 'from_t' || field === 'to_t' || field === 't' ||
//             field === 't_input' || field === 't_output' ||
//             field === 'from_tm' || field === 'to_tm' || field === 'time'
//         ) {
//             return new Date(value*1000).toISOString();
//         }
//         return null;
//     },
// //                 onEditable: function({path, field, value}) {
// //                     return self.gobj_send_event(
// //                         "JE_IS_FIELD_EDITABLE",
// //                         {
// //                             path: path,
// //                             field: field,
// //                             value: value
// //                         },
// //                         self
// //                     );
// //                 },
//     onEvent: function(node, event) {
//         if(event.type=="click") {
//             self.gobj_send_event(
//                 "JE_CLICK",
//                 {
//                     path: node.path,
//                     field: node.field,
//                     value: node.value?node.value:undefined
//                 },
//                 self
//             );
//         }
//     }
//     // onClassName: WARNING no lo uses, demasiados eventos si es un json grande
//

    return w2;
}

/********************************************
 *  Destroy Json editor
 ********************************************/
function destroy_json_editor(editor)
{
    const w2 = editor.w2;
    editor.destroy();
    if(w2) {
        w2.destroy();
    }
}
