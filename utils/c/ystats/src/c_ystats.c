/***********************************************************************
 *          C_YSTATS.C
 *          YStats GClass.
 *
 *          Yuneta Statistics
 *
 *          Copyright (c) 2016 Niyamaka.
 *          All Rights Reserved.
 ***********************************************************************/
#include <string.h>
#include <stdio.h>
#include "c_ystats.h"

/***************************************************************************
 *              Constants
 ***************************************************************************/

/***************************************************************************
 *              Structures
 ***************************************************************************/

/***************************************************************************
 *              Prototypes
 ***************************************************************************/
PRIVATE int do_authenticate_task(hgobj gobj);
PRIVATE int cmd_connect(hgobj gobj);
PRIVATE int poll_attr_data(hgobj gobj);
PRIVATE int poll_command_data(hgobj gobj);
PRIVATE int poll_stats_data(hgobj gobj);

/***************************************************************************
 *          Data: config, public data, private data
 ***************************************************************************/

/*---------------------------------------------*
 *      Attributes - order affect to oid's
 *---------------------------------------------*/
PRIVATE sdata_desc_t tattr_desc[] = {
/*-ATTR-type------------name----------------flag--------default---------description---------- */
SDATA (ASN_BOOLEAN,     "print_with_metadata",0,        0,              "Print response with metadata."),
SDATA (ASN_BOOLEAN,     "verbose",          0,          1,              "Verbose mode."),
SDATA (ASN_OCTET_STR,   "stats",            0,          "",             "Requested statistics."),
SDATA (ASN_OCTET_STR,   "gobj_name",        0,          "",             "Gobj's attribute or command."),
SDATA (ASN_OCTET_STR,   "attribute",        0,          "",             "Requested attribute."),
SDATA (ASN_OCTET_STR,   "command",          0,          "",             "Requested command."),
SDATA (ASN_INTEGER,     "refresh_time",     0,          1,              "Refresh time, in seconds. Set 0 to remove subscription."),
SDATA (ASN_OCTET_STR,   "auth_system",      0,          "",             "OpenID System(interactive jwt)"),
SDATA (ASN_OCTET_STR,   "auth_url",         0,          "",             "OpenID Endpoint (interactive jwt)"),
SDATA (ASN_OCTET_STR,   "azp",              0,          "",             "azp (OAuth2 Authorized Party)"),
SDATA (ASN_OCTET_STR,   "user_id",          0,          "",             "OAuth2 User Id (interactive jwt)"),
SDATA (ASN_OCTET_STR,   "user_passw",       0,          "",             "OAuth2 User password (interactive jwt)"),
SDATA (ASN_OCTET_STR,   "jwt",              0,          "",             "Jwt"),
SDATA (ASN_OCTET_STR,   "url",              0,          "ws://127.0.0.1:1991",  "Url to get Statistics. Can be a ip/hostname or a full url"),
SDATA (ASN_OCTET_STR,   "yuno_name",        0,          "",             "Yuno name"),
SDATA (ASN_OCTET_STR,   "yuno_role",        0,          "",             "Yuno role (No direct connection, all through agent)"),
SDATA (ASN_OCTET_STR,   "yuno_service",     0,          "__default_service__", "Yuno service"),

SDATA (ASN_POINTER,     "gobj_connector",   0,          0,              "connection gobj"),
SDATA (ASN_POINTER,     "user_data",        0,          0,              "user data"),
SDATA (ASN_POINTER,     "user_data2",       0,          0,              "more user data"),
SDATA_END()
};

/*---------------------------------------------*
 *      GClass trace levels
 *---------------------------------------------*/
enum {
    TRACE_USER = 0x0001,
};
PRIVATE const trace_level_t s_user_trace_level[16] = {
{"trace_user",        "Trace user description"},
{0, 0},
};


/*---------------------------------------------*
 *              Private data
 *---------------------------------------------*/
typedef struct _PRIVATE_DATA {
    int32_t refresh_time;
    int32_t verbose;

    hgobj timer;
    hgobj gobj_connector;
} PRIVATE_DATA;




            /******************************
             *      Framework Methods
             ******************************/




/***************************************************************************
 *      Framework Method create
 ***************************************************************************/
PRIVATE void mt_create(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    priv->timer = gobj_create("", GCLASS_TIMER, 0, gobj);

    /*
     *  Do copy of heavy used parameters, for quick access.
     *  HACK The writable attributes must be repeated in mt_writing method.
     */
    SET_PRIV(gobj_connector,        gobj_read_pointer_attr)
    SET_PRIV(refresh_time,          gobj_read_int32_attr)
    SET_PRIV(verbose,               gobj_read_bool_attr)
}

/***************************************************************************
 *      Framework Method writing
 ***************************************************************************/
PRIVATE void mt_writing(hgobj gobj, const char *path)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    IF_EQ_SET_PRIV(refresh_time,         gobj_read_int32_attr)
    ELIF_EQ_SET_PRIV(gobj_connector,     gobj_read_pointer_attr)
    ELIF_EQ_SET_PRIV(verbose,           gobj_read_bool_attr)
    END_EQ_SET_PRIV()
}

/***************************************************************************
 *      Framework Method destroy
 ***************************************************************************/
PRIVATE void mt_destroy(hgobj gobj)
{
}

/***************************************************************************
 *      Framework Method start
 ***************************************************************************/
PRIVATE int mt_start(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    gobj_start(priv->timer);
    const char *auth_url = gobj_read_str_attr(gobj, "auth_url");
    const char *user_id = gobj_read_str_attr(gobj, "user_id");
    if(!empty_string(auth_url) && !empty_string(user_id)) {
        /*
         *  HACK if there are user_id and endpoint
         *  then try to authenticate
         *  else use default local connection
         */
        do_authenticate_task(gobj);
    } else {
        cmd_connect(gobj);
    }
    return 0;
}

/***************************************************************************
 *      Framework Method stop
 ***************************************************************************/
PRIVATE int mt_stop(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);
    clear_timeout(priv->timer);
    gobj_stop(priv->timer);
    return 0;
}




            /***************************
             *      Local Methods
             ***************************/




/***************************************************************************
 *
 ***************************************************************************/
PRIVATE int do_authenticate_task(hgobj gobj)
{
    /*-----------------------------*
     *      Create the task
     *-----------------------------*/
    json_t *kw = json_pack("{s:s, s:s, s:s, s:s, s:s}",
        "auth_system", gobj_read_str_attr(gobj, "auth_system"),
        "auth_url", gobj_read_str_attr(gobj, "auth_url"),
        "user_id", gobj_read_str_attr(gobj, "user_id"),
        "user_passw", gobj_read_str_attr(gobj, "user_passw"),
        "azp", gobj_read_str_attr(gobj, "azp")
    );

    hgobj gobj_task = gobj_create_unique("task-authenticate", GCLASS_TASK_AUTHENTICATE, kw, gobj);
    gobj_subscribe_event(gobj_task, "EV_ON_TOKEN", 0, gobj);
    gobj_set_volatil(gobj_task, TRUE); // auto-destroy

    /*-----------------------*
     *      Start task
     *-----------------------*/
    return gobj_start(gobj_task);
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE char agent_insecure_config[]= "\
{                                               \n\
    'name': '(^^__url__^^)',                    \n\
    'gclass': 'IEvent_cli',                     \n\
    'as_service': true,                          \n\
    'kw': {                                     \n\
        'remote_yuno_name': '(^^__yuno_name__^^)',      \n\
        'remote_yuno_role': '(^^__yuno_role__^^)',      \n\
        'remote_yuno_service': '(^^__yuno_service__^^)' \n\
    },                                          \n\
    'zchilds': [                                 \n\
        {                                               \n\
            'name': '(^^__url__^^)',                    \n\
            'gclass': 'IOGate',                         \n\
            'kw': {                                     \n\
            },                                          \n\
            'zchilds': [                                 \n\
                {                                               \n\
                    'name': '(^^__url__^^)',                    \n\
                    'gclass': 'Channel',                        \n\
                    'kw': {                                     \n\
                    },                                          \n\
                    'zchilds': [                                 \n\
                        {                                               \n\
                            'name': '(^^__url__^^)',                    \n\
                            'gclass': 'GWebSocket',                     \n\
                            'zchilds': [                                \n\
                                {                                       \n\
                                    'name': '(^^__url__^^)',            \n\
                                    'gclass': 'Connex',                 \n\
                                    'kw': {                             \n\
                                        'urls':[                        \n\
                                            '(^^__url__^^)'             \n\
                                        ]                               \n\
                                    }                                   \n\
                                }                                       \n\
                            ]                                           \n\
                        }                                               \n\
                    ]                                           \n\
                }                                               \n\
            ]                                           \n\
        }                                               \n\
    ]                                           \n\
}                                               \n\
";

PRIVATE char agent_secure_config[]= "\
{                                               \n\
    'name': '(^^__url__^^)',                    \n\
    'gclass': 'IEvent_cli',                     \n\
    'as_service': true,                          \n\
    'kw': {                                     \n\
        'jwt': '(^^__jwt__^^)',                         \n\
        'remote_yuno_name': '(^^__yuno_name__^^)',      \n\
        'remote_yuno_role': '(^^__yuno_role__^^)',      \n\
        'remote_yuno_service': '(^^__yuno_service__^^)' \n\
    },                                          \n\
    'zchilds': [                                 \n\
        {                                               \n\
            'name': '(^^__url__^^)',                    \n\
            'gclass': 'IOGate',                         \n\
            'kw': {                                     \n\
            },                                          \n\
            'zchilds': [                                 \n\
                {                                               \n\
                    'name': '(^^__url__^^)',                    \n\
                    'gclass': 'Channel',                        \n\
                    'kw': {                                     \n\
                    },                                          \n\
                    'zchilds': [                                 \n\
                        {                                               \n\
                            'name': '(^^__url__^^)',                    \n\
                            'gclass': 'GWebSocket',                     \n\
                            'zchilds': [                                \n\
                                {                                       \n\
                                    'name': '(^^__url__^^)',            \n\
                                    'gclass': 'Connexs',                \n\
                                    'kw': {                             \n\
                                        'crypto': {                     \n\
                                            'library': 'openssl',       \n\
                                            'trace': false              \n\
                                        },                              \n\
                                        'urls':[                        \n\
                                            '(^^__url__^^)'             \n\
                                        ]                               \n\
                                    }                                   \n\
                                }                                       \n\
                            ]                                           \n\
                        }                                               \n\
                    ]                                           \n\
                }                                               \n\
            ]                                           \n\
        }                                               \n\
    ]                                           \n\
}                                               \n\
";

PRIVATE int cmd_connect(hgobj gobj)
{
    const char *jwt = gobj_read_str_attr(gobj, "jwt");
    const char *url = gobj_read_str_attr(gobj, "url");

    /*
     *  Each display window has a gobj to send the commands (saved in user_data).
     *  For external agents create a filter-chain of gobjs
     */
    json_t * jn_config_variables = json_pack("{s:{s:s, s:s, s:s, s:s, s:s}}",
        "__json_config_variables__",
            "__jwt__", jwt,
            "__url__", url,
            "__yuno_name__", "",
            "__yuno_role__", "yuneta_agent",
            "__yuno_service__", "__default_service__"
    );
    char *sjson_config_variables = json2str(jn_config_variables);
    JSON_DECREF(jn_config_variables);

    /*
     *  Get schema to select tls or not
     */
    char schema[20]={0}, host[120]={0}, port[40]={0};
    if(parse_http_url(url, schema, sizeof(schema), host, sizeof(host), port, sizeof(port), FALSE)<0) {
        log_error(0,
            "gobj",         "%s", gobj_full_name(gobj),
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_PARAMETER_ERROR,
            "msg",          "%s", "parse_http_url() FAILED",
            "url",          "%s", url,
            NULL
        );
    }

    char *agent_config = agent_insecure_config;
    if(strcmp(schema, "wss")==0) {
        agent_config = agent_secure_config;
    }

    hgobj gobj_remote_agent = gobj_create_tree(
        gobj,
        agent_config,
        sjson_config_variables,
        "EV_ON_SETUP",
        "EV_ON_SETUP_COMPLETE"
    );
    gbmem_free(sjson_config_variables);

    gobj_start_tree(gobj_remote_agent);

    printf("Connecting to %s...\n", url);

    return 0;
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE int poll_attr_data(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    GBUFFER *gbuf = gbuf_create(8*1024, 8*1024, 0, 0);
    const char *yuno_name = gobj_read_str_attr(gobj, "yuno_name");
    const char *yuno_role = gobj_read_str_attr(gobj, "yuno_role");
    const char *yuno_service = gobj_read_str_attr(gobj, "yuno_service");
    const char *gobj_name_ = gobj_read_str_attr(gobj, "gobj_name");
    const char *attribute = gobj_read_str_attr(gobj, "attribute");

    gbuf_printf(gbuf, "command-yuno command=read-number");
    if(yuno_service) {
        gbuf_printf(gbuf, " service=%s", yuno_service);
    }
    if(yuno_role) {
        gbuf_printf(gbuf, " yuno_role=%s", yuno_role);
    }
    if(yuno_name) {
        gbuf_printf(gbuf, " yuno_name=%s", yuno_name);
    }

    gbuf_printf(gbuf, " gobj_name='%s'", gobj_name_?gobj_name_:"");
    gbuf_printf(gbuf, " attribute=%s", attribute?attribute:"");

    gobj_command(priv->gobj_connector, gbuf_cur_rd_pointer(gbuf), 0, gobj);
    GBUF_DECREF(gbuf);
    return 0;
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE int poll_command_data(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    GBUFFER *gbuf = gbuf_create(8*1024, 8*1024, 0, 0);
    const char *command = gobj_read_str_attr(gobj, "command");

    gbuf_printf(gbuf, "%s", command);
    gobj_command(priv->gobj_connector, gbuf_cur_rd_pointer(gbuf), 0, gobj);
    GBUF_DECREF(gbuf);
    return 0;
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE int poll_stats_data(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    GBUFFER *gbuf = gbuf_create(8*1024, 8*1024, 0, 0);
    const char *yuno_name = gobj_read_str_attr(gobj, "yuno_name");
    const char *yuno_role = gobj_read_str_attr(gobj, "yuno_role");
    const char *yuno_service = gobj_read_str_attr(gobj, "yuno_service");
    const char *stats = gobj_read_str_attr(gobj, "stats");

    if(yuno_service && (
            strcmp(yuno_service, "__agent__")==0 ||
            strcmp(yuno_service, "__agent_yuno__")==0 ||
            strcmp(yuno_service, "__yuneta_agent__")==0)) {
        gbuf_printf(gbuf, "stats-agent");
        if(strcmp(yuno_service, "__agent_yuno__")==0) {
            gbuf_printf(gbuf, " service=%s", "__yuno__");
        }
    } else {
        gbuf_printf(gbuf, "stats-yuno");
        if(yuno_service) {
            gbuf_printf(gbuf, " service=%s", yuno_service);
        }
    }

    if(!empty_string(yuno_role)) {
        gbuf_printf(gbuf, " yuno_role=%s", yuno_role);
    }
    if(!empty_string(yuno_name)) {
        gbuf_printf(gbuf, " yuno_name=%s", yuno_name);
    }
    gbuf_printf(gbuf, " stats=%s", stats?stats:"");

    gobj_command(priv->gobj_connector, gbuf_cur_rd_pointer(gbuf), 0, gobj);
    GBUF_DECREF(gbuf);
    return 0;
}




            /***************************
             *      Actions
             ***************************/




/***************************************************************************
 *
 ***************************************************************************/
PRIVATE int ac_on_token(hgobj gobj, const char *event, json_t *kw, hgobj src)
{
    int result = kw_get_int(kw, "result", -1, KW_REQUIRED);
    if(result < 0) {
        if(1) {
            const char *comment = kw_get_str(kw, "comment", "", 0);
            printf("\n%s", comment);
            printf("\nAbort.\n");
        }
        gobj_set_exit_code(-1);
        gobj_shutdown();
    } else {
        const char *jwt = kw_get_str(kw, "jwt", "", KW_REQUIRED);
        gobj_write_str_attr(gobj, "jwt", jwt);
        cmd_connect(gobj);
    }

    KW_DECREF(kw);
    return 0;
}

/***************************************************************************
 *  Execute batch of input parameters when the route is opened.
 ***************************************************************************/
PRIVATE int ac_on_open(hgobj gobj, const char *event, json_t *kw, hgobj src)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);
    const char *agent_name = kw_get_str(kw, "remote_yuno_name", 0, 0); // remote agent name

    if(priv->verbose) {
        printf("Connected to '%s', url: '%s'.\n", agent_name, gobj_read_str_attr(gobj, "url"));
    }
    gobj_write_pointer_attr(gobj, "gobj_connector", src);

    const char *command = gobj_read_str_attr(gobj, "command");
    const char *attribute = gobj_read_str_attr(gobj, "attribute");
    const char *gobj_name_ = gobj_read_str_attr(gobj, "gobj_name");
    if(!empty_string(attribute)) {
        if(empty_string(gobj_name_)) {
            printf("Please, the gobj_name of attribute to ask.\n");
            gobj_set_exit_code(-1);
            gobj_shutdown();
        } else {
            if(!priv->verbose) {
                set_timeout(priv->timer, priv->refresh_time * 1000);
            }
            poll_attr_data(gobj);
        }
    } else if(!empty_string(command)) {
        if(!priv->verbose) {
            set_timeout(priv->timer, priv->refresh_time * 1000);
        }
        poll_command_data(gobj);
    } else {
        // Por defecto stats.
        if(!priv->verbose) {
            set_timeout(priv->timer, priv->refresh_time * 1000);
        }
        poll_stats_data(gobj);
// Sistema de subscription. No lo voy a usar aquí.
//         json_t *kw_subscription = json_pack("{s:s, s:i}",
//             "stats", stats,
//             "refresh_time", (int)priv->refresh_time
//         );
//         msg_write_TEMP_str_key(
//             kw_subscription,
//             "yuno_service",
//             gobj_read_str_attr(gobj, "yuno_service")
//         );
//         gobj_subscribe_event(src, "EV_ON_STATS", kw_subscription, gobj);
    }

    KW_DECREF(kw);
    return 0;
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE int ac_on_close(hgobj gobj, const char *event, json_t *kw, hgobj src)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    clear_timeout(priv->timer);
    gobj_write_pointer_attr(gobj, "gobj_connector", 0);
    if(!gobj_is_running(gobj)) {
        KW_DECREF(kw);
        return 0;
    }
    if(priv->verbose) {
        printf("Disconnected.\n");
    }

    gobj_set_exit_code(-1);
    gobj_shutdown();

    KW_DECREF(kw);
    return 0;
}

/***************************************************************************
 *  Stats received.
 ***************************************************************************/
PRIVATE int ac_stats(hgobj gobj, const char *event, json_t *kw, hgobj src)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    int result = WEBIX_RESULT(kw);
    if(result != 0){
        printf("Error %d: %s\n", result, WEBIX_COMMENT(kw));
        gobj_set_exit_code(-1);
        gobj_shutdown();
    } else {
        BOOL to_free = FALSE;
        json_t *jn_data = WEBIX_DATA(kw); //kw_get_dict_value(kw, "data", 0, 0);
        if(!gobj_read_bool_attr(gobj, "print_with_metadata")) {
            jn_data = kw_filter_metadata(jn_data);
            to_free = TRUE;
        }

        if(!priv->verbose) {
            time_t t;
            time(&t);
            printf("\033c");
            printf("Time %llu\n", (unsigned long long)t);
            print_json(jn_data);
        }
        if(to_free) {
            JSON_DECREF(jn_data);
        }
    }

    set_timeout(priv->timer, priv->refresh_time * 1000);

    KW_DECREF(kw);
    return 0;
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE int ac_timeout(hgobj gobj, const char *event, json_t *kw, hgobj src)
{
    const char *attribute = gobj_read_str_attr(gobj, "attribute");
    const char *command = gobj_read_str_attr(gobj, "command");
    if(!empty_string(attribute)) {
        poll_attr_data(gobj);
    } else if(!empty_string(command)) {
        poll_command_data(gobj);
    } else {
        poll_stats_data(gobj);
    }
    KW_DECREF(kw);
    return 0;
}

/***************************************************************************
 *                          FSM
 ***************************************************************************/
PRIVATE const EVENT input_events[] = {
    // top input
    {"EV_MT_COMMAND_ANSWER",EVF_PUBLIC_EVENT,  0,  0},
    {"EV_MT_STATS_ANSWER",  EVF_PUBLIC_EVENT,  0,  0},
    {"EV_ON_TOKEN",         0,  0,  0},
    {"EV_ON_OPEN",          0,  0,  0},
    {"EV_ON_CLOSE",         0,  0,  0},
    {"EV_ON_STATS",         EVF_PUBLIC_EVENT,  0,  0},
    // bottom input
    {"EV_TIMEOUT",          0,  0,  0},
    {"EV_STOPPED",          0,  0,  0},
    // internal
    {NULL, 0, 0, 0}
};
PRIVATE const EVENT output_events[] = {
    {NULL, 0, 0, 0}
};
PRIVATE const char *state_names[] = {
    "ST_DISCONNECTED",
    "ST_CONNECTED",
    NULL
};

PRIVATE EV_ACTION ST_DISCONNECTED[] = {
    {"EV_ON_TOKEN",                 ac_on_token,                0},
    {"EV_ON_OPEN",                  ac_on_open,                 "ST_CONNECTED"},
    {"EV_ON_CLOSE",                 ac_on_close,                0},
    {"EV_STOPPED",                  0,                          0},
    {0,0,0}
};
PRIVATE EV_ACTION ST_CONNECTED[] = {
    {"EV_MT_COMMAND_ANSWER",        ac_stats,                   0},
    {"EV_MT_STATS_ANSWER",          ac_stats,                   0},
    {"EV_ON_STATS",                 ac_stats,                   0},
    {"EV_ON_CLOSE",                 ac_on_close,                "ST_DISCONNECTED"},
    {"EV_TIMEOUT",                  ac_timeout,                 0},
    {"EV_STOPPED",                  0,                          0},
    {0,0,0}
};

PRIVATE EV_ACTION *states[] = {
    ST_DISCONNECTED,
    ST_CONNECTED,
    NULL
};

PRIVATE FSM fsm = {
    input_events,
    output_events,
    state_names,
    states,
};

/***************************************************************************
 *              GClass
 ***************************************************************************/
/*---------------------------------------------*
 *              Local methods table
 *---------------------------------------------*/
PRIVATE LMETHOD lmt[] = {
    {0, 0, 0}
};

/*---------------------------------------------*
 *              GClass
 *---------------------------------------------*/
PRIVATE GCLASS _gclass = {
    0,  // base
    GCLASS_YSTATS_NAME,
    &fsm,
    {
        mt_create,
        0, //mt_create2,
        mt_destroy,
        mt_start,
        mt_stop,
        0, //mt_play,
        0, //mt_pause,
        mt_writing,
        0, //mt_reading,
        0, //mt_subscription_added,
        0, //mt_subscription_deleted,
        0, //mt_child_added,
        0, //mt_child_removed,
        0, //mt_stats,
        0, //mt_command_parser,
        0, //mt_inject_event,
        0, //mt_create_resource,
        0, //mt_list_resource,
        0, //mt_save_resource,
        0, //mt_delete_resource,
        0, //mt_future21
        0, //mt_future22
        0, //mt_get_resource
        0, //mt_state_changed,
        0, //mt_authenticate,
        0, //mt_list_childs,
        0, //mt_stats_updated,
        0, //mt_disable,
        0, //mt_enable,
        0, //mt_trace_on,
        0, //mt_trace_off,
        0, //mt_gobj_created,
        0, //mt_future33,
        0, //mt_future34,
        0, //mt_publish_event,
        0, //mt_publication_pre_filter,
        0, //mt_publication_filter,
        0, //mt_authz_checker,
        0, //mt_future39,
        0, //mt_create_node,
        0, //mt_update_node,
        0, //mt_delete_node,
        0, //mt_link_nodes,
        0, //mt_future44,
        0, //mt_unlink_nodes,
        0, //mt_topic_jtree,
        0, //mt_get_node,
        0, //mt_list_nodes,
        0, //mt_shoot_snap,
        0, //mt_activate_snap,
        0, //mt_list_snaps,
        0, //mt_treedbs,
        0, //mt_treedb_topics,
        0, //mt_topic_desc,
        0, //mt_topic_links,
        0, //mt_topic_hooks,
        0, //mt_node_parents,
        0, //mt_node_childs,
        0, //mt_list_instances,
        0, //mt_node_tree,
        0, //mt_topic_size,
        0, //mt_future62,
        0, //mt_future63,
        0, //mt_future64
    },
    lmt,
    tattr_desc,
    sizeof(PRIVATE_DATA),
    0,  // acl
    s_user_trace_level,
    0,  // cmds
    0,  // gcflag
};

/***************************************************************************
 *              Public access
 ***************************************************************************/
PUBLIC GCLASS *gclass_ystats(void)
{
    return &_gclass;
}
