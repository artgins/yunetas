<!-- ============================================================== -->
(parse_url())=
# `parse_url()`
<!-- ============================================================== -->

Parses a given URL into its components, including schema, host, port, path, and query. Supports optional schema parsing.

<!------------------------------------------------------------>
<!--                    Prototypes                          -->
<!------------------------------------------------------------>

``````{tab-set}

`````{tab-item} C

<!--====================================================-->
<!--                    Tab C                           -->
<!--====================================================-->

**Prototype**

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

::: {list-table}
:widths: 20 20 60
:header-rows: 1

* - Key
  - Type
  - Description

* - `gobj`
  - `hgobj`
  - Handle to the calling object, used for logging errors.

* - `uri`
  - `const char *`
  - The URL string to be parsed.

* - `schema`
  - `char *`
  - Buffer to store the extracted schema (e.g., 'http').

* - `schema_size`
  - `size_t`
  - Size of the schema buffer.

* - `host`
  - `char *`
  - Buffer to store the extracted host (e.g., 'example.com').

* - `host_size`
  - `size_t`
  - Size of the host buffer.

* - `port`
  - `char *`
  - Buffer to store the extracted port (e.g., '8080').

* - `port_size`
  - `size_t`
  - Size of the port buffer.

* - `path`
  - `char *`
  - Buffer to store the extracted path (e.g., '/index.html').

* - `path_size`
  - `size_t`
  - Size of the path buffer.

* - `query`
  - `char *`
  - Buffer to store the extracted query string (e.g., 'id=123').

* - `query_size`
  - `size_t`
  - Size of the query buffer.

* - `no_schema`
  - `BOOL`
  - If TRUE, the function does not parse the schema.
:::

---

**Return Value**

Returns 0 on success, or -1 if the URL cannot be parsed.

**Notes**

Uses `http_parser_parse_url()` to extract URL components. If `no_schema` is TRUE, the schema is ignored.

<!--====================================================-->
<!--                    End Tab C                       -->
<!--====================================================-->

`````

`````{tab-item} JS

<!--====================================================-->
<!--                    Tab JS                          -->
<!--====================================================-->

<!---------------------------------------------------->
<!--                JS Prototype                    -->
<!---------------------------------------------------->

**Prototype**

````JS
// Not applicable in JS
````

<!--====================================================-->
<!--                    EndTab JS                       -->
<!--====================================================-->

`````

`````{tab-item} Python

<!--====================================================-->
<!--                    Tab Python                      -->
<!--====================================================-->

<!---------------------------------------------------->
<!--                Python Prototype                -->
<!---------------------------------------------------->

**Prototype**

````Python
# Not applicable in Python
````

<!--====================================================-->
<!--                    End Tab Python                   -->
<!--====================================================-->

`````

``````

<!------------------------------------------------------------>
<!--                    Examples                            -->
<!------------------------------------------------------------>

```````{dropdown} Examples

``````{tab-set}

`````{tab-item} C

<!--====================================================-->
<!--                    Tab C                           -->
<!--====================================================-->

<!---------------------------------------------------->
<!--                C examples                      -->
<!---------------------------------------------------->

````C
// TODO C examples
````

<!--====================================================-->
<!--                    End Tab C                       -->
<!--====================================================-->

`````

`````{tab-item} JS

<!--====================================================-->
<!--                    Tab JS                          -->
<!--====================================================-->

<!---------------------------------------------------->
<!--                JS examples                     -->
<!---------------------------------------------------->

````JS
// TODO JS examples
````

<!--====================================================-->
<!--                    EndTab JS                       -->
<!--====================================================-->

`````

`````{tab-item} Python

<!--====================================================-->
<!--                    Tab Python                      -->
<!--====================================================-->

<!---------------------------------------------------->
<!--                Python examples                 -->
<!---------------------------------------------------->

````python
# TODO Python examples
````

<!--====================================================-->
<!--                    End Tab Python                  -->
<!--====================================================-->

`````

``````

```````

