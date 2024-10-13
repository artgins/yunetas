/***********************************************************************
 *          C_WEBSOCKET.C
 *          GClass of WEBSOCKET protocol.
 *
 *          Copyright (c) 2013-2014 Niyamaka.
 *          All Rights Reserved.
 ***********************************************************************/
#include <string.h>
#include <stdint.h>
#include <endian.h>
#include <time.h>
#include <arpa/inet.h>
#include "c_websocket.h"

/***************************************************************************
 *              Constants
 ***************************************************************************/
/*
 * # websocket supported version.
 */
#define WEBSOCKET_VERSION 13

/*
 * closing frame status codes.
 */
typedef enum {
    STATUS_NORMAL = 1000,
    STATUS_GOING_AWAY = 1001,
    STATUS_PROTOCOL_ERROR = 1002,
    STATUS_UNSUPPORTED_DATA_TYPE = 1003,
    STATUS_STATUS_NOT_AVAILABLE = 1005,
    STATUS_ABNORMAL_CLOSED = 1006,
    STATUS_INVALID_PAYLOAD = 1007,
    STATUS_POLICY_VIOLATION = 1008,
    STATUS_MESSAGE_TOO_BIG = 1009,
    STATUS_INVALID_EXTENSION = 1010,
    STATUS_UNEXPECTED_CONDITION = 1011,
    STATUS_TLS_HANDSHAKE_ERROR = 1015,
    STATUS_MAXIMUM = STATUS_TLS_HANDSHAKE_ERROR,
} STATUS_CODE;

typedef enum {
    OPCODE_CONTINUATION_FRAME = 0x0,
    OPCODE_TEXT_FRAME = 0x01,
    OPCODE_BINARY_FRAME = 0x02,

    OPCODE_CONTROL_CLOSE = 0x08,
    OPCODE_CONTROL_PING = 0x09,
    OPCODE_CONTROL_PONG = 0x0A,
} OPCODE;

/***************************************************************************
 *              Structures
 ***************************************************************************/
/*
 *  Websocket frame head.
 *  This class analize the first two bytes of the header.
 *  The maximum size of a frame header is 14 bytes.

      0                   1
      0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5
     +-+-+-+-+-------+-+-------------+
     |F|R|R|R| opcode|M| Payload len |
     |I|S|S|S|  (4)  |A|     (7)     |
     |N|V|V|V|       |S|             |
     | |1|2|3|       |K|             |
     +-+-+-+-+-------+-+-------------+

    Full header:

      0                   1                   2                   3
      0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
     +-+-+-+-+-------+-+-------------+-------------------------------+
     |F|R|R|R| opcode|M| Payload len |    Extended payload length    |
     |I|S|S|S|  (4)  |A|     (7)     |             (16/64)           |
     |N|V|V|V|       |S|             |   (if payload len==126/127)   |
     | |1|2|3|       |K|             |                               |
     +-+-+-+-+-------+-+-------------+ - - - - - - - - - - - - - - - +
     |     Extended payload length continued, if payload len == 127  |
     + - - - - - - - - - - - - - - - +-------------------------------+
     |                               |Masking-key, if MASK set to 1  |
     +-------------------------------+-------------------------------+
     | Masking-key (continued)       |          Payload Data         |
     +-------------------------------- - - - - - - - - - - - - - - - +
     :                     Payload Data continued ...                :
     + - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - +
     |                     Payload Data continued ...                |
     +---------------------------------------------------------------+

 */
typedef struct _FRAME_HEAD {
    // Information of the first two bytes header
    char h_fin;         // final fragment in a message
    char h_reserved_bits;
    char h_opcode;
    char h_mask;        // Set to 1 a masking key is present
    char h_payload_len;

    // state of frame
    char busy;              // in half of header
    char header_complete;   // Set True when header is completed

    // must do
    char must_read_2_extended_payload_length;
    char must_read_8_extended_payload_length;
    char must_read_masking_key;
    char must_read_payload_data;

    // information of frame
    uint8_t masking_key[4];
    size_t frame_length;
} FRAME_HEAD;

/***************************************************************************
 *              Prototypes
 ***************************************************************************/
PRIVATE int send_http_message2(hgobj gobj, const char *format, ...) JANSSON_ATTRS((format(printf, 2, 3)));
PRIVATE void start_wait_frame_header(hgobj gobj);
PRIVATE void ws_close(hgobj gobj, int code, const char *reason);
PRIVATE BOOL do_response(hgobj gobj, GHTTP_PARSER *request);
PRIVATE BOOL do_request(
    hgobj gobj,
    const char *host,
    const char *port,
    const char *resource,
    json_t *options
);

PRIVATE int framehead_prepare_new_frame(FRAME_HEAD *frame);
PRIVATE int framehead_consume(FRAME_HEAD *frame, istream istream, char *bf, int len);
PRIVATE int frame_completed(hgobj gobj);

/***************************************************************************
 *          Data: config, public data, private data
 ***************************************************************************/
/*---------------------------------------------*
 *      Attributes - order affect to oid's
 *---------------------------------------------*/
PRIVATE sdata_desc_t tattr_desc[] = {
/*-ATTR-type------------name----------------flag------------------------default---------description---------- */
SDATA (ASN_BOOLEAN,     "connected",        SDF_RD|SDF_STATS,           0,              "Connection state. Important filter!"),
SDATA (ASN_INTEGER,     "timeout_handshake",SDF_WR|SDF_PERSIST,    5*1000,              "Timeout to handshake"),
SDATA (ASN_INTEGER,     "timeout_close",    SDF_WR|SDF_PERSIST,    3*1000,              "Timeout to close"),
SDATA (ASN_INTEGER,     "pingT",            SDF_WR|SDF_PERSIST,   50*1000,      "Ping interval. If value <= 0 then No ping"),
SDATA (ASN_POINTER,     "user_data",        0,                          0,              "user data"),
SDATA (ASN_POINTER,     "user_data2",       0,                          0,              "more user data"),
SDATA (ASN_BOOLEAN,     "iamServer",        SDF_RD,                     0,              "What side? server or client"),
SDATA (ASN_OCTET_STR,   "resource",         SDF_RD,                     "/",            "Resource when iam client"),
SDATA (ASN_JSON,        "kw_connex",        SDF_RD,                     0,              "Kw to create connex at client ws"),
SDATA (ASN_POINTER,     "subscriber",       0,                          0,              "subscriber of output-events. Default if null is parent."),
SDATA_END()
};

/*---------------------------------------------*
 *      GClass trace levels
 *---------------------------------------------*/
enum {
    TRACE_DEBUG = 0x0001,
};
PRIVATE const trace_level_t s_user_trace_level[16] = {
{"debug",        "Trace to debug"},
{0, 0},
};

/*---------------------------------------------*
 *              Private data
 *---------------------------------------------*/
typedef struct _PRIVATE_DATA {
    hgobj timer;
    char iamServer;         // What side? server or client
    int pingT;
    uint32_t *pconnected;

    FRAME_HEAD frame_head;
    istream istream_frame;
    istream istream_payload;

    FRAME_HEAD message_head;
    GBUFFER *gbuf_message;

    char close_frame_sent;              // close frame sent
    char on_close_broadcasted;          // event on_close already broadcasted
    char on_open_broadcasted;           // event on_open already broadcasted

    GHTTP_PARSER *parsing_request;      // A request parser instance
    GHTTP_PARSER *parsing_response;     // A response parser instance
    char ginsfsm_user_agent;    // True when user-agent is ginsfsm
                                // When user is a browser, it'll be sockjs.
                                // when user is not browser, at the moment,
                                // we only recognize ginsfsm websocket.
} PRIVATE_DATA;




            /***************************
             *      Framework Methods
             ***************************/




/***************************************************************************
 *      Framework Method create
 ***************************************************************************/
PRIVATE void mt_create(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    priv->pconnected = gobj_danger_attr_ptr(gobj, "connected");
    priv->iamServer = gobj_read_bool_attr(gobj, "iamServer");

    priv->timer = gobj_create("", GCLASS_TIMER, 0, gobj);
    priv->parsing_request = ghttp_parser_create(gobj, HTTP_REQUEST, 0, 0);
    priv->parsing_response = ghttp_parser_create(gobj, HTTP_RESPONSE, 0, 0);

    priv->istream_frame = istream_create(gobj, 14, 14, 0,0);
    if(!priv->istream_frame) {
        log_error(0,
            "gobj",         "%s", gobj_full_name(gobj),
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_INTERNAL_ERROR,
            "msg",          "%s", "istream_create() FAILED",
            NULL
        );
        return;
    }
    hgobj subscriber = (hgobj)gobj_read_pointer_attr(gobj, "subscriber");
    if(!subscriber)
        subscriber = gobj_parent(gobj);
    gobj_subscribe_event(gobj, NULL, NULL, subscriber);

    /*
     *  Do copy of heavy used parameters, for quick access.
     *  HACK The writable attributes must be repeated in mt_writing method.
     */
    SET_PRIV(pingT,             gobj_read_int32_attr)
}

/***************************************************************************
 *      Framework Method writing
 ***************************************************************************/
PRIVATE void mt_writing(hgobj gobj, const char *path)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    IF_EQ_SET_PRIV(pingT,               gobj_read_int32_attr)
    END_EQ_SET_PRIV()
}

/***************************************************************************
 *      Framework Method start
 *
 *      Start Point for external http server
 *      They must pass the `tcp0` with the connection done
 *      and the http `request`.
 ***************************************************************************/
PRIVATE int mt_start(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    if(!priv->iamServer) {
        hgobj tcp0 = gobj_bottom_gobj(gobj);
        if(!tcp0) {
            // Manual connex configuration
            json_t *kw_connex = gobj_read_json_attr(gobj, "kw_connex");
            json_incref(kw_connex);
            tcp0 = gobj_create(gobj_name(gobj), GCLASS_CONNEX, kw_connex, gobj);
            gobj_set_bottom_gobj(gobj, tcp0);
            gobj_write_str_attr(tcp0, "tx_ready_event_name", 0);
        }
    }

    gobj_start(priv->timer);
    hgobj tcp0 = gobj_bottom_gobj(gobj);
    if(tcp0) {
        if(!gobj_is_running(tcp0)) {
            gobj_start(tcp0);
        }
    }
    return 0;
}

/***************************************************************************
 *      Framework Method stop
 ***************************************************************************/
PRIVATE int mt_stop(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    if(*priv->pconnected) {
        //TODO NOOOO disconnect_on_last_transmit
        ws_close(gobj, STATUS_NORMAL, 0);
    }
    if(priv->timer) {
        clear_timeout(priv->timer);
        gobj_stop(priv->timer);
    }

    hgobj tcp0 = gobj_bottom_gobj(gobj);
    if(tcp0) {
        if(gobj_is_running(tcp0)) {
            gobj_stop(tcp0);
        }
    }

    return 0;
}

/***************************************************************************
 *      Framework Method destroy
 ***************************************************************************/
PRIVATE void mt_destroy(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    if(priv->parsing_request) {
        ghttp_parser_destroy(priv->parsing_request);
        priv->parsing_request = 0;
    }
    if(priv->parsing_response) {
        ghttp_parser_destroy(priv->parsing_response);
        priv->parsing_response = 0;
    }
    if(priv->istream_frame) {
        istream_destroy(priv->istream_frame);
        priv->istream_frame = 0;
    }
    if(priv->istream_payload) {
        istream_destroy(priv->istream_payload);
        priv->istream_payload = 0;
    }
    if(priv->gbuf_message) {
        gbuf_decref(priv->gbuf_message);
        priv->gbuf_message = 0;
    }
}




            /***************************
             *      Local Methods
             ***************************/




/***************************************************************************
 *  Start to wait frame header
 ***************************************************************************/
PRIVATE void start_wait_frame_header(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    if(!gobj_is_running(gobj)) {
        return;
    }
    gobj_change_state(gobj, "ST_WAITING_FRAME_HEADER");
    if(priv->pingT>0) {
        set_timeout(priv->timer, priv->pingT);
    }
    istream_reset_wr(priv->istream_frame);  // Reset buffer for next frame
    priv->frame_head.busy = FALSE;
    priv->frame_head.header_complete = FALSE;
}

/***************************************************************************
 *  mask or unmask data. Just do xor for each byte
 *  mask_key: 4 bytes
 *  data: data to mask/unmask.
 ***************************************************************************/
PRIVATE void mask(char *mask_key, char *data, int len)
{
    for(int i=0; i<len; i++) {
        *(data + i) ^= mask_key[i % 4];
    }
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE int _add_frame_header(hgobj gobj, GBUFFER *gbuf, char h_fin, char h_opcode, size_t ln)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);
    unsigned char byte1, byte2;

    if (h_fin) {
        byte1 = h_opcode | 0x80;
    } else {
        byte1 = h_opcode;
    }

    if (ln < 126) {
        byte2 = ln;
        if (!priv->iamServer) {
            byte2 |= 0x80;
        }
        gbuf_append(gbuf, (char *)&byte1, 1);
        gbuf_append(gbuf, (char *)&byte2, 1);

    } else if (ln <= 0xFFFF) {
        byte2 = 126;
        if (!priv->iamServer) {
            byte2 |= 0x80;
        }
        gbuf_append(gbuf, (char *)&byte1, 1);
        gbuf_append(gbuf, (char *)&byte2, 1);
        uint16_t u16 = htons((uint16_t)ln);
        gbuf_append(gbuf, (char *)&u16, 2);

    } else if (ln <= 0xFFFFFFFF) {
        byte2 = 127;
        if (!priv->iamServer) {
            byte2 |= 0x80;
        }
        gbuf_append(gbuf, (char *)&byte1, 1);
        gbuf_append(gbuf, (char *)&byte2, 1);
        uint64_t u64 = htobe64((uint64_t)ln);
        gbuf_append(gbuf, (char *)&u64, 8);

    } else {
        log_error(0,
            "gobj",         "%s", gobj_full_name(gobj),
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_PARAMETER_ERROR,
            "msg",          "%s", "data TOO LONG",
            "ln",           "%d", ln,
            NULL
        );
        return -1;
    }

    return 0;
}

/***************************************************************************
 *  Copied from http://pine.cs.yale.edu/pinewiki/C/Randomization
 ***************************************************************************/
PRIVATE uint32_t dev_urandom(void)
{
    uint32_t mask_key;

    srand(time(0));
    mask_key = rand();

    return mask_key;
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE int _write_control_frame(hgobj gobj, char h_fin, char h_opcode, char *data, size_t ln)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);
    GBUFFER *gbuf;

    gbuf = gbuf_create(
        14+ln,
        14+ln,
        0,
        h_opcode==OPCODE_TEXT_FRAME?codec_utf_8:codec_binary
    );
    if(!gbuf) {
        log_error(0,
            "gobj",         "%s", gobj_full_name(gobj),
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_INTERNAL_ERROR,
            "msg",          "%s", "gbuf_create() FAILED",
            NULL
        );
        return -1;
    }
    _add_frame_header(gobj, gbuf, h_fin, h_opcode, ln);

    if (!priv->iamServer) {
        uint32_t mask_key = dev_urandom();
        gbuf_append(gbuf, (char *)&mask_key, 4);
        if(ln) {
            mask((char *)&mask_key, data, ln);
        }
    }

    if(ln) {
        gbuf_append(gbuf, data, ln);
    }

    json_t *kw = json_pack("{s:I}",
        "gbuffer", (json_int_t)(size_t)gbuf
    );
    if(h_opcode == OPCODE_CONTROL_CLOSE) {
        //TODO NOOOO disconnect_on_last_transmit
        //json_object_set_new(kw, "disconnect_on_last_transmit", json_true());
    }
    gobj_send_event(gobj_bottom_gobj(gobj), "EV_TX_DATA", kw, gobj);

    return 0;
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE void send_close_frame(hgobj gobj, int status, const char *reason)
{
    uint32_t status_code;

    status_code = status;
    if(status_code < STATUS_NORMAL || status_code >= STATUS_MAXIMUM)
        status_code = STATUS_NORMAL;
    status_code = htons(status_code);
    _write_control_frame(gobj, TRUE, OPCODE_CONTROL_CLOSE, (char *)&status_code, 2);
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE void ws_close(hgobj gobj, int code, const char *reason)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    // Change firstly for avoid new messages from client
    gobj_change_state(gobj, "ST_DISCONNECTED");

    if(!priv->close_frame_sent) {
        priv->close_frame_sent = TRUE;
        send_close_frame(gobj, code, reason);
    }

    if(priv->iamServer) {
        hgobj tcp0 = gobj_bottom_gobj(gobj);
        if(gobj_is_running(tcp0)) {
            gobj_stop(tcp0);
        }
    }
    set_timeout(priv->timer, gobj_read_int32_attr(gobj, "timeout_close"));
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE void ping(hgobj gobj)
{
    _write_control_frame(gobj, TRUE, OPCODE_CONTROL_PING, 0, 0);
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE void pong(hgobj gobj, char *bf, size_t ln)
{
    _write_control_frame(gobj, TRUE, OPCODE_CONTROL_PONG, bf, ln);
}

/***************************************************************************
 *  Reset variables for a new read.
 ***************************************************************************/
PRIVATE int framehead_prepare_new_frame(FRAME_HEAD *frame)
{
    /*
     *  state of frame
     */
    frame->busy = 1;    //in half of header
    frame->header_complete = 0; // Set True when header is completed

    /*
     *  must do
     */
    frame->must_read_2_extended_payload_length = 0;
    frame->must_read_8_extended_payload_length = 0;
    frame->must_read_masking_key = 0;
    frame->must_read_payload_data = 0;

    /*
     *  information of frame
     */
    memset(frame->masking_key, 0, sizeof(frame->masking_key));
    frame->frame_length = 0;

    return 0;
}

/***************************************************************************
 *  Decode the two bytes head.
 ***************************************************************************/
PRIVATE int decode_head(FRAME_HEAD *frame, char *data)
{
    unsigned char byte1, byte2;

    byte1 = *(data+0);
    byte2 = *(data+1);

    /*
     *  decod byte1
     */
    frame->h_fin = byte1 & 0x80;
    frame->h_reserved_bits = byte1 & 0x70;
    frame->h_opcode = byte1 & 0x0f;

    /*
     *  decod byte2
     */
    frame->h_mask = byte2 & 0x80;
    frame->h_payload_len = byte2 & 0x7f;

    /*
     *  analize
     */

    if (frame->h_mask) {
        /*
         *  must read 4 bytes of masking key
         */
        frame->must_read_masking_key = TRUE;
    }

    int ln = frame->h_payload_len;
    if (ln == 0) {
        /*
         * no data to read
         */
    } else {
        frame->must_read_payload_data = TRUE;
        if (ln < 126) {
            /*
             *  Got all we need to read data
             */
            frame->frame_length = frame->h_payload_len;
        } else if(ln == 126) {
            /*
             *  must read 2 bytes of extended payload length
             */
            frame->must_read_2_extended_payload_length = TRUE;
        } else {/* ln == 127 */
            /*
             *  must read 8 bytes of extended payload length
             */
            frame->must_read_8_extended_payload_length = TRUE;
        }
    }

    return 0;
}

/***************************************************************************
 *  Consume input data to get and analyze the frame header.
 *  Return the consumed size.
 ***************************************************************************/
PRIVATE int framehead_consume(FRAME_HEAD *frame, istream istream, char *bf, int len)
{
    int total_consumed = 0;
    int consumed;
    char *data;

    /*
     *
     */
    if (!frame->busy) {
        /*
         * waiting the first two byte's head
         */
        istream_read_until_num_bytes(istream, 2, 0); // idempotent
        consumed = istream_consume(istream, bf, len);
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
        decode_head(frame, data);
    }

    /*
     *  processing extended header
     */
    if (frame->must_read_2_extended_payload_length) {
        istream_read_until_num_bytes(istream, 2, 0);  // idempotent
        consumed = istream_consume(istream, bf, len);
        total_consumed += consumed;
        bf += consumed;
        len -= consumed;
        if(!istream_is_completed(istream)) {
            return total_consumed;  // wait more data
        }

        /*
         *  Read 2 bytes of extended payload length
         */
        data = istream_extract_matched_data(istream, 0);
        uint16_t word;
        memcpy((char *)&word, data, 2);
        frame->frame_length = ntohs(word);
        frame->must_read_2_extended_payload_length = FALSE;
    }

    if (frame->must_read_8_extended_payload_length) {
        istream_read_until_num_bytes(istream, 8, 0);  // idempotent
        consumed = istream_consume(istream, bf, len);
        total_consumed += consumed;
        bf += consumed;
        len -= consumed;
        if(!istream_is_completed(istream)) {
            return total_consumed;  // wait more data
        }

        /*
         *  Read 8 bytes of extended payload length
         */
        data = istream_extract_matched_data(istream, 0);
        uint64_t ddword;
        memcpy((char *)&ddword, data, 8);
        frame->frame_length = be64toh(ddword);
        frame->must_read_8_extended_payload_length = FALSE;
    }

    if (frame->must_read_masking_key) {
        istream_read_until_num_bytes(istream, 4, 0);  // idempotent
        consumed = istream_consume(istream, bf, len);
        total_consumed += consumed;
        bf += consumed;
        len -= consumed;
        if(!istream_is_completed(istream)) {
            return total_consumed;  // wait more data
        }

        /*
         *  Read 4 bytes of masking key
         */
        data = istream_extract_matched_data(istream, 0);
        memcpy(frame->masking_key, data, 4);
        frame->must_read_masking_key = FALSE;
    }

    frame->header_complete = TRUE;
    return total_consumed;
}

/***************************************************************************
 *  Unmask data. Return a new gbuf with the data unmasked
 ***************************************************************************/
PRIVATE GBUFFER * unmask_data(hgobj gobj, GBUFFER *gbuf, uint8_t *h_mask)
{
    GBUFFER *unmasked;

    unmasked = gbuf_create(4*1024, gbmem_get_maximum_block(), 0, gbuf->encoding);

    char *p;
    size_t ln = gbuf_leftbytes(gbuf);
    for(size_t i=0; i<ln; i++) {
        p = gbuf_get(gbuf, 1);
        if(p) {
            *p = (*p) ^ h_mask[i % 4];
            gbuf_append(unmasked, p, 1);
        } else {
            log_error(0,
                "gobj",         "%s", gobj_full_name(gobj),
                "function",     "%s", __FUNCTION__,
                "msgset",       "%s", MSGSET_INTERNAL_ERROR,
                "msg",          "%s", "gbuf_get() return NULL",
                NULL
            );
        }
    }
    gbuf_decref(gbuf);
    return unmasked;
}

/***************************************************************************
 *  Check utf8
 ***************************************************************************/
PRIVATE BOOL check_utf8(GBUFFER *gbuf)
{
    // Available in jannson library
    extern int utf8_check_string(const char *string, size_t length);
    size_t length = gbuf_leftbytes(gbuf);
    const char *string = gbuf_cur_rd_pointer(gbuf);

    return utf8_check_string(string, length);
}

/***************************************************************************
 *  Process the completed frame
 ***************************************************************************/
PRIVATE int frame_completed(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);
    FRAME_HEAD *frame_head = &priv->frame_head;

    GBUFFER *unmasked = 0;

    if (frame_head->frame_length) {
        unmasked = istream_pop_gbuffer(priv->istream_payload);
        istream_destroy(priv->istream_payload);
        priv->istream_payload = 0;

        size_t ln = gbuf_leftbytes(unmasked);
        if(ln != frame_head->frame_length) {
            log_error(0,
                "gobj",         "%s", gobj_full_name(gobj),
                "function",     "%s", __FUNCTION__,
                "msgset",       "%s", MSGSET_INTERNAL_ERROR,
                "msg",          "%s", "BAD message LENGTH",
                "frame_length", "%d", frame_head->frame_length,
                "ln",           "%d", ln,
                NULL
            );
        }

        if (frame_head->h_mask) {
            unmasked = unmask_data(
                gobj,
                unmasked,
                frame_head->masking_key
            );
        }
    }

    if (frame_head->h_fin) {
        /*------------------------------*
         *          Final
         *------------------------------*/
        int operation;

        /*------------------------------*
         *  Get data
         *------------------------------*/
        operation = frame_head->h_opcode;

        if(operation == OPCODE_CONTINUATION_FRAME) {
            /*---------------------------*
             *  End of Fragmented data
             *---------------------------*/
            if (!priv->gbuf_message) {
                // No gbuf_message available?
                if(unmasked) {
                    gbuf_decref(unmasked);
                    unmasked = 0;
                }
                ws_close(gobj, STATUS_PROTOCOL_ERROR, 0);
                return -1;
            }
            operation = priv->message_head.h_opcode;
            memset(&priv->message_head, 0, sizeof(FRAME_HEAD));
            if(unmasked) {
                gbuf_append_gbuf(priv->gbuf_message, unmasked);
                gbuf_decref(unmasked);
                unmasked = 0;
            }
            unmasked = priv->gbuf_message;
            priv->gbuf_message = 0;

        } else {
            /*-------------------------------*
             *  End of NOT Fragmented data
             *-------------------------------*/
            if(operation <= OPCODE_BINARY_FRAME) {
                if (priv->gbuf_message) {
                    /*
                    *  We cannot have data of previous fragmented data
                    */
                    if(unmasked) {
                        gbuf_decref(unmasked);
                        unmasked = 0;
                    }
                    ws_close(gobj, STATUS_PROTOCOL_ERROR, 0);
                    return -1;
                }
            }

            if(!unmasked) {
                unmasked = gbuf_create(0,0,0,
                    frame_head->h_opcode==OPCODE_TEXT_FRAME?codec_utf_8:codec_binary
                );
                if(!unmasked) {
                    log_error(0,
                        "gobj",         "%s", gobj_full_name(gobj),
                        "function",     "%s", __FUNCTION__,
                        "msgset",       "%s", MSGSET_INTERNAL_ERROR,
                        "msg",          "%s", "gbuf_create() FAILED",
                        NULL
                    );
                    ws_close(gobj, STATUS_PROTOCOL_ERROR, 0);
                    return -1;
                }
            }
        }

        /*------------------------------*
         *  Check rsv
         *------------------------------*/
        int rsv;
        rsv = frame_head->h_reserved_bits;
        if(rsv) {
            /*
             *  If there is no extensions, break the connection
             */
            if(unmasked) {
                gbuf_decref(unmasked);
                unmasked = 0;
            }
            ws_close(gobj, STATUS_PROTOCOL_ERROR, 0);
            return 0;
        }

        /*------------------------------*
         *  Exec opcode
         *------------------------------*/
        switch(operation) {
            case OPCODE_TEXT_FRAME:
                {
                    /*
                     *  Check valid utf-8
                     */
                    if(!check_utf8(unmasked)) {
                        if(unmasked) {
                            gbuf_decref(unmasked);
                            unmasked = 0;
                        }
                        ws_close(gobj, STATUS_INVALID_PAYLOAD, 0);
                        return -1;
                    }
                    json_t *kw = json_pack("{s:I}",
                        "gbuffer", (json_int_t)(size_t)unmasked
                    );
                    gbuf_reset_rd(unmasked);
                    gobj_publish_event(gobj, "EV_ON_MESSAGE", kw);
                    unmasked = 0;
                }
                break;

            case OPCODE_BINARY_FRAME:
                {
                    json_t *kw = json_pack("{s:I}",
                        "gbuffer", (json_int_t)(size_t)unmasked
                    );
                    gbuf_reset_rd(unmasked);
                    gobj_publish_event(gobj, "EV_ON_MESSAGE", kw);
                    unmasked = 0;
                }
                break;

            case OPCODE_CONTROL_CLOSE:
                {
                    int close_code = 0;
                    if(frame_head->frame_length==0) {
                        close_code = STATUS_NORMAL;
                    } else if (frame_head->frame_length<2) {
                        close_code = STATUS_PROTOCOL_ERROR;
                    } else if (frame_head->frame_length>=126) {
                        close_code = STATUS_PROTOCOL_ERROR;
                    } else {
                        close_code = STATUS_NORMAL;

                        char *p = gbuf_get(unmasked, 2);

                        close_code = ntohs(*((uint16_t*)p));
                        if(!((close_code>=STATUS_NORMAL &&
                                close_code<=STATUS_UNSUPPORTED_DATA_TYPE) ||
                             (close_code>=STATUS_INVALID_PAYLOAD &&
                                close_code<=STATUS_UNEXPECTED_CONDITION) ||
                             (close_code>=3000 &&
                                close_code<5000))) {
                            close_code = STATUS_PROTOCOL_ERROR;
                        }
                        if(!check_utf8(unmasked)) {
                            close_code = STATUS_INVALID_PAYLOAD;
                        }
                    }
                    if(unmasked) {
                        gbuf_decref(unmasked);
                        unmasked = 0;
                    }
                    ws_close(gobj, close_code, 0);
                }
                return 0;

            case OPCODE_CONTROL_PING:
                if(frame_head->frame_length >= 126) {
                    if(unmasked) {
                        gbuf_decref(unmasked);
                        unmasked = 0;
                    }
                    ws_close(gobj, STATUS_PROTOCOL_ERROR, 0);
                    return 0;
                } else {
                    size_t ln = gbuf_leftbytes(unmasked);
                    char *p=0;
                    if(ln)
                        p = gbuf_get(unmasked, ln);
                    pong(gobj, p, ln);
                    gbuf_decref(unmasked);
                    unmasked = 0;
                }
                break;

            case OPCODE_CONTROL_PONG:
                if(unmasked) {
                    gbuf_decref(unmasked);
                    unmasked = 0;
                }
                // Must start inactivity time here?
                break;

            default:
                if(unmasked) {
                    gbuf_decref(unmasked);
                    unmasked = 0;
                }
                log_error(0,
                    "gobj",         "%s", gobj_full_name(gobj),
                    "function",     "%s", __FUNCTION__,
                    "msgset",       "%s", MSGSET_INTERNAL_ERROR,
                    "msg",          "%s", "Websocket BAD OPCODE",
                    "opcode",       "%d", operation,
                    NULL
                );
                ws_close(gobj, STATUS_PROTOCOL_ERROR, 0);
                return -1;
        }

    } else {
        /*------------------------------*
         *          NO Final
         *------------------------------*/
        /*--------------------------------*
         *  Message with several frames
         *--------------------------------*/

        /*-------------------------------------------*
         *  control message MUST NOT be fragmented.
         *-------------------------------------------*/
        if (frame_head->h_opcode > OPCODE_BINARY_FRAME) {
            if(unmasked) {
                gbuf_decref(unmasked);
                unmasked = 0;
            }
            ws_close(gobj, STATUS_PROTOCOL_ERROR, 0);
            return -1;
        }

        if(!priv->gbuf_message) {
            /*------------------------------*
             *      First frame
             *------------------------------*/
            if(frame_head->h_opcode == OPCODE_CONTINUATION_FRAME) {
                /*
                 *  The first fragmented frame must be a valid opcode.
                 */
                if(unmasked) {
                    gbuf_decref(unmasked);
                    unmasked = 0;
                }
                ws_close(gobj, STATUS_PROTOCOL_ERROR, 0);
                return -1;
            }
            memcpy(&priv->message_head, frame_head, sizeof(FRAME_HEAD));
            priv->gbuf_message = gbuf_create(
                4*1024,
                gbmem_get_maximum_block(),
                0,
                frame_head->h_opcode==OPCODE_TEXT_FRAME?codec_utf_8:codec_binary
            );
            if(!priv->gbuf_message) {
                log_error(0,
                    "gobj",         "%s", gobj_full_name(gobj),
                    "function",     "%s", __FUNCTION__,
                    "msgset",       "%s", MSGSET_INTERNAL_ERROR,
                    "msg",          "%s", "gbuf_create() FAILED",
                    NULL
                );
                if(unmasked) {
                    gbuf_decref(unmasked);
                    unmasked = 0;
                }
                ws_close(gobj, STATUS_MESSAGE_TOO_BIG, "");
                return -1;
            }
        } else {
            /*------------------------------*
             *      Next frame
             *------------------------------*/
            if(frame_head->h_opcode != OPCODE_CONTINUATION_FRAME) {
                /*
                 *  Next frames must be OPCODE_CONTINUATION_FRAME
                 */
                if(unmasked) {
                    gbuf_decref(unmasked);
                    unmasked = 0;
                }
                ws_close(gobj, STATUS_PROTOCOL_ERROR, 0);
                return -1;
            }
        }

        if(unmasked) {
            gbuf_append_gbuf(priv->gbuf_message, unmasked);
            gbuf_decref(unmasked);
            unmasked = 0;
        }
    }

    start_wait_frame_header(gobj);
    return 0;
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE int b64_encode_string(
    const unsigned char *in, int in_len, char *out, int out_size)
{
    static const char encode[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
                 "abcdefghijklmnopqrstuvwxyz0123456789+/";
    unsigned char triple[3];
    int i;
    int len;
    int line = 0;
    int done = 0;

    while (in_len) {
        len = 0;
        for (i = 0; i < 3; i++) {
            if (in_len) {
                triple[i] = *in++;
                len++;
                in_len--;
            } else
                triple[i] = 0;
        }
        if (!len)
            continue;

        if (done + 4 >= out_size)
            return -1;

        *out++ = encode[triple[0] >> 2];
        *out++ = encode[((triple[0] & 0x03) << 4) |
                         ((triple[1] & 0xf0) >> 4)];
        *out++ = (len > 1 ? encode[((triple[1] & 0x0f) << 2) |
                         ((triple[2] & 0xc0) >> 6)] : '=');
        *out++ = (len > 2 ? encode[triple[2] & 0x3f] : '=');

        done += 4;
        line += 4;
    }

    if (done + 1 >= out_size)
        return -1;

    *out++ = '\0';

    return done;
}

/***************************************************************************
 *  Send a http message
 ***************************************************************************/
PRIVATE int send_http_message(hgobj gobj, const char *http_message)
{
    GBUFFER *gbuf = gbuf_create(256, 4*1024, 0,0);
    if(!gbuf) {
        log_error(0,
            "gobj",         "%s", gobj_full_name(gobj),
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_INTERNAL_ERROR,
            "msg",          "%s", "gbuf_create() FAILED",
            NULL
        );
        return -1;
    }
    gbuf_append(gbuf, (char *)http_message, strlen(http_message));

    json_t *kw = json_pack("{s:I}",
        "gbuffer", (json_int_t)(size_t)gbuf
    );
    return gobj_send_event(gobj_bottom_gobj(gobj), "EV_TX_DATA", kw, gobj);
}

/***************************************************************************
 *  Send a http message with format
 ***************************************************************************/
PRIVATE int send_http_message2(hgobj gobj, const char *format, ...)
{
    va_list ap;

    GBUFFER *gbuf = gbuf_create(256, 4*1024, 0,0);
    if(!gbuf) {
        log_error(0,
            "gobj",         "%s", gobj_full_name(gobj),
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_INTERNAL_ERROR,
            "msg",          "%s", "gbuf_create() FAILED",
            NULL
        );
        return -1;
    }
    va_start(ap, format);
    gbuf_vprintf(gbuf, format, ap);
    va_end(ap);

    json_t *kw = json_pack("{s:I}",
        "gbuffer", (json_int_t)(size_t)gbuf
    );
    return gobj_send_event(gobj_bottom_gobj(gobj), "EV_TX_DATA", kw, gobj);
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE unsigned char *generate_uuid(unsigned char bf[16])
{
    char temp[20];
    struct timespec ts;

    clock_gettime(CLOCK_REALTIME, &ts);
    snprintf(temp, sizeof(temp), "%08lX%08lX", ts.tv_sec, ts.tv_nsec);
    memcpy(bf, temp, 16);
    return bf;
}

/***************************************************************************
 *  Send a request to upgrade to websocket
 ***************************************************************************/
PRIVATE BOOL do_request(
    hgobj gobj,
    const char *host,
    const char *port,
    const char *resource,
    json_t *options)
{
    GBUFFER *gbuf;
    unsigned char uuid[16];
    char key_b64[40];

    generate_uuid(uuid);
    b64_encode_string(uuid, 16, key_b64, sizeof(key_b64));

    gbuf = gbuf_create(256, 4*1024, 0,0);
    if(!gbuf) {
        log_error(0,
            "gobj",         "%s", gobj_full_name(gobj),
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_INTERNAL_ERROR,
            "msg",          "%s", "gbuf_create() FAILED",
            NULL
        );
        return FALSE;
    }
    gbuf_printf(gbuf, "GET %s HTTP/1.1\r\n", resource);
    gbuf_printf(gbuf, "User-Agent: yuneta-%s\r\n",  __yuneta_version__);
    gbuf_printf(gbuf, "Upgrade: websocket\r\n");
    gbuf_printf(gbuf, "Connection: Upgrade\r\n");
    if(atoi(port) == 80 || atoi(port) == 443) {
        gbuf_printf(gbuf, "Host: %s\r\n", host);
        gbuf_printf(gbuf, "Origin: %s\r\n", host);
    } else {
        gbuf_printf(gbuf, "Host: %s:%s\r\n", host, port);
        gbuf_printf(gbuf, "Origin: %s:%s\r\n", host, port);
    }

    gbuf_printf(gbuf, "Sec-WebSocket-Key: %s\r\n", key_b64);
    gbuf_printf(gbuf, "Sec-WebSocket-Version: %d\r\n", 13);

//     if "header" in options:
//         headers.extend(options["header"])
//
    gbuf_printf(gbuf, "\r\n");

    json_t *kw = json_pack("{s:I}",
        "gbuffer", (json_int_t)(size_t)gbuf
    );
    gobj_send_event(gobj_bottom_gobj(gobj), "EV_TX_DATA", kw, gobj);
    return TRUE;
}

/***************************************************************************
 *  Got the request: analyze it and send the response.
 ***************************************************************************/
PRIVATE BOOL do_response(hgobj gobj, GHTTP_PARSER *request)
{
    /*
     * Websocket only supports GET method
     */
    if (request->http_parser.method != HTTP_GET) {
        const char *data =
            "HTTP/1.1 405 Method Not Allowed\r\n"
            "Allow: GET\r\n"
            "Connection: Close\r\n"
            "\r\n";
        send_http_message(gobj, data);
        return FALSE;
    }

//     if hasattr(self.request, 'environ'):
//         environ = self.request.environEV_SET_TIMER
//     else:
//         environ = build_environment(self.request, '', '', '')
//
//     user_agent = environ.get("HTTP_USER_AGENT", "").lower()
//     ln = len("ginsfsm")
//     if len(user_agent) >= ln:
//         if user_agent[0:ln] == "ginsfsm":
//             self.ginsfsm_user_agent = True
//
    /*
     *  Upgrade header should be present and should be equal to WebSocket
     */
    const char *upgrade = kw_get_str(request->jn_headers, "UPGRADE", 0, 0);
    if(!upgrade || strcasecmp(upgrade, "websocket")!=0) {
        const char *data =
            "HTTP/1.1 400 Bad Request\r\n"
            "Connection: Close\r\n"
            "\r\n"
            "Can \"Upgrade\" only to \"WebSocket\".";
        send_http_message(gobj, data);
        return FALSE;
    }

    /*
     *  Connection header should be upgrade.
     *  Some proxy servers/load balancers
     *  might mess with it.
     */

//     connection = list(
//         map(
//             lambda s: s.strip().lower(),
//             environ.get("HTTP_CONNECTION", "").split(",")
//         )
//     )
//     if "upgrade" not in connection:

    const char *connection = kw_get_str(request->jn_headers, "CONNECTION", 0, 0);
    if(!connection || !strstr(connection, "Upgrade")) {
        const char *data =
            "HTTP/1.1 400 Bad Request\r\n"
            "Connection: Close\r\n"
            "\r\n"
            "\"Connection\" must be \"Upgrade\".";
        send_http_message(gobj, data);
        return FALSE;
    }

    /*
     *  The difference between version 8 and 13 is that in 8 the
     *  client sends a "Sec-Websocket-Origin" header
     *  and in 13 it's simply "Origin".
     */
    const char *version = kw_get_str(request->jn_headers, "SEC-WEBSOCKET-VERSION", 0, 0);
    if(!version || strtol(version, (char **) NULL, 10) < 8) {
        const char *data =
            "HTTP/1.1 426 Upgrade Required\r\n"
            "Sec-WebSocket-Version: 13\r\n\r\n";
        send_http_message(gobj, data);
        return FALSE;
    }

    const char *host = kw_get_str(request->jn_headers, "HOST", 0, 0);
    if(!host) {
        const char *data =
            "HTTP/1.1 400 Bad Request\r\n"
            "Connection: Close\r\n"
            "\r\n"
            "Missing Host header.";
        send_http_message(gobj, data);
        return FALSE;
    }

    const char *sec_websocket_key = kw_get_str(
        request->jn_headers,
        "SEC-WEBSOCKET-KEY",
        0,
        0
    );
    if(!sec_websocket_key) {
        const char *data =
            "HTTP/1.1 400 Bad Request\r\n"
            "Connection: Close\r\n"
            "\r\n"
            "Sec-Websocket-Key is missing or an invalid key.";
        send_http_message(gobj, data);
        return FALSE;
    }

//     key = environ.get("HTTP_SEC_WEBSOCKET_KEY", None)
//     if not key or len(base64.b64decode(bytes_(key))) != 16:
//         data = bytes_(
//             "HTTP/1.1 400 Bad Request\r\n"
//             "Connection: Close\r\n"
//             "\r\n"
//             "Sec-Websocket-Key is invalid key."
//         )
//         #self.send_event(self.tcp0, 'EV_TX_DATA', data=data)
//         #return False
//
    /*------------------------------*
     *  Accept the connection
     *------------------------------*/
    {
        char subprotocol_header[512] = {0};
        const char *subprotocol = kw_get_str(
            request->jn_headers,
            "SEC-WEBSOCKET-PROTOCOL",
            "",
            0
        );
        if(!empty_string(subprotocol)) {
            // TODO accept all by now, future: publish to top to validate protocol
            snprintf(subprotocol_header, sizeof(subprotocol_header), "%s: %s\r\n",
                "Sec-Websocket-Protocol",
                subprotocol
            );
        }

//        subprotocols = [s.strip() for s in subprotocols.split(',')]
//        if subprotocols:
//         selected = self.select_subprotocol(subprotocols)
//         if selected:
//             assert selected in subprotocols
//             subprotocol_header = "Sec-WebSocket-Protocol: %s\r\n" % (
//                 selected)

        static const char *magic = "258EAFA5-E914-47DA-95CA-C5AB0DC85B11";
        SHA1Context shactx;
        char concat[100], sha[20], b64_sha[sizeof(sha) * 2];
        char sha1buf[45];
        unsigned char sha1mac[20];

        SHA1Reset(&shactx);
        snprintf(concat, sizeof(concat), "%s%s", sec_websocket_key, magic);
        SHA1Input(&shactx, (unsigned char *) concat, strlen(concat));
        SHA1Result(&shactx);
        snprintf(sha1buf, sizeof(sha1buf),
            "%08x%08x%08x%08x%08x",
            shactx.Message_Digest[0],
            shactx.Message_Digest[1],
            shactx.Message_Digest[2],
            shactx.Message_Digest[3],
            shactx.Message_Digest[4]
        );
        for (int n = 0; n < (strlen(sha1buf) / 2); n++) {
            sscanf(sha1buf + (n * 2), "%02hhx", sha1mac + n);
        }
        b64_encode_string((unsigned char *) sha1mac, sizeof(sha1mac), b64_sha, sizeof(b64_sha));

        send_http_message2(gobj,
            "HTTP/1.1 101 Switching Protocols\r\n"
            "Upgrade: websocket\r\n"
            "Connection: Upgrade\r\n"
            "Sec-WebSocket-Accept: %s\r\n"
            "%s"
            "\r\n",
                b64_sha,
                subprotocol_header
        );
    }
    return TRUE;
}

/***************************************************************************
 *  Parse a http message
 *  Return -1 if error: you must close the socket.
 *  Return 0 if no new request.
 *  Return 1 if new request available in `request`.
 ***************************************************************************/
PRIVATE int process_http(hgobj gobj, GBUFFER *gbuf, GHTTP_PARSER *parser)
{
    while (gbuf_leftbytes(gbuf)) {
        size_t ln = gbuf_leftbytes(gbuf);
        char *bf = gbuf_cur_rd_pointer(gbuf);
        int n = ghttp_parser_received(parser, bf, ln);
        if (n == -1) {
            // Some error in parsing
            const char *peername = gobj_read_str_attr(gobj, "peername");
            const char *sockname = gobj_read_str_attr(gobj, "sockname");
            log_error(0,
                "gobj",         "%s", gobj_full_name(gobj),
                "function",     "%s", __FUNCTION__,
                "msgset",       "%s", MSGSET_PROTOCOL_ERROR,
                "msg",          "%s", "http parser failed",
                "peername",     "%s", peername?peername:"",
                "sockname",     "%s", sockname?sockname:"",
                NULL
            );

            ghttp_parser_reset(parser);
            return -1;
        } else if (n > 0) {
            gbuf_get(gbuf, n);  // take out the bytes consumed
        }

        if (parser->message_completed || parser->http_parser.upgrade) {
            /*
             *  The cur_request (with the body) is ready to use.
             */
            return 1;
        }
    }
    return 0;
}




            /***************************
             *      Actions
             ***************************/




/***************************************************************************
 *  iam client. send the request
 ***************************************************************************/
PRIVATE int ac_connected(hgobj gobj, const char *event, json_t *kw, hgobj src)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    *priv->pconnected = 1;
    priv->close_frame_sent = FALSE;

    ghttp_parser_reset(priv->parsing_request);
    ghttp_parser_reset(priv->parsing_response);

    if (priv->iamServer) {
        /*
         * wait the request
         */
    } else {
        /*
         * send the request
         */
        const char *host = gobj_read_str_attr(gobj_bottom_gobj(gobj), "rHost");
        const char *port = gobj_read_str_attr(gobj_bottom_gobj(gobj), "rPort");
        if(host && port) {
            do_request(
                gobj,
                host,
                port,
                gobj_read_str_attr(gobj, "resource"),
                0
            );
        }
    }
    set_timeout(priv->timer, gobj_read_int32_attr(gobj, "timeout_handshake"));
    KW_DECREF(kw);
    return 0;
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE int ac_disconnected(hgobj gobj, const char *event, json_t *kw, hgobj src)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    *priv->pconnected = 0;

    if(gobj_is_volatil(src)) {
        gobj_set_bottom_gobj(gobj, 0);
    }

    ghttp_parser_reset(priv->parsing_request);
    ghttp_parser_reset(priv->parsing_response);

    if(priv->istream_payload) {
        istream_destroy(priv->istream_payload);
        priv->istream_payload = 0;
    }
    if (!priv->on_close_broadcasted) {
        priv->on_close_broadcasted = TRUE;
        gobj_publish_event(gobj, "EV_ON_CLOSE", 0);
    }
    if(priv->timer) {
        clear_timeout(priv->timer);
    }
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
    KW_DECREF(kw);
    return 0;
}

/***************************************************************************
 *  Process handshake rx data.
 *  We can be server or client.
 ***************************************************************************/
PRIVATE int ac_process_handshake(hgobj gobj, const char *event, json_t *kw, hgobj src)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);
    GBUFFER *gbuf = (GBUFFER *)(size_t)kw_get_int(kw, "gbuffer", 0, FALSE);

    if(priv->timer) {
        clear_timeout(priv->timer);
    }
    if (priv->iamServer) {
        /*
         * analyze the request and respond
         */
        int result = process_http(gobj, gbuf, priv->parsing_request);
        if (result < 0) {
            gobj_stop(gobj_bottom_gobj(gobj));

        } else if (result > 0) {
            int ok = do_response(gobj, priv->parsing_request);
            if (!ok) {
                /*
                 * request refused! Drop connection.
                 */
                gobj_stop(gobj_bottom_gobj(gobj));
            } else {
                /*------------------------------------*
                 *   Upgrade to websocket
                 *------------------------------------*/
                start_wait_frame_header(gobj);
                gobj_publish_event(gobj, "EV_ON_OPEN", 0);
                priv->on_open_broadcasted = TRUE;
                priv->on_close_broadcasted = FALSE;
            }
        }
    } else {
        /*
         * analyze the response
         */
        int result = process_http(gobj, gbuf, priv->parsing_response);
        if (result < 0) {
            gobj_send_event(gobj_bottom_gobj(gobj), "EV_DROP", 0, gobj);

        } else if (result > 0) {
            if (priv->parsing_response->http_parser.status_code == 101) {
                /*------------------------------------*
                 *   Upgrade to websocket
                 *------------------------------------*/
                start_wait_frame_header(gobj);
                gobj_publish_event(gobj, "EV_ON_OPEN", 0);
                priv->on_open_broadcasted = TRUE;
                priv->on_close_broadcasted = FALSE;
            } else {
                log_error(0,
                    "gobj",         "%s", gobj_full_name(gobj),
                    "function",     "%s", __FUNCTION__,
                    "msgset",       "%s", MSGSET_PARAMETER_ERROR,
                    "msg",          "%s", "NO 101 HTTP Response",
                    "status",       "%d", 0, //response->status,
                    NULL
                );
                size_t ln = gbuf_leftbytes(gbuf);
                char *bf = gbuf_cur_rd_pointer(gbuf);
                log_debug_dump(0, bf, ln, 0);

                priv->close_frame_sent = TRUE;
                priv->on_close_broadcasted = TRUE;
                ws_close(gobj, STATUS_PROTOCOL_ERROR, 0);
                gobj_send_event(gobj_bottom_gobj(gobj), "EV_DROP", 0, gobj);
            }
        }
    }
    KW_DECREF(kw);
    return 0;
}

/***************************************************************************
 *  Too much time waiting the handshake.
 ***************************************************************************/
PRIVATE int ac_timeout_waiting_handshake(hgobj gobj, const char *event, json_t *kw, hgobj src)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    log_info(0,
        "gobj",         "%s", gobj_full_name(gobj),
        "msgset",       "%s", MSGSET_PROTOCOL_ERROR,
        "msg",          "%s", "Timeout waiting websocket handshake",
        NULL
    );

    priv->on_close_broadcasted = TRUE;  // no on_open was broadcasted
    priv->close_frame_sent = TRUE;
    ws_close(gobj, STATUS_PROTOCOL_ERROR, 0);
    gobj_send_event(gobj_bottom_gobj(gobj), "EV_DROP", 0, gobj);
    KW_DECREF(kw);
    return 0;
}

/***************************************************************************
 *  Too much time waiting disconnected
 ***************************************************************************/
PRIVATE int ac_timeout_waiting_disconnected(hgobj gobj, const char *event, json_t *kw, hgobj src)
{
    log_warning(0,
        "gobj",         "%s", gobj_full_name(gobj),
        "msgset",       "%s", MSGSET_PROTOCOL_ERROR,
        "msg",          "%s", "Timeout waiting websocket disconnected",
        NULL
    );

    gobj_send_event(gobj_bottom_gobj(gobj), "EV_DROP", 0, gobj);
    KW_DECREF(kw);
    return 0;
}

/***************************************************************************
 *  Process the header.
 ***************************************************************************/
PRIVATE int ac_process_frame_header(hgobj gobj, const char *event, json_t *kw, hgobj src)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);
    GBUFFER *gbuf = (GBUFFER *)(size_t)kw_get_int(kw, "gbuffer", 0, FALSE);
    FRAME_HEAD *frame = &priv->frame_head;
    istream istream = priv->istream_frame;

    if(priv->pingT>0) {
        set_timeout(priv->timer, priv->pingT);
    }

    while (gbuf_leftbytes(gbuf)) {
        size_t ln = gbuf_leftbytes(gbuf);
        char *bf = gbuf_cur_rd_pointer(gbuf);
        int n = framehead_consume(frame, istream, bf, ln);
        if (n <= 0) {
            // Some error in parsing
            // on error do break the connection
            ws_close(gobj, STATUS_PROTOCOL_ERROR, "");
            break;
        } else if (n > 0) {
            gbuf_get(gbuf, n);  // take out the bytes consumed
        }

        if (frame->header_complete) {
//             printf("rx OPCODE=%d, FIN=%s, RSV=%d, PAYLOAD_LEN=%ld\n",
//                 frame->h_opcode,
//                 frame->h_fin?"true":"false",
//                 0,
//                 frame->frame_length
//             );
            if (frame->must_read_payload_data) {
                /*
                 *
                 */
                if(priv->istream_payload) {
                    istream_destroy(priv->istream_payload);
                    priv->istream_payload = 0;
                    log_error(0,
                        "gobj",         "%s", gobj_full_name(gobj),
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
                    log_error(0,
                        "gobj",         "%s", gobj_full_name(gobj),
                        "function",     "%s", __FUNCTION__,
                        "msgset",       "%s", MSGSET_MEMORY_ERROR,
                        "msg",          "%s", "no memory for istream_payload",
                        "frame_length", "%d", frame_length,
                        NULL
                    );
                    ws_close(gobj, STATUS_INVALID_PAYLOAD, "");
                    break;
                }
                priv->istream_payload = istream_create(
                    gobj,
                    4*1024,
                    gbmem_get_maximum_block(),
                    0,
                    frame->h_opcode==OPCODE_TEXT_FRAME?codec_utf_8:codec_binary
                );
                if(!priv->istream_payload) {
                    log_error(0,
                        "gobj",         "%s", gobj_full_name(gobj),
                        "function",     "%s", __FUNCTION__,
                        "msgset",       "%s", MSGSET_MEMORY_ERROR,
                        "msg",          "%s", "no memory for istream_payload",
                        "frame_length", "%d", frame_length,
                        NULL
                    );
                    ws_close(gobj, STATUS_MESSAGE_TOO_BIG, "");
                    break;
                }
                istream_read_until_num_bytes(priv->istream_payload, frame_length, 0);

                gobj_change_state(gobj, "ST_WAITING_PAYLOAD_DATA");
                return gobj_send_event(gobj, "EV_RX_DATA", kw, gobj);

            } else {
                if(frame_completed(gobj) == -1)
                    break;
            }
        }
    }

    KW_DECREF(kw);
    return 0;
}

/***************************************************************************
 *  No activity, send ping
 ***************************************************************************/
PRIVATE int ac_timeout_waiting_frame_header(hgobj gobj, const char *event, json_t *kw, hgobj src)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    if(priv->pingT > 0) {
        set_timeout(priv->timer, priv->pingT);
        ping(gobj);
    }

    KW_DECREF(kw);
    return 0;
}

/***************************************************************************
 *  Get payload data
 ***************************************************************************/
PRIVATE int ac_process_payload_data(hgobj gobj, const char *event, json_t *kw, hgobj src)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);
    GBUFFER *gbuf = (GBUFFER *)(size_t)kw_get_int(kw, "gbuffer", 0, FALSE);

    size_t bf_len = gbuf_leftbytes(gbuf);
    char *bf = gbuf_cur_rd_pointer(gbuf);

    int consumed = istream_consume(priv->istream_payload, bf, bf_len);
    if(consumed > 0) {
        gbuf_get(gbuf, consumed);  // take out the bytes consumed
    }
    if(istream_is_completed(priv->istream_payload)) {
        frame_completed(gobj);
    }
    if(gbuf_leftbytes(gbuf)) {
        return gobj_send_event(gobj, "EV_RX_DATA", kw, gobj);
    }

    KW_DECREF(kw);
    return 0;
}

/***************************************************************************
 *  Too much time waiting payload data
 ***************************************************************************/
PRIVATE int ac_timeout_waiting_payload_data(hgobj gobj, const char *event, json_t *kw, hgobj src)
{
    log_info(0,
        "gobj",         "%s", gobj_full_name(gobj),
        "msgset",       "%s", MSGSET_PROTOCOL_ERROR,
        "msg",          "%s", "Timeout waiting websocket PAYLOAD data",
        NULL
    );

    ws_close(gobj, STATUS_PROTOCOL_ERROR, "");
    KW_DECREF(kw);
    return 0;
}

/***************************************************************************
 *  Send data
 ***************************************************************************/
PRIVATE int ac_send_message(hgobj gobj, const char *event, json_t *kw, hgobj src)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);
    GBUFFER *gbuf_data = (GBUFFER *)(size_t)kw_get_int(kw, "gbuffer", 0, FALSE);
    if(!gbuf_data) {
        log_error(LOG_OPT_TRACE_STACK,
            "gobj",         "%s", gobj_full_name(gobj),
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_INTERNAL_ERROR,
            "msg",          "%s", "gbuf NULL",
            NULL
        );
        KW_DECREF(kw)
        return -1;
    }

    size_t ln = gbuf_leftbytes(gbuf_data);
    char h_opcode;

    if(gbuf_data->encoding == codec_binary)
        h_opcode = OPCODE_BINARY_FRAME;
    else
        h_opcode = OPCODE_TEXT_FRAME;

    /*-------------------------------------------------*
     *  Server: no need of mask or re-create the gbuf,
     *  send the header and resend the gbuf.
     *-------------------------------------------------*/
    if (priv->iamServer) {
        GBUFFER *gbuf_header = gbuf_create(
            14,
            14,
            0,
            h_opcode==OPCODE_TEXT_FRAME?codec_utf_8:codec_binary
        );
        _add_frame_header(gobj, gbuf_header, TRUE, h_opcode, ln);

        json_t *kww = json_pack("{s:I}",
            "gbuffer", (json_int_t)(size_t)gbuf_header
        );
        gobj_send_event(gobj_bottom_gobj(gobj), "EV_TX_DATA", kww, gobj);    // header
        gobj_send_event(gobj_bottom_gobj(gobj), "EV_TX_DATA", kw, gobj);     // paylaod
        return 0;
    }

    /*-------------------------------------------------*
     *  Client: recreate the gbuf for mask the data
     *-------------------------------------------------*/
    GBUFFER *gbuf = gbuf_create(
        14+ln,
        14+ln,
        0,
        h_opcode==OPCODE_TEXT_FRAME?codec_utf_8:codec_binary
    );
    _add_frame_header(gobj, gbuf, TRUE, h_opcode, ln);

    /*
     *  write the mask
     */
    uint32_t mask_key = dev_urandom();
    gbuf_append(gbuf, (char *)&mask_key, 4);

    /*
     *  write the data masked data
     */
    while((ln=gbuf_chunk(gbuf_data))>0) {
        char *p = gbuf_get(gbuf_data, ln);
        mask((char *)&mask_key, p, ln);
        gbuf_append(gbuf, p, ln);
    }

    json_t *kww = json_pack("{s:I}",
        "gbuffer", (json_int_t)(size_t)gbuf
    );
    gobj_send_event(gobj_bottom_gobj(gobj), "EV_TX_DATA", kww, gobj);

    KW_DECREF(kw);
    return 0;
}

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
 *                          FSM
 ***************************************************************************/
PRIVATE const EVENT input_events[] = {
    {"EV_RX_DATA",          0},
    {"EV_SEND_MESSAGE",     0},
    {"EV_TX_READY",         0},
    {"EV_TIMEOUT",          0},
    {"EV_CONNECTED",        0},
    {"EV_DISCONNECTED",     0},
    {"EV_STOPPED",          0},
    {"EV_DROP",             0},
    {NULL, 0}
};
PRIVATE const EVENT output_events[] = {
    {"EV_ON_OPEN",          0},
    {"EV_ON_CLOSE",         0},
    {"EV_ON_MESSAGE",       0},
    {NULL, 0}
};
PRIVATE const char *state_names[] = {
    "ST_DISCONNECTED",
    "ST_WAITING_HANDSHAKE",
    "ST_WAITING_FRAME_HEADER",
    "ST_WAITING_PAYLOAD_DATA",
    NULL
};

PRIVATE EV_ACTION ST_DISCONNECTED[] = {
    {"EV_CONNECTED",        ac_connected,                       "ST_WAITING_HANDSHAKE"},
    {"EV_DISCONNECTED",     ac_disconnected,                    0},
    {"EV_TIMEOUT",          ac_timeout_waiting_disconnected,    0},
    {"EV_STOPPED",          ac_stopped,                         0},
    {"EV_TX_READY",         0,                                  0},
    {0,0,0}
};
PRIVATE EV_ACTION ST_WAITING_HANDSHAKE[] = {
    {"EV_RX_DATA",          ac_process_handshake,               0},
    {"EV_DISCONNECTED",     ac_disconnected,                    "ST_DISCONNECTED"},
    {"EV_TIMEOUT",          ac_timeout_waiting_handshake,       0},
    {"EV_DROP",             ac_drop,                            0},
    {"EV_TX_READY",         0,                                  0},
    {0,0,0}
};
PRIVATE EV_ACTION ST_WAITING_FRAME_HEADER[] = {
    {"EV_RX_DATA",          ac_process_frame_header,            0},
    {"EV_SEND_MESSAGE",     ac_send_message,                    0},
    {"EV_DISCONNECTED",     ac_disconnected,                    "ST_DISCONNECTED"},
    {"EV_TIMEOUT",          ac_timeout_waiting_frame_header,    0},
    {"EV_DROP",             ac_drop,                            0},
    {"EV_TX_READY",         0,                                  0},
    {0,0,0}
};
PRIVATE EV_ACTION ST_WAITING_PAYLOAD_DATA[] = {
    {"EV_RX_DATA",          ac_process_payload_data,            0},
    {"EV_SEND_MESSAGE",     ac_send_message,                    0},
    {"EV_DISCONNECTED",     ac_disconnected,                    "ST_DISCONNECTED"},
    {"EV_TIMEOUT",          ac_timeout_waiting_payload_data,    0},
    {"EV_DROP",             ac_drop,                            0},
    {"EV_TX_READY",         0,                                  0},
    {0,0,0}
};

PRIVATE EV_ACTION *states[] = {
    ST_DISCONNECTED,
    ST_WAITING_HANDSHAKE,
    ST_WAITING_FRAME_HEADER,
    ST_WAITING_PAYLOAD_DATA,
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
    GCLASS_GWEBSOCKET_NAME,
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
    0, // cmds
    0, // gcflag
};

/***************************************************************************
 *              Public access
 ***************************************************************************/
PUBLIC GCLASS *gclass_gwebsocket(void)
{
    return &_gclass;
}
