/****************************************************************************
 *          c_prot_tcp4h.c
 *          Prot_tcp4h GClass.
 *
 *          Protocol tcp4h, messages with a header of 4 bytes
 *
 *          Copyright (c) 2017-2023 Niyamaka.
 *          All Rights Reserved.
 ****************************************************************************/
#include <string.h>
#include <strings.h>
#include <comm_prot.h>
#ifdef ESP_PLATFORM
    #include <c_esp_transport.h>
#endif
#ifdef __linux__
    #include <c_linux_transport.h>
#endif
#include <kwid.h>
#include <gbuffer.h>
#include "c_prot_tcp4h.h"

/***************************************************************
 *              Constants
 ***************************************************************/

/***************************************************************
 *              Prototypes
 ***************************************************************/

/***************************************************************
 *              Data
 ***************************************************************/
/*---------------------------------------------*
 *          Attributes
 *---------------------------------------------*/
PRIVATE sdata_desc_t tattr_desc[] = {
/*-ATTR-type--------name----------------flag------------default-----description---------- */
SDATA (DTP_STRING,  "url",              SDF_PERSIST,    "",         "Url to connect"),
SDATA (DTP_STRING,  "jwt",              SDF_PERSIST,    "",         "JWT"),
SDATA (DTP_STRING,  "cert_pem",         SDF_PERSIST,    "",         "SSL server certification, PEM str format"),
SDATA (DTP_INTEGER, "subscriber",       0,              0,          "subscriber of output-events. If null then subscriber is the parent"),
SDATA_END()
};

/*---------------------------------------------*
 *      GClass trace levels
 *  HACK strict ascendant value!
 *  required paired correlative strings
 *  in s_user_trace_level
 *---------------------------------------------*/
enum {
    TRAFFIC         = 0x0001,
};
PRIVATE const trace_level_t s_user_trace_level[16] = {
    {"traffic",         "Trace traffic"},
    {0, 0},
};

/*---------------------------------------------*
 *              Private data
 *---------------------------------------------*/
typedef struct _PRIVATE_DATA {
    char p;
} PRIVATE_DATA;

PRIVATE hgclass gclass = 0;




                    /******************************
                     *      Framework Methods
                     ******************************/




/***************************************************************************
 *      Framework Method
 ***************************************************************************/
PRIVATE void mt_create(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    if(!gobj_is_pure_child(gobj)) {
        /*
         *  Not pure child, explicitly use subscriber
         */
        hgobj subscriber = (hgobj)(size_t)gobj_read_integer_attr(gobj, "subscriber");
        if(subscriber) {
            gobj_subscribe_event(gobj, NULL, NULL, subscriber);
        }
    }
}

/***************************************************************************
 *      Framework Method writing
 ***************************************************************************/
PRIVATE void mt_writing(hgobj gobj, const char *path)
{
}

/***************************************************************************
 *      Framework Method
 ***************************************************************************/
PRIVATE int mt_start(hgobj gobj)
{
    hgobj bottom_gobj = gobj_bottom_gobj(gobj);
    if(!bottom_gobj) {
        json_t *kw = json_pack("{s:s, s:s}",
            "cert_pem", gobj_read_str_attr(gobj, "cert_pem"),
            "url", gobj_read_str_attr(gobj, "url")
        );

        #ifdef ESP_PLATFORM
            hgobj gobj_bottom = gobj_create(gobj_name(gobj), C_ESP_TRANSPORT, kw, gobj);
        #endif
        #ifdef __linux__
            hgobj gobj_bottom = gobj_create(gobj_name(gobj), C_LINUX_TRANSPORT, kw, gobj);
        #endif
        gobj_set_bottom_gobj(gobj, gobj_bottom);
    }

    gobj_start(gobj_bottom_gobj(gobj));

    return 0;
}

/***************************************************************************
 *      Framework Method
 ***************************************************************************/
PRIVATE int mt_stop(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    gobj_stop(gobj_bottom_gobj(gobj));

    return 0;
}

/***************************************************************************
 *      Framework Method destroy
 ***************************************************************************/
PRIVATE void mt_destroy(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);
}




                    /***************************
                     *      Local methods
                     ***************************/








                    /***************************
                     *      Actions
                     ***************************/




/***************************************************************************
 *
 ***************************************************************************/
PRIVATE int ac_connected(hgobj gobj, gobj_event_t event, json_t *kw, hgobj src)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    if(gobj_is_pure_child(gobj)) {
        gobj_send_event(gobj_parent(gobj), EV_ON_OPEN, kw, gobj); // use the same kw
    } else {
        gobj_publish_event(gobj, EV_ON_OPEN, kw); // use the same kw
    }

    return 0;
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE int ac_disconnected(hgobj gobj, gobj_event_t event, json_t *kw, hgobj src)
{
    if(gobj_is_pure_child(gobj)) {
        gobj_send_event(gobj_parent(gobj), EV_ON_CLOSE, kw, gobj); // use the same kw
    } else {
        gobj_publish_event(gobj, EV_ON_CLOSE, kw); // use the same kw
    }

    return 0;
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE int ac_rx_data(hgobj gobj, gobj_event_t event, json_t *kw, hgobj src)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    gbuffer_t *gbuf = (gbuffer_t *)(size_t)kw_get_int(gobj, kw, "gbuffer", 0, 0);

    if(gobj_trace_level(gobj) & TRAFFIC) {
        gobj_trace_dump_gbuf(gobj, gbuf, "%s <- %s",
             gobj_short_name(gobj),
             gobj_short_name(gobj_bottom_gobj(gobj))
        );
    }

    KW_DECREF(kw)
    return 0;
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE int ac_send_message(hgobj gobj, gobj_event_t event, json_t *kw, hgobj src)
{
//    if(gobj_trace_level(gobj) & TRAFFIC) {
//        gobj_trace_dump_gbuf(gobj, gbuf, "%s -> %s",
//             gobj_short_name(gobj),
//             gobj_short_name(gobj_bottom_gobj(gobj))
//        );
//    }
//    KW_DECREF(kw)
//
//    json_t *kw_response = json_pack("{s:I}",
//        "gbuffer", (json_int_t)(size_t)gbuf
//    );
//    return gobj_send_event(gobj_bottom_gobj(gobj), EV_TX_DATA, kw_response, gobj);
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE int ac_drop(hgobj gobj, const char *event, json_t *kw, hgobj src)
{
    gobj_send_event(gobj_bottom_gobj(gobj), EV_DROP, 0, gobj);

    JSON_DECREF(kw)
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
    JSON_DECREF(kw)
    return 0;
}




                    /***************************
                     *          FSM
                     ***************************/




/*---------------------------------------------*
 *          Global methods table
 *---------------------------------------------*/
PRIVATE const GMETHODS gmt = {
    .mt_create = mt_create,
    .mt_writing = mt_writing,
    .mt_destroy = mt_destroy,
    .mt_start = mt_start,
    .mt_stop = mt_stop,
};

/*------------------------*
 *      GClass name
 *------------------------*/
GOBJ_DEFINE_GCLASS(C_PROT_TCP4H);

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
    if(gclass) {
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
    ev_action_t st_disconnected[] = {
        {EV_CONNECTED,          ac_connected,       ST_CONNECTED},
        {EV_STOPPED,            ac_stopped,         0},
        {0,0,0}
    };
    ev_action_t st_connected[] = {
        {EV_RX_DATA,            ac_rx_data,         0},
        {EV_SEND_MESSAGE,       ac_send_message,    0},
        {EV_TX_READY,           0,                  0},
        {EV_DISCONNECTED,       ac_disconnected,    ST_DISCONNECTED},
        {EV_DROP,               ac_drop,            0},
        {0,0,0}
    };

    states_t states[] = {
        {ST_DISCONNECTED,   st_disconnected},
        {ST_CONNECTED,      st_connected},
        {0, 0}
    };

    event_type_t event_types[] = {
        {EV_ON_MESSAGE,     EVF_OUTPUT_EVENT},
        {EV_ON_OPEN,        EVF_OUTPUT_EVENT},
        {EV_ON_CLOSE,       EVF_OUTPUT_EVENT},
        {0, 0}
    };

    /*----------------------------------------*
     *          Create the gclass
     *----------------------------------------*/
    gclass = gclass_create(
        gclass_name,
        event_types,
        states,
        &gmt,
        0,  // lmt,
        tattr_desc,
        sizeof(PRIVATE_DATA),
        0,  // authz_table,
        0,  // command_table,
        s_user_trace_level,
        0   // gcflag_t
    );
    if(!gclass) {
        // Error already logged
        return -1;
    }

    /*----------------------------------------*
     *          Register comm protocol
     *----------------------------------------*/
    comm_prot_register(gclass_name, "tcp4h");
    comm_prot_register(gclass_name, "tcp4hs");

    return 0;
}

/***************************************************************************
 *
 ***************************************************************************/
PUBLIC int register_c_prot_tcp4h(void)
{
    return create_gclass(C_PROT_TCP4H);
}
