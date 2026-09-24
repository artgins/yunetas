/****************************************************************************
 *          dir_listing.h
 *
 *          The answer of the agent's dir-* commands: the entries of a
 *          directory tree.
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
 *  The command response of a dir-* command: every entry (directory, file,
 *  symbolic link, hidden ones too) of the tree under `directory` whose name
 *  matches the regular expression `match`, sorted, as full paths.
 *  A tree that cannot be listed (the directory cannot be opened, no memory
 *  for an entry, a bad `match`, a directory that cannot be read) answers -1,
 *  with a comment that names the yuno and the directory and sends to the
 *  log for the cause ("<role^name>: cannot list '<directory>', see the
 *  log"), and no data: never an empty list, which reads as "the directory
 *  is empty". `kw` is owned, as by msg_iev_build_response().
 */
PUBLIC json_t *build_dir_listing_response(
    hgobj gobj,
    const char *directory,
    const char *match,
    json_t *kw          // owned
);

#ifdef __cplusplus
}
#endif
