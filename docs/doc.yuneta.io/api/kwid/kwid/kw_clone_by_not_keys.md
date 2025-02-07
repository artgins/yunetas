<!-- ============================================================== -->
(kw_clone_by_not_keys)=
# `kw_clone_by_not_keys()`
<!-- ============================================================== -->

Return a new JSON object excluding the specified keys from the input [`json_t *`](#json_t). The keys can be provided as a string, dictionary, or list. If no keys are specified, an empty JSON object is returned.

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
json_t *kw_clone_by_not_keys(
    hgobj      gobj,
    json_t     *kw,     // owned
    json_t     *keys,   // owned
    BOOL       verbose
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
  - Pointer to the GObj instance for logging and error handling.

* - `kw`
  - `json_t *`
  - The input JSON object to be cloned, excluding the specified keys. This parameter is owned and will be decremented.

* - `keys`
  - `json_t *`
  - A JSON object, list, or string specifying the keys to exclude from the cloned object. This parameter is owned and will be decremented.

* - `verbose`
  - `BOOL`
  - If `TRUE`, logs an error when a specified key is not found in the input JSON object.
:::

---

**Return Value**

A new [`json_t *`](#json_t) object with the specified keys removed. If no keys are specified, an empty JSON object is returned.

**Notes**

The function performs a shallow copy of the input JSON object, removing the specified keys. The input `kw` and `keys` parameters are owned and will be decremented.

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
