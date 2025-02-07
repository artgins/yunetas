<!-- ============================================================== -->
(gobj_topic_hooks)=
# `gobj_topic_hooks()`
<!-- ============================================================== -->

Retrieves the hooks of a topic in a TreeDB. The function `gobj_topic_hooks()` queries the specified topic within a TreeDB and returns its associated hooks.

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
json_t *gobj_topic_hooks(
    hgobj gobj,
    const char *treedb_name,
    const char *topic_name,
    json_t *kw,
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
  - The GObj instance that manages the TreeDB.

* - `treedb_name`
  - `const char *`
  - The name of the TreeDB containing the topic.

* - `topic_name`
  - `const char *`
  - The name of the topic whose hooks are to be retrieved.

* - `kw`
  - `json_t *`
  - A JSON object containing query parameters. Owned by the caller.

* - `src`
  - `hgobj`
  - The source GObj requesting the hooks.
:::

---

**Return Value**

Returns a JSON object containing the hooks of the specified topic. If the function fails, it returns NULL.

**Notes**

['If `gobj` is NULL or destroyed, an error is logged and NULL is returned.', 'If `gobj` does not implement `mt_topic_hooks`, an error is logged and NULL is returned.', 'The returned JSON object must be decremented by the caller when no longer needed.']

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
