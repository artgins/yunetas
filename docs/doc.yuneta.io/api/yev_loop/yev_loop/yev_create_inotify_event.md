<!-- ============================================================== -->
(yev_create_inotify_event())=
# `yev_create_inotify_event()`
<!-- ============================================================== -->

`yev_create_inotify_event()` creates a new inotify event within the specified event loop, associating it with a callback function and a given object.

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
yev_event_h yev_create_inotify_event(
    yev_loop_h      yev_loop,
    yev_callback_t  callback,  // if return -1 the loop in yev_loop_run will break;
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
  - The event loop handle in which the inotify event will be created.

* - `callback`
  - `yev_callback_t`
  - The function to be called when the event is triggered. If it returns -1, [`yev_loop_run()`](#yev_loop_run()) will break.

* - `gobj`
  - `hgobj`
  - The associated object that will handle the event.

* - `fd`
  - `int`
  - The file descriptor associated with the inotify event.

* - `gbuf`
  - `gbuffer_t *`
  - A buffer for storing event-related data.
:::

---

**Return Value**

Returns a `yev_event_h` handle to the newly created inotify event, or `NULL` on failure.

**Notes**

The callback function provided will be invoked when the inotify event is triggered. Ensure that the file descriptor `fd` is valid and properly initialized before calling [`yev_create_inotify_event()`](#yev_create_inotify_event()).

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
