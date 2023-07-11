/****************************************************************************
 *          gobj.c
 *
 *          Objects G for Yuneta Simplified
 *
 *          Copyright (c) 2023 Niyamaka.
 *          All Rights Reserved.
 ****************************************************************************/
#include <time.h>
#include <string.h>
#include <ctype.h>
#include <stdio.h>
#include <stddef.h>
#include <math.h>
#include <wchar.h>

#ifdef __linux__
    #include <strings.h>
    #include <sys/utsname.h>
    #include <execinfo.h>
#endif

#include "ansi_escape_codes.h"
#include "gobj_environment.h"
#include "kwid.h"
#include "gobj.h"
#include "gbuffer.h"

extern void jsonp_free(void *ptr);

/***************************************************************
 *              Constants
 ***************************************************************/
#define MAX_LOG_HANDLER_TYPES 8

/***************************************************************
 *              Log Structures
 ***************************************************************/
/*
 *  Syslog priority definitions
 */
#define LOG_EMERG       0       /* system is unusable */
#define LOG_ALERT       1       /* action must be taken immediately */
#define LOG_CRIT        2       /* critical conditions */
#define LOG_ERR         3       /* error conditions */
#define LOG_WARNING     4       /* warning conditions */
#define LOG_NOTICE      5       /* normal but significant condition */
#define LOG_INFO        6       /* informational */
#define LOG_DEBUG       7       /* debug-level messages */

PRIVATE const char *priority_names[]={
    "EMERG",
    "ALERT",
    "CRITICAL",
    "ERROR",
    "WARNING",
    "NOTICE",
    "INFO",
    "DEBUG"
};

typedef struct {
    char handler_type[16+1];
    loghandler_close_fn_t close_fn;
    loghandler_write_fn_t write_fn;
    loghandler_fwrite_fn_t fwrite_fn;
} log_reg_t;

typedef struct {
    DL_ITEM_FIELDS

    char *handler_name;
    log_handler_opt_t handler_options;
    log_reg_t *hr;
    void *h;
} log_handler_t;

/***************************************************************
 *              GClass/GObj Structures
 ***************************************************************/
/*
 *  trace_level_t is used to describe trace levels
 */
typedef struct event_action_s {
    DL_ITEM_FIELDS

    gobj_event_t event;
    gobj_action_fn action;
    gobj_state_t next_state;
} event_action_t;

typedef struct state_s {
    DL_ITEM_FIELDS

    gobj_state_t state_name;
    dl_list_t dl_actions;
} state_t;

typedef struct event_s {
    DL_ITEM_FIELDS

    event_type_t event_type;
} event_t;

typedef enum { // WARNING add new values to opt2json()
    obflag_destroying       = 0x0001,
    obflag_destroyed        = 0x0002,
    obflag_created          = 0x0004,
    obflag_autoplay         = 0x0010,   // Set by gobj_create_tree0
    obflag_autostart        = 0x0020,   // Set by gobj_create_tree0
} obflag_t;

typedef struct gclass_s {
    DL_ITEM_FIELDS

    char *gclass_name;
    dl_list_t dl_states;            // FSM
    dl_list_t dl_events;            // FSM
    const GMETHODS *gmt;            // Global methods
    const LMETHOD *lmt;

    const sdata_desc_t *tattr_desc;
    size_t priv_size;
    const sdata_desc_t *authz_table; // acl
    /*
     *  16 levels of user trace, with name and description,
     *  applicable to gobj or gclass (all his instances)
     *  Plus until 16 global levels.
     */
    const sdata_desc_t *command_table; // if it exits then mt_command is not used. Both must return a webix json.

    const trace_level_t *s_user_trace_level;    // up to 16
    gclass_flag_t gclass_flag;

    int32_t instances;              // instances of this gclass
    uint32_t trace_level;
    uint32_t no_trace_level;
    json_t *jn_trace_filter;
} gclass_t;

typedef struct gobj_s {
    DL_ITEM_FIELDS

    gclass_t *gclass;
    struct gobj_s *parent;
    dl_list_t dl_childs;

    state_t *current_state;
    state_t *last_state;

    gobj_flag_t gobj_flag;
    obflag_t obflag;

    json_t *dl_subscriptions; // external subscriptions to events of this gobj.
    json_t *dl_subscribings;  // subscriptions of this gobj to events of others gobj.

    // Data allocated
    char *gobj_name;
    json_t *jn_attrs;
    json_t *jn_stats;
    json_t *jn_user_data;
    const char *full_name;
    const char *short_name;
    void *priv;

    char running;           // set by gobj_start/gobj_stop
    char playing;           // set by gobj_play/gobj_pause
    char disabled;          // set by gobj_enable/gobj_disable
    hgobj bottom_gobj;

    uint32_t trace_level;
    uint32_t no_trace_level;
} gobj_t;

/***************************************************************
 *              System Events
 ***************************************************************/
// Timer Events, defined here to easy filtering in trace
GOBJ_DEFINE_EVENT(EV_TIMEOUT);
GOBJ_DEFINE_EVENT(EV_TIMEOUT_PERIODIC);

// System Events
GOBJ_DEFINE_EVENT(EV_STATE_CHANGED);

// Channel Messages: Input Events (Requests)
GOBJ_DEFINE_EVENT(EV_SEND_MESSAGE);
GOBJ_DEFINE_EVENT(EV_DROP);

// Channel Messages: Output Events (Publications)
GOBJ_DEFINE_EVENT(EV_ON_OPEN);
GOBJ_DEFINE_EVENT(EV_ON_CLOSE);
GOBJ_DEFINE_EVENT(EV_ON_MESSAGE);      // with GBuffer
GOBJ_DEFINE_EVENT(EV_ON_IEV_MESSAGE);  // with IEvent
GOBJ_DEFINE_EVENT(EV_ON_ID);
GOBJ_DEFINE_EVENT(EV_ON_ID_NAK);

// Tube Messages
GOBJ_DEFINE_EVENT(EV_CONNECT);
GOBJ_DEFINE_EVENT(EV_CONNECTED);
GOBJ_DEFINE_EVENT(EV_DISCONNECT);
GOBJ_DEFINE_EVENT(EV_DISCONNECTED);
GOBJ_DEFINE_EVENT(EV_RX_DATA);
GOBJ_DEFINE_EVENT(EV_TX_DATA);
GOBJ_DEFINE_EVENT(EV_TX_READY);
GOBJ_DEFINE_EVENT(EV_STOPPED);

// Frequent states
GOBJ_DEFINE_STATE(ST_DISCONNECTED);
GOBJ_DEFINE_STATE(ST_WAIT_CONNECTED);
GOBJ_DEFINE_STATE(ST_CONNECTED);
GOBJ_DEFINE_STATE(ST_WAIT_DISCONNECTED);
GOBJ_DEFINE_STATE(ST_WAIT_STOPPED);
GOBJ_DEFINE_STATE(ST_STOPPED);
GOBJ_DEFINE_STATE(ST_SESSION);
GOBJ_DEFINE_STATE(ST_IDLE);
GOBJ_DEFINE_STATE(ST_WAIT_RESPONSE);

/****************************************************************
 *         Data
 ****************************************************************/
PUBLIC const char *event_flag_names[] = { // Strings of event_flag_t type
    "EVF_NO_WARN_SUBS",
    "EVF_OUTPUT_EVENT",
    "EVF_SYSTEM_EVENT",
    0
};

/*
 *  System (gobj) output events
 */
PRIVATE dl_list_t dl_global_event_types;

/***************************************************************
 *              Prototypes
 ***************************************************************/
PRIVATE int stdout_write(void *v, int priority, const char *bf, size_t len);
PRIVATE int stdout_fwrite(void* v, int priority, const char* format, ...);
PRIVATE void show_backtrace(loghandler_fwrite_fn_t fwrite_fn, void *h);

PRIVATE void *_mem_malloc(size_t size);
PRIVATE void _mem_free(void *p);
PRIVATE void *_mem_realloc(void *p, size_t new_size);
PRIVATE void *_mem_calloc(size_t n, size_t size);
PRIVATE int register_named_gobj(gobj_t *gobj);
PRIVATE int deregister_named_gobj(gobj_t *gobj);
PRIVATE int write_json_parameters(
    gobj_t *gobj,
    json_t *kw,     // not own
    json_t *jn_global  // not own
);
// PRIVATE json_t *extract_all_mine(
//     const char *gclass_name,
//     const char *gobj_name,
//     json_t *kw     // not own
// );
// PRIVATE json_t *extract_json_config_variables_mine(
//     const char *gclass_name,
//     const char *gobj_name,
//     json_t *kw     // not own
// );
PRIVATE int rc_walk_by_tree(
    dl_list_t *iter,
    walk_type_t walk_type,
    cb_walking_t cb_walking,
    void *user_data
);
PRIVATE int rc_walk_by_list(
    dl_list_t *iter,
    walk_type_t walk_type,
    cb_walking_t cb_walking,
    void *user_data
);
PRIVATE int rc_walk_by_level(
    dl_list_t *iter,
    walk_type_t walk_type,
    cb_walking_t cb_walking,
    void *user_data
);

PRIVATE json_t *sdata_create(gobj_t *gobj, const sdata_desc_t* schema);
PRIVATE int set_default(gobj_t *gobj, json_t *sdata, const sdata_desc_t *it);
PRIVATE json_t *gobj_hsdata(gobj_t *gobj);
PRIVATE json_t *gobj_hsdata2(gobj_t *gobj, const char *name, BOOL verbose);
PUBLIC void trace_vjson(
    hgobj gobj,
    json_t *jn_data,    // now owned
    const char *msgset,
    const char *fmt,
    va_list ap
);
PRIVATE char *tab(char *bf, int bflen);
PRIVATE inline BOOL is_machine_tracing(gobj_t * gobj, gobj_event_t event);
PRIVATE inline BOOL is_machine_not_tracing(gobj_t * gobj);
PRIVATE void _log_bf(int priority, log_opt_t opt, const char *bf, size_t len);
PRIVATE event_action_t *find_event_action(state_t *state, gobj_event_t event);
PRIVATE int add_event_type(
    dl_list_t *dl,
    event_type_t *event_type_
);

/***************************************************************
 *              Data
 ***************************************************************/
PRIVATE int argc;
PRIVATE char **argv;

PRIVATE char __initialized__ = 0;
PRIVATE int atexit_registered = 0; /* Register atexit just 1 time. */

#ifdef __linux__
PRIVATE struct utsname sys;
#endif

PRIVATE int  __inside__ = 0;  // it's a counter
PRIVATE volatile int  __shutdowning__ = 0;
PRIVATE volatile BOOL __yuno_must_die__ = FALSE;
PRIVATE int  __exit_code__ = 0;
PRIVATE json_t * (*__global_command_parser_fn__)(
    hgobj gobj,
    const char *command,
    json_t *kw,
    hgobj src
) = 0;
PRIVATE json_t * (*__global_stats_parser_fn__)(
    hgobj gobj,
    const char *stats,
    json_t *kw,
    hgobj src
) = 0;
PRIVATE authz_checker_fn __global_authz_checker_fn__ = 0;
PRIVATE authenticate_parser_fn __global_authenticate_parser_fn__ = 0;

PRIVATE json_t *__jn_global_settings__ = 0;
PRIVATE int (*__global_startup_persistent_attrs_fn__)(void) = 0;
PRIVATE void (*__global_end_persistent_attrs_fn__)(void) = 0;
PRIVATE int (*__global_load_persistent_attrs_fn__)(hgobj gobj, json_t *keys) = 0;
PRIVATE int (*__global_save_persistent_attrs_fn__)(hgobj gobj, json_t *keys) = 0;
PRIVATE int (*__global_remove_persistent_attrs_fn__)(hgobj gobj, json_t *keys) = 0;
PRIVATE json_t * (*__global_list_persistent_attrs_fn__)(hgobj gobj, json_t *keys) = 0;

PRIVATE dl_list_t dl_gclass;
PRIVATE json_t *jn_services = 0;        // Dict service:(json_int_t)(size_t)gobj
//PRIVATE kw_match_fn __publish_event_match__ = kw_match_simple;

/*
 *  Global trace levels
 */
PRIVATE const trace_level_t s_global_trace_level[16] = {
    {"machine",         "Trace machine"},
    {"create_delete",   "Trace create/delete of gobjs"},
    {"create_delete2",  "Trace create/delete of gobjs level 2: with kw"},
    {"subscriptions",   "Trace subscriptions of gobjs"},
    {"start_stop",      "Trace start/stop of gobjs"},
    {"monitor",         "Monitor activity of gobjs"},
    {"event_monitor",   "Monitor events of gobjs"},
    {"libuv",           "Trace libuv mixins"},
    {"ev_kw",           "Trace event keywords"},
    {"authzs",          "Trace authorizations"},
    {"states",          "Trace change of states"},
    {"periodic_timer",  "Trace periodic timers"},
    {"gbuffers",        "Trace gbuffers"},
    {"timer",           "Trace timers"},
    {0, 0},
};

#define __trace_gobj_create_delete__(gobj)  (gobj_trace_level(gobj) & TRACE_CREATE_DELETE)
#define __trace_gobj_create_delete2__(gobj) (gobj_trace_level(gobj) & TRACE_CREATE_DELETE2)
#define __trace_gobj_subscriptions__(gobj)  (gobj_trace_level(gobj) & TRACE_SUBSCRIPTIONS)
#define __trace_gobj_start_stop__(gobj)     (gobj_trace_level(gobj) & TRACE_START_STOP)
#define __trace_gobj_oids__(gobj)           (gobj_trace_level(gobj) & TRACE_OIDS)
#define __trace_gobj_uv__(gobj)             (gobj_trace_level(gobj) & TRACE_UV)
#define __trace_gobj_ev_kw__(gobj)          (gobj_trace_level(gobj) & TRACE_EV_KW)
#define __trace_gobj_authzs__(gobj)         (gobj_trace_level(gobj) & TRACE_AUTHZS)
#define __trace_gobj_states__(gobj)         (gobj_trace_level(gobj) & TRACE_STATES)
#define __trace_gobj_periodic_timer__(gobj) (gobj_trace_level(gobj) & TRACE_PERIODIC_TIMER)

PRIVATE uint32_t __global_trace_level__ = 0;
PRIVATE uint32_t __global_trace_no_level__ = 0;
PRIVATE volatile uint32_t __deep_trace__ = 0;

PRIVATE dl_list_t dl_log_handlers;
PRIVATE int max_log_register = 0;
PRIVATE log_reg_t log_register[MAX_LOG_HANDLER_TYPES+1];

PRIVATE gobj_t * __yuno__ = 0;
PRIVATE gobj_t * __default_service__ = 0;

PRIVATE char trace_with_short_name = FALSE; // TODO functions to set this variables
PRIVATE char trace_with_full_name = TRUE;  // TODO functions to set this variables

PRIVATE char last_message[256];
PRIVATE uint32_t __alert_count__ = 0;
PRIVATE uint32_t __critical_count__ = 0;
PRIVATE uint32_t __error_count__ = 0;
PRIVATE uint32_t __warning_count__ = 0;
PRIVATE uint32_t __info_count__ = 0;
PRIVATE uint32_t __debug_count__ = 0;

PRIVATE show_backtrace_fn_t show_backtrace_fn = show_backtrace;

PRIVATE sys_malloc_fn_t sys_malloc_fn = _mem_malloc;
PRIVATE sys_realloc_fn_t sys_realloc_fn = _mem_realloc;
PRIVATE sys_calloc_fn_t sys_calloc_fn = _mem_calloc;
PRIVATE sys_free_fn_t sys_free_fn = _mem_free;

PRIVATE size_t __max_block__ = 1*1024L*1024L;     /* largest memory block, default for no-using apps*/
PRIVATE size_t __max_system_memory__ = 10*1024L*1024L;   /* maximum core memory, default for no-using apps */
PRIVATE size_t __cur_system_memory__ = 0;   /* current system memory */




                    /*---------------------------------*
                     *      Start up functions
                     *---------------------------------*/




/***************************************************************************
 *  Initialize the yuno
 ***************************************************************************/
PUBLIC int gobj_start_up(
    int argc_,
    char *argv_[],
    json_t *jn_global_settings,
    int (*startup_persistent_attrs)(void),
    void (*end_persistent_attrs)(void),
    int (*load_persistent_attrs)(
        hgobj gobj,
        json_t *attrs  // owned
    ),
    int (*save_persistent_attrs)(
        hgobj gobj,
        json_t *attrs  // owned
    ),
    int (*remove_persistent_attrs)(
        hgobj gobj,
        json_t *attrs  // owned
    ),
    json_t * (*list_persistent_attrs)(
        hgobj gobj,
        json_t *attrs  // owned
    ),
    json_function_t global_command_parser,
    json_function_t global_stats_parser,
    authz_checker_fn global_authz_checker,
    authenticate_parser_fn global_authenticate_parser,

    size_t max_block,                       /* largest memory block */
    size_t max_system_memory                /* maximum system memory */
) {
    if(__initialized__) {
        return -1;
    }
    argc = argc_;
    argv = argv_;
    if (!atexit_registered) {
        atexit(gobj_shutdown);
        atexit_registered = 1;
    }
    __shutdowning__ = 0;
    __jn_global_settings__ =  json_deep_copy(jn_global_settings);

    __global_startup_persistent_attrs_fn__ = startup_persistent_attrs;
    __global_end_persistent_attrs_fn__ = end_persistent_attrs;
    __global_load_persistent_attrs_fn__ = load_persistent_attrs;
    __global_save_persistent_attrs_fn__ = save_persistent_attrs;
    __global_remove_persistent_attrs_fn__ = remove_persistent_attrs;
    __global_list_persistent_attrs_fn__ = list_persistent_attrs;
    __global_command_parser_fn__ = global_command_parser;
    __global_stats_parser_fn__ = global_stats_parser;
    __global_authz_checker_fn__ = global_authz_checker;
    __global_authenticate_parser_fn__ = global_authenticate_parser;

    if(__global_startup_persistent_attrs_fn__) {
        __global_startup_persistent_attrs_fn__();
    }

#ifdef __linux__
    if(uname(&sys)==0) {
        change_char(sys.machine, '_', '-');
    }
#endif

    /*----------------------------------------*
     *          Build Global Events
     *----------------------------------------*/
    event_type_t event_types_[] = {
        {EV_STATE_CHANGED,     EVF_SYSTEM_EVENT|EVF_OUTPUT_EVENT|EVF_NO_WARN_SUBS},
        {0, 0}
    };
    dl_init(&dl_global_event_types);

    event_type_t *event_types = event_types_;
    while(event_types && event_types->event) {
        add_event_type(&dl_global_event_types, event_types);
        event_types++;
    }

    dl_init(&dl_gclass);
    dl_init(&dl_log_handlers);
    jn_services = json_object();

    // dl_init(&dl_trans_filter);
    // gobj_add_publication_transformation_filter_fn("webix", webix_trans_filter);
    // helper_quote2doublequote(treedb_schema_gobjs);

    /*
     *  Chequea schema treedb, exit si falla.
     */
    //jn_treedb_schema_gobjs = legalstring2json(treedb_schema_gobjs, TRUE);
    //if(!jn_treedb_schema_gobjs) {
    //    exit(-1);
    //}

    if(max_block) {
        __max_block__ = max_block;
    }
    if(max_system_memory) {
        __max_system_memory__ = max_system_memory;
    }

    kw_add_binary_type(
        "gbuffer",
        "__gbuffer___",
        (serialize_fn_t)gbuffer_serialize,
        (deserialize_fn_t)gbuffer_deserialize,
        (incref_fn_t)gbuffer_incref,
        (decref_fn_t)gbuffer_decref
    );

    /*--------------------------------*
     *  Register included handlers
     *--------------------------------*/
    gobj_log_register_handler(
        "stdout",           // handler_name
        0,                  // close_fn
        stdout_write,       // write_fn
        stdout_fwrite       // fwrite_fn
    );

    __initialized__ = TRUE;


    return 0;
}

/***************************************************************************
 *  Order for shutdown the yuno
 ***************************************************************************/
PUBLIC void gobj_shutdown(void)
{
    if(__shutdowning__) {
        return;
    }
    __shutdowning__ = 1;

    if(__yuno__ && !(__yuno__->obflag & obflag_destroying)) {
        if(gobj_is_playing(__yuno__)) {
            gobj_pause(__yuno__);
        }
        if(gobj_is_running(__yuno__)) {
            gobj_stop(__yuno__);
        }
    }
}

/***************************************************************************
 *  Check if yuno is shutdowning
 ***************************************************************************/
PUBLIC BOOL gobj_is_shutdowning(void)
{
    return __shutdowning__;
}

/***************************************************************************
 *  De-initialize the yuno
 ***************************************************************************/
PUBLIC void gobj_end(void)
{
    if(!__initialized__) {
        return;
    }

    dl_flush(&dl_gclass, gclass_unregister);

    event_type_t *event_type;
    while((event_type = dl_first(&dl_global_event_types))) {
        dl_delete(&dl_global_event_types, event_type, 0);
        sys_free_fn(event_type);
    }

    JSON_DECREF(jn_services)

    if(__cur_system_memory__) {
        gobj_log_error(0, 0,
            "function",             "%s", __FUNCTION__,
            "msgset",               "%s", MSGSET_STATISTICS,
            "msg",                  "%s", "shutdown: system memory not free",
            "cur_system_memory",    "%ld", (long)__cur_system_memory__,
            NULL
        );
    }

    gobj_log_info(0, 0,
        "msgset",               "%s", MSGSET_STARTUP,
        "msg",                  "%s", "gobj_end",
        "hostname",             "%s", node_uuid(),
        NULL
    );

    /*
     *  WARNING Free log handler at the end! to let log the last errors
     */
    log_handler_t *lh;
    while((lh=dl_first(&dl_log_handlers))) {
        gobj_log_del_handler(lh->handler_name);
    }

    __initialized__ = FALSE;
}




                    /*---------------------------------*
                     *      GClass functions
                     *---------------------------------*/




/***************************************************************************
 *
 ***************************************************************************/
PUBLIC hgclass gclass_create(
    gclass_name_t gclass_name,
    event_type_t *event_types,
    states_t *states,
    const GMETHODS *gmt,
    const LMETHOD *lmt,
    const sdata_desc_t *tattr_desc,
    size_t priv_size,
    const sdata_desc_t *authz_table,
    const sdata_desc_t *command_table,
    const trace_level_t *s_user_trace_level,
    gclass_flag_t gclass_flag
) {
    if(!__initialized__) {
        return NULL;
    }

    if(strchr(gclass_name, '`') || strchr(gclass_name, '^') || strchr(gclass_name, '.')) {
        gobj_log_error(0, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_PARAMETER_ERROR,
            "msg",          "%s", "GClass name CANNOT have '.' or '^' or '`' char",
            "gclass",       "%s", gclass_name,
            NULL
        );
        return NULL;
    }

    if(gclass_find_by_name(gclass_name)) {
        gobj_log_error(NULL, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_PARAMETER_ERROR,
            "msg",          "%s", "gclass_name ALREADY exists",
            "gclass_name",  "%s", gclass_name,
            NULL
        );
        return NULL;
    }

    if(empty_string(gclass_name)) {
        gobj_log_error(NULL, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_PARAMETER_ERROR,
            "msg",          "%s", "gclass_name EMPTY",
            NULL
        );
        return NULL;
    }

#ifdef ESP_PLATFORM
    if(strlen(gclass_name) > 15) {
        gobj_log_error(NULL, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_PARAMETER_ERROR,
            "msg",          "%s", "gclass_name name TOO LONG for ESP32",
            "gclass_name",  "%s", gclass_name,
            "len",          "%d", (int)strlen(gclass_name),
            NULL
        );
    }
#endif

    gclass_t *gclass = sys_malloc_fn(sizeof(*gclass));
    if(gclass == NULL) {
        gobj_log_error(NULL, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_MEMORY_ERROR,
            "msg",          "%s", "No memory",
            "gclass_name",  "%s", gclass_name,
            "size",         "%d", (int)sizeof(*gclass),
            NULL
        );
        return NULL;
    }

    dl_add(&dl_gclass, gclass);
    gclass->gclass_name = gobj_strdup(gclass_name);
    gclass->gmt = gmt;
    gclass->lmt = lmt;
    gclass->tattr_desc = tattr_desc;
    gclass->priv_size = priv_size;
    gclass->authz_table = authz_table;
    gclass->command_table = command_table;
    gclass->s_user_trace_level = s_user_trace_level;
    gclass->gclass_flag = gclass_flag;

    /*----------------------------------------*
     *          Build States
     *----------------------------------------*/
    struct states_s *state = states;
    while(state->state_name) {
        gclass_add_state_with_action_list(gclass, state->state_name, state->state);
        state++;
    }

    /*----------------------------------------*
     *          Build Events
     *----------------------------------------*/
    while(event_types && event_types->event) {
        add_event_type(&gclass->dl_events, event_types);
        event_types++;
    }

    /*----------------------------------------*
     *          Check FSM
     *----------------------------------------*/
    // TODO check fsm

    return gclass;
}

/***************************************************************************
 *
 ***************************************************************************/
PUBLIC int gclass_add_state(
    hgclass hgclass,
    gobj_state_t state_name
) {
    if(!hgclass) {
        gobj_log_error(NULL, LOG_OPT_TRACE_STACK,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_PARAMETER_ERROR,
            "msg",          "%s", "hgclass NULL",
            NULL
        );
        return -1;
    }

    gclass_t *gclass = (gclass_t *)hgclass;

    state_t *state = sys_malloc_fn(sizeof(*state));
    if(state == NULL) {
        gobj_log_error(NULL, LOG_OPT_TRACE_STACK,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_MEMORY_ERROR,
            "msg",          "%s", "No memory",
            "state",        "%s", state_name,
            "size",         "%d", (int)sizeof(*state),
            NULL
        );
        return -1;
    }

    state->state_name = state_name;

    dl_add(&gclass->dl_states, state);

    return 0;
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE state_t *find_state(gclass_t *gclass, gobj_state_t state_name)
{
    state_t *state = dl_first(&gclass->dl_states);
    while(state) {
        if(state_name == state->state_name) {
            return state;
        }
        state = dl_next(state);
    }
    return NULL;
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE event_action_t *find_event_action(state_t *state, gobj_event_t event)
{
    event_action_t *event_action = dl_first(&state->dl_actions);
    while(event_action) {
        if(event == event_action->event) {
            return event_action;
        }
        event_action = dl_next(event_action);
    }
    return NULL;
}

/***************************************************************************
 *
 ***************************************************************************/
PUBLIC int gclass_add_ev_action(
    hgclass hgclass,
    gobj_state_t state_name,
    gobj_event_t event,
    gobj_action_fn action,
    gobj_state_t next_state
) {
    if(!hgclass) {
        gobj_log_error(NULL, LOG_OPT_TRACE_STACK,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_PARAMETER_ERROR,
            "msg",          "%s", "hgclass NULL",
            NULL
        );
        return -1;
    }
    if(empty_string(state_name)) {
        gobj_log_error(NULL, LOG_OPT_TRACE_STACK,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_PARAMETER_ERROR,
            "msg",          "%s", "state_name EMPTY",
            NULL
        );
        return -1;
    }

    gclass_t *gclass = (gclass_t *)hgclass;

    state_t *state = find_state(gclass, state_name);
    if(!state) {
        gobj_log_error(NULL, LOG_OPT_TRACE_STACK,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_INTERNAL_ERROR,
            "msg",          "%s", "state not found",
            "gclass",       "%s", gclass->gclass_name,
            "state",        "%s", state_name,
            NULL
        );
        return -1;
    }

    if(find_event_action(state, event)) {
        gobj_log_error(NULL, LOG_OPT_TRACE_STACK,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_INTERNAL_ERROR,
            "msg",          "%s", "event already exists",
            "gclass",       "%s", gclass->gclass_name,
            "state",        "%s", state_name,
            "event",        "%s", event,
            NULL
        );
        return -1;
    }

    event_action_t *event_action = sys_malloc_fn(sizeof(*event_action));
    if(event_action == NULL) {
        gobj_log_error(NULL, LOG_OPT_TRACE_STACK,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_MEMORY_ERROR,
            "msg",          "%s", "No memory",
            "gclass",       "%s", gclass->gclass_name,
            "state",        "%s", state_name,
            "event",        "%s", event,
            "size",         "%d", (int)sizeof(*state),
            NULL
        );
        return -1;
    }

    event_action->event = event;
    event_action->action = action;
    event_action->next_state = next_state;

    dl_add(&state->dl_actions, event_action);

    return 0;
}

/***************************************************************************
 *
 ***************************************************************************/
PUBLIC int gclass_add_state_with_action_list(
    hgclass hgclass,
    gobj_state_t state_name,
    ev_action_t *ev_action_list
) {
    if(gclass_add_state(hgclass, state_name)<0) {
        // Error already logged
        return -1;
    }

    while(ev_action_list->event) {
        gclass_add_ev_action(
            hgclass,
            state_name,
            ev_action_list->event,
            ev_action_list->action,
            ev_action_list->next_state
        );
        ev_action_list++;
    }

    return 0;
}

/***************************************************************************
 *
 ***************************************************************************/
PUBLIC hgclass gclass_find_by_name(gclass_name_t gclass_name)
{
    gclass_t *gclass = dl_first(&dl_gclass);
    while(gclass) {
        if(strcmp(gclass_name, gclass->gclass_name)==0) {
            return gclass;
        }
        gclass = dl_next(gclass);
    }

    return NULL;
}

/***************************************************************************
 *
 ***************************************************************************/
PUBLIC void gclass_unregister(hgclass hgclass)
{
    gclass_t *gclass = (gclass_t *)hgclass;

    if(gclass->instances > 0) {
        gobj_log_error(NULL, LOG_OPT_TRACE_STACK,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_INTERNAL_ERROR,
            "msg",          "%s", "Cannot unregister gclass: instances in use",
            "gclass",       "%s", gclass->gclass_name,
            "instances",    "%d", gclass->instances,
            NULL
        );
        return;
    }

    state_t *state;
    while((state = dl_first(&gclass->dl_states))) {
        dl_delete(&gclass->dl_states, state, 0);

        event_action_t *event_action;
        while((event_action = dl_first(&state->dl_actions))) {
            dl_delete(&state->dl_actions, event_action, 0);
            sys_free_fn(event_action);
        }

        sys_free_fn(state);
    }

    event_type_t *event_type;
    while((event_type = dl_first(&gclass->dl_events))) {
        dl_delete(&gclass->dl_events, event_type, 0);
        sys_free_fn(event_type);
    }

    sys_free_fn(gclass->gclass_name);
    sys_free_fn(gclass);
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE int add_event_type(
    dl_list_t *dl,
    event_type_t *event_type_
) {
    event_t *event = sys_malloc_fn(sizeof(*event));
    if(event == NULL) {
        gobj_log_error(NULL, LOG_OPT_TRACE_STACK,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_MEMORY_ERROR,
            "msg",          "%s", "No memory",
            "size",         "%d", (int)sizeof(*event),
            NULL
        );
        return -1;
    }

    event->event_type.event = event_type_->event;
    event->event_type.event_flag = event_type_->event_flag;

    return dl_add(dl, event);
}




                    /*---------------------------------*
                     *      GObj create functions
                     *---------------------------------*/




/***************************************************************************
 *
 ***************************************************************************/
PUBLIC hgobj gobj_create_obj(
    const char *gobj_name,
    gclass_name_t gclass_name,
    json_t *kw, // owned
    hgobj parent_,
    gobj_flag_t gobj_flag
) {
    if(!gobj_name) {
        gobj_name = "";
    }
    gobj_t *parent = parent_;

    /*--------------------------------*
     *      Check parameters
     *--------------------------------*/
    if(gobj_flag & (gobj_flag_yuno)) {
        if(__yuno__) {
            gobj_log_error(NULL, LOG_OPT_TRACE_STACK,
                "function",     "%s", __FUNCTION__,
                "msgset",       "%s", MSGSET_PARAMETER_ERROR,
                "msg",          "%s", "__yuno__ already created",
                NULL
            );
            JSON_DECREF(kw)
            return NULL;
        }
        gobj_flag |= gobj_flag_service;

    } else {
        if(!parent) {
            gobj_log_error(0, 0,
                "function",     "%s", __FUNCTION__,
                "msgset",       "%s", MSGSET_PARAMETER_ERROR,
                "msg",          "%s", "gobj NEEDS a parent!",
                "gclass",       "%s", gclass_name,
                "name",         "%s", gobj_name,
                NULL
            );
            JSON_DECREF(kw)
            return NULL;
        }
    }

    if(gobj_flag & (gobj_flag_service)) {
        if(gobj_find_service(gobj_name, FALSE)) {
            gobj_log_error(0, 0,
                "function",     "%s", __FUNCTION__,
                "msgset",       "%s", MSGSET_PARAMETER_ERROR,
                "msg",          "%s", "service ALREADY registered!",
                "gclass",       "%s", gclass_name,
                "name",         "%s", gobj_name,
                NULL
            );
            JSON_DECREF(kw)
            return NULL;
        }
    }

    if(gobj_flag & (gobj_flag_default_service)) {
        if(gobj_find_service(gobj_name, FALSE) || __default_service__) {
            gobj_log_error(0, 0,
                "function",     "%s", __FUNCTION__,
                "msgset",       "%s", MSGSET_PARAMETER_ERROR,
                "msg",          "%s", "default service ALREADY registered!",
                "gclass",       "%s", gclass_name,
                "name",         "%s", gobj_name,
                NULL
            );
            JSON_DECREF(kw)
            return NULL;
        }
        gobj_flag |= gobj_flag_service;
    }

    if(empty_string(gclass_name)) {
        gobj_log_error(NULL, LOG_OPT_TRACE_STACK,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_PARAMETER_ERROR,
            "msg",          "%s", "gclass_name EMPTY",
            "gobj_name",    "%s", gobj_name,
            NULL
        );
        JSON_DECREF(kw)
        return NULL;
    }

    if(strchr(gclass_name, '`') || strchr(gclass_name, '^')) {
        gobj_log_error(0, LOG_OPT_TRACE_STACK,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_PARAMETER_ERROR,
            "msg",          "%s", "GObj name CANNOT have '^' or '`' char",
            "gclass",       "%s", gclass_name,
            NULL
        );
        JSON_DECREF(kw)
        return NULL;
    }

    gclass_t *gclass = gclass_find_by_name(gclass_name);
    if(!gclass) {
        gobj_log_error(NULL, LOG_OPT_TRACE_STACK,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_PARAMETER_ERROR,
            "msg",          "%s", "gclass not found",
            "gclass",       "%s", gclass_name,
            "gobj_name",    "%s", gobj_name,
            NULL
        );
        JSON_DECREF(kw)
        return NULL;
    }

    // TODO implement gcflag_singleton

    /*--------------------------------*
     *      Alloc memory
     *--------------------------------*/
    gobj_t *gobj = sys_malloc_fn(sizeof(*gobj));
    if(gobj == NULL) {
        gobj_log_error(0, LOG_OPT_TRACE_STACK,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_MEMORY_ERROR,
            "msg",          "%s", "No memory",
            "gclass",       "%s", gclass->gclass_name,
            "gobj_name",    "%s", gobj_name,
            "size",         "%d", (int)sizeof(*gobj),
            NULL
        );
        JSON_DECREF(kw)
        return NULL;
    }

    /*--------------------------------*
     *      Initialize variables
     *--------------------------------*/
    gobj->gclass = gclass;
    gobj->parent = parent;
    dl_init(&gobj->dl_childs);
    gobj->dl_subscribings = json_array();
    gobj->dl_subscriptions = json_array();
    gobj->current_state = dl_first(&gclass->dl_states);
    gobj->last_state = 0;
    gobj->obflag = 0;
    gobj->gobj_flag = gobj_flag;

    if(__trace_gobj_create_delete__(gobj)) {
         trace_machine("💙💙⏩ creating: %s^%s",
            gclass->gclass_name,
            gobj_name
        );
    }

    /*--------------------------*
     *      Alloc data
     *--------------------------*/
    gobj->gobj_name = gobj_strdup(gobj_name);
    gobj->jn_attrs = sdata_create(gobj, gclass->tattr_desc);
    gobj->jn_stats = json_object();
    gobj->jn_user_data = json_object();
    gobj->priv = gclass->priv_size? sys_malloc_fn(gclass->priv_size):NULL;

    if(!gobj->gobj_name || !gobj->jn_user_data || !gobj->jn_stats ||
            !gobj->jn_attrs || (gclass->priv_size && !gobj->priv)) {
        gobj_log_error(0, LOG_OPT_TRACE_STACK,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_MEMORY_ERROR,
            "msg",          "%s", "No memory",
            "gclass",       "%s", gclass->gclass_name,
            "gobj_name",    "%s", gobj_name,
            NULL
        );
        JSON_DECREF(kw)
        gobj_destroy(gobj);
        return NULL;
    }

    /*--------------------------------*
     *  Write configuration
     *--------------------------------*/
    write_json_parameters(gobj, kw, __jn_global_settings__);

    /*--------------------------------------*
     *  Load writable and persistent attrs
     *  of named-gobjs and __root__
     *--------------------------------------*/
    if(gobj->gobj_flag & (gobj_flag_service)) {
        if(__global_load_persistent_attrs_fn__) {
            __global_load_persistent_attrs_fn__(gobj, 0);
        }
    }

    /*--------------------------*
     *  Register service
     *--------------------------*/
    if(gobj->gobj_flag & (gobj_flag_service)) {
        register_named_gobj(gobj);
    }
    if(gobj->gobj_flag & (gobj_flag_yuno)) {
        __yuno__ = gobj;
    }
    if(gobj->gobj_flag & (gobj_flag_default_service)) {
        __default_service__ = gobj;
    }

    /*--------------------------------------*
     *      Add to parent
     *--------------------------------------*/
    if(!(gobj->gobj_flag & (gobj_flag_yuno))) {
        dl_add(&parent->dl_childs, gobj);
    }

    /*--------------------------------*
     *      Exec mt_create
     *--------------------------------*/
    gobj->obflag |= obflag_created;
    gobj->gclass->instances++;

    if(gobj->gclass->gmt->mt_create2) {
        JSON_INCREF(kw)
        gobj->gclass->gmt->mt_create2(gobj, kw);
    } else if(gobj->gclass->gmt->mt_create) {
        gobj->gclass->gmt->mt_create(gobj);
    }

    /*--------------------------------------*
     *  Inform to parent now,
     *  when the child is full operative
     *-------------------------------------*/
    if(parent && parent->gclass->gmt->mt_child_added) {
        if(__trace_gobj_create_delete__(gobj)) {
            trace_machine("👦👦🔵 child_added(%s): %s",
                gobj_full_name(parent),
                gobj_short_name(gobj)
            );
        }
        parent->gclass->gmt->mt_child_added(parent, gobj);
    }

    if(__trace_gobj_create_delete__(gobj)) {
        trace_machine("💙💙⏪ created: %s",
            gobj_full_name(gobj)
        );
    }

    /*
     *  __root__ wants to know all
     */
    if(__yuno__ && __yuno__->gclass->gmt->mt_gobj_created) {
        __yuno__->gclass->gmt->mt_gobj_created(__yuno__, gobj);
    }

    JSON_DECREF(kw)
    return (hgobj)gobj;
}

/***************************************************************************
 *
 ***************************************************************************/
PUBLIC void gobj_destroy(hgobj hgobj)
{
    if(hgobj == NULL) {
        gobj_log_error(NULL, LOG_OPT_TRACE_STACK,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_PARAMETER_ERROR,
            "msg",          "%s", "hgobj NULL",
            NULL
        );
        return;
    }

    gobj_t *gobj = (gobj_t *)hgobj;

    /*--------------------------------*
     *      Check parameters
     *--------------------------------*/
    if(gobj->obflag & obflag_destroying) {
        gobj_log_error(0, LOG_OPT_TRACE_STACK,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_PARAMETER_ERROR,
            "msg",          "%s", "gobj DESTROYING",
            NULL
        );
        return;
    }
    gobj->obflag |= obflag_destroying;

    if(__trace_gobj_create_delete__(gobj)) {
        trace_machine("💔💔⏩ destroying: %s",
            gobj_full_name(gobj)
        );
    }

    /*----------------------------------------------*
     *  Inform to parent now,
     *  when the child is NOT still full operative
     *----------------------------------------------*/
    gobj_t *parent = gobj->parent;
    if(parent && parent->gclass->gmt->mt_child_removed) {
        if(__trace_gobj_create_delete__(gobj)) {
            trace_machine("👦👦🔴 child_removed(%s): %s",
                gobj_full_name(parent),
                gobj_short_name(gobj)
            );
        }
        parent->gclass->gmt->mt_child_removed(parent, gobj);
    }

    /*--------------------------------*
     *  Deregister if service
     *--------------------------------*/
    if(gobj->gobj_flag & gobj_flag_service) {
        deregister_named_gobj(gobj);
    }

    /*--------------------------------*
     *      Pause
     *--------------------------------*/
    if(gobj->playing) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_OPERATIONAL_ERROR,
            "msg",          "%s", "Destroying a PLAYING gobj",
            "full-name",    "%s", gobj_full_name(gobj),
            NULL
        );
        gobj_pause(gobj);
    }
    /*--------------------------------*
     *      Stop
     *--------------------------------*/
    if(gobj->running) {
        gobj_log_error(gobj, LOG_OPT_TRACE_STACK,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_OPERATIONAL_ERROR,
            "msg",          "%s", "Destroying a RUNNING gobj",
            "full-name",    "%s", gobj_full_name(gobj),
            NULL
        );
        gobj_stop(gobj);
    }

    /*--------------------------------*
     *      Delete subscriptions
     *--------------------------------*/
    json_t *dl_subs;
    dl_subs = json_copy(gobj->dl_subscriptions);
    gobj_unsubscribe_list(dl_subs, TRUE);
    dl_subs = json_copy(gobj->dl_subscribings);
    gobj_unsubscribe_list(dl_subs, TRUE);

    /*--------------------------------*
     *      Delete from parent
     *--------------------------------*/
    if(parent) {
        dl_delete(&gobj->parent->dl_childs, gobj, 0);
    }

    /*--------------------------------*
     *      Delete childs
     *--------------------------------*/
    gobj_destroy_childs(gobj);

    /*-------------------------------------------------*
     *  Exec mt_destroy
     *  Call this after all childs are destroyed.
     *  Then you can delete resources used by childs
     *  (example: event_loop in main/threads)
     *-------------------------------------------------*/
    if(gobj->obflag & obflag_created) {
        if(gobj->gclass->gmt->mt_destroy) {
            gobj->gclass->gmt->mt_destroy(gobj);
        }
    }

    /*--------------------------------*
     *      Mark as destroyed
     *--------------------------------*/
    if(__trace_gobj_create_delete__(gobj)) {
        trace_machine("💔💔⏪ destroyed: %s",
            gobj_full_name(gobj)
        );
    }
    gobj->obflag |= obflag_destroyed;

    /*--------------------------------*
     *      Dealloc data
     *--------------------------------*/
    EXEC_AND_RESET(sys_free_fn, gobj->gobj_name)
    JSON_DECREF(gobj->jn_attrs)
    JSON_DECREF(gobj->jn_stats)
    JSON_DECREF(gobj->jn_user_data)
    JSON_DECREF(gobj->dl_subscribings);
    JSON_DECREF(gobj->dl_subscriptions);
    EXEC_AND_RESET(sys_free_fn, gobj->priv)

    EXEC_AND_RESET(sys_free_fn, gobj->full_name)
    EXEC_AND_RESET(sys_free_fn, gobj->short_name)

    if(gobj->obflag & obflag_created) {
        gobj->gclass->instances--;
    }
    sys_free_fn(gobj);
}

/***************************************************************************
 *  Destroy childs
 ***************************************************************************/
PUBLIC void gobj_destroy_childs(hgobj gobj_)
{
    gobj_t * gobj = gobj_;
    if(!gobj) {
        gobj_log_error(0, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_PARAMETER_ERROR,
            "msg",          "%s", "gobj NULL",
            NULL
        );
        return;
    }

    gobj_t *child = dl_first(&gobj->dl_childs);
    while(child) {
        gobj_t *next = dl_next(child);
        if(!(child->obflag & (obflag_destroyed|obflag_destroying))) {
            gobj_destroy(child);
        }
        /*
         *  Next
         */
        child = next;
    }
}

/***************************************************************************
 *  register named gobj
 ***************************************************************************/
PRIVATE int register_named_gobj(gobj_t *gobj)
{
    if(json_object_get(jn_services, gobj->gobj_name)) {
        gobj_t *prev_gobj = (hgobj)(size_t)json_integer_value(
            json_object_get(jn_services, gobj->gobj_name)
        );

        gobj_log_error(0, LOG_OPT_TRACE_STACK,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_PARAMETER_ERROR,
            "msg",          "%s", "gobj unique ALREADY REGISTERED. Will be UPDATED",
            "prev gclass",  "%s", gobj_gclass_name(prev_gobj),
            "gclass",       "%s", gobj_gclass_name(gobj),
            "name",         "%s", gobj_name(gobj),
            NULL
        );
    }

    int ret = json_object_set_new(
        jn_services,
        gobj->gobj_name,
        json_integer((json_int_t)(size_t)gobj)
    );
    if(ret == -1) {
        gobj_log_error(0, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_JSON_ERROR,
            "msg",          "%s", "json_object_set_new() FAILED",
            "gclass",       "%s", gobj_gclass_name(gobj),
            "name",         "%s", gobj_name(gobj),
            NULL
        );
    }

    return ret;
}

/***************************************************************************
 *  deregister named gobj
 ***************************************************************************/
PRIVATE int deregister_named_gobj(gobj_t *gobj)
{
    json_t *jn_obj = json_object_get(jn_services, gobj->gobj_name);
    if(!jn_obj) {
        gobj_log_error(0, LOG_OPT_TRACE_STACK,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_PARAMETER_ERROR,
            "msg",          "%s", "gobj NOT FOUND",
            "gclass",       "%s", gobj_gclass_name(gobj),
            "name",         "%s", gobj_name(gobj),
            NULL
        );
        return -1;
    }
    json_object_del(jn_services, gobj->gobj_name);

    return 0;
}




                    /*---------------------------------*
                     *      Attributes functions
                     *---------------------------------*/




/***************************************************************************
 *  ATTR:
 *  Contain the key my gobj-name or my gclass-name?
 *  Return NULL if not, or the key cleaned of prefix.
 ***************************************************************************/
// PRIVATE const char * is_for_me(
//     const char *gclass_name,
//     const char *gobj_name,
//     const char *key)
// {
//     char *p = strchr(key, '.');
//     if(p) {
//         char temp[256]; // if your names are longer than 256 bytes, you are ...
//         int ln = p - key;
//         if (ln >= sizeof(temp))
//             return 0; // ... very intelligent!
//         strncpy(temp, key, ln);
//         temp[ln]=0;
//
//         if(gobj_name && strcmp(gobj_name, temp)==0) {
//             // match the name
//             return p+1;
//         }
//         if(gclass_name && strcasecmp(gclass_name, temp)==0) {
//             // match the gclass name
//             return p+1;
//         }
//     }
//     return 0;
// }

/***************************************************************************
 *  ATTR:
 ***************************************************************************/
// PRIVATE json_t *extract_all_mine(
//     const char *gclass_name,
//     const char *gobj_name,
//     json_t *kw)     // not own
// {
//     json_t *kw_mine = json_object();
//     const char *key;
//     json_t *jn_value;
//
//     json_object_foreach(kw, key, jn_value) {
//         const char *pk = is_for_me(
//             gclass_name,
//             gobj_name,
//             key
//         );
//         if(!pk)
//             continue;
//         if(strcasecmp(pk, "kw")==0) {
//             json_object_update(kw_mine, jn_value);
//         } else {
//             json_object_set(kw_mine, pk, jn_value);
//         }
//     }
//
//     return kw_mine;
// }

/***************************************************************************
 *  ATTR:
 ***************************************************************************/
// PRIVATE json_t *extract_json_config_variables_mine(
//     const char *gclass_name,
//     const char *gobj_name,
//     json_t *kw     // not own
// )
// {
//     json_t *kw_mine = json_object();
//     const char *key;
//     json_t *jn_value;
//
//     json_object_foreach(kw, key, jn_value) {
//         const char *pk = is_for_me(
//             gclass_name,
//             gobj_name,
//             key
//         );
//         if(!pk)
//             continue;
//         if(strcasecmp(pk, "__json_config_variables__")==0) {
//             json_object_set(kw_mine, pk, jn_value);
//         }
//     }
//
//     return kw_mine;
// }

/***************************************************************************
 *  ATTR:
 *  Get data from json kw parameter, and put the data in config sdata.

    With filter_own:
    Filter only the parameters in settings belongs to gobj.
    The gobj is searched by his named-gobj or his gclass name.
    The parameter name in settings, must be a dot-named,
    with the first item being the named-gobj o gclass name.
 *
 ***************************************************************************/
PRIVATE int write_json_parameters(
    gobj_t * gobj,
    json_t *kw,     // not own
    json_t *jn_global) // not own
{
    json_t *hs = gobj_hsdata(gobj);

    json_object_update_existing(hs, kw);


//    json_t *jn_global_mine = extract_all_mine(
//        gobj->gclass->gclass_name,
//        gobj->gobj_name,
//        jn_global
//    );

//    if(__trace_gobj_create_delete2__(gobj)) {
//        trace_machine("🔰 %s^%s => global_mine",
//            gobj->gclass->gclass_name,
//            gobj->name
//        );
//        gobj_trace_json(0, jn_global, "global all");
//        gobj_trace_json(0, jn_global_mine, "global_mine");
//    }

//    json_t *__json_config_variables__ = json_object_get(
//        jn_global_mine,
//        "__json_config_variables__"
//    );
//    if(__json_config_variables__) {
//        __json_config_variables__ = json_object();
//    }
//
//    json_object_update_new(
//        __json_config_variables__,
//        gobj_global_variables()
//    );
//    if(gobj_yuno()) {
//        json_object_update_new(
//            __json_config_variables__,
//            json_pack("{s:s, s:b}",
//                "__bind_ip__", gobj_read_str_attr(gobj_yuno(), "bind_ip"),
//                "__multiple__", gobj_read_bool_attr(gobj_yuno(), "yuno_multiple")
//            )
//        );
//    }

//    if(__trace_gobj_create_delete2__(gobj)) {
//        trace_machine("🔰🔰 %s^%s => __json_config_variables__",
//            gobj->gclass->gclass_name,
//            gobj->name
//        );
//        gobj_trace_json(0, __json_config_variables__, "__json_config_variables__");
//    }

//    json_t * new_kw = kw_apply_json_config_variables(kw, jn_global_mine);
//    json_decref(jn_global_mine);
//    if(!new_kw) {
//        return -1;
//    }

//    if(__trace_gobj_create_delete2__(gobj)) {
//        trace_machine("🔰🔰🔰 %s^%s => final kw",
//            gobj->gclass->gclass_name,
//            gobj->name
//        );
//        gobj_trace_json(0, new_kw, "final kw");
//    }

//    json_t *__user_data__ = kw_get_dict(new_kw, "__user_data__", 0, KW_EXTRACT);
//
//    int ret = json2sdata(
//        hs,
//        new_kw,
//        -1,
//        (gobj->gclass->gclass_flag & gcflag_ignore_unknown_attrs)?0:print_attr_not_found,
//        gobj
//    );
//
//    if(__user_data__) {
//        json_object_update(((gobj_t *)gobj)->jn_user_data, __user_data__);
//        json_decref(__user_data__);
//    }
//    json_decref(new_kw);
    return 0;
}

/***************************************************************************
 *  save now persistent and writable attrs
    attrs can be a string, a list of keys, or a dict with the keys to save/delete
 ***************************************************************************/
PUBLIC int gobj_save_persistent_attrs(hgobj gobj_, json_t *jn_attrs)
{
    gobj_t *gobj = gobj_;

    if(!(gobj->gobj_flag & (gobj_flag_service))) {
        gobj_log_error(0, LOG_OPT_TRACE_STACK,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_PARAMETER_ERROR,
            "msg",          "%s", "Cannot save writable-persistent attrs in no named-gobjs",
            NULL
        );
        JSON_DECREF(jn_attrs);
        return -1;
    }
    if(__global_save_persistent_attrs_fn__) {
        return __global_save_persistent_attrs_fn__(gobj, jn_attrs);
    }
    JSON_DECREF(jn_attrs)
    return -1;
}

/***************************************************************************
 *  remove file of persistent and writable attrs
 *  attrs can be a string, a list of keys, or a dict with the keys to save/delete
 *  if attrs is empty save/remove all attrs
 ***************************************************************************/
PUBLIC int gobj_remove_persistent_attrs(hgobj gobj_, json_t *jn_attrs)
{
    gobj_t *gobj = gobj_;

    if(!__global_remove_persistent_attrs_fn__) {
        JSON_DECREF(jn_attrs)
        return -1;
    }
    return __global_remove_persistent_attrs_fn__(gobj, jn_attrs);
}

/***************************************************************************
 *  List persistent attributes
 ***************************************************************************/
PUBLIC json_t * gobj_list_persistent_attrs(hgobj gobj, json_t *jn_attrs)
{
    if(!__global_list_persistent_attrs_fn__) {
        JSON_DECREF(jn_attrs);
        return 0;
    }
    return __global_list_persistent_attrs_fn__(gobj, jn_attrs);
}

/***************************************************************************
 *  WRITE - high level -
 ***************************************************************************/
PRIVATE int sdata_write_default_values(
    gobj_t *gobj,
    sdata_flag_t include_flag,
    sdata_flag_t exclude_flag
)
{
    const sdata_desc_t *it = gobj->gclass->tattr_desc;
    while(it->name) {
        if(exclude_flag && (it->flag & exclude_flag)) {
            it++;
            continue;
        }
        if(include_flag == (sdata_flag_t)-1 || (it->flag & include_flag)) {
            set_default(gobj, gobj->jn_attrs, it);
        }
        it++;
    }

    return 0;
}

/***************************************************************************
 *  Build default values
 ***************************************************************************/
PRIVATE json_t *sdata_create(gobj_t *gobj, const sdata_desc_t* schema)
{
    json_t *sdata = json_object();
    const sdata_desc_t *it = schema;
    while(it->name != 0) {
        set_default(gobj, sdata, it);
        it++;
    }

    return sdata;
}

/***************************************************************************
 *  Set array values to sdata, from json or binary
 ***************************************************************************/
PRIVATE int set_default(gobj_t *gobj, json_t *sdata, const sdata_desc_t *it)
{
    json_t *jn_value = 0;
    const char *svalue = it->default_value;
    if(!svalue) {
        svalue = "";
    }
    switch(it->type) {
        case DTP_STRING:
            jn_value = json_string(svalue);
            if(!jn_value) {
                jn_value = json_null();
            }
            break;
        case DTP_BOOLEAN:
            jn_value = json_boolean(svalue);
            if(strcasecmp(svalue, "true")==0) {
                jn_value = json_true();
            } else if(strcasecmp(svalue, "false")==0) {
                jn_value = json_false();
            } else {
                jn_value = atoi(svalue)? json_true(): json_false();
            }
            break;
        case DTP_INTEGER:
            jn_value = json_integer(atoll(svalue));
            break;
        case DTP_REAL:
            jn_value = json_real(atof(svalue));
            break;
        case DTP_LIST:
            jn_value = anystring2json(gobj, svalue, strlen(svalue), FALSE);
            if(!json_is_array(jn_value)) {
                JSON_DECREF(jn_value)
                jn_value = json_array();
            }
            break;
        case DTP_DICT:
            jn_value = anystring2json(gobj, svalue, strlen(svalue), FALSE);
            if(!json_is_object(jn_value)) {
                JSON_DECREF(jn_value)
                jn_value = json_object();
            }
            break;
        case DTP_JSON:
            jn_value = anystring2json(gobj, svalue, strlen(svalue), FALSE);
            break;
        case DTP_POINTER:
            jn_value = json_integer((json_int_t)(size_t)svalue);
            break;
    }

    if(!jn_value) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_PARAMETER_ERROR,
            "msg",          "%s", "default_value WRONG",
            "item",         "%s", it->name,
            "value",        "%s", svalue,
            NULL
        );
        jn_value = json_null();
    }

    if(json_object_set_new(sdata, it->name, jn_value)<0) {
        gobj_log_error(gobj, LOG_OPT_TRACE_STACK,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_JSON_ERROR,
            "msg",          "%s", "json_object_set() FAILED",
            "attr",         "%s", it->name,
            NULL
        );
        return -1;
    }
    return 0;
}

/***************************************************************************
 *  Set array values to sdata, from json or binary
 ***************************************************************************/
PRIVATE int json2item(
    gobj_t *gobj,
    json_t *sdata,
    const sdata_desc_t *it,
    json_t *jn_value // now owned
)
{
    if(!it) {
        return -1;
    }
    switch(it->type) {
        case DTP_STRING:
            if(!json_is_string(jn_value)) {
                gobj_log_error(gobj, LOG_OPT_TRACE_STACK,
                    "function",     "%s", __FUNCTION__,
                    "msgset",       "%s", MSGSET_PARAMETER_ERROR,
                    "msg",          "%s", "attr must be a string",
                    "attr",         "%s", it->name,
                    NULL
                );
                return -1;
            }
            break;
        case DTP_BOOLEAN:
            if(!json_is_boolean(jn_value)) {
                gobj_log_error(gobj, LOG_OPT_TRACE_STACK,
                    "function",     "%s", __FUNCTION__,
                    "msgset",       "%s", MSGSET_PARAMETER_ERROR,
                    "msg",          "%s", "attr must be an boolean",
                    "attr",         "%s", it->name,
                    NULL
                );
                return -1;
            }
            break;
        case DTP_INTEGER:
            if(!json_is_integer(jn_value)) {
                gobj_log_error(gobj, LOG_OPT_TRACE_STACK,
                    "function",     "%s", __FUNCTION__,
                    "msgset",       "%s", MSGSET_PARAMETER_ERROR,
                    "msg",          "%s", "attr must be an integer",
                    "attr",         "%s", it->name,
                    NULL
                );
                return -1;
            }
            break;
        case DTP_REAL:
            if(!json_is_real(jn_value)) {
                gobj_log_error(gobj, LOG_OPT_TRACE_STACK,
                    "function",     "%s", __FUNCTION__,
                    "msgset",       "%s", MSGSET_PARAMETER_ERROR,
                    "msg",          "%s", "attr must be a real",
                    "attr",         "%s", it->name,
                    NULL
                );
                return -1;
            }
            break;
        case DTP_LIST:
            if(!json_is_array(jn_value)) {
                gobj_log_error(gobj, LOG_OPT_TRACE_STACK,
                    "function",     "%s", __FUNCTION__,
                    "msgset",       "%s", MSGSET_PARAMETER_ERROR,
                    "msg",          "%s", "attr must be an array",
                    "attr",         "%s", it->name,
                    NULL
                );
                return -1;
            }
            break;
        case DTP_DICT:
            if(!json_is_object(jn_value)) {
                gobj_log_error(gobj, LOG_OPT_TRACE_STACK,
                    "function",     "%s", __FUNCTION__,
                    "msgset",       "%s", MSGSET_PARAMETER_ERROR,
                    "msg",          "%s", "attr must be an object",
                    "attr",         "%s", it->name,
                    NULL
                );
                return -1;
            }
            break;
        case DTP_JSON:
            break;
        case DTP_POINTER:
            if(!json_is_integer(jn_value)) {
                gobj_log_error(gobj, LOG_OPT_TRACE_STACK,
                    "function",     "%s", __FUNCTION__,
                    "msgset",       "%s", MSGSET_PARAMETER_ERROR,
                    "msg",          "%s", "attr must be an integer",
                    "attr",         "%s", it->name,
                    NULL
                );
                return -1;
            }
            break;
    }

    if(json_object_set(sdata, it->name, jn_value)<0) {
        gobj_log_error(gobj, LOG_OPT_TRACE_STACK,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_JSON_ERROR,
            "msg",          "%s", "json_object_set() FAILED",
            "attr",         "%s", it->name,
            NULL
        );
        return -1;
    }
    return 0;
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE json_t *gobj_hsdata(gobj_t *gobj)
{
    return gobj?gobj->jn_attrs:NULL;
}

/***************************************************************************
 *  Return the data description of the attribute `attr`
 *  If `attr` is null returns full attr's table
 ***************************************************************************/
PUBLIC const sdata_desc_t *gclass_attr_desc(hgclass gclass_, const char *name, BOOL verbose)
{
    gclass_t *gclass = gclass_;

    if(!gclass) {
        if(verbose) {
            gobj_log_error(0, LOG_OPT_TRACE_STACK,
                "function",     "%s", __FUNCTION__,
                "msgset",       "%s", MSGSET_PARAMETER_ERROR,
                "msg",          "%s", "gclass NULL",
                NULL
            );
        }
        return 0;
    }

    if(!name) {
        return gclass->tattr_desc;
    }
    const sdata_desc_t *it = gclass->tattr_desc;
    while(it->name) {
        if(strcmp(it->name, name)==0) {
            return it;
        }
        it++;
    }

    if(verbose) {
        gobj_log_error(0, LOG_OPT_TRACE_STACK,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_PARAMETER_ERROR,
            "msg",          "%s", "GClass Attribute NOT FOUND",
            "gclass",       "%s", gclass->gclass_name,
            "attr",         "%s", name,
            NULL
        );
    }
    return NULL;
}

/***************************************************************************
 *  Return the data description of the attribute `attr`
 *  If `attr` is null returns full attr's table
 ***************************************************************************/
PUBLIC const sdata_desc_t *gobj_attr_desc(hgobj gobj_, const char *name, BOOL verbose)
{
    gobj_t *gobj = gobj_;
    if(!gobj) {
        if(verbose) {
            gobj_log_error(0, LOG_OPT_TRACE_STACK,
                "function",     "%s", __FUNCTION__,
                "msgset",       "%s", MSGSET_PARAMETER_ERROR,
                "msg",          "%s", "gobj NULL",
                NULL
            );
        }
        return 0;
    }

    if(!name) {
        return gobj->gclass->tattr_desc;
    }

    return gclass_attr_desc(gobj->gclass, name, verbose);
}

/***************************************************************************
 *
 ***************************************************************************/
BOOL gclass_has_attr(hgclass gclass, const char* name)
{
    if(!gclass) { // WARNING must be a silence function!
        return FALSE;
    }
    if(empty_string(name)) {
        return FALSE;
    }
    return gclass_attr_desc(gclass, name, FALSE)?TRUE:FALSE;
}

/***************************************************************************
 *
 ***************************************************************************/
PUBLIC BOOL gobj_has_attr(hgobj hgobj, const char *name)
{
    if(!hgobj) { // WARNING must be a silence function!
        return FALSE;
    }
    if(empty_string(name)) {
        return FALSE;
    }
    gobj_t *gobj = (gobj_t *)hgobj;
    return gclass_attr_desc(gobj->gclass, name, FALSE)?TRUE:FALSE;
}

/***************************************************************************
 *  ATTR: read
 ***************************************************************************/
PUBLIC BOOL gobj_is_readable_attr(hgobj gobj, const char *name)
{
    const sdata_desc_t *it = gobj_attr_desc(gobj, name, TRUE);
    if(it && it->flag & (ATTR_READABLE)) {
        return TRUE;
    } else {
        return FALSE;
    }
}

/***************************************************************************
 *  ATTR: write
 ***************************************************************************/
PUBLIC BOOL gobj_is_writable_attr(hgobj gobj, const char *name)
{
    const sdata_desc_t *it = gobj_attr_desc(gobj, name, TRUE);
    if(it && it->flag & (ATTR_WRITABLE)) {
        return TRUE;
    } else {
        return FALSE;
    }
}

/***************************************************************************
 *  ATTR: write
 *  Reset volatile attributes
 ***************************************************************************/
PUBLIC int gobj_reset_volatil_attrs(hgobj gobj)
{
    return sdata_write_default_values(
        gobj,
        SDF_VOLATIL,    // include_flag
        0               // exclude_flag
    );
}

/***************************************************************************
 *  ATTR: read str
 *  Return is NOT yours! New api (May/2019), js style
 *  Inherit from js-core
 ***************************************************************************/
PUBLIC json_t *gobj_read_attr(
    hgobj gobj_,
    const char *path, // If it has ` then segments are gobj and leaf is the attribute (+bottom)
    hgobj src
) {
    gobj_t *gobj = gobj_;

    // TODO use ` segments
    if(!gobj_has_attr(gobj, path)) {
        gobj_log_error(0, LOG_OPT_TRACE_STACK,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_PARAMETER_ERROR,
            "msg",          "%s", "Attribute not found",
            "path",         "%s", path,
            NULL
        );
    }
    json_t *hs = gobj_hsdata(gobj);

//    if(gobj->gclass->gmt->mt_reading) { TODO como hago el reading de todo el record???
//        if(!(gobj->obflag & obflag_destroyed)) {
//            gobj->gclass->gmt->mt_reading(gobj, name);
//        }
//    }

    return json_object_get(hs, path);
}

/***************************************************************************
 *  ATTR: read
 ***************************************************************************/
PUBLIC json_t *gobj_read_attrs( // Return is yours!
    hgobj gobj_,
    sdata_flag_t include_flag,
    hgobj src
) {
    gobj_t *gobj = gobj_;

    json_t *jn_attrs = json_object();

    const sdata_desc_t *it = gobj->gclass->tattr_desc;
    while(it->name) {
        if(include_flag == (sdata_flag_t)-1 || (it->flag & include_flag)) {
            json_t *jn = json_object_get(gobj->jn_attrs, it->name);
            json_object_set(jn_attrs, it->name, jn);
        }
        it++;
    }

    return jn_attrs;
}

/***************************************************************************
 *  ATTR: read
 ***************************************************************************/
PUBLIC json_t *gobj_read_user_data(
    hgobj gobj,
    const char *name // If empty name return all user record
) {
    if(!gobj) {
        gobj_log_error(0, LOG_OPT_TRACE_STACK,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_PARAMETER_ERROR,
            "msg",          "%s", "gobj NULL",
            NULL
        );
        return 0;
    }
    if(empty_string(name)) {
        return ((gobj_t *)gobj)->jn_user_data;
    } else {
        return json_object_get(((gobj_t *)gobj)->jn_user_data, name);
    }
}

/***************************************************************************
 *  ATTR: write
 *  New api (May/2019), js style
 *  Inherit from js-core
 ***************************************************************************/
PUBLIC int gobj_write_attr(
    hgobj gobj,
    const char *path, // If it has ` then segments are gobj and leaf is the attribute (+bottom)
    json_t *jn_value,  // owned
    hgobj src
)
{
    // TODO use ` segments
    if(!gobj_has_attr(gobj, path)) {
        gobj_log_error(0, LOG_OPT_TRACE_STACK,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_PARAMETER_ERROR,
            "msg",          "%s", "Attribute not found",
            "path",         "%s", path,
            NULL
        );
    }
    json_t *hs = gobj_hsdata(gobj);
    const sdata_desc_t *it = gobj_attr_desc(gobj, path, TRUE);
    int ret = json2item(gobj, hs, it, jn_value);

    JSON_DECREF(jn_value);
    return ret;
}

/***************************************************************************
 *  ATTR: write
 ***************************************************************************/
PUBLIC int gobj_write_attrs(
    hgobj gobj,
    json_t *kw,  // owned
    sdata_flag_t flag,
    hgobj src
) {
    json_t *hs = gobj_hsdata(gobj);

    int ret = 0;
    const char *attr;
    json_t *jn_value;
    json_object_foreach(kw, attr, jn_value) {
        const sdata_desc_t *it = gobj_attr_desc(gobj, attr, TRUE);
        if(!it) {
            continue;
        }
        if(!(flag == (sdata_flag_t)-1 || (it->flag & flag))) {
            continue;
        }
        ret += json2item(gobj, hs, it, jn_value);
    }

    JSON_DECREF(kw);
    return ret;
}

/***************************************************************************
 *  ATTR: write
 ***************************************************************************/
PUBLIC int gobj_write_user_data(
    hgobj gobj,
    const char *name,
    json_t *value // owned
)
{
    if(!gobj) {
        gobj_log_error(0, LOG_OPT_TRACE_STACK,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_PARAMETER_ERROR,
            "msg",          "%s", "gobj NULL",
            NULL
        );
        return -1;
    }
    return json_object_set_new(((gobj_t *)gobj)->jn_user_data, name, value);
}

/***************************************************************************
 *  ATTR: Get hsdata of inherited attribute.
 ***************************************************************************/
PRIVATE json_t *gobj_hsdata2(gobj_t *gobj, const char *name, BOOL verbose)
{
    if(gobj_has_attr(gobj, name)) {
        return gobj_hsdata(gobj);
    } else if(gobj && gobj->bottom_gobj) {
        return gobj_hsdata2(gobj->bottom_gobj, name, verbose);
    }
    if(verbose) {
        gobj_log_warning(gobj, LOG_OPT_TRACE_STACK,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_PARAMETER_ERROR,
            "msg",          "%s", "GClass Attribute NOT FOUND",
            "gclass",       "%s", gobj_gclass_name(gobj),
            "attr",         "%s", name,
            NULL
        );
    }
    return 0;
}

/***************************************************************************
 *  ATTR: read str
 ***************************************************************************/
PUBLIC const char *gobj_read_str_attr(hgobj gobj_, const char *name)
{
    gobj_t *gobj = gobj_;

    json_t *hs = gobj_hsdata2(gobj, name, FALSE);
    if(hs) {
        if(gobj->gclass->gmt->mt_reading) {
            if(!(gobj->obflag & obflag_destroyed)) {
                gobj->gclass->gmt->mt_reading(gobj, name);
            }
        }
        const json_t *jn_value = json_object_get(hs, name);
        return json_string_value(jn_value);
    }

    gobj_log_warning(gobj, LOG_OPT_TRACE_STACK,
        "function",     "%s", __FUNCTION__,
        "msgset",       "%s", MSGSET_PARAMETER_ERROR,
        "msg",          "%s", "GClass Attribute NOT FOUND",
        "gclass",       "%s", gobj_gclass_name(gobj),
        "attr",         "%s", name,
        NULL
    );
    return NULL;
}

/***************************************************************************
 *  ATTR: read bool
 ***************************************************************************/
PUBLIC BOOL gobj_read_bool_attr(hgobj gobj_, const char *name)
{
    gobj_t *gobj = gobj_;

    json_t *hs = gobj_hsdata2(gobj, name, FALSE);
    if(hs) {
        if(gobj->gclass->gmt->mt_reading) {
            if(!(gobj->obflag & obflag_destroyed)) {
                gobj->gclass->gmt->mt_reading(gobj, name);
            }
        }
        const json_t *jn_value = json_object_get(hs, name);
        return json_boolean_value(jn_value);
    }

    gobj_log_warning(gobj, LOG_OPT_TRACE_STACK,
        "function",     "%s", __FUNCTION__,
        "msgset",       "%s", MSGSET_PARAMETER_ERROR,
        "msg",          "%s", "GClass Attribute NOT FOUND",
        "gclass",       "%s", gobj_gclass_name(gobj),
        "attr",         "%s", name,
        NULL
    );
    return 0;
}

/***************************************************************************
 *  ATTR: read
 ***************************************************************************/
PUBLIC json_int_t gobj_read_integer_attr(hgobj gobj_, const char *name)
{
    gobj_t *gobj = gobj_;

    json_t *hs = gobj_hsdata2(gobj, name, FALSE);
    if(hs) {
        if(gobj->gclass->gmt->mt_reading) {
            if(!(gobj->obflag & obflag_destroyed)) {
                gobj->gclass->gmt->mt_reading(gobj, name);
            }
        }
        const json_t *jn_value = json_object_get(hs, name);
        return json_integer_value(jn_value);
    }

    gobj_log_warning(gobj, LOG_OPT_TRACE_STACK,
        "function",     "%s", __FUNCTION__,
        "msgset",       "%s", MSGSET_PARAMETER_ERROR,
        "msg",          "%s", "GClass Attribute NOT FOUND",
        "gclass",       "%s", gobj_gclass_name(gobj),
        "attr",         "%s", name,
        NULL
    );
    return 0;
}

/***************************************************************************
 *  ATTR: read
 ***************************************************************************/
PUBLIC double gobj_read_real_attr(hgobj gobj_, const char *name)
{
    gobj_t *gobj = gobj_;

    json_t *hs = gobj_hsdata2(gobj, name, FALSE);
    if(hs) {
        if(gobj->gclass->gmt->mt_reading) {
            if(!(gobj->obflag & obflag_destroyed)) {
                gobj->gclass->gmt->mt_reading(gobj, name);
            }
        }
        const json_t *jn_value = json_object_get(hs, name);
        return json_real_value(jn_value);
    }

    gobj_log_warning(gobj, LOG_OPT_TRACE_STACK,
        "function",     "%s", __FUNCTION__,
        "msgset",       "%s", MSGSET_PARAMETER_ERROR,
        "msg",          "%s", "GClass Attribute NOT FOUND",
        "gclass",       "%s", gobj_gclass_name(gobj),
        "attr",         "%s", name,
        NULL
    );
    return 0;
}

/***************************************************************************
 *  ATTR: read
 ***************************************************************************/
PUBLIC json_t *gobj_read_json_attr(hgobj gobj_, const char *name) // WARNING return its NOT YOURS
{
    gobj_t *gobj = gobj_;

    json_t *hs = gobj_hsdata2(gobj, name, FALSE);
    if(hs) {
        if(gobj->gclass->gmt->mt_reading) {
            if(!(gobj->obflag & obflag_destroyed)) {
                gobj->gclass->gmt->mt_reading(gobj, name);
            }
        }
        json_t *jn_value = json_object_get(hs, name);
        return jn_value;
    }

    gobj_log_warning(gobj, LOG_OPT_TRACE_STACK,
        "function",     "%s", __FUNCTION__,
        "msgset",       "%s", MSGSET_PARAMETER_ERROR,
        "msg",          "%s", "GClass Attribute NOT FOUND",
        "gclass",       "%s", gobj_gclass_name(gobj),
        "attr",         "%s", name,
        NULL
    );
    return 0;
}

/***************************************************************************
 *  ATTR: read
 ***************************************************************************/
PUBLIC void *gobj_read_pointer_attr(hgobj gobj_, const char *name)
{
    gobj_t *gobj = gobj_;

    json_t *hs = gobj_hsdata2(gobj, name, FALSE);
    if(hs) {
        if(gobj->gclass->gmt->mt_reading) {
            if(!(gobj->obflag & obflag_destroyed)) {
                gobj->gclass->gmt->mt_reading(gobj, name);
            }
        }
        const json_t *jn_value = json_object_get(hs, name);
        return (void *)(size_t)json_integer_value(jn_value);
    }

    gobj_log_warning(gobj, LOG_OPT_TRACE_STACK,
        "function",     "%s", __FUNCTION__,
        "msgset",       "%s", MSGSET_PARAMETER_ERROR,
        "msg",          "%s", "GClass Attribute NOT FOUND",
        "gclass",       "%s", gobj_gclass_name(gobj),
        "attr",         "%s", name,
        NULL
    );
    return 0;
}

/***************************************************************************
 *  ATTR: write
 ***************************************************************************/
PUBLIC int gobj_write_str_attr(hgobj gobj_, const char *name, const char *value)
{
    gobj_t *gobj = gobj_;

    json_t *hs = gobj_hsdata2(gobj, name, FALSE);
    if(hs) {
        int ret = json_object_set_new(hs, name, json_string(value));
        if(gobj->gclass->gmt->mt_writing) {
            if((gobj->obflag & obflag_created) && !(gobj->obflag & obflag_destroyed)) {
                // Avoid call to mt_writing before mt_create!
                gobj->gclass->gmt->mt_writing(gobj, name);
            }
        }
        return ret;
    }

    gobj_log_warning(gobj, LOG_OPT_TRACE_STACK,
        "function",     "%s", __FUNCTION__,
        "msgset",       "%s", MSGSET_PARAMETER_ERROR,
        "msg",          "%s", "GClass Attribute NOT FOUND",
        "gclass",       "%s", gobj_gclass_name(gobj),
        "attr",         "%s", name,
        NULL
    );
    return -1;
}

/***************************************************************************
 *  ATTR: write
 ***************************************************************************/
PUBLIC int gobj_write_bool_attr(hgobj gobj_, const char *name, BOOL value)
{
    gobj_t *gobj = gobj_;

    json_t *hs = gobj_hsdata2(gobj, name, FALSE);
    if(hs) {
        int ret = json_object_set_new(hs, name, json_boolean(value));
        if(gobj->gclass->gmt->mt_writing) {
            if((gobj->obflag & obflag_created) && !(gobj->obflag & obflag_destroyed)) {
                // Avoid call to mt_writing before mt_create!
                gobj->gclass->gmt->mt_writing(gobj, name);
            }
        }
        return ret;
    }

    gobj_log_warning(gobj, LOG_OPT_TRACE_STACK,
        "function",     "%s", __FUNCTION__,
        "msgset",       "%s", MSGSET_PARAMETER_ERROR,
        "msg",          "%s", "GClass Attribute NOT FOUND",
        "gclass",       "%s", gobj_gclass_name(gobj),
        "attr",         "%s", name,
        NULL
    );
    return -1;
}

/***************************************************************************
 *  ATTR: write
 ***************************************************************************/
PUBLIC int gobj_write_integer_attr(hgobj gobj_, const char *name, json_int_t value)
{
    gobj_t *gobj = gobj_;

    json_t *hs = gobj_hsdata2(gobj, name, FALSE);
    if(hs) {
        int ret = json_object_set_new(hs, name, json_integer(value));
        if(gobj->gclass->gmt->mt_writing) {
            if((gobj->obflag & obflag_created) && !(gobj->obflag & obflag_destroyed)) {
                // Avoid call to mt_writing before mt_create!
                gobj->gclass->gmt->mt_writing(gobj, name);
            }
        }
        return ret;
    }

    gobj_log_warning(gobj, LOG_OPT_TRACE_STACK,
        "function",     "%s", __FUNCTION__,
        "msgset",       "%s", MSGSET_PARAMETER_ERROR,
        "msg",          "%s", "GClass Attribute NOT FOUND",
        "gclass",       "%s", gobj_gclass_name(gobj),
        "attr",         "%s", name,
        NULL
    );
    return -1;
}

/***************************************************************************
 *  ATTR: write
 ***************************************************************************/
PUBLIC int gobj_write_real_attr(hgobj gobj_, const char *name, double value)
{
    gobj_t *gobj = gobj_;

    json_t *hs = gobj_hsdata2(gobj, name, FALSE);
    if(hs) {
        int ret = json_object_set_new(hs, name, json_real(value));
        if(gobj->gclass->gmt->mt_writing) {
            if((gobj->obflag & obflag_created) && !(gobj->obflag & obflag_destroyed)) {
                // Avoid call to mt_writing before mt_create!
                gobj->gclass->gmt->mt_writing(gobj, name);
            }
        }
        return ret;
    }

    gobj_log_warning(gobj, LOG_OPT_TRACE_STACK,
        "function",     "%s", __FUNCTION__,
        "msgset",       "%s", MSGSET_PARAMETER_ERROR,
        "msg",          "%s", "GClass Attribute NOT FOUND",
        "gclass",       "%s", gobj_gclass_name(gobj),
        "attr",         "%s", name,
        NULL
    );
    return -1;
}

/***************************************************************************
 *  ATTR: write.  WARNING json is incref! (new V2)
 ***************************************************************************/
PUBLIC int gobj_write_json_attr(hgobj gobj_, const char *name, json_t *jn_value)
{
    gobj_t *gobj = gobj_;

    json_t *hs = gobj_hsdata2(gobj, name, FALSE);
    if(hs) {
        int ret = json_object_set(hs, name, jn_value);
        if(gobj->gclass->gmt->mt_writing) {
            if((gobj->obflag & obflag_created) && !(gobj->obflag & obflag_destroyed)) {
                // Avoid call to mt_writing before mt_create!
                gobj->gclass->gmt->mt_writing(gobj, name);
            }
        }
        return ret;
    }

    gobj_log_warning(gobj, LOG_OPT_TRACE_STACK,
        "function",     "%s", __FUNCTION__,
        "msgset",       "%s", MSGSET_PARAMETER_ERROR,
        "msg",          "%s", "GClass Attribute NOT FOUND",
        "gclass",       "%s", gobj_gclass_name(gobj),
        "attr",         "%s", name,
        NULL
    );
    JSON_INCREF(jn_value); // value is incref always, in error case too
    return -1;
}

/***************************************************************************
 *  ATTR: write.  WARNING json is NOT incref
 ***************************************************************************/
PUBLIC int gobj_write_new_json_attr(hgobj gobj_, const char *name, json_t *jn_value)
{
    gobj_t *gobj = gobj_;

    json_t *hs = gobj_hsdata2(gobj, name, FALSE);
    if(hs) {
        int ret = json_object_set_new(hs, name, jn_value);
        if(gobj->gclass->gmt->mt_writing) {
            if((gobj->obflag & obflag_created) && !(gobj->obflag & obflag_destroyed)) {
                // Avoid call to mt_writing before mt_create!
                gobj->gclass->gmt->mt_writing(gobj, name);
            }
        }
        return ret;
    }

    gobj_log_warning(gobj, LOG_OPT_TRACE_STACK,
        "function",     "%s", __FUNCTION__,
        "msgset",       "%s", MSGSET_PARAMETER_ERROR,
        "msg",          "%s", "GClass Attribute NOT FOUND",
        "gclass",       "%s", gobj_gclass_name(gobj),
        "attr",         "%s", name,
        NULL
    );
    JSON_DECREF(jn_value);
    return -1;
}

/***************************************************************************
 *  ATTR: write
 ***************************************************************************/
PUBLIC int gobj_write_pointer_attr(hgobj gobj_, const char *name, void *value)
{
    gobj_t *gobj = gobj_;

    json_t *hs = gobj_hsdata2(gobj, name, FALSE);
    if(hs) {
        int ret = json_object_set_new(hs, name, json_integer((json_int_t)(size_t)value));
        if(gobj->gclass->gmt->mt_writing) {
            if((gobj->obflag & obflag_created) && !(gobj->obflag & obflag_destroyed)) {
                // Avoid call to mt_writing before mt_create!
                gobj->gclass->gmt->mt_writing(gobj, name);
            }
        }
        return ret;
    }

    gobj_log_warning(gobj, LOG_OPT_TRACE_STACK,
        "function",     "%s", __FUNCTION__,
        "msgset",       "%s", MSGSET_PARAMETER_ERROR,
        "msg",          "%s", "GClass Attribute NOT FOUND",
        "gclass",       "%s", gobj_gclass_name(gobj),
        "attr",         "%s", name,
        NULL
    );
    return -1;
}




                    /*---------------------------------*
                     *  Operational functions
                     *---------------------------------*/




/***************************************************************************
 *
 ***************************************************************************/
PRIVATE json_t *gobj_check_required_attrs(gobj_t *gobj)
{
    json_t *hs = gobj_hsdata(gobj);
    json_t *jn_required_attrs = json_array();

    const sdata_desc_t *it = gobj->gclass->tattr_desc;
    while(it->name) {
        if(it->flag & SDF_REQUIRED) {
            const json_t *jn_value = json_object_get(hs, it->name);
            switch(it->type) {
                case DTP_STRING:
                    if(empty_string(json_string_value(jn_value))) {
                        json_array_append_new(jn_required_attrs, json_string(it->name));
                    }
                    break;
                case DTP_LIST:
                case DTP_DICT:
                case DTP_JSON:
                    if(empty_json(jn_value)) {
                        json_array_append_new(jn_required_attrs, json_string(it->name));
                    }
                    break;
                case DTP_POINTER:
                    if(!json_integer_value(jn_value)) {
                        json_array_append_new(jn_required_attrs, json_string(it->name));
                    }
                    break;
                case DTP_BOOLEAN:
                case DTP_INTEGER:
                case DTP_REAL:
                    break;
            }
        }
        it++;
    }
    if(json_array_size(jn_required_attrs)==0) {
        JSON_DECREF(jn_required_attrs)
    }
    return jn_required_attrs;
}

/***************************************************************************
 *  Execute global method "start" of the gobj
 ***************************************************************************/
PUBLIC int gobj_start(hgobj gobj_)
{
    gobj_t * gobj = gobj_;

    if(!gobj) {
        gobj_log_error(0, LOG_OPT_TRACE_STACK,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_PARAMETER_ERROR,
            "msg",          "%s", "gobj NULL",
            NULL
        );
        return -1;
    }
    if(gobj->running) {
        gobj_log_error(gobj, LOG_OPT_TRACE_STACK,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_OPERATIONAL_ERROR,
            "msg",          "%s", "GObj ALREADY RUNNING",
            NULL
        );
        return -1;
    }
    if(gobj->disabled) {
        gobj_log_warning(gobj, LOG_OPT_TRACE_STACK,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_OPERATIONAL_ERROR,
            "msg",          "%s", "GObj DISABLED",
            NULL
        );
        return -1;
    }

    /*
     *  Check required attributes.
     */
    json_t *jn_required_attrs = gobj_check_required_attrs(gobj);
    if(jn_required_attrs) {
        gobj_log_error(gobj, LOG_OPT_TRACE_STACK,
            "gobj",         "%s", gobj_full_name(gobj),
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_OPERATIONAL_ERROR,
            "msg",          "%s", "Cannot start without all required attributes",
            "attrs",        "%j", jn_required_attrs,
            NULL
        );
        JSON_DECREF(jn_required_attrs)
        return -1;
    }

    if(__trace_gobj_start_stop__(gobj)) {
        trace_machine("⏺ ⏺ start: %s",
            gobj_full_name(gobj)
        );
    }
//    if(__trace_gobj_monitor__(gobj)) {
//        monitor_gobj(MTOR_GOBJ_START, gobj);
//    }

    gobj->running = TRUE;

    int ret = 0;
    if(gobj->gclass->gmt->mt_start) {
        ret = gobj->gclass->gmt->mt_start(gobj);
    }
    return ret;
}

/***************************************************************************
 *  Start all childs of the gobj.
 ***************************************************************************/
PRIVATE int cb_start_child(hgobj child_, void *user_data)
{
    gobj_t *child = child_;
    if(child->gclass->gclass_flag & gcflag_manual_start) {
        return 0;
    }
    if(!gobj_is_running(child) && !gobj_is_disabled(child)) {
        gobj_start(child);
    }
    return 0;
}
PUBLIC int gobj_start_childs(hgobj gobj)
{
    if(!gobj) {
        gobj_log_error(0, LOG_OPT_TRACE_STACK,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_PARAMETER_ERROR,
            "msg",          "%s", "gobj NULL",
            NULL
        );
        return -1;
    }

    return gobj_walk_gobj_childs(gobj, WALK_FIRST2LAST, cb_start_child, 0);
}

/***************************************************************************
 *  Start this gobj and all childs tree of the gobj.
 ***************************************************************************/
PRIVATE int cb_start_child_tree(hgobj child_, void *user_data)
{
    gobj_t *child = child_;
    if(child->gclass->gclass_flag & gcflag_manual_start) {
        return 1;
    }
    if(gobj_is_disabled(child)) {
        return 1;
    }
    if(!gobj_is_running(child)) {
        gobj_start(child);
    }
    return 0;
}
PUBLIC int gobj_start_tree(hgobj gobj_)
{
    gobj_t * gobj = gobj_;
    if(!gobj) {
        gobj_log_error(0, LOG_OPT_TRACE_STACK,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_PARAMETER_ERROR,
            "msg",          "%s", "gobj NULL",
            NULL
        );
        return -1;
    }
    if(__trace_gobj_start_stop__(gobj)) {
        trace_machine("⏺ ⏺ ⏺ ⏺ start_tree: %s",
            gobj_full_name(gobj)
        );
    }

    if((gobj->gclass->gclass_flag & gcflag_manual_start)) {
        return 0;
    } else if(gobj->disabled) {
        return 0;
    } else {
        if(!gobj->running) {
            gobj_start(gobj);
        }
    }
    return gobj_walk_gobj_childs_tree(gobj, WALK_TOP2BOTTOM, cb_start_child_tree, 0);
}

/***************************************************************************
 *  Execute global method "stop" of the gobj
 ***************************************************************************/
PUBLIC int gobj_stop(hgobj gobj_)
{
    gobj_t * gobj = gobj_;

    if(!gobj) {
        gobj_log_error(0, LOG_OPT_TRACE_STACK,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_PARAMETER_ERROR,
            "msg",          "%s", "gobj NULL",
            NULL
        );
        return -1;
    }
    if(gobj->obflag & obflag_destroying) {
        gobj_log_error(gobj, LOG_OPT_TRACE_STACK,
            "gobj",         "%s", gobj_full_name(gobj),
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_PARAMETER_ERROR,
            "msg",          "%s", "hgobj destroying",
            NULL
        );
        return -1;
    }
    if(!gobj->running) {
        if(!gobj_is_shutdowning()) {
            gobj_log_error(gobj, LOG_OPT_TRACE_STACK,
                "gobj",         "%s", gobj_full_name(gobj),
                "function",     "%s", __FUNCTION__,
                "msgset",       "%s", MSGSET_OPERATIONAL_ERROR,
                "msg",          "%s", "GObj NOT RUNNING",
                NULL
            );
        }
        return -1;
    }
    if(gobj->playing) {
        // It's auto-stopping but display error (magic but warn!).
        gobj_log_info(gobj, 0,
            "gobj",         "%s", gobj_full_name(gobj),
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_OPERATIONAL_ERROR,
            "msg",          "%s", "GObj stopping without previous pause",
            NULL
        );
        gobj_pause(gobj);
    }

    if(__trace_gobj_start_stop__(gobj)) {
        trace_machine("⏹ ⏹ stop: %s",
            gobj_full_name(gobj)
        );
    }
//    if(__trace_gobj_monitor__(gobj)) {
//        monitor_gobj(MTOR_GOBJ_STOP, gobj);
//    }

    gobj->running = FALSE;

    int ret = 0;
    if(gobj->gclass->gmt->mt_stop) {
        ret = gobj->gclass->gmt->mt_stop(gobj);
    }

    return ret;
}

/***************************************************************************
 *  Stop all childs of the gobj.
 ***************************************************************************/
PRIVATE int cb_stop_child(hgobj child, void *user_data)
{
    if(gobj_is_running(child)) {
        gobj_stop(child);
    }
    return 0;
}
PUBLIC int gobj_stop_childs(hgobj gobj_)
{
    gobj_t *gobj = gobj_;
    if(!gobj) {
        gobj_log_error(0, LOG_OPT_TRACE_STACK,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_PARAMETER_ERROR,
            "msg",          "%s", "gobj NULL",
            NULL
        );
        return -1;
    }
    if(gobj->obflag & obflag_destroying) {
        gobj_log_error(gobj, LOG_OPT_TRACE_STACK,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_PARAMETER_ERROR,
            "msg",          "%s", "hgobj destroying",
            NULL
        );
        return -1;
    }
    return gobj_walk_gobj_childs(gobj, WALK_FIRST2LAST, cb_stop_child, 0);
}

/***************************************************************************
 *  Stop this gobj and all childs tree of the gobj.
 ***************************************************************************/
PUBLIC int gobj_stop_tree(hgobj gobj_)
{
    gobj_t *gobj = gobj_;
    if(!gobj) {
        gobj_log_error(0, LOG_OPT_TRACE_STACK,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_PARAMETER_ERROR,
            "msg",          "%s", "gobj NULL",
            NULL
        );
        return -1;
    }
    if(gobj->obflag & obflag_destroying) {
        gobj_log_error(0, LOG_OPT_TRACE_STACK,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_PARAMETER_ERROR,
            "msg",          "%s", "hgobj destroying",
            NULL
        );
        return -1;
    }
    if(__trace_gobj_start_stop__(gobj)) {
        trace_machine("⏹ ⏹ ⏹ ⏹ stop_tree: %s",
            gobj_full_name(gobj)
        );
    }

    if(gobj_is_running(gobj)) {
        gobj_stop(gobj);
    }
    return gobj_walk_gobj_childs_tree(gobj, WALK_TOP2BOTTOM, cb_stop_child, 0);
}

/***************************************************************************
 *  Execute global method "play" of the gobj
 ***************************************************************************/
PUBLIC int gobj_play(hgobj gobj_)
{
    gobj_t * gobj = gobj_;

    if(!gobj) {
        gobj_log_error(0, LOG_OPT_TRACE_STACK,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_PARAMETER_ERROR,
            "msg",          "%s", "gobj NULL",
            NULL
        );
        return -1;
    }
    if(gobj->obflag & obflag_destroying) {
        gobj_log_error(0, LOG_OPT_TRACE_STACK,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_PARAMETER_ERROR,
            "msg",          "%s", "hgobj destroying",
            NULL
        );
        return -1;
    }
    if(gobj->playing) {
        gobj_log_warning(gobj, LOG_OPT_TRACE_STACK,
            "gobj",         "%s", gobj_full_name(gobj),
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_OPERATIONAL_ERROR,
            "msg",          "%s", "GObj ALREADY PLAYING",
            NULL
        );
        return -1;
    }
    if(gobj->disabled) {
        gobj_log_warning(gobj, 0,
            "gobj",         "%s", gobj_full_name(gobj),
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_OPERATIONAL_ERROR,
            "msg",          "%s", "GObj DISABLED",
            NULL
        );
        return -1;
    }
    if(!gobj_is_running(gobj)) {
        if(!(gobj->gclass->gclass_flag & gcflag_required_start_to_play)) {
            // Default: It's auto-starting but display error (magic but warn!).
            gobj_log_error(gobj, LOG_OPT_TRACE_STACK,
                "gobj",         "%s", gobj_full_name(gobj),
                "function",     "%s", __FUNCTION__,
                "msgset",       "%s", MSGSET_OPERATIONAL_ERROR,
                "msg",          "%s", "GObj playing without previous start",
                NULL
            );
            gobj_start(gobj);
        } else {
            gobj_log_error(gobj, LOG_OPT_TRACE_STACK,
                "gobj",         "%s", gobj_full_name(gobj),
                "function",     "%s", __FUNCTION__,
                "msgset",       "%s", MSGSET_OPERATIONAL_ERROR,
                "msg",          "%s", "Cannot play, start not done",
                NULL
            );
            return -1;
        }
    }

    if(__trace_gobj_start_stop__(gobj)) {
        if(!is_machine_not_tracing(gobj)) {
            trace_machine("⏯ ⏯ play: %s",
                gobj_full_name(gobj)
            );
        }
    }
//    if(__trace_gobj_monitor__(gobj)) {
//        monitor_gobj(MTOR_GOBJ_PLAY, gobj);
//    }
    gobj->playing = TRUE;

    if(gobj->gclass->gmt->mt_play) {
        int ret = gobj->gclass->gmt->mt_play(gobj);
        if(ret < 0) {
            gobj->playing = FALSE;
        }
        return ret;
    } else {
        return 0;
    }
}

/***************************************************************************
 *  Execute global method "pause" of the gobj
 ***************************************************************************/
PUBLIC int gobj_pause(hgobj gobj_)
{
    gobj_t * gobj = gobj_;

    if(!gobj) {
        gobj_log_error(0, LOG_OPT_TRACE_STACK,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_PARAMETER_ERROR,
            "msg",          "%s", "gobj NULL",
            NULL
        );
        return -1;
    }
    if(gobj->obflag & obflag_destroying) {
        gobj_log_error(0, LOG_OPT_TRACE_STACK,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_PARAMETER_ERROR,
            "msg",          "%s", "hgobj destroying",
            NULL
        );
        return -1;
    }
    if(!gobj->playing) {
        gobj_log_info(gobj, 0,
            "gobj",         "%s", gobj_full_name(gobj),
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_OPERATIONAL_ERROR,
            "msg",          "%s", "GObj NOT PLAYING",
            NULL
        );
        return -1;
    }

    if(__trace_gobj_start_stop__(gobj)) {
        if(!is_machine_not_tracing(gobj)) {
            trace_machine("⏸ ⏸ pause: %s",
                gobj_full_name(gobj)
            );
        }
    }
//    if(__trace_gobj_monitor__(gobj)) {
//        monitor_gobj(MTOR_GOBJ_PAUSE, gobj);
//    }
    gobj->playing = FALSE;

    if(gobj->gclass->gmt->mt_pause) {
        return gobj->gclass->gmt->mt_pause(gobj);
    } else {
        return 0;
    }

}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE gobj_t * _deep_bottom_gobj(gobj_t *gobj)
{
    if(gobj->bottom_gobj) {
        return _deep_bottom_gobj(gobj->bottom_gobj);
    } else {
        return gobj;
    }
}

/***************************************************************************
 *
 ***************************************************************************/
PUBLIC int gobj_disable(hgobj gobj_)
{
    gobj_t * gobj = gobj_;

    if(!gobj) {
        gobj_log_error(0, LOG_OPT_TRACE_STACK,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_PARAMETER_ERROR,
            "msg",          "%s", "gobj NULL",
            NULL
        );
        return -1;
    }
    if(gobj->disabled) {
        gobj_log_info(0, LOG_OPT_TRACE_STACK,
            "gobj",         "%s", gobj_full_name(gobj),
            "msgset",       "%s", MSGSET_OPERATIONAL_ERROR,
            "msg",          "%s", "GObj ALREADY disabled",
            NULL
        );
        return -1;
    }
    gobj->disabled = TRUE;
    if(gobj->gclass->gmt->mt_disable) {
        return gobj->gclass->gmt->mt_disable(gobj);
    } else {
        return gobj_stop_tree(gobj);
    }
}

/***************************************************************************
 *
 ***************************************************************************/
PUBLIC int gobj_enable(hgobj gobj_)
{
    gobj_t * gobj = gobj_;

    if(!gobj) {
        gobj_log_error(0, LOG_OPT_TRACE_STACK,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_PARAMETER_ERROR,
            "msg",          "%s", "gobj NULL",
            NULL
        );
        return -1;
    }
    if(!gobj->disabled) {
        gobj_log_info(0, LOG_OPT_TRACE_STACK,
            "gobj",         "%s", gobj_full_name(gobj),
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_OPERATIONAL_ERROR,
            "msg",          "%s", "GObj NOT disabled",
            NULL
        );
        return -1;
    }
    gobj->disabled = FALSE;
    if(gobj->gclass->gmt->mt_enable) {
        return gobj->gclass->gmt->mt_enable(gobj);
    } else {
        return gobj_start_tree(gobj);
    }
}

/***************************************************************************
 *
 ***************************************************************************/
PUBLIC void gobj_set_yuno_must_die(void)
{
    __yuno_must_die__ = TRUE;
}

/***************************************************************************
 *
 ***************************************************************************/
PUBLIC BOOL gobj_get_yuno_must_die(void)
{
    return __yuno_must_die__;
}

/***************************************************************************
 *
 ***************************************************************************/
PUBLIC void gobj_set_exit_code(int exit_code)
{
    __exit_code__ = exit_code;
}

/***************************************************************************
 *
 ***************************************************************************/
PUBLIC int gobj_get_exit_code(void)
{
    return __exit_code__;
}

/***************************************************************************
 *
 ***************************************************************************/
PUBLIC hgobj gobj_default_service(void)
{
    return __default_service__;
}

/***************************************************************************
 *
 ***************************************************************************/
PUBLIC hgobj gobj_find_service(const char *service, BOOL verbose)
{
    if(empty_string(service)) {
        if(verbose) {
            gobj_log_error(0, LOG_OPT_TRACE_STACK,
                "function",     "%s", __FUNCTION__,
                "msgset",       "%s", MSGSET_PARAMETER_ERROR,
                "msg",          "%s", "service EMPTY",
                NULL
            );
        }
        return NULL;
    }

    json_t *o = json_object_get(jn_services, service);
    if(!o) {
        if(verbose) {
            gobj_log_error(0, LOG_OPT_TRACE_STACK,
                "function",     "%s", __FUNCTION__,
                "msgset",       "%s", MSGSET_PARAMETER_ERROR,
                "msg",          "%s", "service NOT FOUND",
                "service",      "%s", service,
                NULL
            );
        }
        return NULL;
    }

    return (hgobj)(size_t)json_integer_value(o);
}

/***************************************************************************
 *  If service has mt_play then start only the service gobj.
 *      (Let mt_play be responsible to start their tree)
 *  If service has not mt_play then start the tree with gobj_start_tree().
 ***************************************************************************/
PUBLIC int gobj_autostart_services(void)
{
    const char *key; json_t *jn_service;
    json_object_foreach(jn_services, key, jn_service) {
        gobj_t *gobj = (gobj_t *)(size_t)json_integer_value(jn_service);
        if(gobj->gobj_flag & gobj_flag_yuno) {
            continue;
        }
        if(gobj->obflag & obflag_autostart) {
            if(gobj->gclass->gmt->mt_play) { // HACK checking mt_play because if exists he have the power on!
                gobj_start(gobj);
            } else {
                gobj_start_tree(gobj);
            }
        }
    }
    return 0;
}

/***************************************************************************
 *
 ***************************************************************************/
PUBLIC int gobj_autoplay_services(void)
{
    const char *key; json_t *jn_service;
    json_object_foreach(jn_services, key, jn_service) {
        gobj_t *gobj = (gobj_t *)(size_t)json_integer_value(jn_service);
        if(gobj->gobj_flag & gobj_flag_yuno) {
            continue;
        }
        if(gobj->obflag & obflag_autoplay) {
            if(gobj->gclass->gmt->mt_play) { // HACK checking mt_play because if exists he have the power on!
                gobj_start(gobj);
            } else {
                gobj_start_tree(gobj);
            }
        }
    }
    return 0;
}

/***************************************************************************
 *
 ***************************************************************************/
PUBLIC int gobj_stop_services(void)
{
    const char *key; json_t *jn_service;
    json_object_foreach(jn_services, key, jn_service) {
        gobj_t *gobj = (gobj_t *)(size_t)json_integer_value(jn_service);
        if(gobj->gobj_flag & gobj_flag_yuno) {
            continue;
        }
        gobj_log_debug(0,0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_STARTUP,
            "msg",          "%s", "STOP service",
            "service",      "%s", gobj_short_name(gobj),
            NULL
        );
        if(gobj_is_playing(gobj)) {
            gobj_pause(gobj);
        }
        gobj_stop_tree(gobj);
    }

    return 0;
}

/***************************************************************************
 *  Set manually the bottom gobj
 ***************************************************************************/
PUBLIC hgobj gobj_set_bottom_gobj(hgobj gobj_, hgobj bottom_gobj)
{
    gobj_t *gobj = gobj_;
    if(!gobj) {
        gobj_log_error(0, LOG_OPT_TRACE_STACK,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_PARAMETER_ERROR,
            "msg",          "%s", "gobj NULL",
            NULL
        );
        return 0;
    }

    if(is_machine_tracing(gobj, NULL)) {
        trace_machine("🔽 set_bottom_gobj('%s') = '%s'",
            gobj_short_name(gobj),
            bottom_gobj?gobj_short_name(bottom_gobj):""
        );
    }
    if(gobj->bottom_gobj) {
        /*
         *  En los gobj con manual start ellos suelen poner su bottom gobj.
         *  No lo consideramos un error.
         *  Ej: emailsender -> curl
         *      y luego va e intenta poner emailsender -> IOGate
         *      porque está definido con tree, y no tiene en cuenta la creación de un bottom interno.
         *
         */
        if(bottom_gobj) {
            gobj_log_warning(gobj, LOG_OPT_TRACE_STACK,
                "gobj",         "%s", gobj_full_name(gobj),
                "function",     "%s", __FUNCTION__,
                "msgset",       "%s", MSGSET_PARAMETER_ERROR,
                "msg",          "%s", "bottom_gobj already set",
                "prev_gobj",    "%s", gobj_full_name(gobj->bottom_gobj),
                "new_gobj",     "%s", gobj_full_name(bottom_gobj),
                NULL
            );
        }
        // anyway set -> NO! -> the already set has preference. New 8-10-2016
        // anyway set -> YES! -> the new set has preference. New 27-Jan-2017
        //return 0;
    }
    gobj->bottom_gobj = bottom_gobj;
    return 0;
}

/***************************************************************************
 *  Return the last bottom gobj of gobj tree.
 *  Useful when there is a stack of gobjs acting as a unit.
 *  Filled by gobj_create_tree0() function.
 ***************************************************************************/
PUBLIC hgobj gobj_last_bottom_gobj(hgobj gobj_)
{
    gobj_t * gobj = gobj_;

    if(gobj->bottom_gobj) {
        return _deep_bottom_gobj(gobj->bottom_gobj);
    } else {
        return 0;
    }
}

/***************************************************************************
 *  Return the next bottom gobj of the gobj.
 *  See gobj_last_bottom_gobj()
 ***************************************************************************/
PUBLIC hgobj gobj_bottom_gobj(hgobj gobj_)
{
    gobj_t *gobj = gobj_;
    if (!gobj) {
        return 0;
    }
    return gobj->bottom_gobj;
}

/***************************************************************************
 *
 ***************************************************************************/
PUBLIC int gobj_walk_gobj_childs(
    hgobj gobj_,
    walk_type_t walk_type,
    cb_walking_t cb_walking,
    void *user_data
) {
    gobj_t *gobj = gobj_;
    if(!gobj) {
        gobj_log_error(0, LOG_OPT_TRACE_STACK,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_PARAMETER_ERROR,
            "msg",          "%s", "gobj NULL",
            NULL
        );
        return 0;
    }

    return rc_walk_by_list(&gobj->dl_childs, walk_type, cb_walking, user_data);
}

/***************************************************************************
 *
 ***************************************************************************/
PUBLIC int gobj_walk_gobj_childs_tree(
    hgobj gobj_,
    walk_type_t walk_type,
    cb_walking_t cb_walking,
    void *user_data
) {
    gobj_t *gobj = gobj_;
    if(!gobj) {
        gobj_log_error(0, LOG_OPT_TRACE_STACK,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_PARAMETER_ERROR,
            "msg",          "%s", "gobj NULL",
            NULL
        );
        return 0;
    }

    return rc_walk_by_tree(&gobj->dl_childs, walk_type, cb_walking, user_data);
}

/***************************************************************
 *  Walking
 ***************************************************************/
PUBLIC int rc_walk_by_list(
    dl_list_t *iter,
    walk_type_t walk_type,
    cb_walking_t cb_walking,
    void *user_data
) {
    gobj_t *child;
    if(walk_type & WALK_LAST2FIRST) {
        child = dl_last(iter);
    } else {
        child = dl_first(iter);
    }

    while(child) {
        /*
         *  Get next item before calling callback.
         *  This let destroy the current item,
         *  but NOT more ones.
         */
        gobj_t *next;
        if(walk_type & WALK_LAST2FIRST) {
            next = dl_prev(child);
        } else {
            next = dl_next(child);
        }

        int ret = (cb_walking)(child, user_data);
        if(ret < 0) {
            return ret;
        }

        child = next;
    }

    return 0;
}

/***************************************************************
 *  Walking
 *  If cb_walking return negative, the iter will stop.
 *  Valid options:
 *      WALK_BYLEVEL:
 *          WALK_FIRST2LAST
 *          or
 *          WALK_LAST2FIRST
 ***************************************************************/
PRIVATE int rc_walk_by_level(
    dl_list_t *iter,
    walk_type_t walk_type,
    cb_walking_t cb_walking,
    void *user_data
) {
    /*
     *  First my childs
     */
    int ret = rc_walk_by_list(iter, walk_type, cb_walking, user_data);
    if(ret < 0) {
        return ret;
    }

    /*
     *  Now child's childs
     */
    gobj_t *child;
    if(walk_type & WALK_LAST2FIRST) {
        child = dl_last(iter);
    } else {
        child = dl_first(iter);
    }

    while(child) {
        /*
         *  Get next item before calling callback.
         *  This let destroy the current item,
         *  but NOT more ones.
         */
        gobj_t *next;
        if(walk_type & WALK_LAST2FIRST) {
            next = dl_prev(child);
        } else {
            next = dl_next(child);
        }

        dl_list_t *dl_child_list = &child->dl_childs;
        ret = rc_walk_by_level(dl_child_list, walk_type, cb_walking, user_data);
        if(ret < 0) {
            return ret;
        }

        child = next;
    }

    return ret;
}

/***************************************************************
 *  Walking
 *  If cb_walking return negative, the iter will stop.
 *                return 0, continue
 *                return positive, step current branch (only in WALK_TOP2BOTTOM)
 *  Valid options:
 *      WALK_TOP2BOTTOM
 *      or
 *      WALK_BOTTOM2TOP
 *      or
 *      WALK_BYLEVEL:
 *          WALK_FIRST2LAST
 *          or
 *          WALK_LAST2FIRST
 ***************************************************************/
PUBLIC int rc_walk_by_tree(
    dl_list_t *iter,
    walk_type_t walk_type,
    cb_walking_t cb_walking,
    void *user_data
) {
    if(walk_type & WALK_BYLEVEL) {
        return rc_walk_by_level(iter, walk_type, cb_walking, user_data);
    }

    gobj_t *child;
    if(walk_type & WALK_BOTTOM2TOP) {
        child = dl_last(iter);
    } else {
        child = dl_first(iter);
    }

    while(child) {
        /*
         *  Get next item before calling callback.
         *  This let destroy the current item,
         *  but NOT more ones.
         */
        gobj_t *next;
        if(walk_type & WALK_BOTTOM2TOP) {
            next = dl_prev(child);
        } else {
            next = dl_next(child);
        }

        if(walk_type & WALK_BOTTOM2TOP) {
            dl_list_t *dl_child_list = &child->dl_childs;
            int ret = rc_walk_by_tree(dl_child_list, walk_type, cb_walking, user_data);
            if(ret < 0) {
                return ret;
            }

            ret = (cb_walking)(child, user_data);
            if(ret < 0) {
                return ret;
            }
        } else {
            int ret = (cb_walking)(child, user_data);
            if(ret < 0) {
                return ret;
            } else if(ret == 0) {
                dl_list_t *dl_child_list = &child->dl_childs;
                ret = rc_walk_by_tree(dl_child_list, walk_type, cb_walking, user_data);
                if(ret < 0) {
                    return ret;
                }
            } else {
                // positive, continue next
            }
        }

        child = next;
    }

    return 0;
}



                    /*---------------------------------*
                     *      Info functions
                     *---------------------------------*/




/***************************************************************************
 *  Return yuno, the grandfather
 ***************************************************************************/
PUBLIC hgobj gobj_yuno(void)
{
    if(!__yuno__ || __yuno__->obflag & obflag_destroyed) {
        return 0;
    }
    return __yuno__;
}

/***************************************************************************
 *  Return name
 ***************************************************************************/
PUBLIC const char * gobj_name(hgobj hgobj)
{
    gobj_t *gobj = (gobj_t *)hgobj;
    return gobj? gobj->gobj_name: "???";
}

/***************************************************************************
 *  Return mach name. Same as gclass name.
 ***************************************************************************/
PUBLIC gclass_name_t gobj_gclass_name(hgobj hgobj)
{
    gobj_t *gobj = (gobj_t *)hgobj;
    return (gobj && gobj->gclass)? gobj->gclass->gclass_name: "???";
}

/***************************************************************************
 *  Return full name
 ***************************************************************************/
PUBLIC const char * gobj_full_name(hgobj gobj_)
{
    gobj_t *gobj = gobj_;

    if(!gobj || gobj->obflag & obflag_destroyed) {
        return "???";
    }

    if(!gobj->full_name) {
        #define SIZEBUFTEMP (8*1024)
        gobj_t *parent = gobj;
        char *bf = malloc(SIZEBUFTEMP);
        if(bf) {
            size_t ln;
            memset(bf, 0, SIZEBUFTEMP);
            while(parent) {
                char pp[256];
                const char *format;
                if(strlen(bf))
                    format = "%s`";
                else
                    format = "%s";
                snprintf(pp, sizeof(pp), format, gobj_short_name(parent));
                ln = strlen(pp);
                if(ln + strlen(bf) < SIZEBUFTEMP) {
                    memmove(bf+ln, bf, strlen(bf));
                    memmove(bf, pp, ln);
                } else {
                    break;
                }
                if(parent == parent->parent) {
                    break;
                }
                parent = parent->parent;
            }
            gobj->full_name = gobj_strdup(bf);
            free(bf);
        }

    }
    return gobj->full_name;
}

/***************************************************************************
 *  Return short name (gclass^name)
 ***************************************************************************/
PUBLIC const char * gobj_short_name(hgobj gobj_)
{
    gobj_t *gobj = gobj_;

    if(!gobj || gobj->obflag & obflag_destroyed) {
        return "???";
    }

    if(!gobj->short_name) {
        char temp[256];
        snprintf(temp, sizeof(temp),
            "%s^%s",
            gobj_gclass_name(gobj),
            gobj_name(gobj)
        );
        gobj->short_name = gobj_strdup(temp);
    }
    return gobj->short_name;
}

/***************************************************************************
 *
 ***************************************************************************/
PUBLIC json_t * gobj_global_variables(void)
{
    return json_object(); // TODO no deberían cogerse del __root__???
//    return json_pack("{s:s, s:s, s:s, s:s, s:s, s:s, s:s, s:s, s:s, s:s, s:s, s:s, s:s, s:s, s:s, s:s, s:s, s:s}",
//        "__node_owner__", __node_owner__,
//        "__realm_id__", __realm_id__,
//        "__realm_owner__", __realm_owner__,
//        "__realm_role__", __realm_role__,
//        "__realm_name__", __realm_name__,
//        "__realm_env__", __realm_env__,
//        "__yuno_id__", __yuno_id__,
//        "__yuno_role__", __yuno_role__,
//        "__yuno_name__", __yuno_name__,
//        "__yuno_tag__", __yuno_tag__,
//        "__yuno_role_plus_name__", __yuno_role_plus_name__,
//        "__yuno_role_plus_id__", __yuno_role_plus_id__,
//        "__hostname__", get_host_name(),
//#ifdef __linux__
//        "__sys_system_name__", sys.sysname,
//        "__sys_node_name__", sys.nodename,
//        "__sys_version__", sys.version,
//        "__sys_release__", sys.release,
//        "__sys_machine__", sys.machine
//#else
//        "__sys_system_name__", "",
//        "__sys_node_name__", "",
//        "__sys_version__", "",
//        "__sys_release__", "",
//        "__sys_machine__", ""
//#endif
//    );
}

/***************************************************************************
 *  Return private data pointer
 ***************************************************************************/
PUBLIC void * gobj_priv_data(hgobj gobj)
{
    return ((gobj_t *)gobj)->priv;
}

/***************************************************************************
 *  Return parent
 ***************************************************************************/
PUBLIC hgobj gobj_parent(hgobj gobj_)
{
    gobj_t *gobj = gobj_;
    if(!gobj)
        return NULL;
    return gobj->parent;
}

/***************************************************************************
 *
 ***************************************************************************/
PUBLIC BOOL gobj_is_destroying(hgobj gobj_)
{
    gobj_t *gobj = gobj_;
    if(!gobj_ || gobj->obflag & (obflag_destroyed|obflag_destroying)) {
        return TRUE;
    }
    return FALSE;
}

/***************************************************************************
 *
 ***************************************************************************/
PUBLIC BOOL gobj_is_running(hgobj gobj_)
{
    gobj_t *gobj = gobj_;
    if(!gobj_ || gobj->obflag & (obflag_destroyed|obflag_destroying)) {
        gobj_log_error(0, LOG_OPT_TRACE_STACK,
          "function",     "%s", __FUNCTION__,
          "msgset",       "%s", MSGSET_PARAMETER_ERROR,
          "msg",          "%s", "hgobj NULL or DESTROYED",
          NULL
        );
        return FALSE;
    }
    return gobj->running;
}

/***************************************************************************
 *
 ***************************************************************************/
PUBLIC BOOL gobj_is_playing(hgobj gobj_)
{
    gobj_t *gobj = gobj_;

    if(!gobj_ || gobj->obflag & (obflag_destroyed|obflag_destroying)) {
        gobj_log_error(0, LOG_OPT_TRACE_STACK,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_PARAMETER_ERROR,
            "msg",          "%s", "hgobj NULL or DESTROYED",
            NULL
        );
        return FALSE;
    }
    return gobj->playing;
}

/***************************************************************************
 *
 ***************************************************************************/
PUBLIC BOOL gobj_is_service(hgobj gobj_)
{
    gobj_t *gobj = gobj_;

    if(gobj && (gobj->gobj_flag & (gobj_flag_service))) {
        return TRUE;
    } else {
        return FALSE;
    }
}

/***************************************************************************
 *
 ***************************************************************************/
PUBLIC BOOL gobj_is_disabled(hgobj gobj)
{
    return ((gobj_t *)gobj)->disabled;
}

/***************************************************************************
 *
 ***************************************************************************/
PUBLIC BOOL gobj_is_volatil(hgobj gobj_)
{
    gobj_t *gobj = gobj_;
    if(gobj->gobj_flag & gobj_flag_volatil) {
        return TRUE;
    }
    return FALSE;
}

/***************************************************************************
 *
 ***************************************************************************/
PUBLIC BOOL gobj_is_pure_child(hgobj gobj_)
{
    gobj_t *gobj = gobj_;
    if(gobj->gobj_flag & gobj_flag_pure_child) {
        return TRUE;
    }
    return FALSE;
}




                    /*---------------------------------*
                     *      Events and States
                     *---------------------------------*/





/***************************************************************************
 *
 ***************************************************************************/
PUBLIC int gobj_send_event(
    hgobj dst_,
    gobj_event_t event,
    json_t *kw,
    hgobj src_
) {
    if(dst_ == NULL) {
        gobj_log_error(NULL, LOG_OPT_TRACE_STACK,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_PARAMETER_ERROR,
            "msg",          "%s", "hgobj dst NULL",
            "event",        "%s", event,
            NULL
        );
        KW_DECREF(kw)
        return -1;
    }

    gobj_t *src = (gobj_t *)src_;
    gobj_t *dst = (gobj_t *)dst_;
    if(dst->obflag & (obflag_destroyed|obflag_destroying)) {
        gobj_log_error(dst, LOG_OPT_TRACE_STACK,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_PARAMETER_ERROR,
            "msg",          "%s", (dst->obflag & obflag_destroyed)? "gobj DESTROYED":"gobj DESTROYING",
            "event",        "%s", event,
            NULL
        );
        KW_DECREF(kw)
        return -1;
    }

    state_t *state = dst->current_state;
    if(!state) {
        gobj_log_error(NULL, LOG_OPT_TRACE_STACK,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_INTERNAL_ERROR,
            "msg",          "%s", "current_state NULL",
            "gclass_name",  "%s", dst->gclass->gclass_name,
            "event",        "%s", event,
            NULL
        );
        KW_DECREF(kw)
        return -1;
    }

    /*----------------------------------*
     *  Find the event/action in state
     *----------------------------------*/
    BOOL tracea = is_machine_tracing(dst,event) && !is_machine_not_tracing(src);
    __inside__ ++;

    event_action_t *event_action = find_event_action(state, event);
    if(!event_action) {
        if(dst->gclass->gmt->mt_inject_event) {
            __inside__ --;
            if(tracea) {
                trace_machine("🔃 mach(%s%s^%s), st: %s, ev: %s, from(%s%s^%s)",
                    (!dst->running)?"!!":"",
                    gobj_gclass_name(dst), gobj_name(dst),
                    state->state_name,
                    event?event:"",
                    (src && !src->running)?"!!":"",
                    gobj_gclass_name(src), gobj_name(src)
                );
                if(kw) {
                    if(__trace_gobj_ev_kw__(dst)) {
                        if(json_object_size(kw)) {
                            gobj_trace_json(dst, kw, "kw send_event");
                        }
                    }
                }
            }
            return dst->gclass->gmt->mt_inject_event(dst, event, kw, src);
        }

        if(tracea) {
            trace_machine("📛 mach(%s%s^%s), st: %s, ev: %s, 📛📛EVENT NOT DEFINED📛📛",
                        (!dst->running)?"!!":"",
                gobj_gclass_name(dst), gobj_name(dst),
                state->state_name,
                event?event:""
            );
        } else {
            gobj_log_error(dst, 0,
                "msgset",       "%s", MSGSET_PARAMETER_ERROR,
                "msg",          "%s", "Event NOT DEFINED in state",
                "gclass_name",  "%s", dst->gclass->gclass_name,
                "state_name",   "%s", state->state_name,
                "event",        "%s", event,
                "src",          "%s", gobj_short_name(src),
                NULL
            );
        }

        __inside__ --;

        KW_DECREF(kw)
        return -1;
    }

    /*----------------------------------*
     *      Check AUTHZ
     *----------------------------------*/
//     if(ev_desc->authz & EV_AUTHZ_INJECT) {
//     }

    /*----------------------------------*
     *      Exec the event
     *----------------------------------*/
    if(tracea) {
        trace_machine("🔄 mach(%s%s^%s), st: %s, ev: %s%s%s, from(%s%s^%s)",
            (!dst->running)?"!!":"",
            gobj_gclass_name(dst), gobj_name(dst),
            state->state_name,
            On_Black RBlue,
            event?event:"",
            Color_Off,
            (src && !src->running)?"!!":"",
            gobj_gclass_name(src), gobj_name(src)
        );
        if(kw) {
            if(__trace_gobj_ev_kw__(dst)) {
                if(json_object_size(kw)) {
                    gobj_trace_json(dst, kw, "kw exec event :%s", event?event:"");
                }
            }
        }
    }

    /*
     *  IMPORTANT HACK
     *  Set new state BEFORE run 'action'
     *
     *  The next state is changed before executing the action.
     *  If you don’t like this behavior, set the next-state to NULL
     *  and use change_state() to change the state inside the actions.
     */
    if(event_action->next_state) {
        gobj_change_state(dst, event_action->next_state);
    }

    int ret = -1;
    if(event_action->action) {
        // Execute the action
        ret = (*event_action->action)(dst, event, kw, src);
    } else {
        // No action, there is nothing amiss!.
        KW_DECREF(kw)
    }

    if(tracea && !(dst->obflag & obflag_destroyed)) {
        trace_machine("<- mach(%s%s^%s), st: %s, ev: %s, ret: %d",
            (!dst->running)?"!!":"",
            gobj_gclass_name(dst), gobj_name(dst),
            dst->current_state->state_name,
            event?event:"",
            ret
        );
    }

    __inside__ --;

    return ret;
}

/***************************************************************************
 *
 ***************************************************************************/
PUBLIC BOOL gobj_change_state(
    hgobj hgobj,
    gobj_state_t state_name
) {
    if(hgobj == NULL) {
        gobj_log_error(NULL, LOG_OPT_TRACE_STACK,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_PARAMETER_ERROR,
            "msg",          "%s", "hgobj NULL",
            NULL
        );
        return FALSE;
    }

    gobj_t *gobj = (gobj_t *)hgobj;

    if(gobj->obflag & obflag_destroyed) {
        gobj_log_error(0, LOG_OPT_TRACE_STACK,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_PARAMETER_ERROR,
            "msg",          "%s", "gobj DESTROYED",
            NULL
        );
        return FALSE;
    }

    if(gobj->current_state->state_name == state_name) {
        return FALSE;
    }
    state_t *new_state = find_state(gobj->gclass, state_name);
    if(!new_state) {
        gobj_log_error(NULL, LOG_OPT_TRACE_STACK,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_INTERNAL_ERROR,
            "msg",          "%s", "state unknown",
            "gclass_name",  "%s", gobj->gclass->gclass_name,
            "state_name",   "%s", state_name,
            NULL
        );
        return FALSE;
    }
    gobj->last_state = gobj->current_state;
    gobj->current_state = new_state;

    BOOL tracea = is_machine_tracing(gobj, NULL);
    BOOL tracea_states = __trace_gobj_states__(gobj)?TRUE:FALSE;
    if(tracea || tracea_states) {
        trace_machine("🔀🔀 mach(%s%s^%s), st(%s%s%s)",
            (!gobj->running)?"!!":"",
            gobj_gclass_name(gobj), gobj_name(gobj),
            On_Black RGreen,
            gobj_current_state(gobj),
            Color_Off
        );
    }

    json_t *kw_st = json_object();
    json_object_set_new(
        kw_st,
        "previous_state",
        json_string(gobj->last_state->state_name)
    );
    json_object_set_new(
        kw_st,
        "current_state",
        json_string(gobj->current_state->state_name)
    );

    if(gobj->gclass->gmt->mt_state_changed) {
        gobj->gclass->gmt->mt_state_changed(gobj, EV_STATE_CHANGED, kw_st);
    } else {
        gobj_publish_event(gobj, EV_STATE_CHANGED, kw_st);
    }

    return TRUE;
}

/***************************************************************************
 *
 ***************************************************************************/
PUBLIC gobj_state_t gobj_current_state(hgobj hgobj)
{
    if(hgobj == NULL) {
        gobj_log_error(NULL, LOG_OPT_TRACE_STACK,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_PARAMETER_ERROR,
            "msg",          "%s", "hgobj NULL",
            NULL
        );
        return "";
    }
    gobj_t *gobj = (gobj_t *)hgobj;

    return gobj->current_state->state_name;
}

/***************************************************************************
 *
 ***************************************************************************/
PUBLIC BOOL gobj_has_input_event(hgobj gobj_, gobj_event_t event)
{
    if(gobj_ == NULL) {
        gobj_log_error(NULL, LOG_OPT_TRACE_STACK,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_PARAMETER_ERROR,
            "msg",          "%s", "hgobj NULL",
            "event",        "%s", event,
            NULL
        );
        return FALSE;
    }
    gobj_t *gobj = (gobj_t *)gobj_;
    if(find_event_action(gobj->current_state, event)) {
        return TRUE;
    }

    return FALSE;
}

/***************************************************************************
 *
 ***************************************************************************/
PUBLIC event_type_t *gobj_event_type(hgobj gobj_, gobj_event_t event, event_flag_t event_flag)
{
    if(gobj_ == NULL) {
        gobj_log_error(NULL, LOG_OPT_TRACE_STACK,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_PARAMETER_ERROR,
            "msg",          "%s", "hgobj NULL",
            "event",        "%s", event,
            NULL
        );
        return FALSE;
    }
    gobj_t *gobj = (gobj_t *)gobj_;

    event_t *event_ = dl_first(&gobj->gclass->dl_events);
    while(event_) {
        if(event_->event_type.event && event_->event_type.event == event) {
            if(event_flag && (event_->event_type.event_flag & event_flag)) {
                return &event_->event_type;
            }
        }
        event_ = dl_next(event_);
    }

    /*
     *  Check global (gobj) output events
     */
    event_ = dl_first(&dl_global_event_types);
    while(event_) {
        if(event_->event_type.event && event_->event_type.event == event) {
            if(event_flag && (event_->event_type.event_flag & event_flag)) {
                return &event_->event_type;
            }
        }
        event_ = dl_next(event_);
    }

    return NULL;
}




                    /*------------------------------------*
                     *      Publication/Subscriptions
                     *------------------------------------*/




/*
Schema of subs (subscription)
=============================
"publisher":            (pointer)int    // publisher gobj
"subscriber:            (pointer)int    // subscriber gobj
"event":                (pointer)str    // event name subscribed
"subs_flag":            int             // subscription flag. See subs_flag_t
"__config__":           json            // subscription config.
"__global__":           json            // global event kw. This json is extended with publishing kw.
"__local__":            json            // local event kw. The keys in this json are removed from publishing kw.

 */

typedef enum {
    __hard_subscription__   = 0x00000001,
    __own_event__           = 0x00000002,   // If gobj_send_event return -1 don't continue publishing
} subs_flag_t;

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE json_t * _create_subscription(
    gobj_t * publisher,
    gobj_event_t event,
    json_t *kw, // not owned
    gobj_t * subscriber)
{
    json_t *subs = json_object();
    json_object_set_new(subs, "event", json_integer((json_int_t)(size_t)event));
    json_object_set_new(subs, "subscriber", json_integer((json_int_t)(size_t)subscriber));
    json_object_set_new(subs, "publisher", json_integer((json_int_t)(size_t)publisher));
    subs_flag_t subs_flag = 0;

    if(kw) {
        json_t *__config__ = kw_get_dict(publisher, kw, "__config__", 0, 0);
        json_t *__global__ = kw_get_dict(publisher, kw, "__global__", 0, 0);
        json_t *__local__ = kw_get_dict(publisher, kw, "__local__", 0, 0);
        const char *__service__ = kw_get_str(publisher, kw, "__service__", 0, 0);

        if(__global__) {
            json_t *kw_clone = json_deep_copy(__global__);
            json_object_set_new(subs, "__global__", kw_clone);
        }
        if(__config__) {
            json_t *kw_clone = json_deep_copy(__config__);
            json_object_set_new(subs, "__config__", kw_clone);

            if(kw_has_key(kw_clone, "__hard_subscription__")) {
                BOOL hard_subscription = kw_get_bool(
                    publisher, kw_clone, "__hard_subscription__", 0, 0
                );
                json_object_del(kw_clone, "__hard_subscription__");
                if(hard_subscription) {
                    subs_flag |= __hard_subscription__;
                }
            }
            if(kw_has_key(kw_clone, "__own_event__")) {
                BOOL own_event= kw_get_bool(publisher, kw_clone, "__own_event__", 0, 0);
                json_object_del(kw_clone, "__own_event__");
                if(own_event) {
                    subs_flag |= __own_event__;
                }
            }
        }
        if(__local__) {
            json_t *kw_clone = json_deep_copy(__local__);
            json_object_set_new(subs, "__local__", kw_clone);
        }
        if(__service__) { // TODO check if it's used
            json_object_set_new(subs, "__service__", json_string(__service__));
        }
    }
    json_object_set_new(subs, "subs_flag", json_integer((json_int_t)subs_flag));

    return subs;
}

/***************************************************************************
 *  Delete subscription
 ***************************************************************************/
PRIVATE int _delete_subscription(
    gobj_t * gobj,
    json_t *subs, // not owned
    BOOL force,
    BOOL not_inform
) {
    gobj_t *publisher = (gobj_t *)(size_t)kw_get_int(gobj, subs, "publisher", 0, KW_REQUIRED);
    gobj_t *subscriber = (gobj_t *)(size_t)kw_get_int(gobj, subs, "subscriber", 0, KW_REQUIRED);
    gobj_event_t event = (gobj_event_t)(size_t)kw_get_int(gobj, subs, "event", 0, KW_REQUIRED);
    subs_flag_t subs_flag = (subs_flag_t)kw_get_int(gobj, subs, "subs_flag", 0, KW_REQUIRED);
    BOOL hard_subscription = (subs_flag & __hard_subscription__)?1:0;

    /*-------------------------------*
     *  Check if hard subscription
     *-------------------------------*/
    if(hard_subscription) {
        if(!force) {
            return -1;
        }
    }

    /*-----------------------------*
     *          Trace
     *-----------------------------*/
    if(__trace_gobj_subscriptions__(subscriber) || __trace_gobj_subscriptions__(publisher)
    ) {
        trace_machine("💜💜👎 %s unsubscribing event '%s' of %s",
            gobj_full_name(subscriber),
            event?event:"",
            gobj_full_name(publisher)
        );
        gobj_trace_json(gobj, subs, "subs");
    }

    /*------------------------------------------------*
     *      Inform (BEFORE) of subscription removed
     *------------------------------------------------*/
    if(!(publisher->obflag & obflag_destroyed)) {
        if(!not_inform) {
            if(publisher->gclass->gmt->mt_subscription_deleted) {
                publisher->gclass->gmt->mt_subscription_deleted(
                    publisher,
                    subs
                );
            }
        }
    }

    /*--------------------------------*
     *      Delete subscription
     *--------------------------------*/
    int idx = kw_find_json_in_list(publisher->dl_subscriptions, subs);
    if(idx >= 0) {
        if(json_array_remove(publisher->dl_subscriptions, (size_t)idx)<0) {
            gobj_log_error(gobj, LOG_OPT_TRACE_STACK,
                "function",     "%s", __FUNCTION__,
                "msgset",       "%s", MSGSET_INTERNAL_ERROR,
                "msg",          "%s", "json_array_remove() subscriptions FAILED",
                NULL
            );
        }
    } else {
        gobj_log_error(gobj, LOG_OPT_TRACE_STACK,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_INTERNAL_ERROR,
            "msg",          "%s", "subscription in publisher not found",
            NULL
        );
        gobj_trace_json(gobj, subs, "subscription in publisher not found");
    }

    idx = kw_find_json_in_list(subscriber->dl_subscribings, subs);
    if(idx >= 0) {
        if(json_array_remove(subscriber->dl_subscribings, (size_t)idx)<0) {
            gobj_log_error(gobj, LOG_OPT_TRACE_STACK,
                "function",     "%s", __FUNCTION__,
                "msgset",       "%s", MSGSET_INTERNAL_ERROR,
                "msg",          "%s", "json_array_remove() subscribings FAILED",
                NULL
            );
        }
    } else {
        gobj_log_error(gobj, LOG_OPT_TRACE_STACK,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_INTERNAL_ERROR,
            "msg",          "%s", "subscription in subscriber not found",
            NULL
        );
        gobj_trace_json(gobj, subs, "subscription in subscriber not found");
    }

//    if((int)subs->refcount > 0) {
//        gobj_log_error(gobj, 0,
//            "function",     "%s", __FUNCTION__,
//            "msgset",       "%s", MSGSET_INTERNAL_ERROR,
//            "msg",          "%s", "subscription NOT DELETED",
//            NULL
//        );
//        gobj_trace_json(gobj, subs, "subscription NOT DELETED");
//    }
    return 0;
}

/***************************************************************************
 *  Return a iter of subscriptions (sdata),
 *  filtering by matching:
 *      event,kw (__config__, __global__, __local__, __filter__),subscriber
 ***************************************************************************/
PRIVATE json_t * _find_subscription(
    json_t *dl_subs,
    gobj_t *publisher,
    gobj_event_t event,
    json_t *kw, // owned
    gobj_t *subscriber,
    BOOL full
) {
    json_t *__config__ = kw_get_dict(0, kw, "__config__", 0, 0);
    json_t *__global__ = kw_get_dict(0, kw, "__global__", 0, 0);
    json_t *__local__ = kw_get_dict(0, kw, "__local__", 0, 0);

    json_t *iter = json_array();

    size_t idx; json_t *subs;
    json_array_foreach(dl_subs, idx, subs) {
        BOOL match = TRUE;

        if(publisher || full) {
            gobj_t *publisher_ = (gobj_t *)(size_t)kw_get_int(0, subs, "publisher", 0, KW_REQUIRED);
            if(publisher != publisher_) {
                match = FALSE;
            }
        }

        if(subscriber || full) {
            gobj_t *subscriber_ = (gobj_t *)(size_t)kw_get_int(0, subs, "subscriber", 0, KW_REQUIRED);
            if(subscriber != subscriber_) {
                match = FALSE;
            }
        }

        if(event || full) {
            gobj_event_t event_ = (gobj_event_t)(size_t)kw_get_int(0, subs, "event", 0, KW_REQUIRED);
            if(event != event_) {
                match = FALSE;
            }
        }

        if(__config__ || full) {
            json_t *kw_config = kw_get_dict(0, subs, "__config__", 0, 0);
            if(kw_config) {
                if(!json_equal(kw_config, __config__)) {
                    match = FALSE;
                }
            } else if(__config__) {
                match = FALSE;
            }
        }
        if(__global__ || full) {
            json_t *kw_global = kw_get_dict(0, subs, "__global__", 0, 0);
            if(kw_global) {
                if(!json_equal(kw_global, __global__)) {
                    match = FALSE;
                }
            } else if(__global__) {
                match = FALSE;
            }
        }
        if(__local__ || full) {
            json_t *kw_local = kw_get_dict(0, subs, "__local__", 0, 0);
            if(kw_local) {
                if(!json_equal(kw_local, __local__)) {
                    match = FALSE;
                }
            } else if(__local__) {
                match = FALSE;
            }
        }

        if(match) {
            json_array_append(iter, subs);
        }
    }

    KW_DECREF(kw)
    return iter;
}

/***************************************************************************
 *  Subscribe to an event
 *
 *  event is only one event (not a string list like ginsfsm).
 *
 *  See .h
 ***************************************************************************/
PUBLIC json_t *gobj_subscribe_event( // return not yours
    hgobj publisher_,
    gobj_event_t event,
    json_t *kw,
    hgobj subscriber_)
{
    gobj_t * publisher = publisher_;
    gobj_t * subscriber = subscriber_;

    /*---------------------*
     *  Check something
     *---------------------*/
    if(!publisher) {
        gobj_log_error(0, LOG_OPT_TRACE_STACK,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_PARAMETER_ERROR,
            "msg",          "%s", "publisher NULL",
            "event",        "%s", event,
            NULL
        );
        JSON_DECREF(kw)
        return 0;
    }
    if(!subscriber) {
        gobj_log_error(publisher, LOG_OPT_TRACE_STACK,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_PARAMETER_ERROR,
            "msg",          "%s", "subscriber NULL",
            "event",        "%s", event,
            NULL
        );
        JSON_DECREF(kw)
        return 0;
    }

    /*--------------------------------------------------------------*
     *  Event must be in output event list
     *  You can avoid this with gcflag_no_check_output_events flag
     *--------------------------------------------------------------*/
    if(!empty_string(event)) {
        event_type_t *ev = gobj_event_type(publisher, event, EVF_OUTPUT_EVENT|EVF_SYSTEM_EVENT);
        if(!ev) {
            if(!(publisher->gclass->gclass_flag & gcflag_no_check_output_events)) {
                gobj_log_error(publisher, 0,
                    "function",     "%s", __FUNCTION__,
                    "msgset",       "%s", MSGSET_PARAMETER_ERROR,
                    "msg",          "%s", "event NOT in output event list",
                    "event",        "%s", event,
                    NULL
                );
                KW_DECREF(kw)
                return 0;
            }
        }
    }

    /*-------------------------------------------------*
     *  Check AUTHZ
     *-------------------------------------------------*/

    /*------------------------------*
     *  Find repeated subscription
     *------------------------------*/
    json_t *dl_subs = _find_subscription(
        publisher->dl_subscriptions,
        publisher,
        event,
        json_incref(kw),
        subscriber,
        TRUE
    );
    if(json_array_size(dl_subs) > 0) {
        gobj_log_error(publisher, LOG_OPT_TRACE_STACK,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_PARAMETER_ERROR,
            "msg",          "%s", "subscription(s) REPEATED, will be deleted and override",
            "event",        "%s", event,
            "kw",           "%j", kw,
            "publisher",    "%s", gobj_full_name(publisher),
            "subscriber",   "%s", gobj_full_name(subscriber),
            NULL
        );
        gobj_trace_json(publisher, dl_subs, "subscription(s) REPEATED, will be deleted and override");
        gobj_unsubscribe_list(json_incref(dl_subs), FALSE);
    }
    JSON_DECREF(dl_subs)

    /*-----------------------------*
     *  Create subscription
     *-----------------------------*/
    json_t *subs = _create_subscription(
        publisher,
        event,
        kw,  // not owned
        subscriber
    );
    if(!subs) {
        gobj_log_error(publisher, LOG_OPT_TRACE_STACK,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_PARAMETER_ERROR,
            "msg",          "%s", "_create_subscription() FAILED",
            "event",        "%s", event,
            "publisher",    "%s", gobj_full_name(publisher),
            "subscriber",   "%s", gobj_full_name(subscriber),
            NULL
        );
        JSON_DECREF(kw)
        return 0;
    }

    if(json_array_append(publisher->dl_subscriptions, subs)<0) {
        gobj_log_error(publisher, LOG_OPT_TRACE_STACK,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_INTERNAL_ERROR,
            "msg",          "%s", "json_array_append(subscriptions) FAILED",
            "event",        "%s", event,
            "publisher",    "%s", gobj_full_name(publisher),
            "subscriber",   "%s", gobj_full_name(subscriber),
            NULL
        );
    }
    if(json_array_append(subscriber->dl_subscribings, subs)<0) {
        gobj_log_error(publisher, LOG_OPT_TRACE_STACK,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_INTERNAL_ERROR,
            "msg",          "%s", "json_array_append(subscribings) FAILED",
            "event",        "%s", event,
            "publisher",    "%s", gobj_full_name(publisher),
            "subscriber",   "%s", gobj_full_name(subscriber),
            NULL
        );
    }

    /*-----------------------------*
     *          Trace
     *-----------------------------*/
    if(__trace_gobj_subscriptions__(subscriber) || __trace_gobj_subscriptions__(publisher)
    ) {
        trace_machine("💜💜👍 %s subscribing event '%s' of %s",
            gobj_full_name(subscriber),
            event?event:"",
            gobj_full_name(publisher)
        );
        if(kw) {
            if(1 || __trace_gobj_ev_kw__(subscriber) || __trace_gobj_ev_kw__(publisher)) {
                if(json_object_size(kw)) {
                    gobj_trace_json(publisher, kw, "subscribing event");
                }
            }
        }
    }

    /*--------------------------------*
     *      Inform new subscription
     *--------------------------------*/
    if(publisher->gclass->gmt->mt_subscription_added) {
        int result = publisher->gclass->gmt->mt_subscription_added(
            publisher,
            subs
        );
        if(result < 0) {
            _delete_subscription(publisher, subs, TRUE, TRUE);
            subs = 0;
        }
    }

    json_decref(subs);
    json_decref(kw);
    return subs;
}

/***************************************************************************
 *
 ***************************************************************************/
PUBLIC int gobj_unsubscribe_event(
    hgobj publisher_,
    gobj_event_t event,
    json_t *kw,
    hgobj subscriber_)
{
    gobj_t *publisher = publisher_;
    gobj_t *subscriber = subscriber_;

    /*---------------------*
     *  Check something
     *---------------------*/
    if(!publisher) {
        gobj_log_error(0, LOG_OPT_TRACE_STACK,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_PARAMETER_ERROR,
            "msg",          "%s", "publisher NULL",
            "event",        "%s", event,
            NULL
        );
        KW_DECREF(kw)
        return -1;
    }
    if(!subscriber) {
        gobj_log_error(publisher, LOG_OPT_TRACE_STACK,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_PARAMETER_ERROR,
            "msg",          "%s", "subscriber NULL",
            "event",        "%s", event,
            NULL
        );
        KW_DECREF(kw)
        return -1;
    }

    /*-----------------------------*
     *      Find subscription
     *-----------------------------*/
    KW_INCREF(kw);
    json_t *dl_subs = _find_subscription(
        publisher->dl_subscriptions,
        publisher,
        event,
        kw,
        subscriber,
        TRUE
    );
    int deleted = 0;

    size_t idx; json_t *subs;
    json_array_foreach(dl_subs, idx, subs) {
        _delete_subscription(publisher, subs, FALSE, FALSE);
        deleted++;
    }

    if(!deleted) {
        gobj_log_error(publisher, LOG_OPT_TRACE_STACK,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_PARAMETER_ERROR,
            "msg",          "%s", "No subscription found",
            "event",        "%s", event,
            "publisher",    "%s", gobj_full_name(publisher),
            "subscriber",   "%s", gobj_full_name(subscriber),
            NULL
        );
        gobj_trace_json(publisher, kw, "No subscription found");
    }

    JSON_DECREF(dl_subs)
    KW_DECREF(kw)
    return 0;
}

/***************************************************************************
 *  Unsubscribe a list of subscription hsdata
 ***************************************************************************/
PUBLIC int gobj_unsubscribe_list(
    json_t *dl_subs, // owned
    BOOL force  // delete hard_subscription subs too
)
{
    size_t idx; json_t *subs=0;
    json_array_foreach(dl_subs, idx, subs) {
        _delete_subscription(0, subs, force, FALSE);
    }
    JSON_DECREF(dl_subs)
    return 0;
}

/***************************************************************************
 *  Return a iter of subscriptions (sdata) in the publisher gobj,
 *  filtering by matching: event,kw (__config__, __global__, __local__, __filter__),subscriber
 *  Free return with rc_free_iter(iter, TRUE, FALSE);


 *  gobj_find_subscriptions()
 *  Return the list of subscriptions of the publisher gobj,
 *  filtering by matching: event,kw (__config__, __global__, __local__, __filter__),subscriber

    USO
    ---
    TO resend subscriptions in ac_identity_card_ack() in c_ievent_cli.c

        PRIVATE int resend_subscriptions(hgobj gobj)
        {
            dl_list_t *dl_subs = gobj_find_subscriptions(gobj, 0, 0, 0);

            rc_instance_t *i_subs; hsdata subs;
            i_subs = rc_first_instance(dl_subs, (rc_resource_t **)&subs);
            while(i_subs) {
                send_remote_subscription(gobj, subs);
                i_subs = rc_next_instance(i_subs, (rc_resource_t **)&subs);
            }
            rc_free_iter(dl_subs, TRUE, FALSE);
            return 0;
        }

        que son capturadas en:

             PRIVATE int mt_subscription_added(
                hgobj gobj,
                hsdata subs)
            {
                if(!gobj_in_this_state(gobj, "ST_SESSION")) {
                    // on_open will send all subscriptions
                    return 0;
                }
                return send_remote_subscription(gobj, subs);
            }


    EN mt_stop de c_counter.c (???)

        PRIVATE int mt_stop(hgobj gobj)
        {
            PRIVATE_DATA *priv = gobj_priv_data(gobj);

            gobj_unsubscribe_list(gobj_find_subscriptions(gobj, 0, 0, 0), TRUE, FALSE);

            clear_timeout(priv->timer);
            if(gobj_is_running(priv->timer))
                gobj_stop(priv->timer);
            return 0;
        }


 ***************************************************************************/
PUBLIC json_t * gobj_find_subscriptions(
    hgobj publisher_,
    gobj_event_t event,
    json_t *kw,
    hgobj subscriber)
{
    gobj_t * publisher = publisher_;
    return _find_subscription(
        publisher->dl_subscriptions,
        publisher_,
        event,
        kw,
        subscriber,
        FALSE
    );
}

/***************************************************************************
 *  Return a iter of subscribings (sdata) of the subcriber gobj,
 *  filtering by matching: event,kw (__config__, __global__, __local__, __filter__), publisher
 *  Free return with rc_free_iter(iter, TRUE, FALSE);
 *
 *  gobj_find_subscribings()
 *  Return a list of subscribings of the subscriber gobj,
 *  filtering by matching: event,kw (__config__, __global__, __local__, __filter__), publisher
 *
 *

    USO
    ----
     TO  Delete external subscriptions in c_ievent_src en ac_on_close()
        PRIVATE int ac_on_close(hgobj gobj,  gobj_event_t event, json_t *kw, hgobj src)
            json_t *kw2match = json_object();
            kw_set_dict_value(
                kw2match,
                "__local__`__subscription_reference__",
                json_integer((json_int_t)(size_t)gobj)
            );

            dl_list_t * dl_s = gobj_find_subscribings(gobj, 0, kw2match, 0);
            gobj_unsubscribe_list(dl_s, TRUE, FALSE);

        que son creadas en ac_on_message() en
             *   Dispatch event
            if(strcasecmp(msg_type, "__subscribing__")==0) {
                 *  It's a external subscription

                 *   Protect: only public events
                if(!gobj_event_in_output_event_list(gobj_service, iev_event, EVF_PUBLIC_EVENT)) {
                    char temp[256];
                    snprintf(temp, sizeof(temp),
                        "SUBSCRIBING event ignored, not in output_event_list or not PUBLIC event, check service '%s'",
                        iev_dst_service
                    );
                    log_error(0,
                        "gobj",         "%s", gobj_full_name(gobj),
                        "function",     "%s", __FUNCTION__,
                        "msgset",       "%s", MSGSET_PARAMETER_ERROR,
                        "msg",          "%s", temp,
                        "service",      "%s", iev_dst_service,
                        "gobj_service", "%s", gobj_short_name(gobj_service),
                        "event",        "%s", iev_event,
                        NULL
                    );
                    KW_DECREF(iev_kw);
                    KW_DECREF(kw);
                    return -1;
                }

                // Set locals to remove on publishing
                kw_set_subdict_value(
                    iev_kw,
                    "__local__", "__subscription_reference__",
                    json_integer((json_int_t)(size_t)gobj)
                );
                kw_set_subdict_value(iev_kw, "__local__", "__temp__", json_null());

                // Prepare the return of response
                json_t *__md_iev__ = kw_get_dict(iev_kw, "__md_iev__", 0, 0);
                if(__md_iev__) {
                    KW_INCREF(iev_kw);
                    json_t *kw3 = msg_iev_answer(
                        gobj,
                        iev_kw,
                        0,
                        "__publishing__"
                    );
                    json_object_del(iev_kw, "__md_iev__");
                    json_t *__global__ = kw_get_dict(iev_kw, "__global__", 0, 0);
                    if(__global__) {
                        json_object_update_new(__global__, kw3);
                    } else {
                        json_object_set_new(iev_kw, "__global__", kw3);
                    }
                }

                gobj_subscribe_event(gobj_service, iev_event, iev_kw, gobj);

 ***************************************************************************/
PUBLIC json_t *gobj_find_subscribings(
    hgobj subscriber_,
    gobj_event_t event,
    json_t *kw,             // kw (__config__, __global__, __local__, __filter__)
    hgobj publisher
)
{
    gobj_t * subscriber = subscriber_;
    return _find_subscription(
        subscriber->dl_subscribings,
        publisher,
        event,
        kw,
        subscriber,
        FALSE
    );
}

/***************************************************************************
 *  Return the number of sent events
 ***************************************************************************/
PUBLIC int gobj_publish_event(
    hgobj publisher_,
    gobj_event_t event,
    json_t *kw)
{
    gobj_t * publisher = publisher_;

    if(!kw) {
        kw = json_object();
    }

    /*---------------------*
     *  Check something
     *---------------------*/
    if(!publisher) {
        gobj_log_error(0, LOG_OPT_TRACE_STACK,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_PARAMETER_ERROR,
            "msg",          "%s", "gobj NULL",
            NULL
        );
        KW_DECREF(kw)
        return 0;
    }
    if(publisher->obflag & obflag_destroyed) {
        gobj_log_error(0, LOG_OPT_TRACE_STACK,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_PARAMETER_ERROR,
            "msg",          "%s", "gobj DESTROYED",
            NULL
        );
        KW_DECREF(kw)
        return 0;
    }
    if(empty_string(event)) {
        gobj_log_error(publisher, LOG_OPT_TRACE_STACK,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_PARAMETER_ERROR,
            "msg",          "%s", "event EMPTY",
            NULL
        );
        KW_DECREF(kw)
        return 0;
    }

    /*--------------------------------------------------------------*
     *  Event must be in output event list
     *  You can avoid this with gcflag_no_check_output_events flag
     *--------------------------------------------------------------*/
    event_type_t *ev = gobj_event_type(publisher, event, EVF_OUTPUT_EVENT|EVF_SYSTEM_EVENT);
    if(!ev) {
        if(!(publisher->gclass->gclass_flag & gcflag_no_check_output_events)) {
            gobj_log_error(publisher, 0,
                "function",     "%s", __FUNCTION__,
                "msgset",       "%s", MSGSET_PARAMETER_ERROR,
                "msg",          "%s", "event NOT in output event list",
                "event",        "%s", event,
                NULL
            );
            KW_DECREF(kw)
            return 0;
        }
    }

    BOOL tracea = is_machine_tracing(publisher, event) &&
        !is_machine_not_tracing(publisher) &&
        __trace_gobj_subscriptions__(publisher);
    if(tracea) {
        trace_machine("🔝🔝 mach(%s%s^%s), st: %s, ev: %s%s%s",
            (!publisher->running)?"!!":"",
            gobj_gclass_name(publisher), gobj_name(publisher),
            publisher->current_state->state_name,
            On_Black BBlue,
            event?event:"",
            Color_Off
        );
        if(__trace_gobj_ev_kw__(publisher)) {
            if(json_object_size(kw)) {
                gobj_trace_json(publisher, kw, "kw publish event %s", event?event:"");
            }
        }
    }

    /*----------------------------------------------------------*
     *  Own publication method
     *  Return:
     *     -1  broke                        -> return
     *      0  continue without publish     -> return
     *      1  publish and continue         -> continue below
     *----------------------------------------------------------*/
    if(publisher->gclass->gmt->mt_publish_event) {
        int topublish = publisher->gclass->gmt->mt_publish_event(
            publisher,
            event,
            kw  // not owned
        );
        if(topublish<=0) {
            KW_DECREF(kw)
            return 0;
        }
    }

    /*--------------------------------------------------------------*
     *      Default publication method
     *--------------------------------------------------------------*/
    json_t *dl_subs = json_copy(publisher->dl_subscriptions); // Protect to inside deleted subs
    int sent_count = 0;
    json_t *subs; size_t idx;
    json_array_foreach(dl_subs, idx, subs) {
        /*-------------------------------------*
         *  Pre-filter
         *  kw NOT owned! you can modify the publishing kw
         *  Return:
         *     -1  broke
         *      0  continue without publish
         *      1  continue to publish
         *-------------------------------------*/
        if(publisher->gclass->gmt->mt_publication_pre_filter) {
            int topublish = publisher->gclass->gmt->mt_publication_pre_filter(
                publisher,
                subs,
                event,
                kw  // not owned
            );
            if(topublish<0) {
                break;
            } else if(topublish==0) {
                continue;
            }
        }
        gobj_t *subscriber = (gobj_t *)(size_t)kw_get_int(
            publisher, subs, "subscriber", 0, KW_REQUIRED
        );
        if(!(subscriber && !(subscriber->obflag & (obflag_destroying|obflag_destroyed)))) {
            continue;
        }

        /*
         *  Check if event null or event in event_list
         */
        subs_flag_t subs_flag = (subs_flag_t)kw_get_int(publisher, subs, "subs_flag", 0, KW_REQUIRED);
        gobj_event_t event_ = (gobj_event_t)(size_t)kw_get_int(
            publisher, subs, "event", 0, KW_REQUIRED
        );
        if(!event_ || event_ == event) {
            json_t *__global__ = kw_get_dict(publisher, subs, "__global__", 0, 0);
            json_t *__local__ = kw_get_dict(publisher, subs, "__local__", 0, 0);

            json_t *kw2publish = kw_incref(kw);

            /*-------------------------------------*
             *  User filter method or filter parameter
             *  Return:
             *     -1  (broke),
             *      0  continue without publish,
             *      1  continue and publish
             *-------------------------------------*/
            int topublish = 1;
            if(publisher->gclass->gmt->mt_publication_filter) {
                topublish = publisher->gclass->gmt->mt_publication_filter(
                    publisher,
                    event,
                    kw2publish,  // not owned
                    subscriber
                );
            }

            if(topublish<0) {
                KW_DECREF(kw2publish);
                break;
            } else if(topublish==0) {
                /*
                 *  Must not be published
                 *  Next subs
                 */
                KW_DECREF(kw2publish);
                continue;
            }

            /*
             *  Check if System event: don't send if subscriber has not it
             */
            if(event == EV_STATE_CHANGED) {
                if(!gobj_has_input_event(subscriber, event)) {
                    KW_DECREF(kw2publish);
                    continue;
                }
            }

            /*
             *  Remove local keys
             */
            if(__local__) {
                kw_pop(kw2publish,
                    __local__ // not owned
                );
            }

            /*
             *  Add global keys
             */
            if(__global__) {
                json_object_update(kw2publish, __global__);
            }

            /*
             *  Send event
             */
            if(tracea) {
                trace_machine("🔝🔄 mach(%s%s), ev: %s, from(%s%s)",
                    (!subscriber->running)?"!!":"",
                    gobj_short_name(subscriber),
                    event?event:"",
                    (publisher && !publisher->running)?"!!":"",
                    gobj_short_name(publisher)
                );
                if(__trace_gobj_ev_kw__(publisher)) {
                    if(json_object_size(kw2publish)) {
                        gobj_trace_json(publisher, kw2publish, "kw publish send event");
                    }
                }
            }

            int ret = gobj_send_event(
                subscriber,
                event,
                kw2publish,
                publisher
            );
            if(ret < 0 && (subs_flag & __own_event__)) {
                sent_count = -1; // Return of -1 indicates that someone owned the event
                break;
            }
            sent_count++;

            if(publisher->obflag & (obflag_destroying|obflag_destroyed)) {
                /*
                 *  break all, self publisher deleted
                 */
                break;
            }
        }
    }

    if(!sent_count) {
        if(!ev || !(ev->event_flag & EVF_NO_WARN_SUBS)) {
            gobj_log_warning(publisher, 0,
                "msgset",       "%s", MSGSET_INFO,
                "msg",          "%s", "Publish event WITHOUT subscribers",
                "event",        "%s", event,
                NULL
            );
            if(__trace_gobj_ev_kw__(publisher)) {
                if(json_object_size(kw)) {
                    gobj_trace_json(publisher, kw, "Publish event WITHOUT subscribers");
                }
            }
        }
    }

    JSON_DECREF(dl_subs)
    KW_DECREF(kw)
    return sent_count;
}




                    /*---------------------------------*
                     *      Trace functions
                     *---------------------------------*/




/****************************************************************************
 *  Return gobj trace level
 ****************************************************************************/
PUBLIC uint32_t gobj_trace_level(hgobj gobj_)
{
    gobj_t * gobj = gobj_;

    if(__deep_trace__) {
        return (uint32_t)-1;
    }
    uint32_t bitmask = __global_trace_level__;
    if(gobj) {
        if(!gobj->gclass->jn_trace_filter || !gobj->jn_attrs) {
            bitmask |= gobj->trace_level;
            if(gobj->gclass) {
                bitmask |= gobj->gclass->trace_level;
            }
        } else {
            const char *attr; json_t *jn_list_values;
            json_object_foreach(gobj->gclass->jn_trace_filter, attr, jn_list_values) {
                size_t idx; json_t *jn_value;
                json_array_foreach(jn_list_values, idx, jn_value) {
                    const char *value = json_string_value(jn_value);
                    // TODO consider other types than str like int
                    const char *value_ = gobj_read_str_attr(gobj, attr);
                    if(value && value_ && strcmp(value, value_)==0) {
                        bitmask |= gobj->trace_level;
                        if(gobj->gclass) {
                            bitmask |= gobj->gclass->trace_level;
                        }
                        break;
                    }
                }
            }
        }
    }

    return bitmask;
}

/****************************************************************************
 *  Return gobj no trace level
 ****************************************************************************/
PUBLIC uint32_t gobj_trace_no_level(hgobj gobj_)
{
    gobj_t * gobj = gobj_;

    uint32_t bitmask = __global_trace_no_level__;

    if(gobj) {
        bitmask |= gobj->no_trace_level;
        if (gobj->gclass) {
            bitmask |= gobj->gclass->no_trace_level;
        }
    }
    return bitmask;
}

/****************************************************************************
 *  Convert 32 bit to string level
 ****************************************************************************/
json_t *bit2level(  // TODO es PRIVATE, para cuando hagas las funciones de consulta
    const trace_level_t *internal_trace_level,
    const trace_level_t *user_trace_level,
    uint32_t bit)
{
    json_t *jn_list = json_array();
    for(int i=0; i<16; i++) {
        if(!internal_trace_level[i].name) {
            break;
        }
        uint32_t bitmask = 1 << i;
        bitmask <<= 16;
        if(bit & bitmask) {
            json_array_append_new(
                jn_list,
                json_string(internal_trace_level[i].name)
            );
        }
    }
    if(user_trace_level) {
        for(int i=0; i<16; i++) {
            if(!user_trace_level[i].name) {
                break;
            }
            uint32_t bitmask = 1 << i;
            if(bit & bitmask) {
                json_array_append_new(
                    jn_list,
                    json_string(user_trace_level[i].name)
                );
            }
        }
    }
    return jn_list;
}

/****************************************************************************
 *  Convert string level to 32 bit
 ****************************************************************************/
PRIVATE uint32_t level2bit(
    const trace_level_t *internal_trace_level,
    const trace_level_t *user_trace_level,
    const char *level
)
{
    for(int i=0; i<16; i++) {
        if(!internal_trace_level[i].name) {
            break;
        }
        if(strcasecmp(level, internal_trace_level[i].name)==0) {
            uint32_t bitmask = 1 << i;
            bitmask <<= 16;
            return bitmask;
        }
    }
    if(user_trace_level) {
        for(int i=0; i<16; i++) {
            if(!user_trace_level[i].name) {
                break;
            }
            if(strcasecmp(level, user_trace_level[i].name)==0) {
                uint32_t bitmask = 1 << i;
                return bitmask;
            }
        }
    }

    // WARNING Don't log errors, this functions are called in main(), before logger is setup.
    return 0;
}

/****************************************************************************
 *  Set or Reset gobj trace level
 ****************************************************************************/
PRIVATE int _set_gobj_trace_level(gobj_t * gobj, const char *level, BOOL set)
{
    uint32_t bitmask = 0;

    if(level == NULL) {
        bitmask = (uint32_t)-1;
    } else if(empty_string(level)) {
        bitmask = TRACE_USER_LEVEL;
    } else {
        if(isdigit(*((unsigned char *)level))) {
            bitmask = (uint32_t)atoi(level);
        }
        if(!bitmask) {
            bitmask = level2bit(s_global_trace_level, gobj->gclass->s_user_trace_level, level);
            if(!bitmask) {
                if(__initialized__) {
                    gobj_log_error(gobj, LOG_OPT_TRACE_STACK,
                        "function",     "%s", __FUNCTION__,
                        "msgset",       "%s", MSGSET_PARAMETER_ERROR,
                        "msg",          "%s", "gclass trace level NOT FOUND",
                        "gobj_name",    "%s", gobj_short_name(gobj),
                        "level",        "%s", level,
                        NULL
                    );
                }
                return -1;
            }
        }
    }

    if(set) {
        /*
         *  Set
         */
        gobj->trace_level |= bitmask;
    } else {
        /*
         *  Reset
         */
        gobj->trace_level &= ~bitmask;
    }

    return 0;
}

/****************************************************************************
 *  Set or Reset gobj trace level
 *  Call mt_trace_on/mt_trace_off
 ****************************************************************************/
PUBLIC int gobj_set_gobj_trace(hgobj gobj_, const char *level, BOOL set, json_t *kw)
{
    gobj_t * gobj = gobj_;
    if(!gobj) {
        int ret = gobj_set_global_trace(level, set);
        JSON_DECREF(kw)
        return ret;
    }

    if(_set_gobj_trace_level(gobj, level, set)<0) {
        JSON_DECREF(kw)
        return -1;
    }
    if(set) {
        if(gobj->gclass->gmt->mt_trace_on) {
            return gobj->gclass->gmt->mt_trace_on(gobj, level, kw);
        }
    } else {
        if(gobj->gclass->gmt->mt_trace_off) {
            return gobj->gclass->gmt->mt_trace_off(gobj, level, kw);
        }
    }
    JSON_DECREF(kw)
    return 0;
}

/****************************************************************************
 *  Set or Reset gclass trace level
 ****************************************************************************/
PUBLIC int gobj_set_gclass_trace(hgclass gclass_, const char *level, BOOL set)
{
    gclass_t *gclass = gclass_;
    uint32_t bitmask = 0;

    if(!gclass) {
        return gobj_set_global_trace(level, set);
    }
    if(level == NULL) {
        bitmask = (uint32_t)-1;
    } else if(empty_string(level)) {
        bitmask = TRACE_USER_LEVEL;
    } else {
        if(isdigit(*((unsigned char *)level))) {
            bitmask = (uint32_t)atoi(level);
        }
        if(!bitmask) {
            bitmask = level2bit(s_global_trace_level, gclass->s_user_trace_level, level);
            if(!bitmask) {
                if(__initialized__) {
                    gobj_log_error(0, LOG_OPT_TRACE_STACK,
                        "function",     "%s", __FUNCTION__,
                        "msgset",       "%s", MSGSET_PARAMETER_ERROR,
                        "msg",          "%s", "gclass trace level NOT FOUND",
                        "gclass",       "%s", gclass->gclass_name,
                        "level",        "%s", level,
                        NULL
                    );
                }
                return -1;
            }
        }
    }

    if(set) {
        /*
         *  Set
         */
        gclass->trace_level |= bitmask;
    } else {
        /*
         *  Reset
         */
        gclass->trace_level &= ~bitmask;
    }

    return 0;
}

/***************************************************************************
 *  level 1 all but considering no_trace_level
 *  level >1 all
 ***************************************************************************/
PUBLIC int gobj_set_deep_tracing(int level)
{
    __deep_trace__ = (uint32_t)level;

    return 0;
}
PUBLIC int gobj_get_deep_tracing(void)
{
    return (int)__deep_trace__;
}

/****************************************************************************
 *  Set or Reset global trace level
 ****************************************************************************/
PUBLIC int gobj_set_global_trace(const char *level, BOOL set)
{
    uint32_t bitmask = 0;

    if(empty_string(level)) {
        bitmask = TRACE_GLOBAL_LEVEL;
    } else {
        if(isdigit(*((unsigned char *)level))) {
            bitmask = (uint32_t)atoi(level);
        }
        if(!bitmask) {
            bitmask = level2bit(s_global_trace_level, 0, level);
            if(!bitmask) {
                if(__initialized__) {
                    gobj_log_error(0, LOG_OPT_TRACE_STACK,
                        "function",     "%s", __FUNCTION__,
                        "msgset",       "%s", MSGSET_PARAMETER_ERROR,
                        "msg",          "%s", "global trace level NOT FOUND",
                        "level",        "%s", level,
                        NULL
                    );
                }
                return -1;
            }
        }
    }

    if(set) {
        /*
         *  Set
         */
        __global_trace_level__ |= bitmask;
    } else {
        /*
         *  Reset
         */
        __global_trace_level__ &= ~bitmask;
    }
    return 0;
}

/****************************************************************************
 *  Set or Reset global no trace level
 ****************************************************************************/
PUBLIC int gobj_set_global_no_trace(const char *level, BOOL set)
{
    uint32_t bitmask = 0;

    if(empty_string(level)) {
        bitmask = TRACE_GLOBAL_LEVEL;
    } else {
        if(isdigit(*((unsigned char *)level))) {
            bitmask = (uint32_t)atoi(level);
        }
        if(!bitmask) {
            bitmask = level2bit(s_global_trace_level, 0, level);
            if(!bitmask) {
                if(__initialized__) {
                    gobj_log_error(0, LOG_OPT_TRACE_STACK,
                        "function",     "%s", __FUNCTION__,
                        "msgset",       "%s", MSGSET_PARAMETER_ERROR,
                        "msg",          "%s", "global trace level NOT FOUND",
                        "level",        "%s", level,
                        NULL
                    );
                }
                return -1;
            }
        }
    }

    if(set) {
        /*
         *  Set
         */
        __global_trace_no_level__ |= bitmask;
    } else {
        /*
         *  Reset
         */
        __global_trace_no_level__ &= ~bitmask;
    }
    return 0;
}

/****************************************************************************
 *
 ****************************************************************************/
PUBLIC int gobj_load_trace_filter(hgclass gclass_, json_t *jn_trace_filter) // owned
{
    gclass_t *gclass = gclass_;

    JSON_DECREF(gclass->jn_trace_filter)
    gclass->jn_trace_filter = jn_trace_filter;
    return 0;
}

/****************************************************************************
 *
 ****************************************************************************/
PUBLIC int gobj_add_trace_filter(hgclass gclass_, const char *attr, const char *value)
{
    gclass_t *gclass = gclass_;

    if(empty_string(attr)) {
        gobj_log_error(0, LOG_OPT_TRACE_STACK,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_PARAMETER_ERROR,
            "msg",          "%s", "attr NULL",
            NULL
        );
        return -1;
    }
    if(!gclass_has_attr(gclass, attr)) {
        gobj_log_error(0, LOG_OPT_TRACE_STACK,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_PARAMETER_ERROR,
            "msg",          "%s", "gclass has not this attribute",
            "attr",         "%s", attr,
            NULL
        );
        return -1;
    }

    if(!gclass->jn_trace_filter) {
        gclass->jn_trace_filter = json_object();
    }

    json_t *jn_list = kw_get_list(0, gclass->jn_trace_filter, attr, json_array(), KW_CREATE);
    if(!jn_list) {
        gobj_log_error(0, LOG_OPT_TRACE_STACK,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_MEMORY_ERROR,
            "msg",          "%s", "cannot create list",
            "attr",         "%s", attr,
            "value",        "%s", value?value:"",
            NULL
        );
        return -1;
    }

    if(!value) {
        value = "";
    }
    int idx = kw_find_str_in_list(0, jn_list, value);
    if(idx < 0) {
        json_array_append_new(jn_list, json_string(value));
    }
    return 0;
}

/****************************************************************************
 *
 ****************************************************************************/
PUBLIC int gobj_remove_trace_filter(hgclass gclass_, const char *attr, const char *value)
{
    gclass_t *gclass = gclass_;

    if(empty_string(attr)) {
        JSON_DECREF(gclass->jn_trace_filter)
        return 0;
    }
    if(!gclass->jn_trace_filter) {
        return 0;
    }

    json_t *jn_list = kw_get_list(0, gclass->jn_trace_filter, attr, 0, 0);
    if(!jn_list) {
        gobj_log_error(0, LOG_OPT_TRACE_STACK,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_PARAMETER_ERROR,
            "msg",          "%s", "attr not found",
            "attr",         "%s", attr,
            "value",        "%s", value?value:"",
            NULL
        );
        return -1;
    }

    if(empty_string(value)) {
        json_array_clear(jn_list);
        kw_delete(0, gclass->jn_trace_filter, attr);
        if(json_object_size(gclass->jn_trace_filter)==0) {
            JSON_DECREF(gclass->jn_trace_filter)
        }
        return 0;
    }

    int idx = kw_find_str_in_list(0, jn_list, value);
    if(idx < 0) {
        gobj_log_error(0, LOG_OPT_TRACE_STACK,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_PARAMETER_ERROR,
            "msg",          "%s", "value not found",
            "attr",         "%s", attr,
            "value",        "%s", value?value:"",
            NULL
        );
        return -1;
    }

    json_array_remove(jn_list, (size_t)idx);

    if(json_array_size(jn_list) == 0) {
        kw_delete(0, gclass->jn_trace_filter, attr);
        if(json_object_size(gclass->jn_trace_filter)==0) {
            JSON_DECREF(gclass->jn_trace_filter)
        }
    }

    return 0;
}

/****************************************************************************
 *  Return is not YOURS
 ****************************************************************************/
PUBLIC json_t *gobj_get_trace_filter(hgclass gclass_)
{
    gclass_t *gclass = gclass_;
    return gclass->jn_trace_filter;
}

/****************************************************************************
 *  Set or Reset NO gclass trace level
 ****************************************************************************/
PUBLIC int gobj_set_gclass_no_trace(hgclass gclass_, const char *level, BOOL set)
{
    gclass_t *gclass = gclass_;
    uint32_t bitmask = 0;

    if(empty_string(level)) {
        bitmask = (uint32_t)-1;
    } else {
        if(isdigit(*((unsigned char *)level))) {
            bitmask = (uint32_t)atoi(level);
        }
        if(!bitmask) {
            bitmask = level2bit(s_global_trace_level, gclass->s_user_trace_level, level);
            if(!bitmask) {
                if(__initialized__) {
                    gobj_log_error(0, LOG_OPT_TRACE_STACK,
                        "function",     "%s", __FUNCTION__,
                        "msgset",       "%s", MSGSET_PARAMETER_ERROR,
                        "msg",          "%s", "gclass trace level NOT FOUND",
                        "gclass",       "%s", gclass->gclass_name,
                        "level",        "%s", level,
                        NULL
                    );
                }
                return -1;
            }
        }
    }

    if(set) {
        /*
         *  Set
         */
        gclass->no_trace_level |= bitmask;
    } else {
        /*
         *  Reset
         */
        gclass->no_trace_level &= ~bitmask;
    }

    return 0;
}

/****************************************************************************
 *  Set or Reset NO gobj trace level
 ****************************************************************************/
PRIVATE int _set_gobj_no_trace_level(hgobj gobj_, const char *level, BOOL set)
{
    gobj_t * gobj = gobj_;
    if(!gobj) {
        return 0;
    }
    uint32_t bitmask = 0;

    if(empty_string(level)) {
        bitmask = (uint32_t)-1;
    } else {
        if(isdigit(*((unsigned char *)level))) {
            bitmask = (uint32_t)atoi(level);
        }
        if(!bitmask) {
            bitmask = level2bit(s_global_trace_level, gobj->gclass->s_user_trace_level, level);
            if(!bitmask) {
                if(__initialized__) {
                    gobj_log_error(gobj, LOG_OPT_TRACE_STACK,
                        "function",     "%s", __FUNCTION__,
                        "msgset",       "%s", MSGSET_PARAMETER_ERROR,
                        "msg",          "%s", "gclass trace level NOT FOUND",
                        "level",        "%s", level,
                        NULL
                    );
                }
                return -1;
            }
        }
    }

    if(set) {
        /*
         *  Set
         */
        gobj->no_trace_level |= bitmask;
    } else {
        /*
         *  Reset
         */
        gobj->no_trace_level &= ~bitmask;
    }

    return 0;
}

/****************************************************************************
 *  Set or Reset NO gobj trace level
 ****************************************************************************/
PUBLIC int gobj_set_gobj_no_trace(hgobj gobj_, const char *level, BOOL set)
{
    gobj_t * gobj = gobj_;
    if(!gobj) {
        return 0;
    }
    _set_gobj_no_trace_level(gobj, level, set);

    return 0;
}

/***************************************************************************
 *  Must trace?
 ***************************************************************************/
PRIVATE inline BOOL is_machine_tracing(gobj_t * gobj, gobj_event_t event)
{
    if(__deep_trace__) {
        return TRUE;
    }
    if(!gobj) {
        return FALSE;
    }
    uint32_t trace = __global_trace_level__ & TRACE_MACHINE ||
        gobj->trace_level & TRACE_MACHINE ||
        gobj->gclass->trace_level & TRACE_MACHINE;

    if(event == EV_TIMEOUT_PERIODIC) {
        if(!(__global_trace_level__ & TRACE_PERIODIC_TIMER)) {
            trace = 0;
        }
    }

    return trace?TRUE:FALSE;
}

/***************************************************************************
 *  Must no trace?
 ***************************************************************************/
PRIVATE inline BOOL is_machine_not_tracing(gobj_t * gobj)
{
    if(__deep_trace__ > 1) {
        return FALSE;
    }
    if(!gobj) {
        return TRUE;
    }
    uint32_t no_trace = gobj->no_trace_level & TRACE_MACHINE ||
        gobj->gclass->no_trace_level & TRACE_MACHINE;

    return no_trace?TRUE:FALSE;
}

/****************************************************************************
 *  Indent, return spaces multiple of depth level gobj.
 *  With this, we can see the trace messages indenting according
 *  to depth level.
 ****************************************************************************/
PRIVATE char *tab(char *bf, int bflen)
{
    int i;

    bf[0]=' ';
    for(i=1; i<__inside__*2 && i<bflen-2; i++)
        bf[i] = ' ';
    bf[i] = '\0';
    return bf;
}

/****************************************************************************
 *  Trace machine function
 ****************************************************************************/
PUBLIC void trace_machine(const char *fmt, ...)
{
    va_list ap;
    char bf[1024];
    tab(bf, sizeof(bf));
//     printf("tab(%d): %s\n", (int)strlen(bf), bf);
//     printf("fmt: %s\n", fmt);

    va_start(ap, fmt);
    size_t len = strlen(bf);
    vsnprintf(bf+len, sizeof(bf)-len, fmt, ap);
    va_end(ap);

    _log_bf(LOG_DEBUG, 0, bf, strlen(bf));
}



                    /*---------------------------------*
                     *      Log functions
                     *---------------------------------*/




/*****************************************************************
 *
 *****************************************************************/
PUBLIC int gobj_log_register_handler(
    const char* handler_type,
    loghandler_close_fn_t close_fn,
    loghandler_write_fn_t write_fn,
    loghandler_fwrite_fn_t fwrite_fn
) {
    if(max_log_register >= MAX_LOG_HANDLER_TYPES) {
        return -1;
    }
    strncpy(
        log_register[max_log_register].handler_type,
        handler_type,
        sizeof(log_register[0].handler_type) - 1
    );
    log_register[max_log_register].close_fn = close_fn;
    log_register[max_log_register].write_fn = write_fn;
    log_register[max_log_register].fwrite_fn = fwrite_fn;

    max_log_register++;
    return 0;
}

/*****************************************************************
 *  Return if handler exists
 *****************************************************************/
PRIVATE BOOL log_exist_handler(const char *handler_name)
{
    if(empty_string(handler_name)) {
        return FALSE;
    }
    log_handler_t *lh = dl_first(&dl_log_handlers);
    while(lh) {
        log_handler_t *next = dl_next(lh);
        if(strcmp(lh->handler_name, handler_name)==0) {
            return TRUE;
        }
        /*
         *  Next
         */
        lh = next;
    }
    return FALSE;
}

/*****************************************************************
 *
 *****************************************************************/
PUBLIC int gobj_log_add_handler(
    const char *handler_name,
    const char *handler_type,
    log_handler_opt_t handler_options,
    void *h
) {
    if(!__initialized__) {
        gobj_log_set_last_message("gobj_log_add_handler(): gobj not initialized");
        return -1;
    }
    if(empty_string(handler_name)) {
        gobj_log_set_last_message("gobj_log_add_handler(): no handler name");
        return -1;
    }

    if(log_exist_handler(handler_name)) {
        gobj_log_set_last_message("gobj_log_add_handler(): handler name already exists");
        return -1;
    }

    /*-------------------------------------*
     *          Find type
     *-------------------------------------*/
    int type;
    for(type = 0; type < max_log_register; type++) {
        if(strcmp(log_register[type].handler_type, handler_type)==0) {
            break;
        }
    }
    if(type == max_log_register) {
        gobj_log_set_last_message("gobj_log_add_handler(): handler type not found");
        return -1;
    }

    /*-------------------------------------*
     *      Alloc memory
     *  HACK use system memory,
     *  need log handler until the end
     *-------------------------------------*/
    log_handler_t *lh = malloc(sizeof(log_handler_t));
    if(!lh) {
        gobj_log_set_last_message("gobj_log_add_handler(): no memory");
        return -1;
    }
    memset(lh, 0, sizeof(log_handler_t));
    lh->handler_name = strdup(handler_name);
    lh->handler_options = handler_options?handler_options:LOG_OPT_ALL;
    lh->hr = &log_register[type];
    lh->h = h;

    /*----------------*
     *  Add to list
     *----------------*/
    return dl_add(&dl_log_handlers, lh);
}

/*****************************************************************
 *  Return handlers deleted.
 *  Delete all handlers is handle_name is empty
 *****************************************************************/
PUBLIC int gobj_log_del_handler(const char *handler_name)
{
    /*-------------------------------------*
     *      Free memory
     *  HACK use system memory,
     *  need log handler until the end
     *-------------------------------------*/
    BOOL found = FALSE;

    log_handler_t *lh = dl_first(&dl_log_handlers);
    while(lh) {
        log_handler_t *next = dl_next(lh);
        if(empty_string(handler_name) || strcmp(lh->handler_name, handler_name)==0) {
            found = TRUE;
            dl_delete(&dl_log_handlers, lh, 0);
            if(lh->h && lh->hr->close_fn) {
                lh->hr->close_fn(lh->h);
            }
            if(lh->handler_name) {
                free(lh->handler_name);
            }
            free(lh);
        }
        /*
         *  Next
         */
        lh = next;
    }

    if(!found) {
        gobj_log_set_last_message("Handler not found");
        return -1;
    }
    return 0;
}

/*****************************************************************
 *  Return list of handlers
 *****************************************************************/
PUBLIC json_t *gobj_log_list_handlers(void)
{
    json_t *jn_array = json_array();
    log_handler_t *lh = dl_first(&dl_log_handlers);
    while(lh) {
        json_t *jn_dict = json_object();
        json_array_append_new(jn_array, jn_dict);
        json_object_set_new(jn_dict, "handler_name", json_string(lh->handler_name));
        json_object_set_new(jn_dict, "handler_type", json_string(lh->hr->handler_type));
        json_object_set_new(jn_dict, "handler_options", json_integer(lh->handler_options));

        /*
         *  Next
         */
        lh = dl_next(lh);
    }
    return jn_array;
}

/*****************************************************************
 *      Discover extra data
 *****************************************************************/
PRIVATE void discover(gobj_t *gobj, json_t *jn)
{
    const char *yuno_attrs[] = {
        "node_uuid",
        "process",
        "hostname",
        "pid"
    };

    for(size_t i=0; i<ARRAY_NSIZE(yuno_attrs); i++) {
        const char *attr = yuno_attrs[i];
        if(gobj_has_attr(gobj_yuno(), attr)) { // WARNING Check that attr exists:  avoid recursive loop
            json_t *value = gobj_read_attr(gobj_yuno(), attr, 0);
            if(value) {
                json_object_set(jn, attr, value);
            }
        }
    }

#ifdef ESP_PLATFORM
    #include <esp_system.h>
    size_t size = esp_get_free_heap_size();
    json_object_set_new(jn, "HEAP free", json_integer((json_int_t)size));
#endif

    if(!gobj || gobj->obflag & obflag_destroyed) {
        return;
    }
    json_object_set_new(jn, "gclass", json_string(gobj->gclass->gclass_name));
    json_object_set_new(jn, "gobj_name", json_string(gobj->gobj_name));
    json_object_set_new(jn, "state", json_string(gobj_current_state(gobj)));
    if(trace_with_full_name) {
        json_object_set_new(jn,
            "gobj_full_name",
            json_string(gobj_full_name(gobj))
        );
    }
    if(trace_with_short_name) {
        json_object_set_new(jn,
            "gobj_short_name",
            json_string(gobj_short_name(gobj))
        );
    }

    const char *gobj_attrs[] = {
        "id"
    };
    for(size_t i=0; i<ARRAY_NSIZE(gobj_attrs); i++) {
        const char *attr = gobj_attrs[i];
        if(gobj_has_attr(gobj, attr)) { // WARNING Check that attr exists:  avoid recursive loop
            const char *value = gobj_read_str_attr(gobj, attr);
            if(!empty_string(value)) {
                json_object_set_new(jn, attr, json_string(attr));
            }
        }
    }
}

/*****************************************************************
 *  Add key/values from va_list argument
 *****************************************************************/
PRIVATE void json_vappend(json_t *hgen, va_list ap)
{
    char *key;
    char *fmt;
    size_t i;
    char value[256];

    while ((key = (char *)va_arg (ap, char *)) != NULL) {
        fmt = (char *)va_arg (ap, char *);
        for (i = 0; i < strlen (fmt); i++) {
            int eof = 0;

            if (fmt[i] != '%')
                continue;
            i++;
            while (eof != 1) {
                switch (fmt[i]) {
                    case 'd':
                    case 'i':
                    case 'o':
                    case 'u':
                    case 'x':
                    case 'X':
                        if (fmt[i - 1] == 'l') {
                            if (i - 2 > 0 && fmt[i - 2] == 'l') {
                                long long int v;
                                v = va_arg(ap, long long int);
                                json_object_set_new(hgen, key, json_integer(v));

                            } else {
                                long int v;
                                v = va_arg(ap, long int);
                                json_object_set_new(hgen, key, json_integer(v));
                            }
                        } else {
                            int v;
                            v = va_arg(ap, int);
                            json_object_set_new(hgen, key, json_integer(v));
                        }
                        eof = 1;
                        break;
                    case 'e':
                    case 'E':
                    case 'f':
                    case 'F':
                    case 'g':
                    case 'G':
                    case 'a':
                    case 'A':
                        if (fmt[i - 1] == 'L') {
                            long double v;
                            v = va_arg (ap, long double);
                            json_object_set_new(hgen, key, json_real((double)v));
                        } else {
                            double v;
                            v = va_arg (ap, double);
                            json_object_set_new(hgen, key, json_real(v));
                        }
                        eof = 1;
                        break;
                    case 'c':
                        if (fmt [i - 1] == 'l') {
                            wint_t v = va_arg (ap, wint_t);
                            json_object_set_new(hgen, key, json_integer(v));
                        } else {
                            int v = va_arg (ap, int);
                            json_object_set_new(hgen, key, json_integer(v));
                        }
                        eof = 1;
                        break;
                    case 's':
                        if (fmt [i - 1] == 'l') {
                            wchar_t *p;
                            int len;

                            p = va_arg (ap, wchar_t *);
                            if(p && (len = snprintf(value, sizeof(value), "%ls", p))>=0) {
                                if(strcmp(key, "msg")==0) {
                                    gobj_log_set_last_message("%s", value);
                                }
                                json_object_set_new(hgen, key, json_string(value));
                            } else {
                                json_object_set_new(hgen, key, json_null());
                            }

                        } else {
                            char *p;
                            int len;

                            p = va_arg (ap, char *);
                            if(p && (len = snprintf(value, sizeof(value), "%s", p))>=0) {
                                if(strcmp(key, "msg")==0) {
                                    gobj_log_set_last_message("%s", value);
                                }
                                json_object_set_new(hgen, key, json_string(value));
                            } else {
                                json_object_set_new(hgen, key, json_null());
                            }
                        }
                        eof = 1;
                        break;
                    case 'p':
                        {
                            void *p;
                            int len;

                            p = va_arg (ap, void *);
                            eof = 1;
                            if(p && (len = snprintf(value, sizeof(value), "%p", (char *)p))>0) {
                                json_object_set_new(hgen, key, json_string(value));
                            } else {
                                json_object_set_new(hgen, key, json_null());
                            }
                        }
                        break;
                    case 'j':
                        {
                            json_t *jn;

                            jn = va_arg (ap, void *);
                            eof = 1;

                            if(jn) {
                                size_t flags = JSON_ENCODE_ANY |
                                    JSON_INDENT(0) |
                                    JSON_REAL_PRECISION(12);
                                char *bf = json_dumps(jn, flags);
                                if(bf) {
                                    helper_doublequote2quote(bf);
                                    json_object_set_new(hgen, key, json_string(bf));
                                    jsonp_free(bf) ;
                                } else {
                                    json_object_set_new(hgen, key, json_null());
                                }
                            } else {
                                json_object_set_new(hgen, key, json_null());
                            }
                        }
                        break;
                    case '%':
                        eof = 1;
                        break;
                    default:
                        i++;
                }
            }
        }
    }
}

/*****************************************************************
 *
 *****************************************************************/
PRIVATE BOOL must_ignore(log_handler_t *lh, int priority)
{
    BOOL ignore = TRUE;
    log_handler_opt_t handler_options = lh->handler_options;

    switch(priority) {
    case LOG_ALERT:
        if(handler_options & LOG_HND_OPT_ALERT)
            ignore = FALSE;
        break;
    case LOG_CRIT:
        if(handler_options & LOG_HND_OPT_CRITICAL)
            ignore = FALSE;
        break;
    case LOG_ERR:
        if(handler_options & LOG_HND_OPT_ERROR)
            ignore = FALSE;
        break;
    case LOG_WARNING:
        if(handler_options & LOG_HND_OPT_WARNING)
            ignore = FALSE;
        break;
    case LOG_INFO:
        if(handler_options & LOG_HND_OPT_INFO)
            ignore = FALSE;
        break;
    case LOG_DEBUG:
        if(handler_options & LOG_HND_OPT_DEBUG)
            ignore = FALSE;
        break;

    default:
        break;
    }
    return ignore;
}

/*****************************************************************
 *      Log data in transparent format
 *****************************************************************/
PRIVATE void _log_bf(int priority, log_opt_t opt, const char *bf, size_t len)
{
    static char __inside__ = 0;

    if(len <= 0) {
        return;
    }

    if(__inside__) {
        return;
    }
    __inside__ = 1;

    BOOL backtrace_showed = FALSE;
    log_handler_t *lh = dl_first(&dl_log_handlers);
    while(lh) {
        if(must_ignore(lh, priority)) {
            /*
             *  Next
             */
            lh = dl_next(lh);
            continue;
        }

        if(lh->hr->write_fn) {
            int ret = (lh->hr->write_fn)(lh->h, priority, bf, len);
            if(ret < 0) { // Handler owns the message
                break;
            }
        }
        if(opt & (LOG_OPT_TRACE_STACK|LOG_OPT_EXIT_NEGATIVE|LOG_OPT_ABORT)) {
            if(!backtrace_showed) {
                if(show_backtrace_fn && lh->hr->fwrite_fn) {
                    show_backtrace_fn(lh->hr->fwrite_fn, lh->h);
                }
            }
            backtrace_showed = TRUE;
        }

        /*
         *  Next
         */
        lh = dl_next(lh);
    }

    __inside__ = 0;

    if(opt & LOG_OPT_EXIT_NEGATIVE) {
        exit(-1);
    }
    if(opt & LOG_OPT_EXIT_ZERO) {
        exit(0);
    }
    if(opt & LOG_OPT_ABORT) {
        abort();
    }
}

/*****************************************************************
 *      Log
 *****************************************************************/
PRIVATE void _log(hgobj gobj, int priority, log_opt_t opt, va_list ap)
{
    char timestamp[90];

    current_timestamp(timestamp, sizeof(timestamp));
    json_t *jn_log = json_object();
    json_object_set_new(jn_log, "timestamp", json_string(timestamp));
    discover(gobj, jn_log);

    json_vappend(jn_log, ap);

    char *s = json_dumps(jn_log, JSON_INDENT(2)|JSON_ENCODE_ANY);
    _log_bf(priority, opt, s, strlen(s));
    jsonp_free(s);
    json_decref(jn_log);
}

/*****************************************************************
 *      Log alert
 *****************************************************************/
PUBLIC void gobj_log_alert(hgobj gobj, log_opt_t opt, ...)
{
    int priority = LOG_ALERT;

    __alert_count__++;

    va_list ap;
    va_start(ap, opt);
    _log(gobj, priority, opt, ap);
    va_end(ap);
}

/*****************************************************************
 *      Log critical
 *****************************************************************/
PUBLIC void gobj_log_critical(hgobj gobj, log_opt_t opt, ...)
{
    int priority = LOG_CRIT;

    __critical_count__++;

    va_list ap;
    va_start(ap, opt);
    _log(gobj, priority, opt, ap);
    va_end(ap);
}

/*****************************************************************
 *      Log error
 *****************************************************************/
PUBLIC void gobj_log_error(hgobj gobj, log_opt_t opt, ...)
{
    int priority = LOG_ERR;

    __error_count__++;

    va_list ap;
    va_start(ap, opt);
    _log(gobj, priority, opt, ap);
    va_end(ap);
}

/*****************************************************************
 *      Log warning
 *****************************************************************/
PUBLIC void gobj_log_warning(hgobj gobj, log_opt_t opt, ...)
{
    int priority = LOG_WARNING;

    __warning_count__++;

    va_list ap;
    va_start(ap, opt);
    _log(gobj, priority, opt, ap);
    va_end(ap);
}

/*****************************************************************
 *      Log info
 *****************************************************************/
PUBLIC void gobj_log_info(hgobj gobj, log_opt_t opt, ...)
{
    int priority = LOG_INFO;

    __info_count__++;

    va_list ap;
    va_start(ap, opt);
    _log(gobj, priority, opt, ap);
    va_end(ap);
}

/*****************************************************************
 *      Log debug
 *****************************************************************/
PUBLIC void gobj_log_debug(hgobj gobj, log_opt_t opt, ...)
{
    int priority = LOG_DEBUG;

    __debug_count__++;

    va_list ap;
    va_start(ap, opt);
    _log(gobj, priority, opt, ap);
    va_end(ap);
}

/*****************************************************************
 *
 *****************************************************************/
PUBLIC const char *gobj_get_log_priority_name(int priority)
{
    if(priority >= (int)ARRAY_NSIZE(priority_names)) {
        return "";
    }
    return priority_names[priority];
}

/*****************************************************************
 *      Clear counters
 *****************************************************************/
PUBLIC void gobj_log_clear_counters(void)
{
    __debug_count__ = 0;
    __info_count__ = 0;
    __warning_count__ = 0;
    __error_count__ = 0;
    __critical_count__ = 0;
    __alert_count__ = 0;
}

/*****************************************************************
 *
 *****************************************************************/
PUBLIC const char *gobj_log_last_message(void)
{
    return last_message;
}

/*****************************************************************
 *      Log alert
 *****************************************************************/
PUBLIC void gobj_log_set_last_message(const char *msg, ...)
{
    va_list ap;
    va_start(ap, msg);
    vsnprintf(last_message, sizeof(last_message), msg, ap);
    va_end(ap);
}

/***************************************************************************
 *
 ***************************************************************************/
PUBLIC int stdout_write(void* v, int priority, const char* bf, size_t len)
{
    #define BWhite "\033[1;37m"       // White
    #define On_Red "\033[41m"         // Red
    #define Color_Off "\033[0m"       // Text Reset
    #define On_Black "\033[40m"       // Black
    #define BYellow "\033[1;33m"      // Yellow

    if(!bf) {
        // silence
        return -1;
    }
    if(len<=0) {
        // silence
        return -1;
    }
    if(priority >= (int)ARRAY_NSIZE(priority_names)) {
        priority = LOG_DEBUG;
    }

    if(priority <= LOG_ERR) {
        fwrite(On_Red, strlen(On_Red), 1, stdout);
        fwrite(BWhite, strlen(BWhite), 1, stdout);
        fwrite(priority_names[priority], strlen(priority_names[priority]), 1, stdout);
        fwrite(Color_Off, strlen(Color_Off), 1, stdout);

    } else if(priority <= LOG_WARNING) {
        fwrite(On_Black, strlen(On_Black), 1, stdout);
        fwrite(BYellow, strlen(BYellow), 1, stdout);
        fwrite(priority_names[priority], strlen(priority_names[priority]), 1, stdout);
        fwrite(Color_Off, strlen(Color_Off), 1, stdout);
    } else {
        fwrite(priority_names[priority], strlen(priority_names[priority]), 1, stdout);
    }

    fwrite(": ", strlen(": "), 1, stdout);
    fwrite(bf, len, 1, stdout);
    fwrite("\n", strlen("\n"), 1, stdout);
    fflush(stdout);

    return 0;
}

/***************************************************************************
 *
 ***************************************************************************/
PUBLIC int stdout_fwrite(void *v, int priority, const char *fmt, ...)
{
    va_list ap;
    va_list aq;
    int length;
    char *buf;

    va_start(ap, fmt);
    va_copy(aq, ap);

    length = vsnprintf(NULL, 0, fmt, ap);
    if(length>0) {
        buf = sys_malloc_fn((size_t)length + 1);
        if(buf) {
            vsnprintf(buf, (size_t)length + 1, fmt, aq);
            stdout_write(v, priority, buf, strlen(buf));
            sys_free_fn(buf);
        }
    }

    va_end(ap);
    va_end(aq);

    return 0;
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE void show_backtrace(loghandler_fwrite_fn_t fwrite_fn, void *h)
{
#ifdef __linux__
    void* callstack[128];
    int frames = backtrace(callstack, sizeof(callstack) / sizeof(void*));
    char** symbols = backtrace_symbols(callstack, frames);

    if (symbols == NULL) {
        return;
    }
    fwrite_fn(h, LOG_DEBUG, "===============> begin stack trace <==================");
    for (int i = 0; i < frames; i++) {
        fwrite_fn(h, LOG_DEBUG, "%s", symbols[i]);
    }
    fwrite_fn(h, LOG_DEBUG, "===============> end stack trace <==================\n");
    free(symbols);
#endif
}

/***************************************************************************
 *
 ***************************************************************************/
PUBLIC void set_show_backtrace_fn(show_backtrace_fn_t show_backtrace)
{
    show_backtrace_fn = show_backtrace;
}

/***************************************************************************
 *
 ***************************************************************************/
PUBLIC void trace_vjson(
    hgobj gobj,
    json_t *jn_data,    // now owned
    const char *msgset,
    const char *fmt,
    va_list ap
) {
    int priority = LOG_DEBUG;
    log_opt_t opt = 0;
    char timestamp[90];
    char msg[256];

    current_timestamp(timestamp, sizeof(timestamp));
    json_t *jn_log = json_object();
    json_object_set_new(jn_log, "timestamp", json_string(timestamp));
    discover(gobj, jn_log);

    vsnprintf(msg, sizeof(msg), fmt, ap);
    json_object_set_new(jn_log, "msgset", json_string(msgset));
    json_object_set_new(jn_log, "msg", json_string(msg));
    if(jn_data) {
        json_object_set(jn_log, "data", jn_data);
        json_object_set_new(jn_log, "refcount", json_integer(jn_data->refcount));
        json_object_set_new(jn_log, "type", json_integer(jn_data->type));
    }

    char *s = json_dumps(jn_log, JSON_INDENT(2)|JSON_ENCODE_ANY);
    _log_bf(priority, opt, s, strlen(s));
    jsonp_free(s);
    json_decref(jn_log);
}

/***************************************************************************
 *
 ***************************************************************************/
PUBLIC void gobj_trace_msg(hgobj gobj, const char *fmt, ... )
{
    va_list ap;

    va_start(ap, fmt);
    trace_vjson(gobj, 0, "trace_msg", fmt, ap);
    va_end(ap);
}

/***************************************************************************
 *
 ***************************************************************************/
PUBLIC void gobj_trace_json(
    hgobj gobj,
    json_t *jn, // now owned
    const char *fmt, ...
) {
    va_list ap;

    va_start(ap, fmt);
    trace_vjson(gobj, jn, "trace_json", fmt, ap);
    va_end(ap);

    if((gobj_trace_level(gobj) & TRACE_GBUFFERS)) {
        gbuffer_t *gbuf = (gbuffer_t *)(size_t)json_integer_value(json_object_get(jn, "gbuffer"));
        if(gbuf) {
            gobj_trace_dump_gbuf(gobj, gbuf, "gbuffer");
        }
    }
}

/***************************************************************************
 *
 ***************************************************************************/
PUBLIC void gobj_trace_buffer(
    hgobj gobj,
    const char *bf,
    size_t len,
    const char *fmt, ...
) {
    va_list ap;

    json_t *jn_data = json_stringn(bf, len);

    va_start(ap, fmt);
    trace_vjson(gobj, jn_data, "trace_buffer", fmt, ap);
    va_end(ap);

    json_decref(jn_data);
}

/***********************************************************************
 *  byte_to_strhex - sprintf %02X
 *  Usage:
 *        char *byte_to_strhex(char *strh, uint8_t w)
 *  Description:
 *        Print in 'strh' the Hexadecimal ASCII representation of 'w'
 *        Equivalent to sprintf %02X SIN EL NULO
 *        OJO que hay f() que usan esta caracteristica
 *  Return:
 *        Pointer to endp
 ***********************************************************************/
static const char tbhexa[]={'0','1','2','3','4','5','6','7','8','9','A','B','C','D','E','F'};
static inline char * byte_to_strhex(char *s, char w)
{
    *s = tbhexa[ ((w >> 4) & 0x0f) ];
    s++;
    *s = tbhexa[ ((w) & 0x0f) ];
    s++;
    return s;
}

/***********************************************************************
 *  Vuelca en formato tdump un array de longitud 'len'
 *  nivel 1 -> solo en hexa
 *  nivel 2 -> en hexa y en asci
 *  nivel 3 -> en hexa y en asci y con contador (indice)
 ***********************************************************************/
PUBLIC void tdump(const char *prefix, const uint8_t *s, size_t len, view_fn_t view, int nivel)
{
    static char bf[80+1];
    static char asci[40+1];
    char *p;
    size_t i, j;

    if(!nivel) {
        nivel = 3;
    }
    if(!view) {
        view = printf;
    }
    if(!prefix) {
        prefix = (char *) "";
    }

    p = bf;
    for(i=j=0; i<len; i++) {
        asci[j] = (*s<' ' || *s>0x7f)? '.': *s;
        if(asci[j] == '%')
            asci[j] = '.';
        j++;
        p = byte_to_strhex(p, *s++);
        *p++ = ' ';
        if(j == 16) {
            *p++ = '\0';
            asci[j] = '\0';

            if(nivel==1) {
                view("%s%s\n", prefix, bf);

            } else if(nivel==2) {
                view("%s%s  %s\n", prefix, bf, asci);

            } else {
                view("%s%04X: %s  %s\n", prefix, i-15, bf, asci);
            }

            p = bf;
            j = 0;
        }
    }
    if(j) {
        len = 16 - j;
        while(len-- >0) {
            *p++ = ' ';
            *p++ = ' ';
            *p++ = ' ';
        }
        *p++ = '\0';
        asci[j] = '\0';

        if(nivel==1) {
           view("%s%s\n", prefix, bf);

        } else if(nivel==2) {
            view("%s%s  %s\n", prefix, bf, asci);

        } else {
            view("%s%04X: %s  %s\n", prefix, i - j, bf, asci);
        }
    }
}

/***********************************************************************
 *  Vuelca en formato tdump un array de longitud 'len'
 *    Contador, hexa, ascii
 ***********************************************************************/
PUBLIC json_t *tdump2json(const uint8_t *s, size_t len)
{
    char hexa[80+1];
    char asci[40+1];
    char addr[16+1];
    char *p;
    size_t i, j;

    json_t *jn_dump = json_object();

    p = hexa;
    for(i=j=0; i<len; i++) {
        asci[j] = (*s<' ' || *s>0x7f)? '.': *s;
        if(asci[j] == '%')
            asci[j] = '.';
        j++;
        p = byte_to_strhex(p, *s++);
        *p++ = ' ';
        if(j == 16) {
            *p++ = '\0';
            asci[j] = '\0';

            snprintf(addr, sizeof(addr), "%04X", (unsigned int)(i-15));
            json_t *jn_hexa = json_sprintf("%s  %s",  hexa, asci);
            json_object_set_new(jn_dump, addr, jn_hexa);

            p = hexa;
            j = 0;
        }
    }
    if(j) {
        len = 16 - j;
        while(len-- >0) {
            *p++ = ' ';
            *p++ = ' ';
            *p++ = ' ';
        }
        *p++ = '\0';
        asci[j] = '\0';

        snprintf(addr, sizeof(addr), "%04X", (unsigned int)(i-j));
        json_t *jn_hexa = json_sprintf("%s  %s",  hexa, asci);
        json_object_set_new(jn_dump, addr, jn_hexa);
    }

    return jn_dump;
}

/***************************************************************************
 *
 ***************************************************************************/
PUBLIC void gobj_trace_dump(
    hgobj gobj,
    const char *bf,
    size_t len,
    const char *fmt,
    ...
) {
    va_list ap;

    json_t *jn_data = tdump2json((uint8_t *)bf, len);

    va_start(ap, fmt);
    trace_vjson(gobj, jn_data, "trace_dump", fmt, ap);
    va_end(ap);

    json_decref(jn_data);
}

/***************************************************************************
 *  Print json to stdout
 ***************************************************************************/
PUBLIC int print_json2(const char *label, json_t *jn)
{
    if(!label) {
        label = "";
    }
    if(!jn || jn->refcount <= 0) {
        fprintf(stdout, "%s (%p) ERROR print_json2(): json %s %d, type %d\n",
            label,
            jn,
            jn?"NULL":"or refcount is",
            jn?(int)(jn->refcount):0,
            jn?(int)(jn->type):0
        );
        return -1;
    }

    size_t flags = JSON_INDENT(2)|JSON_ENCODE_ANY;
    fprintf(stdout, "%s (%p) (refcount: %d, type %d) ==>\n",
        label,
        jn,
        (int)(jn->refcount),
        (int)(jn->type)
    );
    json_dumpf(jn, stdout, flags);
    fprintf(stdout, "\n");
    return 0;
}




                        /*---------------------------------*
                         *      Utilities functions
                         *---------------------------------*/




/*****************************************************************
 *  timestamp with usec resolution
 *  `bf` must be 90 bytes minimum
 *****************************************************************/
PUBLIC char *current_timestamp(char *bf, size_t bfsize)
{
    struct timespec ts;
    struct tm *tm;
    char stamp[64], zone[16];
    clock_gettime(CLOCK_REALTIME, &ts);
    tm = localtime(&ts.tv_sec);

    strftime(stamp, sizeof (stamp), "%Y-%m-%dT%H:%M:%S", tm);
    strftime(zone, sizeof (zone), "%z", tm);
    snprintf(bf, bfsize, "%s.%09lu%s", stamp, ts.tv_nsec, zone);
    return bf;
}

/****************************************************************************
 *   Arranca un timer de 'seconds' segundos.
 *   El valor retornado es el que hay que usar en la funcion test_timer()
 *   para ver si el timer ha cumplido.
 ****************************************************************************/
PUBLIC time_t start_sectimer(time_t seconds)
{
    time_t timer;

    time(&timer);
    timer += seconds;
    return timer;
}

/****************************************************************************
 *   Retorna TRUE si ha cumplido el timer 'value', FALSE si no.
 ****************************************************************************/
PUBLIC BOOL test_sectimer(time_t value)
{
    time_t timer_actual;

    if(value <= 0) {
        // No value no test true
        return FALSE;
    }
    time(&timer_actual);
    return (timer_actual>=value)? TRUE:FALSE;
}

/****************************************************************************
 *   Arranca un timer de 'miliseconds' mili-segundos.
 *   El valor retornado es el que hay que usar en la funcion test_msectimer()
 *   para ver si el timer ha cumplido.
 ****************************************************************************/
PUBLIC uint64_t start_msectimer(uint64_t miliseconds)
{
    uint64_t ms = time_in_miliseconds();
    ms += miliseconds;
    return ms;
}

/****************************************************************************
 *   Retorna TRUE si ha cumplido el timer 'value', FALSE si no.
 ****************************************************************************/
PUBLIC BOOL test_msectimer(uint64_t value)
{
    if(value == 0) {
        // No value no test true
        return FALSE;
    }

    uint64_t ms = time_in_miliseconds();

    return (ms>value)? TRUE:FALSE;
}

/****************************************************************************
 *  Current time in milisecons
 ****************************************************************************/
PUBLIC uint64_t time_in_miliseconds(void)
{
    struct timespec spec;

    //clock_gettime(CLOCK_MONOTONIC, &spec); //Este no da el time from Epoch
    clock_gettime(CLOCK_REALTIME, &spec);

    // Convert to milliseconds
    return ((uint64_t)spec.tv_sec)*1000 + ((uint64_t)spec.tv_nsec)/1000000;
}

/***************************************************************************
 *  Return current time in seconds (standart time(&t))
 ***************************************************************************/
PUBLIC uint64_t time_in_seconds(void)
{
    time_t __t__;
    time(&__t__);
    return (uint64_t) __t__;
}

/***************************************************************************
 *
 ***************************************************************************/
PUBLIC char *helper_quote2doublequote(char *str)
{
    register size_t len = strlen(str);
    register char *p = str;

    for(size_t i=0; i<len; i++, p++) {
        if(*p== '\'')
            *p = '"';
    }
    return str;
}

/***************************************************************************
 *
 ***************************************************************************/
PUBLIC char *helper_doublequote2quote(char *str)
{
    register size_t len = strlen(str);
    register char *p = str;

    for(size_t i=0; i<len; i++, p++) {
        if(*p== '"')
            *p = '\'';
    }
    return str;
}

/***************************************************************************
 *  Convert any json string to json binary.
 ***************************************************************************/
PUBLIC json_t *anystring2json(hgobj gobj, const char *bf, size_t len, BOOL verbose)
{
    if(empty_string(bf)) {
        if(verbose) {
            gobj_log_error(gobj, LOG_OPT_TRACE_STACK,
                "function",     "%s", __FUNCTION__,
                "msgset",       "%s", MSGSET_PARAMETER_ERROR,
                "msg",          "%s", "bf EMPTY",
                NULL
            );
        }
        return 0;
    }
    size_t flags = JSON_DECODE_ANY;
    json_error_t error;
    json_t *jn = json_loadb(bf, len, flags, &error);
    if(!jn) {
        if(verbose) {
            gobj_log_error(gobj, LOG_OPT_TRACE_STACK,
                "function",     "%s", __FUNCTION__,
                "msgset",       "%s", MSGSET_JSON_ERROR,
                "msg",          "%s", "json_loads() FAILED",
                "bf",          "%s", bf,
                "error",        "%s", error.text,
                "line",         "%d", error.line,
                "column",       "%d", error.column,
                "position",     "%d", error.position,
                NULL
            );
            gobj_trace_dump(gobj, bf, strlen(bf), "json_loads() FAILED");
        }
    }
    return jn;
}

/***************************************************************************
 *  Simple json to int
 ***************************************************************************/
PUBLIC json_int_t jn2integer(json_t *jn_var)
{
    json_int_t val = 0;
    if(json_is_real(jn_var)) {
        val = (json_int_t)json_real_value(jn_var);
    } else if(json_is_integer(jn_var)) {
        val = json_integer_value(jn_var);
    } else if(json_is_string(jn_var)) {
        const char *v = json_string_value(jn_var);
        if(*v == '0') {
            val = strtoll(v, 0, 8);
        } else if(*v == 'x' || *v == 'X') {
            val = strtoll(v, 0, 16);
        } else {
            val = strtoll(v, 0, 10);
        }
    } else if(json_is_true(jn_var)) {
        val = 1;
    } else if(json_is_false(jn_var)) {
        val = 0;
    } else if(json_is_null(jn_var)) {
        val = 0;
    }
    return val;
}

/***************************************************************************
 *  Prints to the provided buffer a nice number of bytes (KB, MB, GB, etc)
 *  https://www.mbeckler.org/blog/?p=114
 ***************************************************************************/
PUBLIC void nice_size(char* bf, size_t bfsize, uint64_t bytes)
{
    const char* suffixes[7];
    suffixes[0] = "B";
    suffixes[1] = "Thousands";
    suffixes[2] = "Millions";
    suffixes[3] = "GB";
    suffixes[4] = "TB";
    suffixes[5] = "PB";
    suffixes[6] = "EB";
    unsigned int s = 0; // which suffix to use
    double count = (double)bytes;
    while (count >= 1000 && s < 7)
    {
        s++;
        count /= 1000;
    }
    if (count - floor(count) == 0.0)
        snprintf(bf, bfsize, "%d %s", (int)count, suffixes[s]);
    else
        snprintf(bf, bfsize, "%.1f %s", count, suffixes[s]);
}

/***************************************************************************
 *    Elimina blancos a la derecha. (Espacios, tabuladores, CR's  o LF's)
 ***************************************************************************/
PUBLIC void delete_right_blanks(char *s)
{
    int l;
    char c;

    /*---------------------------------*
     *  Elimina blancos a la derecha
     *---------------------------------*/
    l = (int)strlen(s);
    if(l==0)
        return;
    while(--l>=0) {
        c= *(s+l);
        if(c==' ' || c=='\t' || c=='\n' || c=='\r')
            *(s+l)='\0';
        else
            break;
    }
}

/***************************************************************************
 *    Elimina blancos a la izquierda. (Espacios, tabuladores, CR's  o LF's)
 ***************************************************************************/
PUBLIC void delete_left_blanks(char *s)
{
    unsigned l;
    char c;

    /*----------------------------*
     *  Busca el primer no blanco
     *----------------------------*/
    l=0;
    while(1) {
        c= *(s+l);
        if(c=='\0')
            break;
        if(c==' ' || c=='\t' || c=='\n' || c=='\r')
            l++;
        else
            break;
    }
    if(l>0) {
        memmove(s,s+l,(unsigned long)strlen(s) -l + 1);
    }
}

/***************************************************************************
 *    Justifica a la izquierda eliminado blancos a la derecha
 ***************************************************************************/
PUBLIC void left_justify(char *s)
{
    if(s) {
        /*---------------------------------*
         *  Elimina blancos a la derecha
         *---------------------------------*/
        delete_right_blanks(s);

        /*-----------------------------------*
         *  Quita los blancos a la izquierda
         *-----------------------------------*/
        delete_left_blanks(s);
    }
}

/***************************************************************************
 *  Convert n bytes of string to upper case
 ***************************************************************************/
PUBLIC char *strntoupper(char* s, size_t n)
{
    if(!s || n == 0)
        return 0;

    char *p = s;
    while (n > 0 && *p != '\0') {
        int c = (int)*p;
        *p = (char)toupper(c);
        p++;
        n--;
    }

    return s;
}

/***************************************************************************
 *  Convert n bytes of string to lower case
 ***************************************************************************/
PUBLIC char *strntolower(char* s, size_t n)
{
    if(!s || n == 0)
        return 0;

    char *p = s;
    while (n > 0 && *p != '\0') {
        int c = (int)*p;
        *p = (char)tolower(c);
        p++;
        n--;
    }

    return s;
}

/***************************************************************************
 *    cambia el character old_d por new_c. Retorna los caracteres cambiados
 ***************************************************************************/
PUBLIC int change_char(char *s, char old_c, char new_c)
{
    int count = 0;

    while(*s) {
        if(*s == old_c) {
            *s = new_c;
            count++;
        }
        s++;
    }
    return count;
}

/***************************************************************************
    Split string `str` by `delim` chars returning the list of strings.
    Fill `list_size` if not null with items size,
        It MUST be initialized to 0 (no limit) or to maximum items wanted.
    WARNING Remember free with split_free3().
    HACK: Yes, It does include the empty strings!
 ***************************************************************************/
PUBLIC const char **split3(const char *str, const char *delim, int *plist_size)
{
    char *ptr, *p;
    int max_items = 0;
    if(plist_size) {
        max_items = *plist_size;
        *plist_size = 0; // error case
    }
    char *buffer = GBMEM_STRDUP(str);
    if(!buffer) {
        return 0;
    }

    // Get list size
    int list_size = 0;

    p = buffer;
    while ((ptr = strsep(&p, delim)) != NULL) {
        list_size++;
    }
    GBMEM_FREE(buffer);

    // Limit list
    if(max_items > 0) {
        list_size = MIN(max_items, list_size);
    }

    buffer = GBMEM_STRDUP(str);   // Prev buffer is destroyed!
    if(!buffer) {
        return 0;
    }

    // Alloc list
    size_t size = sizeof(char *) * (list_size + 1);
    const char **list = GBMEM_MALLOC(size);

    // Fill list
    int i = 0;
    p = buffer;
    while ((ptr = strsep(&p, delim)) != NULL) {
        if (i < list_size) {
            list[i] = GBMEM_STRDUP(ptr);
            i++;
        } else {
            break;
        }
    }
    GBMEM_FREE(buffer);

    if(plist_size) {
        *plist_size = list_size;
    }
    return list;
}

/***************************************************************************
 *  Free split list content
 ***************************************************************************/
PUBLIC void split_free3(const char **list)
{
    if(list) {
        char **p = (char **)list;
        while(*p) {
            GBMEM_FREE(*p);
            *p = 0;
            p++;
        }
        GBMEM_FREE(list);
    }
}




                        /*---------------------------------*
                         *      SECTION: dl_list
                         *---------------------------------*/




/***************************************************************
 *      Initialize double list
 ***************************************************************/
PUBLIC int dl_init(dl_list_t *dl)
{
    if(dl->head || dl->tail || dl->__itemsInContainer__) {
        gobj_trace_msg(0, "dl_init(): Wrong dl_list_t, MUST be empty");
        abort();
    }
    dl->head = 0;
    dl->tail = 0;
    dl->__itemsInContainer__ = 0;
    return 0;
}

/***************************************************************
 *      Seek first item
 ***************************************************************/
PUBLIC void *dl_first(dl_list_t *dl)
{
    return dl->head;
}

/***************************************************************
 *      Seek last item
 ***************************************************************/
PUBLIC void *dl_last(dl_list_t *dl)
{
    return dl->tail;
}

/***************************************************************
 *      next Item
 ***************************************************************/
PUBLIC void *dl_next(void *curr)
{
    if(curr) {
        return ((dl_item_t *) curr)->__next__;
    }
    return (void *)0;
}

/***************************************************************
 *      previous Item
 ***************************************************************/
PUBLIC void *dl_prev(void *curr)
{
    if(curr) {
        return ((dl_item_t *) curr)->__prev__;
    }
    return (void *)0;
}

/***************************************************************
 *  Check if a new item has links: MUST have NO links
 *  Return true if it has links
 ***************************************************************/
static bool check_links(register dl_item_t *item)
{
    if(item->__prev__ || item->__next__ || item->__dl__) {
        gobj_log_error(0, LOG_OPT_TRACE_STACK,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_INTERNAL_ERROR,
            "msg",          "%s", "Wrong dl_item_t, WITH links",
            NULL
        );
        return true;
    }
    return false;
}

/***************************************************************
 *  Check if a item has no links: MUST have links
 *  Return true if it has not links
 ***************************************************************/
static bool check_no_links(register dl_item_t *item)
{
    if(!item->__dl__) {
        gobj_log_error(0, LOG_OPT_TRACE_STACK,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_INTERNAL_ERROR,
            "msg",          "%s", "Wrong dl_item_t, WITHOUT links",
            NULL
        );
        return true;
    }
    return false;
}

/***************************************************************
 *      Add at end of the list
 ***************************************************************/
PUBLIC int dl_add(dl_list_t *dl, void *item)
{
    if(check_links(item)) return -1;

    if(dl->tail==0) { /*---- Empty List -----*/
        ((dl_item_t *)item)->__prev__=0;
        ((dl_item_t *)item)->__next__=0;
        dl->head = item;
        dl->tail = item;
    } else { /* LAST ITEM */
        ((dl_item_t *)item)->__prev__ = dl->tail;
        ((dl_item_t *)item)->__next__ = 0;
        dl->tail->__next__ = item;
        dl->tail = item;
    }
    dl->__itemsInContainer__++;
    dl->__last_id__++;
    ((dl_item_t *)item)->__id__ = dl->__last_id__;
    ((dl_item_t *)item)->__dl__ = dl;

    return 0;
}

/***************************************************************
 *    Delete current item
 ***************************************************************/
PUBLIC int dl_delete(dl_list_t *dl, void * curr_, void (*fnfree)(void *))
{
    register dl_item_t * curr = curr_;
    /*-------------------------*
     *     Check nulls
     *-------------------------*/
    if(curr==0) {
        gobj_log_error(0, LOG_OPT_TRACE_STACK,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_INTERNAL_ERROR,
            "msg",          "%s", "Deleting item NULL",
            NULL
        );
        return -1;
    }
    if(check_no_links(curr))
        return -1;

    if(curr->__dl__ != dl) {
        gobj_log_error(0, LOG_OPT_TRACE_STACK,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_INTERNAL_ERROR,
            "msg",          "%s", "Deleting item with DIFFERENT dl_list_t",
            NULL
        );
        return -1;
    }

    /*-------------------------*
     *     Check list
     *-------------------------*/
    if(dl->head==0 || dl->tail==0 || dl->__itemsInContainer__==0) {
        gobj_log_error(0, LOG_OPT_TRACE_STACK,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_INTERNAL_ERROR,
            "msg",          "%s", "Deleting item in EMPTY list",
            NULL
        );
        return -1;
    }

    /*------------------------------------------------*
     *                 Delete
     *------------------------------------------------*/
    if(((dl_item_t *)curr)->__prev__==0) {
        /*------------------------------------*
         *  FIRST ITEM. (curr==dl->head)
         *------------------------------------*/
        dl->head = dl->head->__next__;
        if(dl->head) /* is last item? */
            dl->head->__prev__=0; /* no */
        else
            dl->tail=0; /* yes */

    } else {
        /*------------------------------------*
         *    MIDDLE or LAST ITEM
         *------------------------------------*/
        ((dl_item_t *)curr)->__prev__->__next__ = ((dl_item_t *)curr)->__next__;
        if(((dl_item_t *)curr)->__next__) /* last? */
            ((dl_item_t *)curr)->__next__->__prev__ = ((dl_item_t *)curr)->__prev__; /* no */
        else
            dl->tail= ((dl_item_t *)curr)->__prev__; /* yes */
    }

    /*-----------------------------*
     *  Decrement items counter
     *-----------------------------*/
    dl->__itemsInContainer__--;

    /*-----------------------------*
     *  Reset pointers
     *-----------------------------*/
    ((dl_item_t *)curr)->__prev__ = 0;
    ((dl_item_t *)curr)->__next__ = 0;
    ((dl_item_t *)curr)->__dl__ = 0;

    /*-----------------------------*
     *  Free item
     *-----------------------------*/
    if(fnfree) {
        (*fnfree)(curr);
    }

    return 0;
}

/***************************************************************
 *      DL_LIST: Flush double list. (Remove all items).
 ***************************************************************/
PUBLIC void dl_flush(dl_list_t *dl, void (*fnfree)(void *))
{
    register dl_item_t *first;

    while((first = dl_first(dl))) {
        dl_delete(dl, first, fnfree);
    }
    if(dl->head || dl->tail || dl->__itemsInContainer__) {
        gobj_log_error(0, LOG_OPT_TRACE_STACK,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_INTERNAL_ERROR,
            "msg",          "%s", "Wrong dl_list_t, MUST be empty",
            NULL
        );
    }
}

/***************************************************************
 *   Return number of items in list
 ***************************************************************/
PUBLIC size_t dl_size(dl_list_t *dl)
{
    if(!dl) {
        return 0;
    }
    return dl->__itemsInContainer__;
}



                        /*---------------------------------*
                         *      SECTION: memory
                         *---------------------------------*/




/***************************************************************************
 *     Set memory functions
 ***************************************************************************/
PUBLIC int gobj_set_allocators(
    sys_malloc_fn_t malloc_func,
    sys_realloc_fn_t realloc_func,
    sys_calloc_fn_t calloc_func,
    sys_free_fn_t free_func
) {
    sys_malloc_fn = malloc_func;
    sys_realloc_fn = realloc_func;
    sys_calloc_fn = calloc_func;
    sys_free_fn = free_func;

    return 0;
}

/***************************************************************************
 *     Get memory functions
 ***************************************************************************/
PUBLIC int gobj_get_allocators(
    sys_malloc_fn_t *malloc_func,
    sys_realloc_fn_t *realloc_func,
    sys_calloc_fn_t *calloc_func,
    sys_free_fn_t *free_func
) {
    if(malloc_func) {
        *malloc_func = sys_malloc_fn;
    }
    if(realloc_func) {
        *realloc_func = sys_realloc_fn;
    }
    if(calloc_func) {
        *calloc_func = sys_calloc_fn;
    }
    if(free_func) {
        *free_func = sys_free_fn;
    }

    return 0;
}

/***********************************************************************
 *      Get memory functions
 ***********************************************************************/
PUBLIC sys_malloc_fn_t gobj_malloc_func(void) { return sys_malloc_fn; }
PUBLIC sys_realloc_fn_t gobj_realloc_func(void) { return sys_realloc_fn; }
PUBLIC sys_calloc_fn_t gobj_calloc_func(void) { return sys_calloc_fn; }
PUBLIC sys_free_fn_t gobj_free_func(void) { return sys_free_fn; }
#define EXTRA_MEM 0  // 32=OK, XXX

/***********************************************************************
 *      Alloc memory
 ***********************************************************************/
PRIVATE void *_mem_malloc(size_t size)
{
    size_t extra = sizeof(size_t) + EXTRA_MEM;
    size += extra;

    if(size > __max_block__) {
        gobj_log_error(0, LOG_OPT_TRACE_STACK,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_MEMORY_ERROR,
            "msg",          "%s", "SIZE GREATER THAN MAX_BLOCK",
            "size",         "%ld", (long)size,
            "max_block",    "%d", (int)__max_block__,
            NULL
        );
        return NULL;
    }

    __cur_system_memory__ += size;

    if(__cur_system_memory__ > __max_system_memory__) {
        gobj_log_critical(0, LOG_OPT_ABORT,
            "function",             "%s", __FUNCTION__,
            "msgset",               "%s", MSGSET_MEMORY_ERROR,
            "msg",                  "%s", "REACHED MAX_SYSTEM_MEMORY",
            "cur_system_memory",    "%ld", (long)__cur_system_memory__,
            "max_system_memory",    "%ld", (long)__max_system_memory__,
            NULL
        );
    }
    char *pm = calloc(1, size);
    if(!pm) {
        gobj_log_critical(0, LOG_OPT_ABORT,
            "function",             "%s", __FUNCTION__,
            "msgset",               "%s", MSGSET_MEMORY_ERROR,
            "msg",                  "%s", "NO MEMORY calloc() failed",
            "size",                 "%ld", (long)size,
            NULL
        );
    }
    size_t *pm_ = (size_t*)pm;
    *pm_ = size;

    pm += extra;

    return pm;
}

/***********************************************************************
 *      Free memory
 ***********************************************************************/
PRIVATE void _mem_free(void *p)
{
    if(!p) {
        return; // El comportamiento como free() es que no salga error; lo quito por libuv (uv_try_write)
    }
    size_t extra = sizeof(size_t) + EXTRA_MEM;

    char *pm = p;
    pm -= extra;

    size_t *pm_ = (size_t*)pm;
    size_t size = *pm_;

    __cur_system_memory__ -= size;
    free(pm);
}

/***************************************************************************
 *     ReAlloca memoria del core
 ***************************************************************************/
PRIVATE void *_mem_realloc(void *p, size_t new_size)
{
    /*---------------------------------*
     *  realloc admit p null
     *---------------------------------*/
    if(!p) {
        return _mem_malloc(new_size);
    }

    size_t extra = sizeof(size_t) + EXTRA_MEM;
    new_size += extra;

    char *pm = p;
    pm -= extra;

    size_t *pm_ = (size_t*)pm;
    size_t size = *pm_;

    __cur_system_memory__ -= size;

    __cur_system_memory__ += new_size;
    if(__cur_system_memory__ > __max_system_memory__) {
        gobj_log_critical(0, LOG_OPT_ABORT,
            "function",             "%s", __FUNCTION__,
            "msgset",               "%s", MSGSET_MEMORY_ERROR,
            "msg",                  "%s", "REACHED MAX_SYSTEM_MEMORY",
            "cur_system_memory",    "%ld", (long)__cur_system_memory__,
            "max_system_memory",    "%ld", (long)__max_system_memory__,
            NULL
        );
    }
    char *pm__ = realloc(pm, new_size);
    if(!pm__) {
        gobj_log_critical(0, LOG_OPT_ABORT,
            "function",             "%s", __FUNCTION__,
            "msgset",               "%s", MSGSET_MEMORY_ERROR,
            "msg",                  "%s", "NO MEMORY realloc() failed",
            "new_size",             "%ld", (long)new_size,
            NULL
        );
    }
    pm = pm__;
    pm_ = (size_t*)pm;
    *pm_ = new_size;
    pm += extra;
    return pm;
}

/***************************************************************************
 *     duplicate a substring
 ***************************************************************************/
PRIVATE void *_mem_calloc(size_t n, size_t size)
{
    size_t total = n * size;
    return sys_malloc_fn(total);
}

/***************************************************************************
 *     duplicate a substring
 ***************************************************************************/
PUBLIC char *gobj_strndup(const char *str, size_t size)
{
    char *s;
    size_t len;

    /*-----------------------------------------*
     *     Check null string
     *-----------------------------------------*/
    if(!str) {
        gobj_log_error(0, LOG_OPT_TRACE_STACK,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_PARAMETER_ERROR,
            "msg",          "%s", "str is NULL",
            NULL
        );
        return NULL;
    }

    /*-----------------------------------------*
     *     Alloca memoria
     *-----------------------------------------*/
    len = size;
    s = (char *)sys_malloc_fn(len+1);
    if(!s) {
        return NULL;
    }

    /*-----------------------------------------*
     *     Copia el substring
     *-----------------------------------------*/
    strncpy(s, str, len);

    return s;
}

/***************************************************************************
 *     Duplica un string
 ***************************************************************************/
PUBLIC char *gobj_strdup(const char *string)
{
    if(!string) {
        return NULL;
    }
    return gobj_strndup(string, strlen(string));
}

/*************************************************************************
 *  Return the maximum memory that you can get
 *************************************************************************/
PUBLIC size_t gobj_get_maximum_block(void)
{
    return __max_block__;
}
