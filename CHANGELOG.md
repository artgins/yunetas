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

New API

    build_stats_response
    trq_append2
    kw_collect
    gobj_load_persistent_attrs

    gobj_change_parent(hgobj gobj, hgobj gobj_new_parent); // TODO already implemented in js
    is_metadata_key
    is_private_key
    PUBLIC size_t json_size(json_t *value);

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
