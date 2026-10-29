/****************************************************************************
 *          test_date_bound_units.c
 *
 *  A time bound given as a DATE ("2000-01-02T00:00:00+0000") is read in
 *  seconds and taken to the unit of its axis: milliseconds for `t` on an
 *  sf_t_ms topic, for `tm` on an sf_tm_ms one. Up to 7.27.4 get_segments()
 *  asked sf_tm_ms for the `t` bounds too: on a topic with t in ms and tm in
 *  seconds a date in from_t/to_t was compared as seconds against ms (it
 *  selected every row, or none), and the reverse on a topic with tm in ms.
 *  An END bound takes its whole second: `to_t` 23:59:59 holds a record at
 *  23:59:59.500 of an axis in ms (it was 23:59:59.000, and lost it).
 *
 *  A text time, as a bound and as a tkey, is read by ONE rule,
 *  tranger2_time_of_text() (REV-CLOUD V46.3, V47.2, V48): "" is no time
 *  (no bound, no tkey value); the text of a JSON integer,
 *  -?(0|[1-9][0-9]*), is that integer in the unit of its axis; a text
 *  written as another number (" 5", "5 ", "10.5", "007", "+5", "1E5", or
 *  with a decimal comma, "1,5", or digits in groups, "5 5") is refused; any other text is a date, and
 *  one that is no date is refused. A json string is judged whole
 *  (tranger2_time_of_json_text()): one with a NUL inside is refused,
 *  "5\u0000junk" was the tkey 5. Each refusal logs
 *  a WARNING. Up to 7.29.3 approxidate() read a text that is no date as
 *  NOW in silence, and a bound with no 'T' was cut by strtoll() ("garbage"
 *  was no bound, "2000-01-02 00:00:00 +0000" the time 2000, " 5" 5, "10.5"
 *  10). R46 read " 5" and "10.5" as dates (the 5th, May 10) in silence,
 *  refused "" (no bound until then) and every integer text in a tkey
 *  ("1761000000000" was a bound, and no tkey), and took "007" as 7.
 *
 *          Copyright (c) 2026, ArtGins.
 *          All Rights Reserved.
 ****************************************************************************/
#include <string.h>
#include <signal.h>
#include <limits.h>
#include <unistd.h>

#include <gobj.h>
#include <kwid.h>
#include <timeranger2.h>
#include <helpers.h>
#include <yev_loop.h>
#include <testing.h>

#define APP "test_date_bound_units"

#define DATABASE    "tr_date_bound_units"
#define TOPIC_T_MS  "topic_t_ms"    // t in ms, tm in seconds
#define TOPIC_TM_MS "topic_tm_ms"   // t in seconds, tm in ms
#define DAY1        946684800       // 2000-01-01
#define DAY_SECS    86400

/***************************************************************
 *              Data
 ***************************************************************/
PRIVATE yev_loop_h yev_loop;

/***************************************************************
 *              Prototypes
 ***************************************************************/
PRIVATE int expect(const char *what, const char *got, const char *expected);

/***************************************************************
 *              Helpers
 ***************************************************************/
PRIVATE json_t *create_topic(json_t *tranger, const char *topic_name, system_flag2_t system_flag)
{
    return tranger2_create_topic(
        tranger, topic_name, "id", "tm",
        json_pack("{s:s}", "filename_mask", "%Y-%m-%d"),
        system_flag,
        json_pack("{s:s, s:I, s:s}", "id", "", "tm", (json_int_t)0, "content", ""),
        0
    );
}

/*
 *  One record of day `d` (0, 1, 2), its t and its tm in the units of the topic
 */
PRIVATE void append_day(json_t *tranger, const char *topic_name, BOOL t_ms, BOOL tm_ms, int d, const char *content)
{
    uint64_t secs = DAY1 + (uint64_t)d*DAY_SECS + 3600;
    json_t *record = json_pack("{s:I, s:I, s:s}",
        "id", (json_int_t)1,
        "tm", (json_int_t)(tm_ms? secs*1000 : secs),
        "content", content
    );
    md2_record_ex_t md = {0};
    tranger2_append_record(tranger, topic_name, t_ms? secs*1000 : secs, 0, &md, record);
}

/*
 *  What a load of the key serves with `cond` (owned), as "<content> ..."
 */
PRIVATE void served(json_t *tranger, const char *topic_name, json_t *cond, char *bf, size_t bfsize)
{
    bf[0] = 0;
    json_t *data = json_array();
    json_t *it = tranger2_open_iterator(
        tranger, topic_name, "0000000000000000001", cond, NULL, "served", "", data, NULL
    );
    int idx; json_t *record;
    json_array_foreach(data, idx, record) {
        if(bf[0]) {
            strncat(bf, " ", bfsize - strlen(bf) - 1);
        }
        strncat(bf, kw_get_str(0, record, "content", "?", 0), bfsize - strlen(bf) - 1);
    }
    if(it) {
        tranger2_close_iterator(tranger, it);
    }
    JSON_DECREF(data)
}

PRIVATE int load_nothing(
    json_t *tranger,
    json_t *topic,
    const char *key,
    json_t *list,
    json_int_t rowid,
    md2_record_ex_t *md_record_ex,
    json_t *jn_record
)
{
    JSON_DECREF(jn_record)
    return 0;
}

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
    char bf[256];
    char path_root[PATH_MAX];
    char path_database[PATH_MAX];
    build_path(path_root, sizeof(path_root), getenv("HOME"), "tests_yuneta", NULL);
    build_path(path_database, sizeof(path_database), path_root, DATABASE, NULL);
    rmrdir(path_database);
    mkrdir(path_root, 02770);

    set_expected_results(
        "date bounds: setup",
        json_pack("[{s:s},{s:s},{s:s}]",
            "msg", "Creating __timeranger2__.json",
            "msg", "Creating topic",
            "msg", "Creating topic"
        ),
        NULL, NULL, 1
    );
    json_t *tranger = tranger2_startup(0,
        json_pack("{s:s, s:s, s:b, s:i}",
            "path", path_root,
            "database", DATABASE,
            "master", 1,
            "on_critical_error", LOG_OPT_TRACE_STACK
        ),
        yev_loop
    );
    if(!tranger ||
            !create_topic(tranger, TOPIC_T_MS, sf_int_key|sf_t_ms) ||
            !create_topic(tranger, TOPIC_TM_MS, sf_int_key|sf_tm_ms)) {
        if(tranger) {
            tranger2_shutdown(tranger);
        }
        return -1;
    }
    const char *contents[] = {"D1", "D2", "D3"};
    for(int d = 0; d < 3; d++) {
        append_day(tranger, TOPIC_T_MS, TRUE, FALSE, d, contents[d]);
        append_day(tranger, TOPIC_TM_MS, FALSE, TRUE, d, contents[d]);
        if(d == 1) {
            /*  The last half second of day 2, on the axis in ms  */
            uint64_t last = DAY1 + 2*DAY_SECS - 1;
            md2_record_ex_t md = {0};
            tranger2_append_record(tranger, TOPIC_T_MS, last*1000 + 500, 0, &md,
                json_pack("{s:I, s:I, s:s}", "id", (json_int_t)1, "tm", (json_int_t)last, "content", "D2b")
            );
            tranger2_append_record(tranger, TOPIC_TM_MS, last, 0, &md,
                json_pack("{s:I, s:I, s:s}", "id", (json_int_t)1, "tm", (json_int_t)(last*1000 + 500), "content", "D2b")
            );
        }
    }
    result += test_json(NULL);

    set_expected_results("date bounds: each axis in its own unit", NULL, NULL, NULL, 1);
    const char *topics[] = {TOPIC_T_MS, TOPIC_TM_MS, NULL};
    for(int i = 0; topics[i]; i++) {
        char what[128];
        served(tranger, topics[i],
            json_pack("{s:s, s:s}",
                "from_t", "2000-01-02T00:00:00+0000",
                "to_t", "2000-01-02T23:59:59+0000"
            ),
            bf, sizeof(bf)
        );
        snprintf(what, sizeof(what), "%s: a day by t", topics[i]);
        result += expect(what, bf, "D2 D2b");

        served(tranger, topics[i],
            json_pack("{s:s, s:s}",
                "from_tm", "2000-01-02T00:00:00+0000",
                "to_tm", "2000-01-02T23:59:59+0000"
            ),
            bf, sizeof(bf)
        );
        snprintf(what, sizeof(what), "%s: a day by tm", topics[i]);
        result += expect(what, bf, "D2 D2b");

        served(tranger, topics[i],
            json_pack("{s:s}", "from_t", "2000-01-02T00:00:00+0000"),
            bf, sizeof(bf)
        );
        snprintf(what, sizeof(what), "%s: from a day by t", topics[i]);
        result += expect(what, bf, "D2 D2b D3");
    }
    result += test_json(NULL);

    /*
     *  A bound that is no date is refused, the iterator or the feed not
     *  opened; a date with no 'T' is a date, an integer text that integer
     */
    set_expected_results(
        "date bounds: a text that is no date is refused",
        json_pack("[{s:s},{s:s},{s:s},{s:s}]",
            "msg", "match_cond refused: a time bound is no date",
            "msg", "match_cond refused: a time bound is no date",
            "msg", "match_cond refused: a time bound is no date",
            "msg", "match_cond refused: a time bound is no date"
        ),
        NULL, NULL, 1
    );
    {
        served(tranger, TOPIC_T_MS, json_pack("{s:s}", "from_t", "garbage"), bf, sizeof(bf));
        result += expect("from_t 'garbage'", bf, "");

        served(tranger, TOPIC_TM_MS, json_pack("{s:s}", "to_tm", "12abc"), bf, sizeof(bf));
        result += expect("to_tm '12abc'", bf, "");

        served(tranger, TOPIC_T_MS,
            json_pack("{s:s}", "from_t", "2000-01-02 00:00:00 +0000"), bf, sizeof(bf));
        result += expect("from_t, a date with no 'T'", bf, "D2 D2b D3");

        served(tranger, TOPIC_T_MS,
            json_pack("{s:s}", "from_t", "946771200000"), bf, sizeof(bf));
        result += expect("from_t, an integer text", bf, "D2 D2b D3");

        json_t *mem = tranger2_open_rt_mem(tranger, TOPIC_T_MS, "",
            json_pack("{s:s}", "from_t", "garbage"), load_nothing, "mem", "", NULL);
        if(mem) {
            printf("%sERROR%s --> an rt_mem opened with from_t 'garbage'\n", On_Red BWhite, Color_Off);
            tranger2_close_rt_mem(tranger, mem);
            result += -1;
        }

        json_t *list = tranger2_open_list(tranger, TOPIC_T_MS,
            json_pack("{s:s, s:I}",
                "to_t", "garbage",
                "load_record_callback", (json_int_t)(size_t)load_nothing
            ),
            NULL, "", FALSE, ""
        );
        if(list) {
            printf("%sERROR%s --> a list opened with to_t 'garbage'\n", On_Red BWhite, Color_Off);
            tranger2_close_list(tranger, list);
            result += -1;
        }
    }
    result += test_json(NULL);

    /*
     *  A bound written as a number that is not the text of a JSON integer
     *  is refused: R46 read " 5", "5 " and "10.5" as dates (0 rows here, in
     *  silence; up to 7.29.3 strtoll() made them 5 and 10), and "007" as 7
     */
    {
        const char *refused[] = {
            " 5", "5 ", "10.5", "007", "+5", "1E5", "-007",
            "1,5", "10,5", " 1,5 ", "-1,5",     // V48: approxidate() read "1,5" as May 1
            "5 5", "2026 10 01", NULL           // V48: May 5, January 10 of 2026
        };
        json_t *expected = json_array();
        for(int i = 0; refused[i]; i++) {
            for(int j = 0; j < 2; j++) {   // the iterator, then the list
                json_array_append_new(expected,
                    json_pack("{s:s}", "msg", "match_cond refused: a time bound is no date"));
            }
        }
        set_expected_results(
            "date bounds: a number that is not a JSON integer is refused",
            expected, NULL, NULL, 1
        );
        for(int i = 0; refused[i]; i++) {
            char what[64];
            served(tranger, TOPIC_T_MS, json_pack("{s:s}", "from_t", refused[i]), bf, sizeof(bf));
            snprintf(what, sizeof(what), "from_t '%s'", refused[i]);
            result += expect(what, bf, "");

            json_t *list = tranger2_open_list(tranger, TOPIC_T_MS,
                json_pack("{s:s, s:I}",
                    "to_t", refused[i],
                    "load_record_callback", (json_int_t)(size_t)load_nothing
                ),
                NULL, "", FALSE, ""
            );
            if(list) {
                printf("%sERROR%s --> a list opened with to_t '%s'\n", On_Red BWhite, Color_Off, refused[i]);
                tranger2_close_list(tranger, list);
                result += -1;
            }
        }
    }
    result += test_json(NULL);

    /*
     *  A bound with a NUL inside is refused, judged whole (V48): its C text
     *  stops at the NUL, and "<a date>\u0000garbage" was that date
     */
    {
        static const char nul_date[] = "2000-01-02T00:00:00+0000\0garbage";
        set_expected_results(
            "date bounds: a text with a NUL inside is refused",
            json_pack("[{s:s}, {s:s}]",
                "msg", "match_cond refused: a time bound is no date",
                "msg", "match_cond refused: a time bound is no date"
            ),
            NULL, NULL, 1
        );
        served(tranger, TOPIC_T_MS,
            json_pack("{s:s%}", "from_t", nul_date, sizeof(nul_date) - 1), bf, sizeof(bf));
        result += expect("from_t '<date>\\0garbage'", bf, "");

        json_t *list = tranger2_open_list(tranger, TOPIC_T_MS,
            json_pack("{s:s%, s:I}",
                "to_t", nul_date, sizeof(nul_date) - 1,
                "load_record_callback", (json_int_t)(size_t)load_nothing
            ),
            NULL, "", FALSE, ""
        );
        if(list) {
            printf("%sERROR%s --> a list opened with to_t '<date>\\0garbage'\n", On_Red BWhite, Color_Off);
            tranger2_close_list(tranger, list);
            result += -1;
        }
    }
    result += test_json(NULL);

    /*
     *  "" is no bound (as up to 7.29.3; R46 refused it), "-0" is 0 (no
     *  bound), "1761000000000" that integer: no WARNING
     */
    set_expected_results("date bounds: '', '-0' and an integer text", NULL, NULL, NULL, 1);
    {
        served(tranger, TOPIC_T_MS, json_pack("{s:s}", "from_t", ""), bf, sizeof(bf));
        result += expect("from_t ''", bf, "D1 D2 D2b D3");

        served(tranger, TOPIC_T_MS, json_pack("{s:s}", "to_t", ""), bf, sizeof(bf));
        result += expect("to_t ''", bf, "D1 D2 D2b D3");

        served(tranger, TOPIC_TM_MS, json_pack("{s:s, s:s}", "from_tm", "-0", "to_tm", ""), bf, sizeof(bf));
        result += expect("from_tm '-0', to_tm ''", bf, "D1 D2 D2b D3");

        served(tranger, TOPIC_T_MS, json_pack("{s:s}", "to_t", "1761000000000"), bf, sizeof(bf));
        result += expect("to_t '1761000000000'", bf, "D1 D2 D2b D3");
    }
    result += test_json(NULL);

    /*
     *  A text tkey that is no date is refused, nothing written: "garbage"
     *  was NOW*1000 in milliseconds up to 7.29.3
     */
    set_expected_results(
        "date bounds: a text tkey that is no date is refused",
        json_pack("[{s:s}]",
            "msg", "Cannot append record, its tkey is a text that is no date"
        ),
        NULL, NULL, 1
    );
    {
        const char *refused[] = {"garbage"};
        for(int i = 0; i < 1; i++) {
            md2_record_ex_t md = {0};
            int ret = tranger2_append_record(tranger, TOPIC_TM_MS, 0, 0, &md,
                json_pack("{s:I, s:s, s:s}", "id", (json_int_t)2, "tm", refused[i], "content", "X")
            );
            if(ret >= 0) {
                printf("%sERROR%s --> tkey '%s' appended, __tm__ %" PRIu64 "\n",
                    On_Red BWhite, Color_Off, refused[i], (uint64_t)md.__tm__);
                result += -1;
            }
        }

        json_t *jn = json_pack("{s:I, s:s}", "id", (json_int_t)2, "tm", "garbage");
        BOOL fits = TRUE;
        tranger2_record_tkey_time(tranger2_topic(tranger, TOPIC_TM_MS), jn, &fits);
        if(fits) {
            printf("%sERROR%s --> tkey 'garbage' fits\n", On_Red BWhite, Color_Off);
            result += -1;
        }
        JSON_DECREF(jn)

        md2_record_ex_t md = {0};
        int ret = tranger2_append_record(tranger, TOPIC_TM_MS, 0, 0, &md,
            json_pack("{s:I, s:s, s:s}", "id", (json_int_t)2, "tm", "2000-01-03T12:00:00+0000", "content", "Y")
        );
        if(ret < 0 || md.__tm__ != 946900800000ULL) {
            printf("%sERROR%s --> tkey of a date: ret %d, __tm__ %" PRIu64 "\n",
                On_Red BWhite, Color_Off, ret, (uint64_t)md.__tm__);
            result += -1;
        }
        if(tranger2_topic_key_size(tranger, TOPIC_TM_MS, "0000000000000000002") != 1) {
            printf("%sERROR%s --> key 2 has %d rows, expected 1\n", On_Red BWhite, Color_Off,
                (int)tranger2_topic_key_size(tranger, TOPIC_TM_MS, "0000000000000000002"));
            result += -1;
        }
    }
    result += test_json(NULL);

    /*
     *  A text tkey written as a number that is not the text of a JSON
     *  integer is refused, as a bound is: R46 appended " 5", "5 ", "10.5"
     *  and "007" as dates (the 5th, May 10, the 7th), in silence
     */
    {
        const char *refused[] = {" 5", "5 ", "10.5", "007", "1,5", "10,5", "garbage", NULL};
        json_t *expected = json_array();
        for(int i = 0; refused[i]; i++) {
            json_array_append_new(expected,
                json_pack("{s:s}", "msg", "Cannot append record, its tkey is a text that is no date"));
        }
        set_expected_results(
            "date bounds: a text tkey that is a number but no JSON integer is refused",
            expected, NULL, NULL, 1
        );
        for(int i = 0; refused[i]; i++) {
            json_t *jn = json_pack("{s:I, s:s}", "id", (json_int_t)3, "tm", refused[i]);
            BOOL fits = TRUE;
            tranger2_record_tkey_time(tranger2_topic(tranger, TOPIC_TM_MS), jn, &fits);
            if(fits) {
                printf("%sERROR%s --> tkey '%s' fits\n", On_Red BWhite, Color_Off, refused[i]);
                result += -1;
            }
            JSON_DECREF(jn)

            /*
             *  The last one with a large content: the trace shows the first
             *  bytes of the record, it dumped the whole of it (V47.2)
             */
            json_t *record = json_pack("{s:I, s:s, s:s}", "id", (json_int_t)3, "tm", refused[i], "content", "X");
            if(!refused[i+1]) {
                char *big = gbmem_malloc(64*1024 + 1);
                memset(big, 'x', 64*1024);
                big[64*1024] = 0;
                json_object_set_new(record, "content", json_string(big));
                gbmem_free(big);
            }
            md2_record_ex_t md = {0};
            int ret = tranger2_append_record(tranger, TOPIC_TM_MS, 0, 0, &md, record);
            if(ret >= 0) {
                printf("%sERROR%s --> tkey '%s' appended, __tm__ %" PRIu64 "\n",
                    On_Red BWhite, Color_Off, refused[i], (uint64_t)md.__tm__);
                result += -1;
            }
        }
    }
    result += test_json(NULL);

    /*
     *  A text tkey with a NUL inside is refused, judged whole (V48):
     *  "5\u0000junk" was appended with __tm__ 5
     */
    set_expected_results(
        "date bounds: a text tkey with a NUL inside is refused",
        json_pack("[{s:s}]",
            "msg", "Cannot append record, its tkey is a text that is no date"
        ),
        NULL, NULL, 1
    );
    {
        static const char nul_tkey[] = "5\0junk";
        json_t *jn = json_pack("{s:I, s:s%}",
            "id", (json_int_t)3, "tm", nul_tkey, sizeof(nul_tkey) - 1);
        BOOL fits = TRUE;
        tranger2_record_tkey_time(tranger2_topic(tranger, TOPIC_TM_MS), jn, &fits);
        if(fits) {
            printf("%sERROR%s --> tkey '5\\0junk' fits\n", On_Red BWhite, Color_Off);
            result += -1;
        }
        JSON_DECREF(jn)

        md2_record_ex_t md = {0};
        int ret = tranger2_append_record(tranger, TOPIC_TM_MS, 0, 0, &md,
            json_pack("{s:I, s:s%, s:s}",
                "id", (json_int_t)3, "tm", nul_tkey, sizeof(nul_tkey) - 1, "content", "X")
        );
        if(ret >= 0) {
            printf("%sERROR%s --> tkey '5\\0junk' appended, __tm__ %" PRIu64 "\n",
                On_Red BWhite, Color_Off, (uint64_t)md.__tm__);
            result += -1;
        }
    }
    result += test_json(NULL);

    /*
     *  The text of a JSON integer is that integer, in the unit of the tm
     *  axis, as a bound is: R46 refused "1761000000000" (ms) and "-0". ""
     *  is no tkey value, as a missing one: 0
     */
    set_expected_results("date bounds: a text tkey that is a JSON integer, or ''", NULL, NULL, NULL, 1);
    {
        static const struct {
            const char *text;
            uint64_t tm;
        } taken[] = {
            {"1761000000000",   1761000000000ULL},
            {"-0",              0},
            {"",                0},
        };
        for(size_t i = 0; i < sizeof(taken)/sizeof(taken[0]); i++) {
            md2_record_ex_t md = {0};
            int ret = tranger2_append_record(tranger, TOPIC_TM_MS, 0, 0, &md,
                json_pack("{s:I, s:s, s:s}", "id", (json_int_t)4, "tm", taken[i].text, "content", "Z")
            );
            if(ret < 0 || md.__tm__ != taken[i].tm) {
                printf("%sERROR%s --> tkey '%s': ret %d, __tm__ %" PRIu64 ", expected %" PRIu64 "\n",
                    On_Red BWhite, Color_Off, taken[i].text, ret, (uint64_t)md.__tm__, taken[i].tm);
                result += -1;
            }
        }
        if(tranger2_topic_key_size(tranger, TOPIC_TM_MS, "0000000000000000003") != 0) {
            printf("%sERROR%s --> key 3 has %d rows, expected 0\n", On_Red BWhite, Color_Off,
                (int)tranger2_topic_key_size(tranger, TOPIC_TM_MS, "0000000000000000003"));
            result += -1;
        }
        if(tranger2_topic_key_size(tranger, TOPIC_TM_MS, "0000000000000000004") != 3) {
            printf("%sERROR%s --> key 4 has %d rows, expected 3\n", On_Red BWhite, Color_Off,
                (int)tranger2_topic_key_size(tranger, TOPIC_TM_MS, "0000000000000000004"));
            result += -1;
        }
    }
    result += test_json(NULL);

    /*
     *  The rule itself, tranger2_time_of_text()
     */
    set_expected_results("date bounds: tranger2_time_of_text()", NULL, NULL, NULL, 1);
    {
        static const struct {
            const char *text;
            BOOL in_ms;
            BOOL end;
            int ret;
            json_int_t value;
        } cases[] = {
            {"",                            FALSE,  FALSE,   0, 0},
            {"0",                           FALSE,  FALSE,   0, 0},
            {"-0",                          FALSE,  FALSE,   0, 0},
            {"5",                           TRUE,   TRUE,    0, 5},
            {"-86400",                      FALSE,  FALSE,   0, -86400},
            {"1761000000000",               TRUE,   FALSE,   0, 1761000000000LL},
            {"9223372036854775807",         FALSE,  FALSE,   0, INT64_MAX},
            {"9223372036854775808",         FALSE,  FALSE,  -2, 0},
            {" 5",                          FALSE,  FALSE,  -1, 0},
            {"5 ",                          FALSE,  FALSE,  -1, 0},
            {"\t5\n",                       FALSE,  FALSE,  -1, 0},
            {"10.5",                        FALSE,  FALSE,  -1, 0},
            {"007",                         FALSE,  FALSE,  -1, 0},
            {"-007",                        FALSE,  FALSE,  -1, 0},
            {"+5",                          FALSE,  FALSE,  -1, 0},
            {".5",                          FALSE,  FALSE,  -1, 0},
            {"5.",                          FALSE,  FALSE,  -1, 0},
            {"1E5",                         FALSE,  FALSE,  -1, 0},
            {"2.0",                         FALSE,  FALSE,  -1, 0},
            {"0x10",                        FALSE,  FALSE,  -1, 0},
            {"1,5",                         FALSE,  FALSE,  -1, 0},
            {"10,5",                        FALSE,  FALSE,  -1, 0},
            {" 1,5 ",                       FALSE,  FALSE,  -1, 0},
            {"-1,5",                        FALSE,  FALSE,  -1, 0},
            {"+1,5",                        FALSE,  FALSE,  -1, 0},
            {"5 5",                         FALSE,  FALSE,  -1, 0},
            {"10 5",                        FALSE,  FALSE,  -1, 0},
            {" 2026 10 01 ",                FALSE,  FALSE,  -1, 0},
            {"-5 5",                        FALSE,  FALSE,  -1, 0},
            {"1700000000 +0000",            FALSE,  FALSE,   0, 1700000000},
            {"garbage",                     FALSE,  FALSE,  -1, 0},
            {"2000-01-02T00:00:00+0000",    FALSE,  FALSE,   0, 946771200},
            {"2000-01-02 00:00:00 +0000",   TRUE,   FALSE,   0, 946771200000LL},
            {"2000-01-02T00:00:00+0000",    TRUE,   TRUE,    0, 946771200999LL},
            {" 2000-01-02T00:00:00+0000 ",  FALSE,  FALSE,   0, 946771200},
            {"@9300000000000000 +0000",     TRUE,   FALSE,  -2, 0},
            {"@9300000000000000 +0000",     FALSE,  FALSE,   0, 9300000000000000LL},
        };
        for(size_t i = 0; i < sizeof(cases)/sizeof(cases[0]); i++) {
            json_int_t value = -12345;
            const char *why = NULL;
            int ret = tranger2_time_of_text(cases[i].text, cases[i].in_ms, cases[i].end, &value, &why);
            if(ret != cases[i].ret || (ret == 0 && value != cases[i].value) ||
                    (ret < 0 && empty_string(why)) || (ret == 0 && why)) {
                printf("%sERROR%s --> tranger2_time_of_text('%s'): %d %" JSON_INTEGER_FORMAT
                    " (%s), expected %d %" JSON_INTEGER_FORMAT "\n",
                    On_Red BWhite, Color_Off, cases[i].text, ret, value, why? why : "",
                    cases[i].ret, cases[i].value);
                result += -1;
            }
        }
    }
    result += test_json(NULL);

    /*
     *  tranger2_time_of_json_text(): the whole of a json string (V48)
     */
    set_expected_results("date bounds: tranger2_time_of_json_text()", NULL, NULL, NULL, 1);
    {
        static const char nul_tkey[] = "5\0junk";
        static const char nul_date[] = "2000-01-02T00:00:00+0000\0garbage";
        static const char nul_end[] = "5\0";
        struct {
            const char *what;
            json_t *jn;
            int ret;
            json_int_t value;
        } cases[] = {
            {"'5'",                     json_string("5"),                                   0, 5},
            {"''",                      json_string(""),                                    0, 0},
            {"a date",                  json_string("2000-01-02T00:00:00+0000"),            0, 946771200},
            {"'5\\0junk'",            json_stringn(nul_tkey, sizeof(nul_tkey) - 1),      -1, 0},
            {"'<date>\\0garbage'",    json_stringn(nul_date, sizeof(nul_date) - 1),      -1, 0},
            {"'5\\0'",                json_stringn(nul_end, sizeof(nul_end) - 1),        -1, 0},
            {"'1,5'",                   json_string("1,5"),                                -1, 0},
            {"an integer",              json_integer(5),                                   -1, 0},
            {"NULL",                    NULL,                                              -1, 0},
        };
        for(size_t i = 0; i < sizeof(cases)/sizeof(cases[0]); i++) {
            json_int_t value = -12345;
            const char *why = NULL;
            int ret = tranger2_time_of_json_text(cases[i].jn, FALSE, FALSE, &value, &why);
            if(ret != cases[i].ret || (ret == 0 && value != cases[i].value) ||
                    (ret < 0 && empty_string(why)) || (ret == 0 && why)) {
                printf("%sERROR%s --> tranger2_time_of_json_text(%s): %d %" JSON_INTEGER_FORMAT
                    " (%s), expected %d %" JSON_INTEGER_FORMAT "\n",
                    On_Red BWhite, Color_Off, cases[i].what, ret, value, why? why : "",
                    cases[i].ret, cases[i].value);
                result += -1;
            }
            JSON_DECREF(cases[i].jn)
        }
    }
    result += test_json(NULL);

    set_expected_results("date bounds: shutdown", NULL, NULL, NULL, 1);
    tranger2_shutdown(tranger);
    for(int i = 0; i < 10; i++) {
        yev_loop_run_once(yev_loop);
    }
    result += test_json(NULL);

    return result;
}

/***************************************************************************
 *              Main
 ***************************************************************************/
int main(int argc, char *argv[])
{
    sys_malloc_fn_t malloc_func;
    sys_free_fn_t free_func;
    gbmem_get_json_allocators(&malloc_func, &free_func);
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
