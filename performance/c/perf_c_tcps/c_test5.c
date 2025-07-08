/***********************************************************************
 *          C_TEST5.C
 *
 *          Copyright (c) 2024 by ArtGins.
 *          All Rights Reserved.
 ***********************************************************************/
#include <string.h>
#include <limits.h>

#include "common/c_pepon.h"
#include "c_test5.h"

/***************************************************************************
 *              Constants
 ***************************************************************************/
#define DATABASE    "perf_topic_integer"
#define TOPIC_NAME  "perf_tcps_test5"

#define MESSAGE "{\"id\": 1, \"tm\": 1, \"content\": \"Pepe el alfa.Pepe el alfa.Pepe el alfa.Pepe el alfa.Pepe el alfa.Pepe el alfa.Pepe el alfa.Pepe el.\"}"

/***************************************************************************
 *              Structures
 ***************************************************************************/

/***************************************************************************
 *              Prototypes
 ***************************************************************************/
PRIVATE int open_tranger(hgobj gobj);
PRIVATE int close_tranger(hgobj gobj);

/***************************************************************************
 *          Data: config, public data, private data
 ***************************************************************************/
time_measure_t time_measure;

/*---------------------------------------------*
 *      Attributes - order affect to oid's
 *---------------------------------------------*/
PRIVATE sdata_desc_t attrs_table[] = {
/*-ATTR-type------------name----------------flag----------------default-----description--*/
SDATA (DTP_INTEGER,     "timeout",          SDF_RD,             "1000",     "Timeout"),
SDATA (DTP_POINTER,     "user_data",        0,                  0,          "user data"),
SDATA (DTP_POINTER,     "user_data2",       0,                  0,          "more user data"),
SDATA (DTP_POINTER,     "subscriber",       0,                  0,          "subscriber of output-events. Not a child gobj."),
SDATA_END()
};

/*---------------------------------------------*
 *      GClass trace levels
 *---------------------------------------------*/
enum {
    TRACE_MESSAGES  = 0x0001,
};
PRIVATE const trace_level_t s_user_trace_level[16] = {
{"messages",        "Trace messages"},
{0, 0},
};

/*---------------------------------------------*
 *              Private data
 *---------------------------------------------*/
typedef struct _PRIVATE_DATA {
    json_int_t timeout;
    hgobj timer;

    hgobj pepon;

    hgobj gobj_output_side;
    json_int_t txMsgs;
    json_int_t rxMsgs;

    json_t *tranger2;
    json_t *list;

} PRIVATE_DATA;

PRIVATE hgclass __gclass__ = 0;




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
    json_t *kw_pepon = json_pack("{s:b}",
        "do_echo", 1
    );
    priv->pepon = gobj_create_pure_child("server", C_PEPON, kw_pepon, gobj);

    /*
     *  Do copy of heavy-used parameters, for quick access.
     *  HACK The writable attributes must be repeated in mt_writing method.
     */
    SET_PRIV(timeout,               gobj_read_integer_attr)
}

/***************************************************************************
 *      Framework Method start
 ***************************************************************************/
PRIVATE int mt_start(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    gobj_start(priv->timer);
    if(!gobj_is_running(priv->pepon)) {
        gobj_start(priv->pepon);
    }

    open_tranger(gobj);

    return 0;
}

/***************************************************************************
 *      Framework Method stop
 ***************************************************************************/
PRIVATE int mt_stop(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    close_tranger(gobj);

    gobj_stop(priv->timer);
    gobj_stop(priv->pepon);

    return 0;
}

/***************************************************************************
 *      Framework Method play
 *  Yuneta rule:
 *  If service has mt_play then start only the service gobj.
 *      (Let mt_play be responsible to start their tree)
 *  If service has not mt_play then start the tree with gobj_start_tree().
 ***************************************************************************/
PRIVATE int mt_play(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    gobj_play(priv->pepon);
    set_timeout(priv->timer, 1000); // timeout to connecting

    return 0;
}

/***************************************************************************
 *      Framework Method pause
 ***************************************************************************/
PRIVATE int mt_pause(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    clear_timeout(priv->timer);
    gobj_pause(priv->pepon);

    return 0;
}



                    /***************************
                     *      Commands
                     ***************************/




                    /***************************
                     *      Local Methods
                     ***************************/




/***************************************************************************
 *  TIMERANGER
 ***************************************************************************/
PRIVATE int rt_disk_record_callback(
    json_t *tranger,
    json_t *topic,
    const char *key,
    json_t *list,
    json_int_t rowid,
    md2_record_ex_t *md_record,
    json_t *record      // must be owned
)
{
    system_flag2_t system_flag = md_record->system_flag;
    if(system_flag & sf_loading_from_disk) {
    }

    JSON_DECREF(record)
    return 0;
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE int open_tranger(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    /*
     *  Write the tests in ~/tests_yuneta/
     */
    const char *home = getenv("HOME");
    char path_root[PATH_MAX];
    char path_database[PATH_MAX];
    char path_topic[PATH_MAX];

    build_path(path_root, sizeof(path_root), home, "tests_yuneta", NULL);
    build_path(path_database, sizeof(path_database), path_root, DATABASE, NULL);
    build_path(path_topic, sizeof(path_topic), path_database, TOPIC_NAME, NULL);

    /*-------------------------------------------------*
     *      Startup the timeranger db
     *-------------------------------------------------*/
    json_t *jn_tranger = json_pack("{s:s, s:s, s:b, s:i}",
        "path", path_root,
        "database", DATABASE,
        "master", 1,
        "on_critical_error", 0
    );
    priv->tranger2 = tranger2_startup(gobj, jn_tranger, yuno_event_loop());

    json_t *match_cond = json_pack("{s:b, s:i, s:I}",
        "backward", 0,
        "from_rowid", -10,
        "load_record_callback", (json_int_t)(size_t)rt_disk_record_callback
    );

    char directory[PATH_MAX];
    snprintf(directory, sizeof(directory), "%s/%s",
        kw_get_str(0, priv->tranger2, "directory", "", KW_REQUIRED),
        TOPIC_NAME    // topic name
    );
    if(is_directory(directory)) {
        rmrdir(directory);
    }

    /*-------------------------------------------------*
     *      Create a topic
     *-------------------------------------------------*/
    tranger2_create_topic(
        priv->tranger2,
        TOPIC_NAME,     // topic name
        "id",           // pkey
        "tm",           // tkey
        json_pack("{s:i, s:s, s:i, s:i}", // jn_topic_desc
            "on_critical_error", 4,
            "filename_mask", "%Y-%m-%d",
            "xpermission" , 02700,
            "rpermission", 0600
        ),
        sf_int_key,  // system_flag
        json_pack("{s:s, s:I, s:s}", // jn_cols, owned
            "id", "",
            "tm", (json_int_t)0,
            "content", ""
        ),
        0
    );

    priv->list = tranger2_open_list(
        priv->tranger2,
        TOPIC_NAME,
        match_cond,             // match_cond, owned
        NULL,                   // extra
        "",                     // rt_id
        FALSE,                  // rt_by_disk
        NULL                    // creator
    );

    return 0;
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE int close_tranger(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    /*-------------------------*
     *  close
     *-------------------------*/
    tranger2_close_list(priv->tranger2, priv->list);

    /*-------------------------------*
     *      Shutdown timeranger
     *-------------------------------*/
    tranger2_shutdown(priv->tranger2);

    return 0;
}




                    /***************************
                     *      Actions
                     ***************************/




/***************************************************************************
 *  Gps connected
 ***************************************************************************/
PRIVATE int ac_on_open(hgobj gobj, const char *event, json_t *kw, hgobj src)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    set_timeout(priv->timer, 1000); // timeout to start sending messages

    JSON_DECREF(kw)
    return 0;
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE int ac_timeout_to_connect(hgobj gobj, const char *event, json_t *kw, hgobj src)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    priv->gobj_output_side = gobj_find_service("__output_side__", TRUE);
    gobj_subscribe_event(priv->gobj_output_side, NULL, 0, gobj);
    gobj_start_tree(priv->gobj_output_side);

    JSON_DECREF(kw)
    return 0;
}

/***************************************************************************
 *
 ***************************************************************************/
gbuffer_t *gbuf_to_send = 0;
PRIVATE int ac_timeout_send_messages(hgobj gobj, const char *event, json_t *kw, hgobj src)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    gbuf_to_send = gbuffer_create(1024, 1024);
    gbuffer_printf(gbuf_to_send, MESSAGE);

    json_t *kw_send = json_pack("{s:I}",
        "gbuffer", (json_int_t)(size_t)gbuf_to_send
    );
    gobj_send_event(priv->gobj_output_side, EV_SEND_MESSAGE, kw_send, gobj);

    JSON_DECREF(kw)
    return 0;
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE int ac_on_close(hgobj gobj, const char *event, json_t *kw, hgobj src)
{
    JSON_DECREF(kw)
    return 0;
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE int ac_on_message(hgobj gobj, const char *event, json_t *kw, hgobj src)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    priv->rxMsgs++;

    gbuffer_t *gbuf = (gbuffer_t *)(size_t)kw_get_int(gobj, kw, "gbuffer", 0, 0);

    if(gobj_trace_level(gobj) & TRACE_MESSAGES) {
        gobj_trace_dump_gbuf(gobj, gbuf, "%s <== %s", gobj_short_name(gobj), gobj_short_name(src));
    }

    char *p = gbuffer_cur_rd_pointer(gbuf);
    if(strcmp(p, MESSAGE)!=0) {
        gobj_log_error(0, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_INTERNAL_ERROR,
            "msg",          "%s", "Message is not the same",
            NULL
        );
    }

    json_t *jn_record = gbuf2json(gbuffer_incref(gbuf), TRUE);
    gbuffer_reset_rd(gbuf);
    md2_record_ex_t md_record;
    tranger2_append_record(priv->tranger2, TOPIC_NAME, 0, 0, &md_record, jn_record);

    if(priv->rxMsgs==1) {
        MT_START_TIME(time_measure)
    }
    if(priv->rxMsgs>=180000) {
        MT_INCREMENT_COUNT(time_measure, 180000)
        MT_PRINT_TIME(time_measure, gobj_short_name(gobj))
        set_yuno_must_die();
    } else {
        GBUFFER_INCREF(gbuf)
        json_t *kw_send = json_pack("{s:I}",
            "gbuffer", (json_int_t)(size_t)gbuf
        );
        gobj_send_event(priv->gobj_output_side, EV_SEND_MESSAGE, kw_send, gobj);
    }

    KW_DECREF(kw)
    return 0;
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE int ac_stopped(hgobj gobj, const char *event, json_t *kw, hgobj src)
{
    gobj_log_info(0, 0,
        "msgset",           "%s", MSGSET_INFO,
        "msg",              "%s", "child stopped",
        "src",              "%s", gobj_full_name(src),
        NULL
    );

    JSON_DECREF(kw)

    if(gobj_is_volatil(src)) {
        gobj_log_info(0, 0,
            "msgset",           "%s", MSGSET_INFO,
            "msg",              "%s", "child destroyed",
            "src",              "%s", gobj_full_name(src),
            NULL
        );
        gobj_destroy(src);
    }

    return 0;
}

/***************************************************************************
 *                          FSM
 ***************************************************************************/
/*---------------------------------------------*
 *          Global methods table
 *---------------------------------------------*/
PRIVATE const GMETHODS gmt = {
    .mt_create = mt_create,
    .mt_start = mt_start,
    .mt_stop = mt_stop,
    .mt_play = mt_play,
    .mt_pause = mt_pause,
};

/*------------------------*
 *      GClass name
 *------------------------*/
GOBJ_DEFINE_GCLASS(C_TEST5);

/*------------------------*
 *      States
 *------------------------*/

/*------------------------*
 *      Events
 *------------------------*/

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE int create_gclass(gclass_name_t gclass_name)
{
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

    /*----------------------------------------*
     *          Define States
     *----------------------------------------*/
    ev_action_t st_closed[] = {
        {EV_STOPPED,                ac_stopped,                 0},
        {EV_TIMEOUT,                ac_timeout_to_connect,      0},
        {EV_ON_OPEN,                ac_on_open,                 ST_OPENED},
        {0,0,0}
    };
    ev_action_t st_opened[] = {
        {EV_ON_MESSAGE,             ac_on_message,              0},
        {EV_ON_CLOSE,               ac_on_close,                ST_CLOSED},
        {EV_TIMEOUT,                ac_timeout_send_messages,   0},
        {0,0,0}
    };

    states_t states[] = {
        {ST_CLOSED,                 st_closed},
        {ST_OPENED,                 st_opened},
        {0, 0}
    };

    event_type_t event_types[] = {
        {EV_ON_MESSAGE,     0},
        {EV_ON_OPEN,        0},
        {EV_ON_CLOSE,       0},
        {EV_STOPPED,        0},
        {EV_TIMEOUT,        0},
        {0, 0}
    };

    /*----------------------------------------*
     *          Create the gclass
     *----------------------------------------*/
    __gclass__ = gclass_create(
        gclass_name,
        event_types,
        states,
        &gmt,
        0,  // lmt,
        attrs_table,
        sizeof(PRIVATE_DATA),
        0,  // authz_table,
        0,  // command_table,
        s_user_trace_level,  // s_user_trace_level,
        0   // gcflag_t
    );
    if(!__gclass__) {
        // Error already logged
        return -1;
    }

    return 0;
}

/***************************************************************************
 *
 ***************************************************************************/
PUBLIC int register_c_test5(void)
{
    return create_gclass(C_TEST5);
}
