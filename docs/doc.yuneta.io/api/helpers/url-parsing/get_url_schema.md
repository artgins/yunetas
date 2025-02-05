<!-- ============================================================== -->
(get_url_schema())=
# `get_url_schema()`
<!-- ============================================================== -->


The `get_url_schema` function extracts the schema (protocol) from a given URL. It populates the provided `schema` buffer with the extracted schema, ensuring that it does not exceed the specified `schema_size`. This function is useful for parsing URLs to determine the protocol being used, such as HTTP, HTTPS, FTP, etc.


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

int get_url_schema(
    hgobj   gobj,
    const char *uri,
    char *schema, size_t schema_size
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
  - A handle to the gobj instance that is calling this function.

* - `uri`
  - `const char *`
  - The URL string from which the schema is to be extracted.

* - `schema`
  - `char *`
  - A buffer where the extracted schema will be stored.

* - `schema_size`
  - `size_t`
  - The size of the `schema` buffer to prevent overflow.

:::


---

**Return Value**


Returns an integer indicating the success or failure of the operation. A return value of 0 indicates success, while a negative value indicates an error occurred during the extraction process.


**Notes**


Ensure that the `schema` buffer is large enough to hold the schema to avoid buffer overflows. The function does not perform checks for the validity of the `uri` format.


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

