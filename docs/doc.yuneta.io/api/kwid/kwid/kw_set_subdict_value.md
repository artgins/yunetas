<!-- ============================================================== -->
(kw_set_subdict_value)=
# `kw_set_subdict_value()`
<!-- ============================================================== -->

The function `kw_set_subdict_value()` sets a key-value pair inside a subdictionary located at the specified path within a JSON object. If the subdictionary does not exist, it is created.

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
int kw_set_subdict_value(
    hgobj      gobj,
    json_t     *kw,
    const char *path,
    const char *key,
    json_t     *value // owned
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
  - A handle to the gobj (generic object) that may be used for logging or context.

* - `kw`
  - `json_t *`
  - A JSON object where the subdictionary is located.

* - `path`
  - `const char *`
  - The path to the subdictionary within `kw`. The last segment of the path is the subdictionary name.

* - `key`
  - `const char *`
  - The key to be set inside the subdictionary.

* - `value`
  - `json_t *`
  - The JSON value to be assigned to `key`. Ownership is transferred to the function.
:::

---

**Return Value**

Returns `0` on success, or `-1` if an error occurs (e.g., if `kw` is not a valid JSON object).

**Notes**

If the subdictionary at `path` does not exist, it is created as a new JSON object. The function uses [`kw_get_dict()`](#kw_get_dict) to retrieve or create the subdictionary.

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
