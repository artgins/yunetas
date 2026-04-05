<!-- ============================================================== -->
(kw_get_dict_value())=
# `kw_get_dict_value()`
<!-- ============================================================== -->

Retrieves a JSON value from the dictionary `kw` using the specified `path`. If the key does not exist, it returns `default_value` based on the provided `flag` options.

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
json_t *kw_get_dict_value(
    hgobj      gobj,
    json_t     *kw,
    const char *path,
    json_t     *default_value,  // owned
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
  - A handle to the calling object, used for logging and error reporting.

* - `kw`
  - `json_t *`
  - A JSON dictionary from which the value is retrieved.

* - `path`
  - `const char *`
  - The key path used to locate the value within `kw`.

* - `default_value`
  - `json_t *`
  - The default value to return if the key is not found. This value is owned by the caller.

* - `flag`
  - `kw_flag_t`
  - Flags that modify the behavior of the function, such as `KW_REQUIRED`, `KW_CREATE`, or `KW_EXTRACT`.
:::

---

**Return Value**

Returns the JSON value found at `path`. If the key does not exist, it returns `default_value` based on the `flag` settings. If `KW_EXTRACT` is set, the value is removed from `kw`.

**Notes**

If `KW_REQUIRED` is set and the key is not found, an error is logged. If `KW_CREATE` is set, `default_value` is inserted into `kw` if the key does not exist.

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
