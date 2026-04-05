<!-- ============================================================== -->
(kw_select())=
# `kw_select()`
<!-- ============================================================== -->

`kw_select()` returns a new JSON list containing **duplicated** objects from `kw` that match the given `jn_filter`. If `match_fn` is `NULL`, [`kw_match_simple()`](#kw_match_simple) is used as the default matching function.

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
json_t *kw_select(
    hgobj gobj,
    json_t *kw,         // NOT owned
    json_t *jn_filter,  // owned
    BOOL (*match_fn) (
        json_t *kw,         // NOT owned
        json_t *jn_filter   // owned
    )
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
  - A handle to the gobj (generic object) context.

* - `kw`
  - `json_t *`
  - A JSON object or array to be filtered. It is **not owned** by the function.

* - `jn_filter`
  - `json_t *`
  - A JSON object containing the filter criteria. It is **owned** by the function.

* - `match_fn`
  - `BOOL (*)(json_t *, json_t *)`
  - A function pointer used to match elements in `kw` against `jn_filter`. If `NULL`, [`kw_match_simple()`](#kw_match_simple) is used.
:::

---

**Return Value**

A new JSON array containing **duplicated** objects that match the filter criteria. The caller is responsible for freeing the returned JSON object.

**Notes**

If `kw` is an array, each element is checked against `jn_filter`, and matching elements are duplicated into the returned list.
If `kw` is an object, it is checked against `jn_filter`, and if it matches, it is duplicated into the returned list.
The returned JSON array contains **duplicated** objects, meaning they have new references and must be freed by the caller.

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
