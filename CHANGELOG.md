# **Changelog**

## v7.0.0 -- 2024-??-??

New apis:

    PUBLIC int gclass_check_fsm(hgclass gclass);
    PUBLIC BOOL gobj_has_state(hgobj gobj, gobj_state_t gobj_state);
    PUBLIC event_type_t *gclass_find_event_in_event_list(hgclass gclass, gobj_event_t event);

New comment in .h

    PUBLIC void gobj_set_exit_code(int exit_code); // set return code to exit when running as daemon

    PUBLIC hgclass gclass_create( // create and register gclass

    PUBLIC gobj_event_t gclass_find_public_event(const char *event, BOOL verbose); // Find a public event in any gclass



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
