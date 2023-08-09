/****************************************************************************
 *          gobj.h
 *
 *          Objects G for Yuneta Simplified
 *
 *          Objects G with this api:
 *              - List of events supported, they can be of output or input or both.
 *              - List of states. Each state can support different list of events.
 *              - List of attributes (json types).
 *              - List of commands.
 *
 *          Events and states are managed by a simple finite state machine.
 *
 *          Communication between gobjs happen through events.
 *
 *          The events are messages with key/value model,
 *          being the key a string and the value a json object.
 *
 *          The output events a gobj can be subscribed by others gobjs.
 *
 *          GObj's are  organized internally as a hierarchical tree forming a Yuno.
 *          They collaborate to reach to goal of the yuno.
 *
 *          This software is battery included with internal:
 *              - log system
 *              - buffer system
 *              - json system based in jansson library (https://github.com/akheron/jansson)
 *
 *          Copyright (c) 1996-2023 Niyamaka.
 *          All Rights Reserved.
 *
 *
 *  Version 7.0.0
 *  -------------
 *      Initial version is a refactoring of Yuneta for Linux,
 *      simplified to work with ESP32 microcontrollers
 *
 *
 ****************************************************************************/
#pragma once

#include <stdbool.h>
#include <string.h>
#include <time.h>
#include <regex.h>
#include <inttypes.h>
#include <jansson.h>

#ifdef __cplusplus
extern "C"{
#endif

/***************************************************************
 *              Constants
 ***************************************************************/
#define YUNETA_VERSION "7.0.0"

#ifndef BOOL
# define BOOL   bool
#endif

#ifndef FALSE
# define FALSE  false
#endif

#ifndef TRUE
# define TRUE   true
#endif

#define PRIVATE static
#define PUBLIC

#ifndef ESP_PLATFORM
#define IRAM_ATTR
#endif

#ifndef MIN
# define MIN(a,b) (((a) < (b)) ? (a) : (b))
#endif

#ifndef MAX
# define MAX(a,b) (((a) > (b)) ? (a) : (b))
#endif

#define EXEC_AND_RESET(function, variable) if(variable) {function((void *)(variable)); (variable)=0;}

#ifndef ARRAY_SIZE
#define ARRAY_SIZE(a) (sizeof(a)/sizeof((a)[0]))
#endif

/*
 *  Macros to assign attr data to priv data.
 */
#define IF_EQ_SET_PRIV(__name__, __func__) \
    if(strcmp(path, #__name__)==0) {\
        priv->__name__ = __func__(gobj, #__name__);
#define ELIF_EQ_SET_PRIV(__name__, __func__) \
    } else if(strcmp(path, #__name__)==0) {\
        priv->__name__ = __func__(gobj, #__name__);
#define END_EQ_SET_PRIV() \
    }

#define SET_PRIV(__name__, __func__) \
    priv->__name__ = __func__(gobj, #__name__);

/***********************************************************************
 *  Macros of switch for strings, copied from:
 *  https://gist.github.com/HoX/abfe15c40f2d9daebc35

Example:

int main(int argc, char **argv) {
     SWITCHS(argv[1]) {
        CASES("foo")
        CASES("bar")
            printf("foo or bar (case sensitive)\n");
            break;

        ICASES("pi")
            printf("pi or Pi or pI or PI (case insensitive)\n");
            break;

        CASES_RE("^D.*",0)
            printf("Something that start with D (case sensitive)\n");
            break;

        CASES_RE("^E.*",REG_ICASE)
            printf("Something that start with E (case insensitive)\n");
            break;

        CASES("1")
            printf("1\n");

        CASES("2")
            printf("2\n");
            break;

        DEFAULTS
            printf("No match\n");
            break;
    } SWITCHS_END

    return 0;
}

 ***********************************************************************/
/** Begin a switch for the string x */
#define SWITCHS(x) \
    { regmatch_t pmatch[1]; (void)pmatch; const char *__sw = (x); bool __done = false; bool __cont = false; \
        regex_t __regex; regcomp(&__regex, ".*", 0); do {

/** Check if the string matches the cases argument (case sensitive) */
#define CASES(x)    } if ( __cont || !strcmp ( __sw, x ) ) \
                        { __done = true; __cont = true;

/** Check if the string matches the icases argument (case insensitive) */
#define ICASES(x)    } if ( __cont || !strcasecmp ( __sw, x ) ) { \
                        __done = true; __cont = true;

/** Check if the string matches the specified regular expression using regcomp(3) */
#define CASES_RE(x,flags) } regfree ( &__regex ); if ( __cont || ( \
                              0 == regcomp ( &__regex, x, flags ) && \
                              0 == regexec ( &__regex, __sw, ARRAY_SIZE(pmatch), pmatch, 0 ) ) ) { \
                                __done = true; __cont = true;

/** Default behaviour */
#define DEFAULTS    } if ( !__done || __cont ) {

/** Close the switchs */
#define SWITCHS_END } while ( 0 ); regfree(&__regex); }

static inline BOOL empty_string(const char *str)
{
    return (str && *str)?0:1;
}

static inline BOOL empty_json(const json_t *jn)
{
    if((json_is_array(jn) && json_array_size(jn)==0) ||
        (json_is_object(jn) && json_object_size(jn)==0) ||
        json_is_null(jn)) {
        return TRUE;
    } else {
        return FALSE;
    }
}

/***************************************************************
 *              DL_LIST Structures
 ***************************************************************/
#define DL_ITEM_FIELDS              \
    struct dl_list_s *__dl__;       \
    struct dl_item_s  *__next__;    \
    struct dl_item_s  *__prev__;    \
    size_t __id__;

typedef struct dl_item_s {
    DL_ITEM_FIELDS
} dl_item_t;

typedef struct dl_list_s {
    struct dl_item_s *head;
    struct dl_item_s *tail;
    size_t __itemsInContainer__;
    size_t __last_id__; // auto-incremental, always.
} dl_list_t;

typedef void (*fnfree)(void *);

/***************************************************************
 *              Log Structures
 ***************************************************************/
/*
 *  Msgid's
 */
// Error's MSGSETs
#define MSGSET_PARAMETER_ERROR          "Parameter Error"
#define MSGSET_MEMORY_ERROR             "No memory"
#define MSGSET_INTERNAL_ERROR           "Internal Error"
#define MSGSET_SYSTEM_ERROR             "OS Error"
#define MSGSET_JSON_ERROR               "Jansson Error"
#define MSGSET_LIBUV_ERROR              "Libuv Error"
#define MSGSET_OAUTH_ERROR              "OAuth Error"
#define MSGSET_TRANGER_ERROR            "Tranger Error"
#define MSGSET_TREEDB_ERROR             "TreeDb Error"
#define MSGSET_MSG2DB_ERROR             "Msg2Db Error"
#define MSGSET_QUEUE_ALARM              "Queue Alarm"
#define MSGSET_REGISTER_ERROR           "Register error"
#define MSGSET_OPERATIONAL_ERROR        "Operational error"
#define MSGSET_SMACHINE_ERROR           "SMachine error"
#define MSGSET_PROTOCOL_ERROR           "Protocol error"

// Info/Debug MSGSETs
#define MSGSET_STATISTICS               "Statistics"
#define MSGSET_STARTUP                  "Startup"
#define MSGSET_INFO                     "Info"
#define MSGSET_CONNECT_DISCONNECT       "Connect Disconnect"
#define MSGSET_DEBUG                    "Debug"
#define MSGSET_PROTOCOL                 "Protocol"
#define MSGSET_GBUFFERS                 "GBuffers"
#define MSGSET_YEV_LOOP                 "Yev_loop"
#define MSGSET_TRACK_MEM                "TrackMem"

/*
 *  Options for handlers
 */
typedef enum {
    LOG_HND_OPT_ALERT           = 0x0001,
    LOG_HND_OPT_CRITICAL        = 0x0002,
    LOG_HND_OPT_ERROR           = 0x0004,
    LOG_HND_OPT_WARNING         = 0x0008,
    LOG_HND_OPT_INFO            = 0x0010,
    LOG_HND_OPT_DEBUG           = 0x0020,
} log_handler_opt_t;

#define LOG_OPT_UP_ERROR \
(LOG_HND_OPT_ALERT|LOG_HND_OPT_CRITICAL|LOG_HND_OPT_ERROR)

#define LOG_OPT_UP_WARNING \
(LOG_HND_OPT_ALERT|LOG_HND_OPT_CRITICAL|LOG_HND_OPT_ERROR|LOG_HND_OPT_WARNING)

#define LOG_OPT_UP_INFO \
(LOG_HND_OPT_ALERT|LOG_HND_OPT_CRITICAL|LOG_HND_OPT_ERROR|LOG_HND_OPT_WARNING|LOG_HND_OPT_INFO)

#define LOG_OPT_ALL \
(LOG_HND_OPT_ALERT|LOG_HND_OPT_CRITICAL|LOG_HND_OPT_ERROR|LOG_HND_OPT_WARNING|LOG_HND_OPT_INFO|LOG_HND_OPT_DEBUG)

/*
 *  Values for log_*() functions
 */
typedef enum {
    LOG_NONE                = 0x0001,   /* Backward compatibility, only log */
    LOG_OPT_EXIT_ZERO       = 0x0002,   /* Exit(0) after log the error or critical */
    LOG_OPT_EXIT_NEGATIVE   = 0x0004,   /* Exit(-1) after log the error or critical */
    LOG_OPT_ABORT           = 0x0008,   /* abort() after log the error or critical */
    LOG_OPT_TRACE_STACK     = 0x0010,   /* Trace stack */
} log_opt_t;

typedef void (*loghandler_close_fn_t)(void *h);
typedef int  (*loghandler_write_fn_t)(void *h, int priority, const char *bf, size_t len);
typedef int  (*loghandler_fwrite_fn_t)(void *h, int priority, const char *format, ...)  JANSSON_ATTRS((format(printf, 3, 4)));
typedef void (*show_backtrace_fn_t)(loghandler_fwrite_fn_t fwrite_fn, void *h);

/***************************************************************
 *              SData
 ***************************************************************/
typedef enum {
    DTP_STRING = 1,
    DTP_BOOLEAN,
    DTP_INTEGER,
    DTP_REAL,
    DTP_LIST,
    DTP_DICT,
    DTP_JSON,
    DTP_POINTER
} data_type_t;

// TODO backward compatibility: migrate old ASN_* to new DTP_*
//#define ASN_OCTET_STR   DTP_STRING
//#define ASN_BOOLEAN     DTP_BOOLEAN
//#define ASN_INTEGER     DTP_INTEGER
//#define ASN_UNSIGNED    DTP_INTEGER
//#define ASN_COUNTER64   DTP_INTEGER
//#define ASN_DOUBLE      DTP_REAL
//#define ASN_INTEGER64   DTP_INTEGER
//#define ASN_UNSIGNED64  DTP_INTEGER
//#define ASN_JSON        DTP_JSON
#define DTP_SCHEMA      DTP_POINTER
//#define ASN_POINTER     DTP_POINTER

#define DTP_IS_INTEGER(type) ((type) == DTP_INTEGER)
#define DTP_IS_REAL(type) ((type) == DTP_REAL)
#define DTP_IS_BOOLEAN(type) ((type) == DTP_BOOLEAN)
#define DTP_IS_STRING(type) ((type) == DTP_STRING)

#define DTP_IS_NUMBER(type)     \
    (DTP_IS_INTEGER(type) ||    \
     DTP_IS_REAL(type) ||       \
     DTP_IS_BOOLEAN(type))

/*
 *  SData field flags.
 */
typedef enum {   // HACK strict ascendent value!, strings in sdata_flag_names[]
    SDF_NOTACCESS   = 0x00000001,   /* snmp/db exposed as not accessible */
    SDF_RD          = 0x00000002,   /* Field only readable by user */
    SDF_WR          = 0x00000004,   /* Field writable (an readable) by user */
    SDF_REQUIRED    = 0x00000008,   /* Required attribute. Must not be null */
    SDF_PERSIST     = 0x00000010,   /* (implicit SDF_WR) Field must be loaded/saved HACK 7-Feb-2020 */
    SDF_VOLATIL     = 0x00000020,   /* (implicit SDF_RD) Field must not be loaded/saved HACK 7-Feb-2020 */
    SDF_RESOURCE    = 0x00000040,   /* Mark as resource.  Use `schema` to specify the sdata schema */
    SDF_PKEY        = 0x00000080,   /* field used as primary key */
    SDF_FUTURE1     = 0x00000100,   /* old SDF_PURECHILD */
    SDF_FUTURE2     = 0x00000200,   /* old SDF_PARENTID */
    SDF_WILD_CMD    = 0x00000400,   /* Command with wild options (no checked) */
    SDF_STATS       = 0x00000800,   /* (implicit SDF_RD) Field with stats (METADATA)*/
    SDF_FKEY        = 0x00001000,   /* Foreign key (no pure child) */
    SDF_RSTATS      = 0x00002000,   /* Field with resettable stats, implicitly SDF_STATS */
    SDF_PSTATS      = 0x00004000,   /* Field with persistent stats, implicitly SDF_STATS */
    SDF_AUTHZ_R     = 0x00008000,   /* Need Attribute '__read_attribute__' authorization */
    SDF_AUTHZ_W     = 0x00010000,   /* Need Attribute '__write_attribute__' authorization */
    SDF_AUTHZ_X     = 0x00020000,   /* Need Command '__execute_command__' authorization */
    SDF_AUTHZ_P     = 0x00040000,   /* authorization constraint parameter */
    SDF_AUTHZ_S     = 0x00080000,   /* Need Stats '__read_stats__' authorization */
    SDF_AUTHZ_RS    = 0x00100000,   /* Need Stats '__reset_stats__' authorization */
} sdata_flag_t;

#define SDF_PUBLIC_ATTR (SDF_WR|SDF_RD|SDF_STATS|SDF_PERSIST|SDF_VOLATIL|SDF_RSTATS|SDF_PSTATS)
#define ATTR_WRITABLE (SDF_WR|SDF_PERSIST)
#define ATTR_READABLE (SDF_RD|SDF_WR|SDF_PERSIST|SDF_STATS|SDF_VOLATIL|SDF_RSTATS|SDF_PSTATS)


#define SDATA_END()                                     \
{                                                       \
    .type=0,                                            \
    .name=0,                                            \
    .alias=0,                                           \
    .flag=0,                                            \
    .default_value=0,                                   \
    .header=0,                                          \
    .fillspace=0,                                       \
    .description=0,                                     \
    .json_fn=0,                                         \
    .schema=0,                                          \
}

/*
 *  GObj Attribute
 */
/*-ATTR-type------------name----------------flag------------------------default---------description----------*/
#define SDATA(type_, name_, flag_, default_value_, description_) \
{                                                       \
    .type=(type_),                                      \
    .name=(name_),                                      \
    .alias=0,                                           \
    .flag=(flag_),                                      \
    .default_value=(default_value_),                    \
    .header=0,                                          \
    .fillspace=0,                                       \
    .description=(description_),                        \
    .json_fn=0,                                         \
    .schema=0,                                          \
}



/*-CMD---type-----------name----------------alias---------------items-----------json_fn---------description---------- */
#define SDATACM(type_, name_, alias_, items_, json_fn_, description_) \
{                                                       \
    .type=(type_),                                      \
    .name=(name_),                                      \
    .alias=(alias_),                                    \
    .flag=0,                                            \
    .default_value=0,                                   \
    .header=0,                                          \
    .fillspace=0,                                       \
    .description=(description_),                        \
    .json_fn=(json_fn_),                                \
    .schema=(items_),                                   \
}

/*-PM----type-----------name------------flag------------default-----description---------- */
#define SDATAPM(type_, name_, flag_, default_value_, description_) \
{                                                       \
    .type=(type_),                                      \
    .name=(name_),                                      \
    .alias=0,                                           \
    .flag=(flag_),                                      \
    .default_value=(default_value_),                    \
    .header=0,                                          \
    .fillspace=0,                                       \
    .description=(description_),                        \
    .json_fn=0,                                         \
    .schema=0,                                          \
}

/*-AUTHZ--type----------name------------flag----alias---items---------------description--*/
#define SDATAAUTHZ(type_, name_, flag_, alias_, items_, description_) \
{                                                       \
    .type=(type_),                                      \
    .name=(name_),                                      \
    .alias=(alias_),                                    \
    .flag=(flag_),                                      \
    .default_value=0,                                   \
    .header=0,                                          \
    .fillspace=0,                                       \
    .description=(description_),                        \
    .json_fn=0,                                         \
    .schema=(items_),                                   \
}

/*
 *  Generic json function
 */
typedef json_t *(*json_function_t)(
    void *param1,
    const char *something,
    json_t *kw, // Owned
    void *param2
);

typedef struct sdata_desc_s {
    data_type_t type;
    const char *name;
    const char **alias;
    sdata_flag_t flag;
    const char *default_value;
    const char *header;
    uint8_t fillspace;
    const char *description;
    json_function_t json_fn;
    struct sdata_desc_s *schema;
} sdata_desc_t;

typedef enum { // HACK strict ascendent value!, strings in event_flag_names[]
    EVF_NO_WARN_SUBS    = 0x0001,   // Don't warn of "Publish event WITHOUT subscribers"
    EVF_OUTPUT_EVENT    = 0x0002,   // Output Event
    EVF_SYSTEM_EVENT    = 0x0004,   // System Event
    EVF_PUBLIC_EVENT    = 0x0008,   // You should document a public event, it's the API
} event_flag_t;

/***************************************************************
 *              GClass/GObj Structures
 ***************************************************************/
typedef const char *gclass_name_t;      /**< unique pointer that exposes gclass names */
typedef const char *gobj_state_t;       /**< unique pointer that exposes states */
typedef const char *gobj_event_t;       /**< unique pointer that exposes events */
typedef const char *gobj_lmethod_t;     /**< unique pointer that exposes local methods */

typedef void *hgclass;
typedef void *hgobj;

typedef int (*gobj_action_fn)(
    hgobj gobj,
    gobj_event_t event,
    json_t *kw,
    hgobj src
);

typedef struct {
    gobj_event_t event;
    gobj_action_fn action;
    gobj_state_t next_state;
} ev_action_t;

typedef struct states_s {
    gobj_state_t state_name;
    ev_action_t *state;
} states_t;

typedef struct event_type_s {
    gobj_event_t event;
    event_flag_t event_flag;
} event_type_t;

/*-------------------------------------------------------*
 *                  GClass Methods
 *-------------------------------------------------------*/
typedef int (*mt_start_fn)(hgobj gobj);
typedef int (*mt_stop_fn)(hgobj gobj);
typedef int (*mt_play_fn)(hgobj gobj);
typedef int (*mt_pause_fn)(hgobj gobj);
typedef void (*mt_create_fn)(hgobj gobj);
typedef void (*mt_create2_fn)(
    hgobj gobj,
    json_t *kw // owned
);
typedef void (*mt_destroy_fn)(hgobj gobj);
typedef void (*mt_writing_fn)(
    hgobj gobj,
    const char *name
);
typedef int (*mt_reading_fn)(
    hgobj gobj,
    const char *name
);
typedef int (*mt_subscription_added_fn)( // Negative return -> subscription deleted ???
    hgobj gobj,
    json_t * subs // NOT owned
);
typedef int (*mt_subscription_deleted_fn)(
    hgobj gobj,
    json_t * subs // NOT owned
);

/*
 *  These three functions return -1 (broke), 0 continue without publish, 1 continue and publish
 */
typedef BOOL  (*mt_publish_event_fn) ( // Return -1,0,1
    hgobj gobj,
    gobj_event_t event,
    json_t *kw  // NOT owned! you can modify this publishing kw
);
typedef BOOL  (*mt_publication_pre_filter_fn) ( // Return -1,0,1
    hgobj gobj,
    json_t *subs,
    gobj_event_t event,
    json_t *kw  // NOT owned! you can modify this publishing kw
);
typedef BOOL  (*mt_publication_filter_fn) ( // Return -1,0,1
    hgobj gobj,
    gobj_event_t event,
    json_t *kw,  // NOT owned! you can modify this publishing kw
    hgobj subscriber
);
typedef int (*mt_child_added_fn)(hgobj gobj, hgobj child);
typedef int (*mt_child_removed_fn)(hgobj gobj, hgobj child);

typedef json_t *(*mt_create_resource_fn)(
    hgobj gobj,
    const char *resource,
    json_t *kw,
    json_t *jn_options
);
typedef void *(*mt_list_resource_fn)(
    hgobj gobj,
    const char *resource,
    json_t *jn_filter,
    json_t *jn_options
);
typedef int (*mt_save_resource_fn)(
    hgobj gobj,
    const char *resource,
    json_t *record,
    json_t *jn_options
);
typedef int (*mt_delete_resource_fn)(
    hgobj gobj,
    const char *resource,
    json_t *record,
    json_t *jn_options
);
typedef json_t *(*mt_get_resource_fn)(
    hgobj gobj,
    const char *resource,
    json_t *jn_filter,
    json_t *jn_options
);

typedef json_t *(*mt_create_node_fn)(
    hgobj gobj,
    const char *topic_name,
    json_t *kw,
    json_t *jn_options,
    hgobj src
);
typedef json_t *(*mt_update_node_fn)(
    hgobj gobj,
    const char *topic_name,
    json_t *kw,
    json_t *jn_options,
    hgobj src
);
typedef int (*mt_delete_node_fn)(
    hgobj gobj,
    const char *topic_name,
    json_t *kw,
    json_t *jn_options,
    hgobj src
);
typedef int (*mt_link_nodes_fn)(
    hgobj gobj,
    const char *hook,
    const char *parent_topic_name,
    json_t *parent,
    const char *child_topic_name,
    json_t *child,
    hgobj src
);
typedef int (*mt_unlink_nodes_fn)(
    hgobj gobj,
    const char *hook,
    const char *parent_topic_name,
    json_t *parent,
    const char *child_topic_name,
    json_t *child,
    hgobj src
);
typedef json_t *(*mt_get_node_fn)(
    hgobj gobj,
    const char *topic_name,
    json_t *kw,
    json_t *options,
    hgobj src
);

typedef json_t *(*mt_topic_jtree_fn)(
    hgobj gobj,
    const char *topic_name,
    const char *hook,
    const char *rename_hook,
    json_t *kw,
    json_t *jn_filter,
    json_t *jn_options,
    hgobj src
);

typedef json_t *(*mt_node_tree_fn)(
    hgobj gobj,
    const char *topic_name,
    json_t *kw,
    json_t *jn_options,
    hgobj src
);

typedef json_t *(*mt_list_nodes_fn)(
    hgobj gobj,
    const char *topic_name,
    json_t *jn_filter,
    json_t *options, hgobj src
);
typedef int (*mt_shoot_snap_fn)(
    hgobj gobj,
    const char *tag,
    json_t *kw,
    hgobj src
);
typedef int (*mt_activate_snap_fn)(
    hgobj gobj,
    const char *tag,
    json_t *kw,
    hgobj src
);
typedef json_t *(*mt_list_snaps_fn)(
    hgobj gobj,
    json_t *filter,
    hgobj src
);

typedef json_t *(*mt_treedbs_fn)(
    hgobj gobj,
    json_t *kw,
    hgobj src
);
typedef json_t *(*mt_treedb_topics_fn)(
    hgobj gobj,
    const char *treedb_name,
    json_t *options,
    hgobj src
);
typedef json_t *(*mt_topic_desc_fn)(
    hgobj gobj,
    const char *topic_name
);
typedef json_t *(*mt_topic_links_fn)(
    hgobj gobj,
    const char *treedb_name,
    const char *topic_name,
    json_t *kw,
    hgobj src
);
typedef json_t *(*mt_topic_hooks_fn)(
    hgobj gobj,
    const char *treedb_name,
    const char *topic_name,
    json_t *kw, hgobj src
);
typedef json_t *(*mt_node_parents_fn)(
    hgobj gobj,
    const char *topic_name,
    json_t *kw,
    const char *link,
    json_t *options,
    hgobj src
);
typedef json_t *(*mt_node_childs_fn)(
    hgobj gobj,
    const char *topic_name,
    json_t *kw,
    const char *hook,
    json_t *filter,
    json_t *options,
    hgobj src
);
typedef json_t *(*mt_list_instances_fn)(
    hgobj gobj,
    const char *topic_name,
    const char *pkey2_field,
    json_t *jn_filter,
    json_t *jn_options,
    hgobj src
);
typedef size_t (*mt_topic_size_fn)(
    hgobj gobj,
    const char *topic_name
);

typedef json_t *(*local_method_fn)(
    hgobj gobj,
    gobj_lmethod_t lmethod,
    json_t *kw,
    hgobj src
);
typedef json_t *(*mt_authenticate_fn)(
    hgobj gobj,
    json_t *kw,
    hgobj src
);
typedef json_t *(*mt_list_childs_fn)(
    hgobj gobj,
    const char *child_gclass,
    const char **attributes
);
typedef int (*mt_stats_updated_fn)(
    hgobj gobj,
    hgobj stats_gobj,
    const char *attr_name,
    int type, // my ASN1 types
    json_t * old_v,
    json_t * new_v
);
typedef int (*mt_disable_fn)(hgobj gobj);
typedef int (*mt_enable_fn)(hgobj gobj);
typedef int (*mt_trace_on_fn)(
    hgobj gobj,
    const char *level,
    json_t *kw
);
typedef int (*mt_trace_off_fn)(
    hgobj gobj,
    const char *level,
    json_t *kw
);

typedef void (*mt_gobj_created_fn)(
    hgobj gobj,
    hgobj gobj_created
);
typedef int (*mt_state_changed_fn)(
    hgobj gobj,
    gobj_event_t event,
    json_t *kw
);

typedef BOOL (*authz_checker_fn)(
    hgobj gobj,
    const char *authz,
    json_t *kw,
    hgobj src
);
typedef json_t *(*authenticate_parser_fn)(
    hgobj gobj,
    json_t *kw,
    hgobj src
);

typedef int (*future_method_fn)(
    hgobj gobj,
    void *data,
    int c,
    char x
);

/*
 *  mt_command:
 *  First find command in gclass.cmds and execute it if found.
 *  If it doesn't exist then execute mt_command if it exists.
 *  Must return a json dict with "result", and "data" keys.
 *  Use the build_webix() helper to build the json.
 *  On error "result" will be a negative value, and "data" a string with the error description
 *  On successful "data" must return an any type of json data.
 *
 *  mt_stats or mt_command return 0 if there is a asynchronous response.
 *  For examples these will occur when the command is redirect to a event.
 */
typedef struct { // GClass methods (Yuneta framework methods)
    mt_create_fn mt_create;
    mt_create2_fn mt_create2;
    mt_destroy_fn mt_destroy;
    mt_start_fn mt_start;   // inside mt_start the gobj is already running
    mt_stop_fn mt_stop;    // inside mt_stop the gobj is already stopped
    mt_play_fn mt_play;
    mt_pause_fn mt_pause;
    mt_writing_fn mt_writing; // someone has written in the `name` attr. When path is 0, the gobj has been created with default values
    mt_reading_fn mt_reading; // opportunity to change the attr value before return attr
    mt_subscription_added_fn mt_subscription_added;      // refcount: 1 on the first subscription of this event. HACK return negative the subscription is rejected (unsubscribe).
    mt_subscription_deleted_fn mt_subscription_deleted;    // refcount: 1 on the last subscription of this event.
    mt_child_added_fn mt_child_added;     // called when the child is built, just after mt_create call.
    mt_child_removed_fn mt_child_removed;   // called when the child is almost live
    json_function_t mt_stats;           // must return a webix json object 0 if asynchronous response, like all below. HACK match the stats's prefix only.
    json_function_t mt_command_parser;  // User command parser. Preference over gclass.command_table. Return webix. HACK must implement AUTHZ
    gobj_action_fn mt_inject_event;     // Won't use the static built-in gclass machine? process yourself your events.
    mt_create_resource_fn mt_create_resource;
    mt_list_resource_fn mt_list_resource; // Can return an iter or a json, depends of gclass
    mt_save_resource_fn mt_save_resource;
    mt_delete_resource_fn mt_delete_resource;
    future_method_fn mt_future21; // OLD mt_add_child_resource_link;
    future_method_fn mt_future22; // OLD mt_delete_child_resource_link;
    mt_get_resource_fn mt_get_resource;
    mt_state_changed_fn mt_state_changed; // If this method is defined then the EV_STATE_CHANGED will not published
    mt_authenticate_fn mt_authenticate; // Return webix
    mt_list_childs_fn mt_list_childs;
    mt_stats_updated_fn mt_stats_updated;       // Return 0 if own the stats, or -1 if not.
    mt_disable_fn mt_disable;
    mt_enable_fn mt_enable;
    mt_trace_on_fn mt_trace_on;                 // Return webix
    mt_trace_off_fn mt_trace_off;               // Return webix
    mt_gobj_created_fn mt_gobj_created;         // ONLY for __yuno__.
    future_method_fn mt_future33;
    future_method_fn mt_future34;
    mt_publish_event_fn mt_publish_event;  // Return -1 (broke), 0 continue without publish, 1 continue and publish
    mt_publication_pre_filter_fn mt_publication_pre_filter; // Return -1,0,1
    mt_publication_filter_fn mt_publication_filter; // Return -1,0,1
    authz_checker_fn mt_authz_checker;
    future_method_fn mt_future39;
    mt_create_node_fn mt_create_node;
    mt_update_node_fn mt_update_node;
    mt_delete_node_fn mt_delete_node;
    mt_link_nodes_fn mt_link_nodes;
    future_method_fn mt_future44;
    mt_unlink_nodes_fn mt_unlink_nodes;
    mt_topic_jtree_fn mt_topic_jtree;
    mt_get_node_fn mt_get_node;
    mt_list_nodes_fn mt_list_nodes;
    mt_shoot_snap_fn mt_shoot_snap;
    mt_activate_snap_fn mt_activate_snap;
    mt_list_snaps_fn mt_list_snaps;
    mt_treedbs_fn mt_treedbs;
    mt_treedb_topics_fn mt_treedb_topics;
    mt_topic_desc_fn mt_topic_desc;
    mt_topic_links_fn mt_topic_links;
    mt_topic_hooks_fn mt_topic_hooks;
    mt_node_parents_fn mt_node_parents;
    mt_node_childs_fn mt_node_childs;
    mt_list_instances_fn mt_list_instances;
    mt_node_tree_fn mt_node_tree;
    mt_topic_size_fn mt_topic_size;
    future_method_fn mt_future62;
    future_method_fn mt_future63;
    future_method_fn mt_future64;
} GMETHODS;

typedef json_t *(*internal_method_fn)(hgobj gobj, const char *lmethod, json_t *kw, hgobj src);

typedef struct { // Internal methods
    const char *lname;
    const internal_method_fn lm;
    const char *authz;
} LMETHOD;

/*
 *  trace_level_t is used to describe trace levels
 */
typedef struct {
    const char *name;
    const char *description;
} trace_level_t;

typedef struct {
    const char *name;
    const char *description;
} authz_level_t;

typedef enum { // HACK strict ascendant value!, strings in s_gcflag (TODO)
    gcflag_manual_start             = 0x0001,   // gobj_start_tree() don't start. TODO do same with stop
    gcflag_no_check_output_events   = 0x0002,   // When publishing don't check events in output_event_list.
    gcflag_ignore_unknown_attrs     = 0x0004,   // When creating a gobj, ignore not existing attrs
    gcflag_required_start_to_play   = 0x0008,   // Don't to play if no start done.
    gcflag_singleton                = 0x0010,   // Can only have one instance
} gclass_flag_t;

typedef enum { // HACK strict ascendant value!, strings in s_gobj_flag (TODO)
    gobj_flag_yuno              = 0x0001,
    gobj_flag_default_service   = 0x0002,
    gobj_flag_service           = 0x0004,   // Interface (events, attrs, commands, stats) available to external access
    gobj_flag_volatil           = 0x0008,
    gobj_flag_pure_child        = 0x0010,   // Pure child send events directly to parent, others publish them
    gobj_flag_autostart         = 0x0020,   // Set by gobj_create_tree0 too
    gobj_flag_autoplay          = 0x0040,   // Set by gobj_create_tree0 too
} gobj_flag_t;


/***************************************************************
 *      Macros to define and declare the
 *          - GClases names
 *          - States
 *          - Events and some System Events
 *
 ***************************************************************/
// Macro GClasses
#define GOBJ_DEFINE_GCLASS(id) gclass_name_t id = #id
#define GOBJ_DECLARE_GCLASS(id) extern gclass_name_t id

// Macro States
#define GOBJ_DEFINE_STATE(id) gobj_state_t id = #id
#define GOBJ_DECLARE_STATE(id) extern gobj_state_t id

// Macro Events
#define GOBJ_DEFINE_EVENT(id) gobj_event_t id = #id
#define GOBJ_DECLARE_EVENT(id) extern gobj_event_t id

// Timer Events, defined here to easy filtering in trace
GOBJ_DECLARE_EVENT(EV_TIMEOUT);
GOBJ_DECLARE_EVENT(EV_TIMEOUT_PERIODIC);

// System Events
GOBJ_DECLARE_EVENT(EV_STATE_CHANGED);

// Channel Messages: Input Events (Requests)
GOBJ_DECLARE_EVENT(EV_SEND_MESSAGE);
GOBJ_DECLARE_EVENT(EV_DROP);

// Channel Messages: Output Events (Publications)
GOBJ_DECLARE_EVENT(EV_ON_OPEN);
GOBJ_DECLARE_EVENT(EV_ON_CLOSE);
GOBJ_DECLARE_EVENT(EV_ON_MESSAGE);      // with GBuffer
GOBJ_DECLARE_EVENT(EV_ON_IEV_MESSAGE);  // with IEvent
GOBJ_DECLARE_EVENT(EV_ON_ID);
GOBJ_DECLARE_EVENT(EV_ON_ID_NAK);

// Tube Messages
GOBJ_DECLARE_EVENT(EV_CONNECT);
GOBJ_DECLARE_EVENT(EV_CONNECTED);
GOBJ_DECLARE_EVENT(EV_DISCONNECT);
GOBJ_DECLARE_EVENT(EV_DISCONNECTED);
GOBJ_DECLARE_EVENT(EV_RX_DATA);
GOBJ_DECLARE_EVENT(EV_TX_DATA);
GOBJ_DECLARE_EVENT(EV_TX_READY);
GOBJ_DECLARE_EVENT(EV_STOPPED);

// Frequent states
GOBJ_DECLARE_STATE(ST_DISCONNECTED);
GOBJ_DECLARE_STATE(ST_WAIT_CONNECTED);
GOBJ_DECLARE_STATE(ST_CONNECTED);
GOBJ_DECLARE_STATE(ST_WAIT_TXED);
GOBJ_DECLARE_STATE(ST_WAIT_DISCONNECTED);
GOBJ_DECLARE_STATE(ST_WAIT_STOPPED);
GOBJ_DECLARE_STATE(ST_STOPPED);
GOBJ_DECLARE_STATE(ST_SESSION);
GOBJ_DECLARE_STATE(ST_IDLE);
GOBJ_DECLARE_STATE(ST_WAIT_RESPONSE);

/***************************************************************
 *              Prototypes
 ***************************************************************/
/*---------------------------------*
 *      Start up functions
 *---------------------------------*/
PUBLIC int gobj_start_up(                   /* Initialize the yuno */
    int argc,
    char *argv[],
    json_t *jn_global_settings,
    int (*startup_persistent_attrs)(void),
    void (*end_persistent_attrs)(void),
    int (*load_persistent_attrs)(
        hgobj gobj,
        json_t *keys  // owned
    ),
    int (*save_persistent_attrs)(
        hgobj gobj,
        json_t *keys  // owned
    ),
    int (*remove_persistent_attrs)(
        hgobj gobj,
        json_t *keys  // owned
    ),
    json_t * (*list_persistent_attrs)(
        hgobj gobj,
        json_t *keys  // owned
    ),
    json_function_t global_command_parser,
    json_function_t global_stats_parser,
    authz_checker_fn global_authz_checker,
    authenticate_parser_fn global_authenticate_parser,

    size_t max_block,                       /* largest memory block */
    size_t max_system_memory                /* maximum system memory */
);
PUBLIC void gobj_shutdown(void);            /* Order for shutdown the yuno */
PUBLIC BOOL gobj_is_shutdowning(void);      /* Check if yuno is shutdowning */
PUBLIC void gobj_end(void);                 /* De-initialize the yuno */

/*---------------------------------*
 *      GClass functions
 *---------------------------------*/
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
);

PUBLIC int gclass_add_state(
    hgclass gclass,
    gobj_state_t state_name
);
PUBLIC int gclass_add_ev_action(
    hgclass gclass,
    gobj_state_t state_name,
    gobj_event_t event,
    gobj_action_fn action,
    gobj_state_t next_state
);
PUBLIC int gclass_add_state_with_action_list(
    hgclass gclass,
    gobj_state_t state_name,
    ev_action_t *ev_action_list
);
PUBLIC hgclass gclass_find_by_name(gclass_name_t gclass_name);
PUBLIC gobj_event_t gclass_find_public_event(const char *event, BOOL verbose);
PUBLIC void gclass_unregister(hgclass hgclass);

/*---------------------------------*
 *      Create functions
 *---------------------------------*/
PUBLIC hgobj gobj_create_gobj(
    const char *gobj_name,
    gclass_name_t gclass_name,
    json_t *kw, // owned
    hgobj parent,
    gobj_flag_t gobj_flag
);
#define gobj_create_yuno(name, gclass, kw) \
    gobj_create_gobj(name, gclass, kw, NULL, gobj_flag_yuno)

#define gobj_create_service(name, gclass, kw, parent) \
    gobj_create_gobj(name, gclass, kw, parent, gobj_flag_service)

/*
 *  Default service has autostart but no autoplay: it will be played by play method of yuno
 */
#define gobj_create_default_service(name, gclass, kw, parent) \
    gobj_create_gobj(name, gclass, kw, parent, gobj_flag_default_service|gobj_flag_autostart)

#define gobj_create_volatil(name, gclass, kw, parent) \
    gobj_create_gobj(name, gclass, kw, parent, gobj_flag_volatil)

#define gobj_create_pure_child(name, gclass, kw, parent) \
    gobj_create_gobj(name, gclass, kw, parent, gobj_flag_pure_child)

#define gobj_create(name, gclass, kw, parent) \
    gobj_create_gobj(name, gclass, kw, parent, 0)

#define gobj_create2(name, gclass, kw, parent, gobj_flag) \
    gobj_create_gobj(name, gclass, kw, parent, gobj_flag)

PUBLIC void gobj_destroy(hgobj gobj);

PUBLIC void gobj_destroy_childs(hgobj gobj);


/*---------------------------------*
 *  Attribute functions
 *---------------------------------*/
#define INCR_ATTR_INTEGER(__name__) \
    gobj_write_integer_attr(gobj, #__name__, gobj_read_integer_attr(gobj, #__name__) + 1);

#define RESET_ATTR_INTEGER(__name__) \
    gobj_write_integer_attr(gobj, #__name__, 0);

/*
 *  gobj_load_persistent_attrs() is automatically executed on creation of gobj.
 *  The persistent attributes saved have the more prevalence over other json configuration
 *  ONLY unique gobjs can load/save persistent attributes.
 *  gobj_save_persistent_attrs() must be manually executed.
 *
 *  Persistent attrs now can be save/remove individually
 *  attrs can be a string, a list of keys, or a dict with the keys to save/delete
 *  if attrs is empty list/save/remove all attrs
 */
PUBLIC int gobj_save_persistent_attrs(hgobj gobj, json_t *jn_attrs); // str, list or dict
PUBLIC int gobj_remove_persistent_attrs(hgobj gobj, json_t *jn_attrs); // str, list or dict
PUBLIC json_t * gobj_list_persistent_attrs(hgobj gobj, json_t *jn_attrs); // str, list or dict

/*
 *  Attribute functions WITHOUT bottom inheritance
 */

// Return the data description of the attribute `attr`
// If `attr` is null returns full attr's table
PUBLIC const sdata_desc_t *gclass_attr_desc(hgclass gclass, const char *name, BOOL verbose);
PUBLIC const sdata_desc_t *gobj_attr_desc(hgobj gobj, const char *attr, BOOL verbose);
PUBLIC json_t *gobj_hsdata(hgobj gobj); // Return is NOT YOURS

PUBLIC BOOL gclass_has_attr(hgclass gclass, const char* name);
PUBLIC BOOL gobj_has_attr(hgobj hgobj, const char *name);
PUBLIC BOOL gobj_is_readable_attr(hgobj gobj, const char *name); // True is attr is SDF_RD (public readable)
PUBLIC BOOL gobj_is_writable_attr(hgobj gobj, const char *name);
PUBLIC int gobj_reset_volatil_attrs(hgobj gobj);
PUBLIC int gobj_reset_rstats_attrs(hgobj gobj);

PUBLIC json_t *gobj_read_attr( // Return is NOT yours!
    hgobj gobj,
    const char *path, // If it has ` then segments are gobj and leaf is the attribute (+bottom) TODO
    hgobj src
);

PUBLIC json_t *gobj_read_attrs( // Return is yours!
    hgobj gobj,
    sdata_flag_t include_flag,
    hgobj src
);

PUBLIC json_t *gobj_read_user_data(
    hgobj gobj,
    const char *name // If empty name return all user record
);

PUBLIC int gobj_write_attr(
    hgobj gobj,
    const char *path, // If it has ` then segments are gobj and leaf is the attribute (+bottom) TODO
    json_t *value,  // owned
    hgobj src
);
PUBLIC int gobj_write_attrs(
    hgobj gobj,
    json_t *kw,  // owned
    sdata_flag_t flag,
    hgobj src
);
PUBLIC int gobj_write_user_data(
    hgobj gobj,
    const char *name,
    json_t *value // owned
);

/*
 *  Attribute functions WITH bottom inheritance
 */

PUBLIC const char *gobj_read_str_attr(hgobj gobj, const char *name);
PUBLIC BOOL gobj_read_bool_attr(hgobj gobj, const char *name);
PUBLIC json_int_t gobj_read_integer_attr(hgobj gobj, const char *name);
PUBLIC double gobj_read_real_attr(hgobj gobj, const char *name);
PUBLIC json_t *gobj_read_json_attr(hgobj gobj, const char *name); // WARNING return its NOT YOURS
PUBLIC void *gobj_read_pointer_attr(hgobj gobj, const char *name);

/*
 *  WARNING these gobj_write_*() functions DO NOT check the type of attr,
 *  it's your responsibility to use the correct json type
 */
PUBLIC int gobj_write_str_attr(hgobj gobj, const char *name, const char *value);
PUBLIC int gobj_write_bool_attr(hgobj gobj, const char *name, BOOL value);
PUBLIC int gobj_write_integer_attr(hgobj gobj, const char *name, json_int_t value);
PUBLIC int gobj_write_real_attr(hgobj gobj, const char *name, double value);
PUBLIC int gobj_write_json_attr(hgobj gobj, const char *name, json_t *value);
PUBLIC int gobj_write_new_json_attr(hgobj gobj, const char *name, json_t *value);
PUBLIC int gobj_write_pointer_attr(hgobj gobj, const char *name, void *value);

/*--------------------------------------------*
 *  Operational functions
 *--------------------------------------------*/
PUBLIC int gobj_start(hgobj gobj);
PUBLIC int gobj_start_childs(hgobj gobj);   // only direct childs
PUBLIC int gobj_start_tree(hgobj gobj);     // childs with gcflag_manual_start flag are not started.
PUBLIC int gobj_stop(hgobj gobj);
PUBLIC int gobj_stop_childs(hgobj gobj);    // only direct childs
PUBLIC int gobj_stop_tree(hgobj gobj);      // all tree of childs
PUBLIC int gobj_play(hgobj gobj);
PUBLIC int gobj_pause(hgobj gobj);
PUBLIC int gobj_disable(hgobj gobj); // if not exists mt_disable() then gobj_stop_tree()
PUBLIC int gobj_enable(hgobj gobj); // if not exists mt_enable() then gobj_start_tree()

PUBLIC void gobj_set_yuno_must_die(void);
PUBLIC BOOL gobj_get_yuno_must_die(void);
PUBLIC void gobj_set_exit_code(int exit_code);
PUBLIC int gobj_get_exit_code(void);

PUBLIC int gobj_autostart_services(void);
PUBLIC int gobj_autoplay_services(void);
PUBLIC int gobj_stop_services(void);

/*
 *  mt_command must return a webix json or 0 on asynchronous responses.
 */
PUBLIC json_t *gobj_command( // With AUTHZ
    hgobj gobj,
    const char *command,
    json_t *kw,
    hgobj src
);

/*
 *  Return a dict with attrs marked with SDF_STATS and stats_metadata
 */
PUBLIC json_t * gobj_stats( // Call mt_stats() or build_stats()
    hgobj gobj,
    const char* stats,
    json_t* kw,
    hgobj src
);

PUBLIC hgobj gobj_set_bottom_gobj(hgobj gobj, hgobj bottom_gobj); // inherit attributes
PUBLIC hgobj gobj_last_bottom_gobj(hgobj gobj); // inherit attributes
PUBLIC hgobj gobj_bottom_gobj(hgobj gobj);

PUBLIC hgobj gobj_default_service(void);
PUBLIC hgobj gobj_find_service(const char *service, BOOL verbose);
PUBLIC hgobj gobj_find_gobj(const char *gobj_path); // find gobj by path (full path)

PUBLIC hgobj gobj_first_child(hgobj gobj);
PUBLIC hgobj gobj_last_child(hgobj gobj);
PUBLIC hgobj gobj_next_child(hgobj child);
PUBLIC hgobj gobj_prev_child(hgobj child);
PUBLIC hgobj gobj_child_by_name(hgobj gobj, const char *name);
PUBLIC size_t gobj_child_size(hgobj gobj);
PUBLIC hgobj gobj_search_path(hgobj gobj, const char *path);

/*
    jn_filter: a dict with names of attributes with the value to mach,
        or a system name like:

        '__inherited_gclass_name__'
        '__gclass_name__'
        '__gobj_name__'
        '__prefix_gobj_name__'
        '__state__'
        '__disabled__'

    gobj_find_child() returns the first matched child.

 */
PUBLIC hgobj gobj_find_child(
    hgobj gobj,
    json_t *jn_filter // owned
);

typedef enum {
    /*
     * One must be selected
     */
    WALK_TOP2BOTTOM = 0x0001,
    WALK_BOTTOM2TOP = 0x0002,
    WALK_BYLEVEL    = 0x0004,

    /*
     * One must be selected when WALK_BYLEVEL is selected.
     */
    WALK_FIRST2LAST = 0x0010,
    WALK_LAST2FIRST = 0x0020,
} walk_type_t;

typedef int (*cb_walking_t)(
    hgobj gobj,
    void *user_data
);

PUBLIC int gobj_walk_gobj_childs(
    hgobj gobj,
    walk_type_t walk_type,
    cb_walking_t cb_walking,
    void *user_data
);
PUBLIC int gobj_walk_gobj_childs_tree(
    hgobj gobj,
    walk_type_t walk_type,
    cb_walking_t cb_walking,
    void *user_data
);

/*---------------------------------*
 *      Info functions
 *---------------------------------*/
PUBLIC hgobj gobj_yuno(void); // Return yuno, the grandfather (Only one yuno per process, single thread)
PUBLIC const char * gobj_name(hgobj gobj);
PUBLIC gclass_name_t gobj_gclass_name(hgobj gobj);
PUBLIC const char * gobj_full_name(hgobj gobj);
PUBLIC const char * gobj_short_name(hgobj gobj);
PUBLIC json_t * gobj_global_variables(void);
PUBLIC void * gobj_priv_data(hgobj gobj);
PUBLIC hgobj gobj_parent(hgobj gobj);
PUBLIC BOOL gobj_is_destroying(hgobj gobj);
PUBLIC BOOL gobj_is_running(hgobj gobj);
PUBLIC BOOL gobj_is_playing(hgobj gobj);
PUBLIC BOOL gobj_is_service(hgobj gobj);
PUBLIC BOOL gobj_is_disabled(hgobj gobj);
PUBLIC BOOL gobj_is_volatil(hgobj gobj);
PUBLIC BOOL gobj_is_pure_child(hgobj gobj);

PUBLIC BOOL gobj_typeof_gclass(hgobj gobj, const char *gclass_name);            /* strict same gclass */
PUBLIC BOOL gobj_typeof_inherited_gclass(hgobj gobj, const char *gclass_name);  /* check inherited (bottom) gclass */
PUBLIC size_t get_max_system_memory(void);
PUBLIC size_t get_cur_system_memory(void);

static inline const char *gobj_yuno_name(void) {return gobj_name(gobj_yuno());}
static inline const char *gobj_yuno_role(void) {return gobj_read_str_attr(gobj_yuno(), "yuno_role");}
static inline const char *gobj_yuno_realm_name(void) {return gobj_read_str_attr(gobj_yuno(), "realm_name");}
static inline const char *gobj_yuno_id(void) {return gobj_read_str_attr(gobj_yuno(), "yuno_id");}

PUBLIC const char *get_host_name(void);
PUBLIC const char *get_user_name(void); // Who started yuno

PUBLIC json_t *attr2json(hgobj gobj);       // Return a list with gobj's public attributes.

/*--------------------------------------------*
 *          Events and States
 *--------------------------------------------*/
PUBLIC int gobj_send_event(
    hgobj dst,
    gobj_event_t event,
    json_t *kw,  // owned
    hgobj src
);
PUBLIC BOOL gobj_change_state(
    hgobj gobj,
    gobj_state_t state_name
);
PUBLIC gobj_state_t gobj_current_state(hgobj gobj);
PUBLIC BOOL gobj_has_input_event(hgobj gobj, gobj_event_t event);
PUBLIC event_type_t *gobj_event_type(hgobj gobj_, gobj_event_t event, event_flag_t event_flag);

/*--------------------------------------------*
 *          Publication/Subscriptions
 *--------------------------------------------*/
/*
Sub-dictionaries to use in gobj_subscribe_event() kw
====================================================
HACK Only these keys are let, remain of keywords will be ignored!

    __config__   Dict. Parameters to custom gobj_subscribe_event() behaviour.
            System Parameters:
            - "__hard_subscription__":  bool
                True for permanent subscription.
                This subscription cannot be remove,
                (Well, with force you can)

            - "__own_event__": bool
                If __own_event__ defined and gobj_send_event inside of gobj_publish_event
                returns -1 then don't continue publishing

    __global__  Dict. Base kw to be merged with the publishing kw
                (base kw will be updated by publishing kw).
                It's the common data received in each publication. Constants, global variables!

    __local__   Dict. Dictionary or list with Keys (path) to be deleted from kw before to publish.

    __service__ Str. ??? in use?
*/

PUBLIC json_t *gobj_subscribe_event( // return not yours
    hgobj publisher,
    gobj_event_t event,
    json_t *kw, // kw (__config__, __global__, __local__)
    hgobj subscriber
);
PUBLIC int gobj_unsubscribe_event(
    hgobj publisher,
    gobj_event_t event,
    json_t *kw, // kw (__config__, __global__, __local__)
    hgobj subscriber
);
PUBLIC int gobj_unsubscribe_list(
    json_t *dl_subs, // owned
    BOOL force  // delete hard_subscription subs too
);

PUBLIC json_t *gobj_find_subscriptions( // Return is YOURS
    hgobj publisher,
    gobj_event_t event,
    json_t *kw,             // kw (__config__, __global__, __local__)
    hgobj subscriber
);

PUBLIC json_t *gobj_find_subscribings( // Return is YOURS
    hgobj subscriber,
    gobj_event_t event,
    json_t *kw,             // kw (__config__, __global__, __local__)
    hgobj publisher
);

/*
 *  In general the meaning of returns are:
 *     -1  broke (could be interpreted like 'own the event and don't publish to others)
 *      0  continue without publish
 *      1  publish and continue
 *
 *  The process of publishing is:
 *  1) Own mt_publish_event method: If publisher gclass has mt_publish_event() call it.
 *      if return <= 0 return (all publishing process done by gclass)
 *      if return >0 continue with publishing process of gobj.c
 *
 *  2) LOOP over subscriptions:
 *      1) Pre-filter: If publisher gclass has mt_publication_pre_filter call it (before KW filling)
 *          kw NOT owned! you can modify the publishing kw
 *          Return:
 *              -1  broke
 *              0  continue without publish
 *              1  continue to publish
 *
 *      2) Check if System event: don't send if subscriber has not it
 *
 *      3) KW filling
 *          - Remove local keys (defined in __local__)
 *          - Add global keys (defined in __global__)
 *
 *      4) Publish (gobj_send_event to subscriber)
 *          If __own_event__ defined and return -1 don't continue publishing
 *
 */
PUBLIC int gobj_publish_event( // Return the number of sent events (>=0), or -1 if someone owned the event (if __own_event__???)
    hgobj publisher,
    gobj_event_t event,
    json_t *kw  // this kw extends kw_request.
);

/*---------------------------------*
 *      Trace functions
 *---------------------------------*/
/*
 *  Global trace levels
 *  16 higher bits for global use.
 *  16 lower bits for user use.
 */
enum { /* String table in s_global_trace_level */
    TRACE_MACHINE           = 0x00010000,
    TRACE_CREATE_DELETE     = 0x00020000,
    TRACE_CREATE_DELETE2    = 0x00040000,
    TRACE_SUBSCRIPTIONS     = 0x00080000,
    TRACE_START_STOP        = 0x00100000,
    TRACE_MONITOR           = 0x00200000,
    TRACE_EVENT_MONITOR     = 0x00400000,
    TRACE_UV                = 0x00800000,
    TRACE_EV_KW             = 0x01000000,
    TRACE_AUTHZS            = 0x02000000,
    TRACE_STATES            = 0x04000000,
    TRACE_PERIODIC_TIMER    = 0x08000000,
    TRACE_GBUFFERS          = 0x10000000,
    TRACE_TIMER             = 0x20000000,
};
#define TRACE_USER_LEVEL    0x0000FFFF
#define TRACE_GLOBAL_LEVEL  0xFFFF0000

/*
 *  Global trace level names:
 *
 *      "machine"
 *      "create_delete"
 *      "create_delete2"
 *      "subscriptions"
 *      "subscriptions2"
 *      "start_stop"
 *      "monitor"
 *      "event_monitor"
 *      "libuv"
 *      "ev_kw"
 *      "authzs"
 *      "states"
 *      "periodic_timer"
 *      "gbuffers"
 *      "timer"
 */

PUBLIC uint32_t gobj_trace_level(hgobj gobj);
PUBLIC uint32_t gobj_trace_no_level(hgobj gobj);
PUBLIC BOOL is_level_tracing(hgobj gobj, uint32_t level);
PUBLIC BOOL is_level_not_tracing(hgobj gobj, uint32_t level);

/*
 *  Set trace levels and no-set trace levels, in gclass and gobj
 *      - if gobj is null then the trace level is global.
 *      - if level is NULL, all global and gclass levels are set/reset.
 *      - if level is "", all gclass levels are set/reset.
 *      - if gobj is not null then call mt_trace_on/mt_trace_off
 */
PUBLIC int gobj_set_gobj_trace(hgobj gobj, const char* level, BOOL set, json_t* kw);
PUBLIC int gobj_set_gclass_trace(hgclass gclass, const char *level, BOOL set);
PUBLIC int gobj_set_deep_tracing(int level); /* 1 all but considering __gobj_no_trace_level__, > 1 all */
PUBLIC int gobj_get_deep_tracing(void);
PUBLIC int gobj_set_global_trace(const char *level, BOOL set); // If level is empty, set all global traces
PUBLIC int gobj_set_global_no_trace(const char *level, BOOL set); // If level is empty, set all global traces

/*
 *  With these trace filter functions you can trace the levels of a gclass
 *  but only those gobjs that match the attributes being filtering.
 *  WARNING: by now only string type attributes can be filtering.
 */
PUBLIC int gobj_load_trace_filter(hgclass gclass, json_t *jn_trace_filter); // owned
PUBLIC int gobj_add_trace_filter(hgclass gclass, const char *attr, const char *value);
PUBLIC int gobj_remove_trace_filter(hgclass gclass, const char *attr, const char *value);  // If attr is empty then remove all filters, if value is empty then remove all values of attr
PUBLIC json_t *gobj_get_trace_filter(hgclass gclass); // Return is not YOURS

/*
 *  Set no-trace-level
 */
PUBLIC int gobj_set_gclass_no_trace(hgclass gclass, const char *level, BOOL set);
PUBLIC int gobj_set_gobj_no_trace(hgobj gobj, const char *level, BOOL set);
PUBLIC void trace_machine(const char *fmt, ...) JANSSON_ATTRS((format(printf, 1, 2)));
PUBLIC char *tab(char *bf, int bflen);

/*---------------------------------*
 *      Log functions
 *---------------------------------*/
PUBLIC void glog_init(void);
PUBLIC void glog_end(void);
/*
 *  log handler "stdout" is included
 */
PUBLIC int gobj_log_register_handler(
    const char *handler_type,
    loghandler_close_fn_t close_fn,
    loghandler_write_fn_t write_fn,
    loghandler_fwrite_fn_t fwrite_fn
);

PUBLIC int gobj_log_add_handler(
    const char *handler_name,
    const char *handler_type,
    log_handler_opt_t handler_options,
    void *h
);
PUBLIC int gobj_log_del_handler(const char *handler_name); // delete all handlers if handle_name is empty
PUBLIC json_t *gobj_log_list_handlers(void);

PUBLIC void gobj_log_alert(hgobj gobj, log_opt_t opt, ...); // WARNING don't put format printf here
PUBLIC void gobj_log_critical(hgobj gobj, log_opt_t opt, ...);
PUBLIC void gobj_log_error(hgobj gobj, log_opt_t opt, ...);
PUBLIC void gobj_log_warning(hgobj gobj, log_opt_t opt, ...);
PUBLIC void gobj_log_info(hgobj gobj, log_opt_t opt, ...);
PUBLIC void gobj_log_debug(hgobj gobj, log_opt_t opt, ...);
PUBLIC const char *gobj_get_log_priority_name(int priority);
PUBLIC void gobj_log_clear_counters(void);
PUBLIC const char *gobj_log_last_message(void);
PUBLIC void gobj_log_set_last_message(const char *msg, ...) JANSSON_ATTRS((format(printf, 1, 2)));
PUBLIC void set_show_backtrace_fn(show_backtrace_fn_t show_backtrace_fn);

PUBLIC void trace_vjson(
    hgobj gobj,
    json_t *jn_data,    // now owned
    const char *msgset,
    const char *fmt,
    va_list ap
);
PUBLIC void gobj_trace_msg(
    hgobj gobj,
    const char *fmt,
    ...
) JANSSON_ATTRS((format(printf, 2, 3)));

PUBLIC void gobj_trace_json(
    hgobj gobj,
    json_t *jn, // now owned
    const char *fmt,
    ...
) JANSSON_ATTRS((format(printf, 3, 4)));

PUBLIC void gobj_trace_buffer(
    hgobj gobj,
    const char *bf,
    size_t len,
    const char *fmt,
    ...
) JANSSON_ATTRS((format(printf, 4, 5)));

PUBLIC void gobj_trace_dump(
    hgobj gobj,
    const char *bf,
    size_t len,
    const char *fmt,
    ...
) JANSSON_ATTRS((format(printf, 4, 5)));

PUBLIC void print_error(
    const char *fmt,
    ...
) JANSSON_ATTRS((format(printf, 1, 2)));

/*---------------------------------*
 *      Memory functions
 *---------------------------------*/
typedef void* (*sys_malloc_fn_t)(size_t sz);
typedef void* (*sys_realloc_fn_t)(void * ptr, size_t sz);
typedef void* (*sys_calloc_fn_t)(size_t n, size_t size);
typedef void (*sys_free_fn_t)(void * ptr);

PUBLIC int gobj_set_allocators(
    sys_malloc_fn_t malloc_func,
    sys_realloc_fn_t realloc_func,
    sys_calloc_fn_t calloc_func,
    sys_free_fn_t free_func
);
PUBLIC int gobj_get_allocators(
    sys_malloc_fn_t *malloc_func,
    sys_realloc_fn_t *realloc_func,
    sys_calloc_fn_t *calloc_func,
    sys_free_fn_t *free_func
);

PUBLIC sys_malloc_fn_t gobj_malloc_func(void);
PUBLIC sys_realloc_fn_t gobj_realloc_func(void);
PUBLIC sys_calloc_fn_t gobj_calloc_func(void);
PUBLIC sys_free_fn_t gobj_free_func(void);
PUBLIC char *gobj_strndup(const char *str, size_t size);
PUBLIC char *gobj_strdup(const char *string);
PUBLIC size_t gobj_get_maximum_block(void);
PUBLIC void set_memory_check_list(unsigned long *memory_check_list);

#define GBMEM_MALLOC(size) (gobj_malloc_func())(size)

#define GBMEM_FREE(ptr)             \
    if((ptr)) {                     \
        (gobj_free_func())((void *)(ptr));  \
        (ptr) = 0;                  \
    }

#define GBMEM_STRDUP gobj_strdup

#define GBMEM_REALLOC(ptr, size) (gobj_realloc_func())((ptr), (size))

/*---------------------------------*
 *      Double link  functions
 *---------------------------------*/
PUBLIC int dl_init(dl_list_t *dl);
PUBLIC void *dl_first(dl_list_t *dl);
PUBLIC void *dl_last(dl_list_t *dl);
PUBLIC void *dl_next(void *curr);
PUBLIC void *dl_prev(void *curr);
PUBLIC int dl_add(dl_list_t *dl, void *item);
PUBLIC int dl_delete(dl_list_t *dl, void * curr_, void (*fnfree)(void *));
PUBLIC void dl_flush(dl_list_t *dl, void (*fnfree)(void *));
PUBLIC size_t dl_size(dl_list_t *dl);


#ifdef __cplusplus
}
#endif
