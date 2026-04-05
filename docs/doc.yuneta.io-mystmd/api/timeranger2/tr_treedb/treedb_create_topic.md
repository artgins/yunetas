<!-- ============================================================== -->
(treedb_create_topic())=
# `treedb_create_topic()`
<!-- ============================================================== -->

`treedb_create_topic()` creates a new topic in the TreeDB with the specified schema and primary key constraints.

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
json_t *treedb_create_topic(
    json_t       *tranger,
    const char   *treedb_name,
    const char   *topic_name,
    int          topic_version,
    const char   *topic_tkey,
    json_t       *pkey2s,      // owned, string or dict of string | [strings]
    json_t       *jn_cols,     // owned
    uint32_t     snap_tag,
    BOOL         create_schema
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
  - Pointer to the tranger instance managing the TreeDB.

* - `treedb_name`
  - `const char *`
  - Name of the TreeDB where the topic will be created.

* - `topic_name`
  - `const char *`
  - Name of the topic to be created.

* - `topic_version`
  - `int`
  - Version number of the topic schema.

* - `topic_tkey`
  - `const char *`
  - Topic key used for indexing.

* - `pkey2s`
  - `json_t *`
  - Primary key(s) for the topic, either a string or a dictionary of strings.

* - `jn_cols`
  - `json_t *`
  - JSON object defining the schema of the topic, including field types and attributes.

* - `snap_tag`
  - `uint32_t`
  - Snapshot tag associated with the topic creation.

* - `create_schema`
  - `BOOL`
  - Flag indicating whether to create the schema if it does not exist.
:::

---

**Return Value**

Returns a JSON object representing the created topic. WARNING: The returned object is not owned by the caller.

**Notes**

The primary key (`pkey`) of all topics must be `id`.
This function does not load hook links.
The returned JSON object should not be modified or freed by the caller.

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
