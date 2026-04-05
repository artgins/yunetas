<!-- ============================================================== -->
(treedb_open_db())=
# `treedb_open_db()`
<!-- ============================================================== -->

`treedb_open_db()` initializes and opens a tree database within a `tranger` instance, using the specified schema and options.

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
json_t *treedb_open_db(
    json_t      *tranger,
    const char  *treedb_name,
    json_t      *jn_schema,  // owned
    const char  *options     // "persistent"
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
  - A reference to the `tranger` instance managing the database.

* - `treedb_name`
  - `const char *`
  - The name of the tree database to open.

* - `jn_schema`
  - `json_t *`
  - A JSON object defining the schema of the tree database. This parameter is owned by the function.

* - `options`
  - `const char *`
  - A string specifying options for opening the database, such as `"persistent"` to load the schema from a file.
:::

---

**Return Value**

A JSON dictionary representing the opened tree database inside `tranger`. The returned object must not be used directly by the caller.

**Notes**

Ensure that `tranger` is already initialized before calling [`treedb_open_db()`](#treedb_open_db()).
The function follows a hierarchical structure where nodes are linked via parent-child relationships.
If the `persistent` option is enabled, the schema is loaded from a file, and modifications require a version update.

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
