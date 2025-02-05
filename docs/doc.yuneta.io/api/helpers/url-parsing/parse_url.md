<!-- ============================================================== -->
(parse_url())=
# `parse_url()`
<!-- ============================================================== -->


The `parse_url` function extracts components from a given URL (`uri`). It populates the provided buffers with the schema, host, port, path, and query parameters of the URL. The function can operate in a mode where it ignores the schema if specified by the `no_schema` parameter.


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
  - The gobj context in which the URL is being parsed.

* - `uri`
  - `const char *`
  - The URL string to be parsed.

* - `schema`
  - `char *`
  - A buffer to store the extracted schema (e.g., "http", "https").

* - `schema_size`
  - `size_t`
  - The size of the schema buffer.

* - `host`
  - `char *`
  - A buffer to store the extracted host.

* - `host_size`
  - `size_t`
  - The size of the host buffer.

* - `port`
  - `char *`
  - A buffer to store the extracted port.

* - `port_size`
  - `size_t`
  - The size of the port buffer.

* - `path`
  - `char *`
  - A buffer to store the extracted path.

* - `path_size`
  - `size_t`
  - The size of the path buffer.

* - `query`
  - `char *`
  - A buffer to store the extracted query string.

* - `query_size`
  - `size_t`
  - The size of the query buffer.

* - `no_schema`
  - `BOOL`
  - If TRUE, the function will not extract the schema, focusing only on the host and port.
:::


---

**Return Value**


The function returns an integer indicating the success or failure of the parsing operation. A return value of 0 typically indicates success, while a negative value indicates an error occurred during parsing.


**Notes**


This function modifies the provided buffers to store the parsed components of the URL. Ensure that the buffers are large enough to hold the expected values to avoid buffer overflows.


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

