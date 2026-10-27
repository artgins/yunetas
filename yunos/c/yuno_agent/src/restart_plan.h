/****************************************************************************
 *          restart_plan.h
 *
 *          What a restart of the node (restart_nodes(): activate-snap,
 *          deactivate-snap) does with each yuno that runs: keep it alive,
 *          restart it, or stop it. A yuno is restarted only when what the
 *          agent launches it with changed.
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
 *  nothing about how it is launched. A record that is not found is a null.
 *  Returns a new dict, to be compared with json_equal().
 */
PUBLIC json_t *yuno_launch_signature(
    hgobj gobj,
    hgobj gobj_resource,
    json_t *yuno
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
 *  What a restart of the node does with a yuno that was running, from its
 *  launch signature before the restart and after it (NULL: no row of that
 *  id any more):
 *      "keep"      the same signature, and enabled: it is left alive;
 *      "stop"      gone, or disabled (a disabled yuno does not run:
 *                  disable-yuno kills it): killed, not launched;
 *      "restart"   changed: killed and launched again.
 */
PUBLIC const char *restart_decision(
    json_t *signature_before,   // not owned
    json_t *signature_after     // not owned, NULL: no row
);

#ifdef __cplusplus
}
#endif
