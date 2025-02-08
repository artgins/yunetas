<!-- ============================================================== -->
(msg2db_append_message())=
# `msg2db_append_message()`
<!-- ============================================================== -->

`msg2db_append_message()` appends a new message to the specified topic in the given message database.

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
json_t *msg2db_append_message(
    json_t      *tranger,
    const char  *msg2db_name,
    const char  *topic_name,
    json_t      *kw,        // owned
    const char  *options    // "permissive"
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
  - A reference to the TimeRanger database instance.

* - `msg2db_name`
  - `const char *`
  - The name of the message database.

* - `topic_name`
  - `const char *`
  - The name of the topic to which the message will be appended.

* - `kw`
  - `json_t *`
  - A JSON object containing the message data. This parameter is owned and will be managed internally.

* - `options`
  - `const char *`
  - Optional flags for message insertion. The value "permissive" allows flexible insertion rules.
:::

---

**Return Value**

A JSON object representing the appended message. The returned object is NOT owned by the caller.

**Notes**

The caller should not modify or free the returned JSON object.
Ensure that [`msg2db_open_db()`](#msg2db_open_db()) has been called before using [`msg2db_append_message()`](#msg2db_append_message()).

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
