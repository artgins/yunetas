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
#include <stdlib.h>
#include <errno.h>
#include <float.h>
#ifdef __linux__
#include <locale.h>
#endif
#include "gobj.h"
#include "command_parser.h"

/***************************************************************
 *              Constants
 ***************************************************************/
/*
 *  A longer parameter value is shown as "<N bytes>" by the masked line of a
 *  trace: the content64 of an install-binary is tens of MB of base64
 */
#define MAX_SHOWN_VALUE 1024

/***************************************************************
 *              Structures
 ***************************************************************/

/***************************************************************
 *              Prototypes
 ***************************************************************/
PRIVATE const sdata_desc_t *find_command_desc(
    hgobj gobj,
    const char *command,
    json_t **jn_error
);
PRIVATE json_t *refuse_command_without_authz(
    hgobj gobj,
    const sdata_desc_t *cnf_cmd,
    json_t *kw,     // NOT owned
    hgobj src
);
PRIVATE json_t *expand_command(
    hgobj gobj,
    const char *command,
    const sdata_desc_t *cnf_cmd,
    json_t *kw,     // NOT owned
    int *result
);
PRIVATE json_t *build_cmd_kw(
    hgobj gobj,
    const char *command,
    const sdata_desc_t *cnf_cmd,
    char *parameters,
    const char *line,
    json_t *kw, // not owned
    int *result
);
PRIVATE const sdata_desc_t *find_ip_parameter(const sdata_desc_t *input_parameters, const char *key);
PRIVATE BOOL is_secret_parameter(const sdata_desc_t *cnf_cmd, const char *key);
PRIVATE json_t *mask_secret_params(json_t *jn, const sdata_desc_t *cnf_cmd);
PRIVATE json_t *text2parameter(data_type_t type, const char *s, BOOL from_kw, const char **refused);
PRIVATE json_t *kw2parameter(data_type_t type, json_t *value, BOOL as_given, const char **refused);
PRIVATE json_t *refuse_parameter(
    hgobj gobj,
    BOOL peer,
    const char *command,
    const char *name,
    data_type_t type,
    const char *got
);
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

    json_t *jn_error = 0;
    const sdata_desc_t *cnf_cmd = find_command_desc(gobj, command, &jn_error);
    if(!cnf_cmd) {
        if(gobj_trace_level(gobj) & (TRACE_EV_KW)) {
            gobj_trace_json(gobj, jn_error, "expanded_command: kw_cmd");
        }
        json_t *kw_response = build_command_response(
            gobj,
            -1,
            jn_error,
            0,
            0
        );
        KW_DECREF(kw)
        return kw_response;
    }

    /*
     *  The permission is asked BEFORE the parameters are read: a caller
     *  without it gets the 403, never the diagnostic of a parameter
     */
    json_t *kw_denied = refuse_command_without_authz(gobj, cnf_cmd, kw, src);
    if(kw_denied) {
        KW_DECREF(kw)
        return kw_denied;
    }

    int result = 0;
    json_t *kw_cmd = expand_command(gobj, command, cnf_cmd, kw, &result);
    if(gobj_trace_level(gobj) & (TRACE_EV_KW)) {
        /*
         *  On a failure kw_cmd is the error string, which masks what it
         *  echoes (build_cmd_kw())
         */
        json_t *kw_shown = (result >= 0)?
            command_mask_secret_kw(gobj, command, kw_cmd) : json_incref(kw_cmd);
        gobj_trace_json(gobj, kw_shown, "expanded_command: kw_cmd");
        JSON_DECREF(kw_shown)
    }
    if(result < 0) {
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
 *
 *  The checker is asked for the command's NAME (`command` of
 *  __execute_command__ is "Command name"), and the refusal logs and answers
 *  the name: it was given the whole line, with its parameters -- a password
 *  among them -- and logged it.
 *
 *  NULL when the command can run, else the answer (YOURS).
 ***************************************************************************/
PRIVATE json_t *refuse_command_without_authz(
    hgobj gobj,
    const sdata_desc_t *cnf_cmd,
    json_t *kw,     // NOT owned
    hgobj src
)
{
    if(!(cnf_cmd->flag & SDF_AUTHZ_X)) {
        return NULL;
    }
    hgobj yuno = gobj_yuno();
    BOOL authz_enabled = yuno &&
        gobj_has_attr(yuno, "enable_command_authz") &&
        gobj_read_bool_attr(yuno, "enable_command_authz");

    const char *cmd_username = json_string_value(json_object_get(kw, "__username__"));
    BOOL external = !empty_string(cmd_username);

    if(!(authz_enabled && external && src != gobj)) {
        return NULL;
    }

    json_t *kw_authz = json_pack("{s:s, s:s}",
        "command", cnf_cmd->name,
        "__username__", cmd_username     // authoritative external principal
    );
    /*
     *  The kw with its SDF_SECRET parameters masked: a permission is no
     *  question of a secret's value, and the authz trace, which prints what
     *  the checker got, masks by names only
     */
    json_object_set_new(kw_authz, "kw", kw? mask_secret_params(kw, cnf_cmd) : json_object());
    /*
     *  gobj_user_has_authz() takes ownership of kw_authz (the checker
     *  KW_DECREF's it on every path), so it is not freed here.
     */
    if(gobj_user_has_authz(
            gobj,
            "__execute_command__",
            kw_authz,
            src
      )) {
        return NULL;
    }

    gobj_log_error(gobj, 0,
        "function",     "%s", __FUNCTION__,
        "msgset",       "%s", MSGSET_AUTH,
        "msg",          "%s", "No permission to execute command",
        "gclass",       "%s", gobj_gclass_name(gobj),
        "command",      "%s", cnf_cmd->name,
        NULL
    );
    return build_command_response(
        gobj,
        -403,
        json_sprintf("%s: No permission to execute command: '%s'",
            gobj_short_name(gobj), cnf_cmd->name
        ),
        0,
        0
    );
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
 *  What a parameter takes, whatever gives it: the command line (text), kw
 *  (json) or its declared default (text):
 *
 *      DTP_STRING      a string
 *      DTP_DICT        a json object (text: the json of one)
 *      DTP_LIST        a json array (text: the json of one)
 *      DTP_INTEGER,    a number or a boolean, never an object or an array.
 *      DTP_REAL,       Text is read as one: a number of the JSON grammar
 *      DTP_BOOLEAN     with nothing before or after it, in its range, and a
 *                      boolean as true/false, yes/no, on/off or an integer
 *                      ("yes" was atoi()'d to FALSE, so a `dry_run=yes`
 *                      saved for real). The text of an integer or a boolean
 *                      is a JSON integer, -?(0|[1-9][0-9]*): "-0", "42";
 *                      not "2.0" (the json 2.0 is 2), "+5", "007", " 5".
 *                      That of a real is a JSON number,
 *                      -?(0|[1-9][0-9]*)(\.[0-9]+)?([eE][+-]?[0-9]+)?:
 *                      "2.5", "-0.5", "1E5", "1e-400" (an underflow is a
 *                      number, 0); not ".5", "5.", "+5", "007", "0x10",
 *                      "nan", "inf", nor an overflow ("1e999"). Read in the
 *                      "C" locale, whatever the LC_NUMERIC of the process.
 *      DTP_JSON        anything; text that is not json is a string, but
 *                      not one that starts as json ({ [ ") and breaks --
 *                      on the line or in a default: in kw a string is a
 *                      json too, and stays as it came
 *
 *  A string from kw with a NUL inside is refused (but for DTP_JSON, where it
 *  stays as it came): C reads it up to the NUL.
 *
 *  The handler gets the EXACT type (number_as_type()): a
 *  number given to a DTP_BOOLEAN is true unless 0; a boolean given to a
 *  DTP_INTEGER or a DTP_REAL is 1 or 0; a real given to a DTP_INTEGER is
 *  that integer, and a real with decimals is refused, as the text "2.7" is.
 *  Handed on as it came, a `1` reached a kw_get_bool() (with no
 *  KW_WILD_NUMBER) that logged an ERROR with a stack and took its default.
 *
 *  A null is no value (but for DTP_JSON): the parameter takes its default,
 *  or is missing. Up to 7.26.9 a value of another shape went to the handler
 *  as it came: its kw_get_*() logged an ERROR with a stack for the caller's
 *  mistake, and a wrong type could widen what the command does (C_TREEDB's
 *  save-schema read `"treedb_name": 5` as no treedb named, and saved every
 *  one).
 ***************************************************************************/
PRIVATE BOOL value_has_shape(data_type_t type, json_t *value)
{
    if(DTP_IS_STRING(type)) {
        return json_is_string(value);
    }
    if(DTP_IS_DICT(type)) {
        return json_is_object(value);
    }
    if(DTP_IS_LIST(type)) {
        return json_is_array(value);
    }
    if(DTP_IS_NUMBER(type)) {
        return json_is_number(value) || json_is_boolean(value);
    }
    return TRUE;
}

/***************************************************************************
 *  What a parameter of `type` must be, said to whoever gave another thing
 ***************************************************************************/
PRIVATE const char *shape_of_type(data_type_t type)
{
    if(DTP_IS_STRING(type)) {
        return "a string";
    }
    if(DTP_IS_DICT(type)) {
        return "a json object";
    }
    if(DTP_IS_LIST(type)) {
        return "a json array";
    }
    if(DTP_IS_BOOLEAN(type)) {
        return "a boolean (true/false, yes/no, on/off, or an integer)";
    }
    if(DTP_IS_INTEGER(type)) {
        return "an integer";
    }
    if(DTP_IS_REAL(type)) {
        return "a number";
    }
    if(DTP_IS_JSON(type)) {
        return "a valid json";
    }
    return "a value of its type";
}

/***************************************************************************
 *  What was given instead (never the value: it may be a secret)
 ***************************************************************************/
PRIVATE const char *shape_of_value(json_t *value)
{
    switch(json_typeof(value)) { // json_typeof PROTECTED
        case JSON_OBJECT:
            return "an object";
        case JSON_ARRAY:
            return "an array";
        case JSON_STRING:
            return "a text that does not read as one";
        case JSON_INTEGER:
            return "an integer";
        case JSON_REAL:
            return "a real";
        case JSON_TRUE:
        case JSON_FALSE:
            return "a boolean";
        default:
            return "null";
    }
}

/***************************************************************************
 *  A json number or boolean (value_has_shape()) as the EXACT type of a
 *  DTP_INTEGER, DTP_REAL or DTP_BOOLEAN parameter. Return a new reference;
 *  NULL with *refused saying why: a DTP_INTEGER takes no real with
 *  decimals, nor one out of its range.
 ***************************************************************************/
PRIVATE json_t *number_as_type(data_type_t type, json_t *value, const char **refused)
{
    *refused = NULL;
    if(DTP_IS_BOOLEAN(type)) {
        if(json_is_boolean(value)) {
            return json_incref(value);
        }
        if(json_is_integer(value)) {
            return json_boolean(json_integer_value(value) != 0);
        }
        return json_boolean(json_real_value(value) != 0.0);
    }
    if(DTP_IS_REAL(type)) {
        if(json_is_real(value)) {
            return json_incref(value);
        }
        if(json_is_integer(value)) {
            return json_real((double)json_integer_value(value));
        }
        return json_real(json_is_true(value)? 1.0 : 0.0);
    }

    if(json_is_integer(value)) {
        return json_incref(value);
    }
    if(json_is_boolean(value)) {
        return json_integer(json_is_true(value)? 1 : 0);
    }
    double d = json_real_value(value);
    if(!(d >= -9223372036854775808.0 && d < 9223372036854775808.0)) {
        *refused = "a number out of its range";    // the cast below would be undefined
        return NULL;
    }
    json_int_t n = (json_int_t)d;
    if((double)n != d) {
        *refused = "a real with decimals";
        return NULL;
    }
    return json_integer(n);
}

/***************************************************************************
 *  Is `s`, whole, a number of the JSON grammar? An integer when `integer`:
 *      -?(0|[1-9][0-9]*)
 *  else a JSON number:
 *      -?(0|[1-9][0-9]*)(\.[0-9]+)?([eE][+-]?[0-9]+)?
 *  strtoll() and strtod() took ".5", "5.", "+5" and "007" (as 7), blanks
 *  before the number, and strtod() hex, "nan" and "inf".
 ***************************************************************************/
PRIVATE BOOL is_ascii_digit(char c)
{
    return (c >= '0' && c <= '9')? TRUE : FALSE;
}

PRIVATE BOOL is_json_number_text(const char *s, BOOL integer)
{
    const char *p = s;
    if(*p == '-') {
        p++;
    }
    if(*p == '0') {
        p++;
    } else if(*p >= '1' && *p <= '9') {
        while(is_ascii_digit(*p)) {
            p++;
        }
    } else {
        return FALSE;
    }
    if(!integer && *p == '.') {
        p++;
        if(!is_ascii_digit(*p)) {
            return FALSE;
        }
        while(is_ascii_digit(*p)) {
            p++;
        }
    }
    if(!integer && (*p == 'e' || *p == 'E')) {
        p++;
        if(*p == '+' || *p == '-') {
            p++;
        }
        if(!is_ascii_digit(*p)) {
            return FALSE;
        }
        while(is_ascii_digit(*p)) {
            p++;
        }
    }
    return (*p == 0)? TRUE : FALSE;
}

/***************************************************************************
 *  The double of a text that is a JSON number (is_json_number_text()),
 *  read in the "C" locale whatever the LC_NUMERIC of the process is: in
 *  es_ES strtod() reads "2.5" as 2 and stops at the '.'. newlocale() of
 *  "C" is the C library's own object, no file is read (it works in a
 *  FULLY_STATIC binary), and it is freed at once. NULL, or *refused saying
 *  why: an overflow ("1e999"). An underflow is a number (1e-400 is 0).
 ***************************************************************************/
PRIVATE json_t *json_number_text_to_real(const char *s, const char **refused)
{
    double d;
#ifdef __linux__
    locale_t c_numeric = newlocale(LC_NUMERIC_MASK, "C", (locale_t)0);
    if(!c_numeric) {
        gobj_log_error(0, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_SYSTEM,
            "msg",          "%s", "newlocale() of \"C\" FAILED: a number text cannot be read",
            "errno",        "%s", strerror(errno),
            NULL
        );
        *refused = "a number that could not be read (see the log)";
        return NULL;
    }
    d = strtod_l(s, NULL, c_numeric);
    freelocale(c_numeric);
#else
    d = strtod(s, NULL);    // the ESP32 port never sets a locale: "C"
#endif
    if(d > DBL_MAX || d < -DBL_MAX) {   // an overflow is +-HUGE_VAL
        *refused = "a number out of its range";
        return NULL;
    }
    return json_real(d);
}

/***************************************************************************
 *  The value of a parameter written as text. Return a new reference; NULL
 *  with *refused NULL is no value (an empty number from kw: a form field
 *  left empty), NULL with *refused set a text that does not read as the
 *  type, *refused says why. An empty number on the command line is 0, as it
 *  always was.
 ***************************************************************************/
PRIVATE json_t *text2parameter(data_type_t type, const char *s, BOOL from_kw, const char **refused)
{
    *refused = NULL;
    if(!s) {
        s = "";
    }

    if(DTP_IS_STRING(type)) {
        return json_string(s);
    }

    if(DTP_IS_NUMBER(type)) {
        if(empty_string(s)) {
            if(from_kw) {
                return NULL;
            }
            if(DTP_IS_BOOLEAN(type)) {
                return json_false();
            }
            return DTP_IS_REAL(type)? json_real(0) : json_integer(0);
        }
        if(DTP_IS_BOOLEAN(type)) {
            if(strcasecmp(s, "true")==0 || strcasecmp(s, "yes")==0 || strcasecmp(s, "on")==0) {
                return json_true();
            }
            if(strcasecmp(s, "false")==0 || strcasecmp(s, "no")==0 || strcasecmp(s, "off")==0) {
                return json_false();
            }
        }
        /*
         *  A number of the JSON grammar, the whole text: the text of a real
         *  a JSON number, that of an integer or a boolean a JSON integer
         */
        if(!is_json_number_text(s, DTP_IS_REAL(type)? FALSE : TRUE)) {
            *refused = "a text that does not read as one";
            return NULL;
        }
        if(DTP_IS_REAL(type)) {
            return json_number_text_to_real(s, refused);
        }
        errno = 0;
        long long n = strtoll(s, NULL, 10);     // digits only: no locale reads them otherwise
        if(errno == ERANGE) {
            *refused = "a number out of its range";   // strtoll() saturates in silence
            return NULL;
        }
        if(DTP_IS_BOOLEAN(type)) {
            return json_boolean(n != 0);
        }
        return json_integer(n);
    }

    if(DTP_IS_DICT(type) || DTP_IS_LIST(type)) {
        json_t *jn = string2json(s, FALSE);   // never logged: the text may be a secret
        if(!jn || !value_has_shape(type, jn)) {
            JSON_DECREF(jn)
            *refused = "a text that does not read as one";
            return NULL;
        }
        return jn;
    }

    if(DTP_IS_JSON(type)) {
        json_t *jn = anystring2json(s, strlen(s), FALSE);  // never logged: it may be a secret
        if(!jn) {
            const char *p = s;
            while(*p == ' ' || *p == '\t') {
                p++;
            }
            if(*p == '{' || *p == '[' || *p == '"') {
                *refused = "a text that does not read as one";    // json written wrong, not a text
                return NULL;
            }
            jn = json_string(s);    // a bare word, a date: text
        }
        return jn;
    }

    *refused = "a text that does not read as one";    // a type a command parameter cannot have
    return NULL;
}

/***************************************************************************
 *  A value of `type` written as text (see command_parser.h)
 ***************************************************************************/
PUBLIC json_t *command_parameter_from_text(
    data_type_t type,
    const char *text,
    const char **refused
)
{
    const char *why = NULL;
    json_t *jn = text2parameter(type, text, FALSE, &why);
    if(!jn && !why) {
        why = "a text that does not read as one";   // no value: out of memory
    }
    if(refused) {
        *refused = why;
    }
    return jn;
}

/***************************************************************************
 *  A value of `type` given as json (see command_parser.h)
 ***************************************************************************/
PUBLIC json_t *command_parameter_from_json(
    data_type_t type,
    json_t *value,  // not owned
    const char **refused
)
{
    const char *why = NULL;
    json_t *jn = kw2parameter(type, value, FALSE, &why);
    if(refused) {
        *refused = why;
    }
    return jn;
}

/***************************************************************************
 *  The value of a parameter given in kw. Return a new reference; NULL with
 *  *refused NULL is no value; NULL with *refused set is a value of another
 *  shape, *refused says which. A text for a number, a boolean, an object or
 *  a list is read as text2parameter() reads the command line, a number as
 *  the exact type (number_as_type()); `as_given` keeps a DTP_JSON as it
 *  came (a required one always did).
 ***************************************************************************/
PRIVATE json_t *kw2parameter(data_type_t type, json_t *value, BOOL as_given, const char **refused)
{
    *refused = NULL;
    if(!value) {
        return NULL;
    }
    /*
     *  A NUL inside a text (an ievent frame is decoded with JSON_ALLOW_NUL):
     *  C reads it up to the NUL, so "5\u0000junk" was 5, "yes\u0000no" true,
     *  and a string reached the handler other than it was sent
     */
    BOOL nul_inside = (json_is_string(value) &&
        json_string_length(value) != strlen(json_string_value(value)))? TRUE : FALSE;
    if(DTP_IS_JSON(type)) {
        if(as_given || !json_is_string(value) || nul_inside) {
            return json_incref(value);
        }
        const char *bad = NULL;
        json_t *jn = text2parameter(type, json_string_value(value), TRUE, &bad);
        if(!jn) {
            jn = json_incref(value);    // a string is a json too
        }
        return jn;
    }
    if(!(DTP_IS_STRING(type) || DTP_IS_DICT(type) || DTP_IS_LIST(type) || DTP_IS_NUMBER(type))) {
        return json_incref(value);
    }
    if(json_is_null(value)) {
        return NULL;
    }
    if(nul_inside) {
        *refused = "a text with a NUL in it";
        return NULL;
    }
    if(json_is_string(value) && !DTP_IS_STRING(type)) {
        return text2parameter(type, json_string_value(value), TRUE, refused);
    }
    if(!value_has_shape(type, value)) {
        *refused = shape_of_value(value);
        return NULL;
    }
    if(DTP_IS_NUMBER(type)) {
        return number_as_type(type, value, refused);
    }
    return json_incref(value);
}

/***************************************************************************
 *  The refusal, and the answer that names the parameter. A command with a
 *  user in kw came from outside (c_ievent_srv puts `__username__` there):
 *  the mistake is the peer's, a WARNING. From C code it is a broken
 *  contract, an ERROR with a stack.
 ***************************************************************************/
PRIVATE json_t *refuse_parameter(
    hgobj gobj,
    BOOL peer,
    const char *command,
    const char *name,
    data_type_t type,
    const char *got
)
{
    if(peer) {
        gobj_log_warning(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_PARAMETER,
            "msg",          "%s", "Command parameter of the wrong type",
            "command",      "%s", command,
            "parameter",    "%s", name,
            "expected",     "%s", shape_of_type(type),
            "got",          "%s", got,
            NULL
        );
    } else {
        gobj_log_error(gobj, LOG_OPT_TRACE_STACK,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_PARAMETER,
            "msg",          "%s", "Command parameter of the wrong type",
            "command",      "%s", command,
            "parameter",    "%s", name,
            "expected",     "%s", shape_of_type(type),
            "got",          "%s", got,
            NULL
        );
    }
    return json_sprintf(
        "%s: command '%s', parameter '%s' must be %s, not %s",
        gobj_short_name(gobj),
        command,
        name,
        shape_of_type(type),
        got
    );
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
 *  them: " key=value", the value of a secret key as "********", a value
 *  longer than MAX_SHOWN_VALUE as "<N bytes>". In a
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
        if(strpbrk(key, " \t")) {
            /*
             *  "password= hunter2 x=1": the word after a "key=" with no
             *  value is read as part of the next key -- maybe the secret
             */
            gbuffer_append_string(gbuf, " <...>");
        } else if(is_secret_parameter(cnf_cmd, key)) {
            gbuffer_printf(gbuf, " %s=%s", key, empty_string(value)? "" : "********");
        } else if(strlen(value) > MAX_SHOWN_VALUE) {
            gbuffer_printf(gbuf, " %s=<%zu bytes>", key, strlen(value));
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
 *  A json value with the SDF_SECRET parameters of `cnf_cmd` masked. Return
 *  a new reference: a masked copy, or jn itself when nothing is secret.
 ***************************************************************************/
PRIVATE json_t *mask_secret_params(json_t *jn, const sdata_desc_t *cnf_cmd)
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
    return jn_masked? jn_masked : json_incref(jn);
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
    json_t *jn_params = mask_secret_params(jn, cnf_cmd);
    json_t *jn_shown = json_mask_secrets(jn_params);
    JSON_DECREF(jn_params)
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
 *  What cannot be parsed as a parameter is not shown; a value longer than
 *  MAX_SHOWN_VALUE (1 KB) is shown as "<N bytes>".
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
            if((ip->flag & SDF_SECRET) || is_secret_name(ip->name, strlen(ip->name))) {
                gbuffer_append_string(gbuf, " ********");
            } else if(strlen(param) > MAX_SHOWN_VALUE) {
                gbuffer_printf(gbuf, " <%zu bytes>", strlen(param));
            } else {
                gbuffer_printf(gbuf, " %s", param);
            }
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
 *  The descriptor of the command of the line `command` (its first word),
 *  NULL if there is none, with *jn_error the answer that says why (YOURS)
 ***************************************************************************/
PRIVATE const sdata_desc_t *find_command_desc(
    hgobj gobj,
    const char *command,
    json_t **jn_error
)
{
    *jn_error = 0;
    const sdata_desc_t *cmd_table = gobj_command_desc(gobj, NULL, FALSE);
    if(!cmd_table) {
        *jn_error = json_sprintf("%s: No command table", gobj_short_name(gobj));
        return NULL;
    }

    char *str, *p;
    str = p = gbmem_strdup(command);
    char *cmd = get_parameter(p, &p);
    if(empty_string(cmd)) {
        *jn_error = json_sprintf("%s: No command", gobj_short_name(gobj));
        GBMEM_FREE(str)
        return NULL;
    }

    const sdata_desc_t *cnf_cmd = command_get_cmd_desc(cmd_table, cmd);
    if(!cnf_cmd) {
        char *cmd_masked = mask_secrets_inline(cmd);  // the caller's word: it can be a secret
        *jn_error = json_sprintf(
            "%s: command not available: '%s'. Try 'help' command.",
            gobj_short_name(gobj),
            cmd_masked? cmd_masked : cmd
        );
        GBMEM_FREE(cmd_masked)
    }
    GBMEM_FREE(str)
    return cnf_cmd;
}

/***************************************************************************
 *  Return a new kw for the command `cnf_cmd`, popping the parameters of the
 *  line `command` (its words after the first). If *result < 0 there is an
 *  error, and the return json is a json string message with the error.
 ***************************************************************************/
PRIVATE json_t *expand_command(
    hgobj gobj,
    const char *command,
    const sdata_desc_t *cnf_cmd,
    json_t *kw,     // NOT owned
    int *result
)
{
    char *str, *p;
    str = p = gbmem_strdup(command);
    get_parameter(p, &p);   // the command's name, found by find_command_desc()

    json_t *kw_cmd = build_cmd_kw(gobj, cnf_cmd->name, cnf_cmd, p, command, kw, result);
    GBMEM_FREE(str)
    return kw_cmd;
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
    char *parameters,   // input line, its words after the command's (cut by the parsing)
    const char *line,   // the whole line, as it came
    json_t *kw, // not owned
    int *result
)
{
    const sdata_desc_t *input_parameters = cnf_cmd->schema;
    BOOL wild_command = (cnf_cmd->flag & SDF_WILD_CMD)?1:0;
    json_t *kw_cmd = json_object();
    char *pxxx = parameters;
    BOOL secret_given = FALSE;      // a secret parameter given on the line
    char bftemp[1] = {0};
    if(!pxxx) {
        pxxx = bftemp;
    }
    BOOL peer = json_is_string(json_object_get(kw, "__username__"));   // see refuse_parameter()

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
            const char *refused = NULL;
            json_t *jn_param = kw2parameter(  // a key, as written below
                ip->type, json_object_get(kw, ip->name), TRUE, &refused
            );
            if(refused) {
                *result = -1;
                JSON_DECREF(kw_cmd);
                return refuse_parameter(gobj, peer, command, ip->name, ip->type, refused);
            }
            if(jn_param) {
                json_object_set_new(kw_cmd, ip->name, jn_param);
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
        const char *refused = NULL;
        json_t *jn_param = text2parameter(ip->type, param, FALSE, &refused);
        if(!jn_param) {
            *result = -1;
            JSON_DECREF(kw_cmd);
            return refuse_parameter(gobj, peer, command, ip->name, ip->type,
                refused? refused : "a text that does not read as one"
            );
        }
        json_object_set_new(kw_cmd, ip->name, jn_param);
        if(is_secret_parameter(cnf_cmd, ip->name)) {
            secret_given = TRUE;    // a positional secret with blanks leaves the rest of it
        }

        ip++;
    }

    /*
     *  Next: get value from kw or default values. A value of kw that is
     *  refused is refused at the end, unless the line gives the parameter:
     *  the line has the last word (key=value below).
     */
    json_t *jn_refused = json_object();     // parameter -> what kw gave instead
    while(ip->name) {
        if(ip->flag & SDF_NOTACCESS) {
            ip++;
            continue;
        }

        const char *refused = NULL;
        json_t *jn_param = kw2parameter(  // a key, as above
            ip->type, json_object_get(kw, ip->name), FALSE, &refused
        );
        if(refused) {
            json_object_set_new(jn_refused, ip->name, json_string(refused));
            ip++;
            continue;
        }
        if(jn_param) {
            json_object_set_new(kw_cmd, ip->name, jn_param);
            ip++;
            continue;
        }

        if(ip->default_value) {
            const char *bad = NULL;
            jn_param = text2parameter(ip->type, ip->default_value, FALSE, &bad);
            if(bad) {
                /*
                 *  The default the gclass declares does not read as its type
                 */
                gobj_log_error(gobj, LOG_OPT_TRACE_STACK,
                    "function",     "%s", __FUNCTION__,
                    "msgset",       "%s", MSGSET_INTERNAL,
                    "msg",          "%s", "Command parameter default does not read as its type",
                    "command",      "%s", command,
                    "parameter",    "%s", ip->name,
                    "expected",     "%s", shape_of_type(ip->type),
                    NULL
                );
                *result = -1;
                JSON_DECREF(kw_cmd);
                JSON_DECREF(jn_refused);
                return json_sprintf(
                    "%s: command '%s', parameter '%s': its default does not read as its type",
                    gobj_short_name(gobj),
                    command,
                    ip->name
                );
            }
            if(jn_param) {
                json_object_set_new(kw_cmd, ip->name, jn_param);
            }
        }

        ip++;
    }

    /*
     *  Get key=value parameters from input line
     */
    char *key;
    char *value;
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
            JSON_DECREF(jn_refused);
            char *key_masked = strpbrk(key, " \t")? NULL : mask_secrets_inline(key);
            json_t *jn_error = json_sprintf(
                "%s: command '%s', parameter '%s': value with no closing quote",
                gobj_short_name(gobj),
                command,
                strpbrk(key, " \t")? "<...>" : (key_masked? key_masked : key)
            );
            GBMEM_FREE(key_masked)
            return jn_error;
        }
        if(!value) {
            break;
        }
        if(!key) {
            // No parameter then stop
            break;
        }
        if(strpbrk(key, " \t")) {
            /*
             *  A key with a blank in it ("password= hunter2 x=1" reads the key
             *  "hunter2 x"): a malformed line, refused, and the key not
             *  echoed -- it may be a secret. Up to 7.25.20 a SDF_WILD_CMD
             *  command took it as a free key, and any other echoed it.
             */
            *result = -1;
            JSON_DECREF(kw_cmd);
            JSON_DECREF(jn_refused);
            return json_sprintf(
                "%s: command '%s' with a malformed parameter: '<...>=...'",
                gobj_short_name(gobj),
                command
            );
        }
        const sdata_desc_t *ip2 = find_ip_parameter(input_parameters, key);
        json_t *jn_param = 0;
        if(ip2) {
            const char *refused = NULL;
            jn_param = text2parameter(ip2->type, value, FALSE, &refused);
            if(!jn_param) {
                *result = -1;
                JSON_DECREF(kw_cmd);
                JSON_DECREF(jn_refused);
                return refuse_parameter(gobj, peer, command, ip2->name, ip2->type,
                    refused? refused : "a text that does not read as one"
                );
            }
            json_object_del(jn_refused, ip2->name);    // the line gives it
        } else {
            if(wild_command) {
                jn_param = json_string(value);
            } else {
                // The key is the caller's: it can be a secret (a JWT written as a key)
                *result = -1;
                JSON_DECREF(kw_cmd);
                JSON_DECREF(jn_refused);
                char *key_masked = mask_secrets_inline(key);
                json_t *jn_error = json_sprintf(
                    "%s: '%s' command has no option '%s'",
                    gobj_short_name(gobj),
                    command,
                    key_masked? key_masked : key
                );
                GBMEM_FREE(key_masked)
                return jn_error;
            }
        }
        /*
         *  Under its DECLARED name: find_ip_parameter() matches any case,
         *  and `cmd N=5` arrived as "N" -- the handler read `n` and took
         *  its default, and a kw `n` beat the line
         */
        const char *name = ip2? ip2->name : key;
        json_object_set_new(kw_cmd, name, jn_param);
        if(is_secret_parameter(cnf_cmd, name)) {
            secret_given = TRUE;
        }
    }

    if(!empty_string(pxxx)) {
        /*
         *  The extra text is not repeated when the line carries a secret:
         *  a secret parameter given on it (SDF_SECRET or a secret's name),
         *  or anything mask_secrets_inline() masks in it -- a secret
         *  name=value, the `value` of a write-attr of a secret attribute,
         *  a JWT, a Bearer token. A secret written with blanks leaves the
         *  rest of it as extra text (password=correct horse battery, as a
         *  key=value or positional), and the `value` of a write-attr is no
         *  secret parameter: `write-attr attribute=password value=correct
         *  horse battery` answered 'horse battery'.
         *  Else the extra text is echoed, its secrets masked.
         */
        *result = -1;
        JSON_DECREF(kw_cmd);
        JSON_DECREF(jn_refused);
        char *line_masked = secret_given? NULL : mask_secrets_inline(line);
        BOOL line_has_secret = (secret_given || line_masked)? TRUE : FALSE;
        GBMEM_FREE(line_masked)
        char *masked = line_has_secret? NULL : mask_secrets_inline(pxxx);
        json_t *jn_error = json_sprintf(
            "%s: command '%s' with extra parameters: '%s'",
            gobj_short_name(gobj),
            command,
            line_has_secret? "<...>" : (masked? masked : pxxx)
        );
        GBMEM_FREE(masked)
        return jn_error;
    }

    /*
     *  What kw gave a parameter, refused, and the line did not give
     */
    const char *name;
    json_t *jn_got;
    json_object_foreach(jn_refused, name, jn_got) {
        const sdata_desc_t *ip_refused = find_ip_parameter(input_parameters, name);
        json_t *jn_error = refuse_parameter(gobj, peer, command, name,
            ip_refused? ip_refused->type : DTP_STRING, json_string_value(jn_got)
        );
        *result = -1;
        JSON_DECREF(kw_cmd);
        JSON_DECREF(jn_refused);
        return jn_error;
    }
    JSON_DECREF(jn_refused);

    /*
     *  The keys of kw that are not parameters reach the handler too;
     *  callers (the GUI) depend on it. Not a parameter that has no value
     *  here (but a DTP_JSON): what kw gave it was no value -- a null, an
     *  empty text for a number (the "" reached the
     *  handler) -- or was refused and the line gave it under another
     *  spelling of its name.
     */
    json_t *jn_no_value = json_array();
    for(ip = input_parameters; ip->name; ip++) {
        if((ip->flag & SDF_NOTACCESS) || DTP_IS_JSON(ip->type)) {
            continue;
        }
        if(!json_object_get(kw_cmd, ip->name)) {
            json_array_append_new(jn_no_value, json_string(ip->name));
        }
    }
    kw_update_missing(gobj, kw_cmd, kw);
    int idx; json_t *jn_name;
    json_array_foreach(jn_no_value, idx, jn_name) {
        json_object_del(kw_cmd, json_string_value(jn_name));
    }
    JSON_DECREF(jn_no_value);

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

        char *cmd_masked = mask_secrets_inline(cmd);  // the caller's word, see find_command_desc()
        json_t *jn_resp = json_sprintf(
            "%s: command '%s' not available.\n",
            gobj_short_name(gobj),
            cmd_masked? cmd_masked : cmd
        );
        GBMEM_FREE(cmd_masked)
        KW_DECREF(kw)   // after: `cmd` is a text of kw
        return jn_resp;
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
