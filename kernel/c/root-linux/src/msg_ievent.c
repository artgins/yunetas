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
#include <helpers.h>
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
 *
 ***************************************************************************/
PUBLIC json_t *iev_create( // For use within Yuno
    hgobj gobj,
    gobj_event_t event,
    json_t *kw // owned
)
{
    if(empty_string(event)) {
        gobj_log_error(gobj, 0,
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
    json_t *jn_iev = json_pack("{s:I, s:o}",
        "event", (json_int_t)(uintptr_t)event,
        "kw", kw
    );
    if(!jn_iev) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_PARAMETER_ERROR,
            "msg",          "%s", "json_pack() FAILED",
            NULL
        );
    }
    return jn_iev;
}

/***************************************************************************
 *
 ***************************************************************************/
PUBLIC json_t *iev_create2( // For use within Yuno
    hgobj gobj,
    gobj_event_t event,
    json_t *jn_data,    // owned
    json_t *kw_request  // owned, used to get ONLY __temp__.
)
{
    json_t* iev = iev_create(
        gobj,
        event,
        jn_data // owned
    );
    json_t *__temp__ = kw_get_dict_value(gobj, kw_request, "__temp__", 0, KW_REQUIRED);
    json_object_set(iev, "__temp__", __temp__);  // Set the channel
    KW_DECREF(kw_request);

    return iev;
}

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
 *  Incorporate event's messages FROM outside world.
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
        gobj_log_error(gobj, 0,
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
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_PARAMETER_ERROR,
            "msg",          "%s", "event EMPTY",
            NULL
        );
        JSON_DECREF(jn_msg)
        return NULL;
    }
    if(!kw) { // WARNING cannot be null!
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_PARAMETER_ERROR,
            "msg",          "%s", "kw EMPTY",
            NULL
        );
        JSON_DECREF(jn_msg)
        return NULL;
    }

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
    Examples:

    ###############################################

    msg_iev_push_stack(
        gobj,
        kw,         // not owned
        STATS_STACK_ID,
        json_pack("{s:s, s:o}",   // owned
            "stats", stats,
            "kw", __md_stats__
        )
    );

    msg_iev_push_stack(
        gobj,
        kw,         // not owned
        IEVENT_STACK_ID,
        jn_ievent_id   // owned
    );

    json_object_set_new(kw, "__stats__", json_string(stats));
    msg_iev_set_msg_type(gobj, kw, "__stats__");

    ###############################################

    msg_iev_push_stack(
        gobj,
        kw,         // not owned
        COMMAND_STACK_ID,
        json_pack("{s:s, s:o}",   // owned
            "command", command,
            "kw", __md_command__
        )
    );

    msg_iev_push_stack(
        gobj,
        kw,         // not owned
        IEVENT_STACK_ID,
        jn_ievent_id   // owned
    );

    json_object_set_new(kw, "__command__", json_string(command));
    msg_iev_set_msg_type(gobj, kw, "__command__");

    ###############################################

     //    __MESSAGE__
     //  Put the ievent if it doesn't come with it,
     //  if it does come with it, it's because it will be a response
     //

    json_t *jn_request = msg_iev_get_stack(gobj, kw, IEVENT_STACK_ID, FALSE);
    if(jn_request) {
         // __RESPONSE__
    } else {
        // __REQUEST__
        json_t *jn_ievent_id = build_cli_ievent_request(
            gobj,
            gobj_name(src),
            kw_get_str(gobj, kw, "__service__", 0, 0)
        );
        json_object_del(kw, "__service__");

        msg_iev_push_stack(
            gobj,
            kw,         // not owned
            IEVENT_STACK_ID,
            jn_ievent_id   // owned
        );

        msg_iev_push_stack(
            gobj,
            kw,         // not owned
            "__message__",
            json_string(event)   // owned
        );
    }

    msg_iev_set_msg_type(gobj, kw, "__message__");


    ###############################################

    msg_iev_push_stack(
        gobj,
        kw,         // not owned
        IEVENT_STACK_ID,
        jn_ievent_id   // owned
    );

    msg_iev_set_msg_type(gobj, kw, "__unsubscribing__");


    ###############################################

    msg_iev_push_stack(
        gobj,
        kw_identity_card,         // not owned
        IEVENT_STACK_ID,
        jn_ievent_id   // owned
    );

    msg_iev_set_msg_type(gobj, kw_identity_card, "__identity__");

    ###############################################

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
    json_t *kw_request,     // owned, kw request, used to get ONLY __md_iev__.
    json_t *kw_response,    // like owned, is returned!, created if null, the body of answer message.
    BOOL reverse_dst
) {
    if(!kw_response) {
        kw_response = json_object();
    }
    json_t *__md_iev_src__ = kw_get_dict(gobj, kw_request, "__md_iev__", 0, 0);
    if(!__md_iev_src__) {
        json_object_del(kw_response, "__temp__");
        KW_DECREF(kw_request)
        return kw_response;
    }

    json_t *__md_iev_dst__ = json_deep_copy(__md_iev_src__);
    json_object_set_new(kw_response, "__md_iev__", __md_iev_dst__);

    json_t *jn_metadata = json_pack("{s:s, s:s, s:s, s:s}",
        "realm_name", gobj_yuno_realm_name(),
        "yuno_role", gobj_yuno_role(),
        "yuno_name", gobj_name(gobj_yuno()),
        "yuno_id", gobj_yuno_id()
    );
    json_object_set_new(kw_response, "__md_yuno__", jn_metadata);

    if(reverse_dst) {
        json_t *jn_ievent_id = msg_iev_get_stack(gobj, kw_response, IEVENT_STACK_ID, TRUE);
        msg_iev_reverse_dst(gobj, jn_ievent_id);
    }

    json_object_del(kw_response, "__temp__");

    KW_DECREF(kw_request)
    return kw_response;
}

/***************************************************************************
 *
 ***************************************************************************/
PUBLIC json_t *msg_iev_build_response( // OLD msg_iev_build_webix()
    hgobj gobj,
    json_int_t result,
    json_t *jn_comment, // owned
    json_t *jn_schema,  // owned
    json_t *jn_data,    // owned
    json_t *kw_request  // owned, used to get ONLY __md_iev__.
) {
    json_t *jn_response = build_command_response(gobj, result, jn_comment, jn_schema, jn_data);
    json_t *jn_answer = msg_iev_set_back_metadata(gobj, kw_request, jn_response, TRUE);
    return jn_answer;
}

/***************************************************************************
 *
 ***************************************************************************/
PUBLIC json_t *msg_iev_build_response_without_reverse_dst( // OLD msg_iev_build_webix2_without_answer_filter()
    hgobj gobj,
    json_int_t result,
    json_t *jn_comment, // owned
    json_t *jn_schema,  // owned
    json_t *jn_data,    // owned
    json_t *kw_request  // owned, used to get ONLY __md_iev__.
) {
    json_t *jn_response = build_command_response(gobj, result, jn_comment, jn_schema, jn_data);
    json_t *jn_answer = msg_iev_set_back_metadata(gobj, kw_request, jn_response, FALSE);
    return jn_answer;
}

/***************************************************************************
 *  Clean next metadata
 *      __md_iev__
 *      __temp__
 *      __md_tranger__
 *      __md_yuno__
 *  TODO shouldn't everything "__" be deleted?
 ***************************************************************************/
PUBLIC json_t *msg_iev_clean_metadata( // return the same kw, OLD ~ msg_iev_pure_clone()
    json_t *kw // not owned
) {
    json_object_del(kw, "__md_iev__");
    json_object_del(kw, "__temp__");
    json_object_del(kw, "__md_tranger__");
    json_object_del(kw, "__md_yuno__");
    return kw;
}

/***************************************************************************
 *
 ***************************************************************************/
PUBLIC int msg_iev_set_msg_type(
    hgobj gobj,
    json_t *kw,
    const char *msg_type // empty string to delete the key
)
{
    if(!empty_string(msg_type)) {
        kw_set_subdict_value(gobj, kw, "__md_iev__", "__msg_type__", json_string(msg_type));
    } else {
        kw_delete(gobj, kw, "__md_iev__`__msg_type__");
    }
    return 0;
}

/***************************************************************************
 *
 ***************************************************************************/
PUBLIC const char *msg_iev_get_msg_type(
    hgobj gobj,
    json_t *kw
)
{
    return kw_get_str(gobj, kw, "__md_iev__`__msg_type__", "", 0);
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
