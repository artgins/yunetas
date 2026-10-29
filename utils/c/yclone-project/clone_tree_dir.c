/****************************************************************************
 *          CLONE_TREE_DIR.C
 *          The cloning engine of yclone-project, yclone-gclass and
 *          ymake-skeleton (see clone_tree_dir.h for what it replaces).
 *
 *          Up to 7.28.6 each tool had its own copy, and all of them broke
 *          what they cloned: keys were replaced as substrings (`time` turned
 *          C_TIMER into C_CLOCKR), a new name holding the old one was
 *          replaced again for every occurrence on a line (myyuno_two_two),
 *          every file went through fgets()/strlen() (a binary was cut at its
 *          first NUL), build/ and .git/ were cloned, and modes were lost.
 *
 *          Copyright (c) 2015-2022 Niyamaka.
 *          Copyright (c) 2026, ArtGins.
 *          All Rights Reserved.
 ****************************************************************************/
#include <sys/types.h>
#include <sys/stat.h>
#include <dirent.h>
#include <fcntl.h>
#include <libgen.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <helpers.h>
#include "clone_tree_dir.h"

/***************************************************************************
 *      Constants
 ***************************************************************************/
PRIVATE const char *skipped_dirs[] = {
    ".git", ".hg", ".svn", "build", "node_modules", "dist", 0
};
#define SKIPPED_DIR_PREFIX "cmake-build-"

/***************************************************************************
 *      Structures
 ***************************************************************************/
typedef struct {
    const char *old;
    size_t old_len;
    const char *new;
    size_t new_len;
} replace_key_t;

typedef struct {
    char *p;
    size_t len;
    size_t size;
} out_buf_t;

/***************************************************************************
 *      Prototypes
 ***************************************************************************/
PRIVATE int clone_dir(
    const char *dst,
    const char *src,
    const char *real_root,
    clone_spec_t *spec
);

/***************************************************************************
 *  A byte that makes a word: on each side of a key there must be none
 ***************************************************************************/
PRIVATE BOOL is_word_byte(unsigned char c)
{
    return (c >= '0' && c <= '9') || (c >= 'a' && c <= 'z') ||
        (c >= 'A' && c <= 'Z') || c >= 0x80;
}

/***************************************************************************
 *  A byte of an identifier, to name the word a key was found inside of
 ***************************************************************************/
PRIVATE BOOL is_ident_byte(unsigned char c)
{
    return is_word_byte(c) || c == '_';
}

/***************************************************************************
 *  Append n bytes to the output
 ***************************************************************************/
PRIVATE int out_append(out_buf_t *out, const char *s, size_t n)
{
    if(out->len + n + 1 > out->size) {
        size_t size = out->size? out->size : 4096;
        while(out->len + n + 1 > size) {
            size *= 2;
        }
        char *p = gbmem_realloc(out->p, size);
        if(!p) {
            fprintf(stderr, "ERROR: no memory for %zu bytes\n", size);
            return -1;
        }
        out->p = p;
        out->size = size;
    }
    memcpy(out->p + out->len, s, n);
    out->len += n;
    out->p[out->len] = 0;
    return 0;
}

/***************************************************************************
 *  The keys of a {old: new} dict, the longest first
 ***************************************************************************/
PRIVATE int cmp_key_len(const void *a, const void *b)
{
    const replace_key_t *ka = a;
    const replace_key_t *kb = b;
    if(ka->old_len != kb->old_len) {
        return ka->old_len < kb->old_len ? 1 : -1;
    }
    return strcmp(ka->old, kb->old);
}

PRIVATE replace_key_t *build_keys(json_t *values, size_t *n_keys)
{
    *n_keys = 0;
    size_t size = json_object_size(values);
    replace_key_t *keys = gbmem_malloc(sizeof(replace_key_t) * (size + 1));
    if(!keys) {
        fprintf(stderr, "ERROR: no memory for the keys\n");
        return NULL;
    }

    const char *old;
    json_t *jn_new;
    json_object_foreach(values, old, jn_new) {
        const char *new = json_string_value(jn_new);
        if(empty_string(old) || !new) {
            continue;
        }
        keys[*n_keys].old = old;
        keys[*n_keys].old_len = strlen(old);
        keys[*n_keys].new = new;
        keys[*n_keys].new_len = strlen(new);
        (*n_keys)++;
    }
    qsort(keys, *n_keys, sizeof(replace_key_t), cmp_key_len);
    return keys;
}

/***************************************************************************
 *  Count the word around a key that is not a whole word
 ***************************************************************************/
PRIVATE void note_untouched(
    clone_spec_t *spec,
    const char *in,
    size_t len,
    size_t pos,
    size_t key_len
) {
    if(!spec->untouched) {
        return;
    }
    size_t start = pos;
    while(start > 0 && is_ident_byte((unsigned char)in[start-1])) {
        start--;
    }
    size_t end = pos + key_len;
    while(end < len && is_ident_byte((unsigned char)in[end])) {
        end++;
    }
    char word[NAME_MAX + 1];
    snprintf(word, sizeof(word), "%.*s", (int)(end - start), in + start);

    // A word is a KEY of the dict, never a kw path
    json_int_t count = json_integer_value(json_object_get(spec->untouched, word));
    json_object_set_new(spec->untouched, word, json_integer(count + 1));
}

/***************************************************************************
 *  Replace the whole-word keys of `values` in `in`, in one pass
 ***************************************************************************/
PRIVATE int render_text(
    out_buf_t *out,
    const char *in,
    size_t len,
    json_t *values,
    clone_spec_t *spec
) {
    out->len = 0;
    if(out_append(out, "", 0) < 0) {
        return -1;  // Error already logged
    }

    size_t n_keys;
    replace_key_t *keys = build_keys(values, &n_keys);
    if(!keys) {
        return -1;  // Error already logged
    }

    size_t i = 0;
    while(i < len) {
        BOOL replaced = FALSE;
        BOOL before_ok = (i == 0) || !is_word_byte((unsigned char)in[i-1]);
        for(size_t k = 0; k < n_keys; k++) {
            replace_key_t *key = &keys[k];
            if(len - i < key->old_len || memcmp(in + i, key->old, key->old_len) != 0) {
                continue;
            }
            size_t after = i + key->old_len;
            BOOL after_ok = (after == len) || !is_word_byte((unsigned char)in[after]);
            if(before_ok && after_ok) {
                if(out_append(out, key->new, key->new_len) < 0) {
                    gbmem_free(keys);
                    return -1;  // Error already logged
                }
                i = after;
                replaced = TRUE;
                break;
            }
            note_untouched(spec, in, len, i, key->old_len);
        }
        if(!replaced) {
            if(out_append(out, in + i, 1) < 0) {
                gbmem_free(keys);
                return -1;  // Error already logged
            }
            i++;
        }
    }

    gbmem_free(keys);
    return 0;
}

/***************************************************************************
 *  Is it binary? A NUL byte anywhere says so. Read by chunks: a binary is
 *  never held in memory (gbmem's default max block is 16M)
 ***************************************************************************/
PRIVATE int is_binary_file(const char *path, BOOL *is_binary)
{
    *is_binary = FALSE;
    int fd = open(path, O_RDONLY|O_CLOEXEC);
    if(fd < 0) {
        fprintf(stderr, "ERROR: cannot open '%s': %s\n", path, strerror(errno));
        return -1;
    }
    char chunk[64*1024];
    while(1) {
        ssize_t n = read(fd, chunk, sizeof(chunk));
        if(n < 0) {
            if(errno == EINTR) {
                continue;
            }
            fprintf(stderr, "ERROR: cannot read '%s': %s\n", path, strerror(errno));
            close(fd);
            return -1;
        }
        if(n == 0) {
            break;
        }
        if(memchr(chunk, 0, (size_t)n)) {
            *is_binary = TRUE;
            break;
        }
    }
    close(fd);
    return 0;
}

/***************************************************************************
 *  Read a whole text file
 ***************************************************************************/
PRIVATE char *read_whole_file(const char *path, size_t *len)
{
    *len = 0;
    int fd = open(path, O_RDONLY|O_CLOEXEC);
    if(fd < 0) {
        fprintf(stderr, "ERROR: cannot open '%s': %s\n", path, strerror(errno));
        return NULL;
    }
    struct stat st;
    if(fstat(fd, &st) < 0) {
        fprintf(stderr, "ERROR: cannot stat '%s': %s\n", path, strerror(errno));
        close(fd);
        return NULL;
    }
    size_t size = (size_t)st.st_size;
    char *p = gbmem_malloc(size + 1);
    if(!p) {
        fprintf(stderr, "ERROR: no memory to read '%s' (%zu bytes)\n", path, size);
        close(fd);
        return NULL;
    }
    size_t got = 0;
    while(got < size) {
        ssize_t n = read(fd, p + got, size - got);
        if(n < 0) {
            if(errno == EINTR) {
                continue;
            }
            fprintf(stderr, "ERROR: cannot read '%s': %s\n", path, strerror(errno));
            gbmem_free(p);
            close(fd);
            return NULL;
        }
        if(n == 0) {
            break;  // the file shrank while read: take what there is
        }
        got += (size_t)n;
    }
    close(fd);
    p[got] = 0;
    *len = got;
    return p;
}

/***************************************************************************
 *  Write a new file, with the mode of its source
 ***************************************************************************/
PRIVATE int write_new_file(const char *path, const char *p, size_t len, mode_t mode)
{
    int fd = open(path, O_WRONLY|O_CREAT|O_EXCL|O_CLOEXEC, 0600);
    if(fd < 0) {
        fprintf(stderr, "ERROR: cannot create '%s': %s\n", path, strerror(errno));
        return -1;
    }
    size_t written = 0;
    while(written < len) {
        ssize_t n = write(fd, p + written, len - written);
        if(n < 0) {
            if(errno == EINTR) {
                continue;
            }
            fprintf(stderr, "ERROR: cannot write '%s': %s\n", path, strerror(errno));
            close(fd);
            return -1;
        }
        written += (size_t)n;
    }
    if(close(fd) < 0) {
        fprintf(stderr, "ERROR: cannot close '%s': %s\n", path, strerror(errno));
        return -1;
    }
    if(chmod(path, mode & 07777) < 0) {
        fprintf(stderr, "ERROR: cannot chmod '%s': %s\n", path, strerror(errno));
        return -1;
    }
    return 0;
}

/***************************************************************************
 *  Render a name (a file or a directory) with the name values
 ***************************************************************************/
PRIVATE int render_name(
    char *bf,
    size_t bfsize,
    const char *name,
    BOOL add_text_suffix,
    clone_spec_t *spec
) {
    out_buf_t out = {0};
    if(render_text(&out, name, strlen(name), spec->name_values, spec) < 0) {
        gbmem_free(out.p);
        return -1;  // Error already logged
    }
    const char *suffix = (add_text_suffix && spec->text_suffix)? spec->text_suffix : "";
    int n = snprintf(bf, bfsize, "%s%s", out.p, suffix);
    gbmem_free(out.p);
    if(n < 0 || (size_t)n >= bfsize) {
        fprintf(stderr, "ERROR: name too long: '%s'\n", name);
        return -1;
    }
    return 0;
}

/***************************************************************************
 *  Write the clone of a file: text is rendered, binary is copied as it is
 ***************************************************************************/
PRIVATE int write_clone(
    const char *dst,
    const char *src,
    BOOL is_binary,
    mode_t mode,
    clone_spec_t *spec
) {
    if(is_binary) {
        printf("Copying (binary, as it is): %s\n", dst);
        if(copyfile(src, dst, (int)(mode & 07777), FALSE) < 0) {
            fprintf(stderr, "ERROR: cannot copy '%s' to '%s'\n", src, dst);
            return -1;
        }
        if(chmod(dst, mode & 07777) < 0) {
            fprintf(stderr, "ERROR: cannot chmod '%s': %s\n", dst, strerror(errno));
            return -1;
        }
        return 0;
    }

    printf("Creating filename: %s\n", dst);
    size_t len;
    char *content = read_whole_file(src, &len);
    if(!content) {
        return -1;  // Error already logged
    }
    out_buf_t out = {0};
    int ret = render_text(&out, content, len, spec->content_values, spec);
    if(ret == 0) {
        ret = write_new_file(dst, out.p, out.len, mode);
    }
    gbmem_free(out.p);
    gbmem_free(content);
    return ret;
}

/***************************************************************************
 *  Clone one file (its name is the caller's)
 ***************************************************************************/
PUBLIC int clone_file(const char *dst, const char *src, clone_spec_t *spec)
{
    struct stat st;
    if(stat(src, &st) < 0) {
        fprintf(stderr, "ERROR: cannot stat '%s': %s\n", src, strerror(errno));
        return -1;
    }
    if(access(dst, F_OK) == 0) {
        fprintf(stderr, "ERROR: the destination already exists: '%s'\n", dst);
        return -1;
    }
    BOOL is_binary;
    if(is_binary_file(src, &is_binary) < 0) {
        return -1;  // Error already logged
    }
    return write_clone(dst, src, is_binary, st.st_mode, spec);
}

/***************************************************************************
 *  Directories of VCS and build output are not cloned
 ***************************************************************************/
PRIVATE BOOL is_skipped_dir(const char *name)
{
    for(int i = 0; skipped_dirs[i]; i++) {
        if(strcmp(name, skipped_dirs[i]) == 0) {
            return TRUE;
        }
    }
    return strncmp(name, SKIPPED_DIR_PREFIX, strlen(SKIPPED_DIR_PREFIX)) == 0;
}

/***************************************************************************
 *  Join a directory and a name, or fail when the path does not fit:
 *  build_path() truncates, and a truncated path names another file
 ***************************************************************************/
PRIVATE int join_path(char *bf, size_t bfsize, const char *dir, const char *name)
{
    if(strlen(dir) + 1 + strlen(name) >= bfsize) {
        fprintf(stderr, "ERROR: path too long: '%s/%s'\n", dir, name);
        return -1;
    }
    build_path(bf, bfsize, dir, name, NULL);
    return 0;
}

/***************************************************************************
 *  The target of a cloned symlink. One that resolves inside the cloned
 *  tree is rendered as the names are (and gets the suffix when it is a
 *  text file), so it points to the file under its new name; an absolute
 *  one, one outside the tree, or a dangling one is kept as it is
 ***************************************************************************/
PRIVATE int render_link_target(
    char *bf,
    size_t bfsize,
    const char *target,
    const char *src_link,
    const char *real_root,
    clone_spec_t *spec
) {
    char resolved[PATH_MAX];
    size_t l = strlen(real_root);
    if(target[0] == '/' || !realpath(src_link, resolved) ||
            strncmp(resolved, real_root, l) != 0 || resolved[l] != '/') {
        int n = snprintf(bf, bfsize, "%s", target);
        if(n < 0 || (size_t)n >= bfsize) {
            fprintf(stderr, "ERROR: link target too long: '%s'\n", target);
            return -1;
        }
        return 0;
    }

    struct stat st;
    if(stat(resolved, &st) < 0) {
        fprintf(stderr, "ERROR: cannot stat '%s': %s\n", resolved, strerror(errno));
        return -1;
    }
    BOOL is_binary = FALSE;
    if(S_ISREG(st.st_mode)) {
        if(is_binary_file(resolved, &is_binary) < 0) {
            return -1;  // Error already logged
        }
    }
    BOOL add_text_suffix = S_ISREG(st.st_mode) && !is_binary;

    out_buf_t out = {0};
    if(render_text(&out, target, strlen(target), spec->name_values, spec) < 0) {
        gbmem_free(out.p);
        return -1;  // Error already logged
    }
    const char *suffix = (add_text_suffix && spec->text_suffix)? spec->text_suffix : "";
    int n = snprintf(bf, bfsize, "%s%s", out.p, suffix);
    gbmem_free(out.p);
    if(n < 0 || (size_t)n >= bfsize) {
        fprintf(stderr, "ERROR: link target too long: '%s'\n", target);
        return -1;
    }
    return 0;
}

/***************************************************************************
 *  Clone a symlink, its target following the names of the clone
 ***************************************************************************/
PRIVATE int clone_link(
    const char *dst,
    const char *src,
    const char *real_root,
    clone_spec_t *spec
) {
    char target[PATH_MAX];
    ssize_t n = readlink(src, target, sizeof(target) - 1);
    if(n < 0) {
        fprintf(stderr, "ERROR: cannot read the link '%s': %s\n", src, strerror(errno));
        return -1;
    }
    target[n] = 0;
    char new_target[PATH_MAX];
    if(render_link_target(new_target, sizeof(new_target), target, src, real_root, spec) < 0) {
        return -1;  // Error already logged
    }
    if(symlink(new_target, dst) < 0) {
        fprintf(stderr, "ERROR: cannot create the link '%s' -> '%s': %s\n",
            dst, new_target, strerror(errno));
        return -1;
    }
    printf("Creating link: %s -> %s\n", dst, new_target);
    return 0;
}

/***************************************************************************
 *  Clone the directory src into dst (dst must not exist)
 ***************************************************************************/
PRIVATE int clone_dir(
    const char *dst,
    const char *src,
    const char *real_root,
    clone_spec_t *spec
) {
    struct stat st_dir;
    if(stat(src, &st_dir) < 0) {
        fprintf(stderr, "ERROR: cannot stat '%s': %s\n", src, strerror(errno));
        return -1;
    }

    printf("Creating directory: %s\n", dst);
    if(mkdir(dst, S_IRWXU) < 0) {
        fprintf(stderr, "ERROR: cannot create the directory '%s': %s\n", dst, strerror(errno));
        return -1;
    }

    DIR *dir = opendir(src);
    if(!dir) {
        fprintf(stderr, "ERROR: cannot open the directory '%s': %s\n", src, strerror(errno));
        return -1;
    }

    int ret = 0;
    struct dirent *entry;
    while(ret == 0 && (entry = readdir(dir))) {
        const char *name = entry->d_name;
        if(strcmp(name, ".") == 0 || strcmp(name, "..") == 0) {
            continue;
        }

        char src_path[PATH_MAX];
        if(join_path(src_path, sizeof(src_path), src, name) < 0) {
            ret = -1;   // Error already logged
            break;
        }

        struct stat st;
        if(lstat(src_path, &st) < 0) {
            fprintf(stderr, "ERROR: cannot stat '%s': %s\n", src_path, strerror(errno));
            ret = -1;
            break;
        }

        if(S_ISDIR(st.st_mode) && is_skipped_dir(name)) {
            printf("Skipping (VCS or build output): %s\n", src_path);
            continue;
        }

        BOOL is_binary = FALSE;
        if(S_ISREG(st.st_mode)) {
            if(is_binary_file(src_path, &is_binary) < 0) {
                ret = -1;   // Error already logged
                break;
            }
        }

        char dst_name[NAME_MAX + 1];
        BOOL add_text_suffix = S_ISREG(st.st_mode) && !is_binary;
        if(render_name(dst_name, sizeof(dst_name), name, add_text_suffix, spec) < 0) {
            ret = -1;   // Error already logged
            break;
        }
        char dst_path[PATH_MAX];
        if(join_path(dst_path, sizeof(dst_path), dst, dst_name) < 0) {
            ret = -1;   // Error already logged
            break;
        }

        if(S_ISLNK(st.st_mode)) {
            ret = clone_link(dst_path, src_path, real_root, spec);
        } else if(S_ISDIR(st.st_mode)) {
            ret = clone_dir(dst_path, src_path, real_root, spec);
        } else if(S_ISREG(st.st_mode)) {
            ret = write_clone(dst_path, src_path, is_binary, st.st_mode, spec);
        } else {
            printf("Skipping (not a file, a directory or a link): %s\n", src_path);
        }
    }
    closedir(dir);

    if(ret == 0 && chmod(dst, st_dir.st_mode & 07777) < 0) {
        fprintf(stderr, "ERROR: cannot chmod '%s': %s\n", dst, strerror(errno));
        ret = -1;
    }
    return ret;
}

/***************************************************************************
 *  Clone the directory src into dst
 ***************************************************************************/
PUBLIC int clone_tree_dir(const char *dst, const char *src, clone_spec_t *spec)
{
    if(!is_directory(src)) {
        fprintf(stderr, "ERROR: the source is not a directory: '%s'\n", src);
        return -1;
    }
    if(access(dst, F_OK) == 0) {
        fprintf(stderr, "ERROR: the destination already exists: '%s'\n", dst);
        return -1;
    }

    /*
     *  A destination inside the source would be cloned into itself
     *  while it is walked
     */
    char real_src[PATH_MAX];
    if(!realpath(src, real_src)) {
        fprintf(stderr, "ERROR: cannot resolve '%s': %s\n", src, strerror(errno));
        return -1;
    }
    char dst_copy[PATH_MAX];
    snprintf(dst_copy, sizeof(dst_copy), "%s", dst);
    char real_dst_parent[PATH_MAX];
    if(!realpath(dirname(dst_copy), real_dst_parent)) {
        fprintf(stderr, "ERROR: the parent directory of '%s' does not exist: %s\n",
            dst, strerror(errno));
        return -1;
    }
    size_t l = strlen(real_src);
    if(strncmp(real_dst_parent, real_src, l) == 0 &&
            (real_dst_parent[l] == 0 || real_dst_parent[l] == '/')) {
        fprintf(stderr, "ERROR: the destination '%s' is inside the source '%s'\n", dst, src);
        return -1;
    }

    return clone_dir(dst, src, real_src, spec);
}

/***************************************************************************
 *  Print the words that hold a key and were left as they were
 ***************************************************************************/
PUBLIC void print_untouched_words(clone_spec_t *spec)
{
    if(!spec->untouched || json_object_size(spec->untouched) == 0) {
        return;
    }
    printf("\nLeft as they were (a key inside a longer word), review them by hand:\n");
    const char *word;
    json_t *jn_count;
    json_object_foreach(spec->untouched, word, jn_count) {
        printf("    %-40s %d\n", word, (int)json_integer_value(jn_count));
    }
}
