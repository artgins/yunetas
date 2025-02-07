<!-- ============================================================== -->
(build_stats)=
# `build_stats()`
<!-- ============================================================== -->

`build_stats()` constructs a JSON object containing statistical data extracted from the attributes of a given [`hgobj`](#hgobj) instance, including attributes marked with `SFD_STATS` flags.

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
json_t *build_stats(
    hgobj      gobj,
    const char *stats,
    json_t     *kw,
    hgobj      src
);
```

**Parameters**

::: {list-table}
:widths: 20 20 60
:header-rows: 1

* - Key
  - Type
  - Description

* - `gobj`
  - `hgobj`
  - The [`hgobj`](#hgobj) instance from which statistics are gathered.

* - `stats`
  - `const char *`
  - A string specifying which statistics to include. If `"__reset__"`, resets the statistics.

* - `kw`
  - `json_t *`
  - A JSON object containing additional parameters. This object is decremented after use.

* - `src`
  - `hgobj`
  - The source [`hgobj`](#hgobj) instance, used for context in the statistics gathering process.
:::

---

**Return Value**

A JSON object containing the collected statistics, structured by the short names of the [`hgobj`](#hgobj) instances.

**Notes**

Internally, [`_build_stats`](#_build_stats) is used to extract statistics from the [`hgobj`](#hgobj) instance and its bottom-level objects.
The function iterates through the hierarchy of [`hgobj`](#hgobj) instances, aggregating statistics from each level.

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
