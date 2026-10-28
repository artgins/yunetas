/****************************************************************************
 *          C_TEST1.C
 *
 *          The batches of a timeranger2 archive shipped by archive_sender.
 *
 *          Two stores, each archived into its own archive:
 *          - native: "archive_sender" ships to "archive_keeper" over
 *            tcp://127.0.0.1:7755. Two batches are there before it plays
 *            (found by its scan), a third comes later (told by its ready
 *            mark), and a fourth is one the keeper already has: answered
 *            stored at once. All of them end in the backup, whole, and gone
 *            from the archive with their marks.
 *          - rsync: "archive_sender_rsync" ships to a local directory, with
 *            max_archive_bytes of ONE batch and two waiting: the oldest is
 *            removed without being shipped (a WARNING), the other ends in
 *            the destination with its manifest.
 *
 *          - poisoning: a batch of the name of one the sender will ship,
 *            planted in the backup first with other sizes: the keeper does
 *            not answer stored, keeps the planted one untouched, and the
 *            sender keeps its batch (failed, tried again later).
 *
 *          The steps are watched with a timer on the stats of the three
 *          services: a test driver, not a yuno.
 *
 *          Copyright (c) 2026, ArtGins.
 *          All Rights Reserved.
 ***********************************************************************/
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <dirent.h>
#include <sys/stat.h>
#include "c_test1.h"

/***************************************************************************
 *              Constants
 ***************************************************************************/
#define DATABASE        "tr_archive_shipping"
#define DATABASE2       "tr_archive_shipping2"
#define TOPIC_NAME      "topic_archive_shipping"
#define NODE_NAME       "test_node"
#define DAY1            946684800   // 2000-01-01T00:00:00Z
#define DAY2            (DAY1 + 86400)
#define DAY3            (DAY2 + 86400)
#define POLL_MS         100
#define STEP_TIMEOUT_MS 10000
#define POISONED        DATABASE "/" TOPIC_NAME "/key9/2000-01-01@1"

/***************************************************************************
 *              Structures
 ***************************************************************************/

/***************************************************************************
 *              Prototypes
 ***************************************************************************/

/***************************************************************************
 *          Data: config, public data, private data
 ***************************************************************************/
/*---------------------------------------------*
 *      Attributes
 *---------------------------------------------*/
PRIVATE sdata_desc_t attrs_table[] = {
/*-ATTR-type------------name----------------flag----------------default-----description--*/
SDATA (DTP_POINTER,     "user_data",        0,                  0,          "user data"),
SDATA (DTP_POINTER,     "user_data2",       0,                  0,          "more user data"),
SDATA (DTP_POINTER,     "subscriber",       0,                  0,          "subscriber of output-events. Not a child gobj."),
SDATA_END()
};

/*---------------------------------------------*
 *      GClass trace levels
 *---------------------------------------------*/
enum {
    TRACE_MESSAGES  = 0x0001,
};
PRIVATE const trace_level_t s_user_trace_level[16] = {
{"messages",        "Trace messages"},
{0, 0},
};

/*---------------------------------------------*
 *              Private data
 *---------------------------------------------*/
typedef struct _PRIVATE_DATA {
    hgobj timer;
    hgobj sender;
    hgobj sender_rsync;
    hgobj keeper;
    json_t *tranger;
    json_t *tranger2;
    int step;
    uint64_t step_timeout;
    char base[PATH_MAX];
    char store[PATH_MAX];
    char archive[PATH_MAX];
    char archive2[PATH_MAX];
    char backup[PATH_MAX];
    char rsync[PATH_MAX];
    char batch_key1[PATH_MAX];  // the path of the first batch of key1, under the archive
} PRIVATE_DATA;





                    /******************************
                     *      Framework Methods
                     ******************************/




/***************************************************************************
 *      Framework Method create
 ***************************************************************************/
PRIVATE void mt_create(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    priv->timer = gobj_create_pure_child(gobj_name(gobj), C_TIMER, 0, gobj);
}

/***************************************************************************
 *      Framework Method start
 ***************************************************************************/
PRIVATE int mt_start(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    gobj_start(priv->timer);
    return 0;
}

/***************************************************************************
 *      Framework Method stop
 ***************************************************************************/
PRIVATE int mt_stop(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    gobj_stop(priv->timer);
    EXEC_AND_RESET(tranger2_shutdown, priv->tranger)
    EXEC_AND_RESET(tranger2_shutdown, priv->tranger2)
    return 0;
}

/***************************************************************************
 *      Framework Method destroy
 ***************************************************************************/
PRIVATE void mt_destroy(hgobj gobj)
{
}




                    /***************************
                     *      Local Methods
                     ***************************/




/***************************************************************************
 *
 ***************************************************************************/
PRIVATE void fail(const char *what, const char *detail)
{
    gobj_log_error(0, 0,
        "function",     "%s", __FUNCTION__,
        "msgset",       "%s", MSGSET_INTERNAL,
        "msg",          "%s", what,
        "detail",       "%s", detail? detail : "",
        NULL
    );
}

/***************************************************************************
 *  A master store with one topic, keys key1..key3, a record a day
 ***************************************************************************/
PRIVATE json_t *open_store(const char *path, const char *database)
{
    json_t *tranger = tranger2_startup(0, json_pack("{s:s, s:s, s:b, s:i, s:s, s:i, s:i}",
        "path", path,
        "database", database,
        "master", 1,
        "on_critical_error", LOG_OPT_TRACE_STACK,
        "filename_mask", "%Y",
        "xpermission" , 02770,
        "rpermission", 0660
    ), yuno_event_loop());
    tranger2_create_topic(
        tranger, TOPIC_NAME, "id", "tm",
        json_pack("{s:i, s:s, s:i, s:i}",
            "on_critical_error", 4,
            "filename_mask", "%Y-%m-%d",
            "xpermission" , 02770,
            "rpermission", 0660
        ),
        sf_string_key,
        json_pack("{s:s, s:I, s:s}",
            "id", "",
            "tm", (json_int_t)0,
            "content", ""
        ),
        0
    );
    const char *keys[] = {"key1", "key2", "key3", NULL};
    uint64_t days[] = {DAY1, DAY2, DAY3, 0};
    for(int k = 0; keys[k]; k++) {
        for(int d = 0; days[d]; d++) {
            md2_record_ex_t md = {0};
            tranger2_append_record(tranger, TOPIC_NAME, days[d] + 100, 0, &md,
                json_pack("{s:s, s:I, s:s}",
                    "id", keys[k],
                    "tm", (json_int_t)(days[d] + 100),
                    "content", "the same length for every key"
                )
            );
        }
    }
    return tranger;
}

/***************************************************************************
 *  The only batch of a key under a tree: its name, or ""
 ***************************************************************************/
PRIVATE void only_batch(const char *key_dir, char *bf, size_t bfsize)
{
    bf[0] = 0;
    DIR *dir = opendir(key_dir);
    struct dirent *entry;
    int n = 0;
    while(dir && (entry = readdir(dir)) != NULL) {
        if(entry->d_name[0] == '.') {
            continue;
        }
        snprintf(bf, bfsize, "%s", entry->d_name);
        n++;
    }
    if(dir) {
        closedir(dir);
    }
    if(n != 1) {
        bf[0] = 0;
    }
}

/***************************************************************************
 *  Entries of a directory, hidden ones too, but not . and ..
 ***************************************************************************/
PRIVATE int count_entries(const char *path)
{
    int n = 0;
    DIR *dir = opendir(path);
    struct dirent *entry;
    while(dir && (entry = readdir(dir)) != NULL) {
        if(strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
            continue;
        }
        n++;
    }
    if(dir) {
        closedir(dir);
    }
    return n;
}

/***************************************************************************
 *  Bytes of the files of a directory
 ***************************************************************************/
PRIVATE json_int_t directory_bytes(const char *path)
{
    json_int_t bytes = 0;
    DIR *dir = opendir(path);
    struct dirent *entry;
    while(dir && (entry = readdir(dir)) != NULL) {
        char file[PATH_MAX];
        struct stat st;
        if(entry->d_name[0] == '.') {
            continue;
        }
        build_path(file, sizeof(file), path, entry->d_name, NULL);
        if(stat(file, &st) == 0 && S_ISREG(st.st_mode)) {
            bytes += (json_int_t)st.st_size;
        }
    }
    if(dir) {
        closedir(dir);
    }
    return bytes;
}

/***************************************************************************
 *  A batch of `key` kept in a backup tree, whole: its manifest names
 *  the first day, and the content is there
 ***************************************************************************/
PRIVATE BOOL batch_whole_in(const char *root, const char *database, const char *key)
{
    char key_dir[PATH_MAX];
    char batch[NAME_MAX + 1];
    char batch_dir[PATH_MAX];
    char file[PATH_MAX];
    build_path(key_dir, sizeof(key_dir), root, NODE_NAME, database, TOPIC_NAME, key, NULL);
    only_batch(key_dir, batch, sizeof(batch));
    if(!batch[0]) {
        fail("No batch, or more than one, of a key in the backup", key_dir);
        return FALSE;
    }
    build_path(batch_dir, sizeof(batch_dir), key_dir, batch, NULL);
    json_t *manifest = load_json_from_file(0, batch_dir, "manifest.json", 0);
    BOOL whole = manifest &&
        strcmp(kw_get_str(0, manifest, "key", "", 0), key) == 0 &&
        strcmp(kw_get_str(0, manifest, "upto", "", 0), "2000-01-01") == 0 &&
        kw_get_int(0, manifest, "rows", 0, 0) == 1;
    JSON_DECREF(manifest)
    build_path(file, sizeof(file), batch_dir, "2000-01-01.json", NULL);
    if(!whole || file_size(file) <= 0) {
        fail("A batch in the backup is not whole", batch_dir);
        return FALSE;
    }
    return TRUE;
}

/***************************************************************************
 *  The first step: the stores, their archives, and the three services
 ***************************************************************************/
PRIVATE int prepare(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    build_path(priv->base, sizeof(priv->base), getenv("HOME"), "tests_yuneta", "archive_shipping", NULL);
    build_path(priv->store, sizeof(priv->store), priv->base, "store", NULL);
    build_path(priv->archive, sizeof(priv->archive), priv->base, "archive", NULL);
    build_path(priv->archive2, sizeof(priv->archive2), priv->base, "archive2", NULL);
    build_path(priv->backup, sizeof(priv->backup), priv->base, "backup", NULL);
    build_path(priv->rsync, sizeof(priv->rsync), priv->base, "rsync", NULL);
    rmrdir(priv->base);
    mkrdir(priv->store, 02770);
    mkrdir(priv->archive, 02770);
    mkrdir(priv->archive2, 02770);
    mkrdir(priv->rsync, 02770);

    priv->tranger = open_store(priv->store, DATABASE);
    priv->tranger2 = open_store(priv->store, DATABASE2);
    if(tranger2_archive_key_files(priv->tranger, TOPIC_NAME, "key1", DAY2, priv->archive) != 1 ||
       tranger2_archive_key_files(priv->tranger, TOPIC_NAME, "key2", DAY2, priv->archive) != 1 ||
       tranger2_archive_key_files(priv->tranger2, TOPIC_NAME, "key1", DAY2, priv->archive2) != 1 ||
       tranger2_archive_key_files(priv->tranger2, TOPIC_NAME, "key2", DAY2, priv->archive2) != 1) {
        fail("The archives cannot be made", NULL);
        return -1;
    }

    char key_dir[PATH_MAX];
    char batch[NAME_MAX + 1];
    build_path(key_dir, sizeof(key_dir), priv->archive, DATABASE, TOPIC_NAME, "key1", NULL);
    only_batch(key_dir, batch, sizeof(batch));
    build_path(priv->batch_key1, sizeof(priv->batch_key1), DATABASE, TOPIC_NAME, "key1", batch, NULL);

    /*
     *  rsync: room for ONE batch
     */
    build_path(key_dir, sizeof(key_dir), priv->archive2, DATABASE2, TOPIC_NAME, "key1", NULL);
    only_batch(key_dir, batch, sizeof(batch));
    char batch_dir[PATH_MAX];
    build_path(batch_dir, sizeof(batch_dir), key_dir, batch, NULL);
    json_int_t one_batch = directory_bytes(batch_dir);

    priv->keeper = gobj_find_service("archive_keeper", TRUE);
    priv->sender = gobj_find_service("archive_sender", TRUE);
    priv->sender_rsync = gobj_find_service("archive_sender_rsync", TRUE);

    gobj_write_str_attr(priv->keeper, "backup_directory", priv->backup);
    gobj_write_str_attr(priv->sender, "archive_directory", priv->archive);
    gobj_write_str_attr(priv->sender_rsync, "archive_directory", priv->archive2);
    gobj_write_str_attr(priv->sender_rsync, "rsync_destination", priv->rsync);
    gobj_write_integer_attr(priv->sender_rsync, "max_archive_bytes", one_batch);

    gobj_start(priv->keeper);
    gobj_play(priv->keeper);
    gobj_start(priv->sender);
    gobj_play(priv->sender);
    gobj_start(priv->sender_rsync);
    gobj_play(priv->sender_rsync);
    return 0;
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE json_int_t stat_of(hgobj service, const char *name)
{
    return gobj_read_integer_attr(service, name);
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE BOOL write_file(const char *dir, const char *name, const char *content)
{
    char path[PATH_MAX];
    build_path(path, sizeof(path), dir, name, NULL);
    int fd = open(path, O_CREAT|O_WRONLY|O_TRUNC|O_CLOEXEC, 0660);
    if(fd < 0) {
        fail("A file cannot be written", path);
        return FALSE;
    }
    BOOL ok = write(fd, content, strlen(content)) == (ssize_t)strlen(content);
    close(fd);
    return ok;
}

/***************************************************************************
 *  A batch planted in the backup, and the real one of the same name in
 *  the archive, with its mark
 ***************************************************************************/
PRIVATE void plant_poison(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    char planted[PATH_MAX];
    char real[PATH_MAX];
    build_path(planted, sizeof(planted), priv->backup, NODE_NAME, POISONED, NULL);
    build_path(real, sizeof(real), priv->archive, POISONED, NULL);
    mkrdir(planted, 02770);
    mkrdir(real, 02770);
    write_file(planted, "2000-01-01.json", "poison");
    write_file(real, "2000-01-01.json", "the real rows");
    write_file(real, "manifest.json", "{}");

    char mark[NAME_MAX + 1];
    size_t len = 0;
    for(const char *c = POISONED; *c && len + 4 < sizeof(mark); c++) {
        if(*c == '/') {
            memcpy(mark + len, "%2F", 3);
            len += 3;
        } else {
            mark[len++] = *c;
        }
    }
    mark[len] = 0;
    char ready[PATH_MAX];
    build_path(ready, sizeof(ready), priv->archive, ".ready", NULL);
    write_file(ready, mark, "");
}

/***************************************************************************
 *  The planted batch untouched, the real one kept by the sender
 ***************************************************************************/
PRIVATE void check_poison(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    char planted[PATH_MAX];
    char real[PATH_MAX];
    char file[PATH_MAX];
    build_path(planted, sizeof(planted), priv->backup, NODE_NAME, POISONED, NULL);
    build_path(real, sizeof(real), priv->archive, POISONED, NULL);
    build_path(file, sizeof(file), planted, "2000-01-01.json", NULL);
    if(file_size(file) != (off_t)strlen("poison")) {
        fail("The planted batch was overwritten", file);
    }
    build_path(file, sizeof(file), real, "2000-01-01.json", NULL);
    if(file_size(file) != (off_t)strlen("the real rows")) {
        fail("The sender removed its batch on a planted one", file);
    }
    if(stat_of(priv->sender, "batches_pending") != 1) {
        fail("The sender does not keep its batch pending", NULL);
    }
}

/***************************************************************************
 *  The checks of the end, once everything has been shipped
 ***************************************************************************/
PRIVATE void check_the_end(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    /*
     *  native: three keys whole in the backup, nothing left in the archive
     */
    batch_whole_in(priv->backup, DATABASE, "key1");
    batch_whole_in(priv->backup, DATABASE, "key2");
    batch_whole_in(priv->backup, DATABASE, "key3");
    char ready[PATH_MAX];
    build_path(ready, sizeof(ready), priv->archive, ".ready", NULL);
    if(count_entries(ready) != 0) {
        fail("Ready marks left in the archive", ready);
    }
    char key_dir[PATH_MAX];
    const char *keys[] = {"key1", "key2", "key3", NULL};
    for(int k = 0; keys[k]; k++) {
        build_path(key_dir, sizeof(key_dir), priv->archive, DATABASE, TOPIC_NAME, keys[k], NULL);
        if(count_entries(key_dir) != 0) {
            fail("A batch left in the archive", key_dir);
        }
    }
    if(stat_of(priv->keeper, "batches_already") != 1) {
        fail("The batch the keeper had was not answered stored at once", NULL);
    }
    if(stat_of(priv->sender, "batches_pending") != 0 || stat_of(priv->sender, "bytes_pending") != 0) {
        fail("The native sender has batches pending", NULL);
    }

    /*
     *  rsync: key2 whole at the destination, key1 dropped, nothing left
     */
    batch_whole_in(priv->rsync, DATABASE2, "key2");
    build_path(key_dir, sizeof(key_dir), priv->rsync, NODE_NAME, DATABASE2, TOPIC_NAME, "key1", NULL);
    if(is_directory(key_dir)) {
        fail("The batch dropped over max_archive_bytes was shipped", key_dir);
    }
    build_path(ready, sizeof(ready), priv->archive2, ".ready", NULL);
    if(count_entries(ready) != 0) {
        fail("Ready marks left in the rsync archive", ready);
    }
}

/***************************************************************************
 *  Each tick: is the step done?
 ***************************************************************************/
PRIVATE void next_step(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    priv->step++;
    priv->step_timeout = start_msectimer(STEP_TIMEOUT_MS);
}

PRIVATE int ac_timeout(hgobj gobj, gobj_event_t event, json_t *kw, hgobj src)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    switch(priv->step) {
        case 0:
            if(prepare(gobj) < 0) {
                set_yuno_must_die();
                JSON_DECREF(kw)
                return 0;
            }
            next_step(gobj);
            break;

        case 1:
            /*
             *  The two batches found by the scan, shipped
             */
            if(stat_of(priv->sender, "batches_shipped") == 2 &&
                    stat_of(priv->keeper, "batches_stored") == 2) {
                /*
                 *  A third, told by its mark
                 */
                if(tranger2_archive_key_files(priv->tranger, TOPIC_NAME, "key3", DAY2, priv->archive) != 1) {
                    fail("The third batch cannot be made", NULL);
                }
                next_step(gobj);
            }
            break;

        case 2:
            if(stat_of(priv->sender, "batches_shipped") == 3 &&
                    stat_of(priv->keeper, "batches_stored") == 3) {
                /*
                 *  A batch the keeper has: the first of key1 again, from
                 *  the backup, with its mark
                 */
                char from[PATH_MAX];
                char to[PATH_MAX];
                char cmd[3 * PATH_MAX];
                build_path(from, sizeof(from), priv->backup, NODE_NAME, priv->batch_key1, NULL);
                build_path(to, sizeof(to), priv->archive, priv->batch_key1, NULL);
                snprintf(cmd, sizeof(cmd), "cp -a '%s' '%s'", from, to);
                if(system(cmd) != 0) {
                    fail("The batch cannot be copied back to the archive", cmd);
                }
                char mark[NAME_MAX + 1];
                char mark_path[PATH_MAX];
                size_t len = 0;
                for(const char *c = priv->batch_key1; *c && len + 4 < sizeof(mark); c++) {
                    if(*c == '/') {
                        memcpy(mark + len, "%2F", 3);
                        len += 3;
                    } else {
                        mark[len++] = *c;
                    }
                }
                mark[len] = 0;
                build_path(mark_path, sizeof(mark_path), priv->archive, ".ready", mark, NULL);
                int fd = open(mark_path, O_CREAT|O_WRONLY|O_CLOEXEC, 0660);
                if(fd < 0) {
                    fail("The mark cannot be written", mark_path);
                } else {
                    close(fd);
                }
                next_step(gobj);
            }
            break;

        case 3:
            if(stat_of(priv->sender, "batches_shipped") == 4 &&
                    stat_of(priv->keeper, "batches_already") == 1 &&
                    stat_of(priv->sender_rsync, "batches_shipped") == 1 &&
                    stat_of(priv->sender_rsync, "batches_dropped") == 1) {
                check_the_end(gobj);
                plant_poison(gobj);
                next_step(gobj);
            }
            break;

        case 4:
            if(stat_of(priv->keeper, "batches_refused") == 1 &&
                    stat_of(priv->sender, "batches_failed") == 1) {
                check_poison(gobj);
                EXEC_AND_RESET(tranger2_shutdown, priv->tranger)
                EXEC_AND_RESET(tranger2_shutdown, priv->tranger2)
                rmrdir(priv->base);
                set_yuno_must_die();
                JSON_DECREF(kw)
                return 0;
            }
            break;
    }

    if(priv->step > 0 && test_msectimer(priv->step_timeout)) {
        char detail[256];
        snprintf(detail, sizeof(detail),
            "step %d: shipped %d stored %d already %d, rsync shipped %d dropped %d failed %d",
            priv->step,
            (int)stat_of(priv->sender, "batches_shipped"),
            (int)stat_of(priv->keeper, "batches_stored"),
            (int)stat_of(priv->keeper, "batches_already"),
            (int)stat_of(priv->sender_rsync, "batches_shipped"),
            (int)stat_of(priv->sender_rsync, "batches_dropped"),
            (int)stat_of(priv->sender_rsync, "batches_failed")
        );
        fail("A step did not end in time", detail);
        set_yuno_must_die();
        JSON_DECREF(kw)
        return 0;
    }

    set_timeout(priv->timer, POLL_MS);
    JSON_DECREF(kw)
    return 0;
}

/***************************************************************************
 *      Framework Method play
 ***************************************************************************/
PRIVATE int mt_play(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    set_timeout(priv->timer, POLL_MS);
    return 0;
}

/***************************************************************************
 *      Framework Method pause
 ***************************************************************************/
PRIVATE int mt_pause(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    clear_timeout(priv->timer);
    hgobj services[] = {priv->sender, priv->sender_rsync, priv->keeper, NULL};
    for(int i = 0; services[i]; i++) {
        if(gobj_is_playing(services[i])) {
            gobj_pause(services[i]);
        }
    }
    return 0;
}

/***************************************************************************
 *                          FSM
 ***************************************************************************/
/*---------------------------------------------*
 *          Global methods table
 *---------------------------------------------*/
PRIVATE const GMETHODS gmt = {
    .mt_create = mt_create,
    .mt_destroy = mt_destroy,
    .mt_start = mt_start,
    .mt_stop = mt_stop,
    .mt_play = mt_play,
    .mt_pause = mt_pause,
};

/*------------------------*
 *      GClass name
 *------------------------*/
GOBJ_DEFINE_GCLASS(C_TEST1);

/*------------------------*
 *      States
 *------------------------*/

/*------------------------*
 *      Events
 *------------------------*/

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE int create_gclass(gclass_name_t gclass_name)
{
    static hgclass __gclass__ = 0;
    if(__gclass__) {
        gobj_log_error(0, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_INTERNAL,
            "msg",          "%s", "GClass ALREADY created",
            "gclass",       "%s", gclass_name,
            NULL
        );
        return -1;
    }

    /*----------------------------------------*
     *          Define States
     *----------------------------------------*/
    ev_action_t st_idle[] = {
        {EV_TIMEOUT,                ac_timeout,                 0},
        {0,0,0}
    };

    states_t states[] = {
        {ST_IDLE,                   st_idle},
        {0, 0}
    };

    event_type_t event_types[] = {
        {EV_TIMEOUT,                0},
        {0, 0}
    };

    /*----------------------------------------*
     *          Create the gclass
     *----------------------------------------*/
    __gclass__ = gclass_create(
        gclass_name,
        event_types,
        states,
        &gmt,
        0,  // lmt,
        attrs_table,
        sizeof(PRIVATE_DATA),
        0,  // authz_table,
        0,  // command_table,
        s_user_trace_level,  // s_user_trace_level,
        0   // gcflag_t
    );
    if(!__gclass__) {
        // Error already logged
        return -1;
    }

    return 0;
}

/***************************************************************************
 *
 ***************************************************************************/
PUBLIC int register_c_test1(void)
{
    return create_gclass(C_TEST1);
}
