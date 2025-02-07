<!-- ============================================================== -->
(gobj_link_nodes)=
# `gobj_link_nodes()`
<!-- ============================================================== -->

The `gobj_link_nodes()` function establishes a relationship between two nodes in a hierarchical data structure by linking a child node to a parent node using a specified hook.

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
int gobj_link_nodes(
    hgobj gobj,
    const char *hook,
    const char *parent_topic_name,
    json_t *parent_record,
    const char *child_topic_name,
    json_t *child_record,
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
  - The GObj instance managing the hierarchical data structure.

* - `hook`
  - `const char *`
  - The name of the hook defining the relationship between the parent and child nodes.

* - `parent_topic_name`
  - `const char *`
  - The topic name of the parent node.

* - `parent_record`
  - `json_t *`
  - A JSON object representing the parent node. This parameter is owned by the function.

* - `child_topic_name`
  - `const char *`
  - The topic name of the child node.

* - `child_record`
  - `json_t *`
  - A JSON object representing the child node. This parameter is owned by the function.

* - `src`
  - `hgobj`
  - The source GObj initiating the link operation.
:::

---

**Return Value**

Returns 0 on success, or -1 if an error occurs (e.g., if `gobj` is NULL, destroyed, or if the `mt_link_nodes` method is not defined).

**Notes**

This function relies on the `mt_link_nodes` method of the GObj's class to perform the actual linking operation. If `mt_link_nodes` is not defined, an error is logged and the function returns -1.

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
