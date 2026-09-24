/****************************************************************************
 *          find_new_yunos.h
 *
 *          The rows of the agent's find-new-yunos command: which yunos have
 *          a newer binary or configuration than the release they run, and
 *          the create-yuno command that registers each one.
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
 *  The name of the release of a yuno: "<role_version>-<name_version>",
 *  the binary version and the configuration version. It is the pkey2 of
 *  the `yunos` topic (`yuno_release`). Returns 0, or -1 (logged) when the
 *  name does not fit in `bf`.
 */
PUBLIC int build_yuno_release_name(
    hgobj gobj,
    char *bf,
    size_t bfsize,
    const char *role_version,
    const char *name_version
);

/*
 *  The rows of find-new-yunos, from the agent's treedb service
 *  `gobj_resource`: for each yuno row matching `filter` (owned) whose
 *  binary or configuration has a greater version, one dict
 *
 *      {
 *          "command":    "create-yuno id=... role_version=... ...",
 *          "registered": false
 *      }
 *
 *  `registered` is true when the yuno instance at that new release already
 *  exists: a previous `find-new-yunos create=1` registered it and nothing
 *  promoted it yet (the old release is still the primary until
 *  `deactivate-snap`). Running its create-yuno again would answer
 *  "Yuno already exists". The existence check is the one of create-yuno
 *  (realm_id, yuno_role, yuno_name, yuno_release, with instances), plus
 *  the id for a yuno_multiple row.
 *
 *  A release that does not move forward is taken only with `force`
 *  (logged either way). `gobj` is the agent, for the log.
 *  Returns a new list, never NULL.
 */
PUBLIC json_t *find_new_yunos(
    hgobj gobj,
    hgobj gobj_resource,
    json_t *filter,     // owned
    BOOL force,
    hgobj src
);

#ifdef __cplusplus
}
#endif
