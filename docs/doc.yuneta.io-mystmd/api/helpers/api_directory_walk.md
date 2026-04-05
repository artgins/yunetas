# Directory Walk Functions

Source code in:

- [helpers.h](https://github.com/artgins/yunetas/blob/main/kernel/c/gobj-c/src/helpers.h)
- [helpers.c](https://github.com/artgins/yunetas/blob/main/kernel/c/gobj-c/src/helpers.c)

(free_ordered_filename_array)=
## `free_ordered_filename_array()`

Deallocates memory used by an array of ordered filenames, freeing each filename string and the array itself.

```C
void free_ordered_filename_array(
    char **array,
    int    size
);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `array` | `char **` | Pointer to an array of dynamically allocated filename strings. |
| `size` | `int` | Number of elements in the array. |

**Returns**

This function does not return a value.

**Notes**

Each filename string in the array is individually freed before freeing the array itself. Ensure that `array` was allocated using [`get_ordered_filename_array()`](#get_ordered_filename_array) to avoid undefined behavior.

---

(get_number_of_files)=
## `get_number_of_files()`

The function `get_number_of_files()` counts the number of files in a directory tree that match a given pattern and options.

```C
int get_number_of_files(
    hgobj gobj,
    const char *root_dir,
    const char *pattern,
    wd_option opt
);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `gobj` | `hgobj` | A handle to the gobj (generic object) system, used for logging and error handling. |
| `root_dir` | `const char *` | The root directory where the search begins. |
| `pattern` | `const char *` | A regular expression pattern to match filenames. |
| `opt` | `wd_option` | Options that control the search behavior, such as recursion and file type filtering. |

**Returns**

Returns the number of matching files found in the directory tree. Returns -1 on error.

**Notes**

This function internally uses [`walk_dir_tree()`](#walk_dir_tree) to traverse the directory tree and count matching files.

---

(get_ordered_filename_array)=
## `get_ordered_filename_array()`

Retrieves an ordered list of filenames from a specified directory, optionally filtered by a pattern and search options.

```C
char **get_ordered_filename_array(
    hgobj gobj,
    const char *root_dir,
    const char *pattern,
    wd_option opt,
    int *size
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
