<!-- ============================================================== -->
(treedb_topic_pkey2s_filter())=
# `treedb_topic_pkey2s_filter()`
<!-- ============================================================== -->

`treedb_topic_pkey2s_filter()` retrieves a filtered list of primary key secondary values (`pkey2s`) for a given topic in a TreeDB, based on the provided node and identifier.

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
json_t *treedb_topic_pkey2s_filter(
    json_t      *tranger,
    const char  *topic_name,
    json_t      *node,      // NOT owned
    const char  *id
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
  - A reference to the TimeRanger instance managing the TreeDB.

* - `topic_name`
  - `const char *`
  - The name of the topic from which to retrieve `pkey2s` values.

* - `node`
  - `json_t *`
  - A JSON object representing the node to filter against. This parameter is not owned by the function.

* - `id`
  - `const char *`
  - The primary key identifier used to filter the `pkey2s` values.
:::

---

**Return Value**

Returns a JSON array containing the filtered `pkey2s` values. The returned object is not owned by the caller and must not be modified or freed.

**Notes**

This function is useful for retrieving secondary key values associated with a primary key in a structured TreeDB topic. The filtering is based on the provided `node` and `id` parameters.

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
