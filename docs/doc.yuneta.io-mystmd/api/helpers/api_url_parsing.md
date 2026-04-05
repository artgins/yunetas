# URL Parsing Functions

Source code in:

- [helpers.h](https://github.com/artgins/yunetas/blob/main/kernel/c/gobj-c/src/helpers.h)
- [helpers.c](https://github.com/artgins/yunetas/blob/main/kernel/c/gobj-c/src/helpers.c)

(get_url_schema)=
## `get_url_schema()`

Extracts the schema (protocol) from a given URL and stores it in the provided buffer.

```C
int get_url_schema(
    hgobj gobj,
    const char *uri,
    char *schema,    size_t schema_size
);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `gobj` | `hgobj` | A handle to the GObj instance, used for logging errors. |
| `uri` | `const char *` | The URL string from which the schema will be extracted. |
| `schema` | `char *` | A buffer where the extracted schema will be stored. |
| `schema_size` | `size_t` | The size of the schema buffer to prevent overflow. |

**Returns**

Returns 0 on success if the schema is extracted successfully, or -1 if no schema is found or an error occurs.

**Notes**

Uses `http_parser_parse_url()` to parse the URL. If no schema is found, an error is logged using [`gobj_log_error()`](<#gobj_log_error>).

---

(parse_url)=
## `parse_url()`

Parses a given URL into its components, including schema, host, port, path, and query. Supports optional schema parsing.

```C
int parse_url(
    hgobj gobj,
    const char *uri,
    char *schema, size_t schema_size,
    char *host, size_t host_size,
    char *port, size_t port_size,
    char *path, size_t path_size,
    char *query, size_t query_size,
    BOOL no_schema
);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `gobj` | `hgobj` | Handle to the calling object, used for logging errors. |
| `uri` | `const char *` | The URL string to be parsed. |
| `schema` | `char *` | Buffer to store the extracted schema (e.g., 'http'). |
| `schema_size` | `size_t` | Size of the schema buffer. |
| `host` | `char *` | Buffer to store the extracted host (e.g., 'example.com'). |
| `host_size` | `size_t` | Size of the host buffer. |
| `port` | `char *` | Buffer to store the extracted port (e.g., '8080'). |
| `port_size` | `size_t` | Size of the port buffer. |
| `path` | `char *` | Buffer to store the extracted path (e.g., '/index.html'). |
| `path_size` | `size_t` | Size of the path buffer. |
| `query` | `char *` | Buffer to store the extracted query string (e.g., 'id=123'). |
| `query_size` | `size_t` | Size of the query buffer. |
| `no_schema` | `BOOL` | If TRUE, the function does not parse the schema. |

**Returns**

Returns 0 on success, or -1 if the URL cannot be parsed.

**Notes**

Uses `http_parser_parse_url()` to extract URL components. If `no_schema` is TRUE, the schema is ignored.

---
