<!-- ============================================================== -->
(parse_url())=
# `parse_url()`
<!-- ============================================================== -->


The `parse_url` function is designed to parse a given URI into its constituent components, including the schema, host, port, path, and query string. It takes a URI as input and extracts these elements, storing them in the provided buffers. The function also allows for the option to ignore the schema if specified.


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
    hgobj   gobj,
    const char *uri,
    char    *schema,     size_t schema_size,
    char    *host,       size_t host_size,
    char    *port,       size_t port_size,
    char    *path,       size_t path_size,
    char    *query,      size_t query_size,
    BOOL    no_schema    // only host:port
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
  - The gobj instance that is associated with the parsing operation.

* - `uri`
  - `const char *`
  - The URI string to be parsed.

* - `schema`
  - `char *`
  - A buffer to store the extracted schema (e.g., "http", "https"). Must be large enough to hold the schema.

* - `schema_size`
  - `size_t`
  - The size of the schema buffer.

* - `host`
  - `char *`
  - A buffer to store the extracted host. Must be large enough to hold the host.

* - `host_size`
  - `size_t`
  - The size of the host buffer.

* - `port`
  - `char *`
  - A buffer to store the extracted port. Must be large enough to hold the port.

* - `port_size`
  - `size_t`
  - The size of the port buffer.

* - `path`
  - `char *`
  - A buffer to store the extracted path. Must be large enough to hold the path.

* - `path_size`
  - `size_t`
  - The size of the path buffer.

* - `query`
  - `char *`
  - A buffer to store the extracted query string. Must be large enough to hold the query.

* - `query_size`
  - `size_t`
  - The size of the query buffer.

* - `no_schema`
  - `BOOL`
  - A flag indicating whether to ignore the schema in the parsing process (only host:port will be extracted if TRUE).
:::


---

**Return Value**


Returns an integer indicating the success or failure of the parsing operation. A return value of 0 typically indicates success, while a non-zero value indicates an error occurred during parsing.


**Notes**


Ensure that the provided buffers are adequately sized to hold the expected components of the URI. If the buffers are too small, the function may not populate them correctly, leading to potential buffer overflows or incomplete data.


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

