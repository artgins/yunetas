<!-- ============================================================== -->
(msg2db_list_messages())=
# `msg2db_list_messages()`
<!-- ============================================================== -->

`msg2db_list_messages()` retrieves a list of messages from the specified database and topic, filtered by the given criteria. It supports optional filtering and a custom matching function.

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
json_t *msg2db_list_messages(
    json_t  *tranger,
    const char  *msg2db_name,
    const char  *topic_name,
    json_t  *jn_ids,     // owned
    json_t  *jn_filter,  // owned
    BOOL (*match_fn) (
        json_t  *kw,         // not owned
        json_t  *jn_filter   // owned
    )
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

* - `msg2db_name`
  - `const char *`
  - Name of the message database to query.

* - `topic_name`
  - `const char *`
  - Name of the topic from which messages are retrieved.

* - `jn_ids`
  - `json_t *`
  - JSON array of message IDs to retrieve. Owned by the caller.

* - `jn_filter`
  - `json_t *`
  - JSON object containing filter criteria. Owned by the caller.

* - `match_fn`
  - `BOOL (*)(json_t *, json_t *)`
  - Optional function pointer for custom message filtering. The first parameter is the message (not owned), and the second is the filter criteria (owned).
:::

---

**Return Value**

A JSON array of messages matching the criteria. The caller must decrement the reference count when done.

**Notes**

The returned JSON array must be decremented using `json_decref()` to avoid memory leaks.
If `match_fn` is provided, it will be used to further filter messages beyond `jn_filter`.
This function is useful for retrieving messages based on specific IDs or filtering conditions.

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
