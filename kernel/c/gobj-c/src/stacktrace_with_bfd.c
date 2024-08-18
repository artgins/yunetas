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
 * Look for an address in a section.
 * This is called via bfd_map_over_sections.
 ***************************************************************************/
static void find_address_in_section (
    bfd *abfd,
    asection *section,
    void *data
) {
    address_info_t *address_info = data;

    bfd_vma vma;
    bfd_size_type size;

    if(address_info->found) {
        return;
    }

    if ((bfd_section_flags (section) & SEC_ALLOC) == 0) {
        return;
    }

    vma = bfd_section_vma (section);
    if (address_info->pc < vma)
        return;

    size = bfd_section_size (section);
    if (address_info->pc >= vma + size)
        return;

    address_info->found = bfd_find_nearest_line(
        abfd,
        section,
        address_info->syms,
        address_info->pc - vma,
        &address_info->filename,
        &address_info->functionname,
        &address_info->line
    );
}

/***************************************************************************
 * Read hexadecimal or symbolic with offset addresses,
 * translate into file_name:line_number and optionally function name.
 ***************************************************************************/
static void translate_addresses(
    bfd *abfd,
    const void *addresses[],
    int addressCount,
    asymbol **syms,
    loghandler_fwrite_fn_t fwrite_fn,
    void *h
) {
    char function_name[256];
    char file_name[256];

    for (int i = 0; i < addressCount; i++) {
        address_info_t address_info = {0};
        address_info.syms = syms;
        address_info.pc = (bfd_vma) addresses[i];

        bfd_map_over_sections(abfd, find_address_in_section, &address_info);
        if (!address_info.found) {
            snprintf(function_name, sizeof(function_name), "%lx", address_info.pc);
            fwrite_fn(h, LOG_DEBUG, "%-32s ??:?", function_name);
            continue;
        }

        if(!address_info.functionname) {
            snprintf(function_name, sizeof(function_name), "???");
        } else {
            char *s = bfd_demangle(abfd, address_info.functionname, DMGL_PARAMS | DMGL_ANSI);
            if(s) {
                snprintf(function_name, sizeof(function_name), "%s()", s);
                free(s);
            } else {
                snprintf(function_name, sizeof(function_name), "%s()", address_info.functionname);
            }
        }

        if(!address_info.filename) {
            snprintf(file_name, sizeof(file_name), "??");
        } else {
            snprintf(file_name, sizeof(file_name), "%s", address_info.filename);
        }

        fwrite_fn(h, LOG_DEBUG, "%-32s %s:%u", function_name, file_name, address_info.line);
    }
}

/***************************************************************************
 *
 ***************************************************************************/
static asymbol **slurp_symtab(bfd *abfd)
{
    asymbol **syms;		/* Symbol table.  */

    long symcount;
    long storage;
    bool dynamic = false;

    if ((bfd_get_file_flags (abfd) & HAS_SYMS) == 0)
        return NULL;

    storage = bfd_get_symtab_upper_bound (abfd);
    if (storage == 0) {
        storage = bfd_get_dynamic_symtab_upper_bound (abfd);
        dynamic = true;
    }
    if (storage < 0) {
        return NULL;
    }

    syms = (asymbol **) xmalloc (storage);
    if (dynamic) {
        symcount = bfd_canonicalize_dynamic_symtab (abfd, syms);
    } else {
        symcount = bfd_canonicalize_symtab (abfd, syms);
    }

    /* If there are no symbols left after canonicalization and
       we have not tried the dynamic symbols then give them a go.  */
    if (symcount == 0 && ! dynamic  && (storage = bfd_get_dynamic_symtab_upper_bound (abfd)) > 0) {
        free (syms);
        syms = xmalloc (storage);
        symcount = bfd_canonicalize_dynamic_symtab (abfd, syms);
    }

    if (symcount <= 0) {
        free (syms);
        syms = NULL;
    }
    return syms;
}

/***************************************************************************
 *
 ***************************************************************************/
PUBLIC void show_backtrace_with_bfd(loghandler_fwrite_fn_t fwrite_fn, void *h) {
    if(!initialized) {
        return;
    }

    void *stackTrace[128] = {0};
    int stackTraceCount = stackTraceCount = backtrace(stackTrace, 128);

    // get symbols and print stack trace
    fwrite_fn(h, LOG_DEBUG, "===============> begin stack trace <==================");

    bfd *abfd;
    char **matching;

    abfd = bfd_openr(program_name, NULL);
    if (abfd == NULL) {
        return;
    }

    /* Decompress sections.  */
    abfd->flags |= BFD_DECOMPRESS;

    if (bfd_check_format (abfd, bfd_archive)) {
        bfd_close (abfd);
        return;
    }

    if (!bfd_check_format_matches (abfd, bfd_object, &matching)) {
        bfd_close (abfd);
        return;
    }

    asymbol **syms = slurp_symtab(abfd);    /* Symbol table.  */

    translate_addresses(
        abfd,
        (const void **)stackTrace,
        stackTraceCount,
        syms,
        fwrite_fn,
        h
    );

    if(syms) {
        free(syms);
        syms = NULL;
    }

    bfd_close (abfd);

    fwrite_fn(h, LOG_DEBUG, "===============> end stack trace <==================\n");
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
