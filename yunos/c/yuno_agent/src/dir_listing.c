/****************************************************************************
 *          dir_listing.c
 *
 *          The answer of the agent's dir-* commands: the entries of a
 *          directory tree. See dir_listing.h.
 *
 *          Up to 7.25.4 each command ignored the return of the listing and
 *          answered the list it got: a directory that could not be listed
 *          answered an EMPTY list with result 0.
 *
 *          Copyright (c) 2026, ArtGins.
 *          All Rights Reserved.
 ****************************************************************************/
#include "dir_listing.h"

/***************************************************************************
 *  See dir_listing.h
 ***************************************************************************/
PUBLIC json_t *build_dir_listing_response(
    hgobj gobj,
    const char *directory,
    const char *match,
    json_t *kw          // owned
)
{
    dir_array_t da;
    if(get_ordered_filename_array(gobj,
        directory,
        match,
        WD_RECURSIVE|WD_MATCH_DIRECTORY|WD_MATCH_REGULAR_FILE|WD_MATCH_SYMBOLIC_LINK|WD_HIDDENFILES,
        &da
    ) < 0) {
        // Error already logged
        dir_array_free(&da);
        return msg_iev_build_response(
            gobj,
            -1,
            json_sprintf("%s: cannot list '%s' (%s)",
                gobj_yuno_role_plus_name(), directory, gobj_log_last_message()),
            0,
            0,
            kw  // owned
        );
    }

    json_t *jn_array = json_array();
    for(json_int_t i=0; i<da.count; i++) {
        json_array_append_new(jn_array, json_string(da.items[i]));
    }
    dir_array_free(&da);

    return msg_iev_build_response(
        gobj,
        0,
        0,
        0,
        jn_array,
        kw  // owned
    );
}
