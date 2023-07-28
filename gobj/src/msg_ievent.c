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
#include "msg_ievent.h"

/****************************************************************
 *         Constants
 ****************************************************************/

/****************************************************************
 *         Structures
 ****************************************************************/

/****************************************************************
 *         Prototypes
 ****************************************************************/


/***************************************************************************
 *  Useful to send event's messages TO outside world.
 ***************************************************************************/
PUBLIC gbuffer_t *iev_create_to_gbuffer( // TODO old iev_create()
    hgobj gobj,
    gobj_event_t event,
    json_t *kw // owned
) {
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
PUBLIC json_t *iev_create_from_gbuffer(
    hgobj gobj,
    gobj_event_t *event,
    gbuffer_t *gbuf,  // WARNING gbuf own and data consumed
    int verbose     // 1 log, 2 log+dump
) {
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
        *event = NULL;
        return NULL;
    }

    /*
     *  Get fields
     */
    const char *event_ = kw_get_str(gobj, jn_msg, "event", "", KW_REQUIRED);
    json_t *kw = kw_get_dict(gobj, jn_msg, "kw", 0, KW_REQUIRED);

    if(empty_string(event_)) {
        gobj_log_error(gobj, LOG_OPT_TRACE_STACK,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_PARAMETER_ERROR,
            "msg",          "%s", "event EMPTY",
            NULL
        );
        JSON_DECREF(jn_msg);
        *event = NULL;
        return NULL;
    }
    if(!kw) { // WARNING cannot be null!
        gobj_log_error(gobj, LOG_OPT_TRACE_STACK,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_PARAMETER_ERROR,
            "msg",          "%s", "kw EMPTY",
            NULL
        );
        JSON_DECREF(jn_msg);
        *event = NULL;
        return NULL;
    }

    /*
     *  Aquí se tendría que tracear el inter-evento de entrada
     */
    /*
     *  Inter-event from world outside, deserialize!
     */
    json_incref(kw);
    json_t *new_kw = kw_deserialize(gobj, kw);

    *event = event_; // TODO convert string to gobj_event_t!!!
    JSON_DECREF(jn_msg);

    return new_kw;
}

/***************************************************************************
 *
 ***************************************************************************/
PUBLIC int msg_iev_push_stack( // Push a record in the stack
    hgobj gobj,
    json_t *kw,             // not owned
    const char *stack,
    json_t *jn_data         // owned, data to be pushed in the stack
)
{
    if(!json_is_object(kw)) {
        gobj_log_error(gobj, LOG_OPT_TRACE_STACK,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_INTERNAL_ERROR,
            "msg",          "%s", "kw is not a dict",
            NULL
        );
        return -1;
    }
    json_t *md_iev = kw_get_dict(gobj, kw, "__md_iev__", 0, 0);
    if(!md_iev) {
        gobj_log_error(gobj, LOG_OPT_TRACE_STACK,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_INTERNAL_ERROR,
            "msg",          "%s", "__md_iev__ NOT FOUND",
            NULL
        );
        gobj_trace_json(gobj, kw, "__md_iev__ NOT FOUND");
        return -1;
    }

    json_t *jn_stack = kw_get_list(gobj, md_iev, stack, 0, 0);
    if(!jn_stack) {
        jn_stack = json_array();
        json_object_set_new(md_iev, stack, jn_stack);
    }
    return json_array_insert_new(jn_stack, 0, jn_data);
}

/***************************************************************************
 *
 ***************************************************************************/
PUBLIC json_t *msg_iev_get_stack( // Get a record without popping from the stack. Return is NOT YOURS!
    hgobj gobj,
    json_t *kw,             // not owned
    const char *stack,
    BOOL verbose
)
{
    if(!json_is_object(kw)) {
        gobj_log_error(gobj, LOG_OPT_TRACE_STACK,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_INTERNAL_ERROR,
            "msg",          "%s", "kw is not a dict",
            NULL
        );
        return NULL;
    }
    json_t *md_iev = kw_get_dict(gobj, kw, "__md_iev__", 0, 0);
    if(!md_iev) {
        if(verbose) {
            gobj_log_error(gobj, LOG_OPT_TRACE_STACK,
                "function",     "%s", __FUNCTION__,
                "msgset",       "%s", MSGSET_INTERNAL_ERROR,
                "msg",          "%s", "__md_iev__ NOT FOUND",
                NULL
            );
            gobj_trace_json(gobj, kw, "__md_iev__ NOT FOUND");
        }
        return NULL;
    }

    json_t *jn_stack = kw_get_list(gobj, md_iev, stack, 0, 0);
    if(!jn_stack) {
        if(verbose) {
            gobj_log_error(gobj, LOG_OPT_TRACE_STACK,
                "function",     "%s", __FUNCTION__,
                "msgset",       "%s", MSGSET_INTERNAL_ERROR,
                "msg",          "%s", "__md_iev__ stack NOT FOUND",
                "stack",        "%s", stack,
                NULL
            );
            gobj_trace_json(gobj, kw, "__md_iev__ stack NOT FOUND");
        }
        return NULL;
    }

    json_t *jn_data = json_array_get(jn_stack, 0);
    return jn_data;
}

/***************************************************************************
 *
 ***************************************************************************/
PUBLIC json_t * msg_iev_pop_stack( // Pop a record from stack. Return is YOURS, must be FREE!
    hgobj gobj,
    json_t *kw,
    const char *stack
)
{
    if(!json_is_object(kw)) {
        gobj_log_error(gobj, LOG_OPT_TRACE_STACK,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_INTERNAL_ERROR,
            "msg",          "%s", "kw is not a dict",
            NULL
        );
        return NULL;
    }
    json_t *md_iev = kw_get_dict(gobj, kw, "__md_iev__", 0, 0);
    if(!md_iev) {
        gobj_log_error(gobj, LOG_OPT_TRACE_STACK,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_INTERNAL_ERROR,
            "msg",          "%s", "__md_iev__ NOT FOUND",
            NULL
        );
        gobj_trace_json(gobj, kw, "__md_iev__ NOT FOUND");
        return NULL;
    }

    json_t *jn_stack = kw_get_list(gobj, md_iev, stack, 0, 0);
    if(!jn_stack) {
        gobj_log_error(gobj, LOG_OPT_TRACE_STACK,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_INTERNAL_ERROR,
            "msg",          "%s", "__md_iev__ stack NOT FOUND",
            "stack",        "%s", stack,
            NULL
        );
        gobj_trace_json(gobj, kw, "__md_iev__ stack NOT FOUND");
        return NULL;
    }

    json_t *jn_data = json_array_get(jn_stack, 0);

    json_incref(jn_data);
    json_array_remove(jn_stack, 0);
    return jn_data;
}

/***************************************************************************
 *  Key: IEVENT_MESSAGE_AREA_ID "ievent_gate_stack"
 *  Dale la vuelta src->dst dst->src
 ***************************************************************************/
//PRIVATE int ievent_answer_filter(
//    hgobj gobj,
//    json_t* jn_ievent_gate_stack)
//{
//    json_t * jn_ievent = json_array_get(jn_ievent_gate_stack, 0);
//
//    /*
//     *  Dale la vuelta src->dst dst->src
//     */
//    const char *iev_src_yuno = kw_get_str(gobj, jn_ievent, "src_yuno", "", KW_REQUIRED);
//    const char *iev_src_role = kw_get_str(gobj, jn_ievent, "src_role", "", KW_REQUIRED);
//    const char *iev_src_service = kw_get_str(gobj, jn_ievent, "src_service", "", KW_REQUIRED);
//
//    json_object_set_new(jn_ievent, "dst_yuno", json_string(iev_src_yuno));
//    json_object_set_new(jn_ievent, "dst_role", json_string(iev_src_role));
//    json_object_set_new(jn_ievent, "dst_service", json_string(iev_src_service));
//    json_object_set_new(jn_ievent, "src_yuno", json_string(gobj_name(gobj_yuno())));
//    json_object_set_new(jn_ievent, "src_role", json_string(gobj_read_str_attr(gobj_yuno(), "yuno_role")));
//
//    json_object_set_new(jn_ievent, "src_yuno", json_string(gobj_yuno_name()));
//    json_object_set_new(jn_ievent, "src_role", json_string(gobj_yuno_role()));
//
//    json_object_set_new(jn_ievent, "src_service", json_string(gobj_name(gobj)));
//
//    return 0;
//}

/***************************************************************************
 *  Apply answer filters
 ***************************************************************************/
//PRIVATE int msg_apply_answer_filters(hgobj gobj, json_t *__request_msg_area__)
//{
//    json_t *jn_value = kw_get_dict_value(gobj, __request_msg_area__, IEVENT_MESSAGE_AREA_ID, 0, 0);
//    if(jn_value) {
//        return ievent_answer_filter(gobj, jn_value);
//    }
//    return -1;
//}

///***************************************************************************
// *  Return a new kw with all minus this keys:
//*  Return a new kw with all minus this keys:
//"__md_iev__"
//"__temp__"
//"__md_tranger__"
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
PUBLIC json_t *build_command_response( // old build_webix
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
    json_t *webix = build_command_response(gobj, result, jn_comment, jn_schema, jn_data);
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
    json_t *webix = build_command_response(gobj, result, jn_comment, jn_schema, jn_data);
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
 ***************************************************************************/
//PRIVATE const char *msg_type_list[] = {
//    "__command__",
//    "__publishing__",
//    "__subscribing__",
//    "__unsubscribing__",
//    "__query__",
//    "__response__",
//    "__order__",
//    "__first_shot__",
//    0
//};

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
