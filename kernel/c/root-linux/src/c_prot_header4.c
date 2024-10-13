/***********************************************************************
 *          C_PROT_HEADER4.C
 *          Prot_header4 GClass.
 *
 *          Protocol with a 4 bytes header.
 *
 *          Copyright (c) 2017 Niyamaka.
 *          All Rights Reserved.
 ***********************************************************************/
#include <string.h>
#include "c_prot_header4.h"

/***************************************************************************
 *              Constants
 ***************************************************************************/

/***************************************************************************
 *              Structures
 ***************************************************************************/
typedef union {
    unsigned char bf[4];
    uint32_t len;
} HEADER_ERPL2;

/***************************************************************************
 *              Prototypes
 ***************************************************************************/


/***************************************************************************
 *          Data: config, public data, private data
 ***************************************************************************/

/*---------------------------------------------*
 *      Attributes - order affect to oid's
 *---------------------------------------------*/
PRIVATE sdata_desc_t tattr_desc[] = {
/*-ATTR-type------------name--------------------flag--------default---------description---------- */
SDATA (ASN_BOOLEAN,     "connected",            SDF_RD|SDF_STATS,0,         "Connection state. Important filter!"),
SDATA (ASN_OCTET_STR,   "on_open_event_name",   SDF_RD,     "EV_ON_OPEN",   "Must be empty if you don't want receive this event"),
SDATA (ASN_OCTET_STR,   "on_close_event_name",  SDF_RD,     "EV_ON_CLOSE",  "Must be empty if you don't want receive this event"),
SDATA (ASN_OCTET_STR,   "on_message_event_name",SDF_RD,     "EV_ON_MESSAGE","Must be empty if you don't want receive this event"),
SDATA (ASN_UNSIGNED,    "max_pkt_size",         SDF_WR,     0,              "Tamaño máximo del paquete"),
SDATA (ASN_POINTER,     "user_data",            0,          0,              "user data"),
SDATA (ASN_POINTER,     "user_data2",           0,          0,              "more user data"),
SDATA (ASN_POINTER,     "subscriber",           0,          0,              "subscriber of output-events. If it's null then subscriber is the parent."),
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
    const char *on_open_event_name;
    const char *on_close_event_name;
    const char *on_message_event_name;

    GBUFFER *last_pkt;  /* packet currently receiving */
    char bf_header_erpl2[sizeof(HEADER_ERPL2)];
    int idx_header;

    int inform_on_close;
    uint32_t max_pkt_size;
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

    /*
     *  CHILD subscription model
     */
    hgobj subscriber = (hgobj)gobj_read_pointer_attr(gobj, "subscriber");
    if(!subscriber)
        subscriber = gobj_parent(gobj);
    gobj_subscribe_event(gobj, NULL, NULL, subscriber);

    /*
     *  Do copy of heavy used parameters, for quick access.
     *  HACK The writable attributes must be repeated in mt_writing method.
     */
    SET_PRIV(max_pkt_size,          gobj_read_uint32_attr)
        if(priv->max_pkt_size == 0) {
            priv->max_pkt_size = (uint32_t)gbmem_get_maximum_block();
        }
    SET_PRIV(on_open_event_name,    gobj_read_str_attr)
    SET_PRIV(on_close_event_name,   gobj_read_str_attr)
    SET_PRIV(on_message_event_name, gobj_read_str_attr)
}

/***************************************************************************
 *      Framework Method writing
 ***************************************************************************/
PRIVATE void mt_writing(hgobj gobj, const char *path)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    IF_EQ_SET_PRIV(max_pkt_size,          gobj_read_uint32_attr)
        if(priv->max_pkt_size == 0) {
            priv->max_pkt_size = (uint32_t)gbmem_get_maximum_block();
        }
    END_EQ_SET_PRIV()
}

/***************************************************************************
 *      Framework Method start
 ***************************************************************************/
PRIVATE int mt_start(hgobj gobj)
{
    gobj_start_childs(gobj);
    return 0;
}

/***************************************************************************
 *      Framework Method stop
 ***************************************************************************/
PRIVATE int mt_stop(hgobj gobj)
{
    gobj_stop_childs(gobj);
    return 0;
}

/***************************************************************************
 *      Framework Method destroy
 ***************************************************************************/
PRIVATE void mt_destroy(hgobj gobj)
{
}




            /***************************
             *      Local Methods
             ***************************/




            /***************************
             *      Actions
             ***************************/




/***************************************************************************
 *
 ***************************************************************************/
PRIVATE int ac_drop(hgobj gobj, const char *event, json_t *kw, hgobj src)
{
    gobj_send_event(gobj_bottom_gobj(gobj), "EV_DROP", 0, gobj);

    KW_DECREF(kw);
    return 0;
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE int ac_connected(hgobj gobj, const char *event, json_t *kw, hgobj src)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    gobj_write_bool_attr(gobj, "connected", TRUE);
    gobj_change_state(gobj, "ST_SESSION");

    priv->inform_on_close = TRUE;
    if(!empty_string(priv->on_open_event_name)) {
        gobj_publish_event(gobj, priv->on_open_event_name, 0);
    }

    KW_DECREF(kw);
    return 0;
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE int ac_disconnected(hgobj gobj, const char *event, json_t *kw, hgobj src)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    if(gobj_is_volatil(src)) {
        gobj_set_bottom_gobj(gobj, 0);
    }
    gobj_write_bool_attr(gobj, "connected", FALSE);

    if(priv->inform_on_close) {
        priv->inform_on_close = FALSE;
        if(!empty_string(priv->on_close_event_name)) {
            gobj_publish_event(gobj, priv->on_close_event_name, 0);
        }
    }

    KW_DECREF(kw);
    return 0;
}

/********************************************************************
 *  Decode 4 bytes header (lenght of message).
 ********************************************************************/
PRIVATE int ac_analyze_rx_data(hgobj gobj, const char *event, json_t *kw, hgobj src)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);
    GBUFFER *gbuf = (GBUFFER *)(size_t)kw_get_int(kw, "gbuffer", 0, FALSE);

    size_t pend_size = 0;
    size_t len = gbuf_leftbytes(gbuf);
    while(len>0) {
        if(priv->last_pkt) {
            /*--------------------*
              *   Estoy a medias
              *--------------------*/
            pend_size = gbuf_freebytes(priv->last_pkt); /* mira lo que falta */
            if(len >= pend_size) {
                /*----------------------------------------*
                 *   Justo lo que falta o
                 *   Resto de uno y principio de otro
                 *---------------------------------------*/
                gbuf_append(
                    priv->last_pkt,
                    gbuf_get(gbuf, pend_size),
                    pend_size
                );
                len -= pend_size;
                json_t *kw_tx = json_pack("{s:I}",
                    "gbuffer", (json_int_t)(size_t)priv->last_pkt
                );
                priv->last_pkt = 0;
                gobj_publish_event(gobj, priv->on_message_event_name, kw_tx);

            } else { /* len < pend_size */
                /*-------------------------------------*
                  *   Falta todavia mas
                  *-------------------------------------*/
                gbuf_append(
                    priv->last_pkt,
                    gbuf_get(gbuf, len),
                    len
                );
                len = 0;
            }
        } else {
            /*--------------------*
             *   Paquete nuevo
             *--------------------*/
            int need2header = sizeof(HEADER_ERPL2) - priv->idx_header;
            if(len < need2header) {
                memcpy(
                    priv->bf_header_erpl2 + priv->idx_header,
                    gbuf_get(gbuf, len),
                    len
                );
                priv->idx_header += len;
                len = 0;
                continue;
            } else {
                memcpy(
                    priv->bf_header_erpl2 + priv->idx_header,
                    gbuf_get(gbuf, need2header),
                    need2header
                );
                len -= need2header;
                priv->idx_header = 0;
            }

            /*
             *  Quita la cabecera
             */
            HEADER_ERPL2 header_erpl2;
            memmove((char *)&header_erpl2, priv->bf_header_erpl2, sizeof(HEADER_ERPL2));
            header_erpl2.len = ntohl(header_erpl2.len);
            header_erpl2.len -= sizeof(HEADER_ERPL2); // remove header

            if(header_erpl2.len > priv->max_pkt_size) {
                log_error(0,
                    "gobj",         "%s", gobj_full_name(gobj),
                    "function",     "%s", __FUNCTION__,
                    "msgset",       "%s", MSGSET_MEMORY_ERROR,
                    "msg",          "%s", "TOO LONG SIZE",
                    "len",          "%d", header_erpl2.len,
                    NULL
                );
                log_debug_gbuf(
                    LOG_DUMP_INPUT,
                    gbuf,
                    "ERROR: TOO LONG SIZE (%d)",
                    header_erpl2.len
                );
                gobj_send_event(gobj_bottom_gobj(gobj), "EV_DROP", 0, gobj);
                break;
            }
            GBUFFER *new_pkt = gbuf_create(
                header_erpl2.len,
                header_erpl2.len,
                0,
                0
            );
            if(!new_pkt) {
                log_error(0,
                    "gobj",         "%s", gobj_full_name(gobj),
                    "function",     "%s", __FUNCTION__,
                    "msgset",       "%s", MSGSET_MEMORY_ERROR,
                    "msg",          "%s", "gbuf_create() FAILED",
                    "len",          "%d", header_erpl2.len,
                    NULL
                );
                gobj_send_event(gobj_bottom_gobj(gobj), "EV_DROP", 0, gobj);
                break;
            }
            /*
             *  Put the data
             */
            if(len >= header_erpl2.len) {
                /* PAQUETE COMPLETO o MULTIPLE */
                if(header_erpl2.len > 0) {
                    // SSQ/SSR are 0 lenght
                    gbuf_append(
                        new_pkt,
                        gbuf_get(gbuf, header_erpl2.len),
                        header_erpl2.len
                    );
                    len -= header_erpl2.len;
                }
                json_t *kw_tx = json_pack("{s:I}",
                    "gbuffer", (json_int_t)(size_t)new_pkt
                );
                gobj_publish_event(gobj, priv->on_message_event_name, kw_tx);

            } else { /* len < header_erpl2.len */
                /* PAQUETE INCOMPLETO */
                if(len>0) {
                    gbuf_append(
                        new_pkt,
                        gbuf_get(gbuf, len),
                        len
                    );
                }
                priv->last_pkt = new_pkt;
                len = 0;
            }
        }
    }

    KW_DECREF(kw);
    return 0;
}

/********************************************************************
 *
 ********************************************************************/
PRIVATE int ac_send_message(hgobj gobj, const char *event, json_t *kw, hgobj src)
{
    GBUFFER *gbuf = (GBUFFER *)(size_t)kw_get_int(kw, "gbuffer", 0, FALSE);
    size_t len = gbuf_leftbytes(gbuf);
    GBUFFER *new_gbuf;
    HEADER_ERPL2 header_erpl2;

    memset(&header_erpl2, 0, sizeof(HEADER_ERPL2));

    len += sizeof(HEADER_ERPL2);
    new_gbuf = gbuf_create(len, len, 0, 0);
    header_erpl2.len = htonl(len);
    gbuf_append(
        new_gbuf,
        &header_erpl2,
        sizeof(HEADER_ERPL2)
    );
    gbuf_append_gbuf(new_gbuf, gbuf);

    json_t *kw_tx = json_pack("{s:I}",
        "gbuffer", (json_int_t)(size_t)new_gbuf
    );
    gobj_send_event(gobj_bottom_gobj(gobj), "EV_TX_DATA", kw_tx, gobj);

    KW_DECREF(kw);
    return 0;
}

/***************************************************************************
 *  Child stopped
 ***************************************************************************/
PRIVATE int ac_stopped(hgobj gobj, const char *event, json_t *kw, hgobj src)
{
    if(gobj_is_volatil(src)) {
        gobj_destroy(src);
    }
    JSON_DECREF(kw);
    return 0;
}


/***************************************************************************
 *                          FSM
 ***************************************************************************/

PRIVATE const EVENT input_events[] = {
    // top input
    // bottom input
    {"EV_RX_DATA",          0,  0,  0},
    {"EV_SEND_MESSAGE",     0,  0,  0},
    {"EV_CONNECTED",        0,  0,  0},
    {"EV_DISCONNECTED",     0,  0,  0},
    {"EV_DROP",             0,  0,  0},
    {"EV_TX_READY",         0,  0,  0},
    {"EV_STOPPED",          0,  0,  0},
    // internal
    {NULL, 0, 0, 0}
};
PRIVATE const EVENT output_events[] = {
    {"EV_ON_OPEN",          0},
    {"EV_ON_CLOSE",         0},
    {"EV_ON_MESSAGE",       0},
    {NULL,                  0}
};
PRIVATE const char *state_names[] = {
    "ST_DISCONNECTED",
    "ST_WAIT_CONNECTED",
    "ST_SESSION",
    NULL
};

PRIVATE EV_ACTION ST_DISCONNECTED[] = {
    {"EV_CONNECTED",        ac_connected,               0},
    {"EV_DISCONNECTED",     ac_disconnected,            0},
    {"EV_STOPPED",          ac_stopped,                 0},
    {0,0,0}
};
PRIVATE EV_ACTION ST_WAIT_CONNECTED[] = {
    {"EV_CONNECTED",        ac_connected,               0},
    {"EV_DISCONNECTED",     ac_disconnected,            "ST_DISCONNECTED"},
    {0,0,0}
};
PRIVATE EV_ACTION ST_SESSION[] = {
    {"EV_RX_DATA",          ac_analyze_rx_data,         0},
    {"EV_SEND_MESSAGE",     ac_send_message,            0},
    {"EV_TX_READY",         0,                          0},
    {"EV_DROP",             ac_drop,                    0},
    {"EV_DISCONNECTED",     ac_disconnected,            "ST_DISCONNECTED"},
    {0,0,0}
};

PRIVATE EV_ACTION *states[] = {
    ST_DISCONNECTED,
    ST_WAIT_CONNECTED,
    ST_SESSION,
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
    GCLASS_PROT_HEADER4_NAME,
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
PUBLIC GCLASS *gclass_prot_header4(void)
{
    return &_gclass;
}
