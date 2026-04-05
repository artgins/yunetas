<!-- ============================================================== -->
(tranger2_create_topic())=
# `tranger2_create_topic()`
<!-- ============================================================== -->

The `tranger2_create_topic()` function creates a new topic in the TimeRanger database if it does not already exist. If the topic exists, it returns the existing topic metadata. The function ensures that the topic is properly initialized with the specified primary key, time key, system flags, and additional metadata.

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
json_t *tranger2_create_topic(
    json_t              *tranger,
    const char          *topic_name,
    const char          *pkey,
    const char          *tkey,
    json_t              *jn_topic_ext,
    system_flag2_t      system_flag,
    json_t              *jn_cols,
    json_t              *jn_var
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
  - Pointer to the TimeRanger database instance. If the topic exists, only `tranger` and `topic_name` are required.

* - `topic_name`
  - `const char *`
  - Name of the topic to be created.

* - `pkey`
  - `const char *`
  - Primary key field of the topic. If not specified, the key type defaults to `sf_int_key`.

* - `tkey`
  - `const char *`
  - Time key field of the topic.

* - `jn_topic_ext`
  - `json_t *`
  - JSON object containing additional topic parameters. See [`topic_json_desc`](#topic_json_desc) for details. This parameter is owned by the function.

* - `system_flag`
  - `system_flag2_t`
  - System flags for the topic, defining its behavior and properties.

* - `jn_cols`
  - `json_t *`
  - JSON object defining the topic's columns. This parameter is owned by the function.

* - `jn_var`
  - `json_t *`
  - JSON object containing variable metadata for the topic. This parameter is owned by the function.
:::

---

**Return Value**

Returns a JSON object representing the topic metadata. The returned JSON object is not owned by the caller and should not be modified or freed.

**Notes**

This function is idempotent, meaning that if the topic already exists, it will return the existing topic metadata instead of creating a new one. If the primary key (`pkey`) is not specified, the function defaults to `sf_string_key` if `pkey` is defined, otherwise it defaults to `sf_int_key`.

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
