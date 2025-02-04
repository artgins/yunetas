<!-- ============================================================== -->
(parse_url())=
# `parse_url()`
<!-- ============================================================== -->


The `parse_url` function extracts components from a given URI string and stores them in separate buffers.

- `gobj`: The gobj instance associated with the function.
- `uri`: The URI string to be parsed.
- `schema`: Buffer to store the schema extracted from the URI.
- `schema_size`: Size of the `schema` buffer.
- `host`: Buffer to store the host extracted from the URI.
- `host_size`: Size of the `host` buffer.
- `port`: Buffer to store the port extracted from the URI.
- `port_size`: Size of the `port` buffer.
- `path`: Buffer to store the path extracted from the URI.
- `path_size`: Size of the `path` buffer.
- `query`: Buffer to store the query extracted from the URI.
- `query_size`: Size of the `query` buffer.
- `no_schema`: Flag to indicate if only host:port is present in the URI.


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
  - The gobj instance associated with the function.
* - `uri`
  - `const char *`
  - The URI string to be parsed.
* - `schema`
  - `char *`
  - Buffer to store the schema extracted from the URI.
* - `schema_size`
  - `size_t`
  - Size of the `schema` buffer.
* - `host`
  - `char *`
  - Buffer to store the host extracted from the URI.
* - `host_size`
  - `size_t`
  - Size of the `host` buffer.
* - `port`
  - `char *`
  - Buffer to store the port extracted from the URI.
* - `port_size`
  - `size_t`
  - Size of the `port` buffer.
* - `path`
  - `char *`
  - Buffer to store the path extracted from the URI.
* - `path_size`
  - `size_t`
  - Size of the `path` buffer.
* - `query`
  - `char *`
  - Buffer to store the query extracted from the URI.
* - `query_size`
  - `size_t`
  - Size of the `query` buffer.
* - `no_schema`
  - `BOOL`
  - Flag to indicate if only host:port is present in the URI.
:::


---

**Return Value**


Returns an integer indicating the success or failure of the parsing operation:
- `0` on success.
- `-1` if an error occurs during parsing.


**Notes**


- The function extracts the schema, host, port, path, and query components from the given URI string.
- If the `no_schema` flag is set to `TRUE`, only the host and port components are extracted.


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

