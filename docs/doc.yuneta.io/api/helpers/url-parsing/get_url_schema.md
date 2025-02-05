<!-- ============================================================== -->
(get_url_schema())=
# `get_url_schema()`
<!-- ============================================================== -->


The `get_url_schema()` function extracts the schema (protocol) from a given URL string and stores it in the provided buffer. 
This function is useful for parsing URLs to determine their schema, such as `http`, `https`, `ftp`, etc.


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

PUBLIC int get_url_schema(
    hgobj   gobj,
    const char *uri,
    char    *schema,
    size_t  schema_size
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
  - The gobj context in which the function is executed.

* - `uri`
  - `const char *`
  - The URL string from which the schema will be extracted.

* - `schema`
  - `char *`
  - A buffer where the extracted schema will be stored.

* - `schema_size`
  - `size_t`
  - The size of the `schema` buffer to ensure no overflow occurs.

:::


---

**Return Value**


Returns an integer indicating the success or failure of the operation:
- `0`: Success, the schema was successfully extracted.
- `-1`: Failure, an error occurred (e.g., invalid URL or insufficient buffer size).


**Notes**


- Ensure that the `schema` buffer is large enough to hold the extracted schema, including the null terminator.
- The function does not validate the full URL, only the schema portion.
- If the `uri` does not contain a schema, the function will return `-1`.


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

