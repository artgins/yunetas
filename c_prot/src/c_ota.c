/****************************************************************************
 *          c_ota.c
 *
 *          GClass Ota
 *
 *          ON THE AIR - Update firmware
 *
 *          Copyright (c) 2023 Niyamaka.
 *          All Rights Reserved.
 ****************************************************************************/
#include <string.h>
#include <kwid.h>
#include <parse_url.h>
#include <command_parser.h>
#include <c_prot_http_cli.h>
#include "c_ota.h"


/***************************************************************
 *              Constants
 ***************************************************************/

/***************************************************************
 *              Prototypes
 ***************************************************************/
PRIVATE int push_data(
    hgobj gobj,
    const char *binary_file
);
PRIVATE json_t *cmd_help(hgobj gobj, const char *cmd, json_t *kw, hgobj src);
PRIVATE json_t *cmd_download_firmware(hgobj gobj, const char *cmd, json_t *kw, hgobj src);

/***************************************************************
 *              Data
 ***************************************************************/
PRIVATE sdata_desc_t pm_help[] = {
/*-PM----type-----------name------------flag------------default-----description---------- */
SDATAPM (DTP_STRING,    "cmd",          0,              "",         "command about you want help."),
SDATAPM (DTP_INTEGER,   "level",        0,              "",         "command search level in childs"),
SDATA_END()
};
PRIVATE sdata_desc_t pm_download_firmware[] = {
/*-PM----type-----------name------------flag------------default-----description---------- */
SDATAPM (DTP_STRING,    "url_ota",      0,              "",         "url_ota, immediately begin download and install firmware"),
SDATAPM (DTP_BOOLEAN,   "force",        0,              "false",    "Force loading of another yuno_role"),
SDATA_END()
};

PRIVATE const char *a_help[] = {"h", "?", 0};

PRIVATE sdata_desc_t command_table[] = {
/*-CMD---type-----------name----------------alias-------items-----------json_fn---------description---------- */
SDATACM (DTP_SCHEMA,    "help",             a_help,     pm_help,        cmd_help,       "Command's help"),
SDATACM (DTP_SCHEMA,    "download-firmware",0,          pm_download_firmware,cmd_download_firmware,"Download and install new firmware. When connected to controlcenter will be validated."),
SDATA_END()
};

/*---------------------------------------------*
 *          Attributes
 *---------------------------------------------*/
PRIVATE sdata_desc_t tattr_desc[] = {
/*-ATTR-type--------name----------------flag--------------------default-description---------- */
SDATA (DTP_STRING,  "url_ota",          SDF_WR,                 "",     "OTA url"),
SDATA (DTP_STRING,  "cert_pem",         SDF_PERSIST,            "",     "SSL server certificate, PEM format"),
SDATA_END()
};

/*---------------------------------------------*
 *      GClass trace levels
 *  HACK strict ascendant value!
 *  required paired correlative strings
 *  in s_user_trace_level
 *---------------------------------------------*/
enum {
    TRACE_MESSAGES              = 0x0001,
};
PRIVATE const trace_level_t s_user_trace_level[16] = {
    {"messages",                "Trace ALL messages"},
    {0, 0},
};

/*---------------------------------------------*
 *              Private data
 *---------------------------------------------*/
typedef struct _PRIVATE_DATA {
    hgobj gobj_http_cli_ota;
    const char *url_ota;
} PRIVATE_DATA;

PRIVATE hgclass __gclass__ = 0;





                    /******************************
                     *      Framework Methods
                     ******************************/




/***************************************************************************
 *      Framework Method
 ***************************************************************************/
PRIVATE void mt_create(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    /*------------------------------*
     *      Http client
     *------------------------------*/
    json_t *kw = json_pack("{s:s, s:I}",
        "url", "",
        "subscriber", (json_int_t)(size_t)gobj
    );
    priv->gobj_http_cli_ota = gobj_create_service("http_cli_ota", C_PROT_HTTP_CL, kw, gobj);
    gobj_set_bottom_gobj(gobj, priv->gobj_http_cli_ota);

    /*------------------------------*
     *      Set cert
     *------------------------------*/
    if(priv->gobj_http_cli_ota) {
        const char *s = gobj_read_str_attr(priv->gobj_http_cli_ota, "cert_pem");
        if(empty_string(s)) {
            gobj_write_str_attr(
                priv->gobj_http_cli_ota,
                "cert_pem",
                gobj_read_str_attr(gobj, "cert_pem")
            );
        }
    }

    /*------------------------------*
     *      Periodic timeout
     *------------------------------*/
//    gobj_subscribe_event(gobj_yuno(), EV_TIMEOUT_PERIODIC, NULL, gobj);
}

/***************************************************************************
 *      Framework Method writing
 ***************************************************************************/
PRIVATE void mt_writing(hgobj gobj, const char *path)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    IF_EQ_SET_PRIV(url_ota,     gobj_read_str_attr)
        if(priv->gobj_http_cli_ota) {
            if(!empty_string(priv->url_ota)) {
                gobj_write_str_attr(
                    priv->gobj_http_cli_ota,
                    "url",
                    priv->url_ota
                );
                gobj_start(priv->gobj_http_cli_ota);
            }
        }
    END_EQ_SET_PRIV()
}

/***************************************************************************
 *      Framework Method
 ***************************************************************************/
PRIVATE int mt_start(hgobj gobj)
{
    /*
     *  Entry point can auto-start this.
     *  We only try to connect and download the firmware when url_ota attribute is written
     *  The url_ota must include the filename to download, example:
     *          "https://esp.ota.mulesol.yunetacontrol.com:9999/firmware-example-v1.0.bin"
     */
    return 0;
}

/***************************************************************************
 *      Framework Method
 ***************************************************************************/
PRIVATE int mt_stop(hgobj gobj)
{
    return 0;
}

/***************************************************************************
 *      Framework Method destroy
 ***************************************************************************/
PRIVATE void mt_destroy(hgobj gobj)
{
}




                    /***************************
                     *      Commands
                     ***************************/




/***************************************************************************
 *
 ***************************************************************************/
PRIVATE json_t *cmd_help(hgobj gobj, const char *cmd, json_t *kw, hgobj src)
{
    json_t *jn_resp = gobj_build_cmds_doc(gobj, kw);
    return build_command_response(
        gobj,
        0,
        jn_resp,
        0,
        0
    );
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE json_t *cmd_download_firmware(hgobj gobj, const char *cmd, json_t *kw, hgobj src)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    if(!priv->gobj_http_cli_ota) {
        json_t *kw_response = build_command_response(
            gobj,
            -1,     // result
            json_sprintf("gobj_http_cli_ota NULL"),   // jn_comment
            0,      // jn_schema
            0       // jn_data
        );
        KW_DECREF(kw)
        return kw_response;
    }

    BOOL force = kw_get_bool(gobj, kw, "force", 0, KW_WILD_NUMBER);
    const char *url_ota = kw_get_str(
        gobj,
        kw,
        "url_ota",
        "",
        0
    );
    if(empty_string(url_ota)) {
        json_t *kw_response = build_command_response(
            gobj,
            -1,     // result
            json_sprintf("what url_ota?"),   // jn_comment
            0,      // jn_schema
            0       // jn_data
        );
        JSON_DECREF(kw)
        return kw_response;
    }

    char schema[16];
    char host[120];
    char port[10];
    char path[80];

    if(parse_url(
        gobj,
        url_ota,
        schema, sizeof(schema),
        host, sizeof(host),
        port, sizeof(port),
        path, sizeof(path),
        0, 0,
        FALSE
    )<0) {
        json_t *kw_response = build_command_response(
            gobj,
            -1,     // result
            json_sprintf("Parsing url_ota failed: %s", url_ota),   // jn_comment
            0,      // jn_schema
            0       // jn_data
        );
        JSON_DECREF(kw)
        return kw_response;
    }

    // TODO parse path, must match yuno_role, add force parameter to let another role
    if(force) {
        // TODO
    }

    /*
     *  At writing the url_ota attr the operation will begin
     */
    gobj_write_str_attr(gobj, "url_ota", url_ota);

    json_t *kw_response = build_command_response(
        gobj,
        0,
        json_sprintf("Downloading %s ...", path),   // jn_comment
        0,
        0
    );
    KW_DECREF(kw)
    return kw_response;
}




                    /***************************
                     *      Local methods
                     ***************************/




/***************************************************************************
 *
 ***************************************************************************/
PRIVATE int push_data(
    hgobj gobj,
    const char *binary_file
) {
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    json_t *jn_headers = json_pack("{s:s, s:s}",
        "Content-Type", "application/json; charset=utf-8",
        "Connection", "keep-alive" // TODO review
    );

    json_t *query = json_pack("{s:s, s:s, s:s, s:o}",
        "method", "POST", // TODO review to download a file
        "resource", "/",
        "query", "",
        "headers", jn_headers
    );

    if(priv->gobj_http_cli_ota && gobj_current_state(priv->gobj_http_cli_ota)==ST_CONNECTED) {
        if(gobj_trace_level(gobj) & TRACE_MESSAGES) {
            gobj_trace_json(gobj, query, "%s ==> %s",
                gobj_short_name(gobj),
                gobj_short_name(priv->gobj_http_cli_ota)
            );
        }
        gobj_send_event(priv->gobj_http_cli_ota, EV_SEND_MESSAGE, json_incref(query), gobj);
    }
    json_decref(query);
    return 0;
}




                    /***************************
                     *      Actions
                     ***************************/



// TODO subscribe NETWORK_OFF?
//if(priv->gobj_http_cli_ota) {
//    gobj_stop(priv->gobj_http_cli_ota);
//}

/***************************************************************************
 *  Http connected
 ***************************************************************************/
PRIVATE int ac_on_open(hgobj gobj, gobj_event_t event, json_t *kw, hgobj src)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    if(gobj_trace_level(gobj) & TRACE_MESSAGES) {
        gobj_trace_msg(gobj, "🔵🔵🔵 OPEN %s", gobj_short_name(src));
    }

    if(src == priv->gobj_http_cli_ota) {
        gobj_change_state(gobj, ST_SESSION);
    }

    JSON_DECREF(kw)
    return 0;
}

/***************************************************************************
 *  Http disconnected
 ***************************************************************************/
PRIVATE int ac_on_close(hgobj gobj, gobj_event_t event, json_t *kw, hgobj src)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    if(gobj_trace_level(gobj) & TRACE_MESSAGES) {
        gobj_trace_msg(gobj, "🔴🔴🔴 CLOSE %s", gobj_short_name(src));
    }

    do {
        if(src == priv->gobj_http_cli_ota) {
            if(priv->gobj_http_cli_ota && gobj_current_state(priv->gobj_http_cli_ota)==ST_CONNECTED) {
                break;
            }
            gobj_change_state(gobj, ST_DISCONNECTED);
        }
    } while(0);

    JSON_DECREF(kw)
    return 0;
}

/***************************************************************************
 *  Message from http connection: ack
 ***************************************************************************/
PRIVATE int ac_on_message(hgobj gobj, gobj_event_t event, json_t *kw, hgobj src)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    if(gobj_trace_level(gobj) & TRACE_MESSAGES) {
        gobj_trace_json(gobj, kw, "%s <== %s",
            gobj_short_name(gobj),
            gobj_short_name(src)
        );
    }

    if(src == priv->gobj_http_cli_ota) {
        // Respuesta al POST
        int status = (int)kw_get_int(gobj, kw, "body`status", -1, KW_REQUIRED);
        if(status != 0) {
            gobj_log_error(gobj, 0,
                "function",     "%s", __FUNCTION__,
                "msgset",       "%s", MSGSET_SYSTEM_ERROR,
                "msg",          "%s", "Status NOT zero",
                "status",       "%d", status,
                NULL
            );
        }
    } else {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_INTERNAL_ERROR,
            "msg",          "%s", "source unknown",
            "src",          "%s", gobj_full_name(src),
            NULL
        );
        gobj_trace_json(gobj, kw, "source unknown");
    }

    KW_DECREF(kw)
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
GOBJ_DEFINE_GCLASS(C_OTA);

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
    ev_action_t st_disconnected[] = {
        {EV_ON_OPEN,                ac_on_open,                 0},
        {0,0,0}
    };
    ev_action_t st_session[] = {
        {EV_ON_OPEN,                0,                          0},
        {EV_ON_CLOSE,               ac_on_close,                0},
        {EV_ON_MESSAGE,             ac_on_message,              0},
        {0,0,0}
    };
    states_t states[] = {
        {ST_DISCONNECTED,       st_disconnected},
        {ST_SESSION,            st_session},
        {0, 0}
    };

    /*----------------------------------------*
     *          Create the gclass
     *----------------------------------------*/
    __gclass__ = gclass_create(
        gclass_name,
        0,  // event_types
        states,
        &gmt,
        0,  // lmt,
        tattr_desc,
        sizeof(PRIVATE_DATA),
        0,  // authz_table,
        command_table,
        s_user_trace_level,
        0   // gclass_flag
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
PUBLIC int register_c_ota(void)
{
    return create_gclass(C_OTA);
}
