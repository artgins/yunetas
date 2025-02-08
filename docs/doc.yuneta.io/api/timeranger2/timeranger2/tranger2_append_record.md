<!-- ============================================================== -->
(tranger2_append_record())=
# `tranger2_append_record()`
<!-- ============================================================== -->

Appends a new record to a topic in the TimeRanger database. The function assigns a timestamp if `__t__` is zero and returns the metadata of the newly created record.

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
int tranger2_append_record(
    json_t            *tranger,
    const char        *topic_name,
    uint64_t           __t__,
    uint16_t           user_flag,
    md2_record_ex_t   *md2_record_ex,
    json_t            *jn_record
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
  - Pointer to the TimeRanger database instance.

* - `topic_name`
  - `const char *`
  - Name of the topic where the record will be appended.

* - `__t__`
  - `uint64_t`
  - Timestamp of the record. If set to 0, the current time is used.

* - `user_flag`
  - `uint16_t`
  - User-defined flag associated with the record.

* - `md2_record_ex`
  - `md2_record_ex_t *`
  - Pointer to the metadata structure of the record. This field is required.

* - `jn_record`
  - `json_t *`
  - JSON object containing the record data. Ownership is transferred to the function.
:::

---

**Return Value**

Returns 0 on success, or a negative value on failure.

**Notes**

The function ensures that the record is appended to the specified topic in [`tranger2_startup()`](#tranger2_startup()). If the topic does not exist, it must be created using [`tranger2_create_topic()`](#tranger2_create_topic()) before calling this function.

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
