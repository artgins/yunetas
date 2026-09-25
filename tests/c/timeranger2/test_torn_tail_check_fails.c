/****************************************************************************
 *          test_torn_tail_check_fails.c
 *
 *  A md2 that ends in a part of a row is cut back only after a check of
 *  its tail (check_torn_md2_tail), which reads the content file. A check
 *  that FOUND damage and a check that could not RUN are not the same:
 *  the second says nothing about the file.
 *
 *      1. a running master's append finds its md2 torn, and the content
 *         file cannot be opened for the check (EMFILE): the append is
 *         refused, its content is cut back, and the file is NOT flagged.
 *         The same process reads the whole rows with no load_failed, and
 *         the next append checks the tail again, cuts it back and goes in.
 *         Before, the file was flagged for the life of the process.
 *      2. a master opens a store whose md2 is torn, and the content file
 *         cannot be opened for the check (EMFILE): the file is flagged.
 *         An open that cannot check the tail cannot tell a torn row from
 *         the shape 7.25.4 left, and reading the whole rows of that shape
 *         reads rows that are not rows. The load says load_failed. The
 *         next append into the file counts it again, and the check runs
 *         then: cut back, unflagged, and the row goes in.
 *      3. a torn md2 whose last 32 bytes name a range of the content file
 *         larger than the largest memory block, with a NUL at its end.
 *         The range crosses the NUL of the first record, so it is not a
 *         record: the check reads it in parts and finds that NUL, with no
 *         block of its size. The tail is a torn row after good rows, and
 *         the master cuts it back with no memory error.
 *      4. a master whose largest memory block (MEM_MAX_BLOCK, set per
 *         yuno) is smaller than the writer's:
 *         a. opens the shape 7.25.4 left: whole rows, 31 bytes of a torn
 *            row, then an acknowledged row whose content is larger than
 *            the master's block, and bytes after it that no row names.
 *            The row before the end reads as a good row (the torn row's
 *            content is 256 bytes), so only rule 1 stops the cut. The
 *            content cannot be parsed, and its only NUL is its last byte:
 *            the file is flagged, not cut.
 *         b. opens a torn row after a last whole row whose content is
 *            larger than its block: the row cannot be checked, the file
 *            is flagged, not cut.
 *      5. the same master, with contents that fit in its block and whose
 *         parse does not (jansson doubles the buffer of a string, and the
 *         table of an array). Each case runs in a child, and a child that
 *         dies by a signal fails the test:
 *         a. the shape 7.25.4 left, the last row one string of 40 000
 *            bytes: flagged, not cut. Before, jansson wrote past its
 *            buffer (the crash of every open).
 *         b. the same shape, the last row an array in 20 000 bytes:
 *            flagged, not cut. Before, the failed parse was taken for "not
 *            a record", and the cut lost the acknowledged row.
 *         c. the read of a record of one string of 40 000 bytes: a
 *            CRITICAL that says why, and the list says load_failed.
 *            Before, the read crashed.
 *
 *  No file mode makes an open fail with EMFILE, so the test links with
 *  `-Wl,--wrap=open` (see CMakeLists.txt): __wrap_open() fails the next
 *  opens for reading of a content file of key A, and passes every other
 *  open through.
 *
 *          Copyright (c) 2026, ArtGins.
 *          All Rights Reserved.
 ****************************************************************************/
#include <string.h>
#include <errno.h>
#include <stdarg.h>
#include <signal.h>
#include <limits.h>
#include <fcntl.h>
#include <unistd.h>
#include <endian.h>
#include <sys/wait.h>

#include <gobj.h>
#include <kwid.h>
#include <timeranger2.h>
#include <helpers.h>
#include <yev_loop.h>
#include <testing.h>

#define APP         "test_torn_tail_check_fails"
#define DATABASE    "tr_torn_tail_check_fails"
#define TOPIC_NAME  "topic_check_fails"
#define DAY1        946684800   // 2000-01-01
#define ROW         32          // sizeof(md2_record_t)
#define TORN        13          // the bytes of the torn row
#define FAULT_DIR   "/keys/A/"

#define MSG_CUT             "md2 file of the key ends in a part of a row: an append that was never acknowledged was cut back"
#define MSG_NOT_CHECKED     "Cannot check the torn tail of a md2 file, its content file cannot be read: not cut"
#define MSG_APPEND_UNCHECKED "Cannot append record, the torn tail of its md2 file cannot be checked now: the append is refused, the file is not flagged"
#define MSG_FLAG            "md2 file of the key unreadable when its cache was built: every load of the key says load_failed"
#define MSG_UNFLAG          "md2 file of the key readable again: it is counted, and the key is not flagged for it"
#define MSG_ITER            "The history of the key is not whole: a md2 file of it could not be read when its cache was built"
#define MSG_SHAPE           "md2 file of the key ends in a whole row that is not on a row boundary: written by 7.25.4 after a torn row; not cut, repair it by hand"
#define MSG_TAIL_BAD        "md2 file of the key ends in a part of a row after a last whole row that is not valid: not cut, repair it by hand"
#define CAUSE_E_TOO_LARGE   "its content has no NUL but the one at its end, and this process has not the memory to parse it (MEM_MAX_BLOCK): it can be a record written by a yuno with a larger block"
#define CAUSE_L_TOO_LARGE   "this process has not the memory to parse its content (MEM_MAX_BLOCK): it cannot be checked"
#define MSG_MAX_BLOCK       "SIZE GREATER THAN MAX_BLOCK"
#define MSG_READ_NO_MEMORY  "Cannot read the record, this process has not the memory to parse its content (MEM_MAX_BLOCK)"
#define MSG_LIST            "Cannot load the whole history of a key of the list: the records read before the failure were handed, the list goes on with the next key"

/***************************************************************
 *              Data
 ***************************************************************/
PRIVATE yev_loop_h yev_loop;
PRIVATE char path_root[PATH_MAX];
PRIVATE char path_database[PATH_MAX];
PRIVATE char got[512];
PRIVATE int failed_content_opens = 0;   // the next opens for reading of A's content fail

/***************************************************************
 *              The open that fails
 ***************************************************************/
int __real_open(const char *path, int flags, ...);
int __wrap_open(const char *path, int flags, ...);

int __wrap_open(const char *path, int flags, ...)
{
    mode_t mode = 0;
    if(flags & (O_CREAT|O_TMPFILE)) {
        va_list ap;
        va_start(ap, flags);
        mode = (mode_t)va_arg(ap, int);
        va_end(ap);
    }
    size_t ln = strlen(path);
    if(failed_content_opens > 0 &&
            (flags & O_ACCMODE) == O_RDONLY &&
            strstr(path, FAULT_DIR) &&
            ln > 5 && strcmp(path + ln - 5, ".json") == 0) {
        failed_content_opens--;
        errno = EMFILE;
        return -1;
    }
    return __real_open(path, flags, mode);
}

/***************************************************************
 *              Helpers
 ***************************************************************/
PRIVATE int on_record(
    json_t *tranger,
    json_t *topic,
    const char *key,
    json_t *list,
    json_int_t rowid,
    md2_record_ex_t *md_record,
    json_t *record
)
{
    size_t ln = strlen(got);
    if(!record) {
        snprintf(got + ln, sizeof(got) - ln, "%s%s@NULL", ln? " ": "", key);
    } else {
        snprintf(got + ln, sizeof(got) - ln, "%s%s@%d", ln? " ": "", key,
            (int)kw_get_int(0, record, "v", 0, 0));
    }
    JSON_DECREF(record)
    return 0;
}

PRIVATE json_t *startup(void)
{
    return tranger2_startup(0, json_pack("{s:s, s:s, s:b, s:i, s:s}",
        "path", path_root,
        "database", DATABASE,
        "master", 1,
        "on_critical_error", LOG_OPT_TRACE_STACK,
        "filename_mask", "%Y-%m-%d"
    ), 0);
}

PRIVATE int append(json_t *tranger, const char *key, int v)
{
    md2_record_ex_t md = {0};
    return tranger2_append_record(tranger, TOPIC_NAME, (uint64_t)(DAY1 + v), 0, &md,
        json_pack("{s:s, s:I, s:i}", "id", key, "tm", (json_int_t)(DAY1 + v), "v", v)
    );
}

/*
 *  A store with A: rows 1, 2, 3 in the file of 2000-01-01, and B: row 1
 */
PRIVATE int build_store(void)
{
    rmrdir(path_database);
    set_expected_results("check fails: setup", NULL, NULL, NULL, 0);
    json_t *tranger = startup();
    if(!tranger || !tranger2_create_topic(
            tranger, TOPIC_NAME, "id", "tm", NULL, sf_string_key,
            json_pack("{s:s, s:I, s:I}", "id", "", "tm", (json_int_t)0, "v", (json_int_t)0),
            0)) {
        printf("%sERROR%s --> cannot create the store\n", On_Red BWhite, Color_Off);
        if(tranger) {
            tranger2_shutdown(tranger);
        }
        test_json(NULL);
        return -1;
    }
    append(tranger, "A", 1);
    append(tranger, "A", 2);
    append(tranger, "A", 3);
    append(tranger, "B", 1);
    tranger2_shutdown(tranger);
    test_json(NULL);    // the setup logs are not what is tested
    return 0;
}

PRIVATE void file_of_a(char *bf, size_t bfsize, const char *ext)
{
    char name[NAME_MAX];
    snprintf(name, sizeof(name), "2000-01-01.%s", ext);
    build_path(bf, bfsize, path_database, TOPIC_NAME, "keys", "A", name, NULL);
}

PRIVATE off_t size_of_a(const char *ext)
{
    char path[PATH_MAX];
    file_of_a(path, sizeof(path), ext);
    return filesize(path);
}

PRIVATE int resize_a(const char *ext, off_t size)
{
    char path[PATH_MAX];
    file_of_a(path, sizeof(path), ext);
    if(truncate(path, size) < 0) {
        printf("%sERROR%s --> cannot resize %s\n", On_Red BWhite, Color_Off, path);
        return -1;
    }
    return 0;
}

PRIVATE int expect(const char *what, const char *found, const char *expected)
{
    if(strcmp(found, expected) != 0) {
        printf("%sERROR%s --> %s: [%s], expected [%s]\n",
            On_Red BWhite, Color_Off, what, found, expected);
        return -1;
    }
    return 0;
}

PRIVATE int expect_size(const char *what, const char *ext, off_t expected)
{
    off_t found = size_of_a(ext);
    if(found != expected) {
        printf("%sERROR%s --> %s: %s size %ld, expected %ld\n",
            On_Red BWhite, Color_Off, what, ext, (long)found, (long)expected);
        return -1;
    }
    return 0;
}

/*
 *  The keyless list forward: what it hands, and whether it says load_failed
 */
PRIVATE int check_list(json_t *tranger, const char *what, const char *expected, BOOL load_failed)
{
    int result = 0;
    got[0] = 0;
    json_t *match_cond = json_pack("{s:I, s:b, s:I}",
        "to_rowid", (json_int_t)1000000,   // no realtime
        "backward", 0,
        "load_record_callback", (json_int_t)(uintptr_t)on_record
    );
    json_t *list = tranger2_open_list(tranger, TOPIC_NAME, match_cond, json_object(), "", FALSE, "");
    if(!list) {
        printf("%sERROR%s --> %s: list REFUSED\n", On_Red BWhite, Color_Off, what);
        return -1;
    }
    result += expect(what, got, expected);
    if(json_is_true(json_object_get(list, "load_failed")) != load_failed) {
        printf("%sERROR%s --> %s: load_failed is %s\n", On_Red BWhite, Color_Off, what,
            load_failed? "not set": "set");
        result += -1;
    }
    tranger2_close_list(tranger, list);
    return result;
}

/***************************************************************************
 *  1. The check cannot run at an append: refused, not flagged
 ***************************************************************************/
PRIVATE int test_append_check_cannot_run(void)
{
    int result = 0;
    if(build_store() < 0) {
        return -1;
    }

    set_expected_results("1. a running master", NULL, NULL, NULL, 1);
    json_t *tranger = startup();
    if(!tranger || !tranger2_open_topic(tranger, TOPIC_NAME, FALSE)) {
        printf("%sERROR%s --> 1: cannot open the store\n", On_Red BWhite, Color_Off);
        if(tranger) {
            tranger2_shutdown(tranger);
        }
        test_json(NULL);
        return -1;
    }
    result += test_json(NULL);

    /*
     *  A row written in part whose cut back failed: the md2 of the running
     *  master ends in a part of a row
     */
    if(resize_a("md2", 3*ROW + TORN) < 0) {
        tranger2_shutdown(tranger);
        return -1;
    }
    off_t content_size = size_of_a("json");

    set_expected_results("1. the check cannot run: the append is refused, the file is not flagged",
        json_pack("[{s:s, s:s, s:i},{s:s, s:s, s:s, s:i}]",
            "msg", MSG_NOT_CHECKED,
            "key", "A",
            "errno", EMFILE,
            "msg", MSG_APPEND_UNCHECKED,
            "key", "A",
            "file_id", "2000-01-01",
            "md2_size", 3*ROW + TORN
        ), NULL, NULL, 1
    );
    failed_content_opens = 1;
    if(append(tranger, "A", 4) == 0) {
        printf("%sERROR%s --> 1: the append was taken\n", On_Red BWhite, Color_Off);
        result += -1;
    }
    failed_content_opens = 0;
    result += test_json(NULL);
    result += expect_size("1. after the refused append", "md2", 3*ROW + TORN);
    result += expect_size("1. after the refused append", "json", content_size);

    set_expected_results("1. the whole rows are read, no load_failed", NULL, NULL, NULL, 1);
    result += check_list(tranger, "1. the list", "A@1 A@2 A@3 B@1", FALSE);
    result += test_json(NULL);

    set_expected_results("1. the next append checks again and cuts back",
        json_pack("[{s:s, s:s, s:s, s:i, s:i}]",
            "msg", MSG_CUT,
            "key", "A",
            "file_id", "2000-01-01",
            "old_size", 3*ROW + TORN,
            "new_size", 3*ROW
        ), NULL, NULL, 1
    );
    if(append(tranger, "A", 5) < 0) {
        printf("%sERROR%s --> 1: the next append was refused\n", On_Red BWhite, Color_Off);
        result += -1;
    }
    result += test_json(NULL);

    set_expected_results("1. every row is read", NULL, NULL, NULL, 1);
    result += expect_size("1. after the next append", "md2", 4*ROW);
    result += check_list(tranger, "1. the list after the next append", "A@1 A@2 A@3 A@5 B@1", FALSE);
    tranger2_shutdown(tranger);
    result += test_json(NULL);

    return result;
}

/***************************************************************************
 *  2. The check cannot run at an open: flagged, the next append counts
 ***************************************************************************/
PRIVATE int test_open_check_cannot_run(void)
{
    int result = 0;
    if(build_store() < 0 || resize_a("md2", 3*ROW + TORN) < 0) {
        return -1;
    }

    set_expected_results("2. the check cannot run at the open: the file is flagged",
        json_pack("[{s:s, s:s, s:i},{s:s, s:s, s:s}]",
            "msg", MSG_NOT_CHECKED,
            "key", "A",
            "errno", EMFILE,
            "msg", MSG_FLAG,
            "key", "A",
            "file_id", "2000-01-01"
        ), NULL, NULL, 1
    );
    failed_content_opens = 1;
    json_t *tranger = startup();
    json_t *topic = tranger? tranger2_open_topic(tranger, TOPIC_NAME, FALSE): NULL;
    failed_content_opens = 0;
    if(!topic) {
        printf("%sERROR%s --> 2: cannot open the store\n", On_Red BWhite, Color_Off);
        if(tranger) {
            tranger2_shutdown(tranger);
        }
        test_json(NULL);
        return -1;
    }
    result += test_json(NULL);
    result += expect_size("2. after the open", "md2", 3*ROW + TORN);

    set_expected_results("2. the load says load_failed",
        json_pack("[{s:s},{s:s}]",
            "msg", MSG_ITER,
            "msg", MSG_LIST
        ), NULL, NULL, 1
    );
    result += check_list(tranger, "2. the list", "B@1", TRUE);
    result += test_json(NULL);

    set_expected_results("2. the append counts the file again: cut back, unflagged",
        json_pack("[{s:s, s:s, s:i, s:i},{s:s, s:s, s:s}]",
            "msg", MSG_CUT,
            "file_id", "2000-01-01",
            "old_size", 3*ROW + TORN,
            "new_size", 3*ROW,
            "msg", MSG_UNFLAG,
            "key", "A",
            "file_id", "2000-01-01"
        ), NULL, NULL, 1
    );
    if(append(tranger, "A", 4) < 0) {
        printf("%sERROR%s --> 2: the append into the file was refused\n", On_Red BWhite, Color_Off);
        result += -1;
    }
    result += test_json(NULL);

    set_expected_results("2. every row is read", NULL, NULL, NULL, 1);
    result += expect_size("2. after the append", "md2", 4*ROW);
    result += check_list(tranger, "2. the list after the append", "A@1 A@2 A@3 A@4 B@1", FALSE);
    tranger2_shutdown(tranger);
    result += test_json(NULL);

    return result;
}

/***************************************************************************
 *  3. A candidate range larger than the largest memory block: not a record
 ***************************************************************************/
PRIVATE int test_candidate_larger_than_max_block(void)
{
    int result = 0;
    if(build_store() < 0) {
        return -1;
    }

    /*
     *  The content file made larger than the largest block (the bytes
     *  after its records are zeros), and a torn row of 31 bytes whose
     *  last 16 bytes put __offset__ 0 and __size__ S in the last 32 bytes
     *  of the md2: the range [0, S) is inside the content file and its
     *  last byte is a NUL.
     */
    uint64_t big = (uint64_t)gbmem_get_maximum_block() + 1;
    if(resize_a("json", (off_t)big + 64) < 0) {
        return -1;
    }
    char path[PATH_MAX];
    file_of_a(path, sizeof(path), "md2");
    int fd = open(path, O_WRONLY|O_APPEND|O_CLOEXEC, 0);
    if(fd < 0) {
        printf("%sERROR%s --> cannot open %s\n", On_Red BWhite, Color_Off, path);
        return -1;
    }
    char torn[31];
    memset(torn, 0, sizeof(torn));
    uint64_t size_be = htobe64(big);
    memcpy(torn + 23, &size_be, sizeof(size_be));  // bytes 24..31 of the last 32: __size__
    ssize_t ln = write(fd, torn, sizeof(torn));
    close(fd);
    if(ln != (ssize_t)sizeof(torn)) {
        printf("%sERROR%s --> cannot write the torn row\n", On_Red BWhite, Color_Off);
        return -1;
    }

    set_expected_results("3. the master cuts the torn row back",
        json_pack("[{s:s, s:s, s:s, s:i, s:i}]",
            "msg", MSG_CUT,
            "key", "A",
            "file_id", "2000-01-01",
            "old_size", 3*ROW + 31,
            "new_size", 3*ROW
        ), NULL, NULL, 1
    );
    json_t *tranger = startup();
    json_t *topic = tranger? tranger2_open_topic(tranger, TOPIC_NAME, FALSE): NULL;
    if(!topic) {
        printf("%sERROR%s --> 3: cannot open the store\n", On_Red BWhite, Color_Off);
        if(tranger) {
            tranger2_shutdown(tranger);
        }
        test_json(NULL);
        return -1;
    }
    result += test_json(NULL);

    set_expected_results("3. the key loads", NULL, NULL, NULL, 1);
    result += expect_size("3. after the open", "md2", 3*ROW);
    result += check_list(tranger, "3. the list", "A@1 A@2 A@3 B@1", FALSE);
    tranger2_shutdown(tranger);
    result += test_json(NULL);

    rmrdir(path_database);  // no big file left behind
    return result;
}

/***************************************************************************
 *  4. A last row larger than the largest memory block of the reader
 ***************************************************************************/
/*
 *  A record of `size` bytes in the content file (its text and the NUL):
 *  the "pad" string fills it
 */
PRIVATE json_t *padded_record(int v, size_t size)
{
    json_t *record = json_pack("{s:s, s:I, s:i, s:s}",
        "id", "A", "tm", (json_int_t)(DAY1 + v), "v", v, "pad", ""
    );
    char *text = json_dumps(record, JSON_COMPACT|JSON_ENCODE_ANY);
    size_t base = text? strlen(text) + 1: 0;
    GBMEM_FREE(text)
    if(base == 0 || size < base) {
        JSON_DECREF(record)
        return NULL;
    }
    char *pad = gbmem_malloc(size - base + 1);
    if(!pad) {
        JSON_DECREF(record)
        return NULL;
    }
    memset(pad, 'x', size - base);
    pad[size - base] = 0;
    json_object_set_new(record, "pad", json_string(pad));
    gbmem_free(pad);
    return record;
}

/*
 *  A record of about `size` bytes whose "pad" is an array of 1s: a text
 *  much smaller than the memory a parse of it takes (jansson keeps a
 *  table of pointers, and doubles it as it grows)
 */
PRIVATE json_t *array_record(int v, size_t size)
{
    json_t *record = json_pack("{s:s, s:I, s:i, s:[]}",
        "id", "A", "tm", (json_int_t)(DAY1 + v), "v", v, "pad"
    );
    char *text = json_dumps(record, JSON_COMPACT|JSON_ENCODE_ANY);
    size_t base = text? strlen(text) + 1: 0;
    GBMEM_FREE(text)
    json_t *pad = json_object_get(record, "pad");
    for(size_t ln = base; ln + 2 <= size; ln += 2) {
        json_array_append_new(pad, json_integer(1));
    }
    return record;
}

/*
 *  A's file: rows of 256 bytes, then one row of about `big` bytes when
 *  `big` is not 0: one long string, or with `as_array` an array. The rows
 *  are written by a process with the default memory block.
 */
PRIVATE BOOL big_as_array = FALSE;

PRIVATE int build_padded_store(int rows, size_t big)
{
    rmrdir(path_database);
    set_expected_results("check fails: padded setup", NULL, NULL, NULL, 0);
    json_t *tranger = startup();
    if(!tranger || !tranger2_create_topic(
            tranger, TOPIC_NAME, "id", "tm", NULL, sf_string_key,
            json_pack("{s:s, s:I, s:I, s:s}",
                "id", "", "tm", (json_int_t)0, "v", (json_int_t)0, "pad", ""),
            0)) {
        printf("%sERROR%s --> cannot create the padded store\n", On_Red BWhite, Color_Off);
        if(tranger) {
            tranger2_shutdown(tranger);
        }
        test_json(NULL);
        return -1;
    }
    int result = 0;
    for(int v = 1; v <= rows + (big? 1: 0); v++) {
        json_t *record = (v > rows && big_as_array)?
            array_record(v, big): padded_record(v, v <= rows? 256: big);
        md2_record_ex_t md = {0};
        if(!record || tranger2_append_record(
                tranger, TOPIC_NAME, (uint64_t)(DAY1 + v), 0, &md, record) < 0) {
            printf("%sERROR%s --> cannot append row %d\n", On_Red BWhite, Color_Off, v);
            result = -1;
            break;
        }
    }
    tranger2_shutdown(tranger);
    test_json(NULL);    // the setup logs are not what is tested
    return result;
}

PRIVATE int append_to_a(const char *ext, const char *bytes, size_t size)
{
    char path[PATH_MAX];
    file_of_a(path, sizeof(path), ext);
    int fd = open(path, O_WRONLY|O_APPEND|O_CLOEXEC, 0);
    if(fd < 0) {
        printf("%sERROR%s --> cannot open %s\n", On_Red BWhite, Color_Off, path);
        return -1;
    }
    ssize_t ln = write(fd, bytes, size);
    close(fd);
    if(ln != (ssize_t)size) {
        printf("%sERROR%s --> cannot write %s\n", On_Red BWhite, Color_Off, path);
        return -1;
    }
    return 0;
}

/*
 *  Open the store as a master whose largest memory block is `max_block`
 *  (MEM_MAX_BLOCK is set per yuno), and expect the file flagged, not cut
 */
PRIVATE int open_small_block_master(const char *what, size_t max_block, off_t md2_size, json_t *expected)
{
    int result = 0;
    size_t default_block = gbmem_get_maximum_block();
    set_expected_results(what, expected, NULL, NULL, 1);
    gbmem_setup(max_block, 0, 0, 0, 0);
    size_t tracking = max_block - gbmem_get_maximum_block();    // what memory tracking adds to a block
    json_t *tranger = startup();
    json_t *topic = tranger? tranger2_open_topic(tranger, TOPIC_NAME, FALSE): NULL;
    if(!topic) {
        printf("%sERROR%s --> %s: cannot open the store\n", On_Red BWhite, Color_Off, what);
        result += -1;
    }
    if(tranger) {
        tranger2_shutdown(tranger);
    }
    gbmem_setup(default_block + tracking, 0, 0, 0, 0);
    result += test_json(NULL);
    result += expect_size(what, "md2", md2_size);
    return result;
}

/*
 *  The shape 7.25.4 left, from a store of 5 rows: rows 1, 2, 3, the first
 *  31 bytes of row 4 (never acknowledged), then row 5 (acknowledged), and
 *  50 bytes after row 5's content that no row names. Row 4's content is
 *  256 bytes, so its last byte (0) is the first byte of row 5, and the
 *  whole row before the end reads as row 4: rule 2 holds. Only rule 1
 *  stops the cut.
 */
PRIVATE int make_shape_7254(const char *what)
{
    char rows[5*ROW];
    char path[PATH_MAX];
    file_of_a(path, sizeof(path), "md2");
    int fd = open(path, O_RDONLY|O_CLOEXEC, 0);
    ssize_t ln = fd < 0? -1: pread(fd, rows, sizeof(rows), 0);
    if(fd >= 0) {
        close(fd);
    }
    char junk[50];
    memset(junk, 'j', sizeof(junk));
    if(ln != (ssize_t)sizeof(rows) ||
            resize_a("md2", 3*ROW) < 0 ||
            append_to_a("md2", rows + 3*ROW, 31) < 0 ||
            append_to_a("md2", rows + 4*ROW, ROW) < 0 ||
            append_to_a("json", junk, sizeof(junk)) < 0) {
        printf("%sERROR%s --> %s: cannot make the shape\n", On_Red BWhite, Color_Off, what);
        return -1;
    }
    return 0;
}

PRIVATE int test_row_larger_than_max_block(void)
{
    int result = 0;
    size_t max_block = 64*1024;
    size_t big = 100*1024;

    /*
     *  a. The shape 7.25.4 left, and row 5's content is larger than the
     *  reader's memory block
     */
    if(build_padded_store(4, big) < 0 || make_shape_7254("4a") < 0) {
        return -1;
    }
    result += open_small_block_master(
        "4a. a last acknowledged row larger than the block: flagged, not cut",
        max_block,
        3*ROW + 31 + ROW,
        json_pack("[{s:s, s:s, s:s, s:i},{s:s, s:s, s:s}]",
            "msg", MSG_SHAPE,
            "cause", CAUSE_E_TOO_LARGE,
            "key", "A",
            "__size__", (int)big,
            "msg", MSG_FLAG,
            "key", "A",
            "file_id", "2000-01-01"
        )
    );

    /*
     *  b. A torn row after a last whole row larger than the reader's
     *  memory block: its content cannot be checked, so the tail is not cut
     */
    if(build_padded_store(3, big) < 0) {
        return -1;
    }
    char torn[TORN];
    memset(torn, 0, sizeof(torn));
    if(append_to_a("md2", torn, sizeof(torn)) < 0) {
        return -1;
    }
    result += open_small_block_master(
        "4b. a last whole row larger than the block: flagged, not cut",
        max_block,
        4*ROW + TORN,
        json_pack("[{s:s, s:s, s:s, s:i},{s:s, s:s, s:s}]",
            "msg", MSG_TAIL_BAD,
            "cause", CAUSE_L_TOO_LARGE,
            "key", "A",
            "__size__", (int)big,
            "msg", MSG_FLAG,
            "key", "A",
            "file_id", "2000-01-01"
        )
    );

    rmrdir(path_database);
    return result;
}

/***************************************************************************
 *  5. A content that fits in the block, and whose parse does not
 ***************************************************************************/
/*
 *  In a child, whose largest memory block is `max_block`: open the store
 *  as a master, and, when `list` is not NULL, load the keyless list, which
 *  must hand `list` and say load_failed. The log must be `expected`
 *  (owned). A crash of the child fails the test: the parent sees a signal,
 *  not an exit. The parent then checks the size of the md2.
 */
PRIVATE int open_in_child(
    const char *what,
    size_t max_block,
    off_t md2_size,
    const char *list,
    json_t *expected
)
{
    fflush(stdout);
    pid_t pid = fork();
    if(pid < 0) {
        printf("%sERROR%s --> %s: fork() failed\n", On_Red BWhite, Color_Off, what);
        JSON_DECREF(expected)
        return -1;
    }
    if(pid == 0) {
        int result = 0;
        set_expected_results(what, expected, NULL, NULL, 1);
        gbmem_setup(max_block, 0, 0, 0, 0);
        json_t *tranger = startup();
        json_t *topic = tranger? tranger2_open_topic(tranger, TOPIC_NAME, FALSE): NULL;
        if(!topic) {
            printf("%sERROR%s --> %s: cannot open the store\n", On_Red BWhite, Color_Off, what);
            result += -1;
        }
        if(topic && list) {
            result += check_list(tranger, what, list, TRUE);
        }
        result += test_json(NULL);
        fflush(stdout);
        _exit(result < 0? 1 : 0);  // no shutdown: the parent's store is what is checked
    }

    JSON_DECREF(expected)   // the child's copy is the one that is used
    int status = 0;
    waitpid(pid, &status, 0);
    char bf[64];
    if(WIFSIGNALED(status)) {
        snprintf(bf, sizeof(bf), "signal %d", WTERMSIG(status));
    } else {
        snprintf(bf, sizeof(bf), "exit %d", WIFEXITED(status)? WEXITSTATUS(status): -1);
    }
    int result = expect(what, bf, "exit 0");
    result += expect_size(what, "md2", md2_size);
    return result;
}

PRIVATE int test_content_that_cannot_be_parsed(void)
{
    int result = 0;
    /*
     *  The lexer's buffer for a string of 40 000 bytes grows to 64 KB. A
     *  block of 64 KB refused it only with CONFIG_DEBUG_TRACK_MEMORY (its
     *  header on top); without, the parse fit and 5a/5c failed.
     */
    size_t max_block = 48*1024;

    /*
     *  a. The shape 7.25.4 left, and row 5's content is one string of
     *  40 000 bytes: it fits in the reader's block, and its parse does not
     *  (the lexer doubles its buffer as the string grows). Before, the
     *  lexer ignored the failure and wrote past its buffer: a crash at
     *  every open. Now the parse fails for memory, and the file is
     *  flagged, not cut.
     */
    big_as_array = FALSE;
    if(build_padded_store(4, 40000) < 0 || make_shape_7254("5a") < 0) {
        return -1;
    }
    result += open_in_child(
        "5a. a last acknowledged row of one long string: no crash, flagged, not cut",
        max_block,
        3*ROW + 31 + ROW,
        NULL,
        json_pack("[{s:s},{s:s, s:s, s:s, s:i},{s:s, s:s, s:s}]",
            "msg", MSG_MAX_BLOCK,
            "msg", MSG_SHAPE,
            "cause", CAUSE_E_TOO_LARGE,
            "key", "A",
            "__size__", 40000,
            "msg", MSG_FLAG,
            "key", "A",
            "file_id", "2000-01-01"
        )
    );

    /*
     *  b. The same shape, and row 5's content is an array in 20 000 bytes:
     *  its parse takes a table of pointers larger than the block. Before,
     *  the failure was taken for "not a record", rule 1 passed, and the
     *  cut lost row 5 (acknowledged) and gave back row 4 (never
     *  acknowledged).
     */
    big_as_array = TRUE;
    if(build_padded_store(4, 20000) < 0 || make_shape_7254("5b") < 0) {
        big_as_array = FALSE;
        return -1;
    }
    big_as_array = FALSE;
    result += open_in_child(
        "5b. a last acknowledged row of a long array: flagged, not cut",
        max_block,
        3*ROW + 31 + ROW,
        NULL,
        json_pack("[{s:s},{s:s, s:s, s:s},{s:s, s:s, s:s}]",
            "msg", MSG_MAX_BLOCK,
            "msg", MSG_SHAPE,
            "cause", CAUSE_E_TOO_LARGE,
            "key", "A",
            "msg", MSG_FLAG,
            "key", "A",
            "file_id", "2000-01-01"
        )
    );

    /*
     *  c. The read of a record: rows 1, 2, then row 3 of one string of
     *  40 000 bytes, no torn row. The load reads rows 1 and 2, and the
     *  read of row 3 fails with a CRITICAL that says why: the list says
     *  load_failed. Before, the read crashed.
     */
    if(build_padded_store(2, 40000) < 0) {
        return -1;
    }
    result += open_in_child(
        "5c. a record that this process cannot parse: no crash, the read fails",
        max_block,
        3*ROW,
        "A@1 A@2",
        json_pack("[{s:s},{s:s, s:s, s:i},{s:s, s:s}]",
            "msg", MSG_MAX_BLOCK,
            "msg", MSG_READ_NO_MEMORY,
            "key", "A",
            "__size__", 40000,
            "msg", MSG_LIST,
            "key", "A"
        )
    );

    rmrdir(path_database);
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

    result += test_append_check_cannot_run();
    result += test_open_check_cannot_run();
    result += test_candidate_larger_than_max_block();
    result += test_row_larger_than_max_block();
    result += test_content_that_cannot_be_parsed();

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
