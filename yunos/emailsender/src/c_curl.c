/***********************************************************************
 *          C_CURL.C
 *          GClass of CURL uv-mixin.
 *
 *          Mixin libcurl-uv-gobj
 *          Based on https://gist.github.com/clemensg/5248927
 *
 *          Copyright (c) 2014 by Niyamaka.
 *          All Rights Reserved.
 ***********************************************************************/
#include <string.h>
#include <stdio.h>
#include <uv.h>
#include <sys/stat.h>
#include <curl/curl.h>
#include "c_curl.h"

/***************************************************************************
 *              Constants
 ***************************************************************************/
/*
** Translation Table as described in RFC1113
*/
static const char cb64[]="ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

/***************************************************************************
 *              Structures
 ***************************************************************************/
typedef struct dl_uv_poll_s {
    DL_ITEM_FIELDS

    uv_poll_t uv_poll;
    curl_socket_t sockfd;
    hgobj gobj;
    hsdata sd_easy;
} dl_uv_poll_t;

/***************************************************************************
 *              Data
 ***************************************************************************/
PRIVATE BOOL __curl_initialized__ = FALSE;

/***************************************************************************
 *              Prototypes
 ***************************************************************************/
PRIVATE int _curl_global_init(hgobj gobj);
PRIVATE int _curl_global_cleanup(hgobj gobj);
PRIVATE int clean_sd_easy(hgobj gobj, hsdata sd_easy);

PRIVATE void start_timeout(
    CURLM *multi,
    long timeout_ms,
    void *userp
);
PRIVATE int on_handle_socket(
    CURL *easy,
    curl_socket_t s,
    int action,
    void *userp,
    void *socketp
);

/***************************************************************************
 *          Data: config, public data, private data
 ***************************************************************************/
PRIVATE sdata_desc_t easyTb_it[] = {
/*-ATTR-type------------name----------------flag------------------------default---------description----------*/
SDATA (DTP_POINTER,     "easyCurl",         0,                          0,              "easy curl object"),
SDATA (DTP_POINTER,     "gobj",             0,                          0,              "curl gobj"),
SDATA (DTP_STRING,      "url",              0,                          0,              "request url"),
SDATA (DTP_POINTER,     "dst_gobj",         0,                          0,              "destination gobj"),
SDATA (DTP_STRING,      "dst_event",        0,                          "EV_RESPONSE",  "destination event"),
SDATA (DTP_POINTER,     "dst_gbuffer",      0,                          0,              "destination gbuffer"),
SDATA (DTP_POINTER,     "src_gbuffer",      0,                          0,              "source gbuffer"),
SDATA (DTP_INTEGER,     "mail_id",          0,                          0,              "mail id currently processing"),
SDATA (DTP_STRING,      "mail_ref",         0,                          0,              "mail reference currently processing"),
SDATA (DTP_BOOLEAN,     "cleanup",          0,                          0,              "True when curl_easy_cleanup is done"),
SDATA (DTP_STRING,      "where",            0,                          0,              "write response to 'where' file"),
SDATA (DTP_POINTER,     "fd",               0,                          0,              "FILE descriptor if using 'where'"),
SDATA (DTP_POINTER,     "user_reference",   0,                          0,              "User reference"),

SDATA_END()
};

/*---------------------------------------------*
 *      Attributes - order affect to oid's
 *---------------------------------------------*/
PRIVATE sdata_desc_t tattr_desc[] = {
/*-DB----type-----------name----------------flag------------------------schema----------free_fn---------header-----------*/
SDATADB (ASN_ITER,      "easyTb",           0,                          easyTb_it,      sdata_destroy,   "Curl Easy table"),

/*-ATTR-type------------name----------------flag------------------------default---------description----------*/
SDATA (DTP_STRING,      "lHost",            SDF_RD,                     0,              "local ip"),
SDATA (DTP_STRING,      "lPort",            SDF_RD,                     0,              "local port"),
SDATA (DTP_STRING,      "peername",         SDF_RD,                     0,              "Peername"),
SDATA (DTP_STRING,      "sockname",         SDF_RD,                     0,              "Sockname"),

SDATA (DTP_POINTER,     "user_data",        0,                          0,              "user data"),
SDATA (DTP_POINTER,     "user_data2",       0,                          0,              "more user data"),
SDATA (DTP_POINTER,     "subscriber",       0,                          0,              "subscriber of output-events. Default if null is parent."),
SDATA_END()
};

/*---------------------------------------------*
 *      GClass trace levels
 *---------------------------------------------*/
enum {
    TRACE_CURL    = 0x0001,
};
PRIVATE const trace_level_t s_user_trace_level[16] = {
{"curl",         "Trace curl"},
{0, 0},
};

/*---------------------------------------------*
 *              Private data
 *---------------------------------------------*/

typedef struct _PRIVATE_DATA {
    hgobj timer;
    dl_list_t *easyTb;
    CURLM *curl_handle;
    struct curl_slist *recipients;
    dl_list_t dl_uv_polls;
} PRIVATE_DATA;

PRIVATE hgclass __gclass__ = 0;

/***************************************************************************
 *              Prototypes
 ***************************************************************************/




            /***************************
             *      Framework Methods
             ***************************/




/***************************************************************************
 *      Framework Method create
 ***************************************************************************/
PRIVATE void mt_create(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    _curl_global_init(gobj);

    dl_init(&priv->dl_uv_polls);

    priv->timer = gobj_create("", C_TIMER, 0, gobj);
    priv->easyTb = gobj_read_iter_attr(gobj, "easyTb");

    /*
     *  Do copy of heavy used parameters, for quick access.
     *  HACK The writable attributes must be repeated in mt_writing method.
     */

    hgobj subscriber = (hgobj)gobj_read_pointer_attr(gobj, "subscriber");
    if(!subscriber)
        subscriber = gobj_parent(gobj);
    gobj_subscribe_event(gobj, NULL, NULL, subscriber);
}

/***************************************************************************
 *      Framework Method writing
 ***************************************************************************/
PRIVATE void mt_writing(hgobj gobj, const char *path)
{
}

/***************************************************************************
 *      Framework Method destroy
 ***************************************************************************/
PRIVATE void mt_destroy(hgobj gobj)
{
    _curl_global_cleanup(gobj);
}

/***************************************************************************
 *      Framework Method start - return nonstart flag
 ***************************************************************************/
PRIVATE int mt_start(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    gobj_start(priv->timer);
    priv->curl_handle = curl_multi_init();
    curl_multi_setopt(priv->curl_handle, CURLMOPT_SOCKETFUNCTION, on_handle_socket);
    curl_multi_setopt(priv->curl_handle, CURLMOPT_SOCKETDATA, gobj);
    curl_multi_setopt(priv->curl_handle, CURLMOPT_TIMERFUNCTION, start_timeout);
    curl_multi_setopt(priv->curl_handle, CURLMOPT_TIMERDATA, gobj);
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

    //gobj_free_iter(priv->easyTb, FALSE, sdata_destroy);

    rc_resource_t *sd_easy; rc_instance_t *resource_i;
    while((resource_i=rc_first_instance(priv->easyTb, &sd_easy))) {
        BOOL cleanup = sdata_read_bool(sd_easy, "cleanup");
        if(!cleanup) {
            sdata_write_bool(sd_easy, "cleanup", TRUE);
            CURL *easyCurl = sdata_read_pointer(sd_easy, "easyCurl");
            curl_easy_cleanup(easyCurl);
            if(priv->recipients) {
                curl_slist_free_all(priv->recipients);
                priv->recipients = 0;
            }
        }
        clean_sd_easy(gobj, sd_easy);

        rc_delete_resource(sd_easy, sdata_destroy);
    }

    curl_multi_cleanup(priv->curl_handle);
    priv->curl_handle = 0;

    return 0;
}




            /***************************
             *      Local Methods
             ***************************/




/*
** encodeblock
**
** encode 3 8-bit binary bytes as 4 '6-bit' characters
*/
static void encodeblock( unsigned char *in, unsigned char *out, int len )
{
    out[0] = (unsigned char) cb64[ (int)(in[0] >> 2) ];
    out[1] = (unsigned char) cb64[ (int)(((in[0] & 0x03) << 4) | ((in[1] & 0xf0) >> 4)) ];
    out[2] = (unsigned char) (len > 1 ? cb64[ (int)(((in[1] & 0x0f) << 2) | ((in[2] & 0xc0) >> 6)) ] : '=');
    out[3] = (unsigned char) (len > 2 ? cb64[ (int)(in[2] & 0x3f) ] : '=');
}

/*
** encode
**
** base64 encode a stream adding padding and line breaks as per spec.
*/
PRIVATE int encode(hgobj gobj, FILE *infile, gbuffer_t *gbuf, int linesize )
{
    //PRIVATE_DATA *priv = gobj_priv_data(gobj);
    unsigned char in[3];
    unsigned char out[4];
    int i, len, blocksout = 0;
    int retcode = 0;

    *in = (unsigned char) 0;
    *out = (unsigned char) 0;
    while( feof( infile ) == 0 ) {
        len = 0;
        for( i = 0; i < 3; i++ ) {
            in[i] = (unsigned char) getc( infile );

            if( feof( infile ) == 0 ) {
                len++;
            }
            else {
                in[i] = (unsigned char) 0;
            }
        }
        if( len > 0 ) {
            encodeblock( in, out, len );
            for( i = 0; i < 4; i++ ) {
                if(gbuffer_append(gbuf, &out[i], 1) != 1) {
                    gobj_log_error(gobj, 0,
                        "function",     "%s", __FUNCTION__,
                        "msgset",       "%s", MSGSET_INTERNAL_ERROR,
                        "msg",          "%s", "gbuffer_append() FAILED",
                        NULL
                    );
                    retcode = -1;
                    break;
                }
            }
            blocksout++;
        }
        if( blocksout >= (linesize/4) || feof( infile ) != 0 ) {
            if( blocksout > 0 ) {
                gbuffer_append(gbuf, "\n", 1);
            }
            blocksout = 0;
        }
    }
    return( retcode );
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE off_t get_file_size(const char *path)
{
    struct stat64 st;
    if(stat64(path, &st)<0) {
        return 0;
    }
    off_t size = st.st_size;
    if(size < 0)
        size = 0;
    return size;
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE dl_uv_poll_t *new_uv_poll(hgobj gobj, hsdata sd_easy, curl_socket_t sockfd)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    dl_uv_poll_t *poll_item = gbmem_malloc(sizeof(dl_uv_poll_t));
    poll_item->sockfd = sockfd;
    poll_item->gobj = gobj;
    poll_item->sd_easy = sd_easy;

    if(gobj_trace_level(gobj) & TRACE_UV) {
        log_debug_printf(0, ">>>> uv_poll_init_socket p=%p, s=%d", &poll_item->uv_poll, sockfd);
    }
    uv_poll_init_socket(
        yuno_uv_event_loop(),
        &poll_item->uv_poll,
        sockfd
    );
    poll_item->uv_poll.data = poll_item;

    dl_add(&priv->dl_uv_polls, poll_item);
    return poll_item;
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE dl_uv_poll_t *find_uv_poll(hgobj gobj, curl_socket_t sockfd)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    dl_uv_poll_t *poll_item = dl_first(&priv->dl_uv_polls);
    while(poll_item) {
        if(poll_item->sockfd == sockfd) {
            return poll_item;
        }
        poll_item = dl_next(poll_item);
    }
    return 0;
}

/***************************************************************************
  *
  ***************************************************************************/
PRIVATE void on_close_cb(uv_handle_t* handle)
{
    dl_uv_poll_t *poll_item = handle->data;
    hgobj gobj = poll_item->gobj;
    if(gobj) {
        if(gobj_trace_level(gobj) & TRACE_UV) {
            log_debug_printf(0, "<<<< on_close_cb poll p=%p", handle);
        }
    }
    gbmem_free(poll_item);
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE void rm_uv_poll(hgobj gobj, dl_uv_poll_t *poll_item)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    dl_delete(&priv->dl_uv_polls, poll_item, 0);
    if(gobj_trace_level(gobj) & TRACE_UV) {
        log_debug_printf(0, ">>>> uv_poll_stop & uv_close p=%p", &poll_item->uv_poll);
    }
    uv_poll_stop(&poll_item->uv_poll);
    uv_close((uv_handle_t*)&poll_item->uv_poll, on_close_cb);
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE int _curl_global_init(hgobj gobj)
{
    if(!__curl_initialized__) {
        CURLcode ret = curl_global_init_mem(
            CURL_GLOBAL_ALL,    //flags,
            gbmem_malloc,       //curl_malloc_callback
            gbmem_free,         //curl_free_callback
            gbmem_realloc,      //curl_realloc_callback
            gbmem_strdup,       //curl_strdup_callback,
            gbmem_calloc        //curl_calloc_callback
        );
        if(ret) {
            gobj_log_error(gobj, 0,
                "function",     "%s", __FUNCTION__,
                "msgset",       "%s", MSGSET_LIBCURL_ERROR,
                "msg",          "%s", "curl__curl_global_init() FAILED",
                "error",        "%s", curl_easy_strerror(ret),
                NULL
            );
        }
        __curl_initialized__ = 1;
    }
    return 0;
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE int _curl_global_cleanup(hgobj gobj)
{
    if(!gobj_instances(gobj)) {
        curl_global_cleanup();
        __curl_initialized__ = 0;
    }
    return 0;
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE int clean_sd_easy(hgobj gobj, hsdata sd_easy)
{
    gbuffer_t *gbuf = sdata_read_pointer(sd_easy, "src_gbuffer");
    if(gbuf) {
        sdata_write_pointer(sd_easy, "src_gbuffer", 0);
        gbuffer_decref(gbuf);
    }
    gbuf = sdata_read_pointer(sd_easy, "dst_gbuffer");
    if(gbuf) {
        sdata_write_pointer(sd_easy, "dst_gbuffer", 0);
        gbuffer_decref(gbuf);
    }
    return 0;
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE void start_timeout(CURLM *multi, long timeout_ms, void *userp)
{
    hgobj gobj = userp;
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    if (timeout_ms <= 0) {
        /* 0 means directly call socket_action, but we'll do it in a bit */
        timeout_ms = 1;
    }
    set_timeout(priv->timer, timeout_ms);
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE int send_response(hgobj gobj, hsdata sd_easy, int result)
{
    hgobj dst = sdata_read_pointer(sd_easy, "dst_gobj");
    int mail_id = sdata_read_int32(sd_easy, "mail_id");
    const char *mail_ref = sdata_read_str(sd_easy, "mail_ref");
    const char *url = sdata_read_str(sd_easy, "url");
    const char *where = sdata_read_str(sd_easy, "where");
    json_t *kw_curl = json_pack("{s:i, s:s, s:s, s:s, s:i, s:I, s:I, s:I, s:I}",
        "result", result,
        "url", url,
        "where", where,
        "mail_ref", mail_ref,
        "mail_id", mail_id,
        "fd", (json_int_t)(size_t)sdata_read_pointer(sd_easy, "fd"),
        "sd_easy", (json_int_t)(size_t)sd_easy,
        "gbuffer", (json_int_t)(size_t)sdata_read_pointer(sd_easy, "dst_gbuffer"),
        "user_reference", (json_int_t)(size_t)sdata_read_pointer(sd_easy, "user_reference")
    );
    sdata_write_pointer(sd_easy, "dst_gbuffer", 0);
    if(result) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_LIBCURL_ERROR,
            "result",       "%d", result,
            "msg",          "%s", "curl FAILED",
            "error",        "%s", curl_easy_strerror(result),
            NULL
        );
    }
    const char *dst_event = sdata_read_str(sd_easy, "dst_event");
    gobj_send_event(dst, dst_event, kw_curl, gobj);
    return 0;
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE void curl_perform(uv_poll_t *uv_poll, int status, int events)
{
    dl_uv_poll_t *poll_item = uv_poll->data;
    if(!poll_item) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_INTERNAL_ERROR,
            "msg",          "%s", "poll_item NULL",
            NULL
        );
        return;
    }
    hsdata sd_easy = poll_item->sd_easy;
    hgobj gobj = poll_item->gobj;
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    int running_handles = 0;
    int flags = 0;
    if (events & UV_READABLE) flags |= CURL_CSELECT_IN;
    if (events & UV_WRITABLE) flags |= CURL_CSELECT_OUT;
    if(status) {
        flags |= CURL_CSELECT_ERR;
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_LIBUV_ERROR,
            "msg",          "%s", "uv_poll FAILED",
            "uv_error",     "%s", uv_err_name(status),
            NULL
        );
    }
    clear_timeout(priv->timer);

    curl_socket_t sockfd = poll_item->sockfd;
    curl_multi_socket_action(
        priv->curl_handle,
        sockfd,
        flags,
        &running_handles
    );

    CURLMsg *message;
    int pending = 0;
    while ((message = curl_multi_info_read(priv->curl_handle, &pending))) {
        CURL *handle = message->easy_handle;
        switch (message->msg) {
        case CURLMSG_DONE:
            sd_easy = 0;
            int ret = curl_easy_getinfo(handle, CURLINFO_PRIVATE, &sd_easy);
            if(ret) {
                gobj_log_error(gobj, 0,
                    "function",     "%s", __FUNCTION__,
                    "msgset",       "%s", MSGSET_LIBCURL_ERROR,
                    "msg",          "%s", "curl_easy_getinfo() FAILED",
                    "error",        "%s", curl_easy_strerror(ret),
                    NULL
                );
            }
            int result = message->data.result;
            /*
             *  Send response
             */
            send_response(gobj, sd_easy, result);

            /*
             *  Cleanup easy curl
             */
            BOOL cleanup = sdata_read_bool(sd_easy, "cleanup");
            if(!cleanup) {
                sdata_write_bool(sd_easy, "cleanup", TRUE);
                curl_easy_cleanup(handle);
                if(priv->recipients) {
                    curl_slist_free_all(priv->recipients);
                    priv->recipients = 0;
                }
            }
            clean_sd_easy(gobj, sd_easy);

            rc_delete_resource(sd_easy, sdata_destroy);

            poll_item->sd_easy = 0;
            break;

        default:
            gobj_log_error(gobj, 0,
                "function",     "%s", __FUNCTION__,
                "msgset",       "%s", MSGSET_LIBCURL_ERROR,
                "msg",          "%s", "curl_multi_info_read BAD RETURN",
                "code",         "%d", message->msg,
                NULL
            );
        }
    }
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE int on_handle_socket(
    CURL *easy,
    curl_socket_t s,
    int action,
    void *userp,
    void *socketp)
{
    hgobj gobj = userp;
    hsdata sd_easy= 0;
    int ret = curl_easy_getinfo(easy, CURLINFO_PRIVATE, &sd_easy);
    if(ret) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_LIBCURL_ERROR,
            "msg",          "%s", "curl_easy_getinfo() FAILED",
            "error",        "%s", curl_easy_strerror(ret),
            NULL
        );
    }
    if(!sd_easy) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_LIBCURL_ERROR,
            "msg",          "%s", "sd_easy NULL",
            NULL
        );
        return -1;
    }

    dl_uv_poll_t *poll_item = find_uv_poll(gobj, s);
    if(!poll_item) {
        poll_item = new_uv_poll(gobj, sd_easy, s);
    }
    if(!poll_item) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_INTERNAL_ERROR,
            "msg",          "%s", "uv_poll MUST NOT BE NULL",
            NULL
        );
        return 0;
    }
    uv_poll_t *uv_poll = &poll_item->uv_poll;

    switch (action) {
    case CURL_POLL_IN:
        if(gobj_trace_level(gobj) & TRACE_UV) {
            log_debug_printf(0, ">>>> uv_poll_in p=%p", uv_poll);
        }
        uv_poll_start(uv_poll, UV_READABLE, curl_perform);
        break;
    case CURL_POLL_OUT:
        if(gobj_trace_level(gobj) & TRACE_UV) {
            log_debug_printf(0, ">>>> uv_poll_out p=%p", uv_poll);
        }
        uv_poll_start(uv_poll, UV_WRITABLE, curl_perform);
        break;
    case CURL_POLL_INOUT:
        if(gobj_trace_level(gobj) & TRACE_UV) {
            log_debug_printf(0, ">>>> uv_poll_in_out p=%p", uv_poll);
        }
        uv_poll_start(uv_poll, UV_WRITABLE|UV_READABLE, curl_perform);
        break;
    case CURL_POLL_REMOVE:
        if(gobj_trace_level(gobj) & TRACE_UV) {
            log_debug_printf(0, ">>>> uv_poll_remove p=%p", uv_poll);
        }
        rm_uv_poll(gobj, poll_item);
        break;
    }
    return 0;
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE size_t write_callback(char *ptr, size_t size, size_t nmemb, void *userdata)
{
    hsdata sd_easy = userdata;

    gbuffer_t *gbuf = sdata_read_pointer(sd_easy, "dst_gbuffer");
    if(!gbuf) {
        gbuf = gbuffer_create(8*1024, 128*1024, 0, 0);
        sdata_write_pointer(sd_easy, "dst_gbuffer", gbuf);
    }
    gbuffer_append(gbuf, ptr, size*nmemb);
    return size*nmemb;
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE size_t read_callback(char *buffer, size_t size, size_t nitems, void *instream)
{
    hsdata sd_easy = instream;

    if((size == 0) || (nitems == 0) || ((size*nitems) < 1)) {
        return 0;
    }
    gbuffer_t *gbuf = sdata_read_pointer(sd_easy, "src_gbuffer");
    int len = MIN(size*nitems, gbuffer_chunk(gbuf));
    if(len > 0) {
        memmove(buffer, gbuffer_get(gbuf, len), len);
    }
    return len;
}

/***************************************************************************
 *
// EMAIL EXAMPLE
// static const char *payload_text[] = {
//   "Date: Mon, 29 Nov 2010 21:54:29 +1100\r\n",
//   "To: " TO "\r\n",
//   "From: " FROM "(Example User)\r\n",
//   "Cc: " CC "(Another example User)\r\n",
//   "Message-ID: <dcd7cb36-11db-487a-9f3a-e652a9458efd@rfcpedant.example.org>\r\n",
//   "Subject: SMTP TLS example message\r\n",
//   "\r\n", //empty line to divide headers from body, see RFC5322
//   "The body of the message starts here.\r\n",
//   "\r\n",
//   "It could be a lot of lines, could be MIME encoded, whatever.\r\n",
//   "Check RFC5322.\r\n",
//   NULL
// };
 *
 ***************************************************************************/
PRIVATE gbuffer_t *build_email(
    hgobj gobj,
    gbuffer_t *gbuf, // not owned
    const char *from,
    const char *from_beatiful,
    const char *to,
    const char *cc,
    const char *reply_to,
    const char *subject,
    BOOL is_html,
    const char *attachment,
    const char *inline_file_id
)
{
    off_t file_size = 0;
    size_t len = gbuffer_leftbytes(gbuf);
    if(!empty_string(attachment)) {
        file_size = get_file_size(attachment);
    }
    len += 4*1024;
    gbuffer_t *new_gbuf = gbuffer_create(len, len, file_size>0?(len+file_size*4):0, 0);

    gbuffer_printf(new_gbuf, "To: %s\r\n", to);
    if(!empty_string(from_beatiful)) {
        gbuffer_printf(new_gbuf, "From: %s\r\n", from_beatiful);
    } else {
        gbuffer_printf(new_gbuf, "From: %s\r\n", from);
    }
    if(!empty_string(cc)) {
        gbuffer_printf(new_gbuf, "Cc: %s\r\n", cc);
    }
    if(!empty_string(reply_to)) {
        gbuffer_printf(new_gbuf, "Reply-To: %s\r\n", reply_to);
    }
    gbuffer_printf(new_gbuf, "Subject: %s\r\n", subject);
    gbuffer_printf(new_gbuf, "MIME-Version: 1.0\r\n");
    gbuffer_printf(new_gbuf, "Content-Type: multipart/mixed;\r\n");
    gbuffer_printf(new_gbuf, " boundary=\"------------030203080101020302070708\"\r\n");
    gbuffer_printf(new_gbuf, "\r\nThis is a multi-part message in MIME format.\r\n");
    gbuffer_printf(new_gbuf, "--------------030203080101020302070708\r\n");

    gbuffer_printf(new_gbuf, "Content-Type: text/html; charset=UTF-8\r\n");
//         gbuffer_printf(new_gbuf, "Content-Type: text/plain; charset=UTF-8; format=Flowed\r\n");
//         gbuffer_printf(new_gbuf, "Content-Transfer-Encoding: 8bit\r\n");

    /*
     *  Mark the body
     */
    gbuffer_append(new_gbuf, "\r\n", 2);
    /*
     *  Now the body
     */
    if(is_html) {
        gbuffer_append_gbuf(new_gbuf, gbuf);
        gbuffer_append(new_gbuf, "\r\n", 2);
    } else {
        gbuffer_append_string(new_gbuf, "<html><body><pre>\r\n");
        gbuffer_append_gbuf(new_gbuf, gbuf);
        gbuffer_append_string(new_gbuf, "</pre></body></html>\r\n");
    }

    /*
     *  Now the attachment
     */
    if(file_size) {
        FILE *infile = fopen(attachment, "rb");
        if(infile) {
            gbuffer_printf(new_gbuf, "\r\n--------------030203080101020302070708\r\n");
            char *filename = basename(attachment);

            gbuffer_printf(new_gbuf, "Content-Type: text/plain; name=\"%s\"\r\n", filename);
            gbuffer_printf(new_gbuf, "Content-Transfer-Encoding: base64\r\n");
            if(!empty_string(inline_file_id)) {
                gbuffer_printf(new_gbuf, "Content-ID: <%s>\r\n", inline_file_id);
                gbuffer_printf(new_gbuf, "Content-Disposition: inline; filename=\"%s\"\r\n", filename);
            } else {
                gbuffer_printf(new_gbuf, "Content-Disposition: attachment; filename=\"%s\"\r\n", filename);
            }
            gbuffer_printf(new_gbuf, "\r\n");

            /* File content here */
            encode(gobj, infile, new_gbuf, 72);

            gbuffer_printf(new_gbuf, "\r\n--------------030203080101020302070708--\r\n");
        }
    }

    return new_gbuf;
}




            /***************************
             *      Actions
             ***************************/



/***************************************************************************
 *
 ***************************************************************************/
PRIVATE int ac_command(hgobj gobj, const char *event, json_t *kw, hgobj src)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    const char *url = kw_get_str(gobj, kw, "url", "", 0);
    const char *where = kw_get_str(gobj, kw, "where", "", 0);
    const char *username = kw_get_str(gobj, kw, "username", "", 0);
    const char *password = kw_get_str(gobj, kw, "password", "", 0);
    const char *command = kw_get_str(gobj, kw, "command", "", 0);
    const char *dst_event = kw_get_str(gobj, kw, "dst_event", "", 0);
    void * user_reference = (void *)(size_t)kw_get_int(gobj, kw, "user_reference", 0, 0);

    /*
     *  mail_id only for RETR & DELE
     */
    int mail_id = kw_get_int(gobj, kw, "mail_id", 0, FALSE);
    /*
     *  mail_ref only for SEND
     */
    const char *mail_ref = kw_get_str(gobj, kw, "mail_ref", "", 0);

    CURL *handle = curl_easy_init();
    if(!handle) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_LIBCURL_ERROR,
            "msg",          "%s", "curl_easy_init() FAILED",
            NULL
        );
        KW_DECREF(kw);
        return -1;
    }

    // __RESOURCE__

    const sdata_desc_t *schema = gobj_read_iter_schema(gobj, "easyTb");
    hsdata sd_easy = sdata_create(schema, gobj, 0, 0, 0, "easyTb");
    if(!sd_easy) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_LIBCURL_ERROR,
            "msg",          "%s", "table_create_row() FAILED",
            NULL
        );
        KW_DECREF(kw);
        return -1;
    }
    rc_add_instance(priv->easyTb, sd_easy, 0);

    // __RESOURCE__

    sdata_write_pointer(sd_easy, "gobj", gobj);
    sdata_write_pointer(sd_easy, "easyCurl", handle);
    if(!empty_string(dst_event)) {
        sdata_write_str(sd_easy, "dst_event", dst_event);
    }
    sdata_write_str(sd_easy, "url", url);
    sdata_write_str(sd_easy, "where", where);
    sdata_write_pointer(sd_easy, "dst_gobj", src);
    sdata_write_int32(sd_easy, "mail_id", mail_id);
    sdata_write_str(sd_easy, "mail_ref", mail_ref);
    sdata_write_pointer(sd_easy, "user_reference", user_reference);

    int ret = curl_easy_setopt(handle, CURLOPT_PRIVATE, sd_easy);
    if(ret) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_LIBCURL_ERROR,
            "msg",          "%s", "curl_easy_setopt() FAILED",
            "error",        "%s", curl_easy_strerror(ret),
            NULL
        );
    }
    if(!empty_string(username)) {
        curl_easy_setopt(handle, CURLOPT_USERNAME, username);
        curl_easy_setopt(handle, CURLOPT_PASSWORD, password);
        //curl_easy_setopt(handle, CURLOPT_USE_SSL, CURLUSESSL_ALL);
        curl_easy_setopt(handle, CURLOPT_USE_SSL, CURLUSESSL_TRY);

        curl_easy_setopt(handle, CURLOPT_SSL_VERIFYPEER, 0);
        curl_easy_setopt(handle, CURLOPT_SSL_VERIFYHOST, 0);
    }
    curl_easy_setopt(handle, CURLOPT_USERAGENT, "libcurl-agent/1.0");
    if(gobj_trace_level(gobj) & TRACE_CURL) {
        curl_easy_setopt(handle, CURLOPT_VERBOSE, 1L);
    }
    curl_easy_setopt(handle, CURLOPT_TCP_KEEPALIVE, 1L);
    curl_easy_setopt(handle, CURLOPT_TCP_NODELAY, 1L);
    curl_easy_setopt(handle, CURLOPT_FORBID_REUSE, 1L);

    curl_easy_setopt(handle, CURLOPT_WRITEFUNCTION, write_callback);
    curl_easy_setopt(handle, CURLOPT_WRITEDATA, sd_easy);
    curl_easy_setopt(handle, CURLOPT_READFUNCTION, read_callback);
    curl_easy_setopt(handle, CURLOPT_READDATA, sd_easy);

    if(strcmp(command, "LIST")==0) {
        char _url[256];
        snprintf(_url, sizeof(_url), "%s/", url);
        curl_easy_setopt(handle, CURLOPT_URL, _url);

    } else if(strcmp(command, "RETR")==0) {
        if(mail_id < 1) {
            gobj_log_error(gobj, 0,
                "function",     "%s", __FUNCTION__,
                "msgset",       "%s", MSGSET_PARAMETER_ERROR,
                "msg",          "%s", "mail_id WRONG",
                "command",      "%s", command,
                "mail_id",      "%d", mail_id,
                NULL
            );
            JSON_DECREF(kw);
            return -1;
        }
        char _url[256];
        snprintf(_url, sizeof(_url), "%s/%d", url, mail_id);
        curl_easy_setopt(handle, CURLOPT_URL, _url);

    } else if(strcmp(command, "DELE")==0) {
        if(mail_id < 1) {
            gobj_log_error(gobj, 0,
                "function",     "%s", __FUNCTION__,
                "msgset",       "%s", MSGSET_PARAMETER_ERROR,
                "msg",          "%s", "mail_id WRONG",
                "command",      "%s", command,
                "mail_id",      "%d", mail_id,
                NULL
            );
            JSON_DECREF(kw);
            return -1;
        }
        char _url[256];
        snprintf(_url, sizeof(_url), "%s/%d", url, mail_id);
        curl_easy_setopt(handle, CURLOPT_URL, _url);
        curl_easy_setopt(handle, CURLOPT_NOBODY, 1L);
        curl_easy_setopt(handle, CURLOPT_CUSTOMREQUEST, "DELE");

    } else if(strcmp(command, "SEND")==0) {
        const char *from = kw_get_str(gobj, kw, "from", 0, 0);
        if(empty_string(from)) {
            gobj_log_error(gobj, 0,
                "function",     "%s", __FUNCTION__,
                "msgset",       "%s", MSGSET_PARAMETER_ERROR,
                "msg",          "%s", "email FROM is NULL",
                NULL
            );
        }
        curl_easy_setopt(handle, CURLOPT_MAIL_FROM, from);

        if(priv->recipients) {
            gobj_log_error(gobj, 0,
                "function",     "%s", __FUNCTION__,
                "msgset",       "%s", MSGSET_INTERNAL_ERROR,
                "msg",          "%s", "recipients MUST BE NULL",
                "warning",      "%s", "Probably curl_easy_init handle is LOST: CURL MEMORY LOST",
                NULL
            );
            curl_slist_free_all(priv->recipients);
            priv->recipients = 0;
        }
        const char *cc = kw_get_str(gobj, kw, "cc", 0, 0);
        const char *to = kw_get_str(gobj, kw, "to", 0, 0);
        if(empty_string(to)) {
            gobj_log_error(gobj, 0,
                "function",     "%s", __FUNCTION__,
                "msgset",       "%s", MSGSET_PARAMETER_ERROR,
                "msg",          "%s", "email TO is NULL",
                NULL
            );
        }

        const char *test_email = gobj_read_str_attr(gobj_default_service(), "test_email");
        int list_size;
        if(!empty_string(test_email) && gobj_read_bool_attr(gobj_default_service(), "only_test")) {
            list_size = 1;
            priv->recipients = curl_slist_append(
                priv->recipients,
                test_email
            );
            to = test_email;
            cc = "";
        } else {
            BOOL add_test = gobj_read_bool_attr(gobj_default_service(), "add_test");

            const char **rcpt_list = split2(to, ",;", &list_size);
            for(int i=0; i<list_size; i++) {
                priv->recipients = curl_slist_append(
                    priv->recipients,
                    rcpt_list[i]
                );
            }
            split_free2(rcpt_list);

            if(!empty_string(cc)) {
                rcpt_list = split2(cc, ",;", &list_size);

                for(int i=0; i<list_size; i++) {
                    priv->recipients = curl_slist_append(
                        priv->recipients,
                        rcpt_list[i]
                    );
                }
                split_free2(rcpt_list);
            }
            if(add_test && !empty_string(test_email)) {
                priv->recipients = curl_slist_append(
                    priv->recipients,
                    test_email
                );
            }
        }

        // TODO V568 It's odd that 'sizeof()' operator evaluates the size of a pointer to a class, but not the size of the 'priv->recipients' class object.
        curl_easy_setopt(handle, CURLOPT_MAIL_RCPT, priv->recipients);

        const char *from_beatiful = kw_get_str(gobj, kw, "from_beatiful", 0, 0);
        const char *subject = kw_get_str(gobj, kw, "subject", 0, 0);
        const char *reply_to = kw_get_str(gobj, kw, "reply_to", 0, 0);
        BOOL is_html = kw_get_bool(gobj, kw, "is_html", 0, 0);
        gbuffer_t *gbuf = (gbuffer_t *)(size_t)kw_get_int(gobj, kw, "gbuffer", 0, 0);
        const char *attachment = kw_get_str(gobj, kw, "attachment", 0, 0);
        const char *inline_file_id = kw_get_str(gobj, kw, "inline_file_id", 0, 0);
        gbuffer_t *src_gbuffer = build_email(
            gobj,
            gbuf,
            from,
            from_beatiful,
            to,
            cc,
            reply_to,
            subject,
            is_html,
            attachment,
            inline_file_id
        );

        curl_off_t uploadsize = gbuffer_leftbytes(src_gbuffer);
        curl_easy_setopt(handle, CURLOPT_INFILESIZE_LARGE, uploadsize);
        sdata_write_pointer(sd_easy, "src_gbuffer", src_gbuffer);

        const char *url = kw_get_str(gobj, kw, "url", 0, 0);
        curl_easy_setopt(handle, CURLOPT_URL, url);
        curl_easy_setopt(handle, CURLOPT_UPLOAD, 1L);

    } else if(strcmp(command, "INPUT_FTP")==0) {
        curl_easy_setopt(handle, CURLOPT_URL, url);

    } else {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_PARAMETER_ERROR,
            "msg",          "%s", "command UNKNOWN",
            "command",      "%s", command,
            NULL
        );
        JSON_DECREF(kw);
        return -1;
    }

    curl_multi_add_handle(priv->curl_handle, handle);
    JSON_DECREF(kw);
    return 0;
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE int ac_drop_request(hgobj gobj, const char *event, json_t *kw, hgobj src)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);
    /*
     *  Cleanup easy query
     */
    hsdata sd_easy = (hsdata)(size_t)kw_get_int(gobj, kw, "sd_easy", 0, FALSE);
    CURL *easy = sdata_read_pointer(sd_easy, "easyCurl");
    curl_multi_remove_handle(priv->curl_handle, easy);
    BOOL cleanup = sdata_read_bool(sd_easy, "cleanup");
    if(!cleanup) {
        sdata_write_bool(sd_easy, "cleanup", TRUE);
        curl_easy_cleanup(easy);
        if(priv->recipients) {
            curl_slist_free_all(priv->recipients);
            priv->recipients = 0;
        }
    }
    BOOL stopped = sdata_read_bool(sd_easy, "stopped");
    if(stopped) {
        clean_sd_easy(gobj, sd_easy);
        rc_delete_resource(sd_easy, sdata_destroy);
    }
    JSON_DECREF(kw);
    return 0;
}

/***************************************************************************
 *  Child stopped
 ***************************************************************************/
PRIVATE int ac_stopped(hgobj gobj, const char *event, json_t *kw, hgobj src)
{
    JSON_DECREF(kw);
    return 0;
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE int ac_timeout(hgobj gobj, const char *event, json_t *kw, hgobj src)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    int running_handles;
    curl_multi_socket_action(
        priv->curl_handle,
        CURL_SOCKET_TIMEOUT,
        0,
        &running_handles
    );

    JSON_DECREF(kw);
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
    .mt_writing     = mt_writing
};

/*------------------------*
 *      GClass name
 *------------------------*/
GOBJ_DEFINE_GCLASS(C_CURL);

/***************************************************************************
 *          Create the GClass
 ***************************************************************************/
PRIVATE int create_gclass(gclass_name_t gclass_name)
{
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
        {EV_CURL_COMMAND,   ac_command,      0},
        {EV_DROP_REQUEST,   ac_drop_request, 0},
        {EV_TIMEOUT,        ac_timeout,      0},
        {EV_STOPPED,        ac_stopped,      0},
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
        {EV_CURL_COMMAND,   0},
        {EV_DROP_REQUEST,   0},
        {EV_TIMEOUT,        0},
        {EV_STOPPED,        0},
        {EV_RESPONSE,       0},
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
        lmt,
        tattr_desc,
        sizeof(PRIVATE_DATA),
        0,                  // acl
        0,                  // command table
        s_user_trace_level,
        gcflag_manual_start
    );
    if(!__gclass__) {
        return -1;
    }

    return 0;
}

/***************************************************************************
 *              Local methods table
 ***************************************************************************/
PRIVATE LMETHOD lmt[] = {
    {0, 0, 0}
};

/***************************************************************************
 *              Public access
 ***************************************************************************/
PUBLIC int register_c_curl(void)
{
    return create_gclass(C_CURL);
}
