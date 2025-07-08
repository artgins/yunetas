/****************************************************************************
 *          gobj.c
 *
 *          Objects G for Yuneta Simplified
 *
 *          Copyright (c) 2023 Niyamaka.
 *          Copyright (c) 2024, ArtGins.
 *          All Rights Reserved.
 ****************************************************************************/
#include <string.h>
#include <ctype.h>
#include <stdio.h>
#include <stddef.h>

#ifdef __linux__
    #include <pwd.h>
    #include <strings.h>
    #include <sys/utsname.h>
    #include <unistd.h>
#endif

#include "command_parser.h"
#include "stats_parser.h"
#include "testing.h"
#include "gobj.h"

extern void jsonp_free(void *ptr);

/***************************************************************
 *              Constants
 ***************************************************************/
#ifdef ESP_PLATFORM
    #define GOBJ_NAME_MAX 15
#else
    #define GOBJ_NAME_MAX 255
#endif

/***************************************************************
 *              GClass/GObj Structures
 ***************************************************************/
/*
 *  trace_level_t is used to describe trace levels
 */
typedef struct event_action_s {
    DL_ITEM_FIELDS

    gobj_event_t event_name;
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
} obflag_t;

typedef struct gclass_s {
    DL_ITEM_FIELDS

    gclass_name_t gclass_name;
    dl_list_t dl_states;            // FSM list states and their ev/action/next
    dl_list_t dl_events;            // FSM list of events (gobj_event_t, event_flag_t)
    const GMETHODS *gmt;            // Global methods
    const LMETHOD *lmt;

    const sdata_desc_t *attrs_table;
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
    BOOL fsm_checked;
} gclass_t;

typedef struct gobj_s {
    DL_ITEM_FIELDS

    int __refs__;
    gclass_t *gclass;
    struct gobj_s *parent;
    dl_list_t dl_children;

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
GOBJ_DEFINE_EVENT(EV_SEND_IEV);
GOBJ_DEFINE_EVENT(EV_SEND_EMAIL);
GOBJ_DEFINE_EVENT(EV_DROP);
GOBJ_DEFINE_EVENT(EV_REMOTE_LOG);

// Channel Messages: Output Events (Publications)
GOBJ_DEFINE_EVENT(EV_ON_OPEN);
GOBJ_DEFINE_EVENT(EV_ON_CLOSE);
GOBJ_DEFINE_EVENT(EV_ON_MESSAGE);      // with GBuffer
GOBJ_DEFINE_EVENT(EV_ON_COMMAND);
GOBJ_DEFINE_EVENT(EV_ON_IEV_MESSAGE);  // with IEvent, old EV_IEV_MESSAGE
GOBJ_DEFINE_EVENT(EV_ON_ID);
GOBJ_DEFINE_EVENT(EV_ON_ID_NAK);
GOBJ_DEFINE_EVENT(EV_ON_HEADER);

GOBJ_DEFINE_EVENT(EV_HTTP_MESSAGE);

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
GOBJ_DEFINE_STATE(ST_WAIT_TXED);
GOBJ_DEFINE_STATE(ST_WAIT_DISCONNECTED);
GOBJ_DEFINE_STATE(ST_WAIT_STOPPED);
GOBJ_DEFINE_STATE(ST_WAIT_HANDSHAKE);
GOBJ_DEFINE_STATE(ST_WAIT_IMEI);
GOBJ_DEFINE_STATE(ST_WAIT_ID);
GOBJ_DEFINE_STATE(ST_STOPPED);
GOBJ_DEFINE_STATE(ST_SESSION);
GOBJ_DEFINE_STATE(ST_IDLE);
GOBJ_DEFINE_STATE(ST_WAIT_RESPONSE);
GOBJ_DEFINE_STATE(ST_OPENED);
GOBJ_DEFINE_STATE(ST_CLOSED);

GOBJ_DEFINE_STATE(ST_WAITING_HANDSHAKE);
GOBJ_DEFINE_STATE(ST_WAITING_FRAME_HEADER);
GOBJ_DEFINE_STATE(ST_WAITING_PAYLOAD_DATA);

/***************************************************************
 *              Prototypes
 ***************************************************************/
PRIVATE state_t *_find_state(gclass_t *gclass, gobj_state_t state_name);
PRIVATE int _register_service(gobj_t *gobj);
PRIVATE int _deregister_service(gobj_t *gobj);
PRIVATE int write_json_parameters(
    gobj_t *gobj,
    json_t *kw,     // not own
    json_t *jn_global  // not own
);
PRIVATE json_t *extract_all_mine(
     const char *gclass_name,
     const char *gobj_name,
     json_t *kw     // not own
);
PRIVATE int rc_walk_by_tree(
    dl_list_t *iter,
    walk_type_t walk_type,
    cb_walking_t cb_walking,
    void *user_data,
    void *user_data2
);
PRIVATE int rc_walk_by_list(
    dl_list_t *iter,
    walk_type_t walk_type,
    cb_walking_t cb_walking,
    void *user_data,
    void *user_data2
);
PRIVATE int rc_walk_by_level(
    dl_list_t *iter,
    walk_type_t walk_type,
    cb_walking_t cb_walking,
    void *user_data,
    void *user_data2
);

PRIVATE json_t *sdata_create(gobj_t *gobj, const sdata_desc_t* schema);
PRIVATE int set_default(gobj_t *gobj, json_t *sdata, const sdata_desc_t *it);
PUBLIC void trace_vjson(
    hgobj gobj,
    int priority,
    json_t *jn_data,    // not owned
    const char *msgset,
    const char *fmt,
    va_list ap
);
PRIVATE event_action_t *_find_event_action(state_t *state, gobj_event_t event);
PRIVATE int _add_event_type(
    dl_list_t *dl,
    gobj_event_t event_name,
    event_flag_t event_flag
);

PRIVATE json_t *bit2level(
    const trace_level_t *internal_trace_level,
    const trace_level_t *user_trace_level,
    uint32_t bit
);
PRIVATE uint32_t level2bit(
    const trace_level_t *internal_trace_level,
    const trace_level_t *user_trace_level,
    const char *level
);
PRIVATE int json2item(
    gobj_t *gobj,
    json_t *sdata,
    const sdata_desc_t *it,
    json_t *jn_value // not owned
);

PRIVATE hgobj gobj_create_tree0(
    hgobj parent_,
    json_t *jn_tree // owned
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

PRIVATE const char *sdata_flag_names[] = {
    "SDF_NOTACCESS",
    "SDF_RD",
    "SDF_WR",
    "SDF_REQUIRED",
    "SDF_PERSIST",
    "SDF_VOLATIL",
    "SDF_RESOURCE",
    "SDF_PKEY",
    "SDF_FUTURE1",
    "SDF_FUTURE2",
    "SDF_WILD_CMD",
    "SDF_STATS",
    "SDF_FKEY",
    "SDF_RSTATS",
    "SDF_PSTATS",
    "SDF_AUTHZ_R",
    "SDF_AUTHZ_W",
    "SDF_AUTHZ_X",
    "SDF_AUTHZ_P",
    "SDF_AUTHZ_S",
    "SDF_AUTHZ_RS",
    0
};

PUBLIC const char *event_flag_names[] = { // Strings of event_flag_t type
    "EVF_NO_WARN_SUBS",
    "EVF_OUTPUT_EVENT",
    "EVF_SYSTEM_EVENT",
    "EVF_PUBLIC_EVENT",
    0
};

PRIVATE const char *gclass_flag_names[] = {
    "gcflag_manual_start",
    "gcflag_no_check_output_events",
    "gcflag_ignore_unknown_attrs",
    "gcflag_required_start_to_play",
    "gcflag_singleton",
    0
};

PRIVATE const char *gobj_flag_names[] = {
    "gobj_flag_yuno",
    "gobj_flag_default_service",
    "gobj_flag_service",
    "gobj_flag_volatil",
    "gobj_flag_pure_child",
    "gobj_flag_autostart",
    "gobj_flag_autoplay",
    0
};

/*
 *  System (gobj) output events
 */
PRIVATE dl_list_t dl_global_event_types;

PRIVATE int  __inside__ = 0;  // it's a counter
PRIVATE volatile int  __shutdowning__ = 0;
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
PRIVATE authorization_checker_fn __global_authorization_checker_fn__ = 0;
PRIVATE authentication_parser_fn __global_authentication_parser_fn__ = 0;

PRIVATE json_t *__jn_global_settings__ = 0;
PRIVATE int (*__global_startup_persistent_attrs_fn__)(void) = 0;
PRIVATE void (*__global_end_persistent_attrs_fn__)(void) = 0;
PRIVATE int (*__global_load_persistent_attrs_fn__)(hgobj gobj, json_t *keys) = 0;
PRIVATE int (*__global_save_persistent_attrs_fn__)(hgobj gobj, json_t *keys) = 0;
PRIVATE int (*__global_remove_persistent_attrs_fn__)(hgobj gobj, json_t *keys) = 0;
PRIVATE json_t * (*__global_list_persistent_attrs_fn__)(hgobj gobj, json_t *keys) = 0;

PRIVATE dl_list_t dl_gclass = {0};
PRIVATE json_t *__jn_services__ = 0;        // Dict service:(json_int_t)(size_t)gobj
PRIVATE dl_list_t dl_trans_filter = {0};
PRIVATE int trace_machine_format = 1;       // 0 legacy, 1 simpler
/*
 *  Global trace levels
 */
PRIVATE const trace_level_t s_global_trace_level[16] = {
    {"machine",         "Trace machine"},
    {"create_delete",   "Trace create/delete of gobjs"},
    {"create_delete2",  "Trace create/delete of gobjs level 2: with kw"},
    {"subscriptions",   "Trace subscriptions of gobjs"},
    {"start_stop",      "Trace start/stop of gobjs"},
    {"liburing",        "Trace liburing mixins"},
    {"ev_kw",           "Trace event keywords"},
    {"authzs",          "Trace authorizations"},
    {"states",          "Trace change of states"},
    {"gbuffers",        "Trace gbuffers"},
    {"timer_periodic",  "Trace periodic timers"},
    {"timer",           "Trace timers"},
    {"liburing_timer",  "Trace liburing timer"},
    {"fs",              "Trace file system"},
    {0, 0}
};

#define __trace_gobj_create_delete__(gobj)  (gobj_trace_level(gobj) & TRACE_CREATE_DELETE)
#define __trace_gobj_create_delete2__(gobj) (gobj_trace_level(gobj) & TRACE_CREATE_DELETE2)
#define __trace_gobj_subscriptions__(gobj)  (gobj_trace_level(gobj) & TRACE_SUBSCRIPTIONS)
#define __trace_gobj_start_stop__(gobj)     (gobj_trace_level(gobj) & TRACE_START_STOP)
#define __trace_gobj_ev_kw__(gobj)          (gobj_trace_level(gobj) & TRACE_EV_KW)
#define __trace_gobj_authzs__(gobj)         (gobj_trace_level(gobj) & TRACE_AUTHZS)
#define __trace_gobj_states__(gobj)         (gobj_trace_level(gobj) & TRACE_STATES)

PRIVATE uint32_t __global_trace_level__ = 0;
PRIVATE uint32_t __global_trace_no_level__ = 0;
PRIVATE volatile uint32_t __deep_trace__ = 0;

PRIVATE gobj_t * __yuno__ = 0;
PRIVATE gobj_t * __default_service__ = 0;

/*---------------------------------------------*
 *      Global authz levels TODO review all authz
 *---------------------------------------------*/
PRIVATE sdata_desc_t pm_read_attr[] = {
/*-PM----type-----------name------------flag------------default-----description---------- */
SDATAPM (DTP_STRING,    "path",         0,              0,          "Attribute path"),
SDATA_END()
};
PRIVATE sdata_desc_t pm_write_attr[] = {
/*-PM----type-----------name------------flag------------default-----description---------- */
SDATAPM (DTP_STRING,    "path",         0,              0,          "Attribute path"),
SDATA_END()
};
PRIVATE sdata_desc_t pm_exec_cmd[] = {
/*-PM----type-----------name------------flag------------default-----description---------- */
SDATAPM (DTP_STRING,    "command",      0,              0,          "Command name"),
SDATAPM (DTP_JSON,      "kw",           0,              0,          "command's kw"),
SDATA_END()
};
PRIVATE sdata_desc_t pm_inject_event[] = {
/*-PM----type-----------name------------flag------------default-----description---------- */
SDATAPM (DTP_STRING,    "event",        0,              0,          "Event name"),
SDATAPM (DTP_JSON,      "kw",           0,              0,          "event's kw"),
SDATA_END()
};
PRIVATE sdata_desc_t pm_subs_event[] = {
/*-PM----type-----------name------------flag------------default-----description---------- */
SDATAPM (DTP_STRING,    "event",        0,              0,          "Event name"),
SDATAPM (DTP_JSON,      "kw",           0,              0,          "event's kw"),
SDATA_END()
};
PRIVATE sdata_desc_t pm_read_stats[] = {
/*-PM----type-----------name------------flag------------default-----description---------- */
SDATAPM (DTP_STRING,    "stats",        0,              0,          "Stats name"),
SDATAPM (DTP_JSON,      "kw",           0,              0,          "stats's kw"),
SDATA_END()
};
PRIVATE sdata_desc_t pm_reset_stats[] = {
/*-PM----type-----------name------------flag------------default-----description---------- */
SDATAPM (DTP_STRING,    "stats",        0,              0,          "Stats name"),
SDATAPM (DTP_JSON,      "kw",           0,              0,          "stats's kw"),
SDATA_END()
};

PRIVATE sdata_desc_t global_authz_table[] = {
/*-AUTHZ-- type---------name--------------------flag----alias---items---description--*/
SDATAAUTHZ (DTP_SCHEMA, "__read_attribute__",   0,      0,      pm_read_attr, "Authorization to read gobj's attributes"),
SDATAAUTHZ (DTP_SCHEMA, "__write_attribute__",  0,      0,      pm_write_attr, "Authorization to write gobj's attributes"),
SDATAAUTHZ (DTP_SCHEMA, "__execute_command__",  0,      0,      pm_exec_cmd, "Authorization to execute gobj's commands"),
SDATAAUTHZ (DTP_SCHEMA, "__inject_event__",     0,      0,      pm_inject_event, "Authorization to inject events to gobj"),
SDATAAUTHZ (DTP_SCHEMA, "__subscribe_event__",  0,      0,      pm_subs_event, "Authorization to subscribe events of gobj"),
SDATAAUTHZ (DTP_SCHEMA, "__read_stats__",       0,      0,      pm_read_stats, "Authorization to read gobj's stats"),
SDATAAUTHZ (DTP_SCHEMA, "__reset_stats__",      0,      0,      pm_reset_stats, "Authorization to reset gobj's stats"),
SDATA_END()
};




                    /*---------------------------------*
                     *      Start up functions
                     *---------------------------------*/




/***************************************************************************
 *  Must trace? TODO set FALSE in non-debug compilation
 ***************************************************************************/
PRIVATE inline BOOL is_machine_tracing(gobj_t * gobj, gobj_event_t event)
{
    if(__deep_trace__) {
        return TRUE;
    }
    if(!gobj) {
        return FALSE;
    }
    uint32_t trace =
        __global_trace_level__ & TRACE_MACHINE ||
        gobj->trace_level & TRACE_MACHINE ||
        gobj->gclass->trace_level & TRACE_MACHINE ||
        ((__global_trace_level__ & TRACE_TIMER_PERIODIC) && event == EV_TIMEOUT_PERIODIC) ||
        ((__global_trace_level__ & TRACE_TIMER) && event == EV_TIMEOUT);

    return trace?TRUE:FALSE;
}

/***************************************************************************
 *  Must no trace? TODO set FALSE in non-debug compilation
 ***************************************************************************/
PRIVATE inline BOOL is_machine_not_tracing(gobj_t * gobj, gobj_event_t event)
{
    if(__deep_trace__ > 1) {
        return FALSE;
    }
    if(!gobj) {
        return TRUE;
    }
    uint32_t no_trace =
        __global_trace_no_level__ & TRACE_MACHINE ||
        gobj->no_trace_level & TRACE_MACHINE ||
        gobj->gclass->no_trace_level & TRACE_MACHINE ||
        ((__global_trace_no_level__ & TRACE_TIMER_PERIODIC) && event == EV_TIMEOUT_PERIODIC) ||
        ((__global_trace_no_level__ & TRACE_TIMER) && event == EV_TIMEOUT);

    return no_trace?TRUE:FALSE;
}

/***************************************************************************
 *  Initialize the yuno
 ***************************************************************************/
PUBLIC int gobj_start_up(
    int                         argc_,                  /* pass main() arguments */
    char                        *argv_[],               /* pass main() arguments */
    const json_t                *jn_global_settings,    /* NOT owned */
    const persistent_attrs_t    *persistent_attrs,
    json_function_fn            global_command_parser,  /* if NULL, use internal command parser */
    json_function_fn            global_statistics_parser, /* if NULL, use internal stats parser */
    authorization_checker_fn    global_authorization_checker, /* authorization checker function */
    authentication_parser_fn    global_authentication_parser  /* authentication parser function */
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

    glog_init();

    __shutdowning__ = 0;
    __jn_global_settings__ =  json_deep_copy(jn_global_settings);

    __global_startup_persistent_attrs_fn__ = persistent_attrs?persistent_attrs->startup:NULL;
    __global_end_persistent_attrs_fn__ = persistent_attrs?persistent_attrs->end:NULL;
    __global_load_persistent_attrs_fn__ = persistent_attrs?persistent_attrs->load:NULL;
    __global_save_persistent_attrs_fn__ = persistent_attrs?persistent_attrs->save:NULL;
    __global_remove_persistent_attrs_fn__ = persistent_attrs?persistent_attrs->remove:NULL;
    __global_list_persistent_attrs_fn__ = persistent_attrs?persistent_attrs->list:NULL;
    __global_command_parser_fn__ = global_command_parser;
    __global_stats_parser_fn__ = global_statistics_parser;
    __global_authorization_checker_fn__ = global_authorization_checker;
    __global_authentication_parser_fn__ = global_authentication_parser;

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
    event_type_t global_events[] = {
        {EV_STATE_CHANGED,  EVF_SYSTEM_EVENT|EVF_OUTPUT_EVENT|EVF_NO_WARN_SUBS},
        {0, 0}
    };
    dl_init(&dl_global_event_types, 0);

    event_type_t *event_types = global_events;
    while(event_types && event_types->event_name) {
        _add_event_type(&dl_global_event_types, event_types->event_name, event_types->event_flag);
        event_types++;
    }

    dl_init(&dl_gclass, 0);
    __jn_services__ = json_object();

    dl_init(&dl_trans_filter, 0);

    kw_add_binary_type(
        "gbuffer",
        "__gbuffer___",
        (serialize_fn_t)gbuffer_serialize,
        (deserialize_fn_t)gbuffer_deserialize,
        (incref_fn_t)gbuffer_incref,
        (decref_fn_t)gbuffer_decref
    );

    __initialized__ = TRUE;

    return 0;
}

/***************************************************************************
 *  shutdown the yuno
 ***************************************************************************/
PUBLIC void gobj_shutdown(void)
{
    if(__shutdowning__) {
        return;
    }
    __shutdowning__ = 1;

    if(__yuno__ && !(__yuno__->obflag & obflag_destroying)) {
        if(gobj_is_playing(__yuno__)) {
            // It will pause default_service, WARNING too slow for big configurations!!
            gobj_pause(__yuno__);
        }

        gobj_pause_autoplay_services();
        gobj_stop_autostart_services();

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
 *  set return code to exit when running as daemon
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
 *  De-initialize the yuno
 ***************************************************************************/
PUBLIC void gobj_end(void)
{
    if(!__initialized__) {
        return;
    }

    if(__yuno__) {
        gobj_log_info(0, 0,
            "msgset",           "%s", MSGSET_STARTUP,
            "msg",              "%s", "Yuno stopped, gobj end",
            "hostname",         "%s", get_hostname(),
            "node_uuid",        "%s", node_uuid(),
            "yuno",             "%s", gobj_yuno_role_plus_name(),
            NULL
        );
        gobj_destroy(__yuno__);
        __yuno__ = 0;
    }

    gclass_t *gclass;
    while((gclass = dl_first(&dl_gclass))) {
        gclass_unregister(gclass);
    }

    event_type_t *event_type;
    while((event_type = dl_first(&dl_global_event_types))) {
        dl_delete(&dl_global_event_types, event_type, 0);
        GBMEM_FREE(event_type);
    }

    JSON_DECREF(__jn_services__)
    JSON_DECREF(__jn_global_settings__)

    comm_prot_free();

    __initialized__ = FALSE;
}




                    /*---------------------------------*
                     *      GClass functions
                     *---------------------------------*/




/***************************************************************************
 *
 ***************************************************************************/
PUBLIC hgclass gclass_create( // create and register gclass
    gclass_name_t gclass_name,
    event_type_t *event_types,
    states_t *states,
    const GMETHODS *gmt,
    const LMETHOD *lmt,
    const sdata_desc_t *attrs_table,
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
        abort();
    }
#endif

    gclass_t *gclass = GBMEM_MALLOC(sizeof(*gclass));
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
    gclass->gclass_name = gclass_name;  // DANGER gobj_strdup(gclass_name); 14/11/2024
    gclass->gmt = gmt;
    gclass->lmt = lmt;
    gclass->attrs_table = attrs_table;
    gclass->priv_size = priv_size;
    gclass->authz_table = authz_table;
    gclass->command_table = command_table;
    gclass->s_user_trace_level = s_user_trace_level;
    gclass->gclass_flag = gclass_flag;

    /*----------------------------------------*
     *          Build States
     *----------------------------------------*/
    states_t *state = states;
    while(state->state_name) {
        if(gclass_add_state(gclass, state->state_name)<0) {
            // Error already logged
            gclass_unregister(gclass);
            return NULL;
        }

        ev_action_t *ev_action_list = state->ev_action_list;
        while(ev_action_list->event) {
            gclass_add_ev_action(
                gclass,
                state->state_name,
                ev_action_list->event,
                ev_action_list->action,
                ev_action_list->next_state
            );
            ev_action_list++;
        }

        state++;
    }

    /*----------------------------------------*
     *          Build Events
     *----------------------------------------*/
    event_type_t *event_type = event_types;
    while(event_type->event_name) {
        if(gclass_find_event_type(gclass, event_type->event_name)) {
            gobj_log_error(0, 0,
                "function",     "%s", __FUNCTION__,
                "msgset",       "%s", MSGSET_INTERNAL_ERROR,
                "msg",          "%s", "SMachine: event repeated in input_events",
                "gclass",       "%s", gclass->gclass_name,
                "event",        "%s", event_type->event_name,
                NULL
            );
            gclass_unregister(gclass);
            return NULL;
        }

        gclass_add_event_type(gclass, event_type->event_name, event_type-> event_flag);
        event_type++;
    }

    /*----------------------------------------*
     *          Check FSM
     *----------------------------------------*/
    if(!gclass->fsm_checked) {
        if(gclass_check_fsm(gclass)<0) {
            gclass_unregister(gclass);
            return NULL;
        }
        gclass->fsm_checked = TRUE;
    }

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

    if(_find_state(gclass, state_name)) {
        gobj_log_error(NULL, LOG_OPT_TRACE_STACK,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_PARAMETER_ERROR,
            "msg",          "%s", "state already exists",
            "state",        "%s", state_name,
            NULL
        );
        return -1;
    }


    state_t *state = GBMEM_MALLOC(sizeof(*state));
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
PRIVATE state_t *_find_state(gclass_t *gclass, gobj_state_t state_name)
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
PRIVATE event_action_t *_find_event_action(state_t *state, gobj_event_t event_name)
{
    event_action_t *event_action = dl_first(&state->dl_actions);
    while(event_action) {
        if(event_name == event_action->event_name) {
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
    gobj_event_t event_name,
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

    state_t *state = _find_state(gclass, state_name);
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

    if(_find_event_action(state, event_name)) {
        gobj_log_error(NULL, LOG_OPT_TRACE_STACK,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_INTERNAL_ERROR,
            "msg",          "%s", "event already exists",
            "gclass",       "%s", gclass->gclass_name,
            "state",        "%s", state_name,
            "event",        "%s", event_name,
            NULL
        );
        return -1;
    }

    event_action_t *event_action = GBMEM_MALLOC(sizeof(*event_action));
    if(event_action == NULL) {
        gobj_log_error(NULL, LOG_OPT_TRACE_STACK,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_MEMORY_ERROR,
            "msg",          "%s", "No memory",
            "gclass",       "%s", gclass->gclass_name,
            "state",        "%s", state_name,
            "event",        "%s", event_name,
            "size",         "%d", (int)sizeof(*state),
            NULL
        );
        return -1;
    }

    event_action->event_name = event_name;
    event_action->action = action;
    event_action->next_state = next_state;

    dl_add(&state->dl_actions, event_action);

    return 0;
}

/***************************************************************************
 *
 ***************************************************************************/
PUBLIC int gclass_add_event_type(
    hgclass gclass_,
    gobj_event_t event_name,
    event_flag_t event_flag
)
{
    gclass_t *gclass = gclass_;

    return _add_event_type(&gclass->dl_events, event_name, event_flag);
}

/***************************************************************************
 *  Find an event in any gclass
 ***************************************************************************/
PUBLIC event_type_t *gclass_find_event(const char *event, event_flag_t event_flag, BOOL verbose)
{
    gclass_t *gclass = dl_first(&dl_gclass);
    while(gclass) {
        event_t *event_ = dl_first(&gclass->dl_events);
        while(event_) {
            if(!event_flag || (event_->event_type.event_flag & event_flag)) {
                if(event_->event_type.event_name && strcasecmp(event_->event_type.event_name, event)==0) {
                    return &event_->event_type;
                }
            }
            event_ = dl_next(event_);
        }
        gclass = dl_next(gclass);
    }
    if(verbose) {
        gobj_log_error(NULL, LOG_OPT_TRACE_STACK,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_PARAMETER_ERROR,
            "msg",          "%s", "EVENT NOT FOUND",
            "event",        "%s", event,
            NULL
        );
    }
    return NULL;
}

/***************************************************************************
 *  Find a public event in any gclass
 ***************************************************************************/
PUBLIC gobj_event_t gclass_find_public_event(const char *event, BOOL verbose)
{
    event_type_t *event_type = gclass_find_event(event, EVF_PUBLIC_EVENT, verbose);
    if(event_type) {
        return event_type->event_name;
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
            GBMEM_FREE(event_action);
        }

        GBMEM_FREE(state);
    }

    event_type_t *event_type;
    while((event_type = dl_first(&gclass->dl_events))) {
        dl_delete(&gclass->dl_events, event_type, 0);
        GBMEM_FREE(event_type);
    }

    dl_delete(&dl_gclass, gclass, gobj_free_func());
}

/***************************************************************************
 *
 ***************************************************************************/
PUBLIC gclass_name_t gclass_gclass_name(hgclass gclass_)
{
    gclass_t *gclass = gclass_;
    if(!gclass) {
        // Silence
        return NULL;
    }
    return gclass->gclass_name;
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
PUBLIC json_t *gclass_gclass_register(void)
{
    json_t *jn_register = json_array();

    gclass_t *gclass = dl_first(&dl_gclass);
    while(gclass) {
        json_t *jn_gclass = json_object();
        json_object_set_new(
            jn_gclass,
            "gclass",
            json_string(gclass->gclass_name)
        );
        json_object_set_new(
            jn_gclass,
            "instances",
            json_integer(gclass->instances)
        );

        json_array_append_new(jn_register, jn_gclass);
        gclass = dl_next(gclass);
    }

    return jn_register;
}

/***************************************************************************
 *
 ***************************************************************************/
PUBLIC hgclass gclass_find_by_name(gclass_name_t gclass_name) // gclass_name can be 'char *' or gclass_name_t
{
    if(!gclass_name) {
        // Silence
        return NULL;
    }

    /*
     *  Find firstly by name type
     */
    gclass_t *gclass = dl_first(&dl_gclass);
    while(gclass) {
        if(gclass_name == gclass->gclass_name) {
            return gclass;
        }
        gclass = dl_next(gclass);
    }

    /*
     *  Find firstly by name str
     */
    gclass = dl_first(&dl_gclass);
    while(gclass) {
        if(strcasecmp(gclass_name, gclass->gclass_name)==0) {
            return gclass;
        }
        gclass = dl_next(gclass);
    }

    return NULL;
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE int _add_event_type(
    dl_list_t *dl,
    gobj_event_t event_name,
    event_flag_t event_flag
) {
    if(empty_string(event_name)) {
        return -1;
    }

    event_t *event = GBMEM_MALLOC(sizeof(*event));
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

    event->event_type.event_name = event_name;
    event->event_type.event_flag = event_flag;

    return dl_add(dl, event);
}

/***************************************************************************
 *
 ***************************************************************************/
PUBLIC event_type_t *gclass_find_event_type(hgclass gclass_, gobj_event_t event)
{
    gclass_t *gclass = gclass_;
    event_t *event_ = dl_first(&gclass->dl_events);
    while(event_) {
        if(event_->event_type.event_name && event_->event_type.event_name == event) {
            return &event_->event_type;
        }
        event_ = dl_next(event_);
    }
    return 0;
}

/***************************************************************************
 *  Check smachine
 ***************************************************************************/
PUBLIC int gclass_check_fsm(hgclass gclass_)
{
    gclass_t *gclass = gclass_;
    int ret = 0;

    /*
     *  check states
     */
    if(!dl_size(&gclass->dl_states)) {
        gobj_log_error(0,0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_INTERNAL_ERROR,
            "msg",          "%s", "GClass without states",
            "gclass",       "%s", gclass->gclass_name,
            NULL
        );
        ret += -1;
    }

    /*
     *  check state's event in input_list
     */
    state_t *state = dl_first(&gclass->dl_states);
    while(state) {
        // gobj_state_t state_name;
        // dl_list_t dl_actions;

        event_action_t *event_action = dl_first(&state->dl_actions);
        while(event_action) {
            // gobj_event_t event;
            // gobj_action_fn action;
            // gobj_state_t next_state;
            event_type_t *event_type = gclass_find_event_type(
                gclass, event_action->event_name
            );
            if(!event_type) {
                gobj_log_error(0, 0,
                    "function",     "%s", __FUNCTION__,
                    "msgset",       "%s", MSGSET_INTERNAL_ERROR,
                    "msg",          "%s", "SMachine: state's event NOT in input_events",
                    "gclass",       "%s", gclass->gclass_name,
                    "state",        "%s", state->state_name,
                    "event",        "%s", event_action->event_name,
                    NULL
                );
                ret += -1;
            }

            if(event_action->next_state) {
                state_t *next_state = _find_state(gclass, event_action->next_state);
                if(!next_state) {
                    gobj_log_error(0, 0,
                        "function",     "%s", __FUNCTION__,
                        "msgset",       "%s", MSGSET_INTERNAL_ERROR,
                        "msg",          "%s", "SMachine: next state NOT in state names",
                        "gclass",       "%s", gclass->gclass_name,
                        "state",        "%s", state->state_name,
                        "next_state",   "%s", event_action->next_state,
                        NULL
                    );
                    ret += -1;
                }
            }

            /*
             *  Next: event/action/state
             */
            event_action = dl_next(event_action);
        }

        /*
         *  Next: state
         */
        state = dl_next(state);
    }

    /*
     *  check input_list's event in state
     */
    event_t *event_ = dl_first(&gclass->dl_events);
    while(event_) {
        // gobj_event_t event_type.event_name;
        // event_flag_t event_type.event_flag;

        if(!(event_->event_type.event_flag & EVF_OUTPUT_EVENT)) {
            BOOL found = FALSE;
            state = dl_first(&gclass->dl_states);
            while(state) {
                event_action_t *ev_ac = _find_event_action(state, event_->event_type.event_name);
                if(ev_ac) {
                    found = TRUE;
                }
                state = dl_next(state);
            }

            if(!found) {
                gobj_log_error(0, 0,
                    "function",     "%s", __FUNCTION__,
                    "msgset",       "%s", MSGSET_INTERNAL_ERROR,
                    "msg",          "%s", "SMachine: input_list's event NOT in state",
                    "gclass",       "%s", gclass->gclass_name,
                    "event",        "%s", event_->event_type.event_name,
                    NULL
                );
                ret += -1;
            }
        }

        /*
         *  Next: event
         */
        event_ = dl_next(event_);
    }

    return ret;
}




                    /*---------------------------------*
                     *      GObj creation functions
                     *---------------------------------*/




/***************************************************************************
 *  TODO IEvent_cli=C_IEVENT_CLI remove when agent is migrated to YunetaS
 ***************************************************************************/
PRIVATE const char *old_to_new_gclass_name(const char *gclass_name)
{
    if(strcasecmp(gclass_name, "IEvent_cli")==0) {
        return "C_IEVENT_CLI";
    } else if(strcasecmp(gclass_name, "IOGate")==0) {
        return "C_IOGATE";
    } else if(strcasecmp(gclass_name, "Channel")==0) {
        return "C_CHANNEL";
    } if(strcasecmp(gclass_name, "GWebSocket")==0) {
        return "C_WEBSOCKET";
    }
    return gclass_name;
}

/***************************************************************************
 *
 ***************************************************************************/
PUBLIC json_t *gobj_service_register(void)
{
    json_t *jn_register = json_array();

    const char *key; json_t *jn_service;
    json_object_foreach(__jn_services__, key, jn_service) {
        gobj_t *gobj = (gobj_t *)(size_t)json_integer_value(jn_service);
        json_t *jn_srv = json_object();

        json_object_set_new(
            jn_srv,
            "gclass",
            json_string(gobj_gclass_name(gobj))
        );
        json_object_set_new(
            jn_srv,
            "service",
            json_string(key)
        );
        json_array_append_new(jn_register, jn_srv);
    }

    return jn_register;
}

/***************************************************************************
 *
 ***************************************************************************/
PUBLIC hgobj gobj_create2(
    const char *gobj_name,
    gclass_name_t gclass_name,
    json_t *kw, // owned
    hgobj parent_,
    gobj_flag_t gobj_flag
) {
    gobj_t *parent = parent_;

    /*--------------------------------*
     *      Check parameters
     *--------------------------------*/
    if(strlen(gobj_name) > GOBJ_NAME_MAX) {
        gobj_log_error(NULL, LOG_OPT_TRACE_STACK,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_PARAMETER_ERROR,
            "msg",          "%s", "gobj_name name TOO LONG",
            "gobj_name",    "%s", gobj_name,
            "len",          "%d", (int)strlen(gobj_name),
            NULL
        );
#ifdef ESP_PLATFORM
        abort();
#else
        JSON_DECREF(kw)
        return NULL;
#endif
    }

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
            gobj_log_error(0, LOG_OPT_TRACE_STACK,
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
            gobj_log_error(0, LOG_OPT_TRACE_STACK,
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
        if(__default_service__) {
            gobj_log_error(0, LOG_OPT_TRACE_STACK,
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

    // TODO IEvent_cli=C_IEVENT_CLI remove when agent is migrated to YunetaS
    gclass_name = old_to_new_gclass_name(gclass_name);

    gclass_t *gclass = gclass_find_by_name(gclass_name);
    if(!gclass) {
        gobj_log_error(NULL, LOG_OPT_TRACE_STACK,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_PARAMETER_ERROR,
            "msg",          "%s", "gclass NOT FOUND",
            "gclass",       "%s", gclass_name,
            "gobj_name",    "%s", gobj_name,
            NULL
        );
        json_t *jn_gclasses = gclass_gclass_register();
        gobj_trace_json(0, jn_gclasses, "gclass NOT FOUND");
        JSON_DECREF(jn_gclasses)
        JSON_DECREF(kw)
        return NULL;
    }

    // TODO implement gcflag_singleton

    /*--------------------------------*
     *      Alloc memory
     *--------------------------------*/
    gobj_t *gobj = GBMEM_MALLOC(sizeof(*gobj));
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
    dl_init(&gobj->dl_children, gobj);
    gobj->dl_subscribings = json_array();
    gobj->dl_subscriptions = json_array();
    gobj->current_state = dl_first(&gclass->dl_states);
    gobj->last_state = 0;
    gobj->obflag = 0;
    gobj->gobj_flag = gobj_flag;

    if(__trace_gobj_create_delete__(gobj)) {
         trace_machine("💙💙⏩ creating: %s^%s (%s%s%s%s%s%s)",
            gclass->gclass_name,
            gobj_name,
            (gobj_flag & gobj_flag_default_service)? "DefaultService,":"",
            (gobj_flag & gobj_flag_service)? "Service,":"",
            (gobj_flag & gobj_flag_volatil)? "Volatil,":"",
            (gobj_flag & gobj_flag_pure_child)? "PureChild,":"",
            (gobj_flag & gobj_flag_autostart)? "AutoStart,":"",
            (gobj_flag & gobj_flag_autoplay)? "AutoPlay,":""
        );
    }

    gobj->gobj_name = gobj_strdup(gobj_name);
    gobj->jn_attrs = sdata_create(gobj, gclass->attrs_table);
    gobj->jn_stats = json_object();
    gobj->jn_user_data = json_object();
    gobj->priv = gclass->priv_size? GBMEM_MALLOC(gclass->priv_size):NULL;

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

    /*--------------------------*
     *  Register service
     *--------------------------*/
    if(gobj->gobj_flag & (gobj_flag_service)) {
        _register_service(gobj);
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
        dl_add(&parent->dl_children, gobj);
    }

    /*--------------------------------*
     *     Mark as created
     *--------------------------------*/
    gobj->obflag |= obflag_created;
    gobj->gclass->instances++;

    /*--------------------------------*
     *  Write configuration
     *  WARNING collateral damage:
     *      write_json_parameters was before "Mark as created"!
     *--------------------------------*/
    write_json_parameters(gobj, kw, __jn_global_settings__);

    /*--------------------------------------*
     *  Load writable and persistent attrs
     *  of services and __root__
     *--------------------------------------*/
    if(gobj->gobj_flag & (gobj_flag_service)) {
        if(__global_load_persistent_attrs_fn__) {
            __global_load_persistent_attrs_fn__(gobj, 0);
        }
    }

    /*--------------------------------*
     *      Exec mt_create
     *--------------------------------*/
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
PUBLIC hgobj gobj_create_yuno(
    const char *gobj_name,
    gclass_name_t gclass_name,
    json_t *kw // owned
) {
    return gobj_create2(gobj_name, gclass_name, kw, NULL, gobj_flag_yuno);
}

PUBLIC hgobj gobj_create_service(
    const char *gobj_name,
    gclass_name_t gclass_name,
    json_t *kw, // owned
    hgobj parent
) {
    return gobj_create2(gobj_name, gclass_name, kw, parent, gobj_flag_service);
}

PUBLIC hgobj gobj_create_default_service(
    const char *gobj_name,
    gclass_name_t gclass_name,
    json_t *kw, // owned
    hgobj parent
) {
    return gobj_create2(
        gobj_name,
        gclass_name,
        kw,
        parent,
        gobj_flag_default_service|gobj_flag_autostart
    );
}

PUBLIC hgobj gobj_create_volatil(
    const char *gobj_name,
    gclass_name_t gclass_name,
    json_t *kw, // owned
    hgobj parent
) {
    return gobj_create2(
        gobj_name,
        gclass_name,
        kw,
        parent,
        gobj_flag_volatil
    );
}

PUBLIC hgobj gobj_create_pure_child(
    const char *gobj_name,
    gclass_name_t gclass_name,
    json_t *kw, // owned
    hgobj parent
) {
    return gobj_create2(
        gobj_name,
        gclass_name,
        kw,
        parent,
        gobj_flag_pure_child
    );
}

PUBLIC hgobj gobj_create(
    const char *gobj_name,
    gclass_name_t gclass_name,
    json_t *kw, // owned
    hgobj parent
) {
    return gobj_create2(
        gobj_name,
        gclass_name,
        kw,
        parent,
        0
    );
}

/***************************************************************************
 *  Find a json value in the list.
 *  Return index or -1 if not found or the index relative to 0.
 ***************************************************************************/
PRIVATE int json_list_find(json_t *list, json_t *value)
{
    size_t idx_found = -1;
    size_t flags = JSON_COMPACT|JSON_ENCODE_ANY;;
    size_t index;
    json_t *_value;
    char *s_found_value = json_dumps(value, flags);
    if(s_found_value) {
        json_array_foreach(list, index, _value) {
            char *s_value = json_dumps(_value, flags);
            if(s_value) {
                if(strcmp(s_value, s_found_value)==0) {
                    idx_found = index;
                    jsonp_free(s_value);
                    break;
                } else {
                    jsonp_free(s_value);
                }
            }
        }
        jsonp_free(s_found_value);
    }
    return idx_found;
}

/***************************************************************************
 *  Check if a list is a integer range:
 *      - must be a list of two integers (first < second)
 ***************************************************************************/
PRIVATE BOOL json_is_range(json_t *list)
{
    if(json_array_size(list) != 2)
        return FALSE;

    json_int_t first = json_integer_value(json_array_get(list, 0));
    json_int_t second = json_integer_value(json_array_get(list, 1));
    if(first <= second) {
        return TRUE;
    } else {
        return FALSE;
    }
}

/***************************************************************************
 *  Return a expanded integer range
 ***************************************************************************/
PRIVATE json_t *json_range_list(json_t *list)
{
    if(!json_is_range(list))
        return 0;
    json_int_t first = json_integer_value(json_array_get(list, 0));
    json_int_t second = json_integer_value(json_array_get(list, 1));
    json_t *range = json_array();
    for(int i=first; i<=second; i++) {
        json_t *jn_int = json_integer(i);
        json_array_append_new(range, jn_int);
    }
    return range;
}

/***************************************************************************
 *  Extend array values.
 *  If as_set is TRUE then not repeated values
 ***************************************************************************/
PRIVATE int json_list_update(json_t *list, json_t *other, BOOL as_set)
{
    if(!json_is_array(list) || !json_is_array(other)) {
        return -1;
    }
    size_t index;
    json_t *value;
    json_array_foreach(other, index, value) {
        if(as_set) {
            int idx = json_list_find(list, value);
            if(idx < 0) {
                json_array_append(list, value);
            }
        } else {
            json_array_append(list, value);
        }
    }
    return 0;
}

/***************************************************************************
 *  Build a list (set) with lists of integer ranges.
 *  [[#from, #to], [#from, #to], #integer, #integer, ...] -> list
 *  WARNING: Arrays of two integers are considered range of integers.
 *  Arrays of one or more of two integers are considered individual integers.
 *
 *  Return the json list
 ***************************************************************************/
PRIVATE json_t *json_listsrange2set(json_t *listsrange)
{
    if(!json_is_array(listsrange)) {
        return 0;
    }
    json_t *ln_list = json_array();

    size_t index;
    json_t *value;
    json_array_foreach(listsrange, index, value) {
        if(json_is_integer(value)) {
            // add new integer item
            if(json_list_find(ln_list, value)<0) {
                json_array_append(ln_list, value);
            }
        } else if(json_is_array(value)) {
            // add new integer list or integer range
            if(json_is_range(value)) {
                json_t *range = json_range_list(value);
                if(range) {
                    json_list_update(ln_list, range, TRUE);
                    json_decref(range);
                }
            } else {
                json_list_update(ln_list, value, TRUE);
            }
        } else {
            // ignore the rest
            continue;
        }
    }

    return ln_list;
}

/***************************************************************************
 *  Expand a dict group to a list [^^children^^]
 ***************************************************************************/
PRIVATE int expand_children_list(hgobj gobj, json_t *kw)
{
    json_t * __range__ = json_object_get(kw, "__range__");
    json_t * __vars__ = json_object_get(kw, "__vars__");
    json_t * __content__ = json_object_get(kw, "__content__");
    if(!__range__ || !__vars__ || !__content__) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_PARAMETER_ERROR,
            "msg",          "%s", "Expand [^^children^^] needs of __range__, __vars__ and __content__ keys",
            NULL
        );
        return -1;
    }

    if(json_is_integer(__range__)) {
        json_int_t max_range = json_integer_value(__range__);
        for(json_int_t range=1; range<=max_range; range++) {
            char temp[64];
            snprintf(temp, sizeof(temp), "%" JSON_INTEGER_FORMAT , range);
            json_object_set_new(__vars__, "__range__", json_string(temp));
            json_t *jn_new_content = json_deep_copy(__content__);
            json_t *jn_child_tree = json_replace_var(jn_new_content, json_incref(__vars__));
            if(jn_child_tree) {
                gobj_create_tree0(
                    gobj,
                    jn_child_tree  // owned
                );
            }
        }
    } else {
        json_t *jn_set = json_listsrange2set(__range__); // TODO TOO SLOW, review, leave as legacy
        json_t *value;
        int index;
        json_array_foreach(jn_set, index, value) {
            char temp[64];
            json_int_t range = json_integer_value(value);
            snprintf(temp, sizeof(temp), "%" JSON_INTEGER_FORMAT , range);
            json_object_set_new(__vars__, "__range__", json_string(temp));
            json_t *jn_new_content = json_deep_copy(__content__);
            json_t *jn_child_tree = json_replace_var(jn_new_content, json_incref(__vars__));
            if(jn_child_tree) {
                gobj_create_tree0(
                    gobj,
                    jn_child_tree  // owned
                );
            }
        }
        json_decref(jn_set);
    }

    return 0;
}


/***************************************************************************
 *  Create tree
 ***************************************************************************/
PRIVATE hgobj gobj_create_tree0(
    hgobj parent__,
    json_t *jn_tree // owned
)
{
    gobj_t *parent = parent__;
    gobj_flag_t gobj_flag = 0;
    const char *gclass_name = kw_get_str(parent, jn_tree, "gclass", "", KW_REQUIRED);
    const char *name = kw_get_str(parent, jn_tree, "name", "", 0);
    BOOL default_service = kw_get_bool(parent, jn_tree, "default_service", 0, 0);
    BOOL as_service = kw_get_bool(parent, jn_tree, "as_service", 0, 0) ||
        kw_get_bool(parent, jn_tree, "service", 0, 0);
    BOOL autostart = kw_get_bool(parent, jn_tree, "autostart", 0, 0);
    BOOL autoplay = kw_get_bool(parent, jn_tree, "autoplay", 0, 0);
    BOOL disabled = kw_get_bool(parent, jn_tree, "disabled", 0, 0);
    BOOL pure_child = kw_get_bool(parent, jn_tree, "pure_child", 0, 0);

    // TODO IEvent_cli=C_IEVENT_CLI remove when agent is migrated to YunetaS V7
    gclass_name = old_to_new_gclass_name(gclass_name);

    gclass_t *gclass = gclass_find_by_name(gclass_name);
    if(!gclass) {
        gobj_log_error(parent, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_PARAMETER_ERROR,
            "msg",          "%s", "gclass NOT FOUND",
            "gclass_name",  "%s", gclass_name,
            "name",         "%s", name?name:"",
            NULL
        );
        json_t *jn_gclasses = gclass_gclass_register();
        gobj_trace_json(0, jn_gclasses, "gclass NOT FOUND");
        JSON_DECREF(jn_gclasses)
        JSON_DECREF(jn_tree)
        return 0;
    }
    BOOL local_kw = FALSE;
    json_t *kw = kw_get_dict(parent, jn_tree, "kw", 0, 0);
    if(!kw) {
        local_kw = TRUE;
        kw = json_object();
    }

    if(gclass_has_attr(gclass, "subscriber")) {
        if(!kw_has_key(kw, "subscriber")) { // WARNING TOO implicit
            if(parent != gobj_yuno()) {
                json_object_set_new(kw, "subscriber", json_integer((json_int_t)(size_t)parent));
            }
        } else {
            json_t *jn_subscriber = kw_get_dict_value(parent, kw, "subscriber", 0, 0);
            if(json_is_string(jn_subscriber)) {
                const char *subscriber_name = json_string_value(jn_subscriber);
                hgobj subscriber = gobj_find_service(subscriber_name, FALSE);
                if(subscriber) {
                    json_object_set_new(kw, "subscriber", json_integer((json_int_t)(size_t)subscriber));
                } else {
                    gobj_log_error(parent, 0,
                        "function",     "%s", __FUNCTION__,
                        "msgset",       "%s", MSGSET_INTERNAL_ERROR,
                        "msg",          "%s", "subscriber service gobj NOT FOUND",
                        "subscriber",   "%s", subscriber_name,
                        "name",         "%s", name?name:"",
                        "gclass",       "%s", gclass_name,
                        NULL
                    );
                }
            } else {
                gobj_log_error(parent, 0,
                    "function",     "%s", __FUNCTION__,
                    "msgset",       "%s", MSGSET_INTERNAL_ERROR,
                    "msg",          "%s", "subscriber json INVALID",
                    "name",         "%s", name?name:"",
                    "gclass",       "%s", gclass_name,
                    NULL
                );
            }
        }
    }

    if(!local_kw) {
        json_incref(kw);
    }
    if(default_service) {
        gobj_flag |= gobj_flag_default_service;
    }
    if(as_service) {
        gobj_flag |= gobj_flag_service;
    }
    if(autoplay) {
        gobj_flag |= gobj_flag_autoplay;
    }
    if(autostart) {
        gobj_flag |= gobj_flag_autostart;
    }
    if(pure_child) {
        gobj_flag |= gobj_flag_pure_child;
    }

    hgobj gobj = gobj_create2(name, gclass_name, kw, parent, gobj_flag);
    if(!gobj) {
        gobj_log_error(parent, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_INTERNAL_ERROR,
            "msg",          "%s", "_create_gobj() FAILED",
            "name",         "%s", name?name:"",
            "gclass",       "%s", gclass_name,
            NULL
        );
        JSON_DECREF(jn_tree);
        return 0;
    }
    if(disabled) {
        gobj_disable(gobj);
    }

    /*
     *  Children
     */
    hgobj last_child = 0;
    json_t *jn_children = kw_get_list(parent, jn_tree, "children", 0, 0);
    if(!jn_children) {
        // TODO remove when agent is migrated to YunetaS
        jn_children = kw_get_list(parent, jn_tree, "zchilds", 0, 0);
    }

    size_t index;
    json_t *jn_child;
    json_array_foreach(jn_children, index, jn_child) {
        if(!json_is_object(jn_child)) {
            continue;
        }
        json_incref(jn_child);
        last_child = gobj_create_tree0(
            gobj,
            jn_child
        );
        if(!last_child) {
            gobj_log_error(parent, 0,
                "function",     "%s", __FUNCTION__,
                "msgset",       "%s", MSGSET_INTERNAL_ERROR,
                "msg",          "%s", "gobj_create_tree0() FAILED",
                "jn_child",     "%j", jn_child,
                NULL
            );
            JSON_DECREF(jn_tree)
            return 0;
        }
    }

    /*
     *  [^^children^^]
     */
    json_t *jn_expand_children = kw_get_dict(parent, jn_tree, "[^^children^^]", 0, 0);
    if(jn_expand_children) {
        expand_children_list(gobj, jn_expand_children);
    } else {
        if(json_array_size(jn_children) == 1) {
            gobj_set_bottom_gobj(gobj, last_child);
        }
    }

    JSON_DECREF(jn_tree)
    return gobj;
}

/***************************************************************************
 *  Factory to create service gobj
 *  Used in entry_point, to run services
 *  Internally it uses gobj_create_tree0()
 *  A set of global variables returned by gobj_global_variables()
 *  are added to ``__json_config_variables__``.
 ***************************************************************************/
PUBLIC hgobj gobj_service_factory(
    const char *name,
    json_t *kw  // owned
)
{
    const char *gclass_name = kw_get_str(0, kw, "gclass", 0, 0);
    if(empty_string(gclass_name)) {
        gobj_log_error(0,0,
            "gobj",         "%s", __FILE__,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_PARAMETER_ERROR,
            "msg",          "%s", "service gclass EMPTY",
            "service",      "%s", name?name:"",
            NULL
        );
        JSON_DECREF(kw)
        return 0;
    }

    // TODO IEvent_cli=C_IEVENT_CLI remove when agent is migrated to YunetaS
    gclass_name = old_to_new_gclass_name(gclass_name);

    gclass_t *gclass = gclass_find_by_name(gclass_name);
    if(!gclass) {
        gobj_log_error(0, 0,
            "gobj",         "%s", __FILE__,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_PARAMETER_ERROR,
            "msg",          "%s", "gclass NOT FOUND",
            "service",      "%s", name?name:"",
            "gclass",       "%s", gclass_name,
            NULL
        );
        json_t *jn_gclasses = gclass_gclass_register();
        gobj_trace_json(0, jn_gclasses, "gclass NOT FOUND");
        JSON_DECREF(jn_gclasses)
        gobj_trace_json(0, kw, "gclass NOT FOUND");
        JSON_DECREF(kw)
        return 0;
    }
    const char *gobj_name = kw_get_str(0, kw, "name", 0, 0);

    /*
     *  Get any own config of global conf
     */
    json_t *jn_global_mine = extract_all_mine(
        gclass_name,
        gobj_name,
        __jn_global_settings__
    );

    if(__trace_gobj_create_delete2__(__yuno__)) {
        trace_machine("🌞 %s^%s => global_mine",
            gclass_name,
            gobj_name
        );
        gobj_trace_json(0, jn_global_mine, "global_mine");
    }

    /*
     *  Extract special __json_config_variables__ if exits
     */
    json_t *__json_config_variables__ = kw_get_dict(0,
        jn_global_mine,
        "__json_config_variables__",
        json_object(),
        KW_EXTRACT
    );

    /*
     *  Join kw with own global
     */
    json_object_update_new(
        kw,
        jn_global_mine
    );

    /*
     *  Join __json_config_variables__ with global variables
     */
    json_object_update_new(
        __json_config_variables__,
        gobj_global_variables()
    );

    json_object_set_new(kw, "service", json_true()); // Mark as service

    if(__trace_gobj_create_delete2__(__yuno__)) {
        trace_machine("🌞🌞 %s^%s => global_mine",
            gclass_name,
            gobj_name
        );
        gobj_trace_json(0, kw, "kw with global_mine");
        gobj_trace_json(0, __json_config_variables__, "__json_config_variables__");
    }

    /*
     *  Replace attribute vars
     */
    json_t * new_kw = json_replace_var(
        kw,          // owned
        __json_config_variables__   // owned
    );

    /*
     *  Final conf
     */
    if(__trace_gobj_create_delete2__(__yuno__)) {
        trace_machine("🔰🔰🔰 %s^%s => final kw",
            gclass_name,
            gobj_name
        );
        gobj_trace_json(0, new_kw, "final kw");
    }

    hgobj gobj = gobj_create_tree0(
        __yuno__,
        new_kw  // owned
    );

    return gobj;
}

/***************************************************************************
 *  Create gobj tree
 *  Parse tree_config and call gobj_create_tree0()
 *  Used by ycommand,ybatch,ystats,ytests,cli,...
 ***************************************************************************/
PUBLIC hgobj gobj_create_tree(
    hgobj parent_,
    const char *tree_config,        // Can have comments #^^.
    json_t *json_config_variables   // owned
)
{
    gobj_t *parent = parent_;

    if(!parent) {
        gobj_log_error(parent_, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_PARAMETER_ERROR,
            "msg",          "%s", "parent null",
            NULL);
        return (hgobj) 0;
    }

    char *config = GBMEM_STRDUP(tree_config);
    helper_quote2doublequote(config);

    json_t *jn_tree_config = json_config(
        0,
        0,
        0,          // fixed_config
        config,     // variable_config
        0,          // config_json_file
        0,          // parameter_config
        PEF_CONTINUE
    );
    GBMEM_FREE(config)

    json_t *jn_tree_config2 = json_replace_var(
        jn_tree_config,         // owned
        json_config_variables   // owned
    );

    return gobj_create_tree0(parent, jn_tree_config2);
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
    if(gobj->__refs__ > 0) {
        gobj_log_error(0, LOG_OPT_TRACE_STACK,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_PARAMETER_ERROR,
            "msg",          "%s", "gobj DESTROYING with references",
            "refs",         "%d", gobj->__refs__,
            NULL
        );
    }

    if(gobj->obflag & obflag_destroying) {
        gobj_log_error(0, LOG_OPT_TRACE_STACK,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_PARAMETER_ERROR,
            "msg",          "%s", "gobj ALREADY DESTROYING",
            "shutdowning",  "%d", gobj_is_shutdowning(),
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
        _deregister_service(gobj);
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
    gobj_unsubscribe_list(gobj, json_incref(gobj->dl_subscriptions), TRUE);
    gobj_unsubscribe_list(gobj, json_incref(gobj->dl_subscribings), TRUE);

    /*--------------------------------*
     *      Delete from parent
     *--------------------------------*/
    if(parent) {
        dl_delete(&gobj->parent->dl_children, gobj, 0);
        if(gobj_is_volatil(gobj)) {
            if (gobj_bottom_gobj(gobj->parent) == gobj && !gobj_is_destroying(gobj->parent)) {
                gobj_set_bottom_gobj(gobj->parent, NULL);
            }
        }
    }

    /*--------------------------------*
     *      Delete children
     *--------------------------------*/
    gobj_destroy_children(gobj);

    /*-------------------------------------------------*
     *  Exec mt_destroy
     *  Call this after all children are destroyed.
     *  Then you can delete resources used by children
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
    JSON_DECREF(gobj->jn_attrs)
    JSON_DECREF(gobj->jn_stats)
    JSON_DECREF(gobj->jn_user_data)
    JSON_DECREF(gobj->dl_subscribings)
    JSON_DECREF(gobj->dl_subscriptions)

    EXEC_AND_RESET(gobj_free_func(), gobj->gobj_name)
    EXEC_AND_RESET(gobj_free_func(), gobj->full_name)
    EXEC_AND_RESET(gobj_free_func(), gobj->short_name)
    EXEC_AND_RESET(gobj_free_func(), gobj->priv)

    if(gobj->obflag & obflag_created) {
        gobj->gclass->instances--;
    }
    GBMEM_FREE(gobj);
}

/***************************************************************************
 *  Destroy children
 ***************************************************************************/
PUBLIC void gobj_destroy_children(hgobj gobj_)
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

    gobj_t *child = dl_first(&gobj->dl_children);
    while(child) {
        gobj_t *next = dl_next(child);
        if(!(child->obflag & (obflag_destroyed|obflag_destroying))) {
            gobj_destroy(child);
        } else {
            gobj_log_error(0, LOG_OPT_TRACE_STACK,
                "function",     "%s", __FUNCTION__,
                "msgset",       "%s", MSGSET_PARAMETER_ERROR,
                "msg",          "%s", "Destroying a destroyed child",
                NULL
            );
        }
        /*
         *  Next
         */
        child = next;
    }
}

/***************************************************************************
 *  register service gobj
 ***************************************************************************/
PRIVATE int _register_service(gobj_t *gobj)
{
    /*
     *  gobj_name to lower, make case-insensitive
     */
    char service_name[GOBJ_NAME_MAX+1];
    snprintf(service_name, sizeof(service_name), "%s", gobj->gobj_name?gobj->gobj_name:"");
    strntolower(service_name, strlen(service_name));

    if(json_object_get(__jn_services__, service_name)) {
        gobj_t *prev_gobj = (hgobj)(size_t)json_integer_value(
            json_object_get(__jn_services__, service_name)
        );

        gobj_log_error(0, LOG_OPT_TRACE_STACK,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_PARAMETER_ERROR,
            "msg",          "%s", "service ALREADY REGISTERED. Will be UPDATED",
            "prev gclass",  "%s", gobj_gclass_name(prev_gobj),
            "gclass",       "%s", gobj_gclass_name(gobj),
            "gobj_name",    "%s", gobj->gobj_name,
            "service",      "%s", service_name,
            NULL
        );
    }

    int ret = json_object_set_new(
        __jn_services__,
        service_name,
        json_integer((json_int_t)(size_t)gobj)
    );
    if(ret == -1) {
        gobj_log_error(0, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_JSON_ERROR,
            "msg",          "%s", "json_object_set_new() FAILED",
            "gclass",       "%s", gobj_gclass_name(gobj),
            "gobj_name",    "%s", gobj->gobj_name,
            "service",      "%s", service_name,
            NULL
        );
    }

    return ret;
}

/***************************************************************************
 *  deregister service
 ***************************************************************************/
PRIVATE int _deregister_service(gobj_t *gobj)
{
    /*
     *  gobj_name to lower, make case-insensitive
     */
    char service_name[GOBJ_NAME_MAX+1];
    snprintf(service_name, sizeof(service_name), "%s", gobj->gobj_name?gobj->gobj_name:"");
    strntolower(service_name, strlen(service_name));

    json_t *jn_obj = json_object_get(__jn_services__, service_name);
    if(!jn_obj) {
        gobj_log_error(0, LOG_OPT_TRACE_STACK,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_PARAMETER_ERROR,
            "msg",          "%s", "service NOT found in register",
            "gclass",       "%s", gobj_gclass_name(gobj),
            "gobj_name",    "%s", gobj->gobj_name,
            "service",      "%s", service_name,
            NULL
        );
        return -1;
    }
    json_object_del(__jn_services__, service_name);

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
PRIVATE const char *is_for_me(
    const char *gclass_name,
    const char *gobj_name,
    const char *key)
{
    char *p = strchr(key, '.');
    if(p) {
        char temp[256]; // if your names are longer than 256 bytes, you are ...
        int ln = p - key;
        if (ln >= sizeof(temp)) {
            return 0; // ... very intelligent!
        }
        strncpy(temp, key, ln);
        temp[ln]=0;

        if(gobj_name && strcasecmp(gobj_name, temp)==0) {
            // match the name
            return p+1;
        }
        if(gclass_name && strcasecmp(gclass_name, temp)==0) {
            // match the gclass name
            return p+1;
        }
    }
    return 0;
}

/***************************************************************************
 *  ATTR: used in write_json_parameters() / gobj_service_factory()
 ***************************************************************************/
PRIVATE json_t *extract_all_mine(
    const char *gclass_name,
    const char *gobj_name,
    json_t *kw  // not own
) {
    json_t *kw_mine = json_object();
    const char *key;
    json_t *jn_value;

    json_object_foreach(kw, key, jn_value) {
        const char *pk = is_for_me(
            gclass_name,
            gobj_name,
            key
        );
        if(!pk) {
            continue;
        }
        if(strcasecmp(pk, "kw")==0) {
            json_object_update(kw_mine, jn_value);
        } else {
            json_object_set(kw_mine, pk, jn_value);
        }
    }

    return kw_mine;
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE int print_attr_not_found(hgobj gobj, const char *attr)
{
    gobj_log_error(gobj, LOG_OPT_TRACE_STACK,
        "function",     "%s", __FUNCTION__,
        "msgset",       "%s", MSGSET_PARAMETER_ERROR,
        "msg",          "%s", "GClass Attribute NOT FOUND",
        "gclass",       "%s", gobj_gclass_name(gobj),
        "attr",         "%s", attr,
        NULL
    );
    return 0;
}

/***************************************************************************
 *  Update sdata fields from a json dict.
 *  Constraint: if flag is -1
 *                  => all fields!
 *              else
 *                  => matched flag
 ***************************************************************************/
/*
 *  Not found callback
 */
typedef int (*not_found_cb_t)(hgobj gobj, const char *name);

PRIVATE int json2sdata(
    json_t *hsdata,
    json_t *kw,  // not owned
    sdata_flag_t flag,
    not_found_cb_t not_found_cb, // Called when the key not exist in hsdata
    gobj_t *gobj)
{
    int ret = 0;
    if(!hsdata || !kw) {
        gobj_log_error(gobj, LOG_OPT_TRACE_STACK,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_PARAMETER_ERROR,
            "msg",          "%s", "hsdata or kw NULL",
            NULL
        );
        return -1;
    }
    const char *key;
    json_t *jn_value;
    json_object_foreach(kw, key, jn_value) {
        const sdata_desc_t *it = gclass_attr_desc(gobj->gclass, key, FALSE);
        if(!it) {
            if(not_found_cb) {
                not_found_cb(gobj, key);
                ret--;
            }
            continue;
        }
        if(!(flag == -1 || (it->flag & flag))) {
            continue;
        }
        ret += json2item(gobj, hsdata, it, jn_value);
    }

    return ret;
}

/***************************************************************************
 *  ATTR:
 *  Get data from json kw parameter, and put the data in config sdata.

    Being services:
     Filter only the parameters in settings belongs to gobj.
     The gobj is searched by his named-gobj or his gclass name.
     The parameter name in settings, must be a dot-named,
     with the first item being the named-gobj o gclass name.

    Only services can have config with attributes variables (template)
        with global attributes belong to the service pass to the template.
        (feed the template)
        The non-service gobjs must be light, they can be thousands or more.
 *
 ***************************************************************************/
PRIVATE int write_json_parameters(
    gobj_t *gobj,
    json_t *kw,     // not own
    json_t *jn_global // not own
) {
    int ret = 0;
    json_t *hs = gobj_hsdata(gobj);

    if(!kw) {
        return -1;
    }

    if(gobj->gobj_flag & (gobj_flag_service)) {
        /*
         *  Services only
         */

        /*
         *  Get any own config of global conf
         */
        json_t *jn_global_mine = extract_all_mine(
            gobj->gclass->gclass_name,
            gobj->gobj_name,
            jn_global
        );

        if(__trace_gobj_create_delete2__(gobj)) {
            trace_machine("🔰 %s^%s => global_mine",
                gobj->gclass->gclass_name,
                gobj->gobj_name
            );
            gobj_trace_json(0, jn_global_mine, "global_mine");
        }

        /*
         *  Extract special __json_config_variables__ if exits
         */
        json_t *__json_config_variables__ = kw_get_dict(0,
            jn_global_mine,
            "__json_config_variables__",
            json_object(),
            KW_EXTRACT
        );

        /*
         *  Join kw with own global
         */
        json_object_update_new(
            kw,
            jn_global_mine
        );

        /*
         *  Join __json_config_variables__ with global variables
         */
        json_object_update_new(
            __json_config_variables__,
            gobj_global_variables()
        );

        if(__trace_gobj_create_delete2__(gobj)) {
            trace_machine("🔰🔰 %s^%s => global_mine",
                gobj->gclass->gclass_name,
                gobj->gobj_name
            );
            gobj_trace_json(0, kw, "kw with global_mine");
            gobj_trace_json(0, __json_config_variables__, "__json_config_variables__");
        }

        /*
         *  Replace attribute vars
         */
        json_t * new_kw = json_replace_var(
            json_incref(kw),            // owned
            __json_config_variables__   // owned
        );

        /*
         *  Final conf
         */
        if(__trace_gobj_create_delete2__(gobj)) {
            trace_machine("🔰🔰🔰 %s^%s => final kw",
                gobj->gclass->gclass_name,
                gobj->gobj_name
            );
            gobj_trace_json(gobj, new_kw, "final kw");
        }
        json_t *__user_data__ = kw_get_dict(gobj, new_kw, "__user_data__", 0, KW_EXTRACT);

        ret = json2sdata(
            hs,
            new_kw,
            -1,
            (gobj->gclass->gclass_flag & gcflag_ignore_unknown_attrs)?0:print_attr_not_found,
            gobj
        );

        if(__user_data__) {
            json_object_update(((gobj_t *)gobj)->jn_user_data, __user_data__);
            json_decref(__user_data__);
        }
        json_decref(new_kw);

    } else {
        /*
         *  Non Services
         */
        ret = json2sdata(
            hs,
            kw,
            -1,
            (gobj->gclass->gclass_flag & gcflag_ignore_unknown_attrs)?0:print_attr_not_found,
            gobj
        );
    }

    if(__trace_gobj_create_delete2__(gobj)) {
        trace_machine("🔰🔰🔰🔰 %s^%s => final hs",
            gobj->gclass->gclass_name,
            gobj->gobj_name
        );
        gobj_trace_json(gobj, hs, "final hs");
    }

    return ret;
}

/***************************************************************************
 *  load persistent and writable attrs
    attrs can be a string, a list of keys, or a dict with the keys to save/delete
    Only gobj services can load/save writable-persistent
 ***************************************************************************/
PUBLIC int gobj_load_persistent_attrs(  // str, list or dict. Only gobj services are loaded!.
    hgobj gobj_,
    json_t *jn_attrs  // owned
)
{
    gobj_t *gobj = gobj_;

    if(!(gobj->gobj_flag & (gobj_flag_service))) {
        gobj_log_warning(gobj, LOG_OPT_TRACE_STACK,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_PARAMETER_ERROR,
            "msg",          "%s", "Only gobj services can load/save persistent attrs",
            NULL
        );
        JSON_DECREF(jn_attrs)
        return -1;
    }
    if(__global_load_persistent_attrs_fn__) {
        return __global_load_persistent_attrs_fn__(gobj, jn_attrs);
    }
    JSON_DECREF(jn_attrs)
    return -1;
}

/***************************************************************************
 *  save persistent and writable attrs
    attrs can be a string, a list of keys, or a dict with the keys to save/delete
    Only gobj services can load/save writable-persistent
 ***************************************************************************/
PUBLIC int gobj_save_persistent_attrs(
    hgobj gobj_,
    json_t *jn_attrs // owned
)
{
    gobj_t *gobj = gobj_;

    if(!(gobj->gobj_flag & (gobj_flag_service))) {
        gobj_log_warning(gobj, LOG_OPT_TRACE_STACK,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_PARAMETER_ERROR,
            "msg",          "%s", "Only gobj services can load/save persistent attrs",
            NULL
        );
        JSON_DECREF(jn_attrs)
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
PUBLIC json_t *gobj_list_persistent_attrs(hgobj gobj, json_t *jn_attrs)
{
    json_t *jn_dict = json_object();

    if(!__global_list_persistent_attrs_fn__) {
        JSON_DECREF(jn_attrs)
        return jn_dict;
    }

    if(gobj) {
        json_t *jn_item = __global_list_persistent_attrs_fn__(
            gobj,
            json_incref(jn_attrs)
        );
        if(jn_item) {
            json_object_set_new(jn_dict, gobj_short_name(gobj), jn_item);
        }
    } else {
        const char *key; json_t *jn_service;
        json_object_foreach(__jn_services__, key, jn_service) {
            gobj_t *gobj_ = (gobj_t *)(size_t)json_integer_value(jn_service);

            json_t *jn_item = __global_list_persistent_attrs_fn__(
                gobj_,
                json_incref(jn_attrs)
            );
            if(jn_item) {
                json_object_set_new(jn_dict, gobj_short_name(gobj_), jn_item);
            }
        }
    }

    JSON_DECREF(jn_attrs)
    return jn_dict;
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
    const sdata_desc_t *it = gobj->gclass->attrs_table;
    while(it->name) {
        if(exclude_flag && (it->flag & exclude_flag)) {
            it++;
            continue;
        }
        if(include_flag == (sdata_flag_t)-1 || (it->flag & include_flag)) {
            set_default(gobj, gobj->jn_attrs, it);

            if((gobj->obflag & obflag_created) && !(gobj->obflag & obflag_destroyed)) {
                // Avoid call to mt_writing before mt_create!
                if(gobj->gclass->gmt->mt_writing) {
                    gobj->gclass->gmt->mt_writing(gobj, it->name);
                }
            }
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
            break;
        case DTP_BOOLEAN:
            jn_value = json_boolean(svalue);
            if(strcasecmp(svalue, "TRUE")==0) {
                jn_value = json_true();
            } else if(strcasecmp(svalue, "FALSE")==0) {
                jn_value = json_false();
            } else {
                jn_value = atoi(svalue)? json_true(): json_false();
            }
            break;
        case DTP_INTEGER:
            jn_value = json_integer(strtoll(svalue, NULL, 0));
            break;
        case DTP_REAL:
            jn_value = json_real(atof(svalue));
            break;
        case DTP_LIST:
            jn_value = anystring2json(svalue, strlen(svalue), FALSE);
            if(!json_is_array(jn_value)) {
                JSON_DECREF(jn_value)
                jn_value = json_array();
            }
            break;
        case DTP_DICT:
            jn_value = anystring2json(svalue, strlen(svalue), FALSE);
            if(!json_is_object(jn_value)) {
                JSON_DECREF(jn_value)
                jn_value = json_object();
            }
            break;
        case DTP_JSON:
            if(!empty_string(svalue)) {
                jn_value = anystring2json(svalue, strlen(svalue), FALSE);
            } else {
                jn_value = json_null();
            }
            break;
        case DTP_POINTER:
            jn_value = json_integer((json_int_t)(size_t)it->default_value);
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
    json_t *jn_value_ // not owned
)
{
    if(!it) {
        return -1;
    }
    json_t *jn_value2 = 0;

    switch(it->type) {
        case DTP_STRING:
            if(json_is_string(jn_value_)) {
                jn_value2 = json_incref(jn_value_);
            } else {
                char *s = json2uglystr(jn_value_);
                if(s) {
                    jn_value2 = json_string(s);
                    GBMEM_FREE(s)
                }
            }
            break;
        case DTP_BOOLEAN:
            if(json_is_boolean(jn_value_)) {
                jn_value2 = json_incref(jn_value_);
            } else {
                char *s = json2uglystr(jn_value_);
                if(s) {
                    if(strcasecmp(s, "TRUE")==0) {
                        jn_value2 = json_true();
                    } else if(strcasecmp(s, "FALSE")==0) {
                        jn_value2 = json_false();
                    } else {
                        jn_value2 = atoi(s)? json_true(): json_false();
                    }
                    GBMEM_FREE(s)
                }
            }
            break;
        case DTP_INTEGER:
            if(json_is_integer(jn_value_)) {
                jn_value2 = json_incref(jn_value_);
            } else if(json_is_string(jn_value_)) {
                char *s = json2uglystr(jn_value_);
                if(s) {
                    jn_value2 = json_integer(strtoll(s, NULL, 0));
                    GBMEM_FREE(s)
                }
            }
            break;
        case DTP_REAL:
            if(json_is_real(jn_value_)) {
                jn_value2 = json_incref(jn_value_);
            } else if(json_is_string(jn_value_)) {
                char *s = json2uglystr(jn_value_);
                if(s) {
                    jn_value2 = json_real(atof(s));
                    GBMEM_FREE(s)
                }
            }
            break;
        case DTP_LIST:
            if(json_is_array(jn_value_)) {
                jn_value2 = json_incref(jn_value_);
            } else if(json_is_string(jn_value_)) {
                char *s = json2uglystr(jn_value_);
                if(s) {
                    jn_value2 = string2json(s, TRUE);
                    GBMEM_FREE(s)
                }
            }

            if(!json_is_array(jn_value2)) {
                gobj_log_error(gobj, LOG_OPT_TRACE_STACK,
                    "function",     "%s", __FUNCTION__,
                    "msgset",       "%s", MSGSET_PARAMETER_ERROR,
                    "msg",          "%s", "attr must be an array",
                    "attr",         "%s", it->name,
                    NULL
                );
                json_decref(jn_value2);
                return -1;
            }
            break;

        case DTP_DICT:
            if(json_is_object(jn_value_)) {
                jn_value2 = json_incref(jn_value_);
            } else if(json_is_string(jn_value_)) {
                char *s = json2uglystr(jn_value_);
                if(s) {
                    jn_value2 = string2json(s, TRUE);
                    GBMEM_FREE(s)
                }
            }

            if(!json_is_object(jn_value2)) {
                gobj_log_error(gobj, LOG_OPT_TRACE_STACK,
                    "function",     "%s", __FUNCTION__,
                    "msgset",       "%s", MSGSET_PARAMETER_ERROR,
                    "msg",          "%s", "attr must be an object",
                    "attr",         "%s", it->name,
                    NULL
                );
                json_decref(jn_value2);
                return -1;
            }
            break;

        case DTP_JSON:
            jn_value2 = json_incref(jn_value_);
            break;
        case DTP_POINTER:
            if(!json_is_integer(jn_value_)) {
                gobj_log_error(gobj, LOG_OPT_TRACE_STACK,
                    "function",     "%s", __FUNCTION__,
                    "msgset",       "%s", MSGSET_PARAMETER_ERROR,
                    "msg",          "%s", "attr must be an integer",
                    "attr",         "%s", it->name,
                    NULL
                );
                return -1;
            }
            jn_value2 = json_incref(jn_value_);
            break;
    }

    if(json_object_set_new(sdata, it->name, jn_value2)<0) {
        gobj_log_error(gobj, LOG_OPT_TRACE_STACK,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_JSON_ERROR,
            "msg",          "%s", "json_object_set() FAILED",
            "attr",         "%s", it->name,
            NULL
        );
        return -1;
    }

    if(gobj->gclass->gmt->mt_writing) {
        if((gobj->obflag & obflag_created) && !(gobj->obflag & obflag_destroyed)) {
            // Avoid call to mt_writing before mt_create!
            gobj->gclass->gmt->mt_writing(gobj, it->name);
        }
    }

    // { TODO
    //     SDATA_METADATA_TYPE *p = item_metadata_pointer(sdata, it);
    //     SDATA_METADATA_TYPE m = *p;
    //     if((it->flag & (SDF_STATS|SDF_RSTATS|SDF_PSTATS))) {
    //         if(sdata->post_write_stats_cb) {
    //             sdata->post_write_stats_cb(sdata->user_data, it->name, it->type, old_value, value);
    //         }
    //     }
    // }

    return 0;
}

/***************************************************************************
 *
 ***************************************************************************/
PUBLIC json_t *gobj_hsdata(hgobj gobj_) // Return is NOT YOURS
{
    gobj_t *gobj = gobj_;
    return gobj?gobj->jn_attrs:NULL;
}

/***************************************************************************
 *
 ***************************************************************************/
PUBLIC const sdata_desc_t *gclass_authz_desc(hgclass gclass_)
{
    gclass_t *gclass = gclass_;
    return gclass->authz_table;
}

/*
 *  Attribute functions WITHOUT bottom inheritance
 */

/***************************************************************************
 *  Return the data description of the attribute `attr`
 *  If `attr` is null returns full attr's table
 ***************************************************************************/
PUBLIC const sdata_desc_t *gclass_attr_desc(hgclass gclass_, const char *attr, BOOL verbose)
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

    if(!attr) {
        return gclass->attrs_table;
    }
    const sdata_desc_t *it = gclass->attrs_table;
    while(it->name) {
        if(strcmp(it->name, attr)==0) {
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
            "attr",         "%s", attr,
            NULL
        );
    }
    return NULL;
}

/***************************************************************************
 *  Return the data description of the attribute `attr`
 *  If `attr` is null returns full attr's table
 ***************************************************************************/
PUBLIC const sdata_desc_t *gobj_attr_desc(hgobj gobj_, const char *attr, BOOL verbose)
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

    if(!attr) {
        return gobj->gclass->attrs_table;
    }

    return gclass_attr_desc(gobj->gclass, attr, verbose);
}

/***************************************************************************
 *
 ***************************************************************************/
PUBLIC data_type_t gobj_attr_type(hgobj gobj, const char *name)
{
    const sdata_desc_t *sdata_desc = gobj_attr_desc(gobj, name, FALSE);
    if(sdata_desc) {
        return sdata_desc->type;
    } else {
        return 0;
    }
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
 *  ATTR: write
 *  Reset rstats attributes
 ***************************************************************************/
PUBLIC int gobj_reset_rstats_attrs(hgobj gobj)
{
    return sdata_write_default_values(
        gobj,
        SDF_RSTATS,     // include_flag
        0               // exclude_flag
    );
}

/***************************************************************************
 *  ATTR: read str
 ***************************************************************************/
PUBLIC json_t *gobj_read_attr( // Return is NOT yours!
    hgobj gobj_,
    const char *name,
    hgobj src
) {
    gobj_t *gobj = gobj_;

    json_t *hs = gobj_hsdata2(gobj, name, FALSE);
    if(hs) {
        // TODO must be a item2json, to call mt_reading
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

    return NULL;
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

    const sdata_desc_t *it = gobj->gclass->attrs_table;
    while(it->name) {
        if(include_flag == (sdata_flag_t)-1 || (it->flag & include_flag)) {
            // TODO must be a item2json, to call mt_reading
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

    JSON_DECREF(jn_value)
    return ret;
}

/***************************************************************************
 *  ATTR: write
 ***************************************************************************/
PUBLIC int gobj_write_attrs(
    hgobj gobj,
    json_t *kw,  // owned
    sdata_flag_t include_flag,
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
        if(!(include_flag == (sdata_flag_t)-1 || (it->flag & include_flag))) {
            continue;
        }
        ret += json2item(gobj, hs, it, jn_value);
    }

    JSON_DECREF(kw)
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

/*
 *  Attribute functions WITH bottom inheritance
 */

/***************************************************************************
 *  ATTR: Get hsdata of inherited attribute.
 ***************************************************************************/
PUBLIC json_t *gobj_hsdata2(hgobj gobj_, const char *name, BOOL verbose)
{
    gobj_t *gobj = gobj_;

    if(!gobj) {
        gobj_log_error(gobj, LOG_OPT_TRACE_STACK,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_PARAMETER_ERROR,
            "msg",          "%s", "gobj NULL",
            NULL
        );
        return 0;
    }

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
    return NULL;
}

/***************************************************************************
 *
 ***************************************************************************/
PUBLIC BOOL gobj_has_bottom_attr(hgobj gobj_, const char *name)
{
    gobj_t *gobj = gobj_;

    if(gobj_has_attr(gobj, name)) {
        return TRUE;
    } else if(gobj && gobj->bottom_gobj) {
        return gobj_has_bottom_attr(gobj->bottom_gobj, name);
    }

    return FALSE;
}

/***************************************************************************
 *  ATTR: read str
 ***************************************************************************/
PUBLIC const char *gobj_read_str_attr(hgobj gobj_, const char *name)
{
    gobj_t *gobj = gobj_;

    if(name && strcasecmp(name, "__state__")==0) {
        return gobj_current_state(gobj);
    }

    json_t *hs = gobj_hsdata2(gobj, name, FALSE);
    if(hs) {
        if(gobj->gclass->gmt->mt_reading) {
            if(!(gobj->obflag & obflag_destroyed)) {
                SData_Value_t v = gobj->gclass->gmt->mt_reading(gobj, name);
                if(v.found) {
                    return v.v.s;
                }
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

    if(name) {
        if(strcasecmp(name, "__disabled__")==0) {
            return gobj_is_disabled(gobj);
        } else if(strcasecmp(name, "__running__")==0) {
            return gobj_is_running(gobj);
        } else if(strcasecmp(name, "__playing__")==0) {
            return gobj_is_playing(gobj);
        } else if(strcasecmp(name, "__service__")==0) {
            return gobj_is_service(gobj);
        }
    }

    json_t *hs = gobj_hsdata2(gobj, name, FALSE);
    if(hs) {
        if(gobj->gclass->gmt->mt_reading) {
            if(!(gobj->obflag & obflag_destroyed)) {
                SData_Value_t v = gobj->gclass->gmt->mt_reading(gobj, name);
                if(v.found) {
                    return v.v.b;
                }
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

    if(name && strcasecmp(name, "__trace_level__")==0) {
        return gobj_trace_level(gobj);
    }

    json_t *hs = gobj_hsdata2(gobj, name, FALSE);
    if(hs) {
        if(gobj->gclass->gmt->mt_reading) {
            if(!(gobj->obflag & obflag_destroyed)) {
                SData_Value_t v = gobj->gclass->gmt->mt_reading(gobj, name);
                if(v.found) {
                    return v.v.i;
                }
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
                SData_Value_t v = gobj->gclass->gmt->mt_reading(gobj, name);
                if(v.found) {
                    return v.v.f;
                }
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
                SData_Value_t v = gobj->gclass->gmt->mt_reading(gobj, name);
                if(v.found) {
                    return v.v.j;
                }
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
                SData_Value_t v = gobj->gclass->gmt->mt_reading(gobj, name);
                if(v.found) {
                    return v.v.p;
                }
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
        // WARNING value == 0  -> json_null()
        int ret = json_object_set_new(hs, name, value?json_string(value):json_null());
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
PUBLIC int gobj_write_strn_attr(hgobj gobj_, const char *name, const char *value_, size_t len)
{
    gobj_t *gobj = gobj_;

    json_t *hs = gobj_hsdata2(gobj, name, FALSE);
    if(hs) {
        char *value = GBMEM_STRNDUP(value_, len);
        if(!value) {
            gobj_log_error(gobj, LOG_OPT_TRACE_STACK,
                "function",     "%s", __FUNCTION__,
                "msgset",       "%s", MSGSET_MEMORY_ERROR,
                "msg",          "%s", "gbmem_strndup() FAILED",
                "gclass",       "%s", gobj_gclass_name(gobj),
                "attr",         "%s", name?name:"",
                "length",       "%d", len,
                NULL
            );
            return -1;
        }

        int ret = json_object_set_new(hs, name, json_string(value));
        if(gobj->gclass->gmt->mt_writing) {
            if((gobj->obflag & obflag_created) && !(gobj->obflag & obflag_destroyed)) {
                // Avoid call to mt_writing before mt_create!
                gobj->gclass->gmt->mt_writing(gobj, name);
            }
        }

        GBMEM_FREE(value);

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
    JSON_DECREF(jn_value)
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

    const sdata_desc_t *it = gobj->gclass->attrs_table;
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
 *  Execute internal method
 ***************************************************************************/
PUBLIC json_t *gobj_local_method(
    hgobj gobj_,
    const char *lmethod,
    json_t *kw,
    hgobj src
)
{
    gobj_t * gobj = gobj_;
    register const LMETHOD *lmt;

    if(!gobj || gobj->obflag & obflag_destroyed) {
        gobj_log_error(0, 0,
            "gobj",         "%s", __FILE__,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_PARAMETER_ERROR,
            "msg",          "%s", "hgobj NULL or DESTROYED",
            NULL
        );
        return 0;
    }

    lmt = gobj->gclass->lmt;
    while(lmt && lmt->lname != 0) {
        if(strncasecmp(lmt->lname, lmethod, strlen(lmt->lname))==0) {
            if(lmt->lm) {
                return (*lmt->lm)(gobj, lmethod, kw, src);
            }
        }
        lmt++;
    }

    gobj_log_error(gobj, 0,
        "function",     "%s", __FUNCTION__,
        "msgset",       "%s", MSGSET_INTERNAL_ERROR,
        "msg",          "%s", "internal method NOT EXIST",
        "full-name",    "%s", gobj_full_name(gobj),
        "lmethod",      "%s", lmethod,
        NULL
    );
    return 0;
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
 *  Start all children of the gobj.
 ***************************************************************************/
PRIVATE int cb_start_child(hgobj child_, void *user_data, void *user_data2)
{
    gobj_t *child = child_;
    if(child->gclass->gclass_flag & gcflag_manual_start) {
        return 0;
    }
    if(!gobj_is_destroying(child) && !gobj_is_running(child) && !gobj_is_disabled(child)) {
        gobj_start(child);
    }
    return 0;
}
PUBLIC int gobj_start_children(hgobj gobj)
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

    return gobj_walk_gobj_children(gobj, WALK_FIRST2LAST, cb_start_child, 0, 0);
}

/***************************************************************************
 *  Start this gobj and all children tree of the gobj.
 ***************************************************************************/
PRIVATE int cb_start_child_tree(hgobj child_, void *user_data, void *user_data2)
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
    return gobj_walk_gobj_children_tree(gobj, WALK_TOP2BOTTOM, cb_start_child_tree, 0, 0);
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
        gobj_log_warning(gobj, LOG_OPT_TRACE_STACK,
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
 *  Stop all children of the gobj.
 ***************************************************************************/
PRIVATE int cb_stop_child(hgobj child, void *user_data, void *user_data2)
{
    if(gobj_is_running(child)) {
        gobj_stop(child);
    }
    return 0;
}
PUBLIC int gobj_stop_children(hgobj gobj_)
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
    return gobj_walk_gobj_children(gobj, WALK_FIRST2LAST, cb_stop_child, 0, 0);
}

/***************************************************************************
 *  Stop this gobj and all children tree of the gobj.
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
    return gobj_walk_gobj_children_tree(gobj, WALK_TOP2BOTTOM, cb_stop_child, 0, 0);
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
        if(!is_machine_not_tracing(gobj, 0)) {
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
        if(!is_machine_not_tracing(gobj, 0)) {
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
 *  If service has mt_play then start only the service gobj.
 *      (Let mt_play be responsible to start their tree)
 *  If service has not mt_play then start the tree with gobj_start_tree().
 ***************************************************************************/
PUBLIC int gobj_autostart_services(void)
{
    const char *key; json_t *jn_service;
    json_object_foreach(__jn_services__, key, jn_service) {
        gobj_t *gobj = (gobj_t *)(size_t)json_integer_value(jn_service);
        if(gobj->gobj_flag & gobj_flag_yuno) {
            continue;
        }
        if(gobj->gobj_flag & gobj_flag_autostart) {
            if(gobj->gclass->gmt->mt_play) { // HACK checking mt_play because if exists he has the power on!
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
    json_object_foreach(__jn_services__, key, jn_service) {
        gobj_t *gobj = (gobj_t *)(size_t)json_integer_value(jn_service);
        if(gobj->gobj_flag & gobj_flag_yuno) {
            continue;
        }
        if(gobj->gobj_flag & gobj_flag_autoplay) {
            gobj_play(gobj);
        }
    }
    return 0;
}

/***************************************************************************
 *
 ***************************************************************************/
PUBLIC int gobj_pause_autoplay_services(void)
{
    const char *key; json_t *jn_service;
    json_object_foreach(__jn_services__, key, jn_service) {
        gobj_t *gobj = (gobj_t *)(size_t)json_integer_value(jn_service);
        if(gobj->gobj_flag & gobj_flag_yuno) {
            continue;
        }

        if(gobj_is_playing(gobj)) {
            if(is_level_tracing(0, TRACE_START_STOP)) {
                gobj_log_debug(0,0,
                    "function",     "%s", __FUNCTION__,
                    "msgset",       "%s", MSGSET_STARTUP,
                    "msg",          "%s", "PAUSE service",
                    "service",      "%s", gobj_short_name(gobj),
                    NULL
                );
            }
            gobj_pause(gobj);
        }
    }

    return 0;
}

/***************************************************************************
 *
 ***************************************************************************/
PUBLIC int gobj_stop_autostart_services(void)
{
    const char *key; json_t *jn_service;
    json_object_foreach(__jn_services__, key, jn_service) {
        gobj_t *gobj = (gobj_t *)(size_t)json_integer_value(jn_service);
        if(gobj->gobj_flag & gobj_flag_yuno) {
            continue;
        }

        if(gobj_is_running(gobj)) {
            if(is_level_tracing(0, TRACE_START_STOP)) {
                gobj_log_debug(0,0,
                    "function",     "%s", __FUNCTION__,
                    "msgset",       "%s", MSGSET_STARTUP,
                    "msg",          "%s", "STOP service",
                    "service",      "%s", gobj_short_name(gobj),
                    NULL
                );
            }
            gobj_stop_tree(gobj);
        }
    }

    return 0;
}

/***************************************************************************
 *  Execute a command of gclass command table
 ***************************************************************************/
PUBLIC json_t *gobj_command( // With AUTHZ
    hgobj gobj_,
    const char *command,
    json_t *kw,
    hgobj src
)
{
    gobj_t *gobj = gobj_;
    if(!gobj || gobj->obflag & (obflag_destroyed|obflag_destroying)) {
        gobj_log_error(NULL, LOG_OPT_TRACE_STACK,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_PARAMETER_ERROR,
            "msg",          "%s", "hgobj NULL or DESTROYED",
            NULL
        );
        KW_DECREF(kw)
        return 0;
    }

    /*-----------------------------------------------*
     *  Trace
     *-----------------------------------------------*/
    BOOL tracea = is_machine_tracing(gobj, 0) && !is_machine_not_tracing(src, 0);
    if(tracea) {
        trace_machine("🌀🌀 mach(%s%s), cmd: %s, src: %s",
            (!gobj->running)?"!!":"",
            gobj_short_name(gobj),
            command,
            gobj_short_name(src)
        );
        if(gobj_trace_level(gobj) & (TRACE_EV_KW)) {
            gobj_trace_json(gobj, kw, "command kw");
        }
    }

    /*-----------------------------------------------*
     *  The local mt_command_parser has preference
     *-----------------------------------------------*/
    if(gobj->gclass->gmt->mt_command_parser) {
        return gobj->gclass->gmt->mt_command_parser(gobj, command, kw, src);
    }

    /*-----------------------------------------------*
     *  If it has command_table
     *  then use the global command parser
     *-----------------------------------------------*/
    if(gobj->gclass->command_table) {
        if(__global_command_parser_fn__) {
            return __global_command_parser_fn__(gobj, command, kw, src);
        } else {
            json_t *kw_response = build_command_response(
                gobj,
                -1,     // result
                json_sprintf("%s: global command parser function not available",
                    gobj_short_name(gobj)
                ),   // jn_comment
                0,      // jn_schema
                0       // jn_data
            );
            KW_DECREF(kw)
            return kw_response;
        }
    } else {
        json_t *kw_response = build_command_response(
            gobj,
            -1,     // result
            json_sprintf("%s: command table not available",
                gobj_short_name(gobj)
            ),   // jn_comment
            0,      // jn_schema
            0       // jn_data
        );
        KW_DECREF(kw)
        return kw_response;
    }
}

/***************************************************************************
 *
 ***************************************************************************/
PUBLIC json_t *gobj_stats(hgobj gobj_, const char *stats, json_t *kw, hgobj src)
{
     gobj_t * gobj = gobj_;

    if(!gobj || gobj->obflag & (obflag_destroyed|obflag_destroying)) {
        gobj_log_error(NULL, LOG_OPT_TRACE_STACK,
            "gobj",         "%s", __FILE__,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_PARAMETER_ERROR,
            "msg",          "%s", "hgobj NULL or DESTROYED",
            NULL
        );
        KW_DECREF(kw)
        return 0;
    }

    /*--------------------------------------*
     *  The local mt_stats has preference
     *--------------------------------------*/
    if(gobj->gclass->gmt->mt_stats) {
        return gobj->gclass->gmt->mt_stats(gobj, stats, kw, src);
    }

    /*-----------------------------------------------*
     *  Then use the global stats parser
     *-----------------------------------------------*/
    if(__global_stats_parser_fn__) {
        return __global_stats_parser_fn__(gobj, stats, kw, src);
    } else {
        json_t *kw_response = build_stats_response(
            gobj,
            -1,     // result
            json_sprintf("%s, stats parser not available",
                gobj_short_name(gobj)
            ),   // jn_comment
            0,      // jn_schema
            0       // jn_data
        );
        KW_DECREF(kw)
        return kw_response;
    }
}

/***************************************************************************
 *  Set manually the bottom gobj
 *  return previous bottom_gobj MT (mt_set_bottom_gobj)
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

    if(__deep_trace__) { // too much traces, limit to deep trace
        trace_machine("🔽 set_bottom_gobj('%s') = '%s', prev '%s'",
            gobj_short_name(gobj),
            bottom_gobj?gobj_short_name(bottom_gobj):"",
            gobj->bottom_gobj?gobj_short_name(gobj->bottom_gobj):""
        );
    }

    hgobj previous_bottom_gobj = 0;

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
        previous_bottom_gobj = gobj->bottom_gobj;
    }

    // anyway set -> YES! -> the new set has preference. New 27-Jan-2017
    gobj->bottom_gobj = bottom_gobj;

    if(gobj->gclass->gmt->mt_set_bottom_gobj) {
        gobj->gclass->gmt->mt_set_bottom_gobj(gobj, bottom_gobj, previous_bottom_gobj);
    }

    return previous_bottom_gobj;
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
PUBLIC json_t *gobj_services(void)
{
    json_t *jn_register = json_array();

    const char *key; json_t *jn_service;
    json_object_foreach(__jn_services__, key, jn_service) {
        json_array_append_new(
            jn_register,
            json_string(key)
        );
    }

    return jn_register;
}

/***************************************************************************
 *
 ***************************************************************************/
PUBLIC hgobj gobj_default_service(void)
{
    return __default_service__;
}

/***************************************************************************
 *  service is case-insensitive, convert to lower to compare
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
    char service_name[GOBJ_NAME_MAX+1];
    snprintf(service_name, sizeof(service_name), "%s", service);
    strntolower(service_name, sizeof(service_name));

    if(strcasecmp(service_name, "__default_service__")==0) {
        return __default_service__;
    }
    if(strcasecmp(service_name, "__yuno__")==0 || strcasecmp(service_name, "__root__")==0) {
        return gobj_yuno();
    }

    json_t *o = json_object_get(__jn_services__, service);
    if(!o) {
        if(verbose) {
            gobj_log_error(0, LOG_OPT_TRACE_STACK,
                "function",     "%s", __FUNCTION__,
                "msgset",       "%s", MSGSET_PARAMETER_ERROR,
                "msg",          "%s", "service NOT FOUND",
                "service",      "%s", service,
                NULL
            );
            json_t *jn_services = gobj_service_register();
            gobj_trace_json(0, jn_services, "service NOT FOUND");
            json_decref(jn_services);
        }
        return NULL;
    }

    return (hgobj)(size_t)json_integer_value(o);
}

/***************************************************************************
 *
 ***************************************************************************/
PUBLIC hgobj gobj_find_service_by_gclass(const char *gclass_name, BOOL verbose) // OLD gobj_find_gclass_service
{
    const char *key; json_t *jn_service;
    json_object_foreach(__jn_services__, key, jn_service) {
        gobj_t *gobj = (gobj_t *)(size_t)json_integer_value(jn_service);
        const char *gclass_name_ = gobj_gclass_name(gobj);
        if(strcasecmp(gclass_name, gclass_name_)==0) {
            return gobj;
        }
    }

    if(verbose) {
        gobj_log_error(0, LOG_OPT_TRACE_STACK,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_PARAMETER_ERROR,
            "msg",          "%s", "service NOT FOUND",
            "gclass",       "%s", gclass_name,
            NULL
        );
        json_t *jn_services = gobj_service_register();
        gobj_trace_json(0, jn_services, "service NOT FOUND");
        json_decref(jn_services);
    }
    return NULL;
}

/***************************************************************************
 *  Find a gobj by path
 ***************************************************************************/
PUBLIC hgobj gobj_find_gobj(const char *path)
{
    if(empty_string(path)) {
        return 0;
    }
    /*
     *  WARNING code repeated
     */
    if(strcasecmp(path, "__default_service__")==0) {
        return __default_service__;
    }
    if(strcasecmp(path, "__yuno__")==0 || strcasecmp(path, "__root__")==0) {
        return __yuno__;
    }

    return gobj_search_path(__yuno__, path);
}

/***************************************************************************
 *
 ***************************************************************************/
PUBLIC hgobj gobj_first_child(hgobj gobj_)
{
    gobj_t * gobj = gobj_;
    if(!gobj) {
        gobj_log_error(gobj, LOG_OPT_TRACE_STACK,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_PARAMETER_ERROR,
            "msg",          "%s", "gobj NULL",
            NULL
        );
        return 0;
    }

    return dl_first(&gobj->dl_children);
}

/***************************************************************************
 *  Return last child, last to `child`
 ***************************************************************************/
PUBLIC hgobj gobj_last_child(hgobj gobj_)
{
    gobj_t * gobj = gobj_;
    if(!gobj) {
        gobj_log_error(gobj, LOG_OPT_TRACE_STACK,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_PARAMETER_ERROR,
            "msg",          "%s", "gobj NULL",
            NULL
        );
        return 0;
    }
    return dl_last(&gobj->dl_children);
}


/***************************************************************************
 *  Return next child, next to `child`
 ***************************************************************************/
PUBLIC hgobj gobj_next_child(hgobj child)
{
    gobj_t * gobj = child;
    if(!gobj) {
        gobj_log_error(gobj, LOG_OPT_TRACE_STACK,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_PARAMETER_ERROR,
            "msg",          "%s", "gobj NULL",
            NULL
        );
        return 0;
    }
    return dl_next(gobj);
}

/***************************************************************************
 *  Return prev child, prev to `child`
 ***************************************************************************/
PUBLIC hgobj gobj_prev_child(hgobj child)
{
    gobj_t * gobj = child;
    if(!gobj) {
        gobj_log_error(gobj, LOG_OPT_TRACE_STACK,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_PARAMETER_ERROR,
            "msg",          "%s", "gobj NULL",
            NULL
        );
        return 0;
    }
    return dl_prev(gobj);
}

/***************************************************************************
 *  Return the child of gobj by name.
 *  The first found is returned.
 ***************************************************************************/
PUBLIC hgobj gobj_child_by_name(hgobj gobj_, const char *name)
{
    gobj_t * gobj = gobj_;
    if(!gobj) {
        gobj_log_error(gobj, LOG_OPT_TRACE_STACK,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_PARAMETER_ERROR,
            "msg",          "%s", "gobj NULL",
            NULL
        );
        return 0;
    }
    if(empty_string(name)) {
        gobj_log_error(gobj, LOG_OPT_TRACE_STACK,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_PARAMETER_ERROR,
            "msg",          "%s", "name NULL",
            NULL
        );
        return 0;
    }

    gobj_t *child = dl_first(&gobj->dl_children);
    while(child) {
        if(!(child->obflag & (obflag_destroyed|obflag_destroying))) {
            const char *name_ = gobj_name(child);
            if(name_ && strcmp(name_, name)==0) {
                return child;
            }
        }
        /*
         *  Next
         */
        child = dl_next(child);
    }
    return 0;
}

/***************************************************************************
 *  Return the child of gobj in the `index` position. (relative to 1)
 ***************************************************************************/
PUBLIC hgobj gobj_child_by_index(hgobj gobj_, size_t idx) // relative to 1
{
    gobj_t * gobj = gobj_;
    if(!gobj) {
        gobj_log_error(gobj, LOG_OPT_TRACE_STACK,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_PARAMETER_ERROR,
            "msg",          "%s", "gobj NULL",
            NULL
        );
        return 0;
    }

    size_t idx_ = 0;
    gobj_t *child = dl_first(&gobj->dl_children);
    while(child) {
        idx_++;    // relative to 1
        if(idx_ == idx) {
            return child;
        }

        /*
         *  Next
         */
        child = dl_next(child);
    }

    return 0; /* not found */
}

/***************************************************************************
 *  Return the size of children of gobj
 ***************************************************************************/
PUBLIC size_t gobj_child_size(hgobj gobj_)
{
    gobj_t * gobj = gobj_;

    return dl_size(&gobj->dl_children);
}

/***************************************************************************
 *  Return the size of matched children of gobj
 ***************************************************************************/
PUBLIC size_t gobj_child_size2(
    hgobj gobj_,
    json_t *jn_filter // owned
)
{
    gobj_t * gobj = gobj_;
    size_t count = 0;
    gobj_t *child = gobj_first_child(gobj);

    while(child) {
        if(gobj_match_gobj(child, json_incref(jn_filter))) {
            count++;
        }
        child = gobj_next_child(child);
    }
    JSON_DECREF(jn_filter)
    return count;
}

/***************************************************************************
 *  Match child.
 ***************************************************************************/
PUBLIC BOOL gobj_match_gobj(
    hgobj gobj_,
    json_t *jn_filter_ // owned
)
{
    gobj_t *gobj = gobj_;
    json_t *jn_filter = json_deep_copy(jn_filter_); // deep copy, jn_filter will be modified
    JSON_DECREF(jn_filter_)

    const char *__inherited_gclass_name__ =  kw_get_str(
        gobj, jn_filter, "__inherited_gclass_name__", 0, 0
    );
    const char * __gclass_name__ = kw_get_str(gobj, jn_filter, "__gclass_name__", 0, 0);
    const char * __state__ = kw_get_str(gobj, jn_filter, "__state__", 0, 0);
    const char *__gobj_name__ = kw_get_str(gobj, jn_filter, "__gobj_name__", 0, 0);
    const char *__prefix_gobj_name__ = kw_get_str(gobj, jn_filter, "__prefix_gobj_name__", 0, 0);

    if(__inherited_gclass_name__) {
        if(!gobj_typeof_inherited_gclass(gobj, __inherited_gclass_name__)) {
            JSON_DECREF(jn_filter) // clone
            return FALSE;
        }
        json_object_del(jn_filter, "__inherited_gclass_name__");
    }

    if(__gclass_name__) {
        if(!gobj_typeof_gclass(gobj, __gclass_name__)) {
            JSON_DECREF(jn_filter) // clone
            return FALSE;
        }
        json_object_del(jn_filter, "__gclass_name__");
    }
    if(__state__) {
        if(strcasecmp(__state__, gobj_current_state(gobj))!=0) {
            JSON_DECREF(jn_filter) // clone
            return FALSE;
        }
        json_object_del(jn_filter, "__state__");
    }

    if(kw_has_key(jn_filter, "__disabled__")) {
        BOOL disabled = kw_get_bool(gobj, jn_filter, "__disabled__", 0, 0);
        if(disabled && !gobj_is_disabled(gobj)) {
            JSON_DECREF(jn_filter) // clone
            return FALSE;
        }
        if(!disabled && gobj_is_disabled(gobj)) {
            JSON_DECREF(jn_filter) // clone
            return FALSE;
        }
        json_object_del(jn_filter, "__disabled__");
    }

    if(__gobj_name__) {
        const char *child_name = gobj_name(gobj);
        if(strcmp(__gobj_name__, child_name)!=0) {
            JSON_DECREF(jn_filter) // clone
            return FALSE;
        }
        json_object_del(jn_filter, "__gobj_name__");
    }

    if(__prefix_gobj_name__) {
        const char *child_name = gobj_name(gobj);
        if(strncmp(__prefix_gobj_name__, child_name, strlen(__prefix_gobj_name__))!=0) {
            JSON_DECREF(jn_filter) // clone
            return FALSE;
        }
        json_object_del(jn_filter, "__prefix_gobj_name__");
    }

    const char *key;
    json_t *jn_value;

    BOOL matched = TRUE;
    json_object_foreach(jn_filter, key, jn_value) {
        json_t *hs = gobj_hsdata2(gobj, key, FALSE);
        if(hs) {
            json_t *jn_var1 = json_object_get(hs, key);
            int cmp = cmp_two_simple_json(jn_var1, jn_value);
            if(cmp!=0) {
                matched = FALSE;
                break;
            }
        }
    }

    JSON_DECREF(jn_filter) // clone
    return matched;
}

/***************************************************************************
 *  Returns the first matched child.
 ***************************************************************************/
PUBLIC hgobj gobj_find_child(
    hgobj gobj,
    json_t *jn_filter  // owned
)
{
    if(!gobj) {
        gobj_log_error(0, LOG_OPT_TRACE_STACK,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_PARAMETER_ERROR,
            "msg",          "%s", "gobj NULL",
            NULL
        );
        JSON_DECREF(jn_filter)
        return 0;
    }

    gobj_t *child = gobj_first_child(gobj);

    while(child) {
        if(gobj_match_gobj(child, json_incref(jn_filter))) {
            JSON_DECREF(jn_filter)
            return child;
        }
        child = gobj_next_child(child);
    }

    JSON_DECREF(jn_filter)
    return 0;
}

/***************************************************************************
 *  Callback building an iter
 ***************************************************************************/
PRIVATE int cb_match_children(
    hgobj child_,
    void *user_data,
    void *user_data2
)
{
    gobj_t *child = child_;
    json_t *dl_list = user_data;
    json_t *jn_filter = user_data2;

    if(gobj_match_gobj(child, json_incref(jn_filter))) {
        child->__refs__++;
        json_array_append_new(dl_list, json_integer((json_int_t)(size_t)child));
    }
    return 0;
}

/***************************************************************************
 *  Returns an iter (json list of hgobj) with all matched children.
 *  Check ONLY first level of children.
 *
 *  WARNING the returned list must be free with gobj_free_iter(json_t *iter)
 ***************************************************************************/
PUBLIC json_t *gobj_match_children(
    hgobj gobj,
    json_t *jn_filter   // owned
)
{
    if(!gobj) {
        gobj_log_error(gobj, LOG_OPT_TRACE_STACK,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_PARAMETER_ERROR,
            "msg",          "%s", "gobj NULL",
            NULL
        );
        JSON_DECREF(jn_filter);
        return 0;
    }

    json_t *iter = json_array();

    gobj_walk_gobj_children(
        gobj,
        WALK_FIRST2LAST,
        cb_match_children,
        iter,
        jn_filter
    );
    JSON_DECREF(jn_filter)
    return iter;
}

/***************************************************************************
 *  Returns an iter (json list of hgobj) with all matched children.
 *  Check deep levels of children
 *  Check in the full tree of children.
 *
 *  WARNING the returned list must be free with gobj_free_iter(json_t *iter)
 ***************************************************************************/
PUBLIC json_t *gobj_match_children_tree(
    hgobj gobj,
    json_t *jn_filter   // owned
)
{
    if(!gobj) {
        gobj_log_error(gobj, LOG_OPT_TRACE_STACK,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_PARAMETER_ERROR,
            "msg",          "%s", "gobj NULL",
            NULL
        );
        JSON_DECREF(jn_filter);
        return 0;
    }
    json_t *iter = json_array();

    gobj_walk_gobj_children_tree(
        gobj,
        WALK_TOP2BOTTOM,
        cb_match_children,
        iter,
        jn_filter
    );
    JSON_DECREF(jn_filter)
    return iter;
}

/***************************************************************************
 *  Free an iter (list of hgobj)
 ***************************************************************************/
PUBLIC int gobj_free_iter(json_t *iter)
{
    int idx; json_t *jn_gobj;
    json_array_foreach(iter, idx, jn_gobj) {
        gobj_t *gobj = (gobj_t *)(size_t)json_integer_value(jn_gobj);
        if(!gobj) {
            gobj_log_error(gobj, LOG_OPT_TRACE_STACK,
                "function",     "%s", __FUNCTION__,
                "msgset",       "%s", MSGSET_INTERNAL_ERROR,
                "msg",          "%s", "gobj NULL",
                NULL
            );
            continue;
        }
        if(gobj->__refs__ <= 0) {
            gobj_log_error(gobj, LOG_OPT_TRACE_STACK,
                "function",     "%s", __FUNCTION__,
                "msgset",       "%s", MSGSET_INTERNAL_ERROR,
                "msg",          "%s", "iter gobj with refs <= 0",
                "refs",         "%d", gobj->__refs__,
                NULL
            );
            continue;
        }
        gobj->__refs__ --;
    }

    JSON_DECREF(iter)

    return 0;
}

/***************************************************************************
 *  Return the object searched by path.
 *  The separator of tree's gobj must be '`'
 ***************************************************************************/
PUBLIC hgobj gobj_search_path(hgobj gobj_, const char *path_)
{
    gobj_t *gobj = gobj_;
    // TODO check, new function implementation
    char delimiter[2] = {'`',0};
    if(empty_string(path_) || !gobj) {
        return 0;
    }
    char *path = GBMEM_STRDUP(path_);

    int list_size = 0;
    const char **ss = split2(path, delimiter, &list_size);

    hgobj child = 0;
    const char *key = 0;
    for(int i=0; i<list_size && gobj; i++) {
        key = *(ss + i);

        const char *gclass_name_ = 0;
        char *gobj_name_ = strchr(key, '^');
        if(gobj_name_) {
            gclass_name_ = key;
            *gobj_name_ = 0;
            gobj_name_++;
        } else {
            gobj_name_ = (char *)key;
        }

        if(gobj_name_) {
            if(strcmp(gobj_name_, gobj_name(gobj))==0) {
                if(!gclass_name_) {
                    continue;
                }
                if(strcasecmp(gclass_name_, gobj_gclass_name(gobj))==0) {
                    continue;
                }
            }
        }

        json_t *jn_filter = json_pack("{s:s}",
            "__gobj_name__", gobj_name_
        );
        if(gclass_name_) {
            json_object_set_new(jn_filter, "__gclass_name__", json_string(gclass_name_));
        }

        child = gobj_find_child(
            gobj,
            jn_filter
        );
        if(!child) {
            break;
        }
        gobj = child;
    }

    split_free2(ss);
    GBMEM_FREE(path)
    return child;
}

/***************************************************************************
 *
 ***************************************************************************/
PUBLIC int gobj_walk_gobj_children(
    hgobj gobj_,
    walk_type_t walk_type,
    cb_walking_t cb_walking,
    void *user_data,
    void *user_data2
) {
    gobj_t *gobj = gobj_;
    if(gobj_is_destroying(gobj)) {
        gobj_log_error(0, LOG_OPT_TRACE_STACK,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_PARAMETER_ERROR,
            "msg",          "%s", "gobj NULL or destroying",
            NULL
        );
        return 0;
    }

    return rc_walk_by_list(&gobj->dl_children, walk_type, cb_walking, user_data, user_data2);
}

/***************************************************************************
 *
 ***************************************************************************/
PUBLIC int gobj_walk_gobj_children_tree(
    hgobj gobj_,
    walk_type_t walk_type,
    cb_walking_t cb_walking,
    void *user_data,
    void *user_data2
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

    return rc_walk_by_tree(&gobj->dl_children, walk_type, cb_walking, user_data, user_data2);
}

/***************************************************************
 *  Walking
 ***************************************************************/
PUBLIC int rc_walk_by_list(
    dl_list_t *iter,
    walk_type_t walk_type,
    cb_walking_t cb_walking,
    void *user_data,
    void *user_data2
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

        int ret = (cb_walking)(child, user_data, user_data2);
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
    void *user_data,
    void *user_data2
) {
    /*
     *  First my children
     */
    int ret = rc_walk_by_list(iter, walk_type, cb_walking, user_data, user_data2);
    if(ret < 0) {
        return ret;
    }

    /*
     *  Now child's children
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

        dl_list_t *dl_child_list = &child->dl_children;
        ret = rc_walk_by_level(dl_child_list, walk_type, cb_walking, user_data, user_data2);
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
    void *user_data,
    void *user_data2
) {
    if(walk_type & WALK_BYLEVEL) {
        return rc_walk_by_level(iter, walk_type, cb_walking, user_data, user_data2);
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
            dl_list_t *dl_child_list = &child->dl_children;
            int ret = rc_walk_by_tree(dl_child_list, walk_type, cb_walking, user_data, user_data2);
            if(ret < 0) {
                return ret;
            }

            ret = (cb_walking)(child, user_data, user_data2);
            if(ret < 0) {
                return ret;
            }
        } else {
            int ret = (cb_walking)(child, user_data, user_data2);
            if(ret < 0) {
                return ret;
            } else if(ret == 0) {
                dl_list_t *dl_child_list = &child->dl_children;
                ret = rc_walk_by_tree(dl_child_list, walk_type, cb_walking, user_data, user_data2);
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
 *  Variables set by agent
 ***************************************************************************/
PUBLIC const char *gobj_yuno_name(void) {
    hgobj yuno = gobj_yuno();
    if(!yuno) {
        return "";
    }
    return gobj_name(yuno);
}
PUBLIC const char *gobj_yuno_role(void) {
    hgobj yuno = gobj_yuno();
    if(!yuno) {
        return "";
    }
    return gobj_read_str_attr(yuno, "yuno_role");
}
PUBLIC const char *gobj_yuno_id(void) {
    hgobj yuno = gobj_yuno();
    if(!yuno) {
        return "";
    }
    return gobj_read_str_attr(yuno, "yuno_id");
}
PUBLIC const char *gobj_yuno_tag(void) {
    hgobj yuno = gobj_yuno();
    if(!yuno) {
        return "";
    }
    return gobj_read_str_attr(yuno, "yuno_tag");
}
PUBLIC const char *gobj_yuno_role_plus_name(void) {
    hgobj yuno = gobj_yuno();
    if(!yuno) {
        return "";
    }
    return gobj_read_str_attr(yuno, "yuno_role_plus_name");
}

PUBLIC const char *gobj_yuno_realm_id(void) {
    hgobj yuno = gobj_yuno();
    if(!yuno) {
        return "";
    }
    return gobj_read_str_attr(yuno, "realm_id");
}
PUBLIC const char *gobj_yuno_realm_owner(void) {
    hgobj yuno = gobj_yuno();
    if(!yuno) {
        return "";
    }
    return gobj_read_str_attr(yuno, "realm_owner");
}
PUBLIC const char *gobj_yuno_realm_role(void) {
    hgobj yuno = gobj_yuno();
    if(!yuno) {
        return "";
    }
    return gobj_read_str_attr(yuno, "realm_role");
}
PUBLIC const char *gobj_yuno_realm_name(void) {
    hgobj yuno = gobj_yuno();
    if(!yuno) {
        return "";
    }
    return gobj_read_str_attr(yuno, "realm_name");
}
PUBLIC const char *gobj_yuno_realm_env(void) {
    hgobj yuno = gobj_yuno();
    if(!yuno) {
        return "";
    }
    return gobj_read_str_attr(yuno, "realm_env");
}
PUBLIC const char *gobj_yuno_node_owner(void) {
    hgobj yuno = gobj_yuno();
    if(!yuno) {
        return "";
    }
    return gobj_read_str_attr(yuno, "node_owner");
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
 *
 ***************************************************************************/
PUBLIC hgclass gobj_gclass(hgobj hgobj)
{
    gobj_t *gobj = (gobj_t *)hgobj;
    return (gobj)? gobj->gclass: NULL;
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
        char temp[2*256];
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
PUBLIC json_t *gobj_global_variables(void)
{
    json_t *jn_global_variables = json_pack("{s:s, s:s, s:s, s:s, s:s, s:s, s:s, s:s, s:s, s:s, s:s, s:s, s:s, s:s, s:s, s:s, s:s}",
        "__node_owner__", gobj_yuno_node_owner(),
        "__realm_id__", gobj_yuno_realm_id(),
        "__realm_owner__", gobj_yuno_realm_owner(),
        "__realm_role__", gobj_yuno_realm_role(),
        "__realm_name__", gobj_yuno_realm_name(),
        "__realm_env__", gobj_yuno_realm_env(),
        "__yuno_id__", gobj_yuno_id(),
        "__yuno_role__", gobj_yuno_role(),
        "__yuno_name__", gobj_yuno_name(),
        "__yuno_tag__", gobj_yuno_tag(),
        "__yuno_role_plus_name__", gobj_yuno_role_plus_name(),
        "__hostname__", get_hostname(),
#ifdef __linux__
        "__sys_system_name__", sys.sysname,
        "__sys_node_name__", sys.nodename,
        "__sys_version__", sys.version,
        "__sys_release__", sys.release,
        "__sys_machine__", sys.machine
#else
        "__sys_system_name__", "",
        "__sys_node_name__", "",
        "__sys_version__", "",
        "__sys_release__", "",
        "__sys_machine__", ""
#endif
    );

    if(gobj_yuno()) {
        json_object_update_new(
            jn_global_variables,
            json_pack("{s:s, s:b}",
                "__bind_ip__", gobj_read_str_attr(gobj_yuno(), "bind_ip"),
                "__multiple__", gobj_read_bool_attr(gobj_yuno(), "yuno_multiple")
            )
        );
    }
    return jn_global_variables;
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
PUBLIC hgobj gobj_parent(hgobj gobj)
{
    if(!gobj) {
        return NULL;
    }
    return ((gobj_t *)gobj)->parent;
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
    if(gobj_is_destroying(gobj)) {
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

    if(gobj_is_destroying(gobj)) {
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
    if(gobj_is_destroying(gobj)) {
        return TRUE;
    }
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
 *  Set as volatil
 ***************************************************************************/
PUBLIC int gobj_set_volatil(hgobj gobj_, BOOL set)
{
    gobj_t *gobj = gobj_;
    if(set) {
        gobj->gobj_flag |= gobj_flag_volatil;
    } else {
        gobj->gobj_flag &= ~gobj_flag_volatil;
    }

    return 0;
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

/***************************************************************************
 *
 ***************************************************************************/
PUBLIC BOOL gobj_is_bottom_gobj(hgobj gobj_)
{
    gobj_t *gobj = gobj_;
    if(gobj && gobj->parent && gobj->parent->bottom_gobj == gobj) {
        return TRUE;
    }
    return FALSE;
}

/***************************************************************************
 *  True if gobj is of gclass gclass_name
 ***************************************************************************/
PUBLIC BOOL gobj_typeof_gclass(hgobj gobj, const char *gclass_name)
{
    if(strcasecmp(((gobj_t *)gobj)->gclass->gclass_name, gclass_name)==0) {
        return TRUE;
    } else {
        return FALSE;
    }
}

/***************************************************************************
 *  Is a inherited (bottom) of this gclass?
 ***************************************************************************/
PUBLIC BOOL gobj_typeof_inherited_gclass(hgobj gobj_, const char *gclass_name)
{
    gobj_t * gobj = gobj_;

    while(gobj) {
        if(strcasecmp(gclass_name, gobj->gclass->gclass_name)==0) {
            return TRUE;
        }
        gobj = gobj->bottom_gobj;
    }

    return FALSE;
}

/***************************************************************************
 *  Return the data description of the command `command`
 *  If `command` is null returns full command's table
 ***************************************************************************/
PUBLIC const sdata_desc_t *gclass_command_desc(hgclass gclass_, const char *name, BOOL verbose)
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
        return gclass->command_table;
    }
    const sdata_desc_t *it = gclass->command_table;
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
            "msg",          "%s", "GClass command NOT FOUND",
            "gclass",       "%s", gclass->gclass_name,
            "command",         "%s", name,
            NULL
        );
    }
    return NULL;
}

/***************************************************************************
 *  Return the data description of the command `command`
 *  If `command` is null returns full command's table
 ***************************************************************************/
PUBLIC const sdata_desc_t *gobj_command_desc(hgobj gobj_, const char *name, BOOL verbose)
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
        return gobj->gclass->command_table;
    }

    return gclass_command_desc(gobj->gclass, name, verbose);
}

/***************************************************************************
 *  Get names table of sdata flag
 ***************************************************************************/
PUBLIC const char **get_sdata_flag_table(void)
{
    return sdata_flag_names;
}

/***************************************************************************
 *
 *  Return list of objects with gobj's attribute description,
 *  restricted to attributes having SDF_RD|SDF_WR|SDF_STATS|SDF_PERSIST|SDF_VOLATIL|SDF_RSTATS|SDF_PSTATS flag.
 *
 *  Esquema tabla webix:

    data:[
        {
            id: integer (index)
            name: string,
            type: string ("real" | "boolean" | "integer" | "string" | "json"),
            flag: string ("SDF_RD",...),
            description: string,
            stats: boolean
        }
    ]
 ***************************************************************************/
PUBLIC json_t *get_attrs_schema(hgobj gobj_)
{
    gobj_t *gobj = gobj_;
    json_t *jn_data = json_array();

    int id = 1;
    const sdata_desc_t *it = gobj->gclass->attrs_table;
    while(it && it->name) {
        if(it->flag & (SDF_PUBLIC_ATTR)) {
            char *type;
            if(it->type == DTP_REAL) {
                type = "real";
            } else if(it->type == DTP_BOOLEAN) {
                type = "boolean";
            } else if(it->type == DTP_INTEGER) {
                type = "integer";
            } else if(it->type == DTP_STRING) {
                type = "string";
            } else if(it->type == DTP_JSON) {
                type = "json";
            } else if(it->type == DTP_DICT) {
                type = "dict";
            } else if(it->type == DTP_LIST) {
                type = "list";
            } else {
                it++;
                continue;
            }

            gbuffer_t *gbuf = bits2gbuffer(get_sdata_flag_table(), it->flag);
            char *flag = gbuf?gbuffer_cur_rd_pointer(gbuf):"";
            json_t *attr_dict = json_pack("{s:I, s:s, s:s, s:s, s:s, s:b}",
                "id", (json_int_t)id++,
                "name", it->name,
                "type", type,
                "flag", flag,
                "description", it->description?it->description:"",
                "stats", (it->flag & (SDF_STATS|SDF_RSTATS|SDF_PSTATS))?1:0
            );
            GBUFFER_DECREF(gbuf);

           json_array_append_new(jn_data, attr_dict);
        }
        it++;
    }

    return jn_data;
}

/***************************************************************************
 *  Return a json object describing the parameter
 ***************************************************************************/
PRIVATE json_t *itdesc2json(const sdata_desc_t *it)
{
    json_t *jn_it = json_object();

    int type = it->type;

    if(DTP_IS_STRING(type)) {
        json_object_set_new(jn_it, "type", json_string("string"));
    } else if(DTP_IS_BOOLEAN(type)) {
        json_object_set_new(jn_it, "type", json_string("boolean"));
    } else if(DTP_IS_INTEGER(type)) {
        json_object_set_new(jn_it, "type", json_string("integer"));
    } else if(DTP_IS_REAL(type)) {
        json_object_set_new(jn_it, "type", json_string("real"));
    } else if(DTP_IS_LIST(type)) {
        json_object_set_new(jn_it, "type", json_string("list"));
    } else if(DTP_IS_DICT(type)) {
        json_object_set_new(jn_it, "type", json_string("dict"));
    } else if(DTP_IS_JSON(type)) {
        json_object_set_new(jn_it, "type", json_string("json"));
    } else if(DTP_IS_POINTER(type)) {
        json_object_set_new(jn_it, "type", json_string("pointer"));
    } else if(DTP_IS_SCHEMA(type)) {
        json_object_set_new(jn_it, "type", json_string("schema"));
    } else {
        json_object_set_new(jn_it, "type", json_string("???"));
    }
    json_object_set_new(
        jn_it,
        "default_value",
        json_string(it->default_value?it->default_value:"")
    );

    json_object_set_new(jn_it, "description", json_string(it->description));
    gbuffer_t *gbuf = bits2gbuffer(sdata_flag_names, it->flag);
    if(gbuf) {
        size_t l = gbuffer_leftbytes(gbuf);
        if(l) {
            char *pflag = gbuffer_get(gbuf, l);
            json_object_set_new(jn_it, "flag", json_string(pflag));
        } else {
            json_object_set_new(jn_it, "flag", json_string(""));
        }
        gbuffer_decref(gbuf);
    }
    return jn_it;
}

/***************************************************************************
 *  Return a json object describing the hsdata for commands
 ***************************************************************************/
PRIVATE json_t *cmddesc2json(const sdata_desc_t *it)
{
    int type = it->type;

    if(!DTP_IS_SCHEMA(type)) {
        return 0; // Only schemas please
    }

    json_t *jn_it = json_object();

    json_object_set_new(jn_it, "command", json_string(it->name));

    if(it->alias) {
        json_t *jn_alias = json_array();
        json_object_set_new(jn_it, "alias", jn_alias);
        const char **alias = it->alias;
        while(*alias) {
            json_array_append_new(jn_alias, json_string(*alias));
            alias++;
        }
    }

    json_object_set_new(jn_it, "description", json_string(it->description));
    gbuffer_t *gbuf = bits2gbuffer(sdata_flag_names, it->flag);
    if(gbuf) {
        size_t l = gbuffer_leftbytes(gbuf);
        if(l) {
            char *pflag = gbuffer_get(gbuf, l);
            json_object_set_new(jn_it, "flag", json_string(pflag));
        } else {
            json_object_set_new(jn_it, "flag", json_string(""));
        }
        gbuffer_decref(gbuf);
    }

    gbuf = gbuffer_create(256, 16*1024);
    gbuffer_printf(gbuf, "%s ", it->name);
    const sdata_desc_t *pparam = it->schema;
    while(pparam && pparam->name) {
        if((pparam->flag & SDF_REQUIRED)) {
            gbuffer_printf(gbuf, " <%s>", pparam->name);
        } else {
            gbuffer_printf(gbuf,
                " [%s='%s']",
                pparam->name, pparam->default_value?(char *)pparam->default_value:"?"
            );
        }
        pparam++;
    }
    json_t *jn_usage = json_string(gbuffer_cur_rd_pointer(gbuf));
    json_object_set_new(jn_it, "usage", jn_usage);
    GBUFFER_DECREF(gbuf);

    json_t *jn_parameters = json_array();
    json_object_set_new(jn_it, "parameters", jn_parameters);

    pparam = it->schema;
    while(pparam && pparam->name) {
        json_t *jn_param = json_object();
        json_object_set_new(jn_param, "parameter", json_string(pparam->name));
        json_object_update_missing_new(jn_param, itdesc2json(pparam));
        json_array_append_new(jn_parameters, jn_param);
        pparam++;
    }

    return jn_it;
}

/***************************************************************************
 *  Return a json object describing the hsdata for commands
 ***************************************************************************/
PRIVATE json_t *sdatacmd2json(
    const sdata_desc_t *items
)
{
    json_t *jn_items = json_array();
    const sdata_desc_t *it = items;
    if(!it) {
        return jn_items;
    }
    while(it->name) {
        json_t *jn_it = cmddesc2json(it);
        if(jn_it) {
            json_array_append_new(jn_items, jn_it);
        }
        it++;
    }
    return jn_items;
}

/***************************************************************************
 *  Return a json array describing the hsdata for attrs
 ***************************************************************************/
PUBLIC json_t *sdatadesc2json2(
    const sdata_desc_t *items,
    sdata_flag_t include_flag,
    sdata_flag_t exclude_flag
)
{
    json_t *jn_items = json_array();

    const sdata_desc_t *it = items;
    if(!it) {
        return jn_items;
    }
    while(it->name) {
        if(exclude_flag && (it->flag & exclude_flag)) {
            it++;
            continue;
        }
        if(include_flag == -1 || (it->flag & include_flag)) {
            json_t *jn_it = json_object();
            json_array_append_new(jn_items, jn_it);
            json_object_set_new(jn_it, "id", json_string(it->name));
            json_object_update_missing_new(jn_it, itdesc2json(it));
        }
        it++;
    }
    return jn_items;
}

/***************************************************************************
 *  Print yuneta gclass's methods in json
 ***************************************************************************/
PRIVATE json_t *yunetamethods2json(const GMETHODS *gmt)
{
    json_t *jn_methods = json_array();

    if(gmt->mt_create)
        json_array_append_new(jn_methods, json_string("mt_create"));
    if(gmt->mt_create2)
        json_array_append_new(jn_methods, json_string("mt_create2"));
    if(gmt->mt_destroy)
        json_array_append_new(jn_methods, json_string("mt_destroy"));
    if(gmt->mt_start)
        json_array_append_new(jn_methods, json_string("mt_start"));
    if(gmt->mt_stop)
        json_array_append_new(jn_methods, json_string("mt_stop"));
    if(gmt->mt_play)
        json_array_append_new(jn_methods, json_string("mt_play"));
    if(gmt->mt_pause)
        json_array_append_new(jn_methods, json_string("mt_pause"));
    if(gmt->mt_writing)
        json_array_append_new(jn_methods, json_string("mt_writing"));
    if(gmt->mt_reading)
        json_array_append_new(jn_methods, json_string("mt_reading"));
    if(gmt->mt_subscription_added)
        json_array_append_new(jn_methods, json_string("mt_subscription_added"));
    if(gmt->mt_subscription_deleted)
        json_array_append_new(jn_methods, json_string("mt_subscription_deleted"));
    if(gmt->mt_child_added)
        json_array_append_new(jn_methods, json_string("mt_child_added"));
    if(gmt->mt_child_removed)
        json_array_append_new(jn_methods, json_string("mt_child_removed"));
    if(gmt->mt_stats)
        json_array_append_new(jn_methods, json_string("mt_stats"));
    if(gmt->mt_command_parser)
        json_array_append_new(jn_methods, json_string("mt_command_parser"));
    if(gmt->mt_inject_event)
        json_array_append_new(jn_methods, json_string("mt_inject_event"));
    if(gmt->mt_create_resource)
        json_array_append_new(jn_methods, json_string("mt_create_resource"));
    if(gmt->mt_list_resource)
        json_array_append_new(jn_methods, json_string("mt_list_resource"));
    if(gmt->mt_save_resource)
        json_array_append_new(jn_methods, json_string("mt_save_resource"));
    if(gmt->mt_delete_resource)
        json_array_append_new(jn_methods, json_string("mt_delete_resource"));
    if(gmt->mt_future21)
        json_array_append_new(jn_methods, json_string("mt_future21"));
    if(gmt->mt_future22)
        json_array_append_new(jn_methods, json_string("mt_future22"));
    if(gmt->mt_get_resource)
        json_array_append_new(jn_methods, json_string("mt_get_resource"));
    if(gmt->mt_state_changed)
        json_array_append_new(jn_methods, json_string("mt_state_changed"));
    if(gmt->mt_authenticate)
        json_array_append_new(jn_methods, json_string("mt_authenticate"));
    if(gmt->mt_list_children)
        json_array_append_new(jn_methods, json_string("mt_list_children"));
    if(gmt->mt_stats_updated)
        json_array_append_new(jn_methods, json_string("mt_stats_updated"));
    if(gmt->mt_disable)
        json_array_append_new(jn_methods, json_string("mt_disable"));
    if(gmt->mt_enable)
        json_array_append_new(jn_methods, json_string("mt_enable"));
    if(gmt->mt_trace_on)
        json_array_append_new(jn_methods, json_string("mt_trace_on"));
    if(gmt->mt_trace_off)
        json_array_append_new(jn_methods, json_string("mt_trace_off"));
    if(gmt->mt_gobj_created)
        json_array_append_new(jn_methods, json_string("mt_gobj_created"));
    if(gmt->mt_set_bottom_gobj)
        json_array_append_new(jn_methods, json_string("mt_set_bottom_gobj"));
    if(gmt->mt_future34)
        json_array_append_new(jn_methods, json_string("mt_future34"));
    if(gmt->mt_publish_event)
        json_array_append_new(jn_methods, json_string("mt_publish_event"));
    if(gmt->mt_publication_pre_filter)
        json_array_append_new(jn_methods, json_string("mt_publication_pre_filter"));
    if(gmt->mt_publication_filter)
        json_array_append_new(jn_methods, json_string("mt_publication_filter"));
    if(gmt->mt_authz_checker)
        json_array_append_new(jn_methods, json_string("mt_authz_checker"));
    if(gmt->mt_future39)
        json_array_append_new(jn_methods, json_string("mt_future39"));
    if(gmt->mt_create_node)
        json_array_append_new(jn_methods, json_string("mt_create_node"));
    if(gmt->mt_update_node)
        json_array_append_new(jn_methods, json_string("mt_update_node"));
    if(gmt->mt_delete_node)
        json_array_append_new(jn_methods, json_string("mt_delete_node"));
    if(gmt->mt_link_nodes)
        json_array_append_new(jn_methods, json_string("mt_link_nodes"));
    if(gmt->mt_future44)
        json_array_append_new(jn_methods, json_string("mt_future44"));
    if(gmt->mt_unlink_nodes)
        json_array_append_new(jn_methods, json_string("mt_unlink_nodes"));
    if(gmt->mt_topic_jtree)
        json_array_append_new(jn_methods, json_string("mt_topic_jtree"));
    if(gmt->mt_get_node)
        json_array_append_new(jn_methods, json_string("mt_get_node"));
    if(gmt->mt_list_nodes)
        json_array_append_new(jn_methods, json_string("mt_list_nodes"));
    if(gmt->mt_shoot_snap)
        json_array_append_new(jn_methods, json_string("mt_shoot_snap"));
    if(gmt->mt_activate_snap)
        json_array_append_new(jn_methods, json_string("mt_activate_snap"));
    if(gmt->mt_list_snaps)
        json_array_append_new(jn_methods, json_string("mt_list_snaps"));
    if(gmt->mt_treedbs)
        json_array_append_new(jn_methods, json_string("mt_treedbs"));
    if(gmt->mt_treedb_topics)
        json_array_append_new(jn_methods, json_string("mt_treedb_topics"));
    if(gmt->mt_topic_desc)
        json_array_append_new(jn_methods, json_string("mt_topic_desc"));
    if(gmt->mt_topic_links)
        json_array_append_new(jn_methods, json_string("mt_topic_links"));
    if(gmt->mt_topic_hooks)
        json_array_append_new(jn_methods, json_string("mt_topic_hooks"));
    if(gmt->mt_node_parents)
        json_array_append_new(jn_methods, json_string("mt_node_parents"));
    if(gmt->mt_node_children)
        json_array_append_new(jn_methods, json_string("mt_node_children"));
    if(gmt->mt_list_instances)
        json_array_append_new(jn_methods, json_string("mt_list_instances"));
    if(gmt->mt_node_tree)
        json_array_append_new(jn_methods, json_string("mt_node_tree"));
    if(gmt->mt_topic_size)
        json_array_append_new(jn_methods, json_string("mt_topic_size"));
    if(gmt->mt_future62)
        json_array_append_new(jn_methods, json_string("mt_future62"));
    if(gmt->mt_future63)
        json_array_append_new(jn_methods, json_string("mt_future63"));
    if(gmt->mt_future64)
        json_array_append_new(jn_methods, json_string("mt_future64"));

    return jn_methods;
}

/***************************************************************************
 *  Print internal gclass's methods in json
 ***************************************************************************/
PRIVATE json_t *internalmethods2json(const LMETHOD *lmt)
{
    json_t *jn_methods = json_array();

    if(lmt) {
        while(lmt->lname) {
            json_array_append_new(jn_methods, json_string(lmt->lname));
            lmt++;
        }
    }

    return jn_methods;
}

/***************************************************************************
 *  Return a json object describing the hsdata for commands
 ***************************************************************************/
PRIVATE json_t *authdesc2json(const sdata_desc_t *it)
{
    int type = it->type;

    if(!DTP_IS_SCHEMA(type)) {
        return 0; // Only schemas please
    }

    json_t *jn_it = json_object();

    json_object_set_new(jn_it, "id", json_string(it->name));

    if(it->alias) {
        json_t *jn_alias = json_array();
        json_object_set_new(jn_it, "alias", jn_alias);
        const char **alias = it->alias;
        while(*alias) {
            json_array_append_new(jn_alias, json_string(*alias));
            alias++;
        }
    }

    json_object_set_new(jn_it, "description", json_string(it->description));

    gbuffer_t *gbuf = bits2gbuffer(sdata_flag_names, it->flag);
    if(gbuf) {
        size_t l = gbuffer_leftbytes(gbuf);
        if(l) {
            char *pflag = gbuffer_get(gbuf, l);
            json_object_set_new(jn_it, "flag", json_string(pflag));
        } else {
            json_object_set_new(jn_it, "flag", json_string(""));
        }
        gbuffer_decref(gbuf);
    }

    json_t *jn_parameters = json_array();
    json_object_set_new(jn_it, "parameters", jn_parameters);

    const sdata_desc_t *pparam = it->schema;
    while(pparam && pparam->name) {
        json_t *jn_param = json_object();
        json_object_set_new(jn_param, "id", json_string(pparam->name));
        json_object_update_missing_new(jn_param, itdesc2json(pparam));
        json_array_append_new(jn_parameters, jn_param);
        pparam++;
    }

    return jn_it;
}

/***************************************************************************
 *  Return a json object describing the hsdata for auths
 ***************************************************************************/
PUBLIC json_t *sdataauth2json(
    const sdata_desc_t *items
)
{
    json_t *jn_items = json_array();
    const sdata_desc_t *it = items;
    if(!it) {
        return jn_items;
    }
    while(it->name) {
        json_t *jn_it = authdesc2json(it);
        if(jn_it) {
            json_array_append_new(jn_items, jn_it);
        }
        it++;
    }
    return jn_items;
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE json_t *events2json(dl_list_t *dl_events)
{
    json_t *jn_events = json_array();

    // while(events->event) {
    //     json_t *jn_ev = json_object();
    //     json_object_set_new(jn_ev, "id", json_string(events->event));
    //     json_object_set_new(jn_ev, "flag", eventflag2json(events->flag));
    //     json_object_set_new(jn_ev, "authz", eventauth2json(events->authz));
    //
    //     json_object_set_new(
    //         jn_ev,
    //         "description",
    //         events->description?json_string(events->description):json_string("")
    //     );
    //
    //     json_array_append_new(jn_events, jn_ev);
    //     events++;
    // }

    event_t *event = dl_first(dl_events);
    while(event) {
        gobj_event_t event_name = event->event_type.event_name;
        event_flag_t event_flag = event->event_type.event_flag;

        json_t *jn_ev = json_object();
        json_object_set_new(jn_ev, "event_name", json_string(event_name));

        json_object_set_new(
            jn_ev,
            "event_flag",
            bits2jn_strlist(event_flag_names, event_flag)
        );

        json_array_append_new(jn_events, jn_ev);

        /*
         *  Next: event
         */
        event = dl_next(event);
    }

    return jn_events;
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE json_t *states2json(dl_list_t *dl_states)
{
    json_t *jn_states = json_object();

    state_t *state = dl_first(dl_states);
    while(state) {
        gobj_state_t state_name = state->state_name;
        dl_list_t *dl_actions = &state->dl_actions;

        json_t *jn_st = json_array();

        event_action_t *event_action = dl_first(dl_actions);
        while(event_action) {
            gobj_event_t event_name = event_action->event_name;
            gobj_action_fn action = event_action->action;
            gobj_state_t next_state = event_action->next_state;

            json_t *jn_ac = json_object();
            json_object_set_new(jn_ac, "event_name", json_string(event_name));
            json_object_set_new(jn_ac, "action", json_string(action?"action":""));
            json_object_set_new(jn_ac, "next_state", json_string(next_state?next_state:""));

            json_array_append_new(jn_st, jn_ac);

            /*
             *  Next: event/action/state
             */
            event_action = dl_next(event_action);
        }

        json_object_set_new(
            jn_states,
            state_name,
            jn_st
        );

        /*
         *  Next: state
         */
        state = dl_next(state);
    }

    return jn_states;
}

/***************************************************************************
 *  Print gclass's fsm in json
 ***************************************************************************/
PRIVATE json_t *fsm2json(gclass_t *gclass)
{
    json_t *jn_fsm = json_object();

    json_object_set_new(
        jn_fsm,
        "events",
        events2json(&gclass->dl_events)
    );
    json_object_set_new(
        jn_fsm,
        "states",
        states2json(&gclass->dl_states)
    );

    return jn_fsm;
}

/***************************************************************************
 *  Return json object with gclass's description.
 ***************************************************************************/
PUBLIC json_t *gclass2json(hgclass gclass_)
{
    gclass_t *gclass = gclass_;

    json_t *jn_dict = json_object();
    if(!gclass) {
        return jn_dict;
    }
    json_object_set_new(
        jn_dict,
        "id",
        json_string(gclass->gclass_name)
    );

    json_object_set_new(
        jn_dict,
        "gcflag",
        bits2jn_strlist(gclass_flag_names, gclass->gclass_flag)
    );

    json_object_set_new(
        jn_dict,
        "priv_size",
        json_integer((json_int_t)gclass->priv_size)
    );

    json_object_set_new(
        jn_dict,
        "attrs",
        sdatadesc2json2(gclass->attrs_table, -1, 0)
    );
    json_object_set_new(
        jn_dict,
        "commands",
        sdatacmd2json(gclass->command_table)
    );
    json_object_set_new(
        jn_dict,
        "gclass_methods",
        yunetamethods2json(gclass->gmt)
    );
    json_object_set_new(
        jn_dict,
        "internal_methods",
        internalmethods2json(gclass->lmt)
    );
    json_object_set_new(
        jn_dict,
        "FSM",
        fsm2json(gclass)
    );

    // json_object_set_new( TODO make a command to see global authz
     //     jn_dict,
     //     "Authzs global",
     //     sdataauth2json(global_authz_table)
     // );

    json_object_set_new(
         jn_dict,
         "Authzs gclass", // Access Control List
         sdataauth2json(gclass->authz_table)
    );

    json_object_set_new(
        jn_dict,
        "info_gclass_trace",
        gobj_trace_level_list(gclass)
    );

    json_object_set_new(
        jn_dict,
        "gclass_trace_level",
        gobj_get_gclass_trace_level(gclass)
    );

    json_object_set_new(
        jn_dict,
        "gclass_trace_no_level",
        gobj_get_gclass_trace_no_level(gclass)
    );

    json_object_set_new(
        jn_dict,
        "instances",
        json_integer(gclass->instances)
    );

    return jn_dict;
}

/***************************************************************************
 *  Return a dict with gobj's description.
 ***************************************************************************/
PUBLIC json_t *gobj2json( // Return a dict with gobj's description.
    hgobj gobj_,
    json_t *jn_filter // owned
) {
    gobj_t * gobj = gobj_;
    json_t *jn_dict = json_object();

    if(kw_find_str_in_list(gobj, jn_filter, "fullname")!=-1 || !json_array_size(jn_filter)) {
        json_object_set_new(
            jn_dict,
            "fullname",
            json_string(gobj_full_name(gobj))
        );
    }
    if(kw_find_str_in_list(gobj, jn_filter, "shortname")!=-1 || !json_array_size(jn_filter)) {
        json_object_set_new(
            jn_dict,
            "shortname",
            json_string(gobj_short_name(gobj))
        );
    }
    if(kw_find_str_in_list(gobj, jn_filter, "gclass")!=-1 || !json_array_size(jn_filter)) {
        json_object_set_new(
            jn_dict,
            "gclass",
            json_string(gobj_gclass_name(gobj))
        );
    }
    if(kw_find_str_in_list(gobj, jn_filter, "name")!=-1 || !json_array_size(jn_filter)) {
        json_object_set_new(
            jn_dict,
            "name",
            json_string(gobj_name(gobj))
        );
    }
    if(kw_find_str_in_list(gobj, jn_filter, "parent")!=-1 || !json_array_size(jn_filter)) {
        json_object_set_new(
            jn_dict,
            "parent",
            json_string(gobj_short_name(gobj_parent(gobj)))
        );
    }
    if(kw_find_str_in_list(gobj, jn_filter, "attrs")!=-1 || !json_array_size(jn_filter)) {
        json_object_set_new(
            jn_dict,
            "attrs",
            gobj_read_attrs(gobj, SDF_PUBLIC_ATTR, gobj)
        );
    }

    if(kw_find_str_in_list(gobj, jn_filter, "user_data")!=-1 || !json_array_size(jn_filter)) {
        json_object_set(
            jn_dict,
            "user_data",
            gobj_read_user_data(gobj, NULL)
        );
    }

    if(kw_find_str_in_list(gobj, jn_filter, "gobj_flags")!=-1 || !json_array_size(jn_filter)) {
        json_object_set_new(
            jn_dict,
            "gobj_flags",
            bits2jn_strlist(gobj_flag_names, ((gobj_t *) gobj)->gobj_flag)
        );
    }

    if(kw_find_str_in_list(gobj, jn_filter, "state")!=-1 || !json_array_size(jn_filter)) {
        json_object_set_new(
            jn_dict,
            "state",
            json_string(gobj_current_state(gobj))
        );
    }

    if(kw_find_str_in_list(gobj, jn_filter, "running")!=-1 || !json_array_size(jn_filter)) {
        json_object_set_new(
            jn_dict,
            "running",
            gobj_is_running(gobj) ? json_true() : json_false()
        );
    }
    if(kw_find_str_in_list(gobj, jn_filter, "playing")!=-1 || !json_array_size(jn_filter)) {
        json_object_set_new(
            jn_dict,
            "playing",
            gobj_is_playing(gobj) ? json_true() : json_false()
        );
    }
    if(kw_find_str_in_list(gobj, jn_filter, "service")!=-1 || !json_array_size(jn_filter)) {
        json_object_set_new(
            jn_dict,
            "service",
            gobj_is_service(gobj) ? json_true() : json_false()
        );
    }
    if(kw_find_str_in_list(gobj, jn_filter, "bottom_gobj")!=-1 || !json_array_size(jn_filter)) {
        json_object_set_new(
            jn_dict,
            "is_bottom_gobj",
            gobj_is_bottom_gobj(gobj)? json_true() : json_false()
        );
        hgobj bottom_gobj = gobj_bottom_gobj(gobj);
        if(bottom_gobj) {
            json_object_set_new(
                jn_dict,
                "bottom_gobj",
                json_string(gobj_short_name(bottom_gobj))
            );
        }
    }
    if(kw_find_str_in_list(gobj, jn_filter, "disabled")!=-1 || !json_array_size(jn_filter)) {
        json_object_set_new(
            jn_dict,
            "disabled",
            gobj_is_disabled(gobj) ? json_true() : json_false()
        );
    }
    if(kw_find_str_in_list(gobj, jn_filter, "volatil")!=-1 || !json_array_size(jn_filter)) {
        json_object_set_new(
            jn_dict,
            "volatil",
            gobj_is_volatil(gobj) ? json_true() : json_false()
        );
    }
    if(kw_find_str_in_list(gobj, jn_filter, "commands")!=-1 || !json_array_size(jn_filter)) {
        json_object_set_new(
            jn_dict,
            "commands",
            gobj->gclass->command_table ? json_true() : json_false()
        );
    }
    if(kw_find_str_in_list(gobj, jn_filter, "gobj_trace_level")!=-1 || !json_array_size(jn_filter)) {
        json_object_set_new(
            jn_dict,
            "gobj_trace_level",
            gobj_get_gobj_trace_level(gobj)
        );
    }
    if(kw_find_str_in_list(gobj, jn_filter, "gobj_trace_no_level")!=-1 || !json_array_size(jn_filter)) {
        json_object_set_new(
            jn_dict,
            "gobj_trace_no_level",
            gobj_get_gobj_trace_no_level(gobj)
        );
    }

    JSON_DECREF(jn_filter)
    return jn_dict;
}

/***************************************************************************
 *  View a gobj tree, with states of running/playing and attributes
 ***************************************************************************/
PRIVATE int _add_gobj_tree(
    json_t *jn_dict,
    gobj_t * gobj,
    json_t *jn_filter // not owned
) {
    json_t *jn_gobj = gobj2json(gobj, json_incref(jn_filter));
    json_object_set_new(jn_dict, gobj_short_name(gobj), jn_gobj);

    if(gobj_child_size(gobj)>0) {
        json_t *jn_data = json_object();
        json_object_set_new(jn_gobj, "children", jn_data);

        gobj_t *child = gobj_first_child(gobj);

        while(child) {
            _add_gobj_tree(jn_data, child, jn_filter);
            child = gobj_next_child(child);
        }
    }
    return 0;
}
PUBLIC json_t *gobj_view_tree(  // Return tree with gobj's tree.
    hgobj gobj,
    json_t *jn_filter // owned
) {
    json_t *jn_tree = json_object();
    _add_gobj_tree(jn_tree, gobj, jn_filter);
    JSON_DECREF(jn_filter)
    return jn_tree;
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
#ifdef CONFIG_DEBUG_PRINT_YEV_LOOP_TIMES
    if(measuring_cur_type) {
        MT_PRINT_TIME(yev_time_measure, "⏩ gobj_send_event()");
    }
#endif
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
    BOOL tracea = is_machine_tracing(dst, event) && !is_machine_not_tracing(src, event);
    __inside__ ++;

    event_action_t *event_action = _find_event_action(state, event);
    if(!event_action) {
        if(dst->gclass->gmt->mt_inject_event) {
            __inside__ --;
            if(tracea) {
                if(trace_machine_format) {
                    trace_machine("🔜 %s%s%s %s%s %s%s%s",
                        On_Black RRed,
                        event?event:"",
                        Color_Off,
                        (!dst->running)?"!!":"",
                        gobj_short_name(dst),
                        On_Black RGreen,
                        state->state_name,
                        Color_Off
                    );
                } else {
                    trace_machine("🔜 mach(%s%s), st: %s, ev: %s, from(%s%s)",
                        (!dst->running)?"!!":"",
                        gobj_short_name(dst),
                        state->state_name,
                        event?event:"",
                        (src && !src->running)?"!!":"",
                        gobj_short_name(src)
                    );
                }
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
            trace_machine(
                "📛 mach(%s%s^%s), st: %s, ev: %s, 📛📛ERROR Event NOT DEFINED in state📛📛, from(%s%s^%s)",
                (!dst->running)?"!!":"",
                gobj_gclass_name(dst), gobj_name(dst),
                state->state_name,
                event?event:"",
                (src && !src->running)?"!!":"",
                gobj_gclass_name(src), gobj_name(src)
            );
        }
        gobj_log_error(dst, LOG_OPT_TRACE_STACK,
            "msgset",       "%s", MSGSET_PARAMETER_ERROR,
            "msg",          "%s", "Event NOT DEFINED in state",
            "msg2",          "%s", "📛📛 Event NOT DEFINED in state 📛📛",
            "gclass_name",  "%s", dst->gclass->gclass_name,
            "state_name",   "%s", state->state_name,
            "event",        "%s", event,
            "src",          "%s", gobj_short_name(src),
            NULL
        );

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
        if(trace_machine_format) {
            trace_machine("🔄 %s%s%s %s%s %s%s%s",
                On_Black RBlue,
                event?event:"",
                Color_Off,
                (!dst->running)?"!!":"",
                gobj_short_name(dst),
                On_Black RGreen,
                state->state_name,
                Color_Off
            );
        } else {
            trace_machine("🔄 mach(%s%s), st: %s, ev: %s%s%s, from(%s%s^%s)",
                (!dst->running)?"!!":"",
                gobj_short_name(dst),
                state->state_name,
                On_Black RBlue,
                event?event:"",
                Color_Off,
                (src && !src->running)?"!!":"",
                gobj_gclass_name(src), gobj_name(src)
            );
        }

        if(kw) {
            if(__trace_gobj_ev_kw__(dst)) {
                if(json_object_size(kw)) {
                    gobj_trace_json(dst, kw, "kw exec event: %s", event?event:"");
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
        if(trace_machine_format) {
            // No trace return
        } else {
            trace_machine("<- mach(%s%s^%s), st: %s, ev: %s, ret: %d",
                (!dst->running)?"!!":"",
                gobj_gclass_name(dst), gobj_name(dst),
                dst->current_state->state_name,
                event?event:"",
                ret
            );
        }
    }

    __inside__ --;

#ifdef CONFIG_DEBUG_PRINT_YEV_LOOP_TIMES
    if(measuring_cur_type) {
        MT_PRINT_TIME(yev_time_measure, "⏪ gobj_send_event()");
    }
#endif

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
    state_t *new_state = _find_state(gobj->gclass, state_name);
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

    BOOL tracea = is_machine_tracing(gobj, EV_STATE_CHANGED);
    BOOL tracea_states = __trace_gobj_states__(gobj)?TRUE:FALSE;
    if(tracea || tracea_states) {
        if(trace_machine_format) {
            // No trace state
        } else {
            trace_machine("🔀🔀 mach(%s%s), new st(%s%s%s), prev st(%s%s%s)",
                (!gobj->running)?"!!":"",
                gobj_short_name(gobj),
                On_Black RGreen,
                gobj_current_state(gobj),
                Color_Off,
                On_Black RGreen,
                gobj->last_state->state_name,
                Color_Off
            );
        }
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
PUBLIC BOOL gobj_in_this_state(hgobj hgobj, gobj_state_t state)
{
    gobj_t *gobj = (gobj_t *)hgobj;
    if(gobj->current_state->state_name == state) {
        return TRUE;
    }
    return FALSE;
}

/***************************************************************************
 *
 ***************************************************************************/
PUBLIC BOOL gobj_has_state(hgobj gobj_, gobj_state_t gobj_state)
{
    gobj_t *gobj = (gobj_t *)gobj_;

    state_t *state = _find_state(gobj->gclass, gobj_state);
    if(state) {
        return TRUE;
    }
    return FALSE;
}

/***************************************************************************
 *
 ***************************************************************************/
PUBLIC BOOL gobj_has_event(hgobj gobj, gobj_event_t event, event_flag_t event_flag)
{
    event_type_t *event_type = gobj_event_type(gobj, event, FALSE);
    if(!event_type) {
        return FALSE;
    }

    if(event_flag && !(event_type->event_flag & event_flag)) {
        return FALSE;
    }
    return TRUE;
}

/***************************************************************************
 *
 ***************************************************************************/
PUBLIC BOOL gobj_has_output_event(hgobj gobj, gobj_event_t event, event_flag_t event_flag)
{
    event_type_t *event_type = gobj_event_type(gobj, event, FALSE);
    if(!event_type) {
        return FALSE;
    }

    if(event_flag && !(event_type->event_flag & event_flag)) {
        return FALSE;
    }

    if(!(event_type->event_flag & EVF_OUTPUT_EVENT)) {
        return FALSE;
    }

    return TRUE;
}

/***************************************************************************
 *
 ***************************************************************************/
PUBLIC event_type_t *gobj_event_type( // silent function
    hgobj gobj_,
    gobj_event_t event,
    BOOL include_system_events
)
{
    if(gobj_ == NULL) {
        gobj_log_error(NULL, LOG_OPT_TRACE_STACK,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_PARAMETER_ERROR,
            "msg",          "%s", "hgobj NULL",
            "event",        "%s", event,
            NULL
        );
        return NULL;
    }
    gobj_t *gobj = (gobj_t *)gobj_;

    if(!event) {
        gobj_log_error(NULL, LOG_OPT_TRACE_STACK,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_PARAMETER_ERROR,
            "msg",          "%s", "event NULL",
            NULL
        );
        return NULL;
    }

    event_type_t *event_type = gclass_find_event_type(gobj->gclass, event);
    if(event_type) {
        return event_type;
    }

    if(include_system_events) {
        /*
         *  Check global (gobj) output events
         */
        event_t *event_ = dl_first(&dl_global_event_types);
        while(event_) {
            if(event_->event_type.event_name && event_->event_type.event_name == event) {
                return &event_->event_type;
            }
            event_ = dl_next(event_);
        }
    }

    return NULL;
}

/***************************************************************************
 *
 ***************************************************************************/
PUBLIC event_type_t *gobj_event_type_by_name(hgobj gobj_, const char *event_name)
{
    if(gobj_ == NULL) {
        gobj_log_error(NULL, LOG_OPT_TRACE_STACK,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_PARAMETER_ERROR,
            "msg",          "%s", "hgobj NULL",
            "event",        "%s", event_name,
            NULL
        );
        return NULL;
    }
    gobj_t *gobj = (gobj_t *)gobj_;

    if(empty_string(event_name)) {
        gobj_log_error(NULL, LOG_OPT_TRACE_STACK,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_PARAMETER_ERROR,
            "msg",          "%s", "event_name NULL",
            NULL
        );
        return NULL;
    }

    event_t *event_ = dl_first(&gobj->gclass->dl_events);
    while(event_) {
        if(event_->event_type.event_name && strcasecmp(event_->event_type.event_name, event_name)==0) {
            return &event_->event_type;
        }
        event_ = dl_next(event_);
    }

    /*
     *  Check global (gobj) output events
     */
    event_ = dl_first(&dl_global_event_types);
    while(event_) {
        if(event_->event_type.event_name && strcasecmp(event_->event_type.event_name, event_name)==0) {
            return &event_->event_type;
        }
        event_ = dl_next(event_);
    }

    return NULL;
}




                    /*------------------------------------*
                     *      Publication/Subscriptions
                     *------------------------------------*/




typedef enum {
    __rename_event_name__   = 0x00000001,
    __hard_subscription__   = 0x00000002,
    __own_event__           = 0x00000004,   // If gobj_send_event return -1 don't continue publishing
} subs_flag_t;

/*
 *
 */
PRIVATE sdata_desc_t subscription_desc[] = {
/*-ATTR-type--------name----------------flag----default-----description---------- */
SDATA (DTP_POINTER, "publisher",        0,      NULL,       "publisher gobj"),
SDATA (DTP_POINTER, "subscriber",       0,      NULL,       "subscriber gobj"),
SDATA (DTP_POINTER, "event",            0,      NULL,       "event name subscribed"),
SDATA (DTP_POINTER, "renamed_event",    0,      NULL,       "rename event name"),
SDATA (DTP_INTEGER, "subs_flag",        0,      0,          "subscription flag. See subs_flag_t"),
SDATA (DTP_JSON,    "__config__",       0,      NULL,       "subscription config"),
SDATA (DTP_JSON,    "__global__",       0,      NULL,       "global kw, merge in publishing"),
SDATA (DTP_JSON,    "__local__",        0,      NULL,       "local kw, remove in publishing"),
SDATA (DTP_JSON,    "__filter__",       0,      NULL,       "filter kw, filter in publishing"),
SDATA (DTP_STRING,  "__service__",      0,      "",         "subscription service"),
SDATA_END()
};

/***************************************************************************
 *
 ***************************************************************************/
PUBLIC const sdata_desc_t *gobj_subs_desc(void)
{
    return subscription_desc;
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE json_t * _create_subscription(
    gobj_t * publisher,
    gobj_event_t event,
    json_t *kw, // not owned
    gobj_t * subscriber)
{
    json_t *subs = sdata_create(publisher, subscription_desc);
    json_object_set_new(subs, "event", json_integer((json_int_t)(size_t)event));
    json_object_set_new(subs, "subscriber", json_integer((json_int_t)(size_t)subscriber));
    json_object_set_new(subs, "publisher", json_integer((json_int_t)(size_t)publisher));
    subs_flag_t subs_flag = 0;

    if(kw) {
        json_t *__config__ = kw_get_dict(publisher, kw, "__config__", 0, 0);
        json_t *__global__ = kw_get_dict(publisher, kw, "__global__", 0, 0);
        json_t *__local__ = kw_get_dict(publisher, kw, "__local__", 0, 0);
        json_t *__filter__ = kw_get_dict_value(publisher, kw, "__filter__", 0, 0);
        const char *__service__ = kw_get_str(publisher, kw, "__service__", "", 0);

        if(json_size(__global__)>0) {
            json_t *kw_clone = json_deep_copy(__global__);
            json_object_set_new(subs, "__global__", kw_clone);
        }
        if(json_size(__config__)>0) {
            json_t *kw_clone = json_deep_copy(__config__);
            json_object_set_new(subs, "__config__", kw_clone);

// TODO
//            if(kw_has_key(kw_clone, "__rename_event_name__")) {
//                const char *renamed_event = kw_get_str(kw_clone, "__rename_event_name__", 0, 0);
//                sdata_write_str(subs, "renamed_event", renamed_event);
//                json_object_del(kw_clone, "__rename_event_name__");
//                subs_flag |= __rename_event_name__;
//
//                // Get/Create __global__
//                json_t *kw_global = sdata_read_json(subs, "__global__");
//                if(!kw_global) {
//                    kw_global = json_object();
//                    sdata_write_json(subs, "__global__", kw_global);
//                    kw_decref(kw_global); // Incref above
//                }
//                kw_set_dict_value(
//                    kw_global,
//                    "__original_event_name__",
//                    json_string(event)
//                );
//            }

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
        if(json_size(__local__)>0) {
            json_t *kw_clone = json_deep_copy(__local__);
            json_object_set_new(subs, "__local__", kw_clone);
        }
        if(json_size(__filter__)>0) {
            json_t *kw_clone = json_deep_copy(__filter__);
            json_object_set_new(subs, "__filter__", kw_clone);
        }
        if(!empty_string(__service__)) {
            json_object_set_new(subs, "__service__", json_string(__service__));
        }
    }
    json_object_set_new(subs, "subs_flag", json_integer((json_int_t)subs_flag));

    return subs;
}

/***************************************************************************
 *  Match subscription
 ***************************************************************************/
PRIVATE BOOL _match_subscription(
    json_t *subs,
    gobj_t *publisher,
    gobj_event_t event,
    json_t *kw, // NOT owned
    gobj_t *subscriber
) {
    json_t *__config__ = kw_get_dict(publisher, kw, "__config__", 0, 0);
    json_t *__global__ = kw_get_dict(publisher, kw, "__global__", 0, 0);
    json_t *__local__ = kw_get_dict(publisher, kw, "__local__", 0, 0);
    json_t *__filter__ = kw_get_dict_value(publisher, kw, "__filter__", 0, 0);

    if(publisher) {
        gobj_t *publisher_ = (gobj_t *)(size_t)kw_get_int(0, subs, "publisher", 0, KW_REQUIRED);
        if(publisher != publisher_) {
            return FALSE;
        }
    }

    if(subscriber) {
        gobj_t *subscriber_ = (gobj_t *)(size_t)kw_get_int(0, subs, "subscriber", 0, KW_REQUIRED);
        if(subscriber != subscriber_) {
            return FALSE;
        }
    }

    if(event) {
        gobj_event_t event_ = (gobj_event_t)(size_t)kw_get_int(0, subs, "event", 0, KW_REQUIRED);
        if(event != event_) {
            return FALSE;
        }
    }

    if(json_size(__config__)>0) {
        json_t *kw_config = kw_get_dict(0, subs, "__config__", 0, 0);
        if(json_size(kw_config)>0) {
            if(!json_is_identical(kw_config, __config__)) {
                return FALSE;
            }
        } else {
            return FALSE;
        }
    }
    if(json_size(__global__)>0) {
        json_t *kw_global = kw_get_dict(0, subs, "__global__", 0, 0);
        if(json_size(kw_global)>0) {
            if(!json_is_identical(kw_global, __global__)) {
                return FALSE;
            }
        } else {
            return FALSE;
        }
    }
    if(json_size(__local__)>0) {
        json_t *kw_local = kw_get_dict(0, subs, "__local__", 0, 0);
        if(json_size(kw_local)>0) {
            if(!json_is_identical(kw_local, __local__)) {
                return FALSE;
            }
        } else {
            return FALSE;
        }
    }
    if(json_size(__filter__)>0) {
        json_t *kw_filter = kw_get_dict_value(0, subs, "__filter__", 0, 0);
        if(json_size(kw_filter)>0) {
            if(!json_is_identical(kw_filter, __filter__)) {
                return FALSE;
            }
        } else {
            return FALSE;
        }
    }

    return TRUE;
}

/***************************************************************************
 *  Return a list of subscriptions in the list dl_subs
 *  filtering by matching:
 *      publisher
 *      event,
 *      kw (__config__, __global__, __local__, __filter__),
 *      subscriber
 ***************************************************************************/
PRIVATE json_t * _find_subscriptions(
    json_t *dl_subs,
    gobj_t *publisher,
    gobj_event_t event,
    json_t *kw, // owned
    gobj_t *subscriber
) {
    json_t *iter = json_array();

    size_t idx; json_t *subs;
    json_array_foreach(dl_subs, idx, subs) {
        if(_match_subscription(
            subs,
            publisher,
            event,
            kw, // NOT owned
            subscriber
        )) {
            json_array_append(iter, subs);
        }
    }

    KW_DECREF(kw)
    return iter;
}

/***************************************************************************
 *  Find idx of subscription in dl_subs, -1 not found
 ***************************************************************************/
PRIVATE int _get_subs_idx(
    json_t *dl_subs,
    gobj_t *publisher,
    gobj_event_t event,
    json_t *kw, // NOT owned
    gobj_t *subscriber
) {
    size_t idx; json_t *subs;
    json_array_foreach(dl_subs, idx, subs) {
        if(_match_subscription(
            subs,
            publisher,
            event,
            kw, // NOT owned
            subscriber
        )) {
            return (int)idx;
        }
    }

    return -1;
}

/***************************************************************************
 *  Delete subscription in publisher and subscriber
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
    BOOL trace = __trace_gobj_subscriptions__(subscriber) || __trace_gobj_subscriptions__(publisher);
    if(trace) {
        trace_machine(
            "💜💜👎 unsubscribing event '%s': subscriber'%s', publisher %s",
            event?event:"",
            gobj_short_name(subscriber),
            gobj_short_name(publisher)
        );

        if(__trace_gobj_ev_kw__(subscriber) || __trace_gobj_ev_kw__(publisher)) {
            gobj_trace_json(
                gobj,
                subs,
                "💜💜👎 unsubscribing event '%s': subscriber'%s', publisher %s",
                event?event:"",
                gobj_short_name(subscriber),
                gobj_short_name(publisher)
            );
        }
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
    int idx = _get_subs_idx(
        publisher->dl_subscriptions,
        publisher,
        event,
        subs,
        subscriber
    );

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

    idx = _get_subs_idx(
        subscriber->dl_subscribings,
        publisher,
        event,
        subs,
        subscriber
    );
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

    return 0;
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

    if(kw == NULL) {
        kw = json_object();
    }

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

    const char *key; json_t *v;
    json_object_foreach(kw, key, v) {
        if(!(strcmp(key, "__config__")==0 ||
            strcmp(key, "__global__")==0 ||
            strcmp(key, "__local__")==0 ||
            strcmp(key, "__filter__")==0 ||
            strcmp(key, "__service__")==0
        )) {
            gobj_log_warning(publisher, LOG_OPT_TRACE_STACK,
                "function",     "%s", __FUNCTION__,
                "msgset",       "%s", MSGSET_PARAMETER_ERROR,
                "msg",          "%s", "key ignored in subscription",
                "key",          "%s", key,
                "event",        "%s", event,
                NULL
            );
        }
    }

    /*--------------------------------------------------------------*
     *  Event must be in output event list
     *  You can avoid this with gcflag_no_check_output_events flag
     *--------------------------------------------------------------*/
    if(!empty_string(event)) {
        if(!gobj_has_output_event(publisher, event, EVF_OUTPUT_EVENT)) {
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
    } else {
        event = NULL;  // In C use NULL, in JS or others use ""
    }

    /*-------------------------------------------------*
     *  Check AUTHZ
     *-------------------------------------------------*/
    // if(output_event->authz & EV_AUTHZ_SUBSCRIBE) {}

    /*------------------------------*
     *  Find repeated subscription
     *------------------------------*/
    json_t *dl_subs = _find_subscriptions(
        publisher->dl_subscriptions,
        publisher,
        event,
        json_incref(kw),
        subscriber
    );
    if(json_array_size(dl_subs) > 0) {
        gobj_log_warning(publisher, LOG_OPT_TRACE_STACK,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_PARAMETER_ERROR,
            "msg",          "%s", "subscription(s) REPEATED, will be deleted and override",
            "event",        "%s", event,
            "kw",           "%j", kw,
            "publisher",    "%s", gobj_full_name(publisher),
            "subscriber",   "%s", gobj_full_name(subscriber),
            NULL
        );
        gobj_unsubscribe_list(publisher, json_incref(dl_subs), FALSE);
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
    BOOL trace = __trace_gobj_subscriptions__(subscriber) || __trace_gobj_subscriptions__(publisher);
    if(trace) {
        trace_machine(
            "💜💜👍 subscribing event '%s', subscriber'%s', publisher %s",
            event?event:"",
            gobj_short_name(subscriber),
            gobj_short_name(publisher)
        );

        if(kw && json_object_size(kw)) {
            if(__trace_gobj_ev_kw__(subscriber) || __trace_gobj_ev_kw__(publisher)) {
                gobj_trace_json(
                    publisher,
                    subs,
                    "💜💜👍 subscribing event '%s', subscriber'%s', publisher %s",
                    event?event:"",
                    gobj_short_name(subscriber),
                    gobj_short_name(publisher)
                );
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

    if(kw == NULL) {
        kw = json_object();
    }

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

    const char *key; json_t *v;
    json_object_foreach(kw, key, v) {
        if(!(strcmp(key, "__config__")==0 ||
            strcmp(key, "__global__")==0 ||
            strcmp(key, "__local__")==0 ||
            strcmp(key, "__filter__")==0 ||
            strcmp(key, "__service__")==0
        )) {
            gobj_log_warning(publisher, LOG_OPT_TRACE_STACK,
                "function",     "%s", __FUNCTION__,
                "msgset",       "%s", MSGSET_PARAMETER_ERROR,
                "msg",          "%s", "key ignored in subscription",
                "key",          "%s", key,
                "event",        "%s", event,
                NULL
            );
        }
    }

    /*--------------------------------------------------------------*
     *  Event must be in output event list
     *  You can avoid this with gcflag_no_check_output_events flag
     *--------------------------------------------------------------*/
    if(!empty_string(event)) {
        if(!gobj_has_output_event(publisher, event, EVF_OUTPUT_EVENT)) {
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
    } else {
        event = NULL;  // In C use NULL, in JS or others use ""
    }

    /*-----------------------------*
     *      Find subscription
     *-----------------------------*/
    json_t *dl_subs = _find_subscriptions(
        publisher->dl_subscriptions,
        publisher,
        event,
        json_incref(kw),
        subscriber
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
    hgobj gobj,
    json_t *dl_subs, // owned
    BOOL force  // delete hard_subscription subs too
)
{
    json_t *dl = json_copy(dl_subs);

    size_t idx; json_t *subs=0;
    json_array_foreach(dl, idx, subs) {
        _delete_subscription(gobj, subs, force, FALSE);
    }
    JSON_DECREF(dl_subs)
    JSON_DECREF(dl)
    return 0;
}

/***************************************************************************
 *  Return list of subscriptions in the publisher gobj,
 *  filtering by matching:
 *      event,
 *      kw (__config__, __global__, __local__, __filter__),
 *      subscriber
 ***************************************************************************/
PUBLIC json_t *gobj_find_subscriptions(
    hgobj gobj_,
    gobj_event_t event,
    json_t *kw,
    hgobj subscriber)
{
    gobj_t * publisher = gobj_;
    if(kw == NULL) {
        kw = json_object();
    }
    return _find_subscriptions(
        publisher->dl_subscriptions,
        gobj_,
        event,
        kw,
        subscriber
    );
}

/***************************************************************************
 *  Return a list of subscribings
 *  filtering by matching:
 *      event,
 *      kw (__config__, __global__, __local__, __filter__),
 *      subscriber
 ***************************************************************************/
PUBLIC json_t *gobj_find_subscribings(
    hgobj gobj_,
    gobj_event_t event,
    json_t *kw,             // kw (__config__, __global__, __local__, __filter__)
    hgobj publisher
)
{
    gobj_t * gobj = gobj_;
    if(kw == NULL) {
        kw = json_object();
    }
    return _find_subscriptions(
        gobj->dl_subscribings,
        publisher,
        event,
        kw,
        gobj
    );
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE json_t *get_subs_info(hgobj gobj, json_t *sub)
{
    json_t *jn_sub = json_object();

    hgobj publisher = (hgobj)(size_t)kw_get_int(gobj, sub, "publisher", 0, 0);
    hgobj subscriber = (hgobj)(size_t)kw_get_int(gobj, sub, "subscriber", 0, 0);
    gobj_event_t event = (gobj_event_t)(size_t)kw_get_int(gobj, sub, "event", 0, 0);
    gobj_event_t renamed_event = (gobj_event_t)(size_t)kw_get_int(gobj, sub, "renamed_event", 0, 0);
    json_int_t subs_flag = kw_get_int(gobj, sub, "subs_flag", 0, 0);
    json_t *__config__ = kw_get_dict(gobj, sub, "__config__", 0, 0);
    json_t *__global__ = kw_get_dict(gobj, sub, "__global__", 0, 0);
    json_t *__local__ = kw_get_dict(gobj, sub, "__local__", 0, 0);
    json_t *__filter__ = kw_get_dict_value(gobj, sub, "__filter__", 0, 0);
    const char *__service__ = kw_get_str(gobj, sub, "__service__", "", 0);

    json_object_set_new(jn_sub, "event", event?json_string(event):json_null());
    json_object_set_new(jn_sub, "publisher", json_string(gobj_short_name(publisher)));
    json_object_set_new(jn_sub, "subscriber", json_string(gobj_short_name(subscriber)));
    json_object_set_new(jn_sub, "renamed_ev", renamed_event?json_string(renamed_event):json_null());
    json_object_set_new(jn_sub, "flag", json_integer(subs_flag));

    json_object_set(jn_sub, "__config__", __config__?__config__:json_null());
    json_object_set(jn_sub, "__global__", __global__?__global__:json_null());
    json_object_set(jn_sub, "__local__", __local__?__local__:json_null());
    json_object_set(jn_sub, "__filter__", __filter__?__filter__:json_null());

    json_object_set_new(jn_sub, "__service__", json_string(__service__));

    return jn_sub;
}

/***************************************************************************
 *
 ***************************************************************************/
PUBLIC json_t *gobj_list_subscriptions(
    hgobj gobj,
    gobj_event_t event,
    json_t *kw,             // kw (__config__, __global__, __local__)
    hgobj subscriber
) {
    json_t *jn_subscriptions = json_array();

    json_t *subscriptions = gobj_find_subscriptions( // Return is YOURS
        gobj,
        event,      // event,
        kw,         // kw (__config__, __global__, __local__)
        subscriber  // subscriber
    );
    int idx; json_t *sub;
    json_array_foreach(subscriptions, idx, sub) {
      	json_t *jn_sub = get_subs_info(gobj, sub);
        json_array_append_new(jn_subscriptions, jn_sub);
    }
    JSON_DECREF(subscriptions)

    return jn_subscriptions;
}

/***************************************************************************
 *
 ***************************************************************************/
PUBLIC json_t *gobj_list_subscribings(
    hgobj gobj,
    gobj_event_t event,
    json_t *kw,             // kw (__config__, __global__, __local__)
    hgobj subscriber
) {
    json_t *jn_subscribings = json_array();

    json_t *subscribings = gobj_find_subscribings( // Return is YOURS
        gobj,
        event,      // event,
        kw,         // kw (__config__, __global__, __local__)
        subscriber  // subscriber
    );
    int idx; json_t *sub;
    json_array_foreach(subscribings, idx, sub) {
      	json_t *jn_sub = get_subs_info(gobj, sub);
        json_array_append_new(jn_subscribings, jn_sub);
    }
    JSON_DECREF(subscribings)

    return jn_subscribings;
}

/***************************************************************************
 *  Return the sum of returns of gobj_send_event
 ***************************************************************************/
PUBLIC int gobj_publish_event(
    hgobj publisher_,
    gobj_event_t event,
    json_t *kw)
{
    gobj_t * publisher = publisher_;

#ifdef CONFIG_DEBUG_PRINT_YEV_LOOP_TIMES
    if(measuring_cur_type) {
        MT_PRINT_TIME(yev_time_measure, "⏩ gobj_publish_event()");
    }
#endif

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
        return -1;
    }
    if(publisher->obflag & obflag_destroyed) {
        gobj_log_error(0, LOG_OPT_TRACE_STACK,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_PARAMETER_ERROR,
            "msg",          "%s", "gobj DESTROYED",
            NULL
        );
        KW_DECREF(kw)
        return -1;
    }
    if(empty_string(event)) {
        gobj_log_error(publisher, LOG_OPT_TRACE_STACK,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_PARAMETER_ERROR,
            "msg",          "%s", "event EMPTY",
            NULL
        );
        KW_DECREF(kw)
        return -1;
    }

    /*--------------------------------------------------------------*
     *  Event must be in output event list
     *  You can avoid this with gcflag_no_check_output_events flag
     *--------------------------------------------------------------*/
    event_type_t *ev = gobj_event_type(publisher, event, TRUE);
    if(!(ev && ev->event_flag & (EVF_SYSTEM_EVENT|EVF_OUTPUT_EVENT))) {
        /*
         *  HACK ev can be null,
         *  there are gclasses like c_iogate that are intermediate of other events.
         */
        if(!(publisher->gclass->gclass_flag & gcflag_no_check_output_events)) {
            gobj_log_error(publisher, 0,
                "function",     "%s", __FUNCTION__,
                "msgset",       "%s", MSGSET_PARAMETER_ERROR,
                "msg",          "%s", "event NOT in output event list",
                "event",        "%s", event,
                NULL
            );
            KW_DECREF(kw)
            return -1;
        }
    }

    BOOL tracea = (__trace_gobj_subscriptions__(publisher) || is_machine_tracing(publisher, event)) &&
        !is_machine_not_tracing(publisher, event);
    if(tracea) {
        if(trace_machine_format) {
            trace_machine("🔝🔝 %s%s%s %s%s %s%s%s",
                On_Black BBlue,
                event?event:"",
                Color_Off,
                (!publisher->running)?"!!":"",
                gobj_short_name(publisher),
                On_Black RGreen,
                publisher->current_state->state_name,
                Color_Off
            );
        } else {
            trace_machine("🔝🔝 mach(%s%s), st: %s, ev: %s%s%s",
                (!publisher->running)?"!!":"",
                gobj_short_name(publisher),
                publisher->current_state->state_name,
                On_Black BBlue,
                event?event:"",
                Color_Off
            );
        }
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
            return topublish;
        }
    }

    /*--------------------------------------------------------------*
     *      Default publication method
     *--------------------------------------------------------------*/
    json_t *dl_subs = json_copy(publisher->dl_subscriptions); // Protect to inside deleted subs
    int sent_count = 0;
    int ret = 0;
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
        subs_flag_t subs_flag = (subs_flag_t)kw_get_int(
            publisher, subs, "subs_flag", 0, KW_REQUIRED
        );
        gobj_event_t event_ = (gobj_event_t)(size_t)kw_get_int(
            publisher, subs, "event", 0, KW_REQUIRED
        );
        if(empty_string(event_) || strcasecmp(event_, event)==0) {
            json_t *__config__ = kw_get_dict(publisher, subs, "__config__", 0, 0);
            json_t *__global__ = kw_get_dict(publisher, subs, "__global__", 0, 0);
            json_t *__local__ = kw_get_dict(publisher, subs, "__local__", 0, 0);
            json_t *__filter__ = kw_get_dict_value(publisher, subs, "__filter__", 0, 0);

            /*
             *  Check renamed_event
             */
//   TODO review        const char *event_name = sdata_read_str(subs, "renamed_event");
//            if(empty_string(event_name)) {
//                event_name = event;
//            }

            /*
             *  Duplicate the kw to publish if not shared
             *  NOW always shared
             */
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
            } else if(json_size(__filter__)>0) {
                topublish = kw_match_simple(kw2publish , json_incref(__filter__));
                if(tracea) {
                    trace_machine(
                        "💜💜🔄%s publishing with filter, event '%s', subscriber'%s', publisher %s",
                        topublish?"👍":"👎",
                        event?event:"",
                        gobj_short_name(subscriber),
                        gobj_short_name(publisher)
                    );
                    gobj_trace_json(
                        publisher,
                        __filter__,
                        "💜💜🔄%s publishing with filter, event '%s', subscriber'%s', publisher %s",
                        topublish?"👍":"👎",
                        event?event:"",
                        gobj_short_name(subscriber),
                        gobj_short_name(publisher)
                    );
                }
            }

            if(topublish<0) {
                KW_DECREF(kw2publish)
                break;
            } else if(topublish==0) {
                /*
                 *  Must not be published
                 *  Next subs
                 */
                KW_DECREF(kw2publish)
                continue;
            }

            /*
             *  Check if System event: don't send it if subscriber has not it
             */
            event_type_t *ev_ = gobj_event_type(subscriber, event, TRUE);
            if(ev_) {
                if(ev_->event_flag & EVF_SYSTEM_EVENT) {
                    if(!gobj_has_event(subscriber, ev_->event_name, 0)) {
                        KW_DECREF(kw2publish)
                        continue;
                    }
                }
            }

            /*
             *  Remove local keys
             */
            if(json_size(__local__)>0) {
                kw_pop(kw2publish,
                    __local__ // not owned
                );
            }

            /*
             *  Apply transformation filters
             *   TODO review, I think it was used only from c_agent
             */
            if(json_size(__config__)>0) {
//                json_t *jn_trans_filters = kw_get_dict_value(
//                    publisher, __config__, "__trans_filter__", 0, 0
//                );
//                if(jn_trans_filters) {
//                    kw2publish = apply_trans_filters(publisher, kw2publish, jn_trans_filters);
//                }
            }

            /*
             *  Add global keys
             */
            if(json_size(__global__)>0) {
                json_object_update(kw2publish, __global__);
            }

            /*
             *  Send event
             */
            if(tracea) {
                if(trace_machine_format) {
                    trace_machine("🔝🔄 %s %s%s",
                        event?event:"",
                        (!subscriber->running)?"!!":"",
                        gobj_short_name(subscriber)
                    );
                } else {
                    trace_machine("🔝🔄 mach(%s%s), st: %s, ev: %s, from(%s%s)",
                        (!subscriber->running)?"!!":"",
                        gobj_short_name(subscriber),
                        gobj_current_state(subscriber),
                        event?event:"",
                        (publisher && !publisher->running)?"!!":"",
                        gobj_short_name(publisher)
                    );
                }
                if(__trace_gobj_ev_kw__(publisher)) {
                    if(json_object_size(kw2publish)) {
                        gobj_trace_json(publisher, kw2publish, "kw publish send event");
                    }
                }
            }

            int ret_ = gobj_send_event(
                subscriber,
                event,
                kw2publish,
                publisher
            );
            if(ret_ < 0 && (subs_flag & __own_event__)) {
                sent_count = -1; // Return of -1 indicates that someone owned the event
                break;
            }
            ret += ret_;
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

#ifdef CONFIG_DEBUG_PRINT_YEV_LOOP_TIMES
    if(measuring_cur_type) {
        MT_PRINT_TIME(yev_time_measure, "⏪ gobj_publish_event()");
    }
#endif

    return ret;
}




                    /*------------------------------------*
                     *  SECTION: Authorization functions
                     *------------------------------------*/




/***************************************************************************
 *  Authenticate
 *  Return json response
 *
        {
            "result": 0,        // 0 successful authentication, -1 error
            "comment": "",
            "username": ""      // username authenticated
        }

 *  HACK if there is no authentication parser the authentication is TRUE
    and the username is the current system user
 *  WARNING Becare and use no parser only in local services!
 ***************************************************************************/
PUBLIC json_t *gobj_authenticate(hgobj gobj_, json_t *kw, hgobj src)
{
    gobj_t *gobj = gobj_;
    if(!gobj || gobj->obflag & obflag_destroyed) {
        gobj_log_error(0, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_PARAMETER_ERROR,
            "msg",          "%s", "hgobj NULL or DESTROYED",
            NULL
        );
        KW_DECREF(kw)
        return 0;
    }

    /*---------------------------------------------*
     *  The local mt_authenticate has preference
     *---------------------------------------------*/
    if(gobj->gclass->gmt->mt_authenticate) {
        return gobj->gclass->gmt->mt_authenticate(gobj, kw, src);
    }

    /*-----------------------------------------------*
     *  Then use the global authzs parser
     *-----------------------------------------------*/
    if(!__global_authentication_parser_fn__) {
#ifdef __linux__
        struct passwd *pw = getpwuid(getuid());

        KW_DECREF(kw)
        return json_pack("{s:i, s:s, s:s, s:o}",
            "result", 0,
            "comment", "Working without authentication",
            "username", pw->pw_name,
            "jwt_payload", json_null()
        );
#else
        KW_DECREF(kw)
        return json_pack("{s:i, s:s, s:s, s:o}",
            "result", 0,
            "comment", "Working without authentication",
            "username", "yuneta",
            "jwt_payload", json_null()
        );
#endif
    }

    return __global_authentication_parser_fn__(gobj, kw, src);
}

/***************************************************************************
 *  Return list authzs of gobj if authz is empty
 *  else return authz dict
 ***************************************************************************/
PUBLIC json_t *authzs_list(
    hgobj gobj,
    const char *authz
)
{
    json_t *jn_list = 0;
    if(!gobj) { // Can be null
        jn_list = sdataauth2json(gobj_get_global_authz_table());
    } else {
        if(!gclass_authz_desc(gobj_gclass(gobj))) {
            return 0;
        }
        jn_list = sdataauth2json(gclass_authz_desc(gobj_gclass(gobj)));
    }

    if(empty_string(authz)) {
        return jn_list;
    }

    int idx; json_t *jn_authz;
    json_array_foreach(jn_list, idx, jn_authz) {
        const char *id = kw_get_str(gobj, jn_authz, "id", "", KW_REQUIRED);
        if(strcmp(authz, id)==0) {
            json_incref(jn_authz);
            json_decref(jn_list);
            return jn_authz;
        }
    }

    gobj_log_error(gobj, 0,
        "function",     "%s", __FUNCTION__,
        "msgset",       "%s", MSGSET_PARAMETER_ERROR,
        "msg",          "%s", "authz not found",
        "authz",        "%s", authz,
        NULL
    );

    json_decref(jn_list);
    return 0;
}

/****************************************************************************
 *  list authzs of gobj
 ****************************************************************************/
PUBLIC json_t *gobj_authzs(
    hgobj gobj  // If null return global authzs
)
{
    return authzs_list(gobj, "");
}

/****************************************************************************
 *  list authzs of gobj
 ****************************************************************************/
PUBLIC json_t *gobj_authz(
    hgobj gobj_,
    const char *authz // return all list if empty string, else return authz desc
)
{
    gobj_t *gobj = gobj_;
    if(!gobj || gobj->obflag & obflag_destroyed) {
        gobj_log_error(0, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_PARAMETER_ERROR,
            "msg",          "%s", "hgobj NULL or DESTROYED",
            NULL
        );
        return 0;
    }
    return authzs_list(gobj, authz);
}

/****************************************************************************
 *  Return if user has authz in gobj in context
 *  HACK if there is no authz checker the authz is TRUE
 ****************************************************************************/
PUBLIC BOOL gobj_user_has_authz(
    hgobj gobj_,
    const char *authz,
    json_t *kw,
    hgobj src
)
{
    gobj_t * gobj = gobj_;

    if(!gobj || gobj->obflag & obflag_destroyed) {
        gobj_log_error(0, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_PARAMETER_ERROR,
            "msg",          "%s", "hgobj NULL or DESTROYED",
            NULL
        );
        KW_DECREF(kw)
        return FALSE;
    }

    /*----------------------------------------------------*
     *  The local mt_authz_checker has preference
     *----------------------------------------------------*/
    if(gobj->gclass->gmt->mt_authz_checker) {
        BOOL has_permission = gobj->gclass->gmt->mt_authz_checker(gobj, authz, kw, src);
        if(__trace_gobj_authzs__(gobj)) {
            gobj_trace_json(gobj, kw,
                "local authzs 🔑🔑 %s => %s",
                gobj_short_name(gobj),
                has_permission?"👍":"🚫"
            );
        }
        return has_permission;
    }

    /*-----------------------------------------------*
     *  Then use the global authz checker
     *-----------------------------------------------*/
    if(__global_authorization_checker_fn__) {
        BOOL has_permission = __global_authorization_checker_fn__(gobj, authz, kw, src);
        if(__trace_gobj_authzs__(gobj)) {
            gobj_trace_json(gobj, kw,
                "global authzs 🔑🔑 %s => %s",
                gobj_short_name(gobj),
                has_permission?"👍":"🚫"
            );
        }
        return has_permission;
    }

    KW_DECREF(kw)
    return TRUE; // HACK if there is no authz checker the authz is TRUE
}

/****************************************************************************
 *
 ****************************************************************************/
PUBLIC const sdata_desc_t *gobj_get_global_authz_table(void)
{
    return global_authz_table;
}

/***************************************************************************
 *
 ***************************************************************************/
PUBLIC const sdata_desc_t *authz_get_level_desc(
    const sdata_desc_t *authz_table,
    const char *auth
)
{
    const sdata_desc_t *pcmd = authz_table;
    while(pcmd->name) {
        /*
         *  Alias have precedence if there is no json_fn authz function.
         *  It's the combination to redirect the authz as `name` event,
         */
        BOOL alias_checked = FALSE;
        if(!pcmd->json_fn && pcmd->alias) {
            alias_checked = TRUE;
            const char **alias = pcmd->alias;
            while(alias && *alias) {
                if(strcasecmp(*alias, auth)==0) {
                    return pcmd;
                }
                alias++;
            }
        }
        if(strcasecmp(pcmd->name, auth)==0) {
            return pcmd;
        }
        if(!alias_checked) {
            const char **alias = pcmd->alias;
            while(alias && *alias) {
                if(strcasecmp(*alias, auth)==0) {
                    return pcmd;
                }
                alias++;
            }
        }

        pcmd++;
    }
    return 0;
}

/***************************************************************************
 *
 ***************************************************************************/
PUBLIC json_t *gobj_build_authzs_doc(
    hgobj gobj,
    const char *cmd,
    json_t *kw
)
{
    const char *authz = kw_get_str(gobj, kw, "authz", "", 0);
    const char *service = kw_get_str(gobj, kw, "service", "", 0);

    if(!empty_string(service)) {
        hgobj service_gobj = gobj_find_service(service, FALSE);
        if(!service_gobj) {
            return json_sprintf("Service not found: '%s'", service);
        }

        json_t *jn_authzs = authzs_list(service_gobj, authz);
        if(!jn_authzs) {
            if(empty_string(authz)) {
                return json_sprintf("Service without authzs table: '%s'", service);
            } else {
                return json_sprintf("Authz not found: '%s' in service: '%s'", authz, service);
            }
        }
        return jn_authzs;
    }

    json_t *jn_authzs = json_object();
    json_t *jn_global_list = sdataauth2json(gobj_get_global_authz_table());
    json_object_set_new(jn_authzs, "global authzs", jn_global_list);

    json_t *jn_services = gobj_services();
    int idx; json_t *jn_service;
    json_array_foreach(jn_services, idx, jn_service) {
        const char *service_ = json_string_value(jn_service);
        hgobj gobj_service = gobj_find_service(service_, TRUE);
        if(gobj_service) {
            if(gclass_authz_desc(gobj_gclass(gobj_service))) {
                json_t *l = authzs_list(gobj_service, authz);
                json_object_set_new(jn_authzs, service_, l);
            }
        }
    }

    json_decref(jn_services);

    return jn_authzs;
}




                    /*---------------------------------*
                     *  SECTION: Stats
                     *---------------------------------*/




/***************************************************************************
 *
 ***************************************************************************/
PUBLIC json_int_t gobj_set_stat(hgobj gobj_, const char *path, json_int_t value)
{
    gobj_t * gobj = gobj_;
    if(!gobj) {
        return 0;
    }
    json_int_t old_value = kw_get_int(gobj, gobj->jn_stats, path, 0, 0);
    kw_set_dict_value(gobj, gobj->jn_stats, path, json_integer(value));

    return old_value;
}

/***************************************************************************
 *
 ***************************************************************************/
PUBLIC json_int_t gobj_incr_stat(hgobj gobj_, const char *path, json_int_t value)
{
    gobj_t * gobj = gobj_;
    if(!gobj) {
        return 0;
    }

    json_int_t cur_value = kw_get_int(gobj, gobj->jn_stats, path, 0, 0);
    cur_value += value;
    kw_set_dict_value(gobj, gobj->jn_stats, path, json_integer(cur_value));

    return cur_value;
}

/***************************************************************************
 *
 ***************************************************************************/
PUBLIC json_int_t gobj_decr_stat(hgobj gobj_, const char *path, json_int_t value)
{
    gobj_t * gobj = gobj_;
    if(!gobj) {
        return 0;
    }
    json_int_t cur_value = kw_get_int(gobj, gobj->jn_stats, path, 0, 0);
    cur_value -= value;
    kw_set_dict_value(gobj, gobj->jn_stats, path, json_integer(cur_value));

    return cur_value;
}

/***************************************************************************
 *
 ***************************************************************************/
PUBLIC json_int_t gobj_get_stat(hgobj gobj, const char *path)
{
    if(!gobj) {
        return 0;
    }
    return kw_get_int(gobj, ((gobj_t *)gobj)->jn_stats, path, 0, 0);
}

/***************************************************************************
 *  WARNING the json return is NOT YOURS!
 ***************************************************************************/
PUBLIC json_t *gobj_jn_stats(hgobj gobj)
{
    return ((gobj_t *)gobj)->jn_stats;
}




                    /*------------------------------*
                     *      Resource functions
                     *------------------------------*/




/***************************************************************************
 *
 ***************************************************************************/
PUBLIC json_t *gobj_create_resource(
    hgobj gobj_,
    const char *resource,
    json_t *kw,  // owned
    json_t *jn_options // owned
)
{
    gobj_t *gobj = gobj_;
    if(!gobj || gobj->obflag & obflag_destroyed) {
        gobj_log_error(gobj, LOG_OPT_TRACE_STACK,
            "gobj",         "%s", __FILE__,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_PARAMETER_ERROR,
            "msg",          "%s", "hgobj NULL or DESTROYED",
            NULL
        );
        KW_DECREF(kw)
        JSON_DECREF(jn_options)
        return 0;
    }
    if(!gobj->gclass->gmt->mt_create_resource) {
        gobj_log_error(gobj, LOG_OPT_TRACE_STACK,
            "gobj",         "%s", gobj_full_name(gobj),
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_INTERNAL_ERROR,
            "msg",          "%s", "mt_create_resource not defined",
            NULL
        );
        KW_DECREF(kw)
        JSON_DECREF(jn_options)
        return 0;
    }

    return gobj->gclass->gmt->mt_create_resource(gobj, resource, kw, jn_options);
}

/***************************************************************************
 *
 ***************************************************************************/
PUBLIC int gobj_save_resource(
    hgobj gobj_,
    const char *resource,
    json_t *record,     // WARNING NOT owned
    json_t *jn_options // owned
)
{
    gobj_t *gobj = gobj_;
    if(!gobj || gobj->obflag & obflag_destroyed) {
        gobj_log_error(gobj, 0,
            "gobj",         "%s", __FILE__,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_PARAMETER_ERROR,
            "msg",          "%s", "hgobj NULL or DESTROYED",
            NULL
        );
        JSON_DECREF(jn_options)
        return -1;
    }
    if(!record) {
        gobj_log_error(gobj, LOG_OPT_TRACE_STACK,
            "gobj",         "%s", gobj_full_name(gobj),
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_PARAMETER_ERROR,
            "msg",          "%s", "gobj_save_resource(): record NULL",
            NULL
        );
        JSON_DECREF(jn_options)
        return -1;
    }
    if(!gobj->gclass->gmt->mt_save_resource) {
        gobj_log_error(gobj, LOG_OPT_TRACE_STACK,
            "gobj",         "%s", gobj_full_name(gobj),
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_PARAMETER_ERROR,
            "msg",          "%s", "mt_save_resource not defined",
            NULL
        );
        JSON_DECREF(jn_options)
        return -1;
    }

    return gobj->gclass->gmt->mt_save_resource(gobj, resource, record, jn_options);
}

/***************************************************************************
 *
 ***************************************************************************/
PUBLIC int gobj_delete_resource(
    hgobj gobj_,
    const char *resource,
    json_t *record,  // owned
    json_t *jn_options // owned
)
{
    gobj_t *gobj = gobj_;
    if(!gobj || gobj->obflag & obflag_destroyed) {
        gobj_log_error(gobj, 0,
            "gobj",         "%s", __FILE__,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_PARAMETER_ERROR,
            "msg",          "%s", "hgobj NULL or DESTROYED",
            NULL
        );
        KW_DECREF(record)
        JSON_DECREF(jn_options)
        return -1;
    }
    if(!record) {
        gobj_log_error(gobj, LOG_OPT_TRACE_STACK,
            "gobj",         "%s", gobj_full_name(gobj),
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_PARAMETER_ERROR,
            "msg",          "%s", "gobj_delete_resource(): record NULL",
            NULL
        );
        JSON_DECREF(jn_options)
        return -1;
    }
    if(!gobj->gclass->gmt->mt_delete_resource) {
        gobj_log_error(gobj, LOG_OPT_TRACE_STACK,
            "gobj",         "%s", gobj_full_name(gobj),
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_PARAMETER_ERROR,
            "msg",          "%s", "mt_delete_resource not defined",
            NULL
        );
        KW_DECREF(record)
        JSON_DECREF(jn_options)
        return -1;
    }

    return gobj->gclass->gmt->mt_delete_resource(gobj, resource, record, jn_options);
}

/***************************************************************************
 *
 ***************************************************************************/
PUBLIC json_t *gobj_list_resource(
    hgobj gobj_,
    const char *resource,
    json_t *jn_filter,  // owned
    json_t *jn_options // owned
)
{
    gobj_t *gobj = gobj_;
    if(!gobj || gobj->obflag & obflag_destroyed) {
        gobj_log_error(gobj, 0,
            "gobj",         "%s", __FILE__,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_PARAMETER_ERROR,
            "msg",          "%s", "hgobj NULL or DESTROYED",
            NULL
        );
        KW_DECREF(jn_filter);
        JSON_DECREF(jn_options)
        return 0;
    }
    if(!gobj->gclass->gmt->mt_list_resource) {
        gobj_log_error(gobj, LOG_OPT_TRACE_STACK,
            "gobj",         "%s", gobj_full_name(gobj),
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_PARAMETER_ERROR,
            "msg",          "%s", "mt_list_resource not defined",
            NULL
        );
        KW_DECREF(jn_filter);
        JSON_DECREF(jn_options)
        return 0;
    }
    return gobj->gclass->gmt->mt_list_resource(gobj, resource, jn_filter, jn_options);
}

/***************************************************************************
 *
 ***************************************************************************/
PUBLIC json_t *gobj_get_resource( // WARNING return is NOT yours!
    hgobj gobj_,
    const char *resource,
    json_t *jn_filter,  // owned
    json_t *jn_options // owned
)
{
    gobj_t *gobj = gobj_;
    if(!gobj || gobj->obflag & obflag_destroyed) {
        gobj_log_error(gobj, LOG_OPT_TRACE_STACK,
            "gobj",         "%s", __FILE__,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_PARAMETER_ERROR,
            "msg",          "%s", "hgobj NULL or DESTROYED",
            NULL
        );
        KW_DECREF(jn_filter);
        JSON_DECREF(jn_options)
        return 0;
    }
    if(!gobj->gclass->gmt->mt_get_resource) {
        gobj_log_error(gobj, LOG_OPT_TRACE_STACK,
            "gobj",         "%s", gobj_full_name(gobj),
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_INTERNAL_ERROR,
            "msg",          "%s", "mt_get_resource not defined",
            NULL
        );
        KW_DECREF(jn_filter);
        JSON_DECREF(jn_options)
        return 0;
    }
    return gobj->gclass->gmt->mt_get_resource(gobj, resource, jn_filter, jn_options);
}




                    /*------------------------------*
                     *      Node functions
                     *------------------------------*/




#ifdef __linux__

/***************************************************************************
 *
 ***************************************************************************/
PUBLIC json_t *gobj_treedbs( // Return a list with treedb names
    hgobj gobj_,
    json_t *kw,
    hgobj src
)
{
    gobj_t *gobj = gobj_;
    if(!gobj || gobj->obflag & obflag_destroyed) {
        gobj_log_error(gobj, 0,
            "gobj",         "%s", __FILE__,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_PARAMETER_ERROR,
            "msg",          "%s", "hgobj NULL or DESTROYED",
            NULL
        );
        KW_DECREF(kw)
        return 0;
    }
    if(!gobj->gclass->gmt->mt_treedbs) {
        gobj_log_error(gobj, 0,
            "gobj",         "%s", gobj_full_name(gobj),
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_INTERNAL_ERROR,
            "msg",          "%s", "mt_treedbs not defined",
            NULL
        );
        KW_DECREF(kw)
        return 0;
    }
    return gobj->gclass->gmt->mt_treedbs(gobj, kw, src);
}

/***************************************************************************
 *
 ***************************************************************************/
PUBLIC json_t *gobj_treedb_topics(
    hgobj gobj_,
    const char *treedb_name,
    json_t *options, // "dict" return list of dicts, otherwise return list of strings
    hgobj src
)
{
    gobj_t *gobj = gobj_;
    if(!gobj || gobj->obflag & obflag_destroyed) {
        gobj_log_error(gobj, 0,
            "gobj",         "%s", __FILE__,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_PARAMETER_ERROR,
            "msg",          "%s", "hgobj NULL or DESTROYED",
            NULL
        );
        KW_DECREF(options);
        return 0;
    }
    if(!gobj->gclass->gmt->mt_treedb_topics) {
        gobj_log_error(gobj, 0,
            "gobj",         "%s", gobj_full_name(gobj),
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_INTERNAL_ERROR,
            "msg",          "%s", "mt_treedb_topics not defined",
            NULL
        );
        KW_DECREF(options);
        return 0;
    }
    return gobj->gclass->gmt->mt_treedb_topics(gobj, treedb_name, options, src);
}

/***************************************************************************
 *
 ***************************************************************************/
PUBLIC json_t *gobj_topic_desc(
    hgobj gobj_,
    const char *topic_name
)
{
    gobj_t *gobj = gobj_;
    if(!gobj || gobj->obflag & obflag_destroyed) {
        gobj_log_error(gobj, 0,
            "gobj",         "%s", __FILE__,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_PARAMETER_ERROR,
            "msg",          "%s", "hgobj NULL or DESTROYED",
            NULL
        );
        return 0;
    }
    if(!gobj->gclass->gmt->mt_topic_desc) {
        gobj_log_error(gobj, 0,
            "gobj",         "%s", gobj_full_name(gobj),
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_INTERNAL_ERROR,
            "msg",          "%s", "mt_topic_desc not defined",
            NULL
        );
        return 0;
    }
    return gobj->gclass->gmt->mt_topic_desc(gobj, topic_name);
}

/***************************************************************************
 *
 ***************************************************************************/
PUBLIC json_t *gobj_topic_links(
    hgobj gobj_,
    const char *treedb_name,
    const char *topic_name,
    json_t *kw,
    hgobj src
)
{
    gobj_t *gobj = gobj_;
    if(!gobj || gobj->obflag & obflag_destroyed) {
        gobj_log_error(gobj, 0,
            "gobj",         "%s", __FILE__,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_PARAMETER_ERROR,
            "msg",          "%s", "hgobj NULL or DESTROYED",
            NULL
        );
        KW_DECREF(kw)
        return 0;
    }
    if(!gobj->gclass->gmt->mt_topic_links) {
        gobj_log_error(gobj, 0,
            "gobj",         "%s", gobj_full_name(gobj),
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_INTERNAL_ERROR,
            "msg",          "%s", "mt_topic_links not defined",
            NULL
        );
        KW_DECREF(kw)
        return 0;
    }
    return gobj->gclass->gmt->mt_topic_links(gobj, treedb_name, topic_name, kw, src);
}

/***************************************************************************
 *
 ***************************************************************************/
PUBLIC json_t *gobj_topic_hooks(
    hgobj gobj_,
    const char *treedb_name,
    const char *topic_name,
    json_t *kw,
    hgobj src
)
{
    gobj_t *gobj = gobj_;
    if(!gobj || gobj->obflag & obflag_destroyed) {
        gobj_log_error(gobj, 0,
            "gobj",         "%s", __FILE__,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_PARAMETER_ERROR,
            "msg",          "%s", "hgobj NULL or DESTROYED",
            NULL
        );
        KW_DECREF(kw)
        return 0;
    }
    if(!gobj->gclass->gmt->mt_topic_hooks) {
        gobj_log_error(gobj, 0,
            "gobj",         "%s", gobj_full_name(gobj),
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_INTERNAL_ERROR,
            "msg",          "%s", "mt_topic_hooks not defined",
            NULL
        );
        KW_DECREF(kw)
        return 0;
    }
    return gobj->gclass->gmt->mt_topic_hooks(gobj, treedb_name, topic_name, kw, src);
}

/***************************************************************************
 *
 ***************************************************************************/
PUBLIC size_t gobj_topic_size(
    hgobj gobj_,
    const char *topic_name,
    const char *key
)
{
    gobj_t *gobj = gobj_;
    if(!gobj || gobj->obflag & obflag_destroyed) {
        gobj_log_error(gobj, 0,
            "gobj",         "%s", __FILE__,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_PARAMETER_ERROR,
            "msg",          "%s", "hgobj NULL or DESTROYED",
            NULL
        );
        return 0;
    }
    if(!gobj->gclass->gmt->mt_topic_size) {
        gobj_log_error(gobj, 0,
            "gobj",         "%s", gobj_full_name(gobj),
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_INTERNAL_ERROR,
            "msg",          "%s", "mt_topic_size not defined",
            NULL
        );
        return 0;
    }
    return gobj->gclass->gmt->mt_topic_size(gobj, topic_name, key);
}

/***************************************************************************
 *
 ***************************************************************************/
PUBLIC json_t *gobj_create_node( // Return is YOURS
    hgobj gobj_,
    const char *topic_name,
    json_t *kw,
    json_t *jn_options, // fkey,hook options
    hgobj src
)
{
    gobj_t *gobj = gobj_;
    if(!gobj || gobj->obflag & obflag_destroyed) {
        gobj_log_error(gobj, 0,
            "gobj",         "%s", __FILE__,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_PARAMETER_ERROR,
            "msg",          "%s", "hgobj NULL or DESTROYED",
            NULL
        );
        KW_DECREF(kw)
        JSON_DECREF(jn_options)
        return 0;
    }
    if(!gobj->gclass->gmt->mt_create_node) {
        gobj_log_error(gobj, 0,
            "gobj",         "%s", gobj_full_name(gobj),
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_INTERNAL_ERROR,
            "msg",          "%s", "mt_create_node not defined",
            NULL
        );
        KW_DECREF(kw)
        JSON_DECREF(jn_options)
        return 0;
    }
    return gobj->gclass->gmt->mt_create_node(gobj, topic_name, kw, jn_options, src);
}

/***************************************************************************
 *
 ***************************************************************************/
PUBLIC json_t *gobj_update_node( // Return is YOURS
    hgobj gobj_,
    const char *topic_name,
    json_t *kw,         // 'id' and pkey2s fields are used to find the node
    json_t *jn_options, // "create" "autolink" "volatil" fkey,hook options
    hgobj src
)
{
    gobj_t *gobj = gobj_;
    if(!gobj || gobj->obflag & obflag_destroyed) {
        gobj_log_error(gobj, 0,
            "gobj",         "%s", __FILE__,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_PARAMETER_ERROR,
            "msg",          "%s", "hgobj NULL or DESTROYED",
            NULL
        );
        KW_DECREF(kw)
        JSON_DECREF(jn_options)
        return 0;
    }
    if(!gobj->gclass->gmt->mt_update_node) {
        gobj_log_error(gobj, 0,
            "gobj",         "%s", gobj_full_name(gobj),
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_INTERNAL_ERROR,
            "msg",          "%s", "mt_update_node not defined",
            NULL
        );
        KW_DECREF(kw)
        JSON_DECREF(jn_options)
        return 0;
    }
    return gobj->gclass->gmt->mt_update_node(gobj, topic_name, kw, jn_options, src);
}

/***************************************************************************
 *
 ***************************************************************************/
PUBLIC int gobj_delete_node(
    hgobj gobj_,
    const char *topic_name,
    json_t *kw,         // 'id' and pkey2s fields are used to find the node
    json_t *jn_options, // "force"
    hgobj src
)
{
    gobj_t *gobj = gobj_;
    if(!gobj || gobj->obflag & obflag_destroyed) {
        gobj_log_error(gobj, 0,
            "gobj",         "%s", __FILE__,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_PARAMETER_ERROR,
            "msg",          "%s", "hgobj NULL or DESTROYED",
            NULL
        );
        KW_DECREF(kw)
        JSON_DECREF(jn_options)
        return -1;
    }
    if(!gobj->gclass->gmt->mt_delete_node) {
        gobj_log_error(gobj, 0,
            "gobj",         "%s", gobj_full_name(gobj),
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_INTERNAL_ERROR,
            "msg",          "%s", "mt_delete_node not defined",
            NULL
        );
        KW_DECREF(kw)
        JSON_DECREF(jn_options)
        return -1;
    }
    return gobj->gclass->gmt->mt_delete_node(gobj, topic_name, kw, jn_options, src);
}

/***************************************************************************
 *
 ***************************************************************************/
PUBLIC int gobj_link_nodes(
    hgobj gobj_,
    const char *hook,
    const char *parent_topic_name,
    json_t *parent_record,  // owned
    const char *child_topic_name,
    json_t *child_record,  // owned
    hgobj src
)
{
    gobj_t *gobj = gobj_;
    if(!gobj || gobj->obflag & obflag_destroyed) {
        gobj_log_error(gobj, 0,
            "gobj",         "%s", __FILE__,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_PARAMETER_ERROR,
            "msg",          "%s", "hgobj NULL or DESTROYED",
            NULL
        );
        JSON_DECREF(parent_record);
        JSON_DECREF(child_record);
        return -1;
    }
    if(!gobj->gclass->gmt->mt_link_nodes) {
        gobj_log_error(gobj, 0,
            "gobj",         "%s", gobj_full_name(gobj),
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_INTERNAL_ERROR,
            "msg",          "%s", "mt_link_nodes not defined",
            NULL
        );
        JSON_DECREF(parent_record);
        JSON_DECREF(child_record);
        return -1;
    }
    return gobj->gclass->gmt->mt_link_nodes(
        gobj,
        hook,
        parent_topic_name,
        parent_record,
        child_topic_name,
        child_record,
        src
    );
}

/***************************************************************************
 *
 ***************************************************************************/
PUBLIC int gobj_unlink_nodes(
    hgobj gobj_,
    const char *hook,
    const char *parent_topic_name,
    json_t *parent_record,  // owned
    const char *child_topic_name,
    json_t *child_record,  // owned
    hgobj src
)
{
    gobj_t *gobj = gobj_;
    if(!gobj || gobj->obflag & obflag_destroyed) {
        gobj_log_error(gobj, 0,
            "gobj",         "%s", __FILE__,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_PARAMETER_ERROR,
            "msg",          "%s", "hgobj NULL or DESTROYED",
            NULL
        );
        JSON_DECREF(parent_record);
        JSON_DECREF(child_record);
        return -1;
    }
    if(!gobj->gclass->gmt->mt_unlink_nodes) {
        gobj_log_error(gobj, 0,
            "gobj",         "%s", gobj_full_name(gobj),
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_INTERNAL_ERROR,
            "msg",          "%s", "mt_unlink_nodes not defined",
            NULL
        );
        JSON_DECREF(parent_record);
        JSON_DECREF(child_record);
        return -1;
    }
    return gobj->gclass->gmt->mt_unlink_nodes(
        gobj,
        hook,
        parent_topic_name,
        parent_record,
        child_topic_name,
        child_record,
        src
    );
}

/***************************************************************************
 *
 ***************************************************************************/
PUBLIC json_t *gobj_get_node(
    hgobj gobj_,
    const char *topic_name,
    json_t *kw,         // 'id' and pkey2s fields are used to find the node
    json_t *jn_options, // fkey,hook options
    hgobj src
)
{
    gobj_t *gobj = gobj_;
    if(!gobj || gobj->obflag & obflag_destroyed) {
        gobj_log_error(gobj, LOG_OPT_TRACE_STACK,
            "gobj",         "%s", __FILE__,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_PARAMETER_ERROR,
            "msg",          "%s", "hgobj NULL or DESTROYED",
            NULL
        );
        JSON_DECREF(kw)
        JSON_DECREF(jn_options)
        return 0;
    }
    if(!gobj->gclass->gmt->mt_get_node) {
        gobj_log_error(gobj, 0,
            "gobj",         "%s", gobj_full_name(gobj),
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_INTERNAL_ERROR,
            "msg",          "%s", "mt_get_node not defined",
            NULL
        );
        JSON_DECREF(kw)
        JSON_DECREF(jn_options)
        return 0;
    }
    return gobj->gclass->gmt->mt_get_node(gobj, topic_name, kw, jn_options, src);
}

/***************************************************************************
 *
 ***************************************************************************/
PUBLIC json_t *gobj_list_nodes(
    hgobj gobj_,
    const char *topic_name,
    json_t *jn_filter,
    json_t *jn_options, // "include-instances" fkey,hook options
    hgobj src
)
{
    gobj_t *gobj = gobj_;
    if(!gobj || gobj->obflag & obflag_destroyed) {
        gobj_log_error(gobj, 0,
            "gobj",         "%s", __FILE__,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_PARAMETER_ERROR,
            "msg",          "%s", "hgobj NULL or DESTROYED",
            NULL
        );
        JSON_DECREF(jn_filter);
        JSON_DECREF(jn_options)
        return 0;
    }
    if(!gobj->gclass->gmt->mt_list_nodes) {
        gobj_log_error(gobj, 0,
            "gobj",         "%s", gobj_full_name(gobj),
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_INTERNAL_ERROR,
            "msg",          "%s", "mt_list_nodes not defined",
            NULL
        );
        JSON_DECREF(jn_filter);
        JSON_DECREF(jn_options)
        return 0;
    }

    return gobj->gclass->gmt->mt_list_nodes(gobj, topic_name, jn_filter, jn_options, src);
}

/***************************************************************************
 *
 ***************************************************************************/
PUBLIC json_t *gobj_list_instances(
    hgobj gobj_,
    const char *topic_name,
    const char *pkey2_field,
    json_t *jn_filter,
    json_t *jn_options, // owned, fkey,hook options
    hgobj src
)
{
    gobj_t *gobj = gobj_;
    if(!gobj || gobj->obflag & obflag_destroyed) {
        gobj_log_error(gobj, 0,
            "gobj",         "%s", __FILE__,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_PARAMETER_ERROR,
            "msg",          "%s", "hgobj NULL or DESTROYED",
            NULL
        );
        JSON_DECREF(jn_filter);
        JSON_DECREF(jn_options)
        return 0;
    }
    if(!gobj->gclass->gmt->mt_list_instances) {
        gobj_log_error(gobj, 0,
            "gobj",         "%s", gobj_full_name(gobj),
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_INTERNAL_ERROR,
            "msg",          "%s", "mt_list_instances not defined",
            NULL
        );

        JSON_DECREF(jn_filter);
        JSON_DECREF(jn_options)
        return 0;
    }

    return gobj->gclass->gmt->mt_list_instances(
        gobj, topic_name, pkey2_field, jn_filter, jn_options, src
    );
}

/***************************************************************************
 *  Return a list of parent **references** pointed by the link (fkey)
 *  If no link return all links
 ***************************************************************************/
PUBLIC json_t *gobj_node_parents( // Return MUST be decref
    hgobj gobj_,
    const char *topic_name,
    json_t *kw,         // 'id' and pkey2s fields are used to find the node
    const char *link,
    json_t *jn_options, // fkey options
    hgobj src
)
{
    gobj_t *gobj = gobj_;
    if(!gobj || gobj->obflag & obflag_destroyed) {
        gobj_log_error(gobj, 0,
            "gobj",         "%s", __FILE__,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_PARAMETER_ERROR,
            "msg",          "%s", "hgobj NULL or DESTROYED",
            NULL
        );
        JSON_DECREF(jn_options)
        JSON_DECREF(kw)
        return 0;
    }
    if(!gobj->gclass->gmt->mt_node_parents) {
        gobj_log_error(gobj, 0,
            "gobj",         "%s", gobj_full_name(gobj),
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_INTERNAL_ERROR,
            "msg",          "%s", "mt_node_parents not defined",
            NULL
        );
        JSON_DECREF(jn_options)
        JSON_DECREF(kw)
        return 0;
    }
    return gobj->gclass->gmt->mt_node_parents(gobj, topic_name, kw, link, jn_options, src);
}

/***************************************************************************
 *  Return a list of child nodes of the hook
 *  If no hook return all hooks
 ***************************************************************************/
PUBLIC json_t *gobj_node_children( // Return MUST be decref
    hgobj gobj_,
    const char *topic_name,
    json_t *kw,         // 'id' and pkey2s fields are used to find the node
    const char *hook,
    json_t *jn_filter,  // filter to children
    json_t *jn_options, // fkey,hook options, "recursive"
    hgobj src
)
{
    gobj_t *gobj = gobj_;
    if(!gobj || gobj->obflag & obflag_destroyed) {
        gobj_log_error(gobj, 0,
            "gobj",         "%s", __FILE__,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_PARAMETER_ERROR,
            "msg",          "%s", "hgobj NULL or DESTROYED",
            NULL
        );
        JSON_DECREF(jn_options)
        JSON_DECREF(kw)
        return 0;
    }
    if(!gobj->gclass->gmt->mt_node_children) {
        gobj_log_error(gobj, 0,
            "gobj",         "%s", gobj_full_name(gobj),
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_INTERNAL_ERROR,
            "msg",          "%s", "mt_node_children not defined",
            NULL
        );
        JSON_DECREF(jn_options)
        JSON_DECREF(kw)
        return 0;
    }
    return gobj->gclass->gmt->mt_node_children(gobj, topic_name, kw, hook, jn_filter, jn_options, src);
}

/***************************************************************************
 *  Return a hierarchical tree of the self-link topic
 *  If "webix" option is TRUE return webix style (dict-list),
 *      else list of dict's
 *  __path__ field in all records (id`id`... style)
 *  If root node is not specified then the first with no parent is used
 ***************************************************************************/
PUBLIC json_t *gobj_topic_jtree( // Return MUST be decref
    hgobj gobj_,
    const char *topic_name,
    const char *hook,   // hook to build the hierarchical tree
    const char *rename_hook, // change the hook name in the tree response
    json_t *kw,         // 'id' and pkey2s fields are used to find the root node
    json_t *jn_filter,  // filter to match records
    json_t *jn_options, // fkey,hook options
    hgobj src
)
{
    gobj_t *gobj = gobj_;
    if(!gobj || gobj->obflag & obflag_destroyed) {
        gobj_log_error(gobj, 0,
            "gobj",         "%s", __FILE__,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_PARAMETER_ERROR,
            "msg",          "%s", "hgobj NULL or DESTROYED",
            NULL
        );
        JSON_DECREF(jn_filter);
        JSON_DECREF(jn_options)
        KW_DECREF(kw)
        return 0;
    }
    if(!gobj->gclass->gmt->mt_topic_jtree) {
        gobj_log_error(gobj, 0,
            "gobj",         "%s", gobj_full_name(gobj),
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_INTERNAL_ERROR,
            "msg",          "%s", "mt_topic_jtree not defined",
            NULL
        );
        JSON_DECREF(jn_filter);
        JSON_DECREF(jn_options)
        KW_DECREF(kw)
        return 0;
    }
    return gobj->gclass->gmt->mt_topic_jtree(
        gobj,
        topic_name,
        hook,   // hook to build the hierarchical tree
        rename_hook,// change the hook name in the tree response
        kw,         // 'id' and pkey2s fields are used to find the node
        jn_filter,  // filter to match records
        jn_options, // fkey,hook options
        src
    );
}

/***************************************************************************
 *  Return the full tree of a node (duplicated)
 ***************************************************************************/
PUBLIC json_t *gobj_node_tree( // Return MUST be decref
    hgobj gobj_,
    const char *topic_name,
    json_t *kw,         // 'id' and pkey2s fields are used to find the root node
    json_t *jn_options, // "with_metatada"
    hgobj src
)
{
    gobj_t *gobj = gobj_;
    if(!gobj || gobj->obflag & obflag_destroyed) {
        gobj_log_error(gobj, 0,
            "gobj",         "%s", __FILE__,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_PARAMETER_ERROR,
            "msg",          "%s", "hgobj NULL or DESTROYED",
            NULL
        );
        JSON_DECREF(jn_options)
        KW_DECREF(kw)
        return 0;
    }
    if(!gobj->gclass->gmt->mt_node_tree) {
        gobj_log_error(gobj, 0,
            "gobj",         "%s", gobj_full_name(gobj),
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_INTERNAL_ERROR,
            "msg",          "%s", "mt_node_tree not defined",
            NULL
        );
        JSON_DECREF(jn_options)
        KW_DECREF(kw)
        return 0;
    }
    return gobj->gclass->gmt->mt_node_tree(
        gobj,
        topic_name,
        kw,         // 'id' and pkey2s fields are used to find the node
        jn_options,
        src
    );
}

/***************************************************************************
 *
 ***************************************************************************/
PUBLIC int gobj_shoot_snap(
    hgobj gobj_,
    const char *tag,
    json_t *kw,
    hgobj src
)
{
    gobj_t *gobj = gobj_;
    if(!gobj || gobj->obflag & obflag_destroyed) {
        gobj_log_error(gobj, 0,
            "gobj",         "%s", __FILE__,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_PARAMETER_ERROR,
            "msg",          "%s", "hgobj NULL or DESTROYED",
            NULL
        );
        KW_DECREF(kw)
        return -1;
    }
    if(!gobj->gclass->gmt->mt_shoot_snap) {
        gobj_log_error(gobj, 0,
            "gobj",         "%s", gobj_full_name(gobj),
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_INTERNAL_ERROR,
            "msg",          "%s", "mt_shoot_snap not defined",
            NULL
        );
        KW_DECREF(kw)
        return -1;
    }
    return gobj->gclass->gmt->mt_shoot_snap(gobj, tag, kw, src);
}

/***************************************************************************
 *
 ***************************************************************************/
PUBLIC int gobj_activate_snap(
    hgobj gobj_,
    const char *tag,
    json_t *kw,
    hgobj src
)
{
    gobj_t *gobj = gobj_;
    if(!gobj || gobj->obflag & obflag_destroyed) {
        gobj_log_error(gobj, 0,
            "gobj",         "%s", __FILE__,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_PARAMETER_ERROR,
            "msg",          "%s", "hgobj NULL or DESTROYED",
            NULL
        );
        KW_DECREF(kw)
        return -1;
    }
    if(!gobj->gclass->gmt->mt_activate_snap) {
        gobj_log_error(gobj, 0,
            "gobj",         "%s", gobj_full_name(gobj),
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_INTERNAL_ERROR,
            "msg",          "%s", "mt_activate_snap not defined",
            NULL
        );
        KW_DECREF(kw)
        return -1;
    }
    return gobj->gclass->gmt->mt_activate_snap(gobj, tag, kw, src);
}

/***************************************************************************
 *
 ***************************************************************************/
PUBLIC json_t *gobj_list_snaps(
    hgobj gobj_,
    json_t *filter,
    hgobj src
)
{
    gobj_t *gobj = gobj_;
    if(!gobj || gobj->obflag & obflag_destroyed) {
        gobj_log_error(gobj, 0,
            "gobj",         "%s", __FILE__,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_PARAMETER_ERROR,
            "msg",          "%s", "hgobj NULL or DESTROYED",
            NULL
        );
        KW_DECREF(filter);
        return 0;
    }
    if(!gobj->gclass->gmt->mt_list_snaps) {
        gobj_log_error(gobj, 0,
            "gobj",         "%s", gobj_full_name(gobj),
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_INTERNAL_ERROR,
            "msg",          "%s", "mt_list_snaps not defined",
            NULL
        );
        KW_DECREF(filter);
        return 0;
    }
    return gobj->gclass->gmt->mt_list_snaps(gobj, filter, src);
}

#endif /* node functions only in __linux__ */




                    /*---------------------------------*
                     *      Trace functions
                     *---------------------------------*/




/***************************************************************************
 *  Debug print global trace levels in json
 ***************************************************************************/
PUBLIC json_t *gobj_repr_global_trace_levels(void)
{
    json_t *jn_register = json_array();

    json_t *jn_internals = json_object();

    json_t *jn_trace_levels = json_object();
    json_object_set_new(
        jn_internals,
        "trace_levels",
        jn_trace_levels
    );

    for(int i=0; i<16; i++) {
        if(!s_global_trace_level[i].name)
            break;
        json_object_set_new(
            jn_trace_levels,
            s_global_trace_level[i].name,
            json_string(s_global_trace_level[i].description)
        );
    }
    json_array_append_new(jn_register, jn_internals);

    return jn_register;
}

/***************************************************************************
 *  Debug print gclass trace levels in json
 ***************************************************************************/
PUBLIC json_t *gobj_repr_gclass_trace_levels(const char *gclass_name)
{
    json_t *jn_register = json_array();

    if(!empty_string(gclass_name)) {
        gclass_t *gclass = gclass_find_by_name(gclass_name);
        if(gclass) {
             json_t *jn_gclass = json_object();
             json_object_set_new(
                 jn_gclass,
                 "gclass",
                 json_string(gclass->gclass_name)
             );
            json_object_set_new(
                jn_gclass,
                "trace_levels",
                gobj_trace_level_list(gclass)
            );

            json_array_append_new(jn_register, jn_gclass);
        }
        return jn_register;
    }

    gclass_t *gclass = dl_first(&dl_gclass);
    while(gclass) {
        json_t *jn_gclass = json_object();
        json_object_set_new(
            jn_gclass,
            "gclass",
            json_string(gclass->gclass_name)
        );
        json_object_set_new(
            jn_gclass,
            "trace_levels",
            gobj_trace_level_list(gclass)
        );

        json_array_append_new(jn_register, jn_gclass);
        gclass = dl_next(gclass);
    }

    return jn_register;
}

/****************************************************************************
 *  Return list of trace levels
 *  Remember decref return
 ****************************************************************************/
PUBLIC json_t *gobj_trace_level_list(hgclass gclass_)
{
    gclass_t *gclass = gclass_;

    json_t *jn_dict = json_object();
    if(gclass->s_user_trace_level) {
        for(int i=0; i<16; i++) {
            if(!gclass->s_user_trace_level[i].name)
                break;
            json_object_set_new(
                jn_dict,
                gclass->s_user_trace_level[i].name,
                json_string(gclass->s_user_trace_level[i].description)
            );
        }
    }
    return jn_dict;
}

/***************************************************************************
 *
 ***************************************************************************/
PUBLIC json_t *gobj_get_global_trace_level(void)
{
    json_t *jn_list;
    jn_list = bit2level(
        s_global_trace_level,
        0,
        __global_trace_level__
    );
    return jn_list;
}

/***************************************************************************
 *
 ***************************************************************************/
PUBLIC json_t *gobj_get_gclass_trace_level(hgclass gclass_)
{
    gclass_t *gclass = gclass_;

    json_t *jn_list = bit2level(
        s_global_trace_level,
        gclass->s_user_trace_level,
        gclass->trace_level | __global_trace_level__
    );
    return jn_list;
}

/***************************************************************************
 *
 ***************************************************************************/
PUBLIC json_t *gobj_get_gclass_trace_no_level(hgclass gclass_)
{
    gclass_t *gclass = gclass_;

    json_t *jn_list = bit2level(
        s_global_trace_level,
        gclass->s_user_trace_level,
        gclass->no_trace_level
    );
    return jn_list;
}

/***************************************************************************
 *
 ***************************************************************************/
PUBLIC json_t *gobj_get_gobj_trace_level(hgobj gobj_)
{
    gobj_t * gobj = gobj_;
    json_t *jn_list;
    if(gobj) {
        jn_list = bit2level(
            s_global_trace_level,
            gobj->gclass->s_user_trace_level,
            gobj_trace_level(gobj)
        );
    } else {
        jn_list = bit2level(
            s_global_trace_level,
            0,
            __global_trace_level__
        );
    }
    return jn_list;
}

/***************************************************************************
 *
 ***************************************************************************/
PUBLIC json_t *gobj_get_gobj_trace_no_level(hgobj gobj_)
{
    gobj_t *gobj = gobj_;
    json_t *jn_list = bit2level(
        s_global_trace_level,
        gobj->gclass->s_user_trace_level,
        gobj_trace_no_level(gobj)
    );
    return jn_list;
}

/***************************************************************************
 *
 ***************************************************************************/
PUBLIC json_t *gobj_get_gclass_trace_level_list(hgclass gclass_)
{
    gclass_t *gclass = gclass_;

    json_t *jn_list = json_array();

    if(gclass) {
        json_t *jn_levels = gobj_get_gclass_trace_level(gclass);
        if(json_array_size(jn_levels)) {
            json_t *jn_gclass = json_object();
            json_object_set_new(
                jn_gclass,
                "gclass",
                json_string(gclass->gclass_name)
            );
            json_object_set_new(
                jn_gclass,
                "trace_levels",
                jn_levels
            );
            json_array_append_new(jn_list, jn_gclass);
        } else {
            JSON_DECREF(jn_levels);
        }
        return jn_list;
    }

    gclass_t *gclass_reg = dl_first(&dl_gclass);
    while(gclass_reg) {
        json_t *jn_levels = gobj_get_gclass_trace_level(gclass_reg);
        if(json_array_size(jn_levels)) {
            json_t *jn_gclass = json_object();
            json_object_set_new(
                jn_gclass,
                "gclass",
                json_string(gclass_reg->gclass_name)
            );
            json_object_set_new(
                jn_gclass,
                "trace_levels",
                jn_levels
            );
            json_array_append_new(jn_list, jn_gclass);
        } else {
            JSON_DECREF(jn_levels);
        }
        gclass_reg = dl_next(gclass_reg);
    }

    return jn_list;
}

/***************************************************************************
 *
 ***************************************************************************/
PUBLIC json_t *gobj_get_gclass_trace_no_level_list(hgclass gclass_)
{
    gclass_t *gclass = gclass_;

    json_t *jn_list = json_array();

    if(gclass) {
        json_t *jn_levels = gobj_get_gclass_trace_no_level(gclass);
        if(json_array_size(jn_levels)) {
            json_t *jn_gclass = json_object();
            json_object_set_new(
                jn_gclass,
                "gclass",
                json_string(gclass->gclass_name)
            );
            json_object_set_new(
                jn_gclass,
                "trace_levels",
                jn_levels
            );
            json_array_append_new(jn_list, jn_gclass);
        } else {
            JSON_DECREF(jn_levels);
        }
        return jn_list;
    }

    gclass_t *gclass_reg = dl_first(&dl_gclass);
    while(gclass_reg) {
        json_t *jn_levels = gobj_get_gclass_trace_no_level(gclass_reg);
        if(json_array_size(jn_levels)) {
            json_t *jn_gclass = json_object();
            json_object_set_new(
                jn_gclass,
                "gclass",
                json_string(gclass_reg->gclass_name)
            );
            json_object_set_new(
                jn_gclass,
                "no_trace_levels",
                jn_levels
            );
            json_array_append_new(jn_list, jn_gclass);
        } else {
            JSON_DECREF(jn_levels);
        }
        gclass_reg = dl_next(gclass_reg);
    }

    return jn_list;
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE int cb_set_xxx_children(hgobj child, void *user_data, void *user_data2)
{
    json_t *jn_list = user_data;
    json_t *jn_level = gobj_get_gobj_trace_level(child);
    if(json_array_size(jn_level)) {
        json_t *jn_o = json_object();

        json_object_set_new(
            jn_o,
            "gobj",
            json_string(gobj_short_name(child))
        );
        json_object_set_new(
            jn_o,
            "trace_levels",
            jn_level
        );
        json_array_append_new(jn_list, jn_o);
    } else {
        JSON_DECREF(jn_level);
    }
    return 0;
}
PUBLIC json_t *gobj_get_gobj_trace_level_tree(hgobj gobj)
{
    json_t *jn_list = json_array();
    cb_set_xxx_children(gobj, jn_list,0);
    gobj_walk_gobj_children_tree(gobj, WALK_TOP2BOTTOM, cb_set_xxx_children, jn_list, 0);
    return jn_list;
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE int cb_set_no_xxx_children(hgobj child, void *user_data, void *user_data2)
{
    json_t *jn_list = user_data;
    json_t *jn_level = gobj_get_gobj_trace_no_level(child);
    if(json_array_size(jn_level)) {
        json_t *jn_o = json_object();

        json_object_set_new(
            jn_o,
            "gobj",
            json_string(gobj_short_name(child))
        );
        json_object_set_new(
            jn_o,
            "no_trace_levels",
            jn_level
        );
        json_array_append_new(jn_list, jn_o);
    } else {
        JSON_DECREF(jn_level);
    }
    return 0;
}
PUBLIC json_t *gobj_get_gobj_trace_no_level_tree(hgobj gobj)
{
    json_t *jn_list = json_array();
    cb_set_no_xxx_children(gobj, jn_list, 0);
    gobj_walk_gobj_children_tree(gobj, WALK_TOP2BOTTOM, cb_set_no_xxx_children, jn_list, 0);
    return jn_list;
}

/****************************************************************************
 *  Return gobj trace level
 ****************************************************************************/
PUBLIC uint32_t gobj_global_trace_level(void)
{
    if(__deep_trace__) {
        return (uint32_t)-1;
    }
    return __global_trace_level__;
}


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
    if(gobj_is_shutdowning()) {
        return bitmask;
    }
    if(gobj && gobj->gclass) {
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
PRIVATE json_t *bit2level(
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

    if(!gclass) {
        gobj_log_error(0, LOG_OPT_TRACE_STACK,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_PARAMETER_ERROR,
            "msg",          "%s", "gclass NULL",
            NULL
        );
        return -1;
    }
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
PRIVATE int _set_gobj_trace_no_level(hgobj gobj_, const char *level, BOOL set)
{
    if(!gobj_) {
        return 0;
    }
    gobj_t * gobj = gobj_;
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
 *  0 legacy default,
 ****************************************************************************/
PUBLIC void gobj_set_trace_machine_format(int format)
{
    trace_machine_format = format;
}

/****************************************************************************
 *  Indent, return spaces multiple of depth level gobj.
 *  With this, we can see the trace messages indenting according
 *  to depth level.
 ****************************************************************************/
PUBLIC char *tab(char *bf, int bflen)
{
    int i;

    bf[0]=' ';
    for(i=1; i<__inside__*2 && i<bflen-2; i++)
        bf[i] = ' ';
    bf[i] = '\0';
    return bf;
}

/****************************************************************************
 *  Set or Reset NO gobj trace level
 ****************************************************************************/
PUBLIC int gobj_set_gobj_no_trace(hgobj gobj_, const char *level, BOOL set)
{
    if(!gobj_) {
        return 0;
    }
    gobj_t * gobj = gobj_;
    _set_gobj_trace_no_level(gobj, level, set);

    return 0;
}

/***************************************************************************
 *  Must trace?
 ***************************************************************************/
PUBLIC BOOL is_level_tracing(hgobj gobj_, uint32_t level)
{
    if(__deep_trace__) {
        return TRUE;
    }
    if(!gobj_) {
        return (__global_trace_level__ & level)? TRUE:FALSE;
    }
    gobj_t * gobj = gobj_;
    uint32_t trace = __global_trace_level__ & level ||
        gobj->trace_level & level ||
        gobj->gclass->trace_level & level;

    return trace?TRUE:FALSE;
}

/***************************************************************************
 *  Must no trace?
 ***************************************************************************/
PUBLIC BOOL is_level_not_tracing(hgobj gobj_, uint32_t level)
{
    if(__deep_trace__ > 1) {
        return FALSE;
    }
    if(!gobj_) {
        return (__global_trace_no_level__ & level)? TRUE:FALSE;
    }
    gobj_t * gobj = gobj_;
    uint32_t no_trace = gobj->no_trace_level & level ||
        gobj->gclass->no_trace_level & level;

    return no_trace?TRUE:FALSE;
}
