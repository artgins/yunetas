/***********************************************************************
 *          C_YSTATS.C
 *          YStats GClass.
 *
 *          Yuneta Statistics
 *
 *          Copyright (c) 2016 Niyamaka.
 *          Copyright (c) 2025, ArtGins.
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
SDATA (DTP_BOOLEAN,     "print_with_metadata",0,        0,              "Print response with metadata."),
SDATA (DTP_BOOLEAN,     "verbose",          0,          "1",            "Verbose mode."),
SDATA (DTP_STRING,      "stats",            0,          "",             "Requested statistics."),
SDATA (DTP_STRING,      "gobj_name",        0,          "",             "Gobj's attribute or command."),
SDATA (DTP_STRING,      "attribute",        0,          "",             "Requested attribute."),
SDATA (DTP_STRING,      "command",          0,          "",             "Requested command."),
SDATA (DTP_INTEGER,     "refresh_time",     0,          "1",            "Refresh time, in seconds. Set 0 to remove subscription."),
SDATA (DTP_STRING,      "auth_system",      0,          "",             "OpenID System(interactive jwt)"),
SDATA (DTP_STRING,      "auth_url",         0,          "",             "OpenID Endpoint (interactive jwt)"),
SDATA (DTP_STRING,      "azp",              0,          "",             "azp (OAuth2 Authorized Party)"),
SDATA (DTP_STRING,      "user_id",          0,          "",             "OAuth2 User Id (interactive jwt)"),
SDATA (DTP_STRING,      "user_passw",       0,          "",             "OAuth2 User password (interactive jwt)"),
SDATA (DTP_STRING,      "jwt",              0,          "",             "Jwt"),
SDATA (DTP_STRING,      "url",              0,          "ws://127.0.0.1:1991",  "Url to get Statistics. Can be a ip/hostname or a full url"),
SDATA (DTP_STRING,      "yuno_name",        0,          "",             "Yuno name"),
SDATA (DTP_STRING,      "yuno_role",        0,          "",             "Yuno role (No direct connection, all through agent)"),
SDATA (DTP_STRING,      "yuno_service",     0,          "__default_service__", "Yuno service"),

SDATA (DTP_POINTER,     "gobj_connector",   0,          0,              "connection gobj"),
SDATA (DTP_POINTER,     "user_data",        0,          0,              "user data"),
SDATA (DTP_POINTER,     "user_data2",       0,          0,              "more user data"),
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

    priv->timer = gobj_create_pure_child("", C_TIMER, 0, gobj);

    /*
     *  Do copy of heavy used parameters, for quick access.
     *  HACK The writable attributes must be repeated in mt_writing method.
     */
    SET_PRIV(gobj_connector,        gobj_read_pointer_attr)
    SET_PRIV(refresh_time,          gobj_read_integer_attr)
    SET_PRIV(verbose,               gobj_read_bool_attr)
}

/***************************************************************************
 *      Framework Method writing
 ***************************************************************************/
PRIVATE void mt_writing(hgobj gobj, const char *path)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    IF_EQ_SET_PRIV(refresh_time,         gobj_read_integer_attr)
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

    hgobj gobj_task = gobj_create_service(
        "task-authenticate",
        C_TASK_AUTHENTICATE,
        kw,
        gobj
    );
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
PRIVATE char agent_config[]= "\
{                                               \n\
    'name': 'agent_client',                    \n\
    'gclass': 'IEvent_cli',                     \n\
    'as_service': true,                          \n\
    'kw': {                                     \n\
        'remote_yuno_name': '(^^__yuno_name__^^)',      \n\
        'remote_yuno_role': '(^^__yuno_role__^^)',      \n\
        'remote_yuno_service': '(^^__yuno_service__^^)' \n\
    },                                          \n\
    'children': [                                 \n\
        {                                               \n\
            'name': 'agent_client',                    \n\
            'gclass': 'C_IOGATE',                         \n\
            'kw': {                                     \n\
            },                                          \n\
            'children': [                                 \n\
                {                                               \n\
                    'name': 'agent_client',                    \n\
                    'gclass': 'C_CHANNEL',                        \n\
                    'kw': {                                     \n\
                    },                                          \n\
                    'children': [                                 \n\
                        {                                               \n\
                            'name': 'agent_client',                    \n\
                            'gclass': 'C_WEBSOCKET',                     \n\
                            'kw': {                                     \n\
                                'url':'(^^__url__^^)'   \n\
                            }                                   \n\
                        }                                       \n\
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
    json_t * jn_config_variables = json_pack("{s:s, s:s, s:s, s:s, s:s}",
        "__jwt__", jwt,
        "__url__", url,
        "__yuno_name__", "",
        "__yuno_role__", "yagent",
        "__yuno_service__", "__default_service__"
    );

    /*
     *  Get schema to select tls or not
     */
    char schema[20]={0}, host[120]={0}, port[40]={0};
    if(parse_url(
        gobj,
        url,
        schema,
        sizeof(schema),
        host, sizeof(host),
        port, sizeof(port),
        0, 0,
        0, 0,
        FALSE
    )<0) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_PARAMETER_ERROR,
            "msg",          "%s", "parse_http_url() FAILED",
            "url",          "%s", url,
            NULL
        );
    }

    hgobj gobj_remote_agent = gobj_create_tree(
        gobj,
        agent_config,
        jn_config_variables // owned
    );

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

    gbuffer_t *gbuf = gbuffer_create(8*1024, 8*1024);
    const char *yuno_name = gobj_read_str_attr(gobj, "yuno_name");
    const char *yuno_role = gobj_read_str_attr(gobj, "yuno_role");
    const char *yuno_service = gobj_read_str_attr(gobj, "yuno_service");
    const char *gobj_name_ = gobj_read_str_attr(gobj, "gobj_name");
    const char *attribute = gobj_read_str_attr(gobj, "attribute");

    gbuffer_printf(gbuf, "command-yuno command=read-number");
    if(yuno_service) {
        gbuffer_printf(gbuf, " service=%s", yuno_service);
    }
    if(yuno_role) {
        gbuffer_printf(gbuf, " yuno_role=%s", yuno_role);
    }
    if(yuno_name) {
        gbuffer_printf(gbuf, " yuno_name=%s", yuno_name);
    }

    gbuffer_printf(gbuf, " gobj_name='%s'", gobj_name_?gobj_name_:"");
    gbuffer_printf(gbuf, " attribute=%s", attribute?attribute:"");

    gobj_command(priv->gobj_connector, gbuffer_cur_rd_pointer(gbuf), 0, gobj);
    GBUFFER_DECREF(gbuf);
    return 0;
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE int poll_command_data(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    gbuffer_t *gbuf = gbuffer_create(8*1024, 8*1024);
    const char *command = gobj_read_str_attr(gobj, "command");

    gbuffer_printf(gbuf, "%s", command);
    gobj_command(priv->gobj_connector, gbuffer_cur_rd_pointer(gbuf), 0, gobj);
    GBUFFER_DECREF(gbuf);
    return 0;
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE int poll_stats_data(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    gbuffer_t *gbuf = gbuffer_create(8*1024, 8*1024);
    const char *yuno_name = gobj_read_str_attr(gobj, "yuno_name");
    const char *yuno_role = gobj_read_str_attr(gobj, "yuno_role");
    const char *yuno_service = gobj_read_str_attr(gobj, "yuno_service");
    const char *stats = gobj_read_str_attr(gobj, "stats");

    if(yuno_service && (
            strcmp(yuno_service, "__agent__")==0 ||
            strcmp(yuno_service, "__agent_yuno__")==0 ||
            strcmp(yuno_service, "__yuneta_agent__")==0)) {
        gbuffer_printf(gbuf, "stats-agent");
        if(strcmp(yuno_service, "__agent_yuno__")==0) {
            gbuffer_printf(gbuf, " service=%s", "__yuno__");
        }
    } else {
        gbuffer_printf(gbuf, "stats-yuno");
        if(yuno_service) {
            gbuffer_printf(gbuf, " service=%s", yuno_service);
        }
    }

    if(!empty_string(yuno_role)) {
        gbuffer_printf(gbuf, " yuno_role=%s", yuno_role);
    }
    if(!empty_string(yuno_name)) {
        gbuffer_printf(gbuf, " yuno_name=%s", yuno_name);
    }
    gbuffer_printf(gbuf, " stats=%s", stats?stats:"");

    gobj_command(priv->gobj_connector, gbuffer_cur_rd_pointer(gbuf), 0, gobj);
    GBUFFER_DECREF(gbuf);
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
    int result = (int)kw_get_int(gobj, kw, "result", -1, KW_REQUIRED);
    if(result < 0) {
        if(1) {
            const char *comment = kw_get_str(gobj, kw, "comment", "", 0);
            printf("\n%s", comment);
            printf("\nAbort.\n");
        }
        gobj_set_exit_code(-1);
        gobj_shutdown();
    } else {
        const char *jwt = kw_get_str(gobj, kw, "jwt", "", KW_REQUIRED);
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
    const char *agent_name = kw_get_str(gobj, kw, "remote_yuno_name", 0, 0); // remote agent name

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

    int result = (int)kw_get_int(gobj, kw, "result", -1, 0);
    const char *comment = kw_get_str(gobj, kw, "comment", "", 0);
    if(result != 0){
        printf("Error %d: %s\n", result, comment);
        gobj_set_exit_code(-1);
        gobj_shutdown();
    } else {
        BOOL to_free = FALSE;
        json_t *jn_data = kw_get_dict_value(gobj, kw, "data", 0, 0);
        if(!gobj_read_bool_attr(gobj, "print_with_metadata")) {
            jn_data = kw_filter_metadata(gobj, jn_data);
            to_free = TRUE;
        }

        if(!priv->verbose) {
            time_t t;
            time(&t);
            printf("\033c");
            printf("Time %llu\n", (unsigned long long)t);
            print_json2("", jn_data);
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

/*---------------------------------------------*
 *          Global methods table
 *---------------------------------------------*/
PRIVATE const GMETHODS gmt = {
    .mt_create      = mt_create,
    .mt_destroy     = mt_destroy,
    .mt_start       = mt_start,
    .mt_stop        = mt_stop,
    .mt_writing     = mt_writing,
};

/*------------------------*
 *      GClass name
 *------------------------*/
GOBJ_DEFINE_GCLASS(C_YSTATS);

/*------------------------*
 *      States
 *------------------------*/

/*------------------------*
 *      Events
 *------------------------*/

/***************************************************************************
 *          Create the GClass
 ***************************************************************************/
PRIVATE int create_gclass(gclass_name_t gclass_name)
{
    static hgclass __gclass__ = 0;
    if(__gclass__) {
        gobj_log_error(0, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_INTERNAL_ERROR,
            "msg",          "%s", "GClass ALREADY created",
            "gclass",       "%s", gclass_name,
            NULL
        );
        return -1;
    }

    /*------------------------*
     *      States
     *------------------------*/
    ev_action_t st_disconnected[] = {
        {EV_ON_TOKEN,           ac_on_token,                0},
        {EV_ON_OPEN,            ac_on_open,                 ST_CONNECTED},
        {EV_ON_CLOSE,           ac_on_close,                0},
        {EV_STOPPED,            0,                          0},
        {0,0,0}
    };

    ev_action_t st_connected[] = {
        {EV_MT_COMMAND_ANSWER,  ac_stats,                   0},
        {EV_MT_STATS_ANSWER,    ac_stats,                   0},
        // {EV_ON_STATS,           ac_stats,                   0},
        {EV_ON_CLOSE,           ac_on_close,                ST_DISCONNECTED},
        {EV_TIMEOUT,            ac_timeout,                 0},
        {EV_STOPPED,            0,                          0},
        {0,0,0}
    };

    states_t states[] = {
        {ST_DISCONNECTED,       st_disconnected},
        {ST_CONNECTED,          st_connected},
        {0, 0}
    };

    /*------------------------*
     *      Events
     *------------------------*/
    event_type_t event_types[] = {
        {EV_MT_COMMAND_ANSWER,  EVF_PUBLIC_EVENT},
        {EV_MT_STATS_ANSWER,    EVF_PUBLIC_EVENT},
        // {EV_ON_STATS,           EVF_PUBLIC_EVENT},
        {EV_ON_TOKEN,           0},
        {EV_ON_OPEN,            0},
        {EV_ON_CLOSE,           0},
        {EV_TIMEOUT,            0},
        {EV_STOPPED,            0},
        {NULL, 0}
    };

    /*----------------------------------------*
     *          Register GClass
     *----------------------------------------*/
    __gclass__ = gclass_create(
        gclass_name,
        event_types,
        states,
        &gmt,
        0,  // Local methods table (LMT)
        tattr_desc,
        sizeof(PRIVATE_DATA),
        0,  // Authorization table
        0,  // Command table
        s_user_trace_level,
        0   // GClass flags
    );
    if(!__gclass__) {
        return -1;
    }

    return 0;
}

/***************************************************************************
 *          Public access
 ***************************************************************************/
PUBLIC int register_c_ystats(void)
{
    return create_gclass(C_YSTATS);
}
