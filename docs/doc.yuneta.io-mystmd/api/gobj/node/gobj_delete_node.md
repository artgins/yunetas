<!-- ============================================================== -->
(gobj_delete_node())=
# `gobj_delete_node()`
<!-- ============================================================== -->

Deletes a node from a tree database in the given [`hgobj`](#hgobj) instance. The node is identified by its topic name and key attributes.

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
int gobj_delete_node(
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
  - The [`hgobj`](#hgobj) instance managing the tree database.

* - `topic_name`
  - `const char *`
  - The name of the topic from which the node will be deleted.

* - `kw`
  - `json_t *`
  - A JSON object containing the key attributes used to identify the node. This parameter is owned and will be decremented.

* - `jn_options`
  - `json_t *`
  - A JSON object containing additional options for deletion, such as 'force'. This parameter is owned and will be decremented.

* - `src`
  - `hgobj`
  - The source [`hgobj`](#hgobj) instance initiating the deletion request.
:::

---

**Return Value**

Returns 0 on success, or -1 if an error occurs (e.g., if the [`hgobj`](#hgobj) is NULL, destroyed, or lacks the `mt_delete_node` method).

**Notes**

['If `gobj` is NULL or destroyed, an error is logged and the function returns -1.', "If `mt_delete_node` is not defined in the [`hgobj`](#hgobj)'s gclass, an error is logged and the function returns -1.", 'The `kw` and `jn_options` parameters are owned and will be decremented within the function.']

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
