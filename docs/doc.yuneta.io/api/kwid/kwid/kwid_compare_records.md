<!-- ============================================================== -->
(kwid_compare_records)=
# `kwid_compare_records()`
<!-- ============================================================== -->

Compares two JSON records deeply, allowing for unordered elements. The function checks for structural and value equivalence, optionally ignoring metadata and private fields.

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
BOOL kwid_compare_records(
    hgobj  gobj,
    json_t *record,          // NOT owned
    json_t *expected,        // NOT owned
    BOOL    without_metadata,
    BOOL    without_private,
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
  - Handle to the GObj instance, used for logging errors.

* - `record`
  - `json_t *`
  - The JSON record to compare, not owned by the function.

* - `expected`
  - `json_t *`
  - The expected JSON record to compare against, not owned by the function.

* - `without_metadata`
  - `BOOL`
  - If TRUE, metadata fields (keys starting with '__') are ignored during comparison.

* - `without_private`
  - `BOOL`
  - If TRUE, private fields (keys starting with '_') are ignored during comparison.

* - `verbose`
  - `BOOL`
  - If TRUE, logs detailed error messages when mismatches occur.
:::

---

**Return Value**

Returns TRUE if the records are identical, considering the specified filtering options. Returns FALSE if differences are found.

**Notes**

This function is useful for validating JSON records in databases or structured data comparisons. It internally calls [`kwid_compare_lists()`](#kwid_compare_lists) for array comparisons.

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
