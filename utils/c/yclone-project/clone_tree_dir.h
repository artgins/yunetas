/****************************************************************************
 *          CLONE_TREE_DIR.H
 *
 *          The cloning engine of yclone-project, yclone-gclass and
 *          ymake-skeleton (the two others compile this same file).
 *
 *          Copyright (c) 2015-2022 Niyamaka.
 *          Copyright (c) 2026, ArtGins.
 *          All Rights Reserved.
 ****************************************************************************/
#pragma once

#include <jansson.h>

#ifdef __cplusplus
extern "C"{
#endif

/*
 *  What to replace, and where.
 *
 *  A key is replaced only as a WHOLE WORD: the characters on each side of
 *  it must not be [A-Za-z0-9] (nor a byte of a UTF-8 sequence). '_', '-',
 *  '.', '/' and the rest are separators, so the key `time` changes in
 *  `c_time.c` and `register_c_time()`, and not in `C_TIMER` or `timeout`.
 *  Every key is tried at every position, the longest first, in one pass:
 *  what was written is never read again, so a new name that contains the
 *  old one (`foo` -> `foo_two`) is not replaced twice.
 *
 *  A file holding a NUL byte is binary: copied as it is, its name still
 *  rendered and no suffix added. Directories of VCS and build output
 *  (.git, .hg, .svn, build, cmake-build-*, node_modules, dist) are not
 *  cloned. Modes are kept. A symlink that resolves inside the tree points
 *  to its target's NEW name (rendered as the names are, with the suffix
 *  when the target is a text file); an absolute one, one that leaves the
 *  tree, or one already dangling is copied unchanged. A path that would
 *  pass PATH_MAX is an error.
 */
typedef struct {
    json_t *content_values;     // {old: new}, applied inside text files
    json_t *name_values;        // {old: new}, applied to file and directory names
    const char *text_suffix;    // appended to the name of every text file, or NULL
    json_t *untouched;          // out, {word: count}: words holding a key, left as they were
} clone_spec_t;

/*
 *  Clone the directory `src` into `dst`, which must not exist yet.
 *  Return -1 on the first error (printed), with `dst` left as far as it got.
 */
int clone_tree_dir(const char *dst, const char *src, clone_spec_t *spec);

/*
 *  Clone one file. `dst` must not exist.
 */
int clone_file(const char *dst, const char *src, clone_spec_t *spec);

/*
 *  Print spec->untouched, for the user to review by hand.
 */
void print_untouched_words(clone_spec_t *spec);

#ifdef __cplusplus
}
#endif
