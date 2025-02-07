<!-- ============================================================== -->
(stats_parser)=
# `stats_parser()`
<!-- ============================================================== -->

`stats_parser()` generates a JSON-formatted statistical report by extracting relevant attributes from the given [`hgobj`](#hgobj) instance and formatting them into a structured response.

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
json_t *stats_parser(
    hgobj       gobj,
    const char *stats,
    json_t    *kw,
    hgobj       src
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
  - The [`hgobj`](#hgobj) instance from which statistics are extracted.

* - `stats`
  - `const char *`
  - A string specifying which statistics to retrieve. If `NULL`, all available statistics are included.

* - `kw`
  - `json_t *`
  - A JSON object containing additional parameters. Ownership is transferred to [`stats_parser()`](#stats_parser).

* - `src`
  - `hgobj`
  - The source [`hgobj`](#hgobj) that initiated the request.
:::

---

**Return Value**

A JSON object containing the formatted statistical report. The caller assumes ownership of the returned object.

**Notes**

Internally, [`stats_parser()`](#stats_parser) calls [`build_stats()`](#build_stats) to generate the statistical data.
The function wraps the generated statistics in a standard command response format before returning.

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
