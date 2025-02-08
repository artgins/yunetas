<!-- ============================================================== -->
(gobj_node_childs())=
# `gobj_node_childs()`
<!-- ============================================================== -->

Returns a list of child nodes for a given topic in a hierarchical tree structure. The function retrieves child nodes based on the specified hook and applies optional filters and options.

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
json_t *gobj_node_childs(
    hgobj gobj_,
    const char *topic_name,
    json_t *kw,
    const char *hook,
    json_t *jn_filter,
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

* - `gobj_`
  - `hgobj`
  - The gobj instance representing the tree database.

* - `topic_name`
  - `const char *`
  - The name of the topic whose child nodes are to be retrieved.

* - `kw`
  - `json_t *`
  - A JSON object containing the 'id' and primary key fields used to locate the node. Owned by the caller.

* - `hook`
  - `const char *`
  - The hook name that defines the relationship between parent and child nodes. If NULL, all hooks are considered.

* - `jn_filter`
  - `json_t *`
  - A JSON object specifying filters to apply to the child nodes. Owned by the caller.

* - `jn_options`
  - `json_t *`
  - A JSON object containing options such as fkey, hook options, and recursion settings. Owned by the caller.

* - `src`
  - `hgobj`
  - The source gobj making the request.
:::

---

**Return Value**

Returns a JSON object containing the list of child nodes. The caller must decrement the reference count of the returned JSON object.

**Notes**

If `gobj_` is NULL or destroyed, an error is logged, and NULL is returned. If the method `mt_node_childs` is not defined in the gclass, an error is logged, and NULL is returned.

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
