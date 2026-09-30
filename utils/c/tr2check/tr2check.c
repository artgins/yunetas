/****************************************************************************
 *          tr2check.c
 *
 *          Check a timeranger2 topic filled by a load test: count,
 *          duplicates, gaps, out-of-range sequences, checksums, storage
 *          rate and latency, in one pass per key.
 *
 *          An acceptance test is usually written in SQL over a table
 *          (COUNT(*), GROUP BY id HAVING COUNT(*) > 1, ...). tr2check
 *          answers the same questions on the topic, in its own format:
 *
 *          - Every record carries a sequence number (--seq-field, "seq" by
 *            default). With --seq-scope=key (the default) the sequence
 *            counts per key, the way a device numbers its own frames; with
 *            --seq-scope=topic it is one sequence for the whole topic.
 *            The sequences are collected, sorted and walked: a value seen
 *            twice is a duplicate, a jump is a gap, a value outside
 *            [--seq-from, --seq-to] is out of range. A record that arrives
 *            out of order is not a duplicate.
 *          - --expected=N is the number of distinct sequences the test
 *            sent. With it, `missing` also counts the losses at the END of
 *            a sequence, which a gap cannot see.
 *          - --checksum-field=F: the sha256 (64 hex chars) of the record
 *            without F and without `__md_tranger__`, dumped as compact
 *            JSON with sorted keys. The generator computes it the same way.
 *          - Latency: `__t__` (stored) minus `__tm__` (message time), in
 *            ms. Exact to the ms when the topic sets sf_t_ms and sf_tm_ms,
 *            to the second otherwise (`latency_resolution_ms` says which).
 *          - Rate: records / (last `__t__` - first `__t__`).
 *
 *          The result is ONE json document on stdout; logs go to stderr.
 *          Exit code: 0 every check passes, 1 a check failed, 2 the topic
 *          could not be checked.
 *
 *          Copyright (c) 2026, ArtGins.
 *          All Rights Reserved.
 ****************************************************************************/
#include <stdio.h>
#include <stdarg.h>
#include <limits.h>
#include <argp-standalone.h>
#include <time.h>
#include <signal.h>
#include <errno.h>
#include <stdint.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <regex.h>
#include <sys/resource.h>

#include <gobj.h>
#include <helpers.h>
#include <kwid.h>
#include <timeranger2.h>
#include <yev_loop.h>

/***************************************************************************
 *              Constants
 ***************************************************************************/
#define NAME        "tr2check"
#define DOC         "Check a topic filled by a load test: count, duplicates, gaps, " \
                    "checksums, storage rate and latency. The soft limit of open files " \
                    "is raised to the hard one, never the hard one itself (not even as " \
                    "root): raise it before the run for a topic with more keys."

#define VERSION     YUNETA_VERSION
#define SUPPORT     "<support at artgins.com>"
#define DATETIME    __DATE__ " " __TIME__

#define EXIT_PASS       0
#define EXIT_FAILED     1
#define EXIT_ERROR      2

#define LATENCY_HIST_MS 600000      // exact percentiles up to 10 minutes

/***************************************************************************
 *              Structures
 ***************************************************************************/
/*
 *  Used by main to communicate with parse_opt.
 */
#define MIN_ARGS 1
#define MAX_ARGS 1
struct arguments {
    char *args[MAX_ARGS+1];     /* positional args */

    char *path;
    char *key;
    char *rkey;
    char *seq_field;
    char *seq_scope;
    char *seq_from;
    char *seq_to;
    char *expected;
    char *checksum_field;
    char *from_t;
    char *to_t;
    char *list_max;
    int per_key;
};

typedef struct {
    json_int_t *v;
    size_t n;
    size_t size;
} seq_array_t;

typedef struct {
    /*
     *  Options
     */
    BOOL scope_topic;
    json_int_t seq_from;
    BOOL seq_to_set;
    json_int_t seq_to;
    BOOL expected_set;
    uint64_t expected;
    size_t list_max;
    BOOL t_ms;
    BOOL tm_ms;

    /*
     *  Totals
     */
    uint64_t keys;
    uint64_t records;
    uint64_t no_seq;
    uint64_t unique;
    uint64_t duplicated;
    uint64_t gaps;
    uint64_t out_of_range;
    uint64_t corrupted;
    uint64_t no_checksum;
    uint64_t unreadable_keys;
    BOOL load_aborted;      // a key's records could not all be kept: the check is incomplete

    BOOL seq_seen;
    json_int_t seq_min;
    json_int_t seq_max;

    BOOL t_seen;
    uint64_t first_t_ms;
    uint64_t last_t_ms;

    /*
     *  Latency, a histogram of 1 ms buckets
     */
    uint64_t *lat_hist;
    uint64_t lat_count;
    uint64_t lat_overflow;
    uint64_t lat_negative;
    double lat_sum_ms;
    uint64_t lat_max_ms;

    /*
     *  Examples, capped at list_max each
     */
    json_t *jn_duplicates;
    json_t *jn_gaps;
    json_t *jn_out_of_range;
    json_t *jn_corrupted;
    json_t *jn_unreadable;
    json_t *jn_per_key;

    /*
     *  The key being walked
     */
    const char *cur_key;
    uint64_t key_records;
    BOOL scope_seen;        // the last scope counted had an in-range seq
    json_int_t scope_min;
    json_int_t scope_max;
    seq_array_t seqs;
} check_t;

/***************************************************************************
 *              Prototypes
 ***************************************************************************/
PRIVATE void yuno_catch_signals(void);
static error_t parse_opt (int key, char *arg, struct argp_state *state);

/***************************************************************************
 *      Data
 ***************************************************************************/
const char *argp_program_version = NAME " " VERSION;
const char *argp_program_bug_address = SUPPORT;

/* Program documentation. */
static char doc[] = DOC;

/* A description of the arguments we accept. */
static char args_doc[] = "TOPIC_PATH";

/*
 *  The options we understand.
 *  See https://www.gnu.org/software/libc/manual/html_node/Argp-Option-Vectors.html
 */
static struct argp_option options[] = {
/*-name-----------------key-----arg-----------------flags---doc-----------------group */
{0,                     0,      0,                  0,      "Keys",             2},
{"key",                 'k',    "KEY",              0,      "Check only this key.", 2},
{"rkey",                1,      "REGEX",            0,      "Check only the keys matching this regular expression.", 2},

{0,                     0,      0,                  0,      "Sequence",         3},
{"seq-field",           2,      "FIELD",            0,      "Field with the sequence number (default: seq).", 3},
{"seq-scope",           3,      "SCOPE",            0,      "key: one sequence per key (default); topic: one for the whole topic.", 3},
{"seq-from",            4,      "N",                0,      "First valid sequence number (default: 1).", 3},
{"seq-to",              5,      "N",                0,      "Last valid sequence number (default: the highest seen).", 3},
{"expected",            6,      "N",                0,      "Distinct sequences the test sent (all keys together).", 3},

{0,                     0,      0,                  0,      "Integrity",        4},
{"checksum-field",      7,      "FIELD",            0,      "Field with the sha256 of the record (default: not checked).", 4},

{0,                     0,      0,                  0,      "Window",           5},
{"from-t",              8,      "TIME",             0,      "From storage time.", 5},
{"to-t",                9,      "TIME",             0,      "To storage time.", 5},

{0,                     0,      0,                  0,      "Output",           6},
{"list-max",            10,     "N",                0,      "Examples listed per defect (default: 20).", 6},
{"per-key",             11,     0,                  0,      "Add the figures of every key.", 6},

{0}
};

/* Our argp parser. */
static struct argp argp = {
    options,
    parse_opt,
    args_doc,
    doc,
    0,
    0,
    0
};

yev_loop_h yev_loop;
struct arguments arguments;
PRIVATE check_t check;

/***************************************************************************
 *  Parse a single option
 ***************************************************************************/
static error_t parse_opt(int key, char *arg, struct argp_state *state)
{
    /*
     *  Get the input argument from argp_parse,
     *  which we know is a pointer to our arguments structure.
     */
    struct arguments *arguments_ = state->input;

    switch (key) {
    case 'k':
        arguments_->key = arg;
        break;
    case 1:
        arguments_->rkey = arg;
        break;
    case 2:
        arguments_->seq_field = arg;
        break;
    case 3:
        arguments_->seq_scope = arg;
        break;
    case 4:
        arguments_->seq_from = arg;
        break;
    case 5:
        arguments_->seq_to = arg;
        break;
    case 6:
        arguments_->expected = arg;
        break;
    case 7:
        arguments_->checksum_field = arg;
        break;
    case 8:
        arguments_->from_t = arg;
        break;
    case 9:
        arguments_->to_t = arg;
        break;
    case 10:
        arguments_->list_max = arg;
        break;
    case 11:
        arguments_->per_key = 1;
        break;

    case ARGP_KEY_ARG:
        if (state->arg_num >= MAX_ARGS) {
            /* Too many arguments_. */
            argp_usage (state);
        }
        arguments_->args[state->arg_num] = arg;
        break;

    case ARGP_KEY_END:
        if (state->arg_num < MIN_ARGS) {
            /* Not enough arguments_. */
            argp_usage (state);
        }
        break;

    default:
        return ARGP_ERR_UNKNOWN;
    }
    return 0;
}

/***************************************************************************
 *  Logs go to stderr: stdout carries only the json result
 ***************************************************************************/
PRIVATE int stderr_write(void *h, int priority, const char *bf, size_t len)
{
    if(len > 0) {
        fwrite(bf, 1, len, stderr);
        fputc('\n', stderr);
    }
    return 0;
}

PRIVATE int stderr_fwrite(void *h, int priority, const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    vfprintf(stderr, fmt, ap);
    va_end(ap);
    fputc('\n', stderr);
    return 0;
}

/***************************************************************************
 *  Parse an integer option, or leave the tool
 ***************************************************************************/
PRIVATE json_int_t option_int(const char *name, const char *value)
{
    char *end = 0;
    errno = 0;
    long long v = strtoll(value, &end, 10);
    if(errno != 0 || !end || *end != 0 || end == value) {
        fprintf(stderr, "%s: --%s must be an integer, not '%s'\n", NAME, name, value);
        exit(EXIT_ERROR);
    }
    return (json_int_t)v;
}

/***************************************************************************
 *  An example of a defect, while the list has room
 ***************************************************************************/
PRIVATE void add_example(json_t *list, json_t *jn_example)
{
    if(json_array_size(list) < check.list_max) {
        json_array_append_new(list, jn_example);
    } else {
        JSON_DECREF(jn_example)
    }
}

/***************************************************************************
 *  A time of the topic, in ms
 ***************************************************************************/
PRIVATE uint64_t to_ms(uint64_t t, BOOL in_ms)
{
    return in_ms? t : t * 1000;
}

/***************************************************************************
 *  ISO 8601 UTC with milliseconds
 ***************************************************************************/
PRIVATE char *ms2iso(char *bf, size_t bfsize, uint64_t ms)
{
    time_t t = (time_t)(ms / 1000);
    struct tm tm;
    gmtime_r(&t, &tm);
    size_t len = strftime(bf, bfsize, "%Y-%m-%dT%H:%M:%S", &tm);
    if(len == 0 || bfsize - len < sizeof(".000Z")) {
        gobj_log_error(0, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_INTERNAL,
            "msg",          "%s", "Buffer too small for an ISO time",
            NULL
        );
        bf[0] = 0;
        return bf;
    }
    snprintf(bf + len, bfsize - len, ".%03uZ", (unsigned)(ms % 1000));
    return bf;
}

/***************************************************************************
 *  Keep a sequence of the current walk
 ***************************************************************************/
PRIVATE int seq_push(seq_array_t *a, json_int_t v)
{
    if(a->n >= a->size) {
        size_t new_size = a->size? a->size * 2 : 1024;
        json_int_t *p = gbmem_realloc(a->v, new_size * sizeof(json_int_t));
        if(!p) {
            // Error already logged
            return -1;
        }
        a->v = p;
        a->size = new_size;
    }
    a->v[a->n++] = v;
    return 0;
}

PRIVATE int cmp_seq(const void *a, const void *b)
{
    json_int_t x = *(const json_int_t *)a;
    json_int_t y = *(const json_int_t *)b;
    if(x < y) {
        return -1;
    }
    if(x > y) {
        return 1;
    }
    return 0;
}

/***************************************************************************
 *  Walk the sorted sequences of one scope (a key, or the whole topic)
 ***************************************************************************/
PRIVATE void count_sequences(const char *scope_key)
{
    seq_array_t *a = &check.seqs;
    if(a->n > 0) {
        qsort(a->v, a->n, sizeof(json_int_t), cmp_seq);
    }

    BOOL have_prev = FALSE;
    json_int_t prev = 0;
    check.scope_seen = FALSE;
    for(size_t i = 0; i < a->n; i++) {
        json_int_t s = a->v[i];
        if(s < check.seq_from || (check.seq_to_set && s > check.seq_to)) {
            check.out_of_range++;
            add_example(check.jn_out_of_range, json_pack("{s:s, s:I}",
                "key", scope_key,
                "seq", s
            ));
            continue;
        }
        if(have_prev && s == prev) {
            check.duplicated++;
            add_example(check.jn_duplicates, json_pack("{s:s, s:I}",
                "key", scope_key,
                "seq", s
            ));
            continue;
        }

        json_int_t expected_next = have_prev? prev + 1 : check.seq_from;
        if(s > expected_next) {
            check.gaps += (uint64_t)(s - expected_next);
            add_example(check.jn_gaps, json_pack("{s:s, s:I, s:I}",
                "key", scope_key,
                "from", expected_next,
                "to", s - 1
            ));
        }

        check.unique++;
        if(!check.seq_seen || s < check.seq_min) {
            check.seq_min = s;
        }
        if(!check.seq_seen || s > check.seq_max) {
            check.seq_max = s;
        }
        check.seq_seen = TRUE;
        if(!check.scope_seen) {
            check.scope_min = s;
            check.scope_seen = TRUE;
        }
        check.scope_max = s;
        have_prev = TRUE;
        prev = s;
    }

    if(check.seq_to_set) {
        json_int_t expected_next = have_prev? prev + 1 : check.seq_from;
        if(check.seq_to >= expected_next) {
            check.gaps += (uint64_t)(check.seq_to - expected_next + 1);
            add_example(check.jn_gaps, json_pack("{s:s, s:I, s:I}",
                "key", scope_key,
                "from", expected_next,
                "to", check.seq_to
            ));
        }
    }

    a->n = 0;
}

/***************************************************************************
 *  Is the checksum of the record the one it carries?
 ***************************************************************************/
PRIVATE BOOL checksum_matches(json_t *record, const char *field, BOOL *present)
{
    const char *stored = json_string_value(json_object_get(record, field));
    if(empty_string(stored)) {
        *present = FALSE;
        return FALSE;
    }
    *present = TRUE;

    json_t *copy = json_copy(record);
    json_object_del(copy, field);
    json_object_del(copy, "__md_tranger__");
    char *s = json_dumps(copy, JSON_COMPACT|JSON_SORT_KEYS);
    JSON_DECREF(copy)
    if(!s) {
        gobj_log_error(0, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_INTERNAL,
            "msg",          "%s", "Cannot dump a record to compute its checksum",
            NULL
        );
        return FALSE;
    }

    char hex[SHA256_HEX_LEN + 1];
    int ret = sha256_hex(s, strlen(s), hex, sizeof(hex));
    GBMEM_FREE(s)
    if(ret < 0) {
        // Error already logged
        return FALSE;
    }
    return strcasecmp(hex, stored) == 0;
}

/***************************************************************************
 *  One record of the key being walked
 ***************************************************************************/
PRIVATE int load_record_callback(
    json_t *tranger,
    json_t *topic,
    const char *key,
    json_t *list,       // iterator, don't own
    json_int_t rowid,
    md2_record_ex_t *md_record,
    json_t *record      // must be owned
)
{
    check.records++;
    check.key_records++;

    /*
     *  Storage time and latency
     */
    uint64_t t_ms = to_ms(md_record->__t__, check.t_ms);
    uint64_t tm_ms = to_ms(md_record->__tm__, check.tm_ms);
    if(!check.t_seen || t_ms < check.first_t_ms) {
        check.first_t_ms = t_ms;
    }
    if(!check.t_seen || t_ms > check.last_t_ms) {
        check.last_t_ms = t_ms;
    }
    check.t_seen = TRUE;

    if(tm_ms > t_ms) {
        check.lat_negative++;
    } else {
        uint64_t lat = t_ms - tm_ms;
        check.lat_count++;
        check.lat_sum_ms += (double)lat;
        if(lat > check.lat_max_ms) {
            check.lat_max_ms = lat;
        }
        if(lat <= LATENCY_HIST_MS) {
            check.lat_hist[lat]++;
        } else {
            check.lat_overflow++;
        }
    }

    /*
     *  Sequence
     */
    json_t *jn_seq = json_object_get(record, arguments.seq_field);
    if(!json_is_integer(jn_seq)) {
        check.no_seq++;
        add_example(check.jn_out_of_range, json_pack("{s:s, s:I, s:s}",
            "key", key,
            "rowid", rowid,
            "reason", "no sequence"
        ));
    } else if(seq_push(&check.seqs, json_integer_value(jn_seq)) < 0) {
        // Error already logged
        check.load_aborted = TRUE;
        JSON_DECREF(record)
        return -1;  // breaks the load: the verdict would be on a part of the key
    }

    /*
     *  Checksum
     */
    if(!empty_string(arguments.checksum_field)) {
        BOOL present = FALSE;
        if(!checksum_matches(record, arguments.checksum_field, &present)) {
            if(!present) {
                check.no_checksum++;
            } else {
                check.corrupted++;
            }
            add_example(check.jn_corrupted, json_pack("{s:s, s:I, s:O, s:s}",
                "key", key,
                "rowid", rowid,
                "seq", jn_seq? jn_seq : json_null(),
                "reason", present? "checksum mismatch" : "no checksum"
            ));
        }
    }

    JSON_DECREF(record)
    return 0;
}

/***************************************************************************
 *  Walk one key
 ***************************************************************************/
PRIVATE void check_key(json_t *tranger, const char *topic_name, const char *key)
{
    uint64_t dup0 = check.duplicated;
    uint64_t gaps0 = check.gaps;
    uint64_t oor0 = check.out_of_range;
    uint64_t corr0 = check.corrupted + check.no_checksum;
    uint64_t unique0 = check.unique;

    check.keys++;
    check.cur_key = key;
    check.key_records = 0;

    json_t *match_cond = json_object();
    if(arguments.from_t) {
        json_object_set_new(match_cond, "from_t", json_string(arguments.from_t));
    }
    if(arguments.to_t) {
        json_object_set_new(match_cond, "to_t", json_string(arguments.to_t));
    }

    json_t *iterator = tranger2_open_iterator(
        tranger,
        topic_name,
        key,
        match_cond,             // owned
        load_record_callback,
        NULL,                   // iterator id: the key
        NAME,                   // creator
        NULL,                   // data
        NULL                    // extra
    );
    if(!iterator || json_is_true(json_object_get(iterator, "load_failed"))) {
        // Error already logged by timeranger2
        check.unreadable_keys++;
        add_example(check.jn_unreadable, json_string(key));
    }
    if(iterator) {
        tranger2_close_iterator(tranger, iterator);
    }

    if(!check.scope_topic) {
        count_sequences(key);
    }

    if(check.jn_per_key) {
        json_t *jn_key = json_pack("{s:s, s:I, s:I, s:I, s:I, s:I, s:I}",
            "key", key,
            "records", (json_int_t)check.key_records,
            "unique", (json_int_t)(check.unique - unique0),
            "duplicated", (json_int_t)(check.duplicated - dup0),
            "gaps", (json_int_t)(check.gaps - gaps0),
            "out_of_range", (json_int_t)(check.out_of_range - oor0),
            "corrupted", (json_int_t)(check.corrupted + check.no_checksum - corr0)
        );
        if(!check.scope_topic && check.scope_seen) {
            json_object_set_new(jn_key, "seq_min", json_integer(check.scope_min));
            json_object_set_new(jn_key, "seq_max", json_integer(check.scope_max));
        }
        json_array_append_new(check.jn_per_key, jn_key);
    }
}

/***************************************************************************
 *  Latency percentile from the histogram, in ms
 ***************************************************************************/
PRIVATE json_int_t latency_percentile(double p)
{
    if(check.lat_count == 0) {
        return 0;
    }
    uint64_t rank = (uint64_t)((p / 100.0) * (double)check.lat_count + 0.999999);
    if(rank < 1) {
        rank = 1;
    }
    uint64_t acc = 0;
    for(uint64_t i = 0; i <= LATENCY_HIST_MS; i++) {
        acc += check.lat_hist[i];
        if(acc >= rank) {
            return (json_int_t)i;
        }
    }
    return (json_int_t)check.lat_max_ms;  // in the overflow: the max bounds it
}

/***************************************************************************
 *  Check the topic
 ***************************************************************************/
PRIVATE int check_topic(char *topic_path)
{
    if(!file_exists(topic_path, "topic_desc.json")) {
        fprintf(stderr, "%s: not a timeranger2 topic: '%s'\n", NAME, topic_path);
        return EXIT_ERROR;
    }

    /*
     *  The database and the directory above it are taken from the path, so
     *  the path must name them: `db/topic` or a bare `topic` have none.
     */
    char path[PATH_MAX];
    if(!realpath(topic_path, path)) {
        fprintf(stderr, "%s: cannot resolve the path '%s': %s\n", NAME, topic_path, strerror(errno));
        return EXIT_ERROR;
    }
    char *topic_name = pop_last_segment(path);
    char *database = pop_last_segment(path);
    if(empty_string(topic_name) || empty_string(database) || database == path) {
        fprintf(stderr, "%s: a topic lives in <directory>/<database>/<topic>, not in '%s'\n",
            NAME, topic_path);
        return EXIT_ERROR;
    }
    if(empty_string(path)) {
        snprintf(path, sizeof(path), "/");  // a database right under the root
    }

    json_t *tranger = tranger2_startup(0, json_pack("{s:s, s:s}",
        "path", path,
        "database", database
    ), 0);
    if(!tranger) {
        fprintf(stderr, "%s: cannot start up timeranger2 at '%s/%s'\n", NAME, path, database);
        return EXIT_ERROR;
    }

    json_t *topic = tranger2_open_topic(tranger, topic_name, FALSE);
    if(!topic) {
        fprintf(stderr, "%s: cannot open the topic '%s'\n", NAME, topic_name);
        tranger2_shutdown(tranger);
        return EXIT_ERROR;
    }

    system_flag2_t system_flag = (system_flag2_t)json_integer_value(
        json_object_get(topic, "system_flag")
    );
    check.t_ms = (system_flag & sf_t_ms)? TRUE : FALSE;
    check.tm_ms = (system_flag & sf_tm_ms)? TRUE : FALSE;

    json_t *jn_keys = tranger2_list_keys(tranger, topic_name);
    size_t idx;
    json_t *jn_key;
    json_array_foreach(jn_keys, idx, jn_key) {
        const char *key = json_string_value(jn_key);
        if(arguments.key && strcmp(key, arguments.key) != 0) {
            continue;
        }
        if(arguments.rkey && !str_match_regex(key, arguments.rkey, REG_EXTENDED|REG_NOSUB)) {
            continue;
        }
        check_key(tranger, topic_name, key);
        if(check.load_aborted) {
            fprintf(stderr, "%s: the records of key '%s' could not all be kept in memory: the check is incomplete\n",
                NAME, key);
            break;
        }
    }
    JSON_DECREF(jn_keys)

    if(check.load_aborted) {
        tranger2_close_topic(tranger, topic_name);
        tranger2_shutdown(tranger);
        return EXIT_ERROR;
    }

    if(check.scope_topic) {
        count_sequences("*");
    }

    tranger2_close_topic(tranger, topic_name);
    tranger2_shutdown(tranger);
    return 0;
}

/***************************************************************************
 *  The result, and whether it passes
 ***************************************************************************/
PRIVATE int print_result(const char *topic_path)
{
    char first[64] = "";
    char last[64] = "";
    double duration_s = 0;
    double rate = 0;
    if(check.t_seen) {
        ms2iso(first, sizeof(first), check.first_t_ms);
        ms2iso(last, sizeof(last), check.last_t_ms);
        duration_s = (double)(check.last_t_ms - check.first_t_ms) / 1000.0;
        if(duration_s > 0) {
            rate = (double)check.records / duration_s;
        }
    }

    uint64_t missing = check.gaps;
    if(check.expected_set) {
        missing = check.expected > check.unique? check.expected - check.unique : 0;
    }

    json_t *jn_result = json_object();
    json_object_set_new(jn_result, "topic", json_string(topic_path));
    json_object_set_new(jn_result, "seq_field", json_string(arguments.seq_field));
    json_object_set_new(jn_result, "seq_scope", json_string(check.scope_topic? "topic" : "key"));
    json_object_set_new(jn_result, "keys", json_integer((json_int_t)check.keys));
    json_object_set_new(jn_result, "records", json_integer((json_int_t)check.records));
    json_object_set_new(jn_result, "unique", json_integer((json_int_t)check.unique));
    if(check.expected_set) {
        json_object_set_new(jn_result, "expected", json_integer((json_int_t)check.expected));
    }
    json_object_set_new(jn_result, "duplicated", json_integer((json_int_t)check.duplicated));
    json_object_set_new(jn_result, "gaps", json_integer((json_int_t)check.gaps));
    json_object_set_new(jn_result, "missing", json_integer((json_int_t)missing));
    json_object_set_new(jn_result, "out_of_range", json_integer((json_int_t)check.out_of_range));
    json_object_set_new(jn_result, "no_seq", json_integer((json_int_t)check.no_seq));
    if(!empty_string(arguments.checksum_field)) {
        json_object_set_new(jn_result, "corrupted", json_integer((json_int_t)check.corrupted));
        json_object_set_new(jn_result, "no_checksum", json_integer((json_int_t)check.no_checksum));
    }
    json_object_set_new(jn_result, "unreadable_keys", json_integer((json_int_t)check.unreadable_keys));
    if(check.seq_seen) {
        json_object_set_new(jn_result, "seq_min", json_integer(check.seq_min));
        json_object_set_new(jn_result, "seq_max", json_integer(check.seq_max));
    }
    if(check.t_seen) {
        json_object_set_new(jn_result, "first_t", json_string(first));
        json_object_set_new(jn_result, "last_t", json_string(last));
        json_object_set_new(jn_result, "duration_s", json_real(duration_s));
        json_object_set_new(jn_result, "rate", json_real(rate));
    }
    json_object_set_new(jn_result, "latency_resolution_ms",
        json_integer((check.t_ms && check.tm_ms)? 1 : 1000)
    );
    json_object_set_new(jn_result, "latency_ms", json_pack("{s:f, s:I, s:I, s:I, s:I, s:I, s:I}",
        "mean", check.lat_count? check.lat_sum_ms / (double)check.lat_count : 0.0,
        "p50", latency_percentile(50),
        "p90", latency_percentile(90),
        "p95", latency_percentile(95),
        "p99", latency_percentile(99),
        "max", (json_int_t)check.lat_max_ms,
        "negative", (json_int_t)check.lat_negative
    ));

    BOOL pass = check.records > 0 &&
        check.duplicated == 0 &&
        check.gaps == 0 &&
        missing == 0 &&
        check.out_of_range == 0 &&
        check.no_seq == 0 &&
        check.corrupted == 0 &&
        check.no_checksum == 0 &&
        check.unreadable_keys == 0 &&
        (!check.expected_set || check.unique == check.expected);
    json_object_set_new(jn_result, "result", json_string(pass? "PASS" : "FAIL"));

    json_object_set_new(jn_result, "examples", json_pack("{s:O, s:O, s:O, s:O, s:O}",
        "duplicated", check.jn_duplicates,
        "gaps", check.jn_gaps,
        "out_of_range", check.jn_out_of_range,
        "corrupted", check.jn_corrupted,
        "unreadable_keys", check.jn_unreadable
    ));
    if(check.jn_per_key) {
        json_object_set(jn_result, "per_key", check.jn_per_key);
    }

    char *s = json_dumps(jn_result, JSON_INDENT(4));
    if(s) {
        printf("%s\n", s);
        GBMEM_FREE(s)
    } else {
        fprintf(stderr, "%s: cannot dump the result\n", NAME);
        JSON_DECREF(jn_result)
        return EXIT_ERROR;
    }
    JSON_DECREF(jn_result)

    return pass? EXIT_PASS : EXIT_FAILED;
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE void delete_right_slash(char *s)
{
    int l = (int)strlen(s);
    while(--l > 0) {
        char c = s[l];
        if(c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '/') {
            s[l] = '\0';
        } else {
            break;
        }
    }
}

/***************************************************************************
 *                      Main
 ***************************************************************************/
int main(int argc, char *argv[])
{
    /*
     *  Default values
     */
    memset(&arguments, 0, sizeof(arguments));
    arguments.seq_field = "seq";
    arguments.seq_scope = "key";

    /*
     *  Parse arguments
     */
    argp_parse(&argp, argc, argv, 0, 0, &arguments);
    arguments.path = arguments.args[0];

    /*----------------------------------*
     *      Startup gobj system
     *----------------------------------*/
    sys_malloc_fn_t malloc_func;
    sys_realloc_fn_t realloc_func;
    sys_calloc_fn_t calloc_func;
    sys_free_fn_t free_func;

    gbmem_get_allocators(
        &malloc_func,
        &realloc_func,
        &calloc_func,
        &free_func
    );

    json_set_alloc_funcs(
        malloc_func,
        free_func
    );

#ifndef CONFIG_BUILD_TYPE_RELEASE
    init_backtrace_with_backtrace(argv[0]);
    set_show_backtrace_fn(show_backtrace_with_backtrace);
#endif

    /*
     *  The sequences of a whole topic go in one array (--seq-scope=topic):
     *  8 bytes per record, so the largest block may take most of the memory.
     */
    uint64_t MEM_MAX_SYSTEM_MEMORY = free_ram_in_kb() * 1024LL;
    MEM_MAX_SYSTEM_MEMORY /= 100LL;
    MEM_MAX_SYSTEM_MEMORY *= 90LL;  // 90% of the free memory

    gbmem_setup(
        MEM_MAX_SYSTEM_MEMORY,  // max_block, largest memory block
        MEM_MAX_SYSTEM_MEMORY,  // max_system_memory, maximum system memory
        FALSE,
        0,
        0
    );
    gobj_start_up(
        argc,
        argv,
        NULL, // jn_global_settings
        NULL, // persistent_attrs
        NULL, // global_command_parser
        NULL, // global_stats_parser
        NULL, // global_authz_checker
        NULL  // global_authentication_parser
    );

    yuno_catch_signals();

    /*--------------------------------*
     *      Log handlers
     *--------------------------------*/
    gobj_log_register_handler("stderr", 0, stderr_write, stderr_fwrite);
    gobj_log_add_handler("stderr", "stderr", LOG_OPT_UP_WARNING, 0);

    /*--------------------------------*
     *      Options
     *--------------------------------*/
    memset(&check, 0, sizeof(check));
    if(strcmp(arguments.seq_scope, "topic") == 0) {
        check.scope_topic = TRUE;
    } else if(strcmp(arguments.seq_scope, "key") != 0) {
        fprintf(stderr, "%s: --seq-scope must be 'key' or 'topic', not '%s'\n",
            NAME, arguments.seq_scope);
        exit(EXIT_ERROR);
    }
    check.seq_from = arguments.seq_from? option_int("seq-from", arguments.seq_from) : 1;
    if(arguments.seq_to) {
        check.seq_to_set = TRUE;
        check.seq_to = option_int("seq-to", arguments.seq_to);
    }
    if(arguments.expected) {
        json_int_t expected = option_int("expected", arguments.expected);
        if(expected < 0) {
            fprintf(stderr, "%s: --expected cannot be negative\n", NAME);
            exit(EXIT_ERROR);
        }
        check.expected_set = TRUE;
        check.expected = (uint64_t)expected;
    }
    check.list_max = 20;
    if(arguments.list_max) {
        json_int_t list_max = option_int("list-max", arguments.list_max);
        check.list_max = list_max > 0? (size_t)list_max : 0;
    }

    check.lat_hist = gbmem_calloc(LATENCY_HIST_MS + 1, sizeof(uint64_t));
    check.jn_duplicates = json_array();
    check.jn_gaps = json_array();
    check.jn_out_of_range = json_array();
    check.jn_corrupted = json_array();
    check.jn_unreadable = json_array();
    if(arguments.per_key) {
        check.jn_per_key = json_array();
    }

    char topic_path[PATH_MAX];
    snprintf(topic_path, sizeof(topic_path), "%s", arguments.path);
    delete_right_slash(topic_path);

    /*
     *  timeranger2 keeps the files of every key open: the soft limit goes up
     *  to the hard one. The hard one is left alone, root included: it is
     *  the operator's to raise.
     */
    struct rlimit rl;
    if(getrlimit(RLIMIT_NOFILE, &rl) < 0) {
        fprintf(stderr, "%s: getrlimit(RLIMIT_NOFILE) failed: %s\n", NAME, strerror(errno));
    } else if(rl.rlim_cur < rl.rlim_max) {
        rl.rlim_cur = rl.rlim_max;
        if(setrlimit(RLIMIT_NOFILE, &rl) < 0) {
            fprintf(stderr, "%s: cannot raise the limit of open files to %llu: %s\n",
                NAME, (unsigned long long)rl.rlim_max, strerror(errno));
        }
    }

    /*--------------------------------*
     *  Create the event loop
     *--------------------------------*/
    yev_loop_create(
        NULL,
        2024,
        10,
        NULL,
        &yev_loop
    );

    int exit_code = EXIT_ERROR;
    if(!check.lat_hist) {
        // Error already logged
    } else if(check_topic(topic_path) == 0) {
        exit_code = print_result(topic_path);
    }

    yev_loop_stop(yev_loop);
    yev_loop_destroy(yev_loop);

    GBMEM_FREE(check.seqs.v)
    GBMEM_FREE(check.lat_hist)
    JSON_DECREF(check.jn_duplicates)
    JSON_DECREF(check.jn_gaps)
    JSON_DECREF(check.jn_out_of_range)
    JSON_DECREF(check.jn_corrupted)
    JSON_DECREF(check.jn_unreadable)
    JSON_DECREF(check.jn_per_key)

    gobj_end();

    return exit_code;
}

/***************************************************************************
 *      Signal handlers
 ***************************************************************************/
PRIVATE void quit_sighandler(int sig)
{
    exit(EXIT_ERROR);
}

PRIVATE void yuno_catch_signals(void)
{
    struct sigaction sigIntHandler;

    signal(SIGPIPE, SIG_IGN);

    memset(&sigIntHandler, 0, sizeof(sigIntHandler));
    sigIntHandler.sa_handler = quit_sighandler;
    sigemptyset(&sigIntHandler.sa_mask);
    sigIntHandler.sa_flags = SA_NODEFER|SA_RESTART;
    sigaction(SIGQUIT, &sigIntHandler, NULL);
    sigaction(SIGINT, &sigIntHandler, NULL);    // ctrl+c
    sigaction(SIGTERM, &sigIntHandler, NULL);
}
