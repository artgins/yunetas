---
title: 'Appendix: Kernel C API — Complete Function Index'
description: >-
  Quick-reference listing of every public C function in the Yuneta kernel,
  grouped by header file with full prototypes.
---

# Appendix: Kernel C API — Complete Function Index

This appendix lists **every public C function** declared in the kernel
header files (excluding `linux-ext-libs` and `root-esp32`).
Each section shows functions in their natural declaration order.
Use your browser's find (**Ctrl+F**) to search by name.
The [Alphabetical Index](#alphabetical-index) at the end lists every function sorted A–Z
with links to the API documentation.

## gobj-c (Core Framework)

### `command_parser.h` — 5 functions

**Source:** `kernel/c/gobj-c/src/command_parser.h`

1. [**`command_parser`**](parsers/command_parser.md#command_parser) — `PUBLIC json_t *command_parser( hgobj gobj, const char *command, json_t *kw, hgobj src )`

2. [**`gobj_build_cmds_doc`**](parsers/command_parser.md#gobj_build_cmds_doc) — `PUBLIC json_t *gobj_build_cmds_doc( hgobj gobj, json_t *kw )`

3. [**`build_command_response`**](parsers/command_parser.md#build_command_response) — `PUBLIC json_t *build_command_response( hgobj gobj, json_int_t result, json_t *jn_comment, json_t *jn_schema, json_t *jn_data )`

4. [**`command_get_cmd_desc`**](parsers/command_parser.md#command_get_cmd_desc) — `PUBLIC const sdata_desc_t *command_get_cmd_desc( const sdata_desc_t *command_table, const char *command )`

5. [**`search_command_desc`**](parsers/command_parser.md#search_command_desc) — `PUBLIC const sdata_desc_t *search_command_desc( hgobj gobj, const char *command, int level, hgobj *gobj_found )`

### `dl_list.h` — 8 functions

**Source:** `kernel/c/gobj-c/src/dl_list.h`

1. [**`dl_init`**](helpers/dl.md#dl_init) — `PUBLIC int dl_init(dl_list_t *dl, hgobj gobj)`

2. [**`dl_insert`**](helpers/dl.md#dl_insert) — `PUBLIC int dl_insert(dl_list_t *dl, void *item)`

3. [**`dl_add`**](helpers/dl.md#dl_add) — `PUBLIC int dl_add(dl_list_t *dl, void *item)`

4. [**`dl_find`**](helpers/dl.md#dl_find) — `PUBLIC void * dl_find(dl_list_t *dl, void *item)`

5. [**`dl_nfind`**](helpers/dl.md#dl_nfind) — `PUBLIC void * dl_nfind(dl_list_t *dl, size_t nitem)`

6. [**`dl_delete`**](helpers/dl.md#dl_delete) — `PUBLIC int dl_delete(dl_list_t *dl, void * curr_, void (*fnfree)(void *))`

7. [**`dl_delete_item`**](helpers/dl.md#dl_delete_item) — `PUBLIC int dl_delete_item(void *curr, void (*fnfree)(void *))`

8. [**`dl_flush`**](helpers/dl.md#dl_flush) — `PUBLIC void dl_flush(dl_list_t *dl, void (*fnfree)(void *))`

### `gbmem.h` — 15 functions

**Source:** `kernel/c/gobj-c/src/gbmem.h`

1. [**`gbmem_setup`**](helpers/memory.md#gbmem_setup) — `PUBLIC int gbmem_setup( size_t mem_max_block, size_t mem_max_system_memory, BOOL use_own_system_memory, size_t mem_min_block, size_t mem_superblock )`

2. [**`gbmem_shutdown`**](helpers/memory.md#gbmem_shutdown) — `PUBLIC void gbmem_shutdown(void)`

3. [**`gbmem_set_allocators`**](helpers/memory.md#gbmem_set_allocators) — `PUBLIC int gbmem_set_allocators( sys_malloc_fn_t malloc_func, sys_realloc_fn_t realloc_func, sys_calloc_fn_t calloc_func, sys_free_fn_t free_func )`

4. [**`gbmem_get_allocators`**](helpers/memory.md#gbmem_get_allocators) — `PUBLIC int gbmem_get_allocators( sys_malloc_fn_t *malloc_func, sys_realloc_fn_t *realloc_func, sys_calloc_fn_t *calloc_func, sys_free_fn_t *free_func )`

5. [**`gbmem_malloc`**](helpers/memory.md#gbmem_malloc) — `PUBLIC void *gbmem_malloc(size_t size)`

6. [**`gbmem_free`**](helpers/memory.md#gbmem_free) — `PUBLIC void gbmem_free(void *ptr)`

7. [**`gbmem_realloc`**](helpers/memory.md#gbmem_realloc) — `PUBLIC void *gbmem_realloc(void *ptr, size_t size)`

8. [**`gbmem_calloc`**](helpers/memory.md#gbmem_calloc) — `PUBLIC void *gbmem_calloc(size_t n, size_t size)`

9. [**`gbmem_strndup`**](helpers/memory.md#gbmem_strndup) — `PUBLIC char *gbmem_strndup(const char *str, size_t size)`

10. [**`gbmem_strdup`**](helpers/memory.md#gbmem_strdup) — `PUBLIC char *gbmem_strdup(const char *str)`

11. [**`gbmem_get_maximum_block`**](helpers/memory.md#gbmem_get_maximum_block) — `PUBLIC size_t gbmem_get_maximum_block(void)`

12. [**`get_max_system_memory`**](helpers/memory.md#get_max_system_memory) — `PUBLIC size_t get_max_system_memory(void)`

13. [**`get_cur_system_memory`**](helpers/memory.md#get_cur_system_memory) — `PUBLIC size_t get_cur_system_memory(void)`

14. [**`set_memory_check_list`**](helpers/memory.md#set_memory_check_list) — `PUBLIC void set_memory_check_list(unsigned long *memory_check_list)`

15. [**`print_track_mem`**](helpers/memory.md#print_track_mem) — `PUBLIC void print_track_mem(void)`

### `gbuffer.h` — 25 functions

**Source:** `kernel/c/gobj-c/src/gbuffer.h`

1. [**`gbuffer_create`**](helpers/gbuffer.md#gbuffer_create) — `PUBLIC gbuffer_t *gbuffer_create( size_t data_size, size_t max_memory_size )`

2. [**`gbuffer_remove`**](helpers/gbuffer.md#gbuffer_remove) — `PUBLIC void gbuffer_remove(gbuffer_t *gbuf)`

3. [**`gbuffer_set_rd_offset`**](helpers/gbuffer.md#gbuffer_set_rd_offset) — `PUBLIC int gbuffer_set_rd_offset(gbuffer_t *gbuf, size_t position)`

4. [**`gbuffer_get`**](helpers/gbuffer.md#gbuffer_get) — `PUBLIC void * gbuffer_get(gbuffer_t *gbuf, size_t len)`

5. [**`gbuffer_getline`**](helpers/gbuffer.md#gbuffer_getline) — `PUBLIC char *gbuffer_getline(gbuffer_t *gbuf, char separator)`

6. [**`gbuffer_set_wr`**](helpers/gbuffer.md#gbuffer_set_wr) — `PUBLIC int gbuffer_set_wr(gbuffer_t *gbuf, size_t offset)`

7. [**`gbuffer_append`**](helpers/gbuffer.md#gbuffer_append) — `PUBLIC size_t gbuffer_append(gbuffer_t *gbuf, void *data, size_t len)`

8. [**`gbuffer_append_json`**](helpers/gbuffer.md#gbuffer_append_json) — `PUBLIC size_t gbuffer_append_json( gbuffer_t *gbuf, json_t *jn )`

9. [**`gbuffer_append_gbuf`**](helpers/gbuffer.md#gbuffer_append_gbuf) — `PUBLIC int gbuffer_append_gbuf( gbuffer_t *dst, gbuffer_t *src )`

10. [**`gbuffer_printf`**](helpers/gbuffer.md#gbuffer_printf) — `PUBLIC int gbuffer_printf(gbuffer_t *gbuf, const char *format, ...) JANSSON_ATTRS((format(printf, 2, 3)))`

11. [**`gbuffer_vprintf`**](helpers/gbuffer.md#gbuffer_vprintf) — `PUBLIC int gbuffer_vprintf(gbuffer_t *gbuf, const char *format, va_list ap) JANSSON_ATTRS((format(printf, 2, 0)))`

12. [**`gbuffer_setlabel`**](helpers/gbuffer.md#gbuffer_setlabel) — `PUBLIC int gbuffer_setlabel(gbuffer_t *gbuf, const char *label)`

13. [**`gbuf2file`**](helpers/gbuffer.md#gbuf2file) — `PUBLIC int gbuf2file( hgobj gobj, gbuffer_t *gbuf, const char *path, int permission, BOOL overwrite )`

14. [**`gbuffer_serialize`**](helpers/gbuffer.md#gbuffer_serialize) — `PUBLIC json_t* gbuffer_serialize( hgobj gobj, gbuffer_t *gbuf )`

15. [**`gbuffer_deserialize`**](helpers/gbuffer.md#gbuffer_deserialize) — `PUBLIC gbuffer_t *gbuffer_deserialize( hgobj gobj, const json_t *jn )`

16. [**`gbuffer_binary_to_base64`**](helpers/gbuffer.md#gbuffer_binary_to_base64) — `PUBLIC gbuffer_t *gbuffer_binary_to_base64(const char *src, size_t len)`

17. [**`gbuffer_file2base64`**](helpers/gbuffer.md#gbuffer_file2base64) — `PUBLIC gbuffer_t *gbuffer_file2base64(const char *path)`

18. [**`gbuffer_base64_to_binary`**](helpers/gbuffer.md#gbuffer_base64_to_binary) — `PUBLIC gbuffer_t *gbuffer_base64_to_binary(const char *base64, size_t base64_len)`

19. [**`gbuffer_encode_base64`**](helpers/gbuffer.md#gbuffer_encode_base64) — `PUBLIC gbuffer_t *gbuffer_encode_base64( gbuffer_t *gbuf_input )`

20. [**`str2gbuf`**](helpers/gbuffer.md#str2gbuf) — `PUBLIC gbuffer_t *str2gbuf( const char *fmt, ... ) JANSSON_ATTRS((format(printf, 1, 2)))`

21. [**`json2gbuf`**](helpers/gbuffer.md#json2gbuf) — `PUBLIC gbuffer_t *json2gbuf( gbuffer_t *gbuf, json_t *jn, size_t flags )`

22. [**`gbuf2json`**](helpers/gbuffer.md#gbuf2json) — `PUBLIC json_t *gbuf2json( gbuffer_t *gbuf, int verbose )`

23. [**`config_gbuffer2json`**](helpers/gbuffer.md#config_gbuffer2json) — `PUBLIC json_t *config_gbuffer2json( gbuffer_t *gbuf, int verbose )`

24. [**`gobj_trace_dump_gbuf`**](helpers/gbuffer.md#gobj_trace_dump_gbuf) — `PUBLIC void gobj_trace_dump_gbuf( hgobj gobj, gbuffer_t *gbuf, const char *fmt, ... ) JANSSON_ATTRS((format(printf, 3, 4)))`

25. [**`gobj_trace_dump_full_gbuf`**](helpers/gbuffer.md#gobj_trace_dump_full_gbuf) — `PUBLIC void gobj_trace_dump_full_gbuf( hgobj gobj, gbuffer_t *gbuf, const char *fmt, ... ) JANSSON_ATTRS((format(printf, 3, 4)))`

### `glogger.h` — 34 functions

**Source:** `kernel/c/gobj-c/src/glogger.h`

1. [**`glog_init`**](logging/log.md#glog_init) — `PUBLIC void glog_init(void)`

2. [**`glog_end`**](logging/log.md#glog_end) — `PUBLIC void glog_end(void)`

3. [**`gobj_log_register_handler`**](logging/log.md#gobj_log_register_handler) — `PUBLIC int gobj_log_register_handler( const char *handler_type, loghandler_close_fn_t close_fn, loghandler_write_fn_t write_fn, loghandler_fwrite_fn_t fwrite_fn )`

4. [**`gobj_log_exist_handler`**](logging/log.md#gobj_log_exist_handler) — `PUBLIC BOOL gobj_log_exist_handler(const char *handler_name)`

5. [**`gobj_log_add_handler`**](logging/log.md#gobj_log_add_handler) — `PUBLIC int gobj_log_add_handler( const char *handler_name, const char *handler_type, log_handler_opt_t handler_options, void *h )`

6. [**`gobj_log_del_handler`**](logging/log.md#gobj_log_del_handler) — `PUBLIC int gobj_log_del_handler(const char *handler_name)`

7. [**`gobj_log_list_handlers`**](logging/log.md#gobj_log_list_handlers) — `PUBLIC json_t *gobj_log_list_handlers(void)`

8. [**`_log_bf`**](logging/log.md#_log_bf) — `PUBLIC void _log_bf(int priority, log_opt_t opt, const char *bf, size_t len)`

9. [**`gobj_log_set_global_handler_option`**](logging/log.md#gobj_log_set_global_handler_option) — `PUBLIC int gobj_log_set_global_handler_option( log_handler_opt_t log_handler_opt, BOOL set )`

10. [**`stdout_write`**](logging/log.md#stdout_write) — `PUBLIC int stdout_write(void *v, int priority, const char *bf, size_t len)`

11. [**`stdout_fwrite`**](logging/log.md#stdout_fwrite) — `PUBLIC int stdout_fwrite(void* v, int priority, const char* format, ...)`

12. [**`gobj_log_alert`**](logging/log.md#gobj_log_alert) — `PUBLIC void gobj_log_alert(hgobj gobj, log_opt_t opt, ...)`

13. [**`gobj_log_critical`**](logging/log.md#gobj_log_critical) — `PUBLIC void gobj_log_critical(hgobj gobj, log_opt_t opt, ...)`

14. [**`gobj_log_error`**](logging/log.md#gobj_log_error) — `PUBLIC void gobj_log_error(hgobj gobj, log_opt_t opt, ...)`

15. [**`gobj_log_warning`**](logging/log.md#gobj_log_warning) — `PUBLIC void gobj_log_warning(hgobj gobj, log_opt_t opt, ...)`

16. [**`gobj_log_info`**](logging/log.md#gobj_log_info) — `PUBLIC void gobj_log_info(hgobj gobj, log_opt_t opt, ...)`

17. [**`gobj_log_debug`**](logging/log.md#gobj_log_debug) — `PUBLIC void gobj_log_debug(hgobj gobj, log_opt_t opt, ...)`

18. [**`gobj_get_log_priority_name`**](logging/log.md#gobj_get_log_priority_name) — `PUBLIC const char *gobj_get_log_priority_name(int priority)`

19. [**`gobj_get_log_data`**](logging/log.md#gobj_get_log_data) — `PUBLIC json_t *gobj_get_log_data(void)`

20. [**`gobj_log_clear_counters`**](logging/log.md#gobj_log_clear_counters) — `PUBLIC void gobj_log_clear_counters(void)`

21. [**`gobj_log_clear_log_file`**](logging/log.md#gobj_log_clear_log_file) — `PUBLIC void gobj_log_clear_log_file(void)`

22. [**`gobj_log_last_message`**](logging/log.md#gobj_log_last_message) — `PUBLIC const char *gobj_log_last_message(void)`

23. [**`gobj_log_set_last_message`**](logging/log.md#gobj_log_set_last_message) — `PUBLIC void gobj_log_set_last_message(const char *msg, ...) JANSSON_ATTRS((format(printf, 1, 2)))`

24. [**`set_show_backtrace_fn`**](logging/log.md#set_show_backtrace_fn) — `PUBLIC void set_show_backtrace_fn(show_backtrace_fn_t show_backtrace_fn)`

25. [**`set_trace_with_short_name`**](logging/log.md#set_trace_with_short_name) — `PUBLIC BOOL set_trace_with_short_name(BOOL trace_with_short_name)`

26. [**`set_trace_with_full_name`**](logging/log.md#set_trace_with_full_name) — `PUBLIC BOOL set_trace_with_full_name(BOOL trace_with_full_name)`

27. [**`print_backtrace`**](logging/log.md#print_backtrace) — `PUBLIC void print_backtrace(void)`

28. [**`trace_vjson`**](logging/log.md#trace_vjson) — `PUBLIC void trace_vjson( hgobj gobj, int priority, json_t *jn_data, const char *msgset, const char *fmt, va_list ap )`

29. [**`gobj_trace_msg`**](logging/log.md#gobj_trace_msg) — `PUBLIC void gobj_trace_msg( hgobj gobj, const char *fmt, ... ) JANSSON_ATTRS((format(printf, 2, 3)))`

30. [**`trace_msg0`**](logging/log.md#trace_msg0) — `PUBLIC int trace_msg0(const char *fmt, ...) JANSSON_ATTRS((format(printf, 1, 2)))`

31. [**`gobj_info_msg`**](logging/log.md#gobj_info_msg) — `PUBLIC void gobj_info_msg( hgobj gobj, const char *fmt, ... ) JANSSON_ATTRS((format(printf, 2, 3)))`

32. [**`gobj_trace_json`**](logging/log.md#gobj_trace_json) — `PUBLIC void gobj_trace_json( hgobj gobj, json_t *jn, const char *fmt, ... ) JANSSON_ATTRS((format(printf, 3, 4)))`

33. [**`gobj_trace_dump`**](logging/log.md#gobj_trace_dump) — `PUBLIC void gobj_trace_dump( hgobj gobj, const char *bf, size_t len, const char *fmt, ... ) JANSSON_ATTRS((format(printf, 4, 5)))`

34. [**`print_error`**](logging/log.md#print_error) — `PUBLIC void print_error( pe_flag_t quit, const char *fmt, ... ) JANSSON_ATTRS((format(printf, 2, 3)))`

### `gobj.h` — 245 functions

**Source:** `kernel/c/gobj-c/src/gobj.h`

1. [**`gobj_start_up`**](gobj/startup.md#gobj_start_up) — `PUBLIC int gobj_start_up( int argc, char *argv[], const json_t *jn_global_settings, const persistent_attrs_t *persistent_attrs, json_function_fn global_command_parser, json_function_fn global_statistics_parser, authorization_checker_fn global_authorization_checker, authentication_parser_fn global_authentication_parser )`

2. [**`gobj_set_shutdown`**](gobj/startup.md#gobj_set_shutdown) — `PUBLIC void gobj_set_shutdown(void)`

3. [**`gobj_is_shutdowning`**](gobj/startup.md#gobj_is_shutdowning) — `PUBLIC BOOL gobj_is_shutdowning(void)`

4. [**`gobj_set_exit_code`**](gobj/startup.md#gobj_set_exit_code) — `PUBLIC void gobj_set_exit_code(int exit_code)`

5. [**`gobj_get_exit_code`**](gobj/startup.md#gobj_get_exit_code) — `PUBLIC int gobj_get_exit_code(void)`

6. [**`gobj_end`**](gobj/startup.md#gobj_end) — `PUBLIC void gobj_end(void)`

7. [**`gclass_create`**](gobj/gclass.md#gclass_create) — `PUBLIC hgclass gclass_create( gclass_name_t gclass_name, event_type_t *event_types, states_t *states, const GMETHODS *gmt, const LMETHOD *lmt, const sdata_desc_t *attrs_table, size_t priv_size, const sdata_desc_t *authz_table, const sdata_desc_t *command_table, const trace_level_t *s_user_trace_level, gclass_flag_t gclass_flag )`

8. [**`gclass_add_state`**](gobj/gclass.md#gclass_add_state) — `PUBLIC int gclass_add_state( hgclass gclass, gobj_state_t state_name )`

9. [**`gclass_add_ev_action`**](gobj/gclass.md#gclass_add_ev_action) — `PUBLIC int gclass_add_ev_action( hgclass gclass, gobj_state_t state_name, gobj_event_t event, gobj_action_fn action, gobj_state_t next_state )`

10. [**`gclass_add_event_type`**](gobj/gclass.md#gclass_add_event_type) — `PUBLIC int gclass_add_event_type( hgclass gclass, gobj_event_t event_name, event_flag_t event_flag )`

11. [**`gobj_find_event_type`**](gobj/events_state.md#gobj_find_event_type) — `PUBLIC event_type_t *gobj_find_event_type(const char *event, event_flag_t event_flag, BOOL verbose)`

12. [**`gclass_find_public_event`**](gobj/gclass.md#gclass_find_public_event) — `PUBLIC gobj_event_t gclass_find_public_event(const char *event, BOOL verbose)`

13. [**`gclass_unregister`**](gobj/gclass.md#gclass_unregister) — `PUBLIC void gclass_unregister(hgclass hgclass)`

14. [**`gclass_gclass_name`**](gobj/gclass.md#gclass_gclass_name) — `PUBLIC gclass_name_t gclass_gclass_name(hgclass gclass)`

15. [**`gclass_has_attr`**](gobj/gclass.md#gclass_has_attr) — `PUBLIC BOOL gclass_has_attr(hgclass gclass, const char* name)`

16. [**`gclass_gclass_register`**](gobj/gclass.md#gclass_gclass_register) — `PUBLIC json_t *gclass_gclass_register(void)`

17. [**`gclass_find_by_name`**](gobj/gclass.md#gclass_find_by_name) — `PUBLIC hgclass gclass_find_by_name(gclass_name_t gclass_name)`

18. [**`gclass_event_type`**](gobj/gclass.md#gclass_event_type) — `PUBLIC event_type_t *gclass_event_type(hgclass gclass, gobj_event_t event)`

19. [**`gclass_check_fsm`**](gobj/gclass.md#gclass_check_fsm) — `PUBLIC int gclass_check_fsm(hgclass gclass)`

20. [**`gclass2json`**](gobj/gclass.md#gclass2json) — `PUBLIC json_t *gclass2json(hgclass gclass)`

21. [**`gclass_command_desc`**](gobj/gclass.md#gclass_command_desc) — `PUBLIC const sdata_desc_t *gclass_command_desc(hgclass gclass, const char *name, BOOL verbose)`

22. [**`gobj_services`**](gobj/op.md#gobj_services) — `PUBLIC json_t *gobj_services(void)`

23. [**`gobj_top_services`**](gobj/info.md#gobj_top_services) — `PUBLIC json_t *gobj_top_services(void)`

24. [**`gobj_create`**](gobj/creation.md#gobj_create) — `PUBLIC hgobj gobj_create( const char *gobj_name, gclass_name_t gclass_name, json_t *kw, hgobj parent )`

25. [**`gobj_create2`**](gobj/creation.md#gobj_create2) — `PUBLIC hgobj gobj_create2( const char *gobj_name, gclass_name_t gclass_name, json_t *kw, hgobj parent, gobj_flag_t gobj_flag )`

26. [**`gobj_create_yuno`**](gobj/creation.md#gobj_create_yuno) — `PUBLIC hgobj gobj_create_yuno( const char *gobj_name, gclass_name_t gclass_name, json_t *kw )`

27. [**`gobj_create_service`**](gobj/creation.md#gobj_create_service) — `PUBLIC hgobj gobj_create_service( const char *gobj_name, gclass_name_t gclass_name, json_t *kw, hgobj parent )`

28. [**`gobj_create_default_service`**](gobj/creation.md#gobj_create_default_service) — `PUBLIC hgobj gobj_create_default_service( const char *gobj_name, gclass_name_t gclass_name, json_t *kw, hgobj parent )`

29. [**`gobj_create_volatil`**](gobj/creation.md#gobj_create_volatil) — `PUBLIC hgobj gobj_create_volatil( const char *gobj_name, gclass_name_t gclass_name, json_t *kw, hgobj parent )`

30. [**`gobj_create_pure_child`**](gobj/creation.md#gobj_create_pure_child) — `PUBLIC hgobj gobj_create_pure_child( const char *gobj_name, gclass_name_t gclass_name, json_t *kw, hgobj parent )`

31. [**`gobj_service_factory`**](gobj/creation.md#gobj_service_factory) — `PUBLIC hgobj gobj_service_factory( const char *name, json_t * jn_service_config )`

32. [**`gobj_create_tree`**](gobj/creation.md#gobj_create_tree) — `PUBLIC hgobj gobj_create_tree( hgobj parent, const char *tree_config, json_t *json_config_variables )`

33. [**`gobj_destroy`**](gobj/creation.md#gobj_destroy) — `PUBLIC void gobj_destroy(hgobj gobj)`

34. [**`gobj_destroy_children`**](gobj/creation.md#gobj_destroy_children) — `PUBLIC void gobj_destroy_children(hgobj gobj)`

35. [**`gobj_destroy_named_children`**](gobj/creation.md#gobj_destroy_named_children) — `PUBLIC int gobj_destroy_named_children(hgobj gobj, const char *name)`

36. [**`gobj_load_persistent_attrs`**](gobj/attrs.md#gobj_load_persistent_attrs) — `PUBLIC int gobj_load_persistent_attrs( hgobj gobj, json_t *jn_attrs )`

37. [**`gobj_save_persistent_attrs`**](gobj/attrs.md#gobj_save_persistent_attrs) — `PUBLIC int gobj_save_persistent_attrs( hgobj gobj, json_t *jn_attrs )`

38. [**`gobj_remove_persistent_attrs`**](gobj/attrs.md#gobj_remove_persistent_attrs) — `PUBLIC int gobj_remove_persistent_attrs( hgobj gobj, json_t *jn_attrs )`

39. [**`gobj_list_persistent_attrs`**](gobj/attrs.md#gobj_list_persistent_attrs) — `PUBLIC json_t *gobj_list_persistent_attrs( hgobj gobj, json_t *jn_attrs )`

40. [**`gobj_sdata_create`**](gobj/creation.md#gobj_sdata_create) — `PUBLIC json_t *gobj_sdata_create(hgobj gobj, const sdata_desc_t* schema)`

41. [**`gclass_attr_desc`**](gobj/gclass.md#gclass_attr_desc) — `PUBLIC const sdata_desc_t *gclass_attr_desc(hgclass gclass, const char *attr, BOOL verbose)`

42. [**`gobj_attr_desc`**](gobj/attrs.md#gobj_attr_desc) — `PUBLIC const sdata_desc_t *gobj_attr_desc(hgobj gobj, const char *attr, BOOL verbose)`

43. [**`gobj_attr_type`**](gobj/attrs.md#gobj_attr_type) — `PUBLIC data_type_t gobj_attr_type(hgobj gobj, const char *name)`

44. [**`gclass_authz_desc`**](gobj/gclass.md#gclass_authz_desc) — `PUBLIC const sdata_desc_t *gclass_authz_desc(hgclass gclass)`

45. [**`gobj_has_attr`**](gobj/attrs.md#gobj_has_attr) — `PUBLIC BOOL gobj_has_attr(hgobj hgobj, const char *name)`

46. [**`gobj_is_readable_attr`**](gobj/attrs.md#gobj_is_readable_attr) — `PUBLIC BOOL gobj_is_readable_attr(hgobj gobj, const char *name)`

47. [**`gobj_is_writable_attr`**](gobj/attrs.md#gobj_is_writable_attr) — `PUBLIC BOOL gobj_is_writable_attr(hgobj gobj, const char *name)`

48. [**`gobj_reset_volatil_attrs`**](gobj/attrs.md#gobj_reset_volatil_attrs) — `PUBLIC int gobj_reset_volatil_attrs(hgobj gobj)`

49. [**`gobj_reset_rstats_attrs`**](gobj/attrs.md#gobj_reset_rstats_attrs) — `PUBLIC int gobj_reset_rstats_attrs(hgobj gobj)`

50. [**`gobj_read_attr`**](gobj/attrs.md#gobj_read_attr) — `PUBLIC json_t *gobj_read_attr( hgobj gobj, const char *path, hgobj src )`

51. [**`gobj_read_attrs`**](gobj/attrs.md#gobj_read_attrs) — `PUBLIC json_t *gobj_read_attrs( hgobj gobj, sdata_flag_t include_flag, hgobj src )`

52. [**`gobj_read_user_data`**](gobj/attrs.md#gobj_read_user_data) — `PUBLIC json_t *gobj_read_user_data( hgobj gobj, const char *name )`

53. [**`gobj_write_attr`**](gobj/attrs.md#gobj_write_attr) — `PUBLIC int gobj_write_attr( hgobj gobj, const char *path, json_t *value, hgobj src )`

54. [**`gobj_write_attrs`**](gobj/attrs.md#gobj_write_attrs) — `PUBLIC int gobj_write_attrs( hgobj gobj, json_t *kw, sdata_flag_t include_flag, hgobj src )`

55. [**`gobj_write_user_data`**](gobj/attrs.md#gobj_write_user_data) — `PUBLIC int gobj_write_user_data( hgobj gobj, const char *name, json_t *value )`

56. [**`gobj_has_bottom_attr`**](gobj/attrs.md#gobj_has_bottom_attr) — `PUBLIC BOOL gobj_has_bottom_attr(hgobj gobj_, const char *name)`

57. [**`gobj_kw_get_user_data`**](gobj/info.md#gobj_kw_get_user_data) — `PUBLIC json_t *gobj_kw_get_user_data( hgobj gobj, const char *path, json_t *default_value, kw_flag_t flag )`

58. [**`gobj_read_str_attr`**](gobj/attrs.md#gobj_read_str_attr) — `PUBLIC const char *gobj_read_str_attr(hgobj gobj, const char *name)`

59. [**`gobj_read_bool_attr`**](gobj/attrs.md#gobj_read_bool_attr) — `PUBLIC BOOL gobj_read_bool_attr(hgobj gobj, const char *name)`

60. [**`gobj_read_integer_attr`**](gobj/attrs.md#gobj_read_integer_attr) — `PUBLIC json_int_t gobj_read_integer_attr(hgobj gobj, const char *name)`

61. [**`gobj_read_real_attr`**](gobj/attrs.md#gobj_read_real_attr) — `PUBLIC double gobj_read_real_attr(hgobj gobj, const char *name)`

62. [**`gobj_read_json_attr`**](gobj/attrs.md#gobj_read_json_attr) — `PUBLIC json_t *gobj_read_json_attr(hgobj gobj, const char *name)`

63. [**`gobj_read_pointer_attr`**](gobj/attrs.md#gobj_read_pointer_attr) — `PUBLIC void *gobj_read_pointer_attr(hgobj gobj, const char *name)`

64. [**`gobj_write_str_attr`**](gobj/attrs.md#gobj_write_str_attr) — `PUBLIC int gobj_write_str_attr(hgobj gobj, const char *name, const char *value)`

65. [**`gobj_write_strn_attr`**](gobj/attrs.md#gobj_write_strn_attr) — `PUBLIC int gobj_write_strn_attr(hgobj gobj_, const char *name, const char *s, size_t len)`

66. [**`gobj_write_bool_attr`**](gobj/attrs.md#gobj_write_bool_attr) — `PUBLIC int gobj_write_bool_attr(hgobj gobj, const char *name, BOOL value)`

67. [**`gobj_write_integer_attr`**](gobj/attrs.md#gobj_write_integer_attr) — `PUBLIC int gobj_write_integer_attr(hgobj gobj, const char *name, json_int_t value)`

68. [**`gobj_write_real_attr`**](gobj/attrs.md#gobj_write_real_attr) — `PUBLIC int gobj_write_real_attr(hgobj gobj, const char *name, double value)`

69. [**`gobj_write_json_attr`**](gobj/attrs.md#gobj_write_json_attr) — `PUBLIC int gobj_write_json_attr(hgobj gobj, const char *name, json_t *value)`

70. [**`gobj_write_new_json_attr`**](gobj/attrs.md#gobj_write_new_json_attr) — `PUBLIC int gobj_write_new_json_attr(hgobj gobj, const char *name, json_t *value)`

71. [**`gobj_write_pointer_attr`**](gobj/attrs.md#gobj_write_pointer_attr) — `PUBLIC int gobj_write_pointer_attr(hgobj gobj, const char *name, void *value)`

72. [**`gobj_local_method`**](gobj/op.md#gobj_local_method) — `PUBLIC json_t *gobj_local_method( hgobj gobj, const char *lmethod, json_t *kw, hgobj src )`

73. [**`gobj_start`**](gobj/op.md#gobj_start) — `PUBLIC int gobj_start(hgobj gobj)`

74. [**`gobj_start_children`**](gobj/op.md#gobj_start_children) — `PUBLIC int gobj_start_children(hgobj gobj)`

75. [**`gobj_start_tree`**](gobj/op.md#gobj_start_tree) — `PUBLIC int gobj_start_tree(hgobj gobj)`

76. [**`gobj_stop`**](gobj/op.md#gobj_stop) — `PUBLIC int gobj_stop(hgobj gobj)`

77. [**`gobj_stop_children`**](gobj/op.md#gobj_stop_children) — `PUBLIC int gobj_stop_children(hgobj gobj)`

78. [**`gobj_stop_tree`**](gobj/op.md#gobj_stop_tree) — `PUBLIC int gobj_stop_tree(hgobj gobj)`

79. [**`gobj_play`**](gobj/op.md#gobj_play) — `PUBLIC int gobj_play(hgobj gobj)`

80. [**`gobj_pause`**](gobj/op.md#gobj_pause) — `PUBLIC int gobj_pause(hgobj gobj)`

81. [**`gobj_enable`**](gobj/op.md#gobj_enable) — `PUBLIC int gobj_enable(hgobj gobj)`

82. [**`gobj_disable`**](gobj/op.md#gobj_disable) — `PUBLIC int gobj_disable(hgobj gobj)`

83. [**`gobj_change_parent`**](gobj/op.md#gobj_change_parent) — `PUBLIC int gobj_change_parent(hgobj gobj, hgobj gobj_new_parent)`

84. [**`gobj_command`**](gobj/op.md#gobj_command) — `PUBLIC json_t *gobj_command( hgobj gobj, const char *command, json_t *kw, hgobj src )`

85. [**`gobj_audit_commands`**](gobj/info.md#gobj_audit_commands) — `PUBLIC int gobj_audit_commands( int (*audit_command_cb)( const char *command, json_t *kw, void *user_data ), void *user_data )`

86. [**`gobj_stats`**](gobj/stats.md#gobj_stats) — `PUBLIC json_t * gobj_stats( hgobj gobj, const char* stats, json_t* kw, hgobj src )`

87. [**`gobj_set_bottom_gobj`**](gobj/op.md#gobj_set_bottom_gobj) — `PUBLIC hgobj gobj_set_bottom_gobj(hgobj gobj, hgobj bottom_gobj)`

88. [**`gobj_last_bottom_gobj`**](gobj/op.md#gobj_last_bottom_gobj) — `PUBLIC hgobj gobj_last_bottom_gobj(hgobj gobj)`

89. [**`gobj_bottom_gobj`**](gobj/op.md#gobj_bottom_gobj) — `PUBLIC hgobj gobj_bottom_gobj(hgobj gobj)`

90. [**`gobj_default_service`**](gobj/op.md#gobj_default_service) — `PUBLIC hgobj gobj_default_service(void)`

91. [**`gobj_find_service`**](gobj/op.md#gobj_find_service) — `PUBLIC hgobj gobj_find_service(const char *service, BOOL verbose)`

92. [**`gobj_find_service_by_gclass`**](gobj/op.md#gobj_find_service_by_gclass) — `PUBLIC hgobj gobj_find_service_by_gclass(const char *gclass_name, BOOL verbose)`

93. [**`gobj_nearest_top_service`**](gobj/info.md#gobj_nearest_top_service) — `PUBLIC hgobj gobj_nearest_top_service(hgobj gobj)`

94. [**`gobj_find_gobj`**](gobj/op.md#gobj_find_gobj) — `PUBLIC hgobj gobj_find_gobj(const char *gobj_path)`

95. [**`gobj_first_child`**](gobj/op.md#gobj_first_child) — `PUBLIC hgobj gobj_first_child(hgobj gobj)`

96. [**`gobj_last_child`**](gobj/op.md#gobj_last_child) — `PUBLIC hgobj gobj_last_child(hgobj gobj)`

97. [**`gobj_next_child`**](gobj/op.md#gobj_next_child) — `PUBLIC hgobj gobj_next_child(hgobj child)`

98. [**`gobj_prev_child`**](gobj/op.md#gobj_prev_child) — `PUBLIC hgobj gobj_prev_child(hgobj child)`

99. [**`gobj_child_by_name`**](gobj/op.md#gobj_child_by_name) — `PUBLIC hgobj gobj_child_by_name(hgobj gobj, const char *name)`

100. [**`gobj_child_by_index`**](gobj/op.md#gobj_child_by_index) — `PUBLIC hgobj gobj_child_by_index(hgobj gobj, size_t index)`

101. [**`gobj_child_size`**](gobj/op.md#gobj_child_size) — `PUBLIC size_t gobj_child_size(hgobj gobj)`

102. [**`gobj_child_size2`**](gobj/op.md#gobj_child_size2) — `PUBLIC size_t gobj_child_size2( hgobj gobj_, json_t *jn_filter )`

103. [**`gobj_search_path`**](gobj/op.md#gobj_search_path) — `PUBLIC hgobj gobj_search_path(hgobj gobj, const char *path)`

104. [**`gobj_match_gobj`**](gobj/op.md#gobj_match_gobj) — `PUBLIC BOOL gobj_match_gobj( hgobj gobj, json_t *jn_filter )`

105. [**`gobj_find_child`**](gobj/op.md#gobj_find_child) — `PUBLIC hgobj gobj_find_child( hgobj gobj, json_t *jn_filter )`

106. [**`gobj_find_child_by_tree`**](gobj/op.md#gobj_find_child_by_tree) — `PUBLIC hgobj gobj_find_child_by_tree( hgobj gobj, json_t *jn_filter )`

107. [**`gobj_match_children`**](gobj/op.md#gobj_match_children) — `PUBLIC json_t *gobj_match_children( hgobj gobj, json_t *jn_filter )`

108. [**`gobj_match_children_tree`**](gobj/op.md#gobj_match_children_tree) — `PUBLIC json_t *gobj_match_children_tree( hgobj gobj, json_t *jn_filter )`

109. [**`gobj_free_iter`**](gobj/op.md#gobj_free_iter) — `PUBLIC int gobj_free_iter(json_t *iter)`

110. [**`gobj_walk_gobj_children`**](gobj/op.md#gobj_walk_gobj_children) — `PUBLIC int gobj_walk_gobj_children( hgobj gobj, walk_type_t walk_type, cb_walking_t cb_walking, void *user_data, void *user_data2, void *user_data3 )`

111. [**`gobj_walk_gobj_children_tree`**](gobj/op.md#gobj_walk_gobj_children_tree) — `PUBLIC int gobj_walk_gobj_children_tree( hgobj gobj, walk_type_t walk_type, cb_walking_t cb_walking, void *user_data, void *user_data2, void *user_data3 )`

112. [**`gobj_yuno`**](gobj/info.md#gobj_yuno) — `PUBLIC hgobj gobj_yuno(void)`

113. [**`gobj_yuno_name`**](gobj/info.md#gobj_yuno_name) — `PUBLIC const char *gobj_yuno_name(void)`

114. [**`gobj_yuno_role`**](gobj/info.md#gobj_yuno_role) — `PUBLIC const char *gobj_yuno_role(void)`

115. [**`gobj_yuno_id`**](gobj/info.md#gobj_yuno_id) — `PUBLIC const char *gobj_yuno_id(void)`

116. [**`gobj_yuno_tag`**](gobj/info.md#gobj_yuno_tag) — `PUBLIC const char *gobj_yuno_tag(void)`

117. [**`gobj_yuno_role_plus_name`**](gobj/info.md#gobj_yuno_role_plus_name) — `PUBLIC const char *gobj_yuno_role_plus_name(void)`

118. [**`gobj_yuno_realm_id`**](gobj/info.md#gobj_yuno_realm_id) — `PUBLIC const char *gobj_yuno_realm_id(void)`

119. [**`gobj_yuno_realm_owner`**](gobj/info.md#gobj_yuno_realm_owner) — `PUBLIC const char *gobj_yuno_realm_owner(void)`

120. [**`gobj_yuno_realm_role`**](gobj/info.md#gobj_yuno_realm_role) — `PUBLIC const char *gobj_yuno_realm_role(void)`

121. [**`gobj_yuno_realm_name`**](gobj/info.md#gobj_yuno_realm_name) — `PUBLIC const char *gobj_yuno_realm_name(void)`

122. [**`gobj_yuno_realm_env`**](gobj/info.md#gobj_yuno_realm_env) — `PUBLIC const char *gobj_yuno_realm_env(void)`

123. [**`gobj_yuno_node_owner`**](gobj/info.md#gobj_yuno_node_owner) — `PUBLIC const char *gobj_yuno_node_owner(void)`

124. [**`gobj_name`**](gobj/info.md#gobj_name) — `PUBLIC const char * gobj_name(hgobj gobj)`

125. [**`gobj_gclass_name`**](gobj/info.md#gobj_gclass_name) — `PUBLIC gclass_name_t gobj_gclass_name(hgobj gobj)`

126. [**`gobj_gclass`**](gobj/info.md#gobj_gclass) — `PUBLIC hgclass gobj_gclass(hgobj gobj)`

127. [**`gobj_full_name`**](gobj/info.md#gobj_full_name) — `PUBLIC const char * gobj_full_name(hgobj gobj)`

128. [**`gobj_short_name`**](gobj/info.md#gobj_short_name) — `PUBLIC const char * gobj_short_name(hgobj gobj)`

129. [**`gobj_global_variables`**](gobj/info.md#gobj_global_variables) — `PUBLIC json_t * gobj_global_variables(void)`

130. [**`gobj_add_global_variable`**](gobj/info.md#gobj_add_global_variable) — `PUBLIC int gobj_add_global_variable(const char *name, json_t *value)`

131. [**`gobj_priv_data`**](gobj/info.md#gobj_priv_data) — `PUBLIC void * gobj_priv_data(hgobj gobj)`

132. [**`gobj_parent`**](gobj/info.md#gobj_parent) — `PUBLIC hgobj gobj_parent(hgobj gobj)`

133. [**`gobj_is_destroying`**](gobj/info.md#gobj_is_destroying) — `PUBLIC BOOL gobj_is_destroying(hgobj gobj)`

134. [**`gobj_is_running`**](gobj/info.md#gobj_is_running) — `PUBLIC BOOL gobj_is_running(hgobj gobj)`

135. [**`gobj_is_playing`**](gobj/info.md#gobj_is_playing) — `PUBLIC BOOL gobj_is_playing(hgobj gobj)`

136. [**`gobj_is_service`**](gobj/info.md#gobj_is_service) — `PUBLIC BOOL gobj_is_service(hgobj gobj)`

137. [**`gobj_is_top_service`**](gobj/info.md#gobj_is_top_service) — `PUBLIC BOOL gobj_is_top_service(hgobj gobj)`

138. [**`gobj_is_disabled`**](gobj/info.md#gobj_is_disabled) — `PUBLIC BOOL gobj_is_disabled(hgobj gobj)`

139. [**`gobj_is_volatil`**](gobj/info.md#gobj_is_volatil) — `PUBLIC BOOL gobj_is_volatil(hgobj gobj)`

140. [**`gobj_set_volatil`**](gobj/info.md#gobj_set_volatil) — `PUBLIC int gobj_set_volatil(hgobj gobj, BOOL set)`

140b. [**`gobj_set_manual_start`**](gobj/info.md#gobj_set_manual_start) — `PUBLIC int gobj_set_manual_start(hgobj gobj, BOOL set)`

141. [**`gobj_is_pure_child`**](gobj/info.md#gobj_is_pure_child) — `PUBLIC BOOL gobj_is_pure_child(hgobj gobj)`

142. [**`gobj_is_bottom_gobj`**](gobj/info.md#gobj_is_bottom_gobj) — `PUBLIC BOOL gobj_is_bottom_gobj(hgobj gobj)`

143. [**`gobj_typeof_gclass`**](gobj/info.md#gobj_typeof_gclass) — `PUBLIC BOOL gobj_typeof_gclass(hgobj gobj, const char *gclass_name)`

144. [**`gobj_typeof_inherited_gclass`**](gobj/info.md#gobj_typeof_inherited_gclass) — `PUBLIC BOOL gobj_typeof_inherited_gclass(hgobj gobj, const char *gclass_name)`

145. [**`gobj_command_desc`**](gobj/info.md#gobj_command_desc) — `PUBLIC const sdata_desc_t *gobj_command_desc(hgobj gobj, const char *name, BOOL verbose)`

146. [**`get_sdata_flag_table`**](gobj/info.md#get_sdata_flag_table) — `PUBLIC const char **get_sdata_flag_table(void)`

147. [**`get_attrs_schema`**](gobj/info.md#get_attrs_schema) — `PUBLIC json_t *get_attrs_schema(hgobj gobj)`

148. [**`gobj2json`**](gobj/info.md#gobj2json) — `PUBLIC json_t *gobj2json( hgobj gobj, json_t *jn_filter )`

149. [**`gobj_view_tree`**](gobj/info.md#gobj_view_tree) — `PUBLIC json_t *gobj_view_tree( hgobj gobj, json_t *jn_filter )`

150. [**`gobj_send_event`**](gobj/events_state.md#gobj_send_event) — `PUBLIC int gobj_send_event( hgobj dst, gobj_event_t event, json_t *kw, hgobj src )`

151. [**`gobj_send_event_to_children`**](gobj/events_state.md#gobj_send_event_to_children) — `PUBLIC int gobj_send_event_to_children( hgobj gobj, gobj_event_t event, json_t *kw, hgobj src )`

152. [**`gobj_send_event_to_children_tree`**](gobj/events_state.md#gobj_send_event_to_children_tree) — `PUBLIC int gobj_send_event_to_children_tree( hgobj gobj, gobj_event_t event, json_t *kw, hgobj src )`

153. [**`gobj_change_state`**](gobj/events_state.md#gobj_change_state) — `PUBLIC BOOL gobj_change_state( hgobj gobj, gobj_state_t state_name )`

154. [**`gobj_current_state`**](gobj/events_state.md#gobj_current_state) — `PUBLIC gobj_state_t gobj_current_state(hgobj gobj)`

155. [**`gobj_in_this_state`**](gobj/events_state.md#gobj_in_this_state) — `PUBLIC BOOL gobj_in_this_state(hgobj gobj, gobj_state_t state)`

156. [**`gobj_has_state`**](gobj/events_state.md#gobj_has_state) — `PUBLIC BOOL gobj_has_state(hgobj gobj, gobj_state_t state)`

157. [**`gobj_state_find_by_name`**](gobj/events_state.md#gobj_state_find_by_name) — `PUBLIC hgclass gobj_state_find_by_name(gclass_name_t gclass_name)`

158. [**`gobj_has_event`**](gobj/events_state.md#gobj_has_event) — `PUBLIC BOOL gobj_has_event(hgobj gobj, gobj_event_t event, event_flag_t event_flag)`

159. [**`gobj_has_output_event`**](gobj/events_state.md#gobj_has_output_event) — `PUBLIC BOOL gobj_has_output_event(hgobj gobj, gobj_event_t event, event_flag_t event_flag)`

160. [**`gobj_event_type`**](gobj/events_state.md#gobj_event_type) — `PUBLIC event_type_t *gobj_event_type( hgobj gobj, gobj_event_t event, BOOL include_system_events )`

161. [**`gobj_event_type_by_name`**](gobj/events_state.md#gobj_event_type_by_name) — `PUBLIC event_type_t *gobj_event_type_by_name(hgobj gobj, const char *event_name)`

162. [**`gobj_subs_desc`**](gobj/publish.md#gobj_subs_desc) — `PUBLIC const sdata_desc_t *gobj_subs_desc(void)`

163. [**`gobj_subscribe_event`**](gobj/publish.md#gobj_subscribe_event) — `PUBLIC json_t *gobj_subscribe_event( hgobj publisher, gobj_event_t event, json_t *kw, hgobj subscriber )`

164. [**`gobj_unsubscribe_event`**](gobj/publish.md#gobj_unsubscribe_event) — `PUBLIC int gobj_unsubscribe_event( hgobj publisher, gobj_event_t event, json_t *kw, hgobj subscriber )`

165. [**`gobj_unsubscribe_list`**](gobj/publish.md#gobj_unsubscribe_list) — `PUBLIC int gobj_unsubscribe_list( hgobj gobj, json_t *dl_subs, BOOL force )`

166. [**`gobj_find_subscriptions`**](gobj/publish.md#gobj_find_subscriptions) — `PUBLIC json_t *gobj_find_subscriptions( hgobj gobj, gobj_event_t event, json_t *kw, hgobj subscriber )`

167. [**`gobj_find_subscribings`**](gobj/publish.md#gobj_find_subscribings) — `PUBLIC json_t *gobj_find_subscribings( hgobj gobj, gobj_event_t event, json_t *kw, hgobj publisher )`

168. [**`gobj_list_subscriptions`**](gobj/publish.md#gobj_list_subscriptions) — `PUBLIC json_t *gobj_list_subscriptions( hgobj gobj, gobj_event_t event, json_t *kw, hgobj subscriber )`

169. [**`gobj_list_subscribings`**](gobj/publish.md#gobj_list_subscribings) — `PUBLIC json_t *gobj_list_subscribings( hgobj gobj, gobj_event_t event, json_t *kw, hgobj subscriber )`

170. [**`gobj_publish_event`**](gobj/publish.md#gobj_publish_event) — `PUBLIC int gobj_publish_event( hgobj publisher, gobj_event_t event, json_t *kw )`

171. [**`gobj_authenticate`**](gobj/authz.md#gobj_authenticate) — `PUBLIC json_t *gobj_authenticate( hgobj gobj, json_t *kw, hgobj src )`

172. [**`gobj_authzs`**](gobj/authz.md#gobj_authzs) — `PUBLIC json_t *gobj_authzs( hgobj gobj )`

173. [**`gobj_authz`**](gobj/authz.md#gobj_authz) — `PUBLIC json_t *gobj_authz( hgobj gobj, const char *authz )`

174. [**`gobj_user_has_authz`**](gobj/authz.md#gobj_user_has_authz) — `PUBLIC BOOL gobj_user_has_authz( hgobj gobj, const char *authz, json_t *kw, hgobj src )`

175. [**`gobj_get_global_authz_table`**](gobj/authz.md#gobj_get_global_authz_table) — `PUBLIC const sdata_desc_t *gobj_get_global_authz_table(void)`

176. [**`authzs_list`**](gobj/authz.md#authzs_list) — `PUBLIC json_t *authzs_list( hgobj gobj, const char *authz )`

177. [**`authz_get_level_desc`**](gobj/authz.md#authz_get_level_desc) — `PUBLIC const sdata_desc_t *authz_get_level_desc( const sdata_desc_t *authz_table, const char *authz )`

178. [**`gobj_build_authzs_doc`**](gobj/authz.md#gobj_build_authzs_doc) — `PUBLIC json_t *gobj_build_authzs_doc( hgobj gobj, const char *cmd, json_t *kw )`

179. [**`gobj_set_stat`**](gobj/stats.md#gobj_set_stat) — `PUBLIC json_int_t gobj_set_stat(hgobj gobj, const char *path, json_int_t value)`

180. [**`gobj_incr_stat`**](gobj/stats.md#gobj_incr_stat) — `PUBLIC json_int_t gobj_incr_stat(hgobj gobj, const char *path, json_int_t value)`

181. [**`gobj_decr_stat`**](gobj/stats.md#gobj_decr_stat) — `PUBLIC json_int_t gobj_decr_stat(hgobj gobj, const char *path, json_int_t value)`

182. [**`gobj_get_stat`**](gobj/stats.md#gobj_get_stat) — `PUBLIC json_int_t gobj_get_stat(hgobj gobj, const char *path)`

183. [**`gobj_jn_stats`**](gobj/stats.md#gobj_jn_stats) — `PUBLIC json_t *gobj_jn_stats(hgobj gobj)`

184. [**`gobj_create_resource`**](gobj/resource.md#gobj_create_resource) — `PUBLIC json_t *gobj_create_resource( hgobj gobj, const char *resource, json_t *kw, json_t *jn_options )`

185. [**`gobj_save_resource`**](gobj/resource.md#gobj_save_resource) — `PUBLIC int gobj_save_resource( hgobj gobj, const char *resource, json_t *record, json_t *jn_options )`

186. [**`gobj_delete_resource`**](gobj/resource.md#gobj_delete_resource) — `PUBLIC int gobj_delete_resource( hgobj gobj, const char *resource, json_t *record, json_t *jn_options )`

187. [**`gobj_list_resource`**](gobj/resource.md#gobj_list_resource) — `PUBLIC json_t *gobj_list_resource( hgobj gobj, const char *resource, json_t *jn_filter, json_t *jn_options )`

188. [**`gobj_get_resource`**](gobj/resource.md#gobj_get_resource) — `PUBLIC json_t *gobj_get_resource( hgobj gobj, const char *resource, json_t *jn_filter, json_t *jn_options )`

189. [**`gobj_treedbs`**](gobj/node.md#gobj_treedbs) — `PUBLIC json_t *gobj_treedbs( hgobj gobj, json_t *kw, hgobj src )`

190. [**`gobj_treedb_topics`**](gobj/node.md#gobj_treedb_topics) — `PUBLIC json_t *gobj_treedb_topics( hgobj gobj, const char *treedb_name, json_t *options, hgobj src )`

191. [**`gobj_topic_desc`**](gobj/node.md#gobj_topic_desc) — `PUBLIC json_t *gobj_topic_desc( hgobj gobj, const char *topic_name )`

192. [**`gobj_topic_links`**](gobj/node.md#gobj_topic_links) — `PUBLIC json_t *gobj_topic_links( hgobj gobj, const char *treedb_name, const char *topic_name, json_t *kw, hgobj src )`

193. [**`gobj_topic_hooks`**](gobj/node.md#gobj_topic_hooks) — `PUBLIC json_t *gobj_topic_hooks( hgobj gobj, const char *treedb_name, const char *topic_name, json_t *kw, hgobj src )`

194. [**`gobj_topic_size`**](gobj/node.md#gobj_topic_size) — `PUBLIC size_t gobj_topic_size( hgobj gobj, const char *topic_name, const char *key )`

195. [**`gobj_create_node`**](gobj/node.md#gobj_create_node) — `PUBLIC json_t *gobj_create_node( hgobj gobj, const char *topic_name, json_t *kw, json_t *jn_options, hgobj src )`

196. [**`gobj_update_node`**](gobj/node.md#gobj_update_node) — `PUBLIC json_t *gobj_update_node( hgobj gobj, const char *topic_name, json_t *kw, json_t *jn_options, hgobj src )`

197. [**`gobj_delete_node`**](gobj/node.md#gobj_delete_node) — `PUBLIC int gobj_delete_node( hgobj gobj, const char *topic_name, json_t *kw, json_t *jn_options, hgobj src )`

198. [**`gobj_link_nodes`**](gobj/node.md#gobj_link_nodes) — `PUBLIC int gobj_link_nodes( hgobj gobj, const char *hook, const char *parent_topic_name, json_t *parent_record, const char *child_topic_name, json_t *child_record, hgobj src )`

199. [**`gobj_unlink_nodes`**](gobj/node.md#gobj_unlink_nodes) — `PUBLIC int gobj_unlink_nodes( hgobj gobj, const char *hook, const char *parent_topic_name, json_t *parent_record, const char *child_topic_name, json_t *child_record, hgobj src )`

200. [**`gobj_get_node`**](gobj/node.md#gobj_get_node) — `PUBLIC json_t *gobj_get_node( hgobj gobj, const char *topic_name, json_t *kw, json_t *jn_options, hgobj src )`

201. [**`gobj_list_nodes`**](gobj/node.md#gobj_list_nodes) — `PUBLIC json_t *gobj_list_nodes( hgobj gobj, const char *topic_name, json_t *jn_filter, json_t *jn_options, hgobj src )`

202. [**`gobj_list_instances`**](gobj/node.md#gobj_list_instances) — `PUBLIC json_t *gobj_list_instances( hgobj gobj, const char *topic_name, const char *pkey2_field, json_t *jn_filter, json_t *jn_options, hgobj src )`

203. [**`gobj_node_parents`**](gobj/node.md#gobj_node_parents) — `PUBLIC json_t *gobj_node_parents( hgobj gobj, const char *topic_name, json_t *kw, const char *link, json_t *jn_options, hgobj src )`

204. [**`gobj_node_children`**](gobj/node.md#gobj_node_children) — `PUBLIC json_t *gobj_node_children( hgobj gobj, const char *topic_name, json_t *kw, const char *hook, json_t *jn_filter, json_t *jn_options, hgobj src )`

205. [**`gobj_topic_jtree`**](gobj/node.md#gobj_topic_jtree) — `PUBLIC json_t *gobj_topic_jtree( hgobj gobj, const char *topic_name, const char *hook, const char *rename_hook, json_t *kw, json_t *jn_filter, json_t *jn_options, hgobj src )`

206. [**`gobj_node_tree`**](gobj/node.md#gobj_node_tree) — `PUBLIC json_t *gobj_node_tree( hgobj gobj, const char *topic_name, json_t *kw, json_t *jn_options, hgobj src )`

207. [**`gobj_shoot_snap`**](gobj/node.md#gobj_shoot_snap) — `PUBLIC int gobj_shoot_snap( hgobj gobj, const char *tag, json_t *kw, hgobj src )`

208. [**`gobj_activate_snap`**](gobj/node.md#gobj_activate_snap) — `PUBLIC int gobj_activate_snap( hgobj gobj, const char *tag, json_t *kw, hgobj src )`

209. [**`gobj_list_snaps`**](gobj/node.md#gobj_list_snaps) — `PUBLIC json_t *gobj_list_snaps( hgobj gobj, json_t *filter, hgobj src )`

210. [**`gobj_repr_global_trace_levels`**](logging/trace.md#gobj_repr_global_trace_levels) — `PUBLIC json_t * gobj_repr_global_trace_levels(void)`

211. [**`gobj_repr_gclass_trace_levels`**](logging/trace.md#gobj_repr_gclass_trace_levels) — `PUBLIC json_t * gobj_repr_gclass_trace_levels(const char *gclass_name)`

212. [**`gobj_trace_level_list`**](logging/trace.md#gobj_trace_level_list) — `PUBLIC json_t *gobj_trace_level_list(hgclass gclass)`

213. [**`gobj_get_global_trace_level`**](logging/trace.md#gobj_get_global_trace_level) — `PUBLIC json_t *gobj_get_global_trace_level(void)`

214. [**`gobj_get_gclass_trace_level`**](logging/trace.md#gobj_get_gclass_trace_level) — `PUBLIC json_t *gobj_get_gclass_trace_level(hgclass gclass)`

215. [**`gobj_get_gclass_trace_no_level`**](logging/trace.md#gobj_get_gclass_trace_no_level) — `PUBLIC json_t *gobj_get_gclass_trace_no_level(hgclass gclass)`

216. [**`gobj_get_gobj_trace_level`**](logging/trace.md#gobj_get_gobj_trace_level) — `PUBLIC json_t *gobj_get_gobj_trace_level(hgobj gobj)`

217. [**`gobj_get_gobj_trace_no_level`**](logging/trace.md#gobj_get_gobj_trace_no_level) — `PUBLIC json_t *gobj_get_gobj_trace_no_level(hgobj gobj)`

218. [**`gobj_get_gclass_trace_level_list`**](logging/trace.md#gobj_get_gclass_trace_level_list) — `PUBLIC json_t *gobj_get_gclass_trace_level_list(hgclass gclass)`

219. [**`gobj_get_gclass_trace_no_level_list`**](logging/trace.md#gobj_get_gclass_trace_no_level_list) — `PUBLIC json_t *gobj_get_gclass_trace_no_level_list(hgclass gclass)`

220. [**`gobj_get_gobj_trace_level_tree`**](logging/trace.md#gobj_get_gobj_trace_level_tree) — `PUBLIC json_t *gobj_get_gobj_trace_level_tree(hgobj gobj)`

221. [**`gobj_get_gobj_trace_no_level_tree`**](logging/trace.md#gobj_get_gobj_trace_no_level_tree) — `PUBLIC json_t *gobj_get_gobj_trace_no_level_tree(hgobj gobj)`

222. [**`gobj_global_trace_level`**](logging/trace.md#gobj_global_trace_level) — `PUBLIC uint32_t gobj_global_trace_level(void)`

223. [**`gobj_global_trace_level2`**](logging/trace.md#gobj_global_trace_level2) — `PUBLIC uint32_t gobj_global_trace_level2(void)`

224. [**`gobj_trace_level`**](logging/trace.md#gobj_trace_level) — `PUBLIC uint32_t gobj_trace_level(hgobj gobj)`

225. [**`gobj_trace_no_level`**](logging/trace.md#gobj_trace_no_level) — `PUBLIC uint32_t gobj_trace_no_level(hgobj gobj)`

226. [**`gobj_is_level_tracing`**](logging/trace.md#gobj_is_level_tracing) — `PUBLIC BOOL gobj_is_level_tracing(hgobj gobj, uint32_t level)`

227. [**`gobj_is_level_not_tracing`**](logging/trace.md#gobj_is_level_not_tracing) — `PUBLIC BOOL gobj_is_level_not_tracing(hgobj gobj, uint32_t level)`

228. [**`gobj_set_gobj_trace`**](logging/trace.md#gobj_set_gobj_trace) — `PUBLIC int gobj_set_gobj_trace(hgobj gobj, const char* level, BOOL set, json_t* kw)`

229. [**`gobj_set_gclass_trace`**](logging/trace.md#gobj_set_gclass_trace) — `PUBLIC int gobj_set_gclass_trace(hgclass gclass, const char *level, BOOL set)`

230. [**`gobj_set_deep_tracing`**](logging/trace.md#gobj_set_deep_tracing) — `PUBLIC int gobj_set_deep_tracing(int level)`

231. [**`gobj_get_deep_tracing`**](logging/trace.md#gobj_get_deep_tracing) — `PUBLIC int gobj_get_deep_tracing(void)`

232. [**`gobj_set_global_trace`**](logging/trace.md#gobj_set_global_trace) — `PUBLIC int gobj_set_global_trace(const char *level, BOOL set)`

233. [**`gobj_set_global_no_trace`**](logging/trace.md#gobj_set_global_no_trace) — `PUBLIC int gobj_set_global_no_trace(const char *level, BOOL set)`

234. [**`gobj_set_global_trace2`**](logging/trace.md#gobj_set_global_trace2) — `PUBLIC int gobj_set_global_trace2(uint32_t level, BOOL set)`

235. [**`gobj_set_global_no_trace2`**](logging/trace.md#gobj_set_global_no_trace2) — `PUBLIC int gobj_set_global_no_trace2(uint32_t level, BOOL set)`

236. [**`gobj_load_trace_filter`**](logging/trace.md#gobj_load_trace_filter) — `PUBLIC int gobj_load_trace_filter(hgclass gclass, json_t *jn_trace_filter)`

237. [**`gobj_add_trace_filter`**](logging/trace.md#gobj_add_trace_filter) — `PUBLIC int gobj_add_trace_filter(hgclass gclass, const char *attr, const char *value)`

238. [**`gobj_remove_trace_filter`**](logging/trace.md#gobj_remove_trace_filter) — `PUBLIC int gobj_remove_trace_filter(hgclass gclass, const char *attr, const char *value)`

239. [**`gobj_get_trace_filter`**](logging/trace.md#gobj_get_trace_filter) — `PUBLIC json_t *gobj_get_trace_filter(hgclass gclass)`

240. [**`gobj_set_gclass_no_trace`**](logging/trace.md#gobj_set_gclass_no_trace) — `PUBLIC int gobj_set_gclass_no_trace(hgclass gclass, const char *level, BOOL set)`

241. [**`gobj_set_gobj_no_trace`**](logging/trace.md#gobj_set_gobj_no_trace) — `PUBLIC int gobj_set_gobj_no_trace(hgobj gobj, const char *level, BOOL set)`

242. [**`trace_machine`**](logging/trace.md#trace_machine) — `PUBLIC void trace_machine(const char *fmt, ...) JANSSON_ATTRS((format(printf, 1, 2)))`

243. [**`trace_machine2`**](logging/trace.md#trace_machine2) — `PUBLIC void trace_machine2(const char *fmt, ...) JANSSON_ATTRS((format(printf, 1, 2)))`

244. [**`gobj_set_trace_machine_format`**](logging/trace.md#gobj_set_trace_machine_format) — `PUBLIC void gobj_set_trace_machine_format(int format)`

245. [**`tab`**](logging/trace.md#tab) — `PUBLIC char *tab(char *bf, int bflen)`

### `helpers.h` — 167 functions

**Source:** `kernel/c/gobj-c/src/helpers.h`

1. [**`newdir`**](helpers/file_system.md#newdir) — `PUBLIC int newdir(const char *path, int xpermission)`

2. [**`newfile`**](helpers/file_system.md#newfile) — `PUBLIC int newfile(const char *path, int rpermission, BOOL overwrite)`

3. [**`open_exclusive`**](helpers/file_system.md#open_exclusive) — `PUBLIC int open_exclusive(const char *path, int flags, int rpermission)`

4. [**`filesize`**](helpers/file_system.md#filesize) — `PUBLIC off_t filesize(const char *path)`

5. [**`filesize2`**](helpers/file_system.md#filesize2) — `PUBLIC off_t filesize2(int fd)`

6. [**`lock_file`**](helpers/file_system.md#lock_file) — `PUBLIC int lock_file(int fd)`

7. [**`unlock_file`**](helpers/file_system.md#unlock_file) — `PUBLIC int unlock_file(int fd)`

8. [**`is_regular_file`**](helpers/file_system.md#is_regular_file) — `PUBLIC BOOL is_regular_file(const char *path)`

9. [**`is_directory`**](helpers/file_system.md#is_directory) — `PUBLIC BOOL is_directory(const char *path)`

10. [**`file_size`**](helpers/file_system.md#file_size) — `PUBLIC off_t file_size(const char *path)`

11. [**`file_permission`**](helpers/file_system.md#file_permission) — `PUBLIC mode_t file_permission(const char *path)`

12. [**`file_exists`**](helpers/file_system.md#file_exists) — `PUBLIC BOOL file_exists(const char *directory, const char *filename)`

13. [**`subdir_exists`**](helpers/file_system.md#subdir_exists) — `PUBLIC BOOL subdir_exists(const char *directory, const char *subdir)`

14. [**`file_remove`**](helpers/file_system.md#file_remove) — `PUBLIC int file_remove(const char *directory, const char *filename)`

15. [**`mkrdir`**](helpers/file_system.md#mkrdir) — `PUBLIC int mkrdir(const char *path, int xpermission)`

16. [**`rmrdir`**](helpers/file_system.md#rmrdir) — `PUBLIC int rmrdir(const char *root_dir)`

17. [**`rmrcontentdir`**](helpers/file_system.md#rmrcontentdir) — `PUBLIC int rmrcontentdir(const char *root_dir)`

18. [**`delete_right_char`**](helpers/string_helper.md#delete_right_char) — `PUBLIC char *delete_right_char(char *s, char x)`

19. [**`delete_left_char`**](helpers/string_helper.md#delete_left_char) — `PUBLIC char *delete_left_char(char *s, char x)`

20. [**`build_path`**](helpers/string_helper.md#build_path) — `PUBLIC char *build_path(char *bf, size_t bfsize, ...)`

21. [**`get_last_segment`**](helpers/string_helper.md#get_last_segment) — `PUBLIC char *get_last_segment(char *path)`

22. [**`pop_last_segment`**](helpers/string_helper.md#pop_last_segment) — `PUBLIC char *pop_last_segment(char *path)`

23. [**`helper_quote2doublequote`**](helpers/string_helper.md#helper_quote2doublequote) — `PUBLIC char *helper_quote2doublequote(char *str)`

24. [**`helper_doublequote2quote`**](helpers/string_helper.md#helper_doublequote2quote) — `PUBLIC char *helper_doublequote2quote(char *str)`

25. [**`all_numbers`**](helpers/string_helper.md#all_numbers) — `PUBLIC BOOL all_numbers(const char* s)`

26. [**`nice_size`**](helpers/string_helper.md#nice_size) — `PUBLIC void nice_size(char* bf, size_t bfsize, uint64_t bytes, BOOL b1024)`

27. [**`delete_right_blanks`**](helpers/string_helper.md#delete_right_blanks) — `PUBLIC void delete_right_blanks(char *s)`

28. [**`delete_left_blanks`**](helpers/string_helper.md#delete_left_blanks) — `PUBLIC void delete_left_blanks(char *s)`

29. [**`left_justify`**](helpers/string_helper.md#left_justify) — `PUBLIC void left_justify(char *s)`

30. [**`strntoupper`**](helpers/string_helper.md#strntoupper) — `PUBLIC char *strntoupper(char* s, size_t n)`

31. [**`strntolower`**](helpers/string_helper.md#strntolower) — `PUBLIC char *strntolower(char* s, size_t n)`

32. [**`translate_string`**](helpers/string_helper.md#translate_string) — `PUBLIC char *translate_string( char *to, int tolen, const char *from, const char *mk_to, const char *mk_from )`

33. [**`change_char`**](helpers/string_helper.md#change_char) — `PUBLIC int change_char(char *s, char old_c, char new_c)`

34. [**`get_parameter`**](helpers/string_helper.md#get_parameter) — `PUBLIC char *get_parameter(char *s, char **save_ptr)`

35. [**`get_key_value_parameter`**](helpers/string_helper.md#get_key_value_parameter) — `PUBLIC char *get_key_value_parameter(char *s, char **key, char **save_ptr)`

36. [**`split2`**](helpers/string_helper.md#split2) — `PUBLIC const char **split2(const char *str, const char *delim, int *list_size)`

37. [**`split_free2`**](helpers/string_helper.md#split_free2) — `PUBLIC void split_free2(const char **list)`

38. [**`split3`**](helpers/string_helper.md#split3) — `PUBLIC const char **split3(const char *str, const char *delim, int *plist_size)`

39. [**`split_free3`**](helpers/string_helper.md#split_free3) — `PUBLIC void split_free3(const char **list)`

40. [**`str_concat`**](helpers/string_helper.md#str_concat) — `PUBLIC char *str_concat(const char *str1, const char *str2)`

41. [**`str_concat3`**](helpers/string_helper.md#str_concat3) — `PUBLIC char *str_concat3(const char *str1, const char *str2, const char *str3)`

42. [**`str_concat_free`**](helpers/string_helper.md#str_concat_free) — `PUBLIC void str_concat_free(char *s)`

43. [**`idx_in_list`**](helpers/string_helper.md#idx_in_list) — `PUBLIC int idx_in_list(const char **list, const char *str, BOOL ignore_case)`

44. [**`str_in_list`**](helpers/string_helper.md#str_in_list) — `PUBLIC BOOL str_in_list(const char **list, const char *str, BOOL ignore_case)`

45. [**`json_config`**](helpers/json_helper.md#json_config) — `PUBLIC json_t *json_config( BOOL print_verbose_config, BOOL print_final_config, const char *fixed_config, const char *variable_config, const char *config_json_file, const char *parameter_config, pe_flag_t quit )`

46. [**`json_replace_var_custom`**](helpers/json_helper.md#json_replace_var_custom) — `PUBLIC json_t *json_replace_var_custom( json_t *jn_dict, json_t *jn_vars, const char *open, const char *close )`

47. [**`json_replace_var`**](helpers/json_helper.md#json_replace_var) — `PUBLIC json_t *json_replace_var( json_t *jn_dict, json_t *jn_vars )`

48. [**`load_persistent_json`**](helpers/json_helper.md#load_persistent_json) — `PUBLIC json_t *load_persistent_json( hgobj gobj, const char *directory, const char *filename, log_opt_t on_critical_error, int *pfd, BOOL exclusive, BOOL silence )`

49. [**`load_json_from_file`**](helpers/json_helper.md#load_json_from_file) — `PUBLIC json_t *load_json_from_file( hgobj gobj, const char *directory, const char *filename, log_opt_t on_critical_error )`

50. [**`save_json_to_file`**](helpers/json_helper.md#save_json_to_file) — `PUBLIC int save_json_to_file( hgobj gobj, const char *directory, const char *filename, int xpermission, int rpermission, log_opt_t on_critical_error, BOOL create, BOOL only_read, json_t *jn_data )`

51. [**`create_json_record`**](helpers/json_helper.md#create_json_record) — `PUBLIC json_t *create_json_record( hgobj gobj, const json_desc_t *json_desc )`

52. [**`json_desc_to_schema`**](helpers/json_helper.md#json_desc_to_schema) — `PUBLIC json_t *json_desc_to_schema(const json_desc_t *json_desc)`

53. [**`bits2jn_strlist`**](helpers/json_helper.md#bits2jn_strlist) — `PUBLIC json_t *bits2jn_strlist( const char **strings_table, uint64_t bits )`

54. [**`bits2gbuffer`**](helpers/json_helper.md#bits2gbuffer) — `PUBLIC gbuffer_t *bits2gbuffer( const char **strings_table, uint64_t bits )`

55. [**`strings2bits`**](helpers/json_helper.md#strings2bits) — `PUBLIC uint64_t strings2bits( const char **strings_table, const char *str, const char *separators )`

56. [**`json_list_str_index`**](helpers/json_helper.md#json_list_str_index) — `PUBLIC int json_list_str_index(json_t *jn_list, const char *str, BOOL ignore_case)`

57. [**`json_list_int`**](helpers/json_helper.md#json_list_int) — `PUBLIC json_int_t json_list_int(json_t *jn_list, size_t idx)`

58. [**`json_list_int_index`**](helpers/json_helper.md#json_list_int_index) — `PUBLIC int json_list_int_index(json_t *jn_list, json_int_t value)`

59. [**`json_list_find`**](helpers/json_helper.md#json_list_find) — `PUBLIC int json_list_find(json_t *list, json_t *value)`

60. [**`json_list_update`**](helpers/json_helper.md#json_list_update) — `PUBLIC int json_list_update(json_t *list, json_t *other, BOOL as_set_type)`

61. [**`json_is_range`**](helpers/json_helper.md#json_is_range) — `PUBLIC BOOL json_is_range(json_t *list, json_int_t *pfirst, json_int_t *psecond)`

62. [**`json_range_list`**](helpers/json_helper.md#json_range_list) — `PUBLIC json_t *json_range_list(json_t *list)`

63. [**`json_listsrange2set`**](helpers/json_helper.md#json_listsrange2set) — `PUBLIC json_t *json_listsrange2set( json_t *listsrange )`

64. [**`json_dict_recursive_update`**](helpers/json_helper.md#json_dict_recursive_update) — `PUBLIC int json_dict_recursive_update(json_t *object, json_t *other, BOOL overwrite)`

65. [**`jn2real`**](helpers/json_helper.md#jn2real) — `PUBLIC double jn2real( json_t *jn_var )`

66. [**`jn2integer`**](helpers/json_helper.md#jn2integer) — `PUBLIC json_int_t jn2integer( json_t *jn_var )`

67. [**`jn2string`**](helpers/json_helper.md#jn2string) — `PUBLIC char *jn2string( json_t *jn_var )`

68. [**`jn2bool`**](helpers/json_helper.md#jn2bool) — `PUBLIC BOOL jn2bool( json_t *jn_var )`

69. [**`cmp_two_simple_json`**](helpers/json_helper.md#cmp_two_simple_json) — `PUBLIC int cmp_two_simple_json( json_t *jn_var1, json_t *jn_var2 )`

70. [**`json_is_identical`**](helpers/json_helper.md#json_is_identical) — `PUBLIC BOOL json_is_identical( json_t *kw1, json_t *kw2 )`

71. [**`anystring2json`**](helpers/json_helper.md#anystring2json) — `PUBLIC json_t *anystring2json(const char *bf, size_t len, BOOL verbose)`

72. [**`anyfile2json`**](helpers/json_helper.md#anyfile2json) — `PUBLIC json_t *anyfile2json(const char *path, BOOL verbose)`

73. [**`string2json`**](helpers/json_helper.md#string2json) — `PUBLIC json_t *string2json(const char *str, BOOL verbose)`

74. [**`json_config_string2json`**](helpers/json_helper.md#json_config_string2json) — `PUBLIC json_t *json_config_string2json(const char *bf, BOOL verbose)`

75. [**`set_real_precision`**](helpers/json_helper.md#set_real_precision) — `PUBLIC int set_real_precision(int precision)`

76. [**`get_real_precision`**](helpers/json_helper.md#get_real_precision) — `PUBLIC int get_real_precision(void)`

77. [**`json2str`**](helpers/json_helper.md#json2str) — `PUBLIC char *json2str(const json_t *jn)`

78. [**`json2uglystr`**](helpers/json_helper.md#json2uglystr) — `PUBLIC char *json2uglystr(const json_t *jn)`

79. [**`json_size`**](helpers/json_helper.md#json_size) — `PUBLIC size_t json_size(json_t *value)`

80. [**`json_check_refcounts`**](helpers/json_helper.md#json_check_refcounts) — `PUBLIC int json_check_refcounts( json_t *kw, int max_refcount, int *result )`

81. [**`json_print_refcounts`**](helpers/json_helper.md#json_print_refcounts) — `PUBLIC int json_print_refcounts( json_t *jn, int level )`

82. [**`json_str_in_list`**](helpers/json_helper.md#json_str_in_list) — `PUBLIC BOOL json_str_in_list(hgobj gobj, json_t *jn_list, const char *str, BOOL ignore_case)`

83. [**`walk_dir_tree`**](helpers/directory_walk.md#walk_dir_tree) — `PUBLIC int walk_dir_tree( hgobj gobj, const char *root_dir, const char *pattern, wd_option opt, walkdir_cb cb, void *user_data )`

84. [**`find_files_with_suffix_array`**](helpers/directory_walk.md#find_files_with_suffix_array) — `PUBLIC int find_files_with_suffix_array( hgobj gobj, const char *directory, const char *suffix, dir_array_t *da )`

85. [**`dir_array_sort`**](helpers/directory_walk.md#dir_array_sort) — `PUBLIC void dir_array_sort( dir_array_t *da )`

86. [**`dir_array_free`**](helpers/directory_walk.md#dir_array_free) — `PUBLIC void dir_array_free( dir_array_t *da )`

87. [**`walk_dir_array`**](helpers/directory_walk.md#walk_dir_array) — `PUBLIC int walk_dir_array( hgobj gobj, const char *root_dir, const char *re, wd_option opt, dir_array_t *da )`

88. [**`get_ordered_filename_array`**](helpers/directory_walk.md#get_ordered_filename_array) — `PUBLIC int get_ordered_filename_array( hgobj gobj, const char *root_dir, const char *re, wd_option opt, dir_array_t *da )`

89. [**`tm_to_time_t`**](helpers/time_date.md#tm_to_time_t) — `PUBLIC time_t tm_to_time_t(const struct tm *tm)`

90. [**`date_mode_from_type`**](helpers/time_date.md#date_mode_from_type) — `PUBLIC struct date_mode *date_mode_from_type(enum date_mode_type type)`

91. [**`show_date`**](helpers/time_date.md#show_date) — `PUBLIC const char *show_date(timestamp_t time, int timezone, const struct date_mode *mode)`

92. [**`show_date_relative`**](helpers/time_date.md#show_date_relative) — `PUBLIC void show_date_relative( timestamp_t time, char *timebuf, int timebufsize )`

93. [**`parse_date`**](helpers/time_date.md#parse_date) — `PUBLIC int parse_date( const char *date, char *out, int outsize )`

94. [**`parse_date_basic`**](helpers/time_date.md#parse_date_basic) — `PUBLIC int parse_date_basic(const char *date, timestamp_t *timestamp, int *offset)`

95. [**`parse_expiry_date`**](helpers/time_date.md#parse_expiry_date) — `PUBLIC int parse_expiry_date(const char *date, timestamp_t *timestamp)`

96. [**`datestamp`**](helpers/time_date.md#datestamp) — `PUBLIC void datestamp( char *out, int outsize )`

97. [**`parse_date_format`**](helpers/time_date.md#parse_date_format) — `PUBLIC void parse_date_format(const char *format, struct date_mode *mode)`

98. [**`date_overflows`**](helpers/time_date.md#date_overflows) — `PUBLIC int date_overflows(timestamp_t date)`

99. [**`hex2bin`**](helpers/string_helper.md#hex2bin) — `PUBLIC char *hex2bin(char *bf, int bfsize, const char *hex, size_t hex_len, size_t *out_len)`

100. [**`bin2hex`**](helpers/string_helper.md#bin2hex) — `PUBLIC char *bin2hex(char *bf, int bfsize, const uint8_t *bin, size_t bin_len)`

101. [**`tdump`**](helpers/backtrace.md#tdump) — `PUBLIC void tdump(const char *prefix, const uint8_t *s, size_t len, view_fn_t view, int nivel)`

102. [**`tdump2json`**](helpers/backtrace.md#tdump2json) — `PUBLIC json_t *tdump2json(const uint8_t *s, size_t len)`

103. [**`print_json`**](helpers/json_helper.md#print_json) — `PUBLIC int print_json(const char *label, json_t *jn)`

104. [**`debug_json`**](helpers/json_helper.md#debug_json) — `PUBLIC int debug_json(const char *label, json_t *jn, BOOL verbose)`

105. [**`debug_json2`**](helpers/json_helper.md#debug_json2) — `PUBLIC int debug_json2(json_t *jn, const char *format, ...)JANSSON_ATTRS((format(printf, 2, 3)))`

106. [**`current_timestamp`**](helpers/time_date.md#current_timestamp) — `PUBLIC char *current_timestamp(char *bf, size_t bfsize)`

107. [**`tm2timestamp`**](helpers/time_date.md#tm2timestamp) — `PUBLIC char *tm2timestamp(char *bf, int bfsize, struct tm *tm)`

108. [**`t2timestamp`**](helpers/time_date.md#t2timestamp) — `PUBLIC char *t2timestamp(char *bf, int bfsize, time_t t, BOOL local)`

109. [**`start_sectimer`**](helpers/time_date.md#start_sectimer) — `PUBLIC time_t start_sectimer(time_t seconds)`

110. [**`test_sectimer`**](helpers/time_date.md#test_sectimer) — `PUBLIC BOOL test_sectimer(time_t value)`

111. [**`start_msectimer`**](helpers/time_date.md#start_msectimer) — `PUBLIC uint64_t start_msectimer(uint64_t milliseconds)`

112. [**`test_msectimer`**](helpers/time_date.md#test_msectimer) — `PUBLIC BOOL test_msectimer(uint64_t value)`

113. [**`time_in_milliseconds_monotonic`**](helpers/time_date.md#time_in_milliseconds_monotonic) — `PUBLIC uint64_t time_in_milliseconds_monotonic(void)`

114. [**`time_in_milliseconds`**](helpers/time_date.md#time_in_milliseconds) — `PUBLIC uint64_t time_in_milliseconds(void)`

115. [**`time_in_seconds`**](helpers/time_date.md#time_in_seconds) — `PUBLIC time_t time_in_seconds(void)`

116. [**`cpu_usage`**](helpers/misc.md#cpu_usage) — `PUBLIC uint64_t cpu_usage(void)`

117. [**`cpu_usage_percent`**](helpers/misc.md#cpu_usage_percent) — `PUBLIC double cpu_usage_percent( uint64_t *last_cpu_ticks, uint64_t *last_ms )`

118. [**`htonll`**](helpers/time_date.md#htonll) — `PUBLIC uint64_t htonll(uint64_t value)`

119. [**`ntohll`**](helpers/time_date.md#ntohll) — `PUBLIC uint64_t ntohll(uint64_t value)`

120. [**`list_open_files`**](helpers/time_date.md#list_open_files) — `PUBLIC void list_open_files(void)`

121. [**`gmtime2timezone`**](helpers/time_date.md#gmtime2timezone) — `PUBLIC time_t gmtime2timezone(time_t t, const char *tz, struct tm *ltm, time_t *offset)`

122. [**`formatdate`**](helpers/time_date.md#formatdate) — `PUBLIC char *formatdate(time_t t, char *bf, int bfsize, const char *format)`

123. [**`count_char`**](helpers/string_helper.md#count_char) — `PUBLIC int count_char(const char *s, char c)`

124. [**`get_hostname`**](helpers/misc.md#get_hostname) — `PUBLIC const char *get_hostname(void)`

125. [**`create_random_uuid`**](helpers/misc.md#create_random_uuid) — `PUBLIC int create_random_uuid(char *bf, int bfsize)`

126. [**`node_uuid`**](helpers/misc.md#node_uuid) — `PUBLIC const char *node_uuid(void)`

127. [**`is_metadata_key`**](helpers/json_helper.md#is_metadata_key) — `PUBLIC BOOL is_metadata_key(const char *key)`

128. [**`is_private_key`**](helpers/json_helper.md#is_private_key) — `PUBLIC BOOL is_private_key(const char *key)`

129. [**`comm_prot_register`**](helpers/common_protocol.md#comm_prot_register) — `PUBLIC int comm_prot_register(gclass_name_t gclass_name, const char *schema)`

130. [**`comm_prot_get_gclass`**](helpers/common_protocol.md#comm_prot_get_gclass) — `PUBLIC gclass_name_t comm_prot_get_gclass(const char *schema)`

131. [**`comm_prot_free`**](helpers/common_protocol.md#comm_prot_free) — `PUBLIC void comm_prot_free(void)`

132. [**`launch_daemon`**](helpers/daemon_launcher.md#launch_daemon) — `PUBLIC int launch_daemon( BOOL redirect_stdio_to_null, const char *program, ... )`

133. [**`parse_url`**](helpers/url_parsing.md#parse_url) — `PUBLIC int parse_url( hgobj gobj, const char *uri, char *schema, size_t schema_size, char *host, size_t host_size, char *port, size_t port_size, char *path, size_t path_size, char *query, size_t query_size, BOOL no_schema )`

134. [**`get_url_schema`**](helpers/url_parsing.md#get_url_schema) — `PUBLIC int get_url_schema( hgobj gobj, const char *uri, char *schema, size_t schema_size )`

135. [**`free_ram_in_kb`**](helpers/misc.md#free_ram_in_kb) — `PUBLIC unsigned long free_ram_in_kb(void)`

136. [**`total_ram_in_kb`**](helpers/misc.md#total_ram_in_kb) — `PUBLIC unsigned long total_ram_in_kb(void)`

137. [**`read_process_cmdline`**](helpers/file_system.md#read_process_cmdline) — `PUBLIC int read_process_cmdline(char *bf, size_t bfsize, pid_t pid)`

138. [**`copyfile`**](helpers/file_system.md#copyfile) — `PUBLIC int copyfile( const char* source, const char* destination, int permission, BOOL overwrite )`

139. [**`set_nonblocking`**](helpers/file_system.md#set_nonblocking) — `PUBLIC int set_nonblocking(int fd)`

140. [**`set_cloexec`**](helpers/file_system.md#set_cloexec) — `PUBLIC int set_cloexec(int fd)`

141. [**`upper`**](helpers/string_helper.md#upper) — `PUBLIC char *upper(char *s)`

142. [**`lower`**](helpers/string_helper.md#lower) — `PUBLIC char *lower(char *s)`

143. [**`capitalize`**](helpers/string_helper.md#capitalize) — `PUBLIC char *capitalize(char *s)`

144. [**`set_tcp_socket_options`**](helpers/common_protocol.md#set_tcp_socket_options) — `PUBLIC int set_tcp_socket_options(int fd, int delay)`

145. [**`is_tcp_socket`**](helpers/common_protocol.md#is_tcp_socket) — `PUBLIC BOOL is_tcp_socket(int fd)`

146. [**`is_udp_socket`**](helpers/common_protocol.md#is_udp_socket) — `PUBLIC BOOL is_udp_socket(int fd)`

147. [**`print_socket_address`**](helpers/common_protocol.md#print_socket_address) — `PUBLIC int print_socket_address(char *buf, size_t buflen, const struct sockaddr *sa)`

148. [**`get_peername`**](helpers/common_protocol.md#get_peername) — `PUBLIC int get_peername(char *bf, size_t bfsize, int fd)`

149. [**`get_sockname`**](helpers/common_protocol.md#get_sockname) — `PUBLIC int get_sockname(char *bf, size_t bfsize, int fd)`

150. [**`check_open_fds`**](helpers/misc.md#check_open_fds) — `PUBLIC int check_open_fds(void)`

151. [**`print_open_fds`**](helpers/misc.md#print_open_fds) — `PUBLIC int print_open_fds(const char *fmt, ...)`

152. [**`is_yuneta_user`**](helpers/misc.md#is_yuneta_user) — `PUBLIC int is_yuneta_user(const char *username)`

153. [**`yuneta_getpwuid`**](helpers/misc.md#yuneta_getpwuid) — `PUBLIC struct passwd *yuneta_getpwuid(uid_t uid)`

154. [**`yuneta_getpwnam`**](helpers/misc.md#yuneta_getpwnam) — `PUBLIC struct passwd *yuneta_getpwnam(const char *name)`

155. [**`yuneta_getgrnam`**](helpers/misc.md#yuneta_getgrnam) — `PUBLIC struct group *yuneta_getgrnam(const char *name)`

156. [**`yuneta_getgrouplist`**](helpers/misc.md#yuneta_getgrouplist) — `PUBLIC int yuneta_getgrouplist(const char *user, gid_t group, gid_t *groups, int *ngroups)`

157. [**`path_basename`**](helpers/string_helper.md#path_basename) — `PUBLIC const char *path_basename(const char *path)`

158. [**`get_yunetas_base`**](helpers/misc.md#get_yunetas_base) — `PUBLIC const char *get_yunetas_base(void)`

159. [**`source2base64_for_yunetas`**](helpers/misc.md#source2base64_for_yunetas) — `PUBLIC gbuffer_t *source2base64_for_yunetas( const char *source, char *comment, int commentlen )`

160. [**`replace_cli_vars`**](helpers/string_helper.md#replace_cli_vars) — `PUBLIC gbuffer_t *replace_cli_vars( const char *command, char *comment, int commentlen )`

161. [**`get_number_from_nn_table`**](helpers/misc.md#get_number_from_nn_table) — `PUBLIC int get_number_from_nn_table(const number_name_table_t *table, const char *name)`

162. [**`get_name_from_nn_table`**](helpers/misc.md#get_name_from_nn_table) — `PUBLIC const char *get_name_from_nn_table(const number_name_table_t *table, int number)`

163. [**`get_hours_range`**](helpers/time_date.md#get_hours_range) — `PUBLIC time_range_t get_hours_range(time_t t, int range, const char *TZ)`

164. [**`get_days_range`**](helpers/time_date.md#get_days_range) — `PUBLIC time_range_t get_days_range(time_t t, int range, const char *TZ)`

165. [**`get_weeks_range`**](helpers/time_date.md#get_weeks_range) — `PUBLIC time_range_t get_weeks_range(time_t t, int range, const char *TZ)`

166. [**`get_months_range`**](helpers/time_date.md#get_months_range) — `PUBLIC time_range_t get_months_range(time_t t, int range, const char *TZ)`

167. [**`get_years_range`**](helpers/time_date.md#get_years_range) — `PUBLIC time_range_t get_years_range(time_t t, int range, const char *TZ)`

### `kwid.h` — 52 functions

**Source:** `kernel/c/gobj-c/src/kwid.h`

1. [**`kw_add_binary_type`**](helpers/kwid.md#kw_add_binary_type) — `PUBLIC int kw_add_binary_type( const char *binary_field_name, const char *serialized_field_name, serialize_fn_t serialize_fn, deserialize_fn_t deserialize_fn, incref_fn_t incref_fn, decref_fn_t decref_fn )`

2. [**`kw_serialize`**](helpers/kwid.md#kw_serialize) — `PUBLIC json_t *kw_serialize( hgobj gobj, json_t *kw )`

3. [**`kw_serialize_to_string`**](helpers/kwid.md#kw_serialize_to_string) — `PUBLIC char *kw_serialize_to_string( hgobj gobj, json_t *kw )`

4. [**`kw_deserialize`**](helpers/kwid.md#kw_deserialize) — `PUBLIC json_t *kw_deserialize( hgobj gobj, json_t *kw )`

5. [**`kw_incref`**](helpers/kwid.md#kw_incref) — `PUBLIC json_t *kw_incref(json_t *kw)`

6. [**`kw_decref`**](helpers/kwid.md#kw_decref) — `PUBLIC json_t *kw_decref(json_t* kw)`

7. [**`kw_has_key`**](helpers/kwid.md#kw_has_key) — `PUBLIC BOOL kw_has_key(json_t *kw, const char *key)`

8. [**`kw_set_path_delimiter`**](helpers/kwid.md#kw_set_path_delimiter) — `PUBLIC char kw_set_path_delimiter(char delimiter)`

9. [**`kw_find_path`**](helpers/kwid.md#kw_find_path) — `PUBLIC json_t *kw_find_path(hgobj gobj, json_t *kw, const char *path, BOOL verbose)`

10. [**`kw_set_dict_value`**](helpers/kwid.md#kw_set_dict_value) — `PUBLIC int kw_set_dict_value( hgobj gobj, json_t *kw, const char *path, json_t *value )`

11. [**`kw_set_subdict_value`**](helpers/kwid.md#kw_set_subdict_value) — `PUBLIC int kw_set_subdict_value( hgobj gobj, json_t* kw, const char *path, const char *key, json_t *value )`

12. [**`kw_delete`**](helpers/kwid.md#kw_delete) — `PUBLIC int kw_delete( hgobj gobj, json_t *kw, const char *path )`

13. [**`kw_delete_subkey`**](helpers/kwid.md#kw_delete_subkey) — `PUBLIC int kw_delete_subkey(hgobj gobj, json_t *kw, const char *path, const char *key)`

14. [**`kw_find_str_in_list`**](helpers/kwid.md#kw_find_str_in_list) — `PUBLIC int kw_find_str_in_list( hgobj gobj, json_t *kw_list, const char *str )`

15. [**`kw_find_json_in_list`**](helpers/kwid.md#kw_find_json_in_list) — `PUBLIC int kw_find_json_in_list( hgobj gobj, json_t *kw_list, json_t *item, kw_flag_t flag )`

16. [**`kw_select`**](helpers/kwid.md#kw_select) — `PUBLIC json_t *kw_select( hgobj gobj, json_t *kw, json_t *jn_filter, BOOL (*match_fn) ( json_t *kw, json_t *jn_filter ) )`

17. [**`kw_collect`**](helpers/kwid.md#kw_collect) — `PUBLIC json_t *kw_collect( hgobj gobj, json_t *kw, json_t *jn_filter, BOOL (*match_fn) ( json_t *kw, json_t *jn_filter ) )`

18. [**`kwid_compare_records`**](helpers/kwid.md#kwid_compare_records) — `PUBLIC BOOL kwid_compare_records( hgobj gobj, json_t *record, json_t *expected, const char **ignore_keys, BOOL without_metadata, BOOL without_private, BOOL verbose )`

19. [**`kwid_compare_lists`**](helpers/kwid.md#kwid_compare_lists) — `PUBLIC BOOL kwid_compare_lists( hgobj gobj, json_t *list, json_t *expected, const char **ignore_keys, BOOL without_metadata, BOOL without_private, BOOL verbose )`

20. [**`kw_get_dict`**](helpers/kwid.md#kw_get_dict) — `PUBLIC json_t *kw_get_dict( hgobj gobj, json_t *kw, const char *path, json_t *default_value, kw_flag_t flag )`

21. [**`kw_get_list`**](helpers/kwid.md#kw_get_list) — `PUBLIC json_t *kw_get_list( hgobj gobj, json_t *kw, const char *path, json_t *default_value, kw_flag_t flag )`

22. [**`kw_get_list_value`**](helpers/kwid.md#kw_get_list_value) — `PUBLIC json_t *kw_get_list_value( hgobj gobj, json_t* kw, int idx, kw_flag_t flag )`

23. [**`kw_get_int`**](helpers/kwid.md#kw_get_int) — `PUBLIC json_int_t kw_get_int( hgobj gobj, json_t *kw, const char *path, json_int_t default_value, kw_flag_t flag )`

24. [**`kw_get_real`**](helpers/kwid.md#kw_get_real) — `PUBLIC double kw_get_real( hgobj gobj, json_t *kw, const char *path, double default_value, kw_flag_t flag )`

25. [**`kw_get_bool`**](helpers/kwid.md#kw_get_bool) — `PUBLIC BOOL kw_get_bool( hgobj gobj, json_t *kw, const char *path, BOOL default_value, kw_flag_t flag )`

26. [**`kw_get_str`**](helpers/kwid.md#kw_get_str) — `PUBLIC const char *kw_get_str( hgobj gobj, json_t *kw, const char *path, const char *default_value, kw_flag_t flag )`

27. [**`kw_get_dict_value`**](helpers/kwid.md#kw_get_dict_value) — `PUBLIC json_t *kw_get_dict_value( hgobj gobj, json_t *kw, const char *path, json_t *default_value, kw_flag_t flag )`

28. [**`kw_get_subdict_value`**](helpers/kwid.md#kw_get_subdict_value) — `PUBLIC json_t *kw_get_subdict_value( hgobj gobj, json_t *kw, const char *path, const char *key, json_t *jn_default_value, kw_flag_t flag )`

29. [**`kw_update_except`**](helpers/kwid.md#kw_update_except) — `PUBLIC void kw_update_except( hgobj gobj, json_t *kw, json_t *other, const char **except_keys )`

30. [**`kw_duplicate`**](helpers/kwid.md#kw_duplicate) — `PUBLIC json_t *kw_duplicate( hgobj gobj, json_t *kw )`

31. [**`kw_clone_by_path`**](helpers/kwid.md#kw_clone_by_path) — `PUBLIC json_t *kw_clone_by_path( hgobj gobj, json_t *kw, const char **paths )`

32. [**`kw_clone_by_keys`**](helpers/kwid.md#kw_clone_by_keys) — `PUBLIC json_t *kw_clone_by_keys( hgobj gobj, json_t *kw, json_t *keys, BOOL verbose )`

33. [**`kw_clone_by_not_keys`**](helpers/kwid.md#kw_clone_by_not_keys) — `PUBLIC json_t *kw_clone_by_not_keys( hgobj gobj, json_t *kw, json_t *keys, BOOL verbose )`

34. [**`kw_pop`**](helpers/kwid.md#kw_pop) — `PUBLIC int kw_pop( json_t *kw1, json_t *kw2 )`

35. [**`kw_match_simple`**](helpers/kwid.md#kw_match_simple) — `PUBLIC BOOL kw_match_simple( json_t *kw, json_t *jn_filter )`

36. [**`kw_delete_private_keys`**](helpers/kwid.md#kw_delete_private_keys) — `PUBLIC int kw_delete_private_keys( json_t *kw )`

37. [**`kw_delete_metadata_keys`**](helpers/kwid.md#kw_delete_metadata_keys) — `PUBLIC int kw_delete_metadata_keys( json_t *kw )`

38. [**`kw_walk`**](helpers/kwid.md#kw_walk) — `PUBLIC int kw_walk( hgobj gobj, json_t *kw, int (*callback)(hgobj gobj, json_t *kw, const char *key, json_t *value) )`

39. [**`kw_collapse`**](helpers/kwid.md#kw_collapse) — `PUBLIC json_t *kw_collapse( hgobj gobj, json_t *kw, int collapse_lists_limit, int collapse_dicts_limit )`

40. [**`kwid_get`**](helpers/kwid.md#kwid_get) — `PUBLIC json_t *kwid_get( hgobj gobj, json_t *kw, kw_flag_t flag, const char *path, ... ) JANSSON_ATTRS((format(printf, 4, 5)))`

41. [**`kwid_new_list`**](helpers/kwid.md#kwid_new_list) — `PUBLIC json_t *kwid_new_list( hgobj gobj, json_t *kw, kw_flag_t flag, const char *path, ... ) JANSSON_ATTRS((format(printf, 4, 5)))`

42. [**`kwid_new_dict`**](helpers/kwid.md#kwid_new_dict) — `PUBLIC json_t *kwid_new_dict( hgobj gobj, json_t *kw, kw_flag_t flag, const char *path, ... ) JANSSON_ATTRS((format(printf, 4, 5)))`

43. [**`kw_filter_private`**](helpers/kwid.md#kw_filter_private) — `PUBLIC json_t *kw_filter_private( hgobj gobj, json_t *kw )`

44. [**`kw_filter_metadata`**](helpers/kwid.md#kw_filter_metadata) — `PUBLIC json_t *kw_filter_metadata( hgobj gobj, json_t *kw )`

45. [**`kw_size`**](helpers/kwid.md#kw_size) — `PUBLIC size_t kw_size(json_t *kw)`

46. [**`kwjr_get`**](helpers/kwid.md#kwjr_get) — `PUBLIC json_t *kwjr_get( hgobj gobj, json_t *kw, const char *id, json_t *new_record, const json_desc_t *json_desc, size_t *idx_, kw_flag_t flag )`

47. [**`kwid_get_ids`**](helpers/kwid.md#kwid_get_ids) — `PUBLIC json_t *kwid_get_ids( json_t *ids )`

48. [**`kw_has_word`**](helpers/kwid.md#kw_has_word) — `PUBLIC BOOL kw_has_word( hgobj gobj, json_t *kw, const char *word, kw_flag_t kw_flag )`

49. [**`kwid_match_id`**](helpers/kwid.md#kwid_match_id) — `PUBLIC BOOL kwid_match_id( hgobj gobj, json_t *ids, const char *id )`

50. [**`kwid_match_nid`**](helpers/kwid.md#kwid_match_nid) — `PUBLIC BOOL kwid_match_nid( hgobj gobj, json_t *ids, const char *id, int max_id_size )`

51. [**`json_flatten_dict`**](helpers/kwid.md#json_flatten_dict) — `PUBLIC json_t *json_flatten_dict(json_t *jn_nested)`

52. [**`json_unflatten_dict`**](helpers/kwid.md#json_unflatten_dict) — `PUBLIC json_t *json_unflatten_dict(json_t *jn_flat)`

### `log_udp_handler.h` — 6 functions

**Source:** `kernel/c/gobj-c/src/log_udp_handler.h`

1. [**`udpc_start_up`**](logging/log_udp_handler.md#udpc_start_up) — `PUBLIC int udpc_start_up(const char *process_name, const char *hostname, int pid)`

2. [**`udpc_end`**](logging/log_udp_handler.md#udpc_end) — `PUBLIC void udpc_end(void)`

3. [**`udpc_open`**](logging/log_udp_handler.md#udpc_open) — `PUBLIC udpc_t udpc_open( const char *url, const char *bindip, const char *if_name, size_t bf_size, size_t udp_frame_size, output_format_t output_format, BOOL exit_on_fail )`

4. [**`udpc_close`**](logging/log_udp_handler.md#udpc_close) — `PUBLIC void udpc_close(udpc_t udpc)`

5. [**`udpc_write`**](logging/log_udp_handler.md#udpc_write) — `PUBLIC int udpc_write(udpc_t udpc, int priority, const char *bf, size_t len)`

6. [**`udpc_fwrite`**](logging/log_udp_handler.md#udpc_fwrite) — `PUBLIC int udpc_fwrite(udpc_t udpc, int priority, const char *format, ...)`

### `rotatory.h` — 10 functions

**Source:** `kernel/c/gobj-c/src/rotatory.h`

1. [**`rotatory_start_up`**](logging/rotatory.md#rotatory_start_up) — `PUBLIC int rotatory_start_up(void)`

2. [**`rotatory_end`**](logging/rotatory.md#rotatory_end) — `PUBLIC void rotatory_end(void)`

3. [**`rotatory_open`**](logging/rotatory.md#rotatory_open) — `PUBLIC hrotatory_h rotatory_open( const char* path, size_t bf_size, size_t max_megas_rotatoryfile_size, size_t min_free_disk_percentage, int xpermission, int rpermission, BOOL exit_on_fail )`

4. [**`rotatory_close`**](logging/rotatory.md#rotatory_close) — `PUBLIC void rotatory_close(hrotatory_h hr)`

5. [**`rotatory_subscribe2newfile`**](logging/rotatory.md#rotatory_subscribe2newfile) — `PUBLIC int rotatory_subscribe2newfile( hrotatory_h hr, int (*cb_newfile)(void *user_data, const char *old_filename, const char *new_filename), void *user_data )`

6. [**`rotatory_write`**](logging/rotatory.md#rotatory_write) — `PUBLIC int rotatory_write(hrotatory_h hr, int priority, const char *bf, size_t len)`

7. [**`rotatory_fwrite`**](logging/rotatory.md#rotatory_fwrite) — `PUBLIC int rotatory_fwrite(hrotatory_h hr_, int priority, const char *format, ...)`

8. [**`rotatory_truncate`**](logging/rotatory.md#rotatory_truncate) — `PUBLIC void rotatory_truncate(hrotatory_h hr)`

9. [**`rotatory_flush`**](logging/rotatory.md#rotatory_flush) — `PUBLIC void rotatory_flush(hrotatory_h hr)`

10. [**`rotatory_path`**](logging/rotatory.md#rotatory_path) — `PUBLIC const char *rotatory_path(hrotatory_h hr)`

### `stats_parser.h` — 3 functions

**Source:** `kernel/c/gobj-c/src/stats_parser.h`

1. [**`stats_parser`**](parsers/stats_parser.md#stats_parser) — `PUBLIC json_t *stats_parser( hgobj gobj, const char *stats, json_t *kw, hgobj src )`

2. [**`build_stats`**](parsers/stats_parser.md#build_stats) — `PUBLIC json_t *build_stats( hgobj gobj, const char *stats, json_t *kw, hgobj src )`

3. [**`build_stats_response`**](parsers/stats_parser.md#build_stats_response) — `PUBLIC json_t *build_stats_response( hgobj gobj, json_int_t result, json_t *jn_comment, json_t *jn_schema, json_t *jn_data )`

### `testing.h` — 9 functions

**Source:** `kernel/c/gobj-c/src/testing.h`

1. [**`capture_log_write`**](testing/testing.md#capture_log_write) — `PUBLIC int capture_log_write(void* v, int priority, const char* bf, size_t len)`

2. [**`set_expected_results`**](testing/testing.md#set_expected_results) — `PUBLIC void set_expected_results( const char *name, json_t *errors_list, json_t *expected, const char **ignore_keys, BOOL verbose )`

3. [**`test_json_file`**](testing/testing.md#test_json_file) — `PUBLIC int test_json_file( const char *file )`

4. [**`test_json`**](testing/testing.md#test_json) — `PUBLIC int test_json( json_t *jn_found )`

5. [**`test_directory_permission`**](testing/testing.md#test_directory_permission) — `PUBLIC int test_directory_permission(const char *path, mode_t permission)`

6. [**`test_file_permission_and_size`**](testing/testing.md#test_file_permission_and_size) — `PUBLIC int test_file_permission_and_size(const char *path, mode_t permission, off_t size)`

7. [**`test_list`**](testing/testing.md#test_list) — `PUBLIC int test_list(json_t *found, json_t *expected, const char *msg, ...) JANSSON_ATTRS((format(printf, 3, 4)))`

8. [**`set_measure_times`**](yev_loop/yev_loop.md#set_measure_times) — `PUBLIC void set_measure_times(int types)`

9. [**`get_measure_times`**](yev_loop/yev_loop.md#get_measure_times) — `PUBLIC int get_measure_times(void)`

**Total: 578 functions**

## libjwt (JWT Authentication)

### `jwt.h` — 80 functions

**Source:** `kernel/c/libjwt/src/jwt.h`

1. [**`jwt_get_alg`**](libjwt.md#jwt_get_alg) — `jwt_alg_t jwt_get_alg(const jwt_t *jwt)`

2. [**`jwt_builder_new`**](libjwt.md#jwt_builder_new) — `jwt_builder_t *jwt_builder_new(void)`

3. [**`jwt_builder_free`**](libjwt.md#jwt_builder_free) — `void jwt_builder_free(jwt_builder_t *builder)`

4. [**`jwt_builder_error`**](libjwt.md#jwt_builder_error) — `int jwt_builder_error(const jwt_builder_t *builder)`

5. [**`jwt_builder_error_msg`**](libjwt.md#jwt_builder_error_msg) — `const char *jwt_builder_error_msg(const jwt_builder_t *builder)`

6. [**`jwt_builder_error_clear`**](libjwt.md#jwt_builder_error_clear) — `void jwt_builder_error_clear(jwt_builder_t *builder)`

7. [**`jwt_builder_setkey`**](libjwt.md#jwt_builder_setkey) — `int jwt_builder_setkey(jwt_builder_t *builder, const jwt_alg_t alg, const jwk_item_t *key)`

8. [**`jwt_builder_enable_iat`**](libjwt.md#jwt_builder_enable_iat) — `int jwt_builder_enable_iat(jwt_builder_t *builder, int enable)`

9. [**`jwt_builder_setcb`**](libjwt.md#jwt_builder_setcb) — `int jwt_builder_setcb(jwt_builder_t *builder, jwt_callback_t cb, void *ctx)`

10. [**`jwt_builder_getctx`**](libjwt.md#jwt_builder_getctx) — `void *jwt_builder_getctx(jwt_builder_t *builder)`

11. [**`jwt_builder_generate`**](libjwt.md#jwt_builder_generate) — `char *jwt_builder_generate(jwt_builder_t *builder)`

12. [**`jwt_checker_new`**](libjwt.md#jwt_checker_new) — `jwt_checker_t *jwt_checker_new(void)`

13. [**`jwt_checker_free`**](libjwt.md#jwt_checker_free) — `void jwt_checker_free(jwt_checker_t *checker)`

14. [**`jwt_checker_error`**](libjwt.md#jwt_checker_error) — `int jwt_checker_error(const jwt_checker_t *checker)`

15. [**`jwt_checker_error_msg`**](libjwt.md#jwt_checker_error_msg) — `const char *jwt_checker_error_msg(const jwt_checker_t *checker)`

16. [**`jwt_checker_error_clear`**](libjwt.md#jwt_checker_error_clear) — `void jwt_checker_error_clear(jwt_checker_t *checker)`

17. [**`jwt_checker_setkey`**](libjwt.md#jwt_checker_setkey) — `int jwt_checker_setkey(jwt_checker_t *checker, const jwt_alg_t alg, const jwk_item_t *key)`

18. [**`jwt_checker_setcb`**](libjwt.md#jwt_checker_setcb) — `int jwt_checker_setcb(jwt_checker_t *checker, jwt_callback_t cb, void *ctx)`

19. [**`jwt_checker_getctx`**](libjwt.md#jwt_checker_getctx) — `void *jwt_checker_getctx(jwt_checker_t *checker)`

20. [**`jwt_checker_verify`**](libjwt.md#jwt_checker_verify) — `int jwt_checker_verify(jwt_checker_t *checker, const char *token)`

21. [**`jwt_builder_header_set`**](libjwt.md#jwt_builder_header_set) — `jwt_value_error_t jwt_builder_header_set(jwt_builder_t *builder, jwt_value_t *value)`

22. [**`jwt_builder_header_get`**](libjwt.md#jwt_builder_header_get) — `jwt_value_error_t jwt_builder_header_get(jwt_builder_t *builder, jwt_value_t *value)`

23. [**`jwt_builder_header_del`**](libjwt.md#jwt_builder_header_del) — `jwt_value_error_t jwt_builder_header_del(jwt_builder_t *builder, const char *header)`

24. [**`jwt_builder_claim_set`**](libjwt.md#jwt_builder_claim_set) — `jwt_value_error_t jwt_builder_claim_set(jwt_builder_t *builder, jwt_value_t *value)`

25. [**`jwt_builder_claim_get`**](libjwt.md#jwt_builder_claim_get) — `jwt_value_error_t jwt_builder_claim_get(jwt_builder_t *builder, jwt_value_t *value)`

26. [**`jwt_builder_claim_del`**](libjwt.md#jwt_builder_claim_del) — `jwt_value_error_t jwt_builder_claim_del(jwt_builder_t *builder, const char *claim)`

27. [**`jwt_builder_time_offset`**](libjwt.md#jwt_builder_time_offset) — `int jwt_builder_time_offset(jwt_builder_t *builder, jwt_claims_t claim, time_t secs)`

28. [**`jwt_checker_claim_get`**](libjwt.md#jwt_checker_claim_get) — `const char *jwt_checker_claim_get(jwt_checker_t *checker, jwt_claims_t type)`

29. [**`jwt_checker_claim_set`**](libjwt.md#jwt_checker_claim_set) — `int jwt_checker_claim_set(jwt_checker_t *checker, jwt_claims_t type, const char *value)`

30. [**`jwt_checker_claim_del`**](libjwt.md#jwt_checker_claim_del) — `int jwt_checker_claim_del(jwt_checker_t *checker, jwt_claims_t type)`

31. [**`jwt_checker_time_leeway`**](libjwt.md#jwt_checker_time_leeway) — `int jwt_checker_time_leeway(jwt_checker_t *checker, jwt_claims_t claim, time_t secs)`

32. [**`jwt_header_set`**](libjwt.md#jwt_header_set) — `jwt_value_error_t jwt_header_set(jwt_t *jwt, jwt_value_t *value)`

33. [**`jwt_header_get`**](libjwt.md#jwt_header_get) — `jwt_value_error_t jwt_header_get(jwt_t *jwt, jwt_value_t *value)`

34. [**`jwt_header_del`**](libjwt.md#jwt_header_del) — `jwt_value_error_t jwt_header_del(jwt_t *jwt, const char *header)`

35. [**`jwt_claim_set`**](libjwt.md#jwt_claim_set) — `jwt_value_error_t jwt_claim_set(jwt_t *jwt, jwt_value_t *value)`

36. [**`jwt_claim_get`**](libjwt.md#jwt_claim_get) — `jwt_value_error_t jwt_claim_get(jwt_t *jwt, jwt_value_t *value)`

37. [**`jwt_claim_del`**](libjwt.md#jwt_claim_del) — `jwt_value_error_t jwt_claim_del(jwt_t *jwt, const char *claim)`

38. [**`jwt_alg_str`**](libjwt.md#jwt_alg_str) — `const char *jwt_alg_str(jwt_alg_t alg)`

39. [**`jwt_str_alg`**](libjwt.md#jwt_str_alg) — `jwt_alg_t jwt_str_alg(const char *alg)`

40. [**`jwks_load`**](libjwt.md#jwks_load) — `jwk_set_t *jwks_load(jwk_set_t *jwk_set, const char *jwk_json_str)`

41. [**`jwks_load_strn`**](libjwt.md#jwks_load_strn) — `jwk_set_t *jwks_load_strn(jwk_set_t *jwk_set, const char *jwk_json_str, const size_t len)`

42. [**`jwks_load_fromfile`**](libjwt.md#jwks_load_fromfile) — `jwk_set_t *jwks_load_fromfile(jwk_set_t *jwk_set, const char *file_name)`

43. [**`jwks_load_fromfp`**](libjwt.md#jwks_load_fromfp) — `jwk_set_t *jwks_load_fromfp(jwk_set_t *jwk_set, FILE *input)`

44. [**`jwks_load_fromurl`**](libjwt.md#jwks_load_fromurl) — `jwk_set_t *jwks_load_fromurl(jwk_set_t *jwk_set, const char *url, int verify)`

45. [**`jwks_create`**](libjwt.md#jwks_create) — `jwk_set_t *jwks_create(const char *jwk_json_str)`

46. [**`jwks_create_strn`**](libjwt.md#jwks_create_strn) — `jwk_set_t *jwks_create_strn(const char *jwk_json_str, const size_t len)`

47. [**`jwks_create_fromfile`**](libjwt.md#jwks_create_fromfile) — `jwk_set_t *jwks_create_fromfile(const char *file_name)`

48. [**`jwks_create_fromfp`**](libjwt.md#jwks_create_fromfp) — `jwk_set_t *jwks_create_fromfp(FILE *input)`

49. [**`jwks_create_fromurl`**](libjwt.md#jwks_create_fromurl) — `jwk_set_t *jwks_create_fromurl(const char *url, int verify)`

50. [**`jwks_error`**](libjwt.md#jwks_error) — `int jwks_error(const jwk_set_t *jwk_set)`

51. [**`jwks_error_any`**](libjwt.md#jwks_error_any) — `int jwks_error_any(const jwk_set_t *jwk_set)`

52. [**`jwks_error_msg`**](libjwt.md#jwks_error_msg) — `const char *jwks_error_msg(const jwk_set_t *jwk_set)`

53. [**`jwks_error_clear`**](libjwt.md#jwks_error_clear) — `void jwks_error_clear(jwk_set_t *jwk_set)`

54. [**`jwks_free`**](libjwt.md#jwks_free) — `void jwks_free(jwk_set_t *jwk_set)`

55. [**`jwks_item_get`**](libjwt.md#jwks_item_get) — `const jwk_item_t *jwks_item_get(const jwk_set_t *jwk_set, size_t index)`

56. [**`jwks_find_bykid`**](libjwt.md#jwks_find_bykid) — `jwk_item_t *jwks_find_bykid(jwk_set_t *jwk_set, const char *kid)`

57. [**`jwks_item_is_private`**](libjwt.md#jwks_item_is_private) — `int jwks_item_is_private(const jwk_item_t *item)`

58. [**`jwks_item_error`**](libjwt.md#jwks_item_error) — `int jwks_item_error(const jwk_item_t *item)`

59. [**`jwks_item_error_msg`**](libjwt.md#jwks_item_error_msg) — `const char *jwks_item_error_msg(const jwk_item_t *item)`

60. [**`jwks_item_curve`**](libjwt.md#jwks_item_curve) — `const char *jwks_item_curve(const jwk_item_t *item)`

61. [**`jwks_item_kid`**](libjwt.md#jwks_item_kid) — `const char *jwks_item_kid(const jwk_item_t *item)`

62. [**`jwks_item_alg`**](libjwt.md#jwks_item_alg) — `jwt_alg_t jwks_item_alg(const jwk_item_t *item)`

63. [**`jwks_item_kty`**](libjwt.md#jwks_item_kty) — `jwk_key_type_t jwks_item_kty(const jwk_item_t *item)`

64. [**`jwks_item_use`**](libjwt.md#jwks_item_use) — `jwk_pub_key_use_t jwks_item_use(const jwk_item_t *item)`

65. [**`jwks_item_key_ops`**](libjwt.md#jwks_item_key_ops) — `jwk_key_op_t jwks_item_key_ops(const jwk_item_t *item)`

66. [**`jwks_item_pem`**](libjwt.md#jwks_item_pem) — `const char *jwks_item_pem(const jwk_item_t *item)`

67. [**`jwks_item_key_oct`**](libjwt.md#jwks_item_key_oct) — `int jwks_item_key_oct(const jwk_item_t *item, const unsigned char **buf, size_t *len)`

68. [**`jwks_item_key_bits`**](libjwt.md#jwks_item_key_bits) — `int jwks_item_key_bits(const jwk_item_t *item)`

69. [**`jwks_item_free`**](libjwt.md#jwks_item_free) — `int jwks_item_free(jwk_set_t *jwk_set, size_t index)`

70. [**`jwks_item_free_all`**](libjwt.md#jwks_item_free_all) — `int jwks_item_free_all(jwk_set_t *jwk_set)`

71. [**`jwks_item_free_bad`**](libjwt.md#jwks_item_free_bad) — `int jwks_item_free_bad(jwk_set_t *jwk_set)`

72. [**`jwks_item_count`**](libjwt.md#jwks_item_count) — `size_t jwks_item_count(const jwk_set_t *jwk_set)`

73. [**`jwt_set_alloc`**](libjwt.md#jwt_set_alloc) — `int jwt_set_alloc(jwt_malloc_t pmalloc, jwt_free_t pfree)`

74. [**`jwt_get_alloc`**](libjwt.md#jwt_get_alloc) — `void jwt_get_alloc(jwt_malloc_t *pmalloc, jwt_free_t *pfree)`

75. [**`jwt_get_crypto_ops`**](libjwt.md#jwt_get_crypto_ops) — `const char *jwt_get_crypto_ops(void)`

76. [**`jwt_get_crypto_ops_t`**](libjwt.md#jwt_get_crypto_ops_t) — `jwt_crypto_provider_t jwt_get_crypto_ops_t(void)`

77. [**`jwt_set_crypto_ops`**](libjwt.md#jwt_set_crypto_ops) — `int jwt_set_crypto_ops(const char *opname)`

78. [**`jwt_set_crypto_ops_t`**](libjwt.md#jwt_set_crypto_ops_t) — `int jwt_set_crypto_ops_t(jwt_crypto_provider_t opname)`

79. [**`jwt_crypto_ops_supports_jwk`**](libjwt.md#jwt_crypto_ops_supports_jwk) — `int jwt_crypto_ops_supports_jwk(void)`

80. [**`jwt_init`**](libjwt.md#jwt_init) — `void jwt_init(void)`

**Total: 80 functions**

## ytls (TLS Abstraction)

### `mbedtls.h` — 1 functions

**Source:** `kernel/c/ytls/src/tls/mbedtls.h`

1. [**`mbed_api_tls`**](ytls/ytls.md#mbed_api_tls) — `PUBLIC api_tls_t *mbed_api_tls(void)`

### `openssl.h` — 1 functions

**Source:** `kernel/c/ytls/src/tls/openssl.h`

1. [**`openssl_api_tls`**](ytls/ytls.md#openssl_api_tls) — `PUBLIC api_tls_t *openssl_api_tls(void)`

### `ytls.h` — 12 functions

**Source:** `kernel/c/ytls/src/ytls.h`

1. [**`ytls_init`**](ytls/ytls.md#ytls_init) — `PUBLIC hytls ytls_init( hgobj gobj, json_t *jn_config, BOOL server )`

2. [**`ytls_cleanup`**](ytls/ytls.md#ytls_cleanup) — `PUBLIC void ytls_cleanup(hytls ytls)`

3. [**`ytls_version`**](ytls/ytls.md#ytls_version) — `PUBLIC const char * ytls_version(hytls ytls)`

4. [**`ytls_new_secure_filter`**](ytls/ytls.md#ytls_new_secure_filter) — `PUBLIC hsskt ytls_new_secure_filter( hytls ytls, int (*on_handshake_done_cb)(void *user_data, int error), int (*on_clear_data_cb)( void *user_data, gbuffer_t *gbuf ), int (*on_encrypted_data_cb)( void *user_data, gbuffer_t *gbuf ), void *user_data )`

5. [**`ytls_shutdown`**](ytls/ytls.md#ytls_shutdown) — `PUBLIC void ytls_shutdown(hytls ytls, hsskt sskt)`

6. [**`ytls_free_secure_filter`**](ytls/ytls.md#ytls_free_secure_filter) — `PUBLIC void ytls_free_secure_filter(hytls ytls, hsskt sskt)`

7. [**`ytls_do_handshake`**](ytls/ytls.md#ytls_do_handshake) — `PUBLIC int ytls_do_handshake(hytls ytls, hsskt sskt)`

8. [**`ytls_encrypt_data`**](ytls/ytls.md#ytls_encrypt_data) — `PUBLIC int ytls_encrypt_data( hytls ytls, hsskt sskt, gbuffer_t *gbuf )`

9. [**`ytls_decrypt_data`**](ytls/ytls.md#ytls_decrypt_data) — `PUBLIC int ytls_decrypt_data( hytls ytls, hsskt sskt, gbuffer_t *gbuf )`

10. [**`ytls_get_last_error`**](ytls/ytls.md#ytls_get_last_error) — `PUBLIC const char *ytls_get_last_error(hytls ytls, hsskt sskt)`

11. [**`ytls_set_trace`**](ytls/ytls.md#ytls_set_trace) — `PUBLIC void ytls_set_trace(hytls ytls, hsskt sskt, BOOL set)`

12. [**`ytls_flush`**](ytls/ytls.md#ytls_flush) — `PUBLIC int ytls_flush(hytls ytls, hsskt sskt)`

13. [**`ytls_reload_certificates`**](ytls/ytls.md#ytls_reload_certificates) — `PUBLIC int ytls_reload_certificates( hytls ytls, json_t *jn_config )`

14. [**`ytls_get_cert_info`**](ytls/ytls.md#ytls_get_cert_info) — `PUBLIC json_t *ytls_get_cert_info( hytls ytls )`

**Total: 16 functions**

## yev_loop (Event Loop)

### `yev_loop.h` — 27 functions

**Source:** `kernel/c/yev_loop/src/yev_loop.h`

1. [**`yev_loop_create`**](yev_loop/yev_loop.md#yev_loop_create) — `PUBLIC int yev_loop_create( hgobj yuno, unsigned entries, int keep_alive, yev_callback_t callback, yev_loop_h *yev_loop )`

2. [**`yev_loop_destroy`**](yev_loop/yev_loop.md#yev_loop_destroy) — `PUBLIC void yev_loop_destroy(yev_loop_h yev_loop)`

3. [**`yev_loop_run`**](yev_loop/yev_loop.md#yev_loop_run) — `PUBLIC int yev_loop_run(yev_loop_h yev_loop, int timeout_in_seconds)`

4. [**`yev_loop_run_once`**](yev_loop/yev_loop.md#yev_loop_run_once) — `PUBLIC int yev_loop_run_once(yev_loop_h yev_loop)`

5. [**`yev_loop_stop`**](yev_loop/yev_loop.md#yev_loop_stop) — `PUBLIC int yev_loop_stop(yev_loop_h yev_loop)`

6. [**`yev_loop_reset_running`**](yev_loop/yev_loop.md#yev_loop_reset_running) — `PUBLIC void yev_loop_reset_running(yev_loop_h yev_loop)`

7. [**`yev_protocol_set_protocol_fill_hints_fn`**](yev_loop/yev_loop.md#yev_protocol_set_protocol_fill_hints_fn) — `PUBLIC int yev_protocol_set_protocol_fill_hints_fn( yev_protocol_fill_hints_fn_t yev_protocol_fill_hints_fn )`

8. [**`yev_get_state_name`**](yev_loop/yev_loop.md#yev_get_state_name) — `PUBLIC const char *yev_get_state_name(yev_event_h yev_event)`

9. [**`yev_set_gbuffer`**](yev_loop/yev_loop.md#yev_set_gbuffer) — `PUBLIC int yev_set_gbuffer( yev_event_h yev_event, gbuffer_t *gbuf )`

10. [**`yev_get_yuno`**](yev_loop/yev_loop.md#yev_get_yuno) — `PUBLIC hgobj yev_get_yuno(yev_loop_h yev_loop)`

11. [**`yev_start_event`**](yev_loop/yev_loop.md#yev_start_event) — `PUBLIC int yev_start_event( yev_event_h yev_event )`

12. [**`yev_start_timer_event`**](yev_loop/yev_loop.md#yev_start_timer_event) — `PUBLIC int yev_start_timer_event( yev_event_h yev_event, time_t timeout_ms, BOOL periodic )`

13. [**`yev_stop_event`**](yev_loop/yev_loop.md#yev_stop_event) — `PUBLIC int yev_stop_event(yev_event_h yev_event)`

14. [**`yev_destroy_event`**](yev_loop/yev_loop.md#yev_destroy_event) — `PUBLIC void yev_destroy_event(yev_event_h yev_event)`

15. [**`yev_create_timer_event`**](yev_loop/yev_loop.md#yev_create_timer_event) — `PUBLIC yev_event_h yev_create_timer_event( yev_loop_h yev_loop, yev_callback_t callback, hgobj gobj )`

16. [**`yev_create_connect_event`**](yev_loop/yev_loop.md#yev_create_connect_event) — `PUBLIC yev_event_h yev_create_connect_event( yev_loop_h yev_loop, yev_callback_t callback, const char *dst_url, const char *src_url, int ai_family, int ai_flags, hgobj gobj )`

17. [**`yev_rearm_connect_event`**](yev_loop/yev_loop.md#yev_rearm_connect_event) — `PUBLIC int yev_rearm_connect_event( yev_event_h yev_event, const char *dst_url, const char *src_url, int ai_family, int ai_flags )`

18. [**`yev_create_accept_event`**](yev_loop/yev_loop.md#yev_create_accept_event) — `PUBLIC yev_event_h yev_create_accept_event( yev_loop_h yev_loop, yev_callback_t callback, const char *listen_url, int backlog, BOOL shared, int ai_family, int ai_flags, hgobj gobj )`

19. [**`yev_dup_accept_event`**](yev_loop/yev_loop.md#yev_dup_accept_event) — `PUBLIC yev_event_h yev_dup_accept_event( yev_event_h yev_server_accept, int dup_idx, hgobj gobj )`

20. [**`yev_dup2_accept_event`**](yev_loop/yev_loop.md#yev_dup2_accept_event) — `PUBLIC yev_event_h yev_dup2_accept_event( yev_loop_h yev_loop, yev_callback_t callback, int fd_listen, hgobj gobj )`

21. [**`yev_create_poll_event`**](yev_loop/yev_loop.md#yev_create_poll_event) — `PUBLIC yev_event_h yev_create_poll_event( yev_loop_h yev_loop, yev_callback_t callback, hgobj gobj, int fd, unsigned poll_mask )`

22. [**`yev_create_read_event`**](yev_loop/yev_loop.md#yev_create_read_event) — `PUBLIC yev_event_h yev_create_read_event( yev_loop_h yev_loop, yev_callback_t callback, hgobj gobj, int fd, gbuffer_t *gbuf )`

23. [**`yev_create_write_event`**](yev_loop/yev_loop.md#yev_create_write_event) — `PUBLIC yev_event_h yev_create_write_event( yev_loop_h yev_loop, yev_callback_t callback, hgobj gobj, int fd, gbuffer_t *gbuf )`

24. [**`yev_create_recvmsg_event`**](yev_loop/yev_loop.md#yev_create_recvmsg_event) — `PUBLIC yev_event_h yev_create_recvmsg_event( yev_loop_h yev_loop, yev_callback_t callback, hgobj gobj, int fd, gbuffer_t *gbuf )`

25. [**`yev_create_sendmsg_event`**](yev_loop/yev_loop.md#yev_create_sendmsg_event) — `PUBLIC yev_event_h yev_create_sendmsg_event( yev_loop_h yev_loop, yev_callback_t callback, hgobj gobj, int fd, gbuffer_t *gbuf, struct sockaddr *dst_addr )`

26. [**`yev_event_type_name`**](yev_loop/yev_loop.md#yev_event_type_name) — `PUBLIC const char *yev_event_type_name(yev_event_h yev_event)`

27. [**`yev_flag_strings`**](yev_loop/yev_loop.md#yev_flag_strings) — `PUBLIC const char **yev_flag_strings(void)`

**Total: 27 functions**

## timeranger2 (Time-Series DB)

### `fs_watcher.h` — 3 functions

**Source:** `kernel/c/timeranger2/src/fs_watcher.h`

1. [**`fs_create_watcher_event`**](timeranger2/fs_watcher.md#fs_create_watcher_event) — `PUBLIC fs_event_t *fs_create_watcher_event( yev_loop_h yev_loop, const char *path, fs_flag_t fs_flag, fs_callback_t callback, hgobj gobj, void *user_data, void *user_data2 )`

2. [**`fs_start_watcher_event`**](timeranger2/fs_watcher.md#fs_start_watcher_event) — `PUBLIC int fs_start_watcher_event( fs_event_t *fs_event )`

3. [**`fs_stop_watcher_event`**](timeranger2/fs_watcher.md#fs_stop_watcher_event) — `PUBLIC int fs_stop_watcher_event( fs_event_t *fs_event )`

### `timeranger2.h` — 47 functions

**Source:** `kernel/c/timeranger2/src/timeranger2.h`

1. [**`tranger2_startup`**](timeranger2/timeranger2.md#tranger2_startup) — `PUBLIC json_t *tranger2_startup( hgobj gobj, json_t *jn_tranger, yev_loop_h yev_loop )`

2. [**`tranger2_stop`**](timeranger2/timeranger2.md#tranger2_stop) — `PUBLIC int tranger2_stop(json_t *tranger)`

3. [**`tranger2_shutdown`**](timeranger2/timeranger2.md#tranger2_shutdown) — `PUBLIC int tranger2_shutdown(json_t *tranger)`

4. [**`tranger2_str2system_flag`**](timeranger2/timeranger2.md#tranger2_str2system_flag) — `PUBLIC system_flag2_t tranger2_str2system_flag(const char *system_flag)`

5. [**`tranger2_create_topic`**](timeranger2/timeranger2.md#tranger2_create_topic) — `PUBLIC json_t *tranger2_create_topic( json_t *tranger, const char *topic_name, const char *pkey, const char *tkey, json_t *jn_topic_ext, system_flag2_t system_flag, json_t *jn_cols, json_t *jn_var )`

6. [**`tranger2_open_topic`**](timeranger2/timeranger2.md#tranger2_open_topic) — `PUBLIC json_t *tranger2_open_topic( json_t *tranger, const char *topic_name, BOOL verbose )`

7. [**`tranger2_topic`**](timeranger2/timeranger2.md#tranger2_topic) — `PUBLIC json_t *tranger2_topic( json_t *tranger, const char *topic_name )`

8. [**`tranger2_topic_path`**](timeranger2/timeranger2.md#tranger2_topic_path) — `PUBLIC int tranger2_topic_path( char *bf, size_t bfsize, json_t *tranger, const char *topic_name )`

9. [**`tranger2_list_topics`**](timeranger2/timeranger2.md#tranger2_list_topics) — `PUBLIC json_t *tranger2_list_topics( json_t *tranger )`

10. [**`tranger2_list_topic_names`**](timeranger2/timeranger2.md#tranger2_list_topic_names) — `PUBLIC json_t *tranger2_list_topic_names( json_t *tranger )`

11. [**`tranger2_list_keys`**](timeranger2/timeranger2.md#tranger2_list_keys) — `PUBLIC json_t *tranger2_list_keys( json_t *tranger, const char *topic_name )`

12. [**`tranger2_topic_size`**](timeranger2/timeranger2.md#tranger2_topic_size) — `PUBLIC uint64_t tranger2_topic_size( json_t *tranger, const char *topic_name )`

13. [**`tranger2_topic_key_size`**](timeranger2/timeranger2.md#tranger2_topic_key_size) — `PUBLIC uint64_t tranger2_topic_key_size( json_t *tranger, const char *topic_name, const char *key )`

14. [**`tranger2_topic_name`**](timeranger2/timeranger2.md#tranger2_topic_name) — `PUBLIC const char *tranger2_topic_name( json_t *topic )`

15. [**`tranger2_close_topic`**](timeranger2/timeranger2.md#tranger2_close_topic) — `PUBLIC int tranger2_close_topic( json_t *tranger, const char *topic_name )`

16. [**`tranger2_delete_topic`**](timeranger2/timeranger2.md#tranger2_delete_topic) — `PUBLIC int tranger2_delete_topic( json_t *tranger, const char *topic_name )`

17. [**`tranger2_backup_topic`**](timeranger2/timeranger2.md#tranger2_backup_topic) — `PUBLIC json_t *tranger2_backup_topic( json_t *tranger, const char *topic_name, const char *backup_path, const char *backup_name, BOOL overwrite_backup, tranger_backup_deleting_callback_t tranger_backup_deleting_callback )`

18. [**`tranger2_write_topic_var`**](timeranger2/timeranger2.md#tranger2_write_topic_var) — `PUBLIC int tranger2_write_topic_var( json_t *tranger, const char *topic_name, json_t *jn_topic_var )`

19. [**`tranger2_write_topic_cols`**](timeranger2/timeranger2.md#tranger2_write_topic_cols) — `PUBLIC int tranger2_write_topic_cols( json_t *tranger, const char *topic_name, json_t *jn_cols )`

20. [**`tranger2_topic_desc`**](timeranger2/timeranger2.md#tranger2_topic_desc) — `PUBLIC json_t *tranger2_topic_desc( json_t *tranger, const char *topic_name )`

21. [**`tranger2_list_topic_desc_cols`**](timeranger2/timeranger2.md#tranger2_list_topic_desc_cols) — `PUBLIC json_t *tranger2_list_topic_desc_cols( json_t *tranger, const char *topic_name )`

22. [**`tranger2_dict_topic_desc_cols`**](timeranger2/timeranger2.md#tranger2_dict_topic_desc_cols) — `PUBLIC json_t *tranger2_dict_topic_desc_cols( json_t *tranger, const char *topic_name )`

23. [**`tranger2_append_record`**](timeranger2/timeranger2.md#tranger2_append_record) — `PUBLIC int tranger2_append_record( json_t *tranger, const char *topic_name, uint64_t __t__, uint16_t user_flag, md2_record_ex_t *md_record_ex, json_t *jn_record )`

24. [**`tranger2_delete_record`**](timeranger2/timeranger2.md#tranger2_delete_record) — `PUBLIC int tranger2_delete_record( json_t *tranger, const char *topic_name, const char *key )`

25. [**`tranger2_write_user_flag`**](timeranger2/timeranger2.md#tranger2_write_user_flag) — `PUBLIC int tranger2_write_user_flag( json_t *tranger, const char *topic_name, const char *key, uint64_t __t__, uint64_t rowid, uint16_t user_flag )`

26. [**`tranger2_set_user_flag`**](timeranger2/timeranger2.md#tranger2_set_user_flag) — `PUBLIC int tranger2_set_user_flag( json_t *tranger, const char *topic_name, const char *key, uint64_t __t__, uint64_t rowid, uint16_t mask, BOOL set )`

27. [**`tranger2_read_user_flag`**](timeranger2/timeranger2.md#tranger2_read_user_flag) — `PUBLIC uint16_t tranger2_read_user_flag( json_t *tranger, const char *topic_name, const char *key, uint64_t __t__, uint64_t rowid )`

28. [**`tranger2_open_iterator`**](timeranger2/timeranger2.md#tranger2_open_iterator) — `PUBLIC json_t *tranger2_open_iterator( json_t *tranger, const char *topic_name, const char *key, json_t *match_cond, tranger2_load_record_callback_t load_record_callback, const char *iterator_id, const char *creator, json_t *data, json_t *extra )`

29. [**`tranger2_close_iterator`**](timeranger2/timeranger2.md#tranger2_close_iterator) — `PUBLIC int tranger2_close_iterator( json_t *tranger, json_t *iterator )`

30. [**`tranger2_get_iterator_by_id`**](timeranger2/timeranger2.md#tranger2_get_iterator_by_id) — `PUBLIC json_t *tranger2_get_iterator_by_id( json_t *tranger, const char *topic_name, const char *iterator_id, const char *creator )`

31. [**`tranger2_iterator_size`**](timeranger2/timeranger2.md#tranger2_iterator_size) — `PUBLIC size_t tranger2_iterator_size( json_t *iterator )`

32. [**`tranger2_iterator_get_page`**](timeranger2/timeranger2.md#tranger2_iterator_get_page) — `PUBLIC json_t *tranger2_iterator_get_page( json_t *tranger, json_t *iterator, json_int_t from_rowid, size_t limit, BOOL backward )`

33. [**`tranger2_open_rt_mem`**](timeranger2/timeranger2.md#tranger2_open_rt_mem) — `PUBLIC json_t *tranger2_open_rt_mem( json_t *tranger, const char *topic_name, const char *key, json_t *match_cond, tranger2_load_record_callback_t load_record_callback, const char *list_id, const char *creator, json_t *extra )`

34. [**`tranger2_close_rt_mem`**](timeranger2/timeranger2.md#tranger2_close_rt_mem) — `PUBLIC int tranger2_close_rt_mem( json_t *tranger, json_t *mem )`

35. [**`tranger2_get_rt_mem_by_id`**](timeranger2/timeranger2.md#tranger2_get_rt_mem_by_id) — `PUBLIC json_t *tranger2_get_rt_mem_by_id( json_t *tranger, const char *topic_name, const char *rt_id, const char *creator )`

36. [**`tranger2_open_rt_disk`**](timeranger2/timeranger2.md#tranger2_open_rt_disk) — `PUBLIC json_t *tranger2_open_rt_disk( json_t *tranger, const char *topic_name, const char *key, json_t *match_cond, tranger2_load_record_callback_t load_record_callback, const char *rt_id, const char *creator, json_t *extra )`

37. [**`tranger2_close_rt_disk`**](timeranger2/timeranger2.md#tranger2_close_rt_disk) — `PUBLIC int tranger2_close_rt_disk( json_t *tranger, json_t *disk )`

38. [**`tranger2_get_rt_disk_by_id`**](timeranger2/timeranger2.md#tranger2_get_rt_disk_by_id) — `PUBLIC json_t *tranger2_get_rt_disk_by_id( json_t *tranger, const char *topic_name, const char *rt_id, const char *creator )`

39. [**`tranger2_open_list`**](timeranger2/timeranger2.md#tranger2_open_list) — `PUBLIC json_t *tranger2_open_list( json_t *tranger, const char *topic_name, json_t *match_cond, json_t *extra, const char *rt_id, BOOL rt_by_disk, const char *creator )`

40. [**`tranger2_close_list`**](timeranger2/timeranger2.md#tranger2_close_list) — `PUBLIC int tranger2_close_list( json_t *tranger, json_t *list )`

41. [**`tranger2_close_all_lists`**](timeranger2/timeranger2.md#tranger2_close_all_lists) — `PUBLIC int tranger2_close_all_lists( json_t *tranger, const char *topic_name, const char *rt_id, const char *creator )`

42. [**`tranger2_read_record_content`**](timeranger2/timeranger2.md#tranger2_read_record_content) — `PUBLIC json_t *tranger2_read_record_content( json_t *tranger, json_t *topic, const char *key, md2_record_ex_t *md_record_ex )`

43. [**`tranger2_print_md0_record`**](timeranger2/timeranger2.md#tranger2_print_md0_record) — `PUBLIC void tranger2_print_md0_record( char *bf, int bfsize, const char *key, json_int_t rowid, const md2_record_ex_t *md_record_ex, BOOL print_local_time )`

44. [**`tranger2_print_md1_record`**](timeranger2/timeranger2.md#tranger2_print_md1_record) — `PUBLIC void tranger2_print_md1_record( char *bf, int bfsize, const char *key, json_int_t rowid, const md2_record_ex_t *md_record_ex, BOOL print_local_time )`

45. [**`tranger2_print_md2_record`**](timeranger2/timeranger2.md#tranger2_print_md2_record) — `PUBLIC void tranger2_print_md2_record( char *bf, int bfsize, json_t *tranger, json_t *topic, const char *key, json_int_t rowid, const md2_record_ex_t *md_record_ex, BOOL print_local_time )`

46. [**`tranger2_print_record_filename`**](timeranger2/timeranger2.md#tranger2_print_record_filename) — `PUBLIC void tranger2_print_record_filename( char *bf, int bfsize, json_t *tranger, json_t *topic, const md2_record_ex_t *md_record_ex, BOOL print_local_time )`

47. [**`tranger2_set_trace_level`**](timeranger2/timeranger2.md#tranger2_set_trace_level) — `PUBLIC void tranger2_set_trace_level( json_t *tranger, int trace_level )`

### `tr_msg.h` — 16 functions

**Source:** `kernel/c/timeranger2/src/tr_msg.h`

1. [**`trmsg_open_topics`**](timeranger2/tr_msg.md#trmsg_open_topics) — `PUBLIC int trmsg_open_topics( json_t *tranger, const topic_desc_t *descs )`

2. [**`trmsg_close_topics`**](timeranger2/tr_msg.md#trmsg_close_topics) — `PUBLIC int trmsg_close_topics( json_t *tranger, const topic_desc_t *descs )`

3. [**`trmsg_add_instance`**](timeranger2/tr_msg.md#trmsg_add_instance) — `PUBLIC int trmsg_add_instance( json_t *tranger, const char *topic_name, json_t *jn_msg, md2_record_ex_t *md_record )`

4. [**`trmsg_open_list`**](timeranger2/tr_msg.md#trmsg_open_list) — `PUBLIC json_t *trmsg_open_list( json_t *tranger, const char *topic_name, json_t *match_cond, json_t *extra, const char *rt_id, BOOL rt_by_disk, const char *creator )`

5. [**`trmsg_close_list`**](timeranger2/tr_msg.md#trmsg_close_list) — `PUBLIC int trmsg_close_list( json_t *tranger, json_t *list )`

6. [**`trmsg_get_messages`**](timeranger2/tr_msg.md#trmsg_get_messages) — `PUBLIC json_t *trmsg_get_messages( json_t *list )`

7. [**`trmsg_get_message`**](timeranger2/tr_msg.md#trmsg_get_message) — `PUBLIC json_t *trmsg_get_message( json_t *list, const char *key )`

8. [**`trmsg_get_active_message`**](timeranger2/tr_msg.md#trmsg_get_active_message) — `PUBLIC json_t *trmsg_get_active_message( json_t *list, const char *key )`

9. [**`trmsg_get_active_md`**](timeranger2/tr_msg.md#trmsg_get_active_md) — `PUBLIC json_t *trmsg_get_active_md( json_t *list, const char *key )`

10. [**`trmsg_get_instances`**](timeranger2/tr_msg.md#trmsg_get_instances) — `PUBLIC json_t *trmsg_get_instances( json_t *list, const char *key )`

11. [**`trmsg_data_tree`**](timeranger2/tr_msg.md#trmsg_data_tree) — `PUBLIC json_t *trmsg_data_tree( json_t *list, json_t *jn_filter )`

12. [**`trmsg_active_records`**](timeranger2/tr_msg.md#trmsg_active_records) — `PUBLIC json_t *trmsg_active_records( json_t *list, json_t *jn_filter )`

13. [**`trmsg_record_instances`**](timeranger2/tr_msg.md#trmsg_record_instances) — `PUBLIC json_t *trmsg_record_instances( json_t *list, const char *key, json_t *jn_filter )`

14. [**`trmsg_foreach_active_messages`**](timeranger2/tr_msg.md#trmsg_foreach_active_messages) — `PUBLIC int trmsg_foreach_active_messages( json_t *list, int (*callback)( json_t *list, const char *key, json_t *record , void *user_data1, void *user_data2 ), void *user_data1, void *user_data2, json_t *jn_filter )`

15. [**`trmsg_foreach_instances_messages`**](timeranger2/tr_msg.md#trmsg_foreach_instances_messages) — `PUBLIC int trmsg_foreach_instances_messages( json_t *list, int (*callback)( json_t *list, const char *key, json_t *instances, void *user_data1, void *user_data2 ), void *user_data1, void *user_data2, json_t *jn_filter )`

16. [**`trmsg_foreach_messages`**](timeranger2/tr_msg.md#trmsg_foreach_messages) — `PUBLIC int trmsg_foreach_messages( json_t *list, BOOL duplicated, int (*callback)( json_t *list, const char *key, json_t *instances, void *user_data1, void *user_data2 ), void *user_data1, void *user_data2, json_t *jn_filter )`

### `tr_msg2db.h` — 6 functions

**Source:** `kernel/c/timeranger2/src/tr_msg2db.h`

1. [**`msg2db_open_db`**](timeranger2/tr_msg2db.md#msg2db_open_db) — `PUBLIC json_t *msg2db_open_db( json_t *tranger, const char *msg2db_name, json_t *jn_schema, const char *options )`

2. [**`msg2db_close_db`**](timeranger2/tr_msg2db.md#msg2db_close_db) — `PUBLIC int msg2db_close_db( json_t *tranger, const char *msg2db_name )`

3. [**`msg2db_append_message`**](timeranger2/tr_msg2db.md#msg2db_append_message) — `PUBLIC json_t *msg2db_append_message( json_t *tranger, const char *msg2db_name, const char *topic_name, json_t *kw, const char *options )`

4. [**`msg2db_list_messages`**](timeranger2/tr_msg2db.md#msg2db_list_messages) — `PUBLIC json_t *msg2db_list_messages( json_t *tranger, const char *msg2db_name, const char *topic_name, json_t *jn_ids, json_t *jn_filter, BOOL (*match_fn) ( json_t *kw, json_t *jn_filter ) )`

5. [**`msg2db_get_message`**](timeranger2/tr_msg2db.md#msg2db_get_message) — `PUBLIC json_t *msg2db_get_message( json_t *tranger, const char *msg2db_name, const char *topic_name, const char *id, const char *id2 )`

6. [**`build_msg2db_index_path`**](timeranger2/tr_msg2db.md#build_msg2db_index_path) — `PUBLIC char *build_msg2db_index_path( char *bf, int bfsize, const char *msg2db_name, const char *topic_name, const char *key )`

### `tr_queue.h` — 17 functions

**Source:** `kernel/c/timeranger2/src/tr_queue.h`

1. [**`trq_open`**](timeranger2/tr_queue.md#trq_open) — `PUBLIC tr_queue_t *trq_open( json_t *tranger, const char *topic_name, const char *tkey, system_flag2_t system_flag, size_t backup_queue_size )`

2. [**`trq_close`**](timeranger2/tr_queue.md#trq_close) — `PUBLIC void trq_close(tr_queue_t * trq)`

3. [**`trq_load`**](timeranger2/tr_queue.md#trq_load) — `PUBLIC int trq_load(tr_queue_t * trq)`

4. [**`trq_load_all`**](timeranger2/tr_queue.md#trq_load_all) — `PUBLIC int trq_load_all(tr_queue_t * trq, int64_t from_rowid, int64_t to_rowid)`

5. [**`trq_load_all_by_time`**](timeranger2/tr_queue.md#trq_load_all_by_time) — `PUBLIC int trq_load_all_by_time(tr_queue_t * trq, int64_t from_t, int64_t to_t)`

6. [**`trq_append2`**](timeranger2/tr_queue.md#trq_append2) — `PUBLIC q_msg_t * trq_append2( tr_queue_t * trq, json_int_t t, json_t *kw, uint16_t user_flag )`

7. [**`trq_get_by_rowid`**](timeranger2/tr_queue.md#trq_get_by_rowid) — `PUBLIC q_msg_t * trq_get_by_rowid(tr_queue_t * trq, uint64_t rowid)`

8. [**`trq_check_pending_rowid`**](timeranger2/tr_queue.md#trq_check_pending_rowid) — `PUBLIC int trq_check_pending_rowid( tr_queue_t * trq, uint64_t __t__, uint64_t rowid )`

9. [**`trq_unload_msg`**](timeranger2/tr_queue.md#trq_unload_msg) — `PUBLIC void trq_unload_msg(q_msg_t *msg, int32_t result)`

10. [**`trq_set_hard_flag`**](timeranger2/tr_queue.md#trq_set_hard_flag) — `PUBLIC int trq_set_hard_flag(q_msg_t *msg, uint16_t hard_mark, BOOL set)`

11. [**`trq_set_soft_mark`**](timeranger2/tr_queue.md#trq_set_soft_mark) — `PUBLIC uint64_t trq_set_soft_mark(q_msg_t *msg, uint64_t soft_mark, BOOL set)`

12. [**`trq_msg_md`**](timeranger2/tr_queue.md#trq_msg_md) — `PUBLIC md2_record_ex_t *trq_msg_md(q_msg_t *msg)`

13. [**`trq_msg_json`**](timeranger2/tr_queue.md#trq_msg_json) — `PUBLIC json_t *trq_msg_json(q_msg_t *msg)`

14. [**`trq_set_metadata`**](timeranger2/tr_queue.md#trq_set_metadata) — `PUBLIC int trq_set_metadata( json_t *kw, const char *key, json_t *jn_value )`

15. [**`trq_get_metadata`**](timeranger2/tr_queue.md#trq_get_metadata) — `PUBLIC json_t *trq_get_metadata( json_t *kw )`

16. [**`trq_answer`**](timeranger2/tr_queue.md#trq_answer) — `PUBLIC json_t *trq_answer( json_t *jn_message, int result )`

17. [**`trq_check_backup`**](timeranger2/tr_queue.md#trq_check_backup) — `PUBLIC int trq_check_backup(tr_queue_t * trq)`

### `tr_treedb.h` — 50 functions

**Source:** `kernel/c/timeranger2/src/tr_treedb.h`

1. [**`treedb_open_db`**](timeranger2/treedb.md#treedb_open_db) — `PUBLIC json_t *treedb_open_db( json_t *tranger, const char *treedb_name, json_t *jn_schema, const char *options )`

2. [**`treedb_close_db`**](timeranger2/treedb.md#treedb_close_db) — `PUBLIC int treedb_close_db( json_t *tranger, const char *treedb_name )`

3. [**`treedb_set_callback`**](timeranger2/treedb.md#treedb_set_callback) — `PUBLIC int treedb_set_callback( json_t *tranger, const char *treedb_name, treedb_callback_t treedb_callback, void *user_data, treedb_callback_flag_t flags )`

4. [**`treedb_create_topic`**](timeranger2/treedb.md#treedb_create_topic) — `PUBLIC json_t *treedb_create_topic( json_t *tranger, const char *treedb_name, const char *topic_name, int topic_version, const char *topic_tkey, json_t *pkey2s, json_t *jn_cols, uint32_t snap_tag, BOOL create_schema )`

5. [**`treedb_close_topic`**](timeranger2/treedb.md#treedb_close_topic) — `PUBLIC int treedb_close_topic( json_t *tranger, const char *treedb_name, const char *topic_name )`

6. [**`treedb_delete_topic`**](timeranger2/treedb.md#treedb_delete_topic) — `PUBLIC int treedb_delete_topic( json_t *tranger, const char *treedb_name, const char *topic_name )`

7. [**`treedb_list_treedb`**](timeranger2/treedb.md#treedb_list_treedb) — `PUBLIC json_t *treedb_list_treedb( json_t *tranger, json_t *kw )`

8. [**`treedb_topics`**](timeranger2/treedb.md#treedb_topics) — `PUBLIC json_t *treedb_topics( json_t *tranger, const char *treedb_name, json_t *jn_options )`

9. [**`treedb_topic_size`**](timeranger2/treedb.md#treedb_topic_size) — `PUBLIC size_t treedb_topic_size( json_t *tranger, const char *treedb_name, const char *topic_name )`

10. [**`_treedb_create_topic_cols_desc`**](timeranger2/treedb.md#_treedb_create_topic_cols_desc) — `PUBLIC json_t *_treedb_create_topic_cols_desc(void)`

11. [**`parse_schema`**](timeranger2/treedb.md#parse_schema) — `PUBLIC int parse_schema( json_t *schema )`

12. [**`parse_schema_cols`**](timeranger2/treedb.md#parse_schema_cols) — `PUBLIC int parse_schema_cols( json_t *cols_desc, json_t *data )`

13. [**`parse_hooks`**](timeranger2/treedb.md#parse_hooks) — `PUBLIC int parse_hooks( json_t *schema )`

14. [**`topic_desc_hook_names`**](timeranger2/treedb.md#topic_desc_hook_names) — `PUBLIC json_t *topic_desc_hook_names( json_t *topic_desc )`

15. [**`topic_desc_fkey_names`**](timeranger2/treedb.md#topic_desc_fkey_names) — `PUBLIC json_t *topic_desc_fkey_names( json_t *topic_desc )`

16. [**`get_hook_list`**](timeranger2/treedb.md#get_hook_list) — `PUBLIC json_t *get_hook_list( hgobj gobj, json_t *hook_data )`

17. [**`current_snap_tag`**](timeranger2/treedb.md#current_snap_tag) — `PUBLIC int current_snap_tag( json_t *tranger, const char *treedb_name )`

18. [**`treedb_is_treedbs_topic`**](timeranger2/treedb.md#treedb_is_treedbs_topic) — `PUBLIC BOOL treedb_is_treedbs_topic( json_t *tranger, const char *treedb_name, const char *topic_name )`

19. [**`treedb_get_id_index`**](timeranger2/treedb.md#treedb_get_id_index) — `PUBLIC json_t *treedb_get_id_index( json_t *tranger, const char *treedb_name, const char *topic_name )`

20. [**`treedb_topic_pkey2s`**](timeranger2/treedb.md#treedb_topic_pkey2s) — `PUBLIC json_t *treedb_topic_pkey2s( json_t *tranger, const char *topic_name )`

21. [**`treedb_topic_pkey2s_filter`**](timeranger2/treedb.md#treedb_topic_pkey2s_filter) — `PUBLIC json_t *treedb_topic_pkey2s_filter( json_t *tranger, const char *topic_name, json_t *node, const char *id )`

22. [**`treedb_set_trace`**](timeranger2/treedb.md#treedb_set_trace) — `PUBLIC int treedb_set_trace(BOOL set)`

23. [**`decode_parent_ref`**](timeranger2/treedb.md#decode_parent_ref) — `PUBLIC BOOL decode_parent_ref( const char *pref, char *topic_name, int topic_name_size, char *id, int id_size, char *hook_name, int hook_name_size )`

24. [**`decode_child_ref`**](timeranger2/treedb.md#decode_child_ref) — `PUBLIC BOOL decode_child_ref( const char *pref, char *topic_name, int topic_name_size, char *id, int id_size )`

25. [**`treedb_create_node`**](timeranger2/treedb.md#treedb_create_node) — `PUBLIC json_t *treedb_create_node( json_t *tranger, const char *treedb_name, const char *topic_name, json_t *kw )`

26. [**`treedb_save_node`**](timeranger2/treedb.md#treedb_save_node) — `PUBLIC int treedb_save_node( json_t *tranger, json_t *node )`

27. [**`treedb_update_node`**](timeranger2/treedb.md#treedb_update_node) — `PUBLIC json_t *treedb_update_node( json_t *tranger, json_t *node, json_t *kw, BOOL save )`

28. [**`set_volatil_values`**](timeranger2/treedb.md#set_volatil_values) — `PUBLIC int set_volatil_values( json_t *tranger, const char *topic_name, json_t *record, json_t *kw, BOOL broadcast )`

29. [**`treedb_delete_node`**](timeranger2/treedb.md#treedb_delete_node) — `PUBLIC int treedb_delete_node( json_t *tranger, json_t *node, json_t *jn_options )`

30. [**`treedb_delete_instance`**](timeranger2/treedb.md#treedb_delete_instance) — `PUBLIC int treedb_delete_instance( json_t *tranger, json_t *node, const char *pkey2_name, json_t *jn_options )`

31. [**`treedb_clean_node`**](timeranger2/treedb.md#treedb_clean_node) — `PUBLIC int treedb_clean_node( json_t *tranger, json_t *node, BOOL save )`

32. [**`treedb_autolink`**](timeranger2/treedb.md#treedb_autolink) — `PUBLIC int treedb_autolink( json_t *tranger, json_t *node, json_t *kw, BOOL save )`

33. [**`treedb_link_nodes`**](timeranger2/treedb.md#treedb_link_nodes) — `PUBLIC int treedb_link_nodes( json_t *tranger, const char *hook, json_t *parent_node, json_t *child_node )`

34. [**`treedb_unlink_nodes`**](timeranger2/treedb.md#treedb_unlink_nodes) — `PUBLIC int treedb_unlink_nodes( json_t *tranger, const char *hook, json_t *parent_node, json_t *child_node )`

35. [**`treedb_get_node`**](timeranger2/treedb.md#treedb_get_node) — `PUBLIC json_t *treedb_get_node( json_t *tranger, const char *treedb_name, const char *topic_name, const char *id )`

36. [**`treedb_get_instance`**](timeranger2/treedb.md#treedb_get_instance) — `PUBLIC json_t *treedb_get_instance( json_t *tranger, const char *treedb_name, const char *topic_name, const char *pkey2_name, const char *id, const char *key2 )`

37. [**`node_collapsed_view`**](timeranger2/treedb.md#node_collapsed_view) — `PUBLIC json_t *node_collapsed_view( json_t *tranger, json_t *node, json_t *jn_options )`

38. [**`treedb_list_nodes`**](timeranger2/treedb.md#treedb_list_nodes) — `PUBLIC json_t *treedb_list_nodes( json_t *tranger, const char *treedb_name, const char *topic_name, json_t *jn_filter, BOOL (*match_fn) ( json_t *topic_desc, json_t *node, json_t *jn_filter ) )`

39. [**`treedb_list_instances`**](timeranger2/treedb.md#treedb_list_instances) — `PUBLIC json_t *treedb_list_instances( json_t *tranger, const char *treedb_name, const char *topic_name, const char *pkey2_name, json_t *jn_filter, BOOL (*match_fn) ( json_t *topic_desc, json_t *node, json_t *jn_filter ) )`

40. [**`treedb_parent_refs`**](timeranger2/treedb.md#treedb_parent_refs) — `PUBLIC json_t *treedb_parent_refs( json_t *tranger, const char *fkey, json_t *node, json_t *jn_options )`

41. [**`treedb_list_parents`**](timeranger2/treedb.md#treedb_list_parents) — `PUBLIC json_t *treedb_list_parents( json_t *tranger, const char *fkey, json_t *node, json_t *jn_options )`

42. [**`treedb_node_children`**](timeranger2/treedb.md#treedb_node_children) — `PUBLIC json_t *treedb_node_children( json_t *tranger, const char *hook, json_t *node, json_t *jn_filter, json_t *jn_options )`

43. [**`add_jtree_path`**](timeranger2/treedb.md#add_jtree_path) — `PUBLIC int add_jtree_path( json_t *parent, json_t *child )`

44. [**`treedb_node_jtree`**](timeranger2/treedb.md#treedb_node_jtree) — `PUBLIC json_t *treedb_node_jtree( json_t *tranger, const char *hook, const char *rename_hook, json_t *node, json_t *jn_filter, json_t *jn_options )`

45. [**`treedb_get_topic_links`**](timeranger2/treedb.md#treedb_get_topic_links) — `PUBLIC json_t *treedb_get_topic_links( json_t *tranger, const char *treedb_name, const char *topic_name )`

46. [**`treedb_get_topic_hooks`**](timeranger2/treedb.md#treedb_get_topic_hooks) — `PUBLIC json_t *treedb_get_topic_hooks( json_t *tranger, const char *treedb_name, const char *topic_name )`

47. [**`treedb_shoot_snap`**](timeranger2/treedb.md#treedb_shoot_snap) — `PUBLIC int treedb_shoot_snap( json_t *tranger, const char *treedb_name, const char *snap_name, const char *description )`

48. [**`treedb_activate_snap`**](timeranger2/treedb.md#treedb_activate_snap) — `PUBLIC int treedb_activate_snap( json_t *tranger, const char *treedb_name, const char *snap_name )`

49. [**`treedb_list_snaps`**](timeranger2/treedb.md#treedb_list_snaps) — `PUBLIC json_t *treedb_list_snaps( json_t *tranger, const char *treedb_name, json_t *filter )`

50. [**`create_template_record`**](timeranger2/treedb.md#create_template_record) — `PUBLIC json_t *create_template_record( const char *template_name, json_t *cols, json_t *kw )`

**Total: 139 functions**

## root-linux (Runtime GClasses)

### `c_auth_bff.h` — 1 functions

**Source:** `kernel/c/root-linux/src/c_auth_bff.h`

1. [**`register_c_auth_bff`**](runtime/registration.md#register_c_auth_bff) — `PUBLIC int register_c_auth_bff(void)`

### `c_authz.h` — 3 functions

**Source:** `kernel/c/root-linux/src/c_authz.h`

1. [**`register_c_authz`**](runtime/registration.md#register_c_authz) — `PUBLIC int register_c_authz(void)`

2. [**`authz_checker`**](gobj/authz.md#authz_checker) — `PUBLIC BOOL authz_checker(hgobj gobj_to_check, const char *authz, json_t *kw, hgobj src)`

3. [**`authentication_parser`**](gobj/authz.md#api-authentication_parser) — `PUBLIC json_t *authentication_parser(hgobj gobj_service, json_t *kw, hgobj src)`

### `c_channel.h` — 1 functions

**Source:** `kernel/c/root-linux/src/c_channel.h`

1. [**`register_c_channel`**](runtime/registration.md#register_c_channel) — `PUBLIC int register_c_channel(void)`

### `c_counter.h` — 1 functions

**Source:** `kernel/c/root-linux/src/c_counter.h`

1. [**`register_c_counter`**](runtime/registration.md#register_c_counter) — `PUBLIC int register_c_counter(void)`

### `c_fs.h` — 1 functions

**Source:** `kernel/c/root-linux/src/c_fs.h`

1. [**`register_c_fs`**](runtime/registration.md#register_c_fs) — `PUBLIC int register_c_fs(void)`

### `c_gss_udp_s.h` — 1 functions

**Source:** `kernel/c/root-linux/src/c_gss_udp_s.h`

1. [**`register_c_gss_udp_s`**](runtime/registration.md#register_c_gss_udp_s) — `PUBLIC int register_c_gss_udp_s(void)`

### `c_ievent_cli.h` — 1 functions

**Source:** `kernel/c/root-linux/src/c_ievent_cli.h`

1. [**`register_c_ievent_cli`**](runtime/registration.md#register_c_ievent_cli) — `PUBLIC int register_c_ievent_cli(void)`

### `c_ievent_srv.h` — 1 functions

**Source:** `kernel/c/root-linux/src/c_ievent_srv.h`

1. [**`register_c_ievent_srv`**](runtime/registration.md#register_c_ievent_srv) — `PUBLIC int register_c_ievent_srv(void)`

### `c_iogate.h` — 1 functions

**Source:** `kernel/c/root-linux/src/c_iogate.h`

1. [**`register_c_iogate`**](runtime/registration.md#register_c_iogate) — `PUBLIC int register_c_iogate(void)`

### `c_mqiogate.h` — 1 functions

**Source:** `kernel/c/root-linux/src/c_mqiogate.h`

1. [**`register_c_mqiogate`**](runtime/registration.md#register_c_mqiogate) — `PUBLIC int register_c_mqiogate(void)`

### `c_node.h` — 1 functions

**Source:** `kernel/c/root-linux/src/c_node.h`

1. [**`register_c_node`**](runtime/registration.md#register_c_node) — `PUBLIC int register_c_node(void)`

### `c_ota.h` — 1 functions

**Source:** `kernel/c/root-linux/src/c_ota.h`

1. [**`register_c_ota`**](runtime/registration.md#register_c_ota) — `PUBLIC int register_c_ota(void)`

### `c_prot_http_cl.h` — 1 functions

**Source:** `kernel/c/root-linux/src/c_prot_http_cl.h`

1. [**`register_c_prot_http_cl`**](runtime/registration.md#register_c_prot_http_cl) — `PUBLIC int register_c_prot_http_cl(void)`

### `c_prot_http_sr.h` — 1 functions

**Source:** `kernel/c/root-linux/src/c_prot_http_sr.h`

1. [**`register_c_prot_http_sr`**](runtime/registration.md#register_c_prot_http_sr) — `PUBLIC int register_c_prot_http_sr(void)`

### `c_prot_raw.h` — 1 functions

**Source:** `kernel/c/root-linux/src/c_prot_raw.h`

1. [**`register_c_prot_raw`**](runtime/registration.md#register_c_prot_raw) — `PUBLIC int register_c_prot_raw(void)`

### `c_prot_tcp4h.h` — 1 functions

**Source:** `kernel/c/root-linux/src/c_prot_tcp4h.h`

1. [**`register_c_prot_tcp4h`**](runtime/registration.md#register_c_prot_tcp4h) — `PUBLIC int register_c_prot_tcp4h(void)`

### `c_pty.h` — 1 functions

**Source:** `kernel/c/root-linux/src/c_pty.h`

1. [**`register_c_pty`**](runtime/registration.md#register_c_pty) — `PUBLIC int register_c_pty(void)`

### `c_qiogate.h` — 1 functions

**Source:** `kernel/c/root-linux/src/c_qiogate.h`

1. [**`register_c_qiogate`**](runtime/registration.md#register_c_qiogate) — `PUBLIC int register_c_qiogate(void)`

### `c_resource2.h` — 1 functions

**Source:** `kernel/c/root-linux/src/c_resource2.h`

1. [**`register_c_resource2`**](runtime/registration.md#register_c_resource2) — `PUBLIC int register_c_resource2(void)`

### `c_task.h` — 1 functions

**Source:** `kernel/c/root-linux/src/c_task.h`

1. [**`register_c_task`**](runtime/registration.md#register_c_task) — `PUBLIC int register_c_task(void)`

### `c_task_authenticate.h` — 1 functions

**Source:** `kernel/c/root-linux/src/c_task_authenticate.h`

1. [**`register_c_task_authenticate`**](runtime/registration.md#register_c_task_authenticate) — `PUBLIC int register_c_task_authenticate(void)`

### `c_tcp.h` — 1 functions

**Source:** `kernel/c/root-linux/src/c_tcp.h`

1. [**`register_c_tcp`**](runtime/registration.md#register_c_tcp) — `PUBLIC int register_c_tcp(void)`

### `c_tcp_s.h` — 1 functions

**Source:** `kernel/c/root-linux/src/c_tcp_s.h`

1. [**`register_c_tcp_s`**](runtime/registration.md#register_c_tcp_s) — `PUBLIC int register_c_tcp_s(void)`

### `c_timer.h` — 4 functions

**Source:** `kernel/c/root-linux/src/c_timer.h`

1. [**`register_c_timer`**](runtime/timer.md#register_c_timer) — `PUBLIC int register_c_timer(void)`

2. [**`set_timeout`**](runtime/timer.md#set_timeout) — `PUBLIC void set_timeout(hgobj gobj, json_int_t msec)`

3. [**`set_timeout_periodic`**](runtime/timer.md#set_timeout_periodic) — `PUBLIC void set_timeout_periodic(hgobj gobj, json_int_t msec)`

4. [**`clear_timeout`**](runtime/timer.md#clear_timeout) — `PUBLIC void clear_timeout(hgobj gobj)`

### `c_timer0.h` — 4 functions

**Source:** `kernel/c/root-linux/src/c_timer0.h`

1. [**`register_c_timer0`**](runtime/timer.md#register_c_timer0) — `PUBLIC int register_c_timer0(void)`

2. [**`set_timeout0`**](runtime/timer.md#set_timeout0) — `PUBLIC void set_timeout0(hgobj gobj, json_int_t msec)`

3. [**`set_timeout_periodic0`**](runtime/timer.md#set_timeout_periodic0) — `PUBLIC void set_timeout_periodic0(hgobj gobj, json_int_t msec)`

4. [**`clear_timeout0`**](runtime/timer.md#clear_timeout0) — `PUBLIC void clear_timeout0(hgobj gobj)`

### `c_tranger.h` — 1 functions

**Source:** `kernel/c/root-linux/src/c_tranger.h`

1. [**`register_c_tranger`**](runtime/registration.md#register_c_tranger) — `PUBLIC int register_c_tranger(void)`

### `c_treedb.h` — 1 functions

**Source:** `kernel/c/root-linux/src/c_treedb.h`

1. [**`register_c_treedb`**](runtime/registration.md#register_c_treedb) — `PUBLIC int register_c_treedb(void)`

### `c_uart.h` — 1 functions

**Source:** `kernel/c/root-linux/src/c_uart.h`

1. [**`register_c_uart`**](runtime/registration.md#register_c_uart) — `PUBLIC int register_c_uart(void)`

### `c_udp.h` — 1 functions

**Source:** `kernel/c/root-linux/src/c_udp.h`

1. [**`register_c_udp`**](runtime/registration.md#register_c_udp) — `PUBLIC int register_c_udp(void)`

### `c_udp_s.h` — 1 functions

**Source:** `kernel/c/root-linux/src/c_udp_s.h`

1. [**`register_c_udp_s`**](runtime/registration.md#register_c_udp_s) — `PUBLIC int register_c_udp_s(void)`

### `c_websocket.h` — 1 functions

**Source:** `kernel/c/root-linux/src/c_websocket.h`

1. [**`register_c_websocket`**](runtime/registration.md#register_c_websocket) — `PUBLIC int register_c_websocket(void)`

### `c_yuno.h` — 10 functions

**Source:** `kernel/c/root-linux/src/c_yuno.h`

1. [**`register_c_yuno`**](runtime/yuno.md#register_c_yuno) — `PUBLIC int register_c_yuno(void)`

2. [**`yuno_event_loop`**](runtime/yuno.md#yuno_event_loop) — `PUBLIC void *yuno_event_loop(void)`

3. [**`yuno_event_detroy`**](runtime/yuno.md#yuno_event_detroy) — `PUBLIC void yuno_event_detroy(void)`

4. [**`set_yuno_must_die`**](runtime/yuno.md#set_yuno_must_die) — `PUBLIC void set_yuno_must_die(void)`

5. [**`is_ip_allowed`**](runtime/yuno.md#is_ip_allowed) — `PUBLIC BOOL is_ip_allowed(const char *peername)`

6. [**`add_allowed_ip`**](runtime/yuno.md#add_allowed_ip) — `PUBLIC int add_allowed_ip(const char *ip, BOOL allowed)`

7. [**`remove_allowed_ip`**](runtime/yuno.md#remove_allowed_ip) — `PUBLIC int remove_allowed_ip(const char *ip)`

8. [**`is_ip_denied`**](runtime/yuno.md#is_ip_denied) — `PUBLIC BOOL is_ip_denied(const char *peername)`

9. [**`add_denied_ip`**](runtime/yuno.md#add_denied_ip) — `PUBLIC int add_denied_ip(const char *ip, BOOL denied)`

10. [**`remove_denied_ip`**](runtime/yuno.md#remove_denied_ip) — `PUBLIC int remove_denied_ip(const char *ip)`

### `dbsimple.h` — 4 functions

**Source:** `kernel/c/root-linux/src/dbsimple.h`

1. [**`db_load_persistent_attrs`**](runtime/dbsimple.md#db_load_persistent_attrs) — `PUBLIC int db_load_persistent_attrs( hgobj gobj, json_t *keys )`

2. [**`db_save_persistent_attrs`**](runtime/dbsimple.md#db_save_persistent_attrs) — `PUBLIC int db_save_persistent_attrs( hgobj gobj, json_t *keys )`

3. [**`db_remove_persistent_attrs`**](runtime/dbsimple.md#db_remove_persistent_attrs) — `PUBLIC int db_remove_persistent_attrs( hgobj gobj, json_t *keys )`

4. [**`db_list_persistent_attrs`**](runtime/dbsimple.md#db_list_persistent_attrs) — `PUBLIC json_t *db_list_persistent_attrs( hgobj gobj, json_t *keys )`

### `entry_point.h` — 4 functions

**Source:** `kernel/c/root-linux/src/entry_point.h`

1. [**`yuneta_setup`**](runtime/entry_point.md#yuneta_setup) — `PUBLIC int yuneta_setup( const persistent_attrs_t *persistent_attrs, json_function_fn command_parser, json_function_fn stats_parser, authorization_checker_fn authz_checker, authentication_parser_fn authentication_parser, size_t mem_max_block, size_t mem_max_system_memory, BOOL use_own_system_memory, size_t mem_min_block, size_t mem_superblock )`

2. [**`yuneta_entry_point`**](runtime/entry_point.md#yuneta_entry_point) — `PUBLIC int yuneta_entry_point(int argc, char *argv[], const char *APP_NAME, const char *APP_VERSION, const char *APP_SUPPORT, const char *APP_DOC, const char *APP_DATETIME, const char *fixed_config, const char *variable_config, int (*register_yuno_and_more)(void), void (*cleaning_fn)(void) )`

3. [**`set_auto_kill_time`**](runtime/entry_point.md#set_auto_kill_time) — `PUBLIC void set_auto_kill_time(int seconds)`

4. [**`yuneta_json_config`**](runtime/entry_point.md#yuneta_json_config) — `PUBLIC json_t *yuneta_json_config(void)`

### `ghttp_parser.h` — 4 functions

**Source:** `kernel/c/root-linux/src/ghttp_parser.h`

1. [**`ghttp_parser_create`**](helpers/http_parser.md#ghttp_parser_create) — `PUBLIC GHTTP_PARSER *ghttp_parser_create( hgobj gobj, llhttp_type_t type, gobj_event_t on_header_event, gobj_event_t on_body_event, gobj_event_t on_message_event, BOOL send_event )`

2. [**`ghttp_parser_received`**](helpers/http_parser.md#ghttp_parser_received) — `PUBLIC int ghttp_parser_received( GHTTP_PARSER *parser, char *bf, size_t len )`

3. [**`ghttp_parser_finish`**](helpers/http_parser.md#ghttp_parser_finish) — `PUBLIC int ghttp_parser_finish(GHTTP_PARSER *parser)`

4. [**`ghttp_parser_destroy`**](helpers/http_parser.md#ghttp_parser_destroy) — `PUBLIC void ghttp_parser_destroy(GHTTP_PARSER *parser)`

### `istream.h` — 15 functions

**Source:** `kernel/c/root-linux/src/istream.h`

1. [**`istream_create`**](helpers/istream.md#istream_create) — `PUBLIC istream_h istream_create( hgobj gobj, size_t data_size, size_t max_size )`

2. [**`istream_destroy`**](helpers/istream.md#istream_destroy) — `PUBLIC void istream_destroy( istream_h istream )`

3. [**`istream_read_until_num_bytes`**](helpers/istream.md#istream_read_until_num_bytes) — `PUBLIC int istream_read_until_num_bytes( istream_h istream, size_t num_bytes, gobj_event_t event )`

4. [**`istream_read_until_delimiter`**](helpers/istream.md#istream_read_until_delimiter) — `PUBLIC int istream_read_until_delimiter( istream_h istream, const char *delimiter, size_t delimiter_size, gobj_event_t event )`

5. [**`istream_consume`**](helpers/istream.md#istream_consume) — `PUBLIC size_t istream_consume( istream_h istream, char *bf, size_t len )`

6. [**`istream_cur_rd_pointer`**](helpers/istream.md#istream_cur_rd_pointer) — `PUBLIC char *istream_cur_rd_pointer( istream_h istream )`

7. [**`istream_length`**](helpers/istream.md#istream_length) — `PUBLIC size_t istream_length( istream_h istream )`

8. [**`istream_get_gbuffer`**](helpers/istream.md#istream_get_gbuffer) — `PUBLIC gbuffer_t *istream_get_gbuffer( istream_h istream )`

9. [**`istream_pop_gbuffer`**](helpers/istream.md#istream_pop_gbuffer) — `PUBLIC gbuffer_t *istream_pop_gbuffer( istream_h istream )`

10. [**`istream_new_gbuffer`**](helpers/istream.md#istream_new_gbuffer) — `PUBLIC int istream_new_gbuffer( istream_h istream, size_t data_size, size_t max_size )`

11. [**`istream_extract_matched_data`**](helpers/istream.md#istream_extract_matched_data) — `PUBLIC char *istream_extract_matched_data( istream_h istream, size_t *len )`

12. [**`istream_reset_wr`**](helpers/istream.md#istream_reset_wr) — `PUBLIC int istream_reset_wr( istream_h istream )`

13. [**`istream_reset_rd`**](helpers/istream.md#istream_reset_rd) — `PUBLIC int istream_reset_rd( istream_h istream )`

14. [**`istream_clear`**](helpers/istream.md#istream_clear) — `PUBLIC void istream_clear( istream_h istream )`

15. [**`istream_is_completed`**](helpers/istream.md#istream_is_completed) — `PUBLIC BOOL istream_is_completed( istream_h istream )`

### `manage_services.h` — 3 functions

**Source:** `kernel/c/root-linux/src/manage_services.h`

1. [**`run_services`**](runtime/entry_point.md#run_services) — `PUBLIC void run_services(void)`

2. [**`stop_services`**](runtime/entry_point.md#stop_services) — `PUBLIC void stop_services(void)`

3. [**`yuno_shutdown`**](runtime/entry_point.md#yuno_shutdown) — `PUBLIC void yuno_shutdown(void)`

### `msg_ievent.h` — 15 functions

**Source:** `kernel/c/root-linux/src/msg_ievent.h`

1. [**`iev_create`**](runtime/msg_ievent.md#iev_create) — `PUBLIC json_t *iev_create( hgobj gobj, gobj_event_t event, json_t *kw )`

2. [**`iev_create2`**](runtime/msg_ievent.md#iev_create2) — `PUBLIC json_t *iev_create2( hgobj gobj, gobj_event_t event, json_t *kw, json_t *kw_request )`

3. [**`iev_create_to_gbuffer`**](runtime/msg_ievent.md#iev_create_to_gbuffer) — `PUBLIC gbuffer_t *iev_create_to_gbuffer( hgobj gobj, gobj_event_t event, json_t *kw )`

4. [**`iev_create_from_gbuffer`**](runtime/msg_ievent.md#iev_create_from_gbuffer) — `PUBLIC json_t *iev_create_from_gbuffer( hgobj gobj, const char **event, gbuffer_t *gbuf, int verbose )`

5. [**`msg_iev_push_stack`**](runtime/msg_ievent.md#msg_iev_push_stack) — `PUBLIC int msg_iev_push_stack( hgobj gobj, json_t *kw, const char *stack, json_t *jn_data )`

6. [**`msg_iev_get_stack`**](runtime/msg_ievent.md#msg_iev_get_stack) — `PUBLIC json_t *msg_iev_get_stack( hgobj gobj, json_t *kw, const char *stack, BOOL verbose )`

7. [**`msg_iev_pop_stack`**](runtime/msg_ievent.md#msg_iev_pop_stack) — `PUBLIC json_t * msg_iev_pop_stack( hgobj gobj, json_t *kw, const char *stack )`

8. [**`msg_iev_set_back_metadata`**](runtime/msg_ievent.md#msg_iev_set_back_metadata) — `PUBLIC json_t *msg_iev_set_back_metadata( hgobj gobj, json_t *kw_request, json_t *kw_response, BOOL reverse_dst )`

9. [**`msg_iev_build_response`**](runtime/msg_ievent.md#msg_iev_build_response) — `PUBLIC json_t *msg_iev_build_response( hgobj gobj, json_int_t result, json_t *jn_comment, json_t *jn_schema, json_t *jn_data, json_t *kw_request )`

10. [**`msg_iev_build_response_without_reverse_dst`**](runtime/msg_ievent.md#msg_iev_build_response_without_reverse_dst) — `PUBLIC json_t *msg_iev_build_response_without_reverse_dst( hgobj gobj, json_int_t result, json_t *jn_comment, json_t *jn_schema, json_t *jn_data, json_t *kw_request )`

11. [**`msg_iev_clean_metadata`**](runtime/msg_ievent.md#msg_iev_clean_metadata) — `PUBLIC json_t *msg_iev_clean_metadata( json_t *kw )`

12. [**`msg_iev_set_msg_type`**](runtime/msg_ievent.md#msg_iev_set_msg_type) — `PUBLIC int msg_iev_set_msg_type( hgobj gobj, json_t *kw, const char *msg_type )`

13. [**`msg_iev_get_msg_type`**](runtime/msg_ievent.md#msg_iev_get_msg_type) — `PUBLIC const char *msg_iev_get_msg_type( hgobj gobj, json_t *kw )`

14. [**`trace_inter_event`**](runtime/msg_ievent.md#trace_inter_event) — `PUBLIC void trace_inter_event(hgobj gobj, const char *prefix, const char *event, json_t *kw)`

15. [**`trace_inter_event2`**](runtime/msg_ievent.md#trace_inter_event2) — `PUBLIC void trace_inter_event2(hgobj gobj, const char *prefix, const char *event, json_t *kw)`

### `run_command.h` — 3 functions

**Source:** `kernel/c/root-linux/src/run_command.h`

1. [**`run_command`**](runtime/run_command.md#run_command) — `PUBLIC gbuffer_t *run_command(const char *command)`

2. [**`run_process2`**](runtime/run_command.md#run_process2) — `PUBLIC int run_process2( const char *path, char *const argv[] )`

3. [**`pty_sync_spawn`**](runtime/run_command.md#pty_sync_spawn) — `PUBLIC int pty_sync_spawn( const char *command )`

### `ydaemon.h` — 7 functions

**Source:** `kernel/c/root-linux/src/ydaemon.h`

1. [**`get_watcher_pid`**](helpers/daemon_launcher.md#get_watcher_pid) — `PUBLIC int get_watcher_pid(void)`

2. [**`daemon_shutdown`**](helpers/daemon_launcher.md#daemon_shutdown) — `PUBLIC void daemon_shutdown(const char *process_name)`

3. [**`daemon_run`**](helpers/daemon_launcher.md#daemon_run) — `PUBLIC int daemon_run( void (*process)( const char *process_name, const char *work_dir, const char *domain_dir, void (*cleaning_fn)(void) ), const char *process_name, const char *work_dir, const char *domain_dir, void (*cleaning_fn)(void) )`

4. [**`search_process`**](helpers/daemon_launcher.md#search_process) — `PUBLIC int search_process( const char *process_name, void (*cb)(void *self, const char *name, pid_t pid), void *self )`

5. [**`get_relaunch_times`**](helpers/daemon_launcher.md#get_relaunch_times) — `PUBLIC int get_relaunch_times(void)`

6. [**`daemon_set_debug_mode`**](helpers/daemon_launcher.md#daemon_set_debug_mode) — `PUBLIC int daemon_set_debug_mode(BOOL set)`

7. [**`daemon_get_debug_mode`**](helpers/daemon_launcher.md#daemon_get_debug_mode) — `PUBLIC BOOL daemon_get_debug_mode(void)`

### `yunetas_environment.h` — 14 functions

**Source:** `kernel/c/root-linux/src/yunetas_environment.h`

1. [**`register_yuneta_environment`**](runtime/environment.md#register_yuneta_environment) — `PUBLIC int register_yuneta_environment( const char *root_dir, const char *domain_dir, int xpermission, int rpermission )`

2. [**`yuneta_xpermission`**](runtime/environment.md#yuneta_xpermission) — `PUBLIC int yuneta_xpermission(void)`

3. [**`yuneta_rpermission`**](runtime/environment.md#yuneta_rpermission) — `PUBLIC int yuneta_rpermission(void)`

4. [**`yuneta_root_dir`**](runtime/environment.md#yuneta_root_dir) — `PUBLIC const char *yuneta_root_dir(void)`

5. [**`yuneta_domain_dir`**](runtime/environment.md#yuneta_domain_dir) — `PUBLIC const char *yuneta_domain_dir(void)`

6. [**`yuneta_realm_dir`**](runtime/environment.md#yuneta_realm_dir) — `PUBLIC char *yuneta_realm_dir( char *bf, int bfsize, const char *subdomain, BOOL create )`

7. [**`yuneta_realm_file`**](runtime/environment.md#yuneta_realm_file) — `PUBLIC char *yuneta_realm_file( char *bf, int bfsize, const char *subdomain, const char *filename, BOOL create )`

8. [**`yuneta_log_dir`**](runtime/environment.md#yuneta_log_dir) — `PUBLIC char *yuneta_log_dir( char *bf, int bfsize, BOOL create )`

9. [**`yuneta_log_file`**](runtime/environment.md#yuneta_log_file) — `PUBLIC char *yuneta_log_file( char *bf, int bfsize, const char *filename, BOOL create )`

10. [**`yuneta_bin_dir`**](runtime/environment.md#yuneta_bin_dir) — `PUBLIC char *yuneta_bin_dir( char *bf, int bfsize, BOOL create )`

11. [**`yuneta_bin_file`**](runtime/environment.md#yuneta_bin_file) — `PUBLIC char *yuneta_bin_file( char *bf, int bfsize, const char *filename, BOOL create )`

12. [**`yuneta_store_dir`**](runtime/environment.md#yuneta_store_dir) — `PUBLIC char *yuneta_store_dir( char *bf, int bfsize, const char *dir, const char *subdir, BOOL create )`

13. [**`yuneta_store_file`**](runtime/environment.md#yuneta_store_file) — `PUBLIC char *yuneta_store_file( char *bf, int bfsize, const char *dir, const char *subdir, const char *filename, BOOL create )`

14. [**`yuneta_realm_store_dir`**](runtime/environment.md#yuneta_realm_store_dir) — `PUBLIC char *yuneta_realm_store_dir( char *bf, int bfsize, const char *service, const char *owner, const char *realm_id, const char *tenant, const char *dir, BOOL create )`

### `yunetas_register.h` — 1 functions

**Source:** `kernel/c/root-linux/src/yunetas_register.h`

1. [**`yunetas_register_c_core`**](runtime/entry_point.md#yunetas_register_c_core) — `PUBLIC int yunetas_register_c_core(void)`

**Total: 119 functions**

(alphabetical-index)=
## Alphabetical Index

All **957 functions** sorted alphabetically with their source header.

| Function | Header | Module |
|----------|--------|--------|
| [**`_log_bf`**](logging/log.md#_log_bf) | `glogger.h` | gobj-c (Core Framework) |
| [**`_treedb_create_topic_cols_desc`**](timeranger2/treedb.md#_treedb_create_topic_cols_desc) | `tr_treedb.h` | timeranger2 (Time-Series DB) |
| [**`add_allowed_ip`**](runtime/yuno.md#add_allowed_ip) | `c_yuno.h` | root-linux (Runtime GClasses) |
| [**`add_denied_ip`**](runtime/yuno.md#add_denied_ip) | `c_yuno.h` | root-linux (Runtime GClasses) |
| [**`add_jtree_path`**](timeranger2/treedb.md#add_jtree_path) | `tr_treedb.h` | timeranger2 (Time-Series DB) |
| [**`all_numbers`**](helpers/string_helper.md#all_numbers) | `helpers.h` | gobj-c (Core Framework) |
| [**`anyfile2json`**](helpers/json_helper.md#anyfile2json) | `helpers.h` | gobj-c (Core Framework) |
| [**`anystring2json`**](helpers/json_helper.md#anystring2json) | `helpers.h` | gobj-c (Core Framework) |
| [**`authentication_parser`**](gobj/authz.md#api-authentication_parser) | `c_authz.h` | root-linux (Runtime GClasses) |
| [**`authz_checker`**](gobj/authz.md#authz_checker) | `c_authz.h` | root-linux (Runtime GClasses) |
| [**`authz_get_level_desc`**](gobj/authz.md#authz_get_level_desc) | `gobj.h` | gobj-c (Core Framework) |
| [**`authzs_list`**](gobj/authz.md#authzs_list) | `gobj.h` | gobj-c (Core Framework) |
| [**`bin2hex`**](helpers/string_helper.md#bin2hex) | `helpers.h` | gobj-c (Core Framework) |
| [**`bits2gbuffer`**](helpers/json_helper.md#bits2gbuffer) | `helpers.h` | gobj-c (Core Framework) |
| [**`bits2jn_strlist`**](helpers/json_helper.md#bits2jn_strlist) | `helpers.h` | gobj-c (Core Framework) |
| [**`build_command_response`**](parsers/command_parser.md#build_command_response) | `command_parser.h` | gobj-c (Core Framework) |
| [**`build_msg2db_index_path`**](timeranger2/tr_msg2db.md#build_msg2db_index_path) | `tr_msg2db.h` | timeranger2 (Time-Series DB) |
| [**`build_path`**](helpers/string_helper.md#build_path) | `helpers.h` | gobj-c (Core Framework) |
| [**`build_stats`**](parsers/stats_parser.md#build_stats) | `stats_parser.h` | gobj-c (Core Framework) |
| [**`build_stats_response`**](parsers/stats_parser.md#build_stats_response) | `stats_parser.h` | gobj-c (Core Framework) |
| [**`capitalize`**](helpers/string_helper.md#capitalize) | `helpers.h` | gobj-c (Core Framework) |
| [**`capture_log_write`**](testing/testing.md#capture_log_write) | `testing.h` | gobj-c (Core Framework) |
| [**`change_char`**](helpers/string_helper.md#change_char) | `helpers.h` | gobj-c (Core Framework) |
| [**`check_open_fds`**](helpers/misc.md#check_open_fds) | `helpers.h` | gobj-c (Core Framework) |
| [**`clear_timeout`**](runtime/timer.md#clear_timeout) | `c_timer.h` | root-linux (Runtime GClasses) |
| [**`clear_timeout0`**](runtime/timer.md#clear_timeout0) | `c_timer0.h` | root-linux (Runtime GClasses) |
| [**`cmp_two_simple_json`**](helpers/json_helper.md#cmp_two_simple_json) | `helpers.h` | gobj-c (Core Framework) |
| [**`comm_prot_free`**](helpers/common_protocol.md#comm_prot_free) | `helpers.h` | gobj-c (Core Framework) |
| [**`comm_prot_get_gclass`**](helpers/common_protocol.md#comm_prot_get_gclass) | `helpers.h` | gobj-c (Core Framework) |
| [**`comm_prot_register`**](helpers/common_protocol.md#comm_prot_register) | `helpers.h` | gobj-c (Core Framework) |
| [**`command_get_cmd_desc`**](parsers/command_parser.md#command_get_cmd_desc) | `command_parser.h` | gobj-c (Core Framework) |
| [**`command_parser`**](parsers/command_parser.md#command_parser) | `command_parser.h` | gobj-c (Core Framework) |
| [**`config_gbuffer2json`**](helpers/gbuffer.md#config_gbuffer2json) | `gbuffer.h` | gobj-c (Core Framework) |
| [**`copyfile`**](helpers/file_system.md#copyfile) | `helpers.h` | gobj-c (Core Framework) |
| [**`count_char`**](helpers/string_helper.md#count_char) | `helpers.h` | gobj-c (Core Framework) |
| [**`cpu_usage`**](helpers/misc.md#cpu_usage) | `helpers.h` | gobj-c (Core Framework) |
| [**`cpu_usage_percent`**](helpers/misc.md#cpu_usage_percent) | `helpers.h` | gobj-c (Core Framework) |
| [**`create_json_record`**](helpers/json_helper.md#create_json_record) | `helpers.h` | gobj-c (Core Framework) |
| [**`create_random_uuid`**](helpers/misc.md#create_random_uuid) | `helpers.h` | gobj-c (Core Framework) |
| [**`create_template_record`**](timeranger2/treedb.md#create_template_record) | `tr_treedb.h` | timeranger2 (Time-Series DB) |
| [**`current_snap_tag`**](timeranger2/treedb.md#current_snap_tag) | `tr_treedb.h` | timeranger2 (Time-Series DB) |
| [**`current_timestamp`**](helpers/time_date.md#current_timestamp) | `helpers.h` | gobj-c (Core Framework) |
| [**`daemon_get_debug_mode`**](helpers/daemon_launcher.md#daemon_get_debug_mode) | `ydaemon.h` | root-linux (Runtime GClasses) |
| [**`daemon_run`**](helpers/daemon_launcher.md#daemon_run) | `ydaemon.h` | root-linux (Runtime GClasses) |
| [**`daemon_set_debug_mode`**](helpers/daemon_launcher.md#daemon_set_debug_mode) | `ydaemon.h` | root-linux (Runtime GClasses) |
| [**`daemon_shutdown`**](helpers/daemon_launcher.md#daemon_shutdown) | `ydaemon.h` | root-linux (Runtime GClasses) |
| [**`date_mode_from_type`**](helpers/time_date.md#date_mode_from_type) | `helpers.h` | gobj-c (Core Framework) |
| [**`date_overflows`**](helpers/time_date.md#date_overflows) | `helpers.h` | gobj-c (Core Framework) |
| [**`datestamp`**](helpers/time_date.md#datestamp) | `helpers.h` | gobj-c (Core Framework) |
| [**`db_list_persistent_attrs`**](runtime/dbsimple.md#db_list_persistent_attrs) | `dbsimple.h` | root-linux (Runtime GClasses) |
| [**`db_load_persistent_attrs`**](runtime/dbsimple.md#db_load_persistent_attrs) | `dbsimple.h` | root-linux (Runtime GClasses) |
| [**`db_remove_persistent_attrs`**](runtime/dbsimple.md#db_remove_persistent_attrs) | `dbsimple.h` | root-linux (Runtime GClasses) |
| [**`db_save_persistent_attrs`**](runtime/dbsimple.md#db_save_persistent_attrs) | `dbsimple.h` | root-linux (Runtime GClasses) |
| [**`debug_json`**](helpers/json_helper.md#debug_json) | `helpers.h` | gobj-c (Core Framework) |
| [**`debug_json2`**](helpers/json_helper.md#debug_json2) | `helpers.h` | gobj-c (Core Framework) |
| [**`decode_child_ref`**](timeranger2/treedb.md#decode_child_ref) | `tr_treedb.h` | timeranger2 (Time-Series DB) |
| [**`decode_parent_ref`**](timeranger2/treedb.md#decode_parent_ref) | `tr_treedb.h` | timeranger2 (Time-Series DB) |
| [**`delete_left_blanks`**](helpers/string_helper.md#delete_left_blanks) | `helpers.h` | gobj-c (Core Framework) |
| [**`delete_left_char`**](helpers/string_helper.md#delete_left_char) | `helpers.h` | gobj-c (Core Framework) |
| [**`delete_right_blanks`**](helpers/string_helper.md#delete_right_blanks) | `helpers.h` | gobj-c (Core Framework) |
| [**`delete_right_char`**](helpers/string_helper.md#delete_right_char) | `helpers.h` | gobj-c (Core Framework) |
| [**`dir_array_free`**](helpers/directory_walk.md#dir_array_free) | `helpers.h` | gobj-c (Core Framework) |
| [**`dir_array_sort`**](helpers/directory_walk.md#dir_array_sort) | `helpers.h` | gobj-c (Core Framework) |
| [**`dl_add`**](helpers/dl.md#dl_add) | `dl_list.h` | gobj-c (Core Framework) |
| [**`dl_delete`**](helpers/dl.md#dl_delete) | `dl_list.h` | gobj-c (Core Framework) |
| [**`dl_delete_item`**](helpers/dl.md#dl_delete_item) | `dl_list.h` | gobj-c (Core Framework) |
| [**`dl_find`**](helpers/dl.md#dl_find) | `dl_list.h` | gobj-c (Core Framework) |
| [**`dl_flush`**](helpers/dl.md#dl_flush) | `dl_list.h` | gobj-c (Core Framework) |
| [**`dl_init`**](helpers/dl.md#dl_init) | `dl_list.h` | gobj-c (Core Framework) |
| [**`dl_insert`**](helpers/dl.md#dl_insert) | `dl_list.h` | gobj-c (Core Framework) |
| [**`dl_nfind`**](helpers/dl.md#dl_nfind) | `dl_list.h` | gobj-c (Core Framework) |
| [**`file_exists`**](helpers/file_system.md#file_exists) | `helpers.h` | gobj-c (Core Framework) |
| [**`file_permission`**](helpers/file_system.md#file_permission) | `helpers.h` | gobj-c (Core Framework) |
| [**`file_remove`**](helpers/file_system.md#file_remove) | `helpers.h` | gobj-c (Core Framework) |
| [**`file_size`**](helpers/file_system.md#file_size) | `helpers.h` | gobj-c (Core Framework) |
| [**`filesize`**](helpers/file_system.md#filesize) | `helpers.h` | gobj-c (Core Framework) |
| [**`filesize2`**](helpers/file_system.md#filesize2) | `helpers.h` | gobj-c (Core Framework) |
| [**`find_files_with_suffix_array`**](helpers/directory_walk.md#find_files_with_suffix_array) | `helpers.h` | gobj-c (Core Framework) |
| [**`formatdate`**](helpers/time_date.md#formatdate) | `helpers.h` | gobj-c (Core Framework) |
| [**`free_ram_in_kb`**](helpers/misc.md#free_ram_in_kb) | `helpers.h` | gobj-c (Core Framework) |
| [**`fs_create_watcher_event`**](timeranger2/fs_watcher.md#fs_create_watcher_event) | `fs_watcher.h` | timeranger2 (Time-Series DB) |
| [**`fs_start_watcher_event`**](timeranger2/fs_watcher.md#fs_start_watcher_event) | `fs_watcher.h` | timeranger2 (Time-Series DB) |
| [**`fs_stop_watcher_event`**](timeranger2/fs_watcher.md#fs_stop_watcher_event) | `fs_watcher.h` | timeranger2 (Time-Series DB) |
| [**`gbmem_calloc`**](helpers/memory.md#gbmem_calloc) | `gbmem.h` | gobj-c (Core Framework) |
| [**`gbmem_free`**](helpers/memory.md#gbmem_free) | `gbmem.h` | gobj-c (Core Framework) |
| [**`gbmem_get_allocators`**](helpers/memory.md#gbmem_get_allocators) | `gbmem.h` | gobj-c (Core Framework) |
| [**`gbmem_get_maximum_block`**](helpers/memory.md#gbmem_get_maximum_block) | `gbmem.h` | gobj-c (Core Framework) |
| [**`gbmem_malloc`**](helpers/memory.md#gbmem_malloc) | `gbmem.h` | gobj-c (Core Framework) |
| [**`gbmem_realloc`**](helpers/memory.md#gbmem_realloc) | `gbmem.h` | gobj-c (Core Framework) |
| [**`gbmem_set_allocators`**](helpers/memory.md#gbmem_set_allocators) | `gbmem.h` | gobj-c (Core Framework) |
| [**`gbmem_setup`**](helpers/memory.md#gbmem_setup) | `gbmem.h` | gobj-c (Core Framework) |
| [**`gbmem_shutdown`**](helpers/memory.md#gbmem_shutdown) | `gbmem.h` | gobj-c (Core Framework) |
| [**`gbmem_strdup`**](helpers/memory.md#gbmem_strdup) | `gbmem.h` | gobj-c (Core Framework) |
| [**`gbmem_strndup`**](helpers/memory.md#gbmem_strndup) | `gbmem.h` | gobj-c (Core Framework) |
| [**`gbuf2file`**](helpers/gbuffer.md#gbuf2file) | `gbuffer.h` | gobj-c (Core Framework) |
| [**`gbuf2json`**](helpers/gbuffer.md#gbuf2json) | `gbuffer.h` | gobj-c (Core Framework) |
| [**`gbuffer_append`**](helpers/gbuffer.md#gbuffer_append) | `gbuffer.h` | gobj-c (Core Framework) |
| [**`gbuffer_append_gbuf`**](helpers/gbuffer.md#gbuffer_append_gbuf) | `gbuffer.h` | gobj-c (Core Framework) |
| [**`gbuffer_append_json`**](helpers/gbuffer.md#gbuffer_append_json) | `gbuffer.h` | gobj-c (Core Framework) |
| [**`gbuffer_base64_to_binary`**](helpers/gbuffer.md#gbuffer_base64_to_binary) | `gbuffer.h` | gobj-c (Core Framework) |
| [**`gbuffer_binary_to_base64`**](helpers/gbuffer.md#gbuffer_binary_to_base64) | `gbuffer.h` | gobj-c (Core Framework) |
| [**`gbuffer_create`**](helpers/gbuffer.md#gbuffer_create) | `gbuffer.h` | gobj-c (Core Framework) |
| [**`gbuffer_deserialize`**](helpers/gbuffer.md#gbuffer_deserialize) | `gbuffer.h` | gobj-c (Core Framework) |
| [**`gbuffer_encode_base64`**](helpers/gbuffer.md#gbuffer_encode_base64) | `gbuffer.h` | gobj-c (Core Framework) |
| [**`gbuffer_file2base64`**](helpers/gbuffer.md#gbuffer_file2base64) | `gbuffer.h` | gobj-c (Core Framework) |
| [**`gbuffer_get`**](helpers/gbuffer.md#gbuffer_get) | `gbuffer.h` | gobj-c (Core Framework) |
| [**`gbuffer_getline`**](helpers/gbuffer.md#gbuffer_getline) | `gbuffer.h` | gobj-c (Core Framework) |
| [**`gbuffer_printf`**](helpers/gbuffer.md#gbuffer_printf) | `gbuffer.h` | gobj-c (Core Framework) |
| [**`gbuffer_remove`**](helpers/gbuffer.md#gbuffer_remove) | `gbuffer.h` | gobj-c (Core Framework) |
| [**`gbuffer_serialize`**](helpers/gbuffer.md#gbuffer_serialize) | `gbuffer.h` | gobj-c (Core Framework) |
| [**`gbuffer_set_rd_offset`**](helpers/gbuffer.md#gbuffer_set_rd_offset) | `gbuffer.h` | gobj-c (Core Framework) |
| [**`gbuffer_set_wr`**](helpers/gbuffer.md#gbuffer_set_wr) | `gbuffer.h` | gobj-c (Core Framework) |
| [**`gbuffer_setlabel`**](helpers/gbuffer.md#gbuffer_setlabel) | `gbuffer.h` | gobj-c (Core Framework) |
| [**`gbuffer_vprintf`**](helpers/gbuffer.md#gbuffer_vprintf) | `gbuffer.h` | gobj-c (Core Framework) |
| [**`gclass2json`**](gobj/gclass.md#gclass2json) | `gobj.h` | gobj-c (Core Framework) |
| [**`gclass_add_ev_action`**](gobj/gclass.md#gclass_add_ev_action) | `gobj.h` | gobj-c (Core Framework) |
| [**`gclass_add_event_type`**](gobj/gclass.md#gclass_add_event_type) | `gobj.h` | gobj-c (Core Framework) |
| [**`gclass_add_state`**](gobj/gclass.md#gclass_add_state) | `gobj.h` | gobj-c (Core Framework) |
| [**`gclass_attr_desc`**](gobj/gclass.md#gclass_attr_desc) | `gobj.h` | gobj-c (Core Framework) |
| [**`gclass_authz_desc`**](gobj/gclass.md#gclass_authz_desc) | `gobj.h` | gobj-c (Core Framework) |
| [**`gclass_check_fsm`**](gobj/gclass.md#gclass_check_fsm) | `gobj.h` | gobj-c (Core Framework) |
| [**`gclass_command_desc`**](gobj/gclass.md#gclass_command_desc) | `gobj.h` | gobj-c (Core Framework) |
| [**`gclass_create`**](gobj/gclass.md#gclass_create) | `gobj.h` | gobj-c (Core Framework) |
| [**`gclass_event_type`**](gobj/gclass.md#gclass_event_type) | `gobj.h` | gobj-c (Core Framework) |
| [**`gclass_find_by_name`**](gobj/gclass.md#gclass_find_by_name) | `gobj.h` | gobj-c (Core Framework) |
| [**`gclass_find_public_event`**](gobj/gclass.md#gclass_find_public_event) | `gobj.h` | gobj-c (Core Framework) |
| [**`gclass_gclass_name`**](gobj/gclass.md#gclass_gclass_name) | `gobj.h` | gobj-c (Core Framework) |
| [**`gclass_gclass_register`**](gobj/gclass.md#gclass_gclass_register) | `gobj.h` | gobj-c (Core Framework) |
| [**`gclass_has_attr`**](gobj/gclass.md#gclass_has_attr) | `gobj.h` | gobj-c (Core Framework) |
| [**`gclass_unregister`**](gobj/gclass.md#gclass_unregister) | `gobj.h` | gobj-c (Core Framework) |
| [**`get_attrs_schema`**](gobj/info.md#get_attrs_schema) | `gobj.h` | gobj-c (Core Framework) |
| [**`get_cur_system_memory`**](helpers/memory.md#get_cur_system_memory) | `gbmem.h` | gobj-c (Core Framework) |
| [**`get_days_range`**](helpers/time_date.md#get_days_range) | `helpers.h` | gobj-c (Core Framework) |
| [**`get_hook_list`**](timeranger2/treedb.md#get_hook_list) | `tr_treedb.h` | timeranger2 (Time-Series DB) |
| [**`get_hostname`**](helpers/misc.md#get_hostname) | `helpers.h` | gobj-c (Core Framework) |
| [**`get_hours_range`**](helpers/time_date.md#get_hours_range) | `helpers.h` | gobj-c (Core Framework) |
| [**`get_key_value_parameter`**](helpers/string_helper.md#get_key_value_parameter) | `helpers.h` | gobj-c (Core Framework) |
| [**`get_last_segment`**](helpers/string_helper.md#get_last_segment) | `helpers.h` | gobj-c (Core Framework) |
| [**`get_max_system_memory`**](helpers/memory.md#get_max_system_memory) | `gbmem.h` | gobj-c (Core Framework) |
| [**`get_measure_times`**](yev_loop/yev_loop.md#get_measure_times) | `testing.h` | gobj-c (Core Framework) |
| [**`get_months_range`**](helpers/time_date.md#get_months_range) | `helpers.h` | gobj-c (Core Framework) |
| [**`get_name_from_nn_table`**](helpers/misc.md#get_name_from_nn_table) | `helpers.h` | gobj-c (Core Framework) |
| [**`get_number_from_nn_table`**](helpers/misc.md#get_number_from_nn_table) | `helpers.h` | gobj-c (Core Framework) |
| [**`get_ordered_filename_array`**](helpers/directory_walk.md#get_ordered_filename_array) | `helpers.h` | gobj-c (Core Framework) |
| [**`get_parameter`**](helpers/string_helper.md#get_parameter) | `helpers.h` | gobj-c (Core Framework) |
| [**`get_peername`**](helpers/common_protocol.md#get_peername) | `helpers.h` | gobj-c (Core Framework) |
| [**`get_real_precision`**](helpers/json_helper.md#get_real_precision) | `helpers.h` | gobj-c (Core Framework) |
| [**`get_relaunch_times`**](helpers/daemon_launcher.md#get_relaunch_times) | `ydaemon.h` | root-linux (Runtime GClasses) |
| [**`get_sdata_flag_table`**](gobj/info.md#get_sdata_flag_table) | `gobj.h` | gobj-c (Core Framework) |
| [**`get_sockname`**](helpers/common_protocol.md#get_sockname) | `helpers.h` | gobj-c (Core Framework) |
| [**`get_url_schema`**](helpers/url_parsing.md#get_url_schema) | `helpers.h` | gobj-c (Core Framework) |
| [**`get_watcher_pid`**](helpers/daemon_launcher.md#get_watcher_pid) | `ydaemon.h` | root-linux (Runtime GClasses) |
| [**`get_weeks_range`**](helpers/time_date.md#get_weeks_range) | `helpers.h` | gobj-c (Core Framework) |
| [**`get_years_range`**](helpers/time_date.md#get_years_range) | `helpers.h` | gobj-c (Core Framework) |
| [**`get_yunetas_base`**](helpers/misc.md#get_yunetas_base) | `helpers.h` | gobj-c (Core Framework) |
| [**`ghttp_parser_create`**](helpers/http_parser.md#ghttp_parser_create) | `ghttp_parser.h` | root-linux (Runtime GClasses) |
| [**`ghttp_parser_destroy`**](helpers/http_parser.md#ghttp_parser_destroy) | `ghttp_parser.h` | root-linux (Runtime GClasses) |
| [**`ghttp_parser_finish`**](helpers/http_parser.md#ghttp_parser_finish) | `ghttp_parser.h` | root-linux (Runtime GClasses) |
| [**`ghttp_parser_received`**](helpers/http_parser.md#ghttp_parser_received) | `ghttp_parser.h` | root-linux (Runtime GClasses) |
| [**`glog_end`**](logging/log.md#glog_end) | `glogger.h` | gobj-c (Core Framework) |
| [**`glog_init`**](logging/log.md#glog_init) | `glogger.h` | gobj-c (Core Framework) |
| [**`gmtime2timezone`**](helpers/time_date.md#gmtime2timezone) | `helpers.h` | gobj-c (Core Framework) |
| [**`gobj2json`**](gobj/info.md#gobj2json) | `gobj.h` | gobj-c (Core Framework) |
| [**`gobj_activate_snap`**](gobj/node.md#gobj_activate_snap) | `gobj.h` | gobj-c (Core Framework) |
| [**`gobj_add_global_variable`**](gobj/info.md#gobj_add_global_variable) | `gobj.h` | gobj-c (Core Framework) |
| [**`gobj_add_trace_filter`**](logging/trace.md#gobj_add_trace_filter) | `gobj.h` | gobj-c (Core Framework) |
| [**`gobj_attr_desc`**](gobj/attrs.md#gobj_attr_desc) | `gobj.h` | gobj-c (Core Framework) |
| [**`gobj_attr_type`**](gobj/attrs.md#gobj_attr_type) | `gobj.h` | gobj-c (Core Framework) |
| [**`gobj_audit_commands`**](gobj/info.md#gobj_audit_commands) | `gobj.h` | gobj-c (Core Framework) |
| [**`gobj_authenticate`**](gobj/authz.md#gobj_authenticate) | `gobj.h` | gobj-c (Core Framework) |
| [**`gobj_authz`**](gobj/authz.md#gobj_authz) | `gobj.h` | gobj-c (Core Framework) |
| [**`gobj_authzs`**](gobj/authz.md#gobj_authzs) | `gobj.h` | gobj-c (Core Framework) |
| [**`gobj_bottom_gobj`**](gobj/op.md#gobj_bottom_gobj) | `gobj.h` | gobj-c (Core Framework) |
| [**`gobj_build_authzs_doc`**](gobj/authz.md#gobj_build_authzs_doc) | `gobj.h` | gobj-c (Core Framework) |
| [**`gobj_build_cmds_doc`**](parsers/command_parser.md#gobj_build_cmds_doc) | `command_parser.h` | gobj-c (Core Framework) |
| [**`gobj_change_parent`**](gobj/op.md#gobj_change_parent) | `gobj.h` | gobj-c (Core Framework) |
| [**`gobj_change_state`**](gobj/events_state.md#gobj_change_state) | `gobj.h` | gobj-c (Core Framework) |
| [**`gobj_child_by_index`**](gobj/op.md#gobj_child_by_index) | `gobj.h` | gobj-c (Core Framework) |
| [**`gobj_child_by_name`**](gobj/op.md#gobj_child_by_name) | `gobj.h` | gobj-c (Core Framework) |
| [**`gobj_child_size`**](gobj/op.md#gobj_child_size) | `gobj.h` | gobj-c (Core Framework) |
| [**`gobj_child_size2`**](gobj/op.md#gobj_child_size2) | `gobj.h` | gobj-c (Core Framework) |
| [**`gobj_command`**](gobj/op.md#gobj_command) | `gobj.h` | gobj-c (Core Framework) |
| [**`gobj_command_desc`**](gobj/info.md#gobj_command_desc) | `gobj.h` | gobj-c (Core Framework) |
| [**`gobj_create`**](gobj/creation.md#gobj_create) | `gobj.h` | gobj-c (Core Framework) |
| [**`gobj_create2`**](gobj/creation.md#gobj_create2) | `gobj.h` | gobj-c (Core Framework) |
| [**`gobj_create_default_service`**](gobj/creation.md#gobj_create_default_service) | `gobj.h` | gobj-c (Core Framework) |
| [**`gobj_create_node`**](gobj/node.md#gobj_create_node) | `gobj.h` | gobj-c (Core Framework) |
| [**`gobj_create_pure_child`**](gobj/creation.md#gobj_create_pure_child) | `gobj.h` | gobj-c (Core Framework) |
| [**`gobj_create_resource`**](gobj/resource.md#gobj_create_resource) | `gobj.h` | gobj-c (Core Framework) |
| [**`gobj_create_service`**](gobj/creation.md#gobj_create_service) | `gobj.h` | gobj-c (Core Framework) |
| [**`gobj_create_tree`**](gobj/creation.md#gobj_create_tree) | `gobj.h` | gobj-c (Core Framework) |
| [**`gobj_create_volatil`**](gobj/creation.md#gobj_create_volatil) | `gobj.h` | gobj-c (Core Framework) |
| [**`gobj_create_yuno`**](gobj/creation.md#gobj_create_yuno) | `gobj.h` | gobj-c (Core Framework) |
| [**`gobj_current_state`**](gobj/events_state.md#gobj_current_state) | `gobj.h` | gobj-c (Core Framework) |
| [**`gobj_decr_stat`**](gobj/stats.md#gobj_decr_stat) | `gobj.h` | gobj-c (Core Framework) |
| [**`gobj_default_service`**](gobj/op.md#gobj_default_service) | `gobj.h` | gobj-c (Core Framework) |
| [**`gobj_delete_node`**](gobj/node.md#gobj_delete_node) | `gobj.h` | gobj-c (Core Framework) |
| [**`gobj_delete_resource`**](gobj/resource.md#gobj_delete_resource) | `gobj.h` | gobj-c (Core Framework) |
| [**`gobj_destroy`**](gobj/creation.md#gobj_destroy) | `gobj.h` | gobj-c (Core Framework) |
| [**`gobj_destroy_children`**](gobj/creation.md#gobj_destroy_children) | `gobj.h` | gobj-c (Core Framework) |
| [**`gobj_destroy_named_children`**](gobj/creation.md#gobj_destroy_named_children) | `gobj.h` | gobj-c (Core Framework) |
| [**`gobj_disable`**](gobj/op.md#gobj_disable) | `gobj.h` | gobj-c (Core Framework) |
| [**`gobj_enable`**](gobj/op.md#gobj_enable) | `gobj.h` | gobj-c (Core Framework) |
| [**`gobj_end`**](gobj/startup.md#gobj_end) | `gobj.h` | gobj-c (Core Framework) |
| [**`gobj_event_type`**](gobj/events_state.md#gobj_event_type) | `gobj.h` | gobj-c (Core Framework) |
| [**`gobj_event_type_by_name`**](gobj/events_state.md#gobj_event_type_by_name) | `gobj.h` | gobj-c (Core Framework) |
| [**`gobj_find_child`**](gobj/op.md#gobj_find_child) | `gobj.h` | gobj-c (Core Framework) |
| [**`gobj_find_child_by_tree`**](gobj/op.md#gobj_find_child_by_tree) | `gobj.h` | gobj-c (Core Framework) |
| [**`gobj_find_event_type`**](gobj/events_state.md#gobj_find_event_type) | `gobj.h` | gobj-c (Core Framework) |
| [**`gobj_find_gobj`**](gobj/op.md#gobj_find_gobj) | `gobj.h` | gobj-c (Core Framework) |
| [**`gobj_find_service`**](gobj/op.md#gobj_find_service) | `gobj.h` | gobj-c (Core Framework) |
| [**`gobj_find_service_by_gclass`**](gobj/op.md#gobj_find_service_by_gclass) | `gobj.h` | gobj-c (Core Framework) |
| [**`gobj_find_subscribings`**](gobj/publish.md#gobj_find_subscribings) | `gobj.h` | gobj-c (Core Framework) |
| [**`gobj_find_subscriptions`**](gobj/publish.md#gobj_find_subscriptions) | `gobj.h` | gobj-c (Core Framework) |
| [**`gobj_first_child`**](gobj/op.md#gobj_first_child) | `gobj.h` | gobj-c (Core Framework) |
| [**`gobj_free_iter`**](gobj/op.md#gobj_free_iter) | `gobj.h` | gobj-c (Core Framework) |
| [**`gobj_full_name`**](gobj/info.md#gobj_full_name) | `gobj.h` | gobj-c (Core Framework) |
| [**`gobj_gclass`**](gobj/info.md#gobj_gclass) | `gobj.h` | gobj-c (Core Framework) |
| [**`gobj_gclass_name`**](gobj/info.md#gobj_gclass_name) | `gobj.h` | gobj-c (Core Framework) |
| [**`gobj_get_deep_tracing`**](logging/trace.md#gobj_get_deep_tracing) | `gobj.h` | gobj-c (Core Framework) |
| [**`gobj_get_exit_code`**](gobj/startup.md#gobj_get_exit_code) | `gobj.h` | gobj-c (Core Framework) |
| [**`gobj_get_gclass_trace_level`**](logging/trace.md#gobj_get_gclass_trace_level) | `gobj.h` | gobj-c (Core Framework) |
| [**`gobj_get_gclass_trace_level_list`**](logging/trace.md#gobj_get_gclass_trace_level_list) | `gobj.h` | gobj-c (Core Framework) |
| [**`gobj_get_gclass_trace_no_level`**](logging/trace.md#gobj_get_gclass_trace_no_level) | `gobj.h` | gobj-c (Core Framework) |
| [**`gobj_get_gclass_trace_no_level_list`**](logging/trace.md#gobj_get_gclass_trace_no_level_list) | `gobj.h` | gobj-c (Core Framework) |
| [**`gobj_get_global_authz_table`**](gobj/authz.md#gobj_get_global_authz_table) | `gobj.h` | gobj-c (Core Framework) |
| [**`gobj_get_global_trace_level`**](logging/trace.md#gobj_get_global_trace_level) | `gobj.h` | gobj-c (Core Framework) |
| [**`gobj_get_gobj_trace_level`**](logging/trace.md#gobj_get_gobj_trace_level) | `gobj.h` | gobj-c (Core Framework) |
| [**`gobj_get_gobj_trace_level_tree`**](logging/trace.md#gobj_get_gobj_trace_level_tree) | `gobj.h` | gobj-c (Core Framework) |
| [**`gobj_get_gobj_trace_no_level`**](logging/trace.md#gobj_get_gobj_trace_no_level) | `gobj.h` | gobj-c (Core Framework) |
| [**`gobj_get_gobj_trace_no_level_tree`**](logging/trace.md#gobj_get_gobj_trace_no_level_tree) | `gobj.h` | gobj-c (Core Framework) |
| [**`gobj_get_log_data`**](logging/log.md#gobj_get_log_data) | `glogger.h` | gobj-c (Core Framework) |
| [**`gobj_get_log_priority_name`**](logging/log.md#gobj_get_log_priority_name) | `glogger.h` | gobj-c (Core Framework) |
| [**`gobj_get_node`**](gobj/node.md#gobj_get_node) | `gobj.h` | gobj-c (Core Framework) |
| [**`gobj_get_resource`**](gobj/resource.md#gobj_get_resource) | `gobj.h` | gobj-c (Core Framework) |
| [**`gobj_get_stat`**](gobj/stats.md#gobj_get_stat) | `gobj.h` | gobj-c (Core Framework) |
| [**`gobj_get_trace_filter`**](logging/trace.md#gobj_get_trace_filter) | `gobj.h` | gobj-c (Core Framework) |
| [**`gobj_global_trace_level`**](logging/trace.md#gobj_global_trace_level) | `gobj.h` | gobj-c (Core Framework) |
| [**`gobj_global_trace_level2`**](logging/trace.md#gobj_global_trace_level2) | `gobj.h` | gobj-c (Core Framework) |
| [**`gobj_global_variables`**](gobj/info.md#gobj_global_variables) | `gobj.h` | gobj-c (Core Framework) |
| [**`gobj_has_attr`**](gobj/attrs.md#gobj_has_attr) | `gobj.h` | gobj-c (Core Framework) |
| [**`gobj_has_bottom_attr`**](gobj/attrs.md#gobj_has_bottom_attr) | `gobj.h` | gobj-c (Core Framework) |
| [**`gobj_has_event`**](gobj/events_state.md#gobj_has_event) | `gobj.h` | gobj-c (Core Framework) |
| [**`gobj_has_output_event`**](gobj/events_state.md#gobj_has_output_event) | `gobj.h` | gobj-c (Core Framework) |
| [**`gobj_has_state`**](gobj/events_state.md#gobj_has_state) | `gobj.h` | gobj-c (Core Framework) |
| [**`gobj_in_this_state`**](gobj/events_state.md#gobj_in_this_state) | `gobj.h` | gobj-c (Core Framework) |
| [**`gobj_incr_stat`**](gobj/stats.md#gobj_incr_stat) | `gobj.h` | gobj-c (Core Framework) |
| [**`gobj_info_msg`**](logging/log.md#gobj_info_msg) | `glogger.h` | gobj-c (Core Framework) |
| [**`gobj_is_bottom_gobj`**](gobj/info.md#gobj_is_bottom_gobj) | `gobj.h` | gobj-c (Core Framework) |
| [**`gobj_is_destroying`**](gobj/info.md#gobj_is_destroying) | `gobj.h` | gobj-c (Core Framework) |
| [**`gobj_is_disabled`**](gobj/info.md#gobj_is_disabled) | `gobj.h` | gobj-c (Core Framework) |
| [**`gobj_is_level_not_tracing`**](logging/trace.md#gobj_is_level_not_tracing) | `gobj.h` | gobj-c (Core Framework) |
| [**`gobj_is_level_tracing`**](logging/trace.md#gobj_is_level_tracing) | `gobj.h` | gobj-c (Core Framework) |
| [**`gobj_is_playing`**](gobj/info.md#gobj_is_playing) | `gobj.h` | gobj-c (Core Framework) |
| [**`gobj_is_pure_child`**](gobj/info.md#gobj_is_pure_child) | `gobj.h` | gobj-c (Core Framework) |
| [**`gobj_is_readable_attr`**](gobj/attrs.md#gobj_is_readable_attr) | `gobj.h` | gobj-c (Core Framework) |
| [**`gobj_is_running`**](gobj/info.md#gobj_is_running) | `gobj.h` | gobj-c (Core Framework) |
| [**`gobj_is_service`**](gobj/info.md#gobj_is_service) | `gobj.h` | gobj-c (Core Framework) |
| [**`gobj_is_shutdowning`**](gobj/startup.md#gobj_is_shutdowning) | `gobj.h` | gobj-c (Core Framework) |
| [**`gobj_is_top_service`**](gobj/info.md#gobj_is_top_service) | `gobj.h` | gobj-c (Core Framework) |
| [**`gobj_is_volatil`**](gobj/info.md#gobj_is_volatil) | `gobj.h` | gobj-c (Core Framework) |
| [**`gobj_is_writable_attr`**](gobj/attrs.md#gobj_is_writable_attr) | `gobj.h` | gobj-c (Core Framework) |
| [**`gobj_jn_stats`**](gobj/stats.md#gobj_jn_stats) | `gobj.h` | gobj-c (Core Framework) |
| [**`gobj_kw_get_user_data`**](gobj/info.md#gobj_kw_get_user_data) | `gobj.h` | gobj-c (Core Framework) |
| [**`gobj_last_bottom_gobj`**](gobj/op.md#gobj_last_bottom_gobj) | `gobj.h` | gobj-c (Core Framework) |
| [**`gobj_last_child`**](gobj/op.md#gobj_last_child) | `gobj.h` | gobj-c (Core Framework) |
| [**`gobj_link_nodes`**](gobj/node.md#gobj_link_nodes) | `gobj.h` | gobj-c (Core Framework) |
| [**`gobj_list_instances`**](gobj/node.md#gobj_list_instances) | `gobj.h` | gobj-c (Core Framework) |
| [**`gobj_list_nodes`**](gobj/node.md#gobj_list_nodes) | `gobj.h` | gobj-c (Core Framework) |
| [**`gobj_list_persistent_attrs`**](gobj/attrs.md#gobj_list_persistent_attrs) | `gobj.h` | gobj-c (Core Framework) |
| [**`gobj_list_resource`**](gobj/resource.md#gobj_list_resource) | `gobj.h` | gobj-c (Core Framework) |
| [**`gobj_list_snaps`**](gobj/node.md#gobj_list_snaps) | `gobj.h` | gobj-c (Core Framework) |
| [**`gobj_list_subscribings`**](gobj/publish.md#gobj_list_subscribings) | `gobj.h` | gobj-c (Core Framework) |
| [**`gobj_list_subscriptions`**](gobj/publish.md#gobj_list_subscriptions) | `gobj.h` | gobj-c (Core Framework) |
| [**`gobj_load_persistent_attrs`**](gobj/attrs.md#gobj_load_persistent_attrs) | `gobj.h` | gobj-c (Core Framework) |
| [**`gobj_load_trace_filter`**](logging/trace.md#gobj_load_trace_filter) | `gobj.h` | gobj-c (Core Framework) |
| [**`gobj_local_method`**](gobj/op.md#gobj_local_method) | `gobj.h` | gobj-c (Core Framework) |
| [**`gobj_log_add_handler`**](logging/log.md#gobj_log_add_handler) | `glogger.h` | gobj-c (Core Framework) |
| [**`gobj_log_alert`**](logging/log.md#gobj_log_alert) | `glogger.h` | gobj-c (Core Framework) |
| [**`gobj_log_clear_counters`**](logging/log.md#gobj_log_clear_counters) | `glogger.h` | gobj-c (Core Framework) |
| [**`gobj_log_clear_log_file`**](logging/log.md#gobj_log_clear_log_file) | `glogger.h` | gobj-c (Core Framework) |
| [**`gobj_log_critical`**](logging/log.md#gobj_log_critical) | `glogger.h` | gobj-c (Core Framework) |
| [**`gobj_log_debug`**](logging/log.md#gobj_log_debug) | `glogger.h` | gobj-c (Core Framework) |
| [**`gobj_log_del_handler`**](logging/log.md#gobj_log_del_handler) | `glogger.h` | gobj-c (Core Framework) |
| [**`gobj_log_error`**](logging/log.md#gobj_log_error) | `glogger.h` | gobj-c (Core Framework) |
| [**`gobj_log_exist_handler`**](logging/log.md#gobj_log_exist_handler) | `glogger.h` | gobj-c (Core Framework) |
| [**`gobj_log_info`**](logging/log.md#gobj_log_info) | `glogger.h` | gobj-c (Core Framework) |
| [**`gobj_log_last_message`**](logging/log.md#gobj_log_last_message) | `glogger.h` | gobj-c (Core Framework) |
| [**`gobj_log_list_handlers`**](logging/log.md#gobj_log_list_handlers) | `glogger.h` | gobj-c (Core Framework) |
| [**`gobj_log_register_handler`**](logging/log.md#gobj_log_register_handler) | `glogger.h` | gobj-c (Core Framework) |
| [**`gobj_log_set_global_handler_option`**](logging/log.md#gobj_log_set_global_handler_option) | `glogger.h` | gobj-c (Core Framework) |
| [**`gobj_log_set_last_message`**](logging/log.md#gobj_log_set_last_message) | `glogger.h` | gobj-c (Core Framework) |
| [**`gobj_log_warning`**](logging/log.md#gobj_log_warning) | `glogger.h` | gobj-c (Core Framework) |
| [**`gobj_match_children`**](gobj/op.md#gobj_match_children) | `gobj.h` | gobj-c (Core Framework) |
| [**`gobj_match_children_tree`**](gobj/op.md#gobj_match_children_tree) | `gobj.h` | gobj-c (Core Framework) |
| [**`gobj_match_gobj`**](gobj/op.md#gobj_match_gobj) | `gobj.h` | gobj-c (Core Framework) |
| [**`gobj_name`**](gobj/info.md#gobj_name) | `gobj.h` | gobj-c (Core Framework) |
| [**`gobj_nearest_top_service`**](gobj/info.md#gobj_nearest_top_service) | `gobj.h` | gobj-c (Core Framework) |
| [**`gobj_next_child`**](gobj/op.md#gobj_next_child) | `gobj.h` | gobj-c (Core Framework) |
| [**`gobj_node_children`**](gobj/node.md#gobj_node_children) | `gobj.h` | gobj-c (Core Framework) |
| [**`gobj_node_parents`**](gobj/node.md#gobj_node_parents) | `gobj.h` | gobj-c (Core Framework) |
| [**`gobj_node_tree`**](gobj/node.md#gobj_node_tree) | `gobj.h` | gobj-c (Core Framework) |
| [**`gobj_parent`**](gobj/info.md#gobj_parent) | `gobj.h` | gobj-c (Core Framework) |
| [**`gobj_pause`**](gobj/op.md#gobj_pause) | `gobj.h` | gobj-c (Core Framework) |
| [**`gobj_play`**](gobj/op.md#gobj_play) | `gobj.h` | gobj-c (Core Framework) |
| [**`gobj_prev_child`**](gobj/op.md#gobj_prev_child) | `gobj.h` | gobj-c (Core Framework) |
| [**`gobj_priv_data`**](gobj/info.md#gobj_priv_data) | `gobj.h` | gobj-c (Core Framework) |
| [**`gobj_publish_event`**](gobj/publish.md#gobj_publish_event) | `gobj.h` | gobj-c (Core Framework) |
| [**`gobj_read_attr`**](gobj/attrs.md#gobj_read_attr) | `gobj.h` | gobj-c (Core Framework) |
| [**`gobj_read_attrs`**](gobj/attrs.md#gobj_read_attrs) | `gobj.h` | gobj-c (Core Framework) |
| [**`gobj_read_bool_attr`**](gobj/attrs.md#gobj_read_bool_attr) | `gobj.h` | gobj-c (Core Framework) |
| [**`gobj_read_integer_attr`**](gobj/attrs.md#gobj_read_integer_attr) | `gobj.h` | gobj-c (Core Framework) |
| [**`gobj_read_json_attr`**](gobj/attrs.md#gobj_read_json_attr) | `gobj.h` | gobj-c (Core Framework) |
| [**`gobj_read_pointer_attr`**](gobj/attrs.md#gobj_read_pointer_attr) | `gobj.h` | gobj-c (Core Framework) |
| [**`gobj_read_real_attr`**](gobj/attrs.md#gobj_read_real_attr) | `gobj.h` | gobj-c (Core Framework) |
| [**`gobj_read_str_attr`**](gobj/attrs.md#gobj_read_str_attr) | `gobj.h` | gobj-c (Core Framework) |
| [**`gobj_read_user_data`**](gobj/attrs.md#gobj_read_user_data) | `gobj.h` | gobj-c (Core Framework) |
| [**`gobj_remove_persistent_attrs`**](gobj/attrs.md#gobj_remove_persistent_attrs) | `gobj.h` | gobj-c (Core Framework) |
| [**`gobj_remove_trace_filter`**](logging/trace.md#gobj_remove_trace_filter) | `gobj.h` | gobj-c (Core Framework) |
| [**`gobj_repr_gclass_trace_levels`**](logging/trace.md#gobj_repr_gclass_trace_levels) | `gobj.h` | gobj-c (Core Framework) |
| [**`gobj_repr_global_trace_levels`**](logging/trace.md#gobj_repr_global_trace_levels) | `gobj.h` | gobj-c (Core Framework) |
| [**`gobj_reset_rstats_attrs`**](gobj/attrs.md#gobj_reset_rstats_attrs) | `gobj.h` | gobj-c (Core Framework) |
| [**`gobj_reset_volatil_attrs`**](gobj/attrs.md#gobj_reset_volatil_attrs) | `gobj.h` | gobj-c (Core Framework) |
| [**`gobj_save_persistent_attrs`**](gobj/attrs.md#gobj_save_persistent_attrs) | `gobj.h` | gobj-c (Core Framework) |
| [**`gobj_save_resource`**](gobj/resource.md#gobj_save_resource) | `gobj.h` | gobj-c (Core Framework) |
| [**`gobj_sdata_create`**](gobj/creation.md#gobj_sdata_create) | `gobj.h` | gobj-c (Core Framework) |
| [**`gobj_search_path`**](gobj/op.md#gobj_search_path) | `gobj.h` | gobj-c (Core Framework) |
| [**`gobj_send_event`**](gobj/events_state.md#gobj_send_event) | `gobj.h` | gobj-c (Core Framework) |
| [**`gobj_send_event_to_children`**](gobj/events_state.md#gobj_send_event_to_children) | `gobj.h` | gobj-c (Core Framework) |
| [**`gobj_send_event_to_children_tree`**](gobj/events_state.md#gobj_send_event_to_children_tree) | `gobj.h` | gobj-c (Core Framework) |
| [**`gobj_service_factory`**](gobj/creation.md#gobj_service_factory) | `gobj.h` | gobj-c (Core Framework) |
| [**`gobj_services`**](gobj/op.md#gobj_services) | `gobj.h` | gobj-c (Core Framework) |
| [**`gobj_set_bottom_gobj`**](gobj/op.md#gobj_set_bottom_gobj) | `gobj.h` | gobj-c (Core Framework) |
| [**`gobj_set_deep_tracing`**](logging/trace.md#gobj_set_deep_tracing) | `gobj.h` | gobj-c (Core Framework) |
| [**`gobj_set_exit_code`**](gobj/startup.md#gobj_set_exit_code) | `gobj.h` | gobj-c (Core Framework) |
| [**`gobj_set_gclass_no_trace`**](logging/trace.md#gobj_set_gclass_no_trace) | `gobj.h` | gobj-c (Core Framework) |
| [**`gobj_set_gclass_trace`**](logging/trace.md#gobj_set_gclass_trace) | `gobj.h` | gobj-c (Core Framework) |
| [**`gobj_set_global_no_trace`**](logging/trace.md#gobj_set_global_no_trace) | `gobj.h` | gobj-c (Core Framework) |
| [**`gobj_set_global_no_trace2`**](logging/trace.md#gobj_set_global_no_trace2) | `gobj.h` | gobj-c (Core Framework) |
| [**`gobj_set_global_trace`**](logging/trace.md#gobj_set_global_trace) | `gobj.h` | gobj-c (Core Framework) |
| [**`gobj_set_global_trace2`**](logging/trace.md#gobj_set_global_trace2) | `gobj.h` | gobj-c (Core Framework) |
| [**`gobj_set_gobj_no_trace`**](logging/trace.md#gobj_set_gobj_no_trace) | `gobj.h` | gobj-c (Core Framework) |
| [**`gobj_set_gobj_trace`**](logging/trace.md#gobj_set_gobj_trace) | `gobj.h` | gobj-c (Core Framework) |
| [**`gobj_set_manual_start`**](gobj/info.md#gobj_set_manual_start) | `gobj.h` | gobj-c (Core Framework) |
| [**`gobj_set_shutdown`**](gobj/startup.md#gobj_set_shutdown) | `gobj.h` | gobj-c (Core Framework) |
| [**`gobj_set_stat`**](gobj/stats.md#gobj_set_stat) | `gobj.h` | gobj-c (Core Framework) |
| [**`gobj_set_trace_machine_format`**](logging/trace.md#gobj_set_trace_machine_format) | `gobj.h` | gobj-c (Core Framework) |
| [**`gobj_set_volatil`**](gobj/info.md#gobj_set_volatil) | `gobj.h` | gobj-c (Core Framework) |
| [**`gobj_shoot_snap`**](gobj/node.md#gobj_shoot_snap) | `gobj.h` | gobj-c (Core Framework) |
| [**`gobj_short_name`**](gobj/info.md#gobj_short_name) | `gobj.h` | gobj-c (Core Framework) |
| [**`gobj_start`**](gobj/op.md#gobj_start) | `gobj.h` | gobj-c (Core Framework) |
| [**`gobj_start_children`**](gobj/op.md#gobj_start_children) | `gobj.h` | gobj-c (Core Framework) |
| [**`gobj_start_tree`**](gobj/op.md#gobj_start_tree) | `gobj.h` | gobj-c (Core Framework) |
| [**`gobj_start_up`**](gobj/startup.md#gobj_start_up) | `gobj.h` | gobj-c (Core Framework) |
| [**`gobj_state_find_by_name`**](gobj/events_state.md#gobj_state_find_by_name) | `gobj.h` | gobj-c (Core Framework) |
| [**`gobj_stats`**](gobj/stats.md#gobj_stats) | `gobj.h` | gobj-c (Core Framework) |
| [**`gobj_stop`**](gobj/op.md#gobj_stop) | `gobj.h` | gobj-c (Core Framework) |
| [**`gobj_stop_children`**](gobj/op.md#gobj_stop_children) | `gobj.h` | gobj-c (Core Framework) |
| [**`gobj_stop_tree`**](gobj/op.md#gobj_stop_tree) | `gobj.h` | gobj-c (Core Framework) |
| [**`gobj_subs_desc`**](gobj/publish.md#gobj_subs_desc) | `gobj.h` | gobj-c (Core Framework) |
| [**`gobj_subscribe_event`**](gobj/publish.md#gobj_subscribe_event) | `gobj.h` | gobj-c (Core Framework) |
| [**`gobj_top_services`**](gobj/info.md#gobj_top_services) | `gobj.h` | gobj-c (Core Framework) |
| [**`gobj_topic_desc`**](gobj/node.md#gobj_topic_desc) | `gobj.h` | gobj-c (Core Framework) |
| [**`gobj_topic_hooks`**](gobj/node.md#gobj_topic_hooks) | `gobj.h` | gobj-c (Core Framework) |
| [**`gobj_topic_jtree`**](gobj/node.md#gobj_topic_jtree) | `gobj.h` | gobj-c (Core Framework) |
| [**`gobj_topic_links`**](gobj/node.md#gobj_topic_links) | `gobj.h` | gobj-c (Core Framework) |
| [**`gobj_topic_size`**](gobj/node.md#gobj_topic_size) | `gobj.h` | gobj-c (Core Framework) |
| [**`gobj_trace_dump`**](logging/log.md#gobj_trace_dump) | `glogger.h` | gobj-c (Core Framework) |
| [**`gobj_trace_dump_full_gbuf`**](helpers/gbuffer.md#gobj_trace_dump_full_gbuf) | `gbuffer.h` | gobj-c (Core Framework) |
| [**`gobj_trace_dump_gbuf`**](helpers/gbuffer.md#gobj_trace_dump_gbuf) | `gbuffer.h` | gobj-c (Core Framework) |
| [**`gobj_trace_json`**](logging/log.md#gobj_trace_json) | `glogger.h` | gobj-c (Core Framework) |
| [**`gobj_trace_level`**](logging/trace.md#gobj_trace_level) | `gobj.h` | gobj-c (Core Framework) |
| [**`gobj_trace_level_list`**](logging/trace.md#gobj_trace_level_list) | `gobj.h` | gobj-c (Core Framework) |
| [**`gobj_trace_msg`**](logging/log.md#gobj_trace_msg) | `glogger.h` | gobj-c (Core Framework) |
| [**`gobj_trace_no_level`**](logging/trace.md#gobj_trace_no_level) | `gobj.h` | gobj-c (Core Framework) |
| [**`gobj_treedb_topics`**](gobj/node.md#gobj_treedb_topics) | `gobj.h` | gobj-c (Core Framework) |
| [**`gobj_treedbs`**](gobj/node.md#gobj_treedbs) | `gobj.h` | gobj-c (Core Framework) |
| [**`gobj_typeof_gclass`**](gobj/info.md#gobj_typeof_gclass) | `gobj.h` | gobj-c (Core Framework) |
| [**`gobj_typeof_inherited_gclass`**](gobj/info.md#gobj_typeof_inherited_gclass) | `gobj.h` | gobj-c (Core Framework) |
| [**`gobj_unlink_nodes`**](gobj/node.md#gobj_unlink_nodes) | `gobj.h` | gobj-c (Core Framework) |
| [**`gobj_unsubscribe_event`**](gobj/publish.md#gobj_unsubscribe_event) | `gobj.h` | gobj-c (Core Framework) |
| [**`gobj_unsubscribe_list`**](gobj/publish.md#gobj_unsubscribe_list) | `gobj.h` | gobj-c (Core Framework) |
| [**`gobj_update_node`**](gobj/node.md#gobj_update_node) | `gobj.h` | gobj-c (Core Framework) |
| [**`gobj_user_has_authz`**](gobj/authz.md#gobj_user_has_authz) | `gobj.h` | gobj-c (Core Framework) |
| [**`gobj_view_tree`**](gobj/info.md#gobj_view_tree) | `gobj.h` | gobj-c (Core Framework) |
| [**`gobj_walk_gobj_children`**](gobj/op.md#gobj_walk_gobj_children) | `gobj.h` | gobj-c (Core Framework) |
| [**`gobj_walk_gobj_children_tree`**](gobj/op.md#gobj_walk_gobj_children_tree) | `gobj.h` | gobj-c (Core Framework) |
| [**`gobj_write_attr`**](gobj/attrs.md#gobj_write_attr) | `gobj.h` | gobj-c (Core Framework) |
| [**`gobj_write_attrs`**](gobj/attrs.md#gobj_write_attrs) | `gobj.h` | gobj-c (Core Framework) |
| [**`gobj_write_bool_attr`**](gobj/attrs.md#gobj_write_bool_attr) | `gobj.h` | gobj-c (Core Framework) |
| [**`gobj_write_integer_attr`**](gobj/attrs.md#gobj_write_integer_attr) | `gobj.h` | gobj-c (Core Framework) |
| [**`gobj_write_json_attr`**](gobj/attrs.md#gobj_write_json_attr) | `gobj.h` | gobj-c (Core Framework) |
| [**`gobj_write_new_json_attr`**](gobj/attrs.md#gobj_write_new_json_attr) | `gobj.h` | gobj-c (Core Framework) |
| [**`gobj_write_pointer_attr`**](gobj/attrs.md#gobj_write_pointer_attr) | `gobj.h` | gobj-c (Core Framework) |
| [**`gobj_write_real_attr`**](gobj/attrs.md#gobj_write_real_attr) | `gobj.h` | gobj-c (Core Framework) |
| [**`gobj_write_str_attr`**](gobj/attrs.md#gobj_write_str_attr) | `gobj.h` | gobj-c (Core Framework) |
| [**`gobj_write_strn_attr`**](gobj/attrs.md#gobj_write_strn_attr) | `gobj.h` | gobj-c (Core Framework) |
| [**`gobj_write_user_data`**](gobj/attrs.md#gobj_write_user_data) | `gobj.h` | gobj-c (Core Framework) |
| [**`gobj_yuno`**](gobj/info.md#gobj_yuno) | `gobj.h` | gobj-c (Core Framework) |
| [**`gobj_yuno_id`**](gobj/info.md#gobj_yuno_id) | `gobj.h` | gobj-c (Core Framework) |
| [**`gobj_yuno_name`**](gobj/info.md#gobj_yuno_name) | `gobj.h` | gobj-c (Core Framework) |
| [**`gobj_yuno_node_owner`**](gobj/info.md#gobj_yuno_node_owner) | `gobj.h` | gobj-c (Core Framework) |
| [**`gobj_yuno_realm_env`**](gobj/info.md#gobj_yuno_realm_env) | `gobj.h` | gobj-c (Core Framework) |
| [**`gobj_yuno_realm_id`**](gobj/info.md#gobj_yuno_realm_id) | `gobj.h` | gobj-c (Core Framework) |
| [**`gobj_yuno_realm_name`**](gobj/info.md#gobj_yuno_realm_name) | `gobj.h` | gobj-c (Core Framework) |
| [**`gobj_yuno_realm_owner`**](gobj/info.md#gobj_yuno_realm_owner) | `gobj.h` | gobj-c (Core Framework) |
| [**`gobj_yuno_realm_role`**](gobj/info.md#gobj_yuno_realm_role) | `gobj.h` | gobj-c (Core Framework) |
| [**`gobj_yuno_role`**](gobj/info.md#gobj_yuno_role) | `gobj.h` | gobj-c (Core Framework) |
| [**`gobj_yuno_role_plus_name`**](gobj/info.md#gobj_yuno_role_plus_name) | `gobj.h` | gobj-c (Core Framework) |
| [**`gobj_yuno_tag`**](gobj/info.md#gobj_yuno_tag) | `gobj.h` | gobj-c (Core Framework) |
| [**`helper_doublequote2quote`**](helpers/string_helper.md#helper_doublequote2quote) | `helpers.h` | gobj-c (Core Framework) |
| [**`helper_quote2doublequote`**](helpers/string_helper.md#helper_quote2doublequote) | `helpers.h` | gobj-c (Core Framework) |
| [**`hex2bin`**](helpers/string_helper.md#hex2bin) | `helpers.h` | gobj-c (Core Framework) |
| [**`htonll`**](helpers/time_date.md#htonll) | `helpers.h` | gobj-c (Core Framework) |
| [**`idx_in_list`**](helpers/string_helper.md#idx_in_list) | `helpers.h` | gobj-c (Core Framework) |
| [**`iev_create`**](runtime/msg_ievent.md#iev_create) | `msg_ievent.h` | root-linux (Runtime GClasses) |
| [**`iev_create2`**](runtime/msg_ievent.md#iev_create2) | `msg_ievent.h` | root-linux (Runtime GClasses) |
| [**`iev_create_from_gbuffer`**](runtime/msg_ievent.md#iev_create_from_gbuffer) | `msg_ievent.h` | root-linux (Runtime GClasses) |
| [**`iev_create_to_gbuffer`**](runtime/msg_ievent.md#iev_create_to_gbuffer) | `msg_ievent.h` | root-linux (Runtime GClasses) |
| [**`is_directory`**](helpers/file_system.md#is_directory) | `helpers.h` | gobj-c (Core Framework) |
| [**`is_ip_allowed`**](runtime/yuno.md#is_ip_allowed) | `c_yuno.h` | root-linux (Runtime GClasses) |
| [**`is_ip_denied`**](runtime/yuno.md#is_ip_denied) | `c_yuno.h` | root-linux (Runtime GClasses) |
| [**`is_metadata_key`**](helpers/json_helper.md#is_metadata_key) | `helpers.h` | gobj-c (Core Framework) |
| [**`is_private_key`**](helpers/json_helper.md#is_private_key) | `helpers.h` | gobj-c (Core Framework) |
| [**`is_regular_file`**](helpers/file_system.md#is_regular_file) | `helpers.h` | gobj-c (Core Framework) |
| [**`is_tcp_socket`**](helpers/common_protocol.md#is_tcp_socket) | `helpers.h` | gobj-c (Core Framework) |
| [**`is_udp_socket`**](helpers/common_protocol.md#is_udp_socket) | `helpers.h` | gobj-c (Core Framework) |
| [**`is_yuneta_user`**](helpers/misc.md#is_yuneta_user) | `helpers.h` | gobj-c (Core Framework) |
| [**`istream_clear`**](helpers/istream.md#istream_clear) | `istream.h` | root-linux (Runtime GClasses) |
| [**`istream_consume`**](helpers/istream.md#istream_consume) | `istream.h` | root-linux (Runtime GClasses) |
| [**`istream_create`**](helpers/istream.md#istream_create) | `istream.h` | root-linux (Runtime GClasses) |
| [**`istream_cur_rd_pointer`**](helpers/istream.md#istream_cur_rd_pointer) | `istream.h` | root-linux (Runtime GClasses) |
| [**`istream_destroy`**](helpers/istream.md#istream_destroy) | `istream.h` | root-linux (Runtime GClasses) |
| [**`istream_extract_matched_data`**](helpers/istream.md#istream_extract_matched_data) | `istream.h` | root-linux (Runtime GClasses) |
| [**`istream_get_gbuffer`**](helpers/istream.md#istream_get_gbuffer) | `istream.h` | root-linux (Runtime GClasses) |
| [**`istream_is_completed`**](helpers/istream.md#istream_is_completed) | `istream.h` | root-linux (Runtime GClasses) |
| [**`istream_length`**](helpers/istream.md#istream_length) | `istream.h` | root-linux (Runtime GClasses) |
| [**`istream_new_gbuffer`**](helpers/istream.md#istream_new_gbuffer) | `istream.h` | root-linux (Runtime GClasses) |
| [**`istream_pop_gbuffer`**](helpers/istream.md#istream_pop_gbuffer) | `istream.h` | root-linux (Runtime GClasses) |
| [**`istream_read_until_delimiter`**](helpers/istream.md#istream_read_until_delimiter) | `istream.h` | root-linux (Runtime GClasses) |
| [**`istream_read_until_num_bytes`**](helpers/istream.md#istream_read_until_num_bytes) | `istream.h` | root-linux (Runtime GClasses) |
| [**`istream_reset_rd`**](helpers/istream.md#istream_reset_rd) | `istream.h` | root-linux (Runtime GClasses) |
| [**`istream_reset_wr`**](helpers/istream.md#istream_reset_wr) | `istream.h` | root-linux (Runtime GClasses) |
| [**`jn2bool`**](helpers/json_helper.md#jn2bool) | `helpers.h` | gobj-c (Core Framework) |
| [**`jn2integer`**](helpers/json_helper.md#jn2integer) | `helpers.h` | gobj-c (Core Framework) |
| [**`jn2real`**](helpers/json_helper.md#jn2real) | `helpers.h` | gobj-c (Core Framework) |
| [**`jn2string`**](helpers/json_helper.md#jn2string) | `helpers.h` | gobj-c (Core Framework) |
| [**`json2gbuf`**](helpers/gbuffer.md#json2gbuf) | `gbuffer.h` | gobj-c (Core Framework) |
| [**`json2str`**](helpers/json_helper.md#json2str) | `helpers.h` | gobj-c (Core Framework) |
| [**`json2uglystr`**](helpers/json_helper.md#json2uglystr) | `helpers.h` | gobj-c (Core Framework) |
| [**`json_check_refcounts`**](helpers/json_helper.md#json_check_refcounts) | `helpers.h` | gobj-c (Core Framework) |
| [**`json_config`**](helpers/json_helper.md#json_config) | `helpers.h` | gobj-c (Core Framework) |
| [**`json_config_string2json`**](helpers/json_helper.md#json_config_string2json) | `helpers.h` | gobj-c (Core Framework) |
| [**`json_desc_to_schema`**](helpers/json_helper.md#json_desc_to_schema) | `helpers.h` | gobj-c (Core Framework) |
| [**`json_dict_recursive_update`**](helpers/json_helper.md#json_dict_recursive_update) | `helpers.h` | gobj-c (Core Framework) |
| [**`json_flatten_dict`**](helpers/kwid.md#json_flatten_dict) | `kwid.h` | gobj-c (Core Framework) |
| [**`json_is_identical`**](helpers/json_helper.md#json_is_identical) | `helpers.h` | gobj-c (Core Framework) |
| [**`json_is_range`**](helpers/json_helper.md#json_is_range) | `helpers.h` | gobj-c (Core Framework) |
| [**`json_list_find`**](helpers/json_helper.md#json_list_find) | `helpers.h` | gobj-c (Core Framework) |
| [**`json_list_int`**](helpers/json_helper.md#json_list_int) | `helpers.h` | gobj-c (Core Framework) |
| [**`json_list_int_index`**](helpers/json_helper.md#json_list_int_index) | `helpers.h` | gobj-c (Core Framework) |
| [**`json_list_str_index`**](helpers/json_helper.md#json_list_str_index) | `helpers.h` | gobj-c (Core Framework) |
| [**`json_list_update`**](helpers/json_helper.md#json_list_update) | `helpers.h` | gobj-c (Core Framework) |
| [**`json_listsrange2set`**](helpers/json_helper.md#json_listsrange2set) | `helpers.h` | gobj-c (Core Framework) |
| [**`json_print_refcounts`**](helpers/json_helper.md#json_print_refcounts) | `helpers.h` | gobj-c (Core Framework) |
| [**`json_range_list`**](helpers/json_helper.md#json_range_list) | `helpers.h` | gobj-c (Core Framework) |
| [**`json_replace_var`**](helpers/json_helper.md#json_replace_var) | `helpers.h` | gobj-c (Core Framework) |
| [**`json_replace_var_custom`**](helpers/json_helper.md#json_replace_var_custom) | `helpers.h` | gobj-c (Core Framework) |
| [**`json_size`**](helpers/json_helper.md#json_size) | `helpers.h` | gobj-c (Core Framework) |
| [**`json_str_in_list`**](helpers/json_helper.md#json_str_in_list) | `helpers.h` | gobj-c (Core Framework) |
| [**`json_unflatten_dict`**](helpers/kwid.md#json_unflatten_dict) | `kwid.h` | gobj-c (Core Framework) |
| [**`jwks_create`**](libjwt.md#jwks_create) | `jwt.h` | libjwt (JWT Authentication) |
| [**`jwks_create_fromfile`**](libjwt.md#jwks_create_fromfile) | `jwt.h` | libjwt (JWT Authentication) |
| [**`jwks_create_fromfp`**](libjwt.md#jwks_create_fromfp) | `jwt.h` | libjwt (JWT Authentication) |
| [**`jwks_create_fromurl`**](libjwt.md#jwks_create_fromurl) | `jwt.h` | libjwt (JWT Authentication) |
| [**`jwks_create_strn`**](libjwt.md#jwks_create_strn) | `jwt.h` | libjwt (JWT Authentication) |
| [**`jwks_error`**](libjwt.md#jwks_error) | `jwt.h` | libjwt (JWT Authentication) |
| [**`jwks_error_any`**](libjwt.md#jwks_error_any) | `jwt.h` | libjwt (JWT Authentication) |
| [**`jwks_error_clear`**](libjwt.md#jwks_error_clear) | `jwt.h` | libjwt (JWT Authentication) |
| [**`jwks_error_msg`**](libjwt.md#jwks_error_msg) | `jwt.h` | libjwt (JWT Authentication) |
| [**`jwks_find_bykid`**](libjwt.md#jwks_find_bykid) | `jwt.h` | libjwt (JWT Authentication) |
| [**`jwks_free`**](libjwt.md#jwks_free) | `jwt.h` | libjwt (JWT Authentication) |
| [**`jwks_item_alg`**](libjwt.md#jwks_item_alg) | `jwt.h` | libjwt (JWT Authentication) |
| [**`jwks_item_count`**](libjwt.md#jwks_item_count) | `jwt.h` | libjwt (JWT Authentication) |
| [**`jwks_item_curve`**](libjwt.md#jwks_item_curve) | `jwt.h` | libjwt (JWT Authentication) |
| [**`jwks_item_error`**](libjwt.md#jwks_item_error) | `jwt.h` | libjwt (JWT Authentication) |
| [**`jwks_item_error_msg`**](libjwt.md#jwks_item_error_msg) | `jwt.h` | libjwt (JWT Authentication) |
| [**`jwks_item_free`**](libjwt.md#jwks_item_free) | `jwt.h` | libjwt (JWT Authentication) |
| [**`jwks_item_free_all`**](libjwt.md#jwks_item_free_all) | `jwt.h` | libjwt (JWT Authentication) |
| [**`jwks_item_free_bad`**](libjwt.md#jwks_item_free_bad) | `jwt.h` | libjwt (JWT Authentication) |
| [**`jwks_item_get`**](libjwt.md#jwks_item_get) | `jwt.h` | libjwt (JWT Authentication) |
| [**`jwks_item_is_private`**](libjwt.md#jwks_item_is_private) | `jwt.h` | libjwt (JWT Authentication) |
| [**`jwks_item_key_bits`**](libjwt.md#jwks_item_key_bits) | `jwt.h` | libjwt (JWT Authentication) |
| [**`jwks_item_key_oct`**](libjwt.md#jwks_item_key_oct) | `jwt.h` | libjwt (JWT Authentication) |
| [**`jwks_item_key_ops`**](libjwt.md#jwks_item_key_ops) | `jwt.h` | libjwt (JWT Authentication) |
| [**`jwks_item_kid`**](libjwt.md#jwks_item_kid) | `jwt.h` | libjwt (JWT Authentication) |
| [**`jwks_item_kty`**](libjwt.md#jwks_item_kty) | `jwt.h` | libjwt (JWT Authentication) |
| [**`jwks_item_pem`**](libjwt.md#jwks_item_pem) | `jwt.h` | libjwt (JWT Authentication) |
| [**`jwks_item_use`**](libjwt.md#jwks_item_use) | `jwt.h` | libjwt (JWT Authentication) |
| [**`jwks_load`**](libjwt.md#jwks_load) | `jwt.h` | libjwt (JWT Authentication) |
| [**`jwks_load_fromfile`**](libjwt.md#jwks_load_fromfile) | `jwt.h` | libjwt (JWT Authentication) |
| [**`jwks_load_fromfp`**](libjwt.md#jwks_load_fromfp) | `jwt.h` | libjwt (JWT Authentication) |
| [**`jwks_load_fromurl`**](libjwt.md#jwks_load_fromurl) | `jwt.h` | libjwt (JWT Authentication) |
| [**`jwks_load_strn`**](libjwt.md#jwks_load_strn) | `jwt.h` | libjwt (JWT Authentication) |
| [**`jwt_alg_str`**](libjwt.md#jwt_alg_str) | `jwt.h` | libjwt (JWT Authentication) |
| [**`jwt_builder_claim_del`**](libjwt.md#jwt_builder_claim_del) | `jwt.h` | libjwt (JWT Authentication) |
| [**`jwt_builder_claim_get`**](libjwt.md#jwt_builder_claim_get) | `jwt.h` | libjwt (JWT Authentication) |
| [**`jwt_builder_claim_set`**](libjwt.md#jwt_builder_claim_set) | `jwt.h` | libjwt (JWT Authentication) |
| [**`jwt_builder_enable_iat`**](libjwt.md#jwt_builder_enable_iat) | `jwt.h` | libjwt (JWT Authentication) |
| [**`jwt_builder_error`**](libjwt.md#jwt_builder_error) | `jwt.h` | libjwt (JWT Authentication) |
| [**`jwt_builder_error_clear`**](libjwt.md#jwt_builder_error_clear) | `jwt.h` | libjwt (JWT Authentication) |
| [**`jwt_builder_error_msg`**](libjwt.md#jwt_builder_error_msg) | `jwt.h` | libjwt (JWT Authentication) |
| [**`jwt_builder_free`**](libjwt.md#jwt_builder_free) | `jwt.h` | libjwt (JWT Authentication) |
| [**`jwt_builder_generate`**](libjwt.md#jwt_builder_generate) | `jwt.h` | libjwt (JWT Authentication) |
| [**`jwt_builder_getctx`**](libjwt.md#jwt_builder_getctx) | `jwt.h` | libjwt (JWT Authentication) |
| [**`jwt_builder_header_del`**](libjwt.md#jwt_builder_header_del) | `jwt.h` | libjwt (JWT Authentication) |
| [**`jwt_builder_header_get`**](libjwt.md#jwt_builder_header_get) | `jwt.h` | libjwt (JWT Authentication) |
| [**`jwt_builder_header_set`**](libjwt.md#jwt_builder_header_set) | `jwt.h` | libjwt (JWT Authentication) |
| [**`jwt_builder_new`**](libjwt.md#jwt_builder_new) | `jwt.h` | libjwt (JWT Authentication) |
| [**`jwt_builder_setcb`**](libjwt.md#jwt_builder_setcb) | `jwt.h` | libjwt (JWT Authentication) |
| [**`jwt_builder_setkey`**](libjwt.md#jwt_builder_setkey) | `jwt.h` | libjwt (JWT Authentication) |
| [**`jwt_builder_time_offset`**](libjwt.md#jwt_builder_time_offset) | `jwt.h` | libjwt (JWT Authentication) |
| [**`jwt_checker_claim_del`**](libjwt.md#jwt_checker_claim_del) | `jwt.h` | libjwt (JWT Authentication) |
| [**`jwt_checker_claim_get`**](libjwt.md#jwt_checker_claim_get) | `jwt.h` | libjwt (JWT Authentication) |
| [**`jwt_checker_claim_set`**](libjwt.md#jwt_checker_claim_set) | `jwt.h` | libjwt (JWT Authentication) |
| [**`jwt_checker_error`**](libjwt.md#jwt_checker_error) | `jwt.h` | libjwt (JWT Authentication) |
| [**`jwt_checker_error_clear`**](libjwt.md#jwt_checker_error_clear) | `jwt.h` | libjwt (JWT Authentication) |
| [**`jwt_checker_error_msg`**](libjwt.md#jwt_checker_error_msg) | `jwt.h` | libjwt (JWT Authentication) |
| [**`jwt_checker_free`**](libjwt.md#jwt_checker_free) | `jwt.h` | libjwt (JWT Authentication) |
| [**`jwt_checker_getctx`**](libjwt.md#jwt_checker_getctx) | `jwt.h` | libjwt (JWT Authentication) |
| [**`jwt_checker_new`**](libjwt.md#jwt_checker_new) | `jwt.h` | libjwt (JWT Authentication) |
| [**`jwt_checker_setcb`**](libjwt.md#jwt_checker_setcb) | `jwt.h` | libjwt (JWT Authentication) |
| [**`jwt_checker_setkey`**](libjwt.md#jwt_checker_setkey) | `jwt.h` | libjwt (JWT Authentication) |
| [**`jwt_checker_time_leeway`**](libjwt.md#jwt_checker_time_leeway) | `jwt.h` | libjwt (JWT Authentication) |
| [**`jwt_checker_verify`**](libjwt.md#jwt_checker_verify) | `jwt.h` | libjwt (JWT Authentication) |
| [**`jwt_claim_del`**](libjwt.md#jwt_claim_del) | `jwt.h` | libjwt (JWT Authentication) |
| [**`jwt_claim_get`**](libjwt.md#jwt_claim_get) | `jwt.h` | libjwt (JWT Authentication) |
| [**`jwt_claim_set`**](libjwt.md#jwt_claim_set) | `jwt.h` | libjwt (JWT Authentication) |
| [**`jwt_crypto_ops_supports_jwk`**](libjwt.md#jwt_crypto_ops_supports_jwk) | `jwt.h` | libjwt (JWT Authentication) |
| [**`jwt_get_alg`**](libjwt.md#jwt_get_alg) | `jwt.h` | libjwt (JWT Authentication) |
| [**`jwt_get_alloc`**](libjwt.md#jwt_get_alloc) | `jwt.h` | libjwt (JWT Authentication) |
| [**`jwt_get_crypto_ops`**](libjwt.md#jwt_get_crypto_ops) | `jwt.h` | libjwt (JWT Authentication) |
| [**`jwt_get_crypto_ops_t`**](libjwt.md#jwt_get_crypto_ops_t) | `jwt.h` | libjwt (JWT Authentication) |
| [**`jwt_header_del`**](libjwt.md#jwt_header_del) | `jwt.h` | libjwt (JWT Authentication) |
| [**`jwt_header_get`**](libjwt.md#jwt_header_get) | `jwt.h` | libjwt (JWT Authentication) |
| [**`jwt_header_set`**](libjwt.md#jwt_header_set) | `jwt.h` | libjwt (JWT Authentication) |
| [**`jwt_init`**](libjwt.md#jwt_init) | `jwt.h` | libjwt (JWT Authentication) |
| [**`jwt_set_alloc`**](libjwt.md#jwt_set_alloc) | `jwt.h` | libjwt (JWT Authentication) |
| [**`jwt_set_crypto_ops`**](libjwt.md#jwt_set_crypto_ops) | `jwt.h` | libjwt (JWT Authentication) |
| [**`jwt_set_crypto_ops_t`**](libjwt.md#jwt_set_crypto_ops_t) | `jwt.h` | libjwt (JWT Authentication) |
| [**`jwt_str_alg`**](libjwt.md#jwt_str_alg) | `jwt.h` | libjwt (JWT Authentication) |
| [**`kw_add_binary_type`**](helpers/kwid.md#kw_add_binary_type) | `kwid.h` | gobj-c (Core Framework) |
| [**`kw_clone_by_keys`**](helpers/kwid.md#kw_clone_by_keys) | `kwid.h` | gobj-c (Core Framework) |
| [**`kw_clone_by_not_keys`**](helpers/kwid.md#kw_clone_by_not_keys) | `kwid.h` | gobj-c (Core Framework) |
| [**`kw_clone_by_path`**](helpers/kwid.md#kw_clone_by_path) | `kwid.h` | gobj-c (Core Framework) |
| [**`kw_collapse`**](helpers/kwid.md#kw_collapse) | `kwid.h` | gobj-c (Core Framework) |
| [**`kw_collect`**](helpers/kwid.md#kw_collect) | `kwid.h` | gobj-c (Core Framework) |
| [**`kw_decref`**](helpers/kwid.md#kw_decref) | `kwid.h` | gobj-c (Core Framework) |
| [**`kw_delete`**](helpers/kwid.md#kw_delete) | `kwid.h` | gobj-c (Core Framework) |
| [**`kw_delete_metadata_keys`**](helpers/kwid.md#kw_delete_metadata_keys) | `kwid.h` | gobj-c (Core Framework) |
| [**`kw_delete_private_keys`**](helpers/kwid.md#kw_delete_private_keys) | `kwid.h` | gobj-c (Core Framework) |
| [**`kw_delete_subkey`**](helpers/kwid.md#kw_delete_subkey) | `kwid.h` | gobj-c (Core Framework) |
| [**`kw_deserialize`**](helpers/kwid.md#kw_deserialize) | `kwid.h` | gobj-c (Core Framework) |
| [**`kw_duplicate`**](helpers/kwid.md#kw_duplicate) | `kwid.h` | gobj-c (Core Framework) |
| [**`kw_filter_metadata`**](helpers/kwid.md#kw_filter_metadata) | `kwid.h` | gobj-c (Core Framework) |
| [**`kw_filter_private`**](helpers/kwid.md#kw_filter_private) | `kwid.h` | gobj-c (Core Framework) |
| [**`kw_find_json_in_list`**](helpers/kwid.md#kw_find_json_in_list) | `kwid.h` | gobj-c (Core Framework) |
| [**`kw_find_path`**](helpers/kwid.md#kw_find_path) | `kwid.h` | gobj-c (Core Framework) |
| [**`kw_find_str_in_list`**](helpers/kwid.md#kw_find_str_in_list) | `kwid.h` | gobj-c (Core Framework) |
| [**`kw_get_bool`**](helpers/kwid.md#kw_get_bool) | `kwid.h` | gobj-c (Core Framework) |
| [**`kw_get_dict`**](helpers/kwid.md#kw_get_dict) | `kwid.h` | gobj-c (Core Framework) |
| [**`kw_get_dict_value`**](helpers/kwid.md#kw_get_dict_value) | `kwid.h` | gobj-c (Core Framework) |
| [**`kw_get_int`**](helpers/kwid.md#kw_get_int) | `kwid.h` | gobj-c (Core Framework) |
| [**`kw_get_list`**](helpers/kwid.md#kw_get_list) | `kwid.h` | gobj-c (Core Framework) |
| [**`kw_get_list_value`**](helpers/kwid.md#kw_get_list_value) | `kwid.h` | gobj-c (Core Framework) |
| [**`kw_get_real`**](helpers/kwid.md#kw_get_real) | `kwid.h` | gobj-c (Core Framework) |
| [**`kw_get_str`**](helpers/kwid.md#kw_get_str) | `kwid.h` | gobj-c (Core Framework) |
| [**`kw_get_subdict_value`**](helpers/kwid.md#kw_get_subdict_value) | `kwid.h` | gobj-c (Core Framework) |
| [**`kw_has_key`**](helpers/kwid.md#kw_has_key) | `kwid.h` | gobj-c (Core Framework) |
| [**`kw_has_word`**](helpers/kwid.md#kw_has_word) | `kwid.h` | gobj-c (Core Framework) |
| [**`kw_incref`**](helpers/kwid.md#kw_incref) | `kwid.h` | gobj-c (Core Framework) |
| [**`kw_match_simple`**](helpers/kwid.md#kw_match_simple) | `kwid.h` | gobj-c (Core Framework) |
| [**`kw_pop`**](helpers/kwid.md#kw_pop) | `kwid.h` | gobj-c (Core Framework) |
| [**`kw_select`**](helpers/kwid.md#kw_select) | `kwid.h` | gobj-c (Core Framework) |
| [**`kw_serialize`**](helpers/kwid.md#kw_serialize) | `kwid.h` | gobj-c (Core Framework) |
| [**`kw_serialize_to_string`**](helpers/kwid.md#kw_serialize_to_string) | `kwid.h` | gobj-c (Core Framework) |
| [**`kw_set_dict_value`**](helpers/kwid.md#kw_set_dict_value) | `kwid.h` | gobj-c (Core Framework) |
| [**`kw_set_path_delimiter`**](helpers/kwid.md#kw_set_path_delimiter) | `kwid.h` | gobj-c (Core Framework) |
| [**`kw_set_subdict_value`**](helpers/kwid.md#kw_set_subdict_value) | `kwid.h` | gobj-c (Core Framework) |
| [**`kw_size`**](helpers/kwid.md#kw_size) | `kwid.h` | gobj-c (Core Framework) |
| [**`kw_update_except`**](helpers/kwid.md#kw_update_except) | `kwid.h` | gobj-c (Core Framework) |
| [**`kw_walk`**](helpers/kwid.md#kw_walk) | `kwid.h` | gobj-c (Core Framework) |
| [**`kwid_compare_lists`**](helpers/kwid.md#kwid_compare_lists) | `kwid.h` | gobj-c (Core Framework) |
| [**`kwid_compare_records`**](helpers/kwid.md#kwid_compare_records) | `kwid.h` | gobj-c (Core Framework) |
| [**`kwid_get`**](helpers/kwid.md#kwid_get) | `kwid.h` | gobj-c (Core Framework) |
| [**`kwid_get_ids`**](helpers/kwid.md#kwid_get_ids) | `kwid.h` | gobj-c (Core Framework) |
| [**`kwid_match_id`**](helpers/kwid.md#kwid_match_id) | `kwid.h` | gobj-c (Core Framework) |
| [**`kwid_match_nid`**](helpers/kwid.md#kwid_match_nid) | `kwid.h` | gobj-c (Core Framework) |
| [**`kwid_new_dict`**](helpers/kwid.md#kwid_new_dict) | `kwid.h` | gobj-c (Core Framework) |
| [**`kwid_new_list`**](helpers/kwid.md#kwid_new_list) | `kwid.h` | gobj-c (Core Framework) |
| [**`kwjr_get`**](helpers/kwid.md#kwjr_get) | `kwid.h` | gobj-c (Core Framework) |
| [**`launch_daemon`**](helpers/daemon_launcher.md#launch_daemon) | `helpers.h` | gobj-c (Core Framework) |
| [**`left_justify`**](helpers/string_helper.md#left_justify) | `helpers.h` | gobj-c (Core Framework) |
| [**`list_open_files`**](helpers/time_date.md#list_open_files) | `helpers.h` | gobj-c (Core Framework) |
| [**`load_json_from_file`**](helpers/json_helper.md#load_json_from_file) | `helpers.h` | gobj-c (Core Framework) |
| [**`load_persistent_json`**](helpers/json_helper.md#load_persistent_json) | `helpers.h` | gobj-c (Core Framework) |
| [**`lock_file`**](helpers/file_system.md#lock_file) | `helpers.h` | gobj-c (Core Framework) |
| [**`lower`**](helpers/string_helper.md#lower) | `helpers.h` | gobj-c (Core Framework) |
| [**`mbed_api_tls`**](ytls/ytls.md#mbed_api_tls) | `mbedtls.h` | ytls (TLS Abstraction) |
| [**`mkrdir`**](helpers/file_system.md#mkrdir) | `helpers.h` | gobj-c (Core Framework) |
| [**`msg2db_append_message`**](timeranger2/tr_msg2db.md#msg2db_append_message) | `tr_msg2db.h` | timeranger2 (Time-Series DB) |
| [**`msg2db_close_db`**](timeranger2/tr_msg2db.md#msg2db_close_db) | `tr_msg2db.h` | timeranger2 (Time-Series DB) |
| [**`msg2db_get_message`**](timeranger2/tr_msg2db.md#msg2db_get_message) | `tr_msg2db.h` | timeranger2 (Time-Series DB) |
| [**`msg2db_list_messages`**](timeranger2/tr_msg2db.md#msg2db_list_messages) | `tr_msg2db.h` | timeranger2 (Time-Series DB) |
| [**`msg2db_open_db`**](timeranger2/tr_msg2db.md#msg2db_open_db) | `tr_msg2db.h` | timeranger2 (Time-Series DB) |
| [**`msg_iev_build_response`**](runtime/msg_ievent.md#msg_iev_build_response) | `msg_ievent.h` | root-linux (Runtime GClasses) |
| [**`msg_iev_build_response_without_reverse_dst`**](runtime/msg_ievent.md#msg_iev_build_response_without_reverse_dst) | `msg_ievent.h` | root-linux (Runtime GClasses) |
| [**`msg_iev_clean_metadata`**](runtime/msg_ievent.md#msg_iev_clean_metadata) | `msg_ievent.h` | root-linux (Runtime GClasses) |
| [**`msg_iev_get_msg_type`**](runtime/msg_ievent.md#msg_iev_get_msg_type) | `msg_ievent.h` | root-linux (Runtime GClasses) |
| [**`msg_iev_get_stack`**](runtime/msg_ievent.md#msg_iev_get_stack) | `msg_ievent.h` | root-linux (Runtime GClasses) |
| [**`msg_iev_pop_stack`**](runtime/msg_ievent.md#msg_iev_pop_stack) | `msg_ievent.h` | root-linux (Runtime GClasses) |
| [**`msg_iev_push_stack`**](runtime/msg_ievent.md#msg_iev_push_stack) | `msg_ievent.h` | root-linux (Runtime GClasses) |
| [**`msg_iev_set_back_metadata`**](runtime/msg_ievent.md#msg_iev_set_back_metadata) | `msg_ievent.h` | root-linux (Runtime GClasses) |
| [**`msg_iev_set_msg_type`**](runtime/msg_ievent.md#msg_iev_set_msg_type) | `msg_ievent.h` | root-linux (Runtime GClasses) |
| [**`newdir`**](helpers/file_system.md#newdir) | `helpers.h` | gobj-c (Core Framework) |
| [**`newfile`**](helpers/file_system.md#newfile) | `helpers.h` | gobj-c (Core Framework) |
| [**`nice_size`**](helpers/string_helper.md#nice_size) | `helpers.h` | gobj-c (Core Framework) |
| [**`node_collapsed_view`**](timeranger2/treedb.md#node_collapsed_view) | `tr_treedb.h` | timeranger2 (Time-Series DB) |
| [**`node_uuid`**](helpers/misc.md#node_uuid) | `helpers.h` | gobj-c (Core Framework) |
| [**`ntohll`**](helpers/time_date.md#ntohll) | `helpers.h` | gobj-c (Core Framework) |
| [**`open_exclusive`**](helpers/file_system.md#open_exclusive) | `helpers.h` | gobj-c (Core Framework) |
| [**`openssl_api_tls`**](ytls/ytls.md#openssl_api_tls) | `openssl.h` | ytls (TLS Abstraction) |
| [**`parse_date`**](helpers/time_date.md#parse_date) | `helpers.h` | gobj-c (Core Framework) |
| [**`parse_date_basic`**](helpers/time_date.md#parse_date_basic) | `helpers.h` | gobj-c (Core Framework) |
| [**`parse_date_format`**](helpers/time_date.md#parse_date_format) | `helpers.h` | gobj-c (Core Framework) |
| [**`parse_expiry_date`**](helpers/time_date.md#parse_expiry_date) | `helpers.h` | gobj-c (Core Framework) |
| [**`parse_hooks`**](timeranger2/treedb.md#parse_hooks) | `tr_treedb.h` | timeranger2 (Time-Series DB) |
| [**`parse_schema`**](timeranger2/treedb.md#parse_schema) | `tr_treedb.h` | timeranger2 (Time-Series DB) |
| [**`parse_schema_cols`**](timeranger2/treedb.md#parse_schema_cols) | `tr_treedb.h` | timeranger2 (Time-Series DB) |
| [**`parse_url`**](helpers/url_parsing.md#parse_url) | `helpers.h` | gobj-c (Core Framework) |
| [**`path_basename`**](helpers/string_helper.md#path_basename) | `helpers.h` | gobj-c (Core Framework) |
| [**`pop_last_segment`**](helpers/string_helper.md#pop_last_segment) | `helpers.h` | gobj-c (Core Framework) |
| [**`print_backtrace`**](logging/log.md#print_backtrace) | `glogger.h` | gobj-c (Core Framework) |
| [**`print_error`**](logging/log.md#print_error) | `glogger.h` | gobj-c (Core Framework) |
| [**`print_json`**](helpers/json_helper.md#print_json) | `helpers.h` | gobj-c (Core Framework) |
| [**`print_open_fds`**](helpers/misc.md#print_open_fds) | `helpers.h` | gobj-c (Core Framework) |
| [**`print_socket_address`**](helpers/common_protocol.md#print_socket_address) | `helpers.h` | gobj-c (Core Framework) |
| [**`print_track_mem`**](helpers/memory.md#print_track_mem) | `gbmem.h` | gobj-c (Core Framework) |
| [**`pty_sync_spawn`**](runtime/run_command.md#pty_sync_spawn) | `run_command.h` | root-linux (Runtime GClasses) |
| [**`read_process_cmdline`**](helpers/file_system.md#read_process_cmdline) | `helpers.h` | gobj-c (Core Framework) |
| [**`register_c_auth_bff`**](runtime/registration.md#register_c_auth_bff) | `c_auth_bff.h` | root-linux (Runtime GClasses) |
| [**`register_c_authz`**](runtime/registration.md#register_c_authz) | `c_authz.h` | root-linux (Runtime GClasses) |
| [**`register_c_channel`**](runtime/registration.md#register_c_channel) | `c_channel.h` | root-linux (Runtime GClasses) |
| [**`register_c_counter`**](runtime/registration.md#register_c_counter) | `c_counter.h` | root-linux (Runtime GClasses) |
| [**`register_c_fs`**](runtime/registration.md#register_c_fs) | `c_fs.h` | root-linux (Runtime GClasses) |
| [**`register_c_gss_udp_s`**](runtime/registration.md#register_c_gss_udp_s) | `c_gss_udp_s.h` | root-linux (Runtime GClasses) |
| [**`register_c_ievent_cli`**](runtime/registration.md#register_c_ievent_cli) | `c_ievent_cli.h` | root-linux (Runtime GClasses) |
| [**`register_c_ievent_srv`**](runtime/registration.md#register_c_ievent_srv) | `c_ievent_srv.h` | root-linux (Runtime GClasses) |
| [**`register_c_iogate`**](runtime/registration.md#register_c_iogate) | `c_iogate.h` | root-linux (Runtime GClasses) |
| [**`register_c_mqiogate`**](runtime/registration.md#register_c_mqiogate) | `c_mqiogate.h` | root-linux (Runtime GClasses) |
| [**`register_c_node`**](runtime/registration.md#register_c_node) | `c_node.h` | root-linux (Runtime GClasses) |
| [**`register_c_ota`**](runtime/registration.md#register_c_ota) | `c_ota.h` | root-linux (Runtime GClasses) |
| [**`register_c_prot_http_cl`**](runtime/registration.md#register_c_prot_http_cl) | `c_prot_http_cl.h` | root-linux (Runtime GClasses) |
| [**`register_c_prot_http_sr`**](runtime/registration.md#register_c_prot_http_sr) | `c_prot_http_sr.h` | root-linux (Runtime GClasses) |
| [**`register_c_prot_raw`**](runtime/registration.md#register_c_prot_raw) | `c_prot_raw.h` | root-linux (Runtime GClasses) |
| [**`register_c_prot_tcp4h`**](runtime/registration.md#register_c_prot_tcp4h) | `c_prot_tcp4h.h` | root-linux (Runtime GClasses) |
| [**`register_c_pty`**](runtime/registration.md#register_c_pty) | `c_pty.h` | root-linux (Runtime GClasses) |
| [**`register_c_qiogate`**](runtime/registration.md#register_c_qiogate) | `c_qiogate.h` | root-linux (Runtime GClasses) |
| [**`register_c_resource2`**](runtime/registration.md#register_c_resource2) | `c_resource2.h` | root-linux (Runtime GClasses) |
| [**`register_c_task`**](runtime/registration.md#register_c_task) | `c_task.h` | root-linux (Runtime GClasses) |
| [**`register_c_task_authenticate`**](runtime/registration.md#register_c_task_authenticate) | `c_task_authenticate.h` | root-linux (Runtime GClasses) |
| [**`register_c_tcp`**](runtime/registration.md#register_c_tcp) | `c_tcp.h` | root-linux (Runtime GClasses) |
| [**`register_c_tcp_s`**](runtime/registration.md#register_c_tcp_s) | `c_tcp_s.h` | root-linux (Runtime GClasses) |
| [**`register_c_timer`**](runtime/timer.md#register_c_timer) | `c_timer.h` | root-linux (Runtime GClasses) |
| [**`register_c_timer0`**](runtime/timer.md#register_c_timer0) | `c_timer0.h` | root-linux (Runtime GClasses) |
| [**`register_c_tranger`**](runtime/registration.md#register_c_tranger) | `c_tranger.h` | root-linux (Runtime GClasses) |
| [**`register_c_treedb`**](runtime/registration.md#register_c_treedb) | `c_treedb.h` | root-linux (Runtime GClasses) |
| [**`register_c_uart`**](runtime/registration.md#register_c_uart) | `c_uart.h` | root-linux (Runtime GClasses) |
| [**`register_c_udp`**](runtime/registration.md#register_c_udp) | `c_udp.h` | root-linux (Runtime GClasses) |
| [**`register_c_udp_s`**](runtime/registration.md#register_c_udp_s) | `c_udp_s.h` | root-linux (Runtime GClasses) |
| [**`register_c_websocket`**](runtime/registration.md#register_c_websocket) | `c_websocket.h` | root-linux (Runtime GClasses) |
| [**`register_c_yuno`**](runtime/yuno.md#register_c_yuno) | `c_yuno.h` | root-linux (Runtime GClasses) |
| [**`register_yuneta_environment`**](runtime/environment.md#register_yuneta_environment) | `yunetas_environment.h` | root-linux (Runtime GClasses) |
| [**`remove_allowed_ip`**](runtime/yuno.md#remove_allowed_ip) | `c_yuno.h` | root-linux (Runtime GClasses) |
| [**`remove_denied_ip`**](runtime/yuno.md#remove_denied_ip) | `c_yuno.h` | root-linux (Runtime GClasses) |
| [**`replace_cli_vars`**](helpers/string_helper.md#replace_cli_vars) | `helpers.h` | gobj-c (Core Framework) |
| [**`rmrcontentdir`**](helpers/file_system.md#rmrcontentdir) | `helpers.h` | gobj-c (Core Framework) |
| [**`rmrdir`**](helpers/file_system.md#rmrdir) | `helpers.h` | gobj-c (Core Framework) |
| [**`rotatory_close`**](logging/rotatory.md#rotatory_close) | `rotatory.h` | gobj-c (Core Framework) |
| [**`rotatory_end`**](logging/rotatory.md#rotatory_end) | `rotatory.h` | gobj-c (Core Framework) |
| [**`rotatory_flush`**](logging/rotatory.md#rotatory_flush) | `rotatory.h` | gobj-c (Core Framework) |
| [**`rotatory_fwrite`**](logging/rotatory.md#rotatory_fwrite) | `rotatory.h` | gobj-c (Core Framework) |
| [**`rotatory_open`**](logging/rotatory.md#rotatory_open) | `rotatory.h` | gobj-c (Core Framework) |
| [**`rotatory_path`**](logging/rotatory.md#rotatory_path) | `rotatory.h` | gobj-c (Core Framework) |
| [**`rotatory_start_up`**](logging/rotatory.md#rotatory_start_up) | `rotatory.h` | gobj-c (Core Framework) |
| [**`rotatory_subscribe2newfile`**](logging/rotatory.md#rotatory_subscribe2newfile) | `rotatory.h` | gobj-c (Core Framework) |
| [**`rotatory_truncate`**](logging/rotatory.md#rotatory_truncate) | `rotatory.h` | gobj-c (Core Framework) |
| [**`rotatory_write`**](logging/rotatory.md#rotatory_write) | `rotatory.h` | gobj-c (Core Framework) |
| [**`run_command`**](runtime/run_command.md#run_command) | `run_command.h` | root-linux (Runtime GClasses) |
| [**`run_process2`**](runtime/run_command.md#run_process2) | `run_command.h` | root-linux (Runtime GClasses) |
| [**`run_services`**](runtime/entry_point.md#run_services) | `manage_services.h` | root-linux (Runtime GClasses) |
| [**`save_json_to_file`**](helpers/json_helper.md#save_json_to_file) | `helpers.h` | gobj-c (Core Framework) |
| [**`search_command_desc`**](parsers/command_parser.md#search_command_desc) | `command_parser.h` | gobj-c (Core Framework) |
| [**`search_process`**](helpers/daemon_launcher.md#search_process) | `ydaemon.h` | root-linux (Runtime GClasses) |
| [**`set_auto_kill_time`**](runtime/entry_point.md#set_auto_kill_time) | `entry_point.h` | root-linux (Runtime GClasses) |
| [**`set_cloexec`**](helpers/file_system.md#set_cloexec) | `helpers.h` | gobj-c (Core Framework) |
| [**`set_expected_results`**](testing/testing.md#set_expected_results) | `testing.h` | gobj-c (Core Framework) |
| [**`set_measure_times`**](yev_loop/yev_loop.md#set_measure_times) | `testing.h` | gobj-c (Core Framework) |
| [**`set_memory_check_list`**](helpers/memory.md#set_memory_check_list) | `gbmem.h` | gobj-c (Core Framework) |
| [**`set_nonblocking`**](helpers/file_system.md#set_nonblocking) | `helpers.h` | gobj-c (Core Framework) |
| [**`set_real_precision`**](helpers/json_helper.md#set_real_precision) | `helpers.h` | gobj-c (Core Framework) |
| [**`set_show_backtrace_fn`**](logging/log.md#set_show_backtrace_fn) | `glogger.h` | gobj-c (Core Framework) |
| [**`set_tcp_socket_options`**](helpers/common_protocol.md#set_tcp_socket_options) | `helpers.h` | gobj-c (Core Framework) |
| [**`set_timeout`**](runtime/timer.md#set_timeout) | `c_timer.h` | root-linux (Runtime GClasses) |
| [**`set_timeout0`**](runtime/timer.md#set_timeout0) | `c_timer0.h` | root-linux (Runtime GClasses) |
| [**`set_timeout_periodic`**](runtime/timer.md#set_timeout_periodic) | `c_timer.h` | root-linux (Runtime GClasses) |
| [**`set_timeout_periodic0`**](runtime/timer.md#set_timeout_periodic0) | `c_timer0.h` | root-linux (Runtime GClasses) |
| [**`set_trace_with_full_name`**](logging/log.md#set_trace_with_full_name) | `glogger.h` | gobj-c (Core Framework) |
| [**`set_trace_with_short_name`**](logging/log.md#set_trace_with_short_name) | `glogger.h` | gobj-c (Core Framework) |
| [**`set_volatil_values`**](timeranger2/treedb.md#set_volatil_values) | `tr_treedb.h` | timeranger2 (Time-Series DB) |
| [**`set_yuno_must_die`**](runtime/yuno.md#set_yuno_must_die) | `c_yuno.h` | root-linux (Runtime GClasses) |
| [**`show_date`**](helpers/time_date.md#show_date) | `helpers.h` | gobj-c (Core Framework) |
| [**`show_date_relative`**](helpers/time_date.md#show_date_relative) | `helpers.h` | gobj-c (Core Framework) |
| [**`source2base64_for_yunetas`**](helpers/misc.md#source2base64_for_yunetas) | `helpers.h` | gobj-c (Core Framework) |
| [**`split2`**](helpers/string_helper.md#split2) | `helpers.h` | gobj-c (Core Framework) |
| [**`split3`**](helpers/string_helper.md#split3) | `helpers.h` | gobj-c (Core Framework) |
| [**`split_free2`**](helpers/string_helper.md#split_free2) | `helpers.h` | gobj-c (Core Framework) |
| [**`split_free3`**](helpers/string_helper.md#split_free3) | `helpers.h` | gobj-c (Core Framework) |
| [**`start_msectimer`**](helpers/time_date.md#start_msectimer) | `helpers.h` | gobj-c (Core Framework) |
| [**`start_sectimer`**](helpers/time_date.md#start_sectimer) | `helpers.h` | gobj-c (Core Framework) |
| [**`stats_parser`**](parsers/stats_parser.md#stats_parser) | `stats_parser.h` | gobj-c (Core Framework) |
| [**`stdout_fwrite`**](logging/log.md#stdout_fwrite) | `glogger.h` | gobj-c (Core Framework) |
| [**`stdout_write`**](logging/log.md#stdout_write) | `glogger.h` | gobj-c (Core Framework) |
| [**`stop_services`**](runtime/entry_point.md#stop_services) | `manage_services.h` | root-linux (Runtime GClasses) |
| [**`str2gbuf`**](helpers/gbuffer.md#str2gbuf) | `gbuffer.h` | gobj-c (Core Framework) |
| [**`str_concat`**](helpers/string_helper.md#str_concat) | `helpers.h` | gobj-c (Core Framework) |
| [**`str_concat3`**](helpers/string_helper.md#str_concat3) | `helpers.h` | gobj-c (Core Framework) |
| [**`str_concat_free`**](helpers/string_helper.md#str_concat_free) | `helpers.h` | gobj-c (Core Framework) |
| [**`str_in_list`**](helpers/string_helper.md#str_in_list) | `helpers.h` | gobj-c (Core Framework) |
| [**`string2json`**](helpers/json_helper.md#string2json) | `helpers.h` | gobj-c (Core Framework) |
| [**`strings2bits`**](helpers/json_helper.md#strings2bits) | `helpers.h` | gobj-c (Core Framework) |
| [**`strntolower`**](helpers/string_helper.md#strntolower) | `helpers.h` | gobj-c (Core Framework) |
| [**`strntoupper`**](helpers/string_helper.md#strntoupper) | `helpers.h` | gobj-c (Core Framework) |
| [**`subdir_exists`**](helpers/file_system.md#subdir_exists) | `helpers.h` | gobj-c (Core Framework) |
| [**`t2timestamp`**](helpers/time_date.md#t2timestamp) | `helpers.h` | gobj-c (Core Framework) |
| [**`tab`**](logging/trace.md#tab) | `gobj.h` | gobj-c (Core Framework) |
| [**`tdump`**](helpers/backtrace.md#tdump) | `helpers.h` | gobj-c (Core Framework) |
| [**`tdump2json`**](helpers/backtrace.md#tdump2json) | `helpers.h` | gobj-c (Core Framework) |
| [**`test_directory_permission`**](testing/testing.md#test_directory_permission) | `testing.h` | gobj-c (Core Framework) |
| [**`test_file_permission_and_size`**](testing/testing.md#test_file_permission_and_size) | `testing.h` | gobj-c (Core Framework) |
| [**`test_json`**](testing/testing.md#test_json) | `testing.h` | gobj-c (Core Framework) |
| [**`test_json_file`**](testing/testing.md#test_json_file) | `testing.h` | gobj-c (Core Framework) |
| [**`test_list`**](testing/testing.md#test_list) | `testing.h` | gobj-c (Core Framework) |
| [**`test_msectimer`**](helpers/time_date.md#test_msectimer) | `helpers.h` | gobj-c (Core Framework) |
| [**`test_sectimer`**](helpers/time_date.md#test_sectimer) | `helpers.h` | gobj-c (Core Framework) |
| [**`time_in_milliseconds`**](helpers/time_date.md#time_in_milliseconds) | `helpers.h` | gobj-c (Core Framework) |
| [**`time_in_milliseconds_monotonic`**](helpers/time_date.md#time_in_milliseconds_monotonic) | `helpers.h` | gobj-c (Core Framework) |
| [**`time_in_seconds`**](helpers/time_date.md#time_in_seconds) | `helpers.h` | gobj-c (Core Framework) |
| [**`tm2timestamp`**](helpers/time_date.md#tm2timestamp) | `helpers.h` | gobj-c (Core Framework) |
| [**`tm_to_time_t`**](helpers/time_date.md#tm_to_time_t) | `helpers.h` | gobj-c (Core Framework) |
| [**`topic_desc_fkey_names`**](timeranger2/treedb.md#topic_desc_fkey_names) | `tr_treedb.h` | timeranger2 (Time-Series DB) |
| [**`topic_desc_hook_names`**](timeranger2/treedb.md#topic_desc_hook_names) | `tr_treedb.h` | timeranger2 (Time-Series DB) |
| [**`total_ram_in_kb`**](helpers/misc.md#total_ram_in_kb) | `helpers.h` | gobj-c (Core Framework) |
| [**`trace_inter_event`**](runtime/msg_ievent.md#trace_inter_event) | `msg_ievent.h` | root-linux (Runtime GClasses) |
| [**`trace_inter_event2`**](runtime/msg_ievent.md#trace_inter_event2) | `msg_ievent.h` | root-linux (Runtime GClasses) |
| [**`trace_machine`**](logging/trace.md#trace_machine) | `gobj.h` | gobj-c (Core Framework) |
| [**`trace_machine2`**](logging/trace.md#trace_machine2) | `gobj.h` | gobj-c (Core Framework) |
| [**`trace_msg0`**](logging/log.md#trace_msg0) | `glogger.h` | gobj-c (Core Framework) |
| [**`trace_vjson`**](logging/log.md#trace_vjson) | `glogger.h` | gobj-c (Core Framework) |
| [**`tranger2_append_record`**](timeranger2/timeranger2.md#tranger2_append_record) | `timeranger2.h` | timeranger2 (Time-Series DB) |
| [**`tranger2_backup_topic`**](timeranger2/timeranger2.md#tranger2_backup_topic) | `timeranger2.h` | timeranger2 (Time-Series DB) |
| [**`tranger2_close_all_lists`**](timeranger2/timeranger2.md#tranger2_close_all_lists) | `timeranger2.h` | timeranger2 (Time-Series DB) |
| [**`tranger2_close_iterator`**](timeranger2/timeranger2.md#tranger2_close_iterator) | `timeranger2.h` | timeranger2 (Time-Series DB) |
| [**`tranger2_close_list`**](timeranger2/timeranger2.md#tranger2_close_list) | `timeranger2.h` | timeranger2 (Time-Series DB) |
| [**`tranger2_close_rt_disk`**](timeranger2/timeranger2.md#tranger2_close_rt_disk) | `timeranger2.h` | timeranger2 (Time-Series DB) |
| [**`tranger2_close_rt_mem`**](timeranger2/timeranger2.md#tranger2_close_rt_mem) | `timeranger2.h` | timeranger2 (Time-Series DB) |
| [**`tranger2_close_topic`**](timeranger2/timeranger2.md#tranger2_close_topic) | `timeranger2.h` | timeranger2 (Time-Series DB) |
| [**`tranger2_create_topic`**](timeranger2/timeranger2.md#tranger2_create_topic) | `timeranger2.h` | timeranger2 (Time-Series DB) |
| [**`tranger2_delete_record`**](timeranger2/timeranger2.md#tranger2_delete_record) | `timeranger2.h` | timeranger2 (Time-Series DB) |
| [**`tranger2_delete_topic`**](timeranger2/timeranger2.md#tranger2_delete_topic) | `timeranger2.h` | timeranger2 (Time-Series DB) |
| [**`tranger2_dict_topic_desc_cols`**](timeranger2/timeranger2.md#tranger2_dict_topic_desc_cols) | `timeranger2.h` | timeranger2 (Time-Series DB) |
| [**`tranger2_get_iterator_by_id`**](timeranger2/timeranger2.md#tranger2_get_iterator_by_id) | `timeranger2.h` | timeranger2 (Time-Series DB) |
| [**`tranger2_get_rt_disk_by_id`**](timeranger2/timeranger2.md#tranger2_get_rt_disk_by_id) | `timeranger2.h` | timeranger2 (Time-Series DB) |
| [**`tranger2_get_rt_mem_by_id`**](timeranger2/timeranger2.md#tranger2_get_rt_mem_by_id) | `timeranger2.h` | timeranger2 (Time-Series DB) |
| [**`tranger2_iterator_get_page`**](timeranger2/timeranger2.md#tranger2_iterator_get_page) | `timeranger2.h` | timeranger2 (Time-Series DB) |
| [**`tranger2_iterator_size`**](timeranger2/timeranger2.md#tranger2_iterator_size) | `timeranger2.h` | timeranger2 (Time-Series DB) |
| [**`tranger2_list_keys`**](timeranger2/timeranger2.md#tranger2_list_keys) | `timeranger2.h` | timeranger2 (Time-Series DB) |
| [**`tranger2_list_topic_desc_cols`**](timeranger2/timeranger2.md#tranger2_list_topic_desc_cols) | `timeranger2.h` | timeranger2 (Time-Series DB) |
| [**`tranger2_list_topic_names`**](timeranger2/timeranger2.md#tranger2_list_topic_names) | `timeranger2.h` | timeranger2 (Time-Series DB) |
| [**`tranger2_list_topics`**](timeranger2/timeranger2.md#tranger2_list_topics) | `timeranger2.h` | timeranger2 (Time-Series DB) |
| [**`tranger2_open_iterator`**](timeranger2/timeranger2.md#tranger2_open_iterator) | `timeranger2.h` | timeranger2 (Time-Series DB) |
| [**`tranger2_open_list`**](timeranger2/timeranger2.md#tranger2_open_list) | `timeranger2.h` | timeranger2 (Time-Series DB) |
| [**`tranger2_open_rt_disk`**](timeranger2/timeranger2.md#tranger2_open_rt_disk) | `timeranger2.h` | timeranger2 (Time-Series DB) |
| [**`tranger2_open_rt_mem`**](timeranger2/timeranger2.md#tranger2_open_rt_mem) | `timeranger2.h` | timeranger2 (Time-Series DB) |
| [**`tranger2_open_topic`**](timeranger2/timeranger2.md#tranger2_open_topic) | `timeranger2.h` | timeranger2 (Time-Series DB) |
| [**`tranger2_print_md0_record`**](timeranger2/timeranger2.md#tranger2_print_md0_record) | `timeranger2.h` | timeranger2 (Time-Series DB) |
| [**`tranger2_print_md1_record`**](timeranger2/timeranger2.md#tranger2_print_md1_record) | `timeranger2.h` | timeranger2 (Time-Series DB) |
| [**`tranger2_print_md2_record`**](timeranger2/timeranger2.md#tranger2_print_md2_record) | `timeranger2.h` | timeranger2 (Time-Series DB) |
| [**`tranger2_print_record_filename`**](timeranger2/timeranger2.md#tranger2_print_record_filename) | `timeranger2.h` | timeranger2 (Time-Series DB) |
| [**`tranger2_read_record_content`**](timeranger2/timeranger2.md#tranger2_read_record_content) | `timeranger2.h` | timeranger2 (Time-Series DB) |
| [**`tranger2_read_user_flag`**](timeranger2/timeranger2.md#tranger2_read_user_flag) | `timeranger2.h` | timeranger2 (Time-Series DB) |
| [**`tranger2_set_trace_level`**](timeranger2/timeranger2.md#tranger2_set_trace_level) | `timeranger2.h` | timeranger2 (Time-Series DB) |
| [**`tranger2_set_user_flag`**](timeranger2/timeranger2.md#tranger2_set_user_flag) | `timeranger2.h` | timeranger2 (Time-Series DB) |
| [**`tranger2_shutdown`**](timeranger2/timeranger2.md#tranger2_shutdown) | `timeranger2.h` | timeranger2 (Time-Series DB) |
| [**`tranger2_startup`**](timeranger2/timeranger2.md#tranger2_startup) | `timeranger2.h` | timeranger2 (Time-Series DB) |
| [**`tranger2_stop`**](timeranger2/timeranger2.md#tranger2_stop) | `timeranger2.h` | timeranger2 (Time-Series DB) |
| [**`tranger2_str2system_flag`**](timeranger2/timeranger2.md#tranger2_str2system_flag) | `timeranger2.h` | timeranger2 (Time-Series DB) |
| [**`tranger2_topic`**](timeranger2/timeranger2.md#tranger2_topic) | `timeranger2.h` | timeranger2 (Time-Series DB) |
| [**`tranger2_topic_desc`**](timeranger2/timeranger2.md#tranger2_topic_desc) | `timeranger2.h` | timeranger2 (Time-Series DB) |
| [**`tranger2_topic_key_size`**](timeranger2/timeranger2.md#tranger2_topic_key_size) | `timeranger2.h` | timeranger2 (Time-Series DB) |
| [**`tranger2_topic_name`**](timeranger2/timeranger2.md#tranger2_topic_name) | `timeranger2.h` | timeranger2 (Time-Series DB) |
| [**`tranger2_topic_path`**](timeranger2/timeranger2.md#tranger2_topic_path) | `timeranger2.h` | timeranger2 (Time-Series DB) |
| [**`tranger2_topic_size`**](timeranger2/timeranger2.md#tranger2_topic_size) | `timeranger2.h` | timeranger2 (Time-Series DB) |
| [**`tranger2_write_topic_cols`**](timeranger2/timeranger2.md#tranger2_write_topic_cols) | `timeranger2.h` | timeranger2 (Time-Series DB) |
| [**`tranger2_write_topic_var`**](timeranger2/timeranger2.md#tranger2_write_topic_var) | `timeranger2.h` | timeranger2 (Time-Series DB) |
| [**`tranger2_write_user_flag`**](timeranger2/timeranger2.md#tranger2_write_user_flag) | `timeranger2.h` | timeranger2 (Time-Series DB) |
| [**`translate_string`**](helpers/string_helper.md#translate_string) | `helpers.h` | gobj-c (Core Framework) |
| [**`treedb_activate_snap`**](timeranger2/treedb.md#treedb_activate_snap) | `tr_treedb.h` | timeranger2 (Time-Series DB) |
| [**`treedb_autolink`**](timeranger2/treedb.md#treedb_autolink) | `tr_treedb.h` | timeranger2 (Time-Series DB) |
| [**`treedb_clean_node`**](timeranger2/treedb.md#treedb_clean_node) | `tr_treedb.h` | timeranger2 (Time-Series DB) |
| [**`treedb_close_db`**](timeranger2/treedb.md#treedb_close_db) | `tr_treedb.h` | timeranger2 (Time-Series DB) |
| [**`treedb_close_topic`**](timeranger2/treedb.md#treedb_close_topic) | `tr_treedb.h` | timeranger2 (Time-Series DB) |
| [**`treedb_create_node`**](timeranger2/treedb.md#treedb_create_node) | `tr_treedb.h` | timeranger2 (Time-Series DB) |
| [**`treedb_create_topic`**](timeranger2/treedb.md#treedb_create_topic) | `tr_treedb.h` | timeranger2 (Time-Series DB) |
| [**`treedb_delete_instance`**](timeranger2/treedb.md#treedb_delete_instance) | `tr_treedb.h` | timeranger2 (Time-Series DB) |
| [**`treedb_delete_node`**](timeranger2/treedb.md#treedb_delete_node) | `tr_treedb.h` | timeranger2 (Time-Series DB) |
| [**`treedb_delete_topic`**](timeranger2/treedb.md#treedb_delete_topic) | `tr_treedb.h` | timeranger2 (Time-Series DB) |
| [**`treedb_get_id_index`**](timeranger2/treedb.md#treedb_get_id_index) | `tr_treedb.h` | timeranger2 (Time-Series DB) |
| [**`treedb_get_instance`**](timeranger2/treedb.md#treedb_get_instance) | `tr_treedb.h` | timeranger2 (Time-Series DB) |
| [**`treedb_get_node`**](timeranger2/treedb.md#treedb_get_node) | `tr_treedb.h` | timeranger2 (Time-Series DB) |
| [**`treedb_get_topic_hooks`**](timeranger2/treedb.md#treedb_get_topic_hooks) | `tr_treedb.h` | timeranger2 (Time-Series DB) |
| [**`treedb_get_topic_links`**](timeranger2/treedb.md#treedb_get_topic_links) | `tr_treedb.h` | timeranger2 (Time-Series DB) |
| [**`treedb_is_treedbs_topic`**](timeranger2/treedb.md#treedb_is_treedbs_topic) | `tr_treedb.h` | timeranger2 (Time-Series DB) |
| [**`treedb_link_nodes`**](timeranger2/treedb.md#treedb_link_nodes) | `tr_treedb.h` | timeranger2 (Time-Series DB) |
| [**`treedb_list_instances`**](timeranger2/treedb.md#treedb_list_instances) | `tr_treedb.h` | timeranger2 (Time-Series DB) |
| [**`treedb_list_nodes`**](timeranger2/treedb.md#treedb_list_nodes) | `tr_treedb.h` | timeranger2 (Time-Series DB) |
| [**`treedb_list_parents`**](timeranger2/treedb.md#treedb_list_parents) | `tr_treedb.h` | timeranger2 (Time-Series DB) |
| [**`treedb_list_snaps`**](timeranger2/treedb.md#treedb_list_snaps) | `tr_treedb.h` | timeranger2 (Time-Series DB) |
| [**`treedb_list_treedb`**](timeranger2/treedb.md#treedb_list_treedb) | `tr_treedb.h` | timeranger2 (Time-Series DB) |
| [**`treedb_node_children`**](timeranger2/treedb.md#treedb_node_children) | `tr_treedb.h` | timeranger2 (Time-Series DB) |
| [**`treedb_node_jtree`**](timeranger2/treedb.md#treedb_node_jtree) | `tr_treedb.h` | timeranger2 (Time-Series DB) |
| [**`treedb_open_db`**](timeranger2/treedb.md#treedb_open_db) | `tr_treedb.h` | timeranger2 (Time-Series DB) |
| [**`treedb_parent_refs`**](timeranger2/treedb.md#treedb_parent_refs) | `tr_treedb.h` | timeranger2 (Time-Series DB) |
| [**`treedb_save_node`**](timeranger2/treedb.md#treedb_save_node) | `tr_treedb.h` | timeranger2 (Time-Series DB) |
| [**`treedb_set_callback`**](timeranger2/treedb.md#treedb_set_callback) | `tr_treedb.h` | timeranger2 (Time-Series DB) |
| [**`treedb_set_trace`**](timeranger2/treedb.md#treedb_set_trace) | `tr_treedb.h` | timeranger2 (Time-Series DB) |
| [**`treedb_shoot_snap`**](timeranger2/treedb.md#treedb_shoot_snap) | `tr_treedb.h` | timeranger2 (Time-Series DB) |
| [**`treedb_topic_pkey2s`**](timeranger2/treedb.md#treedb_topic_pkey2s) | `tr_treedb.h` | timeranger2 (Time-Series DB) |
| [**`treedb_topic_pkey2s_filter`**](timeranger2/treedb.md#treedb_topic_pkey2s_filter) | `tr_treedb.h` | timeranger2 (Time-Series DB) |
| [**`treedb_topic_size`**](timeranger2/treedb.md#treedb_topic_size) | `tr_treedb.h` | timeranger2 (Time-Series DB) |
| [**`treedb_topics`**](timeranger2/treedb.md#treedb_topics) | `tr_treedb.h` | timeranger2 (Time-Series DB) |
| [**`treedb_unlink_nodes`**](timeranger2/treedb.md#treedb_unlink_nodes) | `tr_treedb.h` | timeranger2 (Time-Series DB) |
| [**`treedb_update_node`**](timeranger2/treedb.md#treedb_update_node) | `tr_treedb.h` | timeranger2 (Time-Series DB) |
| [**`trmsg_active_records`**](timeranger2/tr_msg.md#trmsg_active_records) | `tr_msg.h` | timeranger2 (Time-Series DB) |
| [**`trmsg_add_instance`**](timeranger2/tr_msg.md#trmsg_add_instance) | `tr_msg.h` | timeranger2 (Time-Series DB) |
| [**`trmsg_close_list`**](timeranger2/tr_msg.md#trmsg_close_list) | `tr_msg.h` | timeranger2 (Time-Series DB) |
| [**`trmsg_close_topics`**](timeranger2/tr_msg.md#trmsg_close_topics) | `tr_msg.h` | timeranger2 (Time-Series DB) |
| [**`trmsg_data_tree`**](timeranger2/tr_msg.md#trmsg_data_tree) | `tr_msg.h` | timeranger2 (Time-Series DB) |
| [**`trmsg_foreach_active_messages`**](timeranger2/tr_msg.md#trmsg_foreach_active_messages) | `tr_msg.h` | timeranger2 (Time-Series DB) |
| [**`trmsg_foreach_instances_messages`**](timeranger2/tr_msg.md#trmsg_foreach_instances_messages) | `tr_msg.h` | timeranger2 (Time-Series DB) |
| [**`trmsg_foreach_messages`**](timeranger2/tr_msg.md#trmsg_foreach_messages) | `tr_msg.h` | timeranger2 (Time-Series DB) |
| [**`trmsg_get_active_md`**](timeranger2/tr_msg.md#trmsg_get_active_md) | `tr_msg.h` | timeranger2 (Time-Series DB) |
| [**`trmsg_get_active_message`**](timeranger2/tr_msg.md#trmsg_get_active_message) | `tr_msg.h` | timeranger2 (Time-Series DB) |
| [**`trmsg_get_instances`**](timeranger2/tr_msg.md#trmsg_get_instances) | `tr_msg.h` | timeranger2 (Time-Series DB) |
| [**`trmsg_get_message`**](timeranger2/tr_msg.md#trmsg_get_message) | `tr_msg.h` | timeranger2 (Time-Series DB) |
| [**`trmsg_get_messages`**](timeranger2/tr_msg.md#trmsg_get_messages) | `tr_msg.h` | timeranger2 (Time-Series DB) |
| [**`trmsg_open_list`**](timeranger2/tr_msg.md#trmsg_open_list) | `tr_msg.h` | timeranger2 (Time-Series DB) |
| [**`trmsg_open_topics`**](timeranger2/tr_msg.md#trmsg_open_topics) | `tr_msg.h` | timeranger2 (Time-Series DB) |
| [**`trmsg_record_instances`**](timeranger2/tr_msg.md#trmsg_record_instances) | `tr_msg.h` | timeranger2 (Time-Series DB) |
| [**`trq_answer`**](timeranger2/tr_queue.md#trq_answer) | `tr_queue.h` | timeranger2 (Time-Series DB) |
| [**`trq_append2`**](timeranger2/tr_queue.md#trq_append2) | `tr_queue.h` | timeranger2 (Time-Series DB) |
| [**`trq_check_backup`**](timeranger2/tr_queue.md#trq_check_backup) | `tr_queue.h` | timeranger2 (Time-Series DB) |
| [**`trq_check_pending_rowid`**](timeranger2/tr_queue.md#trq_check_pending_rowid) | `tr_queue.h` | timeranger2 (Time-Series DB) |
| [**`trq_close`**](timeranger2/tr_queue.md#trq_close) | `tr_queue.h` | timeranger2 (Time-Series DB) |
| [**`trq_get_by_rowid`**](timeranger2/tr_queue.md#trq_get_by_rowid) | `tr_queue.h` | timeranger2 (Time-Series DB) |
| [**`trq_get_metadata`**](timeranger2/tr_queue.md#trq_get_metadata) | `tr_queue.h` | timeranger2 (Time-Series DB) |
| [**`trq_load`**](timeranger2/tr_queue.md#trq_load) | `tr_queue.h` | timeranger2 (Time-Series DB) |
| [**`trq_load_all`**](timeranger2/tr_queue.md#trq_load_all) | `tr_queue.h` | timeranger2 (Time-Series DB) |
| [**`trq_load_all_by_time`**](timeranger2/tr_queue.md#trq_load_all_by_time) | `tr_queue.h` | timeranger2 (Time-Series DB) |
| [**`trq_msg_json`**](timeranger2/tr_queue.md#trq_msg_json) | `tr_queue.h` | timeranger2 (Time-Series DB) |
| [**`trq_msg_md`**](timeranger2/tr_queue.md#trq_msg_md) | `tr_queue.h` | timeranger2 (Time-Series DB) |
| [**`trq_open`**](timeranger2/tr_queue.md#trq_open) | `tr_queue.h` | timeranger2 (Time-Series DB) |
| [**`trq_set_hard_flag`**](timeranger2/tr_queue.md#trq_set_hard_flag) | `tr_queue.h` | timeranger2 (Time-Series DB) |
| [**`trq_set_metadata`**](timeranger2/tr_queue.md#trq_set_metadata) | `tr_queue.h` | timeranger2 (Time-Series DB) |
| [**`trq_set_soft_mark`**](timeranger2/tr_queue.md#trq_set_soft_mark) | `tr_queue.h` | timeranger2 (Time-Series DB) |
| [**`trq_unload_msg`**](timeranger2/tr_queue.md#trq_unload_msg) | `tr_queue.h` | timeranger2 (Time-Series DB) |
| [**`udpc_close`**](logging/log_udp_handler.md#udpc_close) | `log_udp_handler.h` | gobj-c (Core Framework) |
| [**`udpc_end`**](logging/log_udp_handler.md#udpc_end) | `log_udp_handler.h` | gobj-c (Core Framework) |
| [**`udpc_fwrite`**](logging/log_udp_handler.md#udpc_fwrite) | `log_udp_handler.h` | gobj-c (Core Framework) |
| [**`udpc_open`**](logging/log_udp_handler.md#udpc_open) | `log_udp_handler.h` | gobj-c (Core Framework) |
| [**`udpc_start_up`**](logging/log_udp_handler.md#udpc_start_up) | `log_udp_handler.h` | gobj-c (Core Framework) |
| [**`udpc_write`**](logging/log_udp_handler.md#udpc_write) | `log_udp_handler.h` | gobj-c (Core Framework) |
| [**`unlock_file`**](helpers/file_system.md#unlock_file) | `helpers.h` | gobj-c (Core Framework) |
| [**`upper`**](helpers/string_helper.md#upper) | `helpers.h` | gobj-c (Core Framework) |
| [**`walk_dir_array`**](helpers/directory_walk.md#walk_dir_array) | `helpers.h` | gobj-c (Core Framework) |
| [**`walk_dir_tree`**](helpers/directory_walk.md#walk_dir_tree) | `helpers.h` | gobj-c (Core Framework) |
| [**`yev_create_accept_event`**](yev_loop/yev_loop.md#yev_create_accept_event) | `yev_loop.h` | yev_loop (Event Loop) |
| [**`yev_create_connect_event`**](yev_loop/yev_loop.md#yev_create_connect_event) | `yev_loop.h` | yev_loop (Event Loop) |
| [**`yev_create_poll_event`**](yev_loop/yev_loop.md#yev_create_poll_event) | `yev_loop.h` | yev_loop (Event Loop) |
| [**`yev_create_read_event`**](yev_loop/yev_loop.md#yev_create_read_event) | `yev_loop.h` | yev_loop (Event Loop) |
| [**`yev_create_recvmsg_event`**](yev_loop/yev_loop.md#yev_create_recvmsg_event) | `yev_loop.h` | yev_loop (Event Loop) |
| [**`yev_create_sendmsg_event`**](yev_loop/yev_loop.md#yev_create_sendmsg_event) | `yev_loop.h` | yev_loop (Event Loop) |
| [**`yev_create_timer_event`**](yev_loop/yev_loop.md#yev_create_timer_event) | `yev_loop.h` | yev_loop (Event Loop) |
| [**`yev_create_write_event`**](yev_loop/yev_loop.md#yev_create_write_event) | `yev_loop.h` | yev_loop (Event Loop) |
| [**`yev_destroy_event`**](yev_loop/yev_loop.md#yev_destroy_event) | `yev_loop.h` | yev_loop (Event Loop) |
| [**`yev_dup2_accept_event`**](yev_loop/yev_loop.md#yev_dup2_accept_event) | `yev_loop.h` | yev_loop (Event Loop) |
| [**`yev_dup_accept_event`**](yev_loop/yev_loop.md#yev_dup_accept_event) | `yev_loop.h` | yev_loop (Event Loop) |
| [**`yev_event_type_name`**](yev_loop/yev_loop.md#yev_event_type_name) | `yev_loop.h` | yev_loop (Event Loop) |
| [**`yev_flag_strings`**](yev_loop/yev_loop.md#yev_flag_strings) | `yev_loop.h` | yev_loop (Event Loop) |
| [**`yev_get_state_name`**](yev_loop/yev_loop.md#yev_get_state_name) | `yev_loop.h` | yev_loop (Event Loop) |
| [**`yev_get_yuno`**](yev_loop/yev_loop.md#yev_get_yuno) | `yev_loop.h` | yev_loop (Event Loop) |
| [**`yev_loop_create`**](yev_loop/yev_loop.md#yev_loop_create) | `yev_loop.h` | yev_loop (Event Loop) |
| [**`yev_loop_destroy`**](yev_loop/yev_loop.md#yev_loop_destroy) | `yev_loop.h` | yev_loop (Event Loop) |
| [**`yev_loop_reset_running`**](yev_loop/yev_loop.md#yev_loop_reset_running) | `yev_loop.h` | yev_loop (Event Loop) |
| [**`yev_loop_run`**](yev_loop/yev_loop.md#yev_loop_run) | `yev_loop.h` | yev_loop (Event Loop) |
| [**`yev_loop_run_once`**](yev_loop/yev_loop.md#yev_loop_run_once) | `yev_loop.h` | yev_loop (Event Loop) |
| [**`yev_loop_stop`**](yev_loop/yev_loop.md#yev_loop_stop) | `yev_loop.h` | yev_loop (Event Loop) |
| [**`yev_protocol_set_protocol_fill_hints_fn`**](yev_loop/yev_loop.md#yev_protocol_set_protocol_fill_hints_fn) | `yev_loop.h` | yev_loop (Event Loop) |
| [**`yev_rearm_connect_event`**](yev_loop/yev_loop.md#yev_rearm_connect_event) | `yev_loop.h` | yev_loop (Event Loop) |
| [**`yev_set_gbuffer`**](yev_loop/yev_loop.md#yev_set_gbuffer) | `yev_loop.h` | yev_loop (Event Loop) |
| [**`yev_start_event`**](yev_loop/yev_loop.md#yev_start_event) | `yev_loop.h` | yev_loop (Event Loop) |
| [**`yev_start_timer_event`**](yev_loop/yev_loop.md#yev_start_timer_event) | `yev_loop.h` | yev_loop (Event Loop) |
| [**`yev_stop_event`**](yev_loop/yev_loop.md#yev_stop_event) | `yev_loop.h` | yev_loop (Event Loop) |
| [**`ytls_cleanup`**](ytls/ytls.md#ytls_cleanup) | `ytls.h` | ytls (TLS Abstraction) |
| [**`ytls_decrypt_data`**](ytls/ytls.md#ytls_decrypt_data) | `ytls.h` | ytls (TLS Abstraction) |
| [**`ytls_do_handshake`**](ytls/ytls.md#ytls_do_handshake) | `ytls.h` | ytls (TLS Abstraction) |
| [**`ytls_encrypt_data`**](ytls/ytls.md#ytls_encrypt_data) | `ytls.h` | ytls (TLS Abstraction) |
| [**`ytls_flush`**](ytls/ytls.md#ytls_flush) | `ytls.h` | ytls (TLS Abstraction) |
| [**`ytls_free_secure_filter`**](ytls/ytls.md#ytls_free_secure_filter) | `ytls.h` | ytls (TLS Abstraction) |
| [**`ytls_get_cert_info`**](ytls/ytls.md#ytls_get_cert_info) | `ytls.h` | ytls (TLS Abstraction) |
| [**`ytls_get_last_error`**](ytls/ytls.md#ytls_get_last_error) | `ytls.h` | ytls (TLS Abstraction) |
| [**`ytls_init`**](ytls/ytls.md#ytls_init) | `ytls.h` | ytls (TLS Abstraction) |
| [**`ytls_new_secure_filter`**](ytls/ytls.md#ytls_new_secure_filter) | `ytls.h` | ytls (TLS Abstraction) |
| [**`ytls_reload_certificates`**](ytls/ytls.md#ytls_reload_certificates) | `ytls.h` | ytls (TLS Abstraction) |
| [**`ytls_set_trace`**](ytls/ytls.md#ytls_set_trace) | `ytls.h` | ytls (TLS Abstraction) |
| [**`ytls_shutdown`**](ytls/ytls.md#ytls_shutdown) | `ytls.h` | ytls (TLS Abstraction) |
| [**`ytls_version`**](ytls/ytls.md#ytls_version) | `ytls.h` | ytls (TLS Abstraction) |
| [**`yuneta_bin_dir`**](runtime/environment.md#yuneta_bin_dir) | `yunetas_environment.h` | root-linux (Runtime GClasses) |
| [**`yuneta_bin_file`**](runtime/environment.md#yuneta_bin_file) | `yunetas_environment.h` | root-linux (Runtime GClasses) |
| [**`yuneta_domain_dir`**](runtime/environment.md#yuneta_domain_dir) | `yunetas_environment.h` | root-linux (Runtime GClasses) |
| [**`yuneta_entry_point`**](runtime/entry_point.md#yuneta_entry_point) | `entry_point.h` | root-linux (Runtime GClasses) |
| [**`yuneta_getgrnam`**](helpers/misc.md#yuneta_getgrnam) | `helpers.h` | gobj-c (Core Framework) |
| [**`yuneta_getgrouplist`**](helpers/misc.md#yuneta_getgrouplist) | `helpers.h` | gobj-c (Core Framework) |
| [**`yuneta_getpwnam`**](helpers/misc.md#yuneta_getpwnam) | `helpers.h` | gobj-c (Core Framework) |
| [**`yuneta_getpwuid`**](helpers/misc.md#yuneta_getpwuid) | `helpers.h` | gobj-c (Core Framework) |
| [**`yuneta_json_config`**](runtime/entry_point.md#yuneta_json_config) | `entry_point.h` | root-linux (Runtime GClasses) |
| [**`yuneta_log_dir`**](runtime/environment.md#yuneta_log_dir) | `yunetas_environment.h` | root-linux (Runtime GClasses) |
| [**`yuneta_log_file`**](runtime/environment.md#yuneta_log_file) | `yunetas_environment.h` | root-linux (Runtime GClasses) |
| [**`yuneta_realm_dir`**](runtime/environment.md#yuneta_realm_dir) | `yunetas_environment.h` | root-linux (Runtime GClasses) |
| [**`yuneta_realm_file`**](runtime/environment.md#yuneta_realm_file) | `yunetas_environment.h` | root-linux (Runtime GClasses) |
| [**`yuneta_realm_store_dir`**](runtime/environment.md#yuneta_realm_store_dir) | `yunetas_environment.h` | root-linux (Runtime GClasses) |
| [**`yuneta_root_dir`**](runtime/environment.md#yuneta_root_dir) | `yunetas_environment.h` | root-linux (Runtime GClasses) |
| [**`yuneta_rpermission`**](runtime/environment.md#yuneta_rpermission) | `yunetas_environment.h` | root-linux (Runtime GClasses) |
| [**`yuneta_setup`**](runtime/entry_point.md#yuneta_setup) | `entry_point.h` | root-linux (Runtime GClasses) |
| [**`yuneta_store_dir`**](runtime/environment.md#yuneta_store_dir) | `yunetas_environment.h` | root-linux (Runtime GClasses) |
| [**`yuneta_store_file`**](runtime/environment.md#yuneta_store_file) | `yunetas_environment.h` | root-linux (Runtime GClasses) |
| [**`yuneta_xpermission`**](runtime/environment.md#yuneta_xpermission) | `yunetas_environment.h` | root-linux (Runtime GClasses) |
| [**`yunetas_register_c_core`**](runtime/entry_point.md#yunetas_register_c_core) | `yunetas_register.h` | root-linux (Runtime GClasses) |
| [**`yuno_event_detroy`**](runtime/yuno.md#yuno_event_detroy) | `c_yuno.h` | root-linux (Runtime GClasses) |
| [**`yuno_event_loop`**](runtime/yuno.md#yuno_event_loop) | `c_yuno.h` | root-linux (Runtime GClasses) |
| [**`yuno_shutdown`**](runtime/entry_point.md#yuno_shutdown) | `manage_services.h` | root-linux (Runtime GClasses) |
