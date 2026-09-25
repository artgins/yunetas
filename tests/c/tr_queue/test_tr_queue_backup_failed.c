/****************************************************************************
 *          test_tr_queue_backup_failed.c
 *
 *  A queue backup that FAILS leaves the queue working in its topic.
 *
 *  trq_check_backup() (tr_queue) and tr2q_check_backup() (the mqtt queues)
 *  took the return of tranger2_backup_topic() as the queue's topic without
 *  looking at it. The backup closes the topic first, so when it failed
 *  after that (here: the rename, the backup name is taken by a FILE) the
 *  queue was left with no topic and the call answered 0: every read logged
 *  "What topic?", every ack answered -1, and no backup ever happened again.
 *  Now the backup opens the topic again, the queue takes it, and the call
 *  answers -1.
 *
 *  tranger2_backup_topic() itself also leaked the topic_var it had loaded
 *  when the rename failed: the memory check at the end catches it.
 *
 *  And a backup that fails AFTER the rename: the new topic cannot be
 *  created (here: the mkdir of its directory fails with ENOSPC, in
 *  __wrap_mkdir() below). The backup is moved back, and the queue goes on
 *  in its topic, whole; the next call backs it up. Up to this fix the data stayed
 *  in the backup, nothing was opened (the queue had no topic until a
 *  restart) and, its size read as 0, the backup was never tried again.
 *
 *  And a create that fails only at the mkdir of the topic's keys/ (all
 *  the rest written): tranger2_create_topic() answers NULL and leaves
 *  nothing, on disk or in memory, and a backup that meets it moves the
 *  queue's topic back. Before this fix the create logged the failure and
 *  answered a topic with no keys/ -- and a backup took that half topic as
 *  the queue's new one.
 *
 *  And a backup that fails and cannot open the topic again
 *  either (its topic_desc.json unreadable for a moment, mode 0): the queue
 *  has no topic. Once the file can be read again, the queue takes its topic
 *  again by name, at the next trq_check_backup() / tr2q_check_backup() or
 *  at the next read or ack of a message. Before this fix the topic stayed NULL
 *  for good: every read answered NULL, every ack -1, and the check answered
 *  0 and never backed up again. Skipped as root (mode 0 does not stop it).
 *
 *  And a tranger opened with on_critical_error LOG_OPT_EXIT_ZERO (the MQTT
 *  broker's queues): a backup whose new topic cannot be created moves the
 *  backup back and the queue goes on. Before this fix the CRITICAL of the
 *  failed mkdir called exit(0) before the put-back, and the data stayed in
 *  the backup (an atexit() handler below turns that exit into a failure).
 *
 *  And a plain create with LOG_OPT_EXIT_ZERO whose keys/ cannot be made:
 *  the process exits, as it is told to, but only once what was made is
 *  removed (the create runs in a child; the parent looks at the disk). Up to
 *  this fix it exited in the log of the failed mkdir, and the next start
 *  opened the half topic.
 *
 *  And the CRITICAL "Cannot create TimeRanger subdir. mkrdir() FAILED"
 *  names the cause (ENOSPC): up to this fix the log of mkrdir() before it
 *  changed errno, and it said "Success".
 *
 *  And while the queue's topic cannot be opened, only the FIRST call says
 *  so: the next ones ask the disk quietly (up to this fix each one logged
 *  the three errors of the open again).
 *
 *  And the same when topic_desc.json CAN be read but does not load (broken
 *  json): "readable" is not "changed", so the queue keeps what the file was
 *  when the open failed and tries again only when it changes. Up to this
 *  fix every call opened it again and logged three errors.
 *
 *          Copyright (c) 2026, ArtGins.
 *          All Rights Reserved.
 ****************************************************************************/
#include <string.h>
#include <errno.h>
#include <sys/stat.h>
#include <limits.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/wait.h>

#include <gobj.h>
#include <kwid.h>
#include <timeranger2.h>
#include <tr_queue.h>
#include <tr2q_mqtt.h>
#include <helpers.h>
#include <yev_loop.h>
#include <testing.h>

#define APP         "test_tr_queue_backup_failed"
#define DATABASE    "tr_queue_backup_failed"

#define MSG_MOVING      "Backup timeranger topic, moving"
#define MSG_RENAME      "cannot backup topic"
#define MSG_REOPENED    "Backup of topic failed: the topic is opened again as it was, not backed up"
#define MSG_QUEUE       "Queue backup failed: the queue goes on in its topic, not backed up"
#define MSG_ABANDON     "Cannot create topic: it is not whole, what was made is removed"

/***************************************************************
 *              A mkdir() that fails (no space)
 ***************************************************************/
int __real_mkdir(const char *path, mode_t mode);
int __wrap_mkdir(const char *path, mode_t mode);

PRIVATE char failing_mkdir[PATH_MAX] = "";

int __wrap_mkdir(const char *path, mode_t mode)
{
    if(failing_mkdir[0] && strcmp(path, failing_mkdir) == 0) {
        errno = ENOSPC;
        return -1;
    }
    return __real_mkdir(path, mode);
}

/***************************************************************
 *              Data
 ***************************************************************/
PRIVATE yev_loop_h yev_loop;
PRIVATE char path_root[PATH_MAX];
PRIVATE char path_database[PATH_MAX];

/***************************************************************
 *              Helpers
 ***************************************************************/
PRIVATE json_t *startup(void)
{
    return tranger2_startup(0, json_pack("{s:s, s:s, s:b, s:i}",
        "path", path_root,
        "database", DATABASE,
        "master", 1,
        "on_critical_error", LOG_OPT_TRACE_STACK
    ), 0);
}

/*
 *  The backup's name taken by a regular file: the rename fails (ENOTDIR)
 */
PRIVATE void take_backup_name(const char *topic_name)
{
    char name[NAME_MAX];
    snprintf(name, sizeof(name), "%s.bak", topic_name);
    char path[PATH_MAX];
    build_path(path, sizeof(path), path_database, name, NULL);
    int fd = open(path, O_CREAT|O_WRONLY, 0660);
    if(fd < 0) {
        printf("%sERROR%s --> cannot create %s\n", On_Red BWhite, Color_Off, path);
        return;
    }
    close(fd);
}

PRIVATE int expect_int(const char *what, json_int_t found, json_int_t expected)
{
    if(found != expected) {
        printf("%sERROR%s --> %s: %d, expected %d\n",
            On_Red BWhite, Color_Off, what, (int)found, (int)expected);
        return -1;
    }
    return 0;
}

/*
 *  A message of the mqtt queues: its payload travels as a gbuffer
 */
PRIVATE json_t *tr2q_kw(int mid, json_int_t tm)
{
    gbuffer_t *gbuf = gbuffer_create(16, 16);
    gbuffer_append_string(gbuf, "payload");
    return json_pack("{s:i, s:I, s:I}",
        "mid", mid,
        "tm", tm,
        "gbuffer", (json_int_t)(uintptr_t)gbuf
    );
}

PRIVATE json_t *expected_backup_failure(void)
{
    return json_pack("[{s:s},{s:s},{s:s},{s:s}]",
        "msg", MSG_MOVING,
        "msg", MSG_RENAME,
        "msg", MSG_REOPENED,
        "msg", MSG_QUEUE
    );
}

/***************************************************************************
 *  tr_queue
 ***************************************************************************/
PRIVATE int test_trq(void)
{
    int result = 0;
    const char *topic_name = "trq";
    rmrdir(path_database);

    set_expected_results("trq: setup", NULL, NULL, NULL, 0);
    json_t *tranger = startup();
    tr_queue_t *trq = trq_open(tranger, topic_name, "tm", 0, 1 /* backup_queue_size */);
    trq_load(trq);
    q_msg_t *msg = trq_append2(trq, 946684801, json_pack("{s:i, s:I}", "n", 1, "tm", (json_int_t)946684801), 0);
    trq_unload_msg(msg, 0);
    take_backup_name(topic_name);
    test_json(NULL);    // the setup logs are not what is tested

    set_expected_results("trq: the backup fails", expected_backup_failure(), NULL, NULL, 1);
    int ret = trq_check_backup(trq);
    result += expect_int("trq: trq_check_backup() of a failed backup", ret, -1);
    if(!trq->topic || trq->topic != tranger2_topic(tranger, topic_name)) {
        printf("%sERROR%s --> trq: the queue has no topic after the failed backup\n",
            On_Red BWhite, Color_Off);
        result += -1;
    }
    result += expect_int("trq: topic size after the failed backup",
        (json_int_t)tranger2_topic_size(tranger, topic_name), 1);
    result += test_json(NULL);

    set_expected_results("trq: the queue works after the failed backup", NULL, NULL, NULL, 1);
    if(trq->topic) {
        msg = trq_append2(trq, 946684802, json_pack("{s:i, s:I}", "n", 2, "tm", (json_int_t)946684802), 0);
        if(!msg) {
            printf("%sERROR%s --> trq: append after the failed backup\n", On_Red BWhite, Color_Off);
            result += -1;
        } else {
            json_t *jn = trq_msg_json(msg);
            if(!jn) {
                printf("%sERROR%s --> trq: the message cannot be read\n", On_Red BWhite, Color_Off);
                result += -1;
            }
            JSON_DECREF(jn)
            result += expect_int("trq: ack after the failed backup",
                trq_set_hard_flag(msg, TRQ_MSG_PENDING, 0), 0);
        }
        result += expect_int("trq: topic size", (json_int_t)tranger2_topic_size(tranger, topic_name), 2);
    }
    trq_close(trq);
    tranger2_shutdown(tranger);
    result += test_json(NULL);

    return result;
}

/***************************************************************************
 *  tr2q (the mqtt queues)
 ***************************************************************************/
PRIVATE int test_tr2q(void)
{
    int result = 0;
    const char *topic_name = "tr2q";
    rmrdir(path_database);

    set_expected_results("tr2q: setup", NULL, NULL, NULL, 0);
    json_t *tranger = startup();
    tr2_queue_t *trq = tr2q_open(tranger, topic_name, "tm", 0, 10, 1 /* backup_queue_size */);
    tr2q_load(trq);
    tr2q_append(trq, 946684801, tr2q_kw(1, 946684801), 0);
    take_backup_name(topic_name);
    test_json(NULL);    // the setup logs are not what is tested

    set_expected_results("tr2q: the backup fails", expected_backup_failure(), NULL, NULL, 1);
    int ret = tr2q_check_backup(trq);
    result += expect_int("tr2q: tr2q_check_backup() of a failed backup", ret, -1);
    if(!trq->topic || trq->topic != tranger2_topic(tranger, topic_name)) {
        printf("%sERROR%s --> tr2q: the queue has no topic after the failed backup\n",
            On_Red BWhite, Color_Off);
        result += -1;
    }
    result += expect_int("tr2q: topic size after the failed backup",
        (json_int_t)tranger2_topic_size(tranger, topic_name), 1);
    result += test_json(NULL);

    set_expected_results("tr2q: the queue works after the failed backup", NULL, NULL, NULL, 1);
    if(trq->topic) {
        q2_msg_t *msg = tr2q_append(trq, 946684802, tr2q_kw(2, 946684802), 0);
        if(!msg) {
            printf("%sERROR%s --> tr2q: append after the failed backup\n", On_Red BWhite, Color_Off);
            result += -1;
        }
        result += expect_int("tr2q: topic size", (json_int_t)tranger2_topic_size(tranger, topic_name), 2);
    }
    tr2q_close(trq);
    tranger2_shutdown(tranger);
    result += test_json(NULL);

    return result;
}

/***************************************************************************
 *  tr_queue: the backup is moved, and the new topic cannot be created
 ***************************************************************************/
PRIVATE int test_trq_create_fails(void)
{
    int result = 0;
    const char *topic_name = "trq_create";
    rmrdir(path_database);

    set_expected_results("trq_create: setup", NULL, NULL, NULL, 0);
    json_t *tranger = startup();
    tr_queue_t *trq = trq_open(tranger, topic_name, "tm", 0, 1 /* backup_queue_size */);
    trq_load(trq);
    q_msg_t *msg = trq_append2(trq, 946684801, json_pack("{s:i, s:I}", "n", 1, "tm", (json_int_t)946684801), 0);
    trq_unload_msg(msg, 0);
    test_json(NULL);    // the setup logs are not what is tested

    char topic_dir[PATH_MAX];
    char backup_dir[PATH_MAX];
    char bak_name[NAME_MAX];
    snprintf(bak_name, sizeof(bak_name), "%s.bak", topic_name);
    build_path(topic_dir, sizeof(topic_dir), path_database, topic_name, NULL);
    build_path(backup_dir, sizeof(backup_dir), path_database, bak_name, NULL);
    snprintf(failing_mkdir, sizeof(failing_mkdir), "%s", topic_dir);

    set_expected_results(
        "trq_create: the new topic cannot be created",
        json_pack("[{s:s},{s:s},{s:s},{s:s},{s:s},{s:s}]",
            "msg", MSG_MOVING,
            "msg", "newdir() FAILED",   // the topic directory, ENOSPC
            "msg", "Cannot create TimeRanger subdir. mkrdir() FAILED",
            "msg", MSG_ABANDON,         // the create stops there, nothing half made
            "msg", MSG_REOPENED,        // moved back, and opened again
            "msg", MSG_QUEUE
        ),
        NULL, NULL, 1
    );
    int ret = trq_check_backup(trq);
    failing_mkdir[0] = 0;
    result += expect_int("trq_create: trq_check_backup() of a failed create", ret, -1);
    if(!trq->topic || trq->topic != tranger2_topic(tranger, topic_name)) {
        printf("%sERROR%s --> trq_create: the queue has no topic after the failed create\n",
            On_Red BWhite, Color_Off);
        result += -1;
    }
    result += expect_int("trq_create: topic size after the failed create",
        (json_int_t)tranger2_topic_size(tranger, topic_name), 1);
    result += expect_int("trq_create: the backup was moved back",
        (json_int_t)is_directory(backup_dir), 0);
    result += test_json(NULL);

    /*
     *  The next call tries again, and backs the queue up
     */
    set_expected_results(
        "trq_create: the next call backs up",
        json_pack("[{s:s},{s:s}]",
            "msg", MSG_MOVING,
            "msg", "Creating topic"
        ),
        NULL, NULL, 1
    );
    ret = trq_check_backup(trq);
    result += expect_int("trq_create: trq_check_backup() once it can be done", ret, 0);
    result += expect_int("trq_create: the backup is there", (json_int_t)is_directory(backup_dir), 1);
    result += expect_int("trq_create: the new topic is empty",
        (json_int_t)tranger2_topic_size(tranger, topic_name), 0);
    trq_close(trq);
    tranger2_shutdown(tranger);
    result += test_json(NULL);

    return result;
}

/***************************************************************************
 *  A create whose keys/ cannot be made (only that mkdir fails)
 ***************************************************************************/
PRIVATE int test_create_keys_fails(void)
{
    int result = 0;
    rmrdir(path_database);

    set_expected_results("keys: setup", NULL, NULL, NULL, 0);
    json_t *tranger = startup();
    test_json(NULL);    // the setup logs are not what is tested

    /*
     *  1. The create itself
     */
    const char *direct = "t_keys";
    char topic_dir[PATH_MAX];
    build_path(topic_dir, sizeof(topic_dir), path_database, direct, NULL);
    build_path(failing_mkdir, sizeof(failing_mkdir), topic_dir, "keys", NULL);
    set_expected_results(
        "keys: a create whose keys/ cannot be made",
        json_pack("[{s:s},{s:s, s:s},{s:s, s:s},{s:s}]",
            "msg", "Creating topic",
            "msg", "newdir() FAILED", "serrno", "No space left on device",
            /*
             *  The cause, not "Success": the log of mkrdir() in between
             *  changed errno (up to this fix every log did)
             */
            "msg", "Cannot create TimeRanger subdir. mkrdir() FAILED", "serrno", "No space left on device",
            "msg", MSG_ABANDON
        ),
        NULL, NULL, 1
    );
    json_t *topic = tranger2_create_topic(tranger, direct, "id", "tm", NULL, sf_string_key,
        json_pack("{s:s, s:I}", "id", "", "tm", (json_int_t)0), 0);
    failing_mkdir[0] = 0;
    result += expect_int("keys: tranger2_create_topic() answers NULL", topic? 1: 0, 0);
    result += expect_int("keys: nothing of the topic on disk", (json_int_t)is_directory(topic_dir), 0);
    result += expect_int("keys: nothing of the topic in memory",
        json_object_get(json_object_get(tranger, "topics"), direct)? 1: 0, 0);
    result += test_json(NULL);

    set_expected_results("keys: the next create makes it whole",
        json_pack("[{s:s}]", "msg", "Creating topic"), NULL, NULL, 1);
    topic = tranger2_create_topic(tranger, direct, "id", "tm", NULL, sf_string_key,
        json_pack("{s:s, s:I}", "id", "", "tm", (json_int_t)0), 0);
    char keys_dir[PATH_MAX];
    build_path(keys_dir, sizeof(keys_dir), topic_dir, "keys", NULL);
    result += expect_int("keys: the next create answers the topic", topic? 1: 0, 1);
    result += expect_int("keys: with its keys/", (json_int_t)is_directory(keys_dir), 1);
    result += test_json(NULL);

    /*
     *  2. A queue backup that meets it: the queue keeps its topic
     */
    const char *topic_name = "trq_keys";
    set_expected_results("keys: queue setup", NULL, NULL, NULL, 0);
    tr_queue_t *trq = trq_open(tranger, topic_name, "tm", 0, 1 /* backup_queue_size */);
    trq_load(trq);
    q_msg_t *msg = trq_append2(trq, 946684801, json_pack("{s:i, s:I}", "n", 1, "tm", (json_int_t)946684801), 0);
    trq_unload_msg(msg, 0);
    test_json(NULL);

    char bak_name[NAME_MAX];
    char backup_dir[PATH_MAX];
    snprintf(bak_name, sizeof(bak_name), "%s.bak", topic_name);
    build_path(backup_dir, sizeof(backup_dir), path_database, bak_name, NULL);
    build_path(topic_dir, sizeof(topic_dir), path_database, topic_name, NULL);
    build_path(failing_mkdir, sizeof(failing_mkdir), topic_dir, "keys", NULL);
    set_expected_results(
        "keys: a queue backup whose new topic has no keys/",
        json_pack("[{s:s},{s:s},{s:s},{s:s},{s:s},{s:s},{s:s}]",
            "msg", MSG_MOVING,
            "msg", "Creating topic",
            "msg", "newdir() FAILED",
            "msg", "Cannot create TimeRanger subdir. mkrdir() FAILED",
            "msg", MSG_ABANDON,
            "msg", MSG_REOPENED,
            "msg", MSG_QUEUE
        ),
        NULL, NULL, 1
    );
    int ret = trq_check_backup(trq);
    failing_mkdir[0] = 0;
    result += expect_int("keys: trq_check_backup() answers -1", ret, -1);
    if(!trq->topic || trq->topic != tranger2_topic(tranger, topic_name)) {
        printf("%sERROR%s --> keys: the queue has no topic after the failed backup\n",
            On_Red BWhite, Color_Off);
        result += -1;
    }
    result += expect_int("keys: the queue keeps its message",
        (json_int_t)tranger2_topic_size(tranger, topic_name), 1);
    result += expect_int("keys: the backup was moved back", (json_int_t)is_directory(backup_dir), 0);
    result += test_json(NULL);

    set_expected_results("keys: shutdown", NULL, NULL, NULL, 1);
    trq_close(trq);
    tranger2_shutdown(tranger);
    result += test_json(NULL);

    return result;
}

/***************************************************************************
 *  The queue loses its topic (a backup that fails, and a topic that cannot
 *  be opened again), and takes it again once it can be opened
 ***************************************************************************/
PRIVATE void make_topic_unreadable(const char *topic_name, BOOL unreadable)
{
    char topic_dir[PATH_MAX];
    char topic_desc[PATH_MAX];
    build_path(topic_dir, sizeof(topic_dir), path_database, topic_name, NULL);
    build_path(topic_desc, sizeof(topic_desc), topic_dir, "topic_desc.json", NULL);
    chmod(topic_desc, unreadable? 0: 0660);
}

#define MSG_JSON_FILE   "Cannot open a json file"
#define MSG_DESC        "Cannot open topic: topic_desc.json does not load"
#define MSG_JSON_BROKEN "Cannot load json file, bad json"

PRIVATE json_t *expected_topic_lost(void)
{
    return json_pack("[{s:s},{s:s},{s:s},{s:s},{s:s},{s:s},{s:s},{s:s},{s:s}]",
        "msg", MSG_JSON_FILE,           // the backup loads topic_desc.json
        "msg", "Cannot load topic_desc.json",
        "msg", MSG_JSON_FILE,           // and cannot open the topic again
        "msg", MSG_DESC,
        "msg", "Backup of topic failed, and the topic cannot be opened again",
        "msg", MSG_JSON_FILE,           // nor can the queue
        "msg", MSG_DESC,
        "msg", "Cannot open topic",
        "msg", "Queue backup failed, and the queue has no topic"
    );
}

/*
 *  The first call that finds no topic says it, with the causes the open
 *  logs. The next ones say NOTHING while the topic still cannot be opened:
 *  they ask the disk quietly first. (Up to this fix every call logged the
 *  three errors of the open again; the broker calls tr2q_check_backup()
 *  every second per session.)
 */
PRIVATE json_t *expected_topic_not_opened(BOOL first)
{
    if(first) {
        return json_pack("[{s:s},{s:s},{s:s},{s:s}]",
            "msg", MSG_JSON_FILE,
            "msg", MSG_DESC,
            "msg", "Cannot open topic",
            "msg", "Queue without topic, it cannot be opened"
        );
    }
    return NULL;
}

PRIVATE int test_trq_topic_taken_again(void)
{
    int result = 0;
    const char *topic_name = "trq_retake";
    rmrdir(path_database);

    set_expected_results("trq_retake: setup", NULL, NULL, NULL, 0);
    json_t *tranger = startup();
    tr_queue_t *trq = trq_open(tranger, topic_name, "tm", 0, 1 /* backup_queue_size */);
    trq_load(trq);
    q_msg_t *msg = trq_append2(trq, 946684801, json_pack("{s:i, s:I}", "n", 1, "tm", (json_int_t)946684801), 0);
    trq_unload_msg(msg, 0);
    msg = trq_append2(trq, 946684802, json_pack("{s:i, s:I}", "n", 2, "tm", (json_int_t)946684802), 0);
    test_json(NULL);    // the setup logs are not what is tested

    /*
     *  1. The backup fails, and the topic cannot be opened again
     */
    make_topic_unreadable(topic_name, TRUE);
    set_expected_results("trq_retake: the queue loses its topic", expected_topic_lost(), NULL, NULL, 1);
    int ret = trq_check_backup(trq);
    result += expect_int("trq_retake: trq_check_backup() of a failed backup", ret, -1);
    result += expect_int("trq_retake: the queue has no topic", trq->topic? 1: 0, 0);
    result += test_json(NULL);

    /*
     *  2. While it cannot be opened: a read and an ack fail, said once
     */
    set_expected_results("trq_retake: a read without topic", expected_topic_not_opened(TRUE), NULL, NULL, 1);
    json_t *jn = trq_msg_json(msg);
    result += expect_int("trq_retake: a read without topic answers NULL", jn? 1: 0, 0);
    JSON_DECREF(jn)
    result += test_json(NULL);

    set_expected_results("trq_retake: an ack without topic says nothing more",
        expected_topic_not_opened(FALSE), NULL, NULL, 1);
    result += expect_int("trq_retake: an ack without topic answers -1",
        trq_set_hard_flag(msg, TRQ_MSG_PENDING, 0), -1);
    result += expect_int("trq_retake: a check without topic answers -1", trq_check_backup(trq), -1);
    result += test_json(NULL);

    /*
     *  3. It can be opened again: the read takes it again, and the ack works
     */
    make_topic_unreadable(topic_name, FALSE);
    set_expected_results("trq_retake: the read takes the topic again",
        json_pack("[{s:s}]", "msg", "Queue topic taken again"), NULL, NULL, 1);
    jn = trq_msg_json(msg);
    result += expect_int("trq_retake: the read answers the message", jn? 1: 0, 1);
    JSON_DECREF(jn)
    if(!trq->topic || trq->topic != tranger2_topic(tranger, topic_name)) {
        printf("%sERROR%s --> trq_retake: the queue did not take its topic again\n",
            On_Red BWhite, Color_Off);
        result += -1;
    }
    result += expect_int("trq_retake: the ack works", trq_set_hard_flag(msg, TRQ_MSG_PENDING, 0), 0);
    trq_unload_msg(msg, 0);
    result += test_json(NULL);

    /*
     *  4. And the next check backs the queue up
     */
    set_expected_results(
        "trq_retake: the next check backs up",
        json_pack("[{s:s},{s:s}]",
            "msg", MSG_MOVING,
            "msg", "Creating topic"
        ),
        NULL, NULL, 1
    );
    ret = trq_check_backup(trq);
    result += expect_int("trq_retake: trq_check_backup() backs up", ret, 0);
    result += expect_int("trq_retake: the new topic is empty",
        (json_int_t)tranger2_topic_size(tranger, topic_name), 0);
    trq_close(trq);
    tranger2_shutdown(tranger);
    result += test_json(NULL);

    return result;
}

PRIVATE int test_tr2q_topic_taken_again(void)
{
    int result = 0;
    const char *topic_name = "tr2q_retake";
    rmrdir(path_database);

    set_expected_results("tr2q_retake: setup", NULL, NULL, NULL, 0);
    json_t *tranger = startup();
    tr2_queue_t *trq = tr2q_open(tranger, topic_name, "tm", 0, 10, 1 /* backup_queue_size */);
    tr2q_load(trq);
    q2_msg_t *msg = tr2q_append(trq, 946684801, tr2q_kw(1, 946684801), 0);
    tr2q_unload_msg(msg, 0);
    test_json(NULL);    // the setup logs are not what is tested

    make_topic_unreadable(topic_name, TRUE);
    set_expected_results("tr2q_retake: the queue loses its topic", expected_topic_lost(), NULL, NULL, 1);
    int ret = tr2q_check_backup(trq);
    result += expect_int("tr2q_retake: tr2q_check_backup() of a failed backup", ret, -1);
    result += expect_int("tr2q_retake: the queue has no topic", trq->topic? 1: 0, 0);
    result += test_json(NULL);

    set_expected_results("tr2q_retake: a check without topic", expected_topic_not_opened(TRUE), NULL, NULL, 1);
    ret = tr2q_check_backup(trq);
    result += expect_int("tr2q_retake: tr2q_check_backup() without topic answers -1", ret, -1);
    result += test_json(NULL);

    set_expected_results("tr2q_retake: the next checks without topic say nothing",
        expected_topic_not_opened(FALSE), NULL, NULL, 1);
    for(int i = 0; i < 3; i++) {
        ret = tr2q_check_backup(trq);
        result += expect_int("tr2q_retake: a next check without topic answers -1", ret, -1);
    }
    result += test_json(NULL);

    make_topic_unreadable(topic_name, FALSE);
    set_expected_results(
        "tr2q_retake: the next check takes the topic again, and backs up",
        json_pack("[{s:s},{s:s},{s:s}]",
            "msg", "Queue topic taken again",
            "msg", MSG_MOVING,
            "msg", "Creating topic"
        ),
        NULL, NULL, 1
    );
    ret = tr2q_check_backup(trq);
    result += expect_int("tr2q_retake: tr2q_check_backup() backs up", ret, 0);
    if(!trq->topic || trq->topic != tranger2_topic(tranger, topic_name)) {
        printf("%sERROR%s --> tr2q_retake: the queue has no topic\n", On_Red BWhite, Color_Off);
        result += -1;
    }
    result += expect_int("tr2q_retake: the new topic is empty",
        (json_int_t)tranger2_topic_size(tranger, topic_name), 0);
    result += test_json(NULL);

    set_expected_results("tr2q_retake: the queue works", NULL, NULL, NULL, 1);
    msg = tr2q_append(trq, 946684802, tr2q_kw(2, 946684802), 0);
    result += expect_int("tr2q_retake: append", msg? 1: 0, 1);
    if(msg) {
        result += expect_int("tr2q_retake: a read", tr2q_msg_json(msg)? 1: 0, 1);
        result += expect_int("tr2q_retake: a hard mark", tr2q_save_hard_mark(msg, 0), 0);
        tr2q_unload_msg(msg, 0);
    }
    tr2q_close(trq);
    tranger2_shutdown(tranger);
    result += test_json(NULL);

    return result;
}

/***************************************************************************
 *  A topic_desc.json that CAN be read but does not load (broken json).
 *  Asking the disk "is it readable?" says yes, so up to this fix every call
 *  went through tranger2_topic() again and logged the causes of the failed
 *  open: once per second per session in the broker. Now the queue keeps
 *  what the file was (inode, size, mtime, ctime) when the open failed, and
 *  tries again only when it changes: a new broken content is tried once
 *  (the open says its causes again, the queue does not), the good one
 *  takes the topic again.
 ***************************************************************************/
PRIVATE char good_desc[64*1024];
PRIVATE size_t good_desc_len = 0;

PRIVATE int write_topic_desc(const char *topic_name, const char *content, size_t len)
{
    char topic_dir[PATH_MAX];
    char topic_desc[PATH_MAX];
    build_path(topic_dir, sizeof(topic_dir), path_database, topic_name, NULL);
    build_path(topic_desc, sizeof(topic_desc), topic_dir, "topic_desc.json", NULL);
    chmod(topic_desc, 0660);
    int fd = open(topic_desc, O_WRONLY|O_TRUNC);
    if(fd < 0) {
        printf("%sERROR%s --> cannot open %s\n", On_Red BWhite, Color_Off, topic_desc);
        return -1;
    }
    ssize_t written = write(fd, content, len);
    close(fd);
    if(written != (ssize_t)len) {
        printf("%sERROR%s --> cannot write %s\n", On_Red BWhite, Color_Off, topic_desc);
        return -1;
    }
    return 0;
}

PRIVATE int keep_good_topic_desc(const char *topic_name)
{
    char topic_dir[PATH_MAX];
    char topic_desc[PATH_MAX];
    build_path(topic_dir, sizeof(topic_dir), path_database, topic_name, NULL);
    build_path(topic_desc, sizeof(topic_desc), topic_dir, "topic_desc.json", NULL);
    int fd = open(topic_desc, O_RDONLY);
    if(fd < 0) {
        printf("%sERROR%s --> cannot open %s\n", On_Red BWhite, Color_Off, topic_desc);
        return -1;
    }
    ssize_t n = read(fd, good_desc, sizeof(good_desc));
    close(fd);
    if(n <= 0 || (size_t)n >= sizeof(good_desc)) {
        printf("%sERROR%s --> cannot read %s\n", On_Red BWhite, Color_Off, topic_desc);
        return -1;
    }
    good_desc_len = (size_t)n;
    return 0;
}

#define BROKEN_DESC_1   "{\"topic_name\": "
#define BROKEN_DESC_2   "{\"topic_name\": \"broken, and longer\""

/*
 *  The causes the open logs for a topic_desc.json that does not parse
 */
PRIVATE json_t *expected_broken_open(BOOL with_queue)
{
    if(with_queue) {
        return json_pack("[{s:s},{s:s},{s:s},{s:s}]",
            "msg", MSG_JSON_BROKEN,
            "msg", MSG_DESC,
            "msg", "Cannot open topic",
            "msg", "Queue without topic, it cannot be opened"
        );
    }
    return json_pack("[{s:s},{s:s},{s:s}]",
        "msg", MSG_JSON_BROKEN,
        "msg", MSG_DESC,
        "msg", "Cannot open topic"
    );
}

PRIVATE int test_trq_topic_desc_broken(void)
{
    int result = 0;
    const char *topic_name = "trq_broken";
    rmrdir(path_database);

    set_expected_results("trq_broken: setup", NULL, NULL, NULL, 0);
    json_t *tranger = startup();
    tr_queue_t *trq = trq_open(tranger, topic_name, "tm", 0, 1 /* backup_queue_size */);
    trq_load(trq);
    q_msg_t *msg = trq_append2(trq, 946684801, json_pack("{s:i, s:I}", "n", 1, "tm", (json_int_t)946684801), 0);
    trq_unload_msg(msg, 0);
    msg = trq_append2(trq, 946684802, json_pack("{s:i, s:I}", "n", 2, "tm", (json_int_t)946684802), 0);
    result += keep_good_topic_desc(topic_name);
    test_json(NULL);    // the setup logs are not what is tested

    make_topic_unreadable(topic_name, TRUE);
    set_expected_results("trq_broken: the queue loses its topic", expected_topic_lost(), NULL, NULL, 1);
    result += expect_int("trq_broken: trq_check_backup() of a failed backup", trq_check_backup(trq), -1);
    result += test_json(NULL);

    /*
     *  1. Readable, and broken: said once
     */
    result += write_topic_desc(topic_name, BROKEN_DESC_1, strlen(BROKEN_DESC_1));
    set_expected_results("trq_broken: a read, the desc broken", expected_broken_open(TRUE), NULL, NULL, 1);
    json_t *jn = trq_msg_json(msg);
    result += expect_int("trq_broken: a read answers NULL", jn? 1: 0, 0);
    JSON_DECREF(jn)
    result += test_json(NULL);

    set_expected_results("trq_broken: the next calls say nothing", NULL, NULL, NULL, 1);
    for(int i = 0; i < 3; i++) {
        jn = trq_msg_json(msg);
        result += expect_int("trq_broken: a next read answers NULL", jn? 1: 0, 0);
        JSON_DECREF(jn)
        result += expect_int("trq_broken: a next ack answers -1",
            trq_set_hard_flag(msg, TRQ_MSG_PENDING, 0), -1);
        result += expect_int("trq_broken: a next check answers -1", trq_check_backup(trq), -1);
    }
    result += test_json(NULL);

    /*
     *  2. Another broken content: tried once, the queue says nothing more
     */
    result += write_topic_desc(topic_name, BROKEN_DESC_2, strlen(BROKEN_DESC_2));
    set_expected_results("trq_broken: a changed desc is tried once", expected_broken_open(FALSE), NULL, NULL, 1);
    for(int i = 0; i < 3; i++) {
        result += expect_int("trq_broken: a check answers -1", trq_check_backup(trq), -1);
    }
    result += test_json(NULL);

    /*
     *  3. The good content: the read takes the topic again
     */
    result += write_topic_desc(topic_name, good_desc, good_desc_len);
    set_expected_results("trq_broken: the read takes the topic again",
        json_pack("[{s:s}]", "msg", "Queue topic taken again"), NULL, NULL, 1);
    jn = trq_msg_json(msg);
    result += expect_int("trq_broken: the read answers the message", jn? 1: 0, 1);
    JSON_DECREF(jn)
    result += expect_int("trq_broken: the ack works", trq_set_hard_flag(msg, TRQ_MSG_PENDING, 0), 0);
    trq_unload_msg(msg, 0);
    result += test_json(NULL);

    set_expected_results("trq_broken: shutdown", NULL, NULL, NULL, 1);
    trq_close(trq);
    tranger2_shutdown(tranger);
    result += test_json(NULL);

    return result;
}

PRIVATE int test_tr2q_topic_desc_broken(void)
{
    int result = 0;
    const char *topic_name = "tr2q_broken";
    rmrdir(path_database);

    set_expected_results("tr2q_broken: setup", NULL, NULL, NULL, 0);
    json_t *tranger = startup();
    tr2_queue_t *trq = tr2q_open(tranger, topic_name, "tm", 0, 10, 1 /* backup_queue_size */);
    tr2q_load(trq);
    q2_msg_t *msg = tr2q_append(trq, 946684801, tr2q_kw(1, 946684801), 0);
    tr2q_unload_msg(msg, 0);
    result += keep_good_topic_desc(topic_name);
    test_json(NULL);    // the setup logs are not what is tested

    make_topic_unreadable(topic_name, TRUE);
    set_expected_results("tr2q_broken: the queue loses its topic", expected_topic_lost(), NULL, NULL, 1);
    result += expect_int("tr2q_broken: tr2q_check_backup() of a failed backup", tr2q_check_backup(trq), -1);
    result += test_json(NULL);

    /*
     *  1. Readable, and broken: said once, then the broker's checks of every
     *  second say nothing
     */
    result += write_topic_desc(topic_name, BROKEN_DESC_1, strlen(BROKEN_DESC_1));
    set_expected_results("tr2q_broken: a check, the desc broken", expected_broken_open(TRUE), NULL, NULL, 1);
    result += expect_int("tr2q_broken: a check answers -1", tr2q_check_backup(trq), -1);
    result += test_json(NULL);

    set_expected_results("tr2q_broken: the next checks say nothing", NULL, NULL, NULL, 1);
    for(int i = 0; i < 5; i++) {
        result += expect_int("tr2q_broken: a next check answers -1", tr2q_check_backup(trq), -1);
    }
    result += test_json(NULL);

    /*
     *  2. Another broken content: tried once
     */
    result += write_topic_desc(topic_name, BROKEN_DESC_2, strlen(BROKEN_DESC_2));
    set_expected_results("tr2q_broken: a changed desc is tried once", expected_broken_open(FALSE), NULL, NULL, 1);
    for(int i = 0; i < 3; i++) {
        result += expect_int("tr2q_broken: a check answers -1", tr2q_check_backup(trq), -1);
    }
    result += test_json(NULL);

    /*
     *  3. The good content: taken again, and backed up
     */
    result += write_topic_desc(topic_name, good_desc, good_desc_len);
    set_expected_results(
        "tr2q_broken: the next check takes the topic again, and backs up",
        json_pack("[{s:s},{s:s},{s:s}]",
            "msg", "Queue topic taken again",
            "msg", MSG_MOVING,
            "msg", "Creating topic"
        ),
        NULL, NULL, 1
    );
    result += expect_int("tr2q_broken: tr2q_check_backup() backs up", tr2q_check_backup(trq), 0);
    result += test_json(NULL);

    set_expected_results("tr2q_broken: shutdown", NULL, NULL, NULL, 1);
    tr2q_close(trq);
    tranger2_shutdown(tranger);
    result += test_json(NULL);

    return result;
}

/***************************************************************************
 *  A create whose keys/ cannot be made, in a tranger that exits on a
 *  CRITICAL (LOG_OPT_EXIT_ZERO, the default of C_TRANGER, C_TREEDB and the
 *  broker's queues). The process exits, as it is told to -- but only once
 *  what was made is removed. Up to this fix the CRITICAL of the mkdir
 *  exited first: the topic_desc.json, topic_cols.json and topic_var.json
 *  stayed with no keys/, and the next start opened that half topic.
 *
 *  The exit is the expected end, so the create runs in a child process;
 *  the parent looks at what the child left on disk.
 ***************************************************************************/
PRIVATE int test_create_exit_zero(void)
{
    int result = 0;
    const char *direct = "t_exit_zero";
    rmrdir(path_database);

    char topic_dir[PATH_MAX];
    build_path(topic_dir, sizeof(topic_dir), path_database, direct, NULL);

    set_expected_results("create_exit_zero: the child", NULL, NULL, NULL, 0);
    fflush(stdout);
    pid_t pid = fork();
    if(pid == 0) {
        json_t *tranger = tranger2_startup(0, json_pack("{s:s, s:s, s:b, s:i}",
            "path", path_root,
            "database", DATABASE,
            "master", 1,
            "on_critical_error", (int)LOG_OPT_EXIT_ZERO
        ), 0);
        build_path(failing_mkdir, sizeof(failing_mkdir), topic_dir, "keys", NULL);
        tranger2_create_topic(tranger, direct, "id", "tm", NULL, sf_string_key,
            json_pack("{s:s, s:I}", "id", "", "tm", (json_int_t)0), 0);
        printf("%sERROR%s --> create_exit_zero: the create returned, it did not exit\n",
            On_Red BWhite, Color_Off);
        fflush(stdout);
        _exit(3);
    }
    if(pid < 0) {
        printf("%sERROR%s --> create_exit_zero: fork() FAILED\n", On_Red BWhite, Color_Off);
        return -1;
    }
    int status = 0;
    waitpid(pid, &status, 0);
    test_json(NULL);    // the logs of the child are not in this process

    result += expect_int("create_exit_zero: the child exited(0) on the CRITICAL",
        WIFEXITED(status)? WEXITSTATUS(status): -1, 0);
    result += expect_int("create_exit_zero: nothing of the topic on disk",
        (json_int_t)is_directory(topic_dir), 0);

    set_expected_results("create_exit_zero: the next create makes it whole",
        json_pack("[{s:s}]", "msg", "Creating topic"), NULL, NULL, 1);
    json_t *tranger = startup();
    json_t *topic = tranger2_create_topic(tranger, direct, "id", "tm", NULL, sf_string_key,
        json_pack("{s:s, s:I}", "id", "", "tm", (json_int_t)0), 0);
    char keys_dir[PATH_MAX];
    build_path(keys_dir, sizeof(keys_dir), topic_dir, "keys", NULL);
    result += expect_int("create_exit_zero: the next create answers the topic", topic? 1: 0, 1);
    result += expect_int("create_exit_zero: with its keys/", (json_int_t)is_directory(keys_dir), 1);
    tranger2_shutdown(tranger);
    result += test_json(NULL);

    return result;
}

/***************************************************************************
 *  A backup whose create fails, in a tranger that exits on a CRITICAL
 ***************************************************************************/
PRIVATE BOOL in_exit_zero_case = FALSE;

PRIVATE void exit_in_exit_zero_case(void)
{
    if(in_exit_zero_case) {
        printf("%sERROR%s --> exit_zero: the process exited in the backup (exit(0) of a CRITICAL): "
            "the backup was not moved back\n", On_Red BWhite, Color_Off);
        printf("<-- %sTEST FAILED%s: %s\n", On_Red BWhite, Color_Off, APP);
        fflush(stdout);
        _exit(-1);
    }
}

PRIVATE int test_exit_zero_create_fails(void)
{
    int result = 0;
    const char *topic_name = "tr2q_exit_zero";
    rmrdir(path_database);

    set_expected_results("exit_zero: setup", NULL, NULL, NULL, 0);
    json_t *tranger = tranger2_startup(0, json_pack("{s:s, s:s, s:b, s:i}",
        "path", path_root,
        "database", DATABASE,
        "master", 1,
        "on_critical_error", (int)LOG_OPT_EXIT_ZERO     // as the broker's queues
    ), 0);
    tr2_queue_t *trq = tr2q_open(tranger, topic_name, "tm", 0, 10, 1 /* backup_queue_size */);
    tr2q_load(trq);
    q2_msg_t *msg = tr2q_append(trq, 946684801, tr2q_kw(1, 946684801), 0);
    tr2q_unload_msg(msg, 0);
    test_json(NULL);    // the setup logs are not what is tested

    char topic_dir[PATH_MAX];
    char backup_dir[PATH_MAX];
    char bak_name[NAME_MAX];
    snprintf(bak_name, sizeof(bak_name), "%s.bak", topic_name);
    build_path(topic_dir, sizeof(topic_dir), path_database, topic_name, NULL);
    build_path(backup_dir, sizeof(backup_dir), path_database, bak_name, NULL);
    snprintf(failing_mkdir, sizeof(failing_mkdir), "%s", topic_dir);

    set_expected_results(
        "exit_zero: the new topic cannot be created",
        json_pack("[{s:s},{s:s},{s:s, s:s},{s:s},{s:s},{s:s}]",
            "msg", MSG_MOVING,
            "msg", "newdir() FAILED",
            "msg", "Cannot create TimeRanger subdir. mkrdir() FAILED", "serrno", "No space left on device",
            "msg", MSG_ABANDON,
            "msg", MSG_REOPENED,
            "msg", MSG_QUEUE
        ),
        NULL, NULL, 1
    );
    in_exit_zero_case = TRUE;
    int ret = tr2q_check_backup(trq);
    in_exit_zero_case = FALSE;
    failing_mkdir[0] = 0;
    result += expect_int("exit_zero: tr2q_check_backup() of a failed create", ret, -1);
    result += expect_int("exit_zero: the backup was moved back", (json_int_t)is_directory(backup_dir), 0);
    result += expect_int("exit_zero: the queue keeps its message",
        (json_int_t)tranger2_topic_size(tranger, topic_name), 1);
    result += expect_int("exit_zero: the tranger exits on a CRITICAL again",
        kw_get_int(0, tranger, "on_critical_error", 0, 0), LOG_OPT_EXIT_ZERO);
    result += test_json(NULL);

    set_expected_results("exit_zero: shutdown", NULL, NULL, NULL, 1);
    tr2q_close(trq);
    tranger2_shutdown(tranger);
    result += test_json(NULL);

    return result;
}

/***************************************************************************
 *  do_test
 ***************************************************************************/
PRIVATE int do_test(void)
{
    int result = 0;
    build_path(path_root, sizeof(path_root), getenv("HOME"), "tests_yuneta", NULL);
    mkrdir(path_root, 02770);
    build_path(path_database, sizeof(path_database), path_root, DATABASE, NULL);

    result += test_trq();
    result += test_tr2q();
    result += test_trq_create_fails();
    result += test_create_keys_fails();
    result += test_create_exit_zero();
    if(geteuid() == 0) {
        printf("skip trq_retake, tr2q_retake: as root a file of mode 0 can be read\n");
    } else {
        result += test_trq_topic_taken_again();
        result += test_tr2q_topic_taken_again();
        result += test_trq_topic_desc_broken();
        result += test_tr2q_topic_desc_broken();
    }
    atexit(exit_in_exit_zero_case);
    result += test_exit_zero_create_fails();

    rmrdir(path_database);
    return result;
}

/***************************************************************************
 *              Main
 ***************************************************************************/
int main(int argc, char *argv[])
{
    setvbuf(stdout, NULL, _IOLBF, 0);   // what was checked is printed, also before an exit

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
