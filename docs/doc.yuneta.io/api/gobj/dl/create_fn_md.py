#!/usr/bin/python

from string import Template
import os

template = Template("""

<!-- ============================================================== -->
($_name_())=
# `$_name_()`
<!-- ============================================================== -->

$_description_

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
$_prototype_

```

**Parameters**

$_parameters_

---

**Return Value**

$_return_value_


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

""")

functions_documentation = [

]
functions_documentation.extend([
    {
        "name": "dl_init",
        "description": '''
Initialize a doubly-linked list.
        ''',
        "prototype": '''
PUBLIC void dl_init(
    dl_list_t   *list
);
        ''',
        "parameters": '''
:::{list-table}
:widths: 10 5 40
:header-rows: 1
* - Key
  - Type
  - Description

* - `list`
  - [`dl_list_t *`](dl_list_t)
  - The doubly-linked list to initialize.
:::
        ''',
        "return_value": '''
No return value. This function initializes the doubly-linked list.
        '''
    },
    {
        "name": "dl_first",
        "description": '''
Get the first element in the doubly-linked list.
        ''',
        "prototype": '''
PUBLIC dl_node_t *dl_first(
    dl_list_t   *list
);
        ''',
        "parameters": '''
:::{list-table}
:widths: 10 5 40
:header-rows: 1
* - Key
  - Type
  - Description

* - `list`
  - [`dl_list_t *`](dl_list_t)
  - The doubly-linked list to query.
:::
        ''',
        "return_value": '''
Returns a pointer to the first node in the list, or `NULL` if the list is empty.
        '''
    },
    {
        "name": "dl_last",
        "description": '''
Get the last element in the doubly-linked list.
        ''',
        "prototype": '''
PUBLIC dl_node_t *dl_last(
    dl_list_t   *list
);
        ''',
        "parameters": '''
:::{list-table}
:widths: 10 5 40
:header-rows: 1
* - Key
  - Type
  - Description

* - `list`
  - [`dl_list_t *`](dl_list_t)
  - The doubly-linked list to query.
:::
        ''',
        "return_value": '''
Returns a pointer to the last node in the list, or `NULL` if the list is empty.
        '''
    },
    {
        "name": "dl_next",
        "description": '''
Get the next element in the doubly-linked list relative to a given node.
        ''',
        "prototype": '''
PUBLIC dl_node_t *dl_next(
    dl_node_t   *node
);
        ''',
        "parameters": '''
:::{list-table}
:widths: 10 5 40
:header-rows: 1
* - Key
  - Type
  - Description

* - `node`
  - [`dl_node_t *`](dl_node_t)
  - The current node.
:::
        ''',
        "return_value": '''
Returns a pointer to the next node, or `NULL` if there is no next node.
        '''
    },
    {
        "name": "dl_prev",
        "description": '''
Get the previous element in the doubly-linked list relative to a given node.
        ''',
        "prototype": '''
PUBLIC dl_node_t *dl_prev(
    dl_node_t   *node
);
        ''',
        "parameters": '''
:::{list-table}
:widths: 10 5 40
:header-rows: 1
* - Key
  - Type
  - Description

* - `node`
  - [`dl_node_t *`](dl_node_t)
  - The current node.
:::
        ''',
        "return_value": '''
Returns a pointer to the previous node, or `NULL` if there is no previous node.
        '''
    },
    {
        "name": "dl_insert",
        "description": '''
Insert a node into the doubly-linked list before a specified node.
        ''',
        "prototype": '''
PUBLIC void dl_insert(
    dl_list_t   *list,
    dl_node_t   *node,
    dl_node_t   *new_node
);
        ''',
        "parameters": '''
:::{list-table}
:widths: 10 5 40
:header-rows: 1
* - Key
  - Type
  - Description

* - `list`
  - [`dl_list_t *`](dl_list_t)
  - The doubly-linked list to modify.

* - `node`
  - [`dl_node_t *`](dl_node_t)
  - The node before which the new node will be inserted.

* - `new_node`
  - [`dl_node_t *`](dl_node_t)
  - The new node to insert.
:::
        ''',
        "return_value": '''
No return value. This function modifies the doubly-linked list.
        '''
    },
    {
        "name": "dl_add",
        "description": '''
Add a node to the end of the doubly-linked list.
        ''',
        "prototype": '''
PUBLIC void dl_add(
    dl_list_t   *list,
    dl_node_t   *new_node
);
        ''',
        "parameters": '''
:::{list-table}
:widths: 10 5 40
:header-rows: 1
* - Key
  - Type
  - Description

* - `list`
  - [`dl_list_t *`](dl_list_t)
  - The doubly-linked list to modify.

* - `new_node`
  - [`dl_node_t *`](dl_node_t)
  - The new node to add to the list.
:::
        ''',
        "return_value": '''
No return value. This function modifies the doubly-linked list.
        '''
    },
    {
        "name": "dl_find",
        "description": '''
Find a node in the doubly-linked list that matches a given condition.
        ''',
        "prototype": '''
PUBLIC dl_node_t *dl_find(
    dl_list_t   *list,
    dl_node_t   *(*match_fn)(dl_node_t *, void *),
    void        *arg
);
        ''',
        "parameters": '''
:::{list-table}
:widths: 10 5 40
:header-rows: 1
* - Key
  - Type
  - Description

* - `list`
  - [`dl_list_t *`](dl_list_t)
  - The doubly-linked list to search.

* - `match_fn`
  - `dl_node_t *(*)(dl_node_t *, void *)`
  - A function pointer to the matching function.

* - `arg`
  - `void *`
  - Additional argument passed to the matching function.
:::
        ''',
        "return_value": '''
Returns a pointer to the first node that matches the condition, or `NULL` if no match is found.
        '''
    },
    {
        "name": "dl_delete",
        "description": '''
Delete a node from the doubly-linked list.
        ''',
        "prototype": '''
PUBLIC void dl_delete(
    dl_list_t   *list,
    dl_node_t   *node
);
        ''',
        "parameters": '''
:::{list-table}
:widths: 10 5 40
:header-rows: 1
* - Key
  - Type
  - Description

* - `list`
  - [`dl_list_t *`](dl_list_t)
  - The doubly-linked list to modify.

* - `node`
  - [`dl_node_t *`](dl_node_t)
  - The node to delete.
:::
        ''',
        "return_value": '''
No return value. This function modifies the doubly-linked list.
        '''
    },
    {
        "name": "dl_flush",
        "description": '''
Remove all nodes from the doubly-linked list.
        ''',
        "prototype": '''
PUBLIC void dl_flush(
    dl_list_t   *list
);
        ''',
        "parameters": '''
:::{list-table}
:widths: 10 5 40
:header-rows: 1
* - Key
  - Type
  - Description

* - `list`
  - [`dl_list_t *`](dl_list_t)
  - The doubly-linked list to flush.
:::
        ''',
        "return_value": '''
No return value. This function clears the doubly-linked list.
        '''
    },
    {
        "name": "dl_size",
        "description": '''
Get the number of nodes in the doubly-linked list.
        ''',
        "prototype": '''
PUBLIC size_t dl_size(
    dl_list_t   *list
);
        ''',
        "parameters": '''
:::{list-table}
:widths: 10 5 40
:header-rows: 1
* - Key
  - Type
  - Description

* - `list`
  - [`dl_list_t *`](dl_list_t)
  - The doubly-linked list to query.
:::
        ''',
        "return_value": '''
Returns the number of nodes in the doubly-linked list.
        '''
    }
])


# Loop through the list of names and create a file for each
for fn in functions_documentation:
    # Substitute the variable in the template

    formatted_text = template.substitute(
        _name_          = fn['name'],
        _description_   = fn['description'],
        _prototype_     = fn['prototype'],
        _parameters_    = fn['parameters'],
        _return_value_  = fn['return_value']
    )
    # Create a unique file name for each name
    file_name = f"{fn['name'].lower()}.md"

    # Check if the file already exists
    if os.path.exists(file_name):
        print(f"File {file_name} already exists. =============================> Skipping...")
        continue  # Skip to the next name

    # Write the formatted text to the file
    with open(file_name, "w") as file:
        file.write(formatted_text)

    print(f"File created: {file_name}")
