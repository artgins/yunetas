<!-- ============================================================== -->
(parse_url())=
# `parse_url()`
<!-- ============================================================== -->


The `parse_url()` function parses a given URI into its components: schema, host, port, path, and query. 
It is designed to handle various URL formats and extract the specified parts into separate buffers. 
If the `no_schema` parameter is set to TRUE, the function skips parsing the schema and focuses on the host and port.


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

PUBLIC int parse_url(
    hgobj   gobj,
    const char *uri,
    char    *schema, size_t schema_size,
    char    *host, size_t host_size,
    char    *port, size_t port_size,
    char    *path, size_t path_size,
    char    *query, size_t query_size,
    BOOL    no_schema
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
  - The gobj context used for logging or error handling.

* - `uri`
  - `const char *`
  - The URI string to be parsed.

* - `schema`
  - `char *`
  - Buffer to store the extracted schema (e.g., `http`, `https`).

* - `schema_size`
  - `size_t`
  - The size of the `schema` buffer.

* - `host`
  - `char *`
  - Buffer to store the extracted host.

* - `host_size`
  - `size_t`
  - The size of the `host` buffer.

* - `port`
  - `char *`
  - Buffer to store the extracted port.

* - `port_size`
  - `size_t`
  - The size of the `port` buffer.

* - `path`
  - `char *`
  - Buffer to store the extracted path.

* - `path_size`
  - `size_t`
  - The size of the `path` buffer.

* - `query`
  - `char *`
  - Buffer to store the extracted query string.

* - `query_size`
  - `size_t`
  - The size of the `query` buffer.

* - `no_schema`
  - `BOOL`
  - If TRUE, the schema is not parsed, and only the host and port are extracted.

:::


---

**Return Value**


Returns 0 on success, or -1 if an error occurs during parsing. Errors may include invalid URI formats or insufficient buffer sizes.


**Notes**


- Ensure that all buffers (`schema`, `host`, `port`, `path`, `query`) are properly allocated and their sizes are correctly specified.
- If `no_schema` is TRUE, the `schema` parameter and its buffer are ignored.
- This function does not validate the correctness of the URI components beyond basic parsing.


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

