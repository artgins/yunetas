/****************************************************************************
 *          stacktrace_with_bfd.h
 *
 *          Print stack trace with bfd library
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
PUBLIC void show_backtrace_with_bfd(loghandler_fwrite_fn_t fwrite_fn, void *h);

#ifdef __cplusplus
}
#endif
