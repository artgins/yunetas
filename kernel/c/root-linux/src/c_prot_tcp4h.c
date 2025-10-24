/****************************************************************************
 *          c_prot_tcp4h.c
 *          Prot_tcp4h GClass.
 *
 *          Protocol tcp4h, messages with a header of 4 bytes
 *          The header of 4 bytes in host order includes the payload size and the 4 bytes of header
 *
 *          Copyright (c) 2017-2023 Niyamaka.
 *          Copyright (c) 2024, ArtGins.
 *          All Rights Reserved.
 ****************************************************************************/
#include <string.h>
#include <netinet/in.h>
#include <gobj.h>
#include <g_ev_kernel.h>
#include <g_st_kernel.h>
#include <helpers.h>
#ifdef ESP_PLATFORM
    #include <c_esp_transport.h>
#endif
#ifdef __linux__
    #include "c_tcp.h"
#endif
#include "c_timer.h"
#include "istream.h"
#include "c_prot_tcp4h.h"



/***************************************************************
 *              Constants
 ***************************************************************/

/***************************************************************
 *              Prototypes
 ***************************************************************/
#pragma pack(1)

typedef union {
    unsigned char bf[4];
    uint32_t len;
} HEADER_ERPL4;

typedef struct _FRAME_HEAD {
    // Information of the first two bytes header
// mqtt_message_t command;
    uint8_t flags;

    // state of frame
    char busy;              // in half of header
    char header_complete;   // Set True when header is completed

    // must do
    char must_read_payload;

    size_t frame_length;
} FRAME_HEAD;

#pragma pack()

PRIVATE void start_wait_handshake(hgobj gobj);
PRIVATE void start_wait_frame_header(hgobj gobj);
PRIVATE void ws_close(hgobj gobj, int code);

PRIVATE int framehead_prepare_new_frame(FRAME_HEAD *frame);
PRIVATE int framehead_consume(
    hgobj gobj,
    FRAME_HEAD *frame,
    istream_h istream,
    char *bf,
    size_t len
);
PRIVATE int frame_completed(hgobj gobj);

/***************************************************************
 *              Data
 ***************************************************************/
/*---------------------------------------------*
 *          Attributes
 *---------------------------------------------*/
PRIVATE const sdata_desc_t attrs_table[] = {
/*-ATTR-type--------name----------------flag------------default-----description---------- */
SDATA (DTP_STRING,  "url",              SDF_PERSIST,    "",         "Url to connect"),
SDATA (DTP_STRING,  "cert_pem",         SDF_PERSIST,    "",         "SSL server certification, PEM str format"),
SDATA (DTP_INTEGER, "max_pkt_size",     SDF_WR,         0,          "Package maximum size"),

SDATA (DTP_BOOLEAN, "iamServer",        SDF_RD,                     0,      "What side? server or client"),
SDATA (DTP_INTEGER, "timeout_handshake",SDF_PERSIST,   "5000",      "Timeout to handshake"),
SDATA (DTP_INTEGER, "timeout_payload",  SDF_PERSIST,   "5000",      "Timeout to payload"),
SDATA (DTP_INTEGER, "timeout_close",    SDF_PERSIST,   "3000",      "Timeout to close"),

SDATA (DTP_POINTER, "user_data",        0,              0,          "user data"),
SDATA (DTP_POINTER, "user_data2",       0,              0,          "more user data"),
SDATA (DTP_POINTER, "subscriber",       0,              0,          "subscriber of output-events. If null then subscriber is the parent"),
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
    // gbuffer_t *last_pkt;  /* packet currently receiving */
    // char bf_header_erpl4[sizeof(HEADER_ERPL4)];
    // size_t idx_header;
    uint32_t max_pkt_size;

    hgobj timer;
    dl_list_t dl_msgs_out;  // Output queue of messages

    FRAME_HEAD frame_head;
    istream_h istream_frame;
    istream_h istream_payload;

    FRAME_HEAD message_head;

    /*
     *  Config
     */
    BOOL iamServer;         // What side? server or client
    json_int_t timeout_handshake;
    json_int_t timeout_payload;
    json_int_t timeout_close;

    /*
     *  Dynamic data (reset per connection)
     */

} PRIVATE_DATA;




                    /******************************
                     *      Framework Methods
                     ******************************/




/***************************************************************************
 *      Framework Method
 ***************************************************************************/
PRIVATE void mt_create(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    priv->iamServer = gobj_read_bool_attr(gobj, "iamServer");
    priv->timer = gobj_create_pure_child(gobj_name(gobj), C_TIMER, 0, gobj);

    dl_init(&priv->dl_msgs_out, gobj);

    priv->istream_frame = istream_create(gobj, 14, 14);
    if(!priv->istream_frame) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_INTERNAL_ERROR,
            "msg",          "%s", "istream_create() FAILED",
            NULL
        );
        return;
    }

    /*
     *  CHILD subscription model
     */
    hgobj subscriber = (hgobj)gobj_read_pointer_attr(gobj, "subscriber");
    if(!subscriber) {
        subscriber = gobj_parent(gobj);
    }
    gobj_subscribe_event(gobj, NULL, NULL, subscriber);

    /*
     *  Do copy of heavy-used parameters, for quick access.
     *  HACK The writable attributes must be repeated in mt_writing method.
     */
    SET_PRIV(max_pkt_size,      (uint32_t)gobj_read_integer_attr)
    if(priv->max_pkt_size == 0) {
        priv->max_pkt_size = (uint32_t)gbmem_get_maximum_block();
    }

    SET_PRIV(timeout_handshake,     gobj_read_integer_attr)
    SET_PRIV(timeout_payload,       gobj_read_integer_attr)
    SET_PRIV(timeout_close,         gobj_read_integer_attr)
}

/***************************************************************************
 *      Framework Method writing
 ***************************************************************************/
PRIVATE void mt_writing(hgobj gobj, const char *path)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    IF_EQ_SET_PRIV(max_pkt_size,    gobj_read_integer_attr)
        if(priv->max_pkt_size == 0) {
            priv->max_pkt_size = (uint32_t)gbmem_get_maximum_block();
        }
    ELIF_EQ_SET_PRIV(timeout_handshake,   gobj_read_integer_attr)
    ELIF_EQ_SET_PRIV(timeout_payload,     gobj_read_integer_attr)
    ELIF_EQ_SET_PRIV(timeout_close,       gobj_read_integer_attr)
    END_EQ_SET_PRIV()
}

/***************************************************************************
 *      Framework Method
 ***************************************************************************/
PRIVATE int mt_start(hgobj gobj)
{
    // const char *url = gobj_read_str_attr(gobj, "url");

    /*
     *  The bottom must be a C_TCP (it has manual start/stop!).
     *  If it's a client then start to begin the connection.
     *  If it's a server, wait to give the connection done by C_TCP_S.
     */

    const char *url = gobj_read_str_attr(gobj, "url");
    hgobj bottom_gobj = gobj_bottom_gobj(gobj);
    if(!empty_string(url) && !bottom_gobj) {
        json_t *kw = json_pack("{s:s, s:s}",
            "cert_pem", gobj_read_str_attr(gobj, "cert_pem"),
            "url", url
        );

        #ifdef ESP_PLATFORM
            bottom_gobj = gobj_create_pure_child(gobj_name(gobj), C_ESP_TRANSPORT, kw, gobj);
        #endif
        #ifdef __linux__
            bottom_gobj = gobj_create_pure_child(gobj_name(gobj), C_TCP, kw, gobj);
        #endif
        gobj_set_bottom_gobj(gobj, bottom_gobj);
    }

    if(bottom_gobj) {
        if(!empty_string(gobj_read_str_attr(bottom_gobj, "url"))) {
            /*
             *  Not empty url -> is a client
             */
            gobj_start(bottom_gobj);
        }
    }

    return 0;
}

/***************************************************************************
 *      Framework Method
 ***************************************************************************/
PRIVATE int mt_stop(hgobj gobj)
{
    if(gobj_bottom_gobj(gobj)) {
        gobj_stop(gobj_bottom_gobj(gobj));
    }

    return 0;
}

/***************************************************************************
 *      Framework Method destroy
 ***************************************************************************/
PRIVATE void mt_destroy(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    if(priv->istream_frame) {
        istream_destroy(priv->istream_frame);
        priv->istream_frame = 0;
    }
    if(priv->istream_payload) {
        istream_destroy(priv->istream_payload);
        priv->istream_payload = 0;
    }

    dl_flush(&priv->dl_msgs_out, gbmem_free);
}




                    /***************************
                     *      Local methods
                     ***************************/




/***************************************************************************
 *
 ***************************************************************************/
PRIVATE void ws_close(hgobj gobj, int reason)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    // Change firstly for avoid new messages from client
    gobj_change_state(gobj, ST_DISCONNECTED);

    gobj_send_event(gobj_bottom_gobj(gobj), EV_DROP, 0, gobj);

    if(priv->iamServer) {
        hgobj tcp0 = gobj_bottom_gobj(gobj);
        if(gobj_is_running(tcp0)) {
            gobj_stop(tcp0);
        }
    }
    set_timeout(priv->timer, priv->timeout_close);
}

/***************************************************************************
 *  Start to wait handshake
 ***************************************************************************/
PRIVATE void start_wait_handshake(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    if(!gobj_is_running(gobj)) {
        return;
    }
    gobj_change_state(gobj, ST_WAIT_HANDSHAKE);
    istream_reset_wr(priv->istream_frame);  // Reset buffer for next frame
    memset(&priv->frame_head, 0, sizeof(priv->frame_head));
    set_timeout(priv->timer, priv->timeout_handshake);
}

/***************************************************************************
 *  Start to wait frame header
 ***************************************************************************/
PRIVATE void start_wait_frame_header(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    if(!gobj_is_running(gobj)) {
        return;
    }
    gobj_change_state(gobj, ST_WAIT_FRAME_HEADER);
    istream_reset_wr(priv->istream_frame);  // Reset buffer for next frame
    memset(&priv->frame_head, 0, sizeof(priv->frame_head));
}

/***************************************************************************
 *  Reset variables for a new read.
 ***************************************************************************/
PRIVATE int framehead_prepare_new_frame(FRAME_HEAD *frame)
{
    /*
     *  state of frame
     */
    memset(frame, 0, sizeof(*frame));
    frame->busy = 1;    //in half of header

    return 0;
}

/***************************************************************************
 *  Decode the four bytes head.
 ***************************************************************************/
PRIVATE int decode_head(hgobj gobj, FRAME_HEAD *frame, char *data)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    HEADER_ERPL4 header_erpl4;
    memmove((char *)&header_erpl4, data, sizeof(HEADER_ERPL4));
    header_erpl4.len = ntohl(header_erpl4.len);
    header_erpl4.len -= sizeof(HEADER_ERPL4); // remove header

    if(gobj_trace_level(gobj) & TRAFFIC) {
        trace_msg0("New packet header_erpl4.len: %u",
            header_erpl4.len
        );
    }

    if(header_erpl4.len > priv->max_pkt_size) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_MEMORY_ERROR,
            "msg",          "%s", "TOO LONG SIZE",
            "len",          "%d", header_erpl4.len,
            NULL
        );
        return -1;
    }

    frame->frame_length = header_erpl4.len;
    frame->must_read_payload = TRUE;

    return 0;
}

/***************************************************************************
 *  Consume input data to get and analyze the frame header.
 *  Return the consumed size.
 ***************************************************************************/
PRIVATE int framehead_consume(
    hgobj gobj,
    FRAME_HEAD *frame,
    istream_h istream,
    char *bf,
    size_t len
) {
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    int total_consumed = 0;
    int consumed;
    char *data;

    /*
     *
     */
    if (!frame->busy) {
        /*
         * waiting the head
         */
        istream_read_until_num_bytes(istream, sizeof(HEADER_ERPL4), 0); // idempotent
        consumed = (int)istream_consume(istream, bf, len);
        total_consumed += consumed;
        bf += consumed;
        len -= consumed;
        if(!istream_is_completed(istream)) {
            return total_consumed;  // wait more data
        }

        /*
         *  we've got enough data! Start a new frame
         */
        framehead_prepare_new_frame(frame);  // `busy` flag is set.
        data = istream_extract_matched_data(istream, 0);
        if(decode_head(gobj, frame, data)<0) {
            // Error already logged
            return -1;
        }
    }

    frame->header_complete = TRUE;

    return total_consumed;
}

/***************************************************************************
 *  Process the completed frame
 ***************************************************************************/
PRIVATE int frame_completed(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    gbuffer_t *gbuf = istream_pop_gbuffer(priv->istream_payload);
    istream_destroy(priv->istream_payload);
    priv->istream_payload = NULL;

    json_t *kw_tx = json_pack("{s:I}",
        "gbuffer", (json_int_t)(uintptr_t)gbuf
    );
    gobj_publish_event(gobj, EV_ON_MESSAGE, kw_tx);

    start_wait_frame_header(gobj);

    return 0;
}





                    /***************************
                     *      Actions
                     ***************************/




/***************************************************************************
 *
 ***************************************************************************/
PRIVATE int ac_connected(hgobj gobj, gobj_event_t event, json_t *kw, hgobj src)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    gobj_reset_volatil_attrs(gobj);
    gobj_write_bool_attr(gobj, "connected", TRUE);
    //start_wait_handshake(gobj);   // In tcp4h there is no handshake
    start_wait_frame_header(gobj);

    if (priv->iamServer) {
        /*
         * wait the request
         */
    } else {
        /*
         * send the request
         */
    }
    return gobj_publish_event(gobj, EV_ON_OPEN, kw); // use the same kw
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE int ac_disconnected(hgobj gobj, gobj_event_t event, json_t *kw, hgobj src)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    gobj_write_bool_attr(gobj, "connected", FALSE);
    gobj_reset_volatil_attrs(gobj);

    if(gobj_is_volatil(src)) {
        gobj_set_bottom_gobj(gobj, 0);
    }
    if(priv->istream_payload) {
        istream_destroy(priv->istream_payload);
        priv->istream_payload = NULL;
    }
    if(priv->timer) {
        clear_timeout(priv->timer);
    }

    dl_flush(&priv->dl_msgs_out, gbmem_free);

    return gobj_publish_event(gobj, EV_ON_CLOSE, kw); // use the same kw
}

/***************************************************************************
 *  Too much time waiting disconnected
 ***************************************************************************/
PRIVATE int ac_timeout_wait_disconnected(hgobj gobj, const char *event, json_t *kw, hgobj src)
{
    gobj_log_warning(gobj, 0,
        "msgset",       "%s", MSGSET_MQTT_ERROR,
        "msg",          "%s", "Timeout waiting disconnected",
        NULL
    );

    ws_close(gobj, -1);

    KW_DECREF(kw)
    return 0;
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE int ac_timeout_wait_handshake(hgobj gobj, const char *event, json_t *kw, hgobj src)
{
    gobj_log_warning(gobj, 0,
        "msgset",       "%s", MSGSET_MQTT_ERROR,
        "msg",          "%s", "Timeout waiting handshake",
        NULL
    );

    ws_close(gobj, -1);

    KW_DECREF(kw)
    return 0;
}

/***************************************************************************
 *  Process the header.
 ***************************************************************************/
PRIVATE int ac_process_frame_header(hgobj gobj, const char *event, json_t *kw, hgobj src)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);
    gbuffer_t *gbuf = (gbuffer_t *)(uintptr_t)kw_get_int(gobj, kw, "gbuffer", 0, FALSE);

    clear_timeout(priv->timer);

    FRAME_HEAD *frame = &priv->frame_head;
    istream_h istream = priv->istream_frame;

    if(gobj_trace_level(gobj) & TRAFFIC) {
        gobj_trace_dump_gbuf(gobj, gbuf, "HEADER %s <== %s",
            gobj_short_name(gobj),
            gobj_short_name(src)
        );
    }

    while(gbuffer_leftbytes(gbuf)) {
        size_t ln = gbuffer_leftbytes(gbuf);
        char *bf = gbuffer_cur_rd_pointer(gbuf);
        int n = framehead_consume(gobj, frame, istream, bf, ln);
        if (n <= 0) {
            // Some error in parsing
            // on error do break the connection
            gobj_trace_dump_full_gbuf(gobj, gbuf, "ERROR in packet");
            ws_close(gobj, -1);
            break;
        } else if (n > 0) {
            gbuffer_get(gbuf, n);  // take out the bytes consumed
        }

        if(frame->header_complete) {
            if(frame->frame_length) {
                /*
                 *
                 */
                if(priv->istream_payload) {
                    istream_destroy(priv->istream_payload);
                    priv->istream_payload = NULL;
                    gobj_log_error(gobj, 0,
                        "function",     "%s", __FUNCTION__,
                        "msgset",       "%s", MSGSET_INTERNAL_ERROR,
                        "msg",          "%s", "istream_payload NOT NULL",
                        NULL
                    );
                }

                /*
                 *  Creat a new buffer for payload data
                 */
                size_t frame_length = frame->frame_length;
                if(!frame_length) {
                    gobj_log_error(gobj, 0,
                        "function",     "%s", __FUNCTION__,
                        "msgset",       "%s", MSGSET_MEMORY_ERROR,
                        "msg",          "%s", "no memory for istream_payload",
                        "frame_length", "%d", frame_length,
                        NULL
                    );
                    ws_close(gobj, -1);
                    break;
                }
                priv->istream_payload = istream_create(
                    gobj,
                    frame_length,
                    frame_length
                );
                if(!priv->istream_payload) {
                    gobj_log_error(gobj, 0,
                        "function",     "%s", __FUNCTION__,
                        "msgset",       "%s", MSGSET_MEMORY_ERROR,
                        "msg",          "%s", "no memory for istream_payload",
                        "frame_length", "%d", frame_length,
                        NULL
                    );
                    ws_close(gobj, -1);
                    break;
                }
                istream_read_until_num_bytes(priv->istream_payload, frame_length, 0);

                gobj_change_state(gobj, ST_WAIT_PAYLOAD);
                set_timeout(priv->timer, priv->timeout_payload);

                return gobj_send_event(gobj, EV_RX_DATA, kw, gobj);

            } else {
                gobj_log_error(gobj, 0,
                    "function",     "%s", __FUNCTION__,
                    "msgset",       "%s", MSGSET_INTERNAL_ERROR,
                    "msg",          "%s", "frame_length cannot be 0",
                    NULL
                );
                ws_close(gobj, -1);
                break;
            }
        }
    }

    KW_DECREF(kw)
    return 0;
}

/***************************************************************************
 *  No activity, send ping
 ***************************************************************************/
PRIVATE int ac_timeout_wait_frame_header(hgobj gobj, const char *event, json_t *kw, hgobj src)
{
    KW_DECREF(kw)
    return 0;
}

/***************************************************************************
 *  Get payload data
 ***************************************************************************/
PRIVATE int ac_process_payload_data(hgobj gobj, const char *event, json_t *kw, hgobj src)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);
    gbuffer_t *gbuf = (gbuffer_t *)(uintptr_t)kw_get_int(gobj, kw, "gbuffer", 0, FALSE);

    clear_timeout(priv->timer);

    if(gobj_trace_level(gobj) & TRAFFIC) {
        gobj_trace_dump_gbuf(gobj, gbuf, "PAYLOAD %s <== %s (accumulated %zu)",
            gobj_short_name(gobj),
            gobj_short_name(src),
            istream_length(priv->istream_payload)
        );
    }

    size_t bf_len = gbuffer_leftbytes(gbuf);
    char *bf = gbuffer_cur_rd_pointer(gbuf);

    int consumed = (int)istream_consume(priv->istream_payload, bf, bf_len);
    if(consumed > 0) {
        gbuffer_get(gbuf, consumed);  // take out the bytes consumed
    }
    if(istream_is_completed(priv->istream_payload)) {
        if(frame_completed(gobj)<0) {
            gobj_log_error(gobj, 0,
                "function",     "%s", __FUNCTION__,
                "msgset",       "%s", MSGSET_INTERNAL_ERROR,
                "msg",          "%s", "Protocol error, disconnect",
                NULL
            );
            gobj_trace_dump_full_gbuf(gobj, gbuf, "Protocol error, disconnect");
            ws_close(gobj, -1);
            KW_DECREF(kw)
            return -1;
        }
    } else {
        set_timeout(priv->timer, priv->timeout_payload);
    }

    if(gbuffer_leftbytes(gbuf)) {
        return gobj_send_event(gobj, EV_RX_DATA, kw, gobj);
    }

    KW_DECREF(kw)
    return 0;
}

/***************************************************************************
 *  Too much time waiting payload data
 ***************************************************************************/
PRIVATE int ac_timeout_wait_payload_data(hgobj gobj, const char *event, json_t *kw, hgobj src)
{
    gobj_log_info(gobj, 0,
        "msgset",       "%s", MSGSET_MQTT_ERROR,
        "msg",          "%s", "Timeout waiting PAYLOAD data",
        NULL
    );

    ws_close(gobj, -1);

    KW_DECREF(kw)
    return 0;
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE int ac_send_message(hgobj gobj, gobj_event_t event, json_t *kw, hgobj src)
{
    /*
     *  You can send data with kw as is, or data to send in a gbuffer.
     */
    BOOL internal_gbuf = FALSE;

    gbuffer_t *gbuf = (gbuffer_t *)(uintptr_t)kw_get_int(gobj, kw, "gbuffer", 0, FALSE);
    if(!gbuf) {
        gbuf = json2gbuf(NULL, kw_incref(kw), 0);
        internal_gbuf = TRUE;
        if(!gbuf) {
            gobj_log_error(gobj, 0,
                "function",     "%s", __FUNCTION__,
                "msgset",       "%s", MSGSET_INTERNAL_ERROR,
                "msg",          "%s", "gbuffer NULL",
                NULL
            );
            KW_DECREF(kw)
            return -1;
        }
    }

    size_t len = gbuffer_leftbytes(gbuf);
    gbuffer_t *new_gbuf;
    HEADER_ERPL4 header_erpl4;

    memset(&header_erpl4, 0, sizeof(HEADER_ERPL4));

    len += sizeof(HEADER_ERPL4);
    new_gbuf = gbuffer_create(len, len);
    header_erpl4.len = htonl(len);
    gbuffer_append(
        new_gbuf,
        &header_erpl4,
        sizeof(HEADER_ERPL4)
    );
    gbuffer_append_gbuf(new_gbuf, gbuf);

    json_t *kw_tx = json_pack("{s:I}",
        "gbuffer", (json_int_t)(uintptr_t)new_gbuf
    );
    int ret = gobj_send_event(gobj_bottom_gobj(gobj), EV_TX_DATA, kw_tx, gobj);

    if(internal_gbuf) {
        gbuffer_decref(gbuf);
    }
    KW_DECREF(kw)
    return ret;
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE int ac_drop(hgobj gobj, gobj_event_t event, json_t *kw, hgobj src)
{
    gobj_send_event(gobj_bottom_gobj(gobj), EV_DROP, 0, gobj);

    JSON_DECREF(kw)
    return 0;
}

/***************************************************************************
 *  Child stopped
 ***************************************************************************/
PRIVATE int ac_stopped(hgobj gobj, gobj_event_t event, json_t *kw, hgobj src)
{
    if(gobj_is_volatil(src)) {
        gobj_destroy(src);
    }
    JSON_DECREF(kw)
    return 0;
}

/***************************************************************************
 *                          FSM
 ***************************************************************************/
/*---------------------------------------------*
 *          Global methods table
 *---------------------------------------------*/
PRIVATE const GMETHODS gmt = {
    .mt_create  = mt_create,
    .mt_writing = mt_writing,
    .mt_destroy = mt_destroy,
    .mt_start   = mt_start,
    .mt_stop    = mt_stop,
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

    /*----------------------------------------*
     *          Define States
     *----------------------------------------*/
    ev_action_t st_disconnected[] = {
        {EV_CONNECTED,          ac_connected,                       ST_WAIT_HANDSHAKE},
        {EV_DISCONNECTED,       ac_disconnected,                    0},
        {EV_TIMEOUT,            ac_timeout_wait_disconnected,       0},
        {EV_STOPPED,            ac_stopped,                         0},
        {EV_TX_READY,           0,                                  0},
        {0,0,0}
    };
    ev_action_t st_wait_handshake[] = { // WARNING not used in this protocol
        {EV_RX_DATA,            ac_process_frame_header,            0},
        {EV_DISCONNECTED,       ac_disconnected,                    ST_DISCONNECTED},
        {EV_TIMEOUT,            ac_timeout_wait_handshake,          0},
        {EV_DROP,               ac_drop,                            0},
        {EV_TX_READY,           0,                                  0},
        {0,0,0}
    };
    ev_action_t st_wait_frame_header[] = {
        {EV_RX_DATA,            ac_process_frame_header,            0},
        {EV_SEND_MESSAGE,       ac_send_message,                    0},
        {EV_DISCONNECTED,       ac_disconnected,                    ST_DISCONNECTED},
        {EV_TIMEOUT,            ac_timeout_wait_frame_header,       0},
        {EV_DROP,               ac_drop,                            0},
        {EV_TX_READY,           0,                                  0},
        {0,0,0}
    };
    ev_action_t st_wait_payload[] = {
        {EV_RX_DATA,            ac_process_payload_data,            0},
        {EV_SEND_MESSAGE,       ac_send_message,                    0},
        {EV_DISCONNECTED,       ac_disconnected,                    ST_DISCONNECTED},
        {EV_TIMEOUT,            ac_timeout_wait_payload_data,       0},
        {EV_DROP,               ac_drop,                            0},
        {EV_TX_READY,           0,                                  0},
        {0,0,0}
    };

    states_t states[] = {
        {ST_DISCONNECTED,           st_disconnected},
        {ST_WAIT_HANDSHAKE,         st_wait_handshake},
        {ST_WAIT_FRAME_HEADER,      st_wait_frame_header},
        {ST_WAIT_PAYLOAD,           st_wait_payload},
        {0, 0}
    };

    event_type_t event_types[] = {
        {EV_RX_DATA,            0},
        {EV_SEND_MESSAGE,       0},
        {EV_TX_READY,           0},
        {EV_TIMEOUT,            0},
        {EV_CONNECTED,          0},
        {EV_DISCONNECTED,       0},
        {EV_STOPPED,            0},
        {EV_DROP,               0},

        {EV_ON_OPEN,            EVF_OUTPUT_EVENT},
        {EV_ON_CLOSE,           EVF_OUTPUT_EVENT},
        {EV_ON_MESSAGE,         EVF_OUTPUT_EVENT},
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
        s_user_trace_level,
        0   // gcflag_t
    );
    if(!__gclass__) {
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
