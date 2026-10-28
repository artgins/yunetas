/***********************************************************************
 *          c_archive_keeper.c
 *          Archive_keeper GClass.
 *
 *          Keep the batches of timeranger2 archives that archive_senders
 *          ship
 *
 *          The other end of the "native" transport of archive_sender (see
 *          its README.md for the frames). Each channel of the C_IOGATE
 *          named by `input_service` (C_TCP_S -> C_CHANNEL -> C_PROT_TCP4H
 *          -> C_TCP, TLS with a tcps:// url) is one sender, with at most
 *          one batch in flight. A batch is kept at
 *
 *              <backup_directory>/<node>/<database>/<topic>/<key>/<batch>/
 *
 *          It is written aside, in .part-<batch> next to it, and renamed
 *          when every file has its size and is fsync'ed: a name without the
 *          '.' is a whole batch. Only then "stored" is answered, and only
 *          that removes the batch at the sender. A batch that is already
 *          there is "stored" at once: a sender that crashed between the
 *          answer and its removal ships it again.
 *
 *          Everything in a frame is the peer's: a name that is not one
 *          plain name (empty, hidden, with a '/', too long), data out of
 *          its place or beyond the size declared is refused with a
 *          WARNING, and the batch aside is removed. A batch that is here
 *          already is "stored" only if it has the files and the sizes the
 *          begin declares: a peer that wrote a batch of the same name first
 *          does not make the real sender remove its own. `allowed_nodes` is
 *          required (["*"]: any node, said on purpose): the node a sender
 *          names is its claim, not a proof.
 *
 *          Copyright (c) 2026, ArtGins.
 *          All Rights Reserved.
 ***********************************************************************/
#include <stdio.h>
#include <limits.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>
#include <dirent.h>
#include <sys/stat.h>

#include "c_archive_keeper.h"

/***************************************************************************
 *              Constants
 ***************************************************************************/
#define MAX_LOG_DUMP_SIZE   (256)   // Cap the data dump added to logs, for very large frames

/***************************************************************************
 *              Structures
 ***************************************************************************/

/***************************************************************************
 *              Prototypes
 ***************************************************************************/
PRIVATE void discard_session(hgobj gobj, const char *channel, BOOL remove_aside);

/***************************************************************************
 *          Data: config, public data, private data
 ***************************************************************************/
PRIVATE json_t *cmd_help(hgobj gobj, const char *cmd, json_t *kw, hgobj src);
PRIVATE json_t *cmd_authzs(hgobj gobj, const char *cmd, json_t *kw, hgobj src);
PRIVATE json_t *cmd_list_sessions(hgobj gobj, const char *cmd, json_t *kw, hgobj src);

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

PRIVATE const char *a_help[] = {"h", "?", 0};

PRIVATE sdata_desc_t command_table[] = {
/*-CMD---type-----------name----------------alias---------------items-----------json_fn---------description---------- */
SDATACM (DTP_SCHEMA,    "help",             a_help,             pm_help,        cmd_help,       "Command's help"),
SDATACM2 (DTP_SCHEMA,   "authzs",           0,                  0,              pm_authzs,      cmd_authzs,     "Authorization's help"),
SDATACM2 (DTP_SCHEMA,   "list-sessions",    0,                  0,              0,              cmd_list_sessions,"The batches being received, one per channel"),
SDATA_END()
};

/*---------------------------------------------*
 *      Attributes
 *---------------------------------------------*/
PRIVATE sdata_desc_t attrs_table[] = {
/*-ATTR-type--------name----------------flag----------------default-----description---------- */
SDATA (DTP_STRING,  "backup_directory", SDF_RD,             "",         "Absolute directory where the batches are kept: <backup_directory>/<node>/<database>/<topic>/<key>/<batch>/"),
SDATA (DTP_LIST,    "allowed_nodes",    SDF_RD,             "[]",       "The node names whose batches are taken, [\"*\"] any. Required: empty, nothing is received"),
SDATA (DTP_STRING,  "input_service",    SDF_RD,             "__input_side__", "The C_IOGATE service whose channels are the archive_senders"),
SDATA (DTP_INTEGER, "max_files",        SDF_RD,             "10000",    "Most files a batch may declare"),

SDATA (DTP_INTEGER, "batches_stored",   SDF_RD|SDF_STATS,   "0",        "Batches received whole"),
SDATA (DTP_INTEGER, "bytes_stored",     SDF_RD|SDF_STATS,   "0",        "Bytes of the batches received whole"),
SDATA (DTP_INTEGER, "batches_already",  SDF_RD|SDF_STATS,   "0",        "Batches offered that were already here (answered stored)"),
SDATA (DTP_INTEGER, "batches_refused",  SDF_RD|SDF_STATS,   "0",        "Batches refused (logged)"),

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
{"batches",         "Trace the batches begun, stored and refused"},
{"frames",          "Trace the frames to and from the archive_senders"},
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
    hgobj gobj_input_side;
    char backup_directory[PATH_MAX];
    json_int_t max_files;

    /*
     *  channel name -> {node, batch, aside, final, files: [{name, size, written}], file, fd}
     */
    json_t *jn_sessions;
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

    priv->jn_sessions = json_object();

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
    SET_PRIV(max_files,             gobj_read_integer_attr)
}

/***************************************************************************
 *      Framework Method writing
 ***************************************************************************/
PRIVATE void mt_writing(hgobj gobj, const char *path)
{
}

/***************************************************************************
 *      Framework Method destroy
 ***************************************************************************/
PRIVATE void mt_destroy(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    const char *channel; json_t *jn_session; void *tmp;
    json_object_foreach_safe(priv->jn_sessions, tmp, channel, jn_session) {
        discard_session(gobj, channel, FALSE);
    }
    JSON_DECREF(priv->jn_sessions)
}

/***************************************************************************
 *      Framework Method start
 ***************************************************************************/
PRIVATE int mt_start(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    const char *backup_directory = gobj_read_str_attr(gobj, "backup_directory");
    if(empty_string(backup_directory) || backup_directory[0] != '/') {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_PARAMETER,
            "msg",          "%s", "backup_directory must be an absolute path: NOTHING received",
            "backup_directory", "%s", backup_directory,
            NULL
        );
        return -1;
    }
    if(!build_path(priv->backup_directory, sizeof(priv->backup_directory), backup_directory, NULL)) {
        return -1;  // Error already logged
    }
    if(!is_directory(priv->backup_directory)) {
        if(mkrdir(priv->backup_directory, 02770) < 0) {
            return -1;  // Error already logged
        }
    }

    if(json_array_size(gobj_read_json_attr(gobj, "allowed_nodes")) == 0) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_PARAMETER,
            "msg",          "%s", "allowed_nodes is empty: NOTHING received. Name the nodes, or [\"*\"] for any",
            NULL
        );
        return -1;
    }

    const char *input_service = gobj_read_str_attr(gobj, "input_service");
    priv->gobj_input_side = gobj_find_service(input_service, FALSE);
    if(!priv->gobj_input_side) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_PARAMETER,
            "msg",          "%s", "The input service is not there: NOTHING received",
            "input_service", "%s", input_service,
            NULL
        );
        return -1;
    }
    gobj_subscribe_event(priv->gobj_input_side, 0, 0, gobj);
    return 0;
}

/***************************************************************************
 *      Framework Method stop
 ***************************************************************************/
PRIVATE int mt_stop(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    if(priv->gobj_input_side) {
        gobj_unsubscribe_event(priv->gobj_input_side, 0, 0, gobj);
        priv->gobj_input_side = 0;
    }
    return 0;
}

/***************************************************************************
 *      Framework Method play
 ***************************************************************************/
PRIVATE int mt_play(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    gobj_start_tree(priv->gobj_input_side);
    return 0;
}

/***************************************************************************
 *      Framework Method pause
 ***************************************************************************/
PRIVATE int mt_pause(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    if(priv->gobj_input_side) {
        gobj_stop_tree(priv->gobj_input_side);
    }
    const char *channel; json_t *jn_session; void *tmp;
    json_object_foreach_safe(priv->jn_sessions, tmp, channel, jn_session) {
        discard_session(gobj, channel, TRUE);
    }
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
 *
 ***************************************************************************/
PRIVATE json_t *cmd_list_sessions(hgobj gobj, const char *cmd, json_t *kw, hgobj src)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    json_t *list = json_array();
    const char *channel; json_t *jn_session;
    json_object_foreach(priv->jn_sessions, channel, jn_session) {
        json_array_append_new(list, json_pack("{s:s, s:O, s:O, s:O}",
            "channel", channel,
            "node", json_object_get(jn_session, "node"),
            "batch", json_object_get(jn_session, "batch"),
            "files", json_object_get(jn_session, "files")
        ));
    }

    return msg_iev_build_response(
        gobj,
        0,
        0,
        0,
        list,
        kw  // owned
    );
}




                    /***************************
                     *      Local Methods
                     ***************************/




/***************************************************************************
 *  One plain name: not empty, not hidden, no '/', not too long
 ***************************************************************************/
PRIVATE BOOL is_plain_name(const char *name)
{
    if(!name || !*name || name[0] == '.' || strchr(name, '/') || strlen(name) > NAME_MAX) {
        return FALSE;
    }
    return TRUE;
}

/***************************************************************************
 *  <database>/<topic>/<key>/<batch>, four plain names
 ***************************************************************************/
PRIVATE BOOL is_batch_path(const char *relpath)
{
    if(!relpath) {
        return FALSE;
    }
    char bf[PATH_MAX];
    if(snprintf(bf, sizeof(bf), "%s", relpath) >= (int)sizeof(bf)) {
        return FALSE;
    }
    int parts = 0;
    char *save = NULL;
    if(bf[0] == '/' || bf[strlen(bf) - 1] == '/' || strstr(bf, "//")) {
        return FALSE;
    }
    for(char *p = strtok_r(bf, "/", &save); p; p = strtok_r(NULL, "/", &save)) {
        if(!is_plain_name(p)) {
            return FALSE;
        }
        parts++;
    }
    return parts == 4;
}

/***************************************************************************
 *  A batch kept here has the files of `files` ([{name, size}]), and no other
 ***************************************************************************/
PRIVATE BOOL same_files(const char *path, json_t *files)
{
    size_t found = 0;
    DIR *dir = opendir(path);
    if(!dir) {
        return FALSE;
    }
    struct dirent *entry;
    BOOL same = TRUE;
    while(same && (entry = readdir(dir)) != NULL) {
        if(entry->d_name[0] == '.') {
            continue;
        }
        char file[PATH_MAX];
        struct stat st;
        json_t *declared = NULL;
        size_t idx; json_t *jn_file;
        json_array_foreach(files, idx, jn_file) {
            if(strcmp(json_string_value(json_object_get(jn_file, "name")), entry->d_name) == 0) {
                declared = jn_file;
                break;
            }
        }
        if(!declared ||
                !build_path(file, sizeof(file), path, entry->d_name, NULL) ||
                stat(file, &st) < 0 || !S_ISREG(st.st_mode) ||
                (json_int_t)st.st_size != json_integer_value(json_object_get(declared, "size"))) {
            same = FALSE;
        }
        found++;
    }
    closedir(dir);
    return same && found == json_array_size(files);
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE int fsync_path(hgobj gobj, const char *path, BOOL directory)
{
    int fd = open(path, (directory? O_RDONLY|O_DIRECTORY : O_WRONLY)|O_CLOEXEC);
    if(fd < 0 || fsync(fd) < 0) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_SYSTEM,
            "msg",          "%s", "fsync() FAILED",
            "path",         "%s", path,
            "errno",        "%d", errno,
            "serrno",       "%s", strerror(errno),
            NULL
        );
        if(fd >= 0) {
            close(fd);
        }
        return -1;
    }
    close(fd);
    return 0;
}

/***************************************************************************
 *  A frame to a sender: compact json
 ***************************************************************************/
PRIVATE int send_answer(hgobj gobj, hgobj channel_gobj, json_t *jn_answer)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    if(gobj_trace_level(gobj) & TRACE_FRAMES) {
        gobj_trace_json(gobj, jn_answer, "frame to an archive_sender");
    }
    gbuffer_t *gbuf = json2gbuf(0, jn_answer, JSON_COMPACT);    // jn_answer owned
    if(!gbuf) {
        return -1;  // Error already logged
    }
    if(!priv->gobj_input_side) {
        GBUFFER_DECREF(gbuf)
        return -1;  // stopping: nobody to answer
    }
    return gobj_send_event(
        priv->gobj_input_side,
        EV_SEND_MESSAGE,
        json_pack("{s:I, s:{s:I}}",
            "gbuffer", (json_int_t)(size_t)gbuf,
            "__temp__",
                "channel_gobj", (json_int_t)(size_t)channel_gobj
        ),
        gobj
    );
}

/***************************************************************************
 *  "refused": WARNING (the cause is the peer, or this disk: logged where it
 *  was seen), the batch aside removed
 ***************************************************************************/
PRIVATE int refuse(
    hgobj gobj,
    hgobj channel_gobj,
    const char *channel,
    const char *batch,
    const char *reason
)
{
    gobj_log_warning(gobj, 0,
        "function",     "%s", __FUNCTION__,
        "msgset",       "%s", MSGSET_PROTOCOL,
        "msg",          "%s", "Batch refused",
        "channel",      "%s", channel,
        "batch",        "%s", batch? batch : "",
        "reason",       "%s", reason,
        NULL
    );
    gobj_write_integer_attr(gobj, "batches_refused",
        gobj_read_integer_attr(gobj, "batches_refused") + 1);
    discard_session(gobj, channel, TRUE);
    return send_answer(gobj, channel_gobj, json_pack("{s:s, s:s, s:s}",
        "cmd", "refused",
        "batch", batch? batch : "",
        "msg", reason
    ));
}

/***************************************************************************
 *  A session ends: its file closed, and its batch aside removed if asked
 ***************************************************************************/
PRIVATE void discard_session(hgobj gobj, const char *channel, BOOL remove_aside)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    json_t *jn_session = json_object_get(priv->jn_sessions, channel);
    if(!jn_session) {
        return;
    }
    int fd = (int)json_integer_value(json_object_get(jn_session, "fd"));
    if(fd >= 0) {
        close(fd);
    }
    const char *aside = json_string_value(json_object_get(jn_session, "aside"));
    if(remove_aside && aside && is_directory(aside)) {
        rmrdir(aside);  // a failure is logged by it, and it is wiped by the next begin
    }
    json_object_del(priv->jn_sessions, channel);
}

/***************************************************************************
 *  "begin": a batch, aside, with its files created empty
 ***************************************************************************/
PRIVATE int on_begin(
    hgobj gobj,
    hgobj channel_gobj,
    const char *channel,
    json_t *jn_msg
)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    const char *node = json_string_value(json_object_get(jn_msg, "node"));
    const char *batch = json_string_value(json_object_get(jn_msg, "batch"));
    json_t *files = json_object_get(jn_msg, "files");

    discard_session(gobj, channel, TRUE);   // a batch left by this channel goes

    if(!is_plain_name(node)) {
        return refuse(gobj, channel_gobj, channel, batch, "the node is not a plain name");
    }
    json_t *allowed = gobj_read_json_attr(gobj, "allowed_nodes");
    BOOL found = FALSE;
    size_t i; json_t *jn_node;
    json_array_foreach(allowed, i, jn_node) {
        const char *allowed_node = json_string_value(jn_node);
        if(allowed_node && (strcmp(allowed_node, "*") == 0 || strcmp(allowed_node, node) == 0)) {
            found = TRUE;
            break;
        }
    }
    if(!found) {
        return refuse(gobj, channel_gobj, channel, batch, "the node is not in allowed_nodes");
    }
    if(!is_batch_path(batch)) {
        return refuse(gobj, channel_gobj, channel, batch,
            "the batch is not <database>/<topic>/<key>/<batch> of plain names");
    }
    if(!json_is_array(files) || json_array_size(files) == 0 ||
            (json_int_t)json_array_size(files) > priv->max_files) {
        return refuse(gobj, channel_gobj, channel, batch, "the files are not a list of 1 to max_files");
    }
    json_t *names = json_object();
    size_t idx; json_t *jn_file;
    json_array_foreach(files, idx, jn_file) {
        const char *name = json_string_value(json_object_get(jn_file, "name"));
        json_t *jn_size = json_object_get(jn_file, "size");
        if(!is_plain_name(name) || !json_is_integer(jn_size) || json_integer_value(jn_size) < 0 ||
                json_object_get(names, name)) {
            JSON_DECREF(names)
            return refuse(gobj, channel_gobj, channel, batch,
                "a file is not {name: a plain name, not repeated, size: >= 0}");
        }
        json_object_set_new(names, name, json_true());
    }
    JSON_DECREF(names)

    char final[PATH_MAX];
    if(!build_path(final, sizeof(final), priv->backup_directory, node, batch, NULL)) {
        return refuse(gobj, channel_gobj, channel, batch, "the path of the batch is too long");
    }
    if(is_directory(final)) {
        /*
         *  Here already: the sender did not hear its "stored". Only if it
         *  is THIS batch, the files and sizes declared: a batch of the same
         *  name written first by another peer is kept, and refused
         */
        if(!same_files(final, files)) {
            gobj_log_warning(gobj, 0,
                "function",     "%s", __FUNCTION__,
                "msgset",       "%s", MSGSET_PROTOCOL,
                "msg",          "%s", "A batch of this name is here with other files or sizes: kept as it is, nothing overwritten",
                "node",         "%s", node,
                "batch",        "%s", batch,
                "path",         "%s", final,
                NULL
            );
            return refuse(gobj, channel_gobj, channel, batch,
                "a different batch of this name is here");
        }
        gobj_write_integer_attr(gobj, "batches_already",
            gobj_read_integer_attr(gobj, "batches_already") + 1);
        if(gobj_trace_level(gobj) & TRACE_BATCHES) {
            gobj_log_debug(gobj, 0,
                "function",     "%s", __FUNCTION__,
                "msgset",       "%s", MSGSET_INFO,
                "msg",          "%s", "Batch already here: stored",
                "node",         "%s", node,
                "batch",        "%s", batch,
                NULL
            );
        }
        return send_answer(gobj, channel_gobj, json_pack("{s:s, s:s}",
            "cmd", "stored",
            "batch", batch
        ));
    }

    char parent[PATH_MAX];
    char aside[PATH_MAX];
    char aside_name[NAME_MAX + 1];
    snprintf(parent, sizeof(parent), "%s", final);
    char *slash = strrchr(parent, '/');
    *slash = 0;
    if(snprintf(aside_name, sizeof(aside_name), ".part-%s", slash + 1) >= (int)sizeof(aside_name) ||
            !build_path(aside, sizeof(aside), parent, aside_name, NULL)) {
        return refuse(gobj, channel_gobj, channel, batch, "the name of the batch is too long");
    }
    if((!is_directory(parent) && mkrdir(parent, 02770) < 0) ||
            (is_directory(aside) && rmrdir(aside) < 0) ||
            mkrdir(aside, 02770) < 0) {
        // Error already logged
        return refuse(gobj, channel_gobj, channel, batch, "the batch cannot be written here");
    }

    json_t *jn_files = json_array();
    json_array_foreach(files, idx, jn_file) {
        const char *name = json_string_value(json_object_get(jn_file, "name"));
        char path[PATH_MAX];
        int fd = -1;
        if(build_path(path, sizeof(path), aside, name, NULL)) {
            fd = open(path, O_WRONLY|O_CREAT|O_TRUNC|O_CLOEXEC, 0660);
        }
        if(fd < 0) {
            gobj_log_error(gobj, 0,
                "function",     "%s", __FUNCTION__,
                "msgset",       "%s", MSGSET_SYSTEM,
                "msg",          "%s", "Cannot create a file of a batch",
                "path",         "%s", path,
                "errno",        "%d", errno,
                "serrno",       "%s", strerror(errno),
                NULL
            );
            JSON_DECREF(jn_files)
            rmrdir(aside);
            return refuse(gobj, channel_gobj, channel, batch, "the batch cannot be written here");
        }
        close(fd);
        json_array_append_new(jn_files, json_pack("{s:s, s:I, s:I}",
            "name", name,
            "size", json_integer_value(json_object_get(jn_file, "size")),
            "written", (json_int_t)0
        ));
    }

    json_object_set_new(priv->jn_sessions, channel, json_pack("{s:s, s:s, s:s, s:s, s:o, s:i, s:i}",
        "node", node,
        "batch", batch,
        "aside", aside,
        "final", final,
        "files", jn_files,
        "file", -1,
        "fd", -1
    ));
    if(gobj_trace_level(gobj) & TRACE_BATCHES) {
        gobj_log_debug(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_INFO,
            "msg",          "%s", "Batch begun",
            "node",         "%s", node,
            "batch",        "%s", batch,
            "files",        "%d", (int)json_array_size(jn_files),
            NULL
        );
    }
    return send_answer(gobj, channel_gobj, json_pack("{s:s, s:s}",
        "cmd", "begin-ack",
        "batch", batch
    ));
}

/***************************************************************************
 *  "data": bytes of one file, at the place it is written up to
 ***************************************************************************/
PRIVATE int on_data(
    hgobj gobj,
    hgobj channel_gobj,
    const char *channel,
    json_t *jn_msg,
    const char *data,
    size_t len
)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    const char *batch = json_string_value(json_object_get(jn_msg, "batch"));
    json_t *jn_session = json_object_get(priv->jn_sessions, channel);
    if(!jn_session || !batch ||
            strcmp(batch, json_string_value(json_object_get(jn_session, "batch"))) != 0) {
        return refuse(gobj, channel_gobj, channel, batch, "data of a batch not begun");
    }
    json_t *jn_idx = json_object_get(jn_msg, "file");
    json_t *jn_offset = json_object_get(jn_msg, "offset");
    json_t *files = json_object_get(jn_session, "files");
    if(!json_is_integer(jn_idx) || !json_is_integer(jn_offset) ||
            json_integer_value(jn_idx) < 0 ||
            (size_t)json_integer_value(jn_idx) >= json_array_size(files)) {
        return refuse(gobj, channel_gobj, channel, batch, "data of no file of the batch");
    }
    json_int_t file_idx = json_integer_value(jn_idx);
    json_int_t offset = json_integer_value(jn_offset);
    json_t *jn_file = json_array_get(files, (size_t)file_idx);
    json_int_t size = json_integer_value(json_object_get(jn_file, "size"));
    json_int_t written = json_integer_value(json_object_get(jn_file, "written"));
    if(offset != written || offset + (json_int_t)len > size) {
        return refuse(gobj, channel_gobj, channel, batch,
            "data out of its place, or beyond the size of its file");
    }

    int fd = (int)json_integer_value(json_object_get(jn_session, "fd"));
    if(json_integer_value(json_object_get(jn_session, "file")) != file_idx) {
        if(fd >= 0) {
            close(fd);
        }
        char path[PATH_MAX];
        fd = -1;
        if(build_path(path, sizeof(path),
                json_string_value(json_object_get(jn_session, "aside")),
                json_string_value(json_object_get(jn_file, "name")),
                NULL)) {
            fd = open(path, O_WRONLY|O_CLOEXEC);
        }
        json_object_set_new(jn_session, "fd", json_integer(fd));
        json_object_set_new(jn_session, "file", json_integer(file_idx));
        if(fd < 0) {
            gobj_log_error(gobj, 0,
                "function",     "%s", __FUNCTION__,
                "msgset",       "%s", MSGSET_SYSTEM,
                "msg",          "%s", "Cannot open a file of a batch",
                "path",         "%s", path,
                "errno",        "%d", errno,
                "serrno",       "%s", strerror(errno),
                NULL
            );
            return refuse(gobj, channel_gobj, channel, batch, "the batch cannot be written here");
        }
    }

    size_t done = 0;
    while(done < len) {
        ssize_t n = pwrite(fd, data + done, len - done, (off_t)(offset + (json_int_t)done));
        if(n < 0) {
            if(errno == EINTR) {
                continue;
            }
            gobj_log_error(gobj, 0,
                "function",     "%s", __FUNCTION__,
                "msgset",       "%s", MSGSET_SYSTEM,
                "msg",          "%s", "Cannot write a file of a batch",
                "batch",        "%s", batch,
                "file",         "%s", json_string_value(json_object_get(jn_file, "name")),
                "errno",        "%d", errno,
                "serrno",       "%s", strerror(errno),
                NULL
            );
            return refuse(gobj, channel_gobj, channel, batch, "the batch cannot be written here");
        }
        done += (size_t)n;
    }
    json_object_set_new(jn_file, "written", json_integer(written + (json_int_t)len));

    return send_answer(gobj, channel_gobj, json_pack("{s:s, s:s, s:I, s:I}",
        "cmd", "data-ack",
        "batch", batch,
        "file", file_idx,
        "offset", offset + (json_int_t)len
    ));
}

/***************************************************************************
 *  "end": every file whole, fsync'ed, then the rename makes the batch
 ***************************************************************************/
PRIVATE int on_end(
    hgobj gobj,
    hgobj channel_gobj,
    const char *channel,
    json_t *jn_msg
)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    const char *batch = json_string_value(json_object_get(jn_msg, "batch"));
    json_t *jn_session = json_object_get(priv->jn_sessions, channel);
    if(!jn_session || !batch ||
            strcmp(batch, json_string_value(json_object_get(jn_session, "batch"))) != 0) {
        return refuse(gobj, channel_gobj, channel, batch, "end of a batch not begun");
    }
    int fd = (int)json_integer_value(json_object_get(jn_session, "fd"));
    if(fd >= 0) {
        close(fd);
        json_object_set_new(jn_session, "fd", json_integer(-1));
        json_object_set_new(jn_session, "file", json_integer(-1));
    }

    const char *aside = json_string_value(json_object_get(jn_session, "aside"));
    const char *final = json_string_value(json_object_get(jn_session, "final"));
    json_int_t bytes = 0;
    size_t idx; json_t *jn_file;
    json_array_foreach(json_object_get(jn_session, "files"), idx, jn_file) {
        json_int_t size = json_integer_value(json_object_get(jn_file, "size"));
        if(json_integer_value(json_object_get(jn_file, "written")) != size) {
            return refuse(gobj, channel_gobj, channel, batch, "end of a batch with a file not whole");
        }
        char path[PATH_MAX];
        if(!build_path(path, sizeof(path), aside, json_string_value(json_object_get(jn_file, "name")), NULL) ||
                fsync_path(gobj, path, FALSE) < 0) {
            // Error already logged
            return refuse(gobj, channel_gobj, channel, batch, "the batch cannot be made durable here");
        }
        bytes += size;
    }
    if(fsync_path(gobj, aside, TRUE) < 0) {
        // Error already logged
        return refuse(gobj, channel_gobj, channel, batch, "the batch cannot be made durable here");
    }
    if(rename(aside, final) < 0) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_SYSTEM,
            "msg",          "%s", "rename() of a batch FAILED",
            "path",         "%s", aside,
            "errno",        "%d", errno,
            "serrno",       "%s", strerror(errno),
            NULL
        );
        return refuse(gobj, channel_gobj, channel, batch, "the batch cannot be made durable here");
    }
    char parent[PATH_MAX];
    snprintf(parent, sizeof(parent), "%s", final);
    *strrchr(parent, '/') = 0;
    if(fsync_path(gobj, parent, TRUE) < 0) {
        /*
         *  Error already logged: the batch may not survive a power cut, so
         *  it is not said stored. It is here: the next begin says stored.
         */
        discard_session(gobj, channel, FALSE);
        return send_answer(gobj, channel_gobj, json_pack("{s:s, s:s, s:s}",
            "cmd", "refused",
            "batch", batch,
            "msg", "the batch cannot be made durable here"
        ));
    }

    gobj_write_integer_attr(gobj, "batches_stored",
        gobj_read_integer_attr(gobj, "batches_stored") + 1);
    gobj_write_integer_attr(gobj, "bytes_stored",
        gobj_read_integer_attr(gobj, "bytes_stored") + bytes);
    if(gobj_trace_level(gobj) & TRACE_BATCHES) {
        gobj_log_debug(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_INFO,
            "msg",          "%s", "Batch stored",
            "node",         "%s", json_string_value(json_object_get(jn_session, "node")),
            "batch",        "%s", batch,
            "bytes",        "%ld", (long)bytes,
            NULL
        );
    }
    discard_session(gobj, channel, FALSE);
    return send_answer(gobj, channel_gobj, json_pack("{s:s, s:s}",
        "cmd", "stored",
        "batch", batch
    ));
}




                    /***************************
                     *      Actions
                     ***************************/




/***************************************************************************
 *
 ***************************************************************************/
PRIVATE int ac_on_open(hgobj gobj, gobj_event_t event, json_t *kw, hgobj src)
{
    KW_DECREF(kw);
    return 0;
}

/***************************************************************************
 *  A sender went: its batch aside goes
 ***************************************************************************/
PRIVATE int ac_on_close(hgobj gobj, gobj_event_t event, json_t *kw, hgobj src)
{
    const char *channel = kw_get_str(gobj, kw, "__temp__`channel", "", 0);
    discard_session(gobj, channel, TRUE);

    KW_DECREF(kw);
    return 0;
}

/***************************************************************************
 *  A frame of a sender: a json header, and with data a NUL and the bytes
 ***************************************************************************/
PRIVATE int ac_on_message(hgobj gobj, gobj_event_t event, json_t *kw, hgobj src)
{
    gbuffer_t *gbuf = (gbuffer_t *)(size_t)kw_get_int(gobj, kw, "gbuffer", 0, 0);
    hgobj channel_gobj = (hgobj)(size_t)kw_get_int(gobj, kw, "__temp__`channel_gobj", 0, 0);
    const char *channel = kw_get_str(gobj, kw, "__temp__`channel", "", 0);
    if(!gbuf || !channel_gobj || empty_string(channel)) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_INTERNAL,
            "msg",          "%s", "Message without gbuffer or channel",
            NULL
        );
        KW_DECREF(kw)
        return -1;
    }

    const char *p = gbuffer_cur_rd_pointer(gbuf);
    size_t len = gbuffer_leftbytes(gbuf);
    const char *nul = memchr(p, 0, len);
    size_t header_len = nul? (size_t)(nul - p) : len;
    json_error_t error;
    json_t *jn_msg = json_loadb(p, header_len, 0, &error);
    if(!json_is_object(jn_msg)) {
        gobj_log_warning(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_PROTOCOL,
            "msg",          "%s", "Frame of an archive_sender whose header is not a json object: channel dropped",
            "channel",      "%s", channel,
            "peername",     "%s", gobj_read_str_attr(gobj_bottom_gobj(channel_gobj), "peername"),
            "error",        "%s", error.text,
            NULL
        );
        gobj_trace_dump(gobj, p, MIN(len, MAX_LOG_DUMP_SIZE), "frame of an archive_sender");
        JSON_DECREF(jn_msg)
        discard_session(gobj, channel, TRUE);
        gobj_send_event(channel_gobj, EV_DROP, 0, gobj);
        KW_DECREF(kw)
        return -1;
    }
    if(gobj_trace_level(gobj) & TRACE_FRAMES) {
        gobj_trace_json(gobj, jn_msg, "frame from an archive_sender (+%lu bytes)",
            (unsigned long)(nul? len - header_len - 1 : 0));
    }

    const char *cmd = kw_get_str(gobj, jn_msg, "cmd", "", 0);
    if(strcmp(cmd, "begin") == 0) {
        on_begin(gobj, channel_gobj, channel, jn_msg);
    } else if(strcmp(cmd, "data") == 0) {
        if(!nul) {
            refuse(gobj, channel_gobj, channel, kw_get_str(gobj, jn_msg, "batch", "", 0),
                "data without its bytes");
        } else {
            on_data(gobj, channel_gobj, channel, jn_msg, nul + 1, len - header_len - 1);
        }
    } else if(strcmp(cmd, "end") == 0) {
        on_end(gobj, channel_gobj, channel, jn_msg);
    } else {
        refuse(gobj, channel_gobj, channel, kw_get_str(gobj, jn_msg, "batch", "", 0),
            "cmd unknown");
    }

    JSON_DECREF(jn_msg)
    KW_DECREF(kw)
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
GOBJ_DEFINE_GCLASS(C_ARCHIVE_KEEPER);

/*------------------------*
 *      States
 *------------------------*/

/*------------------------*
 *      Events
 *------------------------*/

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
        {EV_ON_OPEN,                ac_on_open,             0},
        {EV_ON_CLOSE,               ac_on_close,            0},
        {EV_ON_MESSAGE,             ac_on_message,          0},
        {0,0,0}
    };

    states_t states[] = {
        {ST_IDLE, st_idle},
        {0, 0}
    };

    /*------------------------*
     *      Events
     *------------------------*/
    event_type_t event_types[] = {
        {EV_ON_OPEN,                0},
        {EV_ON_CLOSE,               0},
        {EV_ON_MESSAGE,             0},
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
PUBLIC int register_c_archive_keeper(void)
{
    return create_gclass(C_ARCHIVE_KEEPER);
}
