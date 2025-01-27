

<!-- ============================================================== -->
(kw_select())=
# `kw_select()`
<!-- ============================================================== -->

The `kw_select` function filters rows from a JSON object or array of dictionaries (`kw`) based on the criteria specified in `jn_filter`. It returns a new JSON array containing **duplicated** objects that match the filter. If `match_fn` is not provided, the default `kw_match_simple` function is used to evaluate matches.


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
PUBLIC json_t *kw_select(
    json_t      *kw,         // not owned
    const char  **keys,
    json_t      *jn_filter,  // owned
    BOOL (*match_fn)(
        json_t *kw,         // not owned
        json_t *jn_filter   // owned
    )
);
```

**Parameters**

::: {list-table}
:widths: 20 20 60
:header-rows: 1

* - **Parameter**
  - **Type**
  - **Description**

* - `kw`
  - [`json_t *`](json_t)
  - The input JSON object or array to filter (not owned).

* - `keys`
  - `const char **`
  - A list of keys to include in the returned objects. If `NULL`, all keys are included.

* - `jn_filter`
  - [`json_t *`](json_t)
  - JSON object specifying the filter criteria (owned).

* - `match_fn`
  - `BOOL (*)(json_t *, json_t *)`
  - Custom function to evaluate if a row matches the filter criteria. If `NULL`, the default `kw_match_simple` function is used.
:::

---

**Return Value**

- Returns a new JSON array (`json_t *`) containing duplicated objects that match the filter criteria.
- Returns an empty JSON array if no matches are found or if an error occurs.

**Notes**

- **Ownership:**
  - The input parameter `kw` is not owned and remains unchanged.
  - The `jn_filter` parameter is owned by the function and will be decremented internally.
  - The returned JSON array is owned by the caller and must be freed when no longer needed.
- **Error Handling:**
  - Logs an error if `kw` is not a JSON array or object.
- **Duplication:**
  - Objects in the result array are duplicated with only the specified keys using `kw_duplicate_with_only_keys`.

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
