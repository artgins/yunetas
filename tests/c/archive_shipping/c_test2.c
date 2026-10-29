/****************************************************************************
 *          C_TEST2.C
 *
 *          A fake archive_sender against an archive_keeper: the frames of
 *          the native transport written by hand, over tcp://127.0.0.1:7758,
 *          and each answer of the keeper checked. What a real sender never
 *          sends is what a peer can:
 *
 *          1. an end whose sha256 is not of the bytes that arrived;
 *          2. an end with fewer sha256 than files;
 *          3. an end whose sha256 is not 64 lowercase hex characters;
 *          4. data of a file before the files before it are whole, then a
 *             frame of that batch the sender had already sent (answered,
 *             not counted nor logged again), and a begin with an empty
 *             batch;
 *          5. a good batch: stored (the frames of the driver are right);
 *          6. the same batch again: stored, already here;
 *          7. a batch of that name with the same files and sizes and other
 *             bytes, with their own right sha256: refused, the batch kept.
 *
 *          Then a second link (two channels, two senders of one node):
 *
 *          8. link 1 begins a batch and sends its first file;
 *          9. link 2 begins the same batch: refused, it is in flight on
 *             link 1 (it used to remove link 1's copy aside and make its
 *             own, and link 1 was answered stored for files it had not
 *             written: the data file empty in the backup);
 *          10. link 1 ends it: stored, with link 1's bytes;
 *          11. link 1 begins another batch, sends a file, and goes silent
 *              for more than stale_session_timeout;
 *          12. link 2 begins that batch: taken over (link 1 dropped), and
 *              stored with link 2's bytes.
 *
 *          A refused batch leaves nothing in the backup, nor its copy aside.
 *          The batches are refused before require_client_certificate counts
 *          (it is FALSE here: plain tcp; test1 runs the mutual TLS).
 *
 *          Copyright (c) 2026, ArtGins.
 *          All Rights Reserved.
 ***********************************************************************/
#include <string.h>
#include <ctype.h>
#include <unistd.h>
#include <fcntl.h>
#include <dirent.h>
#include <sys/stat.h>
#include "c_test2.h"

/***************************************************************************
 *              Constants
 ***************************************************************************/
#define NODE_NAME       "test_node"
#define BATCH_DIR       "db/topic/%s/2000-01-01@1"
#define FILE_DATA       "2000-01-01.json"
#define FILE_MANIFEST   "manifest.json"
#define TEST_TIMEOUT_MS 10000
#define STALE_WAIT_MS   2500    // more than the stale_session_timeout of main_test2.c

/***************************************************************************
 *              Structures
 ***************************************************************************/

/***************************************************************************
 *              Prototypes
 ***************************************************************************/
PRIVATE void send_scenario(hgobj gobj);

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
    hgobj timer_wait;
    hgobj keeper;
    hgobj gobj_output_side;
    hgobj gobj_output_side2;
    char base[PATH_MAX];
    char backup[PATH_MAX];
    int scenario;           // 1..12, 13 the end
    json_t *jn_expected;    // [{cmd, msg, side}] the answers of the scenario, in order
    size_t got;             // answers of the scenario received
    BOOL side2_open;
    BOOL stale_waited;
    BOOL side1_drop_expected;
    BOOL side1_dropped;
    BOOL finished;
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
    priv->timer_wait = gobj_create_pure_child("wait", C_TIMER, 0, gobj);
}

/***************************************************************************
 *      Framework Method start
 ***************************************************************************/
PRIVATE int mt_start(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    gobj_start(priv->timer);
    gobj_start(priv->timer_wait);
    priv->gobj_output_side = gobj_find_service("__output_side__", TRUE);
    gobj_subscribe_event(priv->gobj_output_side, 0, 0, gobj);
    priv->gobj_output_side2 = gobj_find_service("__output_side2__", TRUE);
    gobj_subscribe_event(priv->gobj_output_side2, 0, 0, gobj);
    return 0;
}

/***************************************************************************
 *      Framework Method stop
 ***************************************************************************/
PRIVATE int mt_stop(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    gobj_stop(priv->timer);
    gobj_stop(priv->timer_wait);
    if(priv->gobj_output_side) {
        gobj_unsubscribe_event(priv->gobj_output_side, 0, 0, gobj);
    }
    if(priv->gobj_output_side2) {
        gobj_unsubscribe_event(priv->gobj_output_side2, 0, 0, gobj);
    }
    return 0;
}

/***************************************************************************
 *      Framework Method destroy
 ***************************************************************************/
PRIVATE void mt_destroy(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    JSON_DECREF(priv->jn_expected)
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
 *  A frame of the native transport, on link `side`: compact json, and with
 *  data a NUL and the bytes after it
 ***************************************************************************/
PRIVATE void send_frame_on(hgobj gobj, hgobj side, json_t *jn_header, const char *data)
{
    char *header = json2uglystr(jn_header);
    JSON_DECREF(jn_header)
    size_t header_len = strlen(header);
    size_t len = data? strlen(data) : 0;
    size_t size = header_len + (data? 1 + len : 0);
    gbuffer_t *gbuf = gbuffer_create(size, size);
    gbuffer_append(gbuf, header, header_len);
    if(data) {
        gbuffer_append_char(gbuf, 0);
        gbuffer_append(gbuf, (void *)data, len);
    }
    GBMEM_FREE(header)
    gobj_send_event(
        side,
        EV_SEND_MESSAGE,
        json_pack("{s:I}", "gbuffer", (json_int_t)(size_t)gbuf),
        gobj
    );
}

PRIVATE void send_frame(hgobj gobj, json_t *jn_header, const char *data)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    send_frame_on(gobj, priv->gobj_output_side, jn_header, data);
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE void batch_of(const char *key, char *bf, size_t bfsize)
{
    snprintf(bf, bfsize, BATCH_DIR, key);
}

PRIVATE void sha_of(const char *text, char hex[SHA256_HEX_LEN + 1])
{
    sha256_hex(text, strlen(text), hex, SHA256_HEX_LEN + 1);
}

/***************************************************************************
 *  begin, the data of the two files (in this order), and an end with these
 *  sha256 (NULL: no end)
 ***************************************************************************/
PRIVATE void send_batch_on(hgobj gobj, hgobj side, const char *key, const char *content, json_t *sha256)
{
    char batch[PATH_MAX];
    batch_of(key, batch, sizeof(batch));
    send_frame_on(gobj, side, json_pack("{s:s, s:s, s:s, s:[{s:s, s:i}, {s:s, s:i}]}",
        "cmd", "begin",
        "node", NODE_NAME,
        "batch", batch,
        "files",
            "name", FILE_DATA, "size", (int)strlen(content),
            "name", FILE_MANIFEST, "size", 2
    ), NULL);
    send_frame_on(gobj, side, json_pack("{s:s, s:s, s:i, s:i}",
        "cmd", "data", "batch", batch, "file", 0, "offset", 0
    ), content);
    send_frame_on(gobj, side, json_pack("{s:s, s:s, s:i, s:i}",
        "cmd", "data", "batch", batch, "file", 1, "offset", 0
    ), "{}");
    if(sha256) {
        send_frame_on(gobj, side, json_pack("{s:s, s:s, s:o}",
            "cmd", "end", "batch", batch, "sha256", sha256
        ), NULL);
    }
}

PRIVATE void send_batch(hgobj gobj, const char *key, const char *content, json_t *sha256)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    send_batch_on(gobj, priv->gobj_output_side, key, content, sha256);
}

/***************************************************************************
 *  begin of a batch of two files, and the data of the first (no more)
 ***************************************************************************/
PRIVATE void send_first_half(hgobj gobj, hgobj side, const char *key, const char *content)
{
    char batch[PATH_MAX];
    batch_of(key, batch, sizeof(batch));
    send_frame_on(gobj, side, json_pack("{s:s, s:s, s:s, s:[{s:s, s:i}, {s:s, s:i}]}",
        "cmd", "begin",
        "node", NODE_NAME,
        "batch", batch,
        "files",
            "name", FILE_DATA, "size", (int)strlen(content),
            "name", FILE_MANIFEST, "size", 2
    ), NULL);
    send_frame_on(gobj, side, json_pack("{s:s, s:s, s:i, s:i}",
        "cmd", "data", "batch", batch, "file", 0, "offset", 0
    ), content);
}

/***************************************************************************
 *  What a batch is in the backup: 0 nothing (neither it nor its copy
 *  aside), 1 the batch alone, -1 anything else
 ***************************************************************************/
PRIVATE int batch_in_backup(hgobj gobj, const char *key)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    char key_dir[PATH_MAX];
    build_path(key_dir, sizeof(key_dir), priv->backup, NODE_NAME, "db", "topic", key, NULL);
    int batches = 0, others = 0;
    DIR *dir = opendir(key_dir);
    struct dirent *entry;
    while(dir && (entry = readdir(dir)) != NULL) {
        if(strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
            continue;
        }
        if(strcmp(entry->d_name, "2000-01-01@1") == 0) {
            batches++;
        } else {
            others++;
        }
    }
    if(dir) {
        closedir(dir);
    }
    if(others) {
        return -1;
    }
    return batches;
}

/***************************************************************************
 *  The content of the data file of a batch kept in the backup
 ***************************************************************************/
PRIVATE BOOL backup_holds(hgobj gobj, const char *key, const char *content)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    char path[PATH_MAX];
    char batch[PATH_MAX];
    char bf[64] = {0};
    batch_of(key, batch, sizeof(batch));
    build_path(path, sizeof(path), priv->backup, NODE_NAME, batch, FILE_DATA, NULL);
    int fd = open(path, O_RDONLY|O_CLOEXEC);
    if(fd < 0) {
        return FALSE;
    }
    ssize_t n = read(fd, bf, sizeof(bf) - 1);
    close(fd);
    return n >= 0 && strcmp(bf, content) == 0;
}

/***************************************************************************
 *  The checks after the answers of a scenario
 ***************************************************************************/
PRIVATE void check_scenario(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    char name[64];
    snprintf(name, sizeof(name), "scenario %d", priv->scenario);
    switch(priv->scenario) {
        case 1:
        case 2:
        case 3:
            if(batch_in_backup(gobj, priv->scenario == 1? "key1" : priv->scenario == 2? "key2" : "key3") != 0) {
                fail("A refused batch left something in the backup", name);
            }
            break;
        case 4:
            if(batch_in_backup(gobj, "key4") != 0) {
                fail("A refused batch left something in the backup", name);
            }
            break;
        case 5:
            if(batch_in_backup(gobj, "key5") != 1 || !backup_holds(gobj, "key5", "hello")) {
                fail("The good batch is not whole in the backup", name);
            }
            break;
        case 6:
            if(batch_in_backup(gobj, "key5") != 1 ||
                    gobj_read_integer_attr(priv->keeper, "batches_already") != 1) {
                fail("The batch here already was not stored once, as it was", name);
            }
            break;
        case 7:
            if(batch_in_backup(gobj, "key5") != 1 || !backup_holds(gobj, "key5", "hello")) {
                fail("A batch of other bytes changed the batch kept", name);
            }
            break;
        case 10:
            if(batch_in_backup(gobj, "key8") != 1 || !backup_holds(gobj, "key8", "AAAAAAAA")) {
                fail("The batch of link 1 is not in the backup with its bytes", name);
            }
            break;
        case 12:
            if(batch_in_backup(gobj, "key11") != 1 || !backup_holds(gobj, "key11", "BBBBBBBB")) {
                fail("The batch taken over is not in the backup with the bytes of link 2", name);
            }
            break;
    }
}

/***************************************************************************
 *  The frames of the scenario, and the answers it must get
 ***************************************************************************/
PRIVATE void send_scenario(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    char sha_hello[SHA256_HEX_LEN + 1];
    char sha_hellx[SHA256_HEX_LEN + 1];
    char sha_manifest[SHA256_HEX_LEN + 1];
    sha_of("hello", sha_hello);
    sha_of("hellX", sha_hellx);
    sha_of("{}", sha_manifest);

    JSON_DECREF(priv->jn_expected)
    priv->got = 0;

    switch(priv->scenario) {
        case 1:
            /*
             *  The bytes of "hello", the sha256 of "hellX"
             */
            priv->jn_expected = json_pack("[{s:s}, {s:s}, {s:s}, {s:s, s:s}]",
                "cmd", "begin-ack", "cmd", "data-ack", "cmd", "data-ack",
                "cmd", "refused", "msg", "a file is not the content that its sha256 says"
            );
            send_batch(gobj, "key1", "hello", json_pack("[s, s]", sha_hellx, sha_manifest));
            break;

        case 2:
            priv->jn_expected = json_pack("[{s:s}, {s:s}, {s:s}, {s:s, s:s}]",
                "cmd", "begin-ack", "cmd", "data-ack", "cmd", "data-ack",
                "cmd", "refused", "msg", "the end does not carry a sha256 for each file"
            );
            send_batch(gobj, "key2", "hello", json_pack("[s]", sha_hello));
            break;

        case 3:
            {
                char upper[SHA256_HEX_LEN + 1];
                for(int i = 0; i <= SHA256_HEX_LEN; i++) {
                    upper[i] = (char)toupper((unsigned char)sha_hello[i]);
                }
                priv->jn_expected = json_pack("[{s:s}, {s:s}, {s:s}, {s:s, s:s}]",
                    "cmd", "begin-ack", "cmd", "data-ack", "cmd", "data-ack",
                    "cmd", "refused", "msg", "a file is not the content that its sha256 says"
                );
                send_batch(gobj, "key3", "hello", json_pack("[s, s]", upper, sha_manifest));
            }
            break;

        case 4:
            {
                /*
                 *  The manifest (file 1) before the data file (file 0) is whole
                 */
                char batch[PATH_MAX];
                batch_of("key4", batch, sizeof(batch));
                priv->jn_expected = json_pack("[{s:s}, {s:s, s:s}, {s:s, s:s}, {s:s, s:s}]",
                    "cmd", "begin-ack",
                    "cmd", "refused", "msg", "data of a file before the files before it are whole",
                    "cmd", "refused", "msg", "data of a batch not begun",
                    "cmd", "refused", "msg", "the batch is not <database>/<topic>/<key>/<batch> of plain names"
                );
                send_frame(gobj, json_pack("{s:s, s:s, s:s, s:[{s:s, s:i}, {s:s, s:i}]}",
                    "cmd", "begin", "node", NODE_NAME, "batch", batch,
                    "files", "name", FILE_DATA, "size", 5, "name", FILE_MANIFEST, "size", 2
                ), NULL);
                send_frame(gobj, json_pack("{s:s, s:s, s:i, s:i}",
                    "cmd", "data", "batch", batch, "file", 1, "offset", 0
                ), "{}");
                /*
                 *  A frame of the batch refused that was on its way: answered,
                 *  and the batch is not refused (nor logged) twice
                 */
                send_frame(gobj, json_pack("{s:s, s:s, s:i, s:i}",
                    "cmd", "data", "batch", batch, "file", 0, "offset", 0
                ), "hello");
                send_frame(gobj, json_pack("{s:s, s:s, s:s, s:[{s:s, s:i}]}",
                    "cmd", "begin", "node", NODE_NAME, "batch", "",
                    "files", "name", FILE_DATA, "size", 5
                ), NULL);
            }
            break;

        case 5:
        case 6:
            priv->jn_expected = json_pack("[{s:s}, {s:s}, {s:s}, {s:s}]",
                "cmd", "begin-ack", "cmd", "data-ack", "cmd", "data-ack", "cmd", "stored"
            );
            send_batch(gobj, "key5", "hello", json_pack("[s, s]", sha_hello, sha_manifest));
            break;

        case 7:
            /*
             *  Same names and sizes as the batch kept, other bytes, and THEIR
             *  sha256: the transfer is right, the batch is not the one here
             */
            priv->jn_expected = json_pack("[{s:s}, {s:s}, {s:s}, {s:s, s:s}]",
                "cmd", "begin-ack", "cmd", "data-ack", "cmd", "data-ack",
                "cmd", "refused", "msg", "a different batch of this name is here"
            );
            send_batch(gobj, "key5", "hellX", json_pack("[s, s]", sha_hellx, sha_manifest));
            break;

        case 8:
            if(!priv->side2_open) {
                gobj_start_tree(priv->gobj_output_side2);   // on its open, again
                break;
            }
            priv->jn_expected = json_pack("[{s:s, s:i}, {s:s, s:i}]",
                "cmd", "begin-ack", "side", 1,
                "cmd", "data-ack", "side", 1
            );
            send_first_half(gobj, priv->gobj_output_side, "key8", "AAAAAAAA");
            break;

        case 9:
            {
                char batch[PATH_MAX];
                batch_of("key8", batch, sizeof(batch));
                priv->jn_expected = json_pack("[{s:s, s:s, s:i}]",
                    "cmd", "refused", "msg", "the batch is in flight on another channel", "side", 2
                );
                send_frame_on(gobj, priv->gobj_output_side2, json_pack(
                    "{s:s, s:s, s:s, s:[{s:s, s:i}, {s:s, s:i}]}",
                    "cmd", "begin", "node", NODE_NAME, "batch", batch,
                    "files", "name", FILE_DATA, "size", 8, "name", FILE_MANIFEST, "size", 2
                ), NULL);
            }
            break;

        case 10:
            {
                char batch[PATH_MAX];
                char sha_a[SHA256_HEX_LEN + 1];
                batch_of("key8", batch, sizeof(batch));
                sha_of("AAAAAAAA", sha_a);
                priv->jn_expected = json_pack("[{s:s, s:i}, {s:s, s:i}]",
                    "cmd", "data-ack", "side", 1,
                    "cmd", "stored", "side", 1
                );
                send_frame(gobj, json_pack("{s:s, s:s, s:i, s:i}",
                    "cmd", "data", "batch", batch, "file", 1, "offset", 0
                ), "{}");
                send_frame(gobj, json_pack("{s:s, s:s, s:[s, s]}",
                    "cmd", "end", "batch", batch, "sha256", sha_a, sha_manifest
                ), NULL);
            }
            break;

        case 11:
            priv->jn_expected = json_pack("[{s:s, s:i}, {s:s, s:i}]",
                "cmd", "begin-ack", "side", 1,
                "cmd", "data-ack", "side", 1
            );
            send_first_half(gobj, priv->gobj_output_side, "key11", "AAAAAAAA");
            break;

        case 12:
            if(!priv->stale_waited) {
                set_timeout(priv->timer_wait, STALE_WAIT_MS);   // link 1 silent; then again
                break;
            }
            {
                char sha_b[SHA256_HEX_LEN + 1];
                sha_of("BBBBBBBB", sha_b);
                priv->side1_drop_expected = TRUE;
                priv->jn_expected = json_pack("[{s:s, s:i}, {s:s, s:i}, {s:s, s:i}, {s:s, s:i}]",
                    "cmd", "begin-ack", "side", 2,
                    "cmd", "data-ack", "side", 2,
                    "cmd", "data-ack", "side", 2,
                    "cmd", "stored", "side", 2
                );
                send_batch_on(gobj, priv->gobj_output_side2, "key11", "BBBBBBBB",
                    json_pack("[s, s]", sha_b, sha_manifest)
                );
            }
            break;

        default:
            /*
             *  The end, when the silent link was dropped
             */
            if(!priv->side1_dropped) {
                break;  // at its close, again
            }
            if(gobj_read_integer_attr(priv->keeper, "batches_refused") != 7 ||
                    gobj_read_integer_attr(priv->keeper, "batches_stored") != 3 ||
                    gobj_read_integer_attr(priv->keeper, "batches_already") != 1) {
                fail("The stats of the keeper are not 7 refused, 3 stored, 1 already", NULL);
            }
            priv->finished = TRUE;
            clear_timeout(priv->timer);
            rmrdir(priv->base);
            set_yuno_must_die();
            break;
    }
}




                    /***************************
                     *      Actions
                     ***************************/




/***************************************************************************
 *  The link to the keeper is up: the scenarios start
 ***************************************************************************/
PRIVATE int ac_on_open(hgobj gobj, gobj_event_t event, json_t *kw, hgobj src)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    if(src == priv->gobj_output_side2) {
        if(!priv->side2_open) {
            priv->side2_open = TRUE;
            send_scenario(gobj);    // scenario 8 waits for it
        }
    } else if(priv->scenario == 0) {
        priv->scenario = 1;
        send_scenario(gobj);
    }

    KW_DECREF(kw)
    return 0;
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE int ac_on_close(hgobj gobj, gobj_event_t event, json_t *kw, hgobj src)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    if(src == priv->gobj_output_side && priv->side1_drop_expected && !priv->side1_dropped) {
        priv->side1_dropped = TRUE;
        if(priv->scenario > 12) {
            send_scenario(gobj);    // the end waits for it
        }
    } else if(!priv->finished && gobj_is_playing(gobj)) {
        fail("The keeper closed the link", NULL);
    }

    KW_DECREF(kw)
    return 0;
}

/***************************************************************************
 *  An answer of the keeper: the next one the scenario expects
 ***************************************************************************/
PRIVATE int ac_on_message(hgobj gobj, gobj_event_t event, json_t *kw, hgobj src)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    gbuffer_t *gbuf = (gbuffer_t *)(size_t)kw_get_int(gobj, kw, "gbuffer", 0, 0);
    json_t *jn_answer = gbuf? json_loadb(gbuffer_cur_rd_pointer(gbuf), gbuffer_leftbytes(gbuf), 0, 0) : NULL;
    json_t *expected = json_array_get(priv->jn_expected, priv->got);
    const char *cmd = kw_get_str(gobj, jn_answer, "cmd", "", 0);
    const char *msg = kw_get_str(gobj, jn_answer, "msg", "", 0);
    char detail[512];
    snprintf(detail, sizeof(detail), "scenario %d, answer %d: %s %s",
        priv->scenario, (int)priv->got + 1, cmd, msg);
    if(!expected) {
        fail("An answer of the keeper that no scenario expects", detail);
    } else if(strcmp(cmd, kw_get_str(gobj, expected, "cmd", "", 0)) != 0 ||
            (json_object_get(expected, "msg") &&
             strcmp(msg, kw_get_str(gobj, expected, "msg", "", 0)) != 0)) {
        fail("An answer of the keeper is not the one expected", detail);
    } else if((kw_get_int(gobj, expected, "side", 1, 0) == 2? priv->gobj_output_side2 : priv->gobj_output_side) != src) {
        fail("An answer of the keeper came on the other link", detail);
    }
    JSON_DECREF(jn_answer)

    priv->got++;
    if(priv->got == json_array_size(priv->jn_expected)) {
        check_scenario(gobj);
        priv->scenario++;
        send_scenario(gobj);
    }

    KW_DECREF(kw)
    return 0;
}

/***************************************************************************
 *  The first tick: the keeper and the link; later: the deadline
 ***************************************************************************/
PRIVATE int ac_timeout(hgobj gobj, gobj_event_t event, json_t *kw, hgobj src)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    if(src == priv->timer_wait) {
        priv->stale_waited = TRUE;
        send_scenario(gobj);    // scenario 12 waits for it
        JSON_DECREF(kw)
        return 0;
    }

    if(!priv->keeper) {
        build_path(priv->base, sizeof(priv->base), getenv("HOME"), "tests_yuneta", "archive_shipping2", NULL);
        build_path(priv->backup, sizeof(priv->backup), priv->base, "backup", NULL);
        rmrdir(priv->base);
        mkrdir(priv->backup, 02770);

        priv->keeper = gobj_find_service("archive_keeper", TRUE);
        gobj_write_str_attr(priv->keeper, "backup_directory", priv->backup);
        gobj_start(priv->keeper);
        gobj_play(priv->keeper);
        gobj_start_tree(priv->gobj_output_side);
        set_timeout(priv->timer, TEST_TIMEOUT_MS);
        JSON_DECREF(kw)
        return 0;
    }

    char detail[64];
    snprintf(detail, sizeof(detail), "scenario %d, answers %d", priv->scenario, (int)priv->got);
    fail("The scenarios did not end in time", detail);
    set_yuno_must_die();
    JSON_DECREF(kw)
    return 0;
}

/***************************************************************************
 *      Framework Method play
 ***************************************************************************/
PRIVATE int mt_play(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    set_timeout(priv->timer, 100);
    return 0;
}

/***************************************************************************
 *      Framework Method pause
 ***************************************************************************/
PRIVATE int mt_pause(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    clear_timeout(priv->timer);
    clear_timeout(priv->timer_wait);
    if(priv->gobj_output_side) {
        gobj_stop_tree(priv->gobj_output_side);
    }
    if(priv->gobj_output_side2) {
        gobj_stop_tree(priv->gobj_output_side2);
    }
    if(priv->keeper && gobj_is_playing(priv->keeper)) {
        gobj_pause(priv->keeper);
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
GOBJ_DEFINE_GCLASS(C_TEST2);

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
        {EV_ON_OPEN,                ac_on_open,                 0},
        {EV_ON_CLOSE,               ac_on_close,                0},
        {EV_ON_MESSAGE,             ac_on_message,              0},
        {0,0,0}
    };

    states_t states[] = {
        {ST_IDLE,                   st_idle},
        {0, 0}
    };

    event_type_t event_types[] = {
        {EV_TIMEOUT,                0},
        {EV_ON_OPEN,                0},
        {EV_ON_CLOSE,               0},
        {EV_ON_MESSAGE,             0},
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
PUBLIC int register_c_test2(void)
{
    return create_gclass(C_TEST2);
}
