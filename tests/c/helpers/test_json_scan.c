/****************************************************************************
 *          test_json_scan.c
 *
 *          json_scan_object_members() (kernel/c/gobj-c/src/json_scan.c)
 *          must give json_loadb()'s verdict on every text: a text it
 *          accepts is one jansson accepts as an object, and the other way
 *          round. A store that takes the text of a record without parsing
 *          it trusts this verdict; a text it lets through that jansson
 *          refuses is a record nobody can read back.
 *
 *          Cases:
 *          1. jansson's own corpus (test/suites/valid, invalid and
 *             invalid-unicode of jansson 2.15, MIT, copied below), each
 *             text as it is and as the value of a member, {"k": <text>};
 *          2. the edges jansson refuses or takes: integer and real ranges
 *             (the real overflow threshold, digit by digit), surrogates,
 *             \u0000 with and without JSON_ALLOW_NUL, nesting depth;
 *          3. the members: their type, their value as text, a repeated
 *             key (the LAST counts, as in jansson), an escaped key, and
 *             the cut that leaves a valid object without the member;
 *          4. a fuzz: mutations of valid objects, every verdict compared
 *             with json_loadb(), every accepted text's members compared
 *             with what jansson parsed. The one difference allowed: a raw
 *             NUL byte right after a number or a literal, which
 *             json_loadb() skips and the scan refuses (json_loads()
 *             refuses it too: it ends there);
 *          5. bad parameters: -1, logged.
 *
 *          Copyright (c) 2026, ArtGins.
 *          All Rights Reserved.
 ****************************************************************************/
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <yunetas.h>

#define APP     "test_json_scan"

PRIVATE int global_result = 0;
PRIVATE int n_mismatches_shown = 0;
PRIVATE int n_skipped_nul = 0;

/***************************************************************
 *  jansson 2.15 test/suites, the input of each case
 *  (recursion-depth is built in test_edges())
 ***************************************************************/
typedef struct {
    const char *name;
    const char *text;
    size_t len;
} corpus_case_t;

PRIVATE const corpus_case_t corpus[] = {
    {"valid/complex-array", "[1,2,3,4,\n\"a\", \"b\", \"c\",\n{\"foo\": \"bar\", \"core\": \"dump\"},\ntrue, false, true, true, null, false\n]\n", 96},
    {"valid/empty-array", "[]\n", 3},
    {"valid/empty-object", "{}\n", 3},
    {"valid/empty-object-in-array", "[{}]\n", 5},
    {"valid/empty-string", "[\"\"]\n", 5},
    {"valid/escaped-utf-control-char", "[\"\\u0012 escaped control character\"]\n", 37},
    {"valid/false", "[false]\n", 8},
    {"valid/negative-int", "[-123]\n", 7},
    {"valid/negative-one", "[-1]\n", 5},
    {"valid/negative-zero", "[-0]\n", 5},
    {"valid/null", "[null]\n", 7},
    {"valid/one-byte-utf-8", "[\"\\u002c one-byte UTF-8\"]\n", 26},
    {"valid/real-capital-e", "[1E22]\n", 7},
    {"valid/real-capital-e-negative-exponent", "[1E-2]\n", 7},
    {"valid/real-capital-e-positive-exponent", "[1E+2]\n", 7},
    {"valid/real-exponent", "[1.23e47, 0.1, 0.3, 9.99]\n", 26},
    {"valid/real-exponent-no-dtoa", "[1.23e47, 0.1, 0.3, 9.99]\n", 26},
    {"valid/real-fraction-exponent", "[123.456e78]\n", 13},
    {"valid/real-negative-exponent", "[1e-2]\n", 7},
    {"valid/real-positive-exponent", "[1e+2]\n", 7},
    {"valid/real-subnormal-number", "[1.8011670033376514e-308]\n", 26},
    {"valid/real-underflow", "[123e-10000000]\n", 16},
    {"valid/short-string", "[\"a\"]\n", 6},
    {"valid/simple-ascii-string", "[\"abcdefghijklmnopqrstuvwxyz1234567890 \"]\n", 42},
    {"valid/simple-int-0", "[0]\n", 4},
    {"valid/simple-int-1", "[1]\n", 4},
    {"valid/simple-int-123", "[123]\n", 6},
    {"valid/simple-object", "{\"a\":[]}\n", 9},
    {"valid/simple-real", "[123.456789]\n", 13},
    {"valid/string-escapes", "[\"\\\"\\\\\\/\\b\\f\\n\\r\\t\"]\n", 21},
    {"valid/three-byte-utf-8", "[\"\\u0821 three-byte UTF-8\"]\n", 28},
    {"valid/true", "[true]\n", 7},
    {"valid/two-byte-utf-8", "[\"\\u0123 two-byte UTF-8\"]\n", 26},
    {"valid/utf-8-string", "[\"\xe2\x82\xac\xc3\xbe\xc4\xb1\xc5\x93\xc9\x99\xc3\x9f\xc3\xb0 some utf-8 \xc4\xb8\xca\x92\xc3\x97\xc5\x8b\xc2\xb5\xc3\xa5\xc3\xa4\xc3\xb6\xf0\x9d\x84\x9e\"]\n", 52},
    {"valid/utf-surrogate-four-byte-encoding", "[\"\\uD834\\uDD1E surrogate, four-byte UTF-8\"]\n", 44},
    {"invalid/apostrophe", "['\n", 3},
    {"invalid/ascii-unicode-identifier", "a\xc3\xa5\n", 4},
    {"invalid/brace-comma", "{,\n", 3},
    {"invalid/bracket-comma", "[,\n", 3},
    {"invalid/bracket-one-comma", "[1,\n", 4},
    {"invalid/empty", "", 0},
    {"invalid/extra-comma-in-array", "[1,]\n", 5},
    {"invalid/extra-comma-in-multiline-array", "[1,\n2,\n3,\n4,\n5,\n]\n", 18},
    {"invalid/garbage-after-newline", "[1,2,3]\nfoo\n", 12},
    {"invalid/garbage-at-the-end", "[1,2,3]foo\n", 11},
    {"invalid/integer-starting-with-zero", "[012]\n", 6},
    {"invalid/invalid-escape", "[\"\\a <-- invalid escape\"]\n", 26},
    {"invalid/invalid-identifier", "[troo\n", 6},
    {"invalid/invalid-negative-integer", "[-123foo]\n", 10},
    {"invalid/invalid-negative-real", "[-123.123foo]\n", 14},
    {"invalid/invalid-second-surrogate", "[\"\\uD888\\u3210 (first surrogate and invalid second surrogate)\"]\n", 64},
    {"invalid/invalid-unicode-escape", "[\"\\uqqqq <-- invalid unicode escape\"]\n", 38},
    {"invalid/lone-open-brace", "{\n", 2},
    {"invalid/lone-open-bracket", "[\n", 2},
    {"invalid/lone-second-surrogate", "[\"\\uDFAA (second surrogate on it's own)\"]\n", 42},
    {"invalid/minus-sign-without-number", "[-foo]\n", 7},
    {"invalid/negative-integer-starting-with-zero", "[-012]\n", 7},
    {"invalid/null", "null\n", 5},
    {"invalid/null-byte-in-object-key", "{\"foo\\u0000bar\": 42}", 20},
    {"invalid/null-byte-in-string", "[\"null byte \x00 not allowed\"]\n", 28},
    {"invalid/null-byte-outside-string", "[\x00\n", 3},
    {"invalid/null-escape-in-string", "[\"null escape \\u0000 not allowed\"]\n", 35},
    {"invalid/object-apostrophes", "{'a'\n", 5},
    {"invalid/object-garbage-at-end", "{\"a\":\"a\" 123}\n", 14},
    {"invalid/object-in-unterminated-array", "[{}\n", 4},
    {"invalid/object-no-colon", "{\"a\"\n", 5},
    {"invalid/object-no-value", "{\"a\":\n", 6},
    {"invalid/object-unterminated-value", "{\"a\":\"a\n", 8},
    {"invalid/real-garbage-after-e", "[1ea]\n", 6},
    {"invalid/real-negative-overflow", "[-123123e100000]\n", 17},
    {"invalid/real-positive-overflow", "[123123e100000]\n", 16},
    {"invalid/real-truncated-at-e", "[1e]\n", 5},
    {"invalid/real-truncated-at-point", "[1.]\n", 5},
    {"invalid/tab-character-in-string", "[\"\t <-- tab character\"]\n", 24},
    {"invalid/too-big-negative-integer", "[-123123123123123123123123123123]\n", 34},
    {"invalid/too-big-positive-integer", "[123123123123123123123123123123]\n", 33},
    {"invalid/truncated-unicode-surrogate", "[\"\\uDADA (first surrogate without the second)\"]\n", 48},
    {"invalid/unicode-identifier", "\xc3\xa5\n", 3},
    {"invalid/unterminated-array", "[\"a\"\n", 5},
    {"invalid/unterminated-array-and-object", "[{\n", 3},
    {"invalid/unterminated-empty-key", "{\"\n", 3},
    {"invalid/unterminated-key", "{\"a\n", 4},
    {"invalid/unterminated-object-and-array", "{[\n", 3},
    {"invalid/unterminated-string", "[\"a\n", 4},
    {"invalid-unicode/encoded-surrogate-half", "[\"\xed\xa2\xab <-- encoded surrogate half\"]\n", 35},
    {"invalid-unicode/invalid-utf-8-after-backslash", "[\"\\\xe5\"]\n", 7},
    {"invalid-unicode/invalid-utf-8-in-array", "[\xe5]\n", 4},
    {"invalid-unicode/invalid-utf-8-in-bigger-int", "[123\xe5]\n", 7},
    {"invalid-unicode/invalid-utf-8-in-escape", "[\"\\u\xe5\"]\n", 8},
    {"invalid-unicode/invalid-utf-8-in-exponent", "[1e1\xe5]\n", 7},
    {"invalid-unicode/invalid-utf-8-in-identifier", "[a\xe5]\n", 5},
    {"invalid-unicode/invalid-utf-8-in-int", "[0\xe5]\n", 5},
    {"invalid-unicode/invalid-utf-8-in-real-after-e", "[1e\xe5]\n", 6},
    {"invalid-unicode/invalid-utf-8-in-string", "[\"\xe5 <-- invalid UTF-8\"]\n", 24},
    {"invalid-unicode/lone-invalid-utf-8", "\xe5\n", 2},
    {"invalid-unicode/lone-utf-8-continuation-byte", "[\"\x81\"]\n", 6},
    {"invalid-unicode/not-in-unicode-range", "[\"\xf4\xbf\xbf\xbf\"]\n", 9},
    {"invalid-unicode/overlong-3-byte-encoding", "[\"\xe0\x80\xa2 <-- overlong encoding\"]\n", 30},
    {"invalid-unicode/overlong-4-byte-encoding", "[\"\xf0\x80\x80\xa2 <-- overlong encoding\"]\n", 31},
    {"invalid-unicode/overlong-ascii-encoding", "[\"\xc1\"]\n", 6},
    {"invalid-unicode/restricted-utf-8", "[\"\xfd\"]\n", 6},
    {"invalid-unicode/truncated-utf-8", "[\"\xe0\xff <-- truncated UTF-8\"]\n", 27},
};

/***************************************************************
 *  A seed shaped like a sensor record (nested object, reals,
 *  escapes, UTF-8)
 ***************************************************************/
PRIVATE const char seed_record[] =
    "{\"id\":\"DEV-0001-E-001\",\"event\":\"measure\",\"device_type\":\"electric\","
    "\"tm\":1792769949,\"tm_ms\":702,\"q\":0,\"seq\":2004,\"src\":\"sim^pp1\","
    "\"note\":\"caf\\u00e9 \\\"quoted\\\" \\/ tab\\t \xc3\xb1 \xe2\x82\xac \xf0\x9d\x84\x9e\","
    "\"m\":{\"v_l1\":229.00675804948125,\"v_l2\":231.42174923767377,\"i_l1\":5.396866858185385,"
    "\"p_total\":3.780237521468895,\"freq\":50.01728080716852,\"ea_exp\":12.0,\"er_q2\":0.0,"
    "\"neg\":-1.5e-3,\"big\":1E+300,\"list\":[1,-2,3.5,true,false,null,\"x\",[],{}]},"
    "\"flags\":[true,false,null],\"empty\":{},\"__md_trq__\":{\"__msg_rowid__\":12345,"
    "\"__msg_md_rowid__\":12345,\"__msg_t__\":1792769949},\"last\":-9223372036854775808}";

/***************************************************************************
 *  What jansson says of the text: the object, or NULL if it refuses it
 *  or if it is not an object
 ***************************************************************************/
PRIVATE json_t *jansson_object(const char *text, size_t len, size_t flags)
{
    json_t *jn = json_loadb(text, len, flags, 0);
    if(jn && !json_is_object(jn)) {
        JSON_DECREF(jn)
        return NULL;
    }
    return jn;
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE void show_text(const char *what, const char *text, size_t len)
{
    if(n_mismatches_shown >= 20) {
        return;
    }
    n_mismatches_shown++;
    printf("FAIL %s, len %zu:\n    ", what, len);
    for(size_t i = 0; i < len && i < 400; i++) {
        unsigned char c = (unsigned char)text[i];
        if(c >= 0x20 && c < 0x7f) {
            putchar(c);
        } else {
            printf("\\x%02x", c);
        }
    }
    printf("%s\n", len > 400? "..." : "");
}

/***************************************************************************
 *  The members the scan finds, against the object jansson built:
 *  every key, its count (1: jansson keeps one of a repeated key, so the
 *  count is checked by the cases that repeat one), type, value text,
 *  and the cut. And a key the object does not have.
 ***************************************************************************/
PRIVATE int check_members(const char *text, size_t len, size_t flags, json_t *jn_object)
{
    int ret = 0;
    size_t n = json_object_size(jn_object) + 1;
    json_member_span_t *members = gbmem_malloc(n * sizeof(json_member_span_t));
    const char **keys = gbmem_malloc(n * sizeof(char *));

    size_t i = 0;
    const char *key;
    json_t *jn_value;
    json_object_foreach(jn_object, key, jn_value) {
        keys[i] = key;
        members[i].key = key;
        i++;
    }
    members[i].key = "\x01 not a key \x01";

    if(json_scan_object_members(text, len, flags, members, (int)n) < 0) {
        show_text("scan refuses with members what it accepted without them", text, len);
        ret = -1;
        goto end;
    }
    if(members[n-1].count != 0) {
        show_text("a key the object does not have is found", text, len);
        ret = -1;
        goto end;
    }

    for(i = 0; i < n - 1; i++) {
        json_member_span_t *m = &members[i];
        json_t *jn_expected = json_object_get(jn_object, keys[i]);
        if(m->count < 1) {
            show_text("member not found", text, len);
            printf("    key '%s'\n", keys[i]);
            ret = -1;
            continue;
        }
        if(m->type != json_typeof(jn_expected)) {
            show_text("member of another type", text, len);
            printf("    key '%s': %d, jansson %d\n", keys[i], m->type, json_typeof(jn_expected));
            ret = -1;
            continue;
        }
        json_t *jn_v = json_loadb(
            text + m->value_offset, m->value_length, flags|JSON_DECODE_ANY, 0
        );
        if(!jn_v || !json_equal(jn_v, jn_expected)) {
            show_text("member value text is not its value", text, len);
            printf("    key '%s': '%.*s'\n", keys[i], (int)m->value_length, text + m->value_offset);
            ret = -1;
        }
        JSON_DECREF(jn_v)

        if(m->count == 1) {
            size_t cut_len = len - m->cut_length;
            char *cut = gbmem_malloc(cut_len + 1);
            memcpy(cut, text, m->cut_offset);
            memcpy(cut + m->cut_offset,
                text + m->cut_offset + m->cut_length,
                len - m->cut_offset - m->cut_length
            );
            json_t *jn_cut = jansson_object(cut, cut_len, flags);
            json_t *jn_without = json_deep_copy(jn_object);
            json_object_del(jn_without, keys[i]);
            if(!jn_cut || !json_equal(jn_cut, jn_without)) {
                show_text("the cut does not leave the object without the member", text, len);
                printf("    key '%s', cut %zu+%zu\n", keys[i], m->cut_offset, m->cut_length);
                ret = -1;
            }
            JSON_DECREF(jn_cut)
            JSON_DECREF(jn_without)
            gbmem_free(cut);
        }
    }

end:
    gbmem_free(members);
    gbmem_free(keys);
    return ret;
}

/***************************************************************************
 *  The verdict of the scan against jansson's, with these flags.
 *  Return 0 if they agree (and the members check), -1 if not.
 ***************************************************************************/
PRIVATE int compare_with_jansson(const char *text, size_t len, size_t flags)
{
    json_t *jn_object = jansson_object(text, len, flags);
    int scan = json_scan_object_members(text, len, flags, NULL, 0);

    int ret = 0;
    if(jn_object && scan < 0 && memchr(text, 0, len)) {
        /*
         *  jansson's json_loadb() skips a NUL byte that follows a number or
         *  a literal ([12\0] is [12]): its stream takes the NUL it ungets
         *  for an empty buffer and reads the next byte. The scan refuses
         *  every raw NUL, as json_loads() does (it ends at the NUL).
         */
        n_skipped_nul++;
    } else if(jn_object && scan < 0) {
        show_text("jansson accepts, the scan refuses", text, len);
        ret = -1;
    } else if(!jn_object && scan == 0) {
        show_text("jansson refuses, the scan ACCEPTS", text, len);
        ret = -1;
    } else if(jn_object) {
        ret = check_members(text, len, flags, jn_object);
    }
    JSON_DECREF(jn_object)
    return ret;
}

/***************************************************************************
 *  Both flags, the text and the text as the value of a member
 ***************************************************************************/
PRIVATE int compare_all_ways(const char *text, size_t len)
{
    int ret = 0;
    ret += compare_with_jansson(text, len, 0);
    ret += compare_with_jansson(text, len, JSON_ALLOW_NUL);

    gbuffer_t *gbuf = gbuffer_create(len + 32, len + 32);
    gbuffer_append_string(gbuf, "{\"k\": ");
    if(len > 0) {
        gbuffer_append(gbuf, (void *)text, len);
    }
    gbuffer_append_string(gbuf, ", \"z\":1}");
    ret += compare_with_jansson(gbuffer_cur_rd_pointer(gbuf), gbuffer_leftbytes(gbuf), 0);
    ret += compare_with_jansson(gbuffer_cur_rd_pointer(gbuf), gbuffer_leftbytes(gbuf), JSON_ALLOW_NUL);
    GBUFFER_DECREF(gbuf)
    return ret? -1 : 0;
}

/***************************************************************************
 *  1. jansson's corpus
 ***************************************************************************/
PRIVATE void test_corpus(void)
{
    int failed = 0;
    size_t n = sizeof(corpus)/sizeof(corpus[0]);
    for(size_t i = 0; i < n; i++) {
        if(compare_all_ways(corpus[i].text, corpus[i].len) < 0) {
            printf("FAIL corpus %s\n", corpus[i].name);
            failed++;
        }
    }
    if(failed) {
        global_result += -1;
    }
    printf("%s corpus: %zu cases, %d disagree\n", failed? "FAIL" : "ok  ", n, failed);
}

/***************************************************************************
 *  A text the scan must accept (expected 0) or refuse (-1), and that
 *  jansson must judge the same way
 ***************************************************************************/
PRIVATE void check_edge(const char *what, const char *text, size_t len, size_t flags, int expected)
{
    int scan = json_scan_object_members(text, len, flags, NULL, 0);
    json_t *jn = jansson_object(text, len, flags);
    int jansson = jn? 0 : -1;
    JSON_DECREF(jn)

    if(scan != expected || jansson != expected) {
        printf("FAIL %-44s scan %d, jansson %d, expected %d\n", what, scan, jansson, expected);
        global_result += -1;
    } else {
        printf("ok   %-44s %s\n", what, expected == 0? "accepted" : "refused");
    }
    if(compare_all_ways(text, len) < 0) {
        printf("FAIL %-44s (all ways)\n", what);
        global_result += -1;
    }
}

#define EDGE(what, text, flags, expected) \
    check_edge(what, text, sizeof(text) - 1, flags, expected)

/***************************************************************************
 *  2. Edges
 ***************************************************************************/
PRIVATE void test_edges(void)
{
    EDGE("empty object", "{}", 0, 0);
    EDGE("whitespace around", " \t\r\n{ \"a\" : 1 } \n", 0, 0);
    EDGE("an array is not an object", "[1]", 0, -1);
    EDGE("a scalar is not an object", "\"x\"", 0, -1);
    EDGE("two objects", "{}{}", 0, -1);
    EDGE("trailing comma", "{\"a\":1,}", 0, -1);
    EDGE("form feed is not whitespace", "{\f}", 0, -1);

    EDGE("max int", "{\"a\":9223372036854775807}", 0, 0);
    EDGE("max int + 1", "{\"a\":9223372036854775808}", 0, -1);
    EDGE("min int", "{\"a\":-9223372036854775808}", 0, 0);
    EDGE("min int - 1", "{\"a\":-9223372036854775809}", 0, -1);
    EDGE("20 digits", "{\"a\":10000000000000000000}", 0, -1);
    EDGE("negative zero", "{\"a\":-0}", 0, 0);
    EDGE("leading zero", "{\"a\":01}", 0, -1);
    EDGE("point without digits", "{\"a\":1.}", 0, -1);
    EDGE("plus sign", "{\"a\":+1}", 0, -1);

    EDGE("1e308", "{\"a\":1e308}", 0, 0);
    EDGE("1e309", "{\"a\":1e309}", 0, -1);
    EDGE("-1e309", "{\"a\":-1e309}", 0, -1);
    EDGE("DBL_MAX", "{\"a\":1.7976931348623157e308}", 0, 0);
    EDGE("overflow threshold",
        "{\"a\":1.79769313486231580793728971405303415079934132710037826936173778980444968292764750946649017977587207096330286416692887910946555547851940402630657488671505820681908902000708383676273854845817711531764475730270069855571366959622842914819860834936475292719074168444365510704342711559699508093042880177904174497792e308}",
        0, -1);
    EDGE("just below the threshold",
        "{\"a\":1.79769313486231580793728971405303415079934132710037826936173778980444968292764750946649017977587207096330286416692887910946555547851940402630657488671505820681908902000708383676273854845817711531764475730270069855571366959622842914819860834936475292719074168444365510704342711559699508093042880177904174497791999e308}",
        0, 0);
    EDGE("threshold as 0.ddd e309",
        "{\"a\":0.000179769313486231580793728971405303415079934132710037826936173778980444968292764750946649017977587207096330286416692887910946555547851940402630657488671505820681908902000708383676273854845817711531764475730270069855571366959622842914819860834936475292719074168444365510704342711559699508093042880177904174497792e312}",
        0, -1);
    EDGE("threshold as an integer",
        "{\"a\":179769313486231580793728971405303415079934132710037826936173778980444968292764750946649017977587207096330286416692887910946555547851940402630657488671505820681908902000708383676273854845817711531764475730270069855571366959622842914819860834936475292719074168444365510704342711559699508093042880177904174497792.0}",
        0, -1);
    EDGE("below it as an integer",
        "{\"a\":179769313486231580793728971405303415079934132710037826936173778980444968292764750946649017977587207096330286416692887910946555547851940402630657488671505820681908902000708383676273854845817711531764475730270069855571366959622842914819860834936475292719074168444365510704342711559699508093042880177904174497791.9}",
        0, 0);
    EDGE("underflow is taken", "{\"a\":1e-400}", 0, 0);
    EDGE("zero with a huge exponent", "{\"a\":0e99999999999999999999}", 0, 0);
    EDGE("huge exponent", "{\"a\":1e99999999999999999999}", 0, -1);
    EDGE("huge negative exponent", "{\"a\":1e-99999999999999999999}", 0, 0);

    EDGE("surrogate pair", "{\"a\":\"\\uD834\\uDD1E\"}", 0, 0);
    EDGE("lone high surrogate", "{\"a\":\"\\uD834\"}", 0, -1);
    EDGE("high + non-low", "{\"a\":\"\\uD834\\u0041\"}", 0, -1);
    EDGE("high + high", "{\"a\":\"\\uD834\\uD834\"}", 0, -1);
    EDGE("lone low surrogate", "{\"a\":\"\\uDD1E\"}", 0, -1);
    EDGE("\\u0000 in a value", "{\"a\":\"x\\u0000y\"}", 0, -1);
    EDGE("\\u0000 in a value, ALLOW_NUL", "{\"a\":\"x\\u0000y\"}", JSON_ALLOW_NUL, 0);
    EDGE("\\u0000 in a key, ALLOW_NUL", "{\"a\\u0000\":1}", JSON_ALLOW_NUL, -1);
    EDGE("raw NUL byte", "{\"a\":\"x\0y\"}", JSON_ALLOW_NUL, -1);
    EDGE("DEL is taken", "{\"a\":\"\x7f\"}", 0, 0);
    EDGE("U+10FFFF", "{\"a\":\"\xf4\x8f\xbf\xbf\"}", 0, 0);
    EDGE("above U+10FFFF", "{\"a\":\"\xf4\x90\x80\x80\"}", 0, -1);
    EDGE("truncated at the end", "{\"a\":\"\xe2\x82", 0, -1);

    /*
     *  Depth: a value inside n containers is at depth n+1, and jansson
     *  takes up to JSON_PARSER_MAX_DEPTH
     */
    for(int extra = -1; extra <= 1; extra++) {
        int n_arrays = JSON_PARSER_MAX_DEPTH - 1 + extra;   // inside the top object
        for(int with_scalar = 0; with_scalar <= 1; with_scalar++) {
            gbuffer_t *gbuf = gbuffer_create(2*n_arrays + 32, 2*n_arrays + 32);
            gbuffer_append_string(gbuf, "{\"a\":");
            for(int i = 0; i < n_arrays; i++) {
                gbuffer_append_char(gbuf, '[');
            }
            if(with_scalar) {
                gbuffer_append_char(gbuf, '1');
            }
            for(int i = 0; i < n_arrays; i++) {
                gbuffer_append_char(gbuf, ']');
            }
            gbuffer_append_char(gbuf, '}');

            /*
             *  The deepest value is at depth 1 + n_arrays (+1 the scalar)
             */
            int depth = 1 + n_arrays + with_scalar;
            int expected = (depth <= JSON_PARSER_MAX_DEPTH)? 0 : -1;
            char what[80];
            snprintf(what, sizeof(what), "depth %d%s", depth, with_scalar? " (a scalar)" : "");
            check_edge(what, gbuffer_cur_rd_pointer(gbuf), gbuffer_leftbytes(gbuf), 0, expected);
            GBUFFER_DECREF(gbuf)
        }
    }

    /*
     *  jansson's recursion-depth case: 100000 '['
     */
    {
        gbuffer_t *gbuf = gbuffer_create(100001, 100001);
        for(int i = 0; i < 100000; i++) {
            gbuffer_append_char(gbuf, '[');
        }
        gbuffer_append_char(gbuf, '\n');
        if(compare_all_ways(gbuffer_cur_rd_pointer(gbuf), gbuffer_leftbytes(gbuf)) < 0) {
            printf("FAIL corpus invalid/recursion-depth\n");
            global_result += -1;
        } else {
            printf("ok   corpus invalid/recursion-depth\n");
        }
        GBUFFER_DECREF(gbuf)
    }
}

/***************************************************************************
 *  3. The members
 ***************************************************************************/
PRIVATE void test_members(void)
{
    const char text[] =
        "{ \"id\" : \"A\\u0042\" , \"tm\":17, \"r\":1.5,\"o\":{\"id\":\"inner\"},"
        "\"\\u0069d2\":true, \"dup\":1, \"dup\":[2], \"__md_trq__\":{\"x\":1} }";
    size_t len = sizeof(text) - 1;

    json_member_span_t m[] = {
        {.key = "id"},
        {.key = "tm"},
        {.key = "r"},
        {.key = "o"},
        {.key = "id2"},
        {.key = "dup"},
        {.key = "__md_trq__"},
        {.key = "absent"},
    };
    int ret = json_scan_object_members(text, len, 0, m, 8);

    struct {
        int count;
        json_type type;
        const char *value;
    } expected[] = {
        {1, JSON_STRING,  "\"A\\u0042\""},
        {1, JSON_INTEGER, "17"},
        {1, JSON_REAL,    "1.5"},
        {1, JSON_OBJECT,  "{\"id\":\"inner\"}"},
        {1, JSON_TRUE,    "true"},
        {2, JSON_ARRAY,   "[2]"},       // the last one, as jansson keeps it
        {1, JSON_OBJECT,  "{\"x\":1}"},
        {0, JSON_NULL,    ""},
    };

    if(ret != 0) {
        printf("FAIL members: the text is refused\n");
        global_result += -1;
        return;
    }
    for(int i = 0; i < 8; i++) {
        BOOL ok = (m[i].count == expected[i].count && m[i].type == expected[i].type &&
            m[i].value_length == strlen(expected[i].value) &&
            memcmp(text + m[i].value_offset, expected[i].value, m[i].value_length) == 0)? TRUE : FALSE;
        printf("%s member %-12s count %d type %d value '%.*s'\n",
            ok? "ok  " : "FAIL",
            m[i].key, m[i].count, m[i].type,
            (int)m[i].value_length, text + m[i].value_offset
        );
        if(!ok) {
            global_result += -1;
        }
    }

    /*
     *  The cut of the last member, and of the first, as the store does it:
     *  memmove of the tail over it
     */
    for(int which = 0; which < 2; which++) {
        json_member_span_t *mm = which == 0? &m[6] : &m[0];
        char bf[sizeof(text)];
        memcpy(bf, text, len);
        memmove(bf + mm->cut_offset,
            bf + mm->cut_offset + mm->cut_length,
            len - mm->cut_offset - mm->cut_length
        );
        size_t cut_len = len - mm->cut_length;
        json_t *jn_cut = json_loadb(bf, cut_len, 0, 0);
        json_t *jn_full = json_loadb(text, len, 0, 0);
        json_object_del(jn_full, mm->key);
        BOOL ok = (jn_cut && json_equal(jn_cut, jn_full))? TRUE : FALSE;
        printf("%s cut of %-10s '%.*s'\n", ok? "ok  " : "FAIL", mm->key, (int)cut_len, bf);
        if(!ok) {
            global_result += -1;
        }
        JSON_DECREF(jn_cut)
        JSON_DECREF(jn_full)
    }

    /*
     *  The only member
     */
    {
        const char one[] = "{ \"__md_trq__\" : {} }";
        json_member_span_t m1 = {.key = "__md_trq__"};
        json_scan_object_members(one, sizeof(one) - 1, 0, &m1, 1);
        char bf[sizeof(one)];
        size_t one_len = sizeof(one) - 1;
        memcpy(bf, one, one_len);
        memmove(bf + m1.cut_offset, bf + m1.cut_offset + m1.cut_length,
            one_len - m1.cut_offset - m1.cut_length);
        size_t cut_len = one_len - m1.cut_length;
        json_t *jn_cut = json_loadb(bf, cut_len, 0, 0);
        BOOL ok = (m1.count == 1 && jn_cut && json_object_size(jn_cut) == 0)? TRUE : FALSE;
        printf("%s cut of the only member '%.*s'\n", ok? "ok  " : "FAIL", (int)cut_len, bf);
        if(!ok) {
            global_result += -1;
        }
        JSON_DECREF(jn_cut)
    }

    if(compare_all_ways(text, len) < 0 || compare_all_ways(seed_record, strlen(seed_record)) < 0) {
        printf("FAIL members against jansson\n");
        global_result += -1;
    }
}

/***************************************************************************
 *  4. Fuzz
 ***************************************************************************/
PRIVATE uint64_t rng_state = 0x9E3779B97F4A7C15ULL;
PRIVATE uint64_t rng(void)
{
    rng_state ^= rng_state << 13;
    rng_state ^= rng_state >> 7;
    rng_state ^= rng_state << 17;
    return rng_state;
}

PRIVATE const unsigned char fuzz_bytes[] = {
    '"', '\\', 'u', '{', '}', '[', ']', ',', ':', '0', '1', '9', 'e', 'E', '-', '+', '.',
    ' ', '\n', '\t', 't', 'f', 'n', 'D', 'd', '8', 'a', 0x00, 0x01, 0x1f, 0x7f, 0x80, 0xbf,
    0xc0, 0xc2, 0xdf, 0xe0, 0xed, 0xef, 0xf0, 0xf4, 0xf5, 0xff
};

PRIVATE void test_fuzz(int iterations)
{
    const char *seeds[sizeof(corpus)/sizeof(corpus[0]) + 2];
    size_t seed_lens[sizeof(corpus)/sizeof(corpus[0]) + 2];
    int n_seeds = 0;
    seeds[n_seeds] = seed_record;
    seed_lens[n_seeds++] = strlen(seed_record);
    seeds[n_seeds] = "{\"a\":\"\\uD834\\uDD1E\",\"b\":-0.0e-0,\"c\":[{\"d\":\"\\u00e9\"}],\"\\u0061\":9223372036854775807}";
    seed_lens[n_seeds] = strlen(seeds[n_seeds]);
    n_seeds++;
    for(size_t i = 0; i < sizeof(corpus)/sizeof(corpus[0]); i++) {
        if(strncmp(corpus[i].name, "valid/", 6) == 0) {
            seeds[n_seeds] = corpus[i].text;
            seed_lens[n_seeds++] = corpus[i].len;
        }
    }

    int failed = 0;
    int accepted = 0;
    char *bf = gbmem_malloc(4096);

    for(int it = 0; it < iterations; it++) {
        int s = (int)(rng() % (uint64_t)n_seeds);
        size_t len = 0;
        if(seeds[s][0] != '{') {
            /*
             *  Not an object: as the value of a member
             */
            memcpy(bf, "{\"k\":", 5);
            memcpy(bf + 5, seeds[s], seed_lens[s]);
            len = 5 + seed_lens[s];
            bf[len++] = '}';
        } else {
            memcpy(bf, seeds[s], seed_lens[s]);
            len = seed_lens[s];
        }

        int n_mutations = 1 + (int)(rng() % 3);
        for(int k = 0; k < n_mutations && len > 0; k++) {
            size_t pos = (size_t)(rng() % len);
            unsigned char b = (rng() % 4 == 0)?
                (unsigned char)rng() : fuzz_bytes[rng() % sizeof(fuzz_bytes)];
            switch(rng() % 5) {
                case 0:     // replace
                    bf[pos] = (char)b;
                    break;
                case 1:     // insert
                    if(len < 4000) {
                        memmove(bf + pos + 1, bf + pos, len - pos);
                        bf[pos] = (char)b;
                        len++;
                    }
                    break;
                case 2:     // delete
                    memmove(bf + pos, bf + pos + 1, len - pos - 1);
                    len--;
                    break;
                case 3:     // truncate
                    len = pos + 1;
                    break;
                case 4:     // repeat a piece
                    {
                        size_t piece = 1 + (size_t)(rng() % 16);
                        if(pos + piece <= len && len + piece < 4000) {
                            memmove(bf + pos + piece, bf + pos, len - pos);
                            len += piece;
                        }
                    }
                    break;
            }
        }

        size_t flags = (it & 1)? JSON_ALLOW_NUL : 0;
        if(compare_with_jansson(bf, len, flags) < 0) {
            failed++;
        }
        if(json_scan_object_members(bf, len, flags, NULL, 0) == 0) {
            accepted++;
        }
    }
    gbmem_free(bf);

    if(failed) {
        global_result += -1;
    }
    printf("%s fuzz: %d texts, %d valid, %d disagree with jansson\n",
        failed? "FAIL" : "ok  ", iterations, accepted, failed);
    printf("     (%d texts with a raw NUL that json_loadb() takes by skipping it,"
        " refused by the scan)\n", n_skipped_nul);
}

/***************************************************************************
 *  5. Bad parameters
 ***************************************************************************/
PRIVATE void test_bad_parameters(void)
{
    json_member_span_t m = {.key = "a"};
    int r1 = json_scan_object_members(NULL, 0, 0, NULL, 0);
    int r2 = json_scan_object_members("{}", 2, 0, NULL, 1);
    int r3 = json_scan_object_members("{}", 2, JSON_DECODE_ANY, &m, 1);
    if(r1 != -1 || r2 != -1 || r3 != -1) {
        printf("FAIL bad parameters: %d %d %d\n", r1, r2, r3);
        global_result += -1;
    } else {
        printf("ok   bad parameters refused (3 errors logged above)\n");
    }
}

/***************************************************************************
 *
 ***************************************************************************/
int main(int argc, char *argv[])
{
    sys_malloc_fn_t malloc_func;
    sys_free_fn_t free_func;
    gbmem_get_json_allocators(&malloc_func, &free_func);
    json_set_alloc_funcs(malloc_func, free_func);

    gobj_start_up(
        argc,
        argv,
        NULL,   // jn_global_settings
        NULL,   // persistent_attrs
        NULL,   // global_command_parser
        NULL,   // global_stats_parser
        NULL,   // global_authz_checker
        NULL    // global_authentication_parser
    );
    gobj_log_add_handler("stdout", "stdout", LOG_OPT_ALL, 0);

    test_corpus();
    test_edges();
    test_members();
    /*
     *  test_json_scan [iterations [seed]]: a longer fuzz by hand
     */
    int iterations = (argc > 1)? atoi(argv[1]) : 200000;
    if(argc > 2) {
        rng_state = strtoull(argv[2], NULL, 0);
    }
    test_fuzz(iterations);
    test_bad_parameters();

    gobj_end();

    if(get_cur_system_memory() != 0) {
        printf("FAIL system memory not free: %lu\n", (unsigned long)get_cur_system_memory());
        print_track_mem();
        global_result += -1;
    }

    printf("\n%s: %s\n", APP, global_result == 0 ? "PASS" : "FAIL");
    return global_result == 0 ? 0 : -1;
}
