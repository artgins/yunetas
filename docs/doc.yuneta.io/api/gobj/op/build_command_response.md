<!-- ============================================================== -->
(build_command_response)=
# `build_command_response()`
<!-- ============================================================== -->

Constructs a JSON response object containing the result, comment, schema, and data fields.

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
json_t *build_command_response(
    hgobj gobj,
    json_int_t result,
    json_t *jn_comment, // owned, if null then not set
    json_t *jn_schema,  // owned, if null then not set
    json_t *jn_data     // owned, if null then not set
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
  - The GObj instance associated with the response.

* - `result`
  - `json_int_t`
  - The result code of the command execution.

* - `jn_comment`
  - `json_t *`
  - A JSON string containing a comment or message. If null, it is not set.

* - `jn_schema`
  - `json_t *`
  - A JSON object representing the schema of the response. If null, it is not set.

* - `jn_data`
  - `json_t *`
  - A JSON object containing the response data. If null, it is not set.
:::

---

**Return Value**

A JSON object containing the response structure with 'result', 'comment', 'schema', and 'data' fields.

**Notes**

The caller is responsible for managing the memory of the returned JSON object. If any of the optional parameters (jn_comment, jn_schema, jn_data) are null, they are replaced with default values.

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
