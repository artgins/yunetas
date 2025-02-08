<!-- ============================================================== -->
(kwid_compare_lists())=
# `kwid_compare_lists()`
<!-- ============================================================== -->

Compare two JSON lists of records, allowing for unordered comparison. The function checks if both lists contain the same elements, considering optional metadata and private key exclusions.

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
BOOL kwid_compare_lists(
    hgobj gobj,
    json_t *list,          // NOT owned
    json_t *expected,      // NOT owned
    BOOL without_metadata,
    BOOL without_private,
    BOOL verbose
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
  - Pointer to the GObj context for logging and error handling.

* - `list`
  - `json_t *`
  - The first JSON list to compare. Not owned by the function.

* - `expected`
  - `json_t *`
  - The second JSON list to compare against. Not owned by the function.

* - `without_metadata`
  - `BOOL`
  - If TRUE, metadata keys (keys starting with '__') are ignored during comparison.

* - `without_private`
  - `BOOL`
  - If TRUE, private keys (keys starting with '_') are ignored during comparison.

* - `verbose`
  - `BOOL`
  - If TRUE, detailed error messages are logged when mismatches occur.
:::

---

**Return Value**

Returns TRUE if both lists contain the same elements, considering the specified exclusions. Returns FALSE if they differ.

**Notes**

This function performs a deep comparison of JSON lists, allowing for unordered elements. It internally calls [`kwid_compare_records()`](#kwid_compare_records) for record-level comparison.

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
