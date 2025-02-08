<!-- ============================================================== -->
(kw_get_list())=
# `kw_get_list()`
<!-- ============================================================== -->

Retrieves a JSON list from the dictionary `kw` at the specified `path`. If the key does not exist, it returns `default_value` or creates a new list if `KW_CREATE` is set in `flag`.

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
json_t *kw_get_list(
    hgobj      gobj,
    json_t     *kw,
    const char *path,
    json_t     *default_value,
    kw_flag_t   flag
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
  - Handle to the calling object, used for logging errors.

* - `kw`
  - `json_t *`
  - JSON dictionary to search for the list.

* - `path`
  - `const char *`
  - Path to the list within `kw`, using the configured delimiter.

* - `default_value`
  - `json_t *`
  - Default value to return if the key does not exist. Owned by the caller.

* - `flag`
  - `kw_flag_t`
  - Flags controlling behavior, such as `KW_CREATE` to create the list if missing.
:::

---

**Return Value**

Returns a JSON list if found. If `KW_CREATE` is set and the key does not exist, a new list is created and returned. If the key is not found and `KW_REQUIRED` is set, an error is logged and `default_value` is returned.

**Notes**

If `KW_EXTRACT` is set in `flag`, the retrieved list is removed from `kw` and returned with an increased reference count. If the key exists but is not a list, an error is logged and `default_value` is returned.

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
