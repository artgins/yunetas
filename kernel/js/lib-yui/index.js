/***********************************************************************
 *          @yuneta/lib-yui/index.js
 *
 *          Yuneta UI Library - Reusable GUI components
 *
 *          NOTE 2026-05-21: the declarative shell stack
 *          (C_YUI_SHELL, C_YUI_NAV, C_YUI_PAGER, C_YUI_WIZARD,
 *          shell_modals + helpers) was removed from this package
 *          and now evolves only in wattyzer/gui/src/lib-yui/
 *          (vendored flat copy).  Legacy apps (estadodelaire)
 *          consume this package and only need the
 *          legacy stack (C_YUI_MAIN + C_YUI_WINDOW + C_YUI_TABS
 *          + C_YUI_ROUTING + TreeDB views + chart/map widgets).
 *          Removing the new stack from here makes their bundles
 *          smaller and keeps the dependency direction clean.
 *
 *          Copyright (c) 2025, ArtGins.
 *          All Rights Reserved.
 ***********************************************************************/

/*
 *  Components — legacy app-shell stack
 */
export { register_c_yui_main, display_volatil_modal, display_info_message, display_warning_message, display_error_message, get_yesnocancel, get_yesno, get_ok } from "./src/c_yui_main.js";
export { register_c_yui_window } from "./src/c_yui_window.js";
export { register_c_yui_tabs } from "./src/c_yui_tabs.js";
export { register_c_yui_form } from "./src/c_yui_form.js";
export { register_c_yui_routing } from "./src/c_yui_routing.js";
export { register_c_yui_map } from "./src/c_yui_map.js";
export { register_c_yui_uplot } from "./src/c_yui_uplot.js";
export { register_c_yui_json_graph } from "./src/c_yui_json_graph.js";
export { register_c_yui_gobj_tree_js } from "./src/c_yui_gobj_tree_js.js";

/*
 *  TreeDB components
 */
export { register_c_yui_treedb_topics } from "./src/c_yui_treedb_topics.js";
export { register_c_yui_treedb_topic_with_form } from "./src/c_yui_treedb_topic_with_form.js";
export { register_c_yui_treedb_graph } from "./src/c_yui_treedb_graph.js";
export { register_c_g6_nodes_tree } from "./src/c_g6_nodes_tree.js";

/*
 *  Libraries and utilities
 */
export { addClasses, removeClasses, toggleClasses, removeChildElements, disableElements, enableElements, set_submit_state, set_cancel_state, set_active_state, getStrokeColor } from "./src/lib_graph.js";
export { inject_svg_icons } from "./src/lib_icons.js";
export { EditControl, MarkerControl } from "./src/lib_maplibre.js";
export { themes } from "./src/themes.js";
export { YTable, createYTable } from "./src/ytable.js";
export { yui_toolbar } from "./src/yui_toolbar.js";
export { info_traffic, setup_dev, build_dev_panel } from "./src/yui_dev.js";

/*
 *  CSS - import these in your main entry point
 *  Example:
 *    import "lib-yui/src/c_yui_main.css";
 *    import "lib-yui/src/c_yui_map.css";
 *    import "lib-yui/src/c_yui_routing.css";
 *    import "lib-yui/src/ytable.css";
 *    import "lib-yui/src/yui_toolbar.css";
 *    import "lib-yui/src/lib_graph.css";
 *    import "lib-yui/src/yui_icons.css";
 */
