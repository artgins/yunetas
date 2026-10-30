/***********************************************************************
 *          MSG_IEVENT.C
 *
 *          Common of Inter-Events messages
 *
 *          Copyright (c) 2016,2023 Niyamaka.
 *          Copyright (c) 2024-2026, ArtGins.
 *          All Rights Reserved.
***********************************************************************/
#include <unistd.h>
#include <helpers.h>
#include "msg_ievent.h"

/****************************************************************
 *         Constants
 ****************************************************************/
#define IEV_LOG_DUMP_SIZE   (256)   // the kw of a refused frame shown in a log, capped

/****************************************************************
 *         Structures
 ****************************************************************/

/****************************************************************
 *         Prototypes
 ****************************************************************/
PRIVATE void log_kw_without_routing(
    hgobj gobj,
    const char *function,
    const char *msg,
    const char *stack,
    json_t *kw
);

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
    json_t *kw // like owned, return same kw
)
{
    if(empty_string(event)) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_PARAMETER,
            "msg",          "%s", "event NULL",
            NULL
        );
        KW_DECREF(kw)
        return 0;
    }

    if(!kw) {
        gobj_log_error(gobj, LOG_OPT_TRACE_STACK,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_PARAMETER,
            "msg",          "%s", "kw NULL",
            NULL
        );
        KW_DECREF(kw)
        return 0;
    }
    json_object_set_new(kw, "__iev_event__", json_integer((json_int_t)(uintptr_t)event));
    return kw;
}

/***************************************************************************
 *
 ***************************************************************************/
PUBLIC json_t *iev_create2( // For use within Yuno
    hgobj gobj,
    gobj_event_t event,
    json_t *kw, // like owned, return same kw
    json_t *kw_request  // owned, used to get ONLY __temp__.
)
{
    json_t* iev = iev_create(
        gobj,
        event,
        kw // like owned, return same kw
    );
    if(!iev) {
        KW_DECREF(kw_request)
        return NULL;
    }
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
    json_t *kw // like owned, return same kw
) {
    if(empty_string(event)) {
        gobj_log_error(gobj, LOG_OPT_TRACE_STACK,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_PARAMETER,
            "msg",          "%s", "event NULL",
            NULL
        );
        KW_DECREF(kw)
        return 0;
    }

    if(!kw) {
        kw = json_object();
    }
    kw = kw_serialize(
        gobj,
        kw  // owned
    );
    if(!kw) {
        gobj_log_error(gobj, LOG_OPT_TRACE_STACK,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_PARAMETER,
            "msg",          "%s", "kw_serialize() FAILED",
            NULL
        );
        return 0;
    }

    json_t *jn_iev = json_pack("{s:s, s:o}",
        "event", event,
        "kw", kw
    );
    size_t flags = JSON_COMPACT;
    return json2gbuf(0, jn_iev, flags);
}

/***************************************************************************
 *  The kw of a refused frame as a log shows it: masked (a credential is
 *  never written, json_mask_secrets() on a budget) and capped, as
 *  C_IEVENT_SRV's peer_kw_dump(). Return the bytes written.
 ***************************************************************************/
PRIVATE size_t iev_kw_dump(json_t *kw, char *bf, size_t bfsize)
{
    memset(bf, 0, bfsize);  // json_dumpb() skips the chunk that does not fit
    json_t *kw_shown = json_mask_secrets_capped(kw, 4*bfsize);
    if(!kw_shown) {
        return 0;   // Error already logged
    }
    json_dumpb(kw_shown, bf, bfsize-1, JSON_COMPACT|JSON_ENCODE_ANY);
    JSON_DECREF(kw_shown)
    return strlen(bf);
}

/***************************************************************************
 *  The peer of `gobj`, for a log: the peername of its bottom, if any
 ***************************************************************************/
PRIVATE const char *iev_peername(hgobj gobj)
{
    const char *peername = NULL;
    if(gobj && gobj_has_bottom_attr(gobj, "peername")) {
        peername = gobj_read_str_attr(gobj, "peername");
    }
    return peername? peername : "";
}

/***************************************************************************
 *  A kw without the routing an inter-event carries (its __md_iev__, or a
 *  stack of it): the peer sent it so. A WARNING, its kw masked and capped.
 ***************************************************************************/
PRIVATE void log_kw_without_routing(
    hgobj gobj,
    const char *function,
    const char *msg,
    const char *stack,
    json_t *kw
)
{
    char dump[IEV_LOG_DUMP_SIZE];
    iev_kw_dump(kw, dump, sizeof(dump));
    gobj_log_warning(gobj, 0,
        "function",     "%s", function,
        "msgset",       "%s", MSGSET_PROTOCOL,
        "msg",          "%s", msg,
        "stack",        "%s", stack,
        "peername",     "%s", iev_peername(gobj),
        "kw",           "%s", dump,
        NULL
    );
}

/***************************************************************************
 *  Incorporate event's messages FROM outside world.
 *  gbuf decref
 *
 *  What a frame gets wrong is the PEER's: one log line, a WARNING here,
 *  saying what it got wrong. With `verbose`, a frame that is not json is
 *  "frame is not json" (gbuf2json_from_peer(): its first bytes dumped,
 *  masked), and one that is not an inter-event (no "event" string, no "kw"
 *  dict) says so, its json masked and capped; with 0 the caller says both.
 *  C_IEVENT_SRV and C_IEVENT_CLI pass 1: they said "Bad json" for both, a
 *  frame of json among them, and dumped its bytes raw, where a JWT cut by
 *  the window showed its header. An event no gclass has
 *  as public is said HERE, whatever `verbose`: only here is the name the
 *  frame gave known -- masked and then capped, as the kw (a
 *  password or a JWT sent as the NAME was written in clear; capped first, a
 *  token that the cap cut showed its first bytes). `frame_size` is the bytes
 *  received; the kw_size up to 7.26.9 was the size of the masked, capped
 *  copy (a kw of 900 KB said 101). Up to 7.26.9 that lookup was silent and
 *  both callers traced the whole kw raw, credentials before the session
 *  among it; and a frame without "event" logged two ERRORs here and the
 *  caller's WARNING.
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
    size_t frame_size = gbuf? gbuffer_leftbytes(gbuf) : 0;
    json_t *jn_msg = verbose?
        gbuf2json_from_peer(gobj, gbuf, gobj) :     // gbuf stolen: decref and data consumed
        gbuf2json(gbuf, 0);
    if(!jn_msg) {
        /*
         *  Said by gbuf2json_from_peer() with `verbose`, a WARNING: a peer
         *  sending bad json is its business, not an internal invariant of
         *  ours. With 0, by the caller.
         */
        return NULL;
    }

    /*
     *  Get fields
     */
    const char *event_ = json_string_value(json_object_get(jn_msg, "event"));
    json_t *kw = json_object_get(jn_msg, "kw");
    if(empty_string(event_) || !json_is_object(kw)) {
        if(verbose) {
            char dump[IEV_LOG_DUMP_SIZE];
            iev_kw_dump(jn_msg, dump, sizeof(dump));
            gobj_log_warning(gobj, 0,
                "function",     "%s", __FUNCTION__,
                "msgset",       "%s", MSGSET_PROTOCOL,
                "msg",          "%s", "Frame is not an inter-event (an \"event\" string and a \"kw\" dict), refused",
                "peername",     "%s", iev_peername(gobj),
                "frame",        "%s", dump,
                "frame_size",   "%lu", (unsigned long)frame_size,
                NULL
            );
        }
        JSON_DECREF(jn_msg)
        return NULL;
    }

    /*
     *  Inter-event from world outside, deserialize!
     */
    json_incref(kw);
    json_t *new_kw = kw_deserialize(gobj, kw);

    if(event) {
        *event = gclass_find_public_event(event_, FALSE);
        if(!*event) {
            char dump[IEV_LOG_DUMP_SIZE];
            iev_kw_dump(new_kw, dump, sizeof(dump));
            char name[65];
            mask_secrets_inline_capped(name, sizeof(name), event_, strlen(event_));
            gobj_log_warning(gobj, 0,
                "function",     "%s", __FUNCTION__,
                "msgset",       "%s", MSGSET_PROTOCOL,
                "msg",          "%s", "Frame of an event that is not public, refused",
                "event",        "%s", name,
                "peername",     "%s", iev_peername(gobj),
                "kw",           "%s", dump,
                "frame_size",   "%lu", (unsigned long)frame_size,
                NULL
            );
        }
    }
    JSON_DECREF(jn_msg)

    return new_kw;
}

/***************************************************************************
 *  The json type, as a word for a log line
 ***************************************************************************/
PRIVATE const char *json_kind(json_t *jn)
{
    switch(json_typeof(jn)) {
        case JSON_OBJECT:   return "dict";
        case JSON_ARRAY:    return "list";
        case JSON_STRING:   return "string";
        case JSON_INTEGER:  return "integer";
        case JSON_REAL:     return "real";
        case JSON_TRUE:     return "true";
        case JSON_FALSE:    return "false";
        case JSON_NULL:     return "null";
        default:            return "?";
    }
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
            "msgset",       "%s", MSGSET_INTERNAL,
            "msg",          "%s", "kw is not a dict",
            NULL
        );
        JSON_DECREF(jn_data)
        return -1;
    }
    /*
     *  `__md_iev__` and its stacks can come in what a PEER sent: one of the
     *  wrong type is a warning (protocol data), not an internal error, and
     *  it is REPLACED, said once, so the entry is pushed and the next push
     *  of the same message finds a good one (no warning per push). What is
     *  logged is its type and size, and a prefix of a string: a dump of the
     *  whole value cost a peer's megabytes per message.
     */
    json_t *md_iev = json_object_get(kw, "__md_iev__");
    if(md_iev && !json_is_object(md_iev) && !json_is_null(md_iev)) {
        gobj_log_warning(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_PROTOCOL,
            "msg",          "%s", "__md_iev__ is not a dict, replaced by an empty one",
            "type",         "%s", json_kind(md_iev),
            "size",         "%zu", json_is_string(md_iev)? json_string_length(md_iev) : json_size(md_iev),
            "value",        "%.64s", json_is_string(md_iev)? json_string_value(md_iev) : "",
            NULL
        );
        json_object_del(kw, "__md_iev__");
    }
    md_iev = kw_get_dict(gobj, kw, "__md_iev__", json_object(), KW_CREATE);
    if(!md_iev) {
        JSON_DECREF(jn_data)
        return -1;  // Error already logged
    }
    json_t *jn_stack = json_object_get(md_iev, stack);
    if(jn_stack && !json_is_array(jn_stack) && !json_is_null(jn_stack)) {
        gobj_log_warning(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_PROTOCOL,
            "msg",          "%s", "a stack of __md_iev__ is not a list, replaced by an empty one",
            "stack",        "%s", stack,
            "type",         "%s", json_kind(jn_stack),
            "size",         "%zu", json_is_string(jn_stack)? json_string_length(jn_stack) : json_size(jn_stack),
            "value",        "%.64s", json_is_string(jn_stack)? json_string_value(jn_stack) : "",
            NULL
        );
    }
    if(!json_is_array(jn_stack)) {
        jn_stack = json_array();
        if(json_object_set_new(md_iev, stack, jn_stack) < 0) {
            gobj_log_error(gobj, LOG_OPT_TRACE_STACK,
                "function",     "%s", __FUNCTION__,
                "msgset",       "%s", MSGSET_INTERNAL,
                "msg",          "%s", "cannot add the stack",
                "stack",        "%s", stack,
                NULL
            );
            JSON_DECREF(jn_data)
            return -1;
        }
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
            "msgset",       "%s", MSGSET_INTERNAL,
            "msg",          "%s", "kw is not a dict",
            NULL
        );
        return NULL;
    }
    json_t *md_iev = kw_get_dict(gobj, kw, "__md_iev__", 0, 0);
    if(!md_iev) {
        if(verbose) {
            log_kw_without_routing(gobj, __FUNCTION__, "__md_iev__ NOT FOUND", stack, kw);
        }
        return NULL;
    }

    json_t *jn_stack = kw_get_list(gobj, md_iev, stack, 0, 0);
    if(!jn_stack) {
        if(verbose) {
            log_kw_without_routing(gobj, __FUNCTION__, "__md_iev__ stack NOT FOUND", stack, kw);
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
            "msgset",       "%s", MSGSET_INTERNAL,
            "msg",          "%s", "kw is not a dict",
            NULL
        );
        return NULL;
    }
    json_t *md_iev = kw_get_dict(gobj, kw, "__md_iev__", 0, 0);
    if(!md_iev) {
        log_kw_without_routing(gobj, __FUNCTION__, "__md_iev__ NOT FOUND", stack, kw);
        return NULL;
    }

    json_t *jn_stack = kw_get_list(gobj, md_iev, stack, 0, 0);
    if(!jn_stack) {
        log_kw_without_routing(gobj, __FUNCTION__, "__md_iev__ stack NOT FOUND", stack, kw);
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
 *  The kw of an inter-event as a trace shows it. A command (v6: its
 *  "__command__" string; v7: the "command" of the "__command__" stack of
 *  __md_iev__) is masked by the command table of its destination service
 *  when that service is here (the SDF_SECRET parameters, positional ones
 *  too), and everything by names (json_mask_secrets(): keys with a
 *  secret's name, "name=value" secrets in a string) -- the table of a
 *  remote service is not known here. Return a new reference.
 ***************************************************************************/
PRIVATE json_t *ievent_kw_masked(json_t *kw)
{
    json_t *md_iev = json_object_get(kw, "__md_iev__");
    json_t *jn_command_stack = json_array_get(json_object_get(md_iev, "__command__"), 0);
    const char *command = json_string_value(json_object_get(kw, "__command__"));
    if(!command) {
        command = json_string_value(json_object_get(jn_command_stack, "command"));
    }
    if(!command) {
        return json_mask_secrets(kw);
    }

    const char *dst_service = json_string_value(json_object_get(
        json_array_get(json_object_get(md_iev, "ievent_gate_stack"), 0), "dst_service"
    ));
    hgobj service = empty_string(dst_service)? NULL : gobj_find_service(dst_service, FALSE);

    json_t *kw_masked = command_mask_secret_kw(service, command, kw);
    char *line = command_mask_secret_line(service, command);
    json_t *kw_shown = json_deep_copy(kw_masked);
    JSON_DECREF(kw_masked)
    if(json_is_string(json_object_get(kw_shown, "__command__"))) {
        json_object_set_new(kw_shown, "__command__", json_string(line?line:""));
    } else {
        json_t *stack = json_array_get(
            json_object_get(json_object_get(kw_shown, "__md_iev__"), "__command__"), 0
        );
        if(json_is_object(stack)) {
            json_object_set_new(stack, "command", json_string(line?line:""));
        }
    }
    GBMEM_FREE(line)
    return kw_shown;
}

/***************************************************************************
 *  What the ievents trace shows: result and __md_iev__ of the kw, the
 *  secrets masked (ievent_kw_masked()). Up to 7.25.21 they were masked by
 *  names only, and a positional secret of a v7 command (in __md_iev__)
 *  went in clear.
 ***************************************************************************/
PUBLIC void trace_inter_event(hgobj gobj, const char *prefix, const char *event, json_t *kw)
{
    json_t *kw_shown = ievent_kw_masked(kw);
    if(!kw_shown) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_INTERNAL,
            "msg",          "%s", "ievent_kw_masked() FAILED: the inter-event is not traced",
            "event",        "%s", event?event:"???",
            NULL
        );
        return;
    }

    json_t *kw_compact = json_object();
    if(kw_has_key(kw_shown, "result")) {
        json_object_set(kw_compact, "result", kw_get_dict_value(gobj, kw_shown, "result", 0, 0));
    }
    if(kw_has_key(kw_shown, "__md_iev__")) {
        json_object_set(kw_compact, "__md_iev__", kw_get_dict_value(gobj, kw_shown, "__md_iev__", 0, 0));
    }
    json_decref(kw_shown);

    json_t *jn_iev = json_pack("{s:s, s:o}",
        "event", event?event:"???",
        "kw", kw_compact
    );
    if(!jn_iev) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_MEMORY,
            "msg",          "%s", "json_pack() FAILED: the inter-event is not traced",
            "event",        "%s", event?event:"???",
            NULL
        );
        return;
    }

    gobj_trace_json(gobj, jn_iev, "%s", prefix);
    json_decref(jn_iev);
}

/***************************************************************************
 *  What the ievents2 trace shows: the whole kw, the secrets masked
 *  (ievent_kw_masked()). Up to 7.25.20 a command's password went in clear.
 ***************************************************************************/
PUBLIC void trace_inter_event2(hgobj gobj, const char *prefix, const char *event, json_t *kw)
{
    json_t *kw_shown = ievent_kw_masked(kw);
    if(!kw_shown) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_INTERNAL,
            "msg",          "%s", "ievent_kw_masked() FAILED: the inter-event is not traced",
            "event",        "%s", event?event:"???",
            NULL
        );
        return;
    }
    json_t *jn_iev = json_pack("{s:s, s:o}",
        "event", event?event:"???",
        "kw", kw_shown
    );
    if(!jn_iev) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_MEMORY,
            "msg",          "%s", "json_pack() FAILED: the inter-event is not traced",
            "event",        "%s", event?event:"???",
            NULL
        );
        return;
    }

    gobj_trace_json(gobj, jn_iev, "%s", prefix);
    json_decref(jn_iev);
}
