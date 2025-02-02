(gobj_set_exit_code())=
# `gobj_set_exit_code()`

Sets the exit code that indicates the reason for the Yuno's termination. 

This value can be retrieved later using [](gobj_get_exit_code()).

``````{tab-set}

`````{tab-item} C

**Prototype**

````C
PUBLIC void gobj_set_exit_code(int exit_code);
````

**Parameters**

````{list-table}
:widths: 20 20 60
:header-rows: 1
* - Key
  - Type
  - Description
* - `exit_code`
  - `int`
  - The exit code to set:
    - **`0`**: Indicates successful termination.
    - **Non-zero values**: Indicate an error or specific termination condition.
````

**Return Value**

    -

`````

`````{tab-item} JS

**Prototype**

````JS
// Not applicable in JS
````

`````

`````{tab-item} Python

**Prototype**

````Python
# Not applicable in Python
````

`````
