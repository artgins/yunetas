/****************************************************************************
 *          test_work_dir.h
 *
 *          A work dir of THIS run, for the c_mqtt tests: two runs at once
 *          (ctest -j, or two suites on one machine) must not share one.
 *
 *          Copyright (c) 2026, ArtGins.
 *          All Rights Reserved.
 ****************************************************************************/
#pragma once

#include <yunetas.h>

#ifdef __cplusplus
extern "C"{
#endif

/***************************************************************
 *              Prototypes
 ***************************************************************/
/*
 *  Create <$TMPDIR, or /tmp>/<name>.<pid>.<n> (lower case, as the yuno
 *  takes its work_dir). Return its path, or NULL (said on stdout: it runs
 *  before the logger).
 */
PUBLIC const char *test_work_dir_create(const char *name);

/*
 *  The dir test_work_dir_create() made, "" before it.
 */
PUBLIC const char *test_work_dir(void);

/*
 *  Copy `config` into `bf`, every `placeholder` replaced by the work dir.
 *  Return 0, or -1 if it does not fit (said on stdout).
 */
PUBLIC int test_work_dir_config(
    char *bf,
    size_t bfsize,
    const char *config,
    const char *placeholder
);

/*
 *  Remove the work dir and what it holds. Call it at the end.
 */
PUBLIC void test_work_dir_remove(void);

#ifdef __cplusplus
}
#endif
