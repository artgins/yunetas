# File System

File and path utilities: existence checks, permissions, mkdir/rmdir, copy/move, size, mtime, and safe path building.

Source code:

- [`helpers.h`](https://github.com/artgins/yunetas/blob/main/kernel/c/gobj-c/src/helpers.h)
- [`helpers.c`](https://github.com/artgins/yunetas/blob/main/kernel/c/gobj-c/src/helpers.c)

(file_exists)=
## `file_exists()`

The `file_exists()` function checks if a given file exists within a specified directory and is a regular file.

```C
BOOL file_exists(
    const char *directory,
    const char *filename
);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `directory` | `const char *` | The path to the directory where the file is expected to be located. |
| `filename` | `const char *` | The name of the file to check for existence. |

**Returns**

Returns `TRUE` if the file exists and is a regular file, otherwise returns `FALSE`.

**Notes**

This function constructs the full file path by combining `directory` and `filename`, then checks if the file exists and is a regular file using `is_regular_file()`.

---

(file_permission)=
## `file_permission()`

The `file_permission()` function retrieves the permission mode of a specified file path.

```C
mode_t file_permission(const char *path);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `path` | `const char *` | The file path whose permission mode is to be retrieved. |

**Returns**

Returns the permission mode of the file as a `mode_t` value. If the file does not exist or an error occurs, returns 0.

**Notes**

This function internally uses `stat()` to obtain the file's mode and extracts the permission bits using bitwise operations.

---

(file_remove)=
## `file_remove()`

`file_remove()` deletes a specified file from a given directory if it exists and is a regular file.

```C
int file_remove(
    const char *directory,
    const char *filename
);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `directory` | `const char *` | The path to the directory containing the file. |
| `filename` | `const char *` | The name of the file to be removed. |

**Returns**

Returns 0 on success, or -1 if the file does not exist or is not a regular file.

**Notes**

This function checks if the file exists and is a regular file before attempting to delete it.

---

(file_size)=
## `file_size()`

`file_size()` returns the size of a file in bytes, given its path.

```C
off_t file_size(const char *path);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `path` | `const char *` | Path to the file whose size is to be determined. |

**Returns**

Returns the file size in bytes on success. Returns 0 if the file does not exist or an error occurs.

**Notes**

This function uses `stat()` to retrieve file information. If `stat()` fails, the function returns 0.

---

(filesize)=
## `filesize()`

`filesize()` returns the size of a file in bytes by using the `stat()` system call.

```C
off_t filesize(const char *path);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `path` | `const char *` | Path to the file whose size is to be determined. |

**Returns**

Returns the size of the file in bytes. If the file does not exist or an error occurs, returns 0.

**Notes**

This function relies on `stat()`, which may fail if the file does not exist or if there are insufficient permissions.

---

(filesize2)=
## `filesize2()`

`filesize2()` returns the size of an open file descriptor in bytes.

```C
off_t filesize2(int fd);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `fd` | `int` | The file descriptor of the open file. |

**Returns**

Returns the size of the file in bytes. If an error occurs, returns 0.

**Notes**

This function uses `fstat()` to retrieve the file size.

---

(is_directory)=
## `is_directory()`

The `is_directory()` function checks whether the given path corresponds to a directory by using the `stat()` system call.

```C
BOOL is_directory(const char *path);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `path` | `const char *` | The file system path to check. |

**Returns**

Returns `TRUE` if the path is a directory, otherwise returns `FALSE`.

**Notes**

This function relies on `stat()` to determine the file type. If `stat()` fails, the function returns `FALSE`.

---

(is_regular_file)=
## `is_regular_file()`

The `is_regular_file()` function checks if the given path corresponds to a regular file.

```C
BOOL is_regular_file(const char *path);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `path` | `const char *` | The file system path to check. |

**Returns**

Returns `TRUE` if the path corresponds to a regular file, otherwise returns `FALSE`.

**Notes**

['This function uses `stat()` to determine the file type.', 'If `stat()` fails, the function returns `FALSE`.']

---

(lock_file)=
## `lock_file()`

The `lock_file()` function applies an advisory write lock to the specified file descriptor using the `fcntl` system call.

```C
int lock_file(int fd);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `fd` | `int` | The file descriptor of the file to be locked. |

**Returns**

Returns 0 on success. On failure, returns -1 and sets `errno` to indicate the error.

**Notes**

This function uses `fcntl` with `F_SETLKW` to apply a blocking write lock. Ensure that the file descriptor is valid before calling this function.

---

(mkrdir)=
## `mkrdir()`

`mkrdir()` creates a directory and all its parent directories if they do not exist, similar to the `mkdir -p` command.

```C
int mkrdir(
    const char *path,
    int xpermission
);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `path` | `const char *` | The directory path to be created. |
| `xpermission` | `int` | The permission mode for the created directories. |

**Returns**

Returns `0` on success, or `-1` if an error occurs.

**Notes**

If a directory in the path already exists, it is not modified. The function ensures that all parent directories are created as needed.

---

(newdir)=
## `newdir()`

`newdir()` creates a new directory with the specified permissions, ensuring that the umask is set to zero for controlled permission handling.

```C
int newdir(
    const char *path,
    int xpermission
);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `path` | `const char *` | The path of the directory to be created. |
| `xpermission` | `int` | The permission mode for the new directory. |

**Returns**

Returns `0` on success, or `-1` if an error occurs.

**Notes**

This function ensures that the umask is cleared before creating the directory to allow precise permission control.

---

(newfile)=
## `newfile()`

`newfile()` creates a new file with the specified permissions, optionally overwriting an existing file.

```C
int newfile(
    const char *path,
    int rpermission,
    BOOL overwrite
);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `path` | `const char *` | The file path to create. |
| `permission` | `int` | The file permission mode (e.g., 0660). |
| `overwrite` | `BOOL` | If `TRUE`, an existing file will be truncated; otherwise, creation fails if the file exists. |

**Returns**

Returns a file descriptor on success, or `-1` on failure.

**Notes**

This function sets `umask(0)` to ensure the specified permissions are applied.

---

(open_exclusive)=
## `open_exclusive()`

`open_exclusive()` opens a file with exclusive access, ensuring that no other process can lock it simultaneously.

```C
int open_exclusive(
    const char *path,
    int flags,
    int rpermission
);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `path` | `const char *` | The file path to be opened. |
| `flags` | `int` | Flags for opening the file. If set to 0, default flags (`O_RDWR \| O_NOFOLLOW`) are used. |
| `permission` | `int` | The file permission mode, used when creating a new file. |

**Returns**

Returns a file descriptor on success, or -1 on failure.

**Notes**

This function applies an exclusive lock (`LOCK_EX | LOCK_NB`) to the file, ensuring that no other process can acquire a lock on it.

---

(rmrcontentdir)=
## `rmrcontentdir()`

The function `rmrcontentdir` recursively removes the contents of a directory without deleting the directory itself.

```C
int rmrcontentdir(const char *root_dir);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `root_dir` | `const char *` | Path to the directory whose contents should be removed. |

**Returns**

Returns 0 on success, or -1 if an error occurs.

**Notes**

This function does not remove the root directory itself, only its contents. It skips special entries like `.` and `..` and handles both files and subdirectories recursively.

---

(rmrdir)=
## `rmrdir()`

`rmrdir()` recursively removes a directory and all its contents, including subdirectories and files.

```C
int rmrdir(
    const char *root_dir
);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `path` | `const char *` | The path of the directory to be removed. |

**Returns**

Returns 0 on success, or -1 if an error occurs.

**Notes**

This function removes all files and subdirectories within the specified directory before deleting the directory itself.

---

(subdir_exists)=
## `subdir_exists()`

Checks if a given subdirectory exists within a specified directory.

```C
BOOL subdir_exists(
    const char *directory,
    const char *subdir
);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `directory` | `const char *` | The path of the parent directory. |
| `subdir` | `const char *` | The name of the subdirectory to check. |

**Returns**

Returns `TRUE` if the subdirectory exists, otherwise returns `FALSE`.

**Notes**

This function constructs the full path of the subdirectory and verifies its existence using `is_directory()`.

---

(unlock_file)=
## `unlock_file()`

The `unlock_file()` function releases an advisory lock on a file descriptor using the `fcntl` system call.

```C
int unlock_file(int fd);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `fd` | `int` | The file descriptor of the file to be unlocked. |

**Returns**

Returns 0 on success, or -1 if an error occurs.

**Notes**

This function is typically used in conjunction with [`lock_file()`](#lock_file) to manage file locking in multi-process environments.

---

(copyfile)=
## `copyfile()`

*Description pending — signature extracted from header.*

```C
int copyfile(
    const char* source,
    const char* destination,
    int permission,
    BOOL overwrite
);
```

---

(read_process_cmdline)=
## `read_process_cmdline()`

*Description pending — signature extracted from header.*

```C
int read_process_cmdline(
    char *bf,
    size_t bfsize,
    pid_t pid
);
```

---

(set_cloexec)=
## `set_cloexec()`

*Description pending — signature extracted from header.*

```C
int set_cloexec(
    int fd
);
```

---

(set_nonblocking)=
## `set_nonblocking()`

*Description pending — signature extracted from header.*

```C
int set_nonblocking(
    int fd
);
```

---

