<!-- ============================================================== -->
(tranger2_open_rt_mem())=
# `tranger2_open_rt_mem()`
<!-- ============================================================== -->

Opens a real-time memory stream for a given topic in `tranger`. This function enables real-time message processing for the specified `key` and applies filtering conditions from `match_cond`. The callback [`tranger2_load_record_callback_t`](#tranger2_load_record_callback_t) is invoked when new records are appended.

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
json_t *tranger2_open_rt_mem(
    json_t *tranger,
    const char *topic_name,
    const char *key,        // if empty receives all keys, else only this key
    json_t *match_cond,     // owned
    tranger2_load_record_callback_t load_record_callback,   // called on append new record on mem
    const char *list_id,    // rt list id, optional (internally will use the pointer of rt)
    const char *creator,
    json_t *extra           // owned, user data, this json will be added to the return iterator
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
  - Name of the topic for which real-time memory streaming is enabled.

* - `key`
  - `const char *`
  - Specific key to monitor. If empty, all keys are monitored.

* - `match_cond`
  - `json_t *`
  - JSON object containing filtering conditions. Owned by the function.

* - `load_record_callback`
  - `tranger2_load_record_callback_t`
  - Callback function invoked when a new record is appended in memory.

* - `list_id`
  - `const char *`
  - Optional identifier for the real-time list. If empty, the internal pointer is used.

* - `creator`
  - `const char *`
  - Identifier of the entity creating the real-time memory stream.

* - `extra`
  - `json_t *`
  - Additional user data stored in the returned iterator. Owned by the function.
:::

---

**Return Value**

Returns a JSON object representing the real-time memory stream. The caller does not own the returned object.

**Notes**

This function is valid when the Yuno instance is the master writing real-time messages from [`tranger2_append_record()`](#tranger2_append_record()).

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
