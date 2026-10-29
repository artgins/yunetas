/****************************************************************************
 *          restart_plan.h
 *
 *          What a restart of the node (restart_nodes(): activate-snap,
 *          deactivate-snap) does with each yuno that runs: keep it alive,
 *          restart it, or stop it. A yuno is restarted only when what the
 *          agent would launch it with now is not what it was launched with.
 *
 *          Copyright (c) 2026, ArtGins.
 *          All Rights Reserved.
 ***************************************************************************/
#pragma once

#include <yunetas.h>

#ifdef __cplusplus
extern "C"{
#endif

/***************************************************************
 *              Prototypes
 ***************************************************************/
/*
 *  What the agent launches the yuno `yuno` (a row of `yunos`, not owned)
 *  with, read from the agent's treedb service `gobj_resource`: the row
 *  itself, the binary it runs (role + role_version), its configuration
 *  (role.name + name_version), its realm and its public services. Of each
 *  record only the columns that are SAVED (persistent, and the fkeys),
 *  without `_geometry` (where a GUI draws it): what is in memory only --
 *  the runtime of the yuno, the hooks, the metadata of the record -- says
 *  nothing about how it is launched. Nor do some saved ones, left out too:
 *  `must_play`, `traced` and `start_priority` of the yuno, and the `yunos`
 *  fkeys of its binary and configuration (the rows linked to them). A
 *  record that is not found is a null. Returns a new dict, to be compared
 *  with json_equal().
 */
PUBLIC json_t *yuno_launch_signature(
    hgobj gobj,
    hgobj gobj_resource,
    json_t *yuno
);

/*
 *  Note what the yuno `yuno_id` is launched with: its launch signature,
 *  read from its row in the agent's treedb service `gobj_resource` at the
 *  moment run_yuno() launches it, is written to
 *  <bin_path>/<role_plus_name>.launch.json, beside the configuration files
 *  it is launched with -- what it describes: its watcher, or its .sh by
 *  hand, launches it again with the same files, and it outlives a restart
 *  of the agent. Returns 0, or -1 (logged): then the file is removed, and
 *  a restart launches the yuno again.
 */
PUBLIC int note_yuno_launch(
    hgobj gobj,
    hgobj gobj_resource,
    const char *yuno_id,
    const char *bin_path,
    const char *role_plus_name
);

/*
 *  The launch signature note_yuno_launch() wrote for the yuno of
 *  `bin_path` and `role_plus_name`, a new reference, or NULL: none (a yuno
 *  launched by an agent up to 7.29.1 -- or one that cannot be read,
 *  logged). A restart compares the signature it would launch the yuno
 *  with NOW against this one.
 */
PUBLIC json_t *yuno_launched_signature(
    hgobj gobj,
    const char *bin_path,
    const char *role_plus_name
);

/*
 *  The columns of the row `yuno` (not owned) that live in memory only (not
 *  persistent, not an fkey, not a hook): what the agent knows of a yuno
 *  that runs -- running, playing, its pids, its channel, its launch. A
 *  reload of the treedb loses them; a yuno kept alive across it gets them
 *  back. Returns a new dict.
 */
PUBLIC json_t *yuno_runtime_fields(
    hgobj gobj,
    hgobj gobj_resource,
    json_t *yuno
);

/*
 *  What a restart of the node does with a yuno that was running, from the
 *  signature it was launched with (note_yuno_launch(); NULL: not launched
 *  by this agent) and the one of after the reload (NULL: no row of that id
 *  any more):
 *      "keep"      the same signature, and enabled: it is left alive;
 *      "stop"      gone, or disabled (a disabled yuno does not run:
 *                  disable-yuno kills it): killed, not launched;
 *      "restart"   changed, or its launch is not known: killed and
 *                  launched again.
 */
PUBLIC const char *restart_decision(
    json_t *signature_launched, // not owned, NULL: not launched by this agent
    json_t *signature_after     // not owned, NULL: no row
);

#ifdef __cplusplus
}
#endif
