<!-- ============================================================== -->
(trmsg_foreach_messages())=
# `trmsg_foreach_messages()`
<!-- ============================================================== -->

`trmsg_foreach_messages()` iterates over all messages in the given `list`, invoking the provided callback function for each message. The callback receives either duplicated or cloned message instances based on the `duplicated` flag.

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
int trmsg_foreach_messages(
    json_t *list,
    BOOL duplicated,
    int (*callback)(
        json_t *list,
        const char *key,
        json_t *instances,
        void *user_data1,
        void *user_data2
    ),
    void *user_data1,
    void *user_data2,
    json_t *jn_filter
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

* - `duplicated`
  - `BOOL`
  - If `true`, the callback receives duplicated messages; otherwise, it receives cloned messages.

* - `callback`
  - `int (*)(json_t *, const char *, json_t *, void *, void *)`
  - A function pointer to the callback that processes each message. It should return a negative value to break the iteration.

* - `user_data1`
  - `void *`
  - User-defined data passed to the callback function.

* - `user_data2`
  - `void *`
  - Additional user-defined data passed to the callback function.

* - `jn_filter`
  - `json_t *`
  - A JSON object specifying filtering criteria for selecting messages. The caller owns this parameter.
:::

---

**Return Value**

Returns `0` on success, a negative value if the iteration was interrupted by the callback, or a positive error code on failure.

**Notes**

The callback function receives a reference to the `list`, the message `key`, and the `instances` JSON object. The `instances` parameter must be owned by the callback function. This function is similar to [`trmsg_foreach_active_messages()`](#trmsg_foreach_active_messages()) and [`trmsg_foreach_instances_messages()`](#trmsg_foreach_instances_messages()), but it allows selecting between duplicated and cloned messages.

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
