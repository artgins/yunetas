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
 *          Copyright (c) Torsten Rupp
 *          All Rights Reserved.
 ****************************************************************************/
#ifdef __linux__

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <syslog.h>
#include <ctype.h>
#include <stdbool.h>
#include <limits.h>
#include <bfd.h>
#include <libiberty/demangle.h>
#include <execinfo.h>
#endif

#include "stacktrace_with_bfd.h"

/***************************************************************
 *              Constants
 ***************************************************************/

/***************************************************************
 *              Structures
 ***************************************************************/

/***************************************************************
 *              Prototypes
 ***************************************************************/

/***************************************************************
 *              Data
 ***************************************************************/
PRIVATE char initialized = FALSE;
PRIVATE char program_name[NAME_MAX];

static int demangle_flags = DMGL_PARAMS | DMGL_ANSI | DMGL_NO_RECURSE_LIMIT;
static long symcount;
static asymbol **syms;		/* Symbol table.  */

static bool unwind_inlines = TRUE;	/* -i, unwind inlined functions. */
static bool with_addresses = TRUE;	/* -a, show addresses.  */
static bool with_functions = TRUE;	/* -f, show function names.  */
static bool do_demangle = TRUE;	/* -C, demangle names.  */
static bool pretty_print = TRUE;	/* -p, print on one line.  */
static bool base_names = FALSE;		/* -s, strip directory names.  */

/***************************************************************************
 * Lookup a symbol with offset in symbol table
 ***************************************************************************/
static bfd_vma lookup_symbol (bfd *abfd, char *sym, size_t offset)
{
    long i;

    for (i = 0; i < symcount; i++) {
        if (!strcmp (syms[i]->name, sym))
            return syms[i]->value + offset + bfd_asymbol_section (syms[i])->vma;
    }
    /* Try again mangled */
    for (i = 0; i < symcount; i++) {
        char *d = bfd_demangle (abfd, syms[i]->name, demangle_flags);
        bool match = d && !strcmp (d, sym);
        free (d);

        if (match) {
            return syms[i]->value + offset + bfd_asymbol_section(syms[i])->vma;
        }
    }
    return 0;
}

/***************************************************************************
 * Split an symbol+offset expression. adr is modified.
 ***************************************************************************/
static bool is_symbol (char *adr, char **symp, size_t *offset)
{
    char *end;

    while (isspace (*adr))
        adr++;
    if (isdigit (*adr) || *adr == 0)
        return false;
    /* Could be either symbol or hex number. Check if it has +.  */
    if (toupper(*adr) >= 'A' && toupper(*adr) <= 'F' && !strchr (adr, '+'))
        return false;

    *symp = adr;
    while (*adr && !isspace (*adr) && *adr != '+')
        adr++;
    end = adr;
    while (isspace (*adr))
        adr++;
    *offset = 0;
    if (*adr == '+')
    {
        adr++;
        *offset = strtoul(adr, NULL, 0);
    }
    *end = 0;
    return true;
}

/***************************************************************************
 * Look for an address in a section.
 * This is called via bfd_map_over_sections.
 ***************************************************************************/
/* These global variables are used to pass information between
   translate_addresses and find_address_in_section.  */

static bfd_vma pc;
static const char *filename;
static const char *functionname;
static unsigned int line;
static unsigned int discriminator;
static bool found;

static void find_address_in_section (
    bfd *abfd,
    asection *section,
     void *data ATTRIBUTE_UNUSED
) {
    bfd_vma vma;
    bfd_size_type size;

    if (found)
        return;

    if ((bfd_section_flags (section) & SEC_ALLOC) == 0)
        return;

    vma = bfd_section_vma (section);
    if (pc < vma)
        return;

    size = bfd_section_size (section);
    if (pc >= vma + size)
        return;

    found = bfd_find_nearest_line_discriminator (
        abfd,
        section,
        syms,
        pc - vma,
        &filename, &functionname,
        &line, &discriminator
    );
}

/***************************************************************************
 * Read hexadecimal or symbolic with offset addresses,
 * translate into file_name:line_number and optionally function name.
 ***************************************************************************/
static void translate_addresses(
    bfd *abfd,
    const void *addresses[],
    uint addressCount
) {
    for (int i = 0; i < addressCount; i++) {
        pc = (bfd_vma) addresses[i];
        if (bfd_get_flavour (abfd) == bfd_target_elf_flavour) {
            printf("KKKK\n");
//            const struct elf_backend_data *bed = get_elf_backend_data(abfd);
//            bfd_vma sign = (bfd_vma) 1 << (bed->s->arch_size - 1);
//
//            pc &= (sign << 1) - 1;
//            if (bed->sign_extend_vma)
//                pc = (pc ^ sign) - sign;
        }

        if (with_addresses)  {
            printf ("0x");
            bfd_printf_vma (abfd, pc);
            if (pretty_print) {
                printf(": ");
            } else {
                printf("\n");
            }
        }

        bfd_map_over_sections(abfd, find_address_in_section, NULL);
        if (!found) {
            if (with_functions)  {
                if (pretty_print) {
                    printf("?? ");
                } else {
                    printf("??\n");
                }
            }
            printf ("??:0\n");
        } else {
            while (1)  {
                if (with_functions) {
                    const char *name;
                    char *alloc = NULL;

                    name = functionname;
                    if (name == NULL || *name == '\0') {
                        name = "??";
                    } else if (do_demangle)  {
                        alloc = bfd_demangle (abfd, name, demangle_flags);
                        if (alloc != NULL)
                            name = alloc;
                    }

                    printf ("%s", name);
                    if (pretty_print) {
                        /* Note for translators:  This printf is used to join the
                           function name just printed above to the line number/
                           file name pair that is about to be printed below.  Eg:

                             foo at 123:bar.c  */
                        printf(" at ");
                    } else {
                        printf("\n");
                    }

                    free (alloc);
                }

                if (base_names && filename != NULL) {
                    char *h;

                    h = strrchr (filename, '/');
                    if (h != NULL)
                        filename = h + 1;
                }

                printf ("%s:", filename ? filename : "??");
                if (line != 0) {
                    if (discriminator != 0)
                        printf ("%u (discriminator %u)\n", line, discriminator);
                    else
                        printf ("%u\n", line);
                } else {
                    printf("?\n");
                }
                if (!unwind_inlines) {
                    found = false;
                } else {
                    found = bfd_find_inliner_info (
                        abfd,
                        &filename,
                        &functionname,
                        &line
                    );
                }
                if (!found) {
                    break;
                }
                if (pretty_print) {
                    /* Note for translators: This printf is used to join the
                       line number/file name pair that has just been printed with
                       the line number/file name pair that is going to be printed
                       by the next iteration of the while loop.  Eg:

                         123:bar.c (inlined by) 456:main.c  */
                    printf(" (inlined by) ");
                }
            }
        }

        /* fflush() is essential for using this command as a server
           child process that reads addresses from a pipe and responds
           with line number information, processing one address at a
           time.  */
        fflush (stdout);
    }
}

/***************************************************************************
 *
 ***************************************************************************/
static void slurp_symtab (bfd *abfd)
{
    long storage;
    bool dynamic = false;

    if ((bfd_get_file_flags (abfd) & HAS_SYMS) == 0)
        return;

    storage = bfd_get_symtab_upper_bound (abfd);
    if (storage == 0) {
        storage = bfd_get_dynamic_symtab_upper_bound (abfd);
        dynamic = true;
    }
    if (storage < 0) {
        //bfd_nonfatal (bfd_get_filename (abfd));
        return;
    }

    syms = (asymbol **) xmalloc (storage);
    if (dynamic) {
        symcount = bfd_canonicalize_dynamic_symtab (abfd, syms);
    } else {
        symcount = bfd_canonicalize_symtab (abfd, syms);
    }
    if (symcount < 0) {
        //bfd_nonfatal(bfd_get_filename(abfd));
    }

    /* If there are no symbols left after canonicalization and
       we have not tried the dynamic symbols then give them a go.  */
    if (symcount == 0
        && ! dynamic
        && (storage = bfd_get_dynamic_symtab_upper_bound (abfd)) > 0)
    {
        free (syms);
        syms = xmalloc (storage);
        symcount = bfd_canonicalize_dynamic_symtab (abfd, syms);
    }

    /* PR 17512: file: 2a1d3b5b.
       Do not pretend that we have some symbols when we don't.  */
    if (symcount <= 0)
    {
        free (syms);
        syms = NULL;
    }
}

/***************************************************************************
 *
 ***************************************************************************/
PUBLIC void show_backtrace_with_bfd(loghandler_fwrite_fn_t fwrite_fn, void *h) {
#ifdef __linux__
    if(!initialized) {
        return;
    }

    void *stackTrace[128] = {0};
    uint stackTraceCount;
    stackTraceCount = backtrace(stackTrace, 128);

    // get symbols and print stack trace
    fwrite_fn(h, LOG_DEBUG, "===============> begin stack trace <==================");

    bfd *abfd;
    //asection *section = NULL;
    char **matching;

    abfd = bfd_openr(program_name, NULL);
    if (abfd == NULL) {
        // TODO bfd_fatal (file_name);
        return;
    }

    /* Decompress sections.  */
    abfd->flags |= BFD_DECOMPRESS;

    if (bfd_check_format (abfd, bfd_archive)) {
        printf("%s: cannot get addresses from archive", program_name);
        bfd_close (abfd);
        return;
    }

    if (!bfd_check_format_matches (abfd, bfd_object, &matching)) {
        printf("%s: bfd_check_format_matches() failed", program_name);
        bfd_close (abfd);
        return;
    }

//    if (section_name != NULL) {
//        section = bfd_get_section_by_name (abfd, section_name);
//        if (section == NULL)
//        {
//            non_fatal (_("%s: cannot find section %s"), file_name, section_name);
//            bfd_close (abfd);
//            return 1;
//        }
//    } else {
//        section = NULL;
//    }

    slurp_symtab (abfd);

    translate_addresses(
        abfd,
        (const void **)stackTrace,
        stackTraceCount
    );

    if(syms) {
        free(syms);
        syms = NULL;
    }

    bfd_close (abfd);


    fwrite_fn(h, LOG_DEBUG, "===============> end stack trace <==================\n");

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
    snprintf(program_name, sizeof(program_name), "%s", program);
    return 0;
}
