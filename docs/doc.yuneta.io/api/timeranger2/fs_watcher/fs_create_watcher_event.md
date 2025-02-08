<!-- ============================================================== -->
(fs_create_watcher_event())=
# `fs_create_watcher_event()`
<!-- ============================================================== -->

`fs_create_watcher_event()` initializes a new file system watcher event, monitoring the specified `path` for changes based on the given `fs_flag`. The event is associated with the provided `yev_loop` and invokes the specified `callback` when triggered.

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
fs_event_t *fs_create_watcher_event(
    yev_loop_h     yev_loop,
    const char     *path,
    fs_flag_t      fs_flag,
    fs_callback_t  callback,
    hgobj          gobj,
    void           *user_data,
    void           *user_data2
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
  - The event loop handle in which the watcher event will be registered.

* - `path`
  - `const char *`
  - The directory or file path to monitor for changes.

* - `fs_flag`
  - `fs_flag_t`
  - Flags specifying monitoring options, such as recursive watching or file modification tracking.

* - `callback`
  - `fs_callback_t`
  - The function to be called when a file system event occurs.

* - `gobj`
  - `hgobj`
  - A generic object handle associated with the watcher event.

* - `user_data`
  - `void *`
  - User-defined data passed to the callback function.

* - `user_data2`
  - `void *`
  - Additional user-defined data passed to the callback function.
:::

---

**Return Value**

Returns a pointer to a newly allocated [`fs_event_t`](#fs_event_t) structure representing the watcher event, or `NULL` on failure.

**Notes**

The created watcher event must be started using [`fs_start_watcher_event()`](#fs_start_watcher_event()) to begin monitoring. When no longer needed, it should be stopped using [`fs_stop_watcher_event()`](#fs_stop_watcher_event()), which will also free the associated resources.

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
