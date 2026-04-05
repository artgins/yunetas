# Directory Walk Functions

Source code in:

- [helpers.h](https://github.com/artgins/yunetas/blob/main/kernel/c/gobj-c/src/helpers.h)
- [helpers.c](https://github.com/artgins/yunetas/blob/main/kernel/c/gobj-c/src/helpers.c)

(get_ordered_filename_array)=
## `get_ordered_filename_array()`

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
| `pattern` | `const char *` | A regex pattern to filter filenames. If NULL, all files are included. |
| `opt` | `wd_option` | Options for directory traversal, such as recursion and file type filtering. |
| `size` | `int *` | Pointer to an integer that will be set to the number of files found. |

**Returns**

Returns a dynamically allocated array of strings containing the ordered filenames. The caller must free the array using `free_ordered_filename_array()`.

**Notes**

This function uses `qsort()` to sort the filenames. The returned array must be freed properly to avoid memory leaks.

---

(walk_dir_tree)=
## `walk_dir_tree()`

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
| `pattern` | `const char *` | A regular expression pattern to match file or directory names. |
| `opt` | `wd_option` | Options controlling the traversal behavior, such as recursion, hidden file inclusion, and file type matching. |
| `cb` | `walkdir_cb` | A callback function that is invoked for each matching file or directory. |
| `user_data` | `void *` | A user-defined pointer passed to the callback function. |

**Returns**

Returns 0 on success, or -1 if an error occurs (e.g., if `root_dir` does not exist or `pattern` is invalid).

**Notes**

The callback function `cb` should return `TRUE` to continue traversal or `FALSE` to stop. The function uses `regcomp()` to compile the `pattern` and `regexec()` to match file names.

---

(dir_array_free)=
## `dir_array_free()`

*Description pending — signature extracted from header.*

```C
void dir_array_free(
    dir_array_t *da
);
```

---

(dir_array_sort)=
## `dir_array_sort()`

*Description pending — signature extracted from header.*

```C
void dir_array_sort(
    dir_array_t *da
);
```

---

(find_files_with_suffix_array)=
## `find_files_with_suffix_array()`

*Description pending — signature extracted from header.*

```C
int find_files_with_suffix_array(
    hgobj gobj,
    const char *directory,
    const char *suffix,
    dir_array_t *da
);
```

---

(walk_dir_array)=
## `walk_dir_array()`

*Description pending — signature extracted from header.*

```C
int walk_dir_array(
    hgobj gobj,
    const char *root_dir,
    const char *re,
    wd_option opt,
    dir_array_t *da
);
```

---

