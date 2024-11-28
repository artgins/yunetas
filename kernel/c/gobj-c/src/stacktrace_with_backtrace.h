/****************************************************************************
 *          stacktrace_with_backtrace.h
 *
 *          Print stack trace with backtrace library
 *
 *              link with backtrace https://github.com/ianlancetaylor/libbacktrace
 *
 *          Copyright (c) 2024, ArtGins.
 *          All Rights Reserved.
 ****************************************************************************/
#pragma once

#include "gobj.h"

#ifdef __cplusplus
extern "C"{
#endif

/***************************************************************
 *              Prototypes
 ***************************************************************/
int init_backtrace_with_backtrace(const char *program);
void show_backtrace_with_backtrace(loghandler_fwrite_fn_t fwrite_fn, void *h);

#ifdef __cplusplus
}
#endif
