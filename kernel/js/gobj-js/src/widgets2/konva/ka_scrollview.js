/***********************************************************************
 *          Ka_scrollview.js
 *
 *          Scrollview built with konvA
 *
 *          Based in Konva
 *

Group(_ka_container)
    |
    |----> Rect(_ka_border_rect)                // with stroke and shadow (or no)
    |
    |----> Rect(_ka_viewport_rect)              // no stroke/shadow
    |
    |----> Group(_ka_wrapper_content_group)     // has the clip
    |   |
    |   |----> Group(_ka_content_group)         // contains all items
    |       |
    |       |----> Rect(_ka_content_rect)       // no stroke/shadow, update to size of all group's items,
    |       |                                   // and has the background color
    |       |----> (items)
    |       ....
    |
    |----> Group(_ka_scrollbars_group)
        |
        |----> Rect(_ka_vscroll_rect)           // Vertical scrollbar
        |
        |----> Rect(_ka_hscroll_rect)           // Horizontal scrollbar


 *          Copyright (c) 2022 Niyamaka.
 *          All Rights Reserved.
 ***********************************************************************/
(function (exports) {
    'use strict';

    /********************************************
     *  Configuration (C attributes)
     *  Attributes without underscore prefix
     *      will be set in self.config
     *  Attributes with underscore ("_") prefix:
     *      will be set in self.private
     ********************************************/
    let CONFIG = {
        //////////////// Public Attributes //////////////////
        subscriber: null,   // subscriber of publishing messages (Child model: if null will be the parent)
        layer: null,        // Konva layer

        modal: false,       // Activation SERVICE: Default false, like webix
        super_modal: false, // Activation SERVICE: Don't close with escape key or clicking out
        autoclose: false,   // Activation SERVICE: Close window on pointerup bubbling to stage or Esc key

        x: undefined,
        y: undefined,
        width: 300,
        height: 300,
        padding: 10,
        background_color: "white",

        visible: true,
        panning: true,             // Enable (inner dragging) panning, default true
        draggable: false,           // Enable (outer dragging) dragging, default false

        autosize: false,            // Change dimension to fix all content visible
        fix_dimension_to_screen: false,  // Change dimension to fix window inside screen
        center: false,              // Change position to screen center
        show_overflow: false,       // True for don't clip the content in the view

        top_on_activate: false,     // Move node to the top of its siblings on activation

        enable_vscroll: true,       // Enable content vertical scrolling, default true
        enable_hscroll: true,       // Enable content horizontal scrolling, default true
        scroll_by_step: false,      // false: use native; true: use child dimension; number: use 'number' step;

        kw_border_shape: { /* Border shape */
            strokeWidth: 4,
            stroke: "black",
            opacity: 1,
            shadowBlur: 0,
            shadowColor: "black",
            shadowForStrokeEnabled: false   // HTML5 Canvas Optimizing Strokes Performance Tip
        },
        kw_border_shape_actived: { /* Border shape for active windows */
            // Only used: stroke, opacity, shadowBlur, shadowColor
        },

        /*
         *  The scrollbars are drawn on top of content instead of taking up space.
         */
        hide_hscrollbar: false,     // Don't show horizontal (auto) scrollbar
        hide_vscrollbar: false,     // Don't show vertical (auto) scrollbar
        scrollbar_min_large: 12,    // for the height if vertical or the width if horizontal
        scrollbar_width: 10,        // width of scrollbars (horizontal and vertical)
        scrollbar_padding: 0,       // padding in extremes of scrollbars

        kw_scrollview_scrollbar_rect: { /* Scrollbars rect */
            fill: "gray",   // background color of scrollbars
            opacity: 0.8
        },

        items: [],

        quick_display: false,       // For debugging, draw quickly

        //////////////// Private Attributes /////////////////
        _ka_container: null,                // the group with all elements of scrollview
        _ka_border_rect: null,
        _ka_viewport_rect: null,
        _ka_wrapper_content_group: null,
        _ka_content_group: null,
        _ka_content_rect: null,
        _ka_scrollbars_group: null,
        _ka_vscroll_rect: null,
        _ka_hscroll_rect: null,
        _vProportion_scroll2content: null,
        _hProportion_scroll2content: null,
        _vProportion_content2scroll: null,
        _hProportion_content2scroll: null,

        _strokeW: 0,
        _shadowW: 0,
        _padding: 0,

        _x: 0,  // auto x,y in undefined case
        _y: 0,

        _original_x: null,
        _original_y: null,
        _original_width: null,
        _original_height: null,

        _util_width: null,
        _util_height: null
    };




            /***************************
             *      Local Methods
             ***************************/




    /************************************************************
     *  HACK stroke suma la mitad, shadow suma su ancho
     *  Ej: { x:0, y:0, width: 200, height:200 }
     *     strokeWidth: 10,
     *     shadowBlur: 20
     *  ==> { x: -25, y: -25, width: 250, height: 250 }
     *  So:
     *      x/y decremented by:  -= (strokeWidth/2 + shadowBlur)
     *      w/h incremented by:  += (strokeWidth + shadowBlur*2)
     *  Blur se sitúa fuera del rectangle,
     *  pero stroke se centra, la mitad fuera la mitad dentro.
     ************************************************************/
    function create_scrollview(self, create)
    {
        /*-----------------------------------------*
         *  ka_container is the group with
         *  all elements of scrollview
         *-----------------------------------------*/
        let ka_container;
        if(create) {
            ka_container = self.private._ka_container = new Konva.Group({
                id: self.gobj_short_name(),
                name: "ka_container",
                x: kw_get_int(self.config, "x", get_auto_x(self), false, false),
                y: kw_get_int(self.config, "y", get_auto_y(self), false, false),
                width: self.config.width,
                height: self.config.height,
                visible: self.config.visible,
                draggable: self.config.draggable,
                listening: true
            });

            if(self.config.layer) {
                // It can be added later to some container
                self.config.layer.add(ka_container);
            }

            ka_container.gobj = self; // cross-link
        } else {
            ka_container = self.private._ka_container;
            ka_container.position({x: self.config.x, y: self.config.y});
            ka_container.size({width: self.config.width, height: self.config.height});
        }

        /*----------------------------------------------------------------------*
         *      Dimension of insider rectangles
         *  All must be inside (config.width,config.height): stroke and shadow
         *----------------------------------------------------------------------*/
        let x = 0;
        let y = 0;
        let width = self.config.width;
        let height = self.config.height;

        /*-----------------------------------------------------*
         *      Border Rect
         *      decrease stroke and shadow
         *-----------------------------------------------------*/
        let kw_border_shape = __duplicate__(
            kw_get_dict(self.config, "kw_border_shape", {}, false, false)
        );
        let strokeW = self.private._strokeW = kw_get_int(
            kw_border_shape, "strokeWidth", 0, false, false
        );
        let shadowW = self.private._shadowW = kw_get_int(
            kw_border_shape, "shadowBlur", 0, false, false
        );

        if(strokeW > 0) {
            // It's half part
            x += strokeW/2;
            y += strokeW/2;
            width -= strokeW;
            height -= strokeW;
        }
        if(shadowW > 0) {
            x += shadowW;
            y += shadowW;
            width -= shadowW*2;
            height -= shadowW*2;
        }
        json_object_update(
            kw_border_shape,
            {
                id: self.gobj_short_name() + "-1",
                name: "ka_border_rect",
                x: x,
                y: y,
                width: width,
                height: height,
                // HACK listening: false, // set to false the dragging will not run, MERDE!!
                fill: kw_get_str(self.config, "background_color", null, false, false)
            }
        );
        if(create) {
            let ka_border_rect = self.private._ka_border_rect = new Konva.Rect(kw_border_shape);
            ka_container.add(ka_border_rect);
        } else {
            self.private._ka_border_rect.position({x: x, y: y});
            self.private._ka_border_rect.size({width: width, height: height});
        }
        if(self.config.quick_display) {
            ka_container.draw();
        }

        /*-----------------------------------------------------*
         *      Decrease Padding
         *-----------------------------------------------------*/
        let padding = self.private._padding = kw_get_int(self.config, "padding", 0, false, false);
        if(padding > 0) { // Decrease padding width
            x += padding;
            y += padding;
            width -= padding*2;
            height -= padding*2;
        }
        if(strokeW > 0) {
            // It's half part
            x += strokeW/2;
            y += strokeW/2;
            width -= strokeW;
            height -= strokeW;
        }

        /*-----------------------------------------------------*
         *      Viewport Rect (child of ka_container)
         *-----------------------------------------------------*/
        let kw_viewport_rect = {
            id: self.gobj_short_name() + "-2",
            name: "ka_viewport_rect",
            x: x,
            y: y,
            width: width,
            height: height,
            strokeWidth: 0,
            shadowBlur: 0,
            strokeEnabled: false,
            shadowEnabled: false
        };

        if(create) {
            let ka_viewport_rect = self.private._ka_viewport_rect = new Konva.Rect(kw_viewport_rect);
            ka_container.add(ka_viewport_rect);
        } else {
            self.private._ka_viewport_rect.position({x: x, y: y});
            self.private._ka_viewport_rect.size({width: width, height: height});
        }
        if(self.config.quick_display) {
            ka_container.draw();
        }

        /*---------------------------------------------------------------------*
         *          Content
         *  ka_content_group contains:
         *
         *  - ka_wrapper_content_group (child of ka_container)
         *      - contains the clip of viewport (if no show_overflow)
         *
         *  - ka_content_group (child of ka_wrapper_content_group)
         *      - contains all items
         *
         *  - ka_content_rect (child of ka_content_group)
         *      - update to size of all group's items
         *      - has the background color
         *      - without 'stroke' nor 'shadow'
         *
         *---------------------------------------------------------------------*/
        /*--------------------------------------------------------------*
         *  - ka_wrapper_content_group (child of ka_container)
         *      - contains the clip of viewport (if no show_overflow)
         *--------------------------------------------------------------*/
        let ka_wrapper_content_group;
        if(create) {
            ka_wrapper_content_group = self.private._ka_wrapper_content_group = new Konva.Group({
                id: self.gobj_short_name() + "-3",
                name: "ka_wrapper_content_group",
                x: x,
                y: y,
                width: width,
                height: height
            });
            ka_container.add(ka_wrapper_content_group);
        } else {
            ka_wrapper_content_group = self.private._ka_wrapper_content_group;
            ka_wrapper_content_group.position({x: x, y: y});
            ka_wrapper_content_group.size({width: width, height: height});
        }

        self.private._util_width = width;
        self.private._util_height = height;

        if(!self.config.show_overflow) {
            ka_wrapper_content_group.clip({
                x: 0,
                y: 0,
                width: width,
                height: height
            });
        }

        /*-----------------------------------------------------------*
         *  - ka_content_group (child of ka_wrapper_content_group)
         *      - contains all items
         *-----------------------------------------------------------*/
        let ka_content_group;
        if(create) {
            ka_content_group = self.private._ka_content_group = new Konva.Group({
                id: self.gobj_short_name() + "-4",
                name: "ka_content_group",
                x: 0,
                y: 0,
                width: 0,
                height: 0,
                draggable: self.config.panning,
                dragBoundFunc: function(pos) {
                    /*
                     *  HACK: pos - is absolute position of the node
                     *  you should return absolute position too
                     */
                    let new_pos = get_new_pos(self, pos, true);
                    return new_pos;
                }
            });
            ka_wrapper_content_group.add(ka_content_group);
        } else {
            ka_content_group = self.private._ka_content_group;
        }

        /*------------------------------------------------*
         *  - ka_content_rect (child of ka_content_group)
         *      - update to size of all group's items
         *      - has the background color
         *      - without 'stroke' nor 'shadow'
         *------------------------------------------------*/
        let kw_scrollview_content_rect = {
            // id: self.gobj_short_name() + "-5",
            name: "ka_content_rect",
            x: 0,
            y: 0,
            width: self.config.autosize?0:width,
            height: self.config.autosize?0:height,
            strokeWidth: 0,
            shadowBlur: 0,
            strokeEnabled: false,
            shadowEnabled: false,
            fill: kw_get_str(self.config, "background_color", null, false, false)
        };
        let ka_content_rect;
        if(create) {
            ka_content_rect = self.private._ka_content_rect = new Konva.Rect(kw_scrollview_content_rect);
            ka_content_group.add(ka_content_rect);
            ka_content_rect.moveToBottom();
        } else {
            ka_content_rect = self.private._ka_content_rect;

            // Give minimum size to content_rect (util zone)
            ka_content_rect.size({width: width, height: height});

            let content_dim = ka_content_group.getClientRect({relativeTo: ka_content_group.getParent()});
            ka_content_group.size({width: content_dim.width, height: content_dim.height});

            // Give content group size to content_rect
            ka_content_rect.size(ka_content_group.size());
        }
        if(self.config.quick_display) {
            ka_container.draw();
        }

        /*--------------------------------------------------*
         *          Scrollbars group
         *  Group with the vertical/horizontal scrollbars
         *--------------------------------------------------*/
        if(create) {
            let ka_scrollbars_group = self.private._ka_scrollbars_group = new Konva.Group({
                id: self.gobj_short_name() + "-6",
                name: "ka_scrollbars_group",
                x: x,
                y: y,
                width: width,
                height: height,
            });
            ka_container.add(ka_scrollbars_group);
        }

        /*-----------------------------------------*
         *          Events: wheel
         *-----------------------------------------*/
        if(!create) {
            // Events only on create
            return;
        }
        ka_container.off("wheel");
        ka_container.on("wheel", function (ev) {
            /*---------------------------------*
             *    HACK End Screen Moving
             *---------------------------------*/
            ev.cancelBubble = true;
            let pos = self.private._ka_content_group.position();
            let dx = ev.evt.deltaX;
            let dy = ev.evt.deltaY;

            let scroll_by_step = self.config.scroll_by_step;
            if(is_number(scroll_by_step)) {
                if(dx !== 0) {
                    if(dx < 0) {
                        dx = -scroll_by_step;
                    } else {
                        dx = scroll_by_step;
                    }
                }
                if(dy !== 0) {
                    if(dy < 0) {
                        dy = -scroll_by_step;
                    } else {
                        dy = scroll_by_step;
                    }
                }
            } else if(scroll_by_step === true) {
                if(dy !== 0) {
                    let pos_ = get_first_visible_vertical_pos(self);
                    if(pos_) {
                        let step = pos_.height;
                        if(dy < 0) {
                            dy = -step;
                        } else {
                            dy = step;
                        }
                    }
                }
                if(dx !== 0) {
                    let pos_ = get_first_visible_horizontal_pos(self);
                    if(pos_) {
                        let step = pos_.width;
                        if(dx < 0) {
                            dx = -step;
                        } else {
                            dx = step;
                        }
                    }
                }
            } else {
                // do nothing
            }
            pos.x -= dx;
            pos.y -= dy;

            let new_pos = get_new_pos(self, pos, false);

            if(self.config.enable_vscroll) {
                self.private._ka_content_group.y(new_pos.y);
            }
            if(self.config.enable_hscroll) {
                self.private._ka_content_group.x(new_pos.x);
            }
            adjust_screen_position(self);
        });

        /*-----------------------------------------*
         *          Events: drag (moving)
         *-----------------------------------------*/
        // if(self.config.draggable) {
        //     ka_container.on('dragstart', function (ev) {
        //         ev.cancelBubble = true;
        //         self.gobj_publish_event("EV_MOVING", ka_container.position());
        //     });
        //     ka_container.on('dragmove', function (ev) {
        //         ev.cancelBubble = true;
        //         document.body.style.cursor = 'pointer';
        //         self.gobj_publish_event("EV_MOVING", ka_container.position());
        //     });
        //     ka_container.on('dragend', function (ev) {
        //         ev.cancelBubble = true;
        //         document.body.style.cursor = 'default';
        //         self.gobj_publish_event("EV_MOVED", ka_container.position());
        //     });
        // }

        /*-----------------------------------------*
         *          Events: drag (panning)
         *-----------------------------------------*/
        if(self.config.panning) {
            ka_content_group.on('dragstart', function (ev) {
                ev.cancelBubble = true;
                self.gobj_publish_event("EV_PANNING", ka_content_group.position());
            });
            ka_content_group.on('dragmove', function (ev) {
                ev.cancelBubble = true;
                document.body.style.cursor = 'pointer';
                move_scrollbars(self);
                // review self.gobj_publish_event("EV_PANNING", ka_content_group.position());
            });
            ka_content_group.on('dragend', function (ev) {
                ev.cancelBubble = true;
                document.body.style.cursor = 'default';
                adjust_screen_position(self);  // by end of screen drag
                self.gobj_publish_event("EV_PANNED", ka_content_group.position());
            });
        }

        if(create) {
            if(self.private._ka_container.isVisible()) {
                self.gobj_publish_event("EV_SHOWED", {});
            }
        }
    }

    /********************************************
     *
     ********************************************/
    function update_scrollbars(self)
    {
        let ka_viewport_rect = self.private._ka_viewport_rect;
        let ka_content_group = self.private._ka_content_group;
        let ka_scrollbars_group = self.private._ka_scrollbars_group;

        if(self.private._ka_vscroll_rect) {
            self.private._ka_vscroll_rect.off("dragmove");
            self.private._ka_vscroll_rect.off("dragend");
            self.private._ka_vscroll_rect.destroy();
            self.private._ka_vscroll_rect = null;
        }
        if(self.private._ka_hscroll_rect) {
            self.private._ka_hscroll_rect.off("dragmove");
            self.private._ka_hscroll_rect.off("dragend");
            self.private._ka_hscroll_rect.destroy();
            self.private._ka_hscroll_rect = null;
        }

        let viewportWidth = ka_viewport_rect.width();
        let viewportHeight = ka_viewport_rect.height();

        let content_dim = ka_content_group.getClientRect({relativeTo: ka_content_group.getParent()});

        let contentWidth = content_dim.width;
        let contentHeight = content_dim.height;

        let vscroll_lane_size = viewportHeight - self.config.scrollbar_padding;
        let hscroll_lane_size = viewportWidth - self.config.scrollbar_padding;
        let minSize = self.config.scrollbar_min_large;

        let must_display_vbar = self.config.enable_vscroll &&
            !self.config.hide_vscrollbar && (contentHeight > viewportHeight);
        let must_display_hbar = self.config.enable_hscroll &&
            !self.config.hide_hscrollbar && (contentWidth > viewportWidth);

        let ka_hscroll_rect = null;
        let ka_vscroll_rect = null;

        let vscroll_x = viewportWidth + self.config.padding - self.config.scrollbar_width;
        let vscroll_y = self.config.scrollbar_padding;
        let vscroll_max_y;

        let hscroll_x = self.config.scrollbar_padding;
        let hscroll_y = viewportHeight + self.config.padding - self.config.scrollbar_width;
        let hscroll_max_x;

        if(must_display_vbar) {
            let scrollBarLength = Math.max(
                minSize,
                Math.min(
                    viewportHeight * (viewportHeight/(contentHeight)),
                    vscroll_lane_size
                )
            );

            vscroll_max_y = vscroll_lane_size - scrollBarLength;
            ka_vscroll_rect = self.private._ka_vscroll_rect = new Konva.Rect({
                id: self.gobj_short_name() + "-7",
                name: "vScrollBar",
                width: self.config.scrollbar_width,
                height: scrollBarLength,
                fill: self.config.kw_scrollview_scrollbar_rect.fill,
                opacity: self.config.kw_scrollview_scrollbar_rect.opacity,
                x: vscroll_x,
                y: vscroll_y,
                draggable: true,
                dragBoundFunc: function(pos) {
                    /*
                     *  HACK: pos - is absolute position of the node
                     *  you should return absolute position too
                     */
                    let viewport_dim = ka_viewport_rect.getClientRect(); // Don't use global variable

                    let new_pos = {};
                    new_pos.x = vscroll_x + viewport_dim.x;
                    pos.y -= viewport_dim.y - self.config.scrollbar_padding;
                    new_pos.y = Math.max( // Que no baje de la posición mínima
                        vscroll_y,
                        Math.min( // Que no sobrepase la posición máxima
                            pos.y, // posición actual
                            vscroll_max_y // posición máxima
                        )
                    );
                    new_pos.y += viewport_dim.y;
                    return new_pos;
                }
            });
            ka_scrollbars_group.add(ka_vscroll_rect);

            self.private._vProportion_scroll2content = new Proportion( //how it does zimjs
                vscroll_y, vscroll_max_y,
                0, contentHeight - viewportHeight,
                1
            );

            self.private._vProportion_content2scroll = new Proportion( //how it does zimjs
                -(contentHeight - viewportHeight), 0,
                vscroll_max_y, vscroll_y,
                1
            );
        }

        if(must_display_hbar) {
            let scrollBarLength = Math.max(
                minSize,
                Math.min(
                    viewportWidth * (viewportWidth/(contentWidth)),
                    hscroll_lane_size
                )
            );
            hscroll_max_x = hscroll_lane_size - scrollBarLength;

            ka_hscroll_rect = self.private._ka_hscroll_rect = new Konva.Rect({
                id: self.gobj_short_name() + "-8",
                name: "hScrollBar",
                width: scrollBarLength,
                height: self.config.scrollbar_width,
                fill: self.config.kw_scrollview_scrollbar_rect.fill,
                opacity: self.config.kw_scrollview_scrollbar_rect.opacity,
                x: hscroll_x,
                y: hscroll_y,
                draggable: true,
                dragBoundFunc: function(pos) {
                    /*
                     *  HACK: pos - is absolute position of the node
                     *  you should return absolute position too
                     */
                    let viewport_dim = ka_viewport_rect.getClientRect(); // Don't use global variable

                    let new_pos = {};
                    new_pos.y = hscroll_y + viewport_dim.y;
                    pos.x -= viewport_dim.x - self.config.scrollbar_padding;
                    new_pos.x = Math.max( // Que no baje de la posición mínima
                        hscroll_x,
                        Math.min( // Que no sobrepase la posición máxima
                            pos.x, // posición actual
                            hscroll_max_x  // posición máxima
                        )
                    );
                    new_pos.x += viewport_dim.x;
                    return new_pos;
                }
            });
            ka_scrollbars_group.add(ka_hscroll_rect);

            self.private._hProportion_scroll2content = new Proportion( //how it does zimjs
                hscroll_x, hscroll_max_x,
                0, contentWidth - viewportWidth,
                1
            );
            self.private._hProportion_content2scroll = new Proportion( //how it does zimjs
                -(contentWidth - viewportWidth), 0,
                hscroll_max_x, hscroll_x,
                1
            );
        }

        if(ka_vscroll_rect) {
            ka_vscroll_rect.on('dragmove', function (ev) {
                ev.cancelBubble = true;
                let y = self.private._vProportion_scroll2content.convert(ka_vscroll_rect.y());
                ka_content_group.y(-y);
            });
            ka_vscroll_rect.on('dragend', function (ev) {
                /*---------------------------------*
                 *    HACK End Screen Moving
                 *---------------------------------*/
                ev.cancelBubble = true;

                let y = self.private._vProportion_scroll2content.convert(ka_vscroll_rect.y());
                ka_content_group.y(-y);
                adjust_screen_position(self); // by end of scrollbar movement
            });
        }

        if(ka_hscroll_rect) {
            ka_hscroll_rect.on('dragmove', function (ev) {
                ev.cancelBubble = true;
                let x = self.private._hProportion_scroll2content.convert(ka_hscroll_rect.x());
                ka_content_group.x(-x);
            });
            ka_hscroll_rect.on('dragend', function (ev) {
                /*---------------------------------*
                 *    HACK End Screen Moving
                 *---------------------------------*/
                ev.cancelBubble = true;
                adjust_screen_position(self);  // by end of scrollbar movement
            });
        }

        if(self.config.quick_display) {
            self.private._ka_container.draw();
        }
    }

    /********************************************
     *
     ********************************************/
    function move_scrollbars(self)
    {
        let ka_content_group = self.private._ka_content_group;

        let ka_vscroll_rect = self.private._ka_vscroll_rect;
        if(ka_vscroll_rect) {
            let y = self.private._vProportion_content2scroll.convert(ka_content_group.y());
            ka_vscroll_rect.y(y);
        }

        let ka_hscroll_rect = self.private._ka_hscroll_rect;
        if(ka_hscroll_rect) {
            let x = self.private._hProportion_content2scroll.convert(ka_content_group.x());
            ka_hscroll_rect.x(x);
        }

        if(self.config.quick_display) {
            self.private._ka_container.draw();
        }
    }

    /***************************************************
     *  HACK: pos - if it is absolute position of the node
     *  you should return absolute position too
     ***************************************************/
    function get_new_pos(self, pos, absolute_pos)
    {
        let ka_content_group = self.private._ka_content_group;
        let contentWidth = ka_content_group.width();
        let contentHeight = ka_content_group.height();

        const viewport_dim = self.private._ka_viewport_rect.getClientRect();
        if(absolute_pos) {
            pos.x -= viewport_dim.x;
            pos.y -= viewport_dim.y;
        }

        const minY = -(contentHeight - viewport_dim.height);
        const maxY = 0;
        let y = Math.max(
            minY,
            Math.min(
                pos.y,
                maxY
            )
        );

        const minX = -(contentWidth - viewport_dim.width);
        const maxX = 0;
        let x = Math.max(
            minX,
            Math.min(
                pos.x,
                maxX
            )
        );

        if(absolute_pos) {
            // return absolute pos
            x += viewport_dim.x;
            y += viewport_dim.y;
        }

        return {
            x: self.config.enable_hscroll? x: viewport_dim.x,
            y: self.config.enable_vscroll? y: viewport_dim.y
        };
    }

    /***************************************************
     *
     ***************************************************/
    function adjust_screen_position(self)
    {
        let ka_content_group = self.private._ka_content_group;
        let pos = ka_content_group.getClientRect({relativeTo: ka_content_group.getParent()});

        if(self.config.scroll_by_step === true) {
            if (self.config.enable_vscroll) {
                let vpos = get_first_full_visible_vertical_pos(self, true);
                if(vpos) {
                    pos.y = -vpos.y;
                }

            } else if (self.config.enable_hscroll) {
                let hpos = get_first_full_visible_horizontal_pos(self, true);
                if(hpos) {
                    pos.x = -hpos.x;
                }
            }
        }

        let new_pos = get_new_pos(self, pos, false);
        if(self.config.enable_vscroll) {
            self.private._ka_content_group.y(new_pos.y);
        }
        if(self.config.enable_hscroll) {
            self.private._ka_content_group.x(new_pos.x);
        }
        if(self.config.quick_display) {
            self.private._ka_container.draw();
        }

        move_scrollbars(self);
    }

    /********************************************
     *
        if max_width > width:
            if x + width > max_width:
                x = max_width - width

                        max_width
                 ┌──────────────────────────┐
                 │                          │
                 │                 width    │
                 │         x ┌──────────────────┐
                 │           │                  │
                 │           │                  │
                 │           └──────────────────┘
                 │                          │
                 └──────────────────────────┘


        elif max_width < width:
            x = 0
            width = max_width (size CHANGED!)
                (save the original dimension (x,width)

                        max_width
                 ┌──────────────────────────┐
                 │                          │
                 │          width           │
          x ┌───────────────────────────────────────────┐
            │                                           │
            │                                           │
            └───────────────────────────────────────────┘
                 │                          │
                 └──────────────────────────┘

     *
     *  kw: {
     *      x:
     *      y:
     *      width:
     *      height:
     *  }
     ********************************************/
    function do_fix_dimension_to_screen(self, kw)
    {
        let x = kw_get_int(kw, "x", self.config.x, false, false);
        let y = kw_get_int(kw, "y", self.config.y, false, false);
        let width = kw_get_int(kw, "width", self.config.width, false, false);
        let height = kw_get_int(kw, "height", self.config.height, false, false);

        let WIDTH,HEIGHT;
        if (window.innerHeight == undefined) {
            WIDTH = document.documentElement.offsetWidth;
            HEIGHT = document.documentElement.offsetHeight;
        } else {
            WIDTH = window.innerWidth;
            HEIGHT = window.innerHeight;
        }

        if(x < 0) {
            x = 0;
        }
        if(y < 0) {
            y = 0;
        }
        if(WIDTH > width) {
            if(x + width > WIDTH) {
                x = WIDTH - width;
                if(self.private._original_x === null) {
                    self.private._original_x = self.config.x;
                }
            }
        } else if(WIDTH <= width) {
            x = 0;
            width = WIDTH;
            if(self.private._original_width === null) {
                self.private._original_width = self.config.width;
            }
            self.config.width = width;
        }

        if(HEIGHT > height) {
            if(y + height > HEIGHT) {
                y = HEIGHT - height;
                if(self.private._original_y === null) {
                    self.private._original_y = self.config.y;
                }
            }
        } else if(HEIGHT <= height) {
            y = 0;
            height = HEIGHT;
            if(self.private._original_height === null) {
                self.private._original_height = self.config.height;
            }
            self.config.height = height;
        }

        self.config.x = x;
        self.config.y = y;

        return 0;
    }

    /********************************************
     *
     ********************************************/
    function do_center(self)
    {
        let x = self.config.x;
        let y = self.config.y;
        let width = self.config.width;
        let height = self.config.height;

        let WIDTH,HEIGHT;
        if (window.innerHeight == undefined) {
            WIDTH = document.documentElement.offsetWidth;
            HEIGHT = document.documentElement.offsetHeight;
        } else {
            WIDTH = window.innerWidth;
            HEIGHT = window.innerHeight;
        }

        if(WIDTH > width) {
            x = (WIDTH - width)/2;
            if(self.private._original_x === null) {
                self.private._original_x = self.config.x;
            }
        } else if(WIDTH <= width) {
            x = 0;
        }

        if(HEIGHT > height) {
            y = (HEIGHT - height)/2;
            if(self.private._original_y === null) {
                self.private._original_y = self.config.y;
            }
        } else if(HEIGHT <= height) {
            y = 0;
        }

        self.config.x = x;
        self.config.y = y;

        return 0;
    }

    /***************************************************
     *
     ***************************************************/
    function get_first_visible_vertical_child(self)
    {
        let pos = self.private._ka_content_group.position();
        pos.y *= -1;

        let children = self.private._ka_content_group.getChildren();
        for(let i=0; i<children.length; i++) {
            let child = children[i];
            if(child.name() == "ka_content_rect") {
                continue;
            }
            let pos_ = child.getClientRect({relativeTo:child.getParent()});
            if(pos.y >= pos_.y && pos.y < pos_.y + pos_.height) {
                return [child, pos_];
            }
        }

        return [null,null];
    }

    /***************************************************
     *
     ***************************************************/
    function get_first_visible_vertical_pos(self)
    {
        let child_and_pos = get_first_visible_vertical_child(self);
        return child_and_pos[1];
    }

    /***************************************************
     *
     ***************************************************/
    function get_first_visible_horizontal_child(self)
    {
        let pos = self.private._ka_content_group.position();
        pos.x *= -1;

        let children = self.private._ka_content_group.getChildren();
        for(let i=0; i<children.length; i++) {
            let child = children[i];
            if(child.name() == "ka_content_rect") {
                continue;
            }
            let pos_ = child.getClientRect({relativeTo:child.getParent()});
            if(pos.x >= pos_.x && pos.x < pos_.x + pos_.width) {
                return [child, pos_];
            }
        }

        return [null,null];
    }

    /***************************************************
     *
     ***************************************************/
    function get_first_visible_horizontal_pos(self)
    {
        let child_and_pos = get_first_visible_horizontal_child(self);
        return child_and_pos[1];
    }

    /***************************************************
     *
     ***************************************************/
    function get_first_full_visible_vertical_child(self)
    {
        let pos = self.private._ka_content_group.position();
        pos.y *= -1;

        let next = null;
        let pos_ = null;
        let children = self.private._ka_content_group.getChildren();
        for(let i=0; i<children.length-1; i++) {
            let child = children[i];
            if(child.name() == "ka_content_rect") {
                continue;
            }
            next = children[i+1];
            pos_ = child.getClientRect({relativeTo:child.getParent()});
            if(pos.y > pos_.y && pos.y <= pos_.y + pos_.height) {
                return [
                    next,
                    next.getClientRect({relativeTo:child.getParent()})
                ];
            }
        }

        return [null,null];
    }

    /***************************************************
     *
     ***************************************************/
    function get_first_full_visible_vertical_pos(self)
    {
        let child_and_pos = get_first_full_visible_vertical_child(self);
        return child_and_pos[1];
    }

    /***************************************************
     *
     ***************************************************/
    function get_first_full_visible_horizontal_child(self)
    {
        let pos = self.private._ka_content_group.position();
        pos.x *= -1;

        let next = null;
        let pos_ = null;
        let children = self.private._ka_content_group.getChildren();
        for(let i=0; i<children.length-1; i++) {
            let child = children[i];
            if(child.name() == "ka_content_rect") {
                continue;
            }
            next = children[i+1];
            pos_ = child.getClientRect({relativeTo:child.getParent()});
            if(pos.x > pos_.x && pos.x <= pos_.x + pos_.width) {
                return [
                    next,
                    next.getClientRect({relativeTo:child.getParent()})
                ];
            }
        }

        return [null,null];
    }

    /***************************************************
     *
     ***************************************************/
    function get_first_full_visible_horizontal_pos(self)
    {
        let child_and_pos = get_first_full_visible_horizontal_child(self);
        return child_and_pos[1];
    }

    /********************************************
     *
     ********************************************/
    function get_auto_x(self)
    {
        let CONFIG = gobj_get_gclass_config("Ka_scrollview", false);
        CONFIG._x += 10;
        return CONFIG._x;
    }

    /********************************************
     *
     ********************************************/
    function get_auto_y(self)
    {
        let CONFIG = gobj_get_gclass_config("Ka_scrollview", false);
        CONFIG._y += 5;
        return CONFIG._y;
    }




            /***************************
             *      Actions
             ***************************/




    /****************************************************
     *  {
     *      "items": [__ka_node__, ...]
     *  }
     *  {
     *      "items": [{"__ka_node__":__ka_node__}, ...]
     *  }
     ****************************************************/
    function ac_add_item(self, event, kw, src)
    {
        if (!self.private._ka_container) {
            log_error("ka_scrollview.ac_add_item(): _ka_container not defined");
            return -1;
        }

        let ka_items = kw_get_list(kw, "items", [], 0, true);
        for (let i = 0; i < ka_items.length; i++) {
            let ka_item = ka_items[i];
            if (ka_item instanceof Konva.Node) {
                // Refuse negative logic
                if(ka_item.x() < 0) {
                    ka_item.x(0);
                }
                if(ka_item.y() < 0) {
                    ka_item.y(0);
                }
                self.private._ka_content_group.add(ka_item);
            } else if(is_object(ka_item)) {
                let __ka_node__ = kw_get_dict_value(ka_item, "__ka_node__", null, false, true);
                if(__ka_node__) {
                    // Refuse negative logic
                    if(__ka_node__.x() < 0) {
                        __ka_node__.x(0);
                    }
                    if(__ka_node__.y() < 0) {
                        __ka_node__.y(0);
                    }
                    self.private._ka_content_group.add(__ka_node__);
                }
            } else {
                log_error("ka_scrollview.ac_add_item(): what fuck is it?");
            }
            if(self.config.quick_display) {
                self.private._ka_container.draw();
            }
        }

        self.private._ka_content_rect.moveToBottom();

        /*
         *  Autosize
         */
        if(self.config.autosize) {
            let content_dim = self.private._ka_content_group.getClientRect();
            self.config.width = content_dim.width;
            self.config.width += self.private._strokeW*2 + self.private._shadowW*2 + self.private._padding*2;

            self.config.height = content_dim.height;
            self.config.height += self.private._strokeW*2 + self.private._shadowW*2 + self.private._padding*2;

            if(self.config.fix_dimension_to_screen) {
                if(self.config.fix_dimension_to_screen === true) {
                    do_fix_dimension_to_screen(self, kw);
                } // TODO else if(fix_dimension_to_screen === "let_ some portion to be out
            }
            if(self.config.center) {
                do_center(self);
            }
        }

        create_scrollview(self, false);
        update_scrollbars(self);
        adjust_screen_position(self);

        return 0;
    }

    /******************************************************
     *  {
     *      "items": [__ka_node__, ...]
     *  }
     *  {
     *      "items": [{"__ka_node__":__ka_node__}, ...]
     *  }
     *  {
     *      "items": [{"id":?, "name":?}, ...]  // id,name: Mandatory fields to search in Konva group nodes
     *  }
     *
     *  "__ka_node__" has precedence,
     *      if not found then a search
     *      with `id` and/or `name` will be used
     ******************************************************/
    function ac_remove_item(self, event, kw, src)
    {
        if (!self.private._ka_container) {
            log_error("ka_scrollview.ac_remove_item(): _ka_container not defined");
            return -1;
        }

        search_ka_nodes(
            self.private._ka_container,
            kw,
            function(kw_item, __ka_node__) {
                __ka_node__.destroy();
                if(kw_item && kw_has_key(kw_item, "__ka_node__")) {
                    kw_item.__ka_node__ = null;
                }
            }
        );

        /*
         *  Update ka_content_rect to size of ka_content_group
         */
        self.private._ka_content_rect.moveToBottom();

        /*
         *  Autosize
         */
        if(self.config.autosize) {
            let content_dim = self.private._ka_content_group.getClientRect();
            self.config.width = content_dim.width;
            self.config.width += self.private._strokeW*2 + self.private._shadowW*2 + self.private._padding*2;

            self.config.height = content_dim.height;
            self.config.height += self.private._strokeW*2 + self.private._shadowW*2 + self.private._padding*2;

            if(self.config.fix_dimension_to_screen) {
                if(self.config.fix_dimension_to_screen === true) {
                    do_fix_dimension_to_screen(self, kw);
                } // TODO else if(fix_dimension_to_screen === "let_ some portion to be out
            }
            if(self.config.center) {
                do_center(self);
            }
        }

        create_scrollview(self, false);
        update_scrollbars(self);
        adjust_screen_position(self);

        return 0;
    }

    /********************************************
     *  Please be idempotent
     ********************************************/
    function ac_activate(self, event, kw, src)
    {
        if(self.config.top_on_activate) {
            self.private._ka_container.moveToTop();
        }

        /*
         *  Only used: stroke, opacity, shadowBlur, shadowColor
         */
        let kw_rect = self.config.kw_border_shape_actived;

        let stroke = kw_get_str(kw_rect,"stroke",null);
        if(!is_null(stroke)) {
            self.private._ka_border_rect.stroke(stroke);
        }

        let opacity = kw_get_real(kw_rect, "opacity", null);
        if(!is_null(opacity)) {
            self.private._ka_border_rect.opacity(opacity);
        }

        let shadowBlur = kw_get_int(kw_rect, "shadowBlur", null);
        if(!is_null(shadowBlur)) {
            self.private._ka_border_rect.shadowBlur(shadowBlur);
        }

        let shadowColor = kw_get_str(kw_rect, "shadowColor", null);
        if(!is_null(shadowColor)) {
            self.private._ka_border_rect.shadowColor(shadowColor);
        }

        return 0;
    }

    /********************************************
     *  Please be idempotent
     ********************************************/
    function ac_deactivate(self, event, kw, src)
    {
        /*
         *  Only used: stroke, opacity, shadowBlur, shadowColor
         */
        let kw_rect = self.config.kw_border_shape;

        let stroke = kw_get_str(kw_rect,"stroke",null);
        if(!is_null(stroke)) {
            self.private._ka_border_rect.stroke(stroke);
        }

        let opacity = kw_get_real(kw_rect, "opacity", null);
        if(!is_null(opacity)) {
            self.private._ka_border_rect.opacity(opacity);
        }

        let shadowBlur = kw_get_int(kw_rect, "shadowBlur", null);
        if(!is_null(shadowBlur)) {
            self.private._ka_border_rect.shadowBlur(shadowBlur);
        }

        let shadowColor = kw_get_str(kw_rect, "shadowColor", null);
        if(!is_null(shadowColor)) {
            self.private._ka_border_rect.shadowColor(shadowColor);
        }

        return 0;
    }

    /********************************************
     *  kw: {
     *      x:
     *      y:
     *  }
     ********************************************/
    function ac_position(self, event, kw, src)
    {
        if(!self.private._ka_container) {
            log_error("_ka_container not defined");
            return -1;
        }

        // TODO: y no valdrá con cambiar la posición?
        self.config.x = kw_get_int(kw, "x", self.config.x, false, false);
        self.config.y = kw_get_int(kw, "y", self.config.y, false, false);

        if(self.config.fix_dimension_to_screen) {
            if(self.config.fix_dimension_to_screen === true) {
                do_fix_dimension_to_screen(self, kw);
            } // TODO else if(fix_dimension_to_screen === "let_ some portion to be out
        }
        if(self.config.center) {
            do_center(self);
        }
        self.private._ka_container.position({x:self.config.x, y:self.config.y});

        return 0;
    }

    /********************************************
     *  kw: {
     *      width:
     *      height:
     *  }
     ********************************************/
    function ac_size(self, event, kw, src)
    {
        if(!self.private._ka_container) {
            log_error("_ka_container not defined");
            return -1;
        }

        self.config.width = kw_get_int(kw, "width", self.config.width, false, false);
        self.config.height = kw_get_int(kw, "height", self.config.height, false, false);

        if(self.config.fix_dimension_to_screen) {
            if(self.config.fix_dimension_to_screen === true) {
                do_fix_dimension_to_screen(self, kw);
            } // TODO else if(fix_dimension_to_screen === "let_ some portion to be out
        }
        if(self.config.center) {
            do_center(self);
        }

        create_scrollview(self, false);
        update_scrollbars(self);
        adjust_screen_position(self);

        return 0;
    }

    /********************************************
     *  Return dimensions (position and size)
     *  kw: {
     *      x:                  // current dimension
     *      y:
     *      width:
     *      height:
     *
     *      _original_x:        // Original dimension
     *      _original_y:
     *      _original_width:
     *      _original_height:
     *
     *      util_dimension: {   // util dimension
     *          x:
     *          y:
     *          width:
     *          height:
     *      }
     *      absolute_dimension: { // To layer
     *          x:
     *          y:
     *          width:
     *          height:
     *      }
     *  }
     *
     ********************************************/
    function ac_get_dimension(self, event, kw, src)
    {
        if(!is_object(kw)) {
            log_error(sprintf("%s: get_dimension(): kw not an object, from %s",
                self.gobj_short_name(), src.gobj_short_name()
            ));
            return -1;
        }
        kw["x"] = self.config.x;
        kw["y"] = self.config.y;
        kw["width"] = self.config.width;
        kw["height"] = self.config.height;

        kw["absolute_dimension"] = self.private._ka_container.getClientRect();

        kw["_original_x"] = self.private._original_x;
        kw["_original_y"] = self.private._original_y;
        kw["_original_width"] = self.private._original_width;
        kw["_original_height"] = self.private._original_height;

        kw["util_dimension"] = {
            x: 0,
            y: 0,
            width: self.private._util_width,
            height: self.private._util_height
        };

        return 0;
    }

    /********************************************
     *
     ********************************************/
    function ac_show_or_hide(self, event, kw, src)
    {
        if(!self.private._ka_container) {
            log_error("_ka_container not defined");
            return -1;
        }

        switch(event) {
            case "EV_TOGGLE":
                if(self.private._ka_container.isVisible()) {
                    self.private._ka_container.hide();
                } else {
                    self.private._ka_container.show();
                }
                break;
            case "EV_SHOW":
                self.private._ka_container.show();
                break;
            case "EV_HIDE":
                self.private._ka_container.hide();
                break;
        }

        if(self.private._ka_container.isVisible()) {
            self.gobj_publish_event("EV_SHOWED", {});
        } else {
            self.gobj_publish_event("EV_HIDDEN", {});
        }

        return 0;
    }

    /********************************************
     *
     ********************************************/
    function ac_resize(self, event, kw, src)
    {
        if(!self.private._ka_container) {
            log_error("_ka_container not defined");
            return -1;
        }

        self.config.x = kw_get_int(kw, "x", self.config.x, true, false);
        self.config.y = kw_get_int(kw, "y", self.config.y, true, false);
        self.config.width = kw_get_int(kw, "width", self.config.width, true, false);
        self.config.height = kw_get_int(kw, "height", self.config.height, true, false);

        if(self.config.fix_dimension_to_screen) {
            if(self.config.fix_dimension_to_screen === true) {
                do_fix_dimension_to_screen(self, kw);
            } // TODO else if(fix_dimension_to_screen === "let_ some portion to be out
        }
        if(self.config.center) {
            do_center(self);
        }

        create_scrollview(self, false);
        update_scrollbars(self);
        adjust_screen_position(self);
        return 0;
    }




            /***************************
             *      GClass/Machine
             ***************************/




    let FSM = {
        "event_list": [
            "EV_ADD_ITEM",
            "EV_REMOVE_ITEM",
            "EV_ACTIVATE",
            "EV_DEACTIVATE",
            "EV_POSITION",
            "EV_SIZE",
            "EV_GET_DIMENSION",
            "EV_TOGGLE",
            "EV_SHOW",
            "EV_HIDE",
            "EV_MOVING: output no_warn_subs",
            "EV_MOVED: output no_warn_subs",
            "EV_PANNING: output no_warn_subs",
            "EV_PANNED: output no_warn_subs",
            "EV_SHOWED: output no_warn_subs",
            "EV_HIDDEN: output no_warn_subs",
            "EV_RESIZE"
        ],
        "state_list": [
            "ST_IDLE"
        ],
        "machine": {
            "ST_IDLE":
            [
                ["EV_ADD_ITEM",         ac_add_item,            undefined],
                ["EV_REMOVE_ITEM",      ac_remove_item,         undefined],
                ["EV_ACTIVATE",         ac_activate,            undefined],
                ["EV_DEACTIVATE",       ac_deactivate,          undefined],
                ["EV_POSITION",         ac_position,            undefined],
                ["EV_SIZE",             ac_size,                undefined],
                ["EV_GET_DIMENSION",    ac_get_dimension,       undefined],
                ["EV_TOGGLE",           ac_show_or_hide,        undefined],
                ["EV_SHOW",             ac_show_or_hide,        undefined],
                ["EV_HIDE",             ac_show_or_hide,        undefined],
                ["EV_RESIZE",           ac_resize,              undefined]
            ]
        }
    };

    let Ka_scrollview = GObj.__makeSubclass__();
    let proto = Ka_scrollview.prototype; // Easy access to the prototype
    proto.__init__= function(name, kw) {
        GObj.prototype.__init__.call(
            this,
            FSM,
            CONFIG,
            name,
            "Ka_scrollview",
            kw,
            0
        );
        return this;
    };
    gobj_register_gclass(Ka_scrollview, "Ka_scrollview");




            /***************************
             *      Framework Methods
             ***************************/




    /************************************************
     *      Framework Method create
     ************************************************/
    proto.mt_create = function(kw)
    {
        let self = this;

        /*
         *  Child model
         */
        if(!self.config.subscriber) {
            self.config.subscriber = self.gobj_parent();  // Remove if not child model
        }
        if(self.config.subscriber) {
            self.gobj_subscribe_event(null, {}, self.config.subscriber);
        }

        if(!self.config.layer) {
            self.config.layer = self.gobj_parent().config.layer;
        }

        create_scrollview(self, true);
    };

    /************************************************
     *      Framework Method destroy
     *      In this point, all childs
     *      and subscriptions are already deleted.
     ************************************************/
    proto.mt_destroy = function()
    {
        let self = this;

        if(self.private._ka_container) {
            self.private._ka_container.destroy();
            self.private._ka_container = null;
        }
    };

    /************************************************
     *      Framework Method start
     ************************************************/
    proto.mt_start = function(kw)
    {
        let self = this;
    };

    /************************************************
     *      Framework Method stop
     ************************************************/
    proto.mt_stop = function(kw)
    {
        let self = this;
    };

    /************************************************
     *  Framework Method mt_child_added
     ************************************************/
    proto.mt_child_added = function(child)
    {
        let self = this;

        self.gobj_send_event(
            "EV_ADD_ITEM",
            {
                items: [child.get_konva_container()]
            },
            self
        );
    };

    /************************************************
     *      Local Method
     ************************************************/
    proto.get_viewport_rect = function()
    {
        let self = this;
        return self.private._ka_viewport_rect.size();
    };

    /************************************************
     *      Local Method
     ************************************************/
    proto.isVisible = function()
    {
        let self = this;
        return self.private._ka_container.isVisible();
    };

    /************************************************
     *      Local Method
     ************************************************/
    proto.get_konva_container = function()
    {
        let self = this;
        return self.private._ka_container;
    };

    /************************************************
     *      Local Method
     ************************************************/
    proto.get_util_dimension = function() {
        let self = this;
        return {
            x: 0,
            y: 0,
            width: self.private._util_width,
            height: self.private._util_height
        };
    };

    /************************************************
     *      Framework Method CONFIG
     *  Example how change CONFIG of a gclass (temporarily)
     *
            let CONFIG = gobj_get_gclass_config("Ka_scrollview", true);
            let old_dragging = CONFIG.draggable;
            CONFIG.draggable = true;

            let gobj_ka_scrollview = self.yuno.gobj_create(
                "xxx",
                Ka_scrollview,
                {
                    ...
                },
                self // this will provoke EV_SHOWED,EV_KEYDOWN
            );

            CONFIG.draggable = old_dragging;

     ************************************************/
    proto.mt_get_gclass_config = function()
    {
        return CONFIG;
    };

    //=======================================================================
    //      Expose the class via the global object
    //=======================================================================
    exports.Ka_scrollview = Ka_scrollview;

})(this);
