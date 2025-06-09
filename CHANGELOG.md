# **Changelog**

## v7.0.0 -- 2024-??-??

Change API

    tranger2_write_user_flag
    tranger2_set_user_flag
    tranger2_read_user_flag

    trq_check_pending_rowid
    trq_msg_md_record -> trq_msg_md

    tranger2_print_md0_record
    tranger2_print_md1_record
    tranger2_print_md2_record

    trq_msg_rowid

    build_command_response moved from gobj.c/h to command_parser.c/h

    gobj_read_bool_attr:
        implicit BOOL attrs: __disabled__,__running__,__playing__,__service__
    gobj_read_int_attr:
        implicit int attrs: __trace_level__
    gobj_read_str_attr:
        implicit char attrs: __state__

    newfile (only to write) -> (wr/rd)
    newdir          - parameter name changed
    open_exclusive  - parameter name changed
    mkrdir          - parameter name changed

    gobj_write_attrs - parameter name changed

    typedef struct states_s:
        ev_action_t *state; => ev_action_t *ev_action_list;

    gclass_add_event_type()

    json_record_to_schema -> json_desc_to_schema
    gobj_unsubscribe_list

    rename childs -> children

        gobj.h:PUBLIC void gobj_destroy_childs(hgobj gobj);
        gobj.h:PUBLIC int gobj_start_childs(hgobj gobj);   // only direct childs
        gobj.h:PUBLIC int gobj_start_tree(hgobj gobj);     // childs with gcflag_manual_start flag are not started.
        gobj.h:PUBLIC int gobj_stop_childs(hgobj gobj);    // only direct childs
        gobj.h:PUBLIC int gobj_stop_tree(hgobj gobj);      // all tree of childs
        gobj.h:PUBLIC json_t *gobj_match_childs( // return an iter of first level matching jn_filter
        gobj.h:PUBLIC json_t *gobj_match_childs_tree( // return an iter of any level matching jn_filter
        gobj.h:PUBLIC int gobj_walk_gobj_childs(
        gobj.h:PUBLIC int gobj_walk_gobj_childs_tree(
        gobj.h:PUBLIC json_t *gobj_node_childs( // Return MUST be decref
        tr_treedb.h:PUBLIC json_t *treedb_node_childs(

    rename gobj_set_yuno_must_die() to set_yuno_must_die();
    rename gobj_get_yuno_must_die() to get_yuno_must_die();

    remove set_ordered_death()
    yev_create_accept_event(), yev_setup_accept_event() ->  new parameters
    yev_create_connect_event(), yev_setup_connect_event() -> yev_ream_connect_event() 
 
    important! gclass and event names are case insensitive

    gobj_start_up split in gobj_start_up and gobj_setup_memory

    gobj_create_tree0() PRIVATE
    gobj_set_bottom_gobj()  return previous bottom_gobj MT (mt_set_bottom_gobj)
        typedef void (*mt_set_bottom_gobj_fn)(
            hgobj gobj,
            hgobj new_bottom_gobj,
            hgobj prev_bottom_gobj
        );

    tranger2_list_keys()

    get_ordered_filename_array() change
    free_ordered_filename_array() removed

    time_in_miliseconds_monotonic -> time_in_milliseconds_monotonic
    time_in_miliseconds -> time_in_milliseconds

New API

    build_stats_response
    trq_append2
    kw_collect
    gobj_load_persistent_attrs

    gobj_change_parent(hgobj gobj, hgobj gobj_new_parent); // TODO already implemented in js
    is_metadata_key
    is_private_key
    PUBLIC size_t json_size(json_t *value);
    gobj_subs_desc
    gobj_list_subscribings
    gobj_is_bottom_gobj
    str2gbuf
    gobj_set_trace_machine_format
    get_net_core_somaxconn(void)
    gclass_find_event
    mt_get_time
    set_measure_times
    get_measure_times
    yev_dup_accept_event
    yev_event_is_idle

    find_files_with_suffix_array
    dir_array_sort
    dir_array_free
    gobj_global_trace_level(void)


Delete API


<!-- ([full changelog](https://github.com/executablebooks/sphinx-book-theme/compare/v1.1.1...3da24da74f6042599fe6c9e2d612f5cbdef42280)) -->

### Enhancements made

- ENH:

### Bugs fixed

- FIX:

### Maintenance and upkeep improvements

- MAINT:

### Documentation improvements

- DOCS:

### Other merged PRs

### Contributors to this release
