<!-- ============================================================== -->
(trmsg_foreach_instances_messages())=
# `trmsg_foreach_instances_messages()`
<!-- ============================================================== -->

Iterates over all instance messages in the given `list`, invoking the specified callback function for each message. The callback receives a cloned instance of each message.

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
int trmsg_foreach_instances_messages(
    json_t *list,
    int (*callback)(
        json_t *list,      // Not yours!
        const char *key,
        json_t *instances, // It's yours, Must be owned
        void *user_data1,
        void *user_data2
    ),
    void *user_data1,
    void *user_data2,
    json_t *jn_filter  // owned
);
```

**Parameters**

::: {list-table}
:widths: 20 20 60
:header-rows: 1

* - Key
  - Type
  - Description

* - `list`
  - `json_t *`
  - The list containing messages to iterate over.

* - `callback`
  - `int (*)(json_t *, const char *, json_t *, void *, void *)`
  - Function pointer to the callback that will be invoked for each message instance.

* - `user_data1`
  - `void *`
  - User-defined data passed to the callback function.

* - `user_data2`
  - `void *`
  - Additional user-defined data passed to the callback function.

* - `jn_filter`
  - `json_t *`
  - Filter criteria to select specific messages. The caller owns this parameter.
:::

---

**Return Value**

Returns 0 on success, a negative value if the iteration was interrupted by the callback, or an error code on failure.

**Notes**

The callback function should return a negative value to break the iteration. The `instances` parameter passed to the callback is owned by the caller and must be managed accordingly.

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
