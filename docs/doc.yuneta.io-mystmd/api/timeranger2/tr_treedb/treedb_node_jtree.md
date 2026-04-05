<!-- ============================================================== -->
(treedb_node_jtree())=
# `treedb_node_jtree()`
<!-- ============================================================== -->

`treedb_node_jtree()` constructs a hierarchical tree representation of child nodes linked through a specified hook.

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
json_t *treedb_node_jtree(
    json_t      *tranger,
    const char  *hook,
    const char  *rename_hook,
    json_t      *node,
    json_t      *jn_filter,
    json_t      *jn_options
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
  - Hook name used to establish parent-child relationships.

* - `rename_hook`
  - `const char *`
  - Optional new name for the hook in the resulting tree.

* - `node`
  - `json_t *`
  - Pointer to the parent node from which the tree is built. Not owned.

* - `jn_filter`
  - `json_t *`
  - Filter criteria for selecting child nodes. Not owned.

* - `jn_options`
  - `json_t *`
  - Options for controlling the structure of the resulting tree, including fkey and hook options.
:::

---

**Return Value**

A JSON object representing the hierarchical tree of child nodes. The caller must decrement the reference count when done.

**Notes**

The function recursively traverses child nodes using the specified `hook`. The `rename_hook` parameter allows renaming the hook in the output tree.

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
