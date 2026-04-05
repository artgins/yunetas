<!-- ============================================================== -->
(msg2db_get_message())=
# `msg2db_get_message()`
<!-- ============================================================== -->

`msg2db_get_message()` retrieves a message from the specified database and topic using the given primary and secondary keys.

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
json_t *msg2db_get_message(
    json_t      *tranger,
    const char  *msg2db_name,
    const char  *topic_name,
    const char  *id,
    const char  *id2
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
  - Name of the message database.

* - `topic_name`
  - `const char *`
  - Name of the topic within the database.

* - `id`
  - `const char *`
  - Primary key of the message to retrieve.

* - `id2`
  - `const char *`
  - Secondary key of the message to retrieve.
:::

---

**Return Value**

A JSON object containing the requested message. The returned object is not owned by the caller and must not be modified or freed.

**Notes**

The function returns a reference to an internal JSON object, so the caller must not modify or free it.
If the message is not found, the function may return `NULL`.
The function relies on the structure and indexing of the database, which must be properly initialized using [`msg2db_open_db()`](#msg2db_open_db()).

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
