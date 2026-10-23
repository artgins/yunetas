/****************************************************************************
 *          json_scan.c
 *
 *          Read the top-level members of a json object from its TEXT,
 *          without building the object.
 *
 *          json_scan_object_members() answers the question json_loadb()
 *          answers -- is this text one json object? -- with the same
 *          verdict: the grammar, the UTF-8, the escapes and surrogate
 *          pairs, the \u0000, the range of integers and reals and the
 *          nesting depth are jansson's (2.15: load.c, utf.c, strconv.c).
 *          It builds no node and allocates nothing. A caller that needs
 *          two or three members of a large object, and the rest of it only
 *          as text, saves the parse, the free and the dump of the whole.
 *
 *          The verdict is the contract: a text this function accepts is a
 *          text jansson accepts, and the other way round. The test
 *          (tests/c/helpers/test_json_scan.c) holds it to that with
 *          jansson's own corpus and a fuzz against json_loadb().
 *
 *          Copyright (c) 2026, ArtGins.
 *          All Rights Reserved.
 ****************************************************************************/
#include <string.h>
#include <stdint.h>

#include "helpers.h"

/***************************************************************
 *              Constants
 ***************************************************************/
/*
 *  The smallest decimal that strtod() turns into an infinite, jansson's
 *  "real number overflow": 2^1024 - 2^970, halfway between DBL_MAX and
 *  2^1024, which round-half-even takes up. Its 309 digits, 0.ddd x 10^309.
 */
PRIVATE const char real_overflow_digits[] =
    "17976931348623158079372897140530341507993413271003782693617377898044496829276475094664901797758720709633028641669288791094655554785194040263065748867150582068190890200070838367627385484581771153176447573027006985557136695962284291481986083493647529271907416844436551070434271155969950809304288017790417449779"
    "2";
#define REAL_OVERFLOW_EXPONENT  309

/***************************************************************
 *              Structures
 ***************************************************************/
typedef struct {
    const unsigned char *text;
    const unsigned char *p;
    const unsigned char *end;
    size_t flags;
} scan_t;

/***************************************************************
 *              Prototypes
 ***************************************************************/




                    /***************************
                     *      Helpers
                     ***************************/




/***************************************************************************
 *  Length of the valid UTF-8 sequence at p, 0 if it is not one:
 *  jansson's utf8_check_first() + utf8_check_full(), which refuse an
 *  overlong form, a surrogate half and anything above U+10FFFF.
 ***************************************************************************/
PRIVATE size_t utf8_sequence_length(const unsigned char *p, const unsigned char *end)
{
    unsigned char u = p[0];
    size_t count;
    int32_t value;

    if(u < 0x80) {
        return 1;
    }
    if(u >= 0xC2 && u <= 0xDF) {
        count = 2;
        value = u & 0x1F;
    } else if(u >= 0xE0 && u <= 0xEF) {
        count = 3;
        value = u & 0x0F;
    } else if(u >= 0xF0 && u <= 0xF4) {
        count = 4;
        value = u & 0x07;
    } else {
        return 0;
    }
    if((size_t)(end - p) < count) {
        return 0;
    }
    for(size_t i = 1; i < count; i++) {
        if(p[i] < 0x80 || p[i] > 0xBF) {
            return 0;
        }
        value = (value << 6) + (p[i] & 0x3F);
    }
    if(value > 0x10FFFF) {
        return 0;
    }
    if(value >= 0xD800 && value <= 0xDFFF) {
        return 0;
    }
    if((count == 2 && value < 0x80) ||
       (count == 3 && value < 0x800) ||
       (count == 4 && value < 0x10000)) {
        return 0;
    }
    return count;
}

/***************************************************************************
 *  The 4 hex digits at p as a number, -1 if they are not 4 hex digits
 ***************************************************************************/
PRIVATE int32_t decode_hex4(const unsigned char *p, const unsigned char *end)
{
    if(end - p < 4) {
        return -1;
    }
    int32_t value = 0;
    for(int i = 0; i < 4; i++) {
        unsigned char c = p[i];
        value <<= 4;
        if(c >= '0' && c <= '9') {
            value += c - '0';
        } else if(c >= 'a' && c <= 'f') {
            value += c - 'a' + 10;
        } else if(c >= 'A' && c <= 'F') {
            value += c - 'A' + 10;
        } else {
            return -1;
        }
    }
    return value;
}

/***************************************************************************
 *  The code point of the \u escape at p (p at the 'u'), a surrogate pair
 *  taken whole; *escape_len is what it takes from the text.
 *  -1 if it is not one that jansson decodes.
 ***************************************************************************/
PRIVATE int32_t decode_unicode_escape(
    const unsigned char *p,
    const unsigned char *end,
    size_t *escape_len
)
{
    int32_t value = decode_hex4(p + 1, end);
    if(value < 0) {
        return -1;
    }
    *escape_len = 5;

    if(value >= 0xD800 && value <= 0xDBFF) {
        if(end - (p + 5) < 2 || p[5] != '\\' || p[6] != 'u') {
            return -1;
        }
        int32_t value2 = decode_hex4(p + 7, end);
        if(value2 < 0xDC00 || value2 > 0xDFFF) {
            return -1;
        }
        *escape_len = 11;
        return ((value - 0xD800) << 10) + (value2 - 0xDC00) + 0x10000;
    }
    if(value >= 0xDC00 && value <= 0xDFFF) {
        return -1;
    }
    return value;
}

/***************************************************************************
 *  Scan the string whose opening quote is at s->p, leave s->p after the
 *  closing one. A \u0000 is refused in a key always, in a value without
 *  JSON_ALLOW_NUL (jansson's parse_object() and parse_value()).
 ***************************************************************************/
PRIVATE int scan_string(scan_t *s, BOOL is_key, BOOL *has_escapes)
{
    const unsigned char *p = s->p + 1;
    const unsigned char *end = s->end;
    BOOL allow_nul = (!is_key && (s->flags & JSON_ALLOW_NUL))? TRUE : FALSE;

    *has_escapes = FALSE;

    while(p < end) {
        unsigned char c = *p;
        if(c == '"') {
            s->p = p + 1;
            return 0;
        }
        if(c < 0x20) {
            return -1;
        }
        if(c == '\\') {
            *has_escapes = TRUE;
            if(p + 1 >= end) {
                return -1;
            }
            switch(p[1]) {
                case 'u':
                    {
                        size_t escape_len;
                        int32_t value = decode_unicode_escape(p + 1, end, &escape_len);
                        if(value < 0) {
                            return -1;
                        }
                        if(value == 0 && !allow_nul) {
                            return -1;
                        }
                        p += 1 + escape_len;
                    }
                    break;
                case '"':
                case '\\':
                case '/':
                case 'b':
                case 'f':
                case 'n':
                case 'r':
                case 't':
                    p += 2;
                    break;
                default:
                    return -1;
            }
            continue;
        }
        if(c >= 0x80) {
            size_t n = utf8_sequence_length(p, end);
            if(!n) {
                return -1;
            }
            p += n;
            continue;
        }
        p++;
    }
    return -1;
}

/***************************************************************************
 *  Is the raw text of a key, escapes included, the key `key`?
 *  The raw text is already validated by scan_string().
 ***************************************************************************/
PRIVATE BOOL escaped_key_equals(const unsigned char *raw, size_t raw_len, const char *key)
{
    const unsigned char *p = raw;
    const unsigned char *end = raw + raw_len;
    const unsigned char *k = (const unsigned char *)key;

    while(p < end) {
        unsigned char bf[4];
        size_t n;
        if(*p != '\\') {
            bf[0] = *p;
            n = 1;
            p++;
        } else {
            switch(p[1]) {
                case 'u':
                    {
                        size_t escape_len;
                        int32_t value = decode_unicode_escape(p + 1, end, &escape_len);
                        p += 1 + escape_len;
                        if(value < 0x80) {
                            bf[0] = (unsigned char)value;
                            n = 1;
                        } else if(value < 0x800) {
                            bf[0] = (unsigned char)(0xC0 + (value >> 6));
                            bf[1] = (unsigned char)(0x80 + (value & 0x3F));
                            n = 2;
                        } else if(value < 0x10000) {
                            bf[0] = (unsigned char)(0xE0 + (value >> 12));
                            bf[1] = (unsigned char)(0x80 + ((value >> 6) & 0x3F));
                            bf[2] = (unsigned char)(0x80 + (value & 0x3F));
                            n = 3;
                        } else {
                            bf[0] = (unsigned char)(0xF0 + (value >> 18));
                            bf[1] = (unsigned char)(0x80 + ((value >> 12) & 0x3F));
                            bf[2] = (unsigned char)(0x80 + ((value >> 6) & 0x3F));
                            bf[3] = (unsigned char)(0x80 + (value & 0x3F));
                            n = 4;
                        }
                    }
                    break;
                case 'b':
                    bf[0] = '\b';
                    n = 1;
                    p += 2;
                    break;
                case 'f':
                    bf[0] = '\f';
                    n = 1;
                    p += 2;
                    break;
                case 'n':
                    bf[0] = '\n';
                    n = 1;
                    p += 2;
                    break;
                case 'r':
                    bf[0] = '\r';
                    n = 1;
                    p += 2;
                    break;
                case 't':
                    bf[0] = '\t';
                    n = 1;
                    p += 2;
                    break;
                default:    // '"', '\\', '/'
                    bf[0] = p[1];
                    n = 1;
                    p += 2;
                    break;
            }
        }
        for(size_t i = 0; i < n; i++) {
            if(*k == 0 || *k != bf[i]) {
                return FALSE;
            }
            k++;
        }
    }
    return (*k == 0)? TRUE : FALSE;
}

/***************************************************************************
 *  Is the key at `raw` (its text between the quotes) one of the members?
 *  Return its index, -1 if none.
 ***************************************************************************/
PRIVATE int find_member(
    json_member_span_t *members,
    int n_members,
    const unsigned char *raw,
    size_t raw_len,
    BOOL has_escapes
)
{
    for(int i = 0; i < n_members; i++) {
        const char *key = members[i].key;
        if(has_escapes) {
            if(escaped_key_equals(raw, raw_len, key)) {
                return i;
            }
        } else if((unsigned char)key[0] == raw[0] || raw_len == 0) {
            if(strlen(key) == raw_len && memcmp(key, raw, raw_len) == 0) {
                return i;
            }
        }
    }
    return -1;
}

/***************************************************************************
 *  Is the real whose digits are these one that strtod() takes to an
 *  infinite? (jansson refuses it: "real number overflow")
 *  An underflow is not refused: jansson takes it as strtod() leaves it.
 ***************************************************************************/
PRIVATE BOOL real_overflows(
    const unsigned char *int_digits, size_t n_int,
    const unsigned char *frac_digits, size_t n_frac,
    int64_t exponent
)
{
    /*
     *  value = 0.ddd x 10^e, ddd starting at the first non-zero digit
     */
    const unsigned char *first;
    int64_t e;
    size_t k = 0;
    while(k < n_int && int_digits[k] == '0') {
        k++;
    }
    if(k < n_int) {
        first = int_digits + k;
        e = (int64_t)(n_int - k) + exponent;
    } else {
        size_t j = 0;
        while(j < n_frac && frac_digits[j] == '0') {
            j++;
        }
        if(j == n_frac) {
            return FALSE;   // zero
        }
        first = frac_digits + j;
        e = exponent - (int64_t)j;
    }

    if(e > REAL_OVERFLOW_EXPONENT) {
        return TRUE;
    }
    if(e < REAL_OVERFLOW_EXPONENT) {
        return FALSE;
    }

    /*
     *  Same exponent: compare the digits with the threshold's, those of
     *  the integer part from `first` and then those of the fraction
     */
    const unsigned char *d = first;
    const unsigned char *d_end;
    const unsigned char *next = NULL;
    const unsigned char *next_end = NULL;
    if(k < n_int) {
        d_end = int_digits + n_int;
        next = frac_digits;
        next_end = frac_digits + n_frac;
    } else {
        d_end = frac_digits + n_frac;
    }

    for(const char *t = real_overflow_digits; *t; t++) {
        if(d == d_end) {
            if(next && next < next_end) {
                d = next;
                d_end = next_end;
                next = NULL;
            } else {
                return FALSE;   // the same digits, fewer of them: below
            }
        }
        if(*d != (unsigned char)*t) {
            return (*d > (unsigned char)*t)? TRUE : FALSE;
        }
        d++;
    }
    return TRUE;    // the threshold itself, or above it
}

/***************************************************************************
 *  Scan the number at s->p: jansson's lex_scan_number(), with its
 *  ranges (an integer is a json_int_t, a long long).
 ***************************************************************************/
PRIVATE int scan_number(scan_t *s, json_type *type)
{
    const unsigned char *p = s->p;
    const unsigned char *end = s->end;
    BOOL negative = FALSE;

    if(*p == '-') {
        negative = TRUE;
        p++;
        if(p >= end) {
            return -1;
        }
    }

    const unsigned char *int_digits = p;
    if(*p == '0') {
        p++;
        if(p < end && *p >= '0' && *p <= '9') {
            return -1;
        }
    } else if(*p >= '1' && *p <= '9') {
        while(p < end && *p >= '0' && *p <= '9') {
            p++;
        }
    } else {
        return -1;
    }
    size_t n_int = (size_t)(p - int_digits);

    if(p >= end || (*p != '.' && *p != 'e' && *p != 'E')) {
        /*
         *  Integer, in the range of a long long
         */
        static const char max_positive[] = "9223372036854775807";
        static const char max_negative[] = "9223372036854775808";
        if(n_int > 19) {
            return -1;
        }
        if(n_int == 19) {
            const char *max = negative? max_negative : max_positive;
            if(memcmp(int_digits, max, 19) > 0) {
                return -1;
            }
        }
        *type = JSON_INTEGER;
        s->p = p;
        return 0;
    }

    const unsigned char *frac_digits = p;
    size_t n_frac = 0;
    if(*p == '.') {
        p++;
        frac_digits = p;
        if(p >= end || *p < '0' || *p > '9') {
            return -1;
        }
        while(p < end && *p >= '0' && *p <= '9') {
            p++;
        }
        n_frac = (size_t)(p - frac_digits);
    }

    int64_t exponent = 0;
    if(p < end && (*p == 'e' || *p == 'E')) {
        BOOL negative_exponent = FALSE;
        p++;
        if(p < end && (*p == '+' || *p == '-')) {
            negative_exponent = (*p == '-')? TRUE : FALSE;
            p++;
        }
        if(p >= end || *p < '0' || *p > '9') {
            return -1;
        }
        while(p < end && *p >= '0' && *p <= '9') {
            if(exponent < 1000000000000LL) {    // past it the answer no longer changes
                exponent = exponent * 10 + (*p - '0');
            }
            p++;
        }
        if(negative_exponent) {
            exponent = -exponent;
        }
    }

    if(real_overflows(int_digits, n_int, frac_digits, n_frac, exponent)) {
        return -1;
    }

    *type = JSON_REAL;
    s->p = p;
    return 0;
}

/***************************************************************************
 *  Scan a literal (true, false, null) at s->p
 ***************************************************************************/
PRIVATE int scan_literal(scan_t *s, const char *literal, size_t literal_len)
{
    if((size_t)(s->end - s->p) < literal_len || memcmp(s->p, literal, literal_len) != 0) {
        return -1;
    }
    s->p += literal_len;
    return 0;
}

/***************************************************************************
 *
 ***************************************************************************/
static inline void skip_whitespace(scan_t *s)
{
    while(s->p < s->end &&
            (*s->p == ' ' || *s->p == '\t' || *s->p == '\n' || *s->p == '\r')) {
        s->p++;
    }
}




                    /***************************
                     *      Public
                     ***************************/




/***************************************************************************
 *  Scan `text` (`len` bytes): 0 if it is ONE json object that
 *  json_loadb(text, len, flags) accepts, -1 if it is not. For each of
 *  `members` it says whether the object has that key at its top level,
 *  and where in the text it is -- the LAST occurrence, as jansson keeps.
 ***************************************************************************/
PUBLIC int json_scan_object_members(
    const char *text,
    size_t len,
    size_t flags,
    json_member_span_t *members,
    int n_members
)
{
    if(!text || (n_members > 0 && !members) || n_members < 0) {
        gobj_log_error(0, LOG_OPT_TRACE_STACK,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_PARAMETER,
            "msg",          "%s", "text or members NULL",
            "n_members",    "%d", n_members,
            NULL
        );
        return -1;
    }
    if(flags & ~(size_t)JSON_ALLOW_NUL) {
        gobj_log_error(0, LOG_OPT_TRACE_STACK,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_PARAMETER,
            "msg",          "%s", "only JSON_ALLOW_NUL is a flag of the scan",
            "flags",        "0x%x", (unsigned)flags,
            NULL
        );
        return -1;
    }

    for(int i = 0; i < n_members; i++) {
        members[i].count = 0;
        members[i].type = JSON_NULL;
        members[i].value_offset = 0;
        members[i].value_length = 0;
        members[i].cut_offset = 0;
        members[i].cut_length = 0;
    }

    scan_t scan = {
        .text = (const unsigned char *)text,
        .p = (const unsigned char *)text,
        .end = (const unsigned char *)text + len,
        .flags = flags
    };
    scan_t *s = &scan;

    /*
     *  The open containers, a bit each: 1 object, 0 array.
     *  jansson counts the depth per value, scalars included, and refuses
     *  one past JSON_PARSER_MAX_DEPTH: a value inside n containers is at
     *  depth n+1.
     */
    uint8_t is_object[(JSON_PARSER_MAX_DEPTH + 7) / 8];
    size_t n_open = 0;

    /*
     *  The member of the top-level object being read
     */
    int member_idx = -1;            // the one of `members` it is, -1 if none
    size_t key_offset = 0;
    size_t value_offset = 0;
    size_t prev_value_end = 0;
    int n_top_members = 0;
    int first_member_idx = -1;      // its cut ends at the second key, still unknown
    json_type value_type = JSON_NULL;

    skip_whitespace(s);
    if(s->p >= s->end || *s->p != '{') {
        return -1;
    }

    /*
     *  Three states: a value is expected, a key is expected, a value ended
     */
    enum {EXPECT_VALUE, EXPECT_KEY, AFTER_VALUE} state = EXPECT_VALUE;

    while(1) {
        switch(state) {
            case EXPECT_VALUE:
                {
                    skip_whitespace(s);
                    if(s->p >= s->end) {
                        return -1;
                    }
                    if(n_open + 1 > JSON_PARSER_MAX_DEPTH) {
                        return -1;
                    }
                    BOOL is_member_value = (n_open == 1)? TRUE : FALSE;
                    if(is_member_value) {
                        value_offset = (size_t)(s->p - s->text);
                    }

                    json_type type;
                    switch(*s->p) {
                        case '{':
                        case '[':
                            {
                                BOOL obj = (*s->p == '{')? TRUE : FALSE;
                                if(obj) {
                                    is_object[n_open/8] |= (uint8_t)(1u << (n_open%8));
                                } else {
                                    is_object[n_open/8] &= (uint8_t)~(1u << (n_open%8));
                                }
                                n_open++;
                                type = obj? JSON_OBJECT : JSON_ARRAY;
                                s->p++;
                                skip_whitespace(s);
                                if(s->p < s->end && *s->p == (obj? '}' : ']')) {
                                    s->p++;
                                    n_open--;
                                    state = AFTER_VALUE;
                                } else {
                                    state = obj? EXPECT_KEY : EXPECT_VALUE;
                                }
                            }
                            break;

                        case '"':
                            {
                                BOOL has_escapes;
                                if(scan_string(s, FALSE, &has_escapes) < 0) {
                                    return -1;
                                }
                                type = JSON_STRING;
                                state = AFTER_VALUE;
                            }
                            break;

                        case 't':
                            if(scan_literal(s, "true", 4) < 0) {
                                return -1;
                            }
                            type = JSON_TRUE;
                            state = AFTER_VALUE;
                            break;

                        case 'f':
                            if(scan_literal(s, "false", 5) < 0) {
                                return -1;
                            }
                            type = JSON_FALSE;
                            state = AFTER_VALUE;
                            break;

                        case 'n':
                            if(scan_literal(s, "null", 4) < 0) {
                                return -1;
                            }
                            type = JSON_NULL;
                            state = AFTER_VALUE;
                            break;

                        default:
                            if(*s->p != '-' && (*s->p < '0' || *s->p > '9')) {
                                return -1;
                            }
                            if(scan_number(s, &type) < 0) {
                                return -1;
                            }
                            state = AFTER_VALUE;
                            break;
                    }
                    if(is_member_value) {
                        value_type = type;
                    }
                }
                break;

            case EXPECT_KEY:
                {
                    skip_whitespace(s);
                    if(s->p >= s->end || *s->p != '"') {
                        return -1;
                    }
                    const unsigned char *key_start = s->p;
                    BOOL has_escapes;
                    if(scan_string(s, TRUE, &has_escapes) < 0) {
                        return -1;
                    }
                    if(n_open == 1) {
                        key_offset = (size_t)(key_start - s->text);
                        if(n_top_members == 1 && first_member_idx >= 0) {
                            members[first_member_idx].cut_length =
                                key_offset - members[first_member_idx].cut_offset;
                        }
                        member_idx = find_member(
                            members,
                            n_members,
                            key_start + 1,
                            (size_t)(s->p - key_start) - 2,
                            has_escapes
                        );
                    }
                    skip_whitespace(s);
                    if(s->p >= s->end || *s->p != ':') {
                        return -1;
                    }
                    s->p++;
                    state = EXPECT_VALUE;
                }
                break;

            case AFTER_VALUE:
                if(n_open == 0) {
                    skip_whitespace(s);
                    return (s->p == s->end)? 0 : -1;
                }
                if(n_open == 1) {
                    size_t value_end = (size_t)(s->p - s->text);
                    if(member_idx >= 0) {
                        json_member_span_t *m = &members[member_idx];
                        m->count++;
                        m->type = value_type;
                        m->value_offset = value_offset;
                        m->value_length = value_end - value_offset;
                        if(n_top_members == 0) {
                            m->cut_offset = key_offset;
                            m->cut_length = value_end - key_offset;
                        } else {
                            m->cut_offset = prev_value_end;
                            m->cut_length = value_end - prev_value_end;
                        }
                    }
                    if(n_top_members == 0) {
                        first_member_idx = member_idx;
                    }
                    prev_value_end = value_end;
                    n_top_members++;
                    member_idx = -1;
                }

                skip_whitespace(s);
                if(s->p >= s->end) {
                    return -1;
                }
                {
                    size_t top = n_open - 1;
                    BOOL in_object = (is_object[top/8] & (1u << (top%8)))? TRUE : FALSE;
                    if(*s->p == ',') {
                        s->p++;
                        state = in_object? EXPECT_KEY : EXPECT_VALUE;
                    } else if(*s->p == (in_object? '}' : ']')) {
                        s->p++;
                        n_open--;
                        state = AFTER_VALUE;
                    } else {
                        return -1;
                    }
                }
                break;
        }
    }
}
