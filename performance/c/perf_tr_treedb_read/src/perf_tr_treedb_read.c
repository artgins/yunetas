/****************************************************************************
 *          perf_tr_treedb_read.c
 *
 *          Benchmark of the treedb reads (tr_treedb over timeranger2),
 *          without the C_TREEDB / C_NODE gclasses: what a list-nodes or a
 *          get-node answers through.
 *
 *          Usage:  perf_tr_treedb_read [--small] [N]
 *
 *          One treedb of two topics: N users, each linked to one department
 *          and one in four to a second one (users.departments, a list fkey
 *          -> departments.users), and N/40 departments under a top one
 *          (departments.department_id -> departments.departments). Then it
 *          is closed, and:
 *            reopen            the open of the treedb: every node and link,
 *                              per node
 *            list_nodes        treedb_list_nodes() of every user, per user
 *            list_filter       the same with a filter of one column, per
 *                              user
 *            view_users        the views of every user, as a list
 *                              (node_collapsed_view(), default options),
 *                              per user
 *            view_departments  the views of every department (about 50
 *                              users in its hook), per department
 *            get_view          treedb_get_node() + node_collapsed_view() of
 *                              every user, one by one, per user
 *            dump_views        the json text of the views of every user
 *                              (what a command answer carries), per user
 *          N is 8000 by default; --small is N 2000 (for ctest: it checks
 *          that the benchmark builds and runs, its figures are not the
 *          reference). Each list case is repeated R times (R is 10, 2 with
 *          --small).
 *
 *          Every result is one line of JSON on stdout:
 *            {"bench": "perf_tr_treedb_read", "case": "<case>",
 *             "seconds": <s>, "cpu_seconds": <s>, "ops": <n>,
 *             "us_per_op": <us>, "cpu_us_per_op": <us>, "check": <n>}
 *          `seconds`/`us_per_op` are wall clock, `cpu_*` the CPU time of
 *          the process: compare releases with the CPU time. `check` is a
 *          count the case found (nodes listed, users in the hooks, bytes),
 *          the same in every release. The first line says the version, the
 *          build and N. The store lives in
 *          ~/tests_yuneta/perf_tr_treedb_read and is removed at the end.
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
#define BENCH       "perf_tr_treedb_read"
#define DATABASE    "perf_tr_treedb_read"
#define TREEDB_NAME "treedb_perf_read"
#define USERS_PER_DEPARTMENT    40

PRIVATE char schema_perf[]= "\
{                                                                   \n\
    'id': 'treedb_perf_read',                                       \n\
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
                'email': {                                          \n\
                    'header': 'Email',                              \n\
                    'type': 'string',                               \n\
                    'flag': ['persistent', 'email']                 \n\
                },                                                  \n\
                'enabled': {                                        \n\
                    'header': 'Enabled',                            \n\
                    'type': 'boolean',                              \n\
                    'flag': ['persistent', 'writable'],             \n\
                    'default': true                                 \n\
                },                                                  \n\
                'properties': {                                     \n\
                    'header': 'Properties',                         \n\
                    'type': 'object',                               \n\
                    'flag': ['persistent', 'writable']              \n\
                },                                                  \n\
                'time': {                                           \n\
                    'header': 'Time',                               \n\
                    'type': 'integer',                              \n\
                    'flag': ['persistent', 'time']                  \n\
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
PRIVATE int N = 8000;
PRIVATE int R = 10;

/***************************************************************
 *              Helpers
 ***************************************************************/
/*
 *  A point in time: the wall clock, and the CPU time of the process. The
 *  CPU time is the figure to compare between releases: the wall clock
 *  also counts what the other processes of the machine take.
 */
typedef struct {
    double wall;
    double cpu;
} clock_mark_t;

PRIVATE double clock_s(clockid_t clock_id)
{
    struct timespec ts;
    clock_gettime(clock_id, &ts);
    return (double)ts.tv_sec + (double)ts.tv_nsec/1e9;
}

PRIVATE clock_mark_t clock_now(void)
{
    clock_mark_t mark = {clock_s(CLOCK_MONOTONIC), clock_s(CLOCK_PROCESS_CPUTIME_ID)};
    return mark;
}

PRIVATE void result_line_since(const char *name, clock_mark_t t0, long ops, long check)
{
    clock_mark_t t1 = clock_now();
    double wall = t1.wall - t0.wall;
    double cpu = t1.cpu - t0.cpu;
    printf("{\"bench\": \"%s\", \"case\": \"%s\", \"seconds\": %.6f, \"cpu_seconds\": %.6f, \"ops\": %ld, \"us_per_op\": %.3f, \"cpu_us_per_op\": %.3f, \"check\": %ld}\n",
        BENCH,
        name,
        wall,
        cpu,
        ops,
        ops > 0? wall*1e6/ops : 0.0,
        ops > 0? cpu*1e6/ops : 0.0,
        check
    );
    fflush(stdout);
}

PRIVATE json_t *startup_tranger(const char *path_root)
{
    return tranger2_startup(
        0,
        json_pack("{s:s, s:s, s:b, s:i}",
            "path", path_root,
            "database", DATABASE,
            "master", 1,
            "on_critical_error", 0
        ),
        0
    );
}

PRIVATE int open_treedb(json_t *tranger)
{
    json_t *jn_schema = legalstring2json(schema_perf, TRUE);
    if(!treedb_open_db(tranger, TREEDB_NAME, jn_schema, "persistent")) {
        return -1;  // Error already logged
    }
    return 0;
}

/***************************************************************
 *  The treedb the cases read: N users, N/40 departments
 ***************************************************************/
PRIVATE int build_treedb(json_t *tranger, int departments)
{
    char name[NAME_MAX];
    char email[NAME_MAX];
    char dep_id[NAME_MAX];

    json_t *top = treedb_create_node(tranger, TREEDB_NAME, "departments",
        json_pack("{s:s, s:s}", "id", "top", "name", "Top")
    );
    if(!top) {
        return -1;  // Error already logged
    }
    for(int d = 0; d < departments; d++) {
        snprintf(name, sizeof(name), "dep%04d", d);
        json_t *dep = treedb_create_node(tranger, TREEDB_NAME, "departments",
            json_pack("{s:s, s:s}", "id", name, "name", name)
        );
        if(!dep) {
            return -1;  // Error already logged
        }
        treedb_link_nodes(tranger, "departments", top, dep);
    }

    for(int i = 0; i < N; i++) {
        snprintf(name, sizeof(name), "user%06d", i);
        snprintf(email, sizeof(email), "user%06d@example.com", i);
        json_t *user = treedb_create_node(tranger, TREEDB_NAME, "users",
            json_pack("{s:s, s:s, s:s, s:b, s:{s:s, s:i}, s:I}",
                "id", name,
                "username", name,
                "email", email,
                "enabled", i%3 != 0,
                "properties", "lang", "es", "level", i%7,
                "time", (json_int_t)1700000000 + i
            )
        );
        if(!user) {
            return -1;  // Error already logged
        }
        snprintf(dep_id, sizeof(dep_id), "dep%04d", i % departments);
        treedb_link_nodes(tranger, "users",
            treedb_get_node(tranger, TREEDB_NAME, "departments", dep_id),
            user
        );
        if(i % 4 == 0 && (i/4) % departments != i % departments) {
            snprintf(dep_id, sizeof(dep_id), "dep%04d", (i/4) % departments);
            treedb_link_nodes(tranger, "users",
                treedb_get_node(tranger, TREEDB_NAME, "departments", dep_id),
                user
            );
        }
    }
    return 0;
}

/***************************************************************
 *              Benchmark
 ***************************************************************/
PRIVATE int do_bench(const char *path_root)
{
    int departments = N / USERS_PER_DEPARTMENT;
    if(departments < 1) {
        departments = 1;
    }

    json_t *tranger = startup_tranger(path_root);
    if(!tranger) {
        return -1;  // Error already logged
    }
    helper_quote2doublequote(schema_perf);
    if(open_treedb(tranger) < 0) {
        tranger2_shutdown(tranger);
        return -1;  // Error already logged
    }
    if(build_treedb(tranger, departments) < 0) {
        treedb_close_db(tranger, TREEDB_NAME);
        tranger2_shutdown(tranger);
        return -1;  // Error already logged
    }
    treedb_close_db(tranger, TREEDB_NAME);
    tranger2_shutdown(tranger);

    /*
     *  reopen
     */
    tranger = startup_tranger(path_root);
    if(!tranger) {
        return -1;  // Error already logged
    }
    clock_mark_t t0 = clock_now();
    if(open_treedb(tranger) < 0) {
        tranger2_shutdown(tranger);
        return -1;  // Error already logged
    }
    result_line_since("reopen", t0, N + departments + 1,
        (long)json_array_size(json_object_get(
            treedb_get_node(tranger, TREEDB_NAME, "departments", "dep0000"), "users"
        ))
    );

    /*
     *  list_nodes
     */
    long count = 0;
    t0 = clock_now();
    for(int r = 0; r < R; r++) {
        json_t *list = treedb_list_nodes(tranger, TREEDB_NAME, "users", NULL, NULL);
        count += (long)json_array_size(list);
        JSON_DECREF(list)
    }
    result_line_since("list_nodes", t0, (long)R*N, count);

    /*
     *  list_filter
     */
    count = 0;
    t0 = clock_now();
    for(int r = 0; r < R; r++) {
        json_t *list = treedb_list_nodes(tranger, TREEDB_NAME, "users",
            json_pack("{s:b}", "enabled", 0),
            NULL
        );
        count += (long)json_array_size(list);
        JSON_DECREF(list)
    }
    result_line_since("list_filter", t0, (long)R*N, count);

    /*
     *  view_users
     */
    count = 0;
    t0 = clock_now();
    for(int r = 0; r < R; r++) {
        json_t *iter = treedb_list_nodes(tranger, TREEDB_NAME, "users", NULL, NULL);
        json_t *views = json_array();
        size_t idx; json_t *node;
        json_array_foreach(iter, idx, node) {
            json_t *view = node_collapsed_view(tranger, node, json_object());
            count += (long)json_array_size(json_object_get(view, "departments"));
            json_array_append_new(views, view);
        }
        JSON_DECREF(iter)
        JSON_DECREF(views)
    }
    result_line_since("view_users", t0, (long)R*N, count);

    /*
     *  view_departments
     */
    count = 0;
    long n_departments = 0;
    t0 = clock_now();
    for(int r = 0; r < R; r++) {
        json_t *iter = treedb_list_nodes(tranger, TREEDB_NAME, "departments", NULL, NULL);
        json_t *views = json_array();
        size_t idx; json_t *node;
        json_array_foreach(iter, idx, node) {
            json_t *view = node_collapsed_view(tranger, node, json_object());
            count += (long)json_array_size(json_object_get(view, "users"));
            json_array_append_new(views, view);
            n_departments++;
        }
        JSON_DECREF(iter)
        JSON_DECREF(views)
    }
    result_line_since("view_departments", t0, n_departments, count);

    /*
     *  get_view
     */
    char name[NAME_MAX];
    count = 0;
    t0 = clock_now();
    for(int r = 0; r < R; r++) {
        for(int i = 0; i < N; i++) {
            snprintf(name, sizeof(name), "user%06d", i);
            json_t *node = treedb_get_node(tranger, TREEDB_NAME, "users", name);
            json_t *view = node_collapsed_view(tranger, node, json_object());
            count += (long)json_object_size(view);
            JSON_DECREF(view)
        }
    }
    result_line_since("get_view", t0, (long)R*N, count);

    /*
     *  dump_views
     */
    json_t *iter = treedb_list_nodes(tranger, TREEDB_NAME, "users", NULL, NULL);
    json_t *views = json_array();
    size_t idx; json_t *node;
    json_array_foreach(iter, idx, node) {
        json_array_append_new(views, node_collapsed_view(tranger, node, json_object()));
    }
    JSON_DECREF(iter)
    count = 0;
    t0 = clock_now();
    for(int r = 0; r < R; r++) {
        gbuffer_t *gbuf = json2gbuf(0, json_incref(views), JSON_COMPACT);
        count += (long)gbuffer_leftbytes(gbuf);
        GBUFFER_DECREF(gbuf)
    }
    result_line_since("dump_views", t0, (long)R*N, count);
    JSON_DECREF(views)

    treedb_close_db(tranger, TREEDB_NAME);
    tranger2_shutdown(tranger);
    return 0;
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
            N = 2000;
            R = 2;
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
