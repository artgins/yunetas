/***************************************************************************
 *          replace_string.c
 *
 *          A copy of `str` with every occurrence of `old` replaced by
 *          `snew`: one pass counts the occurrences, so the result is
 *          allocated once with its exact size, and a second one copies.
 *
 *          Written for Yuneta: up to 7.28.4 this file was the str_replace()
 *          of Laird Shaw, with libc's malloc() and free().
 *
 *          Copyright (c) 2026, ArtGins.
 *          All Rights Reserved.
 ***************************************************************************/
#include <string.h>
#include "gtypes.h"
#include "gbmem.h"
#include "glogger.h"
#include "replace_string.h"

/***************************************************************************
 *
 ***************************************************************************/
PUBLIC char *replace_string(const char *str, const char *old, const char *snew)
{
    if(!str || !old || !*old || !snew) {
        gobj_log_error(0, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_PARAMETER,
            "msg",          "%s", "replace_string(): str, old or snew missing, or old empty",
            NULL
        );
        return NULL;
    }

    size_t str_len = strlen(str);
    size_t old_len = strlen(old);
    size_t new_len = strlen(snew);

    size_t count = 0;
    for(const char *p = strstr(str, old); p; p = strstr(p + old_len, old)) {
        count++;
    }

    /*
     *  The occurrences do not overlap, so count * old_len <= str_len
     */
    size_t ret_len = str_len - count * old_len + count * new_len;
    char *ret = gbmem_malloc(ret_len + 1);
    if(!ret) {
        // Error already logged
        return NULL;
    }

    char *w = ret;
    const char *r = str;
    for(const char *p = strstr(r, old); p; p = strstr(r, old)) {
        memcpy(w, r, (size_t)(p - r));
        w += p - r;
        memcpy(w, snew, new_len);
        w += new_len;
        r = p + old_len;
    }
    size_t tail_len = str_len - (size_t)(r - str);
    memcpy(w, r, tail_len);
    w[tail_len] = '\0';

    return ret;
}
