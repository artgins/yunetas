<!-- ============================================================== -->
(yev_create_write_event())=
# `yev_create_write_event()`
<!-- ============================================================== -->

`yev_create_write_event()` creates a write event associated with a given file descriptor and buffer within the specified event loop.

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
yev_event_h yev_create_write_event(
    yev_loop_h      yev_loop,
    yev_callback_t  callback,
    hgobj           gobj,
    int            fd,
    gbuffer_t *    gbuf
);
```

**Parameters**

::: {list-table}
:widths: 20 20 60
:header-rows: 1

* - Key
  - Type
  - Description

* - `yev_loop`
  - `yev_loop_h`
  - The event loop in which the write event will be created.

* - `callback`
  - `yev_callback_t`
  - The callback function to be invoked when the event is triggered. If it returns -1, [`yev_loop_run()`](#yev_loop_run()) will break.

* - `gobj`
  - `hgobj`
  - The associated GObj instance for event handling.

* - `fd`
  - `int`
  - The file descriptor to be monitored for write readiness.

* - `gbuf`
  - `gbuffer_t *`
  - The buffer containing data to be written. If `NULL`, no buffer is associated.
:::

---

**Return Value**

Returns a handle to the newly created write event (`yev_event_h`). If creation fails, `NULL` is returned.

**Notes**

The write event monitors the specified file descriptor for write readiness. Use [`yev_set_gbuffer()`](#yev_set_gbuffer()) to modify the associated buffer.

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
