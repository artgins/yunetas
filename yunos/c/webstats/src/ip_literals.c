/****************************************************************************
 *          ip_literals.c
 *
 *          The IPv4 addresses of a mail body, written as [a.b.c.d].
 *
 *          Copyright (c) 2026, ArtGins.
 *          All Rights Reserved.
 ****************************************************************************/
#include <string.h>

#include "ip_literals.h"

/***************************************************************************
 *  Write every IPv4 address of the text as [a.b.c.d].
 *
 *  MANDATORY for a mail body, like break_tag_lines(). OVH's outbound
 *  relay reads "34.140.132.132" as a Spanish phone number (+34 and nine
 *  digits), and in a report full of addresses marked "banned" that is
 *  enough for it to accept the mail (250 queued) and deliver it to NOBODY
 *  -- no bounce, no Junk, not at gmail or outlook either. Found on
 *  2026-09-30, bisecting wattyzer's report of 2026-09-29 down to one row
 *  of Top clients. Google Cloud addresses start with 34, so it comes back.
 *  Proven on the relay: [34.140.132.132] and 34.140.132.132/32 pass, and
 *  so does a middle dot; 34[.]140[.]132[.]132 -- the usual defang -- does
 *  NOT, nor does the last dot alone.
 *
 *  Only the TEXT between tags is touched, and only an address that stands
 *  on its own: one glued to a word, a slash or another dot is a version
 *  (Chrome/142.0.0.0, nginx-1.25.3.1, 1.2.3.4-beta), not an address. What
 *  does not glue, and stays outside the brackets:
 *      - the dot that ends a sentence: [a.b.c.d].
 *      - a colon, before or after: client:[a.b.c.d], [a.b.c.d]:443,
 *        [a.b.c.d]: refused -- in a text a colon separates, and an IPv4
 *        is never followed by one inside an IPv6 address;
 *      - a dash that is not part of a word: -[a.b.c.d], [a.b.c.d]-, and a
 *        range, [a.b.c.d]-[e.f.g.h]. A dash between a word or a number
 *        and the digits (nginx-1.25.3.1, 7.25.20.1-1) keeps them a
 *        version.
 *  An IPv6 literal that ends in an IPv4 -- the mapped ::ffff:a.b.c.d, and
 *  any other (64:ff9b::a.b.c.d, 0:0:0:0:0:ffff:a.b.c.d) -- is bracketed
 *  whole, as one address: [::ffff:a.b.c.d], [64:ff9b::a.b.c.d]. Up to
 *  7.25.20 all of these stayed bare, because '.', ':' and '-' counted as
 *  glue -- and each one brings the phone number back.
 *
 *  The stored record keeps the plain address; this is the mail's way of
 *  writing it.
 ***************************************************************************/

PRIVATE BOOL ip_octet(const char *p, size_t len, size_t *used)
{
    size_t n = 0;
    int value = 0;
    while(n < len && n < 4 && p[n] >= '0' && p[n] <= '9') {
        value = value*10 + (p[n] - '0');
        n++;
    }
    if(n == 0 || n > 3 || value > 255) {
        return FALSE;
    }
    *used = n;
    return TRUE;
}

PRIVATE BOOL ip_neighbour(char c)
{
    return (c >= '0' && c <= '9') || (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
           c == '.' || c == '/' || c == '_' || c == '-' || c == '[' || c == ']' || c == ':';
}

/*
 *  The length of the IPv6 head of an IPv6 literal that ends in an IPv4 at
 *  the start of p: hex digits and colons, at least two colons, up to the
 *  colon the dotted quad follows. 0 when there is none.
 */
PRIVATE size_t ipv4_length(const char *p, size_t len);

PRIVATE size_t ipv6_head_length(const char *p, size_t len)
{
    size_t run = 0;
    while(run < len && (p[run] == ':' ||
            (p[run] >= '0' && p[run] <= '9') ||
            (p[run] >= 'a' && p[run] <= 'f') ||
            (p[run] >= 'A' && p[run] <= 'F'))) {
        run++;
    }
    size_t head = run;
    while(head > 0 && p[head-1] != ':') {
        head--;     // back over the first octet of the quad
    }
    size_t colons = 0;
    for(size_t k = 0; k < head; k++) {
        if(p[k] == ':') {
            colons++;
        }
    }
    if(colons < 2 || ipv4_length(p+head, len-head) == 0) {
        return 0;
    }
    return head;
}

/*
 *  The length of the dotted quad at the start of p, 0 when there is none.
 */
PRIVATE size_t ipv4_length(const char *p, size_t len)
{
    size_t at = 0;
    for(int octet = 0; octet < 4; octet++) {
        if(octet > 0) {
            if(at >= len || p[at] != '.') {
                return 0;
            }
            at++;
        }
        size_t used = 0;
        if(!ip_octet(p+at, len-at, &used)) {
            return 0;
        }
        at += used;
    }
    return at;
}

PRIVATE BOOL ip_word_char(char c)
{
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '_';
}

/*
 *  TRUE when the address that ends at p[at] stands on its own: the end, a
 *  character that is no neighbour, the dot of a sentence end, a colon, or
 *  a dash that no word follows.
 */
PRIVATE BOOL ip_stands_alone(const char *p, size_t len, size_t at)
{
    if(at >= len || !ip_neighbour(p[at])) {
        return TRUE;
    }
    switch(p[at]) {
        case '.':
            return at+1 >= len || !ip_neighbour(p[at+1]);
        case ':':
            return TRUE;
        case '-':
            if(at+1 >= len || !ip_neighbour(p[at+1])) {
                return TRUE;
            }
            return ipv4_length(p+at+1, len-at-1) > 0;   // a range
        default:
            return FALSE;
    }
}

/*
 *  TRUE when what comes before p[i] glues itself to an address there.
 *  `last_end` is where the last address bracketed ended: a dash right
 *  after one is a range.
 */
PRIVATE BOOL ip_glued_before(const char *p, size_t i, size_t last_end)
{
    if(i == 0) {
        return FALSE;
    }
    char c = p[i-1];
    if(c == ':') {
        return FALSE;
    }
    if(c == '-') {
        if(i-1 == last_end) {
            return FALSE;
        }
        return i >= 2 && (ip_word_char(p[i-2]) || (p[i-2] >= '0' && p[i-2] <= '9'));
    }
    return ip_neighbour(c);
}

PUBLIC gbuffer_t *bracket_ip_literals(gbuffer_t *src)
{
    if(!src) {
        gobj_log_error(0, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_PARAMETER,
            "msg",          "%s", "src NULL",
            NULL
        );
        return NULL;
    }

    char *p = gbuffer_cur_rd_pointer(src);
    size_t len = gbuffer_leftbytes(src);

    gbuffer_t *dst = gbuffer_create(len + len/8 + 1024, 16*1024*1024);
    if(!dst) {
        // Error already logged
        return NULL;
    }

    BOOL in_tag = FALSE;
    size_t last_end = (size_t)-1;
    size_t i = 0;
    while(i < len) {
        char c = p[i];
        if(c == '<') {
            in_tag = TRUE;
        } else if(c == '>') {
            in_tag = FALSE;
        }

        size_t total = 0;
        if(!in_tag && !ip_glued_before(p, i, last_end)) {
            size_t prefix = ipv6_head_length(p+i, len-i);
            size_t quad = ipv4_length(p+i+prefix, len-i-prefix);
            if(quad > 0 && ip_stands_alone(p, len, i+prefix+quad)) {
                total = prefix + quad;
            }
        }

        BOOL appended;
        if(total > 0) {
            appended = gbuffer_append(dst, "[", 1) == 1 &&
                gbuffer_append(dst, p+i, total) == total &&
                gbuffer_append(dst, "]", 1) == 1;
            i += total;
            last_end = i;
        } else {
            appended = gbuffer_append(dst, p+i, 1) == 1;
            i++;
        }
        if(!appended) {
            /*
             *  Error already logged. A cut body would be mailed as if it
             *  were the whole report: no body at all is the honest answer.
             */
            GBUFFER_DECREF(dst)
            return NULL;
        }
    }

    return dst;
}
