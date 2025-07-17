/***************************************************************************
 *  Función copiada de http://creativeandcritical.net/str-replace-c
 *  Description:
 *      Replaces in the string str all the occurrences of the source string old
 *      with the destination string new.
 *      The lengths of the strings old and new may differ.
 *      The string new may be of any length, but the string old must be of non-zero length -
 *      the penalty for providing an empty string for the old parameter is an infinite loop.
 *      In addition, none of the three parameters may be NULL.
 *
 *  Returns:
 *      The post-replacement string, or NULL if memory for the new string could not be allocated.
 *      Does not modify the original string.
 *      The memory for the returned post-replacement string
 *      may be deallocated with the standard library function free when it is no longer required.
 *
 *  Dependencies:
 *  For this function to compile, you will need to also
 *      #include the following files: <string.h>, <stdlib.h> and <stddef.h>.
 *  Portability:
 *      Portable to C compilers implementing the ISO C Standard,
 *      any version i.e. from C89/C90 onwards.
 *  Author:
 *      Me (Laird Shaw).
 *  Licence:
 *      Public domain.
 *  Attribution:    Optional. If you choose to indicate attribution when using this function,
 *      feel free to link to this page.
 ***************************************************************************/
#include <string.h>
#include <stddef.h>
#include <stdlib.h>
#include "00_replace_string.h"

char *replace_string(const char *str, const char *old, const char *snew)
{

    /* Adjust each of the below values to suit your needs. */

    /* Increment positions cache size initially by this number. */
    size_t cache_sz_inc = 16;
    /* Thereafter, each time capacity needs to be increased,
     * multiply the increment by this factor. */
    const size_t cache_sz_inc_factor = 3;
    /* But never increment capacity by more than this number. */
    const size_t cache_sz_inc_max = 1048576;

    char *pret, *ret = NULL;
    const char *pstr2, *pstr = str;
    size_t i, count = 0;
    ptrdiff_t *pos_cache = NULL;
    size_t cache_sz = 0;
    size_t cpylen, orglen, retlen, newlen, oldlen = strlen(old);

    /* Find all matches and cache their positions. */
    while ((pstr2 = strstr(pstr, old)) != NULL) {
        count++;

        /* Increase the cache size when necessary. */
        if (cache_sz < count) {
            cache_sz += cache_sz_inc;
            ptrdiff_t * pos_cache_ = realloc(pos_cache, sizeof(*pos_cache) * cache_sz);
            if (pos_cache_ == NULL) {
                goto end_repl_str;
            }
            pos_cache = pos_cache_;
            cache_sz_inc *= cache_sz_inc_factor;
            if (cache_sz_inc > cache_sz_inc_max) {
                cache_sz_inc = cache_sz_inc_max;
            }
        }

        pos_cache[count-1] = pstr2 - str;
        pstr = pstr2 + oldlen;
    }

    orglen = pstr - str + strlen(pstr);

    /* Allocate memory for the post-replacement string. */
    if (count > 0) {
        newlen = strlen(snew);
        retlen = orglen + (newlen - oldlen) * count;
    } else  retlen = orglen;
    ret = malloc(retlen + 1);
    if (ret == NULL) {
        goto end_repl_str;
    }

    if (count == 0) {
        /* If no matches, then just duplicate the string. */
        strcpy(ret, str);
    } else {
        /* Otherwise, duplicate the string whilst performing
         * the replacements using the position cache. */
        pret = ret;
        memcpy(pret, str, pos_cache[0]);
        pret += pos_cache[0];
        for (i = 0; i < count; i++) {
            memcpy(pret, snew, newlen);
            pret += newlen;
            pstr = str + pos_cache[i] + oldlen;
            cpylen = (i == count-1 ? orglen : pos_cache[i+1]) - pos_cache[i] - oldlen;
            memcpy(pret, pstr, cpylen);
            pret += cpylen;
        }
        ret[retlen] = '\0';
    }

end_repl_str:
    /* Free the cache and return the post-replacement string,
     * which will be NULL in the event of an error. */
    free(pos_cache);
    return ret;
}

