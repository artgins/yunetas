/****************************************************************************
 *          test_watch_request.c
 *
 *          What the agent takes from a watch-yuno-stats request:
 *          yunos/c/yuno_agent/src/watch_request.c, compiled into this test.
 *
 *          Cases:
 *          1. the requester is told by the hop the agent can trust: its own
 *             input channel when direct, the control center's hop (with its
 *             `cc_connection`) when relayed. Up to 7.25.20 it was the LAST
 *             hop of the stack, which a client sends as it likes: a forged
 *             extra hop named the watch of another user, to stop or
 *             replace it;
 *          2. a requester that does not say it takes EV_YUNO_STATS
 *             (`__relays__`) is refused, directly connected too: up to
 *             7.25.20 a direct `ycommand -c 'watch-yuno-stats ids=x'` was
 *             pushed events it does not know until the watch expired;
 *          3. more yuno ids than `max_ids` are refused.
 *
 *          Copyright (c) 2026, ArtGins.
 *          All Rights Reserved.
 ****************************************************************************/
#include <stdio.h>
#include <string.h>
#include <limits.h>
#include <yunetas.h>
#include "watch_request.h"

#define APP     "test_watch_request"

PRIVATE int global_result = 0;

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE void check(BOOL ok, const char *name)
{
    if(ok) {
        printf("ok   %s\n", name);
    } else {
        printf("FAIL %s\n", name);
        global_result += -1;
    }
}

/***************************************************************************
 *  A hop of the ievent stack
 ***************************************************************************/
PRIVATE json_t *hop(const char *src_yuno, const char *host, const char *input_channel, int cc_connection)
{
    json_t *jn_hop = json_pack("{s:s, s:s, s:s, s:s}",
        "src_yuno", src_yuno,
        "src_service", "agent_link",
        "host", host,
        "input_channel", input_channel
    );
    if(cc_connection > 0) {
        json_object_set_new(jn_hop, "cc_connection", json_integer(cc_connection));
    }
    return jn_hop;
}

/***************************************************************************
 *  A request as the agent gets it, the hops newest first (index 0 is the
 *  hop of the agent's own input channel)
 ***************************************************************************/
PRIVATE json_t *request(json_t *hop0, json_t *hop1, json_t *hop2)
{
    json_t *jn_stack = json_array();
    json_array_append_new(jn_stack, hop0);
    if(hop1) {
        json_array_append_new(jn_stack, hop1);
    }
    if(hop2) {
        json_array_append_new(jn_stack, hop2);
    }
    return json_pack("{s:{s:o}}", "__md_iev__", "ievent_gate_stack", jn_stack);
}

/***************************************************************************
 *  1. The requester is told by a hop the agent can trust
 ***************************************************************************/
PRIVATE void test_route_name(void)
{
    char a[PATH_MAX], b[PATH_MAX], forged[PATH_MAX];

    /*
     *  Relayed: two web clients of one control center (one channel of
     *  the agent), each in its own channel of the control center.
     *  B adds a hop naming A as the far end.
     */
    json_t *kw_a = request(
        hop("artgins.com", "cc", "controlcenter", 0),
        hop("gui_agent_yuno", "browser-a", "top-1", 7),
        NULL
    );
    json_t *kw_b = request(
        hop("artgins.com", "cc", "controlcenter", 0),
        hop("gui_agent_yuno", "browser-b", "top-2", 9),
        NULL
    );
    json_t *kw_b_forged = request(
        hop("artgins.com", "cc", "controlcenter", 0),
        hop("gui_agent_yuno", "browser-b", "top-2", 9),
        hop("gui_agent_yuno", "browser-a", "top-1", 7)
    );
    watch_route_name(0, a, sizeof(a), "controlcenter", "controlcenter", kw_a, TRUE);
    watch_route_name(0, b, sizeof(b), "controlcenter", "controlcenter", kw_b, TRUE);
    watch_route_name(0, forged, sizeof(forged), "controlcenter", "controlcenter", kw_b_forged, TRUE);
    check(strcmp(a, b) != 0, "(route) relayed: two web clients have two watches");
    check(strcmp(forged, a) != 0, "(route) relayed: a forged extra hop does not name the watch of another client");
    check(strcmp(forged, b) == 0, "(route) relayed: a forged extra hop is ignored, the watch is the sender's own");
    check(strstr(a, "^7") != NULL, "(route) relayed: the connection of the control center is in the name");
    JSON_DECREF(kw_a)
    JSON_DECREF(kw_b)
    JSON_DECREF(kw_b_forged)

    /*
     *  Direct: the hop of the agent's own input channel
     */
    json_t *kw_direct = request(hop("ycommand", "host-a", "input-3", 0), NULL, NULL);
    json_t *kw_direct_forged = request(
        hop("ycommand", "host-a", "input-3", 0),
        hop("other", "host-b", "input-4", 0),
        NULL
    );
    watch_route_name(0, a, sizeof(a), "__input_side__", "input-3", kw_direct, FALSE);
    watch_route_name(0, forged, sizeof(forged), "__input_side__", "input-3", kw_direct_forged, FALSE);
    check(strcmp(a, forged) == 0, "(route) direct: a forged extra hop is ignored");
    check(strstr(a, "host-b") == NULL && strstr(forged, "host-b") == NULL,
        "(route) direct: nothing of a deeper hop is in the name");
    JSON_DECREF(kw_direct)
    JSON_DECREF(kw_direct_forged)
}

/***************************************************************************
 *  2. Only a requester that says it takes the event is sent it
 ***************************************************************************/
PRIVATE BOOL refused(json_t *kw, BOOL relayed)
{
    json_t *jn_comment = watch_refusal(kw, EV_YUNO_STATS, relayed);
    BOOL refused = (jn_comment && strstr(json_string_value(jn_comment), "stats-yuno"))? TRUE : FALSE;
    JSON_DECREF(jn_comment)
    return refused;
}

PRIVATE void test_refusal(void)
{
    json_t *kw = json_object();
    check(refused(kw, TRUE),
        "(refusal) relayed, no __relays__: refused");
    check(refused(kw, FALSE),
        "(refusal) direct (ycommand), no __relays__: refused");
    json_object_set_new(kw, "__relays__", json_pack("[s]", EV_YUNO_STATS));
    check(!refused(kw, TRUE),
        "(refusal) relayed, __relays__ with EV_YUNO_STATS: accepted");
    check(!refused(kw, FALSE),
        "(refusal) direct, __relays__ with EV_YUNO_STATS: accepted");
    json_object_set_new(kw, "__relays__", json_string(EV_YUNO_STATS));
    check(refused(kw, FALSE),
        "(refusal) direct, __relays__ not a list: refused");
    JSON_DECREF(kw)
}

/***************************************************************************
 *  3. A cap on the ids of one watch
 ***************************************************************************/
PRIVATE void test_ids(void)
{
    json_t *jn_comment = NULL;
    json_t *jn_yunos = watch_ids("a,b:db,b:app,b:db", 4, &jn_comment);
    check(jn_yunos && json_object_size(jn_yunos) == 2 &&
        json_array_size(json_object_get(jn_yunos, "b")) == 2 && !jn_comment,
        "(ids) 4 names, 2 yunos, b read through 2 services: accepted");
    JSON_DECREF(jn_yunos)

    char ids[4096] = "";
    for(int i=0; i<300; i++) {
        char id[16];
        snprintf(id, sizeof(id), "%s%d", i? ",": "", i);
        strcat(ids, id);
    }
    jn_yunos = watch_ids(ids, 256, &jn_comment);
    check(!jn_yunos && jn_comment && strstr(json_string_value(jn_comment), "too many") != NULL,
        "(ids) 300 names over a cap of 256: refused, said so");
    JSON_DECREF(jn_yunos)
    JSON_DECREF(jn_comment)
}

/***************************************************************************
 *                      Main
 ***************************************************************************/
int main(int argc, char *argv[])
{
    sys_malloc_fn_t malloc_func;
    sys_realloc_fn_t realloc_func;
    sys_calloc_fn_t calloc_func;
    sys_free_fn_t free_func;

    gbmem_get_allocators(
        &malloc_func,
        &realloc_func,
        &calloc_func,
        &free_func
    );

    json_set_alloc_funcs(
        malloc_func,
        free_func
    );

    unsigned long memory_check_list[] = {0}; // WARNING: list ended with 0
    set_memory_check_list(memory_check_list);

    init_backtrace_with_backtrace(argv[0]);
    set_show_backtrace_fn(show_backtrace_with_backtrace);

    gobj_start_up(
        argc,
        argv,
        NULL,   // jn_global_settings
        NULL,   // persistent_attrs
        NULL,   // global_command_parser
        NULL,   // global_stats_parser
        NULL,   // global_authz_checker
        NULL    // global_authentication_parser
    );

    gobj_log_add_handler("stdout", "stdout", LOG_OPT_UP_WARNING, 0);

    test_route_name();
    test_refusal();
    test_ids();

    gobj_end();

    int result = global_result;
    if(get_cur_system_memory()!=0) {
        printf("%sERROR --> %s%s\n", On_Red BWhite, "system memory not free", Color_Off);
        print_track_mem();
        result += -1;
    }
    if(result<0) {
        printf("<-- %sTEST FAILED%s: %s\n", On_Red BWhite, Color_Off, APP);
    } else {
        printf("\n%s: PASS\n", APP);
    }
    return result<0?-1:0;
}
