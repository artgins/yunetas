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
#include <unistd.h>

#include <testing.h>
#include <helpers.h>
#include <kwid.h>
#include <ytls.h>
#include <yev_loop.h>
#include "c_yuno.h"
#include "c_channel.h"
#include "c_tcp.h"
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
PRIVATE int yev_callback(yev_event_h yev_event);


/***************************************************************************
 *          Data: config, public data, private data
 ***************************************************************************/

/*---------------------------------------------*
 *      Attributes - order affect to oid's
 *---------------------------------------------*/
PRIVATE sdata_desc_t attrs_table[] = {
/*-ATTR-type------------name--------------------flag----------------default---------description---------- */
SDATA (DTP_STRING,      "url",                  SDF_WR|SDF_PERSIST, 0,              "url listening"),
SDATA (DTP_INTEGER,     "backlog",              SDF_WR|SDF_PERSIST, "4096",         "Value for listen() backlog argument. It must be lower or equal to net.core.somaxconn. Change dynamically with 'sysctl -w net.core.somaxconn=?'. Change persistent with a file in /etc/sysctl.d/. Consult with 'cat /proc/sys/net/core/somaxconn'."),
SDATA (DTP_BOOLEAN,     "shared",               SDF_WR|SDF_PERSIST, 0,              "Share the port"),
SDATA (DTP_INTEGER,     "use_dups",             SDF_WR|SDF_PERSIST, 0,              "Use yev_dup_accept_event() to set more accept yev_events, only with legacy method (with child_tree_filter). (I don't see more speed using it)"),
SDATA (DTP_JSON,        "crypto",               SDF_WR|SDF_PERSIST, 0,              "Crypto config"),
SDATA (DTP_BOOLEAN,     "only_allowed_ips",     SDF_WR|SDF_PERSIST, 0,              "Only allowed ips"),
SDATA (DTP_BOOLEAN,     "trace_tls",            SDF_WR|SDF_PERSIST, 0,              "Trace TLS"),
SDATA (DTP_BOOLEAN,     "use_ssl",              SDF_RD,             "false",        "True if schema is secure. Set internally"),
SDATA (DTP_STRING,      "lHost",                SDF_RD,             0,              "Listening ip, got internally from url"),
SDATA (DTP_STRING,      "lPort",                SDF_RD,             0,              "Listening port, got internally from url"),
SDATA (DTP_BOOLEAN,     "exitOnError",          SDF_RD,             "1",            "Exit if Listen failed"),
SDATA (DTP_DICT,        "child_tree_filter",    SDF_RD,             0,              "tree of children to create on new accept, legacy method"),

SDATA (DTP_DICT,        "clisrv_kw",            SDF_RD,             0,              "kw of clisrv gobj"),
SDATA (DTP_INTEGER,     "connxs",               SDF_RD|SDF_STATS,   0,              "Current connections"),
SDATA (DTP_INTEGER,     "tconnxs",              SDF_RD|SDF_STATS,   0,              "Total connections"),
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

    json_t * clisrv_kw;
    BOOL trace_tls;

    json_int_t connxs;
    json_int_t tconnxs;

    yev_event_h yev_server_accept;
    int use_dups;
    yev_event_h *yev_dups;
    int fd_listen;
    hytls ytls;
    BOOL use_ssl;

    BOOL only_allowed_ips;
    json_t *child_tree_filter;

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
    SET_PRIV(url,               gobj_read_str_attr)
    SET_PRIV(exitOnError,       gobj_read_bool_attr)
    SET_PRIV(trace_tls,         gobj_read_bool_attr)
    SET_PRIV(only_allowed_ips,  gobj_read_bool_attr)
    SET_PRIV(child_tree_filter, gobj_read_json_attr)
    SET_PRIV(use_dups,          (int)gobj_read_integer_attr)
    SET_PRIV(clisrv_kw,         gobj_read_json_attr)

    /*
     *  subscription model: no send or publish events
     */
    priv->subscriber = (hgobj)gobj_read_pointer_attr(gobj, "subscriber");
    if(!priv->subscriber) {
        priv->subscriber = gobj_parent(gobj);
    }
}

/***************************************************************************
 *      Framework Method writing
 ***************************************************************************/
PRIVATE void mt_writing(hgobj gobj, const char *path)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    IF_EQ_SET_PRIV(url,                 gobj_read_str_attr)
    ELIF_EQ_SET_PRIV(exitOnError,       gobj_read_bool_attr)
    ELIF_EQ_SET_PRIV(trace_tls,         gobj_read_bool_attr)
    ELIF_EQ_SET_PRIV(only_allowed_ips,  gobj_read_bool_attr)
    END_EQ_SET_PRIV()
}

/***************************************************************************
 *      Framework Method destroy
 ***************************************************************************/
PRIVATE void mt_destroy(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    EXEC_AND_RESET(ytls_cleanup, priv->ytls)
    EXEC_AND_RESET(yev_destroy_event, priv->yev_server_accept)

    if(priv->yev_dups) {
        for(int dup_idx=1; dup_idx <= priv->use_dups; dup_idx++) {
            if(priv->yev_dups[dup_idx]) {
                yev_destroy_event(priv->yev_dups[dup_idx]);
                priv->yev_dups[dup_idx] = 0;
            }
        }

        GBMEM_FREE(priv->yev_dups)
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
        false
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

    /*--------------------------------*
     *      Setup server
     *--------------------------------*/
    int backlog = (int)gobj_read_integer_attr(gobj, "backlog");
    BOOL shared = gobj_read_bool_attr(gobj, "shared");
    priv->yev_server_accept = yev_create_accept_event(
        yuno_event_loop(),
        yev_callback,
        url,    // server_url,
        backlog,
        shared, // shared
        0,      // ai_family AF_UNSPEC
        0,      // ai_flags AI_V4MAPPED | AI_ADDRCONFIG
        gobj
    );

    if(!priv->yev_server_accept) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_SYSTEM_ERROR,
            "msg",          "%s", "yev_create_accept_event() FAILED",
            "url",          "%s", url,
            NULL
        );
        if(priv->exitOnError) {
            exit(0); //WARNING exit with 0 to stop daemon watcher!
        } else {
            if(priv->yev_server_accept) {
                yev_destroy_event(priv->yev_server_accept);
                priv->yev_server_accept = 0;
            }
            return -1;
        }
    }

    if(yev_get_flag(priv->yev_server_accept) & YEV_FLAG_USE_TLS) {
        priv->use_ssl = true;
        gobj_write_bool_attr(gobj, "use_ssl", true);

        json_t *jn_crypto = gobj_read_json_attr(gobj, "crypto");
        json_object_set_new(jn_crypto, "trace", json_boolean(priv->trace_tls));
        priv->ytls = ytls_init(gobj, jn_crypto, true);
    }

    gobj_write_str_attr(gobj, "lHost", host);
    gobj_write_str_attr(gobj, "lPort", port);

    /*
     *  Info of "listening..."
     */
    if(gobj_trace_level(gobj) & TRACE_LISTEN) {
        gobj_log_info(gobj, 0,
            "msgset",       "%s", MSGSET_CONNECT_DISCONNECT,
            "msg",          "%s", "Listening...",
            "msg2",          "%s", "Listening...🔷",
            "url",          "%s", priv->url,
            "lHost",        "%s", host,
            "lPort",        "%s", port,
            NULL
        );
    }

    gobj_change_state(gobj, ST_IDLE);

    if(json_object_size(priv->child_tree_filter) > 0) {
        /*--------------------------------*
         *      Legacy method
         *--------------------------------*/
        yev_start_event(priv->yev_server_accept);
        if(priv->use_dups > 0) {
            priv->yev_dups = GBMEM_MALLOC((priv->use_dups + 1)* sizeof(yev_event_h *));
            for(int dup_idx=1; dup_idx<=priv->use_dups; dup_idx++) {
                priv->yev_dups[dup_idx] = yev_dup_accept_event(priv->yev_server_accept, dup_idx, gobj);
                yev_start_event(priv->yev_dups[dup_idx]);
            }
        }

    } else {
        /*-------------------------------------------*
         *      New method
         *  Set an accept yev_event in each TCP gobj
         *-------------------------------------------*/
        hgobj parent = gobj_parent(gobj);
        hgobj child = gobj_first_child(parent);
        int fd_listen = yev_get_fd(priv->yev_server_accept);

        while(child) {
            if(gobj_gclass_name(child) == C_CHANNEL) {
                hgobj gobj_bottom = gobj_last_bottom_gobj(child);
                if(!gobj_bottom) {
                    gobj_log_error(gobj, 0,
                        "function",     "%s", __FUNCTION__,
                        "msgset",       "%s", MSGSET_PARAMETER_ERROR,
                        "msg",          "%s", "Bottom gobj not found",
                        "child",        "%s", gobj_full_name(child),
                        "url",          "%s", priv->url,
                        NULL
                    );
                    child = gobj_next_child(child);
                    continue;
                }

                /*----------------------------------------*
                 *  Create the clisrv gobj if not exist
                 *----------------------------------------*/
                hgobj clisrv = NULL;
                if(gobj_gclass_name(gobj_bottom) != C_TCP) {
                    /*
                     *  Create a tcp gobj
                     */
                    json_t *kw_clisrv = json_deep_copy(priv->clisrv_kw);
                    if(!kw_clisrv) {
                        kw_clisrv = json_object();
                    }

                    /*-------------------*
                     *  Name of clisrv
                     *-------------------*/
                    priv->tconnxs++;
                    char xname[80];
                    snprintf(xname, sizeof(xname), "clisrv-%"JSON_INTEGER_FORMAT,
                        priv->tconnxs
                    );

                    clisrv = gobj_create_pure_child(
                        xname, // the same name as the filter.
                        C_TCP,
                        kw_clisrv,
                        gobj_bottom
                    );
                    gobj_set_bottom_gobj(gobj_bottom, clisrv);

                    /*
                     *  srvsock needs to know of disconnected event
                     *  for deleting gobjs or do statistics TODO sure? review
                     */
                    // TODO json_t *kw_subs = json_pack("{s:{s:b}}", "__config__", "__hard_subscription__", 1);
                    //  gobj_subscribe_event(clisrv, EV_STOPPED, kw_subs, gobj);

                } else {
                    clisrv = gobj_bottom;
                }

                gobj_write_bool_attr(clisrv, "__clisrv__", true);
                gobj_write_bool_attr(clisrv, "use_ssl", priv->use_ssl);
                gobj_write_pointer_attr(clisrv, "ytls", priv->ytls);
                gobj_write_integer_attr(clisrv, "fd_clisrv", -1);
                gobj_write_integer_attr(clisrv, "fd_listen", fd_listen);
                gobj_start(clisrv); // this create a yev_dup2_accept_event
            }

            /*
             *  Next child
             */
            child = gobj_next_child(child);
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

    if(priv->yev_server_accept) {
        yev_stop_event(priv->yev_server_accept);
        if(yev_event_is_stopped(priv->yev_server_accept)) {
            yev_destroy_event(priv->yev_server_accept);
            priv->yev_server_accept = 0;
        } else {
            gobj_change_state(gobj, ST_WAIT_STOPPED);
        }
    }

    if(priv->yev_dups) {
        for(int dup_idx=1; dup_idx<=priv->use_dups; dup_idx++) {
            if(!priv->yev_dups[dup_idx]) {
                continue;
            }
            yev_stop_event(priv->yev_dups[dup_idx]);
            if(yev_event_is_stopped(priv->yev_dups[dup_idx])) {
                yev_destroy_event(priv->yev_dups[dup_idx]);
                priv->yev_dups[dup_idx] = 0;
            } else {
                gobj_change_state(gobj, ST_WAIT_STOPPED);
            }
        }
    }

    EXEC_AND_RESET(ytls_cleanup, priv->ytls)

    return 0;
}




                    /***************************
                     *      Local Methods
                     ***************************/




/***************************************************************************
 *  Returns the first matched child.
 *  TODO improve to manage multiple connections quickly
 *      for example a two list? of connected and disconnected channels?
 ***************************************************************************/
PUBLIC hgobj my_gobj_find_child(
    hgobj gobj,
    json_t *jn_filter  // owned
)
{
    size_t n_children = gobj_child_size(gobj);
    static hgobj child = NULL;

    if(!child) {
        child = gobj_first_child(gobj);
    }
    for(int i=0; i<n_children; i++) {
        if(gobj_match_gobj(child, json_incref(jn_filter))) {
            JSON_DECREF(jn_filter)
            return child;
        }
        child = gobj_next_child(child);
        if(!child) {
            child = gobj_first_child(gobj);
        }
    }

    JSON_DECREF(jn_filter)
    return 0;
}

/***************************************************************************
 *  Accept cb
 *  WARNING yev_callback() return -1 will break the loop of yevent
 ***************************************************************************/
PRIVATE int yev_callback(yev_event_h yev_event)
{
    #ifdef CONFIG_DEBUG_PRINT_YEV_LOOP_TIMES
    int measuring_times = get_measure_times();
    if(measuring_times & YEV_ACCEPT_TYPE) {
        MT_PRINT_TIME(yev_time_measure, "C_TCP_S yev_callback() entry");
    }
    #endif
    hgobj gobj = yev_get_gobj(yev_event);
    if(!gobj) {
        gobj_log_error(gobj, LOG_OPT_TRACE_STACK,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_INTERNAL_ERROR,
            "msg",          "%s", "gobj NULL",
            NULL
        );
        return 0;
    }

    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    int trace = (int)gobj_trace_level(gobj) & TRACE_URING;
    if(trace) {
        json_t *jn_flags = bits2jn_strlist(yev_flag_strings(), yev_get_flag(yev_event));
        gobj_log_debug(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_YEV_LOOP,
            "msg",          "%s", "yev callback",
            "msg2",         "%s", "TCP_S 🌐🌐💥 yev callback",
            "event type",   "%s", yev_event_type_name(yev_event),
            "state",        "%s", yev_get_state_name(yev_event),
            "result",       "%d", yev_get_result(yev_event),
            "sres",         "%s", (yev_get_result(yev_event)<0)? strerror(-yev_get_result(yev_event)):"",
            "flag",         "%j", jn_flags,
            "p",            "%p", yev_event,
            "fd",           "%d", yev_get_fd(yev_event),
            "gbuffer",      "%p", yev_get_gbuf(yev_event),
            NULL
        );
        json_decref(jn_flags);
    }

    if(yev_event_is_stopped(yev_event)) {
        if(priv->yev_server_accept == yev_event) {
            yev_destroy_event(yev_event);
            priv->yev_server_accept = 0;
            gobj_change_state(gobj, ST_STOPPED);
        } else {
            int dup_idx = yev_get_dup_idx(yev_event);
            if(dup_idx > 0 && yev_event == priv->yev_dups[dup_idx]) {
                yev_destroy_event(yev_event);
                priv->yev_dups[dup_idx] = 0;
            } else {
                gobj_log_error(gobj, LOG_OPT_TRACE_STACK,
                    "function",     "%s", __FUNCTION__,
                    "msgset",       "%s", MSGSET_INTERNAL_ERROR,
                    "msg",          "%s", "dup_idx not match",
                    NULL
                );
            }
        }

        return 0;
    }

    if(yev_get_type(yev_event) != YEV_ACCEPT_TYPE) {
        gobj_log_error(gobj, LOG_OPT_TRACE_STACK,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_SYSTEM_ERROR,
            "msg",          "%s", "TCP_S: event type NOT IMPLEMENTED",
            "msg2",         "%s", "🌐TCP_S: event type NOT IMPLEMENTED",
            "event_type",   "%s", yev_event_type_name(yev_event),
            NULL
        );
        return 0;
    }

    /*-------------------------------------------------*
     *  WARNING: Here only with YEV_ACCEPT_TYPE event
     *-------------------------------------------------*/
    int fd_clisrv = yev_get_result(yev_event);
    if(fd_clisrv<0) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_SYSTEM_ERROR,
            "msg",          "%s", "TCP_S: yev_callback fd_clisrv FAILED ",
            "msg2",         "%s", "🌐TCP_S: yev_callback fd_clisrv FAILED ",
            "event_type",   "%s", yev_event_type_name(yev_event),
            NULL
        );
        return 0;
    }

    #ifdef CONFIG_DEBUG_PRINT_YEV_LOOP_TIMES
    if(measuring_times & YEV_ACCEPT_TYPE) {
        MT_PRINT_TIME(yev_time_measure, "C_TCP_S yev_callback() checks");
    }
    #endif

    if(priv->only_allowed_ips) {
        char peername[80];
        get_peername(peername, sizeof(peername), fd_clisrv);
        const char *localhost = "127.0.0.";
        if(strncmp(peername, localhost, strlen(localhost))!=0) {
            if(!is_ip_allowed(peername)) {
                gobj_log_info(gobj, 0,
                    "function",     "%s", __FUNCTION__,
                    "msgset",       "%s", MSGSET_CONNECT_DISCONNECT,
                    "msg",          "%s", "TCP_S: Ip not allowed",
                    "msg2",         "%s", "🌐TCP_S: Ip not allowed",
                    "url",          "%s", priv->url,
                    "peername",     "%s", peername,
                    NULL
                );
                close(fd_clisrv);
                return 0;
            }
        }
    }

    #ifdef CONFIG_DEBUG_PRINT_YEV_LOOP_TIMES
    if(measuring_times & YEV_ACCEPT_TYPE) {
        MT_PRINT_TIME(yev_time_measure, "C_TCP_S yev_callback() only_allowed_ips");
    }
    #endif

    /*
     *  Concurrent connections
     */
    priv->connxs++;

    /*-----------------------------------------------------------*
     *  Create a filter, if.
     *  A filter is a top level gobj tree over the clisrv gobj.
     *-----------------------------------------------------------*/
    if(json_object_size(priv->child_tree_filter) > 0) {
        /*--------------------------------*
         *      Legacy method
         *--------------------------------*/
        hgobj gobj_top = 0;
        hgobj gobj_bottom = 0;

        // obsolete: const char *op = kw_get_str(gobj, jn_child_tree_filter, "op", "find", 0);
        json_t *jn_filter = kw_get_dict(gobj, priv->child_tree_filter, "kw", json_object(), 0);
        gobj_top = my_gobj_find_child(gobj_parent(gobj), json_incref(jn_filter));
        if(!gobj_top) {
            gobj_log_error(gobj, 0,
                "function",     "%s", __FUNCTION__,
                "msgset",       "%s", MSGSET_CONNECT_DISCONNECT,
                "msg",          "%s", "TCP_S: Connection not accepted: no free child tree found",
                "msg2",         "%s", "🌐TCP_S: Connection not accepted: no free child tree found",
                "lHost",        "%s", gobj_read_str_attr(gobj, "lHost"),
                "lPort",        "%s", gobj_read_str_attr(gobj, "lPort"),
                NULL
            );
            close(fd_clisrv);
            return 0;
        }
        #ifdef CONFIG_DEBUG_PRINT_YEV_LOOP_TIMES
        if(measuring_times & YEV_ACCEPT_TYPE) {
            MT_PRINT_TIME(yev_time_measure, "C_TCP_S yev_callback() my_gobj_find_child");
        }
        #endif
        gobj_bottom = gobj_last_bottom_gobj(gobj_top);
        if(!gobj_bottom) {
            gobj_log_error(gobj, 0,
                "function",     "%s", __FUNCTION__,
                "msgset",       "%s", MSGSET_CONNECT_DISCONNECT,
                "msg",          "%s", "TCP_S: Connection not accepted: no gobj_bottom found",
                "msg2",         "%s", "🌐TCP_S: Connection not accepted: no gobj_bottom  found",
                "lHost",        "%s", gobj_read_str_attr(gobj, "lHost"),
                "lPort",        "%s", gobj_read_str_attr(gobj, "lPort"),
                NULL
            );
            close(fd_clisrv);
            return 0;
        }

        if(gobj_trace_level(gobj) & TRACE_ACCEPTED) {
            const char *top_tree = gobj_full_name(gobj_top);
            const char *bottom_tree = gobj_full_name(gobj_bottom);
            gobj_log_info(gobj, 0,
                "function",     "%s", __FUNCTION__,
                "msgset",       "%s", MSGSET_CONNECT_DISCONNECT,
                "msg",          "%s", "Clisrv accepted",
                "msg2",         "%s", "🌐TCP_S: Clisrv accepted...🔷👍",
                "top_tree",     "%s", top_tree,
                "bottom_tree",  "%s", bottom_tree,
                NULL
            );
        }

        #ifdef CONFIG_DEBUG_PRINT_YEV_LOOP_TIMES
        if(measuring_times & YEV_ACCEPT_TYPE) {
            MT_PRINT_TIME(yev_time_measure, "C_TCP_S yev_callback() trace accepted");
        }
        #endif
        /*----------------------------------------*
         *  Create the clisrv gobj if not exist
         *----------------------------------------*/
        hgobj clisrv = NULL;
        if(gobj_gclass_name(gobj_bottom) != C_TCP) {
            /*
             *  Legacy method, create a volatil tcp gobj
             */
            json_t *kw_clisrv = json_deep_copy(priv->clisrv_kw);
            if(!kw_clisrv) {
                kw_clisrv = json_object();
            }

            /*-------------------*
             *  Name of clisrv
             *-------------------*/
            priv->tconnxs++;
            char xname[80];
            snprintf(xname, sizeof(xname), "clisrv-%"JSON_INTEGER_FORMAT,
                priv->tconnxs
            );

            clisrv = gobj_create_volatil(
                xname, // the same name as the filter.
                C_TCP,
                kw_clisrv,
                gobj_bottom
            );
            gobj_set_bottom_gobj(gobj_bottom, clisrv);

            /*
             *  srvsock needs to know of disconnected event
             *  for deleting gobjs or do statistics TODO sure? review
             */
            // TODO json_t *kw_subs = json_pack("{s:{s:b}}", "__config__", "__hard_subscription__", 1);
            //  gobj_subscribe_event(clisrv, EV_STOPPED, kw_subs, gobj);

            #ifdef CONFIG_DEBUG_PRINT_YEV_LOOP_TIMES
            if(measuring_times & YEV_ACCEPT_TYPE) {
                MT_PRINT_TIME(yev_time_measure, "C_TCP_S yev_callback() gobj_create_volatil");
            }
            #endif
        } else {
            clisrv = gobj_bottom;
        }

        #ifdef CONFIG_DEBUG_PRINT_YEV_LOOP_TIMES
        if(measuring_times & YEV_ACCEPT_TYPE) {
            MT_PRINT_TIME(yev_time_measure, "C_TCP_S yev_callback() clisrv selected");
        }
        #endif

        gobj_write_pointer_attr(clisrv, "ytls", priv->ytls);
        gobj_write_bool_attr(clisrv, "use_ssl", priv->use_ssl);
        gobj_write_bool_attr(clisrv, "__clisrv__", true);
        gobj_write_integer_attr(clisrv, "fd_clisrv", fd_clisrv);

        #ifdef CONFIG_DEBUG_PRINT_YEV_LOOP_TIMES
        if(measuring_times & YEV_ACCEPT_TYPE) {
            MT_PRINT_TIME(yev_time_measure, "C_TCP_S yev_callback() clisrv configured");
        }
        #endif
        gobj_start(clisrv); // this call set_connected(clisrv);

        #ifdef CONFIG_DEBUG_PRINT_YEV_LOOP_TIMES
        if(measuring_times & YEV_ACCEPT_TYPE) {
            MT_PRINT_TIME(yev_time_measure, "C_TCP_S yev_callback() clisrv started");
        }
        #endif

    } else {
        /*-------------------------------------------*
         *      New method
         *  WARNING Connection cannot come by here!!
         *-------------------------------------------*/
        gobj_log_error(gobj, LOG_OPT_TRACE_STACK,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_CONNECT_DISCONNECT,
            "msg",          "%s", "TCP_S: child_tree_filter NULL",
            "msg2",         "%s", "🌐TCP_S: child_tree_filter NULL",
            "lHost",        "%s", gobj_read_str_attr(gobj, "lHost"),
            "lPort",        "%s", gobj_read_str_attr(gobj, "lPort"),
            NULL
        );
        close(fd_clisrv);
        return 0;
    }

    #ifdef CONFIG_DEBUG_PRINT_YEV_LOOP_TIMES
    if(measuring_times & YEV_ACCEPT_TYPE) {
        MT_PRINT_TIME(yev_time_measure, "C_TCP_S yev_callback() end");
    }
    #endif
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

    priv->connxs--;

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
        attrs_table,
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
