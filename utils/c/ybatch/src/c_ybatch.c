/***********************************************************************
 *          C_YBATCH.C
 *          YBatch GClass.
 *
 *          Yuneta Batch
 *
 *          Copyright (c) 2016 Niyamaka.
 *          Copyright (c) 2025, ArtGins.
 *          All Rights Reserved.
 ***********************************************************************/
#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <limits.h>

#include "c_ybatch.h"

/***************************************************************************
 *              Constants
 ***************************************************************************/

/***************************************************************************
 *              Structures
 ***************************************************************************/

/***************************************************************************
 *              Prototypes
 ***************************************************************************/
PRIVATE int do_authenticate_task(hgobj gobj);
PRIVATE int extrae_json(hgobj gobj);
PRIVATE int cmd_connect(hgobj gobj);
PRIVATE int display_webix_result(
    hgobj gobj,
    const char *command,
    json_t *webix
);

/***************************************************************************
 *          Data: config, public data, private data
 ***************************************************************************/
PRIVATE sdata_desc_t commands_desc[] = {
/*-ATTR-type------------name----------------flag------------------------default---------description---------- */
SDATA (DTP_STRING,      "command",          0,                          0,              "command"),
SDATA (DTP_STRING,      "date",             0,                          0,              "date of command"),
SDATA (DTP_JSON,        "kw",               0,                          0,              "kw"),
SDATA (DTP_BOOLEAN,     "ignore_fail",      0,                          0,              "continue batch although fail"),
SDATA (DTP_JSON,        "response_filter",  0,                          0,              "Keys to validate the response"),

SDATA_END()
};

/*---------------------------------------------*
 *      Attributes - order affect to oid's
 *---------------------------------------------*/
PRIVATE sdata_desc_t tattr_desc[] = {
/*-ATTR-type------------name----------------flag--------default---------description---------- */
SDATA (DTP_INTEGER,     "verbose",          0,          0,              "Verbose mode."),
SDATA (DTP_STRING,      "path",             0,          0,              "Batch filename to execute."),
SDATA (DTP_INTEGER,     "pause",            0,          0,              "Pause between executions"),

SDATA (DTP_STRING,      "auth_system",      0,          "",             "OpenID System(interactive jwt)"),
SDATA (DTP_STRING,      "auth_url",         0,          "",             "OpenID Endpoint(interactive jwt)"),
SDATA (DTP_STRING,      "azp",              0,          "",             "azp (OAuth2 Authorized Party)"),
SDATA (DTP_STRING,      "user_id",          0,          "",             "OAuth2 User Id (interactive jwt)"),
SDATA (DTP_STRING,      "user_passw",       0,          "",             "OAuth2 User password (interactive jwt)"),
SDATA (DTP_STRING,      "jwt",              0,          "",             "Jwt"),
SDATA (DTP_STRING,      "url",              0,                          "ws://127.0.0.1:1991",  "Agent's url to connect. Can be a ip/hostname or a full url"),
SDATA (DTP_STRING,      "yuno_name",        0,          "",             "Yuno name"),
SDATA (DTP_STRING,      "yuno_role",        0,          "yuneta_agent", "Yuno role"),
SDATA (DTP_STRING,      "yuno_service",     0,          "agent",        "Yuno service"),
SDATA (DTP_STRING,      "display_mode",     0,          "form",         "Display mode: table or form"),

SDATA (DTP_INTEGER,     "timeout",          0,          "900000",       "Timeout service responses"),
SDATA (DTP_POINTER,     "user_data",        0,          0,              "user data"),
SDATA (DTP_POINTER,     "user_data2",       0,          0,              "more user data"),
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
    int32_t timeout;
    int32_t pause;
    int verbose;
    const char *path;
    hgobj timer;
    hgobj remote_service;
    json_t *batch_iter;
    json_t *hs;
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

    priv->timer = gobj_create_pure_child(gobj_name(gobj), C_TIMER, 0, gobj);
    priv->batch_iter = json_array();

    /*
     *  Do copy of heavy used parameters, for quick access.
     *  HACK The writable attributes must be repeated in mt_writing method.
     */
    SET_PRIV(timeout,               gobj_read_integer_attr)
    SET_PRIV(pause,                 gobj_read_integer_attr)
    SET_PRIV(verbose,               gobj_read_integer_attr)
    SET_PRIV(path,                  gobj_read_str_attr)
}

/***************************************************************************
 *      Framework Method writing
 ***************************************************************************/
PRIVATE void mt_writing(hgobj gobj, const char *path)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    IF_EQ_SET_PRIV(timeout,             gobj_read_integer_attr)
    END_EQ_SET_PRIV()
}

/***************************************************************************
 *      Framework Method destroy
 ***************************************************************************/
PRIVATE void mt_destroy(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);
    JSON_DECREF(priv->hs)
    json_decref(priv->batch_iter);
}

/***************************************************************************
 *      Framework Method start
 ***************************************************************************/
PRIVATE int mt_start(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    extrae_json(gobj);
    gobj_start(priv->timer);

    const char *auth_url = gobj_read_str_attr(gobj, "auth_url");
    const char *user_id = gobj_read_str_attr(gobj, "user_id");
    if(!empty_string(auth_url) && !empty_string(user_id)) {
        /*
         *  HACK if there are user_id and endpoint
         *  then try to authenticate
         *  else use default local connection
         */
        do_authenticate_task(gobj);
    } else {
        cmd_connect(gobj);
    }

    return 0;
}

/***************************************************************************
 *      Framework Method stop
 ***************************************************************************/
PRIVATE int mt_stop(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);
    clear_timeout(priv->timer);
    return 0;
}




            /***************************
             *      Commands
             ***************************/




            /***************************
             *      Local Methods
             ***************************/




/***************************************************************************
 *
 ***************************************************************************/
PRIVATE int do_authenticate_task(hgobj gobj)
{
    /*-----------------------------*
     *      Create the task
     *-----------------------------*/
    json_t *kw = json_pack("{s:s, s:s, s:s, s:s, s:s}",
        "auth_system", gobj_read_str_attr(gobj, "auth_system"),
        "auth_url", gobj_read_str_attr(gobj, "auth_url"),
        "user_id", gobj_read_str_attr(gobj, "user_id"),
        "user_passw", gobj_read_str_attr(gobj, "user_passw"),
        "azp", gobj_read_str_attr(gobj, "azp")
    );

    hgobj gobj_task = gobj_create_service(
        "task-authenticate",
        C_TASK_AUTHENTICATE,
        kw,
        gobj
    );
    gobj_subscribe_event(gobj_task, EV_ON_TOKEN, 0, gobj);
    gobj_set_volatil(gobj_task, TRUE); // auto-destroy

    /*-----------------------*
     *      Start task
     *-----------------------*/
    return gobj_start(gobj_task);
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE gbuffer_t *jsontable2str(json_t *jn_schema, json_t *jn_data)
{
    gbuffer_t *gbuf = gbuffer_create(4*1024, gbmem_get_maximum_block());
    hgobj gobj = NULL;
    size_t col;
    json_t *jn_col;
    /*
     *  Paint Headers
     */
    json_array_foreach(jn_schema, col, jn_col) {
        const char *header = kw_get_str(gobj, jn_col, "header", "", 0);
        int fillspace = (int)kw_get_int(gobj, jn_col, "fillspace", 10, 0);
        if(fillspace > 0) {
            gbuffer_printf(gbuf, "%-*.*s ", fillspace, fillspace, header);
        }
    }
    gbuffer_printf(gbuf, "\n");

    /*
     *  Paint ===
     */
    json_array_foreach(jn_schema, col, jn_col) {
        int fillspace = (int)kw_get_int(gobj, jn_col, "fillspace", 10, 0);
        if(fillspace > 0) {
            gbuffer_printf(gbuf,
                "%*.*s ",
                fillspace,
                fillspace,
                "==========================================================================="
            );
        }
    }
    gbuffer_printf(gbuf, "\n");

    /*
     *  Paint data
     */
    size_t row;
    json_t *jn_row;
    json_array_foreach(jn_data, row, jn_row) {
        json_array_foreach(jn_schema, col, jn_col) {
            const char *id = kw_get_str(gobj, jn_col, "id", 0, 0);
            int fillspace = (int)kw_get_int(gobj, jn_col, "fillspace", 10, 0);
            if(fillspace > 0) {
                json_t *jn_cell = kw_get_dict_value(gobj, jn_row, id, 0, 0);
                char *text = json2uglystr(jn_cell);
                if(json_is_number(jn_cell) || json_is_boolean(jn_cell)) {
                    //gbuffer_printf(gbuf, "%*s ", fillspace, text);
                    gbuffer_printf(gbuf, "%-*.*s ", fillspace, fillspace, text);
                } else {
                    gbuffer_printf(gbuf, "%-*.*s ", fillspace, fillspace, text);
                }
                GBMEM_FREE(text);
            }
        }
        gbuffer_printf(gbuf, "\n");
    }
    gbuffer_printf(gbuf, "\nTotal: %d\n", (int)row);

    return gbuf;
}

/***************************************************************************
 *  Print json response in display list window
 ***************************************************************************/
PRIVATE int display_webix_result(
    hgobj gobj,
    const char *command,
    json_t *webix)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);
    int result = (int)kw_get_int(gobj, webix, "result", -1, 0);
    json_t *jn_schema = kw_get_dict_value(gobj, webix, "schema", 0, 0);
    json_t *jn_data = kw_get_dict_value(gobj, webix, "data", 0, 0);

    const char *display_mode = gobj_read_str_attr(gobj, "display_mode");
    json_t *jn_display_mode = kw_get_subdict_value(
        gobj, webix, "__md_iev__", "display_mode", 0, 0
    );
    if(jn_display_mode) {
        display_mode = json_string_value(jn_display_mode);
    }

    BOOL mode_form = FALSE;
    if(!empty_string(display_mode)) {
        if(strcasecmp(display_mode, "form")==0)  {
            mode_form = TRUE;
        }
    }

    if(json_is_array(jn_data) && json_array_size(jn_data)>0) {
        if (mode_form) {
            char *data = json2str(jn_data);
            if(priv->verbose >=2)  {
                printf("%s\n", data);
            }
            gbmem_free(data);
        } else {
            /*
            *  display as table
            */
            if(jn_schema && json_array_size(jn_schema)) {
                gbuffer_t *gbuf = jsontable2str(jn_schema, jn_data);
                if(gbuf) {
                    if(priv->verbose >=2)  {
                        printf("%s\n", (char *)gbuffer_cur_rd_pointer(gbuf));
                    }
                    gbuffer_decref(gbuf);
                }
            } else {
                char *text = json2str(jn_data);
                if(text) {
                    if(priv->verbose >=2)  {
                        printf("%s\n", text);
                    }
                    gbmem_free(text);
                }
            }
        }
    } else if(json_is_object(jn_data)) {
        char *data = json2str(jn_data);
        if(priv->verbose >=2)  {
            printf("%s\n", data);
        }
        gbmem_free(data);
    }

    JSON_DECREF(webix);
    return result;
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE int extrae_json(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    /*
     *  Open commands file
     */
    FILE *file = fopen(priv->path, "r");
    if(!file) {
        printf("ybatch: cannot open '%s' file\n", priv->path);
        exit(-1);
    }

    /*
     *  Load commands
     */
    #define WAIT_BEGIN_DICT 0
    #define WAIT_END_DICT   1
    int c;
    int st = WAIT_BEGIN_DICT;
    int brace_indent = 0;
    gbuffer_t *gbuf = gbuffer_create(4*1024, gbmem_get_maximum_block());
    while((c=fgetc(file))!=EOF) {
        switch(st) {
        case WAIT_BEGIN_DICT:
            if(c != '{') {
                continue;
            }
            gbuffer_clear(gbuf);
            gbuffer_append(gbuf, &c, 1);
            brace_indent = 1;
            st = WAIT_END_DICT;
            break;
        case WAIT_END_DICT:
            if(c == '{') {
                brace_indent++;
            } else if(c == '}') {
                brace_indent--;
            }
            gbuffer_append(gbuf, &c, 1);
            if(brace_indent == 0) {
                //log_debug_gbuf("TEST", gbuf);
                json_t *jn_dict = legalstring2json(gbuffer_cur_rd_pointer(gbuf), TRUE);
                if(jn_dict) {
                    if(kw_get_str(gobj, jn_dict, "command", 0, 0)) {
                        json_t *hs_cmd = gobj_sdata_create(gobj, commands_desc);
                        json_object_update_existing(hs_cmd, jn_dict);
                        const char *command = kw_get_str(gobj, hs_cmd, "command", "", KW_REQUIRED);
                        if(command && (*command == '-')) {
                            json_object_set_new(hs_cmd, "command", json_string(command+1));
                            json_object_set_new(hs_cmd, "ignore_fail", json_true());
                        }
                        json_array_append_new(priv->batch_iter, hs_cmd);
                    } else {
                        printf("Line ignored: '%s'\n", (char *)gbuffer_cur_rd_pointer(gbuf));
                    }
                    json_decref(jn_dict);
                } else {
                    gobj_log_error(gobj, 0,
                        "function",     "%s", __FUNCTION__,
                        "msgset",       "%s", MSGSET_SERVICE_ERROR,
                        "msg",          "%s", "Error json",
                        NULL
                    );
                    //log_debug_gbuf("FAILED", gbuf);
                }
                st = WAIT_BEGIN_DICT;
            }
            break;
        }
    }
    fclose(file);
    gbuffer_decref(gbuf);

    //log_debug_sd_iter("TEST", 0, &priv->batch_iter);

    return 0;
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE char agent_config[]= "\
{                                               \n\
    'name': 'agent_client',                    \n\
    'gclass': 'IEvent_cli',                     \n\
    'as_service': true,                          \n\
    'kw': {                                     \n\
        'jwt': '(^^__jwt__^^)',                         \n\
        'remote_yuno_name': '(^^__yuno_name__^^)',      \n\
        'remote_yuno_role': '(^^__yuno_role__^^)',      \n\
        'remote_yuno_service': '(^^__yuno_service__^^)' \n\
    },                                          \n\
    'children': [                                 \n\
        {                                               \n\
            'name': 'agent_client',                    \n\
            'gclass': 'C_IOGATE',                         \n\
            'kw': {                                     \n\
            },                                          \n\
            'children': [                                 \n\
                {                                               \n\
                    'name': 'agent_client',                    \n\
                    'gclass': 'C_CHANNEL',                        \n\
                    'kw': {                                     \n\
                    },                                          \n\
                    'children': [                                 \n\
                        {                                               \n\
                            'name': 'agent_client',                    \n\
                            'gclass': 'C_WEBSOCKET',                     \n\
                            'children': [                           \n\
                                {                                   \n\
                                    'name': 'agent_client',         \n\
                                    'gclass': 'C_TCP',              \n\
                                    'kw': {                         \n\
                                        'url':'(^^__url__^^)'       \n\
                                    }                               \n\
                                }                                   \n\
                            ]                                       \n\
                        }                                       \n\
                    ]                                           \n\
                }                                               \n\
            ]                                           \n\
        }                                               \n\
    ]                                           \n\
}                                               \n\
";

PRIVATE int cmd_connect(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);
    const char *jwt = gobj_read_str_attr(gobj, "jwt");
    const char *url = gobj_read_str_attr(gobj, "url");
    const char *yuno_name = gobj_read_str_attr(gobj, "yuno_name");
    const char *yuno_role = gobj_read_str_attr(gobj, "yuno_role");
    const char *yuno_service = gobj_read_str_attr(gobj, "yuno_service");

    /*
     *  Each display window has a gobj to send the commands (saved in user_data).
     *  For external agents create a filter-chain of gobjs
     */
    json_t *jn_config_variables = json_pack("{s:s, s:s, s:s, s:s, s:s}",
        "__jwt__", jwt,
        "__url__", url,
        "__yuno_name__", yuno_name,
        "__yuno_role__", yuno_role,
        "__yuno_service__", yuno_service
    );

    /*
     *  Get schema to select tls or not
     */
    char schema[20]={0}, host[120]={0}, port[40]={0};
    if(parse_url(
        gobj,
        url,
        schema,
        sizeof(schema),
        host, sizeof(host),
        port, sizeof(port),
        0, 0,
        0, 0,
        FALSE
    )<0) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_PARAMETER_ERROR,
            "msg",          "%s", "parse_http_url() FAILED",
            "url",          "%s", url,
            NULL
        );
    }

    hgobj gobj_remote_agent = gobj_create_tree(
        gobj,
        agent_config,
        jn_config_variables // owned
    );

    gobj_start_tree(gobj_remote_agent);

    if(priv->verbose)  {
        printf("Connecting to %s...\n", url);
    }

    return 0;
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE int execute_command(hgobj gobj, json_t *kw_command)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    const char *command = kw_get_str(gobj, kw_command, "command", 0, 0);
    if(!command) {
        printf("\nError: no command\n");
        return 0;
    }
    char comment[512]={0};
    if(priv->verbose) {
        printf("\n--> '%s'\n", command);
    }

    gbuffer_t *gbuf_parsed_command = replace_cli_vars(command, comment, sizeof(comment));
    if(!gbuf_parsed_command) {
        printf("Error %s.\n", empty_string(comment)?"replace_cli_vars() FAILED":comment),
        gobj_set_exit_code(-1);
        gobj_shutdown();
        return 0;
    }
    char *xcmd = gbuffer_cur_rd_pointer(gbuf_parsed_command);

    json_t *kw_clone = 0;
    json_t *kw = kw_get_dict(gobj, kw_command, "kw", 0, 0);
    if(kw) {
        kw_clone = json_deep_copy(msg_iev_clean_metadata(kw));
    }
    gobj_command(priv->remote_service, xcmd, kw_clone, gobj);
    gbuffer_decref(gbuf_parsed_command);

    set_timeout(priv->timer, priv->timeout);
    gobj_change_state(gobj, ST_WAIT_RESPONSE);
    return 0;
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE int tira_dela_cola(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    /*
     *  A por el próximo command
     */
    JSON_DECREF(priv->hs)
    priv->hs = kw_get_list_value(gobj, priv->batch_iter, 0, KW_EXTRACT);
    if(priv->hs) {
        /*
         *  Hay mas comandos
         */
        return execute_command(gobj, priv->hs);
    }

    /*
     *  No hay mas comandos.
     *  Se ha terminado el ciclo
     *  Mira si se repite
     */
    if(priv->verbose) {
        printf("\n==> All done!\n\n");
    }

    gobj_change_state(gobj, ST_CONNECTED);
    set_timeout(priv->timer, 2*1000);

    return 0;
}



            /***************************
             *      Actions
             ***************************/




/***************************************************************************
 *
 ***************************************************************************/
PRIVATE int ac_on_token(hgobj gobj, const char *event, json_t *kw, hgobj src)
{
    int result = (int)kw_get_int(gobj, kw, "result", -1, KW_REQUIRED);
    if(result < 0) {
        if(1) {
            const char *comment = kw_get_str(gobj, kw, "comment", "", 0);
            printf("\n%s", comment);
            printf("\nAbort.\n");
        }
        gobj_set_exit_code(-1);
        gobj_shutdown();
    } else {
        const char *jwt = kw_get_str(gobj, kw, "jwt", "", KW_REQUIRED);
        gobj_write_str_attr(gobj, "jwt", jwt);
        cmd_connect(gobj);
    }

    KW_DECREF(kw);
    return 0;
}

/***************************************************************************
 *  Execute batch of input parameters when the route is opened.
 ***************************************************************************/
PRIVATE int ac_on_open(hgobj gobj, const char *event, json_t *kw, hgobj src)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);
    const char *agent_name = kw_get_str(gobj, kw, "remote_yuno_name", 0, 0); // remote agent name

    printf("Connected to '%s'.\n", agent_name);

    priv->remote_service = src;

    tira_dela_cola(gobj);

    KW_DECREF(kw);
    return 0;
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE int ac_on_close(hgobj gobj, const char *event, json_t *kw, hgobj src)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    clear_timeout(priv->timer);

    if(!gobj_is_running(gobj)) {
        KW_DECREF(kw);
        return 0;
    }
    printf("Disconnected.\n"),

    gobj_set_exit_code(-1);
    gobj_shutdown();

    // No puedo parar y destruir con libuv.
    // De momento conexiones indestructibles, destruibles solo con la salida del yuno.
    // Hasta que quite la dependencia de libuv. FUTURE
    //gobj_stop_tree(src);
    //gobj_destroy(tree);

    KW_DECREF(kw);
    return 0;
}

/***************************************************************************
 *  Response from agent mt_stats
 *  Response from agent mt_command
 *  Response to asynchronous queries
 *  The received event is generated by a Counter with kw:
 *      max_count: items raised
 *      cur_count: items reached with success
 ***************************************************************************/
PRIVATE int ac_mt_command_answer(hgobj gobj, const char *event, json_t *kw, hgobj src)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    clear_timeout(priv->timer);

    const char *command = kw_get_str(gobj, priv->hs, "command", "", KW_REQUIRED);
    json_t *jn_response_filter = kw_get_dict(gobj, priv->hs, "response_filter", 0, 0);

    if(jn_response_filter) {
        json_t *jn_data = kw_get_dict_value(gobj, kw, "data", 0, 0);
        json_t *jn_tocmp = jn_data;
        if(json_is_array(jn_data)) {
            jn_tocmp = json_array_get(jn_data, 0);
        }
        JSON_INCREF(jn_response_filter);
        if(!kw_match_simple(jn_tocmp, jn_response_filter)) {
            if(priv->verbose > 1) {
                char *text = json2str(jn_tocmp);
                if(text) {
                    printf("NOT MATCH: %s\n", text);
                    gbmem_free(text);
                }
            }
            KW_DECREF(kw);
            return 0;
        }
    }

    int result = (int)kw_get_int(gobj, kw, "result", -1, 0);
    const char *comment = kw_get_str(gobj, kw, "comment", "", 0);
    BOOL ignore_fail = kw_get_bool(gobj, priv->hs, "ignore_fail", 0, 0);
    if(!ignore_fail && result < 0) {
        /*
         *  Comando con error y sin ignorar error, aborta
         */
        printf("%s%s: %s%s\n", On_Red BWhite, "ERROR", comment, Color_Off);

        gobj_set_exit_code(-1);
        gobj_shutdown();
        KW_DECREF(kw);
        return -1;
    }

    if(priv->verbose) {
        if(result < 0) {
            printf("<-- %s%s: %s%s\n", On_Red BWhite, "ERROR", comment, Color_Off);
        } else {
            printf("<-- %s%s: %s%s\n", On_Green BWhite, "Ok", comment, Color_Off);
        }
    }

    display_webix_result(
        gobj,
        command,
        kw  // owned
    );

    tira_dela_cola(gobj);

    return 0;
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE int ac_timeout(hgobj gobj, const char *event, json_t *kw, hgobj src)
{
    if(gobj_current_state(gobj) == ST_WAIT_RESPONSE) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_INTERNAL_ERROR,
            "msg",          "%s", "Timeout",
            NULL
        );
        gobj_set_exit_code(-1);
    } else {
        gobj_set_exit_code(0);
    }
    gobj_shutdown();

    KW_DECREF(kw);
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
    .mt_writing     = mt_writing,
};

/*------------------------*
 *      GClass name
 *------------------------*/
GOBJ_DEFINE_GCLASS(C_YBATCH);

/*------------------------*
 *      States
 *------------------------*/

/*------------------------*
 *      Events
 *------------------------*/

/***************************************************************************
 *          Create the GClass
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

    /*------------------------*
     *      States
     *------------------------*/
    ev_action_t st_disconnected[] = {
        {EV_ON_TOKEN,           ac_on_token,                0},
        {EV_ON_OPEN,            ac_on_open,                 ST_CONNECTED},
        {EV_ON_CLOSE,           ac_on_close,                0},
        {EV_STOPPED,            0,                          0},
        {0,0,0}
    };

    ev_action_t st_connected[] = {
        {EV_ON_CLOSE,           ac_on_close,                ST_DISCONNECTED},
        {EV_TIMEOUT,            ac_timeout,                 0},
        {EV_STOPPED,            0,                          0},
        {0,0,0}
    };

    ev_action_t st_wait_response[] = {
        {EV_ON_CLOSE,           ac_on_close,                ST_DISCONNECTED},
        {EV_MT_STATS_ANSWER,    ac_mt_command_answer,       ST_CONNECTED},
        {EV_MT_COMMAND_ANSWER,  ac_mt_command_answer,       ST_CONNECTED},
        {EV_TIMEOUT,            ac_timeout,                 0},
        {EV_STOPPED,            0,                          0},
        {0,0,0}
    };

    states_t states[] = {
        {ST_DISCONNECTED,       st_disconnected},
        {ST_CONNECTED,          st_connected},
        {ST_WAIT_RESPONSE,      st_wait_response},
        {0, 0}
    };

    /*------------------------*
     *      Events
     *------------------------*/
    event_type_t event_types[] = {
        {EV_MT_STATS_ANSWER,    EVF_PUBLIC_EVENT},
        {EV_MT_COMMAND_ANSWER,  EVF_PUBLIC_EVENT},
        {EV_ON_TOKEN,           0},
        {EV_ON_OPEN,            0},
        {EV_ON_CLOSE,           0},
        {EV_STOPPED,            0},
        {EV_TIMEOUT,            0},
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
        0,  // Local methods table (LMT)
        tattr_desc,
        sizeof(PRIVATE_DATA),
        0,  // Authorization table
        0,  // Command table
        s_user_trace_level,
        0   // GClass flags
    );
    if(!__gclass__) {
        return -1;
    }

    return 0;
}

/***************************************************************************
 *          Public access
 ***************************************************************************/
PUBLIC int register_c_ybatch(void)
{
    return create_gclass(C_YBATCH);
}
