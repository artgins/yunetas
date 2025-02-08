<!-- ============================================================== -->
(msg2db_open_db())=
# `msg2db_open_db()`
<!-- ============================================================== -->

`msg2db_open_db()` initializes and opens a message database using TimeRanger, loading its schema and configuration. The function supports persistence by loading the schema from a file if the 'persistent' option is enabled.

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
json_t *msg2db_open_db(
    json_t      *tranger,
    const char  *msg2db_name,
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
  - A reference to the TimeRanger instance managing the database.

* - `msg2db_name`
  - `const char *`
  - The name of the message database to open.

* - `jn_schema`
  - `json_t *`
  - A JSON object defining the schema of the database. Ownership is transferred to [`msg2db_open_db()`](#msg2db_open_db()).

* - `options`
  - `const char *`
  - Optional settings, such as "persistent" to load the schema from a file.
:::

---

**Return Value**

A JSON object representing the opened message database, or `NULL` on failure.

**Notes**

The function [`tranger2_startup()`](#tranger2_startup()) must be called before invoking [`msg2db_open_db()`](#msg2db_open_db()).
If the 'persistent' option is enabled, the schema is loaded from a file, which takes precedence over any provided schema.
To modify the schema after it has been saved, the schema version and topic version must be updated.

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
