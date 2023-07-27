/***********************************************************************
 *          MSG_IEVENT.C
 *
 *          Common of Inter-Events messages
 *
 *          Copyright (c) 2016,2023 Niyamaka.
 *          All Rights Reserved.
***********************************************************************/
#include <sys/types.h>
#include <unistd.h>
#include <gbuffer.h>
#include "msg_ievent.h"

/****************************************************************
 *         Constants
 ****************************************************************/

/****************************************************************
 *         Structures
 ****************************************************************/

/****************************************************************
 *         Data
 ****************************************************************/

/***************************************************************
 *              Prototypes
 ***************************************************************/

/***************************************************************
 *              Data
 ***************************************************************/

/***************************************************************************
 *  Useful to send event's messages TO outside world.
 ***************************************************************************/
PUBLIC gbuffer_t *iev_create_to_gbuffer( // TODO old iev_create()
    hgobj gobj,
    gobj_event_t event,
    json_t *kw // owned
)
{
    if(empty_string(event)) {
        gobj_log_error(gobj, LOG_OPT_TRACE_STACK,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_PARAMETER_ERROR,
            "msg",          "%s", "event NULL",
            NULL
        );
        return 0;
    }

    if(!kw) {
        kw = json_object();
    }
    kw = kw_serialize(
        gobj,
        kw  // owned
    );
    json_t *jn_iev = json_pack("{s:s, s:o}",
        "event", event,
        "kw", kw
    );
    if(!jn_iev) {
        gobj_log_error(gobj, LOG_OPT_TRACE_STACK,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_PARAMETER_ERROR,
            "msg",          "%s", "json_pack() FAILED",
            NULL
        );
    }
    size_t flags = JSON_COMPACT;
    return json2gbuf(0, jn_iev, flags);
}

/***************************************************************************
 *  Incorporate event's messages from outside world.
 *  gbuf decref
 ***************************************************************************/
PUBLIC int iev_create_from_gbuffer(
    hgobj gobj,
    iev_msg_t *iev_msg,
    gbuffer_t *gbuf,  // WARNING gbuf own and data consumed
    int verbose     // 1 log, 2 log+dump
)
{
    /*---------------------------------------*
     *  Convert gbuf msg in json
     *---------------------------------------*/
    json_t *jn_msg = gbuf2json(gbuf, verbose); // gbuf stolen: decref and data consumed
    if(!jn_msg) {
        gobj_log_error(gobj, LOG_OPT_TRACE_STACK,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_JSON_ERROR,
            "msg",          "%s", "gbuf2json() FAILED",
            NULL
        );
        iev_msg->event[0] = 0;
        iev_msg->kw = 0;
        return -1;
    }



    /*
     *  Get fields
     */
    const char *event = kw_get_str(gobj, jn_msg, "event", "", KW_REQUIRED);
    json_t *kw = kw_get_dict(gobj, jn_msg, "kw", 0, KW_REQUIRED);

    if(empty_string(event)) {
        gobj_log_error(gobj, LOG_OPT_TRACE_STACK,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_PARAMETER_ERROR,
            "msg",          "%s", "event EMPTY",
            NULL
        );
        JSON_DECREF(jn_msg);
        iev_msg->event[0] = 0;
        iev_msg->kw = 0;
        return -1;
    }
    if(!kw) { // WARNING cannot be null!
        gobj_log_error(gobj, LOG_OPT_TRACE_STACK,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_PARAMETER_ERROR,
            "msg",          "%s", "kw EMPTY",
            NULL
        );
        JSON_DECREF(jn_msg);
        iev_msg->event[0] = 0;
        iev_msg->kw = 0;
        return -1;
    }
    /*
     *  Aquí se tendría que tracear el inter-evento de entrada
     */
    /*
     *  Inter-event from world outside, deserialize!
     */
    json_incref(kw);
    json_t *new_kw = kw_deserialize(gobj, kw);

    if(strlen(event) >= sizeof(iev_msg->event)) {
        gobj_log_error(gobj, LOG_OPT_TRACE_STACK,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_PARAMETER_ERROR,
            "msg",          "%s", "Event name TOO LARGE",
            "len",          "%d", strlen(event),
            "maxlen",       "%d", sizeof(iev_msg->event)-1,
            NULL
        );
    }
    snprintf(iev_msg->event, sizeof(iev_msg->event), "%s", event);
    iev_msg->kw = new_kw;
    JSON_DECREF(jn_msg);

    return 0;
}

/***************************************************************************
 *  Key: IEVENT_MESSAGE_AREA_ID "ievent_gate_stack"
 *  Dale la vuelta src->dst dst->src
 ***************************************************************************/
PRIVATE int ievent_answer_filter(
    hgobj gobj,
    json_t* jn_ievent_gate_stack)
{
    json_t * jn_ievent = json_array_get(jn_ievent_gate_stack, 0);

    /*
     *  Dale la vuelta src->dst dst->src
     */
    const char *iev_src_yuno = kw_get_str(gobj, jn_ievent, "src_yuno", "", KW_REQUIRED);
    const char *iev_src_role = kw_get_str(gobj, jn_ievent, "src_role", "", KW_REQUIRED);
    const char *iev_src_service = kw_get_str(gobj, jn_ievent, "src_service", "", KW_REQUIRED);

    json_object_set_new(jn_ievent, "dst_yuno", json_string(iev_src_yuno));
    json_object_set_new(jn_ievent, "dst_role", json_string(iev_src_role));
    json_object_set_new(jn_ievent, "dst_service", json_string(iev_src_service));
    json_object_set_new(jn_ievent, "src_yuno", json_string(gobj_name(gobj_yuno())));
    json_object_set_new(jn_ievent, "src_role", json_string(gobj_read_str_attr(gobj_yuno(), "yuno_role")));

    json_object_set_new(jn_ievent, "src_yuno", json_string(gobj_yuno_name()));
    json_object_set_new(jn_ievent, "src_role", json_string(gobj_yuno_role()));

    json_object_set_new(jn_ievent, "src_service", json_string(gobj_name(gobj)));

    return 0;
}

/***************************************************************************
 *  Apply answer filters
 ***************************************************************************/
PRIVATE int msg_apply_answer_filters(hgobj gobj, json_t *__request_msg_area__)
{
    json_t *jn_value = kw_get_dict_value(gobj, __request_msg_area__, IEVENT_MESSAGE_AREA_ID, 0, 0);
    if(jn_value) {
        return ievent_answer_filter(gobj, jn_value);
    }
    return -1;
}

///***************************************************************************
// *  Return a new kw with all minus this keys:
// ***************************************************************************/
//PUBLIC json_t *msg_iev_pure_clone(
//    json_t *kw  // NOT owned
//)
//{
//    json_t *kw_clone = kw_duplicate(kw); // not owned
//    json_object_del(kw_clone, "__md_iev__");
//    json_object_del(kw_clone, "__temp__");
//    json_object_del(kw_clone, "__md_tranger__");
//    return kw_clone;
//}

/***************************************************************************
 *
 ***************************************************************************/
PUBLIC json_t *build_webix(
    hgobj gobj,
    json_int_t result,
    json_t *jn_comment, // owned
    json_t *jn_schema,  // owned
    json_t *jn_data)    // owned
{
    if(!jn_comment) {
        jn_comment = json_string("");
    }
    if(!jn_schema) {
        jn_schema = json_null();
    }
    if(!jn_data) {
        jn_data = json_null();
    }

    const char *comment = json_string_value(jn_comment);
    json_t *webix = json_pack("{s:I, s:s, s:o, s:o}",
        "result", (json_int_t)result,
        "comment", comment?comment:"",
        "schema", jn_schema,
        "data", jn_data
    );
    JSON_DECREF(jn_comment);
    if(!webix) {
        gobj_log_error(gobj, LOG_OPT_TRACE_STACK,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_INTERNAL_ERROR,
            "msg",          "%s", "build webix FAILED",
            NULL
        );
    }
    return webix;
}

/***************************************************************************
 *
 ***************************************************************************/
PUBLIC json_t *msg_iev_build_webix(
    hgobj gobj,
    json_int_t result,
    json_t *jn_comment, // owned
    json_t *jn_schema,  // owned
    json_t *jn_data,    // owned
    json_t *kw_request, // owned, used to extract ONLY __md_iev__.
    const char *msg_type)
{
    json_t *webix = build_webix(gobj, result, jn_comment, jn_schema, jn_data);
    json_t *webix_answer = msg_iev_answer(gobj, kw_request, webix, msg_type);

    return webix_answer;
}

/***************************************************************************
 *
 ***************************************************************************/
PUBLIC json_t *msg_iev_build_webix2_without_answer_filter(
    hgobj gobj,
    json_int_t result,
    json_t *jn_comment, // owned
    json_t *jn_schema,  // owned
    json_t *jn_data,    // owned
    json_t *kw_request, // owned, used to extract ONLY __md_iev__.
    const char *msg_type
)
{
    json_t *webix = build_webix(gobj, result, jn_comment, jn_schema, jn_data);
    json_t *webix_answer = msg_iev_answer_without_answer_filter(gobj, kw_request, webix, msg_type);

    return webix_answer;
}

/***************************************************************************
 *  server: build the answer of kw
 *  Return a new kw only with the identification message area.
 ***************************************************************************/
PUBLIC json_t *msg_iev_answer(
    hgobj gobj,
    json_t *kw_request,     // owned, kw request, used to extract ONLY __md_iev__.
    json_t *kw_answer,      // like owned, is returned!, created if null, body of answer message.
    const char *msg_type
)
{
//    if(!kw_answer) {
//        kw_answer = json_object();
//    }
//    json_t *__md_iev__ = kw_get_dict(kw_request, "__md_iev__", 0, 0);
//    if(__md_iev__) {
//        json_t *request_msg_area = kw_duplicate(__md_iev__);
//        json_object_set_new(kw_answer, "__md_iev__", request_msg_area);
//        msg_set_msg_type(kw_answer, msg_type);
//        if(!kw_has_key(request_msg_area, "__md_yuno__")) {
//            json_t *jn_metadata = json_pack("{s:s, s:s, s:s, s:s, s:I, s:s}",
//                "realm_name", gobj_yuno_realm_name(),
//                "yuno_role", gobj_yuno_role(),
//                "yuno_name", gobj_name(gobj_yuno()),
//                "yuno_id", gobj_yuno_id(),
//                "pid", (json_int_t)getpid(),
//                "user", get_user_name()
//            );
//            json_object_set_new(request_msg_area, "__md_yuno__", jn_metadata);
//        }
//
//        msg_apply_answer_filters(gobj, request_msg_area, kw_answer);
//    }
//    json_object_del(kw_answer, "__temp__");
//
//    KW_DECREF(kw_request);
//    return kw_answer;
return 0; //TODO
}

/***************************************************************************
 *  server: build the answer of kw
 *  Return a new kw only with the identification message area.
 ***************************************************************************/
PUBLIC json_t *msg_iev_answer_without_answer_filter(
    hgobj gobj,
    json_t *kw_request,         // kw request, owned
    json_t *kw_answer,          // like owned, is returned!, created if null, body of answer message.
    const char *msg_type
)
{
//    if(!kw_answer) {
//        kw_answer = json_object();
//    }
//    json_t *__md_iev__ = kw_get_dict(kw_request, "__md_iev__", 0, 0);
//    if(__md_iev__) {
//        json_t *request_msg_area = kw_duplicate(__md_iev__);
//        json_object_set_new(kw_answer, "__md_iev__", request_msg_area);
//        msg_set_msg_type(kw_answer, msg_type);
//
//        if(!kw_has_key(request_msg_area, "__md_yuno__")) {
//            json_t *jn_metadata = json_pack("{s:s, s:s, s:s, s:s, s:I, s:s}",
//                "realm_name", gobj_yuno_realm_name(),
//                "yuno_role", gobj_yuno_role(),
//                "yuno_name", gobj_name(gobj_yuno()),
//                "yuno_id", gobj_yuno_id(),
//                "pid", (json_int_t)getpid(),
//                "user", get_user_name()
//            );
//            json_object_set_new(request_msg_area, "__md_yuno__", jn_metadata);
//        }
//    }
//    json_object_del(kw_answer, "__temp__");
//
//    KW_DECREF(kw_request);
//    return kw_answer;
return 0; //TODO
}

/***************************************************************************
 *
 *  __your_stack__ is a LIFO queue, ultimo en entrar, primero en salir
 *
 {
    '__md_iev__': {
        '__your_stack__': [
            request-msg_id-last,
            ...
            request-msg_id-first
        ],
        ...
    }
 }

 ***************************************************************************/
PUBLIC int msg_iev_push_stack(
    hgobj gobj,
    json_t *kw,             // not owned
    const char *stack,
    json_t *jn_user_info         // owned
)
{
//    if(!json_is_object(kw)) {
//        gobj_log_error(gobj, LOG_OPT_TRACE_STACK,
//            "function",     "%s", __FUNCTION__,
//            "msgset",       "%s", MSGSET_INTERNAL_ERROR,
//            "msg",          "%s", "kw is not a dict.",
//            NULL
//        );
//        return -1;
//    }
//    json_t *jn_stack = kw_get_subdict_value(gobj, kw, "__md_iev__", stack, 0, 0);
//    if(!jn_stack) {
//        jn_stack = json_array();
//        kw_set_subdict_value(gobj, kw, "__md_iev__", stack, jn_stack);
//    }
//    return json_array_insert_new(jn_stack, 0, jn_user_info);
return 0; //TODO
}

/***************************************************************************
 *  Get current item from stack, without poping.
 ***************************************************************************/
PUBLIC json_t *msg_iev_get_stack( // return is not yours!
    json_t *kw,
    const char *stack,
    BOOL print_not_found
)
{
//    json_t *jn_stack = kw_get_subdict_value(gobj, kw, "__md_iev__", stack, 0, 0);
//    if(!jn_stack) {
//        if(print_not_found) {
//    gobj_log_error(gobj, LOG_OPT_TRACE_STACK,
//                "gobj",         "%s", __FILE__,
//                "function",     "%s", __FUNCTION__,
//                "msgset",       "%s", MSGSET_INTERNAL_ERROR,
//                "msg",          "%s", "stack NOT EXIST.",
//                "stack",        "%s", stack,
//                NULL
//            );
//        }
//        return 0;
//    }
//    json_t *jn_user_info = json_array_get(jn_stack, 0);
//    return jn_user_info;
return 0; //TODO
}

/***************************************************************************
 *  Ppo current item from stack, poping
 *  WARNING free the return
 ***************************************************************************/
PUBLIC json_t * msg_iev_pop_stack(
    json_t *kw,
    const char *stack
)
{
//    /*-----------------------------------*
//     *  Recover the current item
//     *-----------------------------------*/
//    json_t *jn_stack = kw_get_subdict_value(gobj, kw, "__md_iev__", stack, 0, 0);
//    if(!jn_stack) {
//    gobj_log_error(gobj, LOG_OPT_TRACE_STACK,
//            "gobj",         "%s", __FILE__,
//            "function",     "%s", __FUNCTION__,
//            "msgset",       "%s", MSGSET_INTERNAL_ERROR,
//            "msg",          "%s", "stack NOT EXIST.",
//            "stack",        "%s", stack,
//            NULL
//        );
//        return 0;
//    }
//    json_t *jn_user_info = json_array_get(jn_stack, 0);
//    json_incref(jn_user_info);
//    json_array_remove(jn_stack, 0);
//    if(json_array_size(jn_stack)==0) {
//        kw_delete_subkey(kw, "__md_iev__", stack);
//    }
//    return jn_user_info;
return 0; //TODO
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE const char *msg_type_list[] = {
    "__command__",
    "__publishing__",
    "__subscribing__",
    "__unsubscribing__",
    "__query__",
    "__response__",
    "__order__",
    "__first_shot__",
    0
};

PUBLIC int msg_set_msg_type(
    json_t *kw,
    const char *msg_type
)
{
//    if(!empty_string(msg_type)) {
//        if(is_metadata_key(msg_type) && !str_in_list(msg_type_list, msg_type, TRUE)) {
//            // HACK If it's a metadata key then only admit our message inter-event msg_type_list
//            return kw_delete(kw, "__md_iev__`__msg_type__");
//        }
//        return kw_set_subdict_value(kw, "__md_iev__", "__msg_type__", json_string(msg_type));
//    }
    return 0;
}

///***************************************************************************
// *
// ***************************************************************************/
//PUBLIC const char *msg_get_msg_type(
//    json_t *kw
//)
//{
//    return kw_get_str(kw, "__md_iev__`__msg_type__", "", 0);
//}

/***************************************************************************
 *
 ***************************************************************************/
PUBLIC void trace_inter_event(hgobj gobj, const char *prefix, const char *event, json_t *kw)
{
    json_t * kw_compact = json_object();
    if(kw_has_key(kw, "result")) {
        json_object_set(kw_compact, "result", kw_get_dict_value(gobj, kw, "result", 0, 0));
    }
    if(kw_has_key(kw, "__md_iev__")) {
        json_object_set(kw_compact, "__md_iev__", kw_get_dict_value(gobj, kw, "__md_iev__", 0, 0));
    }

    json_t *jn_iev = json_pack("{s:s, s:o}",
        "event", event?event:"???",
        "kw", kw_compact
    );

    gobj_trace_json(gobj, jn_iev, "%s", prefix);
    json_decref(jn_iev);
}

/***************************************************************************
 *
 ***************************************************************************/
PUBLIC void trace_inter_event2(hgobj gobj, const char *prefix, const char *event, json_t *kw)
{
    json_t *jn_iev = json_pack("{s:s, s:O}",
        "event", event?event:"???",
        "kw", kw
    );

    gobj_trace_json(gobj, jn_iev, "%s", prefix);
    json_decref(jn_iev);
}
