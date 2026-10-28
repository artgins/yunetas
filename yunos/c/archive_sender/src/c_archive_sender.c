/***********************************************************************
 *          c_archive_sender.c
 *          Archive_sender GClass.
 *
 *          Ship the batches of a timeranger2 archive to a backup, and
 *          remove them
 *
 *          The archive is what tranger2_archive_key_files() leaves: one
 *          batch per purge of a key,
 *
 *              <archive_directory>/<database>/<topic>/<key>/<upto>@<t>/
 *
 *          with the files of the purge, the schema of the topic and a
 *          manifest.json. A name that starts with '.' is not a batch (one
 *          half built, or this yuno's own files). Each finished batch has a
 *          mark in <archive_directory>/.ready/, the path of the batch with
 *          '%' written %25 and '/' written %2F: this gclass watches that ONE
 *          directory (C_FS), and scans the whole tree when it plays (a
 *          batch without its mark is found there) and when the watch lost
 *          events.
 *
 *          A batch is shipped whole, the oldest first, one at a time, and
 *          removed (with its mark) only when the backup said it has it. Two
 *          transports, `transport`:
 *
 *          - "native": to an archive_keeper, over the C_IOGATE named by
 *            `output_service` (C_CHANNEL -> C_PROT_TCP4H -> C_TCP, TLS with
 *            a tcps:// url). The keeper writes the batch aside and
 *            renames it when every file is whole and fsync'ed; its "stored"
 *            is what removes the batch here. See README.md for the frames.
 *          - "rsync": `rsync_path` run twice per batch, the files and then
 *            the manifest, to <rsync_destination>/<node_name>/<path of the
 *            batch>/ (--mkpath). On the backup a batch is whole when its
 *            manifest.json is there. The exit 0 of the second run is what
 *            removes the batch here. The process is watched with a pidfd
 *            polled by the event loop: nothing blocks.
 *
 *          A batch that fails goes to the back of the queue, and nothing is
 *          tried for `retry_period` seconds. A link that closes while
 *          shipping is not a failure of the batch: it is shipped again when
 *          the link opens.
 *
 *          `max_archive_bytes` (0: no limit): above it the OLDEST batches
 *          are removed without being shipped, each one with a WARNING. A
 *          batch takes on disk what its files took in the store (they are
 *          hard links), so an archive that is never shipped is a store
 *          that is never purged: the limit keeps the node alive, and loses
 *          the oldest part of the backup instead of the newest.
 *
 *          Copyright (c) 2026, ArtGins.
 *          All Rights Reserved.
 ***********************************************************************/
#include <stdio.h>
#include <limits.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <poll.h>
#include <signal.h>
#include <spawn.h>
#include <unistd.h>
#include <dirent.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <sys/syscall.h>

#include "c_archive_sender.h"

/***************************************************************************
 *              Constants
 ***************************************************************************/
#ifndef SYS_pidfd_open
#define SYS_pidfd_open 434
#endif

#define READY_DIRECTORY     ".ready"
#define RSYNC_OUTPUT        ".rsync.out"
#define MANIFEST            "manifest.json"
#define RSYNC_OUTPUT_LOGGED 1024    // bytes of the output of a failed rsync put in the log

extern char **environ;

/***************************************************************************
 *              Structures
 ***************************************************************************/

/***************************************************************************
 *              Prototypes
 ***************************************************************************/
PRIVATE int scan_archive(hgobj gobj);
PRIVATE int add_pending(hgobj gobj, const char *relpath);
PRIVATE void remove_batch(hgobj gobj, const char *relpath, BOOL shipped);
PRIVATE void enforce_limit(hgobj gobj);
PRIVATE void ship_next(hgobj gobj);
PRIVATE void finish_batch(hgobj gobj, BOOL shipped, const char *reason);
PRIVATE int start_native(hgobj gobj, const char *relpath);
PRIVATE int pump_native(hgobj gobj);
PRIVATE int start_rsync_step(hgobj gobj);
PRIVATE void kill_rsync(hgobj gobj);
PRIVATE void update_pending_stats(hgobj gobj);

/***************************************************************************
 *          Data: config, public data, private data
 ***************************************************************************/
PRIVATE json_t *cmd_help(hgobj gobj, const char *cmd, json_t *kw, hgobj src);
PRIVATE json_t *cmd_authzs(hgobj gobj, const char *cmd, json_t *kw, hgobj src);
PRIVATE json_t *cmd_list_pending(hgobj gobj, const char *cmd, json_t *kw, hgobj src);
PRIVATE json_t *cmd_rescan(hgobj gobj, const char *cmd, json_t *kw, hgobj src);

PRIVATE sdata_desc_t pm_help[] = {
/*-PM----type-----------name------------flag------------default-----description---------- */
SDATAPM (DTP_STRING,    "cmd",          0,              0,          "command about you want help."),
SDATAPM (DTP_INTEGER,   "level",        0,              0,          "command search level in childs"),
SDATA_END()
};
PRIVATE sdata_desc_t pm_authzs[] = {
/*-PM----type-----------name------------flag------------default-----description---------- */
SDATAPM (DTP_STRING,    "authz",        0,              0,          "permission to search"),
SDATAPM (DTP_STRING,    "service",      0,              0,          "Service where to search the permission. If empty print all service's permissions"),
SDATA_END()
};
PRIVATE sdata_desc_t pm_list_pending[] = {
/*-PM----type-----------name------------flag------------default-----description---------- */
SDATAPM (DTP_INTEGER,   "limit",        0,              "100",      "How many batches, the oldest first. 0: all of them"),
SDATA_END()
};

PRIVATE const char *a_help[] = {"h", "?", 0};

PRIVATE sdata_desc_t command_table[] = {
/*-CMD---type-----------name----------------alias---------------items-----------json_fn---------description---------- */
SDATACM (DTP_SCHEMA,    "help",             a_help,             pm_help,        cmd_help,       "Command's help"),
SDATACM2 (DTP_SCHEMA,   "authzs",           0,                  0,              pm_authzs,      cmd_authzs,     "Authorization's help"),
SDATACM2 (DTP_SCHEMA,   "list-pending",     0,                  0,              pm_list_pending,cmd_list_pending,"Batches waiting to be shipped, the oldest first"),
SDATACM2 (DTP_SCHEMA,   "rescan",           0,                  0,              0,              cmd_rescan,     "Scan the archive tree again, and ship what it finds"),
SDATA_END()
};

/*---------------------------------------------*
 *      Attributes
 *---------------------------------------------*/
PRIVATE sdata_desc_t attrs_table[] = {
/*-ATTR-type--------name----------------flag----------------default-----description---------- */
SDATA (DTP_STRING,  "archive_directory",SDF_RD,             "",         "Absolute directory of the archive: the archive_directory given to tranger2_archive_key_files()"),
SDATA (DTP_STRING,  "transport",        SDF_RD,             "native",   "How the batches travel: 'native' (to an archive_keeper, over output_service) or 'rsync'"),
SDATA (DTP_STRING,  "node_name",        SDF_RD,             "",         "Name of this node in the backup: the first directory of its batches there. Empty: the host name"),
SDATA (DTP_STRING,  "output_service",   SDF_RD,             "__output_side__", "native: the C_IOGATE service whose channel goes to the archive_keeper"),
SDATA (DTP_INTEGER, "chunk_size",       SDF_RD,             "262144",   "native: bytes of a file sent in one frame"),
SDATA (DTP_INTEGER, "window",           SDF_RD,             "4",        "native: frames of data sent and not yet acknowledged"),
SDATA (DTP_STRING,  "rsync_path",       SDF_RD,             "/usr/bin/rsync", "rsync: the program"),
SDATA (DTP_STRING,  "rsync_destination",SDF_RD,             "",         "rsync: where the batches go, [user@]host:dir, rsync://host/module/dir or a local directory. Each batch goes to <rsync_destination>/<node_name>/<database>/<topic>/<key>/<batch>/"),
SDATA (DTP_LIST,    "rsync_options",    SDF_RD,             "[]",       "rsync: more arguments, before the paths, e.g. [\"-e\", \"ssh -i /yuneta/store/certs/private/backup_key\"]"),
SDATA (DTP_INTEGER, "rsync_timeout",    SDF_RD,             "300",      "rsync: --timeout, seconds without I/O before rsync gives up"),
SDATA (DTP_INTEGER, "max_archive_bytes",SDF_WR,             "0",        "Above these bytes pending, the OLDEST batches are removed without being shipped, each one with a WARNING. 0: no limit"),
SDATA (DTP_INTEGER, "retry_period",     SDF_WR,             "60",       "Seconds without trying anything after a batch failed"),

SDATA (DTP_INTEGER, "batches_pending",  SDF_RD|SDF_STATS,   "0",        "Batches waiting to be shipped"),
SDATA (DTP_INTEGER, "bytes_pending",    SDF_RD|SDF_STATS,   "0",        "Bytes of the batches waiting to be shipped"),
SDATA (DTP_INTEGER, "oldest_pending_t", SDF_RD|SDF_STATS,   "0",        "Time (the @<t> of its name) of the oldest batch waiting, 0 none"),
SDATA (DTP_INTEGER, "batches_shipped",  SDF_RD|SDF_STATS,   "0",        "Batches the backup has, and removed here"),
SDATA (DTP_INTEGER, "bytes_shipped",    SDF_RD|SDF_STATS,   "0",        "Bytes of the batches shipped"),
SDATA (DTP_INTEGER, "batches_failed",   SDF_RD|SDF_STATS,   "0",        "Tries of a batch that failed (it is tried again)"),
SDATA (DTP_INTEGER, "batches_dropped",  SDF_RD|SDF_STATS,   "0",        "Batches removed WITHOUT being shipped, over max_archive_bytes"),
SDATA (DTP_INTEGER, "bytes_dropped",    SDF_RD|SDF_STATS,   "0",        "Bytes of the batches dropped"),

SDATA (DTP_POINTER, "subscriber",       0,                  0,          "Subscriber of output-events"),
SDATA (DTP_POINTER, "user_data",        0,                  0,          "user data"),
SDATA (DTP_POINTER, "user_data2",       0,                  0,          "more user data"),
SDATA_END()
};

/*---------------------------------------------*
 *      GClass trace levels
 *---------------------------------------------*/
enum {
    TRACE_BATCHES   = 0x0001,
    TRACE_FRAMES    = 0x0002,
};
PRIVATE const trace_level_t s_user_trace_level[16] = {
{"batches",         "Trace the batches found, shipped, failed and dropped"},
{"frames",          "Trace the frames to and from the archive_keeper"},
{0, 0},
};

/*---------------------------------------------*
 *      GClass authz levels
 *---------------------------------------------*/
PRIVATE sdata_desc_t authz_table[] = {
/*-AUTHZ-- type---------name----------------flag----alias---items---description--*/
SDATA_END()
};

/*---------------------------------------------*
 *              Private data
 *---------------------------------------------*/
typedef struct _PRIVATE_DATA {
    hgobj timer;
    hgobj gobj_fs;
    hgobj gobj_output_side;
    BOOL native;
    BOOL link_open;
    char archive_directory[PATH_MAX];
    char ready_directory[PATH_MAX];
    const char *node_name;
    json_int_t chunk_size;
    json_int_t window;
    json_int_t max_archive_bytes;
    json_int_t retry_period;

    json_t *jn_pending;     // relpath -> {t, bytes, failures}
    json_int_t bytes_pending;

    /*
     *  The batch in flight
     */
    json_t *jn_batch;       // {relpath, bytes, files: [{name, size}], file, offset, in_flight}
    int fd;                 // native: the file being read

    pid_t rsync_pid;
    int rsync_pidfd;
    int rsync_step;         // 1 the files, 2 the manifest
    yev_event_h yev_rsync;
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
    priv->fd = -1;
    priv->rsync_pid = -1;
    priv->rsync_pidfd = -1;
    priv->jn_pending = json_object();

    /*
     *  SERVICE subscription model
     */
    hgobj subscriber = (hgobj)gobj_read_pointer_attr(gobj, "subscriber");
    if(subscriber) {
        gobj_subscribe_event(gobj, NULL, NULL, subscriber);
    } else if(gobj_is_pure_child(gobj)) {
        subscriber = gobj_parent(gobj);
        gobj_subscribe_event(gobj, NULL, NULL, subscriber);
    }

    /*
     *  Do copy of heavy used parameters, for quick access.
     *  HACK The writable attributes must be repeated in mt_writing method.
     */
    SET_PRIV(chunk_size,            gobj_read_integer_attr)
    SET_PRIV(window,                gobj_read_integer_attr)
    SET_PRIV(max_archive_bytes,     gobj_read_integer_attr)
    SET_PRIV(retry_period,          gobj_read_integer_attr)

    priv->node_name = gobj_read_str_attr(gobj, "node_name");
    if(empty_string(priv->node_name)) {
        priv->node_name = get_hostname();
    }
    if(priv->chunk_size <= 0) {
        priv->chunk_size = 262144;
    }
    if(priv->window <= 0) {
        priv->window = 1;
    }
}

/***************************************************************************
 *      Framework Method writing
 ***************************************************************************/
PRIVATE void mt_writing(hgobj gobj, const char *path)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    IF_EQ_SET_PRIV(max_archive_bytes,   gobj_read_integer_attr)
        enforce_limit(gobj);
    ELIF_EQ_SET_PRIV(retry_period,      gobj_read_integer_attr)
    END_EQ_SET_PRIV()
}

/***************************************************************************
 *      Framework Method destroy
 ***************************************************************************/
PRIVATE void mt_destroy(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    EXEC_AND_RESET(yev_destroy_event, priv->yev_rsync)
    if(priv->rsync_pidfd >= 0) {
        close(priv->rsync_pidfd);
        priv->rsync_pidfd = -1;
    }
    if(priv->fd >= 0) {
        close(priv->fd);
        priv->fd = -1;
    }
    JSON_DECREF(priv->jn_batch)
    JSON_DECREF(priv->jn_pending)
}

/***************************************************************************
 *      Framework Method start
 ***************************************************************************/
PRIVATE int mt_start(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    const char *archive_directory = gobj_read_str_attr(gobj, "archive_directory");
    if(empty_string(archive_directory) || archive_directory[0] != '/') {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_PARAMETER,
            "msg",          "%s", "archive_directory must be an absolute path: NOTHING shipped",
            "archive_directory", "%s", archive_directory,
            NULL
        );
        return -1;
    }
    if(!build_path(priv->archive_directory, sizeof(priv->archive_directory),
            archive_directory, NULL) ||
       !build_path(priv->ready_directory, sizeof(priv->ready_directory),
            archive_directory, READY_DIRECTORY, NULL)) {
        return -1;  // Error already logged
    }

    const char *transport = gobj_read_str_attr(gobj, "transport");
    if(strcmp(transport, "native") == 0) {
        priv->native = TRUE;
    } else if(strcmp(transport, "rsync") == 0) {
        priv->native = FALSE;
        if(empty_string(gobj_read_str_attr(gobj, "rsync_destination"))) {
            gobj_log_error(gobj, 0,
                "function",     "%s", __FUNCTION__,
                "msgset",       "%s", MSGSET_PARAMETER,
                "msg",          "%s", "transport rsync without rsync_destination: NOTHING shipped",
                NULL
            );
            return -1;
        }
    } else {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_PARAMETER,
            "msg",          "%s", "transport unknown, it is 'native' or 'rsync': NOTHING shipped",
            "transport",    "%s", transport,
            NULL
        );
        return -1;
    }

    if(!is_directory(priv->ready_directory)) {
        if(mkrdir(priv->ready_directory, 02770) < 0) {
            return -1;  // Error already logged
        }
    }
    if(!priv->gobj_fs) {
        priv->gobj_fs = gobj_create_pure_child(
            gobj_name(gobj),
            C_FS,
            json_pack("{s:s, s:b}",
                "path", priv->ready_directory,
                "recursive", 0
            ),
            gobj
        );
        if(!priv->gobj_fs) {
            return -1;  // Error already logged
        }
    }

    if(priv->native) {
        const char *output_service = gobj_read_str_attr(gobj, "output_service");
        priv->gobj_output_side = gobj_find_service(output_service, FALSE);
        if(!priv->gobj_output_side) {
            gobj_log_error(gobj, 0,
                "function",     "%s", __FUNCTION__,
                "msgset",       "%s", MSGSET_PARAMETER,
                "msg",          "%s", "transport native without its output service: NOTHING shipped",
                "output_service", "%s", output_service,
                NULL
            );
            return -1;
        }
        gobj_subscribe_event(priv->gobj_output_side, 0, 0, gobj);
    }

    return 0;
}

/***************************************************************************
 *      Framework Method stop
 ***************************************************************************/
PRIVATE int mt_stop(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    clear_timeout(priv->timer);
    kill_rsync(gobj);
    if(priv->gobj_fs && gobj_is_running(priv->gobj_fs)) {
        gobj_stop(priv->gobj_fs);
    }
    if(priv->gobj_output_side) {
        gobj_unsubscribe_event(priv->gobj_output_side, 0, 0, gobj);
        priv->gobj_output_side = 0;
    }
    return 0;
}

/***************************************************************************
 *      Framework Method play
 ***************************************************************************/
PRIVATE int mt_play(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    if(!gobj_is_running(priv->gobj_fs)) {
        gobj_start(priv->gobj_fs);
    }
    scan_archive(gobj);     // Errors already logged

    if(priv->native) {
        gobj_start_tree(priv->gobj_output_side);
    }
    ship_next(gobj);

    return 0;
}

/***************************************************************************
 *      Framework Method pause
 ***************************************************************************/
PRIVATE int mt_pause(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    clear_timeout(priv->timer);
    kill_rsync(gobj);
    if(priv->gobj_fs && gobj_is_running(priv->gobj_fs)) {
        gobj_stop(priv->gobj_fs);   // its watcher is freed by the loop: while it turns
    }
    if(priv->jn_batch) {
        finish_batch(gobj, FALSE, NULL);
    }
    if(priv->native && priv->gobj_output_side) {
        gobj_stop_tree(priv->gobj_output_side);
    }
    gobj_change_state(gobj, ST_IDLE);

    return 0;
}




                    /***************************
                     *      Commands
                     ***************************/




/***************************************************************************
 *
 ***************************************************************************/
PRIVATE json_t *cmd_help(hgobj gobj, const char *cmd, json_t *kw, hgobj src)
{
    KW_INCREF(kw);
    json_t *jn_resp = gobj_build_cmds_doc(gobj, kw);
    return msg_iev_build_response(
        gobj,
        0,
        jn_resp,
        0,
        0,
        kw  // owned
    );
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE json_t *cmd_authzs(hgobj gobj, const char *cmd, json_t *kw, hgobj src)
{
    /*
     *  The authzs doc is a LIST: it goes in `jn_data`, never in
     *  `jn_comment`, which must be a json string -- the client reads it
     *  with kw_get_str(). And a command handler must always come back
     *  through msg_iev_build_response(): it is what sets `__md_iev__`
     *  back, and an answer without it never reaches the requester.
     */
    KW_INCREF(kw)
    json_t *jn_resp = gobj_build_authzs_doc(gobj, cmd, kw);
    return msg_iev_build_response(
        gobj,
        0,
        0,
        0,
        jn_resp,
        kw  // owned
    );
}

/***************************************************************************
 *  The pending batches, the oldest first
 ***************************************************************************/
PRIVATE int cmp_pending(const void *a, const void *b)
{
    json_t *ja = *(json_t **)a;
    json_t *jb = *(json_t **)b;
    json_int_t fa = json_integer_value(json_object_get(ja, "failures"));
    json_int_t fb = json_integer_value(json_object_get(jb, "failures"));
    if(fa != fb) {
        return (fa < fb)? -1 : 1;
    }
    json_int_t ta = json_integer_value(json_object_get(ja, "t"));
    json_int_t tb = json_integer_value(json_object_get(jb, "t"));
    if(ta != tb) {
        return (ta < tb)? -1 : 1;
    }
    return strcmp(
        json_string_value(json_object_get(ja, "batch")),
        json_string_value(json_object_get(jb, "batch"))
    );
}

/*
 *  The pending batches as a list sorted by (failures, t, path): yours
 */
PRIVATE json_t *sorted_pending(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    size_t n = json_object_size(priv->jn_pending);
    json_t *list = json_array();
    if(n == 0) {
        return list;
    }
    json_t **items = gbmem_malloc(n * sizeof(json_t *));
    if(!items) {
        return list;    // Error already logged
    }
    size_t i = 0;
    const char *relpath; json_t *jn_item;
    json_object_foreach(priv->jn_pending, relpath, jn_item) {
        json_t *row = json_deep_copy(jn_item);
        json_object_set_new(row, "batch", json_string(relpath));
        items[i++] = row;
    }
    qsort(items, n, sizeof(json_t *), cmp_pending);
    for(i = 0; i < n; i++) {
        json_array_append_new(list, items[i]);
    }
    gbmem_free(items);
    return list;
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE json_t *cmd_list_pending(hgobj gobj, const char *cmd, json_t *kw, hgobj src)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    json_int_t limit = kw_get_int(gobj, kw, "limit", 100, 0);
    json_t *list = sorted_pending(gobj);
    if(limit > 0) {
        while((json_int_t)json_array_size(list) > limit) {
            json_array_remove(list, json_array_size(list) - 1);
        }
    }

    return msg_iev_build_response(
        gobj,
        0,
        json_sprintf("%s: %d batches pending, %" JSON_INTEGER_FORMAT " bytes",
            gobj_yuno_role_plus_name(),
            (int)json_object_size(priv->jn_pending),
            priv->bytes_pending
        ),
        0,
        list,
        kw  // owned
    );
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE json_t *cmd_rescan(hgobj gobj, const char *cmd, json_t *kw, hgobj src)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    if(!gobj_is_playing(gobj)) {
        return msg_iev_build_response(
            gobj,
            -1,
            json_sprintf("%s: not playing, nothing scanned", gobj_yuno_role_plus_name()),
            0,
            0,
            kw  // owned
        );
    }
    int before = (int)json_object_size(priv->jn_pending);
    scan_archive(gobj);     // Errors already logged
    int found = (int)json_object_size(priv->jn_pending) - before;
    if(gobj_current_state(gobj) == ST_IDLE) {
        ship_next(gobj);
    }

    return msg_iev_build_response(
        gobj,
        0,
        json_sprintf("%s: %d new batches found, %d pending",
            gobj_yuno_role_plus_name(),
            found,
            (int)json_object_size(priv->jn_pending)
        ),
        0,
        0,
        kw  // owned
    );
}




                    /***************************
                     *      Local Methods
                     ***************************/




/***************************************************************************
 *  The path of a batch under the archive -> its directory
 ***************************************************************************/
PRIVATE char *batch_directory(hgobj gobj, const char *relpath, char *bf, size_t bfsize)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);
    return build_path(bf, bfsize, priv->archive_directory, relpath, NULL);
}

/***************************************************************************
 *  The path of a batch <-> the name of its ready mark ('%' %25, '/' %2F)
 ***************************************************************************/
PRIVATE BOOL mark_of_batch(const char *relpath, char *bf, size_t bfsize)
{
    size_t len = 0;
    for(const char *c = relpath; *c; c++) {
        const char *s = (*c == '%')? "%25" : (*c == '/')? "%2F" : NULL;
        size_t need = s? 3 : 1;
        if(len + need >= bfsize) {
            return FALSE;
        }
        if(s) {
            memcpy(bf + len, s, 3);
        } else {
            bf[len] = *c;
        }
        len += need;
    }
    bf[len] = 0;
    return TRUE;
}

PRIVATE BOOL batch_of_mark(const char *mark, char *bf, size_t bfsize)
{
    size_t len = 0;
    for(const char *c = mark; *c; c++) {
        char out = *c;
        if(*c == '%') {
            if(strncmp(c, "%25", 3) == 0) {
                out = '%';
            } else if(strncmp(c, "%2F", 3) == 0) {
                out = '/';
            } else {
                return FALSE;
            }
            c += 2;
        }
        if(len + 1 >= bfsize) {
            return FALSE;
        }
        bf[len++] = out;
    }
    bf[len] = 0;
    return TRUE;
}

/***************************************************************************
 *  A batch path is four names that are not hidden: <db>/<topic>/<key>/<batch>
 ***************************************************************************/
PRIVATE BOOL is_batch_path(const char *relpath)
{
    int parts = 0;
    const char *p = relpath;
    while(*p) {
        const char *slash = strchr(p, '/');
        size_t len = slash? (size_t)(slash - p) : strlen(p);
        if(len == 0 || p[0] == '.' || len > NAME_MAX) {
            return FALSE;
        }
        parts++;
        if(!slash) {
            break;
        }
        p = slash + 1;
        if(!*p) {
            return FALSE;
        }
    }
    return parts == 4;
}

/***************************************************************************
 *  Unlink the ready mark of a batch, if it has one
 ***************************************************************************/
PRIVATE void unlink_mark(hgobj gobj, const char *relpath)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);
    char mark[NAME_MAX + 1];
    char mark_path[PATH_MAX];
    if(!mark_of_batch(relpath, mark, sizeof(mark))) {
        return;     // a batch whose mark could not be written: it has none
    }
    if(!build_path(mark_path, sizeof(mark_path), priv->ready_directory, mark, NULL)) {
        return;     // Error already logged
    }
    if(unlink(mark_path) < 0 && errno != ENOENT) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_SYSTEM,
            "msg",          "%s", "Cannot remove the ready mark of a batch",
            "path",         "%s", mark_path,
            "errno",        "%d", errno,
            "serrno",       "%s", strerror(errno),
            NULL
        );
    }
}

/***************************************************************************
 *  Directory names of a level of the archive (not hidden): yours, sorted
 ***************************************************************************/
PRIVATE json_t *subdirectories(hgobj gobj, const char *path)
{
    json_t *list = json_array();
    DIR *dir = opendir(path);
    if(!dir) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_SYSTEM,
            "msg",          "%s", "Cannot read a directory of the archive",
            "path",         "%s", path,
            "errno",        "%d", errno,
            "serrno",       "%s", strerror(errno),
            NULL
        );
        return list;
    }
    struct dirent *entry;
    while((entry = readdir(dir)) != NULL) {
        if(entry->d_name[0] == '.') {
            continue;
        }
        char child[PATH_MAX];
        if(!build_path(child, sizeof(child), path, entry->d_name, NULL)) {
            continue;   // Error already logged
        }
        if(is_directory(child)) {
            json_array_append_new(list, json_string(entry->d_name));
        }
    }
    closedir(dir);
    return list;
}

/***************************************************************************
 *  Walk the whole archive: every batch is pending; marks of no batch go
 ***************************************************************************/
PRIVATE int scan_archive(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    json_t *dbs = subdirectories(gobj, priv->archive_directory);
    size_t i0; json_t *jn_db;
    json_array_foreach(dbs, i0, jn_db) {
        char p_db[PATH_MAX];
        build_path(p_db, sizeof(p_db), priv->archive_directory, json_string_value(jn_db), NULL);
        json_t *topics = subdirectories(gobj, p_db);
        size_t i1; json_t *jn_topic;
        json_array_foreach(topics, i1, jn_topic) {
            char p_topic[PATH_MAX];
            build_path(p_topic, sizeof(p_topic), p_db, json_string_value(jn_topic), NULL);
            json_t *keys = subdirectories(gobj, p_topic);
            size_t i2; json_t *jn_key;
            json_array_foreach(keys, i2, jn_key) {
                char p_key[PATH_MAX];
                build_path(p_key, sizeof(p_key), p_topic, json_string_value(jn_key), NULL);
                json_t *batches = subdirectories(gobj, p_key);
                size_t i3; json_t *jn_batch;
                json_array_foreach(batches, i3, jn_batch) {
                    char relpath[PATH_MAX];
                    build_path(relpath, sizeof(relpath),
                        json_string_value(jn_db),
                        json_string_value(jn_topic),
                        json_string_value(jn_key),
                        json_string_value(jn_batch),
                        NULL
                    );
                    add_pending(gobj, relpath);     // Errors already logged
                }
                JSON_DECREF(batches)
            }
            JSON_DECREF(keys)
        }
        JSON_DECREF(topics)
    }
    JSON_DECREF(dbs)

    /*
     *  The marks of batches that are not there any more
     */
    DIR *dir = opendir(priv->ready_directory);
    if(dir) {
        struct dirent *entry;
        while((entry = readdir(dir)) != NULL) {
            if(entry->d_name[0] == '.') {
                continue;
            }
            char relpath[PATH_MAX];
            if(!batch_of_mark(entry->d_name, relpath, sizeof(relpath)) ||
                    !json_object_get(priv->jn_pending, relpath)) {
                char mark_path[PATH_MAX];
                if(build_path(mark_path, sizeof(mark_path), priv->ready_directory, entry->d_name, NULL)) {
                    unlink(mark_path);
                }
            }
        }
        closedir(dir);
    }

    enforce_limit(gobj);
    update_pending_stats(gobj);
    return 0;
}

/***************************************************************************
 *  A batch is pending: its bytes, and its t (after the '@' of its name)
 ***************************************************************************/
PRIVATE int add_pending(hgobj gobj, const char *relpath)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    if(json_object_get(priv->jn_pending, relpath)) {
        return 0;
    }
    if(!is_batch_path(relpath)) {
        gobj_log_warning(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_PARAMETER,
            "msg",          "%s", "Not the path of a batch: <database>/<topic>/<key>/<batch>, ignored",
            "batch",        "%s", relpath,
            NULL
        );
        return -1;
    }
    char path[PATH_MAX];
    if(!batch_directory(gobj, relpath, path, sizeof(path))) {
        return -1;  // Error already logged
    }
    DIR *dir = opendir(path);
    if(!dir) {
        return -1;  // gone: shipped, dropped, or a mark of a purge that did not happen
    }
    json_int_t bytes = 0;
    struct dirent *entry;
    while((entry = readdir(dir)) != NULL) {
        char file[PATH_MAX];
        struct stat st;
        if(entry->d_name[0] == '.') {
            continue;
        }
        if(build_path(file, sizeof(file), path, entry->d_name, NULL) &&
                stat(file, &st) == 0 && S_ISREG(st.st_mode)) {
            bytes += (json_int_t)st.st_size;
        }
    }
    closedir(dir);

    const char *at = strrchr(relpath, '@');
    json_int_t t = at? (json_int_t)strtoll(at + 1, NULL, 10) : 0;
    json_object_set_new(priv->jn_pending, relpath, json_pack("{s:I, s:I, s:i}",
        "t", t,
        "bytes", bytes,
        "failures", 0
    ));
    priv->bytes_pending += bytes;

    if(gobj_trace_level(gobj) & TRACE_BATCHES) {
        gobj_log_debug(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_INFO,
            "msg",          "%s", "Batch pending",
            "batch",        "%s", relpath,
            "bytes",        "%ld", (long)bytes,
            NULL
        );
    }
    return 0;
}

/***************************************************************************
 *  A batch goes: shipped, or dropped over the limit
 ***************************************************************************/
PRIVATE void remove_batch(hgobj gobj, const char *relpath, BOOL shipped)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    json_t *jn_item = json_object_get(priv->jn_pending, relpath);
    json_int_t bytes = jn_item? json_integer_value(json_object_get(jn_item, "bytes")) : 0;

    char path[PATH_MAX];
    if(batch_directory(gobj, relpath, path, sizeof(path))) {
        if(is_directory(path) && rmrdir(path) < 0) {
            // Error already logged: it would be found again by the next scan
        }
    }
    unlink_mark(gobj, relpath);

    if(shipped) {
        gobj_write_integer_attr(gobj, "batches_shipped",
            gobj_read_integer_attr(gobj, "batches_shipped") + 1);
        gobj_write_integer_attr(gobj, "bytes_shipped",
            gobj_read_integer_attr(gobj, "bytes_shipped") + bytes);
        if(gobj_trace_level(gobj) & TRACE_BATCHES) {
            gobj_log_debug(gobj, 0,
                "function",     "%s", __FUNCTION__,
                "msgset",       "%s", MSGSET_INFO,
                "msg",          "%s", "Batch shipped, removed",
                "batch",        "%s", relpath,
                "bytes",        "%ld", (long)bytes,
                NULL
            );
        }
    } else {
        gobj_write_integer_attr(gobj, "batches_dropped",
            gobj_read_integer_attr(gobj, "batches_dropped") + 1);
        gobj_write_integer_attr(gobj, "bytes_dropped",
            gobj_read_integer_attr(gobj, "bytes_dropped") + bytes);
        gobj_log_warning(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_APP,
            "msg",          "%s", "Batch removed WITHOUT being shipped: the archive is over max_archive_bytes",
            "batch",        "%s", relpath,
            "bytes",        "%ld", (long)bytes,
            "max_archive_bytes", "%ld", (long)priv->max_archive_bytes,
            NULL
        );
    }

    if(jn_item) {
        priv->bytes_pending -= bytes;
        json_object_del(priv->jn_pending, relpath);
    }
    update_pending_stats(gobj);
}

/***************************************************************************
 *  Over max_archive_bytes: the oldest go, never the one in flight
 ***************************************************************************/
PRIVATE void enforce_limit(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    if(priv->max_archive_bytes <= 0 || priv->bytes_pending <= priv->max_archive_bytes) {
        return;
    }
    const char *in_flight = priv->jn_batch?
        json_string_value(json_object_get(priv->jn_batch, "relpath")) : NULL;

    json_t *list = sorted_pending(gobj);
    /*
     *  By age only: a batch that failed is not younger for it
     */
    size_t n = json_array_size(list);
    while(priv->bytes_pending > priv->max_archive_bytes) {
        json_t *oldest = NULL;
        size_t oldest_idx = 0;
        for(size_t i = 0; i < n; i++) {
            json_t *row = json_array_get(list, i);
            if(json_is_null(json_object_get(row, "batch"))) {
                continue;
            }
            const char *relpath = json_string_value(json_object_get(row, "batch"));
            if(in_flight && strcmp(relpath, in_flight) == 0) {
                continue;
            }
            if(!oldest || json_integer_value(json_object_get(row, "t")) <
                    json_integer_value(json_object_get(oldest, "t"))) {
                oldest = row;
                oldest_idx = i;
            }
        }
        if(!oldest) {
            break;
        }
        char relpath[PATH_MAX];
        snprintf(relpath, sizeof(relpath), "%s", json_string_value(json_object_get(oldest, "batch")));
        json_object_set_new(json_array_get(list, oldest_idx), "batch", json_null());
        remove_batch(gobj, relpath, FALSE);
    }
    JSON_DECREF(list)
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE void update_pending_stats(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    json_int_t oldest = 0;
    const char *relpath; json_t *jn_item;
    json_object_foreach(priv->jn_pending, relpath, jn_item) {
        json_int_t t = json_integer_value(json_object_get(jn_item, "t"));
        if(oldest == 0 || t < oldest) {
            oldest = t;
        }
    }
    gobj_write_integer_attr(gobj, "batches_pending", (json_int_t)json_object_size(priv->jn_pending));
    gobj_write_integer_attr(gobj, "bytes_pending", priv->bytes_pending);
    gobj_write_integer_attr(gobj, "oldest_pending_t", oldest);
}

/***************************************************************************
 *  In ST_IDLE: the next batch, if there is one and a way to ship it
 ***************************************************************************/
PRIVATE void ship_next(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    if(!gobj_is_playing(gobj) || gobj_current_state(gobj) != ST_IDLE) {
        return;
    }
    if(priv->native && !priv->link_open) {
        return;     // when the link opens
    }

    json_t *list = sorted_pending(gobj);
    char relpath[PATH_MAX] = {0};
    json_int_t bytes = 0;
    BOOL gone = FALSE;
    size_t idx; json_t *jn_item;
    json_array_foreach(list, idx, jn_item) {
        snprintf(relpath, sizeof(relpath), "%s", json_string_value(json_object_get(jn_item, "batch")));
        bytes = json_integer_value(json_object_get(jn_item, "bytes"));
        char path[PATH_MAX];
        if(batch_directory(gobj, relpath, path, sizeof(path)) && is_directory(path)) {
            break;
        }
        /*
         *  Gone from outside this yuno: nothing to ship
         */
        priv->bytes_pending -= bytes;
        json_object_del(priv->jn_pending, relpath);
        unlink_mark(gobj, relpath);
        relpath[0] = 0;
        gone = TRUE;
    }
    JSON_DECREF(list)
    if(gone) {
        update_pending_stats(gobj);
    }
    if(!relpath[0]) {
        return;     // nothing pending
    }

    priv->jn_batch = json_pack("{s:s, s:I, s:[], s:i, s:I, s:i}",
        "relpath", relpath,
        "bytes", bytes,
        "files",
        "file", 0,
        "offset", (json_int_t)0,
        "in_flight", 0
    );
    gobj_change_state(gobj, ST_SHIPPING);

    int ret;
    if(priv->native) {
        ret = start_native(gobj, relpath);
    } else {
        priv->rsync_step = 1;
        ret = start_rsync_step(gobj);
    }
    if(ret < 0) {
        finish_batch(gobj, FALSE, "the batch cannot be started");
    }
}

/***************************************************************************
 *  The batch in flight ends. shipped: removed. Else, with a reason, it
 *  failed: to the back of the queue, and a wait; without one, the link
 *  closed: it waits for the link.
 ***************************************************************************/
PRIVATE void finish_batch(hgobj gobj, BOOL shipped, const char *reason)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    if(priv->fd >= 0) {
        close(priv->fd);
        priv->fd = -1;
    }
    if(!priv->jn_batch) {
        return;
    }
    char relpath[PATH_MAX];
    snprintf(relpath, sizeof(relpath), "%s", json_string_value(json_object_get(priv->jn_batch, "relpath")));
    JSON_DECREF(priv->jn_batch)

    if(shipped) {
        remove_batch(gobj, relpath, TRUE);
        gobj_change_state(gobj, ST_IDLE);
        ship_next(gobj);
        return;
    }

    if(!reason) {
        gobj_change_state(gobj, ST_IDLE);
        return;
    }

    json_t *jn_item = json_object_get(priv->jn_pending, relpath);
    json_int_t failures = 0;
    if(jn_item) {
        failures = json_integer_value(json_object_get(jn_item, "failures")) + 1;
        json_object_set_new(jn_item, "failures", json_integer(failures));
    }
    gobj_write_integer_attr(gobj, "batches_failed",
        gobj_read_integer_attr(gobj, "batches_failed") + 1);
    gobj_log_warning(gobj, 0,
        "function",     "%s", __FUNCTION__,
        "msgset",       "%s", MSGSET_APP,
        "msg",          "%s", "Batch NOT shipped: tried again later, after the others",
        "batch",        "%s", relpath,
        "reason",       "%s", reason,
        "failures",     "%ld", (long)failures,
        "retry_period", "%ld", (long)priv->retry_period,
        NULL
    );
    gobj_change_state(gobj, ST_RETRY_WAIT);
    set_timeout(priv->timer, (priv->retry_period > 0? priv->retry_period : 1) * 1000);
}




                    /***************************
                     *      Native transport
                     ***************************/




/***************************************************************************
 *  A frame: compact json, and with data a NUL and the bytes after it
 ***************************************************************************/
PRIVATE int send_frame(hgobj gobj, json_t *jn_header, const char *data, size_t len)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    char *header = json2uglystr(jn_header);
    if(!header) {
        JSON_DECREF(jn_header)
        return -1;  // Error already logged
    }
    size_t header_len = strlen(header);
    size_t size = header_len + (data? 1 + len : 0);
    gbuffer_t *gbuf = gbuffer_create(size, size);
    if(!gbuf) {
        GBMEM_FREE(header)
        JSON_DECREF(jn_header)
        return -1;  // Error already logged
    }
    gbuffer_append(gbuf, header, header_len);
    if(data) {
        gbuffer_append_char(gbuf, 0);
        if(len > 0) {
            gbuffer_append(gbuf, (void *)data, len);
        }
    }
    if(gobj_trace_level(gobj) & TRACE_FRAMES) {
        gobj_trace_json(gobj, jn_header, "frame to the archive_keeper (+%lu bytes)", (unsigned long)len);
    }
    GBMEM_FREE(header)
    JSON_DECREF(jn_header)

    return gobj_send_event(
        priv->gobj_output_side,
        EV_SEND_MESSAGE,
        json_pack("{s:I}", "gbuffer", (json_int_t)(size_t)gbuf),
        gobj
    );
}

/***************************************************************************
 *  The files of the batch, by name, the manifest last: "begin"
 ***************************************************************************/
PRIVATE int start_native(hgobj gobj, const char *relpath)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    char path[PATH_MAX];
    if(!batch_directory(gobj, relpath, path, sizeof(path))) {
        return -1;  // Error already logged
    }
    json_t *files = json_object_get(priv->jn_batch, "files");
    json_t *names = json_array();
    DIR *dir = opendir(path);
    if(!dir) {
        JSON_DECREF(names)
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_SYSTEM,
            "msg",          "%s", "Cannot read the directory of a batch",
            "path",         "%s", path,
            "errno",        "%d", errno,
            "serrno",       "%s", strerror(errno),
            NULL
        );
        return -1;
    }
    struct dirent *entry;
    while((entry = readdir(dir)) != NULL) {
        if(entry->d_name[0] == '.' || strcmp(entry->d_name, MANIFEST) == 0) {
            continue;
        }
        json_array_append_new(names, json_string(entry->d_name));
    }
    closedir(dir);
    size_t n = json_array_size(names);
    for(size_t i = 0; i + 1 < n; i++) {
        for(size_t j = i + 1; j < n; j++) {
            if(strcmp(json_string_value(json_array_get(names, j)),
                    json_string_value(json_array_get(names, i))) < 0) {
                json_t *a = json_incref(json_array_get(names, i));
                json_array_set(names, i, json_array_get(names, j));
                json_array_set_new(names, j, a);
            }
        }
    }
    json_array_append_new(names, json_string(MANIFEST));

    size_t idx; json_t *jn_name;
    json_array_foreach(names, idx, jn_name) {
        char file[PATH_MAX];
        struct stat st;
        if(!build_path(file, sizeof(file), path, json_string_value(jn_name), NULL) ||
                stat(file, &st) < 0 || !S_ISREG(st.st_mode)) {
            gobj_log_error(gobj, 0,
                "function",     "%s", __FUNCTION__,
                "msgset",       "%s", MSGSET_SYSTEM,
                "msg",          "%s", "A file of a batch cannot be read: a batch without its manifest is not whole",
                "path",         "%s", file,
                "errno",        "%d", errno,
                "serrno",       "%s", strerror(errno),
                NULL
            );
            JSON_DECREF(names)
            return -1;
        }
        json_array_append_new(files, json_pack("{s:s, s:I}",
            "name", json_string_value(jn_name),
            "size", (json_int_t)st.st_size
        ));
    }
    JSON_DECREF(names)

    return send_frame(gobj,
        json_pack("{s:s, s:s, s:s, s:O}",
            "cmd", "begin",
            "node", priv->node_name,
            "batch", relpath,
            "files", files
        ),
        NULL, 0
    );
}

/***************************************************************************
 *  Data frames while the window allows, then "end"
 ***************************************************************************/
PRIVATE int pump_native(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    const char *relpath = json_string_value(json_object_get(priv->jn_batch, "relpath"));
    json_t *files = json_object_get(priv->jn_batch, "files");
    size_t n_files = json_array_size(files);
    json_int_t file_idx = json_integer_value(json_object_get(priv->jn_batch, "file"));
    json_int_t offset = json_integer_value(json_object_get(priv->jn_batch, "offset"));
    json_int_t in_flight = json_integer_value(json_object_get(priv->jn_batch, "in_flight"));

    char *chunk = NULL;
    int ret = 0;
    while(in_flight < priv->window && (size_t)file_idx < n_files) {
        json_t *jn_file = json_array_get(files, (size_t)file_idx);
        json_int_t size = json_integer_value(json_object_get(jn_file, "size"));
        if(offset >= size) {
            if(priv->fd >= 0) {
                close(priv->fd);
                priv->fd = -1;
            }
            file_idx++;
            offset = 0;
            continue;
        }
        if(priv->fd < 0) {
            char dir[PATH_MAX];
            char path[PATH_MAX];
            if(!batch_directory(gobj, relpath, dir, sizeof(dir)) ||
                    !build_path(path, sizeof(path), dir, json_string_value(json_object_get(jn_file, "name")), NULL)) {
                ret = -1;   // Error already logged
                break;
            }
            priv->fd = open(path, O_RDONLY|O_CLOEXEC);
            if(priv->fd < 0) {
                gobj_log_error(gobj, 0,
                    "function",     "%s", __FUNCTION__,
                    "msgset",       "%s", MSGSET_SYSTEM,
                    "msg",          "%s", "Cannot open a file of a batch",
                    "path",         "%s", path,
                    "errno",        "%d", errno,
                    "serrno",       "%s", strerror(errno),
                    NULL
                );
                ret = -1;
                break;
            }
            if(lseek(priv->fd, (off_t)offset, SEEK_SET) < 0) {
                ret = -1;
                break;
            }
        }
        if(!chunk) {
            chunk = gbmem_malloc((size_t)priv->chunk_size);
            if(!chunk) {
                ret = -1;   // Error already logged
                break;
            }
        }
        size_t want = (size_t)MIN(priv->chunk_size, size - offset);
        ssize_t got = read(priv->fd, chunk, want);
        if(got <= 0) {
            gobj_log_error(gobj, 0,
                "function",     "%s", __FUNCTION__,
                "msgset",       "%s", MSGSET_SYSTEM,
                "msg",          "%s", "A file of a batch is shorter than when the batch started",
                "batch",        "%s", relpath,
                "file",         "%s", json_string_value(json_object_get(jn_file, "name")),
                "offset",       "%ld", (long)offset,
                "size",         "%ld", (long)size,
                "errno",        "%d", got < 0? errno : 0,
                NULL
            );
            ret = -1;
            break;
        }
        if(send_frame(gobj,
                json_pack("{s:s, s:s, s:I, s:I}",
                    "cmd", "data",
                    "batch", relpath,
                    "file", file_idx,
                    "offset", offset
                ),
                chunk, (size_t)got) < 0) {
            ret = -1;   // Error already logged
            break;
        }
        offset += got;
        in_flight++;
    }
    GBMEM_FREE(chunk)

    json_object_set_new(priv->jn_batch, "file", json_integer(file_idx));
    json_object_set_new(priv->jn_batch, "offset", json_integer(offset));
    json_object_set_new(priv->jn_batch, "in_flight", json_integer(in_flight));
    if(ret < 0) {
        return -1;
    }

    if((size_t)file_idx >= n_files && in_flight == 0 &&
            !json_is_true(json_object_get(priv->jn_batch, "ended"))) {
        json_object_set_new(priv->jn_batch, "ended", json_true());
        return send_frame(gobj,
            json_pack("{s:s, s:s}",
                "cmd", "end",
                "batch", relpath
            ),
            NULL, 0
        );
    }
    return 0;
}

/***************************************************************************
 *  An answer of the archive_keeper about the batch in flight
 ***************************************************************************/
PRIVATE int on_keeper_answer(hgobj gobj, json_t *jn_msg, hgobj src)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    const char *cmd = json_string_value(json_object_get(jn_msg, "cmd"));
    const char *batch = json_string_value(json_object_get(jn_msg, "batch"));
    const char *relpath = priv->jn_batch?
        json_string_value(json_object_get(priv->jn_batch, "relpath")) : NULL;

    if(!cmd || !batch || !relpath || strcmp(batch, relpath) != 0) {
        gobj_log_warning(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_PROTOCOL,
            "msg",          "%s", "Answer of the archive_keeper about no batch in flight, ignored",
            "cmd",          "%s", cmd? cmd : "",
            "batch",        "%s", batch? batch : "",
            "in_flight",    "%s", relpath? relpath : "",
            NULL
        );
        return -1;
    }

    if(strcmp(cmd, "begin-ack") == 0) {
        if(pump_native(gobj) < 0) {
            finish_batch(gobj, FALSE, "the batch cannot be read");
        }
        return 0;
    }
    if(strcmp(cmd, "data-ack") == 0) {
        json_int_t in_flight = json_integer_value(json_object_get(priv->jn_batch, "in_flight"));
        json_object_set_new(priv->jn_batch, "in_flight", json_integer(in_flight > 0? in_flight - 1 : 0));
        if(pump_native(gobj) < 0) {
            finish_batch(gobj, FALSE, "the batch cannot be read");
        }
        return 0;
    }
    if(strcmp(cmd, "stored") == 0) {
        finish_batch(gobj, TRUE, NULL);
        return 0;
    }
    if(strcmp(cmd, "refused") == 0) {
        char reason[256];
        snprintf(reason, sizeof(reason), "the archive_keeper refused it: %s",
            kw_get_str(gobj, jn_msg, "msg", "", 0)
        );
        finish_batch(gobj, FALSE, reason);
        return 0;
    }

    gobj_log_warning(gobj, 0,
        "function",     "%s", __FUNCTION__,
        "msgset",       "%s", MSGSET_PROTOCOL,
        "msg",          "%s", "Answer of the archive_keeper unknown, ignored",
        "cmd",          "%s", cmd,
        NULL
    );
    return -1;
}




                    /***************************
                     *      Rsync transport
                     ***************************/




/***************************************************************************
 *  The exit of rsync, told by its pidfd: an event of the machine
 ***************************************************************************/
PRIVATE int yev_rsync_callback(yev_event_h yev_event)
{
    hgobj gobj = yev_get_gobj(yev_event);
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    BOOL exited = (yev_get_state(yev_event) == YEV_ST_IDLE)? TRUE : FALSE;
    yev_destroy_event(yev_event);
    priv->yev_rsync = 0;
    if(priv->rsync_pidfd >= 0) {
        close(priv->rsync_pidfd);
        priv->rsync_pidfd = -1;
    }
    if(!exited || priv->rsync_pid <= 0) {
        return 0;   // stopped by this yuno, the process already reaped (kill_rsync)
    }

    int status = 0;
    pid_t pid = waitpid(priv->rsync_pid, &status, 0);
    priv->rsync_pid = -1;
    int exit_code;
    if(pid < 0) {
        exit_code = -1;
    } else if(WIFEXITED(status)) {
        exit_code = WEXITSTATUS(status);
    } else {
        exit_code = 128 + (WIFSIGNALED(status)? WTERMSIG(status) : 0);
    }

    gobj_send_event(gobj, EV_RSYNC_EXITED, json_pack("{s:i}", "exit_code", exit_code), gobj);
    return 0;
}

/***************************************************************************
 *  Kill a rsync in course (stop, pause): reaped here, its event cancelled
 ***************************************************************************/
PRIVATE void kill_rsync(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    if(priv->rsync_pid > 0) {
        kill(priv->rsync_pid, SIGKILL);
        waitpid(priv->rsync_pid, NULL, 0);
        priv->rsync_pid = -1;
    }
    if(priv->yev_rsync) {
        yev_stop_event(priv->yev_rsync);
        if(yev_event_is_stopped(priv->yev_rsync)) {
            EXEC_AND_RESET(yev_destroy_event, priv->yev_rsync)
            if(priv->rsync_pidfd >= 0) {
                close(priv->rsync_pidfd);
                priv->rsync_pidfd = -1;
            }
        }
        // else its callback comes, cancelled: it destroys it and closes the pidfd
    }
}

/***************************************************************************
 *  The tail of what the last rsync said, for the log
 ***************************************************************************/
PRIVATE void read_rsync_output(hgobj gobj, char *bf, size_t bfsize)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    bf[0] = 0;
    char path[PATH_MAX];
    if(!build_path(path, sizeof(path), priv->archive_directory, RSYNC_OUTPUT, NULL)) {
        return;     // Error already logged
    }
    int fd = open(path, O_RDONLY|O_CLOEXEC);
    if(fd < 0) {
        return;     // nothing said
    }
    off_t size = lseek(fd, 0, SEEK_END);
    off_t from = (size > (off_t)bfsize - 1)? size - (off_t)bfsize + 1 : 0;
    ssize_t got = pread(fd, bf, bfsize - 1, from);
    close(fd);
    if(got > 0) {
        bf[got] = 0;
    }
}

/***************************************************************************
 *  One rsync of the batch in flight: step 1 its files, step 2 its manifest
 ***************************************************************************/
PRIVATE int start_rsync_step(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    const char *relpath = json_string_value(json_object_get(priv->jn_batch, "relpath"));
    char batch_dir[PATH_MAX];
    char source[PATH_MAX];
    char destination[PATH_MAX];
    char timeout[64];
    char output[PATH_MAX];
    if(!batch_directory(gobj, relpath, batch_dir, sizeof(batch_dir)) ||
            !build_path(output, sizeof(output), priv->archive_directory, RSYNC_OUTPUT, NULL)) {
        return -1;  // Error already logged
    }
    if(priv->rsync_step == 1) {
        if(snprintf(source, sizeof(source), "%s/", batch_dir) >= (int)sizeof(source)) {
            gobj_log_error(gobj, 0,
                "function",     "%s", __FUNCTION__,
                "msgset",       "%s", MSGSET_PARAMETER,
                "msg",          "%s", "The path of a batch is too long for rsync",
                "batch",        "%s", relpath,
                NULL
            );
            return -1;
        }
    } else if(!build_path(source, sizeof(source), batch_dir, MANIFEST, NULL)) {
        return -1;  // Error already logged
    }
    /*
     *  Not build_path(): the destination may be host:dir or rsync://
     */
    if(snprintf(destination, sizeof(destination), "%s/%s/%s/",
            gobj_read_str_attr(gobj, "rsync_destination"),
            priv->node_name,
            relpath) >= (int)sizeof(destination)) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_PARAMETER,
            "msg",          "%s", "The rsync destination of a batch is too long",
            "batch",        "%s", relpath,
            NULL
        );
        return -1;
    }
    snprintf(timeout, sizeof(timeout), "--timeout=%d",
        (int)gobj_read_integer_attr(gobj, "rsync_timeout"));

    json_t *options = gobj_read_json_attr(gobj, "rsync_options");
    size_t n_options = json_array_size(options);
    const char **argv = gbmem_malloc((n_options + 10) * sizeof(char *));
    if(!argv) {
        return -1;  // Error already logged
    }
    size_t argc = 0;
    argv[argc++] = gobj_read_str_attr(gobj, "rsync_path");
    argv[argc++] = "-a";
    argv[argc++] = "--mkpath";
    argv[argc++] = "--protect-args";    // the names come from data: no remote shell sees them
    argv[argc++] = timeout;
    size_t idx; json_t *jn_option;
    json_array_foreach(options, idx, jn_option) {
        if(json_is_string(jn_option)) {
            argv[argc++] = json_string_value(jn_option);
        }
    }
    if(priv->rsync_step == 1) {
        argv[argc++] = "--exclude=/" MANIFEST;
    }
    argv[argc++] = source;
    argv[argc++] = destination;
    argv[argc] = NULL;

    posix_spawn_file_actions_t actions;
    posix_spawnattr_t attr;
    posix_spawn_file_actions_init(&actions);
    posix_spawn_file_actions_addopen(&actions, 0, "/dev/null", O_RDONLY, 0);
    posix_spawn_file_actions_addopen(&actions, 1, output, O_WRONLY|O_CREAT|O_TRUNC, 0660);
    posix_spawn_file_actions_adddup2(&actions, 1, 2);
    posix_spawn_file_actions_addclosefrom_np(&actions, 3);
    posix_spawnattr_init(&attr);
    sigset_t no_signals, all_signals;
    sigemptyset(&no_signals);
    sigfillset(&all_signals);
    posix_spawnattr_setsigmask(&attr, &no_signals);
    posix_spawnattr_setsigdefault(&attr, &all_signals);
    posix_spawnattr_setflags(&attr, POSIX_SPAWN_SETSIGMASK|POSIX_SPAWN_SETSIGDEF);

    pid_t pid = -1;
    int err = posix_spawn(&pid, argv[0], &actions, &attr, (char *const *)argv, environ);
    posix_spawn_file_actions_destroy(&actions);
    posix_spawnattr_destroy(&attr);

    if(gobj_trace_level(gobj) & TRACE_BATCHES) {
        gobj_log_debug(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_INFO,
            "msg",          "%s", "rsync of a batch",
            "batch",        "%s", relpath,
            "step",         "%d", priv->rsync_step,
            "source",       "%s", source,
            "destination",  "%s", destination,
            NULL
        );
    }
    gbmem_free(argv);

    if(err != 0) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_SYSTEM,
            "msg",          "%s", "Cannot run rsync",
            "rsync_path",   "%s", gobj_read_str_attr(gobj, "rsync_path"),
            "errno",        "%d", err,
            "serrno",       "%s", strerror(err),
            NULL
        );
        return -1;
    }
    priv->rsync_pid = pid;

    priv->rsync_pidfd = (int)syscall(SYS_pidfd_open, pid, 0);
    if(priv->rsync_pidfd < 0) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_SYSTEM,
            "msg",          "%s", "pidfd_open() of rsync FAILED: it is killed",
            "errno",        "%d", errno,
            "serrno",       "%s", strerror(errno),
            NULL
        );
        kill_rsync(gobj);
        return -1;
    }
    priv->yev_rsync = yev_create_poll_event(
        yuno_event_loop(),
        yev_rsync_callback,
        gobj,
        priv->rsync_pidfd,
        POLLIN
    );
    if(!priv->yev_rsync || yev_start_event(priv->yev_rsync) < 0) {
        // Error already logged
        kill_rsync(gobj);
        EXEC_AND_RESET(yev_destroy_event, priv->yev_rsync)
        if(priv->rsync_pidfd >= 0) {
            close(priv->rsync_pidfd);
            priv->rsync_pidfd = -1;
        }
        return -1;
    }
    return 0;
}




                    /***************************
                     *      Actions
                     ***************************/




/***************************************************************************
 *  A change in .ready/: a mark born (or an overflow: everything again)
 ***************************************************************************/
PRIVATE int ac_fs_changed(hgobj gobj, gobj_event_t event, json_t *kw, hgobj src)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    const char *filename = kw_get_str(gobj, kw, "filename", "", 0);
    if(empty_string(filename)) {
        /*
         *  The watch lost events: the tree says what there is
         */
        scan_archive(gobj);
    } else if(filename[0] != '.') {
        char relpath[PATH_MAX];
        char mark_path[PATH_MAX];
        if(batch_of_mark(filename, relpath, sizeof(relpath)) &&
                build_path(mark_path, sizeof(mark_path), priv->ready_directory, filename, NULL) &&
                access(mark_path, F_OK) == 0) {
            if(add_pending(gobj, relpath) < 0) {
                unlink(mark_path);  // a mark of no batch
            }
            enforce_limit(gobj);
            update_pending_stats(gobj);
        }
        // else a mark that went: this yuno removed it
    }

    if(gobj_current_state(gobj) == ST_IDLE) {
        ship_next(gobj);
    }

    KW_DECREF(kw);
    return 0;
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE int ac_on_open(hgobj gobj, gobj_event_t event, json_t *kw, hgobj src)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    priv->link_open = TRUE;
    if(gobj_current_state(gobj) == ST_IDLE) {
        ship_next(gobj);
    }

    KW_DECREF(kw);
    return 0;
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE int ac_on_close(hgobj gobj, gobj_event_t event, json_t *kw, hgobj src)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    priv->link_open = FALSE;
    if(priv->native && priv->jn_batch) {
        gobj_log_info(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_CONNECT_DISCONNECT,
            "msg",          "%s", "Link to the archive_keeper closed while shipping: the batch goes again when it opens",
            "batch",        "%s", json_string_value(json_object_get(priv->jn_batch, "relpath")),
            NULL
        );
        finish_batch(gobj, FALSE, NULL);
    }

    KW_DECREF(kw);
    return 0;
}

/***************************************************************************
 *  A frame of the archive_keeper: json, the bytes of a peer
 ***************************************************************************/
PRIVATE int ac_on_message(hgobj gobj, gobj_event_t event, json_t *kw, hgobj src)
{
    gbuffer_t *gbuf = (gbuffer_t *)(size_t)kw_get_int(gobj, kw, "gbuffer", 0, 0);
    if(!gbuf) {
        gobj_log_warning(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_PROTOCOL,
            "msg",          "%s", "Message of the archive_keeper without gbuffer",
            NULL
        );
        KW_DECREF(kw)
        return -1;
    }
    GBUFFER_INCREF(gbuf)
    json_t *jn_msg = gbuf2json_from_peer(
        gobj,
        gbuf,
        (hgobj)(size_t)kw_get_int(gobj, kw, "__temp__`channel_gobj", (json_int_t)(size_t)src, 0)
    );
    if(!jn_msg) {
        // Error already logged, as a warning: the bytes are the peer's
        KW_DECREF(kw)
        return -1;
    }
    if(gobj_trace_level(gobj) & TRACE_FRAMES) {
        gobj_trace_json(gobj, jn_msg, "frame from the archive_keeper");
    }

    if(gobj_current_state(gobj) == ST_SHIPPING && gobj_is_playing(gobj)) {
        on_keeper_answer(gobj, jn_msg, src);  // Errors already logged
    } else {
        gobj_log_warning(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_PROTOCOL,
            "msg",          "%s", "Answer of the archive_keeper with no batch in flight, ignored",
            "cmd",          "%s", kw_get_str(gobj, jn_msg, "cmd", "", 0),
            "state",        "%s", gobj_current_state(gobj),
            NULL
        );
    }

    JSON_DECREF(jn_msg)
    KW_DECREF(kw)
    return 0;
}

/***************************************************************************
 *  rsync ended: step 1 done goes to step 2, step 2 done is shipped
 ***************************************************************************/
PRIVATE int ac_rsync_exited(hgobj gobj, gobj_event_t event, json_t *kw, hgobj src)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    int exit_code = (int)kw_get_int(gobj, kw, "exit_code", -1, 0);
    KW_DECREF(kw);

    if(!priv->jn_batch) {
        return 0;   // the batch went with a pause
    }
    if(exit_code != 0) {
        char output[RSYNC_OUTPUT_LOGGED];
        read_rsync_output(gobj, output, sizeof(output));
        gobj_log_warning(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_APP,
            "msg",          "%s", "rsync of a batch FAILED",
            "batch",        "%s", json_string_value(json_object_get(priv->jn_batch, "relpath")),
            "step",         "%d", priv->rsync_step,
            "exit_code",    "%d", exit_code,
            "output",       "%s", output,
            NULL
        );
        finish_batch(gobj, FALSE, "rsync failed");
        return 0;
    }
    if(priv->rsync_step == 1) {
        priv->rsync_step = 2;
        if(start_rsync_step(gobj) < 0) {
            finish_batch(gobj, FALSE, "rsync cannot be run");
        }
        return 0;
    }
    finish_batch(gobj, TRUE, NULL);
    return 0;
}

/***************************************************************************
 *  The wait after a failure is over
 ***************************************************************************/
PRIVATE int ac_timeout(hgobj gobj, gobj_event_t event, json_t *kw, hgobj src)
{
    gobj_change_state(gobj, ST_IDLE);
    ship_next(gobj);

    KW_DECREF(kw);
    return 0;
}

/***************************************************************************
 *                          FSM
 ***************************************************************************/
/*---------------------------------------------*
 *          Global methods table
 *---------------------------------------------*/
PRIVATE const GMETHODS gmt = {
    .mt_create  = mt_create,
    .mt_destroy = mt_destroy,
    .mt_start   = mt_start,
    .mt_stop    = mt_stop,
    .mt_play    = mt_play,
    .mt_pause   = mt_pause,
    .mt_writing = mt_writing,
};

/*------------------------*
 *      GClass name
 *------------------------*/
GOBJ_DEFINE_GCLASS(C_ARCHIVE_SENDER);

/*------------------------*
 *      States
 *------------------------*/
GOBJ_DEFINE_STATE(ST_SHIPPING);
GOBJ_DEFINE_STATE(ST_RETRY_WAIT);

/*------------------------*
 *      Events
 *------------------------*/
GOBJ_DEFINE_EVENT(EV_RSYNC_EXITED);

/***************************************************************************
 *          Create the GClass
 ***************************************************************************/
PRIVATE int create_gclass(gclass_name_t gclass_name)
{
    static hgclass __gclass__ = 0;
    if(__gclass__) {
        gobj_log_error(0, 0,
            "function", "%s", __FUNCTION__,
            "msgset",   "%s", MSGSET_INTERNAL,
            "msg",      "%s", "GClass ALREADY created",
            "gclass",   "%s", gclass_name,
            NULL
        );
        return -1;
    }

    /*------------------------*
     *      States
     *------------------------*/
    ev_action_t st_idle[] = {
        {EV_FS_CHANGED,             ac_fs_changed,          0},
        {EV_ON_OPEN,                ac_on_open,             0},
        {EV_ON_CLOSE,               ac_on_close,            0},
        {EV_ON_MESSAGE,             ac_on_message,          0},
        {0,0,0}
    };
    ev_action_t st_shipping[] = {
        {EV_FS_CHANGED,             ac_fs_changed,          0},
        {EV_ON_OPEN,                ac_on_open,             0},
        {EV_ON_CLOSE,               ac_on_close,            0},
        {EV_ON_MESSAGE,             ac_on_message,          0},
        {EV_RSYNC_EXITED,           ac_rsync_exited,        0},
        {0,0,0}
    };
    ev_action_t st_retry_wait[] = {
        {EV_FS_CHANGED,             ac_fs_changed,          0},
        {EV_ON_OPEN,                ac_on_open,             0},
        {EV_ON_CLOSE,               ac_on_close,            0},
        {EV_ON_MESSAGE,             ac_on_message,          0},
        {EV_TIMEOUT,                ac_timeout,             0},
        {0,0,0}
    };

    states_t states[] = {
        {ST_IDLE,           st_idle},
        {ST_SHIPPING,       st_shipping},
        {ST_RETRY_WAIT,     st_retry_wait},
        {0, 0}
    };

    /*------------------------*
     *      Events
     *------------------------*/
    event_type_t event_types[] = {
        {EV_FS_CHANGED,             0},
        {EV_ON_OPEN,                0},
        {EV_ON_CLOSE,               0},
        {EV_ON_MESSAGE,             0},
        {EV_RSYNC_EXITED,           0},
        {EV_TIMEOUT,                0},
        {NULL, 0}
    };

    /*----------------------------------------*
     *          Register GClass
     *----------------------------------------*/
    __gclass__ = gclass_create(
        gclass_name,
        event_types,
        states,
        &gmt,
        0, // local methods
        attrs_table,
        sizeof(PRIVATE_DATA),
        authz_table,
        command_table,
        s_user_trace_level,
        0 // gcflags
    );
    if(!__gclass__) {
        return -1;
    }

    return 0;
}

/***************************************************************************
 *              Public access
 ***************************************************************************/
PUBLIC int register_c_archive_sender(void)
{
    return create_gclass(C_ARCHIVE_SENDER);
}
