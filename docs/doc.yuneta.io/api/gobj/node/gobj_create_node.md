<!-- ============================================================== -->
(gobj_create_node)=
# `gobj_create_node()`
<!-- ============================================================== -->

Creates a new node in the specified topic. The function [`gobj_create_node()`](#gobj_create_node) allows inserting a new record into a hierarchical data structure managed by the gobj system.

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
json_t *gobj_create_node(
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
  - The gobj instance managing the hierarchical data structure.

* - `topic_name`
  - `const char *`
  - The name of the topic where the node will be created.

* - `kw`
  - `json_t *`
  - A JSON object containing the attributes of the new node. This parameter is owned by the function.

* - `jn_options`
  - `json_t *`
  - A JSON object specifying options such as foreign key and hook configurations. This parameter is owned by the function.

* - `src`
  - `hgobj`
  - The source gobj that initiated the node creation request.
:::

---

**Return Value**

Returns a JSON object representing the newly created node. If the operation fails, returns NULL.

**Notes**

The function [`gobj_create_node()`](#gobj_create_node) requires that the gobj instance supports the `mt_create_node` method. If the method is not defined, an error is logged, and the function returns NULL.

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
