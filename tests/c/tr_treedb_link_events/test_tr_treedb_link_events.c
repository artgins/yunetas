/****************************************************************************
 *          test_tr_treedb_link_events.c
 *
 *          Low-level tests for EV_TREEDB_NODE_LINKED / EV_TREEDB_NODE_UNLINKED
 *          Tests the tr_treedb.c callback mechanism directly.
 *
 *          Copyright (c) 2024, ArtGins.
 *          All Rights Reserved.
 ****************************************************************************/
#include <stdlib.h>
#include <string.h>
#include <locale.h>
#include <time.h>
#include <signal.h>
#include <limits.h>

#include <gobj.h>
#include <timeranger2.h>
#include <helpers.h>
#include <yev_loop.h>
#include <kwid.h>
#include <testing.h>
#include <tr_treedb.h>

#include "schema_sample.c"

#define APP "test_tr_treedb_link_events"

/***************************************************************************
 *      Constants
 ***************************************************************************/
#define DATABASE    "tr_treedb_link_events"
#define MAX_LINK_EVENTS 64

/***************************************************************
 *              Prototypes
 ***************************************************************/
PRIVATE void yuno_catch_signals(void);

/***************************************************************************
 *      Callback tracking data
 ***************************************************************************/
typedef struct {
    const char *operation;
    char hook_name[64];
    char parent_topic_name[64];
    char child_topic_name[64];
    char parent_id[64];
    char child_id[64];
} link_event_record_t;

PRIVATE link_event_record_t link_events[MAX_LINK_EVENTS];
PRIVATE int link_event_count = 0;

PRIVATE int total_created_events = 0;
PRIVATE int total_updated_events = 0;
PRIVATE int total_deleted_events = 0;
PRIVATE int total_linked_events = 0;
PRIVATE int total_unlinked_events = 0;

PRIVATE void reset_event_counters(void)
{
    link_event_count = 0;
    total_created_events = 0;
    total_updated_events = 0;
    total_deleted_events = 0;
    total_linked_events = 0;
    total_unlinked_events = 0;
    memset(link_events, 0, sizeof(link_events));
}

/***************************************************************************
 *      Data
 ***************************************************************************/
PRIVATE yev_loop_h yev_loop;
PRIVATE int global_result = 0;

/***************************************************************************
 *  Callback for link events mode (TREEDB_CALLBACK_LINK_EVENTS)
 ***************************************************************************/
PRIVATE int treedb_callback_with_link_events(
    void *user_data,
    json_t *tranger,
    const char *treedb_name,
    const char *topic_name,
    const char *operation,
    json_t *node    // owned
)
{
    if(operation == EV_TREEDB_NODE_LINKED) {
        total_linked_events++;
        if(link_event_count < MAX_LINK_EVENTS) {
            link_event_record_t *rec = &link_events[link_event_count++];
            rec->operation = operation;
            snprintf(rec->hook_name, sizeof(rec->hook_name), "%s",
                json_string_value(json_object_get(node, "hook_name")));
            snprintf(rec->parent_topic_name, sizeof(rec->parent_topic_name), "%s",
                json_string_value(json_object_get(node, "parent_topic_name")));
            snprintf(rec->child_topic_name, sizeof(rec->child_topic_name), "%s",
                json_string_value(json_object_get(node, "child_topic_name")));
            snprintf(rec->parent_id, sizeof(rec->parent_id), "%s",
                json_string_value(json_object_get(node, "parent_id")));
            snprintf(rec->child_id, sizeof(rec->child_id), "%s",
                json_string_value(json_object_get(node, "child_id")));
        }
    } else if(operation == EV_TREEDB_NODE_UNLINKED) {
        total_unlinked_events++;
        if(link_event_count < MAX_LINK_EVENTS) {
            link_event_record_t *rec = &link_events[link_event_count++];
            rec->operation = operation;
            snprintf(rec->hook_name, sizeof(rec->hook_name), "%s",
                json_string_value(json_object_get(node, "hook_name")));
            snprintf(rec->parent_topic_name, sizeof(rec->parent_topic_name), "%s",
                json_string_value(json_object_get(node, "parent_topic_name")));
            snprintf(rec->child_topic_name, sizeof(rec->child_topic_name), "%s",
                json_string_value(json_object_get(node, "child_topic_name")));
            snprintf(rec->parent_id, sizeof(rec->parent_id), "%s",
                json_string_value(json_object_get(node, "parent_id")));
            snprintf(rec->child_id, sizeof(rec->child_id), "%s",
                json_string_value(json_object_get(node, "child_id")));
        }
    } else if(operation == EV_TREEDB_NODE_CREATED) {
        total_created_events++;
    } else if(operation == EV_TREEDB_NODE_UPDATED) {
        total_updated_events++;
    } else if(operation == EV_TREEDB_NODE_DELETED) {
        total_deleted_events++;
    }

    json_decref(node);
    return 0;
}

/***************************************************************************
 *  Callback for backward-compatible mode (no link events flag)
 ***************************************************************************/
PRIVATE int backward_updated_count = 0;

PRIVATE int treedb_callback_without_link_events(
    void *user_data,
    json_t *tranger,
    const char *treedb_name,
    const char *topic_name,
    const char *operation,
    json_t *node    // owned
)
{
    if(operation == EV_TREEDB_NODE_UPDATED) {
        backward_updated_count++;
    }
    json_decref(node);
    return 0;
}

/***************************************************************************
 *  Test 1: Basic link event fires EV_TREEDB_NODE_LINKED
 ***************************************************************************/
PRIVATE int test_link_fires_linked_event(
    json_t *tranger,
    const char *treedb_name
)
{
    int result = 0;
    const char *test = "link fires EV_TREEDB_NODE_LINKED";
    time_measure_t time_measure;

    set_expected_results(test, NULL, NULL, NULL, 1);
    MT_START_TIME(time_measure)

    reset_event_counters();

    /*
     *  Create nodes
     */
    json_t *dept_direction = treedb_create_node(
        tranger, treedb_name, "departments",
        json_pack("{s:s, s:s}", "id", "direction", "name", "Direction")
    );
    json_t *dept_admin = treedb_create_node(
        tranger, treedb_name, "departments",
        json_pack("{s:s, s:s}", "id", "administration", "name", "Administration")
    );

    reset_event_counters();

    /*
     *  Link: direction -> departments -> administration
     */
    treedb_link_nodes(
        tranger,
        "departments",      // hook_name
        dept_direction,     // parent
        dept_admin          // child
    );

    /*
     *  Verify: exactly 1 linked event.
     *  NOTE: treedb_save_node (called by _link_nodes for child) also fires
     *  EV_TREEDB_NODE_UPDATED for the saved child — that's expected.
     */
    if(total_linked_events != 1) {
        printf("%s  --> ERROR %s: expected 1 linked event, got %d%s\n",
            On_Red BWhite, test, total_linked_events, Color_Off);
        result += -1;
    }

    /*
     *  Verify event payload
     */
    if(link_event_count >= 1) {
        link_event_record_t *rec = &link_events[0];
        if(rec->operation != EV_TREEDB_NODE_LINKED) {
            printf("%s  --> ERROR %s: wrong operation%s\n", On_Red BWhite, test, Color_Off);
            result += -1;
        }
        if(strcmp(rec->hook_name, "departments") != 0) {
            printf("%s  --> ERROR %s: expected hook_name 'departments', got '%s'%s\n",
                On_Red BWhite, test, rec->hook_name, Color_Off);
            result += -1;
        }
        if(strcmp(rec->parent_topic_name, "departments") != 0) {
            printf("%s  --> ERROR %s: expected parent_topic 'departments', got '%s'%s\n",
                On_Red BWhite, test, rec->parent_topic_name, Color_Off);
            result += -1;
        }
        if(strcmp(rec->child_topic_name, "departments") != 0) {
            printf("%s  --> ERROR %s: expected child_topic 'departments', got '%s'%s\n",
                On_Red BWhite, test, rec->child_topic_name, Color_Off);
            result += -1;
        }
        if(strcmp(rec->parent_id, "direction") != 0) {
            printf("%s  --> ERROR %s: expected parent_id 'direction', got '%s'%s\n",
                On_Red BWhite, test, rec->parent_id, Color_Off);
            result += -1;
        }
        if(strcmp(rec->child_id, "administration") != 0) {
            printf("%s  --> ERROR %s: expected child_id 'administration', got '%s'%s\n",
                On_Red BWhite, test, rec->child_id, Color_Off);
            result += -1;
        }
    }

    MT_INCREMENT_COUNT(time_measure, 1)
    MT_PRINT_TIME(time_measure, test)
    result += test_json(NULL);

    return result;
}

/***************************************************************************
 *  Test 2: Unlink event fires EV_TREEDB_NODE_UNLINKED
 ***************************************************************************/
PRIVATE int test_unlink_fires_unlinked_event(
    json_t *tranger,
    const char *treedb_name
)
{
    int result = 0;
    const char *test = "unlink fires EV_TREEDB_NODE_UNLINKED";
    time_measure_t time_measure;

    set_expected_results(test, NULL, NULL, NULL, 1);
    MT_START_TIME(time_measure)

    reset_event_counters();

    json_t *dept_direction = treedb_get_node(
        tranger, treedb_name, "departments", "direction"
    );
    json_t *dept_admin = treedb_get_node(
        tranger, treedb_name, "departments", "administration"
    );

    /*
     *  Unlink: direction -/-> administration
     */
    treedb_unlink_nodes(
        tranger,
        "departments",
        dept_direction,
        dept_admin
    );

    /*
     *  Verify: exactly 1 unlinked event, 0 linked.
     *  NOTE: save_node also fires EV_TREEDB_NODE_UPDATED for child — expected.
     */
    if(total_unlinked_events != 1) {
        printf("%s  --> ERROR %s: expected 1 unlinked event, got %d%s\n",
            On_Red BWhite, test, total_unlinked_events, Color_Off);
        result += -1;
    }
    if(total_linked_events != 0) {
        printf("%s  --> ERROR %s: expected 0 linked events, got %d%s\n",
            On_Red BWhite, test, total_linked_events, Color_Off);
        result += -1;
    }

    /*
     *  Verify unlink event payload
     */
    if(link_event_count >= 1) {
        link_event_record_t *rec = &link_events[0];
        if(rec->operation != EV_TREEDB_NODE_UNLINKED) {
            printf("%s  --> ERROR %s: wrong operation%s\n", On_Red BWhite, test, Color_Off);
            result += -1;
        }
        if(strcmp(rec->hook_name, "departments") != 0) {
            printf("%s  --> ERROR %s: expected hook_name 'departments', got '%s'%s\n",
                On_Red BWhite, test, rec->hook_name, Color_Off);
            result += -1;
        }
        if(strcmp(rec->parent_id, "direction") != 0) {
            printf("%s  --> ERROR %s: expected parent_id 'direction', got '%s'%s\n",
                On_Red BWhite, test, rec->parent_id, Color_Off);
            result += -1;
        }
        if(strcmp(rec->child_id, "administration") != 0) {
            printf("%s  --> ERROR %s: expected child_id 'administration', got '%s'%s\n",
                On_Red BWhite, test, rec->child_id, Color_Off);
            result += -1;
        }
    }

    MT_INCREMENT_COUNT(time_measure, 1)
    MT_PRINT_TIME(time_measure, test)
    result += test_json(NULL);

    return result;
}

/***************************************************************************
 *  Test 3: Cross-topic link (department -> users hook)
 ***************************************************************************/
PRIVATE int test_cross_topic_link_event(
    json_t *tranger,
    const char *treedb_name
)
{
    int result = 0;
    const char *test = "cross-topic link fires correct event";
    time_measure_t time_measure;

    set_expected_results(test, NULL, NULL, NULL, 1);
    MT_START_TIME(time_measure)

    /*
     *  Create a user
     */
    json_t *user_alice = treedb_create_node(
        tranger, treedb_name, "users",
        json_pack("{s:s, s:s}", "id", "alice", "username", "alice_w")
    );

    json_t *dept_admin = treedb_get_node(
        tranger, treedb_name, "departments", "administration"
    );

    reset_event_counters();

    /*
     *  Link: administration -> users -> alice
     */
    treedb_link_nodes(
        tranger,
        "users",            // hook_name (on department)
        dept_admin,         // parent
        user_alice          // child
    );

    /*
     *  Verify cross-topic link event
     */
    if(total_linked_events != 1) {
        printf("%s  --> ERROR %s: expected 1 linked event, got %d%s\n",
            On_Red BWhite, test, total_linked_events, Color_Off);
        result += -1;
    }
    if(link_event_count >= 1) {
        link_event_record_t *rec = &link_events[0];
        if(strcmp(rec->hook_name, "users") != 0) {
            printf("%s  --> ERROR %s: expected hook_name 'users', got '%s'%s\n",
                On_Red BWhite, test, rec->hook_name, Color_Off);
            result += -1;
        }
        if(strcmp(rec->parent_topic_name, "departments") != 0) {
            printf("%s  --> ERROR %s: expected parent_topic 'departments', got '%s'%s\n",
                On_Red BWhite, test, rec->parent_topic_name, Color_Off);
            result += -1;
        }
        if(strcmp(rec->child_topic_name, "users") != 0) {
            printf("%s  --> ERROR %s: expected child_topic 'users', got '%s'%s\n",
                On_Red BWhite, test, rec->child_topic_name, Color_Off);
            result += -1;
        }
        if(strcmp(rec->parent_id, "administration") != 0) {
            printf("%s  --> ERROR %s: expected parent_id 'administration', got '%s'%s\n",
                On_Red BWhite, test, rec->parent_id, Color_Off);
            result += -1;
        }
        if(strcmp(rec->child_id, "alice") != 0) {
            printf("%s  --> ERROR %s: expected child_id 'alice', got '%s'%s\n",
                On_Red BWhite, test, rec->child_id, Color_Off);
            result += -1;
        }
    }

    MT_INCREMENT_COUNT(time_measure, 1)
    MT_PRINT_TIME(time_measure, test)
    result += test_json(NULL);

    return result;
}

/***************************************************************************
 *  Test 4: Multiple links produce multiple events
 ***************************************************************************/
PRIVATE int test_multiple_link_events(
    json_t *tranger,
    const char *treedb_name
)
{
    int result = 0;
    const char *test = "multiple links produce multiple events";
    time_measure_t time_measure;

    set_expected_results(test, NULL, NULL, NULL, 1);
    MT_START_TIME(time_measure)

    json_t *user_bob = treedb_create_node(
        tranger, treedb_name, "users",
        json_pack("{s:s, s:s}", "id", "bob", "username", "bob_m")
    );
    json_t *user_carol = treedb_create_node(
        tranger, treedb_name, "users",
        json_pack("{s:s, s:s}", "id", "carol", "username", "carol_s")
    );

    json_t *dept_direction = treedb_get_node(
        tranger, treedb_name, "departments", "direction"
    );

    reset_event_counters();

    /*
     *  Link two users to direction via managers hook
     */
    treedb_link_nodes(tranger, "managers", dept_direction, user_bob);
    treedb_link_nodes(tranger, "managers", dept_direction, user_carol);

    if(total_linked_events != 2) {
        printf("%s  --> ERROR %s: expected 2 linked events, got %d%s\n",
            On_Red BWhite, test, total_linked_events, Color_Off);
        result += -1;
    }

    /*
     *  Verify first event is bob, second is carol
     */
    if(link_event_count >= 2) {
        if(strcmp(link_events[0].child_id, "bob") != 0) {
            printf("%s  --> ERROR %s: first event child should be 'bob', got '%s'%s\n",
                On_Red BWhite, test, link_events[0].child_id, Color_Off);
            result += -1;
        }
        if(strcmp(link_events[0].hook_name, "managers") != 0) {
            printf("%s  --> ERROR %s: first event hook should be 'managers', got '%s'%s\n",
                On_Red BWhite, test, link_events[0].hook_name, Color_Off);
            result += -1;
        }
        if(strcmp(link_events[1].child_id, "carol") != 0) {
            printf("%s  --> ERROR %s: second event child should be 'carol', got '%s'%s\n",
                On_Red BWhite, test, link_events[1].child_id, Color_Off);
            result += -1;
        }
    }

    MT_INCREMENT_COUNT(time_measure, 1)
    MT_PRINT_TIME(time_measure, test)
    result += test_json(NULL);

    return result;
}

/***************************************************************************
 *  Test 5: Link + unlink sequence produces correct event order
 ***************************************************************************/
PRIVATE int test_link_unlink_sequence(
    json_t *tranger,
    const char *treedb_name
)
{
    int result = 0;
    const char *test = "link+unlink sequence produces correct event order";
    time_measure_t time_measure;

    set_expected_results(test, NULL, NULL, NULL, 1);
    MT_START_TIME(time_measure)

    json_t *dept_direction = treedb_get_node(
        tranger, treedb_name, "departments", "direction"
    );
    json_t *dept_admin = treedb_get_node(
        tranger, treedb_name, "departments", "administration"
    );

    reset_event_counters();

    /*
     *  Re-link then unlink
     */
    treedb_link_nodes(tranger, "departments", dept_direction, dept_admin);
    treedb_unlink_nodes(tranger, "departments", dept_direction, dept_admin);

    if(total_linked_events != 1) {
        printf("%s  --> ERROR %s: expected 1 linked, got %d%s\n",
            On_Red BWhite, test, total_linked_events, Color_Off);
        result += -1;
    }
    if(total_unlinked_events != 1) {
        printf("%s  --> ERROR %s: expected 1 unlinked, got %d%s\n",
            On_Red BWhite, test, total_unlinked_events, Color_Off);
        result += -1;
    }
    if(link_event_count >= 2) {
        if(link_events[0].operation != EV_TREEDB_NODE_LINKED) {
            printf("%s  --> ERROR %s: first event should be LINKED%s\n",
                On_Red BWhite, test, Color_Off);
            result += -1;
        }
        if(link_events[1].operation != EV_TREEDB_NODE_UNLINKED) {
            printf("%s  --> ERROR %s: second event should be UNLINKED%s\n",
                On_Red BWhite, test, Color_Off);
            result += -1;
        }
    }

    MT_INCREMENT_COUNT(time_measure, 1)
    MT_PRINT_TIME(time_measure, test)
    result += test_json(NULL);

    return result;
}

/***************************************************************************
 *  Test 6: Backward compatibility — no flag means EV_TREEDB_NODE_UPDATED
 ***************************************************************************/
PRIVATE int test_backward_compatibility(
    json_t *tranger,
    const char *treedb_name
)
{
    int result = 0;
    const char *test = "backward compat: no flag sends EV_TREEDB_NODE_UPDATED";
    time_measure_t time_measure;

    set_expected_results(test, NULL, NULL, NULL, 1);
    MT_START_TIME(time_measure)

    /*
     *  Switch callback to backward-compatible mode (no LINK_EVENTS flag)
     */
    backward_updated_count = 0;
    treedb_set_callback(
        tranger,
        treedb_name,
        treedb_callback_without_link_events,
        NULL,
        TREEDB_CALLBACK_NO_FLAG
    );

    json_t *dept_direction = treedb_get_node(
        tranger, treedb_name, "departments", "direction"
    );
    json_t *dept_admin = treedb_get_node(
        tranger, treedb_name, "departments", "administration"
    );

    /*
     *  Link — should fire EV_TREEDB_NODE_UPDATED (not LINKED).
     *  Expect 2 UPDATED: one from _link_nodes (parent), one from save_node (child).
     */
    treedb_link_nodes(tranger, "departments", dept_direction, dept_admin);

    if(backward_updated_count != 2) {
        printf("%s  --> ERROR %s: expected 2 updated events in backward mode after link, got %d%s\n",
            On_Red BWhite, test, backward_updated_count, Color_Off);
        result += -1;
    }

    /*
     *  Unlink — should also fire 2 EV_TREEDB_NODE_UPDATED (parent + child save).
     */
    treedb_unlink_nodes(tranger, "departments", dept_direction, dept_admin);

    if(backward_updated_count != 4) {
        printf("%s  --> ERROR %s: expected 4 updated events in backward mode after link+unlink, got %d%s\n",
            On_Red BWhite, test, backward_updated_count, Color_Off);
        result += -1;
    }

    /*
     *  Restore link-events callback for cleanup
     */
    treedb_set_callback(
        tranger,
        treedb_name,
        treedb_callback_with_link_events,
        NULL,
        TREEDB_CALLBACK_LINK_EVENTS
    );

    MT_INCREMENT_COUNT(time_measure, 1)
    MT_PRINT_TIME(time_measure, test)
    result += test_json(NULL);

    return result;
}

/***************************************************************************
 *  Test 7: Managers hook links user (cross-topic child type)
 ***************************************************************************/
PRIVATE int test_managers_hook_link(
    json_t *tranger,
    const char *treedb_name
)
{
    int result = 0;
    const char *test = "managers hook cross-topic link event";
    time_measure_t time_measure;

    set_expected_results(test, NULL, NULL, NULL, 1);
    MT_START_TIME(time_measure)

    json_t *dept_admin = treedb_get_node(
        tranger, treedb_name, "departments", "administration"
    );
    json_t *user_alice = treedb_get_node(
        tranger, treedb_name, "users", "alice"
    );

    reset_event_counters();

    /*
     *  Link alice as manager of administration
     *  managers hook: {'users': 'manager', 'departments': 'users'}
     *  Linking a user → user's 'manager' fkey is updated
     */
    treedb_link_nodes(tranger, "managers", dept_admin, user_alice);

    if(total_linked_events != 1) {
        printf("%s  --> ERROR %s: expected 1 linked event, got %d%s\n",
            On_Red BWhite, test, total_linked_events, Color_Off);
        result += -1;
    }
    if(link_event_count >= 1) {
        link_event_record_t *rec = &link_events[0];
        if(strcmp(rec->hook_name, "managers") != 0) {
            printf("%s  --> ERROR %s: expected hook_name 'managers', got '%s'%s\n",
                On_Red BWhite, test, rec->hook_name, Color_Off);
            result += -1;
        }
        if(strcmp(rec->parent_topic_name, "departments") != 0) {
            printf("%s  --> ERROR %s: expected parent_topic 'departments', got '%s'%s\n",
                On_Red BWhite, test, rec->parent_topic_name, Color_Off);
            result += -1;
        }
        if(strcmp(rec->child_topic_name, "users") != 0) {
            printf("%s  --> ERROR %s: expected child_topic 'users', got '%s'%s\n",
                On_Red BWhite, test, rec->child_topic_name, Color_Off);
            result += -1;
        }
    }

    /*
     *  Unlink to clean up
     */
    reset_event_counters();
    treedb_unlink_nodes(tranger, "managers", dept_admin, user_alice);

    if(total_unlinked_events != 1) {
        printf("%s  --> ERROR %s: expected 1 unlinked event on cleanup, got %d%s\n",
            On_Red BWhite, test, total_unlinked_events, Color_Off);
        result += -1;
    }

    MT_INCREMENT_COUNT(time_measure, 1)
    MT_PRINT_TIME(time_measure, test)
    result += test_json(NULL);

    return result;
}

/***************************************************************************
 *              Test
 ***************************************************************************/
PRIVATE int do_test(void)
{
    int result = 0;

    /*
     *  Write the tests in ~/tests_yuneta/
     */
    const char *home = getenv("HOME");
    char path_root[PATH_MAX];
    char path_database[PATH_MAX];

    build_path(path_root, sizeof(path_root), home, "tests_yuneta", NULL);
    mkrdir(path_root, 02770);

    build_path(path_database, sizeof(path_database), path_root, DATABASE, NULL);
    rmrdir(path_database);

    /*-------------------------------------------------*
     *      Startup the timeranger db
     *-------------------------------------------------*/
    json_t *tranger;
    {
        const char *test = "open tranger";
        set_expected_results(
            test,
            json_pack("[{s:s}]", "msg", "Creating __timeranger2__.json"),
            NULL, NULL, 1
        );
        time_measure_t time_measure;
        MT_START_TIME(time_measure)

        json_t *jn_tranger = json_pack("{s:s, s:s, s:b, s:i}",
            "path", path_root,
            "database", DATABASE,
            "master", 1,
            "on_critical_error", LOG_OPT_TRACE_STACK
        );
        tranger = tranger2_startup(0, jn_tranger, 0);

        MT_INCREMENT_COUNT(time_measure, 1)
        MT_PRINT_TIME(time_measure, test)
        result += test_json(NULL);
    }

    /*------------------------------*
     *      Open treedb
     *------------------------------*/
    const char *treedb_name = "treedb_link_test";
    {
        const char *test = "open treedb";
        set_expected_results(
            test,
            json_pack("[{s:s}, {s:s}, {s:s}, {s:s}]",
                "msg", "Creating topic",
                "msg", "Creating topic",
                "msg", "Creating topic",
                "msg", "Creating topic"
            ),
            NULL, NULL, 1
        );
        time_measure_t time_measure;
        MT_START_TIME(time_measure)

        helper_quote2doublequote(schema_sample);
        json_t *jn_schema = legalstring2json(schema_sample, TRUE);
        if(!jn_schema) {
            printf("Can't decode schema_sample json\n");
            exit(-1);
        }

        if(!treedb_open_db(tranger, treedb_name, jn_schema, 0)) {
            result += -1;
        }

        MT_INCREMENT_COUNT(time_measure, 1)
        MT_PRINT_TIME(time_measure, test)
        result += test_json(NULL);
    }

    /*------------------------------*
     *  Set callback with link events
     *------------------------------*/
    treedb_set_callback(
        tranger,
        treedb_name,
        treedb_callback_with_link_events,
        NULL,
        TREEDB_CALLBACK_LINK_EVENTS
    );

    /*------------------------------*
     *  Execute tests
     *------------------------------*/
    result += test_link_fires_linked_event(tranger, treedb_name);
    result += test_unlink_fires_unlinked_event(tranger, treedb_name);
    result += test_cross_topic_link_event(tranger, treedb_name);
    result += test_multiple_link_events(tranger, treedb_name);
    result += test_link_unlink_sequence(tranger, treedb_name);
    result += test_backward_compatibility(tranger, treedb_name);
    result += test_managers_hook_link(tranger, treedb_name);

    /*
     *  Check refcounts
     */
    json_check_refcounts(tranger, 1000, &result);

    /*------------------------------*
     *      Shutdown
     *------------------------------*/
    {
        const char *test = "Close and shutdown";
        set_expected_results(test, NULL, NULL, NULL, 1);
        time_measure_t time_measure;
        MT_START_TIME(time_measure)

        treedb_close_db(tranger, treedb_name);
        tranger2_shutdown(tranger);

        MT_INCREMENT_COUNT(time_measure, 1)
        MT_PRINT_TIME(time_measure, test)
        result += test_json(NULL);
    }

    return result;
}

/***************************************************************************
 *              Main
 ***************************************************************************/
int main(int argc, char *argv[])
{
    /*----------------------------------*
     *      Startup gobj system
     *----------------------------------*/
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
     *  Create the event loop
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
    result += global_result;

    /*--------------------------------*
     *  Stop the event loop
     *--------------------------------*/
    yev_loop_stop(yev_loop);
    yev_loop_destroy(yev_loop);

    gobj_end();

    if(get_cur_system_memory()!=0) {
        printf("%sERROR%s <-- %s\n", On_Red BWhite, Color_Off, "system memory not free");
        print_track_mem();
        result += -1;
    }

    if(result<0) {
        printf("<-- %sTEST FAILED%s: %s\n", On_Red BWhite, Color_Off, APP);
    }
    return result<0?-1:0;
}

/***************************************************************************
 *      Signal handlers
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
