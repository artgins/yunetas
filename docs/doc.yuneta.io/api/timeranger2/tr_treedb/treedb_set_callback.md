<!-- ============================================================== -->
(treedb_set_callback())=
# `treedb_set_callback()`
<!-- ============================================================== -->

Sets a callback function for `treedb_name` in `tranger`. The callback is triggered on node operations such as creation, update, or deletion.

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
int treedb_set_callback(
    json_t            *tranger,
    const char        *treedb_name,
    treedb_callback_t  treedb_callback,
    void              *user_data
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
  - Pointer to the `tranger` instance managing the tree database.

* - `treedb_name`
  - `const char *`
  - Name of the tree database where the callback will be set.

* - `treedb_callback`
  - `treedb_callback_t`
  - Function pointer to the callback that will be invoked on node operations.

* - `user_data`
  - `void *`
  - User-defined data that will be passed to the callback function.
:::

---

**Return Value**

Returns `0` on success, or a negative error code on failure.

**Notes**

The callback function must follow the `treedb_callback_t` signature and will receive parameters such as `tranger`, `treedb_name`, `topic_name`, `operation`, and `node`. The callback is triggered on events like `EV_TREEDB_NODE_CREATED`, `EV_TREEDB_NODE_UPDATED`, and `EV_TREEDB_NODE_DELETED`.

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
