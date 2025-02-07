<!-- ============================================================== -->
(kw_get_subdict_value)=
# `kw_get_subdict_value()`
<!-- ============================================================== -->

Retrieve a value from a subdictionary within a JSON object. If the subdictionary does not exist, it can be created based on the provided flags. The function searches for the key within the subdictionary and returns the corresponding value.

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
json_t *kw_get_subdict_value(
    hgobj      gobj,
    json_t    *kw,
    const char *path,
    const char *key,
    json_t    *jn_default_value,  // owned
    kw_flag_t  flag
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
  - Pointer to the GObj (generic object) context.

* - `kw`
  - `json_t *`
  - JSON object containing the dictionary to search within.

* - `path`
  - `const char *`
  - Path to the subdictionary within the JSON object.

* - `key`
  - `const char *`
  - Key to retrieve from the subdictionary.

* - `jn_default_value`
  - `json_t *`
  - Default value to return if the key is not found. This value is owned by the caller.

* - `flag`
  - `kw_flag_t`
  - Flags controlling behavior, such as whether to create the subdictionary if it does not exist.
:::

---

**Return Value**

Returns the JSON value associated with the specified key in the subdictionary. If the key is not found, the default value is returned. If `KW_CREATE` is set, the subdictionary is created if it does not exist.

**Notes**

If `KW_REQUIRED` is set and the key is not found, an error is logged. If `KW_EXTRACT` is set, the value is removed from the dictionary before returning.

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

