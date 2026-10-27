/****************************************************************************
 *          file_content.c
 *
 *          The content of a file, whole. See file_content.h.
 *
 *          It lives out of c_agent.c, which no test compiles, so that
 *          tests/c/c_agent_file_content can read files of every size
 *          against a small maximum block.
 *
 *          Copyright (c) 2026, ArtGins.
 *          All Rights Reserved.
 ****************************************************************************/
#include <string.h>
#include <stdio.h>
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>

#include "file_content.h"

/***************************************************************************
 *              Constants
 ***************************************************************************/
#define FIRST_SIZE  4096    // when the size is not known (not a regular file, or 0 for /proc)

/***************************************************************************
 *  See file_content.h
 ***************************************************************************/
PUBLIC char *read_file_content(
    hgobj gobj,
    const char *path,
    size_t *plen,
    char *why,
    size_t why_size
)
{
    *plen = 0;
    *why = 0;

    int fd = open(path, O_RDONLY|O_CLOEXEC);
    if(fd < 0) {
        snprintf(why, why_size, "cannot open it: %s", strerror(errno));
        return NULL;
    }

    size_t max = gbmem_get_maximum_block();
    size_t size = FIRST_SIZE;
    struct stat st;
    if(fstat(fd, &st)==0 && S_ISREG(st.st_mode) && st.st_size > 0) {
        if((uint64_t)st.st_size >= (uint64_t)max) {
            snprintf(why, why_size,
                "it has %llu bytes, and a yuno reads at most %zu at once",
                (unsigned long long)st.st_size, max > 0? max - 1 : 0
            );
            close(fd);
            return NULL;
        }
        size = (size_t)st.st_size + 1;
    }
    if(size > max) {
        size = max;
    }

    char *s = gbmem_malloc(size);
    if(!s) {
        snprintf(why, why_size, "no memory for %zu bytes", size);
        close(fd);
        return NULL;
    }

    size_t len = 0;
    while(1) {
        if(len == size - 1) {
            /*
             *  Full: read one byte more to tell the end of the file from a
             *  file that does not fit (a size that grew, or was unknown)
             */
            char probe;
            ssize_t n = read(fd, &probe, 1);
            if(n < 0 && errno == EINTR) {
                continue;
            }
            if(n < 0) {
                snprintf(why, why_size, "read() failed: %s", strerror(errno));
                break;
            }
            if(n == 0) {
                break;
            }
            if(size >= max) {
                snprintf(why, why_size,
                    "it has more than %zu bytes, the most a yuno reads at once", max - 1
                );
                break;
            }
            size_t new_size = (size > max / 2)? max : size * 2;
            char *t = gbmem_realloc(s, new_size);
            if(!t) {
                snprintf(why, why_size, "no memory for %zu bytes", new_size);
                break;
            }
            s = t;
            size = new_size;
            s[len++] = probe;
            continue;
        }

        ssize_t n = read(fd, s + len, size - 1 - len);
        if(n < 0 && errno == EINTR) {
            continue;
        }
        if(n < 0) {
            snprintf(why, why_size, "read() failed: %s", strerror(errno));
            break;
        }
        if(n == 0) {
            break;
        }
        len += (size_t)n;
    }
    close(fd);

    if(*why) {
        gbmem_free(s);
        return NULL;
    }

    s[len] = 0;
    *plen = len;
    return s;
}
