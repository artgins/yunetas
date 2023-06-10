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
// callback for symbol information
typedef void(*SymbolFunction)(
    const void *address,
    const char *fileName,
    const char *symbolName,
    uint lineNb,
    void *userData
);

// section info
typedef struct {
    const asymbol **symbols;
    ulong symbolCount;
    bfd_vma address;

    bool sectionFound;
    bool symbolFound;
    const char *fileName;
    const char *symbolName;
    uint lineNb;
} AddressInfo;

// file match info
typedef struct {
    bool found;
    const void *address;

    const char *fileName;
    void *base;
    void *hdr;
} FileMatchInfo;

// symbol line info
typedef struct {
    char **lines;
    uint lineCount;
    uint maxLines;
} SymbolLineInfo;

/***************************************************************
 *              Prototypes
 ***************************************************************/

/***************************************************************
 *              Data
 ***************************************************************/
PRIVATE char initialized = FALSE;
PRIVATE char program_name[NAME_MAX];

/***********************************************************************\
* Name   : readSymbolTable
* Purpose: read symbol table from BFD
* Input  : abfd - BFD handle
* Output : symbols     - array with symbols
*          symbolCount - number of entries in array
* Return : TRUE iff symbol table read
* Notes  : -
\***********************************************************************/
PRIVATE bool readSymbolTable(
    bfd *abfd,
    const asymbol **symbols[],
    ulong *symbolCount
) {
    uint size;
    long n;
    if(!symbols || !symbolCount) {
        return FALSE;
    }

    // check if symbols available
    if ((bfd_get_file_flags(abfd) & HAS_SYMS) == 0) {
        return FALSE;
    }

    // read mini-symbols
    (*symbols) = NULL;
    n = bfd_read_minisymbols(
        abfd,
        FALSE,  // not dynamic
        (void **) symbols,
        &size
    );
    if (n == 0) {
        if ((*symbols) != NULL) {
            free(*symbols);
        }

        (*symbols) = NULL;
        n = bfd_read_minisymbols(
            abfd,
            TRUE /* dynamic */ ,
            (void **) symbols,
            &size
        );
    }
    if (n < 0) {
        return FALSE;
    } else if (n == 0) {
        return FALSE;
    }
    (void) size;
    (*symbolCount) = (ulong) n;

    return TRUE;
}

/***********************************************************************\
* Name   : freeSymbolTable
* Purpose: free symbol table
* Input  : symbols     - symbol array
*          symbolCount - number of entries in symbol array
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/
PRIVATE void freeSymbolTable(
    const asymbol *symbols[],
    ulong symbolCount
) {
    if(!symbols) {
        return;
    }
    // unused
    (void) symbolCount;
    free(symbols);
}

/***********************************************************************\
* Name   : demangleSymbolName
* Purpose: demangle C++ name
* Input  : symbolName - symbolname
*          demangledSymbolName     - variable for demangled symbol name
*          demangledSymbolNameSize - max. length of demangled symbol name
* Output : -
* Return : TRUE iff name demangled
* Notes  : -
\***********************************************************************/
PRIVATE bool demangleSymbolName(
    const char *symbolName,
    char *demangledSymbolName,
    uint demangledSymbolNameSize
) {
    char *s;
    if(!symbolName || !demangledSymbolName) {
        return FALSE;
    }

    s = bfd_demangle(NULL, symbolName, DMGL_ANSI | DMGL_PARAMS);
    if (s != NULL) {
        strncpy(demangledSymbolName, s, demangledSymbolNameSize);
        free(s);
        return TRUE;
    } else {
        strncpy(demangledSymbolName, symbolName, demangledSymbolNameSize);
        return FALSE;
    }
}

/***********************************************************************\
* Name   : findAddressInSection
* Purpose: callback for find symbol in section
* Input  : abfd     - BFD handle
*          section  - section
*          userData - callback user data
* Output : -
* Return : -
* Notes  : fills in data into AddressInfo structure
\***********************************************************************/
PRIVATE void findAddressInSection(bfd *abfd, asection *section, void *data)
{
    AddressInfo *addressInfo = (AddressInfo *) data;
    bfd_vma vma;
    bfd_size_type size;

    // TODO PIERDE en esta function
    if(!addressInfo) {
        return;
    }
    // check if already found
    if (addressInfo->symbolFound) {
        return;
    }

    // find section
    if ((bfd_section_flags(section) & SEC_ALLOC) == 0)
    {
        return;
    }
    vma = bfd_section_vma(section);
    size = bfd_section_size(section);
    if ((addressInfo->address < vma)
        || (addressInfo->address >= vma + size)
        ) {
        return;
    }
    addressInfo->sectionFound = TRUE;

    // TODO pierde por aqui
    // find symbol
    addressInfo->symbolFound = bfd_find_nearest_line(
        abfd,
        section,
        (asymbol **) addressInfo->symbols,
        addressInfo->address - vma,
        &addressInfo->fileName,
        &addressInfo->symbolName,
        &addressInfo->lineNb
    );
}

/***********************************************************************\
* Name   : addressToSymbolInfo
* Purpose: get symbol info for address
* Input  : abfd           - BFD handle
*          symbols        - symbol array
*          symbolCount    - number of entries in symbol array
*          address        - address
*          symbolFunction - callback function for symbol
*          symbolUserData - callback user data
* Output : -
* Return : TRUE iff symbol information found
* Notes  : -
\***********************************************************************/
PRIVATE bool addressToSymbolInfo(
    bfd *abfd,
    const asymbol *symbols[],
    ulong symbolCount,
    bfd_vma address,
    SymbolFunction symbolFunction,
    void *symbolUserData
) {
    if(!symbolFunction) {
        return FALSE;
    }

    AddressInfo addressInfo = {0};

    // find symbol
    addressInfo.symbols = symbols;
    addressInfo.symbolCount = symbolCount;
    addressInfo.address = address;
    addressInfo.symbolFound = FALSE;

    // TODO pierde por aqui
    bfd_map_over_sections(abfd, findAddressInSection, (PTR) &addressInfo);

    if (!addressInfo.sectionFound) {
        return FALSE;
    }
    if (!addressInfo.symbolFound) {
        return FALSE;
    }

    while (addressInfo.symbolFound) {
        char buffer[256];
        const char *symbolName = NULL;
        const char *fileName = NULL;

        // get symbol data
        if ((addressInfo.symbolName != NULL) && ((*addressInfo.symbolName) != '\0')) {
            if (demangleSymbolName(addressInfo.symbolName, buffer, sizeof(buffer))) {
                symbolName = buffer;
            } else {
                symbolName = addressInfo.symbolName;
            }
        } else {
            symbolName = NULL;
        }

        if (addressInfo.fileName != NULL) {
            fileName = addressInfo.fileName;
        } else {
            fileName = NULL;
        }

        // handle found symbol TODO en esta parece que no pierde
        symbolFunction((void *) address, fileName, symbolName, addressInfo.lineNb, symbolUserData);

        // get next information
        addressInfo.symbolFound = bfd_find_inliner_info(
            abfd,
            &addressInfo.fileName,
            &addressInfo.symbolName,
            &addressInfo.lineNb
        );
    }

    return TRUE;
}

/***********************************************************************\
* Name   : openBFD
* Purpose: open BFD and read symbol table
* Input  : fileName - file name
* Output : symbols     - symbol array
*          symbolCount - number of entries in symbol array
* Return : TRUE iff BFD opened
* Notes  : -
\***********************************************************************/
PRIVATE bfd *openBFD(
    const char *fileName,
    const asymbol **symbols[],
    ulong *symbolCount
) {
    bfd *abfd;
    char **matching;
    if(!fileName || !symbols || !symbolCount) {
        return NULL;
    }

    abfd = bfd_openr(fileName, NULL);
    if (abfd == NULL) {
        //fprintf(stderr, "ERROR: can not open file '%s' (error: %s)\n", fileName, strerror(errno));
        return NULL;
    }

    if (bfd_check_format(abfd, bfd_archive)) {
        //fprintf(stderr, "ERROR: invalid format\n");
        bfd_close(abfd);
        return NULL;
    }

    if (!bfd_check_format_matches(abfd, bfd_object, &matching)) {
        if (bfd_get_error() == bfd_error_file_ambiguously_recognized) {
            free(matching);
        }
        //fprintf(stderr, "ERROR: format does not match\n");
        bfd_close(abfd);
        return NULL;
    }

    if (!readSymbolTable(abfd, symbols, symbolCount)) {
        bfd_close(abfd);
        return NULL;
    }

    return abfd;
}

/***********************************************************************\
* Name   : closeBFD
* Purpose: close BFD
* Input  : abfd - BFD handle
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/
PRIVATE void closeBFD(
    bfd *abfd,
    const asymbol *symbols[],
    ulong symbolCount
) {
    if(!abfd) {
        return;
    }
    freeSymbolTable(symbols, symbolCount);
    bfd_close(abfd);
}

/***********************************************************************\
* Name   : storeSymbolLine
* Purpose: callback to store symbol line into array
* Input  : address    - address
*          fileName   - file name
*          symbolName - symbol name
*          lineNb     - line number
*          userData   - callback user data
* Output : -
* Return : -
* Notes  : get data from SymbolLineInfo structure
\***********************************************************************/
PRIVATE void storeSymbolLine(
    const void *address,
    const char *fileName,
    const char *symbolName,
    uint lineNb,
    void *userData
) {
    SymbolLineInfo *symbolLineInfo = (SymbolLineInfo *) userData;
    char line[512];

    if(!symbolLineInfo || !symbolLineInfo->lines) {
        return;
    }

    if (symbolLineInfo->lineCount < symbolLineInfo->maxLines) {
        if (fileName == NULL) {
            fileName = "<unknown file>";
        }
        if (symbolName == NULL) {
            symbolName = "<unknown symbol>";
        }
        snprintf(line, sizeof(line), "%-32s %s:%u",
            symbolName,
            fileName,
            lineNb
        );

        symbolLineInfo->lines[symbolLineInfo->lineCount] = strdup(line);
        symbolLineInfo->lineCount++;
    }
}

/***********************************************************************\
* Name   : getSymbolInfoFromFile
* Purpose: get symbol information from file
* Input  : fileName       - file name
*          address        - address
*          symbolFunction - callback function for symbol
*          symbolUserData - callback user data
* Output : -
* Return : TRUE iff symbol read
* Notes  : -
\***********************************************************************/
PRIVATE bool getSymbolInfoFromFile(
    const char *fileName,
    bfd_vma address,
    SymbolFunction symbolFunction,
    void *symbolUserData
) {
    bfd *abfd;
    const asymbol **symbols;
    ulong symbolCount;
    bool result = false;

    if(!fileName) {
        return FALSE;
    }

    abfd = openBFD(fileName, &symbols, &symbolCount);
    if (abfd == NULL) {
        return 0;
    }
    // TODO pierde en next sentence
    result = addressToSymbolInfo(abfd, symbols, symbolCount, address, symbolFunction, symbolUserData);
    closeBFD(abfd, symbols, symbolCount);
    return result;
}

/***********************************************************************\
* Name   : getSymbolInfo
* Purpose: get symbol information
* Input  : executableFileName - executable name
*          addresses          - addresses
*          addressCount       - number of addresses
*          symbolFunction     - callback function for symbol
*          symbolUserData     - callback user data
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/
PRIVATE void getSymbolInfo(
    const char *executableFileName,
    const void *addresses[],
    uint addressCount,
    SymbolFunction symbolFunction,
    void *symbolUserData
) {
    uint i;
    if(!executableFileName || !addresses || !symbolFunction) {
        return;
    }

    for (i = 0; i < addressCount; i++) {
        getSymbolInfoFromFile(executableFileName,
            (bfd_vma) addresses[i],
            symbolFunction,
            symbolUserData
        );
    }
}

/***********************************************************************\
* Name   : getSymbolInfoLines
* Purpose: get symbol information lines
* Input  : executableFileName - executable name
*          addresses          - addresses
*          addressCount       - number of addresses
*          lines              - lines array to fill
*          maxLines           - max. number of lines
* Output : -
* Return : number of lines in array or 0 on error
* Notes  : Convenient function to get symbol informations as array of
*          strings
\***********************************************************************/
PRIVATE uint getSymbolInfoLines(
    const char *executableFileName,
    const void *addresses[],
    uint addressCount,
    char *lines[],
    uint maxLines
) {
    SymbolLineInfo symbolLineInfo;
    if(!executableFileName || !lines || maxLines <= 0) {
        return 0;
    }
    symbolLineInfo.lines = lines;
    symbolLineInfo.lineCount = 0;
    symbolLineInfo.maxLines = maxLines;
    getSymbolInfo(executableFileName, addresses, addressCount, storeSymbolLine, &symbolLineInfo);

    return symbolLineInfo.lineCount;
}

/***********************************************************************\
* Name   : freeSymbolInfoLines
* Purpose: free symbol information
* Input  : lines     - lines
*          lineCount - line count
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/
PRIVATE void freeSymbolInfoLines(char *lines[], uint lineCount)
{
    uint i;
    if(!lines) {
        return;
    }

    for (i = 0; i < lineCount; i++) {
        if(lines[i]) {
            free(lines[i]);
            lines[i] = NULL;
        }
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

    const uint MAX_LINES = 64;

    char *lines[MAX_LINES];
    uint lineCount;
    uint i;

    void *stackTrace[64] = {0};
    uint stackTraceCount;
    stackTraceCount = backtrace(stackTrace, 64);

    // TODO HASTA aqui Bien, no pierde

    // get symbols and print stack trace
    fwrite_fn(h, LOG_DEBUG, "===============> begin stack trace <==================");

    lineCount = getSymbolInfoLines(
        program_name,
        (const void **) stackTrace,
        stackTraceCount,
        lines,
        MAX_LINES
    );

    for (i = 0; i < lineCount; i++) {
        fwrite_fn(h, LOG_DEBUG, "%s", lines[i]);
    }
    freeSymbolInfoLines(lines, lineCount);

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
