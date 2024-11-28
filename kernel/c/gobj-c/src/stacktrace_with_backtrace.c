/****************************************************************************
*          stacktrace_with_backtrace.c
 *
 *          Print stack trace with backtrace library
 *
 *              link with backtrace https://github.com/ianlancetaylor/libbacktrace
 *
 *          Copyright (c) 2024, ArtGins.
 *          All Rights Reserved.
 ****************************************************************************/

#ifdef __linux__
#include <stdlib.h>
#include <stdio.h>
#include <syslog.h>
#include <backtrace.h>

#include "stacktrace_with_backtrace.h"

/***************************************************************
 *              Constants
 ***************************************************************/

/***************************************************************
 *              Structures
 ***************************************************************/

/***************************************************************
 *              Prototypes
 ***************************************************************/
struct pass_data {
    loghandler_fwrite_fn_t fwrite_fn;
    void *h;
};

/***************************************************************
 *              Data
 ***************************************************************/
PRIVATE char initialized = FALSE;
PRIVATE char program_name[1024];

PRIVATE struct backtrace_state *state = NULL;

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE void error_callback(void *data_, const char *msg, int errnum)
{
    struct pass_data *data = data_;
    loghandler_fwrite_fn_t fwrite_fn =  data->fwrite_fn;
    void *h = data->h;

    fwrite_fn(h, LOG_DEBUG, "Error: %s (%d)\n", msg, errnum);
}

/***************************************************************************
 *  Callback for full backtrace information
 ***************************************************************************/
PRIVATE int full_callback(
    void *data_,
    uintptr_t pc,
    const char *filename,
    int lineno,
    const char *function
)
{
    struct pass_data *data = data_;
    loghandler_fwrite_fn_t fwrite_fn =  data->fwrite_fn;
    void *h = data->h;

    if (filename && function) {
        fwrite_fn(h, LOG_DEBUG, "%-34s %s:%d (0x%lx) ", function, filename, lineno, (unsigned long)pc);
    } else if (filename) {
        fwrite_fn(h, LOG_DEBUG, "%s:%d (0x%lx)", filename, lineno, (unsigned long)pc);
    } else {
        fwrite_fn(h, LOG_DEBUG, "(unknown) (0x%lx)", (unsigned long)pc);
    }
    return 0;
}

/***************************************************************************
 *
 ***************************************************************************/
PUBLIC void show_backtrace_with_backtrace(loghandler_fwrite_fn_t fwrite_fn, void *h) {
    if(!initialized) {
        return;
    }

    struct pass_data {
        loghandler_fwrite_fn_t fwrite_fn;
        void *h;
    } data = {
        .fwrite_fn = fwrite_fn,
        .h = h
    };

    // get symbols and print stack trace
    fwrite_fn(h, LOG_DEBUG, "===============> begin stack trace <==================");

    backtrace_full(state, 0, full_callback, error_callback, &data);
}

/***************************************************************************
 *
 ***************************************************************************/
PUBLIC int init_backtrace_with_backtrace(const char *program)
{
    if(!initialized) {
        // Initialize the backtrace state
        state = backtrace_create_state(program, 1, error_callback, NULL);

        if (!state) {
            print_error(
                PEF_CONTINUE,
                "Failed to create backtrace state in %s",
                program
            );
            return -1;
        }

        initialized = TRUE;
    }
    snprintf(program_name, sizeof(program_name), "%s", program?program:"");
    return 0;
}

#endif
