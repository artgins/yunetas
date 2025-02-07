<!-- ============================================================== -->
(gobj_node_tree)=
# `gobj_node_tree()`
<!-- ============================================================== -->

Returns the full hierarchical tree of a node in a given topic. The tree is duplicated and can include metadata if specified in `jn_options`.

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
json_t *gobj_node_tree(
    hgobj gobj,
    const char *topic_name,
    json_t *kw,
    json_t *jn_options,
    hgobj src
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
  - The GObj instance that manages the tree database.

* - `topic_name`
  - `const char *`
  - The name of the topic whose node tree is to be retrieved.

* - `kw`
  - `json_t *`
  - A JSON object containing the 'id' and primary key fields used to locate the root node. Owned by the function.

* - `jn_options`
  - `json_t *`
  - A JSON object specifying options such as 'with_metadata' to include metadata in the response. Owned by the function.

* - `src`
  - `hgobj`
  - The source GObj requesting the node tree.
:::

---

**Return Value**

A JSON object representing the full hierarchical tree of the specified node. The caller must free the returned JSON object.

**Notes**

['If `gobj` is NULL or destroyed, an error is logged and NULL is returned.', 'If `gobj` does not implement `mt_node_tree`, an error is logged and NULL is returned.', 'The returned JSON object must be freed by the caller using `json_decref()`.']

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

