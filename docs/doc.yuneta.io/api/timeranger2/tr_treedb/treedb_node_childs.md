<!-- ============================================================== -->
(treedb_node_childs())=
# `treedb_node_childs()`
<!-- ============================================================== -->

`treedb_node_childs()` returns a list of child nodes linked to a given node through a specified hook, optionally applying filters and recursive traversal.

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
json_t *treedb_node_childs(
    json_t       *tranger,
    const char   *hook,
    json_t       *node,       // NOT owned, pure node
    json_t       *jn_filter,  // filter to childs tree
    json_t       *jn_options  // fkey,hook options, "recursive"
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
  - Pointer to the tranger database instance.

* - `hook`
  - `const char *`
  - The hook name used to retrieve child nodes.

* - `node`
  - `json_t *`
  - The parent node from which child nodes are retrieved. This parameter is not owned.

* - `jn_filter`
  - `json_t *`
  - Optional filter criteria to apply to the child nodes. This parameter is owned.

* - `jn_options`
  - `json_t *`
  - Options for controlling the retrieval, including fkey and hook options, and whether to perform recursive traversal.
:::

---

**Return Value**

Returns a JSON array containing the child nodes that match the specified criteria. The caller must decrement the reference count when done.

**Notes**

If the `recursive` option is enabled in `jn_options`, [`treedb_node_childs()`](#treedb_node_childs()) will traverse the hierarchy recursively.

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
