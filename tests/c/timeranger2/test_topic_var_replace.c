/****************************************************************************
 *          test_topic_var_replace.c
 *
 *  topic_var.json holds the topic_version and the `last_rowid_id` counter
 *  treedb takes a rowid id from, and tranger2_write_topic_var() runs on
 *  every create of a rowid-key node. 7.25.4 rewrote the file IN PLACE
 *  (O_TRUNC then write): a process that died between the two left it
 *  empty, and the counter and the version were lost. Only the version
 *  change went through a temporary file and a rename().
 *
 *  Now every write of topic_var.json writes `topic_var.json.new` and
 *  renames it over the old one: the file is the old one or the new one,
 *  never a truncated one. What the test SEES of it: the file is a new inode
 *  after the write, and a reader that had the old one open still reads it
 *  whole.
 *
 *  The temporary file is unlinked first and created O_EXCL|O_NOFOLLOW,
 *  with the tranger's rpermission. Opened with O_TRUNC, a `.new` left
 *  behind by a process that died would keep its mode and owner, and a
 *  `.new` that is a symlink would be followed, so the write would go to
 *  whatever it points at.
 *
 *  tranger2_write_topic_cols() wrote topic_cols.json in place, put the
 *  new cols in memory BEFORE the write, ignored the result and returned 0.
 *  It is replaced the same way now, and the memory takes the new cols only
 *  when the file did.
 *
 *          Copyright (c) 2026, ArtGins.
 *          All Rights Reserved.
 ****************************************************************************/
#include <string.h>
#include <signal.h>
#include <limits.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>

#include <gobj.h>
#include <kwid.h>
#include <timeranger2.h>
#include <helpers.h>
#include <yev_loop.h>
#include <testing.h>

#define APP         "test_topic_var_replace"
#define DATABASE    "tr_topic_var_replace"
#define TOPIC_NAME  "topic_var_replace"

/***************************************************************
 *              Data
 ***************************************************************/
PRIVATE yev_loop_h yev_loop;

/***************************************************************
 *              Helpers
 ***************************************************************/
PRIVATE int expect(const char *what, const char *got, const char *expected)
{
    if(strcmp(got, expected) != 0) {
        printf("%sERROR%s --> %s: '%s', expected '%s'\n",
            On_Red BWhite, Color_Off, what, got, expected);
        return -1;
    }
    return 0;
}

/***************************************************************************
 *  do_test
 ***************************************************************************/
PRIVATE int do_test(void)
{
    int result = 0;
    char path_root[PATH_MAX];
    char path_database[PATH_MAX];
    char path_var[PATH_MAX];
    char path_new[PATH_MAX];
    build_path(path_root, sizeof(path_root), getenv("HOME"), "tests_yuneta", NULL);
    mkrdir(path_root, 02770);
    build_path(path_database, sizeof(path_database), path_root, DATABASE, NULL);
    build_path(path_var, sizeof(path_var), path_database, TOPIC_NAME, "topic_var.json", NULL);
    build_path(path_new, sizeof(path_new), path_database, TOPIC_NAME, "topic_var.json.new", NULL);
    rmrdir(path_database);

    set_expected_results(
        "topic_var: setup",
        json_pack("[{s:s},{s:s}]",
            "msg", "Creating __timeranger2__.json",
            "msg", "Creating topic"
        ),
        NULL, NULL, 1
    );
    json_t *tranger = tranger2_startup(0, json_pack("{s:s, s:s, s:b, s:i}",
        "path", path_root,
        "database", DATABASE,
        "master", 1,
        "on_critical_error", LOG_OPT_TRACE_STACK
    ), 0);
    json_t *topic = tranger? tranger2_create_topic(
        tranger, TOPIC_NAME, "id", "", NULL, sf_rowid_key,
        json_pack("{s:s, s:s}", "id", "", "content", ""),
        json_pack("{s:I}", "topic_version", (json_int_t)7)
    ) : NULL;
    if(!topic) {
        printf("%sERROR%s --> cannot create the topic\n", On_Red BWhite, Color_Off);
        if(tranger) {
            tranger2_shutdown(tranger);
        }
        return -1;
    }
    result += test_json(NULL);

    set_expected_results("topic_var: a write replaces the file", NULL, NULL, NULL, 1);
    for(int i = 1; i <= 3; i++) {
        struct stat st_before;
        if(stat(path_var, &st_before) < 0) {
            printf("%sERROR%s --> cannot stat %s\n", On_Red BWhite, Color_Off, path_var);
            result += -1;
            break;
        }
        int fd_old = open(path_var, O_RDONLY|O_CLOEXEC);

        tranger2_write_topic_var(tranger, TOPIC_NAME,
            json_pack("{s:I}", "last_rowid_id", (json_int_t)i)
        );

        struct stat st_after;
        if(stat(path_var, &st_after) < 0) {
            printf("%sERROR%s --> cannot stat %s\n", On_Red BWhite, Color_Off, path_var);
            result += -1;
        } else if(st_after.st_ino == st_before.st_ino) {
            printf("%sERROR%s --> write %d: topic_var.json was rewritten in place (same inode)\n",
                On_Red BWhite, Color_Off, i);
            result += -1;
        }

        /*  The reader of the old file still reads it whole  */
        json_t *old = fd_old >= 0? json_loadfd(fd_old, 0, 0): NULL;
        if(!old || kw_get_int(0, old, "topic_version", 0, 0) != 7) {
            printf("%sERROR%s --> write %d: the old topic_var.json was truncated under its reader\n",
                On_Red BWhite, Color_Off, i);
            result += -1;
        }
        JSON_DECREF(old)
        if(fd_old >= 0) {
            close(fd_old);
        }

        json_t *now = json_load_file(path_var, 0, 0);
        char expected[64];
        snprintf(expected, sizeof(expected), "7 %d", i);
        char got[64];
        snprintf(got, sizeof(got), "%d %d",
            (int)kw_get_int(0, now, "topic_version", 0, 0),
            (int)kw_get_int(0, now, "last_rowid_id", 0, 0)
        );
        result += expect("topic_var.json merges the write", got, expected);
        JSON_DECREF(now)
    }
    if(is_regular_file(path_new)) {
        printf("%sERROR%s --> the temporary file is left behind\n", On_Red BWhite, Color_Off);
        result += -1;
    }
    result += expect("the topic in memory has the counter",
        kw_get_int(0, topic, "last_rowid_id", 0, 0) == 3? "3": "other", "3");
    result += test_json(NULL);

    /*-------------------------------------*
     *  A `.new` left behind: a file with
     *  another mode, then a symlink
     *-------------------------------------*/
    set_expected_results("topic_var: a .new left behind", NULL, NULL, NULL, 1);
    int fd = open(path_new, O_CREAT|O_WRONLY|O_TRUNC, 0666);
    if(fd >= 0) {
        fchmod(fd, 0666);
        if(write(fd, "garbage{{{ a long leftover .............................", 57) < 0) {
            printf("%sERROR%s --> cannot write the leftover\n", On_Red BWhite, Color_Off);
        }
        close(fd);
    }
    tranger2_write_topic_var(tranger, TOPIC_NAME, json_pack("{s:I}", "last_rowid_id", (json_int_t)4));
    struct stat st;
    char mode[16] = "";
    if(stat(path_var, &st) == 0) {
        snprintf(mode, sizeof(mode), "%o", (unsigned)(st.st_mode & 07777));
    }
    result += expect("topic_var.json takes the tranger's mode, not the leftover's", mode, "660");
    json_t *now = json_load_file(path_var, 0, 0);
    result += expect("topic_var.json is whole after a leftover",
        kw_get_int(0, now, "last_rowid_id", 0, 0) == 4? "4": "other", "4");
    JSON_DECREF(now)

    char path_victim[PATH_MAX];
    build_path(path_victim, sizeof(path_victim), path_database, "victim.txt", NULL);
    FILE *f = fopen(path_victim, "w");
    if(f) {
        fputs("not yours", f);
        fclose(f);
    }
    unlink(path_new);
    if(symlink(path_victim, path_new) < 0) {
        printf("%sERROR%s --> cannot plant the symlink\n", On_Red BWhite, Color_Off);
        result += -1;
    }
    tranger2_write_topic_var(tranger, TOPIC_NAME, json_pack("{s:I}", "last_rowid_id", (json_int_t)5));
    char victim[64] = "";
    f = fopen(path_victim, "r");
    if(f) {
        if(!fgets(victim, sizeof(victim), f)) {
            victim[0] = 0;
        }
        fclose(f);
    }
    result += expect("a .new that is a symlink is not followed", victim, "not yours");
    now = json_load_file(path_var, 0, 0);
    result += expect("topic_var.json is written after a symlinked .new",
        kw_get_int(0, now, "last_rowid_id", 0, 0) == 5? "5": "other", "5");
    JSON_DECREF(now)
    result += test_json(NULL);

    /*-------------------------------------*
     *  topic_cols.json: replaced too, and
     *  a write that fails changes nothing
     *-------------------------------------*/
    set_expected_results("topic_cols: a write replaces the file", NULL, NULL, NULL, 1);
    char path_cols[PATH_MAX];
    build_path(path_cols, sizeof(path_cols), path_database, TOPIC_NAME, "topic_cols.json", NULL);
    struct stat st_before;
    stat(path_cols, &st_before);
    int ret = tranger2_write_topic_cols(tranger, TOPIC_NAME,
        json_pack("{s:s, s:s, s:s}", "id", "", "content", "", "extra", "")
    );
    struct stat st_after;
    stat(path_cols, &st_after);
    result += expect("write_topic_cols answers 0", ret == 0? "0": "-1", "0");
    result += expect("topic_cols.json is a new inode",
        st_after.st_ino != st_before.st_ino? "new": "same", "new");
    now = json_load_file(path_cols, 0, 0);
    result += expect("topic_cols.json has the new column",
        json_object_get(now, "extra")? "extra": "none", "extra");
    JSON_DECREF(now)
    result += test_json(NULL);

    set_expected_results(
        "topic_cols: a write that fails changes nothing",
        json_pack("[{s:s}]",
            "msg", "Cannot replace topic_cols.json, rename() FAILED"
        ),
        NULL, NULL, 1
    );
    unlink(path_cols);
    mkdir(path_cols, 0700);     // a rename() over a directory fails
    ret = tranger2_write_topic_cols(tranger, TOPIC_NAME,
        json_pack("{s:s, s:s, s:s}", "id", "", "content", "", "other", "")
    );
    result += expect("write_topic_cols answers -1 when the file is not written",
        ret < 0? "-1": "0", "-1");
    json_t *cols_in_memory = json_object_get(topic, "cols");
    result += expect("the cols in memory are the ones on disk",
        json_object_get(cols_in_memory, "other")? "the refused ones":
            json_object_get(cols_in_memory, "extra")? "the written ones": "other",
        "the written ones");
    rmdir(path_cols);
    result += test_json(NULL);

    /*-------------------------------------*
     *  A topic_version that goes up and a
     *  topic_cols.json that cannot be
     *  written: 7.25.4 removed
     *  topic_cols.json first, ignored the
     *  failed write, logged "Re-Creating"
     *  and saved the new version. Nothing
     *  moves now, and the topic is not
     *  opened.
     *-------------------------------------*/
    set_expected_results(
        "topic_cols: a version change whose cols cannot be written",
        json_pack("[{s:s},{s:s}]",
            "msg", "Cannot replace topic_cols.json, cannot remove a temporary file left behind",
            "msg", "Cannot re-create topic_cols.json for a new topic_version: the topic keeps its version, and is not opened"
        ),
        NULL, NULL, 1
    );
    tranger2_write_topic_cols(tranger, TOPIC_NAME,     // the case before left none
        json_pack("{s:s, s:s}", "id", "", "content", "")
    );
    tranger2_shutdown(tranger);
    char path_cols_new[PATH_MAX];
    build_path(path_cols_new, sizeof(path_cols_new), path_database, TOPIC_NAME, "topic_cols.json.new", NULL);
    mkdir(path_cols_new, 0700);     // a .new that cannot be removed nor created
    tranger = tranger2_startup(0, json_pack("{s:s, s:s, s:b, s:i}",
        "path", path_root,
        "database", DATABASE,
        "master", 1,
        "on_critical_error", LOG_OPT_TRACE_STACK
    ), 0);
    topic = tranger2_create_topic(
        tranger, TOPIC_NAME, "id", "", NULL, sf_rowid_key,
        json_pack("{s:s, s:s, s:s}", "id", "", "content", "", "v8", ""),
        json_pack("{s:I}", "topic_version", (json_int_t)8)
    );
    result += expect("the topic is not opened", topic? "opened": "refused", "refused");
    json_t *var_now = json_load_file(path_var, 0, 0);
    result += expect("topic_var.json keeps its version",
        json_integer_value(json_object_get(var_now, "topic_version")) == 7? "7": "other", "7");
    JSON_DECREF(var_now)
    json_t *cols_now = json_load_file(path_cols, 0, 0);
    result += expect("topic_cols.json keeps its cols",
        !cols_now? "gone": json_object_get(cols_now, "v8")? "the new ones": "the old ones",
        "the old ones");
    JSON_DECREF(cols_now)
    rmdir(path_cols_new);
    result += test_json(NULL);

    set_expected_results(
        "topic_cols: the version change once it can be written",
        json_pack("[{s:s},{s:s}]",
            "msg", "Re-Creating topic_var.json",
            "msg", "Re-Creating topic_cols.json"
        ),
        NULL, NULL, 1
    );
    topic = tranger2_create_topic(
        tranger, TOPIC_NAME, "id", "", NULL, sf_rowid_key,
        json_pack("{s:s, s:s, s:s}", "id", "", "content", "", "v8", ""),
        json_pack("{s:I}", "topic_version", (json_int_t)8)
    );
    result += expect("the topic is opened", topic? "opened": "refused", "opened");
    var_now = json_load_file(path_var, 0, 0);
    result += expect("topic_var.json has the new version",
        json_integer_value(json_object_get(var_now, "topic_version")) == 8? "8": "other", "8");
    JSON_DECREF(var_now)
    cols_now = json_load_file(path_cols, 0, 0);
    result += expect("topic_cols.json has the new cols",
        json_object_get(cols_now, "v8")? "the new ones": "the old ones", "the new ones");
    JSON_DECREF(cols_now)
    result += test_json(NULL);

    set_expected_results("topic_var: shutdown", NULL, NULL, NULL, 1);
    tranger2_shutdown(tranger);
    result += test_json(NULL);

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

    init_backtrace_with_backtrace(argv[0]);
    set_show_backtrace_fn(show_backtrace_with_backtrace);

    gobj_start_up(argc, argv, NULL, NULL, NULL, NULL, NULL, NULL);

    gobj_log_add_handler("stdout", "stdout", LOG_OPT_ALL, 0);
    gobj_log_register_handler("testing", 0, capture_log_write, 0);
    gobj_log_add_handler("test_capture", "testing", LOG_OPT_UP_INFO, 0);

    yev_loop_create(0, 2024, 10, NULL, &yev_loop);

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
    } else {
        printf("<-- %sTEST OK%s: %s\n", On_Green BWhite, Color_Off, APP);
    }
    return result < 0? -1 : 0;
}
