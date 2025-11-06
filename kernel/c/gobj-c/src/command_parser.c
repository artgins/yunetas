/***********************************************************************
 *          COMMAND_PARSER.C
 *
 *          Command parser
 *
 *          Copyright (c) 2017-2023 Niyamaka.
 *          Copyright (c) 2024, ArtGins
 *          All Rights Reserved.
***********************************************************************/
#include <string.h>
#include "gobj.h"
#include "command_parser.h"

/***************************************************************
 *              Constants
 ***************************************************************/

/***************************************************************
 *              Structures
 ***************************************************************/

/***************************************************************
 *              Prototypes
 ***************************************************************/
PRIVATE json_t *expand_command(
    hgobj gobj,
    const char *command,
    json_t *kw,     // NOT owned
    const sdata_desc_t **cmd_desc
);
PRIVATE json_t *build_cmd_kw(
    hgobj gobj,
    const char *command,
    const sdata_desc_t *cnf_cmd,
    char *parameters,
    json_t *kw, // not owned
    int *result
);

/***************************************************************
 *              Data
 ***************************************************************/

/***************************************************************************
 *
 ***************************************************************************/
PUBLIC json_t * command_parser(
    hgobj gobj,
    const char *command,
    json_t *kw,
    hgobj src
)
{
    const sdata_desc_t *cnf_cmd = 0;
    json_t *kw_cmd = expand_command(gobj, command, kw, &cnf_cmd);
    if(gobj_trace_level(gobj) & (TRACE_EV_KW)) {
        gobj_trace_json(gobj, kw_cmd, "expanded_command: kw_cmd");
    }
    if(!cnf_cmd) {
        json_t *kw_response = build_command_response(
            gobj,
            -1,
            kw_cmd,
            0,
            0
        );
        KW_DECREF(kw)
        return kw_response;
    }

    /*-----------------------------------------------*
     *  Check AUTHZ
     *-----------------------------------------------*/
//     if(cnf_cmd->flag & SDF_AUTHZ_X) {
//         /*
//          *  AUTHZ Required
//          */
//         json_t *kw_authz = json_pack("{s:s}",
//             "command", command
//         );
//         if(kw) {
//             json_object_set(kw_authz, "kw", kw);
//         } else {
//             json_object_set_new(kw_authz, "kw", json_object());
//         }
//         if(!gobj_user_has_authz(
//                 gobj,
//                 "__execute_command__",
//                 kw_authz,
//                 src
//           )) {
//             log_error(0,
//                 "function",     "%s", __FUNCTION__,
//                 "msgset",       "%s", MSGSET_AUTH_ERROR,
//                 "msg",          "%s", "No permission to execute command",
//                 //"user",         "%s", gobj_get_user(src),
//                 "gclass",       "%s", gobj_gclass_name(gobj),
//                 "command",      "%s", command?command:"",
//                 NULL
//             );
//             return msg_iev_build_response(
//                 gobj,
//                 -403,
//                 json_sprintf(
//                     "No permission to execute command"
//                 ),
//                 0,
//                 0,
//                 kw
//             );
//         }
//     }

    json_t *kw_response = 0;
    if(cnf_cmd->json_fn) {
        kw_response = (cnf_cmd->json_fn)(gobj, cnf_cmd->name, kw_cmd, src);
    } else {
        /*
         *  Redirect command to event
         */
        const char *event;
        if(*cnf_cmd->alias) {
            event = *cnf_cmd->alias;
        } else {
            event = cnf_cmd->name;
        }

        event_type_t *event_type = gobj_find_event_type(event, 0, TRUE);
        if(event_type) {
            event = event_type->event_name;
        }

        gobj_send_event(gobj, event, kw_cmd, src);
        KW_DECREF(kw)
        return 0;   /* asynchronous response */
    }
    KW_DECREF(kw)
    return kw_response;  /* can be null if asynchronous response */
}

/***************************************************************************
 *  Find the command descriptor
 ***************************************************************************/
PRIVATE const sdata_desc_t *command_get_cmd_desc(const sdata_desc_t *command_table, const char *cmd)
{
    const sdata_desc_t *pcmd = command_table;
    while(pcmd->name) {
        /*
         *  Alias have precedence if there is no json_fn command function.
         *  It's the combination to redirect the command as `name` event,
         */
        BOOL alias_checked = FALSE;
        if(!pcmd->json_fn && pcmd->alias) {
            alias_checked = TRUE;
            const char **alias = pcmd->alias;
            while(alias && *alias) {
                if(strcasecmp(*alias, cmd)==0) {
                    return pcmd;
                }
                alias++;
            }
        }
        if(strcasecmp(pcmd->name, cmd)==0) {
            return pcmd;
        }
        if(!alias_checked) {
            const char **alias = pcmd->alias;
            while(alias && *alias) {
                if(strcasecmp(*alias, cmd)==0) {
                    return pcmd;
                }
                alias++;
            }
        }

        pcmd++;
    }
    return 0;
}

/***************************************************************************
 *  Return a new kw for command, popping the parameters inside of `command`
 *  If cmd_desc is 0 then there is a error
 *  and the return json is a json string message with the error.
 ***************************************************************************/
PRIVATE json_t *expand_command(
    hgobj gobj,
    const char *command,
    json_t *kw,     // NOT owned
    const sdata_desc_t **cmd_desc
)
{
    if(cmd_desc) {
        *cmd_desc = 0; // It's error
    }
    const sdata_desc_t *cmd_table = gobj_command_desc(gobj, NULL, FALSE);
    if(!cmd_table) {
        return json_sprintf("%s: No command table", gobj_short_name(gobj));
    }

    char *str, *p;
    str = p = gbmem_strdup(command);
    char *cmd = get_parameter(p, &p);
    if(empty_string(cmd)) {
        json_t *jn_error = json_sprintf("%s: No command", gobj_short_name(gobj));
        GBMEM_FREE(str)
        return jn_error;
    }

    const sdata_desc_t *cnf_cmd = command_get_cmd_desc(cmd_table, cmd);
    if(!cnf_cmd) {
        json_t *jn_error = json_sprintf(
            "%s: command not available: '%s'. Try 'help' command.",
            gobj_short_name(gobj),
            cmd
        );
        GBMEM_FREE(str)
        return jn_error;
    }

    if(cmd_desc) {
        *cmd_desc = cnf_cmd;
    }

    int ok = 0;
    json_t *kw_cmd = build_cmd_kw(gobj, cnf_cmd->name, cnf_cmd, p, kw, &ok);
    GBMEM_FREE(str)

    if(ok < 0) {
        if(cmd_desc) {
            *cmd_desc = 0;
        }
        return kw_cmd;
    }
    return kw_cmd;
}

/***************************************************************************
 *  Parameters of command are described as sdata_desc_t
 ***************************************************************************/
PRIVATE json_t *parameter2json(
    hgobj gobj,
    int type,
    const char *name,
    const char *s,
    int *result
)
{
    *result = 0;

    if(DTP_IS_STRING(type)) {
        if(!s) {
            s = "";
        }
        return json_string(s);

    } else if(DTP_IS_BOOLEAN(type)) {
        BOOL value;
        if(strcasecmp(s, "TRUE")==0) {
            value = 1;
        } else if(strcasecmp(s, "FALSE")==0) {
            value = 0;
        } else {
            value = atoi(s);
        }
        if(value) {
            return json_true();
        } else {
            return json_false();
        }

    } else if(DTP_IS_INTEGER(type)) {
        return json_integer(atoll(s));

    } else if(DTP_IS_REAL(type)) {
        return json_real(atof(s));

    } else if(DTP_IS_JSON(type)) {
        return anystring2json(s, strlen(s), TRUE);
    } else {
        *result = -1;
        json_t *jn_data = json_sprintf(
            "%s: type %d of parameter '%s' is unknown",
            gobj_short_name(gobj),
            (int)type,
            name
        );
        return jn_data;
    }
}

/***************************************************************************
 *  Find an input parameter
 ***************************************************************************/
PRIVATE const sdata_desc_t *find_ip_parameter(const sdata_desc_t *input_parameters, const char *key)
{
    const sdata_desc_t *ip = input_parameters;
    while(ip->name) {
        if(strcasecmp(ip->name, key)==0) {
            return ip;
        }
        /* check alias */
        const char **alias = ip->alias;
        while(alias && *alias) {
            if(strcasecmp(*alias, key)==0) {
                return ip;
            }
            alias++;
        }

        ip++;
    }
    return 0;
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE const char *sdata_command_type(uint8_t type)
{
    if(DTP_IS_STRING(type)) {
        return "string";
    } else if(DTP_IS_BOOLEAN(type)) {
        return "boolean";
    } else if(DTP_IS_INTEGER(type)) {
        return "integer";
    } else if(DTP_IS_REAL(type)) {
        return "real";
    } else if(DTP_IS_JSON(type)) {
        return "json";
    } else {
        return "unknown";
    }
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE void add_command_help(gbuffer_t *gbuf, const sdata_desc_t *pcmds, BOOL extended)
{
    if(pcmds->alias) {
        gbuffer_printf(gbuf, "- %-28s (", pcmds->name);
        const char **alias = pcmds->alias;
        if(*alias) {
            gbuffer_printf(gbuf, "%s ", *alias);
        }
        alias++;
        while(*alias) {
            gbuffer_printf(gbuf, ", %s", *alias);
            alias++;
        }
        gbuffer_printf(gbuf, ")");
    } else {
        gbuffer_printf(gbuf, "- %-28s", pcmds->name);
    }
    BOOL add_point = FALSE;
    const sdata_desc_t *pparam = pcmds->schema;
    while(pparam && pparam->name) {
        if((pparam->flag & SDF_REQUIRED) && !(pparam->flag & SDF_PERSIST)) { // TODO PERSITS? why?
            gbuffer_printf(gbuf, " <%s>", pparam->name);
        } else {
            gbuffer_printf(gbuf,
                " [%s='%s']",
                pparam->name, pparam->default_value?(char *)pparam->default_value:"?"
            );
        }
        add_point = TRUE;
        pparam++;
    }
    if(add_point) {
        gbuffer_printf(gbuf, ". %s\n", (pcmds->description)?pcmds->description:"");
    } else {
        gbuffer_printf(gbuf, " %s\n", (pcmds->description)?pcmds->description:"");
    }

    if(extended) {
        gbuffer_printf(gbuf, "\n");
        pparam = pcmds->schema;
        while(pparam && pparam->name) {
            //get_sdata_flag_desc
            gbuffer_t *gbuf_flag = bits2gbuffer(get_sdata_flag_table(), pparam->flag);
            char *p = gbuffer_cur_rd_pointer(gbuf_flag);
            gbuffer_printf(gbuf, "    - %-16s Type:%-8s, Desc:%-35s, Default:%s, Flag:%s\n",
                pparam->name,
                sdata_command_type(pparam->type),
                (pparam->description)?pparam->description:"",
                 (pparam->default_value)?pparam->default_value:"",
                p?p:""
            );
            gbuffer_decref(gbuf_flag);
            pparam++;
        }
    }
}

/***************************************************************************
 *  string parameters to json dict
 *  If error (result < 0) return a json string message
 ***************************************************************************/
PRIVATE json_t *build_cmd_kw(
    hgobj gobj,
    const char *command,
    const sdata_desc_t *cnf_cmd,
    char *parameters,   // input line
    json_t *kw, // not owned
    int *result
)
{
    const sdata_desc_t *input_parameters = cnf_cmd->schema;
    BOOL wild_command = (cnf_cmd->flag & SDF_WILD_CMD)?1:0;
    json_t *kw_cmd = json_object();
    char *pxxx = parameters;
    char bftemp[1] = {0};
    if(!pxxx) {
        pxxx = bftemp;
    }

    *result = 0;

    if(!input_parameters) {
        json_object_update_missing(kw_cmd, kw);
        return kw_cmd;
    }
    /*
     *  Check the required parameters of pure command.
     */
    /*
     *  Firstly, get required parameters
     */
    const sdata_desc_t *ip = input_parameters;
    while(ip->name) {
        if(ip->flag & SDF_NOTACCESS) {
            ip++;
            continue;
        }
        if(!(ip->flag & SDF_REQUIRED)) {
            break;
        }

        char *param = get_parameter(pxxx, &pxxx);
        if(!param) {
            /*
             *  Required: si no está en pxxx buscalo en kw
             */
            json_t *jn_param = kw_get_dict_value(0, kw, ip->name, 0, 0);
            if(jn_param) {
                json_object_set(kw_cmd, ip->name, jn_param);
                ip++;
                continue;
            } else {
                *result = -1;
                JSON_DECREF(kw_cmd);
                return json_sprintf(
                    "%s: command '%s', parameter '%s' is required",
                    gobj_short_name(gobj),
                    command,
                    ip->name
                );
            }
        }
        if(strchr(param, '=')) {
            // es ya un key=value, falta el required
            *result = -1;
            JSON_DECREF(kw_cmd);
            return json_sprintf(
                "%s: required parameter '%s' not found",
                gobj_short_name(gobj),
                ip->name
            );
        }
        json_t *jn_param = parameter2json(gobj, ip->type, ip->name, param, result);
        if(*result < 0) {
            JSON_DECREF(kw_cmd);
            return jn_param;
        }
        if(!jn_param) {
            *result = -1;
            JSON_DECREF(kw_cmd);
            return json_sprintf(
                "%s: internal error, command '%s', parameter '%s'",
                gobj_short_name(gobj),
                command,
                ip->name
            );
        }
        json_object_set_new(kw_cmd, ip->name, jn_param);

        ip++;
    }

    /*
     *  Next: get value from kw or default values
     */
    while(ip->name) {
        if(ip->flag & SDF_NOTACCESS) {
            ip++;
            continue;
        }

        if(kw_has_key(kw, ip->name)) {
            json_t *value = kw_get_dict_value(0, kw, ip->name, 0, 0);
            if(!DTP_IS_STRING(ip->type) && json_is_string(value)) {
                const char *s = json_string_value(value);
                json_t *new_value = anystring2json(s, strlen(s), TRUE);
                json_object_set_new(kw_cmd, ip->name, new_value);
            } else {
                json_object_set(kw_cmd, ip->name, value);
            }
            ip++;
            continue;
        }

        if(ip->default_value) {
            json_t *jn_param = parameter2json(
                gobj,
                ip->type,
                ip->name,
                (char *)ip->default_value,
                result
            );
            if(*result < 0) {
                JSON_DECREF(kw_cmd);
                return jn_param;
            }
            json_object_set_new(kw_cmd, ip->name, jn_param);

            ip++;
            continue;
        }

        ip++;
    }

    /*
     *  Get key=value parameters from input line
     */
    char *key;
    char *value;
    while((value=get_key_value_parameter(pxxx, &key, &pxxx))) {
        if(!key) {
            // No parameter then stop
            break;
        }
        const sdata_desc_t *ip2 = find_ip_parameter(input_parameters, key);
        json_t *jn_param = 0;
        if(ip2) {
            jn_param = parameter2json(gobj, ip2->type, ip2->name, value, result);
        } else {
            if(wild_command) {
                jn_param = parameter2json(gobj, DTP_STRING, "wild-option", value, result);
            } else {
                *result = -1;
                JSON_DECREF(kw_cmd);
                return json_sprintf(
                    "%s: '%s' command has no option '%s'",
                    gobj_short_name(gobj),
                    command,
                    key?key:"?"
                );
            }
        }
        if(*result < 0) {
            JSON_DECREF(kw_cmd);
            return jn_param;
        }
        if(!jn_param) {
            *result = -1;
            JSON_DECREF(kw_cmd);
            jn_param = json_sprintf(
                "%s: internal error, command '%s', parameter '%s', value '%s'",
                gobj_short_name(gobj),
                command,
                key,
                value
            );
            return jn_param;
        }
        json_object_set_new(kw_cmd, key, jn_param);
    }

    if(!empty_string(pxxx)) {
        *result = -1;
        JSON_DECREF(kw_cmd);
        return json_sprintf(
            "%s: command '%s' with extra parameters: '%s'",
            gobj_short_name(gobj),
            command,
            pxxx
        );
    }

    json_object_update_missing(kw_cmd, kw); // HACK lo quité y dejó de funcionar el GUI

    return kw_cmd;
}

/***************************************************************************
 *  level == 1 search in bottom_gobjs
 *  level == 2 search in all children
 ***************************************************************************/
PRIVATE const sdata_desc_t *search_command(
    hgobj gobj,
    const char *cmd,
    int level,
    hgobj *gobj_found
) {
    const sdata_desc_t *cnf_cmd = NULL;

    const sdata_desc_t *command_desc = gobj_command_desc(gobj, NULL, FALSE);
    if(command_desc) {
        cnf_cmd = command_get_cmd_desc(command_desc, cmd);
        if(cnf_cmd) {
            /*
             *  Found in gobj
             */
            if(gobj_found) {
                *gobj_found = gobj;
            }
            return cnf_cmd;
        }
    }

    /*
     *  Search in bottoms
     */
    if(level == 1) {
        hgobj bottom = gobj_bottom_gobj(gobj);
        while(bottom) {
            if(gobj_command_desc(bottom, NULL, FALSE)) {
                cnf_cmd = command_get_cmd_desc(gobj_command_desc(bottom, NULL, FALSE), cmd);
                if(cnf_cmd) {
                    /*
                     *  Found in bottom
                     */
                    if(gobj_found) {
                        *gobj_found = bottom;
                    }
                    return cnf_cmd;
                }
            }
            bottom = gobj_bottom_gobj(bottom);
        }
    }

    /*
     *  Search in children
     */
    if(level == 2) {
        hgobj child = gobj_first_child(gobj);
        while(child) {
            command_desc = gobj_command_desc(child, NULL, FALSE);
            if(command_desc) {
                cnf_cmd = command_get_cmd_desc(command_desc, cmd);
                if(cnf_cmd) {
                    /*
                     *  Found in child
                     */
                    if(gobj_found) {
                        *gobj_found = child;
                    }
                    return cnf_cmd;
                }
            }
            child = gobj_next_child(child);
        }
    }

    if(gobj_found) {
        *gobj_found = NULL;
    }
    return NULL; // Not found
}

/***************************************************************************
 *  level == 1 search in bottom_gobjs
 *  level == 2 search in all children
 ***************************************************************************/
PRIVATE int list_commands(
    hgobj gobj,
    gbuffer_t *gbuf,
    int level
) {
    /*
     *  GObj commands
     */
    const sdata_desc_t *command_desc = gobj_command_desc(gobj, NULL, FALSE);
    if(command_desc) {
        gbuffer_printf(gbuf, "\n> %s\n", gobj_short_name(gobj));
        const sdata_desc_t *pcmds = command_desc;
        while(pcmds->name) {
            if(!empty_string(pcmds->name)) {
                add_command_help(gbuf, pcmds, FALSE);
            } else {
                /*
                *  Empty command (not null) is for print a blank line or a title is desc is not empty
                */
                if(!empty_string(pcmds->description)) {
                    gbuffer_printf(gbuf, "%s\n", pcmds->description);
                } else {
                    gbuffer_printf(gbuf, "\n");
                }
            }
            pcmds++;
        }
    }

    /*
     *  Search in bottoms
     */
    if(level == 1) {
        hgobj bottom = gobj_bottom_gobj(gobj);
        while(bottom) {
            command_desc = gobj_command_desc(bottom, NULL, FALSE);
            if(command_desc) {
                gbuffer_printf(gbuf, "\n>> %s\n", gobj_short_name(bottom));
                const sdata_desc_t *pcmds = command_desc;
                while(pcmds->name) {
                    if(!empty_string(pcmds->name)) {
                        add_command_help(gbuf, pcmds, FALSE);
                    } else {
                        /*
                        *  Empty command (not null) is for print a blank line or a title is desc is not empty
                        */
                        if(!empty_string(pcmds->description)) {
                            gbuffer_printf(gbuf, "%s\n", pcmds->description);
                        } else {
                            gbuffer_printf(gbuf, "\n");
                        }
                    }
                    pcmds++;
                }
            }
            bottom = gobj_bottom_gobj(bottom);
        }
    }

    /*
     *  Search in children
     */
    if(level == 2) {
        hgobj child = gobj_first_child(gobj);
        while(child) {
            command_desc = gobj_command_desc(child, NULL, FALSE);
            if(command_desc) {
                gbuffer_printf(gbuf, "\n>> %s\n", gobj_short_name(child));
                const sdata_desc_t *pcmds = command_desc;
                while(pcmds->name) {
                    if(!empty_string(pcmds->name)) {
                        add_command_help(gbuf, pcmds, FALSE);
                    } else {
                        /*
                        *  Empty command (not null) is for print a blank line or a title is desc is not empty
                        */
                        if(!empty_string(pcmds->description)) {
                            gbuffer_printf(gbuf, "%s\n", pcmds->description);
                        } else {
                            gbuffer_printf(gbuf, "\n");
                        }
                    }
                    pcmds++;
                }
            }
            child = gobj_next_child(child);
        }
    }

    /*
     *  Child commands
     */
    if(level) {
        hgobj child = gobj_first_child(gobj);
        while(child) {
            if(gobj_command_desc(child, NULL, FALSE)) {
                gbuffer_printf(gbuf, "\n>> %s\n", gobj_short_name(child));
                const sdata_desc_t *pcmds = gobj_command_desc(child, NULL, FALSE);
                while(pcmds->name) {
                    if(!empty_string(pcmds->name)) {
                        add_command_help(gbuf, pcmds, FALSE);
                    } else {
                        /*
                        *  Empty command (not null) is for print a blank line or a title is desc is not empty
                        */
                        if(!empty_string(pcmds->description)) {
                            gbuffer_printf(gbuf, "%s\n", pcmds->description);
                        } else {
                            gbuffer_printf(gbuf, "\n");
                        }
                    }
                    pcmds++;
                }
            }
            child = gobj_next_child(child);
        }
    }

    return 0;
}

/***************************************************************************
 *  Return a string json
 *  level == 1 search in bottom_gobjs
 *  level == 2 search in all children
 ***************************************************************************/
PUBLIC json_t *gobj_build_cmds_doc(hgobj gobj, json_t *kw)
{
    int level = (int)kw_get_int(gobj, kw, "level", 0, KW_WILD_NUMBER);
    const char *cmd = kw_get_str(gobj, kw, "cmd", 0, 0);
    if(!empty_string(cmd)) {
        /*--------------------------*
         *      Find a command
         *--------------------------*/
        hgobj gobj_found = NULL;
        const sdata_desc_t *cnf_cmd = search_command(gobj, cmd, level, &gobj_found);
        if(cnf_cmd) {
            gbuffer_t *gbuf = gbuffer_create(256, 4*1024);
            gbuffer_printf(gbuf, "%s (%s)\n", cmd, gobj_short_name(gobj_found));
            int len = (int)strlen(cmd);
            while(len > 0) {
                gbuffer_printf(gbuf, "%c", '=');
                len--;
            }
            gbuffer_printf(gbuf, "\n");
            if(!empty_string(cnf_cmd->description)) {
                gbuffer_printf(gbuf, "%s\n", cnf_cmd->description);
            }
            add_command_help(gbuf, cnf_cmd, TRUE);
            gbuffer_printf(gbuf, "\n");
            json_t *jn_resp = json_string(gbuffer_cur_rd_pointer(gbuf));
            gbuffer_decref(gbuf);
            KW_DECREF(kw)
            return jn_resp;
        }

        KW_DECREF(kw)
        return json_sprintf(
            "%s: command '%s' not available.\n",
            gobj_short_name(gobj),
            cmd
        );
    }

    /*--------------------------*
     *      List commands
     *--------------------------*/
    gbuffer_t *gbuf = gbuffer_create(4*1024, 64*1024);
    gbuffer_printf(gbuf, "Available commands\n");
    gbuffer_printf(gbuf, "==================\n");

    list_commands(gobj, gbuf, level);
    json_t *jn_resp = json_string(gbuffer_cur_rd_pointer(gbuf));
    gbuffer_decref(gbuf);
    KW_DECREF(kw)
    return jn_resp;
}

/***************************************************************************
 *
 ***************************************************************************/
PUBLIC json_t *build_command_response( // // old build_webix()
    hgobj gobj,
    json_int_t result,
    json_t *jn_comment, // owned
    json_t *jn_schema,  // owned
    json_t *jn_data     // owned
) {
    if(!jn_comment) {
        jn_comment = json_string("");
    }
    if(!jn_schema) {
        jn_schema = json_null();
    }
    if(!jn_data) {
        jn_data = json_null();
    }

    json_t *response = json_object();
    json_object_set_new(response, "result", json_integer(result));
    json_object_set_new(response, "comment", jn_comment);
    json_object_set_new(response, "schema", jn_schema);
    json_object_set_new(response, "data", jn_data);

    return response;
}
