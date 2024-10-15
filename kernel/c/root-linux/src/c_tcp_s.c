/****************************************************************************
 *          c_tcp_s.c
 *
 *          TCP Server
 *          Low level linux with io_uring
 *
 *          Copyright (c) 2023 Niyamaka.
 *          Copyright (c) 2024, ArtGins.
 *          All Rights Reserved.
 ****************************************************************************/
#include <string.h>
#include <netdb.h>

#include <parse_url.h>
#include <kwid.h>
#include <ytls.h>
#include <yunetas_ev_loop.h>
#include "c_timer.h"
#include "c_yuno.h"
#include "c_tcp_s.h"

/***************************************************************************
 *              Constants
 ***************************************************************************/

/***************************************************************************
 *              Structures
 ***************************************************************************/

/***************************************************************************
 *              Prototypes
 ***************************************************************************/
PRIVATE int yev_server_callback(yev_event_t *event);
//PRIVATE void on_close_cb(uv_handle_t* handle);
//PRIVATE void on_connection_cb(uv_stream_t *uv_server_socket, int status);


/***************************************************************************
 *          Data: config, public data, private data
 ***************************************************************************/

/*---------------------------------------------*
 *      Attributes - order affect to oid's
 *---------------------------------------------*/
PRIVATE sdata_desc_t tattr_desc[] = {
/*-ATTR-type------------name--------------------flag----------------default---------description---------- */
SDATA (DTP_JSON,        "crypto",               SDF_RD,             0,              "Crypto config"),
SDATA (DTP_BOOLEAN,     "use_ssl",              SDF_RD,             "false",        "True if schema is secure. Set internally"),
SDATA (DTP_INTEGER,     "connxs",               SDF_RD,             0,              "Current connections"),
SDATA (DTP_STRING,      "url",                  SDF_WR|SDF_PERSIST, 0,              "url listening"),
SDATA (DTP_STRING,      "lHost",                SDF_RD,             0,              "Listening ip, got from url"),
SDATA (DTP_STRING,      "lPort",                SDF_RD,             0,              "Listening port, got from url"),
SDATA (DTP_STRING,      "stopped_event_name",   SDF_RD,             "EV_STOPPED",   "Stopped event name"),
SDATA (DTP_BOOLEAN,     "only_allowed_ips",     SDF_RD,             0,              "Only allowed ips"),
SDATA (DTP_BOOLEAN,     "trace",                SDF_WR|SDF_PERSIST, 0,              "Trace TLS"),
SDATA (DTP_BOOLEAN,     "shared",               SDF_RD,             0,              "Share the port"),
SDATA (DTP_BOOLEAN,     "exitOnError",          SDF_RD,             "1",            "Exit if Listen failed"),
SDATA (DTP_JSON,        "child_tree_filter",    SDF_RD,             0,              "tree of chids to create on new accept"),

SDATA (DTP_STRING,      "top_name",             SDF_RD,             0,              "name of filter gobj"),
SDATA (DTP_STRING,      "top_gclass_name",      SDF_RD,             0,              "The name of a registered gclass to use in creation of the filter gobj"),
SDATA (DTP_POINTER,     "top_parent",           SDF_RD,             0,              "parent of the top filter gobj"),
SDATA (DTP_JSON,        "top_kw",               SDF_RD,             0,              "kw of filter gobj"),
SDATA (DTP_JSON,        "clisrv_kw",            SDF_RD,             0,              "kw of clisrv gobj"),
SDATA (DTP_POINTER,     "user_data",            0,                  0,              "user data"),
SDATA (DTP_POINTER,     "user_data2",           0,                  0,              "more user data"),
SDATA (DTP_POINTER,     "subscriber",           0,                  0,              "subscriber of output-events. Default if null is parent."),
SDATA_END()
};

/*---------------------------------------------*
 *      GClass trace levels
 *---------------------------------------------*/
enum {
    TRACE_LISTEN        = 0x0001,
    TRACE_NOT_ACCEPTED  = 0x0002,
    TRACE_ACCEPTED      = 0x0004,
};
PRIVATE const trace_level_t s_user_trace_level[16] = {
{"listen",          "Trace listen"},
{"not-accepted",    "Trace not accepted connections"},
{"accepted",        "Trace accepted connections"},
{0, 0},
};

/*---------------------------------------------*
 *              Private data
 *---------------------------------------------*/
typedef struct _PRIVATE_DATA {
    const char *url;
    BOOL exitOnError;

    const char *top_name;
    const char *top_gclass_name;
    hgobj top_parent;
    json_t * top_kw;
    json_t * clisrv_kw;
    BOOL trace;

    json_int_t *pconnxs;

    yev_event_t *yev_server_accept;
    BOOL uv_socket_open;
    int fd_listen;
    hytls ytls;
    BOOL use_ssl;

    hgobj subscriber;
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

    /*
     *  Do copy of heavy used parameters, for quick access.
     *  HACK The writable attributes must be repeated in mt_writing method.
     */
    SET_PRIV(url, gobj_read_str_attr)
    SET_PRIV(exitOnError, gobj_read_bool_attr)
    SET_PRIV(trace, gobj_read_bool_attr)

    SET_PRIV(top_name, gobj_read_str_attr)
    SET_PRIV(top_gclass_name, gobj_read_str_attr)
    SET_PRIV(top_parent, gobj_read_pointer_attr)
    SET_PRIV(top_kw, gobj_read_json_attr)
    SET_PRIV(clisrv_kw, gobj_read_json_attr)

    priv->pconnxs = gobj_danger_attr_ptr(gobj, "connxs");

    priv->subscriber = (hgobj)gobj_read_pointer_attr(gobj, "subscriber");
    if(!priv->subscriber)
        priv->subscriber = gobj_parent(gobj);
}

/***************************************************************************
 *      Framework Method writing
 ***************************************************************************/
PRIVATE void mt_writing(hgobj gobj, const char *path)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    IF_EQ_SET_PRIV(url, gobj_read_str_attr)

    ELIF_EQ_SET_PRIV(top_name, gobj_read_str_attr)
    ELIF_EQ_SET_PRIV(top_gclass_name, gobj_read_str_attr)
    ELIF_EQ_SET_PRIV(top_parent, gobj_read_pointer_attr)
    ELIF_EQ_SET_PRIV(top_kw, gobj_read_json_attr)
    ELIF_EQ_SET_PRIV(clisrv_kw, gobj_read_json_attr)
    ELIF_EQ_SET_PRIV(exitOnError, gobj_read_bool_attr)
    ELIF_EQ_SET_PRIV(trace, gobj_read_bool_attr)
    END_EQ_SET_PRIV()
}

/***************************************************************************
 *      Framework Method destroy
 ***************************************************************************/
PRIVATE void mt_destroy(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    EXEC_AND_RESET(ytls_cleanup, priv->ytls);

    if(gobj_current_state(gobj) != ST_STOPPED) {
        gobj_log_error(gobj, LOG_OPT_TRACE_STACK,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_LIBUV_ERROR,
            "msg",          "%s", "GObj NOT STOPPED. UV handler ACTIVE!",
            NULL
        );
    }
}

/***************************************************************************
 *      Framework Method start - return nonstart flag
 ***************************************************************************/
PRIVATE int mt_start(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    if(empty_string(priv->url)) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_CONNECT_DISCONNECT,
            "msg",          "%s", "URL NULL",
            NULL);
        if(priv->exitOnError) {
            exit(0); //WARNING exit with 0 to stop daemon watcher!
        } else {
            return -1;
        }
    }

    char schema[40];
    char host[120];
    char port[40];

    const char *url = gobj_read_str_attr(gobj, "url");
    if(parse_url(
        gobj,
        url,
        schema, sizeof(schema),
        host, sizeof(host),
        port, sizeof(port),
        0, 0,
        0, 0,
        FALSE
    )<0) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_PARAMETER_ERROR,
            "msg",          "%s", "Parsing url failed",
            "url",          "%s", url,
            NULL
        );
        if(priv->exitOnError) {
            exit(0); //WARNING exit with 0 to stop daemon watcher!
        } else {
            return -1;
        }
    }
    if(strlen(schema) > 0 && schema[strlen(schema)-1]=='s') {
        priv->use_ssl = TRUE;
        gobj_write_bool_attr(gobj, "use_ssl", TRUE);
    }
    gobj_write_str_attr(gobj, "schema", schema);
    gobj_write_str_attr(gobj, "host", host);
    gobj_write_str_attr(gobj, "port", port);

    if(atoi(port) == 0) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_CONNECT_DISCONNECT,
            "msg",          "%s", "Cannot Listen on port 0",
            "url",          "%s", priv->url,
            NULL
        );
        if(priv->exitOnError) {
            exit(0); //WARNING exit with 0 to stop daemon watcher!
        } else {
            return -1;
        }
    }

    if(priv->use_ssl) {
        json_t *jn_crypto = gobj_read_json_attr(gobj, "crypto");
        json_object_set_new(jn_crypto, "trace", json_boolean(priv->trace));
        priv->ytls = ytls_init(gobj, jn_crypto, TRUE);
    }

    /*--------------------------------*
     *      Setup server
     *--------------------------------*/
    priv->yev_server_accept = yev_create_accept_event(
        yuno_event_loop(),
        yev_server_callback,
        NULL
    );

    priv->fd_listen = yev_setup_accept_event(
        priv->yev_server_accept,
        url,        // server_url,
        0,          // backlog, default 512
        gobj_read_bool_attr(gobj, "shared") // shared
    );
    if(priv->fd_listen < 0) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_SYSTEM_ERROR,
            "msg",          "%s", "yev_setup_accept_event() FAILED",
            "url",          "%s", url,
            NULL
        );
        if(priv->exitOnError) {
            exit(0); //WARNING exit with 0 to stop daemon watcher!
        } else {
            yev_destroy_event(priv->yev_server_accept);
            priv->yev_server_accept = 0;
            return -1;
        }
    }

    yev_start_event(priv->yev_server_accept);

    gobj_write_str_attr(gobj, "lHost", host);
    gobj_write_str_attr(gobj, "lPort", port);

    /*
     *  Info of "listening..."
     */
    if(gobj_trace_level(gobj) & TRACE_LISTEN) {
        gobj_log_info(gobj, 0,
            "msgset",       "%s", MSGSET_CONNECT_DISCONNECT,
            "msg",          "%s", "Listening...",
            "url",          "%s", priv->url,
            "lHost",        "%s", host,
            "lPort",        "%s", port,
            NULL
        );
    }

    gobj_change_state(gobj, ST_IDLE);

    return 0;
}

/***************************************************************************
 *      Framework Method stop
 ***************************************************************************/
PRIVATE int mt_stop(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    if(priv->uv_socket_open) {
        if(gobj_trace_level(gobj) & TRACE_UV) {
            gobj_trace_msg(gobj, ">>> uv_close tcpS p=%p", &priv->yev_server_accept);
        }
        gobj_change_state(gobj, ST_WAIT_STOPPED);
        uv_close((uv_handle_t *)&priv->yev_server_accept, on_close_cb);
        priv->uv_socket_open = 0;
    }

    return 0;
}




                    /***************************
                     *      Local Methods
                     ***************************/




/***************************************************************************
 *
 ***************************************************************************/
//PRIVATE void on_close_cb(uv_handle_t* handle)
//{
//    hgobj gobj = handle->data;
//    PRIVATE_DATA *priv = gobj_priv_data(gobj);
//
//    if(gobj_trace_level(gobj) & TRACE_UV) {
//        gobj_trace_msg(gobj, "<<< on_close_cb tcp_s0 p=%p",
//            &priv->yev_server_accept
//        );
//    }
//    gobj_change_state(gobj, "ST_STOPPED");
//
//    if(gobj_trace_level(gobj) & TRACE_LISTEN) {
//        gobj_log_info(gobj, 0,
//            "msgset",       "%s", MSGSET_CONNECT_DISCONNECT,
//            "msg",          "%s", "Unlistening...",
//            "url",          "%s", priv->url,
//            "lHost",        "%s", gobj_read_str_attr(gobj, "lHost"),
//            "lPort",        "%s", gobj_read_str_attr(gobj, "lPort"),
//            NULL
//        );
//    }
//
//    /*
//     *  Only NOW you can destroy this gobj,
//     *  when uv has released the handler.
//     */
//    const char *stopped_event_name = gobj_read_str_attr(
//        gobj,
//        "stopped_event_name"
//    );
//    if(!empty_string(stopped_event_name)) {
//        gobj_send_event(
//            gobj_parent(gobj),
//            stopped_event_name ,
//            0,
//            gobj
//        );
//    }
//}

/***************************************************************************
 *  Accept cb
 ***************************************************************************/
PRIVATE void on_connection_cb(uv_stream_t *uv_server_socket, int status)
{
    hgobj gobj = uv_server_socket->data;
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    if(gobj_trace_level(gobj) & TRACE_UV) {
        gobj_trace_msg(gobj, "<<< on_connection_cb p=%p", &priv->yev_server_accept);
    }

    if (status) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_CONNECT_DISCONNECT,
            "msg",          "%s", "on_connection_cb FAILED",
            "status",       "%d", status,
            "url",          "%s", priv->url,
            "uv_error",     "%s", uv_err_name(status),
            NULL
        );
        // return; HACK si retorna se mete en bucle llamando a este callback
    }

    /*--------------------------------------*
     *  statistics
     *--------------------------------------*/
    (*priv->pconnxs)++;

    /*-------------------*
     *  Name of clisrv
     *-------------------*/
    char xname[80];
    snprintf(xname, sizeof(xname), "clisrv-%u",
        *priv->pconnxs
    );

    /*-----------------------------------------------------------*
     *  Create a filter, if.
     *  A filter is a top level gobj tree over the clisrv gobj.
     *-----------------------------------------------------------*/
    hgobj gobj_top = 0;
    hgobj gobj_bottom = 0;

    json_t *jn_child_tree_filter = gobj_read_json_attr(gobj, "child_tree_filter");
    if(json_is_object(jn_child_tree_filter)) {
        /*--------------------------------*
         *      New method
         *--------------------------------*/
        const char *op = kw_get_str(jn_child_tree_filter, "op", "find", 0);
        json_t *jn_filter = json_deep_copy(kw_get_dict(jn_child_tree_filter, "kw", json_object(), 0));
        // HACK si llegan dos on_connection_cb seguidos coge el mismo tree, protege internamente
        json_object_set_new(jn_filter, "__clisrv__", json_false());
        if(1 || strcmp(op, "find")==0) { // here, only find operation is valid.
            gobj_top = gobj_find_child(gobj_parent(gobj), jn_filter);
            if(!gobj_top) {
                if(gobj_trace_level(gobj) & TRACE_NOT_ACCEPTED) {
                    gobj_log_error(gobj, 0,
                        "function",     "%s", __FUNCTION__,
                        "msgset",       "%s", MSGSET_CONNECT_DISCONNECT,
                        "msg",          "%s", "Connection not accepted: no free child tree found",
                        "lHost",        "%s", gobj_read_str_attr(gobj, "lHost"),
                        "lPort",        "%s", gobj_read_str_attr(gobj, "lPort"),
                        NULL
                    );
                }
                uv_not_accept((uv_stream_t *)uv_server_socket);
                return;
            } else {
                gobj_bottom = gobj_last_bottom_gobj(gobj_top);
                if(!gobj_bottom) {
                    gobj_bottom = gobj_top;
                }
            }
            if(gobj_trace_level(gobj) & TRACE_ACCEPTED) {
                char tree_name[512];
                gobj_full_bottom_name(gobj_top, tree_name, sizeof(tree_name));
                gobj_log_info(gobj, 0,
                    "function",     "%s", __FUNCTION__,
                    "msgset",       "%s", MSGSET_CONNECT_DISCONNECT,
                    "msg",          "%s", "Clisrv accepted",
                    "tree_name",    "%s", tree_name,
                    NULL
                );
            }
        }
    } else if(!empty_string(priv->top_gclass_name)) {
        /*---------------------------------------------------------*
         *      Old method
         *  Crea la clase top de un arbol implicito (gobj_top)
         *  y luego le pregunta si tiene gobj_botom,
         *  por si ha creado un pipe de objetos.
         *---------------------------------------------------------*/
        GCLASS *gc = gobj_find_gclass(priv->top_gclass_name, TRUE);
        /*
         *  We must create a top level, a gobj filter
         */
        if(!gc) {
            gobj_log_info(gobj, 0,
                "msgset",       "%s", MSGSET_INTERNAL_ERROR,
                "msg",          "%s", "GClass not registered",
                "gclass",       "%s", priv->top_gclass_name,
                NULL
            );
            return;
        }
        if(priv->top_kw) {
            json_incref(priv->top_kw);
        }
        gobj_top = gobj_create_volatil(
            xname,
            gc,
            priv->top_kw,
            priv->top_parent?priv->top_parent:priv->subscriber
        );
        gobj_bottom = gobj_last_bottom_gobj(gobj_top);
        if(!gobj_bottom) {
            gobj_bottom = gobj_top;
        }
        gobj_start(gobj_top);
    }

    if(!gobj_bottom && !priv->subscriber) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_INTERNAL_ERROR,
            "msg",          "%s", "Bad tree filter or no subscriber",
            NULL
        );
        uv_not_accept((uv_stream_t *)uv_server_socket);
        return;
    }

    /*----------------------------*
     *  Create the clisrv gobj.
     *----------------------------*/
    json_t *kw_clisrv = json_deep_copy(priv->clisrv_kw);
    if(!kw_clisrv) {
        kw_clisrv = json_object();
    }
    json_object_set_new(kw_clisrv, "ytls", json_integer((json_int_t)(size_t)priv->ytls));
    json_object_set_new(kw_clisrv, "trace", json_boolean(priv->trace));

    hgobj clisrv = gobj_create_volatil(
        xname, // the same name as the filter, if filter.
        GCLASS_TCP1,
        kw_clisrv,
        gobj_bottom?gobj_bottom:priv->subscriber
    );
    gobj_write_str_attr(
        clisrv,
        "lHost",
        gobj_read_str_attr(gobj, "lHost")
    );
    gobj_write_str_attr(
        clisrv,
        "lPort",
        gobj_read_str_attr(gobj, "lPort")
    );
    gobj_write_bool_attr(
        clisrv,
        "__clisrv__",
        TRUE
    );

    if(gobj_bottom) {
        gobj_set_bottom_gobj(gobj_bottom, clisrv);
    }

    /*
     *  srvsock needs to know of disconnected event
     *  for deleting gobjs or do statistics
     */
    json_t *kw_subs = json_pack("{s:{s:b}}", "__config__", "__hard_subscription__", 1);
    gobj_subscribe_event(clisrv, "EV_STOPPED", kw_subs, gobj);

    gobj_start(clisrv);

    /*--------------------------------------*
     *  All ready: accept the connection
     *  to the new child.
     *--------------------------------------*/
    if (accept_connection1(clisrv, uv_server_socket)!=0) {
        gobj_destroy(clisrv);
        return;
    }
    if(gobj_read_bool_attr(gobj, "only_allowed_ips")) {
        const char *peername = gobj_read_str_attr(clisrv, "peername");
        const char *localhost = "127.0.0.";
        if(strncmp(peername, localhost, strlen(localhost))!=0) {
            if(!is_ip_allowed(peername)) {
                gobj_log_info(gobj, 0,
                    "function",     "%s", __FUNCTION__,
                    "msgset",       "%s", MSGSET_CONNECT_DISCONNECT,
                    "msg",          "%s", "Ip not allowed",
                    "url",          "%s", priv->url,
                    "peername",     "%s", peername,
                    NULL
                );
                gobj_stop(clisrv);
                return;
            }
        }
    }
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE int yev_server_callback(yev_event_t *yev_event)
{
    hgobj gobj = yev_event->gobj;

    if(dump) {
        json_t *jn_flags = bits2jn_strlist(yev_flag_strings(), yev_event->flag);
        gobj_log_info(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_YEV_LOOP,
            "msg",          "%s", "yev callback",
            "msg2",         "%s", "💥 yev server callback",
            "event type",   "%s", yev_event_type_name(yev_event),
            "result",       "%d", yev_event->result,
            "sres",         "%s", (yev_event->result<0)? strerror(-yev_event->result):"",
            "flag",         "%j", jn_flags,
            "p",            "%p", yev_event,
            NULL
        );
        json_decref(jn_flags);
    }

    if(!yev_loop->running) {
        return 0;
    }

    switch(yev_event->type) {
        case YEV_READ_TYPE:
            {
                if(yev_event->result < 0) {
                    /*
                     *  Disconnected
                     */
                    yev_loop_stop(yev_loop);
                    break;
                }

                msg_per_second++;
                bytes_per_second += gbuffer_leftbytes(yev_event->gbuf);
                if(test_msectimer(t)) {
                    seconds_count++;
                    if(seconds_count && (seconds_count % drop_in_seconds)==0) {
                        if(who_drop) {
                            printf(Cursor_Down, 3);
                            printf(Move_Horizontal, 1);
                            switch(who_drop) {
                                case 1:
                                    close(fd_connect);
                                    break;
                                case 2:
                                    close(srv_cli_fd);
                                    break;
                                case 3:
                                    close(fd_listen);
                                    break;
                            }
                        }
                    }

                    char nice[64];
                    nice_size(nice, sizeof(nice), msg_per_second);
                    printf("\n" Erase_Whole_Line Move_Horizontal, 1);
                    printf("Msg/sec    : %s\n", nice);
                    printf(Erase_Whole_Line Move_Horizontal, 1);
                    nice_size(nice, sizeof(nice), bytes_per_second);
                    printf("Bytes/sec  : %s\n", nice);
                    printf(Cursor_Up, 3);
                    printf(Move_Horizontal, 1);

                    fflush(stdout);
                    msg_per_second = 0;
                    bytes_per_second = 0;
                    t = start_msectimer(1000);
                }

                /*
                 *  Save received data to transmit: do echo
                 */
                if(dump) {
                    gobj_trace_dump_gbuf(gobj, yev_event->gbuf, "Server receiving");
                }
                gbuffer_clear(gbuf_server_tx);
                gbuffer_append_gbuf(gbuf_server_tx, yev_event->gbuf);

                /*
                 *  Transmit
                 */
                if(dump) {
                    gobj_trace_dump_gbuf(gobj, gbuf_server_tx, "Server transmitting");
                }
                yev_set_gbuffer(yev_server_tx, gbuf_server_tx);
                yev_start_event(yev_server_tx);

                /*
                 *  Clear buffer
                 *  Re-arm read
                 */
                gbuffer_clear(yev_event->gbuf);
                yev_set_gbuffer(yev_server_rx, yev_event->gbuf);
                yev_start_event(yev_server_rx);
            }
            break;

        case YEV_WRITE_TYPE:
            {
                if(yev_event->result < 0) {
                    /*
                     *  Disconnected
                     */
                    yev_loop_stop(yev_loop);
                    break;
                }
            }
            break;

        case YEV_ACCEPT_TYPE:
            {
                // TODO Create a srv_cli structure
                srv_cli_fd = yev_event->result;

                char sockname[80], peername[80];
                get_peername(peername, sizeof(peername), srv_cli_fd);
                get_sockname(sockname, sizeof(sockname), srv_cli_fd);
                printf("ACCEPTED  sockname %s <- peername %s \n", sockname, peername);

                /*
                 *  Ready to receive
                 */
                if(!gbuf_server_rx) {
                    gbuf_server_rx = gbuffer_create(BUFFER_SIZE, BUFFER_SIZE);
                    gbuffer_setlabel(gbuf_server_rx, "server-rx");
                }
                if(!yev_server_rx) {
                    yev_server_rx = yev_create_read_event(
                        yev_event->yev_loop,
                        yev_server_callback,
                        NULL,
                        srv_cli_fd,
                        0
                    );
                }

                /*
                 *  Read to Transmit
                 */
                if(!gbuf_server_tx) {
                    gbuf_server_tx = gbuffer_create(BUFFER_SIZE, BUFFER_SIZE);
                    gbuffer_setlabel(gbuf_server_tx, "server-tx");
                }
                if(!yev_server_tx) {
                    yev_server_tx = yev_create_write_event(
                        yev_event->yev_loop,
                        yev_server_callback,
                        NULL,
                        srv_cli_fd,
                        0
                    );
                }
                yev_set_gbuffer(yev_server_rx, gbuf_server_rx);
                yev_start_event(yev_server_rx);
            }
            break;

        default:
            gobj_log_error(gobj, LOG_OPT_TRACE_STACK,
                "function",     "%s", __FUNCTION__,
                "msgset",       "%s", MSGSET_SYSTEM_ERROR,
                "msg",          "%s", "event type NOT IMPLEMENTED",
                "event_type",   "%s", yev_event_type_name(yev_event),
                NULL
            );
            break;
    }

    return 0;
}




            /***************************
             *      Actions
             ***************************/




/***************************************************************************
 *
 ***************************************************************************/
PRIVATE int ac_clisrv_stopped(hgobj gobj, const char *event, json_t *kw, hgobj src)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    (*priv->pconnxs)--;

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
    .mt_create = mt_create,
    .mt_writing = mt_writing,
    .mt_destroy = mt_destroy,
    .mt_start = mt_start,
    .mt_stop = mt_stop,
};

/*------------------------*
 *      GClass name
 *------------------------*/
GOBJ_DEFINE_GCLASS(C_TCP_S);

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
    ev_action_t st_stopped[] = {
        {EV_STOPPED,            ac_clisrv_stopped,       0},
        {0,0,0}
    };
    ev_action_t st_wait_stopped[] = {
        {EV_STOPPED,            ac_clisrv_stopped,       0},
        {0,0,0}
    };
    ev_action_t st_idle[] = {
        {EV_STOPPED,            ac_clisrv_stopped,       0},
        {0,0,0}
    };

    states_t states[] = {
        {ST_STOPPED,            st_stopped},
        {ST_WAIT_STOPPED,       st_wait_stopped},
        {ST_IDLE,               st_idle},
        {0, 0}
    };

    event_type_t event_types[] = {
        {EV_STOPPED,        0},
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
        tattr_desc,
        sizeof(PRIVATE_DATA),
        0,  // authz_table,
        0,  // command_table,
        s_user_trace_level,
        0 // gclass_flag
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
PUBLIC int register_c_tcp_s(void)
{
    return create_gclass(C_TCP_S);
}
