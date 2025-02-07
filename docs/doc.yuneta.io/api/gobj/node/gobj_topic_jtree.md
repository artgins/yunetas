<!-- ============================================================== -->
(gobj_topic_jtree)=
# `gobj_topic_jtree()`
<!-- ============================================================== -->

`gobj_topic_jtree()` returns a hierarchical tree representation of a topic's self-linked structure, optionally filtering and renaming hooks.

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
json_t *gobj_topic_jtree(
    hgobj gobj,
    const char *topic_name,
    const char *hook,
    const char *rename_hook,
    json_t *kw,
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

* - `gobj`
  - `hgobj`
  - The GObj instance managing the topic tree.

* - `topic_name`
  - `const char *`
  - The name of the topic whose tree structure is to be retrieved.

* - `hook`
  - `const char *`
  - The hook defining the hierarchical relationship between nodes.

* - `rename_hook`
  - `const char *`
  - An optional new name for the hook in the returned tree.

* - `kw`
  - `json_t *`
  - A JSON object containing the 'id' and pkey2s fields used to find the root node.

* - `jn_filter`
  - `json_t *`
  - A JSON object specifying filters to match records in the tree.

* - `jn_options`
  - `json_t *`
  - A JSON object containing options such as fkey and hook configurations.

* - `src`
  - `hgobj`
  - The source GObj requesting the topic tree.
:::

---

**Return Value**

Returns a JSON object representing the hierarchical tree of the specified topic. The caller must free the returned JSON object.

**Notes**

["If 'webix' is set in `jn_options`, the function returns the tree in Webix format (dict-list).", "The `__path__` field in all records follows the 'id`id`...' format.", 'If no root node is specified, the first node with no parent is used.', 'If `gobj` is NULL or destroyed, an error is logged and NULL is returned.', 'If `mt_topic_jtree` is not defined in the GClass, an error is logged and NULL is returned.']

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
