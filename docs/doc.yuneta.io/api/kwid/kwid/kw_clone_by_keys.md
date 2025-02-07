<!-- ============================================================== -->
(kw_clone_by_keys)=
# `kw_clone_by_keys()`
<!-- ============================================================== -->

The function `kw_clone_by_keys()` returns a new JSON object containing only the keys specified in `keys`. The keys can be provided as a string, a list of strings, or a dictionary where the keys are the desired fields. If `keys` is empty, the original JSON object is returned.

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
json_t *kw_clone_by_keys(
    hgobj   gobj,
    json_t *kw,     // owned
    json_t *keys,   // owned
    BOOL    verbose
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
  - A handle to the GObj (Generic Object) system, used for logging and error handling.

* - `kw`
  - `json_t *`
  - The JSON object to be filtered. This parameter is owned and will be decremented.

* - `keys`
  - `json_t *`
  - A JSON object specifying the keys to retain. It can be a string, a list of strings, or a dictionary. This parameter is owned and will be decremented.

* - `verbose`
  - `BOOL`
  - If `TRUE`, logs an error when a specified key is not found in `kw`.
:::

---

**Return Value**

A new JSON object containing only the specified keys. If `keys` is empty, the original `kw` is returned. The caller owns the returned JSON object.

**Notes**

If `keys` is a dictionary or list, only the keys present in `keys` will be retained in the returned JSON object. If `verbose` is enabled, missing keys will be logged as errors.

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

