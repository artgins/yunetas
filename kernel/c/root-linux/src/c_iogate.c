/***********************************************************************
 *          C_IOGATE.C
 *          Iogate GClass.
 *          Copyright (c) 2016 Niyamaka.
 *          All Rights Reserved.
***********************************************************************/
#include <string.h>
#include <inttypes.h>
#include "c_iogate.h"

/***************************************************************************
 *              Constants
 ***************************************************************************/
/*
 *  HACK Variables added by decoding url: 'schema' 'host' 'port' to __json_config_variables__
 *  HACK If name is empty, name=url.
 */

// protocol_gclass = GWebSocket
PRIVATE char client_config[]= "\
{                                               \n\
    'name': '(^^channel_name^^)',               \n\
    'gclass': 'Channel',                        \n\
    'zchilds': [                                \n\
        {                                       \n\
            'name': '(^^channel_name^^)',       \n\
            'gclass': '(^^protocol_gclass^^)',  \n\
            'kw': {                             \n\
                'kw_connex': {                  \n\
                    'urls':[                    \n\
                        '(^^url^^)'             \n\
                    ]                           \n\
                }                               \n\
            }                                   \n\
        }                                       \n\
    ]                                           \n\
}                                               \n\
";

// top_gclass = IEvent_srv
PRIVATE char server_config[]= "\
{                                                               \n\
    'name': '(^^channel_name^^)',                               \n\
    'gclass': 'TcpS0',                                          \n\
    'kw': {                                                     \n\
        'url': '(^^url^^)',                                     \n\
        'child_tree_filter': {                                  \n\
            'op': 'find',                                       \n\
            'kw': {                                             \n\
                '__prefix_gobj_name__': '(^^channel_name^^)-',  \n\
                '__gclass_name__': '(^^top_gclass^^)',          \n\
                '__disabled__': false,                          \n\
                'lHost': '(^^host^^)',                          \n\
                'lPort': '(^^port^^)'                           \n\
            }                                                   \n\
        }                                                       \n\
    }                                                           \n\
}                                                               \n\
";

// top_gclass = IEvent_srv
// protocol_gclass = GWebSocket
PRIVATE char clisrv_config[]= "\
{                                                               \n\
    'name': '(^^channel_name^^)-(^^index^^)',                   \n\
    'gclass': '(^^top_gclass^^)',                               \n\
    'kw': {                                                     \n\
    },                                                          \n\
    'zchilds': [                                                \n\
        {                                                       \n\
            'name': '(^^channel_name^^)-(^^index^^)',           \n\
            'gclass': 'Channel',                                \n\
            'kw': {                                             \n\
                'lHost': '(^^host^^)',                          \n\
                'lPort': '(^^port^^)'                           \n\
            },                                                  \n\
            'zchilds': [                                        \n\
                {                                               \n\
                    'name': '(^^channel_name^^)-(^^index^^)',   \n\
                    'gclass': '(^^protocol_gclass^^)',          \n\
                    'kw': {                                     \n\
                        'iamServer': true                       \n\
                    }                                           \n\
                }                                               \n\
            ]                                                   \n\
        }                                                       \n\
    ]                                                           \n\
}                                                               \n\
";

#define MAX_GATE_CONFIGS 30

typedef struct gate_config_s {
    const char *type;
    const char *config;
} gate_config_t;

PRIVATE gate_config_t gate_config_pool[MAX_GATE_CONFIGS+1] = {
{"client_gate",     client_config},
{"server_gate",     server_config},
{"clisrv_gate",     clisrv_config},
{0,0}
};

/***************************************************************************
 *              Structures
 ***************************************************************************/

/***************************************************************************
 *              Prototypes
 ***************************************************************************/
PRIVATE int channels_opened(hgobj gobj);
PRIVATE int trace_on_channels(hgobj gobj, const char *cmd, json_t *kw, hgobj src);
PRIVATE int trace_off_channels(hgobj gobj, const char *cmd, json_t *kw, hgobj src);
PRIVATE gate_config_t *get_tree_config_type(hgobj gobj, const char* type);

/***************************************************************************
 *              Resources
 ***************************************************************************/
PRIVATE sdata_desc_t tb_channels[] = {
/*-FIELD-type-----------name----------------flag------------------------resource----header----------fillsp--description---------*/
SDATADF (ASN_JSON,      "ids",              SDF_NOTACCESS,              0,          0,              0,      "List of id's to match."),
SDATADF (ASN_COUNTER64, "id",               SDF_PERSIST|SDF_PKEY,       0,          "Id",           8,      "Id."),
SDATADF (ASN_OCTET_STR, "type",             SDF_PERSIST|SDF_REQUIRED,   0,          "Channel Type", 20,     "Channel type."),
SDATADF (ASN_OCTET_STR, "channel_name",     SDF_PERSIST|SDF_REQUIRED,   0,          "Channel Name", 30,     "Channel name."),
SDATADF (ASN_OCTET_STR, "url",              SDF_PERSIST|SDF_WR,         0,          "Url",          30,     "Connection url."),
SDATADF (ASN_BOOLEAN,   "opened",           SDF_PERSIST|SDF_RD,         0,          "Opened",       10,     "Channel opened."),
SDATADF (ASN_BOOLEAN,   "disabled",         SDF_PERSIST|SDF_WR,         0,          "Disabled",     10,     "Channel disabled."),
SDATADF (ASN_BOOLEAN,   "static",           SDF_PERSIST,                0,          "Static",       10,     "Static channel."),
SDATADF (ASN_BOOLEAN,   "traced",           SDF_PERSIST|SDF_WR,         0,          "Traced",       8,      "Channel traced"),
SDATADF (ASN_OCTET_STR, "date",             SDF_PERSIST,                0,          "Date",         21,     "Creation date."),
SDATADF (ASN_OCTET_STR, "description",      SDF_PERSIST|SDF_WR,         0,          "Description",  20,     "Description."),
SDATADF (ASN_OCTET_STR, "top_gclass",       SDF_PERSIST|SDF_WR,         0,          "Top Gclass",   20,     "Top gclass."),
SDATADF (ASN_OCTET_STR, "protocol_gclass",  SDF_PERSIST|SDF_WR,         0,          "Protocol Gclass",20,   "Protocol gclass."),
SDATADF (ASN_UNSIGNED,  "idx",              SDF_PERSIST|SDF_WR,         0,          "Idx",          8,      "Channel index (TODO) ~ priority?"),
SDATADF (ASN_OCTET_STR, "zcontent",         SDF_PERSIST|SDF_WR,         0,          "Content",      10,     "The child tree json configuration."),
SDATADF (ASN_POINTER,   "channel_gobj",     SDF_VOLATIL|SDF_RD,         0,          "Channel gobj", 8,      "Channel gobj"),
SDATA_END()
};

/*
 *  Flag SDF_RESOURCE because is a searchable resource.
 */
PRIVATE sdata_desc_t tb_resources[] = {
/*-DB----type-----------name----------------flag--------------------schema--------------free_fn---------header-----------*/
SDATADB (ASN_ITER,      "channels",         SDF_RESOURCE,           tb_channels,    sdata_destroy,  "DGObjs"),
SDATA_END()
};

/***************************************************************************
 *          Data: config, public data, private data
 ***************************************************************************/

PRIVATE json_t *cmd_help(hgobj gobj, const char *cmd, json_t *kw, hgobj src);

PRIVATE json_t *cmd_add_channel(hgobj gobj, const char *cmd, json_t *kw, hgobj src);
PRIVATE json_t *cmd_delete_channel(hgobj gobj, const char *cmd, json_t *kw, hgobj src);
PRIVATE json_t *cmd_list_db(hgobj gobj, const char *cmd, json_t *kw, hgobj src);

PRIVATE json_t *cmd_view_channels(hgobj gobj, const char *cmd, json_t *kw, hgobj src);
PRIVATE json_t *cmd_enable_channels(hgobj gobj, const char *cmd, json_t *kw, hgobj src);
PRIVATE json_t *cmd_disable_channels(hgobj gobj, const char *cmd, json_t *kw, hgobj src);
PRIVATE json_t *cmd_drop_channels(hgobj gobj, const char *cmd, json_t *kw, hgobj src);
PRIVATE json_t *cmd_trace_on_channels(hgobj gobj, const char *cmd, json_t *kw, hgobj src);
PRIVATE json_t *cmd_trace_off_channels(hgobj gobj, const char *cmd, json_t *kw, hgobj src);
PRIVATE json_t *cmd_reset_stats_channels(hgobj gobj, const char *cmd, json_t *kw, hgobj src);

PRIVATE sdata_desc_t pm_help[] = {
/*-PM----type-----------name------------flag------------default-----description---------- */
SDATAPM (ASN_OCTET_STR, "cmd",          0,              0,          "command about you want help."),
SDATAPM (ASN_UNSIGNED,  "level",        0,              0,          "command search level in childs"),
SDATA_END()
};

PRIVATE const char *a_help[] = {"h", "?", 0};

PRIVATE sdata_desc_t pm_add_channel[] = {
/*-PM----type-----------name------------flag------------default-----description---------- */
SDATAPM (ASN_COUNTER64, "id",           0,              0,          "Id"),
SDATAPM (ASN_OCTET_STR, "channel_name", 0,              0,          "Channel name"),
SDATAPM (ASN_OCTET_STR, "url",          0,              0,          "Channel url"),
SDATAPM (ASN_OCTET_STR, "type",         0,              0,          "Type name (input_gate, output_gate)"),
SDATAPM (ASN_OCTET_STR, "top_gclass",   0,              0,          "Top gclass"),
SDATAPM (ASN_OCTET_STR, "protocol_gclass",0,            0,          "Protocol gclass"),
SDATAPM (ASN_UNSIGNED,  "idx",          0,              0,          "Channel index"),
SDATAPM (ASN_BOOLEAN,   "disabled",     0,              0,          "Channel disabled"),
SDATAPM (ASN_BOOLEAN,   "traced",       0,              0,          "Channel traced"),
SDATAPM (ASN_OCTET_STR, "description",  0,              0,          "Description"),
SDATAPM (ASN_OCTET_STR, "content64",    0,              0,          "content in base64"),
SDATA_END()
};
PRIVATE sdata_desc_t pm_delete_channel[] = {
/*-PM----type-----------name------------flag------------default-----description---------- */
SDATAPM (ASN_COUNTER64, "channel_id",   0,              0,          "Id"),
SDATAPM (ASN_OCTET_STR, "channel_name", 0,              0,          "configuration name"),
SDATAPM (ASN_OCTET_STR, "url",          0,              0,          "Channel url"),
SDATA_END()
};
PRIVATE sdata_desc_t pm_list_db[] = {
/*-PM----type-----------name------------flag------------default-----description---------- */
SDATAPM (ASN_COUNTER64, "channel_id",   0,              0,          "Id"),
SDATAPM (ASN_OCTET_STR, "channel_name", 0,              0,          "Channel name"),
SDATAPM (ASN_OCTET_STR, "url",          0,              0,          "Channel url"),
SDATAPM (ASN_OCTET_STR, "type",         0,              0,          "Type name (input_gate, output_gate)"),
SDATAPM (ASN_OCTET_STR, "top_gclass",   0,              0,          "Top gclass"),
SDATAPM (ASN_OCTET_STR, "protocol_gclass",0,            0,          "Protocol gclass"),
SDATAPM (ASN_BOOLEAN,   "disabled",     0,              0,          "Channel disabled"),
SDATAPM (ASN_BOOLEAN,   "traced",       0,              0,          "Channel traced"),
SDATAPM (ASN_OCTET_STR, "description",  0,              0,          "Description"),
SDATA_END()
};

PRIVATE sdata_desc_t pm_channel[] = {
/*-PM----type-----------name------------flag----------------default-----description---------- */
SDATAPM (ASN_OCTET_STR, "channel_name", 0,                  0,          "Channel name."),
SDATAPM (ASN_BOOLEAN,   "opened",    0,                     0,          "Channel opened"),
SDATA_END()
};

PRIVATE sdata_desc_t command_table[] = {
/*-CMD---type-----------name------------------------alias---items-----------json_fn-----------------description---------- */
SDATACM (ASN_SCHEMA,    "help",                     a_help, pm_help,        cmd_help,               "Available commands or help about a command."),
SDATACM (ASN_SCHEMA,    "",                         0,      0,              0,                      "\nDeploy\n-----------"),
SDATACM (ASN_SCHEMA,    "add-channel",              0,      pm_add_channel, cmd_add_channel,        "Add channel."),
SDATACM (ASN_SCHEMA,    "delete-channel",           0,      pm_delete_channel,cmd_delete_channel,   "Delete channel."),
SDATACM (ASN_SCHEMA,    "list-db",                  0,      pm_list_db,     cmd_list_db,            "List channel db."),
SDATACM (ASN_SCHEMA,    "",                         0,      0,              0,                      "\nOperation\n-----------"),
SDATACM (ASN_SCHEMA,    "view-channels",            0,      pm_channel,     cmd_view_channels,      "View channels."),
SDATACM (ASN_SCHEMA,    "enable-channel",           0,      pm_channel,     cmd_enable_channels,    "Enable channel."),
SDATACM (ASN_SCHEMA,    "disable-channel",          0,      pm_channel,     cmd_disable_channels,   "Disable channel."),
SDATACM (ASN_SCHEMA,    "drop-channel",             0,      pm_channel,     cmd_drop_channels,      "Drop channel."),
SDATACM (ASN_SCHEMA,    "trace-on-channel",         0,      pm_channel,     cmd_trace_on_channels, "Trace on channel."),
SDATACM (ASN_SCHEMA,    "trace-off-channel",        0,      pm_channel,     cmd_trace_off_channels,"Trace off channel."),
SDATACM (ASN_SCHEMA,    "reset-stats-channel",      0,      pm_channel,     cmd_reset_stats_channels,"Reset stats of channel."),
SDATA_END()
};

/*---------------------------------------------*
 *      Attributes - order affect to oid's
 *---------------------------------------------*/
PRIVATE sdata_desc_t tattr_desc[] = {
/*-ATTR-type------------name----------------flag----------------default---------description---------- */
SDATA (ASN_BOOLEAN,     "persistent_channels",SDF_RD,           0,              "Set True to do channels persistent (in sqlite database)."),
SDATA (ASN_BOOLEAN,     "local_store",      SDF_RD,             0,              "Set True to have the persistent_channels in local store (in /yuneta/realms instead of /yuneta/store)"),

SDATA (ASN_INTEGER,     "send_type",        SDF_RD,             0,              "Send type: 0 one dst, 1 all destinations"),
SDATA (ASN_INTEGER,     "timeout",          SDF_RD,             1*1000,         "Timeout"),
SDATA (ASN_COUNTER64,   "txMsgs",           SDF_RD,             0,              "Messages transmitted"),
SDATA (ASN_COUNTER64,   "rxMsgs",           SDF_RD,             0,              "Messages received"),
SDATA (ASN_COUNTER64,   "txMsgsec",         SDF_RD,             0,              "Messages by second"),
SDATA (ASN_COUNTER64,   "rxMsgsec",         SDF_RD,             0,              "Messages by second"),
SDATA (ASN_COUNTER64,   "maxtxMsgsec",      SDF_WR,             0,              "Max Messages by second"),
SDATA (ASN_COUNTER64,   "maxrxMsgsec",      SDF_WR,             0,              "Max Messages by second"),

SDATA (ASN_OCTET_STR,   "last_channel",     SDF_RD,             0,              "Last channel name used to send"),

SDATA (ASN_POINTER,     "user_data",        0,                  0,              "user data"),
SDATA (ASN_POINTER,     "user_data2",       0,                  0,              "more user data"),
SDATA (ASN_POINTER,     "subscriber",       0,                  0,              "subscriber of output-events. Not a child gobj."),
SDATA_END()
};

/*---------------------------------------------*
 *      GClass trace levels
 *---------------------------------------------*/
enum {
    TRACE_CONNECTION = 0x0001,
    TRACE_MESSAGES    = 0x0002,
};
PRIVATE const trace_level_t s_user_trace_level[16] = {
{"connection",      "Trace connections of iogates"},
{"messages",        "Trace messages of iogates"},
{0, 0},
};

/*---------------------------------------------*
 *              Private data
 *---------------------------------------------*/
typedef struct _PRIVATE_DATA {
    hgobj resource;
    hgobj timer;
    int32_t timeout;

    hgobj subscriber;
    send_type_t send_type;
    uint64_t *ptxMsgs;
    uint64_t *prxMsgs;
    uint64_t last_txMsgs;
    uint64_t last_rxMsgs;
    uint64_t last_ms;
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

    priv->ptxMsgs = gobj_danger_attr_ptr(gobj, "txMsgs");
    priv->prxMsgs = gobj_danger_attr_ptr(gobj, "rxMsgs");
    priv->timer = gobj_create("", GCLASS_TIMER, 0, gobj);

    BOOL persistent_channels = gobj_read_bool_attr(gobj, "persistent_channels");

    /*
     *  Create resources. Persistent if set.
     */
    json_t *kw_resource = json_pack("{s:I}",
        "tb_resources", (json_int_t)(size_t)tb_resources
    );

    /*
     *  A db file for each iogate gobj. Best for remove individual databases.
     *  Cons: only one table by db file.
     */
    char database[256]={0};
    if(persistent_channels) {
        snprintf(database, sizeof(database), "iogate^%s.db", gobj_name(gobj));
        json_t *kw_resource2 = json_pack("{s:s, s:s, s:b}",
            "service", "iogates",
            "database", database,
            "local_store", 1    // db-file in realms/..yuno../store
        );
        json_object_update(kw_resource, kw_resource2);
        JSON_DECREF(kw_resource2);
    }

    char rc_name[80];
    snprintf(rc_name, sizeof(rc_name), "rc_%s", gobj_name(gobj));
    priv->resource = gobj_create( // WARNING before was gobj_create_unique(). new in version 3.2.4
        rc_name,
        GCLASS_RESOURCE,
        kw_resource,
        gobj
    );

    /*
     *  SERVICE subscription model
     */
    hgobj subscriber = (hgobj)gobj_read_pointer_attr(gobj, "subscriber");
    if(subscriber) {
        gobj_subscribe_event(gobj, NULL, NULL, subscriber);
    }

    /*
     *  Do copy of heavy used parameters, for quick access.
     *  HACK The writable attributes must be repeated in mt_writing method.
     */
    SET_PRIV(timeout,                   gobj_read_int32_attr)
    SET_PRIV(send_type,                 gobj_read_int32_attr)
    SET_PRIV(subscriber,                gobj_read_pointer_attr)
}

/***************************************************************************
 *      Framework Method writing
 ***************************************************************************/
PRIVATE void mt_writing(hgobj gobj, const char *path)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    IF_EQ_SET_PRIV(timeout,             gobj_read_int32_attr)
    ELIF_EQ_SET_PRIV(subscriber,            gobj_read_pointer_attr)
    END_EQ_SET_PRIV()
}

/***************************************************************************
 *      Framework Method start
 ***************************************************************************/
PRIVATE int mt_start(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);
    char *resource = "channels";

    if(priv->resource) {
        gobj_start(priv->resource);
    }
    gobj_start(priv->timer);
    set_timeout_periodic(priv->timer, priv->timeout);

    /*-----------------------------------------------*
     *  Load static channels
     *  defined in config-file or hardcoded-in-main
     *-----------------------------------------------*/
    dl_list_t *dl_childs = gobj_match_childs_tree_by_strict_gclass(gobj, "Channel");
    if(rc_iter_size(dl_childs)) {
        hgobj child; rc_instance_t *i_hs;
        i_hs = rc_first_instance(dl_childs, (rc_resource_t **)&child);
        while(i_hs) {
            /*--------------------------------------------*
             *  Firstly check if it exists.
             *  Hard-coded channels cannot be repeated.
             *--------------------------------------------*/
            const char *channel_name = gobj_name(child);
            json_t *kw_rc = json_pack("{s:s}",
                "channel_name", channel_name
            );
            dl_list_t *iter = (dl_list_t *)gobj_list_resource(priv->resource, resource, kw_rc, 0);
            if(rc_iter_size(iter) == 0) {
                kw_rc = json_pack("{s:s, s:s, s:s, s:b, s:b, s:b, s:s, s:s, s:s, s:s, s:I, s:I}",
                    "type", "client_gate",
                    "channel_name", channel_name,
                    "url", "",
                    "disabled", 0,
                    "static", 1,
                    "traced", gobj_read_uint32_attr(child, "__trace_level__")?1:0,
                    "date", "",
                    "description", "Hard-coded channel",
                    "top_gclass", "",
                    "protocol_gclass", "",
                    "idx", (json_int_t)0,
                    "channel_gobj", (json_int_t)(size_t)child
                );
                hsdata hs = gobj_create_resource(priv->resource, resource, kw_rc, 0);
                gobj_write_pointer_attr(child, "user_data2", hs);
            } else if(rc_iter_size(iter) == 1) {
                // TODO se ignoran los disabled y trace_level de los static channel.
                hsdata hs;
                rc_first_instance(iter, (rc_resource_t **)&hs);
                sdata_write_pointer(hs, "channel_gobj", child);
                gobj_write_pointer_attr(child, "user_data2", hs);
            } else {
                log_error(0,
                    "gobj",         "%s", gobj_full_name(gobj),
                    "function",     "%s", __FUNCTION__,
                    "msgset",       "%s", MSGSET_INTERNAL_ERROR,
                    "msg",          "%s", "hard-coded channel: TOO MANY instances",
                    "channel_name", "%s", channel_name,
                    NULL
                );
            }
            rc_free_iter(iter, TRUE, 0);

            i_hs = rc_next_instance(i_hs, (rc_resource_t **)&child);
        }
    }
    rc_free_iter(dl_childs, TRUE, 0);

    /*-------------------------*
     *  Load dynamic channels
     *-------------------------*/
    dl_list_t *iter = (dl_list_t *)gobj_list_resource(priv->resource, resource, 0, 0);

    hsdata hs_channel; rc_instance_t *i_hs;
    i_hs = rc_first_instance(iter, (rc_resource_t **)&hs_channel);
    while(i_hs) {
        const char *channel_name = sdata_read_str(hs_channel, "channel_name");
        const char *url = sdata_read_str(hs_channel, "url");
        const char *top_gclass = sdata_read_str(hs_channel, "top_gclass");
        const char *protocol_gclass = sdata_read_str(hs_channel, "protocol_gclass");
        uint32_t idx = sdata_read_uint32(hs_channel, "idx");

        const char *tree_config = sdata_read_str(hs_channel, "zcontent");
        BOOL channel_static = sdata_read_bool(hs_channel, "static");
        hgobj channel_gobj = sdata_read_pointer(hs_channel, "channel_gobj");
        if(!channel_static && !channel_gobj) {
            /*------------------------------------------------*
             *      Create the dynamic tree gobj.
             *------------------------------------------------*/
            char schema[20], host[120], port[40];
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

            json_t * jn_config_variables = json_pack("{s:{s:s, s:s, s:s, s:s, s:I, s:s, s:s, s:s}}",
                "__json_config_variables__",
                "channel_name", channel_name,
                "url", url,
                "top_gclass", top_gclass,
                "protocol_gclass", protocol_gclass,
                "idx", (json_int_t)idx,
                "schema", schema,
                "host", host,
                "port", port
            );

            char *sjson_config_variables = json2str(jn_config_variables);
            JSON_DECREF(jn_config_variables);

            channel_gobj = gobj_create_tree(
                gobj,
                tree_config,
                sjson_config_variables,
                0,  //"EV_ON_SETUP",
                0   //"EV_ON_SETUP_COMPLETE"
            );
            gbmem_free(sjson_config_variables);

            sdata_write_pointer(hs_channel, "channel_gobj", channel_gobj);
            gobj_write_pointer_attr(channel_gobj, "user_data2", hs_channel);

            /*
             *  Start if not disabled
             */
            if(!sdata_read_bool(hs_channel, "disabled")) {
                gobj_start_tree(channel_gobj);
            }
        }
        i_hs = rc_next_instance(i_hs, (rc_resource_t **)&hs_channel);
    }
    rc_free_iter(iter, TRUE, 0);


    return 0;
}

/***************************************************************************
 *      Framework Method stop
 ***************************************************************************/
PRIVATE int mt_stop(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    if(priv->resource) {
        gobj_stop(priv->resource);
    }
    clear_timeout(priv->timer);
    gobj_stop(priv->timer);
    gobj_stop_childs(gobj);
    return 0;
}

/***************************************************************************
 *      Framework Method stats
 ***************************************************************************/
PRIVATE json_t *mt_stats(hgobj gobj, const char *stats, json_t *kw, hgobj src)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    if(stats && strcmp(stats, "__reset__")==0) {
        (*priv->ptxMsgs) = 0;
        (*priv->prxMsgs) = 0;
        gobj_write_uint64_attr(gobj, "txMsgsec", 0);
        gobj_write_uint64_attr(gobj, "rxMsgsec", 0);
        gobj_write_uint64_attr(gobj, "maxtxMsgsec", 0);
        gobj_write_uint64_attr(gobj, "maxrxMsgsec", 0);

        /*
         *  Reset stats of channels too
         */
        dl_list_t *dl_childs = gobj_match_childs_tree_by_strict_gclass(gobj, "Channel");
        hgobj child; rc_instance_t *i_hs;
        i_hs = rc_first_instance(dl_childs, (rc_resource_t **)&child);
        while(i_hs) {
            KW_INCREF(kw);
            json_t *jn_child_stats = gobj_stats(child, stats, kw, gobj);
            JSON_DECREF(jn_child_stats);
            i_hs = rc_next_instance(i_hs, (rc_resource_t **)&child);
        }
        rc_free_iter(dl_childs, TRUE, 0);
    }

    json_t *jn_stats = json_object();

    uint64_t txMsgsec = gobj_read_uint64_attr(gobj, "txMsgsec");
    uint64_t rxMsgsec = gobj_read_uint64_attr(gobj, "rxMsgsec");
    uint64_t maxtxMsgsec = gobj_read_uint64_attr(gobj, "maxtxMsgsec");
    uint64_t maxrxMsgsec = gobj_read_uint64_attr(gobj, "maxrxMsgsec");
    int opened = channels_opened(gobj);

    json_object_set_new(
        jn_stats,
        "opened",
        json_integer(opened)
    );
    json_object_set_new(
        jn_stats,
        "txMsgs",
        json_integer(*(priv->ptxMsgs))
    );
    json_object_set_new(
        jn_stats,
        "rxMsgs",
        json_integer(*(priv->prxMsgs))
    );
    json_object_set_new(
        jn_stats,
        "txMsgsec",
        json_integer(txMsgsec)
    );
    json_object_set_new(
        jn_stats,
        "rxMsgsec",
        json_integer(rxMsgsec)
    );
    json_object_set_new(
        jn_stats,
        "maxtxMsgsec",
        json_integer(maxtxMsgsec)
    );
    json_object_set_new(
        jn_stats,
        "maxrxMsgsec",
        json_integer(maxrxMsgsec)
    );
    append_yuno_metadata(gobj, jn_stats, stats);

    return msg_iev_build_webix(
        gobj,
        0,
        0,
        0,
        jn_stats, // owned
        kw  // owned
    );
}

/***************************************************************************
 *      Framework Method trace_on
 ***************************************************************************/
PRIVATE int mt_trace_on(hgobj gobj, const char *level, json_t *kw)
{
    return trace_on_channels(gobj, "trace_on", kw, gobj);
}

/***************************************************************************
 *      Framework Method trace_off
 ***************************************************************************/
PRIVATE int mt_trace_off(hgobj gobj, const char *level, json_t *kw)
{
    return trace_off_channels(gobj, "trace_off", kw, gobj);
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
    return msg_iev_build_webix(
        gobj,
        0,
        jn_resp,
        0,
        0,
        kw  // owned
    );
}

/***************************************************************************
 *  command
 *  Add a dynamic gobj has sense only in **client** gobjs, not servers.
 ***************************************************************************/
PRIVATE json_t *cmd_add_channel(hgobj gobj, const char *cmd, json_t *kw, hgobj src)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);
    char *resource = "channels";

    if(1) {
        return msg_iev_build_webix(
            gobj,
            -1,
            json_sprintf("add channel not available"),
            0,
            0,
            kw  // owned
        );
    }

    if(!priv->resource) {
        return msg_iev_build_webix(gobj,
            -1,
            json_sprintf("Channel resource not in use."),
            0,
            0,
            kw  // owned
        );
    }

    json_int_t id = atoi(kw_get_str(kw, "channel_id", "", 0));
    const char *channel_name = kw_get_str(kw, "channel_name", "", 0);
    const char *url = kw_get_str(kw, "url", "", 0);

    /*------------------------------------------------*
     *      Check required parameters
     *------------------------------------------------*/
    if(empty_string(url)) {
        return msg_iev_build_webix(gobj,
            -1,
            json_sprintf(
                "What url?"
            ),
            0,
            0,
            kw  // owned
        );
    }
    char schema[20], host[120], port[40];
    int r = parse_http_url(url, schema, sizeof(schema), host, sizeof(host), port, sizeof(port), FALSE);
    if(r<0) {
        return msg_iev_build_webix(gobj,
            -1,
            json_sprintf(
                "Bad url: '%s'", url
            ),
            0,
            0,
            kw  // owned
        );
    }
    json_object_set_new(
        kw,
        "schema",
        json_string(schema)
    );
    json_object_set_new(
        kw,
        "host",
        json_string(host)
    );
    json_object_set_new(
        kw,
        "port",
        json_string(port)
    );

    if(empty_string(channel_name)) {
        // Use the url as name
        json_object_set_new(kw, "channel_name", json_string(url));
    }
    if(!kw_has_key(kw, "type")) {
        json_object_set_new(kw, "type", json_string("client_gate"));
    }
    if(!kw_has_key(kw, "top_gclass")) {
        json_object_set_new(kw, "top_gclass", json_string("IEvent_srv"));
    }
    if(!kw_has_key(kw, "protocol_gclass")) {
        json_object_set_new(kw, "protocol_gclass", json_string("GWebSocket"));
    }

    /*------------------------------------------------*
     *  Get content in base64 and decode
     *------------------------------------------------*/
    GBUFFER *gbuf_content = 0;
    const char *content64 = kw_get_str(kw, "content64", "", 0);
    if(!empty_string(content64)) {
        gbuf_content = gbuf_decodebase64string(content64);
        json_object_set_new(
            kw,
            "zcontent",
            json_string(gbuf_cur_rd_pointer(gbuf_content))
        );
        GBUF_DECREF(gbuf_content);

    } else {
        /*
         *  Use default dgobj tree
         */
        const char *type = kw_get_str(kw, "type", "", 0);
        gate_config_t *gate_config = get_tree_config_type(gobj, type);
        if(!gate_config) {
            return msg_iev_build_webix(gobj,
                -1,
                json_sprintf(
                    "Bad config type: '%s'", type
                ),
                0,
                0,
                kw  // owned
            );
        }

        char *dgobj_tree = gbmem_strdup(client_config);
        helper_quote2doublequote(dgobj_tree);
        json_object_set_new(
            kw,
            "zcontent",
            json_string(dgobj_tree)
        );
        gbmem_free(dgobj_tree);
    }

    /*------------------------------------------------*
     *      Create record
     *------------------------------------------------*/
    char current_date[22];
    current_timestamp(current_date, sizeof(current_date));  // "CCYY/MM/DD hh:mm:ss"
    json_object_set_new(
        kw,
        "date",
        json_string(current_date)
    );
    if(id) {
        json_object_set_new(
            kw,
            "id",
            json_integer(id)
        );
    }

    KW_INCREF(kw);
    hsdata hs = gobj_create_resource(priv->resource, resource, kw, 0);
    if(!hs) {
        return msg_iev_build_webix(
            gobj,
            -1,
            json_sprintf("Cannot create resource."),
            0,
            0,
            kw  // owned
        );
    }

    /*------------------------------------------------*
     *      Create the dynamic tree gobj.
     *------------------------------------------------*/
    json_t * jn_config_variables = json_pack("{s:O}",
        "__json_config_variables__", kw
    );
    char *sjson_config_variables = json2str(jn_config_variables);
    JSON_DECREF(jn_config_variables);

    const char *tree_config = kw_get_str(kw, "zcontent", "", 0);
    hgobj channel_gobj = gobj_create_tree(
        gobj,
        tree_config,
        sjson_config_variables,
        0,  //"EV_ON_SETUP",
        0   //"EV_ON_SETUP_COMPLETE"
    );
    gbmem_free(sjson_config_variables);

    if(!channel_gobj) {
        sdata_destroy(hs);
        return msg_iev_build_webix(gobj,
            -1,
            json_sprintf(
                "Cannot create the channel gobj."
            ),
            0,
            0,
            kw  // owned
        );
    }

    sdata_write_pointer(hs, "channel_gobj", channel_gobj);
    gobj_write_pointer_attr(channel_gobj, "user_data2", hs);
    //gobj_save_resource(priv->resource, hs);

    /*
     *  Start if not disabled
     */
    if(!sdata_read_bool(hs, "disabled")) {
        gobj_start_tree(channel_gobj);
    }

    /*
     *  Convert result in json
     */
    json_t *jn_data = json_array();
    json_array_append_new(jn_data, sdata2json(hs, SDF_PERSIST|SDF_VOLATIL, 0));

    /*
     *  Inform
     */
    json_t *webix = msg_iev_build_webix(
        gobj,
        0,
        0,
        0, //RESOURCE_WEBIX_SCHEMA(priv->resource, resource),
        jn_data, // owned
        kw  // owned
    );

    return webix;
}

/***************************************************************************
 *  command
 ***************************************************************************/
PRIVATE json_t *cmd_delete_channel(hgobj gobj, const char *cmd, json_t *kw, hgobj src)
{
    return msg_iev_build_webix(
        gobj,
        -1,
        json_sprintf("delete channel not available"),
        0,
        0,
        kw  // owned
    );
}

/***************************************************************************
 *  command
 ***************************************************************************/
PRIVATE json_t *cmd_list_db(hgobj gobj, const char *cmd, json_t *kw, hgobj src)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);
    const char *resource = "channels";

    if(!priv->resource) {
        return msg_iev_build_webix(gobj,
            -1,
            json_sprintf("Channel resource not in use."),
            0,
            0,
            kw  // owned
        );
    }

    /*
     *  Get a iter of matched resources
     */
    dl_list_t *iter = (dl_list_t *)gobj_list_resource(priv->resource, resource, kw_incref(kw), 0);

    /*
     *  Convert hsdata to json
     */
    json_t *jn_data = sdata_iter2json(iter, SDF_PERSIST|SDF_RESOURCE|SDF_VOLATIL, 0);

    /*
     *  Inform
     */
    json_t *webix = msg_iev_build_webix(
        gobj,
        0,
        json_sprintf("%s", cmd),
        0, //RESOURCE_WEBIX_SCHEMA(priv->resource, resource),
        jn_data, // owned
        kw  // owned
    );

    rc_free_iter(iter, TRUE, 0);

    return webix;
}

/***************************************************************************
 *  CLI usage
 ***************************************************************************/
#define FORMATT_VIEW_CHANNELS "%-22s %-6s %-12s %-12s %-8s %-8s %-8s %-8s %-20s %-20s"
#define FORMATD_VIEW_CHANNELS "%-22s %6d %12" PRIu64 " %12" PRIu64 " %8" PRIu64 " %8" PRIu64 " %8d %8d %20s %20s"
PRIVATE int add_child_to_data(hgobj gobj, json_t *jn_data, hgobj child)
{
    const char *channel_name = gobj_name(child);
    BOOL disabled = gobj_read_bool_attr(child, "__disabled__");
    BOOL opened = gobj_read_bool_attr(child, "opened");
    BOOL traced = gobj_read_uint32_attr(child, "__trace_level__");
    const char *sockname = "";
    const char *peername = "";
    //uint64_t txBytes = 0;
    //uint64_t rxBytes = 0;
    uint64_t txMsgs = 0;
    uint64_t rxMsgs = 0;
    uint64_t txMsgsec = 0;
    uint64_t rxMsgsec = 0;
    if(gobj_has_bottom_attr(child, "sockname")) {
        // Only when connected has this attrs.
        sockname = gobj_read_str_attr(child, "sockname");
        if(!sockname)
            sockname = "";
        peername = gobj_read_str_attr(child, "peername");
        if(!peername)
            peername = "";
        txMsgs = gobj_read_uint64_attr(child, "txMsgs");
        rxMsgs = gobj_read_uint64_attr(child, "rxMsgs");
        txMsgsec = gobj_read_uint64_attr(child, "txMsgsec");
        rxMsgsec = gobj_read_uint64_attr(child, "rxMsgsec");
    }
    return json_array_append_new(
        jn_data,
        json_sprintf(FORMATD_VIEW_CHANNELS,
            channel_name,
            opened,
            txMsgs,
            rxMsgs,
            txMsgsec,
            rxMsgsec,
            traced,
            disabled,
            sockname,
            peername
        )
    );
}
PRIVATE json_t *cmd_view_channels(hgobj gobj, const char *cmd, json_t *kw, hgobj src)
{
    json_t *jn_data = json_array();
    json_array_append_new(
        jn_data,
        json_sprintf(FORMATT_VIEW_CHANNELS,
            "  Channel name",
            "Opened",
            "Tx Msgs",
            "Rx Msgs",
            "Tx m/s",
            "Rx m/s",
            "Traced",
            "Disabled",
            "Sockname",
            "Peername"
        )
    );
    json_array_append_new(
        jn_data,
        json_sprintf(FORMATT_VIEW_CHANNELS,
            " =====================",
            "======",
            "============",
            "============",
            "========",
            "========",
            "========",
            "========",
            "====================",
            "===================="
        )
    );

    dl_list_t *dl_childs = gobj_match_childs_tree_by_strict_gclass(gobj, "Channel");
    const char *channel = kw_get_str(kw, "channel_name", 0, 0);
    BOOL opened = kw_get_bool(kw, "opened", 0, KW_WILD_NUMBER);
    if(empty_string(channel)) {
        hgobj child; rc_instance_t *i_hs;
        i_hs = rc_first_instance(dl_childs, (rc_resource_t **)&child);
        while(i_hs) {
            if(opened) {
                if(gobj_read_bool_attr(child, "opened")) {
                    add_child_to_data(gobj, jn_data, child);
                }
            } else {
                add_child_to_data(gobj, jn_data, child);
            }

            i_hs = rc_next_instance(i_hs, (rc_resource_t **)&child);
        }
    } else {
        dl_list_t *dl_childs2 = gobj_filter_childs_by_re_name(dl_childs, channel);
        hgobj child; rc_instance_t *i_hs;
        i_hs = rc_first_instance(dl_childs2, (rc_resource_t **)&child);
        while(i_hs) {
            if(opened) {
                if(gobj_read_bool_attr(child, "opened")) {
                    add_child_to_data(gobj, jn_data, child);
                }
            } else {
                add_child_to_data(gobj, jn_data, child);
            }

            i_hs = rc_next_instance(i_hs, (rc_resource_t **)&child);
        }
        rc_free_iter(dl_childs2, TRUE, 0);
    }

    rc_free_iter(dl_childs, TRUE, 0);

    jn_data = sort_json_list_by_string(jn_data);

    return msg_iev_build_webix(
        gobj,
        0,
        0,
        0,
        jn_data, // owned
        kw  // owned
    );
}

/***************************************************************************
 *  command
 ***************************************************************************/
PRIVATE json_t *cmd_drop_channels(hgobj gobj, const char *cmd, json_t *kw, hgobj src)
{
    int count = 0;
    const char *channel = kw_get_str(kw, "channel_name", 0, 0);
    dl_list_t *dl_childs = gobj_match_childs_tree_by_strict_gclass(gobj, "Channel");

    if(empty_string(channel)) {
        hgobj child; rc_instance_t *i_hs;
        i_hs = rc_first_instance(dl_childs, (rc_resource_t **)&child);
        while(i_hs) {
            if(gobj_is_running(child)) {
                trace_msg("send EV_DROP to %s", gobj_short_name(child));
                gobj_send_event(child, "EV_DROP", 0, gobj);
                count++;
            }
            i_hs = rc_next_instance(i_hs, (rc_resource_t **)&child);
        }
    } else {
        dl_list_t *dl_childs2 = gobj_filter_childs_by_re_name(dl_childs, channel);
        hgobj child; rc_instance_t *i_hs;
        i_hs = rc_first_instance(dl_childs2, (rc_resource_t **)&child);
        while(i_hs) {
            if(gobj_is_running(child)) {
                trace_msg("send EV_DROP to %s", gobj_short_name(child));
                gobj_send_event(child, "EV_DROP", 0, gobj);
                count++;
            }
            i_hs = rc_next_instance(i_hs, (rc_resource_t **)&child);
        }
        rc_free_iter(dl_childs2, TRUE, 0);
    }

    rc_free_iter(dl_childs, TRUE, 0);

    return cmd_view_channels(gobj, cmd, kw, src);
}

/***************************************************************************
 *  command
 ***************************************************************************/
PRIVATE json_t *cmd_enable_channels(hgobj gobj, const char *cmd, json_t *kw, hgobj src)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);
    char *resource = "channels";
    int count = 0;
    if(!priv->resource) {
        return msg_iev_build_webix(gobj,
            -1,
            json_sprintf("Channel resource not in use."),
            0,
            0,
            kw  // owned
        );
    }

    /*
     *  Get a iter of matched resources
     */
    dl_list_t *iter = (dl_list_t *)gobj_list_resource(priv->resource, resource, kw_incref(kw), 0);

    hgobj hs; rc_instance_t *i_hs;
    i_hs = rc_first_instance(iter, (rc_resource_t **)&hs);
    while(i_hs) {
        BOOL disabled = sdata_read_bool(hs, "disabled");
        if(disabled) {
            count++;
            hgobj channel_gobj = sdata_read_pointer(hs, "channel_gobj");
            sdata_write_bool(hs, "disabled", 0);
            gobj_enable(channel_gobj);
            //gobj_save_resource(priv->resource, hs);
        }

        i_hs = rc_next_instance(i_hs, (rc_resource_t **)&hs);
    }

    rc_free_iter(iter, TRUE, 0);

    // HACK dont't return iter list, the id will corrupt webix tables (id in webix is gobj full name).

    return cmd_view_channels(gobj, cmd, kw, src);
}

/***************************************************************************
 *  command
 ***************************************************************************/
PRIVATE json_t *cmd_disable_channels(hgobj gobj, const char *cmd, json_t *kw, hgobj src)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);
    char *resource = "channels";
    int count = 0;

    if(!priv->resource) {
        return msg_iev_build_webix(gobj,
            -1,
            json_sprintf("Channel resource not in use."),
            0,
            0,
            kw  // owned
        );
    }

    /*
     *  Get a iter of matched resources
     */
    dl_list_t *iter = (dl_list_t *)gobj_list_resource(priv->resource, resource, kw_incref(kw), 0);

    hgobj hs; rc_instance_t *i_hs;
    i_hs = rc_first_instance(iter, (rc_resource_t **)&hs);
    while(i_hs) {
        BOOL disabled = sdata_read_bool(hs, "disabled");
        if(!disabled) {
            count++;
            hgobj channel_gobj = sdata_read_pointer(hs, "channel_gobj");
            sdata_write_bool(hs, "disabled", 1);
            gobj_disable(channel_gobj);
            //gobj_save_resource(priv->resource, hs );
        }

        i_hs = rc_next_instance(i_hs, (rc_resource_t **)&hs);
    }

    rc_free_iter(iter, TRUE, 0);

    return cmd_view_channels(gobj, cmd, kw, src);
}

/***************************************************************************
 *  command
 ***************************************************************************/
PRIVATE json_t *cmd_trace_on_channels(hgobj gobj, const char *cmd, json_t *kw, hgobj src)
{
    trace_on_channels(gobj, cmd, kw_incref(kw), src);
    return cmd_view_channels(gobj, cmd, kw, src);
}

/***************************************************************************
 *  command
 ***************************************************************************/
PRIVATE json_t *cmd_trace_off_channels(hgobj gobj, const char *cmd, json_t *kw, hgobj src)
{
    trace_off_channels(gobj, cmd, kw_incref(kw), src);
    return cmd_view_channels(gobj, cmd, kw, src);
}

/***************************************************************************
 *  command
 ***************************************************************************/
PRIVATE json_t *cmd_reset_stats_channels(hgobj gobj, const char *cmd, json_t *kw, hgobj src)
{
    int count = 0;
    const char *channel = kw_get_str(kw, "channel_name", 0, 0);
    dl_list_t *dl_childs = gobj_match_childs_tree_by_strict_gclass(gobj, "Channel");

    if(empty_string(channel)) {
        hgobj child; rc_instance_t *i_hs;
        i_hs = rc_first_instance(dl_childs, (rc_resource_t **)&child);
        while(i_hs) {
            json_t *jn_stats = gobj_stats(child, "__reset__", 0, gobj);
            JSON_DECREF(jn_stats);
            count++;
            i_hs = rc_next_instance(i_hs, (rc_resource_t **)&child);
        }
    } else {
        dl_list_t *dl_childs2 = gobj_filter_childs_by_re_name(dl_childs, channel);
        hgobj child; rc_instance_t *i_hs;
        i_hs = rc_first_instance(dl_childs2, (rc_resource_t **)&child);
        while(i_hs) {
            json_t *jn_stats = gobj_stats(child, "__reset__", 0, gobj);
            JSON_DECREF(jn_stats);
            count++;
            i_hs = rc_next_instance(i_hs, (rc_resource_t **)&child);
        }
        rc_free_iter(dl_childs2, TRUE, 0);
    }

    rc_free_iter(dl_childs, TRUE, 0);

    return cmd_view_channels(gobj, cmd, kw, src);
}




            /***************************
             *      Local Methods
             ***************************/




/***************************************************************************
 *  command
 ***************************************************************************/
PRIVATE int trace_on_channels(hgobj gobj, const char *cmd, json_t *kw, hgobj src)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);
    char *resource = "channels";
    int count = 0;

    if(!priv->resource) {
        KW_DECREF(kw);
        return -1;
    }

    /*
     *  Get a iter of matched resources
     */
    dl_list_t *iter = (dl_list_t *)gobj_list_resource(
        priv->resource,
        resource,
        kw_incref(kw), // owned
        0
    );

    hgobj hs; rc_instance_t *i_hs;
    i_hs = rc_first_instance(iter, (rc_resource_t **)&hs);
    while(i_hs) {
        count++;
        hgobj channel_gobj = sdata_read_pointer(hs, "channel_gobj");
        sdata_write_bool(hs, "traced", 1);
        gobj_set_gobj_trace(channel_gobj, "", TRUE, 0); // TODO change by gobj_set_gclass_trace()?
        //gobj_save_resource(priv->resource, hs);

        i_hs = rc_next_instance(i_hs, (rc_resource_t **)&hs);
    }

    rc_free_iter(iter, TRUE, 0);

    KW_DECREF(kw);
    return 0;
}

/***************************************************************************
 *  command
 ***************************************************************************/
PRIVATE int trace_off_channels(hgobj gobj, const char* cmd, json_t* kw, hgobj src)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);
    char *resource = "channels";
    int count = 0;

    if(!priv->resource) {
        KW_DECREF(kw);
        return -1;
    }

    /*
     *  Get a iter of matched resources
     */
    dl_list_t *iter = (dl_list_t *)gobj_list_resource(
        priv->resource,
        resource,
        kw_incref(kw),
        0
    );

    hgobj hs; rc_instance_t *i_hs;
    i_hs = rc_first_instance(iter, (rc_resource_t **)&hs);
    while(i_hs) {
        count++;
        hgobj channel_gobj = sdata_read_pointer(hs, "channel_gobj");
        sdata_write_bool(hs, "traced", 0);
        gobj_set_gobj_trace(channel_gobj, "", FALSE, 0);
        //gobj_save_resource(priv->resource, hs);

        i_hs = rc_next_instance(i_hs, (rc_resource_t **)&hs);
    }

    rc_free_iter(iter, TRUE, 0);

    KW_DECREF(kw);
    return 0;
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE gate_config_t *get_tree_config_type(hgobj gobj, const char *type)

{
    for(int i=0; gate_config_pool[i].type!=0; i++) {
        if(strcmp(gate_config_pool[i].type, type)==0) {
            return &gate_config_pool[i];
        }
    }

    return 0;
}

/***************************************************************************
 *  Filter channels
 ***************************************************************************/
PRIVATE hgobj get_next_destination(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);
    char *resource = "channels";

    if(!priv->resource) {
        return 0;
    }

    hgobj channel_gobj = 0;

    /*
     *  Get a iter of matched resources
     */
    const char *last_channel = gobj_read_str_attr(gobj, "last_channel");
    dl_list_t *iter = (dl_list_t *)gobj_list_resource(priv->resource, resource, 0, 0);

    json_t *jn_filter = json_pack("{s:b, s:b}",
        "opened", 1,
        "disabled", 0
    );
    dl_list_t *iter_open = sdata_iter_match(iter, -1, 0, jn_filter, 0);

    hsdata hs=0; rc_instance_t* i_hs=0;
    int size = dl_size(iter_open);
    if(!empty_string(last_channel)) {
        json_t *jn_filter2 = json_pack("{s:s}",
            "channel_name", last_channel
        );
        hs = sdata_iter_find_one(iter_open, jn_filter2, &i_hs);
    }

    while(size > 0) {
        if(!i_hs) {
            i_hs = rc_first_instance(iter_open, (rc_resource_t **)&hs); // first time
        } else {
            i_hs = rc_next_instance(i_hs, (rc_resource_t **)&hs);
            if(!i_hs) {
                i_hs = rc_first_instance(iter_open, (rc_resource_t **)&hs); // turns
            }
        }
        if(i_hs) {
            channel_gobj = sdata_read_pointer(hs, "channel_gobj");
            if(channel_gobj) {
                const char *channel_name = sdata_read_str(hs, "channel_name");
                gobj_write_str_attr(gobj, "last_channel", channel_name);
                break;
            }
        }
        size--;
    }
    rc_free_iter(iter_open, TRUE, 0);
    rc_free_iter(iter, TRUE, 0);

    return channel_gobj;
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE int send_one_rotate(hgobj gobj, const char *event, json_t *kw, hgobj src)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    const char *channel = kw_get_str(kw, "__temp__`channel", "", 0);
    hgobj channel_gobj = (hgobj)(size_t)kw_get_int(kw, "__temp__`channel_gobj", 0, 0);
    if(!channel_gobj) {
        if(!empty_string(channel)) {
            channel_gobj = gobj_child_by_name(gobj, channel, 0);
        } else {
            channel_gobj = get_next_destination(gobj);
        }
        if(!channel_gobj) {
            static int repeated = 0;

            if(repeated%1000 == 0) {
                log_error(0,
                    "gobj",         "%s", gobj_full_name(gobj),
                    "function",     "%s", __FUNCTION__,
                    "msgset",       "%s", MSGSET_PARAMETER_ERROR,
                    "msg",          "%s", "No channel FOUND to send",
                    "channel",      "%s", channel?channel:"???",
                    "event",        "%s", event,
                    "repeated",     "%d", repeated*1000,
                    NULL
                );
            }
            KW_DECREF(kw);
            return -1;
        }
    }

    if(gobj_trace_level(gobj) & TRACE_MESSAGES) {
        GBUFFER *gbuf = (GBUFFER *)(size_t)kw_get_int(kw, "gbuffer", 0, 0);
        if(gbuf) {
            log_debug_gbuf(
                LOG_DUMP_OUTPUT,
                gbuf, // not own
                "GBUFFER %s ==> %s",
                gobj_short_name(gobj),
                gobj_short_name(channel_gobj)
            );
        } else {
            log_debug_json(
                LOG_DUMP_OUTPUT,
                kw, // not own,
                "KW %s ==> %s",
                gobj_short_name(gobj),
                gobj_short_name(channel_gobj)
            );
        }
    }

    int ret = gobj_send_event(channel_gobj, event, kw, gobj); // reuse kw
    if(ret == 0) {
        (*priv->ptxMsgs)++;
    }

    return ret;
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE int send_all(hgobj gobj, const char *event, json_t *kw, hgobj src)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);
    char *resource = "channels";

    if(!priv->resource) {
        return 0;
    }

    hgobj channel_gobj = 0;

    /*
     *  Get a iter of matched resources
     */
    dl_list_t *iter = (dl_list_t *)gobj_list_resource(priv->resource, resource, 0, 0);

    json_t *jn_filter = json_pack("{s:b, s:b}",
        "opened", 1,
        "disabled", 0
    );
    dl_list_t *iter_open = sdata_iter_match(iter, -1, 0, jn_filter, 0);

    int some = 0;
    hsdata hs=0; rc_instance_t* i_hs=0;
    i_hs = rc_first_instance(iter_open, (rc_resource_t **)&hs); // first time
    while(i_hs) {
        channel_gobj = sdata_read_pointer(hs, "channel_gobj");
        if(channel_gobj) {
            if(gobj_trace_level(gobj) & TRACE_MESSAGES) {
                GBUFFER *gbuf = (GBUFFER *)(size_t)kw_get_int(kw, "gbuffer", 0, 0);
                if(gbuf) {
                    log_debug_gbuf(
                        LOG_DUMP_OUTPUT,
                        gbuf, // not own
                        "GBUFFER %s ==> %s",
                        gobj_short_name(gobj),
                        gobj_short_name(channel_gobj)
                    );
                } else {
                    log_debug_json(
                        LOG_DUMP_OUTPUT,
                        kw, // not own,
                        "KW %s ==> %s",
                        gobj_short_name(gobj),
                        gobj_short_name(channel_gobj)
                    );
                }
            }

            int ret = gobj_send_event(channel_gobj, event, kw_incref(kw), gobj);
            if(ret == 0) {
                some++;
                (*priv->ptxMsgs)++;
            }
            i_hs = rc_next_instance(i_hs, (rc_resource_t **)&hs);
        }
    }
    rc_free_iter(iter_open, TRUE, 0);
    rc_free_iter(iter, TRUE, 0);

    if(!some) {
        log_error(LOG_OPT_TRACE_STACK,
            "gobj",         "%s", gobj_full_name(gobj),
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_PARAMETER_ERROR,
            "msg",          "%s", "No channel FOUND to send",
            "event",        "%s", event,
            NULL
        );
    }

    KW_DECREF(kw);
    return 0;
}

/***************************************************************************
 *  How many channel opened?
 ***************************************************************************/
PRIVATE int channels_opened(hgobj gobj)
{
    int opened = 0;

    dl_list_t *dl_childs = gobj_match_childs_tree_by_strict_gclass(gobj, "Channel");
    hgobj child; rc_instance_t *i_hs;
    i_hs = rc_first_instance(dl_childs, (rc_resource_t **)&child);
    while(i_hs) {
        if(gobj_read_bool_attr(child, "opened")) {
            opened++;
        }
        i_hs = rc_next_instance(i_hs, (rc_resource_t **)&child);
    }

    rc_free_iter(dl_childs, TRUE, 0);

    return opened;
}


            /***************************
             *      Actions
             ***************************/




/***************************************************************************
 *
 ***************************************************************************/
PRIVATE int ac_on_open(hgobj gobj, const char *event, json_t *kw, hgobj src)
{
    hsdata hs = gobj_read_pointer_attr(src, "user_data2");
    if(hs) {
        sdata_write_bool(hs, "opened", 1);
    }

    if(gobj_trace_level(gobj) & TRACE_CONNECTION) {
        log_info(0,
            "gobj",             "%s", gobj_full_name(gobj),
            "function",         "%s", __FUNCTION__,
            "msgset",           "%s", MSGSET_OPEN_CLOSE,
            "msg",              "%s", "ON_OPEN",
            "channel",          "%s", gobj_short_name(src),
            NULL
        );
    }

    int opened = channels_opened(gobj);
    if(!kw) {
        kw = json_object();
    }
    kw_set_subdict_value(
        kw,
        "__temp__",
        "channels_opened",
        json_integer(opened)
    );
    kw_set_subdict_value(
        kw,
        "__temp__",
        "channel",
        json_string(gobj_name(src))
    );
    const char *__username__ = gobj_read_str_attr(src, "__username__");
    kw_set_subdict_value(
        kw,
        "__temp__",
        "__username__",
        json_string(__username__)
    );
    kw_set_subdict_value(
        kw,
        "__temp__",
        "channel_gobj",
        json_integer((json_int_t)(size_t)src)
    );

    gobj_publish_event(gobj, event, kw); // reuse kw
    return 0;
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE int ac_on_close(hgobj gobj, const char *event, json_t *kw, hgobj src)
{
    hsdata hs = gobj_read_pointer_attr(src, "user_data2");
    if(hs) {
        sdata_write_bool(hs, "opened", 0);
    }

    if(gobj_trace_level(gobj) & TRACE_CONNECTION) {
        log_info(0,
            "gobj",             "%s", gobj_full_name(gobj),
            "function",         "%s", __FUNCTION__,
            "msgset",           "%s", MSGSET_OPEN_CLOSE,
            "msg",              "%s", "ON_CLOSE",
            "channel",          "%s", gobj_short_name(src),
            NULL
        );
    }

    int opened = channels_opened(gobj);
    if(!kw) {
        kw = json_object();
    }
    kw_set_subdict_value(
        kw,
        "__temp__",
        "channels_opened",
        json_integer(opened)
    );
    kw_set_subdict_value(
        kw,
        "__temp__",
        "channel",
        json_string(gobj_name(src))
    );
    kw_set_subdict_value(
        kw,
        "__temp__",
        "channel_gobj",
        json_integer((json_int_t)(size_t)src)
    );

    gobj_publish_event(gobj, event, kw); // reuse kw
    return 0;
}

/***************************************************************************
 *  Client/Server, receiving a message to publish.
 ***************************************************************************/
PRIVATE int ac_on_message(hgobj gobj, const char *event, json_t *kw, hgobj src)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    if(gobj_trace_level(gobj) & TRACE_MESSAGES) {
        GBUFFER *gbuf = (GBUFFER *)(size_t)kw_get_int(kw, "gbuffer", 0, 0);
        if(gbuf) {
            log_debug_gbuf(
                LOG_DUMP_INPUT,
                gbuf, // not own
                "GBUFFER %s <== %s",
                gobj_short_name(gobj),
                gobj_short_name(src)
            );
        } else {
            log_debug_json(
                LOG_DUMP_INPUT,
                kw, // not own
                "KW %s <== %s",
                gobj_short_name(gobj),
                gobj_short_name(src)
            );
        }
    }

    (*priv->prxMsgs)++;

    if(!kw) {
        kw = json_object();
    }
    kw_set_subdict_value(
        kw,
        "__temp__",
        "channel",
        json_string(gobj_name(src))
    );
    kw_set_subdict_value(
        kw,
        "__temp__",
        "__username__",
        json_string(gobj_read_str_attr(src, "__username__"))
    );
    kw_set_subdict_value(
        kw,
        "__temp__",
        "channel_gobj",
        json_integer((json_int_t)(size_t)src)
    );

    gobj_publish_event(gobj, event, kw); // reuse kw
    return 0;
}

/***************************************************************************
 *  Server, receiving a IEvent inter-event to publish.
 ***************************************************************************/
PRIVATE int ac_iev_message(hgobj gobj, const char *event, json_t *kw, hgobj src)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    const char *iev_event = kw_get_str(kw, "event", 0, KW_REQUIRED);
    json_t *iev_kw = kw_get_dict(kw, "kw", 0, KW_REQUIRED|KW_EXTRACT);

    if(gobj_trace_level(gobj) & TRACE_MESSAGES) {
        GBUFFER *gbuf = (GBUFFER *)(size_t)kw_get_int(kw, "gbuffer", 0, 0);
        if(gbuf) {
            log_debug_gbuf(
                LOG_DUMP_INPUT,
                gbuf, // not own
                "GBUFFER %s <== %s",
                gobj_short_name(gobj),
                gobj_short_name(src)
            );
        } else {
            log_debug_json(
                LOG_DUMP_INPUT,
                kw, // not own
                "KW %s <== %s",
                gobj_short_name(gobj),
                gobj_short_name(src)
            );
        }
    }

    (*priv->prxMsgs)++;

    kw_set_subdict_value(
        iev_kw,
        "__temp__",
        "channel",
        json_string(gobj_name(src))
    );
    kw_set_subdict_value(
        iev_kw,
        "__temp__",
        "channel_gobj",
        json_integer((json_int_t)(size_t)src)
    );
    gobj_publish_event(gobj, iev_event, iev_kw);  // reuse kw

    KW_DECREF(kw);
    return 0;
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE int ac_send_message(hgobj gobj, const char *event, json_t *kw, hgobj src)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    int send_type = kw_get_int(kw, "__send_type__", priv->send_type, 0);

    switch(send_type) {
    case TYPE_SEND_ALL:
        send_all(gobj, event, kw_incref(kw), src);
        break;

    case TYPE_SEND_ONE_ROTATED:
        send_one_rotate(gobj, event, kw_incref(kw), src);
        break;
    }

    KW_DECREF(kw);
    return 0;
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE int ac_send_iev(hgobj gobj, const char *event, json_t *kw, hgobj src)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    const char *iev_event = kw_get_str(kw, "event", "", KW_REQUIRED);
    json_t *iev_kw = kw_get_dict(kw, "kw", 0, KW_REQUIRED|KW_EXTRACT);
    json_t *__temp__ = kw_get_dict(kw, "__temp__", 0, 0);

    int send_type = kw_get_int(kw, "__send_type__", priv->send_type, 0);

    switch(send_type) {
    case TYPE_SEND_ALL:
        send_all(gobj, iev_event, iev_kw, src);
        break;

    case TYPE_SEND_ONE_ROTATED:
        if(__temp__) {
            json_object_set(iev_kw, "__temp__", __temp__);
        }
        send_one_rotate(gobj, iev_event, iev_kw, src);
        break;
    }

    KW_DECREF(kw);
    return 0;
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE int ac_drop(hgobj gobj, const char *event, json_t *kw, hgobj src)
{
    const char *channel = kw_get_str(kw, "__temp__`channel", "", 0);
    hgobj channel_gobj = (hgobj)(size_t)kw_get_int(kw, "__temp__`channel_gobj", 0, 0);

    if(!channel_gobj) {
        if(!empty_string(channel)) {
            channel_gobj = gobj_child_by_name(gobj, channel, 0);
            if(!channel_gobj) {
                log_error(0,
                    "gobj",         "%s", gobj_full_name(gobj),
                    "function",     "%s", __FUNCTION__,
                    "msgset",       "%s", MSGSET_PARAMETER_ERROR,
                    "msg",          "%s", "channel NOT FOUND",
                    "channel",  "%s", channel,
                    "event",        "%s", event,
                    NULL
                );
                KW_DECREF(kw);
                return -1;
            }
        }
    }

    /*
     *  Only drop one
     */
    if(channel_gobj) {
        if(gobj_read_bool_attr(channel_gobj, "opened")) {
            gobj_send_event(channel_gobj, "EV_DROP", 0, gobj);
        }
        KW_DECREF(kw);
        return 0;
    }

    /*
     *  Drop all
     */
    hgobj child; rc_instance_t *i_child;
    i_child = gobj_first_child(gobj, &child);

    while(i_child) {
        if(gobj_typeof_gclass(child, GCLASS_CHANNEL_NAME)) {
            if(gobj_read_bool_attr(child, "opened")) {
                gobj_send_event(child, "EV_DROP", 0, gobj);
            }
        }

        i_child = gobj_next_child(i_child, &child);
    }

    JSON_DECREF(kw);
    return 0;
}

/***************************************************************************
 *  This event comes from clisrv TCP gobjs
 *  that haven't found a free server link.
 ***************************************************************************/
PRIVATE int ac_stopped(hgobj gobj, const char *event, json_t *kw, hgobj src)
{
    JSON_DECREF(kw);

    if(gobj_typeof_subgclass(src, GCLASS_TCP0_NAME)) {
        gobj_destroy(src);
    }

    return 0;
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE int ac_timeout(hgobj gobj, const char *event, json_t *kw, hgobj src)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    uint64_t ms = time_in_miliseconds();
    if(!priv->last_ms) {
        priv->last_ms = ms;
    }
    uint64_t t = (ms - priv->last_ms);
    if(t>0) {
        uint64_t txMsgsec = *(priv->ptxMsgs) - priv->last_txMsgs;
        uint64_t rxMsgsec = *(priv->prxMsgs) - priv->last_rxMsgs;

        txMsgsec *= 1000;
        rxMsgsec *= 1000;
        txMsgsec /= t;
        rxMsgsec /= t;

        uint64_t maxtxMsgsec = gobj_read_uint64_attr(gobj, "maxtxMsgsec");
        uint64_t maxrxMsgsec = gobj_read_uint64_attr(gobj, "maxrxMsgsec");
        if(txMsgsec > maxtxMsgsec) {
            gobj_write_uint64_attr(gobj, "maxtxMsgsec", txMsgsec);
        }
        if(rxMsgsec > maxrxMsgsec) {
            gobj_write_uint64_attr(gobj, "maxrxMsgsec", rxMsgsec);
        }

        gobj_write_uint64_attr(gobj, "txMsgsec", txMsgsec);
        gobj_write_uint64_attr(gobj, "rxMsgsec", rxMsgsec);
    }
    priv->last_ms = ms;
    priv->last_txMsgs = *(priv->ptxMsgs);
    priv->last_rxMsgs = *(priv->prxMsgs);

    JSON_DECREF(kw);
    return 0;
}


/***************************************************************************
 *                          FSM
 ***************************************************************************/
PRIVATE const EVENT input_events[] = {
    // bottom input
    {"EV_IEV_MESSAGE",              0, 0, 0},
    {"EV_ON_MESSAGE",               0, 0, 0},
    {"EV_ON_COMMAND",               0, 0, 0},
    {"EV_ON_ID",                    0, 0, 0},
    {"EV_ON_ID_NAK",                0, 0, 0},
    {"EV_ON_SETUP",                 0, 0, 0},
    {"EV_ON_SETUP_COMPLETE",        0, 0, 0},
    {"EV_ON_OPEN",                  0, 0, 0},
    {"EV_ON_CLOSE",                 0, 0, 0},

    // top input
    {"EV_SEND_MESSAGE",             0,  0,  "Send a message"},
    {"EV_SEND_IEV",                 0,  0,  "Send a inter-event message"},
    {"EV_DROP",                     0,  0,  "Drop connection"},

    // internal
    {"EV_STOPPED",                  0, 0, 0},
    {"EV_TIMEOUT",                  0, 0, 0},
    {NULL, 0}
};
PRIVATE const EVENT output_events[] = {
    {"EV_ON_MESSAGE",               0,   0,  "Message received"},
    {"EV_ON_ID",                    0,   0,  "Id received"},
    {"EV_ON_ID_NAK",                0,   0,  "Id refused"},
    {"EV_ON_OPEN",                  0,   0,  "Channel opened"},
    {"EV_ON_CLOSE",                 0,   0,  "Channel closed"},
    {NULL, 0}
};
PRIVATE const char *state_names[] = {
    "ST_IDLE",
    NULL
};

PRIVATE EV_ACTION ST_IDLE[] = {
    {"EV_ON_MESSAGE",           ac_on_message,      0},
    {"EV_SEND_MESSAGE",         ac_send_message,    0},
    {"EV_IEV_MESSAGE",          ac_iev_message,     0},
    {"EV_SEND_IEV",             ac_send_iev,        0},
    {"EV_ON_COMMAND",           ac_on_message,      0},
    {"EV_ON_ID",                ac_on_message,      0},
    {"EV_ON_ID_NAK",            ac_on_message,      0},
    {"EV_DROP",                 ac_drop,            0},
    {"EV_ON_OPEN",              ac_on_open,         0},
    {"EV_ON_CLOSE",             ac_on_close,        0},
    {"EV_ON_SETUP",             0,                  0},
    {"EV_ON_SETUP_COMPLETE",    0,                  0},
    {"EV_TIMEOUT",              ac_timeout,         0},
    {"EV_STOPPED",              ac_stopped,         0},
    {0,0,0}
};

PRIVATE EV_ACTION *states[] = {
    ST_IDLE,
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
    GCLASS_IOGATE_NAME,
    &fsm,
    {
        mt_create,
        0, //mt_create2,
        0, //mt_destroy,
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
        mt_stats,
        0, //mt_command,
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
        mt_trace_on,
        mt_trace_off,
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
    command_table,
    gcflag_no_check_output_events, // gcflag
};

/***************************************************************************
 *              Public access
 ***************************************************************************/
PUBLIC GCLASS *gclass_iogate(void)
{
    return &_gclass;
}
