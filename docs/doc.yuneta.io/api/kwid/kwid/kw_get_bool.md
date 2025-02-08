<!-- ============================================================== -->
(kw_get_bool())=
# `kw_get_bool()`
<!-- ============================================================== -->

Retrieves a boolean value from a JSON dictionary at the specified `path`. If the key does not exist, the `default_value` is returned. Supports type conversion from integers, strings, and null values.

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
BOOL kw_get_bool(
    hgobj      gobj,
    json_t     *kw,
    const char *path,
    BOOL       default_value,
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
  - Pointer to the GObj instance for logging and error handling.

* - `kw`
  - `json_t *`
  - JSON dictionary to search for the boolean value.

* - `path`
  - `const char *`
  - Path to the boolean value within the JSON dictionary.

* - `default_value`
  - `BOOL`
  - Default boolean value to return if the key is not found.

* - `flag`
  - `kw_flag_t`
  - Flags controlling behavior, such as `KW_REQUIRED` for logging errors if the key is missing.
:::

---

**Return Value**

Returns the boolean value found at `path`. If the key does not exist, the `default_value` is returned. If `KW_EXTRACT` is set, the value is removed from `kw`.

**Notes**

Supports type conversion: integers and real numbers are treated as `TRUE` if nonzero, `FALSE` otherwise. Strings are compared case-insensitively against 'true' and 'false'.

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
