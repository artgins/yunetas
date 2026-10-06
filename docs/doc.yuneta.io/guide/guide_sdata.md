(sdata)=
# **SData**


**`Structured Data`** (`SData`) is a mechanism to define and manage structured fields,
`attributes`, and `commands` in a hierarchical and schema-driven manner.
It is used to define the attributes of objects, command parameters,
and database-like records in a highly structured and reusable way.

---

## Core Concepts

### SData Fields
SData fields are descriptors that define individual fields or attributes. These fields include information about their type, name, default value, description, flags, and other properties.

### SData Tables
SData tables are arrays of field descriptors (`sdata_desc_t`) that define structured data. These tables allow hierarchical definitions. This enables the creation of complex schemas.

---

## Data Types (`data_type_t`)

The `data_type_t` enumeration defines the types of data that SData fields can represent:

| Type | Description |
|---|---|
| `DTP_STRING`  | A text string. |
| `DTP_BOOLEAN` | A boolean value (`TRUE` or `FALSE`). |
| `DTP_INTEGER` | An integer value. |
| `DTP_REAL`    | A floating-point number. |
| `DTP_LIST`    | A list (array) of values. |
| `DTP_DICT`    | A dictionary (key-value pairs). |
| `DTP_JSON`    | A JSON object. |
| `DTP_POINTER` | A generic pointer. |


### Data Type Utilities
- **String types:** `DTP_IS_STRING(type)`
- **Boolean types:** `DTP_IS_BOOLEAN(type)`
- **Integer types:** `DTP_IS_INTEGER(type)`
- **Number types:** `DTP_IS_NUMBER(type)` (includes integer, real, and boolean)
- **List types:** `DTP_IS_LIST(type)`
- **Dictionary types:** `DTP_IS_DICT(type)`
- **JSON types:** `DTP_IS_JSON(type)`
- **Pointer types:** `DTP_IS_POINTER(type)`

---

(sdata_flag_t)=
## Field Flags (`sdata_flag_t`)

The `sdata_flag_t` enumeration defines the properties and characteristics of each field. Flags are bitwise-combinable to give fields multiple properties.

| Flag | Description |
|---|---|
| `SDF_NOTACCESS` | Field is not accessible. |
| `SDF_RD`        | Field is read-only. |
| `SDF_WR`        | Field is writable (and readable). |
| `SDF_REQUIRED`  | Field is required. It must not be null. |
| `SDF_PERSIST`   | Field is persistent and must be saved/loaded. |
| `SDF_VOLATIL`   | Field is volatile and must not be saved/loaded. |
| `SDF_RESOURCE`  | Field is a resource, referencing another schema. |
| `SDF_PKEY`      | Field is a primary key. |
| `SDF_STATS`     | Field holds statistical data (metadata). |
| `SDF_RSTATS`    | Field holds resettable statistics, implicitly `SDF_STATS`. |
| `SDF_PSTATS`    | Field holds persistent statistics, implicitly `SDF_STATS`. |
| `SDF_AUTHZ_R`   | Read access requires authorization (`__read_attribute__`). |
| `SDF_AUTHZ_W`   | Write access requires authorization (`__write_attribute__`). |
| `SDF_AUTHZ_X`   | Execution requires authorization (`__execute_command__`). |
| `SDF_AUTHZ_P`   | Authorization constraint parameter. |
| `SDF_AUTHZ_S`   | Stats read requires authorization (`__read_stats__`). |
| `SDF_AUTHZ_RS`  | Stats reset requires authorization (`__reset_stats__`). |
| `SDF_SECRET`    | A secret: read and saved as usual, SHOWN as `********`. |

(SDF_NOTACCESS)=
### SDF_NOTACCESS
    Field is not accessible.

(SDF_RD)=
### SDF_RD
    Field is read-only.

(SDF_WR)=
### SDF_WR
    Field is writable (and readable). Only an attribute with SDF_WR can be
    written at run time by `write-attr` (`gobj_is_writable_attr()`).

(SDF_REQUIRED)=
### SDF_REQUIRED
    Field is required. It must not be null.

(SDF_PERSIST)=
### SDF_PERSIST
    Field is persistent and must be saved/loaded. It does not make the
    field writable by `write-attr`: without SDF_WR it is set by the config,
    or by the gclass's own command, which checks the value (up to 7.25.22
    SDF_PERSIST alone was writable, so `write-attr` went round the checks
    of such a command).

```C
SDATA (DTP_INTEGER, "max_sessions_per_user", SDF_PERSIST,        "0", "config or set-max-sessions"),
SDATA (DTP_INTEGER, "cert_sync_interval_sec",SDF_WR|SDF_PERSIST, "900", "config or write-attr"),
```

(SDF_VOLATIL)=
### SDF_VOLATIL
    Field is volatile and must not be saved/loaded.

(SDF_RESOURCE)=
### SDF_RESOURCE
    Field is a resource, referencing another schema.

(SDF_PKEY)=
### SDF_PKEY
    Field is a primary key.

(SDF_STATS)=
### SDF_STATS
    Field holds statistical data (metadata).

(SDF_RSTATS)=
### SDF_RSTATS
    Field holds resettable statistics, implicitly `SDF_STATS`.

(SDF_PSTATS)=
### SDF_PSTATS
    Field holds persistent statistics, implicitly `SDF_STATS`.

(SDF_AUTHZ_R)=
### SDF_AUTHZ_R
    Read access requires authorization (`__read_attribute__`).

(SDF_AUTHZ_W)=
### SDF_AUTHZ_W
    Write access requires authorization (`__write_attribute__`).

(SDF_AUTHZ_X)=
### SDF_AUTHZ_X
    Execution requires authorization (`__execute_command__`).

(SDF_AUTHZ_S)=
### SDF_AUTHZ_S
    Stats read requires authorization (`__read_stats__`).

(SDF_AUTHZ_P)=
### SDF_AUTHZ_P
    Authorization constraint parameter.

(SDF_AUTHZ_RS)=
### SDF_AUTHZ_RS
    Stats reset requires authorization (`__reset_stats__`).

(SDF_SECRET)=
### SDF_SECRET
    A secret -- a password, a client secret, a token. Since 7.25.19. The
    attr is read, written and persisted exactly as without the flag (the
    emailsender still sends its real password, the persistent-attrs file
    still keeps it); what changes is what is SHOWN: `view-attrs`,
    `write-attr`, `list-persistent-attrs`, `view-gobj`, `view-config`, the
    start-up trace of the yuno's attrs and the `create_delete2` trace of a
    gobj being built answer `********` for it, whatever its json type (a
    number or a boolean too). An absent value or an empty string is shown
    as it is, so "not set" still reads as such. A dict of attrs is masked with
    [`gobj_mask_secret_attrs()`](#gobj_mask_secret_attrs), a whole
    configuration with [`gobj_mask_secret_config()`](#gobj_mask_secret_config).
    The C_TCP `traffic` dump prints the bytes on the wire: a frame that
    carries a credential is sent with `"__secret__": true` in the kw of
    `EV_TX_DATA` (or marked with [`gbuffer_set_secret()`](#gbuffer_set_secret))
    and is dumped as `<N bytes hidden>`.

```C
SDATA (DTP_STRING, "password", SDF_PERSIST|SDF_SECRET, "", "email password"),
```

```text
> command-yuno id=1 service=__yuno__ command=view-attrs attribute=password
{ "C_EMAILSENDER^emailsender": "********" }
```

    The persistent-attrs file itself (`<realm>/<yuno>/data/*-persistent-attrs.json`)
    is written 0600 since 7.25.19: up to 7.25.18 it took the process umask,
    0666 on every node, and it holds these secrets in clear. A file of
    the yuno's own that is not as a save writes it -- left wider by an
    older release, or a hard link -- is REPLACED when it is loaded, by a
    0600 one with the same content (logged as *"Persistent attrs file
    replaced by a 0600 one of the yuno's own"*), not only at its next
    save; nothing is changed through its other names. A file of another
    user is left as it is, with a warning: a load only reads.
    A symlink in place of the file is not read (*"Refused the persistent
    attrs file: it is a symlink"*).

    A save writes a NEW file in the same directory (`<file>.tmp-XXXXXX`,
    created 0600 with `O_EXCL`), syncs it, and renames it over the old
    one: a hard link or a symlink in its place is replaced (nothing is
    written through it), and the old file is never truncated before the
    new one is complete -- a save that fails leaves it as it was. A
    `<file>.tmp-XXXXXX` left by a save that did not end is removed at the
    next load, logged (one that is not a regular file is left, logged).

    Whose file it is after a save: the yuno's user's. A file there of
    another user was loaded first, so its attrs are in the save and
    nothing is lost: run as the yuno's user, the save takes it over
    (logged at INFO with the old owner); run as root (once, to debug), the
    new file is given to the old owner, so root never takes the file from
    the yuno. A file there that CANNOT be read refuses the save (its other
    attrs would be lost): remove it (its attrs go back to their defaults)
    or repair it. An empty file is no data and refuses nothing.

    In a directory the yuno cannot write the save goes IN PLACE, into a
    file of the yuno's own, regular and of one name only: room is reserved
    first without growing the file, a shorter content is padded with blanks
    and cut only after it is on disk, and a write that stops half way (a
    full disk where the room could not be reserved, or on a copy-on-write
    filesystem) writes the old content back. A crash from the write until
    its sync returns, or a write back that fails too, can leave a file that
    cannot be parsed -- the "never truncated" above holds for the rename,
    not for this path -- and that file refuses the next saves as above. `write-attr` answers a save that fails
    (*"<gobj>: <attr> written, but NOT saved (see the log)"*, `result`
    -1); up to 7.25.20 it answered "done" with nothing on disk. A
    persistent attr of a gobj that is no service is written and NOT
    persisted, and the answer says so (*"..., NOT persisted (only a
    service persists its attrs)"*, with a warning in the log).

    A COMMAND PARAMETER takes the flag too: the `commands` trace prints
    the command line, and with `ev_kw` its kw, with the parameter masked
    (see [`command_mask_secret_kw()`](#command_mask_secret_kw)). A key the
    command table cannot know -- the free keys of a `SDF_WILD_CMD` command,
    which the agent's `command-yuno` forwards to another yuno -- is masked
    when its NAME is a secret's ([`is_secret_name()`](#is_secret_name)).

```C
SDATAPM (DTP_STRING,    "password",     SDF_SECRET,     0,          "Password"),
```

```text
🌀🌀 mach(C_AUTHZ^authz), cmd: set-user-pwd username=bob password=********
🌀🌀 mach(C_AGENT^agent), cmd: command-yuno id=1 service=authz command=set-user-pwd password=********
```


## Common Flag Combinations
- **Public Attributes:** Combine `SDF_RD|SDF_WR|SDF_STATS|SDF_PERSIST|SDF_VOLATIL|SDF_RSTATS|SDF_PSTATS`.
- **Writable and persistent Attributes:** Combine `SDF_WR|SDF_PERSIST`.

---

(sdata_desc_t)=
## Descriptor Fields (`sdata_desc_t`)

The `sdata_desc_t` structure defines a field or schema. Each descriptor specifies the following:

| Field | Type | Description |
|---|---|---|
| `type`          | `data_type_t`          | The type of the field (for example string, boolean). |
| `name`          | `const char *`         | The name of the field. |
| `alias`         | `const char **`        | Alternative names (aliases) for the field. |
| `flag`          | `sdata_flag_t`         | Flags defining the field's properties. |
| `default_value` | `const char *`         | The default value of the field. |
| `header`        | `const char *`         | Header text for table columns. |
| `fillspace`     | `int`                  | Column width for table formatting. |
| [`description`](https://github.com/artgins/yunetas/blob/7.26.5/utils/c/yuno-skeleton/make_skeleton.c#L199)   | `const char *`         | A description of the field's purpose. |
| `json_fn`       | `json_function_fn`     | Custom function for processing JSON data. |
| `schema`        | `const sdata_desc_t *` | Pointer to a sub-schema for compound fields. |
| `authpth`       | `const char *`         | Authorization path for accessing or modifying the field. |


---

## Application

### Attributes
SData tables define attributes by listing fields with their types, default values, and flags. These fields form the basis of object definitions. This enables schema-based validation and management.

### Commands
SData tables can define commands with associated parameters, schemas, and descriptions. Commands extend the function of objects, providing structured inputs and outputs.

### Nested Schemas
Fields in SData can reference other schemas. This enables hierarchical definitions. This allows for the creation of complex, nested structures while maintaining clarity and reusability.
