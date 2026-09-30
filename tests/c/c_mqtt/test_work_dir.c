/****************************************************************************
 *          test_work_dir.c
 *
 *          A work dir of THIS run, for the c_mqtt tests. Up to 7.25.20
 *          each test used a fixed /tmp/test_mqtt_<name>, wiped at its
 *          start: two runs at once wiped each other's store.
 *
 *          Copyright (c) 2026, ArtGins.
 *          All Rights Reserved.
 ****************************************************************************/
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <ftw.h>
#include <errno.h>
#include <unistd.h>
#include <sys/stat.h>

#include "test_work_dir.h"

/***************************************************************************
 *              Data
 ***************************************************************************/
PRIVATE char work_dir[PATH_MAX] = "";

/***************************************************************************
 *
 ***************************************************************************/
PUBLIC const char *test_work_dir_create(const char *name)
{
    const char *tmp = getenv("TMPDIR");
    if(empty_string(tmp)) {
        tmp = "/tmp";
    }

    /*
     *  <name>.<pid>.<n>, not mkdtemp(): the yuno takes its work_dir in
     *  lower case, and a mkdtemp() name has upper case letters -- the yuno
     *  wrote into another dir, which nobody removed
     */
    for(int n=0; n<100; n++) {
        char dirname[NAME_MAX];
        snprintf(dirname, sizeof(dirname), "%s.%d.%d", name, (int)getpid(), n);
        build_path(work_dir, sizeof(work_dir), tmp, dirname, NULL);
        if(mkdir(work_dir, 0700) == 0) {
            return work_dir;
        }
        if(errno != EEXIST) {
            break;
        }
    }
    printf("ERROR --> cannot create the work dir of the test: %s, %s\n", work_dir, strerror(errno));
    work_dir[0] = 0;
    return NULL;
}

/***************************************************************************
 *
 ***************************************************************************/
PUBLIC const char *test_work_dir(void)
{
    return work_dir;
}

/***************************************************************************
 *
 ***************************************************************************/
PUBLIC int test_work_dir_config(
    char *bf,
    size_t bfsize,
    const char *config,
    const char *placeholder
)
{
    size_t plen = strlen(placeholder);
    size_t wlen = strlen(work_dir);
    size_t n = 0;
    const char *p = config;
    const char *hit;
    while((hit = strstr(p, placeholder)) != NULL) {
        size_t chunk = (size_t)(hit - p);
        if(n + chunk + wlen >= bfsize) {
            printf("ERROR --> the config of the test does not fit, with its work dir\n");
            return -1;
        }
        memcpy(bf + n, p, chunk);
        n += chunk;
        memcpy(bf + n, work_dir, wlen);
        n += wlen;
        p = hit + plen;
    }
    size_t rest = strlen(p);
    if(n + rest >= bfsize) {
        printf("ERROR --> the config of the test does not fit, with its work dir\n");
        return -1;
    }
    memcpy(bf + n, p, rest + 1);
    return 0;
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE int remove_entry(const char *path, const struct stat *sb, int typeflag, struct FTW *ftwbuf)
{
    if(remove(path) < 0) {
        printf("ERROR --> cannot remove %s of the work dir of the test\n", path);
    }
    return 0;
}

/***************************************************************************
 *  Plain libc (nftw): it runs after gobj_end(), when rmrdir() can no
 *  longer log what it could not remove
 ***************************************************************************/
PUBLIC void test_work_dir_remove(void)
{
    if(work_dir[0]) {
        nftw(work_dir, remove_entry, 16, FTW_DEPTH|FTW_PHYS);
        work_dir[0] = 0;
    }
}
