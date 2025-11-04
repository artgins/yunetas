/***********************************************************************
 *          C_MQTT_BROKER.C
 *          Mqtt_broker GClass.
 *
 *          Mqtt broker
 *
 *          Copyright (c) 2025, ArtGins.
 *          All Rights Reserved.
 ***********************************************************************/
#include <stdio.h>
#include <limits.h>
#include <string.h>

#include <grp.h>
#include <pwd.h>
#include <unistd.h>

#include <gobj.h>
#include <g_ev_kernel.h>
#include <g_st_kernel.h>
#include <helpers.h>

#ifdef __linux__
#if defined(CONFIG_HAVE_OPENSSL)
    #include <openssl/ssl.h>
    #include <openssl/rand.h>
#elif defined(CONFIG_HAVE_MBEDTLS)
    #include <mbedtls/md.h>
    #include <mbedtls/ctr_drbg.h>
    #include <mbedtls/pkcs5.h>
#else
    #error "No crypto library defined"
#endif
#endif

#include "treedb_schema_mqtt_broker.c"
#include "c_mqtt_broker.h"

/***************************************************************************
 *              Constants
 ***************************************************************************/

/***************************************************************************
 *              Structures
 ***************************************************************************/

/***************************************************************************
 *              Prototypes
 ***************************************************************************/
PRIVATE json_t *hash_password(
    hgobj gobj,
    const char *password,
    const char *digest,
    unsigned int iterations
);
PRIVATE int check_password(
    hgobj gobj,
    const char *username,
    const char *password
);

/***************************************************************************
 *          Data: config, public data, private data
 ***************************************************************************/
PRIVATE json_t *cmd_help(hgobj gobj, const char *cmd, json_t *kw, hgobj src);
PRIVATE json_t *cmd_authzs(hgobj gobj, const char *cmd, json_t *kw, hgobj src);
PRIVATE json_t *cmd_allow_anonymous(hgobj gobj, const char *cmd, json_t *kw, hgobj src);
PRIVATE json_t *cmd_list_users(hgobj gobj, const char *cmd, json_t *kw, hgobj src);
PRIVATE json_t *cmd_create_user(hgobj gobj, const char *cmd, json_t *kw, hgobj src);
PRIVATE json_t *cmd_enable_user(hgobj gobj, const char *cmd, json_t *kw, hgobj src);
PRIVATE json_t *cmd_disable_user(hgobj gobj, const char *cmd, json_t *kw, hgobj src);
PRIVATE json_t *cmd_delete_user(hgobj gobj, const char *cmd, json_t *kw, hgobj src);
PRIVATE json_t *cmd_set_user_passw(hgobj gobj, const char *cmd, json_t *kw, hgobj src);
PRIVATE json_t *cmd_check_user_passw(hgobj gobj, const char *cmd, json_t *kw, hgobj src);

PRIVATE sdata_desc_t pm_help[] = {
/*-PM----type-----------name------------flag------------default-----description---------- */
SDATAPM (DTP_STRING,    "cmd",          0,              0,          "command about you want help."),
SDATAPM (DTP_INTEGER,   "level",        0,              0,          "command search level in childs"),
SDATA_END()
};
PRIVATE sdata_desc_t pm_authzs[] = {
/*-PM----type-----------name------------flag------------default-----description---------- */
SDATAPM (DTP_STRING,    "authz",        0,              0,          "permission to search"),
SDATAPM (DTP_STRING,    "service",      0,              0,          "Service where to search the permission. If empty print all service's permissions"),
SDATA_END()
};

PRIVATE sdata_desc_t pm_allow_anonymous[] = {
/*-PM----type-----------name------------flag------------default-----description---------- */
SDATAPM (DTP_BOOLEAN,   "set",          0,              0,          "Allow anonymous: set 1 o 0"),
SDATA_END()
};
PRIVATE sdata_desc_t pm_list_users[] = {
/*-PM----type-----------name------------flag------------default-----description---------- */
SDATA_END()
};
PRIVATE sdata_desc_t pm_create_user[] = {
/*-PM----type-----------name---------------flag--------default---------description---------- */
SDATAPM (DTP_STRING,    "username",         0,          0,              "Username"),
SDATAPM (DTP_STRING,    "password",         0,          0,              "Password"),
SDATAPM (DTP_INTEGER,   "hashIterations",   0,          "27500",        "Default To build a password"),
SDATAPM (DTP_STRING,    "algorithm",        0,          "sha256",       "Default To build a password"),
SDATA_END()
};
PRIVATE sdata_desc_t pm_user[] = {
/*-PM----type-----------name------------flag------------default-----description---------- */
SDATAPM (DTP_STRING,    "username",         0,          0,              "Username"),
SDATA_END()
};
PRIVATE sdata_desc_t pm_check_passw[] = {
/*-PM----type-----------name------------flag------------default-----description---------- */
SDATAPM (DTP_STRING,    "username",         0,          0,              "Username"),
SDATAPM (DTP_STRING,    "password",         0,          0,              "Password"),
SDATA_END()
};
PRIVATE sdata_desc_t pm_set_passw[] = {
/*-PM----type-----------name------------flag------------default-----description---------- */
SDATAPM (DTP_STRING,    "username",         0,          0,              "Username"),
SDATAPM (DTP_STRING,    "password",         0,          0,              "Password"),
SDATAPM (DTP_INTEGER,   "hashIterations",   0,          "27500",        "Default To build a password"),
SDATAPM (DTP_STRING,    "algorithm",        0,          "sha256",       "Default To build a password"),
SDATA_END()
};

PRIVATE const char *a_help[] = {"h", "?", 0};

PRIVATE sdata_desc_t command_table[] = {
/*-CMD---type-----------name----------------alias---items-------json_fn-------------description---------- */
SDATACM (DTP_SCHEMA,    "help",             a_help, pm_help,    cmd_help,           "Command's help"),

/*-CMD2---type----------name----------------flag----alias---items---------------json_fn-------------description--*/
SDATACM2 (DTP_SCHEMA,   "authzs",           0,      0,      pm_authzs,          cmd_authzs,         "Authorization's help"),
SDATACM2 (DTP_SCHEMA,   "allow-anonymous",  0,      0,      pm_allow_anonymous, cmd_allow_anonymous,"Allow anonymous users (don't check user/password of CONNECT mqtt command)"),
SDATACM2 (DTP_SCHEMA,   "list-users",       0,      0,      pm_list_users,      cmd_list_users,     "List users"),
SDATACM2 (DTP_SCHEMA,   "create-user",      0,      0,      pm_create_user,     cmd_create_user,    "Create user"),
SDATACM2 (DTP_SCHEMA,   "enable-user",      0,      0,      pm_user,            cmd_enable_user,    "Enable user"),
SDATACM2 (DTP_SCHEMA,   "disable-user",     0,      0,      pm_user,            cmd_disable_user,    "Disable user"),
SDATACM2 (DTP_SCHEMA,   "delete-user",      0,      0,      pm_user,            cmd_delete_user,    "Delete user"),
SDATACM2 (DTP_SCHEMA,   "check-user-pwd",   0,      0,      pm_check_passw,     cmd_check_user_passw, "Check user password"),
SDATACM2 (DTP_SCHEMA,   "set-user-pwd",     0,      0,      pm_set_passw,       cmd_set_user_passw, "Set user password"),
SDATA_END()
};

/*---------------------------------------------*
 *      Attributes - order affect to oid's
 *---------------------------------------------*/
PRIVATE sdata_desc_t tattr_desc[] = {
/*-ATTR-type--------name----------------flag--------default-----description---------- */
// TODO a 0 cuando funcionen bien los out schemas
SDATA (DTP_BOOLEAN, "use_internal_schema",SDF_PERSIST, "1",     "Use internal (hardcoded) schema"),

SDATA (DTP_BOOLEAN, "allow_anonymous",  SDF_PERSIST, "1",       "Boolean value that determines whether clients that connect without providing a username are allowed to connect. If set to FALSE then another means of connection should be created to control authenticated client access. Defaults to TRUE, (TODO but connections are only allowed from the local machine)."),

SDATA (DTP_INTEGER, "hashIterations",   0,          "27500",    "Default To build a password"),
SDATA (DTP_STRING,  "algorithm",        0,          "sha256",   "Default To build a password"),

SDATA (DTP_INTEGER, "on_critical_error",SDF_RD,     "2",        "LOG_OPT_EXIT_ZERO exit on error (Zero to avoid restart)"),
SDATA (DTP_POINTER, "subscriber",       0,          0,          "Subscriber of output-events. If it's null then the subscriber is the parent."),
SDATA (DTP_INTEGER, "timeout",          SDF_RD,     "1000",     "Timeout"),
SDATA (DTP_POINTER, "user_data",        0,          0,          "user data"),
SDATA (DTP_POINTER, "user_data2",       0,          0,          "more user data"),
SDATA_END()
};

/*---------------------------------------------*
 *      GClass trace levels
 *---------------------------------------------*/
enum {
    TRACE_MESSAGES = 0x0001,
};
PRIVATE const trace_level_t s_user_trace_level[16] = {
{"messages",        "Trace messages"},
{0, 0},
};

/*---------------------------------------------*
 *      GClass authz levels
 *---------------------------------------------*/
PRIVATE sdata_desc_t authz_table[] = {
/*-AUTHZ-- type---------name----------------flag----alias---items---description--*/
SDATA_END()
};

/*---------------------------------------------*
 *              Private data
 *---------------------------------------------*/
typedef struct _PRIVATE_DATA {
    hgobj timer;
    int32_t timeout;
    hgobj gobj_input_side;
    hgobj gobj_tranger_clients;

    hgobj gobj_treedbs;
    hgobj gobj_treedb_mqtt_broker;
    hgobj gobj_authz;

    BOOL allow_anonymous;
    char treedb_name[NAME_MAX];

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

    priv->timer = gobj_create_pure_child(gobj_name(gobj), C_TIMER, 0, gobj);

    /*
     *  SERVICE subscription model
     */
    hgobj subscriber = (hgobj)gobj_read_pointer_attr(gobj, "subscriber");
    if(subscriber) {
        gobj_subscribe_event(gobj, NULL, NULL, subscriber);
    } else if(gobj_is_pure_child(gobj)) {
        subscriber = gobj_parent(gobj);
        gobj_subscribe_event(gobj, NULL, NULL, subscriber);
    }

    /*----------------------------------------*
     *      Check user yuneta
     *----------------------------------------*/
    // Use __username__ from yuno

#if defined(__linux__)
#if defined(CONFIG_HAVE_OPENSSL)
    OpenSSL_add_all_digests();
// #elif defined(CONFIG_HAVE_MBEDTLS)
#else
    #error "No crypto library defined"
#endif
#endif

    /*
     *  Do copy of heavy used parameters, for quick access.
     *  HACK The writable attributes must be repeated in mt_writing method.
     */
    SET_PRIV(timeout,                   gobj_read_integer_attr)
    SET_PRIV(allow_anonymous,           gobj_read_bool_attr)
}

/***************************************************************************
 *      Framework Method writing
 ***************************************************************************/
PRIVATE void mt_writing(hgobj gobj, const char *path)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    IF_EQ_SET_PRIV(timeout,                 gobj_read_integer_attr)
    ELIF_EQ_SET_PRIV(allow_anonymous,       gobj_read_bool_attr)
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

    /*-----------------------------*
     *      Get Authzs service
     *-----------------------------*/
    priv->gobj_authz =  gobj_find_service("authz", TRUE);
    gobj_subscribe_event(priv->gobj_authz, 0, 0, gobj);

    return 0;
}

/***************************************************************************
 *      Framework Method stop
 ***************************************************************************/
PRIVATE int mt_stop(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    gobj_unsubscribe_event(priv->gobj_authz, 0, 0, gobj);

    return 0;
}

/***************************************************************************
 *      Framework Method play
 ***************************************************************************/
PRIVATE int mt_play(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    /*-------------------------------------------*
     *          Create Treedb System
     *-------------------------------------------*/
    char path[PATH_MAX];
    yuneta_realm_store_dir(
        path,
        sizeof(path),
        gobj_yuno_role(),
        gobj_yuno_realm_owner(),
        gobj_yuno_realm_id(),
        "",  // gclass-treedb controls the directories
        TRUE
    );
    json_t *kw_treedbs = json_pack("{s:s, s:s, s:b, s:i, s:i, s:i}",
        "path", path,
        "filename_mask", "%Y",  // to management treedbs we don't need multifiles (per day)
        "master", 1,
        "xpermission", 02770,
        "rpermission", 0660,
        "exit_on_error", LOG_OPT_EXIT_ZERO
    );
    priv->gobj_treedbs = gobj_create_service(
        "treedbs",
        C_TREEDB,
        kw_treedbs,
        gobj
    );

    /*
     *  HACK pipe inheritance
     */
    gobj_set_bottom_gobj(gobj, priv->gobj_treedbs);

    /*
     *  Start treedbs
     */
    gobj_subscribe_event(priv->gobj_treedbs, 0, 0, gobj);
    gobj_start_tree(priv->gobj_treedbs);

    /*-------------------------------------------*
     *      Load schema
     *      Open treedb mqtt_broker service
     *-------------------------------------------*/
    helper_quote2doublequote(treedb_schema_mqtt_broker);
    json_t *jn_treedb_schema_mqtt_broker = legalstring2json(treedb_schema_mqtt_broker, TRUE);
    if(!jn_treedb_schema_mqtt_broker) {
        /*
         *  Exit if schema fails
         */
        exit(-1);
    }

    if(parse_schema(jn_treedb_schema_mqtt_broker)<0) {
        /*
         *  Exit if schema fails
         */
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_APP_ERROR,
            "msg",          "%s", "Parse schema fails",
            NULL
        );
        exit(-1);
    }

    BOOL use_internal_schema = gobj_read_bool_attr(gobj, "use_internal_schema");

    const char *treedb_name = kw_get_str(gobj,
        jn_treedb_schema_mqtt_broker,
        "id",
        "treedb_mqtt_broker",
        KW_REQUIRED
    );
    snprintf(priv->treedb_name, sizeof(priv->treedb_name), "%s", treedb_name);

    json_t *kw_treedb = json_pack("{s:s, s:i, s:s, s:o, s:b, s:s}",
        "filename_mask", "%Y",
        "exit_on_error", 0,
        "treedb_name", priv->treedb_name,
        "treedb_schema", jn_treedb_schema_mqtt_broker,
        "use_internal_schema", use_internal_schema,
        "__username__", gobj_read_str_attr(gobj_yuno(), "__username__")
    );
    json_t *jn_resp = gobj_command(priv->gobj_treedbs,
        "open-treedb",
        kw_treedb,
        gobj
    );
    int result = (int)kw_get_int(gobj, jn_resp, "result", -1, KW_REQUIRED);
    if(result < 0) {
        const char *comment = kw_get_str(gobj, jn_resp, "comment", "", KW_REQUIRED);
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_APP_ERROR,
            "msg",          "%s", comment,
            NULL
        );
    }
    json_decref(jn_resp);

    priv->gobj_treedb_mqtt_broker = gobj_find_service(priv->treedb_name, TRUE);
    gobj_subscribe_event(priv->gobj_treedb_mqtt_broker, 0, 0, gobj);

    /*------------------------------------------------------*
     *      Open mqtt_broker tranger for clients (topics)
     *------------------------------------------------------*/
    json_t *kw_tranger = json_pack("{s:s, s:s, s:b, s:I, s:i}",
        "path", path,
        "database", "clients",
        "master", 1,
        "subscriber", (json_int_t)(uintptr_t)gobj,
        "on_critical_error", (int)gobj_read_integer_attr(gobj, "on_critical_error")
    );
    priv->gobj_tranger_clients = gobj_create_service(
        "gobj_tranger_clients",
        C_TRANGER,
        kw_tranger,
        gobj
    );
    gobj_start(priv->gobj_tranger_clients);

    /*-------------------------*
     *      Start services
     *-------------------------*/
    priv->gobj_input_side = gobj_find_service("__input_side__", TRUE);
    gobj_subscribe_event(priv->gobj_input_side, 0, 0, gobj);
    gobj_start_tree(priv->gobj_input_side);

    // TODO
    // priv->trq_msgs = trq_open(
    //     priv->tranger,
    //     topic_name,
    //     gobj_read_str_attr(gobj, "tkey"),
    //     tranger2_str2system_flag(gobj_read_str_attr(gobj, "system_flag")),
    //     gobj_read_integer_attr(gobj, "backup_queue_size")
    // );

    // TODO trq_load(priv->trq_msgs);

    return 0;
}

/***************************************************************************
 *      Framework Method pause
 ***************************************************************************/
PRIVATE int mt_pause(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    /*---------------------------------------*
     *      Stop services
     *---------------------------------------*/
    if(priv->gobj_input_side) {
        if(gobj_is_playing(priv->gobj_input_side)) {
            gobj_pause(priv->gobj_input_side);
        }
        gobj_stop_tree(priv->gobj_input_side);
    }

    /*---------------------------------------*
     *      Close treedb mqtt_broker
     *---------------------------------------*/
    json_decref(gobj_command(priv->gobj_treedbs,
        "close-treedb",
        json_pack("{s:s, s:s}",
            "treedb_name", priv->treedb_name,
            "__username__", gobj_read_str_attr(gobj_yuno(), "__username__")
        ),
        gobj
    ));

    priv->gobj_treedb_mqtt_broker = 0;

    if(priv->gobj_tranger_clients) {
        gobj_stop(priv->gobj_tranger_clients);
        EXEC_AND_RESET(gobj_destroy, priv->gobj_tranger_clients)
    }

    /*-------------------------*
     *      Stop treedbs
     *-------------------------*/
    if(priv->gobj_treedbs) {
        gobj_unsubscribe_event(priv->gobj_treedbs, 0, 0, gobj);
        gobj_stop_tree(priv->gobj_treedbs);
        EXEC_AND_RESET(gobj_destroy, priv->gobj_treedbs)
    }

    clear_timeout(priv->timer);

    return 0;
}




                    /***************************
                     *      Commands
                     ***************************/




/***************************************************************************
 *
 ***************************************************************************/
PRIVATE json_t *cmd_help(hgobj gobj, const char *cmd, json_t *kw, hgobj src)
{
    KW_INCREF(kw);
    json_t *jn_resp = gobj_build_cmds_doc(gobj, kw);
    return msg_iev_build_response(
        gobj,
        0,
        jn_resp,
        0,
        0,
        kw  // owned
    );
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE json_t *cmd_authzs(hgobj gobj, const char *cmd, json_t *kw, hgobj src)
{
    return gobj_build_authzs_doc(gobj, cmd, kw);
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE json_t *cmd_allow_anonymous(hgobj gobj, const char *cmd, json_t *kw, hgobj src)
{
    BOOL set = kw_get_bool(gobj, kw, "set", 0, KW_WILD_NUMBER);

    if(set) {
        gobj_write_bool_attr(gobj, "allow_anonymous", TRUE);
    } else {
        gobj_write_bool_attr(gobj, "allow_anonymous", FALSE);
    }
    return msg_iev_build_response(
        gobj,
        0,
        json_sprintf("Set allow_anonymous %s", set?"true":"false"),
        0,
        0,
        kw  // owned
    );
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE json_t *cmd_list_users(hgobj gobj, const char *cmd, json_t *kw, hgobj src)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    json_t *users = gobj_list_nodes(
        priv->gobj_treedb_mqtt_broker,
        "users",
        0, // filter
        0, // options
        src
    );

    return msg_iev_build_response(
        gobj,
        0,
        0,
        tranger2_list_topic_desc_cols(
            gobj_read_pointer_attr(priv->gobj_treedb_mqtt_broker, "tranger"),
            "users"
        ),
        users, // owned
        kw  // owned
    );
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE json_t *cmd_create_user(hgobj gobj, const char *cmd, json_t *kw, hgobj src)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    /*--------------------------*
     *      Get parameters
     *--------------------------*/
    const char *username = kw_get_str(gobj, kw, "username", "", 0);
    const char *password = kw_get_str(gobj, kw, "password", "", 0);
    if(empty_string(username)) {
        return msg_iev_build_response(
            gobj,
            -1,
            json_sprintf("What username?"),
            0,
            0,
            kw  // owned
        );
    }
    if(empty_string(password)) {
        return msg_iev_build_response(
            gobj,
            -1,
            json_sprintf("What password?"),
            0,
            0,
            kw  // owned
        );
    }

    int hashIterations = (int)kw_get_int(
        gobj,
        kw,
        "hashIterations",
        gobj_read_integer_attr(gobj, "hashIterations"),
        KW_WILD_NUMBER
    );
    const char *algorithm = kw_get_str(
        gobj,
        kw,
        "algorithm",
        gobj_read_str_attr(gobj, "algorithm"),
        0
    );

    /*-----------------------------*
     *  Check if username exists
     *-----------------------------*/
    json_t *user = gobj_get_node(
        priv->gobj_treedb_mqtt_broker,
        "users",
        json_pack("{s:s}",
            "id", username
        ),
        0,
        src
    );
    if(user) {
        JSON_DECREF(user)
        return msg_iev_build_response(gobj,
            -1,
            json_sprintf("User already exists: %s", username),
            0,
            0,
            kw  // owned
        );
    }

    /*-----------------------------*
     *      Create user
     *-----------------------------*/
    json_t *credentials = hash_password(
        gobj,
        password,
        algorithm,
        hashIterations
    );
    if(!credentials) {
        return msg_iev_build_response(
            gobj,
            -1,
            json_sprintf("Error creating credentials: %s", gobj_log_last_message()),
            0,
            0,
            kw  // owned
        );
    }

    user = gobj_create_node(
        priv->gobj_treedb_mqtt_broker,
        "users",
        json_pack("{s:s, s:o}",
            "id", username,
            "credentials", credentials
        ),
        0,
        gobj
    );
    if(!user) {
        return msg_iev_build_response(
            gobj,
            -1,
            json_sprintf("Cannot create user: %s", gobj_log_last_message()),
            0,
            0,
            kw  // owned
        );
    }
    JSON_DECREF(user)

    return msg_iev_build_response(
        gobj,
        0,
        json_sprintf("Created user: %s", username),
        0,
        0, // owned
        kw  // owned
    );
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE json_t *cmd_enable_user(hgobj gobj, const char *cmd, json_t *kw, hgobj src)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);
    const char *username = kw_get_str(gobj, kw, "username", "", 0);

    if(empty_string(username)) {
        return msg_iev_build_response(
            gobj,
            -1,
            json_sprintf("What username?"),
            0,
            0,
            kw  // owned
        );
    }

    json_t *user = gobj_get_node(
        priv->gobj_treedb_mqtt_broker,
        "users",
        json_pack("{s:s}", "id", username),
        json_pack("{s:b}",
            "with_metadata", 1
        ),
        gobj
    );
    if(!user) {
        return msg_iev_build_response(
            gobj,
            -1,
            json_sprintf("User not found: '%s'", username),
            0,
            0,
            kw  // owned
        );
    }

    json_object_set_new(user, "disabled", json_false());
    user = gobj_update_node(
        priv->gobj_treedb_mqtt_broker,
        "users",
        user,
        0,
        src
    );
    JSON_DECREF(user)

    return msg_iev_build_response(
        gobj,
        0,
        json_sprintf("User enabled: %s", username),
        0,
        0,
        kw  // owned
    );
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE json_t *cmd_disable_user(hgobj gobj, const char *cmd, json_t *kw, hgobj src)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);
    const char *username = kw_get_str(gobj, kw, "username", "", 0);

    if(empty_string(username)) {
        return msg_iev_build_response(
            gobj,
            -1,
            json_sprintf("What username?"),
            0,
            0,
            kw  // owned
        );
    }

    json_t *user = gobj_get_node(
        priv->gobj_treedb_mqtt_broker,
        "users",
        json_pack("{s:s}", "id", username),
        json_pack("{s:b}",
            "with_metadata", 1
        ),
        gobj
    );
    if(!user) {
        return msg_iev_build_response(
            gobj,
            -1,
            json_sprintf("User not found: '%s'", username),
            0,
            0,
            kw  // owned
        );
    }

    json_object_set_new(user, "disabled", json_true());

    user = gobj_update_node(
        priv->gobj_treedb_mqtt_broker,
        "users",
        user,
        0,
        gobj
    );
    JSON_DECREF(user)

    return msg_iev_build_response(
        gobj,
        0,
        json_sprintf("User disabled: %s", username),
        0,
        0,
        kw  // owned
    );
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE json_t *cmd_delete_user(hgobj gobj, const char *cmd, json_t *kw, hgobj src)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);
    const char *username = kw_get_str(gobj, kw, "username", "", 0);

    if(empty_string(username)) {
        return msg_iev_build_response(
            gobj,
            -1,
            json_sprintf("What username?"),
            0,
            0,
            kw  // owned
        );
    }

    json_t *user = gobj_get_node(
        priv->gobj_treedb_mqtt_broker,
        "users",
        json_pack("{s:s}", "id", username),
        json_pack("{s:b}",
            "with_metadata", 1
        ),
        gobj
    );
    if(!user) {
        return msg_iev_build_response(
            gobj,
            -1,
            json_sprintf("User not found: '%s'", username),
            0,
            0,
            kw  // owned
        );
    }

    int ret = gobj_delete_node(
        priv->gobj_treedb_mqtt_broker,
        "users",
        user,
        0,
        gobj
    );

    return msg_iev_build_response(
        gobj,
        ret,
        json_sprintf("User deleted: %s", username),
        0,
        0,
        kw  // owned
    );
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE json_t *cmd_check_user_passw(hgobj gobj, const char *cmd, json_t *kw, hgobj src)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    /*--------------------------*
     *      Get parameters
     *--------------------------*/
    const char *username = kw_get_str(gobj, kw, "username", "", 0);
    const char *password = kw_get_str(gobj, kw, "password", "", 0);
    if(empty_string(username)) {
        return msg_iev_build_response(
            gobj,
            -1,
            json_sprintf("What username?"),
            0,
            0,
            kw  // owned
        );
    }
    if(empty_string(password)) {
        return msg_iev_build_response(
            gobj,
            -1,
            json_sprintf("What password?"),
            0,
            0,
            kw  // owned
        );
    }

    /*-----------------------------*
     *  Get username
     *-----------------------------*/
    json_t *user = gobj_get_node(
        priv->gobj_treedb_mqtt_broker,
        "users",
        json_pack("{s:s}",
            "id", username
        ),
        0,
        src
    );
    if(!user) {
        return msg_iev_build_response(gobj,
            -1,
            json_sprintf("User not exist: %s", username),
            0,
            0,
            kw  // owned
        );
    }

    int authorization = check_password(gobj, username, password);

    JSON_DECREF(user)
    return msg_iev_build_response( // TODO TEST
        gobj,
        0,
        json_sprintf("Password match: %s", authorization==0?"Yes":"No"),
        0,
        0, // owned
        kw  // owned
    );

}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE json_t *cmd_set_user_passw(hgobj gobj, const char *cmd, json_t *kw, hgobj src)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    /*--------------------------*
     *      Get parameters
     *--------------------------*/
    const char *username = kw_get_str(gobj, kw, "username", "", 0);
    const char *password = kw_get_str(gobj, kw, "password", "", 0);
    if(empty_string(username)) {
        return msg_iev_build_response(
            gobj,
            -1,
            json_sprintf("What username?"),
            0,
            0,
            kw  // owned
        );
    }
    if(empty_string(password)) {
        return msg_iev_build_response(
            gobj,
            -1,
            json_sprintf("What password?"),
            0,
            0,
            kw  // owned
        );
    }

    /*-----------------------------*
     *  Get username
     *-----------------------------*/
    json_t *user = gobj_get_node(
        priv->gobj_treedb_mqtt_broker,
        "users",
        json_pack("{s:s}",
            "id", username
        ),
        0,
        src
    );
    if(!user) {
        return msg_iev_build_response(gobj,
            -1,
            json_sprintf("User not exist: %s", username),
            0,
            0,
            kw  // owned
        );
    }

    int hashIterations = (int)kw_get_int(
        gobj,
        kw,
        "hashIterations",
        gobj_read_integer_attr(gobj, "hashIterations"),
        KW_WILD_NUMBER
    );
    const char *algorithm = kw_get_str(
        gobj,
        kw,
        "algorithm",
        gobj_read_str_attr(gobj, "algorithm"),
        0
    );

    /*-----------------------------*
     *      Update user
     *-----------------------------*/
    json_t *credentials = hash_password(
        gobj,
        password,
        algorithm,
        hashIterations
    );
    if(!credentials) {
        return msg_iev_build_response(
            gobj,
            -1,
            json_sprintf("Error creating credentials: %s", gobj_log_last_message()),
            0,
            0,
            kw  // owned
        );
    }
    JSON_DECREF(user)

    user = gobj_update_node(
        priv->gobj_treedb_mqtt_broker,
        "users",
        json_pack("{s:s, s:o}",
            "id", username,
            "credentials", credentials
        ),
        0,
        gobj
    );
    if(!user) {
        return msg_iev_build_response(
            gobj,
            -1,
            json_sprintf("Cannot update user password: %s", gobj_log_last_message()),
            0,
            0,
            kw  // owned
        );
    }
    JSON_DECREF(user)

    return msg_iev_build_response(
        gobj,
        0,
        json_sprintf("Updated user password: %s", username),
        0,
        0, // owned
        kw  // owned
    );
}



                    /***************************
                     *      Local Methods
                     ***************************/




/***************************************************************************
 *  Constant-time comparison
 *      Always compares all bytes, takes constant time.
 *      ✅ Yes — resistant to timing attacks.
 ***************************************************************************/
PRIVATE int secure_eq(const uint8_t *a, const uint8_t *b, size_t n)
{
    if(!a || !b) {
        return -1;
    }
    unsigned int diff = 0;
    for(size_t i = 0; i < n; i++) {
        diff |= (unsigned int)(a[i] ^ b[i]);
    }
    return diff == 0 ? 0 : -1;
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE int gen_salt(hgobj gobj, uint8_t *salt, size_t salt_len)
{
#if defined(__linux__)
#if defined(CONFIG_HAVE_OPENSSL)
    if(RAND_bytes(salt, (int)salt_len) != 1) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_INTERNAL_ERROR,
            "msg",          "%s", "RAND_bytes() FAILED",
            NULL
        );
        return -1;
    }
#elif defined(CONFIG_HAVE_MBEDTLS)
#else
#error "No crypto library defined"
#endif
#endif
    return 0;
}

/***************************************************************************
 *  PBKDF2-HMAC with arbitrary digest
 *
 *  password    : NUL-terminated string
 *  salt        : salt bytes
 *  salt_len    : length of salt
 *  iterations  : cost factor (>=1)
 *  digest_name : e.g. "sha256", "sha3-512", "sm3"
 *  out_key     : output buffer
 *  out_len     : desired key length
 *
 *  Returns 0 on success, −1 on failure ***************************************************************************/
PRIVATE int pbkdf2_any(
    hgobj gobj,
    const char *password,
    const uint8_t *salt,
    size_t salt_len,
    unsigned int iterations,
    const char *digest_name,
    uint8_t *out_key,
    size_t out_len
) {
    int ret = 0;

#if defined(__linux__)
#if defined(CONFIG_HAVE_OPENSSL)

    EVP_MD *md = EVP_MD_fetch(NULL, digest_name, NULL);
    if(!md) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_INTERNAL_ERROR,
            "msg",          "%s", "Unable to get openssl digest",
            "digest",       "%s", digest_name,
            NULL
        );
        return -1;
    }

    int md_size = EVP_MD_get_size(md);
    if(md_size <= 0) { /* Should not happen for HMAC-capable digests */
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_INTERNAL_ERROR,
            "msg",          "%s", "EVP_MD_get_size() failed",
            "digest",       "%s", digest_name,
            NULL
        );
        EVP_MD_free(md);
        return -1;
    }

    if(md_size > out_len) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_INTERNAL_ERROR,
            "msg",          "%s", "out_key size too small",
            "digest",       "%s", digest_name,
            "md_size",      "%d", (int)md_size,
            "out_len",      "%d", (int)out_len,
            NULL
        );
        EVP_MD_free(md);
        return -1;
    }

    if(PKCS5_PBKDF2_HMAC(
        password, (int)strlen(password),
        salt, (int)salt_len,
        (int)iterations, md,
        (int)md_size, out_key
    ) != 1) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_INTERNAL_ERROR,
            "msg",          "%s", "PKCS5_PBKDF2_HMAC() failed",
            "digest",       "%s", digest_name,
            NULL
        );
        ret = -1;
    }

    EVP_MD_free(md);
    ret = md_size;

#elif defined(CONFIG_HAVE_MBEDTLS)
#else
#error "No crypto library defined"
#endif
#endif

    return ret;
}

/***************************************************************************
 *  Return

    "credentials" : [
        {
            "type": "password",
            "secretData": {
                "value": "???",
                "salt": "???=="
            },
            "credentialData" : {
                "hashIterations": 27500,
                "algorithm": "sha512",
                "additionalParameters": {
                }
            }
        }
    ]

 ***************************************************************************/
PRIVATE json_t *hash_password(
    hgobj gobj,
    const char *password,
    const char *digest,
    unsigned int iterations
)
{
    if(empty_string(digest)) {
        digest = "sha512";
    }
    if(iterations < 1) {
        iterations = 27500;
    }

    uint8_t salt[16];
    if(gen_salt(gobj, salt, sizeof(salt)) != 0) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_INTERNAL_ERROR,
            "msg",          "%s", "RAND_bytes() FAILED",
            NULL
        );
        return NULL;
    }

    uint8_t hash[EVP_MAX_MD_SIZE];

    int hash_len = pbkdf2_any(
        gobj,
        password,
        salt, sizeof(salt),
        iterations, digest,
        hash, sizeof(hash)
    );
    if(hash_len <= 0) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_INTERNAL_ERROR,
            "msg",          "%s", "pbkdf2_any() failed",
            "digest",       "%s", digest,
            NULL
        );
        return NULL;
    }

    gbuffer_t *gbuf_hash = gbuffer_string_to_base64((const char *)hash, hash_len);
    gbuffer_t *gbuf_salt = gbuffer_string_to_base64((const char *)salt, sizeof(salt));
    char *hash_b64 = gbuffer_cur_rd_pointer(gbuf_hash);
    char *salt_b64 = gbuffer_cur_rd_pointer(gbuf_salt);

    json_t *credential_list = json_array();
    json_t *credential = json_pack("{s:s, s:{s:s, s:s}, s:{s:I, s:s, s:{}}}",
        "type", "password",
        "secretData",
            "value", hash_b64,
            "salt", salt_b64,
        "credentialData",
            "hashIterations", (json_int_t)iterations,
            "algorithm", digest,
            "additionalParameters"
    );
    json_array_append_new(credential_list, credential);

    GBUFFER_DECREF(gbuf_hash);
    GBUFFER_DECREF(gbuf_salt);

    return credential_list;
}

/***************************************************************************
 *  Verify a PBKDF2-HMAC derived key matches the expected value
 *
 *  password     : NUL-terminated password string
 *  salt         : salt bytes
 *  salt_len     : length of salt
 *  iterations   : cost factor (>=1)
 *  digest_name  : e.g., "sha256", "sha3-512", "sm3"
 *  expected_dk  : expected derived key bytes
 *  expected_len : length of expected_dk (and of recomputed dk)
 *
 *  Returns 0 on match, -1 on error or mismatch
 ***************************************************************************/
PRIVATE int pbkdf2_verify_any(
    hgobj gobj,
    const char *password,
    const uint8_t *salt,
    size_t salt_len,
    unsigned int iterations,
    const char *digest,
    const uint8_t *expected_dk,
    size_t expected_len
)
{
    uint8_t hash[EVP_MAX_MD_SIZE];

    int hash_len = pbkdf2_any(
        gobj,
        password,
        salt, salt_len,
        iterations, digest,
        hash, sizeof(hash)
    );

    if(hash_len <= 0) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_INTERNAL_ERROR,
            "msg",          "%s", "pbkdf2_any() failed",
            "digest",       "%s", digest,
            NULL
        );
        return -1;
    }
    if(hash_len != expected_len) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_INTERNAL_ERROR,
            "msg",          "%s", "expected_len don't match",
            "digest",       "%s", digest,
            "hash_len",     "%d", (int)hash_len,
            "expected_len", "%d", (int)expected_len,
            NULL
        );
        return -1;
    }

    return secure_eq(hash, expected_dk, expected_len); /* 0 = match, -1 = mismatch/error */
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE int match_hash(
    hgobj gobj,
    const char *password,
    const char *hash_b64,
    const char *salt_b64,
    const char *digest,
    json_int_t iterations
)
{
    gbuffer_t *gbuf_hash = gbuffer_base64_to_string((const char *)hash_b64, strlen(hash_b64));
    gbuffer_t *gbuf_salt = gbuffer_base64_to_string((const char *)salt_b64, strlen(salt_b64));
    uint8_t *hash = gbuffer_cur_rd_pointer(gbuf_hash);
    uint8_t *salt = gbuffer_cur_rd_pointer(gbuf_salt);
    size_t hash_len = gbuffer_leftbytes(gbuf_hash);
    size_t salt_len = gbuffer_leftbytes(gbuf_salt);

    int ret = pbkdf2_verify_any(
        gobj,
        password,
        salt, salt_len,
        iterations, digest,
        hash, hash_len
    );
    GBUFFER_DECREF(gbuf_hash);
    GBUFFER_DECREF(gbuf_salt);

    return ret;
}

/***************************************************************************
 *  Return -2 if username is not authorized
 *  Return 0 if password matches
 ***************************************************************************/
PRIVATE int check_password(
    hgobj gobj,
    const char *username,
    const char *password
) {
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    // TODO a client_id can have permitted usernames in blank

    if(empty_string(username)) {
        gobj_log_warning(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_AUTH_ERROR,
            "msg",          "%s", "No username given to check password",
            NULL
        );
        return -2;
    }
    json_t *user = gobj_get_node(
        priv->gobj_treedb_mqtt_broker,
        "users",
        json_pack("{s:s}",
            "id", username
        ),
        0,
        gobj
    );
    if(!user) {
        gobj_log_warning(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_AUTH_ERROR,
            "msg",          "%s", "Username not exist",
            "username",     "%s", username,
            NULL
        );
        return -2;
    }

    json_t *credentials = kw_get_list(gobj, user, "credentials", 0, KW_REQUIRED);

    int idx; json_t *credential;
    json_array_foreach(credentials, idx, credential) {
        const char *hash_saved = kw_get_str(
            gobj,
            credential,
            "secretData`value",
            "",
            KW_REQUIRED
        );
        const char *salt = kw_get_str(
            gobj,
            credential,
            "secretData`salt",
            "",
            KW_REQUIRED
        );
        json_int_t hashIterations = kw_get_int(
            gobj,
            credential,
            "credentialData`hashIterations",
            0,
            KW_REQUIRED
        );
        const char *algorithm = kw_get_str(
            gobj,
            credential,
            "credentialData`algorithm",
            "",
            KW_REQUIRED
        );

        if(match_hash(
            gobj,
            password,
            hash_saved,
            salt,
            algorithm,
            hashIterations
        )==0) {
            gobj_log_info(gobj, 0,
                "function",     "%s", __FUNCTION__,
                "msgset",       "%s", MSGSET_INFO,
                "msg",          "%s", "Username authorized",
                "username",     "%s", username,
                NULL
            );
            JSON_DECREF(user)
            return 0;
        }
    }

    gobj_log_warning(gobj, 0,
        "function",     "%s", __FUNCTION__,
        "msgset",       "%s", MSGSET_AUTH_ERROR,
        "msg",          "%s", "Username not authorized",
        "username",     "%s", username,
        NULL
    );

    JSON_DECREF(user)
    return -2;
}




                    /***************************
                     *      Actions
                     ***************************/




/***************************************************************************
 *  Identity_card on from
 *      mqtt clients (__input_side__)
 *
    {
        "client_id": "DVES_40AC66",
        "assigned_id": false,       #^^ if assigned_id is true the client_id is temporary.
        "username": "DVES_USER",
        "password": "DVES_PASS",
        "clean_start": true,
        "protocol_version": 2,
        "protocol_name": "MQTT",
        "keepalive": 30,
        "session_expiry_interval": 0,
        "max_qos": 2,

        "will": true,
        "will_retain": true,
        "will_qos": 1,
        "will_topic": "tele/tasmota_40AC66/LWT",    #^^ these will fields are optionals
        "will_delay_interval": 0,
        "will_expiry_interval": 0,
        "gbuffer": 95091873745312,  #^^ it contents the will payload

        "__temp__": {
            "channel": "input-1",
            "channel_gobj": 95091872991280
        }
    }
 *
 ***************************************************************************/
PRIVATE int ac_on_open(hgobj gobj, const char *event, json_t *kw, hgobj src)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    if(src != priv->gobj_input_side) {
         gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_INTERNAL_ERROR,
            "msg",          "%s", "on_open NOT from input_size",
            "src",          "%s", gobj_full_name(src),
            NULL
        );
        KW_DECREF(kw);
        return -1;
    }

    if(gobj_trace_level(gobj) & TRACE_MESSAGES) {
        gobj_trace_json(
            gobj,
            kw, // not own
            "ON_OPEN %s", gobj_short_name(src)
        );
    }

    /*---------------------------------------------*
     *      Check user/password
     *---------------------------------------------*/
    const char *client_id = kw_get_str(gobj, kw, "client_id", "", KW_REQUIRED);
    const char *username = kw_get_str(gobj, kw, "username", "", KW_REQUIRED);
    const char *password = kw_get_str(gobj, kw, "password", "", KW_REQUIRED);

    int authorization = 0;
    if(priv->allow_anonymous) {
        username = "yuneta";
    } else {
        authorization = check_password(gobj, username, password);
    }

    if(authorization < 0) {
        KW_DECREF(kw);
        return authorization;
    }

    /*---------------------------------------------*
     *  MQTT Client ID <==> topic in Timeranger
     *---------------------------------------------*/
    BOOL clean_start = kw_get_bool(gobj, kw, "clean_start", 0, KW_REQUIRED);
    BOOL will = kw_get_bool(gobj, kw, "will", 0, KW_REQUIRED);
    if(will) {
        // TODO read the remain will fields
    }

    /*----------------------------------------------------------------*
     *  Open the topic (client_id) or create it if it doesn't exist
     *----------------------------------------------------------------*/
    json_t *jn_response = gobj_command(
        priv->gobj_tranger_clients,
        "open-topic",
        json_pack("{s:s, s:s}",
            "topic_name", client_id,
            "__username__", username
        ),
        gobj
    );

    int result = COMMAND_RESULT(gobj, jn_response);
    if(result < 0) {
        JSON_DECREF(jn_response)
        jn_response = gobj_command(
            priv->gobj_tranger_clients,
            "create-topic", // idempotent function
            json_pack("{s:s, s:s}",
                "topic_name", client_id,
                "__username__", username
            ),
            gobj
        );
        result = COMMAND_RESULT(gobj, jn_response);
    }
    if(result < 0) {
        const char *comment = COMMAND_COMMENT(gobj, jn_response);
        JSON_DECREF(jn_response)
        KW_DECREF(kw);
        return -1;
    }

    json_t *topic = COMMAND_DATA(gobj, jn_response);

    // TODO if session already exists with below conditions return 1!
    // if(priv->clean_start == FALSE && prev_session_expiry_interval > 0) {
    //     if(priv->protocol_version == mosq_p_mqtt311 || priv->protocol_version == mosq_p_mqtt5) {
    //         connect_ack |= 0x01;
    //          result = 1;
    //     }
    //     // copia client session TODO
    // }

    JSON_DECREF(jn_response)
    KW_DECREF(kw);
    return result;
}

/***************************************************************************
 *  Identity_card off from
 *      mqtt clients (__input_side__)
 ***************************************************************************/
PRIVATE int ac_on_close(hgobj gobj, const char *event, json_t *kw, hgobj src)
{
    // PRIVATE_DATA *priv = gobj_priv_data(gobj);

    if(gobj_trace_level(gobj) & TRACE_MESSAGES) {
        gobj_trace_json(
            gobj,
            kw, // not own
            "ON_CLOSE %s", gobj_short_name(src)
        );
    }

    // TODO do will job ?

    KW_DECREF(kw);
    return 0;
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE int ac_timeout(hgobj gobj, const char *event, json_t *kw, hgobj src)
{
//    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    // printf("Timeout\n");

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
    .mt_create                  = mt_create,
    .mt_destroy                 = mt_destroy,
    .mt_start                   = mt_start,
    .mt_stop                    = mt_stop,
    .mt_play                    = mt_play,
    .mt_pause                   = mt_pause,
    .mt_writing                 = mt_writing,
};

/*------------------------*
 *      GClass name
 *------------------------*/
GOBJ_DEFINE_GCLASS(C_MQTT_BROKER);

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
            "function", "%s", __FUNCTION__,
            "msgset",   "%s", MSGSET_INTERNAL_ERROR,
            "msg",      "%s", "GClass ALREADY created",
            "gclass",   "%s", gclass_name,
            NULL
        );
        return -1;
    }

    /*------------------------*
     *      States
     *------------------------*/
    ev_action_t st_idle[] = {
        {EV_ON_OPEN,                ac_on_open,              0},
        {EV_ON_CLOSE,               ac_on_close,             0},
        {EV_TREEDB_NODE_CREATED,    0,   0},
        {EV_TREEDB_NODE_UPDATED,    0,  0},
        {EV_TREEDB_NODE_DELETED,    0,  0},
        {EV_TIMEOUT,                ac_timeout,              0},
        {0,0,0}
    };

    states_t states[] = {
        {ST_IDLE, st_idle},
        {0, 0}
    };

    /*------------------------*
     *      Events
     *------------------------*/
    event_type_t event_types[] = {
        {EV_ON_OPEN,                0},
        {EV_ON_CLOSE,               0},
        {EV_TREEDB_NODE_CREATED,    0},
        {EV_TREEDB_NODE_UPDATED,    0},
        {EV_TREEDB_NODE_DELETED,    0},
        {EV_TIMEOUT,                0},
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
        0, // local methods
        tattr_desc,
        sizeof(PRIVATE_DATA),
        authz_table,
        command_table,
        s_user_trace_level,
        0 // gcflags
    );
    if(!__gclass__) {
        return -1;
    }

    return 0;
}

/***************************************************************************
 *              Public access
 ***************************************************************************/
PUBLIC int register_c_mqtt_broker(void)
{
    return create_gclass(C_MQTT_BROKER);
}
