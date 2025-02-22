<!-- ============================================================== -->
(kw_set_dict_value())=
# `kw_set_dict_value()`
<!-- ============================================================== -->

The function `kw_set_dict_value()` sets a value in a JSON dictionary at the specified path. If intermediate objects do not exist, they are created as dictionaries. Arrays are not created automatically.

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
int kw_set_dict_value(
    hgobj      gobj,
    json_t    *kw,
    const char *path,   // The last word after delimiter (.) is the key
    json_t    *value    // owned
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
  - A handle to the GObj context, used for logging and error handling.

* - `kw`
  - `json_t *`
  - A JSON dictionary where the value will be set. Must be a valid JSON object.

* - `path`
  - `const char *`
  - A dot-delimited path specifying where to set the value. The last segment is the key.

* - `value`
  - `json_t *`
  - The JSON value to set at the specified path. Ownership is transferred.
:::

---

**Return Value**

Returns `0` on success, or `-1` if an error occurs (e.g., if `kw` is not a dictionary).

**Notes**

If the path does not exist, intermediate objects are created as dictionaries. Arrays are not created automatically. The function uses [`kw_find_path()`](#kw_find_path()) internally to navigate the JSON structure.

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
