/****************************************************************************
 *          C_TEST_RUNNER.C
 *
 *          Generic JSON-driven test runner GClass.
 *
 *          Executes a script of actions defined in a JSON test file:
 *            - start_tree:   start a service tree (gobj_start_tree)
 *            - stop_tree:    stop a service tree (gobj_stop_tree)
 *            - send_event:   send an event directly (gobj_send_event)
 *            - send_iev:     send an inter-event (iev_create + EV_SEND_IEV)
 *            - wait_event:   wait for a specific event from a service
 *            - wait_timeout: wait N milliseconds
 *            - subscribe:    subscribe to events from a service
 *            - check_state:  verify FSM state of a service
 *
 *          Copyright (c) 2026, ArtGins.
 *          All Rights Reserved.
 ****************************************************************************/
#include "c_test_runner.h"

/***************************************************************************
 *              Constants
 ***************************************************************************/
#define MAX_EVENT_TRACE     256

/***************************************************************************
 *              Structures
 ***************************************************************************/

/***************************************************************************
 *              Prototypes
 ***************************************************************************/
PRIVATE int execute_next_step(hgobj gobj);
PRIVATE gobj_event_t resolve_event(hgobj gobj, const char *event_name);

/***************************************************************************
 *          Data: config, public data, private data
 ***************************************************************************/
PRIVATE sdata_desc_t attrs_table[] = {
/*-ATTR-type----------name-----------flag----default-----description---------*/
SDATA (DTP_JSON,      "script",      SDF_RD, "[]",       "Script steps array"),
SDATA (DTP_JSON,      "expected_event_trace", SDF_RD, "[]", "Expected event trace"),
SDATA (DTP_INTEGER,   "step_timeout",SDF_RD, "100",      "Default ms between steps"),
SDATA (DTP_POINTER,   "user_data",   0,      0,          "user data"),
SDATA (DTP_POINTER,   "subscriber",  0,      0,          "subscriber of output-events"),
SDATA_END()
};

/*---------------------------------------------*
 *      GClass trace levels
 *---------------------------------------------*/
enum {
    TRACE_MESSAGES = 0x0001,
};
PRIVATE const trace_level_t s_user_trace_level[16] = {
    {"messages",    "Trace messages"},
    {0, 0},
};

/*---------------------------------------------*
 *              Private data
 *---------------------------------------------*/
typedef struct _PRIVATE_DATA {
    hgobj timer;

    json_t *script;             // borrowed from attr
    int current_step;
    int total_steps;

    // For wait_event
    BOOL waiting_for_event;
    const char *wait_event_name;        // event name we're waiting for
    gobj_event_t wait_event;            // resolved event pointer
    json_t *wait_expected_kw;           // expected kw to validate (borrowed)
    hgobj wait_source;                  // service we're waiting on
    int wait_timeout_ms;

    // Event trace recording
    json_t *event_trace;                // array of recorded events
    json_t *expected_trace;             // borrowed from attr

    // Track subscribed services to avoid duplicate subscriptions
    json_t *subscribed_services;        // set of service names (json object as set)

    // Index into event_trace: events before this index have been consumed by wait_event
    size_t trace_consumed_idx;

    int test_result;
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
    priv->event_trace = json_array();
    priv->subscribed_services = json_object();
}

/***************************************************************************
 *      Framework Method start
 ***************************************************************************/
PRIVATE int mt_start(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    gobj_start(priv->timer);
    return 0;
}

/***************************************************************************
 *      Framework Method stop
 ***************************************************************************/
PRIVATE int mt_stop(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    clear_timeout(priv->timer);
    gobj_stop(priv->timer);
    return 0;
}

/***************************************************************************
 *      Framework Method play
 ***************************************************************************/
PRIVATE int mt_play(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    priv->script = gobj_read_json_attr(gobj, "script");
    priv->expected_trace = gobj_read_json_attr(gobj, "expected_event_trace");
    priv->total_steps = (int)json_array_size(priv->script);
    priv->current_step = 0;

    if(priv->total_steps == 0) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_PARAMETER_ERROR,
            "msg",          "%s", "Empty script, nothing to test",
            NULL
        );
        priv->test_result = -1;
        set_yuno_must_die();
        return 0;
    }

    // Start executing the first step after a short delay
    set_timeout(priv->timer, 100);

    return 0;
}

/***************************************************************************
 *      Framework Method pause
 ***************************************************************************/
PRIVATE int mt_pause(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    clear_timeout(priv->timer);
    return 0;
}

/***************************************************************************
 *      Framework Method destroy
 ***************************************************************************/
PRIVATE void mt_destroy(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    JSON_DECREF(priv->event_trace)
    JSON_DECREF(priv->subscribed_services)
}


                    /***************************
                     *      Local Methods
                     ***************************/


/***************************************************************************
 *  Resolve an event name string to the global gobj_event_t pointer
 ***************************************************************************/
PRIVATE gobj_event_t resolve_event(hgobj gobj, const char *event_name)
{
    if(!event_name || strlen(event_name) == 0) {
        return NULL;
    }

    event_type_t *ev_type = gobj_find_event_type(event_name, 0, FALSE);
    if(ev_type) {
        return ev_type->event_name;
    }

    // Fallback: return the string itself (for dynamically registered events)
    return event_name;
}

/***************************************************************************
 *  Record an event in the trace
 ***************************************************************************/
PRIVATE void record_event(hgobj gobj, const char *event, json_t *kw, hgobj src)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    json_t *record = json_object();
    json_object_set_new(record, "event", json_string(event));
    if(src) {
        json_object_set_new(record, "source", json_string(gobj_name(src)));
    }

    // Record relevant kw fields (skip internal metadata)
    if(kw) {
        const char *key;
        json_t *value;
        json_object_foreach(kw, key, value) {
            if(key[0] != '_' || key[1] != '_') { // skip __metadata__
                if(json_is_string(value) || json_is_integer(value) ||
                   json_is_real(value) || json_is_boolean(value)) {
                    json_object_set(record, key, value);
                }
            }
        }
    }

    json_array_append_new(priv->event_trace, record);
}

/***************************************************************************
 *  Validate the event trace against expected trace
 ***************************************************************************/
PRIVATE int validate_event_trace(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    if(!priv->expected_trace || json_array_size(priv->expected_trace) == 0) {
        return 0; // No trace validation requested
    }

    size_t expected_size = json_array_size(priv->expected_trace);
    size_t found_idx = 0;

    // Each expected event must appear in order in the recorded trace
    // (but there can be extra events between them)
    for(size_t i = 0; i < expected_size; i++) {
        json_t *expected_ev = json_array_get(priv->expected_trace, i);
        const char *exp_event = json_string_value(json_object_get(expected_ev, "event"));
        const char *exp_source = json_string_value(json_object_get(expected_ev, "source"));
        json_t *exp_kw = json_object_get(expected_ev, "kw");

        BOOL found = FALSE;
        while(found_idx < json_array_size(priv->event_trace)) {
            json_t *recorded = json_array_get(priv->event_trace, found_idx);
            found_idx++;

            const char *rec_event = json_string_value(json_object_get(recorded, "event"));
            const char *rec_source = json_string_value(json_object_get(recorded, "source"));

            // Match event name
            if(!rec_event || !exp_event || strcmp(rec_event, exp_event) != 0) {
                continue;
            }

            // Match source if specified
            if(exp_source && rec_source && strcmp(exp_source, rec_source) != 0) {
                continue;
            }

            // Match kw fields if specified
            if(exp_kw && json_object_size(exp_kw) > 0) {
                json_t *filter_copy = json_deep_copy(exp_kw);
                if(!kw_match_simple(recorded, filter_copy)) {
                    // kw_match_simple owns filter_copy, already freed
                    continue;
                }
                // kw_match_simple owns filter_copy, already freed
            }

            found = TRUE;
            break;
        }

        if(!found) {
            gobj_log_error(gobj, 0,
                "function",     "%s", __FUNCTION__,
                "msgset",       "%s", MSGSET_APP_ERROR,
                "msg",          "%s", "Expected event not found in trace",
                "expected_event", "%s", exp_event ? exp_event : "?",
                "expected_source", "%s", exp_source ? exp_source : "?",
                "trace_idx",    "%d", (int)i,
                NULL
            );
            return -1;
        }
    }

    return 0;
}

/***************************************************************************
 *  Finish the test: validate traces and exit
 ***************************************************************************/
PRIVATE void finish_test(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    priv->test_result += validate_event_trace(gobj);

    if(priv->test_result == 0) {
        gobj_log_info(gobj, 0,
            "msgset",       "%s", MSGSET_INFO,
            "msg",          "%s", "Test script completed successfully",
            NULL
        );
    } else {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_APP_ERROR,
            "msg",          "%s", "Test script FAILED",
            NULL
        );
    }

    set_yuno_must_die();
}

/***************************************************************************
 *  Execute the current step in the script
 ***************************************************************************/
PRIVATE int execute_next_step(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    if(priv->current_step >= priv->total_steps) {
        finish_test(gobj);
        return 0;
    }

    json_t *step = json_array_get(priv->script, priv->current_step);
    const char *action = json_string_value(json_object_get(step, "action"));
    const char *target_name = json_string_value(json_object_get(step, "target"));
    const char *source_name = json_string_value(json_object_get(step, "source"));
    const char *comment = json_string_value(json_object_get(step, "comment"));

    if(comment && strlen(comment) > 0) {
        gobj_log_info(gobj, 0,
            "msgset",       "%s", MSGSET_INFO,
            "msg",          "%s", "Executing step",
            "step",         "%d", priv->current_step,
            "action",       "%s", action ? action : "?",
            "comment",      "%s", comment,
            NULL
        );
    }

    if(!action) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_PARAMETER_ERROR,
            "msg",          "%s", "Script step missing 'action' field",
            "step",         "%d", priv->current_step,
            NULL
        );
        priv->test_result = -1;
        finish_test(gobj);
        return -1;
    }

    /*--------------------------------------------*
     *  ACTION: start_tree
     *--------------------------------------------*/
    if(strcmp(action, "start_tree") == 0) {
        hgobj target = gobj_find_service(target_name, TRUE);
        if(!target) {
            gobj_log_error(gobj, 0,
                "function",     "%s", __FUNCTION__,
                "msgset",       "%s", MSGSET_APP_ERROR,
                "msg",          "%s", "Service not found for start_tree",
                "target",       "%s", target_name ? target_name : "?",
                "step",         "%d", priv->current_step,
                NULL
            );
            priv->test_result = -1;
            finish_test(gobj);
            return -1;
        }
        gobj_start_tree(target);

        priv->current_step++;
        set_timeout(priv->timer, gobj_read_integer_attr(gobj, "step_timeout"));
        return 0;
    }

    /*--------------------------------------------*
     *  ACTION: stop_tree
     *--------------------------------------------*/
    if(strcmp(action, "stop_tree") == 0) {
        hgobj target = gobj_find_service(target_name, TRUE);
        if(!target) {
            gobj_log_error(gobj, 0,
                "function",     "%s", __FUNCTION__,
                "msgset",       "%s", MSGSET_APP_ERROR,
                "msg",          "%s", "Service not found for stop_tree",
                "target",       "%s", target_name ? target_name : "?",
                "step",         "%d", priv->current_step,
                NULL
            );
            priv->test_result = -1;
            finish_test(gobj);
            return -1;
        }
        gobj_stop_tree(target);

        priv->current_step++;
        set_timeout(priv->timer, gobj_read_integer_attr(gobj, "step_timeout"));
        return 0;
    }

    /*--------------------------------------------*
     *  ACTION: send_event
     *--------------------------------------------*/
    if(strcmp(action, "send_event") == 0) {
        hgobj target = gobj_find_service(target_name, TRUE);
        if(!target) {
            gobj_log_error(gobj, 0,
                "function",     "%s", __FUNCTION__,
                "msgset",       "%s", MSGSET_APP_ERROR,
                "msg",          "%s", "Service not found for send_event",
                "target",       "%s", target_name ? target_name : "?",
                "step",         "%d", priv->current_step,
                NULL
            );
            priv->test_result = -1;
            finish_test(gobj);
            return -1;
        }

        const char *event_name = json_string_value(json_object_get(step, "event"));
        gobj_event_t event = resolve_event(gobj, event_name);
        if(!event) {
            gobj_log_error(gobj, 0,
                "function",     "%s", __FUNCTION__,
                "msgset",       "%s", MSGSET_APP_ERROR,
                "msg",          "%s", "Cannot resolve event for send_event",
                "event",        "%s", event_name ? event_name : "?",
                "step",         "%d", priv->current_step,
                NULL
            );
            priv->test_result = -1;
            finish_test(gobj);
            return -1;
        }

        json_t *kw_send = json_deep_copy(json_object_get(step, "kw"));
        if(!kw_send) {
            kw_send = json_object();
        }

        gobj_send_event(target, event, kw_send, gobj);

        priv->current_step++;
        set_timeout(priv->timer, gobj_read_integer_attr(gobj, "step_timeout"));
        return 0;
    }

    /*--------------------------------------------*
     *  ACTION: send_iev
     *--------------------------------------------*/
    if(strcmp(action, "send_iev") == 0) {
        hgobj target = gobj_find_service(target_name, TRUE);
        if(!target) {
            gobj_log_error(gobj, 0,
                "function",     "%s", __FUNCTION__,
                "msgset",       "%s", MSGSET_APP_ERROR,
                "msg",          "%s", "Service not found for send_iev",
                "target",       "%s", target_name ? target_name : "?",
                "step",         "%d", priv->current_step,
                NULL
            );
            priv->test_result = -1;
            finish_test(gobj);
            return -1;
        }

        const char *event_name = json_string_value(json_object_get(step, "event"));
        gobj_event_t event = resolve_event(gobj, event_name);
        if(!event) {
            gobj_log_error(gobj, 0,
                "function",     "%s", __FUNCTION__,
                "msgset",       "%s", MSGSET_APP_ERROR,
                "msg",          "%s", "Cannot resolve event for send_iev",
                "event",        "%s", event_name ? event_name : "?",
                "step",         "%d", priv->current_step,
                NULL
            );
            priv->test_result = -1;
            finish_test(gobj);
            return -1;
        }

        json_t *kw_iev_data = json_deep_copy(json_object_get(step, "kw"));
        if(!kw_iev_data) {
            kw_iev_data = json_object();
        }

        // Handle "payload" convenience field: convert to gbuffer
        json_t *jn_payload = json_object_get(kw_iev_data, "payload");
        if(json_is_string(jn_payload)) {
            const char *payload = json_string_value(jn_payload);
            size_t payloadlen = strlen(payload);
            gbuffer_t *gbuf = gbuffer_create(payloadlen, payloadlen);
            gbuffer_append_string(gbuf, payload);
            json_object_set_new(kw_iev_data, "gbuffer",
                json_integer((json_int_t)(uintptr_t)gbuf));
            json_object_del(kw_iev_data, "payload");
        }

        json_t *kw_iev = iev_create(gobj, event, kw_iev_data);
        gobj_send_event(target, EV_SEND_IEV, kw_iev, gobj);

        priv->current_step++;
        set_timeout(priv->timer, gobj_read_integer_attr(gobj, "step_timeout"));
        return 0;
    }

    /*--------------------------------------------*
     *  ACTION: wait_event
     *--------------------------------------------*/
    if(strcmp(action, "wait_event") == 0) {
        const char *svc_name = source_name ? source_name : target_name;
        hgobj source = gobj_find_service(svc_name, TRUE);
        if(!source) {
            gobj_log_error(gobj, 0,
                "function",     "%s", __FUNCTION__,
                "msgset",       "%s", MSGSET_APP_ERROR,
                "msg",          "%s", "Service not found for wait_event",
                "source",       "%s", svc_name ? svc_name : "?",
                "step",         "%d", priv->current_step,
                NULL
            );
            priv->test_result = -1;
            finish_test(gobj);
            return -1;
        }

        const char *event_name = json_string_value(json_object_get(step, "event"));
        gobj_event_t event = resolve_event(gobj, event_name);

        priv->waiting_for_event = TRUE;
        priv->wait_event_name = event_name;
        priv->wait_event = event;
        priv->wait_expected_kw = json_object_get(step, "expected_kw");
        priv->wait_source = source;

        int timeout = (int)json_integer_value(json_object_get(step, "timeout"));
        if(timeout <= 0) {
            timeout = 5;
        }
        priv->wait_timeout_ms = timeout * 1000;

        // Subscribe to all events from the source service (if not already subscribed)
        if(!json_object_get(priv->subscribed_services, svc_name)) {
            gobj_subscribe_event(source, NULL, 0, gobj);
            json_object_set_new(priv->subscribed_services, svc_name, json_true());
        }

        // Check if the event was already recorded (arrived before wait_event was set up)
        size_t trace_size = json_array_size(priv->event_trace);
        for(size_t i = priv->trace_consumed_idx; i < trace_size; i++) {
            json_t *record = json_array_get(priv->event_trace, i);
            const char *rec_event = json_string_value(json_object_get(record, "event"));
            if(rec_event && strcmp(rec_event, event_name) == 0) {
                // Event already arrived — consume it and advance
                priv->trace_consumed_idx = i + 1;
                priv->waiting_for_event = FALSE;
                priv->current_step++;
                set_timeout(priv->timer, gobj_read_integer_attr(gobj, "step_timeout"));
                return 0;
            }
        }

        // Set timeout for wait
        set_timeout(priv->timer, priv->wait_timeout_ms);

        return 0;
    }

    /*--------------------------------------------*
     *  ACTION: subscribe
     *--------------------------------------------*/
    if(strcmp(action, "subscribe") == 0) {
        hgobj target = gobj_find_service(target_name, TRUE);
        if(!target) {
            gobj_log_error(gobj, 0,
                "function",     "%s", __FUNCTION__,
                "msgset",       "%s", MSGSET_APP_ERROR,
                "msg",          "%s", "Service not found for subscribe",
                "target",       "%s", target_name ? target_name : "?",
                "step",         "%d", priv->current_step,
                NULL
            );
            priv->test_result = -1;
            finish_test(gobj);
            return -1;
        }

        gobj_subscribe_event(target, NULL, 0, gobj);
        json_object_set_new(priv->subscribed_services, target_name, json_true());

        priv->current_step++;
        set_timeout(priv->timer, gobj_read_integer_attr(gobj, "step_timeout"));
        return 0;
    }

    /*--------------------------------------------*
     *  ACTION: wait_timeout
     *--------------------------------------------*/
    if(strcmp(action, "wait_timeout") == 0) {
        int ms = (int)json_integer_value(json_object_get(step, "ms"));
        if(ms <= 0) {
            ms = 1000;
        }

        priv->current_step++;
        set_timeout(priv->timer, ms);
        return 0;
    }

    /*--------------------------------------------*
     *  ACTION: check_state
     *--------------------------------------------*/
    if(strcmp(action, "check_state") == 0) {
        hgobj target = gobj_find_service(target_name, TRUE);
        if(!target) {
            gobj_log_error(gobj, 0,
                "function",     "%s", __FUNCTION__,
                "msgset",       "%s", MSGSET_APP_ERROR,
                "msg",          "%s", "Service not found for check_state",
                "target",       "%s", target_name ? target_name : "?",
                "step",         "%d", priv->current_step,
                NULL
            );
            priv->test_result = -1;
            finish_test(gobj);
            return -1;
        }

        const char *expected_state = json_string_value(json_object_get(step, "expected_state"));
        const char *current_state = gobj_current_state(target);

        if(!expected_state || !current_state || strcmp(current_state, expected_state) != 0) {
            gobj_log_error(gobj, 0,
                "function",     "%s", __FUNCTION__,
                "msgset",       "%s", MSGSET_APP_ERROR,
                "msg",          "%s", "State check failed",
                "target",       "%s", target_name,
                "expected",     "%s", expected_state ? expected_state : "?",
                "current",      "%s", current_state ? current_state : "?",
                "step",         "%d", priv->current_step,
                NULL
            );
            priv->test_result = -1;
        }

        priv->current_step++;
        set_timeout(priv->timer, gobj_read_integer_attr(gobj, "step_timeout"));
        return 0;
    }

    /*--------------------------------------------*
     *  Unknown action
     *--------------------------------------------*/
    gobj_log_error(gobj, 0,
        "function",     "%s", __FUNCTION__,
        "msgset",       "%s", MSGSET_PARAMETER_ERROR,
        "msg",          "%s", "Unknown script action",
        "action",       "%s", action,
        "step",         "%d", priv->current_step,
        NULL
    );
    priv->test_result = -1;
    finish_test(gobj);
    return -1;
}


                    /***************************
                     *      Actions
                     ***************************/


/***************************************************************************
 *  Timer fired: execute next step or handle wait timeout
 ***************************************************************************/
PRIVATE int ac_timeout(hgobj gobj, gobj_event_t event, json_t *kw, hgobj src)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    if(priv->waiting_for_event) {
        // Timeout while waiting for an event
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_APP_ERROR,
            "msg",          "%s", "Timeout waiting for event",
            "event",        "%s", priv->wait_event_name ? priv->wait_event_name : "?",
            "step",         "%d", priv->current_step,
            NULL
        );
        priv->waiting_for_event = FALSE;
        priv->test_result = -1;
        finish_test(gobj);
    } else {
        // Normal step execution
        execute_next_step(gobj);
    }

    KW_DECREF(kw)
    return 0;
}

/***************************************************************************
 *  Handle any event received from subscribed services
 *  This is the catch-all handler for events we're waiting on
 ***************************************************************************/
PRIVATE int ac_on_event(hgobj gobj, gobj_event_t event, json_t *kw, hgobj src)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    // Record all events in the trace
    record_event(gobj, event, kw, src);

    if(priv->waiting_for_event) {
        // Check if this is the event we're waiting for
        BOOL event_matches = FALSE;

        if(priv->wait_event) {
            // Compare by pointer (canonical event) or by string
            event_matches = (event == priv->wait_event) ||
                           (strcmp(event, priv->wait_event_name) == 0);
        }

        if(event_matches) {
            // Validate expected_kw if specified
            if(priv->wait_expected_kw && json_object_size(priv->wait_expected_kw) > 0) {
                json_t *filter = json_deep_copy(priv->wait_expected_kw);

                // Handle "payload" convenience: compare against gbuffer content
                json_t *jn_exp_payload = json_object_get(filter, "payload");
                if(json_is_string(jn_exp_payload)) {
                    const char *expected_payload = json_string_value(jn_exp_payload);
                    gbuffer_t *gbuf = (gbuffer_t *)(uintptr_t)kw_get_int(
                        gobj, kw, "gbuffer", 0, 0);
                    if(gbuf) {
                        const char *actual = gbuffer_cur_rd_pointer(gbuf);
                        size_t actual_len = gbuffer_chunk(gbuf);
                        if(actual_len != strlen(expected_payload) ||
                           memcmp(actual, expected_payload, actual_len) != 0) {
                            gobj_log_error(gobj, 0,
                                "function",     "%s", __FUNCTION__,
                                "msgset",       "%s", MSGSET_APP_ERROR,
                                "msg",          "%s", "Payload mismatch in wait_event",
                                "expected",     "%s", expected_payload,
                                "step",         "%d", priv->current_step,
                                NULL
                            );
                            priv->test_result = -1;
                        }
                    } else {
                        gobj_log_error(gobj, 0,
                            "function",     "%s", __FUNCTION__,
                            "msgset",       "%s", MSGSET_APP_ERROR,
                            "msg",          "%s", "No gbuffer in event for payload check",
                            "step",         "%d", priv->current_step,
                            NULL
                        );
                        priv->test_result = -1;
                    }
                    json_object_del(filter, "payload");
                }

                // Match remaining fields
                if(json_object_size(filter) > 0) {
                    if(!kw_match_simple(kw, filter)) {
                        gobj_log_error(gobj, 0,
                            "function",     "%s", __FUNCTION__,
                            "msgset",       "%s", MSGSET_APP_ERROR,
                            "msg",          "%s", "KW mismatch in wait_event",
                            "event",        "%s", event,
                            "step",         "%d", priv->current_step,
                            NULL
                        );
                        priv->test_result = -1;
                    }
                } else {
                    JSON_DECREF(filter)
                }
            }

            // Event received, move to next step
            priv->waiting_for_event = FALSE;
            priv->wait_source = NULL;
            priv->trace_consumed_idx = json_array_size(priv->event_trace);
            clear_timeout(priv->timer);

            priv->current_step++;
            set_timeout(priv->timer, gobj_read_integer_attr(gobj, "step_timeout"));
        }
    }

    KW_DECREF(kw)
    return 0;
}

/***************************************************************************
 *  Volatil child stopped: destroy it
 ***************************************************************************/
PRIVATE int ac_stopped(hgobj gobj, gobj_event_t event, json_t *kw, hgobj src)
{
    if(gobj_is_volatil(src)) {
        gobj_destroy(src);
    }

    KW_DECREF(kw)
    return 0;
}

/***************************************************************************
 *  Catch-all: handle any event not in the FSM state table.
 *  This allows the test runner to receive any event type (MQTT, custom, etc.)
 *  without having to register them all in advance.
 ***************************************************************************/
PRIVATE int mt_inject_event(hgobj gobj, gobj_event_t event, json_t *kw, hgobj src)
{
    return ac_on_event(gobj, event, kw, src);
}


                    /***************************
                     *      FSM
                     ***************************/


/***************************************************************************
 *
 ***************************************************************************/
PRIVATE const GMETHODS gmt = {
    .mt_create  = mt_create,
    .mt_start   = mt_start,
    .mt_stop    = mt_stop,
    .mt_play    = mt_play,
    .mt_pause   = mt_pause,
    .mt_destroy = mt_destroy,
    .mt_inject_event = mt_inject_event,
};

/*------------------------*
 *      GClass name
 *------------------------*/
GOBJ_DEFINE_GCLASS(C_TEST_RUNNER);

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

    /*----------------------------------------*
     *          Define States
     *----------------------------------------*/
    ev_action_t st_idle[] = {
        {EV_TIMEOUT,            ac_timeout,         0},
        {EV_ON_OPEN,            ac_on_event,        0},
        {EV_ON_CLOSE,           ac_on_event,        0},
        {EV_ON_MESSAGE,         ac_on_event,        0},
        {EV_STOPPED,            ac_stopped,         0},
        {0, 0, 0}
    };

    states_t states[] = {
        {ST_IDLE,               st_idle},
        {0, 0}
    };

    event_type_t event_types[] = {
        {EV_TIMEOUT,            0},
        {EV_ON_OPEN,            0},
        {EV_ON_CLOSE,           0},
        {EV_ON_MESSAGE,         0},
        {EV_STOPPED,            0},
        {0, 0}
    };

    /*----------------------------------------*
     *          Create the gclass
     *----------------------------------------*/
    __gclass__ = gclass_create(
        gclass_name,
        event_types,
        states,
        &gmt,
        0,              // lmt
        attrs_table,
        sizeof(PRIVATE_DATA),
        0,              // authz_table
        0,              // command_table
        s_user_trace_level,
        0               // gcflag_t
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
PUBLIC int register_c_test_runner(void)
{
    return create_gclass(C_TEST_RUNNER);
}
