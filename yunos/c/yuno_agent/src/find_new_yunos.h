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
 *  TRUE when the releases of a yuno with this binary version compare in
 *  order: "x.y.z" (a yuno of the SDK), "x.y.z-a.b.c" (a project yuno with
 *  the SDK it is built with), or the pre-7.27.11 "x.y.z.0". install-binary
 *  logs a WARNING for any other shape and installs it all the same.
 */
PUBLIC BOOL yuno_version_has_a_comparable_shape(const char *version);

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

/*
 *  What find-new-yunos does with the rows of find_new_yunos() (`jn_rows`,
 *  not owned). Returns the preview it answers, a new list: every row's
 *  command, a registered one as "already registered, pending promotion
 *  (deactivate-snap): <command>". Appends to `jn_commands` the create-yuno
 *  commands that `create=1` runs: only the rows NOT registered, because a
 *  registered one would answer "Yuno already exists". The registered rows
 *  stay in the preview so that `yunetas upgrade-yunos`, which stops at an
 *  empty preview, still runs deactivate-snap for them. `*registered` gets
 *  their count.
 */
PUBLIC json_t *find_new_yunos_preview(
    hgobj gobj,
    json_t *jn_rows,        // not owned
    json_t *jn_commands,    // not owned, the commands to run are appended
    int *registered
);

/*
 *  The instance of the binary `role` (an id of `binaries`) at `version`,
 *  or the first one the treedb lists when `version` is empty. NULL when
 *  that version is not installed. Returns a new reference.
 */
PUBLIC json_t *find_binary_version(
    hgobj gobj,
    hgobj gobj_resource,
    const char *role,
    const char *version
);

/*
 *  The instance of the configuration "<role>.<name>" at `version`, or the
 *  first one the treedb lists when `version` is empty. NULL when that
 *  version does not exist (or the id does not fit, logged). Returns a new
 *  reference.
 */
PUBLIC json_t *find_configuration_version(
    hgobj gobj,
    hgobj gobj_resource,
    const char *role,
    const char *name,
    const char *version
);

#ifdef __cplusplus
}
#endif
