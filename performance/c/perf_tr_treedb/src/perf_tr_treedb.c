/****************************************************************************
 *          perf_tr_treedb.c
 *
 *          Benchmark of the treedb writes (tr_treedb over timeranger2),
 *          without the C_TREEDB / C_NODE gclasses.
 *
 *          Usage:  perf_tr_treedb [--small] [N]
 *
 *          Cases, on one treedb of two topics (users, departments) linked
 *          by a list fkey (users.departments -> departments.users):
 *            update_memory     N updates of one node, not saved
 *            update_saved      N updates of one node, saved
 *            link_unlink       N/2 links + N/2 unlinks of one user to a
 *                              department (each saves the child)
 *            create_link_half  N/50 creates, half of them linked
 *            reopen            the open of the treedb: every node and link
 *            delete_force      N/50 forced deletes (half of them linked)
 *            delete_parent     N/5000 forced deletes of a parent with 200
 *                              children (each child is unlinked and saved)
 *          N is 100000 by default; --small is N 10000 (for ctest: it checks
 *          that the benchmark builds and runs, its figures are not the
 *          reference). A link callback is set, as C_NODE does, and every
 *          event it gets is counted.
 *
 *          Every result is one line of JSON on stdout:
 *            {"bench": "perf_tr_treedb", "case": "<case>",
 *             "seconds": <s>, "ops": <n>, "us_per_op": <us>, "events": <n>}
 *          The first line says the version, the build and N. The store
 *          lives in ~/tests_yuneta/perf_tr_treedb and is removed at the end.
 *
 *          The figures of a release are taken on the same machine with this
 *          benchmark linked against the tr_treedb of both releases, run
 *          alternated, and the medians compared (see README.md).
 *
 *          Copyright (c) 2026, ArtGins.
 *          All Rights Reserved.
 ****************************************************************************/
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <time.h>

#include <yuneta_version.h>
#include <yuneta_config.h>
#include <gobj.h>
#include <timeranger2.h>
#include <helpers.h>
#include <kwid.h>
#include <tr_treedb.h>

/***************************************************************
 *              Constants
 ***************************************************************/
#define BENCH       "perf_tr_treedb"
#define DATABASE    "perf_tr_treedb"
#define TREEDB_NAME "treedb_perf"
#define DELETE_PARENT_CHILDREN  200

/*
 *  users.departments is a list fkey to departments.users;
 *  departments.department_id is a single fkey to departments.departments
 */
PRIVATE char schema_perf[]= "\
{                                                                   \n\
    'id': 'treedb_perf',                                            \n\
    'schema_version': 1,                                            \n\
    'topics': [                                                     \n\
        {                                                           \n\
            'topic_name': 'users',                                  \n\
            'pkey': 'id',                                           \n\
            'system_flag': 'sf_string_key',                         \n\
            'cols': {                                               \n\
                'id': {                                             \n\
                    'header': 'Id',                                 \n\
                    'type': 'string',                               \n\
                    'flag': ['persistent','required']               \n\
                },                                                  \n\
                'username': {                                       \n\
                    'header': 'User Name',                          \n\
                    'type': 'string',                               \n\
                    'flag': ['persistent']                          \n\
                },                                                  \n\
                'departments': {                                    \n\
                    'header': 'Departments',                        \n\
                    'type': 'array',                                \n\
                    'flag': ['fkey']                                \n\
                }                                                   \n\
            }                                                       \n\
        },                                                          \n\
        {                                                           \n\
            'topic_name': 'departments',                            \n\
            'pkey': 'id',                                           \n\
            'system_flag': 'sf_string_key',                         \n\
            'cols': {                                               \n\
                'id': {                                             \n\
                    'header': 'Id',                                 \n\
                    'type': 'string',                               \n\
                    'flag': ['persistent','required']               \n\
                },                                                  \n\
                'name': {                                           \n\
                    'header': 'Name',                               \n\
                    'type': 'string',                               \n\
                    'flag': ['persistent']                          \n\
                },                                                  \n\
                'department_id': {                                  \n\
                    'header': 'Top Department',                     \n\
                    'type': 'string',                               \n\
                    'flag': ['fkey']                                \n\
                },                                                  \n\
                'departments': {                                    \n\
                    'header': 'Departments',                        \n\
                    'type': 'object',                               \n\
                    'flag': ['hook'],                               \n\
                    'hook': {                                       \n\
                        'departments': 'department_id'              \n\
                    }                                               \n\
                },                                                  \n\
                'users': {                                          \n\
                    'header': 'Users',                              \n\
                    'type': 'array',                                \n\
                    'flag': ['hook'],                               \n\
                    'hook': {                                       \n\
                        'users': 'departments'                      \n\
                    }                                               \n\
                }                                                   \n\
            }                                                       \n\
        }                                                           \n\
    ]                                                               \n\
}                                                                   \n\
";


/***************************************************************
 *              Data
 ***************************************************************/
PRIVATE int N = 100000;
PRIVATE long events_told = 0;

/***************************************************************
 *              Helpers
 ***************************************************************/
PRIVATE double now_s(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (double)ts.tv_sec + (double)ts.tv_nsec/1e9;
}

PRIVATE void result_line(const char *name, double seconds, int ops)
{
    printf("{\"bench\": \"%s\", \"case\": \"%s\", \"seconds\": %.6f, \"ops\": %d, \"us_per_op\": %.3f, \"events\": %ld}\n",
        BENCH,
        name,
        seconds,
        ops,
        ops > 0? seconds*1e6/ops : 0.0,
        events_told
    );
    fflush(stdout);
}

PRIVATE int treedb_callback(
    void *user_data,
    json_t *tranger,
    const char *treedb_name,
    const char *topic_name,
    const char *operation,
    json_t *node
)
{
    events_told++;
    JSON_DECREF(node)
    return 0;
}

PRIVATE int open_treedb(json_t *tranger)
{
    json_t *jn_schema = legalstring2json(schema_perf, TRUE);
    if(!treedb_open_db(tranger, TREEDB_NAME, jn_schema, "persistent")) {
        return -1;  // Error already logged
    }
    treedb_set_callback(tranger, TREEDB_NAME, treedb_callback, NULL, TREEDB_CALLBACK_LINK_EVENTS);
    return 0;
}

/***************************************************************
 *              Benchmark
 ***************************************************************/
PRIVATE int do_bench(const char *path_root)
{
    json_t *tranger = tranger2_startup(
        0,
        json_pack("{s:s, s:s, s:b, s:i}",
            "path", path_root,
            "database", DATABASE,
            "master", 1,
            "on_critical_error", 0
        ),
        0
    );
    if(!tranger) {
        return -1;  // Error already logged
    }
    helper_quote2doublequote(schema_perf);
    if(open_treedb(tranger) < 0) {
        tranger2_shutdown(tranger);
        return -1;
    }

    json_t *alice = treedb_create_node(tranger, TREEDB_NAME, "users",
        json_pack("{s:s, s:s}", "id", "alice", "username", "alice"));
    json_t *direction = treedb_create_node(tranger, TREEDB_NAME, "departments",
        json_pack("{s:s, s:s}", "id", "direction", "name", "Direction"));
    if(!alice || !direction) {
        treedb_close_db(tranger, TREEDB_NAME);
        tranger2_shutdown(tranger);
        return -1;  // Error already logged
    }

    int result = 0;
    char name[NAME_MAX];

    events_told = 0;
    double t0 = now_s();
    for(int i = 0; i < N; i++) {
        snprintf(name, sizeof(name), "alice %d", i);
        if(!treedb_update_node(tranger, alice, json_pack("{s:s}", "username", name), FALSE)) {
            result = -1;  // Error already logged
            break;
        }
    }
    result_line("update_memory", now_s() - t0, N);

    events_told = 0;
    t0 = now_s();
    for(int i = 0; i < N; i++) {
        snprintf(name, sizeof(name), "alice %d", i);
        if(!treedb_update_node(tranger, alice, json_pack("{s:s}", "username", name), TRUE)) {
            result = -1;  // Error already logged
            break;
        }
    }
    result_line("update_saved", now_s() - t0, N);

    events_told = 0;
    int n2 = N/2;
    t0 = now_s();
    for(int i = 0; i < n2; i++) {
        if(treedb_link_nodes(tranger, "users", direction, alice) < 0 ||
                treedb_unlink_nodes(tranger, "users", direction, alice) < 0) {
            result = -1;  // Error already logged
            break;
        }
    }
    result_line("link_unlink", now_s() - t0, 2*n2);

    int nc = N/50;
    events_told = 0;
    t0 = now_s();
    for(int i = 0; i < nc; i++) {
        snprintf(name, sizeof(name), "u%06d", i);
        json_t *u = treedb_create_node(tranger, TREEDB_NAME, "users",
            json_pack("{s:s, s:s}", "id", name, "username", name));
        if(!u) {
            result = -1;  // Error already logged
            break;
        }
        if(i % 2 == 0 && treedb_link_nodes(tranger, "users", direction, u) < 0) {
            result = -1;  // Error already logged
            break;
        }
    }
    result_line("create_link_half", now_s() - t0, nc);

    treedb_close_db(tranger, TREEDB_NAME);

    events_told = 0;
    t0 = now_s();
    if(open_treedb(tranger) < 0) {
        tranger2_shutdown(tranger);
        return -1;
    }
    result_line("reopen", now_s() - t0, nc + 2);

    direction = treedb_get_node(tranger, TREEDB_NAME, "departments", "direction");
    size_t linked = json_array_size(json_object_get(direction, "users"));
    if(linked != (size_t)((nc + 1)/2)) {
        printf("{\"bench\": \"%s\", \"error\": \"reopen: %d users linked, %d expected\"}\n",
            BENCH, (int)linked, (nc + 1)/2);
        result = -1;
    }

    events_told = 0;
    t0 = now_s();
    for(int i = 0; i < nc; i++) {
        snprintf(name, sizeof(name), "u%06d", i);
        json_t *u = treedb_get_node(tranger, TREEDB_NAME, "users", name);
        if(!u || treedb_delete_node(tranger, u, json_pack("{s:b}", "force", 1)) < 0) {
            result = -1;  // Error already logged
            break;
        }
    }
    result_line("delete_force", now_s() - t0, nc);

    /*
     *  Forced deletes of a PARENT: each one unlinks and saves its
     *  children. Only the delete is timed, not the build of the family.
     */
    int np = N/5000 > 0? N/5000 : 1;
    double seconds = 0;
    long delete_events = 0;
    for(int p = 0; p < np && result == 0; p++) {
        snprintf(name, sizeof(name), "parent%04d", p);
        json_t *parent = treedb_create_node(tranger, TREEDB_NAME, "departments",
            json_pack("{s:s}", "id", name));
        if(!parent) {
            result = -1;  // Error already logged
            break;
        }
        for(int k = 0; k < DELETE_PARENT_CHILDREN; k++) {
            char child_id[NAME_MAX];
            snprintf(child_id, sizeof(child_id), "p%04d_c%04d", p, k);
            json_t *child = treedb_create_node(tranger, TREEDB_NAME, "users",
                json_pack("{s:s}", "id", child_id));
            if(!child || treedb_link_nodes(tranger, "users", parent, child) < 0) {
                result = -1;  // Error already logged
                break;
            }
        }
        if(result < 0) {
            break;
        }
        events_told = 0;
        t0 = now_s();
        if(treedb_delete_node(tranger, parent, json_pack("{s:b}", "force", 1)) < 0) {
            result = -1;  // Error already logged
        }
        seconds += now_s() - t0;
        delete_events += events_told;
    }
    events_told = delete_events;
    result_line("delete_parent", seconds, np);

    treedb_close_db(tranger, TREEDB_NAME);
    tranger2_shutdown(tranger);
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
    gbmem_get_allocators(&malloc_func, &realloc_func, &calloc_func, &free_func);
    json_set_alloc_funcs(malloc_func, free_func);

    unsigned long memory_check_list[] = {0, 0};
    set_memory_check_list(memory_check_list);

    gobj_start_up(argc, argv, NULL, NULL, NULL, NULL, NULL, NULL);
    gobj_log_add_handler("stdout", "stdout", LOG_OPT_UP_WARNING, 0);

    for(int i = 1; i < argc; i++) {
        if(strcmp(argv[i], "--small") == 0) {
            N = 10000;
        } else if(atoi(argv[i]) > 0) {
            N = atoi(argv[i]);
        } else {
            printf("Usage: %s [--small] [N]\n", argv[0]);
            gobj_end();
            return -1;
        }
    }

    char path_root[PATH_MAX];
    char path_database[PATH_MAX];
    build_path(path_root, sizeof(path_root), getenv("HOME"), "tests_yuneta", NULL);
    mkrdir(path_root, 02770);
    build_path(path_database, sizeof(path_database), path_root, DATABASE, NULL);
    rmrdir(path_database);

#ifdef CONFIG_DEBUG_TRACK_MEMORY
    int track_memory = 1;
#else
    int track_memory = 0;
#endif
    printf("{\"bench\": \"%s\", \"yuneta_version\": \"%s\", \"track_memory\": %d, \"n\": %d}\n",
        BENCH, YUNETA_VERSION, track_memory, N
    );

    int ret = do_bench(path_root);
    rmrdir(path_database);

    gobj_end();

    if(get_cur_system_memory() != 0) {
        printf("{\"bench\": \"%s\", \"error\": \"system memory not free\"}\n", BENCH);
        ret = -1;
    }
    if(ret < 0) {
        printf("{\"bench\": \"%s\", \"result\": \"FAILED\"}\n", BENCH);
        return -1;
    }
    return 0;
}
