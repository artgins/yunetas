/***********************************************************************
 *          COMMAND_PARSER.C
 *
 *          Command parser
 *
 *          Copyright (c) 2017-2023 Niyamaka.
 *          Copyright (c) 2024-2026, ArtGins.
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
PRIVATE const sdata_desc_t *find_ip_parameter(const sdata_desc_t *input_parameters, const char *key);
PRIVATE BOOL is_secret_parameter(const sdata_desc_t *cnf_cmd, const char *key);
PRIVATE void append_masked_parameters(
    gbuffer_t *gbuf,
    const char *line,
    const sdata_desc_t *cnf_cmd
);

/***************************************************************
 *              Data
 ***************************************************************/

/***************************************************************************
 *
 ***************************************************************************/
PUBLIC json_t *command_parser(
    hgobj gobj,
    const char *command,
    json_t *kw,
    hgobj src
)
{
    /*
     *  Reset last_message so this command's failure path cannot
     *  leak the strerror of an unrelated earlier error. Classic
     *  symptom: a shoot-snap dup-name failure displayed the comment
     *  "Connection reset by peer" — the strerror that an earlier
     *  TCP disconnect had stamped into the static last_message
     *  buffer. Per-command reset makes last_message effectively
     *  "this command's most recent error", which is what every
     *  json_string(gobj_log_last_message()) caller actually wants.
     */
    gobj_log_set_last_message("%s", "");

    const sdata_desc_t *cnf_cmd = 0;
    json_t *kw_cmd = expand_command(gobj, command, kw, &cnf_cmd);
    if(gobj_trace_level(gobj) & (TRACE_EV_KW)) {
        /*
         *  With no cnf_cmd kw_cmd is the error string, which masks what it
         *  echoes (build_cmd_kw())
         */
        json_t *kw_shown = cnf_cmd?
            command_mask_secret_kw(gobj, command, kw_cmd) : json_incref(kw_cmd);
        gobj_trace_json(gobj, kw_shown, "expanded_command: kw_cmd");
        JSON_DECREF(kw_shown)
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
     *  Check AUTHZ (per-command authorization)
     *
     *  Gated opt-in: the SDF_AUTHZ_X role check runs only when the yuno sets
     *  `enable_command_authz` TRUE. Default (attr absent or FALSE) preserves
     *  the historical behaviour, so yunos without a C_AUTHZ role model are not
     *  broken — the global authz_checker is fail-closed when no C_AUTHZ service
     *  is running, which most yunos lack.
     *
     *  ONLY EXTERNAL commands are gated. The external entry layer (c_ievent_srv)
     *  injects the authenticated `__username__` into the command kw
     *  (kw_set_dict_value, overwrite — a wire client cannot spoof it). Internal
     *  framework calls — gobj_command() in C, e.g. the agent's mt_play
     *  `open-treedb`, the c_yuno cert-reload walk — carry NO `__username__` in
     *  kw, so they are never gated (otherwise a yuno would deny its own startup
     *  commands and exit). Self-issued commands (src == gobj) are likewise
     *  bypassed. This gate adds *authorization* (permission) on top of the
     *  authentication already enforced upstream.
     *-----------------------------------------------*/
    if(cnf_cmd->flag & SDF_AUTHZ_X) {
        hgobj yuno = gobj_yuno();
        BOOL authz_enabled = yuno &&
            gobj_has_attr(yuno, "enable_command_authz") &&
            gobj_read_bool_attr(yuno, "enable_command_authz");

        const char *cmd_username = kw_get_str(gobj, kw, "__username__", NULL, 0);
        BOOL external = !empty_string(cmd_username);

        if(authz_enabled && external && src != gobj) {
            json_t *kw_authz = json_pack("{s:s, s:s}",
                "command", command,
                "__username__", cmd_username     // authoritative external principal
            );
            if(kw) {
                json_object_set(kw_authz, "kw", kw);    // incref
            } else {
                json_object_set_new(kw_authz, "kw", json_object());
            }
            /*
             *  gobj_user_has_authz() takes ownership of kw_authz (the checker
             *  KW_DECREF's it on every path), so it is not freed here.
             */
            if(!gobj_user_has_authz(
                    gobj,
                    "__execute_command__",
                    kw_authz,
                    src
              )) {
                gobj_log_error(gobj, 0,
                    "function",     "%s", __FUNCTION__,
                    "msgset",       "%s", MSGSET_AUTH,
                    "msg",          "%s", "No permission to execute command",
                    "gclass",       "%s", gobj_gclass_name(gobj),
                    "command",      "%s", command?command:"",
                    NULL
                );
                json_t *kw_response = build_command_response(
                    gobj,
                    -403,
                    json_sprintf("No permission to execute command: '%s'", command?command:""),
                    0,
                    0
                );
                KW_DECREF(kw_cmd)
                KW_DECREF(kw)
                return kw_response;
            }
        }
    }

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
PUBLIC const sdata_desc_t *command_get_cmd_desc(const sdata_desc_t *command_table, const char *command)
{
    char *str, *p;
    str = p = gbmem_strdup(command);
    char *cmd = get_parameter(p, &p);
    if(empty_string(cmd)) {
        GBMEM_FREE(str)
        return NULL;
    }


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
                    GBMEM_FREE(str)
                    return pcmd;
                }
                alias++;
            }
        }
        if(strcasecmp(pcmd->name, cmd)==0) {
            GBMEM_FREE(str)
            return pcmd;
        }
        if(!alias_checked) {
            const char **alias = pcmd->alias;
            while(alias && *alias) {
                if(strcasecmp(*alias, cmd)==0) {
                    GBMEM_FREE(str)
                    return pcmd;
                }
                alias++;
            }
        }

        pcmd++;
    }

    GBMEM_FREE(str)
    return NULL;
}

/***************************************************************************
 *  The descriptor of `command` ("name [parameters]") in its gobj's command
 *  table, NULL if none
 ***************************************************************************/
PRIVATE const sdata_desc_t *command_cnf(hgobj gobj, const char *command)
{
    if(!gobj || empty_string(command)) {
        return NULL;    // not known here: masked by names only
    }
    const sdata_desc_t *cmd_table = gobj_command_desc(gobj, NULL, FALSE);
    if(!cmd_table) {
        return NULL;
    }
    return command_get_cmd_desc(cmd_table, command);
}

/***************************************************************************
 *  Is `key` a parameter a trace must not show? A SDF_SECRET one of the
 *  command, or any key whose name is a secret's (is_secret_name()): the
 *  free keys of a SDF_WILD_CMD command are forwarded to a table that is
 *  not known here (command-yuno password=...), and a key the table does
 *  not have is still what somebody typed.
 ***************************************************************************/
PRIVATE BOOL is_secret_parameter(const sdata_desc_t *cnf_cmd, const char *key)
{
    if(!key) {
        return FALSE;
    }
    const sdata_desc_t *ip = (cnf_cmd && cnf_cmd->schema)?
        find_ip_parameter(cnf_cmd->schema, key) : NULL;
    if(ip && (ip->flag & SDF_SECRET)) {
        return TRUE;
    }
    return is_secret_name(key, strlen(key));
}

/***************************************************************************
 *  A secret is shown masked unless it is absent or an empty string: "not
 *  set" still shows. A number or a boolean is masked too.
 ***************************************************************************/
PRIVATE BOOL secret_is_set(json_t *value)
{
    if(!value || json_is_null(value)) {
        return FALSE;
    }
    if(json_is_string(value) && json_string_length(value) == 0) {
        return FALSE;
    }
    return TRUE;
}

/***************************************************************************
 *  Append to gbuf the key=value parameters of `line`, as a trace shows
 *  them: " key=value", the value of a secret key as "********". In a
 *  SDF_WILD_CMD command a value with a '=' is a command line going on (the
 *  `command` of command-yuno): it is shown key='...', masked by names
 *  (mask_secrets_inline()).
 *  What cannot be parsed as key=value is not shown: " <...>".
 ***************************************************************************/
PRIVATE void append_masked_parameters(
    gbuffer_t *gbuf,
    const char *line,
    const sdata_desc_t *cnf_cmd     // NULL: by names only
)
{
    if(empty_string(line)) {
        return;
    }
    BOOL wild = (cnf_cmd && (cnf_cmd->flag & SDF_WILD_CMD))? TRUE : FALSE;
    char *str = gbmem_strdup(line);
    if(!str) {
        // Error already logged
        gbuffer_append_string(gbuf, " <...>");
        return;
    }
    char *p = str;
    char *rest = p;
    char *key;
    char *value;
    while(p) {
        rest = p;
        value = get_key_value_parameter(p, &key, &p);
        if(!value || !key) {
            break;
        }
        rest = p;
        if(is_secret_parameter(cnf_cmd, key)) {
            gbuffer_printf(gbuf, " %s=%s", key, empty_string(value)? "" : "********");
        } else if(wild && strchr(value, '=')) {
            char *shown = mask_secrets_inline(value);
            gbuffer_printf(gbuf, " %s='%s'", key, shown? shown : value);
            GBMEM_FREE(shown)
        } else {
            gbuffer_printf(gbuf, " %s=%s", key, value);
        }
    }
    while(rest && (*rest == ' ' || *rest == '\t')) {
        rest++;
    }
    if(rest && *rest) {
        gbuffer_append_string(gbuf, " <...>");
    }
    GBMEM_FREE(str)
}

/***************************************************************************
 *  A json value as a trace shows it, for a command of `cnf_cmd`: its
 *  SDF_SECRET parameters masked, and then what json_mask_secrets() masks
 *  (keys with a secret's name at any depth, the "value" of a write-attr of
 *  a secret, "name=value" secrets inside a string: the `command` of a
 *  command-yuno). Return a new reference.
 ***************************************************************************/
PRIVATE json_t *mask_secret_json(json_t *jn, const sdata_desc_t *cnf_cmd)
{
    json_t *jn_masked = NULL;
    if(json_is_object(jn) && cnf_cmd && cnf_cmd->schema) {
        const char *key;
        json_t *value;
        json_object_foreach(jn, key, value) {
            const sdata_desc_t *ip = find_ip_parameter(cnf_cmd->schema, key);
            if(ip && (ip->flag & SDF_SECRET) && secret_is_set(value)) {
                if(!jn_masked) {
                    jn_masked = json_copy(jn);
                }
                json_object_set_new(jn_masked, key, json_string("********"));
            }
        }
    }
    if(!jn_masked) {
        return json_mask_secrets(jn);
    }
    json_t *jn_shown = json_mask_secrets(jn_masked);
    JSON_DECREF(jn_masked)
    return jn_shown;
}

/***************************************************************************
 *  The kw of a command as a trace shows it: the SDF_SECRET parameters of
 *  the command and the keys with a secret's name masked (the number or
 *  boolean of one too: "password": 1234)
 ***************************************************************************/
PUBLIC json_t *command_mask_secret_kw(
    hgobj gobj,
    const char *command,
    json_t *kw  // not owned
)
{
    if(!kw) {
        return NULL;
    }
    if(!json_is_object(kw)) {
        return json_incref(kw);
    }
    return mask_secret_json(kw, command_cnf(gobj, command));
}

/***************************************************************************
 *  The command line as a trace shows it: the value of every secret
 *  parameter masked, positional (the leading required ones) or key=value.
 *  What cannot be parsed as a parameter is not shown.
 ***************************************************************************/
PUBLIC char *command_mask_secret_line(
    hgobj gobj,
    const char *command
)
{
    if(!command) {
        return gbmem_strdup("");
    }
    gbuffer_t *gbuf = gbuffer_create(256, 64*1024);
    if(!gbuf) {
        // Error already logged
        return gbmem_strdup("");
    }
    const sdata_desc_t *cnf_cmd = command_cnf(gobj, command);

    char *str, *p;
    str = p = gbmem_strdup(command);
    if(!str) {
        // Error already logged
        gbuffer_decref(gbuf);
        return gbmem_strdup("");
    }
    char *cmd = get_parameter(p, &p);
    gbuffer_append_string(gbuf, cmd?cmd:"");

    /*
     *  The leading required parameters can go without key, as
     *  build_cmd_kw() takes them. get_parameter() cuts the token with a
     *  NUL: it is put back, so a token that is a key=value is parsed as
     *  one from its start, with the rest of the line after it.
     */
    const sdata_desc_t *ip = cnf_cmd? cnf_cmd->schema : NULL;
    while(ip && ip->name && p) {
        if(ip->flag & SDF_NOTACCESS) {
            ip++;
            continue;
        }
        if(!(ip->flag & SDF_REQUIRED)) {
            break;
        }
        char *save = p;
        while(*save == ' ' || *save == '\t') {
            save++;
        }
        BOOL quoted = (*save == '\'' || *save == '"')? TRUE : FALSE;
        char *param = get_parameter(p, &p);
        if(!param) {
            break;
        }
        BOOL is_key_value = FALSE;
        char *eq = quoted? NULL : strchr(param, '=');
        if(eq) {
            /*
             *  "key=value", or a value with a '=' in it (abc==): a key of
             *  the command, or one with a secret's name, is a key=value
             */
            *eq = 0;
            is_key_value = (find_ip_parameter(cnf_cmd->schema, param) ||
                is_secret_name(param, strlen(param)))? TRUE : FALSE;
            *eq = '=';
        }
        if(!is_key_value) {
            gbuffer_printf(gbuf, " %s",
                ((ip->flag & SDF_SECRET) || is_secret_name(ip->name, strlen(ip->name)))?
                    "********" : param
            );
        }
        if(p) {
            *(p-1) = command[(p-1) - str];  // put back what get_parameter() cut
        }
        if(is_key_value) {
            p = save;   // from here on, all are key=value
            break;
        }
        ip++;
    }

    append_masked_parameters(gbuf, p, cnf_cmd);
    GBMEM_FREE(str)

    /*
     *  And by names, over the whole line: the "value" of a write-attr of a
     *  secret attribute ("attribute=password value=...")
     */
    char *line = mask_secrets_inline(gbuffer_cur_rd_pointer(gbuf));
    if(!line) {
        line = gbmem_strdup(gbuffer_cur_rd_pointer(gbuf));
    }
    gbuffer_decref(gbuf);
    return line;
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
        GBMEM_FREE(str)
        return kw_cmd;
    }
    GBMEM_FREE(str)
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
    } else if(DTP_IS_LIST(type) || DTP_IS_DICT(type)) {
        return string2json(s, TRUE);
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
        kw_update_missing(gobj, kw_cmd, kw);
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
                json_t *new_value = anystring2json(s, strlen(s), FALSE);
                if(new_value) {
                    json_object_set_new(kw_cmd, ip->name, new_value);
                    ip++;
                    continue;
                }
            } else if(value) {
                json_object_set(kw_cmd, ip->name, value);
                ip++;
                continue;
            }
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
    const char *last_key = NULL;    // the parameter before an extra word
    BOOL last_value_empty = FALSE;
    while(1) {
        key = NULL;
        value = get_key_value_parameter(pxxx, &key, &pxxx);
        if(!value && key) {
            /*
             *  key='value with no closing quote: get_key_value_parameter()
             *  answers no value and no rest. Up to 7.25.20 the parameter
             *  was dropped and the command ran without it, with no log.
             */
            *result = -1;
            JSON_DECREF(kw_cmd);
            return json_sprintf(
                "%s: command '%s', parameter '%s': value with no closing quote",
                gobj_short_name(gobj),
                command,
                key
            );
        }
        if(!value) {
            break;
        }
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
                is_secret_parameter(cnf_cmd, key)? "********" : value
            );
            return jn_param;
        }
        json_object_set_new(kw_cmd, key, jn_param);
        last_key = key;
        last_value_empty = empty_string(value);
    }

    if(!empty_string(pxxx)) {
        /*
         *  The extra text is echoed with its secrets masked: a secret
         *  name=value in it, and all of it when it is the value of a secret
         *  parameter given with a blank after its '=' (password= hunter2)
         */
        *result = -1;
        JSON_DECREF(kw_cmd);
        BOOL extra_is_secret = last_key && last_value_empty &&
            is_secret_parameter(cnf_cmd, last_key);
        char *masked = extra_is_secret? NULL : mask_secrets_inline(pxxx);
        json_t *jn_error = json_sprintf(
            "%s: command '%s' with extra parameters: '%s'",
            gobj_short_name(gobj),
            command,
            extra_is_secret? "<...>" : (masked? masked : pxxx)
        );
        GBMEM_FREE(masked)
        return jn_error;
    }

    /*
     *  The keys of kw that are not parameters reach the handler too;
     *  callers (the GUI) depend on it.
     */
    kw_update_missing(gobj, kw_cmd, kw);

    return kw_cmd;
}

/***************************************************************************
 *  Search a command in gobj, if not found then
 *      level == 1 search in bottom_gobjs
 *      level == 2 search in all children
 ***************************************************************************/
PUBLIC const sdata_desc_t *search_command_desc(
    hgobj gobj,
    const char *command,
    int level,
    hgobj *gobj_found
) {
    const sdata_desc_t *cnf_cmd = NULL;

    char *str, *p;
    str = p = gbmem_strdup(command);
    char *cmd = get_parameter(p, &p);
    if(empty_string(cmd)) {
        GBMEM_FREE(str)
        return NULL; // Not found
    }

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
            GBMEM_FREE(str)
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
                    GBMEM_FREE(str)
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
                    GBMEM_FREE(str)
                    return cnf_cmd;
                }
            }
            child = gobj_next_child(child);
        }
    }

    if(gobj_found) {
        *gobj_found = NULL;
    }
    GBMEM_FREE(str)
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
        const sdata_desc_t *cnf_cmd = search_command_desc(gobj, cmd, level, &gobj_found);
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
    /*
     *  If a failure response carries no comment (caller used
     *  json_string(gobj_log_last_message()) but no LOG_ERR ran
     *  during this command, so last_message is now empty thanks
     *  to the per-command reset in command_parser), substitute
     *  a generic hint so the user at least sees something more
     *  informative than "ERROR -1: ". Success responses keep
     *  their empty comment — cmd_topics and similar return data
     *  with no comment by design.
     */
    if(result != 0 &&
       json_is_string(jn_comment) &&
       empty_string(json_string_value(jn_comment))) {
        JSON_DECREF(jn_comment)
        jn_comment = json_string("(see log)");
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
