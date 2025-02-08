<!-- ============================================================== -->
(tranger2_backup_topic())=
# `tranger2_backup_topic()`
<!-- ============================================================== -->

Creates a backup of a topic in the TimeRanger database. If `backup_path` is empty, the topic path is used. If `backup_name` is empty, the backup file is named `topic_name.bak`. If `overwrite_backup` is true and the backup exists, it is overwritten unless `tranger_backup_deleting_callback` returns true.

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
json_t *tranger2_backup_topic(
    json_t *tranger,
    const char *topic_name,
    const char *backup_path,
    const char *backup_name,
    BOOL overwrite_backup,
    tranger_backup_deleting_callback_t tranger_backup_deleting_callback
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
  - The TimeRanger database instance.

* - `topic_name`
  - `const char *`
  - The name of the topic to back up.

* - `backup_path`
  - `const char *`
  - The directory where the backup will be stored. If empty, the topic path is used.

* - `backup_name`
  - `const char *`
  - The name of the backup file. If empty, `topic_name.bak` is used.

* - `overwrite_backup`
  - `BOOL`
  - If true, an existing backup is overwritten unless `tranger_backup_deleting_callback` prevents it.

* - `tranger_backup_deleting_callback`
  - `tranger_backup_deleting_callback_t`
  - A callback function that determines whether an existing backup should be deleted before overwriting.
:::

---

**Return Value**

A JSON object representing the new topic backup.

**Notes**

If `overwrite_backup` is true and the backup exists, `tranger_backup_deleting_callback` is called. If it returns true, the existing backup is not removed.

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
