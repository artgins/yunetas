/***********************************************************************
 *          TR_QUEUE.C
 *
 *          Persistent queue based in TimeRanger
 *
 *          Copyright (c) 2019 Niyamaka.
 *          Copyright (c) 2024-2026, ArtGins.
 *          All Rights Reserved.
***********************************************************************/
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <fcntl.h>
#include <dirent.h>
#include <limits.h>
#include <sys/stat.h>

#include <kwid.h>
#include <helpers.h>
#include "tr_queue.h"

/***************************************************************
 *              Constants
 ***************************************************************/
/*
 *  What stopped the last open of the queue's topic (topic_blocked_by)
 */
#define QUEUE_TOPIC_BLOCKED_UNSEEN  0   // nothing the queue can see: tried when a file changes
#define QUEUE_TOPIC_BLOCKED_BY_DESC 1   // topic_desc.json is no json: tried when it changes
#define QUEUE_TOPIC_BLOCKED_BY_DISK 2   // a file or keys/ cannot be opened: tried when they can

/***************************************************************
 *              Structures
 ***************************************************************/

/***************************************************************
 *              Prototypes
 ***************************************************************/
PRIVATE void free_msg(void *msg_);

/***************************************************************
 *              Data
 ***************************************************************/

/***************************************************************************
    Open queue
 ***************************************************************************/
PUBLIC tr_queue_t *trq_open(
    json_t *tranger,
    const char *topic_name,
    const char *tkey,
    system_flag2_t system_flag, // KEY_TYPE_MASK2 forced to sf_rowid_key
    size_t backup_queue_size
) {
    hgobj gobj = (hgobj)json_integer_value(json_object_get(tranger, "gobj"));

    /*-------------------------------*
     *  Alloc memory
     *-------------------------------*/
    tr_queue_t *trq = GBMEM_MALLOC(sizeof(tr_queue_t));
    if(!trq) {
        gobj_log_error(gobj, LOG_OPT_TRACE_STACK,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_MEMORY,
            "msg",          "%s", "Cannot create tr_queue. GBMEM_MALLOC() FAILED",
            NULL
        );
        return 0;
    }
    trq->tranger = tranger;
    snprintf(trq->topic_name, sizeof(trq->topic_name), "%s", topic_name);
    trq->load_failed = FALSE;
    trq->backup_refused_said = FALSE;

    json_t *jn_topic_ext = json_object();
    json_object_set_new(jn_topic_ext, "filename_mask", json_string("queue"));

    system_flag &= ~KEY_TYPE_MASK2;
    system_flag |= sf_rowid_key;

    /*-------------------------------*
     *  Open/Create topic
     *-------------------------------*/
    trq->topic = tranger2_create_topic( // IDEMPOTENT function
        trq->tranger,
        topic_name,
        "",
        tkey,
        jn_topic_ext,
        system_flag,
        0,
        0
    );
    if(!trq->topic) {
        trq_close(trq);
        return 0;
    }
    dl_init(&trq->dl_q_msg, 0);

    if(backup_queue_size > 0 && kw_get_bool(gobj, trq->tranger, "master", 0, KW_REQUIRED)) {
        json_t *jn_topic_var = json_object();
        json_object_set_new(
            jn_topic_var,
            "backup_queue_size",
            json_integer((json_int_t)backup_queue_size)
        );
        tranger2_write_topic_var(
            trq->tranger,
            topic_name,
            jn_topic_var  // owned
        );
    }

    return trq;
}

/***************************************************************************
 *  The path of a file of the queue's topic, quietly
 ***************************************************************************/
PRIVATE BOOL queue_topic_file_path(char *path, size_t size, tr_queue_t *trq, const char *filename)
{
    char topic_dir[PATH_MAX];
    if(tranger2_topic_path(topic_dir, sizeof(topic_dir), trq->tranger, trq->topic_name) < 0) {
        return FALSE;   // Error already logged
    }
    if(!build_path(path, size, topic_dir, filename, NULL)) {
        return FALSE;   // Error already logged
    }
    return TRUE;
}

/***************************************************************************
 *  What a file of the queue's topic is on disk (inode, size, mtime, ctime),
 *  asked quietly. Zeroed when it is not there.
 ***************************************************************************/
PRIVATE void stat_queue_topic_file(tr_queue_t *trq, const char *filename, struct stat *st)
{
    char path[PATH_MAX];
    if(!queue_topic_file_path(path, sizeof(path), trq, filename) || stat(path, st) < 0) {
        memset(st, 0, sizeof(*st));
    }
}

PRIVATE BOOL same_file_stat(const struct stat *a, const struct stat *b)
{
    return (a->st_dev == b->st_dev &&
            a->st_ino == b->st_ino &&
            a->st_mode == b->st_mode &&
            a->st_size == b->st_size &&
            a->st_mtim.tv_sec == b->st_mtim.tv_sec &&
            a->st_mtim.tv_nsec == b->st_mtim.tv_nsec &&
            a->st_ctim.tv_sec == b->st_ctim.tv_sec &&
            a->st_ctim.tv_nsec == b->st_ctim.tv_nsec)? TRUE: FALSE;
}

/***************************************************************************
 *  Does the queue's topic_desc.json load? Asked quietly, the way the open
 *  reads it: -1 it cannot be opened (not there, no permission, no fd),
 *  0 it opens and is not json, 1 it loads.
 ***************************************************************************/
PRIVATE int queue_topic_desc_loads(tr_queue_t *trq)
{
    char path[PATH_MAX];
    if(!queue_topic_file_path(path, sizeof(path), trq, "topic_desc.json")) {
        return -1;
    }
    int fd = open(path, O_RDONLY|O_NOFOLLOW|O_CLOEXEC);
    if(fd < 0) {
        return -1;
    }
    json_error_t error;
    json_t *jn = json_loadfd(fd, 0, &error);
    close(fd);
    int loads = jn? 1: 0;
    JSON_DECREF(jn)
    return loads;
}

/***************************************************************************
 *  Can the queue's keys/ be listed? Asked quietly, the way the open lists
 *  it (find_keys_in_disk): a keys/ that is not there is nothing missing;
 *  one that cannot be opened or read, or holds an entry whose type cannot
 *  be asked (EIO; an EACCES is listed), fails the open.
 ***************************************************************************/
PRIVATE BOOL queue_topic_keys_listable(tr_queue_t *trq)
{
    char keys[PATH_MAX];
    if(!queue_topic_file_path(keys, sizeof(keys), trq, "keys")) {
        return FALSE;
    }
    DIR *dir = opendir(keys);
    if(!dir) {
        return (errno == ENOENT)? TRUE: FALSE;
    }
    BOOL listable = TRUE;
    struct dirent *entry;
    while((errno = 0, entry = readdir(dir)) != NULL) {
        #ifdef DT_DIR
        if(entry->d_type != DT_UNKNOWN) {
            continue;
        }
        #endif
        if(entry->d_name[0] == '.' &&
          (entry->d_name[1] == '\0' ||
           (entry->d_name[1] == '.' && entry->d_name[2] == '\0'))) {
            continue;
        }
        char path[PATH_MAX];
        struct stat st;
        if(!build_path(path, sizeof(path), keys, entry->d_name, NULL) ||
                (lstat(path, &st) < 0 && errno != ENOENT && errno != EACCES)) {
            listable = FALSE;
            break;
        }
    }
    if(listable && errno != 0) {
        listable = FALSE;
    }
    closedir(dir);
    return listable;
}

/***************************************************************************
 *  What stopped the last open of the topic, asked quietly once it failed.
 *  It decides what the queue asks before it tries the open again.
 ***************************************************************************/
PRIVATE int what_blocks_queue_topic(tr_queue_t *trq)
{
    int desc = queue_topic_desc_loads(trq);
    if(desc == 0) {
        return QUEUE_TOPIC_BLOCKED_BY_DESC;
    }
    if(desc < 0 || !queue_topic_keys_listable(trq)) {
        return QUEUE_TOPIC_BLOCKED_BY_DISK;
    }
    return QUEUE_TOPIC_BLOCKED_UNSEEN;
}

/***************************************************************************
 *  Is the topic worth opening again? Asked quietly, so a call that finds
 *  the cause still there logs nothing:
 *  - topic_desc.json loaded as no json: only once it changes. A readable
 *    file that does not load fails the same way every time, and each open
 *    logs its causes again.
 *  - a file or keys/ could not be opened or listed (permissions, EMFILE,
 *    EIO, a key whose type cannot be asked): once both can. Their cause
 *    goes away with the file untouched.
 *  - nothing the queue could see: once topic_desc.json or keys/ changes.
 ***************************************************************************/
PRIVATE BOOL queue_topic_worth_opening(tr_queue_t *trq)
{
    struct stat st;
    switch(trq->topic_blocked_by) {
        case QUEUE_TOPIC_BLOCKED_BY_DESC:
            if(queue_topic_desc_loads(trq) < 0) {
                return FALSE;
            }
            stat_queue_topic_file(trq, "topic_desc.json", &st);
            return !same_file_stat(&st, &trq->topic_desc_stat);

        case QUEUE_TOPIC_BLOCKED_BY_DISK:
            return (queue_topic_desc_loads(trq) >= 0 && queue_topic_keys_listable(trq))?
                TRUE: FALSE;

        case QUEUE_TOPIC_BLOCKED_UNSEEN:
        default:
            stat_queue_topic_file(trq, "topic_desc.json", &st);
            if(!same_file_stat(&st, &trq->topic_desc_stat)) {
                return TRUE;
            }
            stat_queue_topic_file(trq, "keys", &st);
            return !same_file_stat(&st, &trq->topic_keys_stat);
    }
}

/***************************************************************************
 *  The topic of the queue. A backup that failed, and could not open the
 *  topic again, left it NULL: it is taken again by name as soon as it can
 *  be opened (before this fix it stayed NULL for good: every read and ack of
 *  the queue failed, and the backup was never tried again). The topic the
 *  tranger already has open is taken as it is (an append opens it by name).
 *  NULL while it cannot, said once, and once more when it is taken again.
 *  In between the open is tried again only when what stopped it may be gone
 *  (queue_topic_worth_opening), asked quietly: through tranger2_topic() at
 *  every call, a topic that cannot be opened would log the errors of the
 *  open on every call -- the broker asks every second per session.
 *  What the files were is kept BEFORE each open, so a change during the
 *  open is not missed.
 ***************************************************************************/
PRIVATE json_t *take_queue_topic(tr_queue_t *trq)
{
    if(trq->topic) {
        return trq->topic;
    }

    hgobj gobj = (hgobj)json_integer_value(json_object_get(trq->tranger, "gobj"));
    json_t *topic = json_object_get(json_object_get(trq->tranger, "topics"), trq->topic_name);
    if(!topic) {
        if(trq->topic_missing_said && !queue_topic_worth_opening(trq)) {
            return NULL;    // said already
        }

        stat_queue_topic_file(trq, "topic_desc.json", &trq->topic_desc_stat);
        stat_queue_topic_file(trq, "keys", &trq->topic_keys_stat);
        topic = tranger2_topic(trq->tranger, trq->topic_name);  // logs the cause when it fails
        if(!topic) {
            trq->topic_blocked_by = what_blocks_queue_topic(trq);
            if(!trq->topic_missing_said) {
                trq->topic_missing_said = TRUE;
                gobj_log_error(gobj, 0,
                    "function",     "%s", __FUNCTION__,
                    "msgset",       "%s", MSGSET_TRANGER,
                    "msg",          "%s", "Queue without topic, it cannot be opened",
                    "topic_name",   "%s", trq->topic_name,
                    NULL
                );
            }
            return NULL;
        }
    }

    trq->topic = topic;
    trq->topic_missing_said = FALSE;
    gobj_log_info(gobj, 0,
        "function",     "%s", __FUNCTION__,
        "msgset",       "%s", MSGSET_TRANGER,
        "msg",          "%s", "Queue topic taken again",
        "topic_name",   "%s", trq->topic_name,
        NULL
    );
    return trq->topic;
}

/***************************************************************************
    Close queue (After close the queue remember tranger2_shutdown())
 ***************************************************************************/
PUBLIC void trq_close(tr_queue_t * trq)
{
    dl_flush(&((tr_queue_t *)trq)->dl_q_msg, free_msg);
    GBMEM_FREE(trq);
}

/***************************************************************************
    Set first rowid to search
 ***************************************************************************/
PRIVATE void trq_set_first_rowid(tr_queue_t * trq, uint64_t first_rowid)
{
    hgobj gobj = 0;

    trq->first_rowid = first_rowid;

    if(kw_get_bool(gobj, trq->tranger, "master", 0, KW_REQUIRED)) {
        json_t *jn_topic_var = json_object();
        json_object_set_new(
            jn_topic_var,
            "first_rowid",
            json_integer((json_int_t)first_rowid)
        );
        // first_rowid will be set in trq->topic too
        tranger2_write_topic_var(
            trq->tranger,
            trq->topic_name,
            jn_topic_var  // owned
        );
    }
}

/***************************************************************************
    New msg
 ***************************************************************************/
PRIVATE q_msg_t *new_msg(
    tr_queue_t *trq,
    json_int_t rowid, // global rowid that it must match the rowid in md_record
    const md2_record_ex_t *md_record,
    json_t *jn_record // owned
) {
    hgobj gobj = 0;

    /*
     *  Alloc memory
     */
    q_msg_t *msg = GBMEM_MALLOC(sizeof(q_msg_t));
    if(!msg) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_MEMORY,
            "msg",          "%s", "Cannot create msg. GBMEM_MALLOC() FAILED",
            NULL
        );
        json_decref(jn_record);
        return 0;
    }
    if(rowid != md_record->rowid) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_INTERNAL,
            "msg",          "%s", "rowid NOT MATCH",
            "rowid",        "%d", (int)rowid,
            "md_rowid",     "%d", (int)md_record->rowid,
            NULL
        );
    }
    memmove(&msg->md_record, md_record, sizeof(md2_record_ex_t));
    json_decref(jn_record);
    msg->trq = trq;
    msg->rowid = rowid;

    dl_add(&trq->dl_q_msg, msg);

    return msg;
}

/***************************************************************************
    Free msg
 ***************************************************************************/
PRIVATE void free_msg(void *msg_)
{
    q_msg_t *msg = msg_;
    memset(msg, 0, sizeof(q_msg_t));
    GBMEM_FREE(msg);
}

/***************************************************************************

 ***************************************************************************/
PRIVATE int load_record_callback(
    json_t *tranger,
    json_t *topic,
    const char *key,
    json_t *list, // iterator or rt_list/rt_disk id, don't own
    json_int_t rowid,   // global rowid of key
    md2_record_ex_t *md_record,
    json_t *jn_record // must be owned, can be null if only_md
) {
    hgobj gobj = (hgobj)json_integer_value(json_object_get(tranger, "gobj"));
    tr_queue_t *trq = (tr_queue_t *)(uintptr_t)kw_get_int(gobj, list, "trq", 0, KW_REQUIRED);

    if(trq->first_rowid==0) {
        // The first record called is the first pending record
        trq->first_rowid = rowid;
    }

    new_msg(trq, rowid, md_record, jn_record);

    return 0;
}

PUBLIC int trq_load(tr_queue_t * trq)
{
    hgobj gobj = 0;

    if(!trq) {
        gobj_log_error(gobj, LOG_OPT_TRACE_STACK,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_INTERNAL,
            "msg",          "%s", "trq NULL",
            NULL
        );
        return -1;
    }

    json_t *match_cond = json_object();
    json_object_set_new(
        match_cond,
        "user_flag_mask_set",
        json_integer(TRQ_MSG_PENDING)
    );

    json_object_set_new(match_cond, "only_md", json_true());

    uint64_t last_first_rowid = kw_get_int(
        gobj,
        take_queue_topic(trq),
        "first_rowid",  // get from topic_var (trq_set_first_rowid)
        0,
        0
    );
    if(last_first_rowid) {
        if(last_first_rowid <= tranger2_topic_size(trq->tranger, trq->topic_name)) {
            json_object_set_new(match_cond, "from_rowid", json_integer((json_int_t)last_first_rowid));
        }
    }

    /*
     *  We manage the callback, user not implied.
     *  Maintains a list of message's metadata.
     *  Load the message when need it.
     */
    json_object_set_new(
        match_cond,
        "load_record_callback",
        json_integer((json_int_t)(uintptr_t)load_record_callback)
    );

    trq->first_rowid = 0;

    json_t *jn_extra = json_pack("{s:s, s:I}",
        "topic_name", trq->topic_name,
        "trq", (json_int_t)(uintptr_t)trq
    );

    json_t *tr_list = tranger2_open_list(
        trq->tranger,
        trq->topic_name,
        match_cond, // owned
        jn_extra,   // owned
        NULL,       // rt_id
        FALSE,
        NULL        // creator
    );
    if(!tr_list) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_TREEDB,
            "msg",          "%s", "tranger2_open_list() failed",
            "topic_name",   "%s", trq->topic_name,
            NULL
        );
    }
    BOOL load_failed = (!tr_list || json_is_true(json_object_get(tr_list, "load_failed")))?
        TRUE: FALSE;
    tranger2_close_list(trq->tranger, tr_list);

    /*
     *  A load that did not read every pending message says nothing of where
     *  the first one is: first_rowid is not moved nor saved. Up to 7.25.4
     *  it was set to the size of the topic and saved, and the messages the
     *  load could not read were skipped for ever, even once the store was
     *  repaired. The next load starts where the last good one said.
     */
    if(load_failed) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_TRANGER,
            "msg",          "%s", "Queue loaded without some of its messages: its first_rowid is not moved nor saved",
            "topic_name",   "%s", trq->topic_name,
            "first_rowid",  "%ld", (long)last_first_rowid,
            NULL
        );
        trq->first_rowid = last_first_rowid;
        trq->load_failed = TRUE;
        return -1;
    }
    trq->load_failed = FALSE;
    trq->backup_refused_said = FALSE;

    if(trq->first_rowid==0) {
        // No pending msg, set the last rowid
        trq->first_rowid = tranger2_topic_size(trq->tranger, trq->topic_name);
    }
    if(trq->first_rowid) {
        trq_set_first_rowid(trq, trq->first_rowid);
    }

    return 0;
}

/***************************************************************************

 ***************************************************************************/
PUBLIC int trq_load_all(tr_queue_t * trq, int64_t from_rowid, int64_t to_rowid)
{
    json_t *match_cond = json_object();
    if(from_rowid) {
        json_object_set_new(match_cond, "from_rowid", json_integer(from_rowid));
    }
    if(to_rowid) {
        json_object_set_new(match_cond, "to_rowid", json_integer(to_rowid));
    }
    json_object_set_new(match_cond, "load_record_callback", json_integer((json_int_t)(uintptr_t)load_record_callback));

    json_t *jn_extra = json_pack("{s:s, s:I}",
        "topic_name", trq->topic_name,
        "trq", (json_int_t)(uintptr_t)trq
    );
    json_t *tr_list = tranger2_open_list(
        trq->tranger,
        trq->topic_name,
        match_cond, // owned
        jn_extra,   // owned
        NULL,       // rt_id
        FALSE,
        NULL        // creator
    );
    tranger2_close_list(trq->tranger, tr_list);

    return 0;
}

/***************************************************************************

 ***************************************************************************/
PUBLIC int trq_load_all_by_time(tr_queue_t * trq, int64_t from_t, int64_t to_t)
{
    json_t *match_cond = json_object();
    if(from_t) {
        json_object_set_new(match_cond, "from_t", json_integer(from_t));
    }
    if(to_t) {
        json_object_set_new(match_cond, "to_t", json_integer(to_t));
    }
    json_object_set_new(
        match_cond,
        "load_record_callback",
        json_integer((json_int_t)(uintptr_t)load_record_callback)
    );

    json_object_set_new(match_cond, "only_md", json_true());

    json_t *jn_extra = json_pack("{s:s, s:I}",
        "topic_name", trq->topic_name,
        "trq", (json_int_t)(uintptr_t)trq
    );
    json_t *tr_list = tranger2_open_list(
        trq->tranger,
        trq->topic_name,
        match_cond, // owned
        jn_extra,   // owned
        NULL,       // rt_id
        FALSE,
        NULL        // creator
    );
    tranger2_close_list(trq->tranger, tr_list);

    return 0;
}

/***************************************************************************
    Append a new message to queue forcing t

    If t (__t__) is 0 then the time will be set by TimeRanger with now time.

    The message (kw) is saved in disk with the user_flag TRQ_MSG_PENDING,
    leaving in q_msg_t only the metadata (to save memory).

    You can recover the message content with trq_msg_json().

    You must use trq_unload_msg() to mark a message as processed, removing from memory and
    resetting in disk the TRQ_MSG_PENDING user flag.
 ***************************************************************************/
PUBLIC q_msg_t *trq_append2(
    tr_queue_t * trq,
    json_int_t t,   // __t__ if 0 then the time will be set by TimeRanger with now time
    json_t *kw,     // owned
    uint16_t user_flag  // extra flags in addition to TRQ_MSG_PENDING
) {
    hgobj gobj = 0;

    if(!kw || kw->refcount <= 0) {
        gobj_log_error(gobj, LOG_OPT_TRACE_STACK,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_INTERNAL,
            "msg",          "%s", "kw NULL",
            "topic",        "%s", trq->topic_name,
            NULL
        );
        return 0;
    }

    md2_record_ex_t md_record;
    if(tranger2_append_record(
        trq->tranger,
        trq->topic_name,
        t,                              // __t__
        user_flag | TRQ_MSG_PENDING,    // __flag__
        &md_record,
        json_incref(kw) // owned
    )<0) {
        // Error already logged; md_record is not filled, do not enqueue garbage
        JSON_DECREF(kw)
        return 0;
    }

    q_msg_t *msg = new_msg(
        trq,
        (json_int_t)md_record.rowid,
        &md_record,
        kw  // owned
    );

    return msg;
}

/***************************************************************************
    Get a message from iter by his rowid
 ***************************************************************************/
PUBLIC q_msg_t *trq_get_by_rowid(tr_queue_t * trq, uint64_t rowid)
{
    register q_msg_t *msg;

    qmsg_foreach_forward(trq, msg) {
        if(msg->rowid == rowid) {
            return msg;
        }
    }

    return 0;
}

/***************************************************************************
    Check pending status of a rowid (low level)
 ***************************************************************************/
PUBLIC int trq_check_pending_rowid(
    tr_queue_t * trq,
    uint64_t __t__,
    uint64_t rowid
) {
    uint16_t __user_flag__ = tranger2_read_user_flag(
        trq->tranger,
        trq->topic_name,
        "",
        __t__,
        rowid
    );

    if(__user_flag__ & TRQ_MSG_PENDING) {
        return 1;
    } else {
        return 0;
    }
}

/***************************************************************************
    Unload a message successfully from iter (TRQ_MSG_PENDING set to 0)
 ***************************************************************************/
PUBLIC void trq_unload_msg(q_msg_t *msg, int32_t result)
{
    trq_set_hard_flag(msg, TRQ_MSG_PENDING, 0);

    dl_delete(&msg->trq->dl_q_msg, msg, free_msg);
}

/***************************************************************************
    Put a hard flag in a message.
    You must flag a message after append it if you want recover it in the next open.
 ***************************************************************************/
PUBLIC int trq_set_hard_flag(q_msg_t *msg, uint16_t hard_mark, BOOL set)
{
    if(!take_queue_topic(msg->trq)) {
        return -1;  // Error already logged
    }
    return tranger2_set_user_flag(
        msg->trq->tranger,
        msg->trq->topic_name,
        "",
        msg->md_record.__t__,
        msg->md_record.rowid,
        hard_mark,
        set
    );
}

/***************************************************************************
    Set soft mark
 ***************************************************************************/
PUBLIC uint64_t trq_set_soft_mark(q_msg_t *msg, uint64_t soft_mark, BOOL set)
{
    if(set) {
        /*
         *  Set
         */
        ((q_msg_t *)msg)->mark |= soft_mark;
    } else {
        /*
         *  Reset
         */
        ((q_msg_t *)msg)->mark &= ~soft_mark;
    }

    return ((q_msg_t *)msg)->mark;
}

/***************************************************************************
    Get info of message
 ***************************************************************************/
PUBLIC json_t *trq_msg_json(q_msg_t *msg) // Load the message, Return json is YOURS!!
{
    json_t *topic = take_queue_topic(msg->trq);
    if(!topic) {
        return NULL;    // Error already logged
    }
    json_t *jn_record = tranger2_read_record_content( // return is yours
        msg->trq->tranger,
        topic,
        "",
        &msg->md_record
    );
    if(!jn_record) {
        hgobj gobj = (hgobj)json_integer_value(json_object_get(msg->trq->tranger, "gobj"));
        gobj_log_error(gobj, LOG_OPT_TRACE_STACK,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_INTERNAL,
            "msg",          "%s", "jn_msg NULL",
            "topic",        "%s", msg->trq->topic_name,
            NULL
        );
    }
    return jn_record;
}


//PUBLIC BOOL trq_msg_is_t_ms(q_msg_t *msg)
//{
//    return (((q_msg_t *)msg)->md_record.__system_flag__ & sf_t_ms)?true:FALSE;
//}
//PUBLIC BOOL trq_msg_is_tm_ms(q_msg_t *msg)
//{
//    return (((q_msg_t *)msg)->md_record.__system_flag__ & sf_tm_ms)?true:FALSE;
//}


/***************************************************************************
    Metadata
 ***************************************************************************/
PUBLIC int trq_set_metadata(
    json_t *kw,
    const char *key,
    json_t *jn_value // owned
) {
    hgobj gobj = 0;
    return kw_set_subdict_value(
        gobj,
        kw,
        __MD_TRQ__,
        key,
        jn_value // owned
    );
}

PUBLIC json_t *trq_get_metadata( // WARNING The return json is not yours!
    json_t *kw
) {
    hgobj gobj = 0;
    return kw_get_dict(gobj,
        kw,
        __MD_TRQ__,
        0,
        KW_REQUIRED
    );
}

/***************************************************************************
 *  Return a new kw only with the message metadata
 ***************************************************************************/
PUBLIC json_t *trq_answer(
    json_t *jn_message,  // not owned, Gps message, to get only __MD_TRQ__
    int result
) {
    hgobj gobj = 0;

    json_t *kw_response = json_object();
    json_t *__md__ = trq_get_metadata(jn_message);
    if(__md__) {
        json_object_set(kw_response, __MD_TRQ__, __md__);
        trq_set_metadata(kw_response, "result", json_integer(result));
    } else {
        gobj_log_error(gobj, LOG_OPT_TRACE_STACK,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_INTERNAL,
            "msg",          "%s", "queue message without metadata",
            NULL
        );
        gobj_trace_json(gobj, jn_message, "queue message without metadata");
    }

    return kw_response;
}

/***************************************************************************
 *  Do backup
 ***************************************************************************/
PUBLIC int trq_check_backup(tr_queue_t * trq)
{
    hgobj gobj = (hgobj)json_integer_value(json_object_get(trq->tranger, "gobj"));
    json_t *topic_ = take_queue_topic(trq);
    if(!topic_) {
        return -1;  // Error already logged
    }
    uint64_t backup_queue_size = kw_get_int(gobj, topic_, "backup_queue_size", 0, 0);

    if(backup_queue_size) {
        uint64_t sz = tranger2_topic_size(trq->tranger, trq->topic_name);
        if(sz >= backup_queue_size && trq->load_failed) {
            /*
             *  The queue is empty because its load failed, not because its
             *  messages were processed: a backup would re-create the topic
             *  empty and reset first_rowid, and take for good the pending
             *  messages that first_rowid still points to. Said once: the
             *  caller asks every period.
             */
            if(!trq->backup_refused_said) {
                trq->backup_refused_said = TRUE;
                gobj_log_error(gobj, 0,
                    "function",     "%s", __FUNCTION__,
                    "msgset",       "%s", MSGSET_TRANGER,
                    "msg",          "%s", "Queue backup refused: its last load did not read every pending message",
                    "topic_name",   "%s", trq->topic_name,
                    "topic_size",   "%ld", (long)sz,
                    "backup_queue_size", "%ld", (long)backup_queue_size,
                    NULL
                );
            }
            return -1;
        }
        if(sz >= backup_queue_size) {
            trq_set_first_rowid(trq, sz);
            json_t *topic = tranger2_backup_topic(
                trq->tranger,
                trq->topic_name,
                0,
                0,
                TRUE,
                0
            );
            if(!topic) {
                /*
                 *  The topic is still the queue's, not backed up: the
                 *  backup opens it again (tranger2_backup_topic). Up to
                 *  7.25.4 the queue was left with no topic and this
                 *  answered 0: every read and every ack failed after it.
                 */
                trq->topic = tranger2_topic(trq->tranger, trq->topic_name);
                gobj_log_error(gobj, 0,
                    "function",     "%s", __FUNCTION__,
                    "msgset",       "%s", MSGSET_TRANGER,
                    "msg",          "%s", trq->topic?
                        "Queue backup failed: the queue goes on in its topic, not backed up":
                        "Queue backup failed, and the queue has no topic",
                    "topic_name",   "%s", trq->topic_name,
                    NULL
                );
                return -1;
            }
            trq->topic = topic;
            trq_set_first_rowid(trq, 0);
        }
    }

    return 0;
}
