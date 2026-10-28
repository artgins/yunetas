# Misc

Miscellaneous helpers that do not fit anywhere else: byte-order conversion, base64, hex dumps, small math utilities.

Source code:

- [`helpers.h`](https://github.com/artgins/yunetas/blob/7.28.11/kernel/c/gobj-c/src/helpers.h)
- [`helpers.c`](https://github.com/artgins/yunetas/blob/7.28.11/kernel/c/gobj-c/src/helpers.c)

(get_hostname)=
## [`get_hostname()`](https://github.com/artgins/yunetas/blob/7.28.11/kernel/c/gobj-c/src/helpers.c#L9147)

Retrieves the hostname of the current system. On Linux, it reads from `/etc/hostname`, while on ESP32, it returns a default value.

```C
const char *get_hostname(void);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `-` | `-` | This function does not take any parameters. |

**Returns**

A pointer to a static string containing the hostname of the system.

**Notes**

The returned string is statically allocated and must not be modified or freed by the caller.

---

(node_uuid)=
## [`node_uuid()`](https://github.com/artgins/yunetas/blob/7.28.11/kernel/c/gobj-c/src/helpers.c#L9297)

The `node_uuid()` function retrieves a unique identifier for the current machine. On Linux, it reads or generates a UUID stored in `/yuneta/store/agent/uuid/uuid.json`. On ESP32, it derives the UUID from the device's MAC address.

```C
const char *node_uuid(void);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `-` | `-` | This function does not take any parameters. |

**Returns**

Returns a pointer to a string containing the unique node UUID.

**Notes**

The returned UUID is generated once and stored persistently. On Linux, if the UUID file does not exist, a new one is created. On ESP32, the UUID is derived from the MAC address.

---

(get_inotify_self_usage)=
## [`get_inotify_self_usage()`](%s/helpers.c#L7109)

How many inotify instances and watches THIS process is holding.

```C
int get_inotify_self_usage(
    int *instances,
    int *watches
);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `instances` | `int *` | Filled with the number of open inotify instances of this process. |
| `watches` | `int *` | Filled with the number of watches those instances hold. |

**Returns**

0, or -1 on error.

**Notes**

It counts what this process uses, not the system-wide totals: the kernel limits
(`max_user_instances`, `max_user_watches`) are per user, so a leak here is what
eventually makes an unrelated daemon fail to watch anything.

---

(check_open_fds)=
## [`check_open_fds()`](https://github.com/artgins/yunetas/blob/7.28.11/kernel/c/gobj-c/src/helpers.c#L10489)

Returns the number of currently open file descriptors in the process. On Linux, reads from `/proc/self/fd`. On other systems, returns 0.

```C
int check_open_fds(void);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `-` | `-` | This function does not take any parameters. |

**Returns**

The number of open file descriptors in the calling process. Returns 0 on non-Linux platforms.

---

(cpu_usage)=
## [`cpu_usage()`](https://github.com/artgins/yunetas/blob/7.28.11/kernel/c/gobj-c/src/helpers.c#L8880)

Returns the accumulated CPU time of the current process in jiffies (scheduler clock ticks). On Linux, reads from `/proc/self/stat`. On other systems, returns 0.

```C
uint64_t cpu_usage(void);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `-` | `-` | This function does not take any parameters. |

**Returns**

The sum of user time (utime) and system time (stime) in jiffies. Returns 0 on non-Linux platforms.

---

(cpu_usage_percent)=
## [`cpu_usage_percent()`](https://github.com/artgins/yunetas/blob/7.28.11/kernel/c/gobj-c/src/helpers.c#L8916)

Calculates the CPU usage percentage of the current process over an elapsed time interval. Must be called repeatedly to track CPU usage over time.

```C
double cpu_usage_percent(
    uint64_t *last_cpu_ticks,
    uint64_t *last_ms
);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `last_cpu_ticks` | `uint64_t *` | IN/OUT — pointer to previous CPU ticks value. |
| `last_ms` | `uint64_t *` | IN/OUT — pointer to previous timestamp in milliseconds. |

**Returns**

CPU usage percentage as a double ranging from 0.0 to 100.0+. Returns 0.0 on the first call or if the time interval is zero.

---

(create_random_uuid)=
## [`create_random_uuid()`](https://github.com/artgins/yunetas/blob/7.28.11/kernel/c/gobj-c/src/helpers.c#L9173)

Generates a cryptographically secure random UUID (version 4) and formats it as a string.

```C
int create_random_uuid(
    char *bf,
    int bfsize
);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `bf` | `char *` | Output buffer for the UUID string. |
| `bfsize` | `int` | Size of the output buffer, must be at least 37 bytes. |

**Returns**

Returns 0 on success. Returns -1 if the buffer is too small or if random number generation fails.

---

(free_ram_in_kb)=
## [`free_ram_in_kb()`](https://github.com/artgins/yunetas/blob/7.28.11/kernel/c/gobj-c/src/helpers.c#L9971)

Returns the amount of free RAM available in the system in kilobytes. On Linux, reads from `/proc/meminfo`. On other systems, returns 0.

```C
unsigned long free_ram_in_kb(void);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `-` | `-` | This function does not take any parameters. |

**Returns**

The amount of free RAM in kilobytes.

---

(get_name_from_nn_table)=
## [`get_name_from_nn_table()`](https://github.com/artgins/yunetas/blob/7.28.11/kernel/c/gobj-c/src/helpers.c#L11182)

Looks up a name string in a number-to-name lookup table by matching a numeric value.

```C
const char *get_name_from_nn_table(
    const number_name_table_t *table,
    int number
);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `table` | `const number_name_table_t *` | Pointer to the lookup table. |
| `number` | `int` | The numeric value to search for. |

**Returns**

A pointer to the name string corresponding to the given number, or NULL if not found.

---

(get_number_from_nn_table)=
## [`get_number_from_nn_table()`](https://github.com/artgins/yunetas/blob/7.28.11/kernel/c/gobj-c/src/helpers.c#L11161)

Looks up a numeric value in a number-to-name lookup table by matching a name string.

```C
int get_number_from_nn_table(
    const number_name_table_t *table,
    const char *name
);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `table` | `const number_name_table_t *` | Pointer to the lookup table. |
| `name` | `const char *` | The name string to search for. |

**Returns**

The numeric value corresponding to the given name, or -1 if not found.

---

(get_yunetas_base)=
## [`get_yunetas_base()`](https://github.com/artgins/yunetas/blob/7.28.11/kernel/c/gobj-c/src/helpers.c#L11023)

Resolves the Yunetas base directory path. The environment variable `YUNETAS_BASE` takes priority, followed by standard paths.

```C
const char *get_yunetas_base(void);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `-` | `-` | This function does not take any parameters. |

**Returns**

A pointer to a static string containing the Yunetas base directory path.

---

(is_yuneta_user)=
## [`is_yuneta_user()`](https://github.com/artgins/yunetas/blob/7.28.11/kernel/c/gobj-c/src/helpers.c#L10955)

Checks whether a given username belongs to the "yuneta" group or is exactly the "yuneta" user.

```C
int is_yuneta_user(
    const char *username
);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `username` | `const char *` | The username to check. |

**Returns**

Returns TRUE if the user is "yuneta" or belongs to the "yuneta" group. Returns FALSE otherwise.

---

(print_open_fds)=
## [`print_open_fds()`](https://github.com/artgins/yunetas/blob/7.28.11/kernel/c/gobj-c/src/helpers.c#L10525)

Logs information about all currently open file descriptors in the process using a formatted prefix string.

```C
int print_open_fds(
    const char *fmt,
    ...
);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `fmt` | `const char *` | Printf-style format string for the log prefix. |
| `...` | variadic | Variable arguments for the format string. |

**Returns**

The number of open file descriptors found and logged.

---

(sha256_digest)=
## [`sha256_digest()`](https://github.com/artgins/yunetas/blob/7.28.11/kernel/c/gobj-c/src/helpers.c#L11513)

Computes the SHA-256 (FIPS 180-4) of a buffer. Standalone: no TLS backend is involved, so it works in any yuno, whatever `.config` selects.

```C
#define SHA256_DIGEST_LEN   32

void sha256_digest(
    const void *data,
    size_t      len,
    uint8_t     digest[SHA256_DIGEST_LEN]
);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `data` | `const void *` | The bytes to hash. |
| `len` | `size_t` | Number of bytes. `0` hashes the empty input. |
| `digest` | `uint8_t [32]` | Where the 32 raw bytes of the digest are written. |

**Returns**

Nothing: it cannot fail.

**Example**

```C
uint8_t digest[SHA256_DIGEST_LEN];
sha256_digest("abc", 3, digest);
/* digest[0] == 0xba, digest[1] == 0x78, ... */
```

---

(sha256_hex)=
## [`sha256_hex()`](https://github.com/artgins/yunetas/blob/7.28.11/kernel/c/gobj-c/src/helpers.c#L11555)

The SHA-256 of a buffer as 64 lowercase hex characters plus the terminating nul — the form a content address or a checksum field stores. It is [`sha256_digest()`](#sha256_digest), written out.

```C
#define SHA256_HEX_LEN      64

int sha256_hex(
    const void *data,
    size_t      len,
    char       *bf,
    size_t      bflen
);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `data` | `const void *` | The bytes to hash. |
| `len` | `size_t` | Number of bytes. |
| `bf` | `char *` | Output buffer. |
| `bflen` | `size_t` | Size of `bf`: at least `SHA256_HEX_LEN + 1` (65). |

**Returns**

`0` on success. `-1` when `bf` is NULL or smaller than 65 bytes, with the error *"buffer too small for a sha256"* logged; `bf` is not written.

**Example**

```C
char hex[SHA256_HEX_LEN + 1];
if(sha256_hex("abc", 3, hex, sizeof(hex)) == 0) {
    /* hex == "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad" */
}
```

---

(sha256_init)=
## [`sha256_init()`](https://github.com/artgins/yunetas/blob/7.28.11/kernel/c/gobj-c/src/helpers.c#L11436)

Starts the SHA-256 of bytes that come in parts: a file read a chunk at a time, data that arrives in frames. Give each part to [`sha256_update()`](#sha256_update) and end with [`sha256_final()`](#sha256_final) or [`sha256_final_hex()`](#sha256_final_hex): the digest is the one of [`sha256_digest()`](#sha256_digest) over all the parts joined. The context holds no memory, so there is nothing to free; it can be a local variable, or live in a struct for as long as the parts keep coming.

```C
typedef struct {
    uint32_t h[8];
    uint64_t len;           // bytes hashed
    uint8_t block[64];      // bytes not hashed yet, `used` of them
    size_t used;
} sha256_ctx_t;

void sha256_init(
    sha256_ctx_t *ctx
);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `ctx` | `sha256_ctx_t *` | The context. Init it again to hash something else after a final. |

**Returns**

Nothing: it cannot fail.

**Example**

```C
/* The sha256 of a file, 64 KB at a time */
sha256_ctx_t ctx;
char bf[65536];
char hex[SHA256_HEX_LEN + 1];
ssize_t got;

sha256_init(&ctx);
while((got = read(fd, bf, sizeof(bf))) > 0) {
    sha256_update(&ctx, bf, (size_t)got);
}
sha256_final_hex(&ctx, hex, sizeof(hex));
```

---

(sha256_update)=
## [`sha256_update()`](https://github.com/artgins/yunetas/blob/7.28.11/kernel/c/gobj-c/src/helpers.c#L11450)

Adds one part of the bytes to a SHA-256 started with [`sha256_init()`](#sha256_init). The parts can have any size, `0` included; how the bytes are cut does not change the digest.

```C
void sha256_update(
    sha256_ctx_t *ctx,
    const void   *data,
    size_t        len
);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `ctx` | `sha256_ctx_t *` | The context, init'ed and not yet final. |
| `data` | `const void *` | The bytes of this part. Can be `NULL` when `len` is `0`. |
| `len` | `size_t` | Number of bytes. |

**Returns**

Nothing: it cannot fail.

**Example**

```C
/* "abc" given in two parts is the sha256 of "abc" */
sha256_ctx_t ctx;
char hex[SHA256_HEX_LEN + 1];

sha256_init(&ctx);
sha256_update(&ctx, "a", 1);
sha256_update(&ctx, "bc", 2);
sha256_final_hex(&ctx, hex, sizeof(hex));
/* hex == "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad" */
```

---

(sha256_final)=
## [`sha256_final()`](https://github.com/artgins/yunetas/blob/7.28.11/kernel/c/gobj-c/src/helpers.c#L11484)

Ends a SHA-256 started with [`sha256_init()`](#sha256_init) and writes the 32 raw bytes of the digest. After it the context is spent: init it again to hash something else.

```C
void sha256_final(
    sha256_ctx_t *ctx,
    uint8_t       digest[SHA256_DIGEST_LEN]
);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `ctx` | `sha256_ctx_t *` | The context. |
| `digest` | `uint8_t [32]` | Where the digest is written. |

**Returns**

Nothing: it cannot fail.

**Example**

```C
sha256_ctx_t ctx;
uint8_t digest[SHA256_DIGEST_LEN];

sha256_init(&ctx);
sha256_update(&ctx, "abc", 3);
sha256_final(&ctx, digest);
/* digest[0] == 0xba, digest[1] == 0x78, ... */
```

---

(sha256_final_hex)=
## [`sha256_final_hex()`](https://github.com/artgins/yunetas/blob/7.28.11/kernel/c/gobj-c/src/helpers.c#L11569)

[`sha256_final()`](#sha256_final), written out as [`sha256_hex()`](#sha256_hex) writes it: 64 lowercase hex characters and the terminating nul.

```C
int sha256_final_hex(
    sha256_ctx_t *ctx,
    char         *bf,
    size_t        bflen
);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `ctx` | `sha256_ctx_t *` | The context. |
| `bf` | `char *` | Output buffer. |
| `bflen` | `size_t` | Size of `bf`: at least `SHA256_HEX_LEN + 1` (65). |

**Returns**

`0` on success, and the context is spent. `-1` when `bf` is NULL or smaller than 65 bytes, with the error *"buffer too small for a sha256"* logged: `bf` is not written and the context is not spent, so a final with a good buffer still gives the digest.

**Example**

```C
sha256_ctx_t ctx;
char hex[SHA256_HEX_LEN + 1];

sha256_init(&ctx);
sha256_update(&ctx, "", 0);
if(sha256_final_hex(&ctx, hex, sizeof(hex)) == 0) {
    /* hex == "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855" */
}
```

---

(source2base64_for_yunetas)=
## [`source2base64_for_yunetas()`](https://github.com/artgins/yunetas/blob/7.28.11/kernel/c/gobj-c/src/helpers.c#L11056)

Converts a file to base64-encoded format, finding the file in either the current path or relative to the Yunetas base directory.

```C
gbuffer_t *source2base64_for_yunetas(
    const char *source,
    char *comment,
    int commentlen
);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `source` | `const char *` | Path to the file to encode. |
| `comment` | `char *` | Output buffer for error messages. |
| `commentlen` | `int` | Size of the comment buffer. |

**Returns**

A `gbuffer_t` pointer containing the base64-encoded file content, or NULL if the file is not found.

---

(total_ram_in_kb)=
## [`total_ram_in_kb()`](https://github.com/artgins/yunetas/blob/7.28.11/kernel/c/gobj-c/src/helpers.c#L10018)

Returns the total amount of RAM installed in the system in kilobytes. On Linux, reads from `/proc/meminfo`.

```C
unsigned long total_ram_in_kb(void);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `-` | `-` | This function does not take any parameters. |

**Returns**

The total RAM in kilobytes.

---

(yuneta_getgrnam)=
## [`yuneta_getgrnam()`](https://github.com/artgins/yunetas/blob/7.28.11/kernel/c/gobj-c/src/helpers.c#L10929)

Portable wrapper around `getgrnam()` that looks up group information by name. Uses static implementations when `CONFIG_FULLY_STATIC` is defined.

```C
struct group *yuneta_getgrnam(
    const char *name
);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `name` | `const char *` | The group name to look up. |

**Returns**

A pointer to a `struct group`, or NULL if the group is not found.

---

(yuneta_getgrouplist)=
## [`yuneta_getgrouplist()`](https://github.com/artgins/yunetas/blob/7.28.11/kernel/c/gobj-c/src/helpers.c#L10938)

Portable wrapper around `getgrouplist()` that retrieves all groups a user belongs to.

```C
int yuneta_getgrouplist(
    const char *user,
    gid_t group,
    gid_t *groups,
    int *ngroups
);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `user` | `const char *` | The username to look up groups for. |
| `group` | `gid_t` | Primary group ID. |
| `groups` | `gid_t *` | Output array for group IDs. |
| `ngroups` | `int *` | IN/OUT — pointer to the size of the groups array. |

**Returns**

The number of groups on success. Returns -1 if the array is too small.

---

(yuneta_getpwnam)=
## [`yuneta_getpwnam()`](https://github.com/artgins/yunetas/blob/7.28.11/kernel/c/gobj-c/src/helpers.c#L10920)

Portable wrapper around `getpwnam()` that looks up user information by username.

```C
struct passwd *yuneta_getpwnam(
    const char *name
);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `name` | `const char *` | The username to look up. |

**Returns**

A pointer to a `struct passwd`, or NULL if the username is not found.

---

(yuneta_getpwuid)=
## [`yuneta_getpwuid()`](https://github.com/artgins/yunetas/blob/7.28.11/kernel/c/gobj-c/src/helpers.c#L10911)

Portable wrapper around `getpwuid()` that looks up user information by user ID.

```C
struct passwd *yuneta_getpwuid(
    uid_t uid
);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `uid` | `uid_t` | The user ID to look up. |

**Returns**

A pointer to a `struct passwd`, or NULL if the user ID is not found.

---

