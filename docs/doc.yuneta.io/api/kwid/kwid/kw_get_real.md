<!-- ============================================================== -->
(kw_get_real)=
# `kw_get_real()`
<!-- ============================================================== -->

Retrieves the real (floating-point) value associated with the given `path` in the JSON dictionary `kw`. If the key does not exist, the `default_value` is returned. Supports automatic type conversion for integers, booleans, and strings when `KW_WILD_NUMBER` is set.

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
double kw_get_real(
    hgobj      gobj,
    json_t     *kw,
    const char *path,
    double     default_value,
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
  - Handle to the GObj instance, used for logging errors.

* - `kw`
  - `json_t *`
  - JSON dictionary to search for the specified `path`.

* - `path`
  - `const char *`
  - Path to the desired real value within the JSON dictionary.

* - `default_value`
  - `double`
  - Value to return if the key does not exist or is of an incompatible type.

* - `flag`
  - `kw_flag_t`
  - Flags controlling behavior, such as `KW_REQUIRED`, `KW_CREATE`, and `KW_WILD_NUMBER`.
:::

---

**Return Value**

Returns the real (floating-point) value found at `path`. If the key does not exist or is incompatible, `default_value` is returned.

**Notes**

If `KW_WILD_NUMBER` is set, the function attempts to convert integers, booleans, and numeric strings to a real value. If `KW_REQUIRED` is set and the key is missing, an error is logged.

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

