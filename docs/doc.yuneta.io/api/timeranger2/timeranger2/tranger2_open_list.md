<!-- ============================================================== -->
(tranger2_open_list())=
# `tranger2_open_list()`
<!-- ============================================================== -->

`tranger2_open_list()` opens a list of records in memory, optionally enabling real-time updates via memory or disk.

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
json_t *tranger2_open_list(
    json_t *tranger,
    const char *topic_name,
    json_t *match_cond, // owned
    json_t *extra,      // owned, will be added to the returned rt
    const char *rt_id,
    BOOL rt_by_disk,
    const char *creator
);
```

**Parameters**

::: {list-table}
:widths: 20 20 60
:header-rows: 1

* - Key
  - Type
  - Description

* - `tranger`
  - `json_t *`
  - The TimeRanger database instance.

* - `topic_name`
  - `const char *`
  - The name of the topic to open.

* - `match_cond`
  - `json_t *`
  - A JSON object specifying filtering conditions for records. Owned by the function.

* - `extra`
  - `json_t *`
  - Additional user data to be added to the returned real-time object. Owned by the function.

* - `rt_id`
  - `const char *`
  - The real-time list identifier.

* - `rt_by_disk`
  - `BOOL`
  - If `TRUE`, enables real-time updates via disk; otherwise, uses memory.

* - `creator`
  - `const char *`
  - The identifier of the entity creating the list.
:::

---

**Return Value**

Returns a JSON object representing the real-time list (`rt_mem` or `rt_disk`) or a static list if real-time is disabled. The caller does not own the returned object.

**Notes**

Loading all records may introduce delays in application startup. Use filtering conditions in `match_cond` to optimize performance.

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
