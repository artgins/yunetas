/****************************************************************************
 *          watch_request.h
 *
 *          What the agent takes from a watch-yuno-stats request: who the
 *          requester is (the name of its watch), whether it may be sent
 *          EV_YUNO_STATS, and the yunos it watches.
 *
 *          Copyright (c) 2026, ArtGins.
 *          All Rights Reserved.
 ****************************************************************************/
#pragma once

#include <yunetas.h>

#ifdef __cplusplus
extern "C"{
#endif

/***************************************************************
 *              Prototypes
 ***************************************************************/
/*
 *  The name of the watch of the requester of `kw` (not owned): the
 *  channel it came in by (`route_service`.`route_child`), and the client
 *  at the other end of the route, told by a hop of the ievent stack.
 *  `relayed`: the request came through a control center. Returns 0, or
 *  -1 (logged) when the name does not fit in `bf`.
 */
PUBLIC int watch_route_name(
    hgobj gobj,
    char *bf,
    size_t bfsize,
    const char *route_service,
    const char *route_child,
    json_t *kw,
    BOOL relayed
);

/*
 *  NULL if the requester of `kw` (not owned) says it takes `event`
 *  (`__relays__`, a list naming it), else the comment (yours) of the
 *  refusal. `relayed`: the request came through a control center.
 */
PUBLIC json_t *watch_refusal(
    json_t *kw,
    const char *event,
    BOOL relayed
);

/*
 *  The yunos of `ids` ("<id>[:<service>],..."): {id: [service, ...]}, a
 *  yuno named more than once with different services listing each one.
 *  More than `max_ids` names, or a name too long, is refused: NULL, and
 *  *jn_comment (yours) says why. A `max_ids` under 1 (a bad
 *  max_watch_ids) refuses every watch, logged.
 */
PUBLIC json_t *watch_ids(
    const char *ids,
    json_int_t max_ids,
    json_t **jn_comment
);

#ifdef __cplusplus
}
#endif
