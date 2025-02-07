<!-- ============================================================== -->
(kw_get_int)=
# `kw_get_int()`
<!-- ============================================================== -->

Retrieves an integer value from a JSON object by following a specified path. If the path does not exist, a default value is returned. Supports optional creation of the key if it does not exist.

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
json_int_t kw_get_int(
    hgobj      gobj,
    json_t     *kw,
    const char *path,
    json_int_t default_value,
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
  - Pointer to the GObj context, used for logging errors.

* - `kw`
  - `json_t *`
  - JSON object to search within.

* - `path`
  - `const char *`
  - Path to the desired integer value within the JSON object.

* - `default_value`
  - `json_int_t`
  - Default integer value to return if the path does not exist.

* - `flag`
  - `kw_flag_t`
  - Flags controlling behavior, such as `KW_REQUIRED` for error logging or `KW_CREATE` for creating the key if missing.
:::

---

**Return Value**

Returns the integer value found at the specified path, or the `default_value` if the path does not exist or is not an integer.

**Notes**

If `KW_CREATE` is set and the path does not exist, a new key is created with the `default_value`. If `KW_REQUIRED` is set and the path is missing, an error is logged.

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

