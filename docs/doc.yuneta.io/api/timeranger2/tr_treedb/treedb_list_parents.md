<!-- ============================================================== -->
(treedb_list_parents())=
# `treedb_list_parents()`
<!-- ============================================================== -->

`treedb_list_parents()` returns a list of parent nodes linked to the given node through a specified foreign key (`fkey`). The function can return either full parent nodes or collapsed views based on the `collapsed_view` parameter.

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
json_t *treedb_list_parents(
    json_t      *tranger,
    const char  *fkey,
    json_t      *node,
    BOOL        collapsed_view,
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
  - A reference to the tranger database instance.

* - `fkey`
  - `const char *`
  - The foreign key field used to identify parent nodes.

* - `node`
  - `json_t *`
  - The node whose parents are to be retrieved. This parameter is not owned by the function.

* - `collapsed_view`
  - `BOOL`
  - If `TRUE`, returns a collapsed view of the parent nodes; otherwise, returns full parent nodes.

* - `jn_options`
  - `json_t *`
  - Options for filtering and formatting the returned parent nodes. This parameter is owned by the function.
:::

---

**Return Value**

A JSON array containing the list of parent nodes. The caller must decrement the reference count of the returned JSON object.

**Notes**

The function retrieves parent nodes based on the specified `fkey`. If `collapsed_view` is `TRUE`, the function returns a simplified representation of the parent nodes. The `jn_options` parameter allows customization of the output format.

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
