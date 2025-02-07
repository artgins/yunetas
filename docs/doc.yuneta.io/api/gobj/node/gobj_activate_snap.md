<!-- ============================================================== -->
(gobj_activate_snap)=
# `gobj_activate_snap()`
<!-- ============================================================== -->

Activates a previously saved snapshot identified by `tag` in the given [`hgobj`](#hgobj) instance. This operation typically involves stopping and restarting the associated [`hgobj`](#hgobj) to restore the saved state.

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
int gobj_activate_snap(
    hgobj gobj,
    const char *tag,
    json_t *kw,
    hgobj src
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
  - The [`hgobj`](#hgobj) instance where the snapshot will be activated.

* - `tag`
  - `const char *`
  - The identifier of the snapshot to activate.

* - `kw`
  - `json_t *`
  - Additional options for activation, passed as a JSON object. Owned by the function.

* - `src`
  - `hgobj`
  - The source [`hgobj`](#hgobj) initiating the activation request.
:::

---

**Return Value**

Returns 0 on success, or -1 if an error occurs (e.g., if `gobj` is NULL, destroyed, or if `mt_activate_snap` is not defined).

**Notes**

['If `gobj` is NULL or destroyed, an error is logged and the function returns -1.', "If `mt_activate_snap` is not defined in the [`hgobj`](#hgobj)'s gclass, an error is logged and the function returns -1.", "The function ensures that the snapshot activation is properly handled by the [`hgobj`](#hgobj)'s gclass."]

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

