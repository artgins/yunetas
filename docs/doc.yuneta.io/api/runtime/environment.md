# Environment

Functions for managing yuneta directory paths and file permissions.

**Source:** `kernel/c/root-linux/src/yunetas_environment.h`

---

(register_yuneta_environment)=
## `register_yuneta_environment()`

Registers the yuneta environment with root directory, domain directory,
and file permission masks.

```C
int register_yuneta_environment(
    const char *root_dir,
    const char *domain_dir,
    int xpermission,
    int rpermission
);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `root_dir` | `const char *` | Root directory path. |
| `domain_dir` | `const char *` | Domain (realms) directory path. |
| `xpermission` | `int` | Permission mask for directories and executable files (e.g. `02775`). |
| `rpermission` | `int` | Permission mask for regular files (e.g. `0664`). |

**Returns**

`0`.

---

(yuneta_xpermission)=
## `yuneta_xpermission()`

Returns the permission mask for directories and executable files.

```C
int yuneta_xpermission(void);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `-` | `-` | This function does not take any parameters. |

**Returns**

The `xpermission` value registered with
[`register_yuneta_environment()`](#register_yuneta_environment).

---

(yuneta_rpermission)=
## `yuneta_rpermission()`

Returns the permission mask for regular files.

```C
int yuneta_rpermission(void);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `-` | `-` | This function does not take any parameters. |

**Returns**

The `rpermission` value registered with
[`register_yuneta_environment()`](#register_yuneta_environment).

---

(yuneta_root_dir)=
## `yuneta_root_dir()`

Returns the main root directory path.

```C
const char *yuneta_root_dir(void);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `-` | `-` | This function does not take any parameters. |

**Returns**

Root directory path string.

---

(yuneta_domain_dir)=
## `yuneta_domain_dir()`

Returns the domain directory path (base path for realms).

```C
const char *yuneta_domain_dir(void);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `-` | `-` | This function does not take any parameters. |

**Returns**

Domain directory path string.

---

(yuneta_realm_dir)=
## `yuneta_realm_dir()`

Builds the path for a realm directory: `root_dir/domain_dir/subdomain`.

```C
char *yuneta_realm_dir(
    char *bf,
    int bfsize,
    const char *subdomain,
    BOOL create
);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `bf` | `char *` | Output buffer. |
| `bfsize` | `int` | Buffer size. |
| `subdomain` | `const char *` | Subdomain name. |
| `create` | `BOOL` | `TRUE` to create the directory if it does not exist. |

**Returns**

Pointer to `bf`, or `NULL` if `create` is `TRUE` and directory creation failed.

---

(yuneta_realm_file)=
## `yuneta_realm_file()`

Builds the path for a file inside a realm directory.

```C
char *yuneta_realm_file(
    char *bf,
    int bfsize,
    const char *subdomain,
    const char *filename,
    BOOL create
);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `bf` | `char *` | Output buffer. |
| `bfsize` | `int` | Buffer size. |
| `subdomain` | `const char *` | Subdomain name. |
| `filename` | `const char *` | File name. |
| `create` | `BOOL` | `TRUE` to create the parent directory if it does not exist. |

**Returns**

Pointer to `bf`.

---

(yuneta_log_dir)=
## `yuneta_log_dir()`

Builds the path for the logs directory: `root_dir/domain_dir/logs`.

```C
char *yuneta_log_dir(
    char *bf,
    int bfsize,
    BOOL create
);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `bf` | `char *` | Output buffer. |
| `bfsize` | `int` | Buffer size. |
| `create` | `BOOL` | `TRUE` to create the directory if it does not exist. |

**Returns**

Pointer to `bf`.

---

(yuneta_log_file)=
## `yuneta_log_file()`

Builds the path for a log file.

```C
char *yuneta_log_file(
    char *bf,
    int bfsize,
    const char *filename,
    BOOL create
);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `bf` | `char *` | Output buffer. |
| `bfsize` | `int` | Buffer size. |
| `filename` | `const char *` | Log file name. |
| `create` | `BOOL` | `TRUE` to create the parent directory if it does not exist. |

**Returns**

Pointer to `bf`.

---

(yuneta_bin_dir)=
## `yuneta_bin_dir()`

Builds the path for the bin directory: `root_dir/domain_dir/bin`.

```C
char *yuneta_bin_dir(
    char *bf,
    int bfsize,
    BOOL create
);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `bf` | `char *` | Output buffer. |
| `bfsize` | `int` | Buffer size. |
| `create` | `BOOL` | `TRUE` to create the directory if it does not exist. |

**Returns**

Pointer to `bf`.

---

(yuneta_bin_file)=
## `yuneta_bin_file()`

Builds the path for a binary file.

```C
char *yuneta_bin_file(
    char *bf,
    int bfsize,
    const char *filename,
    BOOL create
);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `bf` | `char *` | Output buffer. |
| `bfsize` | `int` | Buffer size. |
| `filename` | `const char *` | Binary file name. |
| `create` | `BOOL` | `TRUE` to create the parent directory if it does not exist. |

**Returns**

Pointer to `bf`.

---

(yuneta_store_dir)=
## `yuneta_store_dir()`

Builds the path for a store directory: `root_dir/store/dir/subdir`.

```C
char *yuneta_store_dir(
    char *bf,
    int bfsize,
    const char *dir,
    const char *subdir,
    BOOL create
);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `bf` | `char *` | Output buffer. |
| `bfsize` | `int` | Buffer size. |
| `dir` | `const char *` | First-level directory name. |
| `subdir` | `const char *` | Second-level directory name. |
| `create` | `BOOL` | `TRUE` to create the directory if it does not exist. |

**Returns**

Pointer to `bf`.

---

(yuneta_store_file)=
## `yuneta_store_file()`

Builds the path for a file in a store directory.

```C
char *yuneta_store_file(
    char *bf,
    int bfsize,
    const char *dir,
    const char *subdir,
    const char *filename,
    BOOL create
);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `bf` | `char *` | Output buffer. |
| `bfsize` | `int` | Buffer size. |
| `dir` | `const char *` | First-level directory name. |
| `subdir` | `const char *` | Second-level directory name. |
| `filename` | `const char *` | File name. |
| `create` | `BOOL` | `TRUE` to create the parent directory if it does not exist. |

**Returns**

Pointer to `bf`.

---

(yuneta_realm_store_dir)=
## `yuneta_realm_store_dir()`

Builds the path for a realm store directory:
`root_dir/store/service/owner/realm_id/tenant/dir`.

```C
char *yuneta_realm_store_dir(
    char *bf,
    int bfsize,
    const char *service,
    const char *owner,
    const char *realm_id,
    const char *tenant,
    const char *dir,
    BOOL create
);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `bf` | `char *` | Output buffer. |
| `bfsize` | `int` | Buffer size. |
| `service` | `const char *` | Service name. |
| `owner` | `const char *` | Owner identifier. |
| `realm_id` | `const char *` | Realm identifier. |
| `tenant` | `const char *` | Tenant identifier. |
| `dir` | `const char *` | Directory name. |
| `create` | `BOOL` | `TRUE` to create the directory if it does not exist. |

**Returns**

Pointer to `bf`.
