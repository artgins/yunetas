/****************************************************************************
 *          stacktrace_with_bfd.c
 *
 *          Print stack trace with bfd library
 *          Source code copied from http://www.kigen.de/demos/stacktrace/index.html
 *
 *          The development files of binutils are required:
 *              sudo apt -y install binutils-dev
 *              sudo apt -y install libiberty-dev
 *
 *              link with bfd

 *          Copyright (c) 2023 Niyamaka.
 *          Copyright (c) 2024, ArtGins.
 *          All Rights Reserved.
 ****************************************************************************/
#ifdef __linux__
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <syslog.h>
#include <stdbool.h>
#include <limits.h>
#include <bfd.h>
#include <libiberty/demangle.h>
#include <execinfo.h>

#include "stacktrace_with_bfd.h"

/***************************************************************
 *              Constants
 ***************************************************************/

/***************************************************************
 *              Structures
 ***************************************************************/
typedef struct {
    asymbol **syms;		/* Symbol table.  */
    bfd_vma pc;
    bool found;
    const char *filename;
    const char *functionname;
    unsigned int line;
    unsigned int discriminator;
} address_info_t;

/***************************************************************
 *              Prototypes
 ***************************************************************/

/***************************************************************
 *              Data
 ***************************************************************/
PRIVATE char initialized = FALSE;
PRIVATE char program_name[NAME_MAX];

/***************************************************************************
 *
 ***************************************************************************/
PUBLIC void show_backtrace_with_bfd(loghandler_fwrite_fn_t fwrite_fn, void *h)
{
#ifdef __linux__
    void* callstack[128];
    int frames = backtrace(callstack, sizeof(callstack) / sizeof(void*));
    char** symbols = backtrace_symbols(callstack, frames);

    if (symbols == NULL) {
        return;
    }
    fwrite_fn(h, LOG_DEBUG, "===============> begin stack trace <==================");
    for (int i = 0; i < frames; i++) {
        fwrite_fn(h, LOG_DEBUG, "%s", symbols[i]);
    }
    free(symbols);
#endif
}

/***************************************************************************
 *
 ***************************************************************************/
PUBLIC int init_backtrace_with_bfd(const char *program)
{
    if(!initialized) {
        bfd_init();
        initialized = TRUE;
    }
    snprintf(program_name, sizeof(program_name), "%s", program?program:"");
    return 0;
}

#endif
