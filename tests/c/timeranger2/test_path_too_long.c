/****************************************************************************
 *          test_path_too_long.c
 *
 *  A path that does not fit in PATH_MAX: build_path() answers NULL (logged)
 *  and leaves in the buffer the segments that fitted -- an ANCESTOR of the
 *  path asked for. Every caller here must stop at the NULL, never use that
 *  ancestor. Up to 7.29.3 these did not, and acted on the ancestor:
 *
 *      1. tranger2_startup(), the store path too long: the store was made
 *         in the directory above it (or in the current directory);
 *      2. remove_rt_disk_directory(), a feed id too long: the rename() of
 *         disks/ failed, and its fallback removed disks/ whole, EVERY
 *         replica of the topic;
 *      3. mirror_key_delete_to_disks(), a key too long for disks/<rt>/<key>
 *         (it fits in keys/<key>, a shorter path): the whole replica of
 *         that follower was removed;
 *      4. mirror_key_purge_to_disks(), the purge signal too long: an
 *         empty replica was removed;
 *      5. sweep_left_delete_signals(), a leftover signal whose path is too
 *         long: the replica was moved out of disks/;
 *      6. build_md2_and_data_paths() answered TRUE with the ancestors;
 *      7. load_key_cache_from_disk() listed an ancestor as the key: a key
 *         of no files, not flagged;
 *      8. get_topic_wr_fd() built keys/<key>/<file> with snprintf(), cut
 *         in silence: a directory named as the start of the key was made
 *         in keys/, and the file was asked of that cut path;
 *      9. treedb_open_db() built the path of its schema file in NAME_MAX*2
 *         bytes: a store directory of 600 bytes did not find the file, and
 *         an older literal was written over a newer schema (2 -> 1).
 *
 *  Each case asks that the target is left as it was, and that an ERROR
 *  was logged.
 *
 *  A unit test: it #includes timeranger2.c to reach the PRIVATE functions.
 *
 *          Copyright (c) 2026, ArtGins.
 *          All Rights Reserved.
 ****************************************************************************/
#include <syslog.h>
#include "timeranger2.c"
#include <tr_treedb.h>

#define BASE    "/tmp/test_path_too_long"

/***************************************************************************
 *  The ERRORs logged
 ***************************************************************************/
PRIVATE int errors_logged = 0;

PRIVATE int count_errors(void *v, int priority, const char *bf, size_t len)
{
    if(priority <= LOG_ERR) {
        errors_logged++;
    }
    return 0;
}

PRIVATE int check(const char *what, BOOL ok)
{
    if(!ok) {
        printf("%sFAILED%s %s\n", On_Red BWhite, Color_Off, what);
        return -1;
    }
    printf("ok   %s\n", what);
    return 0;
}

/***************************************************************************
 *  Make the directory BASE/aaa.../aaa... whose path is `len` bytes long
 ***************************************************************************/
PRIVATE int make_deep_dir(char *bf, size_t bfsize, size_t len)
{
    snprintf(bf, bfsize, "%s", BASE);
    while(strlen(bf) < len) {
        size_t need = len - strlen(bf);     // the '/' and the segment
        size_t seg = need - 1;
        if(seg > 200) {
            seg = 200;
            if(need - 1 - seg == 1) {
                seg--;      // the next segment cannot be a lone '/'
            }
        }
        size_t at = strlen(bf);
        bf[at] = '/';
        memset(bf + at + 1, 'a', seg);
        bf[at + 1 + seg] = 0;
        if(mkdir(bf, 0700) < 0 && errno != EEXIST) {
            printf("cannot make %zu bytes deep: %s\n", strlen(bf), strerror(errno));
            return -1;
        }
    }
    return 0;
}

PRIVATE int open_close_treedb(const char *path, int schema_version)
{
    json_t *tr = tranger2_startup(0, json_pack("{s:s, s:s, s:b, s:i}",
        "path", path, "database", "db", "master", 1, "on_critical_error", 0), 0);
    if(!tr) {
        return -1;
    }
    json_t *jn_schema = json_pack(
        "{s:s, s:i, s:[{s:s, s:i, s:s, s:s, s:{s:{s:s, s:s, s:[s,s]}}}]}",
        "id", "treedb_x",
        "schema_version", schema_version,
        "topics",
            "id", "users",
            "topic_version", 1,
            "pkey", "id",
            "system_flag", "sf_string_key",
            "cols",
                "id",
                    "header", "Id",
                    "type", "string",
                    "flag", "persistent", "required"
    );
    parse_schema(jn_schema);
    json_t *db = treedb_open_db(tr, "treedb_x", jn_schema, "persistent");
    if(db) {
        treedb_close_db(tr, "treedb_x");
    }
    tranger2_shutdown(tr);
    return db? 0 : -1;
}

PRIVATE void fill(char *bf, char c, size_t len)
{
    memset(bf, c, len);
    bf[len] = 0;
}

PRIVATE BOOL dir_there(const char *parent, const char *name)
{
    char path[PATH_MAX];
    snprintf(path, sizeof(path), "%s/%s", parent, name);
    return is_directory(path);
}

/***************************************************************************
 *
 ***************************************************************************/
int main(int argc, char *argv[])
{
    int result = 0;

    glog_init();
    gobj_log_register_handler("errors", 0, count_errors, 0);
    gobj_log_add_handler("test_errors", "errors", LOG_OPT_UP_ERROR, 0);
    gobj_log_add_handler("stdout", "stdout", LOG_OPT_ALL, 0);
    gobj_start_up(argc, argv, NULL, NULL, NULL, NULL, NULL, NULL);

    if(system("rm -rf " BASE) != 0) {
        printf("cannot clean %s\n", BASE);
    }
    mkdir(BASE, 0700);

    char topic_dir[PATH_MAX];
    char disks[PATH_MAX];
    char path[PATH_MAX];
    char long_name[NAME_MAX+1];

    /*
     *  The topic directory, 3950 bytes: disks/<rt_id>/ fits for a short id,
     *  and disks/<rt_id>/<key> does not for a key of 200
     */
    if(make_deep_dir(topic_dir, sizeof(topic_dir), 3950) < 0) {
        return -1;
    }
    build_path(disks, sizeof(disks), topic_dir, "disks", NULL);
    mkdir(disks, 0700);

    /*
     *  1. tranger2_startup(): no store above the path asked for
     */
    if(1) {
        char store_parent[PATH_MAX];
        if(make_deep_dir(store_parent, sizeof(store_parent), 4050) < 0) {
            return -1;
        }
        fill(long_name, 'd', 60);
        errors_logged = 0;
        json_t *tr = tranger2_startup(0, json_pack("{s:s, s:s, s:b}",
            "path", store_parent,
            "database", long_name,
            "master", 1
        ), 0);
        result += check("startup, path/database too long: NULL", tr == NULL);
        result += check("startup, path/database too long: no store made in the path above",
            !file_exists(store_parent, "__timeranger2__.json"));
        result += check("startup, path/database too long: an ERROR", errors_logged > 0);
        if(tr) {
            tranger2_shutdown(tr);
        }

        char too_long[PATH_MAX + 64];
        memset(too_long, 'p', sizeof(too_long) - 1);
        too_long[0] = '/';
        too_long[sizeof(too_long) - 1] = 0;
        char cwd[PATH_MAX];
        if(!getcwd(cwd, sizeof(cwd))) {
            cwd[0] = 0;
        }
        if(chdir(BASE) < 0) {
            printf("cannot chdir to %s\n", BASE);
        }
        errors_logged = 0;
        tr = tranger2_startup(0, json_pack("{s:s, s:s, s:b}",
            "path", too_long,
            "database", "db",
            "master", 1
        ), 0);
        result += check("startup, path longer than PATH_MAX: NULL", tr == NULL);
        result += check("startup, path longer than PATH_MAX: no store made in the current directory",
            !is_directory(BASE "/db"));
        result += check("startup, path longer than PATH_MAX: an ERROR", errors_logged > 0);
        if(tr) {
            tranger2_shutdown(tr);
        }
        if(!empty_string(cwd) && chdir(cwd) < 0) {
            printf("cannot chdir back to %s\n", cwd);
        }
    }

    /*
     *  2. remove_rt_disk_directory(), an id too long: no other feed removed
     */
    if(1) {
        build_path(path, sizeof(path), disks, "feedA", NULL);
        mkdir(path, 0700);
        fill(long_name, 'r', 200);
        errors_logged = 0;
        remove_rt_disk_directory(0, topic_dir, long_name);
        result += check("close of a feed whose id is too long: disks/ is still there",
            is_directory(disks));
        result += check("close of a feed whose id is too long: the other feed is still there",
            dir_there(disks, "feedA"));
        result += check("close of a feed whose id is too long: an ERROR", errors_logged > 0);
        mkdir(disks, 0700);
        mkdir(path, 0700);
    }

    /*
     *  3. mirror_key_delete_to_disks(), a key too long for disks/<rt>/<key>:
     *  the replicas stay
     */
    if(1) {
        build_path(path, sizeof(path), disks, "feedA", "file", NULL);
        int fd = open(path, O_CREAT|O_WRONLY|O_CLOEXEC, 0600);
        if(fd >= 0) {
            close(fd);
        }
        char feed_b[PATH_MAX];
        build_path(feed_b, sizeof(feed_b), disks, "feedB", NULL);
        mkdir(feed_b, 0700);
        build_path(path, sizeof(path), disks, "feedA", NULL);

        json_t *tranger = json_pack("{s:i}", "xpermission", 0700);
        json_t *topic = json_pack("{s:s, s:s}", "directory", topic_dir, "topic_name", "topic");
        fill(long_name, 'k', 200);
        errors_logged = 0;
        mirror_key_delete_to_disks(0, tranger, topic, long_name, 5);
        result += check("delete of a key too long for disks/<rt>/<key>: the full replica stays",
            dir_there(disks, "feedA") && file_exists(path, "file"));
        result += check("delete of a key too long for disks/<rt>/<key>: the empty replica stays",
            dir_there(disks, "feedB"));
        result += check("delete of a key too long for disks/<rt>/<key>: an ERROR", errors_logged > 0);

        /*
         *  4. mirror_key_purge_to_disks(): the empty replica stays
         */
        mkdir(feed_b, 0700);
        errors_logged = 0;
        mirror_key_purge_to_disks(0, tranger, topic, long_name);
        result += check("purge signal too long: the empty replica stays", dir_there(disks, "feedB"));
        result += check("purge signal too long: an ERROR", errors_logged > 0);

        JSON_DECREF(topic)
        JSON_DECREF(tranger)
    }

    /*
     *  5. sweep_left_delete_signals(): a leftover whose path is too long,
     *  an old one (taken unheard): the replica is not moved away
     */
    if(1) {
        build_path(path, sizeof(path), disks, "feedC", NULL);
        mkdir(path, 0700);
        int dir_fd = open(path, O_RDONLY|O_DIRECTORY|O_CLOEXEC);
        fill(long_name, 'k', 200);
        char signal[NAME_MAX+8];
        snprintf(signal, sizeof(signal), ".d3.%s", long_name);
        char purge_signal[NAME_MAX+8];
        snprintf(purge_signal, sizeof(purge_signal), ".p.%s", long_name);
        if(dir_fd < 0 ||
                mkdirat(dir_fd, signal, 0700) < 0 ||
                mkdirat(dir_fd, purge_signal, 0700) < 0) {
            printf("cannot make the signals: %s\n", strerror(errno));
            result += -1;
        }
        if(dir_fd >= 0) {
            close(dir_fd);
        }

        json_t *topic = json_pack("{s:s, s:s, s:I}",
            "directory", topic_dir, "topic_name", "topic", "delete_seq", (json_int_t)10
        );
        errors_logged = 0;
        sweep_left_delete_signals(0, topic, path);
        result += check("leftover signal too long: the replica is not moved away",
            dir_there(disks, "feedC"));
        result += check("leftover signal too long: an ERROR", errors_logged > 0);
        JSON_DECREF(topic)
    }

    /*
     *  6. build_md2_and_data_paths(): FALSE
     */
    if(1) {
        char md2_path[PATH_MAX];
        char data_path[PATH_MAX];
        fill(long_name, 'k', 200);
        errors_logged = 0;
        BOOL ok = build_md2_and_data_paths(
            0, topic_dir, long_name, "2026-10-29", md2_path, data_path, sizeof(md2_path)
        );
        result += check("md2 and data paths too long: FALSE", !ok);
        result += check("md2 and data paths too long: an ERROR", errors_logged > 0);
    }

    /*
     *  7. load_key_cache_from_disk(): the key is flagged, not an ancestor
     *  listed
     */
    if(1) {
        build_path(path, sizeof(path), topic_dir, "keys", NULL);
        mkdir(path, 0700);
        fill(long_name, 'k', 200);
        errors_logged = 0;
        json_t *key_cache = load_key_cache_from_disk(0, topic_dir, long_name, TRUE);
        result += check("cache of a key too long: flagged unlisted",
            json_is_true(json_object_get(key_cache, "unlisted")));
        result += check("cache of a key too long: an ERROR", errors_logged > 0);
        JSON_DECREF(key_cache)
    }

    /*
     *  8. get_topic_wr_fd(), keys/<key>/<file> too long: -1, nothing made
     *  in keys/
     */
    if(1) {
        char keys_dir[PATH_MAX];
        build_path(keys_dir, sizeof(keys_dir), topic_dir, "keys", NULL);
        json_t *tranger = json_pack("{s:b, s:i, s:i, s:i}",
            "master", 1, "xpermission", 0700, "rpermission", 0600, "on_critical_error", 0
        );
        json_t *topic = json_pack("{s:s, s:s, s:{}}",
            "directory", topic_dir, "topic_name", "topic", "wr_fd_files"
        );
        fill(long_name, 'k', 200);
        errors_logged = 0;
        int fd = get_topic_wr_fd(0, tranger, topic, long_name, TRUE, "2026-10-29", TRUE);
        result += check("write fd of a key too long: -1", fd < 0);
        DIR *dir = opendir(keys_dir);
        int entries = 0;
        struct dirent *entry;
        while(dir && (entry = readdir(dir)) != NULL) {
            if(entry->d_name[0] != '.') {
                entries++;
            }
        }
        if(dir) {
            closedir(dir);
        }
        result += check("write fd of a key too long: nothing made in keys/", dir && entries == 0);
        result += check("write fd of a key too long: an ERROR", errors_logged > 0);
        if(fd >= 0) {
            close(fd);
        }
        JSON_DECREF(topic)
        JSON_DECREF(tranger)
    }

    /*
     *  9. treedb_open_db(): the schema file of a store 600 bytes deep is
     *  found, and a newer one is kept
     */
    if(1) {
        char store[PATH_MAX];
        if(make_deep_dir(store, sizeof(store), 600) < 0) {
            return -1;
        }
        result += check("treedb in a store 600 bytes deep: opens with schema 2",
            open_close_treedb(store, 2) == 0);
        result += check("treedb in a store 600 bytes deep: opens with schema 1",
            open_close_treedb(store, 1) == 0);
        char db_dir[PATH_MAX];
        build_path(db_dir, sizeof(db_dir), store, "db", NULL);
        json_t *file = load_json_from_file(0, db_dir, "treedb_x.treedb_schema.json", 0);
        result += check("treedb in a store 600 bytes deep: the newer schema file is kept",
            kw_get_int(0, file, "schema_version", 0, KW_WILD_NUMBER) == 2);
        JSON_DECREF(file)
    }

    if(system("rm -rf " BASE) != 0) {
        printf("cannot remove %s\n", BASE);
    }
    gobj_end();

    if(get_cur_system_memory() != 0) {
        printf("%sERROR%s --> system memory not free\n", On_Red BWhite, Color_Off);
        print_track_mem();
        result = -1;
    }

    printf("%s: %s\n", "test_path_too_long", result == 0? "PASS" : "FAIL");
    return result < 0? -1 : 0;
}
