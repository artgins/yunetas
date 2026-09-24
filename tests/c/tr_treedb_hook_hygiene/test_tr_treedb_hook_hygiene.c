/****************************************************************************
 *          test_tr_treedb_hook_hygiene.c
 *
 *          Regression coverage for the treedb hook fixes on a versioned
 *          (pkey2) parent with a hook.
 *
 *          Four quirks (and, on a treedb of its own, the ids and refs of
 *          test_ids_and_refs()):
 *
 *            1. Duplicate hook entries. Linking the same child id twice used
 *               to append it twice to the parent hook (and twice to the child
 *               fkey). Link is now idempotent and warns on the skipped dup.
 *               (commit "fix(treedb): version-aware reverse-hook unlink +
 *               idempotent link dedup").
 *
 *            2. Unlink targeted only the primary parent version. A child's
 *               fkey ref carries the parent id but not the version, so cleaning
 *               a child hooked on a NON-primary config version used to unlink
 *               the primary (where the child isn't), leaving a stale entry on
 *               the real version. Clean now locates the holding version.
 *
 *            3. Force-delete skipped children of an array hook. The teardown
 *               iterated the parent hook array while _unlink_nodes() removed
 *               each child from it in place, so the index-based loop stepped
 *               over the shifted tail and left a child linked, aborting the
 *               delete. The teardown now snapshots the child refs first.
 *
 *            4. A FAILED open leaked the shared topic_cols_desc. The open path
 *               increfs/creates that module-global before validating the
 *               schema; an early error return skipped the matching decref, so
 *               it was still alive at gobj_end. Undone on the error path now.
 *
 *          Copyright (c) 2026, ArtGins.
 *          All Rights Reserved.
 ****************************************************************************/
#include <string.h>
#include <signal.h>
#include <limits.h>

#include <gobj.h>
#include <timeranger2.h>
#include <tr_treedb.h>
#include <yev_loop.h>
#include <testing.h>
#include <helpers.h>
#include <kwid.h>

#include "schema_sample.c"

#define APP "test_tr_treedb_hook_hygiene"

/***************************************************************
 *              Constants
 ***************************************************************/
#define DATABASE        "tr_hook_hygiene"
#define PKEY2_NAME      "version"
#define HOOK_NAME       "yunos"
#define CONFIG_FKEY     "config"

/***************************************************************
 *              Prototypes
 ***************************************************************/
PRIVATE void yuno_catch_signals(void);

/***************************************************************
 *              Data
 ***************************************************************/
PRIVATE yev_loop_h yev_loop;

/***************************************************************************
 *  Helper: size of a node's array field (hook or fkey). -1 if absent.
 ***************************************************************************/
PRIVATE int array_field_size(json_t *node, const char *field)
{
    json_t *arr = json_object_get(node, field);
    if(!arr || !json_is_array(arr)) {
        return -1;
    }
    return (int)json_array_size(arr);
}

/***************************************************************************
 *  Quirk 2: linking the same child twice is idempotent and warns.
 ***************************************************************************/
PRIVATE int test_idempotent_link_dedup(
    json_t *tranger,
    const char *treedb_name
)
{
    int result = 0;
    time_measure_t time_measure;

    /*------------------------------------*
     *  Seed: one config (v1) + one yuno,
     *  link them once. No logs expected.
     *------------------------------------*/
    {
        const char *test = "dedup: seed + first link (clean)";
        set_expected_results(test, NULL, NULL, NULL, 0);
        MT_START_TIME(time_measure)

        treedb_create_node(tranger, treedb_name, "configs",
            json_pack("{s:s, s:s}", "id", "cfg-A", "version", "v1"));
        treedb_create_node(tranger, treedb_name, "yunos",
            json_pack("{s:s}", "id", "y-A"));

        json_t *cfg = treedb_get_node(tranger, treedb_name, "configs", "cfg-A");
        json_t *yuno = treedb_get_node(tranger, treedb_name, "yunos", "y-A");

        if(treedb_link_nodes(tranger, HOOK_NAME, cfg, yuno) != 0) {
            printf("%s  FAIL: first link returned non-zero%s\n", On_Red BWhite, Color_Off);
            result += -1;
        }
        if(array_field_size(cfg, HOOK_NAME) != 1) {
            printf("%s  FAIL: parent hook size != 1 after first link (got %d)%s\n",
                On_Red BWhite, array_field_size(cfg, HOOK_NAME), Color_Off);
            result += -1;
        }
        if(array_field_size(yuno, CONFIG_FKEY) != 1) {
            printf("%s  FAIL: child fkey size != 1 after first link (got %d)%s\n",
                On_Red BWhite, array_field_size(yuno, CONFIG_FKEY), Color_Off);
            result += -1;
        }

        MT_INCREMENT_COUNT(time_measure, 1)
        MT_PRINT_TIME(time_measure, test)
        result += test_json(NULL);
    }

    /*------------------------------------*
     *  Re-link the SAME pair: must be a
     *  no-op that warns twice (parent hook
     *  + child fkey), in that order.
     *------------------------------------*/
    {
        const char *test = "dedup: duplicate link warns, stays single";
        set_expected_results(test,
            json_pack("[{s:s},{s:s}]",
                "msg", "Child already in parent hook, skipping duplicate link",
                "msg", "Parent ref already in child fkey, skipping duplicate"
            ),
            NULL, NULL, 0);
        MT_START_TIME(time_measure)

        json_t *cfg = treedb_get_node(tranger, treedb_name, "configs", "cfg-A");
        json_t *yuno = treedb_get_node(tranger, treedb_name, "yunos", "y-A");

        if(treedb_link_nodes(tranger, HOOK_NAME, cfg, yuno) != 0) {
            printf("%s  FAIL: duplicate link returned non-zero%s\n", On_Red BWhite, Color_Off);
            result += -1;
        }
        if(array_field_size(cfg, HOOK_NAME) != 1) {
            printf("%s  FAIL: parent hook duplicated (size %d, want 1)%s\n",
                On_Red BWhite, array_field_size(cfg, HOOK_NAME), Color_Off);
            result += -1;
        }
        if(array_field_size(yuno, CONFIG_FKEY) != 1) {
            printf("%s  FAIL: child fkey duplicated (size %d, want 1)%s\n",
                On_Red BWhite, array_field_size(yuno, CONFIG_FKEY), Color_Off);
            result += -1;
        }

        MT_INCREMENT_COUNT(time_measure, 1)
        MT_PRINT_TIME(time_measure, test)
        result += test_json(NULL);
    }

    return result;
}

/***************************************************************************
 *  Quirk 1: clean a child hooked on a NON-primary parent version must
 *  unlink the real version, not the primary, leaving no stale entry and
 *  logging no "Child data not found" error.
 ***************************************************************************/
PRIVATE int test_clean_unlinks_nonprimary_version(
    json_t *tranger,
    const char *treedb_name
)
{
    int result = 0;
    time_measure_t time_measure;

    /*------------------------------------*
     *  Seed: config cfg-B with two
     *  versions + one yuno. Link the yuno
     *  to the NON-primary version.
     *------------------------------------*/
    json_t *target = NULL;       // the non-primary instance we hook onto
    const char *target_ver = NULL;
    json_t *other = NULL;        // the primary instance (must stay empty)
    {
        const char *test = "nonprimary: seed + link to non-primary version";
        set_expected_results(test, NULL, NULL, NULL, 0);
        MT_START_TIME(time_measure)

        treedb_create_node(tranger, treedb_name, "configs",
            json_pack("{s:s, s:s}", "id", "cfg-B", "version", "v1"));
        treedb_create_node(tranger, treedb_name, "configs",
            json_pack("{s:s, s:s}", "id", "cfg-B", "version", "v2"));
        treedb_create_node(tranger, treedb_name, "yunos",
            json_pack("{s:s}", "id", "y-B"));

        json_t *v1 = treedb_get_instance(tranger, treedb_name, "configs", PKEY2_NAME, "cfg-B", "v1");
        json_t *v2 = treedb_get_instance(tranger, treedb_name, "configs", PKEY2_NAME, "cfg-B", "v2");
        json_t *primary = treedb_get_node(tranger, treedb_name, "configs", "cfg-B");

        /*  Pick whichever instance is NOT the in-memory primary  */
        if(primary == v2) {
            target = v1; target_ver = "v1"; other = v2;
        } else {
            target = v2; target_ver = "v2"; other = v1;
        }
        if(!target || target == primary) {
            printf("%s  FAIL: could not pick a non-primary version (precondition)%s\n",
                On_Red BWhite, Color_Off);
            result += -1;
        }

        json_t *yuno = treedb_get_node(tranger, treedb_name, "yunos", "y-B");
        if(treedb_link_nodes(tranger, HOOK_NAME, target, yuno) != 0) {
            printf("%s  FAIL: link to non-primary version returned non-zero%s\n",
                On_Red BWhite, Color_Off);
            result += -1;
        }

        /*  The child is on the non-primary version only  */
        if(array_field_size(target, HOOK_NAME) != 1) {
            printf("%s  FAIL: non-primary (%s) hook size != 1 after link (got %d)%s\n",
                On_Red BWhite, target_ver, array_field_size(target, HOOK_NAME), Color_Off);
            result += -1;
        }
        if(array_field_size(other, HOOK_NAME) != 0) {
            printf("%s  FAIL: primary hook should be empty, got %d%s\n",
                On_Red BWhite, array_field_size(other, HOOK_NAME), Color_Off);
            result += -1;
        }
        if(array_field_size(yuno, CONFIG_FKEY) != 1) {
            printf("%s  FAIL: child fkey size != 1 after link (got %d)%s\n",
                On_Red BWhite, array_field_size(yuno, CONFIG_FKEY), Color_Off);
            result += -1;
        }

        MT_INCREMENT_COUNT(time_measure, 1)
        MT_PRINT_TIME(time_measure, test)
        result += test_json(NULL);
    }

    /*------------------------------------*
     *  Clean the child: must unlink the
     *  NON-primary version cleanly, with
     *  no "Child data not found" error.
     *------------------------------------*/
    {
        const char *test = "nonprimary: clean unlinks the holding version";
        set_expected_results(test, NULL, NULL, NULL, 0);
        MT_START_TIME(time_measure)

        json_t *yuno = treedb_get_node(tranger, treedb_name, "yunos", "y-B");
        if(treedb_clean_node(tranger, yuno, TRUE) != 0) {
            printf("%s  FAIL: clean_node returned non-zero%s\n", On_Red BWhite, Color_Off);
            result += -1;
        }

        /*  Re-fetch the instance (clean may have re-saved nodes)  */
        json_t *target_after = treedb_get_instance(
            tranger, treedb_name, "configs", PKEY2_NAME, "cfg-B", target_ver);
        if(array_field_size(target_after, HOOK_NAME) != 0) {
            printf("%s  FAIL: stale entry left on non-primary (%s) hook (size %d)%s\n",
                On_Red BWhite, target_ver, array_field_size(target_after, HOOK_NAME), Color_Off);
            result += -1;
        }
        if(array_field_size(yuno, CONFIG_FKEY) != 0) {
            printf("%s  FAIL: child fkey not cleared by clean (size %d)%s\n",
                On_Red BWhite, array_field_size(yuno, CONFIG_FKEY), Color_Off);
            result += -1;
        }

        MT_INCREMENT_COUNT(time_measure, 1)
        MT_PRINT_TIME(time_measure, test)
        result += test_json(NULL);
    }

    return result;
}

/***************************************************************************
 *  Quirk 3: force-deleting a parent whose ARRAY hook holds several children
 *  used to iterate that hook array while _unlink_nodes removed from it in
 *  place, so the index-based loop skipped every other child. The re-check
 *  then found a leftover down link and ABORTED the delete, leaving a
 *  half-unlinked graph. Force-delete must now clear EVERY child and remove
 *  the node.
 ***************************************************************************/
PRIVATE int test_force_delete_unlinks_all_array_children(
    json_t *tranger,
    const char *treedb_name
)
{
    int result = 0;
    time_measure_t time_measure;
    const char *child_ids[] = {"y-C1", "y-C2", "y-C3"};

    /*------------------------------------*
     *  Seed: config cfg-C (v1) + three
     *  yunos, all linked to it. The hook
     *  array holds three children.
     *------------------------------------*/
    {
        const char *test = "force-delete: seed config + three linked children";
        set_expected_results(test, NULL, NULL, NULL, 0);
        MT_START_TIME(time_measure)

        treedb_create_node(tranger, treedb_name, "configs",
            json_pack("{s:s, s:s}", "id", "cfg-C", "version", "v1"));
        treedb_create_node(tranger, treedb_name, "yunos", json_pack("{s:s}", "id", "y-C1"));
        treedb_create_node(tranger, treedb_name, "yunos", json_pack("{s:s}", "id", "y-C2"));
        treedb_create_node(tranger, treedb_name, "yunos", json_pack("{s:s}", "id", "y-C3"));

        json_t *cfg = treedb_get_node(tranger, treedb_name, "configs", "cfg-C");
        for(int i=0; i<3; i++) {
            json_t *yuno = treedb_get_node(tranger, treedb_name, "yunos", child_ids[i]);
            if(treedb_link_nodes(tranger, HOOK_NAME, cfg, yuno) != 0) {
                printf("%s  FAIL: link of %s returned non-zero%s\n",
                    On_Red BWhite, child_ids[i], Color_Off);
                result += -1;
            }
        }
        if(array_field_size(cfg, HOOK_NAME) != 3) {
            printf("%s  FAIL: parent hook size != 3 after linking (got %d)%s\n",
                On_Red BWhite, array_field_size(cfg, HOOK_NAME), Color_Off);
            result += -1;
        }

        MT_INCREMENT_COUNT(time_measure, 1)
        MT_PRINT_TIME(time_measure, test)
        result += test_json(NULL);
    }

    /*------------------------------------*
     *  Force-delete the parent: must
     *  succeed, unlink ALL three children
     *  and remove the node. Before the fix
     *  this aborted, leaving y-C2 linked.
     *------------------------------------*/
    {
        const char *test = "force-delete: parent gone, every child unlinked";
        set_expected_results(test, NULL, NULL, NULL, 0);
        MT_START_TIME(time_measure)

        json_t *cfg = treedb_get_node(tranger, treedb_name, "configs", "cfg-C");
        if(treedb_delete_node(tranger, cfg, json_pack("{s:b}", "force", 1)) != 0) {
            printf("%s  FAIL: force-delete returned non-zero%s\n", On_Red BWhite, Color_Off);
            result += -1;
        }

        if(treedb_get_node(tranger, treedb_name, "configs", "cfg-C") != NULL) {
            printf("%s  FAIL: parent still present after force-delete%s\n",
                On_Red BWhite, Color_Off);
            result += -1;
        }

        for(int i=0; i<3; i++) {
            json_t *yuno = treedb_get_node(tranger, treedb_name, "yunos", child_ids[i]);
            if(array_field_size(yuno, CONFIG_FKEY) != 0) {
                printf("%s  FAIL: child %s still linked after force-delete (fkey size %d)%s\n",
                    On_Red BWhite, child_ids[i], array_field_size(yuno, CONFIG_FKEY), Color_Off);
                result += -1;
            }
        }

        MT_INCREMENT_COUNT(time_measure, 1)
        MT_PRINT_TIME(time_measure, test)
        result += test_json(NULL);
    }

    return result;
}

/***************************************************************************
 *  Quirk 4: a FAILED open must not leak the shared topic_cols_desc. The
 *  open path increfs/creates that module-global before validating the
 *  schema; an early error return ("No topics found") used to skip the
 *  matching decref, so the descriptor was still alive at gobj_end (a leak
 *  under CONFIG_DEBUG_TRACK_MEMORY). The end-of-test memory check is the
 *  real assertion here.
 ***************************************************************************/
PRIVATE int test_failed_open_no_desc_leak(
    json_t *tranger
)
{
    int result = 0;
    time_measure_t time_measure;

    const char *test = "failed open: no-topics schema rejected, no desc leak";
    set_expected_results(test,
        json_pack("[{s:s}]", "msg", "No topics found"),
        NULL, NULL, 0);
    MT_START_TIME(time_measure)

    /*  A schema with no 'topics' list: open must fail after the desc incref  */
    json_t *jn_schema = json_pack("{s:s}", "id", "bad_schema");
    if(treedb_open_db(tranger, "treedb_bad", jn_schema, 0) != NULL) {
        printf("%s  FAIL: open of a no-topics schema should return NULL%s\n",
            On_Red BWhite, Color_Off);
        result += -1;
    }

    MT_INCREMENT_COUNT(time_measure, 1)
    MT_PRINT_TIME(time_measure, test)
    result += test_json(NULL);

    return result;
}

/***************************************************************************
 *  Ids and references, on a treedb of its own in the same tranger:
 *  `owners` hooks `users` AND `groups`, through a list hook (`members`)
 *  and a dict hook (`tagged`).
 *
 *    5. A hook tested membership by the bare id, so the user `x` and the
 *       group `x` were one node for it: the second link was "already in
 *       the hook" (list), or took the first one's place (dict).
 *    6. The id of a node with hooks is in every ref its children hold: an
 *       id holding '^' was accepted, and made every ref to the node
 *       undecodable; an id of NAME_MAX made refs that were cut in silence.
 *       Both are refused at create.
 *    7. A ref whose part is too long for the decode was cut in silence.
 ***************************************************************************/
static char schema_ids[]= "\
{                                                                   \n\
    'id': 'treedb_ids',                                             \n\
    'topics': [                                                     \n\
        {                                                           \n\
            'topic_name': 'owners',                                 \n\
            'pkey': 'id',                                           \n\
            'system_flag': 'sf_string_key',                         \n\
            'cols': {                                               \n\
                'id': {'header': 'Id', 'type': 'string', 'flag': ['persistent','required']}, \n\
                'members': {'header': 'Members', 'type': 'array', 'flag': ['hook'], 'hook': {'users': 'owner', 'groups': 'owner'}}, \n\
                'tagged': {'header': 'Tagged', 'type': 'object', 'flag': ['hook'], 'hook': {'users': 'tags', 'groups': 'tags'}} \n\
            }                                                       \n\
        },                                                          \n\
        {                                                           \n\
            'topic_name': 'users',                                  \n\
            'pkey': 'id',                                           \n\
            'system_flag': 'sf_string_key',                         \n\
            'cols': {                                               \n\
                'id': {'header': 'Id', 'type': 'string', 'flag': ['persistent','required']}, \n\
                'owner': {'header': 'Owner', 'type': 'string', 'flag': ['fkey']}, \n\
                'tags': {'header': 'Tags', 'type': 'array', 'flag': ['fkey']} \n\
            }                                                       \n\
        },                                                          \n\
        {                                                           \n\
            'topic_name': 'groups',                                 \n\
            'pkey': 'id',                                           \n\
            'system_flag': 'sf_string_key',                         \n\
            'cols': {                                               \n\
                'id': {'header': 'Id', 'type': 'string', 'flag': ['persistent','required']}, \n\
                'owner': {'header': 'Owner', 'type': 'string', 'flag': ['fkey']}, \n\
                'tags': {'header': 'Tags', 'type': 'array', 'flag': ['fkey']} \n\
            }                                                       \n\
        }                                                           \n\
    ]                                                               \n\
}                                                                   \n\
";

PRIVATE int test_ids_and_refs(json_t *tranger)
{
    int result = 0;
    const char *treedb_name = "treedb_ids";
    const char *test = "ids and refs: open";
    set_expected_results(test,
        json_pack("[{s:s}, {s:s}, {s:s}]",
            "msg", "Creating topic",
            "msg", "Creating topic",
            "msg", "Creating topic"
        ),
        NULL, NULL, 1
    );
    helper_quote2doublequote(schema_ids);
    json_t *jn_schema = legalstring2json(schema_ids, TRUE);
    if(!jn_schema || !treedb_open_db(tranger, treedb_name, jn_schema, 0)) {
        printf("%s  FAIL: cannot open treedb_ids%s\n", On_Red BWhite, Color_Off);
        return -1;
    }
    json_t *owner = treedb_create_node(tranger, treedb_name, "owners", json_pack("{s:s}", "id", "O"));
    json_t *user_x = treedb_create_node(tranger, treedb_name, "users", json_pack("{s:s}", "id", "x"));
    json_t *group_x = treedb_create_node(tranger, treedb_name, "groups", json_pack("{s:s}", "id", "x"));
    if(!owner || !user_x || !group_x) {
        printf("%s  FAIL: setup of treedb_ids%s\n", On_Red BWhite, Color_Off);
        result += -1;
    }
    result += test_json(NULL);

    /*
     *  5a. A list hook holds the user x AND the group x
     */
    test = "a list hook holds two nodes of two topics with one id";
    set_expected_results(test, NULL, NULL, NULL, 1);
    if(treedb_link_nodes(tranger, "members", owner, user_x) < 0 ||
            treedb_link_nodes(tranger, "members", owner, group_x) < 0) {
        printf("%s  FAIL: %s: a link failed%s\n", On_Red BWhite, test, Color_Off);
        result += -1;
    }
    json_t *members = json_object_get(owner, "members");
    if(json_array_size(members) != 2 ||
            json_array_get(members, 0) != user_x ||
            json_array_get(members, 1) != group_x) {
        printf("%s  FAIL: %s: the hook holds %d nodes, not the user and the group%s\n",
            On_Red BWhite, test, (int)json_array_size(members), Color_Off);
        result += -1;
    }
    result += test_json(NULL);

    /*
     *  5b. A dict hook, keyed by the id alone, refuses the group x
     *      while it holds the user x
     */
    test = "a dict hook refuses the node of another topic with an id it holds";
    set_expected_results(test,
        json_pack("[{s:s}]",
            "msg", "Cannot link, the dict hook holds a node of another topic with this id"
        ),
        NULL, NULL, 1
    );
    if(treedb_link_nodes(tranger, "tagged", owner, user_x) < 0) {
        printf("%s  FAIL: %s: the link of the user failed%s\n", On_Red BWhite, test, Color_Off);
        result += -1;
    }
    if(treedb_link_nodes(tranger, "tagged", owner, group_x) >= 0) {
        printf("%s  FAIL: %s: the link of the group answered success%s\n",
            On_Red BWhite, test, Color_Off);
        result += -1;
    }
    if(json_object_get(json_object_get(owner, "tagged"), "x") != user_x ||
            json_array_size(json_object_get(group_x, "tags")) != 0) {
        printf("%s  FAIL: %s: the group took the user's place%s\n", On_Red BWhite, test, Color_Off);
        result += -1;
    }
    result += test_json(NULL);

    /*
     *  6. Ids that cannot make a ref are refused at create
     */
    test = "an id holding '^', or too long for a ref, is refused at create";
    /*  `owners` has hooks: its ids are in refs; `users` has none  */
    set_expected_results(test,
        json_pack("[{s:s}, {s:s}]",
            "msg", "Invalid 'id': it holds a '^', the separator of a reference",
            "msg", "Invalid 'id': too long to be part of a reference"
        ),
        NULL, NULL, 1
    );
    if(treedb_create_node(tranger, treedb_name, "owners", json_pack("{s:s}", "id", "a^b"))) {
        printf("%s  FAIL: %s: the id 'a^b' was created%s\n", On_Red BWhite, test, Color_Off);
        result += -1;
    }
    if(!treedb_create_node(tranger, treedb_name, "users", json_pack("{s:s}", "id", "u^1"))) {
        printf("%s  FAIL: %s: the id 'u^1' of a topic without hooks was refused%s\n",
            On_Red BWhite, test, Color_Off);
        result += -1;
    }
    char long_id[NAME_MAX + 1];
    memset(long_id, 'L', NAME_MAX);
    long_id[NAME_MAX] = 0;
    if(treedb_create_node(tranger, treedb_name, "owners", json_pack("{s:s}", "id", long_id))) {
        printf("%s  FAIL: %s: an id of NAME_MAX was created%s\n", On_Red BWhite, test, Color_Off);
        result += -1;
    }
    result += test_json(NULL);

    /*
     *  7. A ref whose id part does not fit the decode is refused, not cut
     */
    test = "a ref with a part too long is refused, not cut";
    set_expected_results(test,
        json_pack("[{s:s}, {s:s}]",
            "msg", "Wrong reference: a part of it is too long",
            "msg", "Wrong parent reference: must be \"parent_topic_name^parent_id^hook_name\""
        ),
        NULL, NULL, 1
    );
    char long_ref[3*NAME_MAX];
    snprintf(long_ref, sizeof(long_ref), "owners^%s^members", long_id);
    json_t *user_y = treedb_create_node(tranger, treedb_name, "users", json_pack("{s:s}", "id", "y"));
    if(!user_y || treedb_autolink(tranger, user_y, json_pack("{s:s}", "owner", long_ref), TRUE) >= 0) {
        printf("%s  FAIL: %s: the autolink answered success%s\n", On_Red BWhite, test, Color_Off);
        result += -1;
    }
    result += test_json(NULL);

    json_check_refcounts(tranger, 1000, &result);
    treedb_close_db(tranger, treedb_name);
    return result;
}

/***************************************************************************
 *  Parents of a store of before, whose ids a create refuses now (6): one
 *  with an id of NAME_MAX, one with an id holding '^'. They load, but
 *  their id cannot make a reference.
 *
 *    8. A link to them answered 0: the ref of NAME_MAX was saved and lost
 *       at the reopen ("a part of it is too long"), the ref with a '^'
 *       was refused by the save. Now the link is refused, and nothing
 *       moves.
 *    9. A save that met one wrong ref in an fkey column saved the record
 *       WITHOUT the column: the valid refs of it were lost at the reopen.
 *       Now the wrong ref alone is left out, and logged.
 ***************************************************************************/
PRIVATE int test_parents_of_before(json_t *tranger)
{
    int result = 0;
    const char *treedb_name = "treedb_ids";
    const char *test = "parents of before: setup";
    char long_id[NAME_MAX + 1];
    memset(long_id, 'L', NAME_MAX);
    long_id[NAME_MAX] = 0;

    set_expected_results(test, NULL, NULL, NULL, 1);
    md2_record_ex_t md;
    if(tranger2_append_record(tranger, "owners", 0, 0, &md, json_pack("{s:s}", "id", long_id)) < 0 ||
            tranger2_append_record(tranger, "owners", 0, 0, &md, json_pack("{s:s}", "id", "a^b")) < 0) {
        printf("%s  FAIL: %s: the parents of before were not written%s\n", On_Red BWhite, test, Color_Off);
        result += -1;
    }
    json_t *jn_schema = legalstring2json(schema_ids, TRUE);
    if(!jn_schema || !treedb_open_db(tranger, treedb_name, jn_schema, 0)) {
        printf("%s  FAIL: cannot open treedb_ids%s\n", On_Red BWhite, Color_Off);
        return -1;
    }
    json_t *p_long = treedb_get_node(tranger, treedb_name, "owners", long_id);
    json_t *p_caret = treedb_get_node(tranger, treedb_name, "owners", "a^b");
    json_t *owner = treedb_get_node(tranger, treedb_name, "owners", "O");
    json_t *u1 = treedb_create_node(tranger, treedb_name, "users", json_pack("{s:s}", "id", "u1"));
    json_t *u2 = treedb_create_node(tranger, treedb_name, "users", json_pack("{s:s}", "id", "u2"));
    json_t *u3 = treedb_create_node(tranger, treedb_name, "users", json_pack("{s:s}", "id", "u3"));
    if(!p_long || !p_caret || !owner || !u1 || !u2 || !u3) {
        printf("%s  FAIL: %s: the nodes are not there%s\n", On_Red BWhite, test, Color_Off);
        result += -1;
    }
    result += test_json(NULL);

    /*
     *  8. A link to a parent whose id cannot make a ref is refused
     */
    test = "a link to a parent whose id cannot make a ref is refused";
    set_expected_results(test,
        json_pack("[{s:s}, {s:s}, {s:s}]",
            "msg", "Cannot build the reference of a node: a part of it is too long, or holds a '^'",
            "msg", "Cannot build the reference of a node: a part of it is too long, or holds a '^'",
            "msg", "Cannot build the reference of a node: a part of it is too long, or holds a '^'"
        ),
        NULL, NULL, 1
    );
    if(treedb_link_nodes(tranger, "members", p_long, u1) >= 0) {
        printf("%s  FAIL: %s: the link to the id of NAME_MAX answered success%s\n",
            On_Red BWhite, test, Color_Off);
        result += -1;
    }
    if(treedb_link_nodes(tranger, "members", p_caret, u2) >= 0) {
        printf("%s  FAIL: %s: the link to the id with a '^' answered success%s\n",
            On_Red BWhite, test, Color_Off);
        result += -1;
    }
    if(treedb_link_nodes(tranger, "tagged", owner, u3) < 0) {
        printf("%s  FAIL: %s: the link of u3 to O failed%s\n", On_Red BWhite, test, Color_Off);
        result += -1;
    }
    if(treedb_link_nodes(tranger, "tagged", p_caret, u3) >= 0) {
        printf("%s  FAIL: %s: the tag to the id with a '^' answered success%s\n",
            On_Red BWhite, test, Color_Off);
        result += -1;
    }
    if(strcmp(kw_get_str(0, u1, "owner", "?", 0), "")!=0 ||
            strcmp(kw_get_str(0, u2, "owner", "?", 0), "")!=0 ||
            json_array_size(json_object_get(p_long, "members")) != 0 ||
            json_array_size(json_object_get(p_caret, "members")) != 0 ||
            json_object_size(json_object_get(p_caret, "tagged")) != 0 ||
            json_array_size(json_object_get(u3, "tags")) != 1) {
        printf("%s  FAIL: %s: a refused link moved something%s\n", On_Red BWhite, test, Color_Off);
        result += -1;
    }
    result += test_json(NULL);

    /*
     *  9. A wrong ref in an fkey column: the save keeps the valid ones
     */
    test = "a save leaves out a wrong ref, not the whole column";
    set_expected_results(test,
        json_pack("[{s:s}]",
            "msg", "Wrong fkey reference: must be \"topic_name^id^hook_name\""
        ),
        NULL, NULL, 1
    );
    json_array_append_new(json_object_get(u3, "tags"), json_string("owners^a^b^tagged"));
    if(treedb_save_node(tranger, u3) < 0) {
        printf("%s  FAIL: %s: the save failed%s\n", On_Red BWhite, test, Color_Off);
        result += -1;
    }
    result += test_json(NULL);

    json_check_refcounts(tranger, 1000, &result);
    treedb_close_db(tranger, treedb_name);

    test = "a save leaves out a wrong ref, not the whole column: reopen";
    set_expected_results(test, NULL, NULL, NULL, 1);
    jn_schema = legalstring2json(schema_ids, TRUE);
    if(!jn_schema || !treedb_open_db(tranger, treedb_name, jn_schema, 0)) {
        printf("%s  FAIL: cannot open treedb_ids%s\n", On_Red BWhite, Color_Off);
        return -1;
    }
    u3 = treedb_get_node(tranger, treedb_name, "users", "u3");
    owner = treedb_get_node(tranger, treedb_name, "owners", "O");
    json_t *tags = json_object_get(u3, "tags");
    if(json_array_size(tags) != 1 ||
            strcmp(json_string_value(json_array_get(tags, 0)), "owners^O^tagged")!=0 ||
            json_object_get(json_object_get(owner, "tagged"), "u3") != u3) {
        printf("%s  FAIL: %s: the valid tag of u3 is lost%s\n", On_Red BWhite, test, Color_Off);
        gobj_trace_json(0, u3, "u3");
        result += -1;
    }
    result += test_json(NULL);

    json_check_refcounts(tranger, 1000, &result);
    treedb_close_db(tranger, treedb_name);
    return result;
}

/***************************************************************************
 *              do_test
 ***************************************************************************/
PRIVATE int do_test(void)
{
    int result = 0;
    const char *home = getenv("HOME");
    char path_root[PATH_MAX];
    char path_database[PATH_MAX];

    build_path(path_root, sizeof(path_root), home, "tests_yuneta", NULL);
    mkrdir(path_root, 02770);

    build_path(path_database, sizeof(path_database), path_root, DATABASE, NULL);
    rmrdir(path_database);

    /*------------------------------------*
     *  Open tranger as master
     *------------------------------------*/
    json_t *tranger;
    {
        const char *test = "open tranger";
        set_expected_results(
            test,
            json_pack("[{s:s}]", "msg", "Creating __timeranger2__.json"),
            NULL, NULL, 1
        );

        json_t *jn_tranger = json_pack("{s:s, s:s, s:b, s:i}",
            "path", path_root,
            "database", DATABASE,
            "master", 1,
            "on_critical_error", LOG_OPT_TRACE_STACK
        );
        tranger = tranger2_startup(0, jn_tranger, 0);
        result += test_json(NULL);
    }

    /*------------------------------------*
     *  Open treedb with the schema
     *  (2 user topics + __snaps__ + __graphs__ + __assets__ = 5 "Creating topic")
     *------------------------------------*/
    const char *treedb_name = "treedb_hook_hygiene";
    {
        const char *test = "open treedb";
        set_expected_results(
            test,
            json_pack("[{s:s}, {s:s}, {s:s}, {s:s}, {s:s}]",
                "msg", "Creating topic",
                "msg", "Creating topic",
                "msg", "Creating topic",
                "msg", "Creating topic",
                "msg", "Creating topic"
            ),
            NULL, NULL, 1
        );

        helper_quote2doublequote(schema_sample);
        json_t *jn_schema = legalstring2json(schema_sample, TRUE);
        if(!jn_schema) {
            printf("Can't decode schema_sample json\n");
            exit(-1);
        }

        if(!treedb_open_db(tranger, treedb_name, jn_schema, 0)) {
            result += -1;
        }
        result += test_json(NULL);
    }

    /*------------------------------------*
     *  Execute scenarios
     *------------------------------------*/
    result += test_idempotent_link_dedup(tranger, treedb_name);
    result += test_clean_unlinks_nonprimary_version(tranger, treedb_name);
    result += test_force_delete_unlinks_all_array_children(tranger, treedb_name);
    result += test_failed_open_no_desc_leak(tranger);
    result += test_ids_and_refs(tranger);
    result += test_parents_of_before(tranger);

    /*------------------------------------*
     *  Shutdown
     *------------------------------------*/
    {
        const char *test = "close and shutdown";
        set_expected_results(test, NULL, NULL, NULL, 1);
        treedb_close_db(tranger, treedb_name);
        tranger2_shutdown(tranger);
        result += test_json(NULL);
    }

    return result;
}

/***************************************************************************
 *              Main
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

    unsigned long memory_check_list[] = {0, 0};
    set_memory_check_list(memory_check_list);

    init_backtrace_with_backtrace(argv[0]);
    set_show_backtrace_fn(show_backtrace_with_backtrace);

    gobj_start_up(
        argc,
        argv,
        NULL, NULL, NULL, NULL, NULL, NULL
    );

    yuno_catch_signals();

    /*--------------------------------*
     *      Log handlers
     *--------------------------------*/
    gobj_log_add_handler("stdout", "stdout", LOG_OPT_ALL, 0);

    gobj_log_register_handler(
        "testing",
        0,
        capture_log_write,
        0
    );
    gobj_log_add_handler("test_capture", "testing", LOG_OPT_UP_INFO, 0);

    /*--------------------------------*
     *      Event loop
     *--------------------------------*/
    yev_loop_create(
        0,
        2024,
        10,
        NULL,
        &yev_loop
    );

    /*--------------------------------*
     *      Test
     *--------------------------------*/
    int result = do_test();

    yev_loop_stop(yev_loop);
    yev_loop_destroy(yev_loop);

    gobj_end();

    if(get_cur_system_memory() != 0) {
        printf("%sERROR --> %s%s\n", On_Red BWhite, "system memory not free", Color_Off);
        print_track_mem();
        result += -1;
    }

    if(result < 0) {
        printf("<-- %sTEST FAILED%s: %s\n", On_Red BWhite, Color_Off, APP);
    }
    return result < 0 ? -1 : 0;
}

/***************************************************************************
 *              Signal handlers
 ***************************************************************************/
PRIVATE void quit_sighandler(int sig)
{
    static int xtimes_once = 0;
    xtimes_once++;
    yev_loop_reset_running(yev_loop);
    if(xtimes_once > 1) {
        exit(-1);
    }
}

PUBLIC void yuno_catch_signals(void)
{
    struct sigaction sigIntHandler;

    signal(SIGPIPE, SIG_IGN);
    signal(SIGTERM, SIG_IGN);

    memset(&sigIntHandler, 0, sizeof(sigIntHandler));
    sigIntHandler.sa_handler = quit_sighandler;
    sigemptyset(&sigIntHandler.sa_mask);
    sigIntHandler.sa_flags = SA_NODEFER|SA_RESTART;
    sigaction(SIGALRM, &sigIntHandler, NULL);
    sigaction(SIGQUIT, &sigIntHandler, NULL);
    sigaction(SIGINT, &sigIntHandler, NULL);
}
