<!-- ============================================================== -->
(treedb_unlink_nodes())=
# `treedb_unlink_nodes()`
<!-- ============================================================== -->

The `treedb_unlink_nodes()` function removes the hierarchical relationship between a parent and a child node in the tree database, identified by the specified hook.

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
int treedb_unlink_nodes(
    json_t      *tranger,
    const char  *hook,
    json_t      *parent_node,    // NOT owned, pure node
    json_t      *child_node      // NOT owned, pure node
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
  - A pointer to the tranger database instance.

* - `hook`
  - `const char *`
  - The name of the hook defining the relationship to be removed.

* - `parent_node`
  - `json_t *`
  - A pointer to the parent node from which the child node will be unlinked. This node is not owned by the function.

* - `child_node`
  - `json_t *`
  - A pointer to the child node that will be unlinked from the parent node. This node is not owned by the function.
:::

---

**Return Value**

Returns `0` on success, or a negative error code if the unlinking operation fails.

**Notes**

The function does not take ownership of `parent_node` or `child_node`, meaning the caller is responsible for managing their memory. Ensure that the specified `hook` exists before calling [`treedb_unlink_nodes()`](#treedb_unlink_nodes()).

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
