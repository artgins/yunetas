/****************************************************************************
 *          c_ota.c
 *
 *          GClass Ota
 *
 *          ON THE AIR - Update firmware
 *
 *          Copyright (c) 2023 Niyamaka.
 *          Copyright (c) 2024, ArtGins.
 *          All Rights Reserved.
 ****************************************************************************/
#include <string.h>
#include <unistd.h>

#include <c_timer.h>
#include <c_prot_http_cl.h>

#ifdef ESP_PLATFORM
    #include <esp_ota_ops.h>
    #include <esp_flash_partitions.h>
    #include <esp_partition.h>
    #include <esp_system.h>
    #include <esp_image_format.h>
#endif

#include "c_ota.h"


/***************************************************************
 *              Constants
 ***************************************************************/
#ifdef __linux__
/// OTA_DATA states for checking operability of the app.
typedef enum {
    ESP_OTA_IMG_NEW             = 0x0U,         /*!< Monitor the first boot. In bootloader this state is changed to ESP_OTA_IMG_PENDING_VERIFY. */
    ESP_OTA_IMG_PENDING_VERIFY  = 0x1U,         /*!< First boot for this app was. If while the second boot this state is then it will be changed to ABORTED. */
    ESP_OTA_IMG_VALID           = 0x2U,         /*!< App was confirmed as workable. App can boot and work without limits. */
    ESP_OTA_IMG_INVALID         = 0x3U,         /*!< App was confirmed as non-workable. This app will not selected to boot at all. */
    ESP_OTA_IMG_ABORTED         = 0x4U,         /*!< App could not confirm the workable or non-workable. In bootloader IMG_PENDING_VERIFY state will be changed to IMG_ABORTED. This app will not selected to boot at all. */
    ESP_OTA_IMG_UNDEFINED       = 0xFFFFFFFFU,  /*!< Undefined. App can boot and work without limits. */
} esp_ota_img_states_t;

#endif

/***************************************************************
 *              Prototypes
 ***************************************************************/
PRIVATE int get_binary_file(
    hgobj gobj,
    const char *binary_file
);
PRIVATE json_t *cmd_help(hgobj gobj, const char *cmd, json_t *kw, hgobj src);
PRIVATE json_t *cmd_download_firmware(hgobj gobj, const char *cmd, json_t *kw, hgobj src);

/***************************************************************
 *              Data
 ***************************************************************/
PRIVATE const sdata_desc_t pm_help[] = {
/*-PM----type-----------name------------flag------------default-----description---------- */
SDATAPM (DTP_STRING,    "cmd",          0,              "",         "command about you want help."),
SDATAPM (DTP_INTEGER,   "level",        0,              "",         "command search level in childs"),
SDATA_END()
};
PRIVATE const sdata_desc_t pm_download_firmware[] = {
/*-PM----type-----------name------------flag------------default-----description---------- */
SDATAPM (DTP_STRING,    "url_ota",      0,              "",         "url_ota, immediately begin download and install firmware"),
SDATAPM (DTP_BOOLEAN,   "force",        0,              "false",    "Force loading of another yuno_role"),
SDATA_END()
};

PRIVATE const char *a_help[] = {"h", "?", 0};

PRIVATE const sdata_desc_t command_table[] = {
/*-CMD---type-----------name----------------alias-------items-----------json_fn---------description---------- */
SDATACM (DTP_SCHEMA,    "help",             a_help,     pm_help,        cmd_help,       "Command's help"),
SDATACM (DTP_SCHEMA,    "download-firmware",0,          pm_download_firmware,cmd_download_firmware,"Download and install new firmware. When connected to controlcenter will be validated."),
SDATA_END()
};

/*---------------------------------------------*
 *          Attributes
 *---------------------------------------------*/
PRIVATE const sdata_desc_t tattr_desc[] = {
/*-ATTR-type--------name----------------flag--------------------default-description---------- */
SDATA (DTP_STRING,  "url_ota",          SDF_WR,                 "",     "OTA url"),
SDATA (DTP_STRING,  "cert_pem",         SDF_PERSIST,            "",     "SSL server certificate, PEM format"),
SDATA (DTP_INTEGER, "ota_state",        SDF_RD|SDF_STATS,       "",     "Image state"),
SDATA (DTP_BOOLEAN, "force",            0,                      "false", "Force loading of another yuno_role"),
SDATA (DTP_INTEGER, "timeout_validate", SDF_PERSIST,           "60000", "timeout to invalidate new imag"),
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
    hgobj gobj_timer;
    const char *url_ota;
    char binary_file[80];
    json_int_t content_length;
    size_t content_received;
    hgobj gobj_controlcenter_c;
#ifdef ESP_PLATFORM
    const esp_partition_t *update_partition;
    esp_ota_handle_t update_handle;
    BOOL image_header_was_checked;
#endif
#ifdef __linux__
    int fp;
#endif
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
    json_t *kw = json_pack("{s:b, s:s, s:I}",
        "raw_body_data", TRUE,
        "url", "",
        "subscriber", (json_int_t)(size_t)gobj
    );
    priv->gobj_http_cli_ota = gobj_create_pure_child("http_cli_ota", C_PROT_HTTP_CL, kw, gobj);
    gobj_set_bottom_gobj(gobj, priv->gobj_http_cli_ota);

    priv->gobj_timer = gobj_create_pure_child(gobj_name(gobj), C_TIMER, 0, gobj);

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
    priv->gobj_controlcenter_c = gobj_find_service("controlcenter_c", TRUE);
    gobj_subscribe_event(priv->gobj_controlcenter_c, EV_ON_OPEN, NULL, gobj);
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
 *      Framework Method reading
 ***************************************************************************/
PRIVATE SData_Value_t mt_reading(hgobj gobj, const char *name)
{
    SData_Value_t v = {0,{0}};
    if(strcmp(name, "ota_state")==0) {
#ifdef ESP_PLATFORM
        TODO
        const esp_partition_t *running = esp_ota_get_running_partition();
        esp_ota_img_states_t ota_state;
        if (esp_ota_get_state_partition(running, &ota_state) == ESP_OK) {
            gobj_write_integer_attr(gobj, name, ota_state);
        }
#endif
    }
    return v;
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

    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    gobj_start(priv->gobj_timer);

#ifdef __linux__
    gobj_write_integer_attr(gobj, "ota_state", ESP_OTA_IMG_PENDING_VERIFY); // To TEST
#endif

    esp_ota_img_states_t ota_state = (esp_ota_img_states_t)gobj_read_integer_attr(gobj, "ota_state");
    gobj_trace_msg(gobj, "🌀 ota_state %d", (int)ota_state);

    switch(ota_state) {
        case ESP_OTA_IMG_NEW: /*!< Monitor the first boot. In bootloader this state is changed to ESP_OTA_IMG_PENDING_VERIFY. */
            break;
        case ESP_OTA_IMG_PENDING_VERIFY: /*!< First boot for this app was. If while the second boot this state is then it will be changed to ABORTED. */
            set_timeout(priv->gobj_timer, gobj_read_integer_attr(gobj, "timeout_validate"));
            break;
        case ESP_OTA_IMG_VALID: /*!< App was confirmed as workable. App can boot and work without limits. */
        case ESP_OTA_IMG_INVALID: /*!< App was confirmed as non-workable. This app will not selected to boot at all. */
        case ESP_OTA_IMG_ABORTED: /*!< App could not confirm the workable or non-workable. In bootloader IMG_PENDING_VERIFY state will be changed to IMG_ABORTED. This app will not selected to boot at all. */
        case ESP_OTA_IMG_UNDEFINED: /*!< Undefined. App can boot and work without limits. */
        default:
            gobj_log_error(gobj, 0,
                "function",     "%s", __FUNCTION__,
                "msgset",       "%s", MSGSET_INTERNAL_ERROR,
                "msg",          "%s", "ota state",
                "ota_state",    "%d", (int)ota_state,
                NULL
            );
            break;
    }

    return 0;
}

/***************************************************************************
 *      Framework Method
 ***************************************************************************/
PRIVATE int mt_stop(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    gobj_stop(priv->gobj_timer);
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
    KW_INCREF(kw)
    json_t *jn_resp = gobj_build_cmds_doc(gobj, kw);
    return msg_iev_build_response(
        gobj,
        0,
        jn_resp,
        0,
        0,
        "",  // msg_type
        kw  // owned
    );

    // TODO is ok build_command_response or msg_iev_build_response?
    // json_t *jn_resp = gobj_build_cmds_doc(gobj, kw);
    //  return build_command_response(
    //      gobj,
    //      0,
    //      jn_resp,
    //      0,
    //      0
    //  );
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE json_t *cmd_download_firmware(hgobj gobj, const char *cmd, json_t *kw, hgobj src)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    esp_ota_img_states_t ota_state = (esp_ota_img_states_t)gobj_read_integer_attr(gobj, "ota_state");
    if(ota_state == ESP_OTA_IMG_PENDING_VERIFY) {
        json_t *kw_response = build_command_response(
            gobj,
            -1,     // result
            json_sprintf("Ota image pending of verify"),   // jn_comment
            0,      // jn_schema
            0       // jn_data
        );
        KW_DECREF(kw)
        return kw_response;
    }

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

    if(gobj_is_running(priv->gobj_http_cli_ota)) {
        json_t *kw_response = build_command_response(
            gobj,
            -1,     // result
            json_sprintf("gobj_http_cli_ota is ALREADY running"),   // jn_comment
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

    if(parse_url(
        gobj,
        url_ota,
        schema, sizeof(schema),
        host, sizeof(host),
        port, sizeof(port),
        priv->binary_file, sizeof(priv->binary_file),
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

    if(strlen(priv->binary_file)<1) {
        json_t *kw_response = build_command_response(
            gobj,
            -1,     // result
            json_sprintf("what binary file?"),   // jn_comment
            0,      // jn_schema
            0       // jn_data
        );
        JSON_DECREF(kw)
        return kw_response;
    }

    int same = strncasecmp(gobj_yuno_role(), basename(priv->binary_file), strlen(gobj_yuno_role()));
    if(same!=0 && !force) {
        json_t *kw_response = build_command_response(
            gobj,
            -1,     // result
            json_sprintf("Binary yuno role NOT MATCH, me: %s, other: %s",
                gobj_yuno_role(),
                basename(priv->binary_file)
            ),   // jn_comment
            0,      // jn_schema
            0       // jn_data
        );
        JSON_DECREF(kw)
        return kw_response;
    }

    /*
     *  At writing the url_ota attr the operation will begin
     */
    gobj_write_str_attr(gobj, "url_ota", url_ota);
    gobj_write_bool_attr(gobj, "force", force);

    json_t *kw_response = build_command_response(
        gobj,
        0,
        json_sprintf("Downloading %s ...", priv->binary_file),   // jn_comment
        0,
        json_pack("{s:s, s:s, s:s, s:s}",
            "schema", schema,
            "host", host,
            "port", port,
            "file", priv->binary_file
        )
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
PRIVATE int get_binary_file(
    hgobj gobj,
    const char *binary_file
) {
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    json_t *jn_headers = json_pack("{s:s, s:s, s:s}",
        "Accept", "*/*",
        "Content-Type", "application/octet-stream",
        "Accept-Ranges", "bytes"
    );

    json_t *query = json_pack("{s:s, s:s, s:s, s:o}",
        "method", "GET",
        "resource", binary_file,
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

/***************************************************************************
 *
 ***************************************************************************/
#ifdef ESP_PLATFORM
PRIVATE BOOL check_image(hgobj gobj, char *ota_write_data, size_t data_read)
{
    BOOL image_checked = FALSE;

    esp_app_desc_t new_app_info;
    if (data_read < sizeof(esp_image_header_t) + sizeof(esp_image_segment_header_t) + sizeof(esp_app_desc_t)) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_INTERNAL_ERROR,
            "msg",          "%s", "file message data TOO SHORT",
            "msg2",         "%s", "🌀👎file message data TOO SHORT",
            NULL
        );
        return FALSE;
    }

    // check current version with downloading
    memcpy(
        &new_app_info,
        &ota_write_data[sizeof(esp_image_header_t) + sizeof(esp_image_segment_header_t)],
        sizeof(esp_app_desc_t)
    );

    BOOL force = gobj_read_bool_attr(gobj, "force");
    int same = strncasecmp(gobj_yuno_role(), new_app_info.project_name, strlen(gobj_yuno_role()));
    if(same!=0 && !force) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_INTERNAL_ERROR,
            "msg",          "%s", "Binary yuno role NOT MATCH",
            "msg",          "%s", "🌀👎Binary yuno role NOT MATCH",
            "me",           "%s", gobj_yuno_role(),
            "other",        "%s", new_app_info.project_name,
            NULL
        );
        return FALSE;
    }

    gobj_log_info(gobj, 0,
        "function",     "%s", __FUNCTION__,
        "msgset",       "%s", MSGSET_INFO,
        "msg",          "%s", "Ok to download new firmware",
        "msg2",         "%s", "🌀 ✅Ok to download new firmware",
        "current",      "%s", gobj_yuno_role(),
        "new",          "%s", new_app_info.project_name,
        "force",        "%d", force,
        NULL
    );

    const esp_partition_t *running = esp_ota_get_running_partition();
    esp_app_desc_t running_app_info;
    esp_ota_get_partition_description(running, &running_app_info);
    if (memcmp(new_app_info.version, running_app_info.version, sizeof(new_app_info.version)) == 0) {
        /* Current running version is the same as a new */
        // Allow write the same version
    }

    image_checked = true;

    return image_checked;
}
#endif

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE int check_and_validate_new_image(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    esp_ota_img_states_t ota_state = (esp_ota_img_states_t)gobj_read_integer_attr(gobj, "ota_state");
    if(ota_state == ESP_OTA_IMG_PENDING_VERIFY) {
        gobj_log_info(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_STARTUP,
            "msg",          "%s", "OTA image verified",
            "msg2",         "%s", "🌀🌀🌀 ✅OTA image verified",
            NULL
        );
#ifdef ESP_PLATFORM
        esp_ota_mark_app_valid_cancel_rollback();
#endif
#ifdef __linux__
        gobj_write_integer_attr(gobj, "ota_state", ESP_OTA_IMG_UNDEFINED);
#endif
        clear_timeout(priv->gobj_timer);
    }
    return 0;
}




                    /***************************
                     *      Actions
                     ***************************/




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
        get_binary_file(gobj, priv->binary_file);

    } else if(src == priv->gobj_controlcenter_c) {
        check_and_validate_new_image(gobj);
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

    if(src == priv->gobj_http_cli_ota) {
#ifdef ESP_PLATFORM
        if(priv->update_handle) {
            // If update_handle is not zero then the firmware downloading was not successful
            esp_ota_abort(priv->update_handle);
            priv->update_handle = 0;
        }
        priv->update_partition = NULL;
        priv->update_handle = 0;
        priv->image_header_was_checked = FALSE;
#endif
        gobj_change_state(gobj, ST_DISCONNECTED);
        gobj_stop(priv->gobj_http_cli_ota);
    }


    JSON_DECREF(kw)
    return 0;
}

/***************************************************************************
 *  Message from http connection: response header
 ***************************************************************************/
PRIVATE int ac_on_header(hgobj gobj, gobj_event_t event, json_t *kw, hgobj src)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    if(gobj_trace_level(gobj) & TRACE_MESSAGES) {
        gobj_trace_json(gobj, kw, "%s <== HEADER %s",
            gobj_short_name(gobj),
            gobj_short_name(src)
        );
    }

    if(src == priv->gobj_http_cli_ota) {
        int response_status_code = (int)kw_get_int(gobj, kw, "response_status_code", -1, KW_REQUIRED);
        if(response_status_code != 200) {
            gobj_log_error(gobj, 0,
                "function",     "%s", __FUNCTION__,
                "msgset",       "%s", MSGSET_SYSTEM_ERROR,
                "msg",          "%s", "BAD HTTP Status",
                "msg2",         "%s", "🌀👎BAD HTTP Status",
                "status",       "%d", response_status_code,
                NULL
            );
            gobj_send_event(priv->gobj_http_cli_ota, EV_DROP, 0, gobj);
            KW_DECREF(kw)
            return 0;
        }

        priv->content_length = kw_get_int(gobj, kw, "headers`CONTENT-LENGTH", 0, KW_WILD_NUMBER);
        if(priv->content_length == 0) {
            gobj_log_error(gobj, 0,
                "function",     "%s", __FUNCTION__,
                "msgset",       "%s", MSGSET_SYSTEM_ERROR,
                "msg",          "%s", "content_length is 0",
                NULL
            );
            gobj_send_event(priv->gobj_http_cli_ota, EV_DROP, 0, gobj);
            KW_DECREF(kw)
            return 0;
        }
        priv->content_received = 0;

#ifdef __linux__
        const char *filename = basename(priv->binary_file);
        priv->fp = newfile(filename, 02770, TRUE);
#endif
    } else {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_INTERNAL_ERROR,
            "msg",          "%s", "source unknown",
            NULL
        );
        gobj_trace_json(gobj, kw, "source unknown");
    }

    KW_DECREF(kw)
    return 0;
}

/***************************************************************************
 *  Message from http connection: response message or partial body
 ***************************************************************************/
PRIVATE int ac_on_message(hgobj gobj, gobj_event_t event, json_t *kw, hgobj src)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    if(gobj_trace_level(gobj) & TRACE_MESSAGES) {
        gobj_trace_json(gobj, kw, "%s <== BODY %s",
            gobj_short_name(gobj),
            gobj_short_name(src)
        );
    }

    if(src == priv->gobj_http_cli_ota) {
        int response_status_code = (int)kw_get_int(gobj, kw, "response_status_code", -1, KW_REQUIRED);
        if(response_status_code != 200) {
            gobj_log_error(gobj, 0,
                "function",     "%s", __FUNCTION__,
                "msgset",       "%s", MSGSET_SYSTEM_ERROR,
                "msg",          "%s", "BAD HTTP Status",
                "msg2",         "%s", "🌀👎BAD HTTP Status",
                "status",       "%d", response_status_code,
                NULL
            );
            gobj_send_event(priv->gobj_http_cli_ota, EV_DROP, 0, gobj);
            KW_DECREF(kw)
            return 0;
        }

        char *ota_write_data = (char *)(size_t)kw_get_int(gobj, kw, "__pbf__", 0, 0);
        if(ota_write_data) {
            /*---------------------------------------------*
             *          Receiving file
             *---------------------------------------------*/
            size_t data_read = (size_t)kw_get_int(gobj, kw, "__pbf_size__", 0, 0);
            priv->content_received += data_read;
#ifdef __linux__
            if(priv->fp > 0) {
                write(priv->fp, ota_write_data, data_read);
            }
#endif
#ifdef ESP_PLATFORM
            if(!priv->image_header_was_checked) {
                priv->image_header_was_checked = check_image(gobj, ota_write_data, data_read);
                if(!priv->image_header_was_checked) {
                    // Error already logged
                    gobj_send_event(priv->gobj_http_cli_ota, EV_DROP, 0, gobj);
                    KW_DECREF(kw)
                    return 0;
                }
                priv->update_partition = esp_ota_get_next_update_partition(NULL);
                esp_err_t err = esp_ota_begin(
                    priv->update_partition,
                    OTA_WITH_SEQUENTIAL_WRITES, //priv->content_length,
                    &priv->update_handle
                );
                if (err != ESP_OK) {
                    gobj_log_error(gobj, 0,
                        "function",     "%s", __FUNCTION__,
                        "msgset",       "%s", MSGSET_SYSTEM_ERROR,
                        "msg",          "%s", "esp_ota_begin() FAILED",
                        "msg2",         "%s", "🌀👎esp_ota_begin() FAILED",
                        "error",        "%d", err,
                        "serror",       "%s", esp_err_to_name(err),
                        NULL
                    );
                    if(priv->update_handle) {
                        esp_ota_abort(priv->update_handle);
                        priv->update_handle = 0;
                    }
                    gobj_send_event(priv->gobj_http_cli_ota, EV_DROP, 0, gobj);
                    KW_DECREF(kw)
                    return 0;
                }
                gobj_trace_msg(gobj, "🌀esp_ota_begin() succeeded");
            }

            esp_err_t err = esp_ota_write(priv->update_handle, (const void *)ota_write_data, data_read);
            if (err != ESP_OK) {
                gobj_log_error(gobj, 0,
                    "function",     "%s", __FUNCTION__,
                    "msgset",       "%s", MSGSET_SYSTEM_ERROR,
                    "msg",          "%s", "esp_ota_write() FAILED",
                    "msg2",         "%s", "🌀👎esp_ota_write() FAILED",
                    "error",        "%d", err,
                    "serror",       "%s", esp_err_to_name(err),
                    NULL
                );
                if(priv->update_handle) {
                    esp_ota_abort(priv->update_handle);
                    priv->update_handle = 0;
                }
                gobj_send_event(priv->gobj_http_cli_ota, EV_DROP, 0, gobj);
                KW_DECREF(kw)
                return 0;
            }
            gobj_trace_msg(gobj, "🌀esp_ota_write() total:%lu, partial:%lu, rx %lu bytes",
                (unsigned long)priv->content_length,
                (unsigned long)priv->content_received,
                (unsigned long)data_read
            );
#endif
        } else {
            /*---------------------------------------------*
             *          End of file
             *---------------------------------------------*/
#ifdef __linux__
            if(priv->fp > 0) {
                close(priv->fp);
                priv->fp = 0;
            }
            gobj_send_event(priv->gobj_http_cli_ota, EV_DROP, 0, gobj);
#endif
#ifdef ESP_PLATFORM
            esp_err_t err = esp_ota_end(priv->update_handle);
            priv->update_handle = 0;
            if (err != ESP_OK) {
                gobj_log_error(gobj, 0,
                    "function",     "%s", __FUNCTION__,
                    "msgset",       "%s", MSGSET_SYSTEM_ERROR,
                    "msg",          "%s", "esp_ota_end() FAILED",
                    "msg2",         "%s", "🌀👎esp_ota_end() FAILED",
                    "error",        "%d", err,
                    "serror",       "%s", esp_err_to_name(err),
                    NULL
                );
                gobj_send_event(priv->gobj_http_cli_ota, EV_DROP, 0, gobj);
                KW_DECREF(kw)
                return 0;
            }
            err = esp_ota_set_boot_partition(priv->update_partition);
            if (err != ESP_OK) {
                gobj_log_error(gobj, 0,
                    "function",     "%s", __FUNCTION__,
                    "msgset",       "%s", MSGSET_SYSTEM_ERROR,
                    "msg",          "%s", "esp_ota_set_boot_partition() FAILED",
                    "msg2",         "%s", "🌀👎esp_ota_set_boot_partition() FAILED",
                    "error",        "%d", err,
                    "serror",       "%s", esp_err_to_name(err),
                    NULL
                );
                gobj_send_event(priv->gobj_http_cli_ota, EV_DROP, 0, gobj);
                KW_DECREF(kw)
                return 0;
            }
            gobj_log_info(gobj, 0,
                "function",     "%s", __FUNCTION__,
                "msgset",       "%s", MSGSET_INFO,
                "msg",          "%s", "Prepare to restart system",
                "msg2",         "%s", "🌀🌀🌀 ✅Prepare to restart system",
                NULL
            );
            esp_restart();
#endif
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

/***************************************************************************
 *      Timeout without connecting to controlcenter, invalidate image
 ***************************************************************************/
PRIVATE int ac_timeout(hgobj gobj, gobj_event_t event, json_t *kw, hgobj src)
{
    gobj_log_error(gobj, 0,
        "function",     "%s", __FUNCTION__,
        "msgset",       "%s", MSGSET_STARTUP,
        "msg",          "%s", "OTA image NOT verified, rollback",
        "msg2",         "%s", "🌀👎OTA image NOT verified,rollback",
        NULL
    );
#ifdef ESP_PLATFORM
    esp_ota_mark_app_invalid_rollback_and_reboot();
#endif
#ifdef __linux__
    gobj_write_integer_attr(gobj, "ota_state", ESP_OTA_IMG_UNDEFINED);
#endif
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
    .mt_reading = mt_reading,
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
        {EV_TIMEOUT,                ac_timeout,                 0},
        {0,0,0}
    };
    ev_action_t st_session[] = {
        {EV_ON_MESSAGE,             ac_on_message,              0},
        {EV_ON_HEADER,              ac_on_header,               0},
        {EV_ON_OPEN,                0,                          0},
        {EV_ON_CLOSE,               ac_on_close,                0},
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
