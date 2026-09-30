/****************************************************************************
 *          ip_literals.c
 *
 *          The IPv4 addresses of a mail body, written as [a.b.c.d].
 *
 *          Copyright (c) 2026, ArtGins.
 *          All Rights Reserved.
 ****************************************************************************/
#include <string.h>
#include <strings.h>    /* strncasecmp() */

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
 *  (Chrome/142.0.0.0), not an address. Three things may follow an address
 *  without gluing it, and stay outside the brackets: the dot that ends a
 *  sentence ([a.b.c.d].), a port ([a.b.c.d]:443), and nothing else. The
 *  IPv4-mapped IPv6 form is bracketed whole ([::ffff:a.b.c.d]). Up to
 *  7.25.20 the three of them stayed bare, because '.' and ':' counted as
 *  glue -- and each one brings the phone number back.
 *
 *  The stored record keeps the plain address; this is the mail's way of
 *  writing it.
 ***************************************************************************/
#define MAPPED_PREFIX       "::ffff:"
#define MAPPED_PREFIX_LEN   (sizeof(MAPPED_PREFIX) - 1)
#define MAX_PORT_DIGITS     5

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

/*
 *  TRUE when the text at p[at] does not glue itself to what comes before:
 *  the end, a character that is no neighbour, or the dot of a sentence end.
 */
PRIVATE BOOL ip_text_ends(const char *p, size_t len, size_t at)
{
    if(at >= len || !ip_neighbour(p[at])) {
        return TRUE;
    }
    if(p[at] == '.') {
        return at+1 >= len || !ip_neighbour(p[at+1]);
    }
    return FALSE;
}

/*
 *  TRUE when the address that ends at p[at] stands on its own, a port
 *  (":443") included.
 */
PRIVATE BOOL ip_stands_alone(const char *p, size_t len, size_t at)
{
    if(ip_text_ends(p, len, at)) {
        return TRUE;
    }
    if(p[at] != ':') {
        return FALSE;
    }
    size_t digits = 0;
    while(at+1+digits < len && p[at+1+digits] >= '0' && p[at+1+digits] <= '9') {
        digits++;
    }
    if(digits == 0 || digits > MAX_PORT_DIGITS) {
        return FALSE;
    }
    return ip_text_ends(p, len, at+1+digits);
}

PUBLIC gbuffer_t *bracket_ip_literals(gbuffer_t *src)
{
    if(!src) {
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
    size_t i = 0;
    while(i < len) {
        char c = p[i];
        if(c == '<') {
            in_tag = TRUE;
        } else if(c == '>') {
            in_tag = FALSE;
        }

        size_t total = 0;
        if(!in_tag && (i == 0 || !ip_neighbour(p[i-1]))) {
            size_t prefix = 0;
            if(len - i > MAPPED_PREFIX_LEN &&
                    strncasecmp(p+i, MAPPED_PREFIX, MAPPED_PREFIX_LEN) == 0) {
                prefix = MAPPED_PREFIX_LEN;
            }
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
