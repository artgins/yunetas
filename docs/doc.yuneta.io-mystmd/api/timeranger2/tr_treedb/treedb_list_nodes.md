<!-- ============================================================== -->
(treedb_list_nodes())=
# `treedb_list_nodes()`
<!-- ============================================================== -->

`treedb_list_nodes()` retrieves a list of nodes from a specified topic in a tree database, optionally filtering the results based on a provided filter and a custom matching function.

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
json_t *treedb_list_nodes(
    json_t *tranger,
    const char *treedb_name,
    const char *topic_name,
    json_t *jn_filter,  // owned
    BOOL (*match_fn) (
        json_t *topic_desc, // NOT owned
        json_t *node,       // NOT owned
        json_t *jn_filter   // NOT owned
    )
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
  - A reference to the tranger database instance.

* - `treedb_name`
  - `const char *`
  - The name of the tree database to query.

* - `topic_name`
  - `const char *`
  - The name of the topic from which to retrieve nodes.

* - `jn_filter`
  - `json_t *`
  - A JSON object containing filter criteria for selecting nodes. This parameter is owned by the function.

* - `match_fn`
  - `BOOL (*)(json_t *, json_t *, json_t *)`
  - A function pointer to a custom matching function that determines whether a node matches the filter criteria.
:::

---

**Return Value**

Returns a JSON array containing the list of nodes that match the filter criteria. The caller must decrement the reference count of the returned JSON object when done.

**Notes**

If `match_fn` is provided, it is used to further refine the selection of nodes based on custom logic. The returned JSON object must be properly decremented to avoid memory leaks.

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
