<!-- ============================================================== -->
(gobj_node_parents())=
# `gobj_node_parents()`
<!-- ============================================================== -->

`gobj_node_parents()` returns a list of parent references for a given node in a tree database, optionally filtered by a specific link.

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
json_t *gobj_node_parents(
    hgobj gobj,
    const char *topic_name,
    json_t *kw,
    const char *link,
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
  - The GObj instance managing the tree database.

* - `topic_name`
  - `const char *`
  - The name of the topic representing the node.

* - `kw`
  - `json_t *`
  - A JSON object containing the node's primary key fields. Owned by the caller.

* - `link`
  - `const char *`
  - The specific link to filter parent references. If NULL, all parent references are returned.

* - `jn_options`
  - `json_t *`
  - A JSON object specifying options for retrieving parent references. Owned by the caller.

* - `src`
  - `hgobj`
  - The source GObj requesting the operation.
:::

---

**Return Value**

A JSON array containing the parent references. The caller must decrement the reference count when done.

**Notes**

['If `gobj` is NULL or destroyed, an error is logged and NULL is returned.', 'If `gobj->gclass->gmt->mt_node_parents` is not defined, an error is logged and NULL is returned.', 'The returned JSON array contains references formatted according to the specified options.']

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
