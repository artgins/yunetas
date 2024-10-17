/***********************************************************************
 *          MSG_IEVENT.C
 *
 *          Common of Inter-Events messages
 *
 *          Copyright (c) 2016,2023 Niyamaka.
 *          Copyright (c) 2024, ArtGins.
 *          All Rights Reserved.
***********************************************************************/
#include <unistd.h>
#include "msg_ievent.h"

/****************************************************************
 *         Constants
 ****************************************************************/

/****************************************************************
 *         Structures
 ****************************************************************/
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

/****************************************************************
 *         Prototypes
 ****************************************************************/

/****************************************************************
 *         FSM
 ****************************************************************/
/*------------------------*
 *      Events
 *------------------------*/
GOBJ_DEFINE_EVENT(EV_IDENTITY_CARD);
GOBJ_DEFINE_EVENT(EV_IDENTITY_CARD_ACK);
GOBJ_DEFINE_EVENT(EV_PLAY_YUNO);
GOBJ_DEFINE_EVENT(EV_PLAY_YUNO_ACK);
GOBJ_DEFINE_EVENT(EV_PAUSE_YUNO);
GOBJ_DEFINE_EVENT(EV_PAUSE_YUNO_ACK);
GOBJ_DEFINE_EVENT(EV_MT_STATS);
GOBJ_DEFINE_EVENT(EV_MT_STATS_ANSWER);
GOBJ_DEFINE_EVENT(EV_MT_COMMAND);
GOBJ_DEFINE_EVENT(EV_MT_COMMAND_ANSWER);
GOBJ_DEFINE_EVENT(EV_SEND_COMMAND_ANSWER);

/***************************************************************************
 *  Useful to send event's messages TO outside world.
 ***************************************************************************/
PUBLIC gbuffer_t *iev_create_to_gbuffer( // old iev_create()
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
    if(event) {
        *event = NULL;
    }
    json_t *jn_msg = gbuf2json(gbuf, verbose); // gbuf stolen: decref and data consumed
    if(!jn_msg) {
        gobj_log_error(gobj, LOG_OPT_TRACE_STACK,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_JSON_ERROR,
            "msg",          "%s", "gbuf2json() FAILED",
            NULL
        );
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
        JSON_DECREF(jn_msg)
        return NULL;
    }
    if(!kw) { // WARNING cannot be null!
        gobj_log_error(gobj, LOG_OPT_TRACE_STACK,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_PARAMETER_ERROR,
            "msg",          "%s", "kw EMPTY",
            NULL
        );
        JSON_DECREF(jn_msg)
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

    if(event) {
        *event = gclass_find_public_event(event_, verbose);
    }
    JSON_DECREF(jn_msg)

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
    json_t *md_iev = kw_get_dict(gobj, kw, "__md_iev__", json_object(), KW_CREATE);
    json_t *jn_stack = kw_get_list(gobj, md_iev, stack, 0, 0);
    if(!jn_stack) {
        jn_stack = json_array();
        json_object_set_new(md_iev, stack, jn_stack);
    }
    if(json_is_array(jn_stack)) {
        return json_array_insert_new(jn_stack, 0, jn_data);
    } else {
        gobj_log_error(gobj, LOG_OPT_TRACE_STACK,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_INTERNAL_ERROR,
            "msg",          "%s", "jn_stack is not a list",
            "stack",        "%s", stack,
            NULL
        );
        gobj_trace_json(gobj, kw, "jn_stack is not a list");
        return -1;
    }
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
    if(json_array_size(jn_stack)==0) {
        json_object_del(md_iev, stack);
    }
    return jn_data;
}

/***************************************************************************
 *
 ***************************************************************************/
//PUBLIC int append_yuno_metadata(hgobj gobj, json_t *kw, const char *source)
//{
//    if(!kw) {
//        return -1;
//    }
//
//    time_t t;
//    time(&t);
//
//    if(!source) {
//        source = "";
//    }
//    json_t *jn_metadatos = json_object();
//    json_object_set_new(jn_metadatos, "__t__", json_integer(t));
//    json_object_set_new(jn_metadatos, "__origin__", json_string(source));
//    json_object_set_new(jn_metadatos, "hostname", json_string(hostname));
//    json_object_set_new(jn_metadatos, "node_owner", json_string(__node_owner__));
//    json_object_set_new(jn_metadatos, "realm_id", json_string(__realm_id__));
//    json_object_set_new(jn_metadatos, "realm_owner", json_string(__realm_owner__));
//    json_object_set_new(jn_metadatos, "realm_role", json_string(__realm_role__));
//    json_object_set_new(jn_metadatos, "realm_name", json_string(__realm_name__));
//    json_object_set_new(jn_metadatos, "realm_env", json_string(__realm_env__));
//    json_object_set_new(jn_metadatos, "yuno_id", json_string(__yuno_id__));
//    json_object_set_new(jn_metadatos, "yuno_role", json_string(__yuno_role__));
//    json_object_set_new(jn_metadatos, "yuno_name", json_string(__yuno_name__));
//    json_object_set_new(jn_metadatos, "gobj_name", json_string(gobj_short_name(gobj)));
//    json_object_set_new(jn_metadatos, "pid", json_integer(getpid()));
//    json_object_set_new(kw, "__md_yuno__", jn_metadatos);
//
//    return 0;
//}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE int msg_iev_reverse_dst( // Put in destining the source, and in the source the gobj
    hgobj gobj,
    json_t* jn_stack_record
) {
    const char *iev_src_yuno = kw_get_str(gobj, jn_stack_record, "src_yuno", "", KW_REQUIRED);
    const char *iev_src_role = kw_get_str(gobj, jn_stack_record, "src_role", "", KW_REQUIRED);
    const char *iev_src_service = kw_get_str(gobj, jn_stack_record, "src_service", "", KW_REQUIRED);

    json_object_set_new(jn_stack_record, "dst_yuno", json_string(iev_src_yuno));
    json_object_set_new(jn_stack_record, "dst_role", json_string(iev_src_role));
    json_object_set_new(jn_stack_record, "dst_service", json_string(iev_src_service));

    json_object_set_new(jn_stack_record, "src_yuno", json_string(gobj_yuno_name()));
    json_object_set_new(jn_stack_record, "src_role", json_string(gobj_yuno_role()));
    json_object_set_new(jn_stack_record, "src_service", json_string(gobj_name(gobj)));

    return 0;
}

/***************************************************************************
 *  Set iev metadata back in a response
 *  Implicitly:
 *      - Delete __temp__ key in kw_response
 *      - Set __md_yuno__ key
 *
 *  old msg_iev_answer() with reverse_dst TRUE
 *  old msg_iev_answer_without_answer_filter() with reverse_dst FALSE
 ***************************************************************************/
PUBLIC json_t *msg_iev_set_back_metadata(
    hgobj gobj,
    json_t *kw_request,     // owned, kw request, used to extract ONLY __md_iev__.
    json_t *kw_response,    // like owned, is returned!, created if null, the body of answer message.
    BOOL reverse_dst
) {
    if(!kw_response) {
        kw_response = json_object();
    }
    json_t *__md_iev_src__ = kw_get_dict(gobj, kw_request, "__md_iev__", 0, 0);
    if(!__md_iev_src__) {
        gobj_log_error(gobj, LOG_OPT_TRACE_STACK,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_INTERNAL_ERROR,
            "msg",          "%s", "__md_iev__ NOT FOUND",
            NULL
        );
        gobj_trace_json(gobj, kw_request, "__md_iev__ NOT FOUND");
        KW_DECREF(kw_request);
        return kw_response;
    }

    json_t *__md_iev_dst__ = json_deep_copy(__md_iev_src__);
    json_object_set_new(kw_response, "__md_iev__", __md_iev_dst__);

    time_t t;
    time(&t);

    json_t *jn_metadata = json_pack("{s:s, s:s, s:s, s:s, s:I}",
        "realm_name", gobj_yuno_realm_name(),
        "yuno_role", gobj_yuno_role(),
        "yuno_name", gobj_name(gobj_yuno()),
        "yuno_id", gobj_yuno_id(),
        "__t__", t
    );
    json_object_set_new(__md_iev_dst__, "__md_yuno__", jn_metadata);

    if(reverse_dst) {
        json_t *jn_ievent_id = msg_iev_get_stack(gobj, kw_response, IEVENT_MESSAGE_AREA_ID, TRUE);
        msg_iev_reverse_dst(gobj, jn_ievent_id);
    }

    json_object_del(kw_response, "__temp__");

    KW_DECREF(kw_request);
    return kw_response;
}

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
