<!-- ============================================================== -->
(trmsg_open_list())=
# `trmsg_open_list()`
<!-- ============================================================== -->

`trmsg_open_list()` opens a list of messages from a specified topic in the TimeRanger database, applying optional filtering conditions and extra parameters.

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
json_t *trmsg_open_list(
    json_t       *tranger,
    const char   *topic_name,
    json_t       *match_cond, // owned
    json_t       *extra,      // owned
    const char   *rt_id,
    BOOL          rt_by_disk,
    const char   *creator
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
  - Name of the topic from which to open the list.

* - `match_cond`
  - `json_t *`
  - JSON object specifying filtering conditions for messages. Owned by the function.

* - `extra`
  - `json_t *`
  - Additional parameters for list configuration. Owned by the function.

* - `rt_id`
  - `const char *`
  - Real-time identifier for the list.

* - `rt_by_disk`
  - `BOOL`
  - If `true`, the list is opened from disk instead of memory.

* - `creator`
  - `const char *`
  - Identifier of the entity creating the list.
:::

---

**Return Value**

Returns a JSON object representing the opened list. The caller is responsible for managing its lifecycle.

**Notes**

`trmsg_open_list()` internally uses [`tranger2_open_list()`](#tranger2_open_list()) to perform the operation. If `rt_by_disk` is set to `true`, the list is loaded from disk, which may introduce delays in application startup.

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
