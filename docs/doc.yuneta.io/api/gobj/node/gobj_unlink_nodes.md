<!-- ============================================================== -->
(gobj_unlink_nodes())=
# `gobj_unlink_nodes()`
<!-- ============================================================== -->

The `gobj_unlink_nodes()` function removes the relationship between a parent and child node in a hierarchical data structure managed by a [`hgobj`](#hgobj).

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
int gobj_unlink_nodes(
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
  - The [`hgobj`](#hgobj) instance managing the nodes.

* - `hook`
  - `const char *`
  - The name of the relationship (hook) to be removed.

* - `parent_topic_name`
  - `const char *`
  - The topic name of the parent node.

* - `parent_record`
  - `json_t *`
  - A JSON object containing the parent node's data. This parameter is owned and will be decremented.

* - `child_topic_name`
  - `const char *`
  - The topic name of the child node.

* - `child_record`
  - `json_t *`
  - A JSON object containing the child node's data. This parameter is owned and will be decremented.

* - `src`
  - `hgobj`
  - The source [`hgobj`](#hgobj) initiating the unlink operation.
:::

---

**Return Value**

Returns `0` on success, or `-1` if an error occurs (e.g., if `gobj` is `NULL` or destroyed, or if `mt_unlink_nodes` is not defined).

**Notes**

['The function checks if `gobj` is valid before proceeding.', "If `mt_unlink_nodes` is not defined in the [`hgobj`](#hgobj)'s gclass, an error is logged and `-1` is returned.", 'Both `parent_record` and `child_record` are decremented after use.']

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
