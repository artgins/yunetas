# Directory Walk

Recursive directory traversal with filters for hidden files, regular files vs. directories, and user-supplied callbacks.

Source code:

- [`helpers.h`](https://github.com/artgins/yunetas/blob/7.25.4/kernel/c/gobj-c/src/helpers.h)
- [`helpers.c`](https://github.com/artgins/yunetas/blob/7.25.4/kernel/c/gobj-c/src/helpers.c)

(get_ordered_filename_array)=
## [`get_ordered_filename_array()`](https://github.com/artgins/yunetas/blob/7.25.4/kernel/c/gobj-c/src/helpers.c#L3418)

Retrieves an ordered list of filenames from a specified directory, optionally filtered by a pattern and search options.

```C
int get_ordered_filename_array(
    hgobj gobj,
    const char *root_dir,
    const char *re,
    wd_option opt,
    dir_array_t *da
);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `gobj` | `hgobj` | A handle to the GObj instance, used for logging and error reporting. |
| `root_dir` | `const char *` | The root directory from which to retrieve filenames. |
| `re` | `const char *` | A regex pattern to filter filenames. If `NULL`, all entries are included (up to 7.25.4 a `NULL` crashed in `regcomp()`). |
| `opt` | `wd_option` | Options for directory traversal, such as recursion and file type filtering. |
| `da` | `dir_array_t *` | Pointer to a `dir_array_t` structure that will receive the results. |

**Returns**

Returns `0` on success, or `-1` on error (logged): `root_dir` is not a directory or cannot be opened, a directory of the tree cannot be READ (`readdir()` fails: `EIO`, `ESTALE`), the pattern does not compile, or an entry cannot be kept (no memory). On error `da` is empty -- a listing that lost an entry is not the listing of the directory (up to 7.25.4 the entry was dropped, a root that could not be opened listed as empty, a `readdir()` that failed was taken as the end of the directory, and the call answered `0`). The log says which: *"Cannot open directory"* (not a directory, or the root cannot be opened, with `errno`), *"Cannot read directory, readdir() FAILED"* (with `errno`), *"regcomp() FAILED"*, then *"Cannot list directory tree, the directory cannot be opened or read"*; or *"Cannot list directory tree, no memory for an entry"*. Results are stored in the `da` structure. Free with `dir_array_free()`.

**Notes**

This function uses `qsort()` to sort the filenames. The returned array must be freed properly to avoid memory leaks.

---

(walk_dir_tree)=
## [`walk_dir_tree()`](https://github.com/artgins/yunetas/blob/7.25.4/kernel/c/gobj-c/src/helpers.c#L3149)

The `walk_dir_tree()` function traverses a directory tree starting from `root_dir`, applying a user-defined callback function `cb` to each file or directory that matches the specified `pattern` and `opt` options.

```C
int walk_dir_tree(
    hgobj gobj,
    const char *root_dir,
    const char *pattern,
    wd_option opt,
    walkdir_cb cb,
    void *user_data
);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `gobj` | `hgobj` | A handle to the Yuneta framework object, used for logging and error handling. |
| `root_dir` | `const char *` | The root directory from which the traversal begins. |
| `pattern` | `const char *` | A regular expression pattern to match file or directory names. `NULL` matches every name. |
| `opt` | `wd_option` | Options controlling the traversal behavior, such as recursion, hidden file inclusion, and file type matching. |
| `cb` | `walkdir_cb` | A callback function that is invoked for each matching file or directory. |
| `user_data` | `void *` | A user-defined pointer passed to the callback function. |

**Returns**

Returns 0 on success, or -1 (logged) if an error occurs: `root_dir` is not a directory, it exists and cannot be opened (*"Cannot open directory"*, with `errno`), a directory of the tree cannot be read (*"Cannot read directory, readdir() FAILED"*), or `pattern` is invalid. Every -1 is logged, so a caller may say *"Error already logged"*: up to 7.25.4 a root that existed and could not be opened (`EACCES`, mode `0`) answered -1 with nothing logged, and a `readdir()` that failed was taken as the end of the directory (0, with the entries not read yet never given to `cb`).

**Notes**

The callback function `cb` must return `TRUE` to continue traversal or `FALSE` to stop. The function uses `regcomp()` to compile the `pattern` and `regexec()` to match file names. A SUBdirectory that cannot be opened is skipped (silently for `EACCES` and `ENOENT`); one that opens and cannot be read fails the walk.

**Example**

```C
PRIVATE BOOL count_cb(hgobj gobj, void *user_data, wd_found_type type,
    char *fullpath, const char *directory, char *name, int level, wd_option opt)
{
    (*(int *)user_data)++;
    return TRUE;    // go on
}

int files = 0;
if(walk_dir_tree(gobj, "/yuneta/store", NULL, WD_RECURSIVE|WD_MATCH_REGULAR_FILE,
        count_cb, &files) < 0) {
    return -1;  // Error already logged
}
```

---

(dir_array_free)=
## [`dir_array_free()`](https://github.com/artgins/yunetas/blob/7.25.4/kernel/c/gobj-c/src/helpers.c#L3201)

Frees all memory associated with a directory array structure.

```C
void dir_array_free(
    dir_array_t *da
);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `da` | `dir_array_t *` | Pointer to the directory array structure to free. |

**Returns**

This function does not return a value.

---

(dir_array_sort)=
## [`dir_array_sort()`](https://github.com/artgins/yunetas/blob/7.25.4/kernel/c/gobj-c/src/helpers.c#L3333)

Sorts the filenames in a directory array in lexicographic order using `qsort()`.

```C
void dir_array_sort(
    dir_array_t *da
);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `da` | `dir_array_t *` | Pointer to the directory array structure to sort. |

**Returns**

This function does not return a value.

---

(find_files_with_suffix_array)=
## [`find_files_with_suffix_array()`](https://github.com/artgins/yunetas/blob/7.25.4/kernel/c/gobj-c/src/helpers.c#L3243)

Finds all regular files in a directory with a given suffix.

```C
int find_files_with_suffix_array(
    hgobj gobj,
    const char *directory,
    const char *suffix,
    dir_array_t *da
);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `gobj` | `hgobj` | A handle to the GObj instance, used for logging and error reporting. |
| `directory` | `const char *` | The directory to search in. |
| `suffix` | `const char *` | The file suffix to match (for example `".json"`). |
| `da` | `dir_array_t *` | Pointer to a `dir_array_t` structure that will receive the results. |

**Returns**

Returns `0` on success, or `-1` on error (logged): the directory cannot be opened, cannot be read (`readdir()` fails: `EIO`, `ESTALE`; *"Cannot list directory, readdir() FAILED"*), or an entry cannot be kept (no memory, *"Cannot list directory, no memory for an entry"*). On error `da` is empty -- a listing that lost an entry is not the listing of the directory (up to 7.25.4 the entry was dropped, a `readdir()` that failed was taken as the end of the directory, and the call answered `0`: timeranger2 read a key without the `.md2` files not listed yet).

**Notes**

The array holds the file NAMES, not full paths, in the order of the directory (not sorted: call `dir_array_sort()`). Only regular files are listed. A symbolic link is never listed, even a link to a regular file, and a directory is never listed. On a filesystem that gives no entry type (`DT_UNKNOWN`), the function uses `lstat()` so that the result is the same as with the entry type. Up to 7.25.4 that path used `stat()` and listed a link to a file.

**Example**

```C
dir_array_t da;
if(find_files_with_suffix_array(gobj, key_directory, ".md2", &da) == 0) {
    dir_array_sort(&da);
    for(size_t i = 0; i < da.count; i++) {
        printf("%s\n", da.items[i]);    // "2026-09-23.md2", ...
    }
}
dir_array_free(&da);
```

---

(walk_dir_array)=
## [`walk_dir_array()`](https://github.com/artgins/yunetas/blob/7.25.4/kernel/c/gobj-c/src/helpers.c#L3371)

Recursively traverses a directory tree and populates an array with paths matching a regex pattern.

```C
int walk_dir_array(
    hgobj gobj,
    const char *root_dir,
    const char *re,
    wd_option opt,
    dir_array_t *da
);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `gobj` | `hgobj` | A handle to the GObj instance, used for logging and error reporting. |
| `root_dir` | `const char *` | The root directory from which the traversal begins. |
| `re` | `const char *` | A regex pattern to filter filenames. If `NULL`, all entries are included. |
| `opt` | `wd_option` | Options controlling the traversal behavior, such as recursion and file type filtering. |
| `da` | `dir_array_t *` | Pointer to a `dir_array_t` structure that will receive the results. |

**Returns**

Returns `0` on success, or `-1` on error (logged): `root_dir` is not a directory or cannot be opened (mode `0`, `EMFILE`), a directory of the tree cannot be read (`readdir()` fails), the pattern does not compile, or an entry cannot be kept (no memory). On error `da` is empty (up to 7.25.4 a lost entry was dropped, a root that could not be opened listed as empty, a failed `readdir()` ended the directory, and the call answered `0`). A SUBdirectory that cannot be opened is skipped, silently for `EACCES` and `ENOENT`; one that opens and cannot be read fails the listing. `tests/c/helpers/test_dir_read_error`.

```C
dir_array_t da;
if(walk_dir_array(gobj, "/yuneta/realms", NULL, WD_RECURSIVE|WD_MATCH_REGULAR_FILE, &da) < 0) {
    return -1;  // Error already logged, da is empty
}
for(json_int_t i = 0; i < da.count; i++) {
    printf("%s\n", da.items[i]);    // full paths, in the order of the walk
}
dir_array_free(&da);
```

---

