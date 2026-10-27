/****************************************************************************
 *          file_content.h
 *
 *          The content of a file, whole, for the agent's read-file and
 *          read-json commands.
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
 *  The whole content of the file `path`, NUL-terminated, in a block the
 *  caller frees with gbmem_free(); its length (without the NUL) in `*plen`.
 *  An empty file gives an empty string.
 *
 *  NULL when the file cannot be read whole, with the reason in `why`, to be
 *  said in the command's answer: it does not open, a read() fails, or it
 *  does not fit in the largest block the yuno may allocate
 *  (gbmem_get_maximum_block()). Up to 7.28.9 the agent read up to that size
 *  and cut the rest without saying so, and a read() that failed was taken
 *  for an empty file.
 */
PUBLIC char *read_file_content(
    hgobj gobj,
    const char *path,
    size_t *plen,
    char *why,
    size_t why_size
);

#ifdef __cplusplus
}
#endif
