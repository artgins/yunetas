/****************************************************************************
 *          watch_request.c
 *
 *          What the agent takes from a watch-yuno-stats request. See
 *          watch_request.h.
 *
 *          Copyright (c) 2026, ArtGins.
 *          All Rights Reserved.
 ****************************************************************************/
#include <string.h>
#include <stdio.h>
#include <limits.h>

#include "watch_request.h"

/***************************************************************************
 *  A string of a hop (data of a peer): "" if absent or not a string
 ***************************************************************************/
PRIVATE const char *hop_str(json_t *jn_hop, const char *key)
{
    json_t *jn = json_object_get(jn_hop, key);
    return json_is_string(jn)? json_string_value(jn) : "";
}

/***************************************************************************
 *  See watch_request.h
 *
 *  The stack is newest first. Its hop 0 is the one of the agent's own
 *  input channel, stamped by it (input_service, input_channel). Through a
 *  control center, hop 1 is the web client's at the control center,
 *  stamped THERE (input_channel, and cc_connection by command-agent).
 *  Anything deeper is written by the client as it likes: never read.
 ***************************************************************************/
PUBLIC int watch_route_name(
    hgobj gobj,
    char *bf,
    size_t bfsize,
    const char *route_service,
    const char *route_child,
    json_t *kw,
    BOOL relayed
)
{
    json_t *jn_stack = kw_get_list(gobj, kw, "__md_iev__`ievent_gate_stack", 0, 0);
    json_t *jn_origin = json_array_get(jn_stack, (relayed && json_array_size(jn_stack) > 1)? 1 : 0);
    json_t *jn_connection = json_object_get(jn_origin, "cc_connection");
    int len = snprintf(bf, bfsize, "%s.%s|%s^%s^%s^%s^%lld",
        route_service,
        route_child,
        hop_str(jn_origin, "src_yuno"),
        hop_str(jn_origin, "src_service"),
        hop_str(jn_origin, "host"),
        hop_str(jn_origin, "input_channel"),
        json_is_integer(jn_connection)? (long long)json_integer_value(jn_connection) : 0LL
    );
    if(len < 0 || (size_t)len >= bfsize) {
        gobj_log_warning(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_PARAMETER,
            "msg",          "%s", "watch-yuno-stats requester too long to be named",
            "route_len",    "%d", len,
            NULL
        );
        return -1;
    }
    return 0;
}

/***************************************************************************
 *  See watch_request.h
 *
 *  A client that gets an event it does not know drops its connection (a
 *  control center, an older C client) or logs "Event NOT DEFINED" on each
 *  one (ycommand): whoever is sent EV_YUNO_STATS says it takes it. Through
 *  a control center it is the control center that writes `__relays__`, and
 *  only when it relays the event AND its client said it takes it.
 ***************************************************************************/
PUBLIC json_t *watch_refusal(
    json_t *kw,
    const char *event,
    BOOL relayed
)
{
    json_t *jn_relays = json_object_get(kw, "__relays__");
    size_t idx; json_t *jn_relay;
    json_array_foreach(jn_relays, idx, jn_relay) {
        if(json_is_string(jn_relay) && strcmp(json_string_value(jn_relay), event)==0) {
            return NULL;
        }
    }
    if(relayed) {
        return json_sprintf("%s: the client, or the control center in between, does not say it takes %s (__relays__), ask stats-yuno instead",
            gobj_yuno_role_plus_name(), event);
    }
    return json_sprintf("%s: this client does not say it takes %s (__relays__), ask stats-yuno instead",
        gobj_yuno_role_plus_name(), event);
}

/*
 *  The bad max_watch_ids last logged (1: none). Every request is refused
 *  while it is bad, at the clients' rate (renewals): it is logged once per
 *  bad value, again only after it changes.
 */
PRIVATE json_int_t bad_cap_logged = 1;

/***************************************************************************
 *  See watch_request.h
 ***************************************************************************/
PUBLIC json_t *watch_ids(
    const char *ids,
    json_int_t max_ids,
    json_t **jn_comment
)
{
    if(max_ids < 1) {
        if(max_ids != bad_cap_logged) {
            gobj_log_error(0, 0,
                "function",     "%s", __FUNCTION__,
                "msgset",       "%s", MSGSET_PARAMETER,
                "msg",          "%s", "max_watch_ids must be 1 or more, every watch-yuno-stats is refused",
                "max_watch_ids","%lld", (long long)max_ids,
                NULL
            );
            bad_cap_logged = max_ids;
        }
        *jn_comment = json_sprintf("%s: max_watch_ids is %lld, it must be 1 or more: no watch taken",
            gobj_yuno_role_plus_name(), (long long)max_ids);
        return NULL;
    }

    bad_cap_logged = 1;

    int list_size = 0;
    const char **list = split2(ids, ", ", &list_size);
    if(list_size > max_ids) {
        *jn_comment = json_sprintf("%s: too many yuno ids: %d, max_watch_ids is %d",
            gobj_yuno_role_plus_name(), list_size, (int)max_ids);
        split_free2(list);
        return NULL;
    }
    json_t *jn_yunos = json_object();
    for(int i=0; i<list_size; i++) {
        char id[NAME_MAX];
        if(snprintf(id, sizeof(id), "%s", list[i]) >= (int)sizeof(id)) {
            *jn_comment = json_sprintf("%s: yuno id too long: '%.40s...'",
                gobj_yuno_role_plus_name(), list[i]);
            split_free2(list);
            JSON_DECREF(jn_yunos)
            return NULL;
        }
        const char *service = "";
        char *colon = strchr(id, ':');
        if(colon) {
            *colon = 0;
            service = colon + 1;
        }
        json_t *jn_services = json_object_get(jn_yunos, id);
        if(!jn_services) {
            jn_services = json_array();
            json_object_set_new(jn_yunos, id, jn_services);
        }
        if(!json_str_in_list(0, jn_services, service, FALSE)) {
            json_array_append_new(jn_services, json_string(service));
        }
    }
    split_free2(list);
    return jn_yunos;
}
