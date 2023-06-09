/****************************************************************************
 *          stacktrace_with_bfd.h
 *
 *          Print stack trace with bfd library
 *
 *          The development files of binutils are required:
 *              sudo apt -y install binutils-dev
 *              sudo apt -y install libiberty-dev
 *
 *          Copyright (c) 2023 Niyamaka.
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
PUBLIC int init_backtrace_with_bfd(const char *program);
PUBLIC void show_backtrace_with_bfd(loghandler_fwrite_fn_t fwrite_fn, void *h);

#ifdef __cplusplus
}
#endif
