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

functions = [
]
functions.extend([
    {
        "name": "gobj_treedbs",
        "description": '''
Return a list of treedb names available in the specified GObj.
        ''',
        "prototype": '''
PUBLIC json_t *gobj_treedbs(
    hgobj gobj,
    json_t *kw,
    hgobj src
);
        ''',
        "parameters": '''
:::{list-table}
:widths: 10 5 40
:header-rows: 1
* - Key
  - Type
  - Description

* - `gobj`
  - [`hgobj`](hgobj)
  - The GObj to query for treedb names.

* - `kw`
  - [`json_t`](json_t)
  - Optional input parameters in JSON format (owned).

* - `src`
  - [`hgobj`](hgobj)
  - The source GObj making the request.
:::
        ''',
        "return_value": '''
Returns a JSON list of treedb names available in the specified GObj.  
If `gobj` is null or destroyed, returns `NULL`.
        '''
    },
    {
        "name": "gobj_treedb_topics",
        "description": '''
Retrieve the list of topics for a specific treedb.
        ''',
        "prototype": '''
PUBLIC json_t *gobj_treedb_topics(
    hgobj gobj,
    const char *treedb_name,
    json_t *options,
    hgobj src
);
        ''',
        "parameters": '''
:::{list-table}
:widths: 10 5 40
:header-rows: 1
* - Key
  - Type
  - Description

* - `gobj`
  - [`hgobj`](hgobj)
  - The GObj from which to query the treedb topics.

* - `treedb_name`
  - `const char *`
  - The name of the treedb.

* - `options`
  - [`json_t`](json_t)
  - Optional input parameters specifying how the list is formatted (owned).

* - `src`
  - [`hgobj`](hgobj)
  - The source GObj making the request.
:::
        ''',
        "return_value": '''
Returns a JSON list of topics in the specified treedb.  
If `gobj` is null, destroyed, or the method is undefined, returns `NULL`.
        '''
    },
    {
        "name": "gobj_topic_desc",
        "description": '''
Get the description of a specific topic in a treedb.
        ''',
        "prototype": '''
PUBLIC json_t *gobj_topic_desc(
    hgobj gobj,
    const char *topic_name
);
        ''',
        "parameters": '''
:::{list-table}
:widths: 10 5 40
:header-rows: 1
* - Key
  - Type
  - Description

* - `gobj`
  - [`hgobj`](hgobj)
  - The GObj from which to query the topic description.

* - `topic_name`
  - `const char *`
  - The name of the topic.
:::
        ''',
        "return_value": '''
Returns a JSON object containing the description of the topic.  
If `gobj` is null, destroyed, or the method is undefined, returns `NULL`.
        '''
    },
    {
        "name": "gobj_topic_links",
        "description": '''
Retrieve the links associated with a topic in a treedb.
        ''',
        "prototype": '''
PUBLIC json_t *gobj_topic_links(
    hgobj gobj,
    const char *treedb_name,
    const char *topic_name,
    json_t *kw,
    hgobj src
);
        ''',
        "parameters": '''
:::{list-table}
:widths: 10 5 40
:header-rows: 1
* - Key
  - Type
  - Description

* - `gobj`
  - [`hgobj`](hgobj)
  - The GObj from which to query the topic links.

* - `treedb_name`
  - `const char *`
  - The name of the treedb.

* - `topic_name`
  - `const char *`
  - The name of the topic.

* - `kw`
  - [`json_t`](json_t)
  - Optional parameters to refine the query (owned).

* - `src`
  - [`hgobj`](hgobj)
  - The source GObj making the request.
:::
        ''',
        "return_value": '''
Returns a JSON object containing the links of the topic.  
If `gobj` is null, destroyed, or the method is undefined, returns `NULL`.
        '''
    }
])

functions.extend([
    {
        "name": "gobj_topic_hooks",
        "description": '''
Retrieve the hooks associated with a specific topic in a treedb.
        ''',
        "prototype": '''
PUBLIC json_t *gobj_topic_hooks(
    hgobj gobj,
    const char *treedb_name,
    const char *topic_name,
    json_t *kw,
    hgobj src
);
        ''',
        "parameters": '''
:::{list-table}
:widths: 10 5 40
:header-rows: 1
* - Key
  - Type
  - Description

* - `gobj`
  - [`hgobj`](hgobj)
  - The GObj from which to query the topic hooks.

* - `treedb_name`
  - `const char *`
  - The name of the treedb.

* - `topic_name`
  - `const char *`
  - The name of the topic.

* - `kw`
  - [`json_t`](json_t)
  - Optional parameters for additional filtering (owned).

* - `src`
  - [`hgobj`](hgobj)
  - The source GObj making the request.
:::
        ''',
        "return_value": '''
Returns a JSON object containing the hooks of the topic.  
If `gobj` is null, destroyed, or the method is undefined, returns `NULL`.
        '''
    },
    {
        "name": "gobj_topic_size",
        "description": '''
Get the size of a topic based on a specific key in a treedb.
        ''',
        "prototype": '''
PUBLIC size_t gobj_topic_size(
    hgobj gobj,
    const char *topic_name,
    const char *key
);
        ''',
        "parameters": '''
:::{list-table}
:widths: 10 5 40
:header-rows: 1
* - Key
  - Type
  - Description

* - `gobj`
  - [`hgobj`](hgobj)
  - The GObj containing the topic.

* - `topic_name`
  - `const char *`
  - The name of the topic.

* - `key`
  - `const char *`
  - The key to evaluate for the topic size.
:::
        ''',
        "return_value": '''
Returns the size of the topic as `size_t`.  
If the `gobj` is null or the key is not found, the size returned is `0`.
        '''
    },
    {
        "name": "gobj_create_node",
        "description": '''
Create a new node in a specific topic of a treedb.
        ''',
        "prototype": '''
PUBLIC json_t *gobj_create_node(
    hgobj gobj,
    const char *topic_name,
    json_t *kw,
    json_t *jn_options,
    hgobj src
);
        ''',
        "parameters": '''
:::{list-table}
:widths: 10 5 40
:header-rows: 1
* - Key
  - Type
  - Description

* - `gobj`
  - [`hgobj`](hgobj)
  - The GObj responsible for creating the node.

* - `topic_name`
  - `const char *`
  - The name of the topic where the node will be created.

* - `kw`
  - [`json_t`](json_t)
  - JSON data representing the node attributes (owned).

* - `jn_options`
  - [`json_t`](json_t)
  - Additional options for node creation (owned).

* - `src`
  - [`hgobj`](hgobj)
  - The source GObj initiating the request.
:::
        ''',
        "return_value": '''
Returns a JSON object representing the created node.  
If the creation fails, returns `NULL`.
        '''
    },
    {
        "name": "gobj_update_node",
        "description": '''
Update an existing node in a specific topic of a treedb.
        ''',
        "prototype": '''
PUBLIC json_t *gobj_update_node(
    hgobj gobj,
    const char *topic_name,
    json_t *kw,
    json_t *jn_options,
    hgobj src
);
        ''',
        "parameters": '''
:::{list-table}
:widths: 10 5 40
:header-rows: 1
* - Key
  - Type
  - Description

* - `gobj`
  - [`hgobj`](hgobj)
  - The GObj responsible for updating the node.

* - `topic_name`
  - `const char *`
  - The name of the topic containing the node.

* - `kw`
  - [`json_t`](json_t)
  - JSON data representing the updated node attributes (owned).

* - `jn_options`
  - [`json_t`](json_t)
  - Additional options for the update operation (owned).

* - `src`
  - [`hgobj`](hgobj)
  - The source GObj initiating the update request.
:::
        ''',
        "return_value": '''
Returns a JSON object representing the updated node.  
If the update fails, returns `NULL`.
        '''
    },
    {
        "name": "gobj_delete_node",
        "description": '''
Delete a node from a specific topic in a treedb.
        ''',
        "prototype": '''
PUBLIC int gobj_delete_node(
    hgobj gobj,
    const char *topic_name,
    json_t *kw,
    json_t *jn_options,
    hgobj src
);
        ''',
        "parameters": '''
:::{list-table}
:widths: 10 5 40
:header-rows: 1
* - Key
  - Type
  - Description

* - `gobj`
  - [`hgobj`](hgobj)
  - The GObj responsible for deleting the node.

* - `topic_name`
  - `const char *`
  - The name of the topic containing the node to delete.

* - `kw`
  - [`json_t`](json_t)
  - JSON data identifying the node to delete (owned).

* - `jn_options`
  - [`json_t`](json_t)
  - Additional options for the delete operation (owned).

* - `src`
  - [`hgobj`](hgobj)
  - The source GObj initiating the delete request.
:::
        ''',
        "return_value": '''
- Returns `0` if the node was successfully deleted.  
- Returns `-1` if the deletion failed.
        '''
    }
])

functions.extend([
    {
        "name": "gobj_link_nodes",
        "description": '''
Create a link between two nodes in a treedb.
        ''',
        "prototype": '''
PUBLIC int gobj_link_nodes(
    hgobj gobj,
    const char *hook,
    const char *parent_topic_name,
    json_t *parent,
    const char *child_topic_name,
    json_t *child,
    hgobj src
);
        ''',
        "parameters": '''
:::{list-table}
:widths: 10 5 40
:header-rows: 1
* - Key
  - Type
  - Description

* - `gobj`
  - [`hgobj`](hgobj)
  - The GObj responsible for linking the nodes.

* - `hook`
  - `const char *`
  - The hook name that defines the relationship.

* - `parent_topic_name`
  - `const char *`
  - The topic name of the parent node.

* - `parent`
  - [`json_t`](json_t)
  - JSON data identifying the parent node (owned).

* - `child_topic_name`
  - `const char *`
  - The topic name of the child node.

* - `child`
  - [`json_t`](json_t)
  - JSON data identifying the child node (owned).

* - `src`
  - [`hgobj`](hgobj)
  - The source GObj initiating the request.
:::
        ''',
        "return_value": '''
- Returns `0` if the link was successfully created.  
- Returns `-1` if the operation failed.
        '''
    },
    {
        "name": "gobj_unlink_nodes",
        "description": '''
Remove a link between two nodes in a treedb.
        ''',
        "prototype": '''
PUBLIC int gobj_unlink_nodes(
    hgobj gobj,
    const char *hook,
    const char *parent_topic_name,
    json_t *parent,
    const char *child_topic_name,
    json_t *child,
    hgobj src
);
        ''',
        "parameters": '''
:::{list-table}
:widths: 10 5 40
:header-rows: 1
* - Key
  - Type
  - Description

* - `gobj`
  - [`hgobj`](hgobj)
  - The GObj responsible for unlinking the nodes.

* - `hook`
  - `const char *`
  - The hook name that defines the relationship.

* - `parent_topic_name`
  - `const char *`
  - The topic name of the parent node.

* - `parent`
  - [`json_t`](json_t)
  - JSON data identifying the parent node (owned).

* - `child_topic_name`
  - `const char *`
  - The topic name of the child node.

* - `child`
  - [`json_t`](json_t)
  - JSON data identifying the child node (owned).

* - `src`
  - [`hgobj`](hgobj)
  - The source GObj initiating the request.
:::
        ''',
        "return_value": '''
- Returns `0` if the link was successfully removed.  
- Returns `-1` if the operation failed.
        '''
    },
    {
        "name": "gobj_get_node",
        "description": '''
Retrieve a node from a specific topic in a treedb.
        ''',
        "prototype": '''
PUBLIC json_t *gobj_get_node(
    hgobj gobj,
    const char *topic_name,
    json_t *kw,
    json_t *options,
    hgobj src
);
        ''',
        "parameters": '''
:::{list-table}
:widths: 10 5 40
:header-rows: 1
* - Key
  - Type
  - Description

* - `gobj`
  - [`hgobj`](hgobj)
  - The GObj responsible for retrieving the node.

* - `topic_name`
  - `const char *`
  - The name of the topic containing the node.

* - `kw`
  - [`json_t`](json_t)
  - JSON data specifying the node to retrieve (owned).

* - `options`
  - [`json_t`](json_t)
  - Additional options for retrieval (owned).

* - `src`
  - [`hgobj`](hgobj)
  - The source GObj initiating the request.
:::
        ''',
        "return_value": '''
Returns a JSON object representing the node.  
If the node is not found, returns `NULL`.
        '''
    },
    {
        "name": "gobj_list_nodes",
        "description": '''
List all nodes in a specific topic of a treedb.
        ''',
        "prototype": '''
PUBLIC json_t *gobj_list_nodes(
    hgobj gobj,
    const char *topic_name,
    json_t *jn_filter,
    json_t *options,
    hgobj src
);
        ''',
        "parameters": '''
:::{list-table}
:widths: 10 5 40
:header-rows: 1
* - Key
  - Type
  - Description

* - `gobj`
  - [`hgobj`](hgobj)
  - The GObj responsible for listing the nodes.

* - `topic_name`
  - `const char *`
  - The name of the topic to list nodes from.

* - `jn_filter`
  - [`json_t`](json_t)
  - JSON filter to refine the results (owned).

* - `options`
  - [`json_t`](json_t)
  - Additional options for the list operation (owned).

* - `src`
  - [`hgobj`](hgobj)
  - The source GObj initiating the request.
:::
        ''',
        "return_value": '''
Returns a JSON array of nodes matching the specified criteria.  
If no nodes match, returns an empty JSON array.
        '''
    },
    {
        "name": "gobj_list_instances",
        "description": '''
Retrieve a list of instances for a specific topic in a treedb.
        ''',
        "prototype": '''
PUBLIC json_t *gobj_list_instances(
    hgobj gobj,
    const char *topic_name,
    const char *pkey2_field,
    json_t *jn_filter,
    json_t *jn_options,
    hgobj src
);
        ''',
        "parameters": '''
:::{list-table}
:widths: 10 5 40
:header-rows: 1
* - Key
  - Type
  - Description

* - `gobj`
  - [`hgobj`](hgobj)
  - The GObj responsible for retrieving the instances.

* - `topic_name`
  - `const char *`
  - The name of the topic containing the instances.

* - `pkey2_field`
  - `const char *`
  - The secondary key field for filtering instances.

* - `jn_filter`
  - [`json_t`](json_t)
  - JSON filter to refine the results (owned).

* - `jn_options`
  - [`json_t`](json_t)
  - Additional options for the operation (owned).

* - `src`
  - [`hgobj`](hgobj)
  - The source GObj initiating the request.
:::
        ''',
        "return_value": '''
Returns a JSON array of instances matching the specified criteria.  
If no instances match, returns an empty JSON array.
        '''
    }
])

functions.extend([
    {
        "name": "gobj_node_parents",
        "description": '''
Retrieve the parent nodes of a specific node in a topic.
        ''',
        "prototype": '''
PUBLIC json_t *gobj_node_parents(
    hgobj gobj,
    const char *topic_name,
    json_t *kw,
    const char *link,
    json_t *options,
    hgobj src
);
        ''',
        "parameters": '''
:::{list-table}
:widths: 10 5 40
:header-rows: 1
* - Key
  - Type
  - Description

* - `gobj`
  - `hgobj`
  - The GObj handle initiating the request.

* - `topic_name`
  - `const char *`
  - The name of the topic containing the node.

* - `kw`
  - `json_t *`
  - JSON data identifying the node (owned).

* - `link`
  - `const char *`
  - The link type connecting the parent and child nodes.

* - `options`
  - `json_t *`
  - Additional options for retrieval (owned).

* - `src`
  - `hgobj`
  - The source GObj making the request.
:::
        ''',
        "return_value": '''
Returns a JSON object containing the parent nodes.  
If no parents exist or the operation fails, returns `NULL`.
        '''
    },
    {
        "name": "gobj_node_childs",
        "description": '''
Retrieve the child nodes of a specific node in a topic.
        ''',
        "prototype": '''
PUBLIC json_t *gobj_node_childs(
    hgobj gobj,
    const char *topic_name,
    json_t *kw,
    const char *hook,
    json_t *filter,
    json_t *options,
    hgobj src
);
        ''',
        "parameters": '''
:::{list-table}
:widths: 10 5 40
:header-rows: 1
* - Key
  - Type
  - Description

* - `gobj`
  - `hgobj`
  - The GObj handle initiating the request.

* - `topic_name`
  - `const char *`
  - The name of the topic containing the node.

* - `kw`
  - `json_t *`
  - JSON data identifying the node (owned).

* - `hook`
  - `const char *`
  - The relationship type between the node and its children.

* - `filter`
  - `json_t *`
  - A filter to narrow down the child nodes (owned).

* - `options`
  - `json_t *`
  - Additional options for the retrieval (owned).

* - `src`
  - `hgobj`
  - The source GObj making the request.
:::
        ''',
        "return_value": '''
Returns a JSON object containing the child nodes.  
If no children exist or the operation fails, returns `NULL`.
        '''
    },
    {
        "name": "gobj_topic_jtree",
        "description": '''
Retrieve a JSON representation of a topic's hierarchical tree.
        ''',
        "prototype": '''
PUBLIC json_t *gobj_topic_jtree(
    hgobj gobj,
    const char *topic_name,
    const char *hook,
    const char *rename_hook,
    json_t *kw,
    json_t *filter,
    json_t *options,
    hgobj src
);
        ''',
        "parameters": '''
:::{list-table}
:widths: 10 5 40
:header-rows: 1
* - Key
  - Type
  - Description

* - `gobj`
  - `hgobj`
  - The GObj handle initiating the request.

* - `topic_name`
  - `const char *`
  - The name of the topic.

* - `hook`
  - `const char *`
  - The relationship type between nodes.

* - `rename_hook`
  - `const char *`
  - Optional parameter to rename the relationship type in the output.

* - `kw`
  - `json_t *`
  - JSON data for the base node (owned).

* - `filter`
  - `json_t *`
  - A filter to narrow down nodes in the tree (owned).

* - `options`
  - `json_t *`
  - Additional options for the tree representation (owned).

* - `src`
  - `hgobj`
  - The source GObj making the request.
:::
        ''',
        "return_value": '''
Returns a JSON object representing the topic's hierarchical tree.  
If the operation fails, returns `NULL`.
        '''
    },
    {
        "name": "gobj_node_tree",
        "description": '''
Retrieve a JSON representation of a node's subtree.
        ''',
        "prototype": '''
PUBLIC json_t *gobj_node_tree(
    hgobj gobj,
    const char *topic_name,
    json_t *kw,
    json_t *options,
    hgobj src
);
        ''',
        "parameters": '''
:::{list-table}
:widths: 10 5 40
:header-rows: 1
* - Key
  - Type
  - Description

* - `gobj`
  - `hgobj`
  - The GObj handle initiating the request.

* - `topic_name`
  - `const char *`
  - The name of the topic containing the node.

* - `kw`
  - `json_t *`
  - JSON data identifying the node (owned).

* - `options`
  - `json_t *`
  - Additional options for the subtree representation (owned).

* - `src`
  - `hgobj`
  - The source GObj making the request.
:::
        ''',
        "return_value": '''
Returns a JSON object representing the node's subtree.  
If the operation fails, returns `NULL`.
        '''
    },
    {
        "name": "gobj_shoot_snap",
        "description": '''
Create a snapshot of the current state of the GObj's TreeDB.
        ''',
        "prototype": '''
PUBLIC int gobj_shoot_snap(
    hgobj gobj,
    const char *tag,
    json_t *kw,
    hgobj src
);
        ''',
        "parameters": '''
:::{list-table}
:widths: 10 5 40
:header-rows: 1
* - Key
  - Type
  - Description

* - `gobj`
  - `hgobj`
  - The GObj handle initiating the snapshot.

* - `tag`
  - `const char *`
  - The tag to associate with the snapshot.

* - `kw`
  - `json_t *`
  - JSON data containing snapshot metadata (owned).

* - `src`
  - `hgobj`
  - The source GObj making the request.
:::
        ''',
        "return_value": '''
- Returns `0` if the snapshot was successfully created.  
- Returns `-1` if the operation failed.
        '''
    },
    {
        "name": "gobj_activate_snap",
        "description": '''
Activate a specific snapshot in the GObj's TreeDB.
        ''',
        "prototype": '''
PUBLIC int gobj_activate_snap(
    hgobj gobj,
    const char *tag,
    json_t *kw,
    hgobj src
);
        ''',
        "parameters": '''
:::{list-table}
:widths: 10 5 40
:header-rows: 1
* - Key
  - Type
  - Description

* - `gobj`
  - `hgobj`
  - The GObj handle initiating the activation.

* - `tag`
  - `const char *`
  - The tag of the snapshot to activate.

* - `kw`
  - `json_t *`
  - JSON data with activation parameters (owned).

* - `src`
  - `hgobj`
  - The source GObj making the request.
:::
        ''',
        "return_value": '''
- Returns `0` if the snapshot was successfully activated.  
- Returns `-1` if the operation failed.
        '''
    },
    {
        "name": "gobj_list_snaps",
        "description": '''
Retrieve a list of snapshots available in the GObj's TreeDB.
        ''',
        "prototype": '''
PUBLIC json_t *gobj_list_snaps(
    hgobj       gobj,
    json_t      *filter,
    hgobj       src
);
        ''',
        "parameters": '''
:::{list-table}
:widths: 10 5 40
:header-rows: 1
* - Key
  - Type
  - Description

* - `gobj`
  - `hgobj`
  - The GObj handle initiating the request.

* - `filter`
  - `json_t *`
  - JSON filter for narrowing down the list of snapshots (owned).

* - `src`
  - `hgobj`
  - The source GObj making the request.
:::
        ''',
        "return_value": '''
Returns a JSON array of available snapshots.  
If no snapshots are available or the operation fails, returns an empty array.
        '''
    }
])


# Loop through the list of names and create a file for each
for fn in functions:
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
