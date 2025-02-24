(gclass)=
# **GClass**

## What is a GClass?

A `GClass` (short for GObject Class) is a core construct in the Yuneta framework. It defines the behavior, attributes, and structure of GObjects (runtime instances of a `GClass`). The `GClass` encapsulates the functionality and state machine of a particular object type, serving as a blueprint for creating and managing GObjects.

A `GClass` in Yuneta is the foundation for defining the behavior and structure of objects in the framework. It encapsulates attributes, methods, and finite state machines, enabling the creation of reusable, modular, and extensible components.

Source code in:
- [gobj.c](https://github.com/artgins/yunetas/blob/main/kernel/c/gobj-c/src/gobj.c)
- [gobj.h](https://github.com/artgins/yunetas/blob/main/kernel/c/gobj-c/src/gobj.h)

## Components of a GClass

A `GClass` consists of several key components that define its operation:

1. [`GClass Name`](gclass_name_t)
    - Unique name to identify it.

2. [`Attributes`](tattr_desc)
    - Set of public attributes

3. [`Private Variables`](private_vars)
    - Set of private variables

4. [`Global Methods`](GMETHODS)
    - Set of global methods that supports

5. [`Local Methods`](LMETHOD)
    - Set of local methods that supports

6. [`Event Table`](event_type_t)
    - Set of public and private events.

7. [`State Table`](states_t)
    - Set of states with event/actions

8. [`Authorization Table`](authz_table)

    - Specifies access control rules.

9. [`Command Table`](command_table)

    - Describes executable commands for the GClass.

10. [`Trace Levels`](trace_level_t)

    - Defines trace levels for debugging and monitoring.

11. [`gclass_flag`](gclass_flag_t)

    - Modifier of behavior

## How a GClass Works

1. **Definition**:
    - A `GClass` is created using the [`gclass_create()`](gclass_create()) function, combining its attributes, methods, states, events, and other metadata.
2. **Instantiation**:
    - A GObject instance is created from a `GClass` using the GObject API.
3. **Interaction**:
    - The GObject API (e.g., [`gobj_start()`](gobj_start()), [`gobj_stop()`](gobj_stop())) dynamically invokes the global methods ([`gmt`](GMETHODS)) defined in the `GClass`.
4. **Extensibility**:
    - The modular design of `GClass` allows new behaviors and features to be added by defining additional methods, events, or states.

(gclass_name_t)=
## gclass_name_t

The name of the gclass.

Each `GClass` has a unique name to identify it.

In C must be defined with the macro [`GOBJ_DEFINE_GCLASS`](GOBJ_DEFINE_GCLASS) in `.c` file 
and [`GOBJ_DECLARE_GCLASS`](GOBJ_DECLARE_GCLASS) in `.h` file.


(tattr_desc)=
## Attributes

Attributes in a GClass define its properties or state. They are represented as structured data
 of type [`sdata_desc_t`](sdata_desc_t) in the `tattr_desc` table, where each entry describes an attribute's type, access flags, default value, and purpose. Attributes are the foundation of a GClass, providing a schema for configuration, runtime state, and operational behavior.

---

### Key Features of Attributes

#### 1. **Data Types**
Attributes support various data types, enabling them to store different kinds of information. Common data types include:
- **String (`DTP_STRING`)**: Textual data.
- **Boolean (`DTP_BOOLEAN`)**: Logical values (`true` or `false`).
- **Integer (`DTP_INTEGER`)**: Whole numbers.
- **Real (`DTP_REAL`)**: Floating-point numbers.
- **JSON (`DTP_JSON`)**: JSON objects for structured data.
- **Dictionary (`DTP_DICT`)**: Key-value pairs.
- **Pointer (`DTP_POINTER`)**: References for advanced use cases.

#### 2. **Flags**
Flags define the access permissions and characteristics of attributes:
- **Access Control:**
  - `SDF_RD`: Read-only attributes.
  - `SDF_WR`: Writable attributes.
- **Persistence:**
  - `SDF_PERSIST`: Attributes that are saved and loaded automatically.
  - `SDF_VOLATIL`: Temporary attributes that are not persisted.
- **Statistical Data:**
  - `SDF_STATS`: Attributes holding statistics (read-only).
  - `SDF_RSTATS`: Resettable statistics.
  - `SDF_PSTATS`: Persistent statistics.
- **Authorization:**
  - `SDF_AUTHZ_R`: Read access requires authorization.
  - `SDF_AUTHZ_W`: Write access requires authorization.

#### 3. **Default Values**
Default values ensure that attributes are initialized with predictable and meaningful data. They are applied when no explicit value is provided during configuration or runtime.

#### 4. **Descriptions**
Each attribute includes a description that explains its purpose and behavior. Descriptions serve as documentation for developers and users interacting with the GClass.

#### 5. **Hierarchical Definitions**
Attributes can reference other schemas, allowing for nested and hierarchical data structures. This capability enables complex configurations while maintaining clarity.

---

### Purpose of `tattr_desc`

The `tattr_desc` table serves multiple purposes in a GClass:
- **Configuration:** Defines configurable properties for the GClass.
- **State Management:** Represents the runtime state of an instance.
- **Validation:** Ensures that attributes conform to defined types and constraints.
- **Access Control:** Enforces security policies through flags and authorization mechanisms.

---

### High-Level Overview of Attributes

| **Component**       | **Description**                                                                 |
|----------------------|---------------------------------------------------------------------------------|
| **Type**            | Specifies the data type of the attribute (e.g., string, boolean, integer).      |
| **Name**            | The unique identifier for the attribute.                                       |
| **Flags**           | Define access permissions and additional properties (e.g., read-only, persist). |
| **Default Value**   | The initial value of the attribute if none is provided.                        |
| **Description**     | Explains the purpose and behavior of the attribute.                            |

---

### Examples of Common Attribute Use Cases

#### 1. **Runtime Statistics**
Attributes can store real-time statistical data, such as:
- Messages transmitted (`txMsgs`) and received (`rxMsgs`).
- CPU usage (`cpu`) and uptime (`uptime`).
- Disk size and free space.

These attributes often use flags like `SDF_RD` (read-only) and `SDF_RSTATS` (resettable statistics).

#### 2. **Configuration Settings**
Attributes can define configurable properties, such as:
- `timeout`: A configurable timeout value.
- `persistent_channels`: Whether channels are persistent.
- `deep_trace`: Enable or disable detailed tracing.

#### 3. **Environment Metadata**
Attributes can represent metadata about the GClass or its environment, including:
- `hostname`: The host machine's name.
- `node_uuid`: A unique identifier for the node.
- `realm_id`: The identifier for the realm where the GClass operates.

---

### Benefits of Attribute Design with `tattr_desc`

- **Modularity:** Attributes are defined in structured schemas, making them reusable and maintainable.
- **Flexibility:** Support for various data types and nested schemas allows for complex configurations.
- **Validation:** Ensures that data conforms to the schema, reducing errors.
- **Security:** Access control flags and authorization paths enforce robust security policies.
- **Documentation:** Descriptions provide built-in documentation for each attribute.


## Finite State Machine

In Yuneta, `gobj_state_t` and `gobj_event_t` define the behavior of `GObj` finite state machines (FSM). This event-driven architecture allows for structured and predictable execution.

(gobj_state_t)=
### gobj_state_t

GObj `states` represent different stages in an object's lifecycle, defining how it behaves and responds to events. Each state is identified by a unique name and is used to manage transitions between different operational conditions.

Unique pointer that exposes state names, defined as:

In C:
```C
typedef const char *gobj_state_t;
```

(gobj_event_t)=
### gobj_event_t

GObj `events` represent actions or signals that trigger transitions between states, facilitating communication between objects. Each event has a specific meaning within the system and determines how a GObj responds to changes or external inputs.

Unique pointer that exposes event names, defined as:

In C:
```C
typedef const char *gobj_event_t;
```

The FSM ensures clear behavior and modular event handling.

### 🛠️ How It Works

#### State Machine Structure
Each `GObj` has a state table mapping events to actions:

- **Event**: The event name (`gobj_event_t`).
- **Action**: The function executed when the event occurs.
- **Next State**: The state transitioned to after execution.

#### Event Handling
When an event occurs, the FSM:

1. Checks the current state.
2. Finds the event in the state table.
3. Executes the associated action.
4. Moves to the `next_state` if defined.

## Predefined States and Events in the System

Predefined States (`gobj_state_t`)

The following states are defined in the system using `GOBJ_DEFINE_STATE()`:

- `ST_DISCONNECTED`
- `ST_WAIT_CONNECTED`
- `ST_CONNECTED`
- `ST_WAIT_TXED`
- `ST_WAIT_DISCONNECTED`
- `ST_WAIT_STOPPED`
- `ST_WAIT_HANDSHAKE`
- `ST_WAIT_IMEI`
- `ST_WAIT_ID`
- `ST_STOPPED`
- `ST_SESSION`
- `ST_IDLE`
- `ST_WAIT_RESPONSE`
- `ST_OPENED`
- `ST_CLOSED`
- `ST_WAITING_HANDSHAKE`
- `ST_WAITING_FRAME_HEADER`
- `ST_WAITING_PAYLOAD_DATA`

### Predefined Events (`gobj_event_t`)

The following events are defined in the system using `GOBJ_DEFINE_EVENT()`:

- `EV_TIMEOUT`
- `EV_TIMEOUT_PERIODIC`
- `EV_STATE_CHANGED`
- `EV_SEND_MESSAGE`
- `EV_SEND_IEV`
- `EV_SEND_EMAIL`
- `EV_DROP`
- `EV_ON_OPEN`
- `EV_ON_CLOSE`
- `EV_ON_MESSAGE` *(with **`GBuffer`**)*
- `EV_ON_COMMAND`
- `EV_ON_IEV_MESSAGE` *(with **`IEvent`**, old **`EV_IEV_MESSAGE`**)*
- `EV_ON_ID`
- `EV_ON_ID_NAK`
- `EV_ON_HEADER`
- `EV_HTTP_MESSAGE`
- `EV_CONNECT`
- `EV_CONNECTED`
- `EV_DISCONNECT`
- `EV_DISCONNECTED`
- `EV_RX_DATA`
- `EV_TX_DATA`
- `EV_TX_READY`
- `EV_STOPPED`

These events represent system signals for timeouts, state transitions, messaging, connection handling, and data transmission. They are used to trigger actions within the system, allowing GObjs to react to external stimuli, manage their internal state, and facilitate communication between different components.

These predefined states and events form the core of Yuneta's GObj framework, enabling structured event-driven programming and state management. States define the behavior of an object at any given time, while events act as triggers that drive transitions between these states. Together, they create a dynamic and responsive system that efficiently handles various operational scenarios.

(EV_STATE_CHANGED)=
- `EV_STATE_CHANGED`

Is a system event. Publish when a FSM change his state.


(private_vars)=
## Private Attributes
- Private attributes, they are implemented accord the language    

(GMETHODS)=
## GMETHODS

The `GMETHODS` structure in Yuneta defines the global methods that a `GClass` can implement. These methods encapsulate key behaviors and operations associated with the lifecycle, state management, and functionality of GObjects.

Each method serves a specific purpose and is invoked through the GObject API, enabling dynamic and modular behavior across different `GClasses`.

The `GMETHODS` structure enables flexible, modular behavior in the Yuneta framework. Each method is optional, allowing `GClasses` to implement only the functionality they need.

### Methods

```{warning}
Some methods are not currently utilized in the gobj API. 
Refer to the [](mapping_gmethods) section for details.
```

(mt_create)=
- `mt_create`:
   - Purpose: Initializes the GObject. Called when a GObject is instantiated.
   - Example: Allocating memory or setting default attribute values.

(mt_create2)=
- `mt_create2`:
   - Purpose: Provides extended initialization capabilities with additional parameters.
   - Example: Custom initialization logic based on specific input.

(mt_destroy)=
- `mt_destroy`:
   - Purpose: Cleans up and destroys the GObject. Called when the GObject is deleted.
   - Example: Freeing resources or stopping child GObjects.

(mt_start)=
- `mt_start`:
   - Purpose: Starts the operation of the GObject.
   - Example: Transitioning to a running state or starting timers.

(mt_stop)=
- `mt_stop`:
   - Purpose: Stops the operation of the GObject.
   - Example: Halting operations, stopping timers, or freeing temporary resources.

(mt_play)=
- `mt_play`:
   - Purpose: Resumes the GObject's operation after being paused.
   - Example: Resuming services or processes.

(mt_pause)=
- `mt_pause`:
   - Purpose: Pauses the GObject's operation.
   - Example: Temporarily halting services without stopping them.

(mt_writing)=
- `mt_writing`:
   - Purpose: Handles attribute write operations dynamically.
   - Example: Validating or processing the value being written.

(mt_reading)=
- `mt_reading`:
   - Purpose: Handles attribute read operations dynamically.
   - Example: Adjusting or filtering the value before returning it.

(mt_subscription_added)=
- `mt_subscription_added`:
   - Purpose: Called when a subscription to an event is added.
   - Example: Managing event listeners or counting subscriptions.

(mt_subscription_deleted)=
- `mt_subscription_deleted`:
   - Purpose: Called when a subscription to an event is removed.
   - Example: Cleaning up resources or notifying other components.

(mt_child_added)=
- `mt_child_added`:
   - Purpose: Called after a child GObject is created and added.
   - Example: Managing hierarchical relationships.

(mt_child_removed)=
- `mt_child_removed`:
   - Purpose: Called before a child GObject is removed.
   - Example: Cleaning up references or handling dependencies.

(mt_stats)=
- `mt_stats`:
   - Purpose: Returns statistics related to the GObject in a JSON format.
   - Example: Providing performance or usage metrics.

(mt_command_parser)=
- `mt_command_parser`:
   - Purpose: Parses and executes user commands specific to the GClass.
   - Example: Implementing custom command handling logic.

(mt_inject_event)=
- `mt_inject_event`:
   - Purpose: Handles events manually, bypassing the built-in state machine.
   - Example: Custom event-processing logic.

(mt_create_resource)=
- `mt_create_resource`:
   - Purpose: Creates a new resource managed by the GObject.
   - Example: Initializing data structures or external dependencies.

(mt_list_resource)=
- `mt_list_resource`:
   - Purpose: Lists resources managed by the GObject.
   - Example: Returning an iterator or JSON representation.

(mt_save_resource)=
- `mt_save_resource`:
   - Purpose: Saves or updates a resource.
   - Example: Persisting data to storage.

(mt_delete_resource)=
- `mt_delete_resource`:
   - Purpose: Deletes a resource.
   - Example: Removing an entry from a database.

(mt_state_changed)=
- `mt_state_changed`:
   - Purpose: Handles transitions between states.
   - Example: Logging state changes or triggering side effects.

(mt_authenticate)=
- `mt_authenticate`:
   - Purpose: Authenticates users or services.
   - Example: Verifying credentials or tokens.

(mt_list_childs)=
- `mt_list_childs`:
   - Purpose: Lists the child GObjects of the current GObject.
   - Example: Returning hierarchical information.

(mt_stats_updated)=
- `mt_stats_updated`:
   - Purpose: Notifies that statistics have been updated.
   - Example: Refreshing metrics in real time.

(mt_disable)=
- `mt_disable`:
   - Purpose: Disables the GObject.
   - Example: Preventing further operations until re-enabled.

(mt_enable)=
- `mt_enable`:
   - Purpose: Enables the GObject.
   - Example: Allowing operations after being disabled.

(mt_trace_on)=
- `mt_trace_on`:
   - Purpose: Enables tracing for the GObject.
   - Example: Activating debug or log output.

(mt_trace_off)=
- `mt_trace_off`:
   - Purpose: Disables tracing for the GObject.
   - Example: Deactivating debug or log output.

(mt_gobj_created)=
- `mt_gobj_created`:
   - Purpose: Special method invoked when a GObject is created. Typically for the root object (`__yuno__`).
   - Example: Custom initialization for the Yuno.

(mt_publish_event)=
- `mt_publish_event`:
   - Purpose: Manages the publication of events.
   - Example: Filtering or modifying events before they are emitted.

(mt_authz_checker)=
- `mt_authz_checker`:
   - Purpose: Checks authorization for specific actions or events.
   - Example: Enforcing access control policies.

(mt_create_node)=
- `mt_create_node`
  - Purpose: Methods for managing nodes in TreeDB.
  - Example: Adding, updating, deleting, or linking nodes.

(mt_update_node)=
- `mt_update_node`
  - Purpose: Methods for managing nodes in TreeDB.
  - Example: Adding, updating, deleting, or linking nodes.

(mt_delete_node)=
- `mt_delete_node`
  - Purpose: Methods for managing nodes in TreeDB.
  - Example: Adding, updating, deleting, or linking nodes.

(mt_link_nodes)=
- `mt_link_nodes`
  - Purpose: Methods for managing nodes in TreeDB.
  - Example: Adding, updating, deleting, or linking nodes.

(mt_unlink_nodes)=
- `mt_unlink_nodes`:
   - Purpose: Methods for managing nodes in TreeDB.
   - Example: Adding, updating, deleting, or linking nodes.

(mt_topic_desc)=
- `mt_topic_desc`:
  - Purpose: Methods for inspecting TreeDB topics.
  - Example: Describing topics or retrieving metadata.

(mt_topic_links)=
- `mt_topic_links`:
  - Purpose: Methods for inspecting TreeDB topics.
  - Example: Describing topics or retrieving metadata.

(mt_topic_hooks)=
- `mt_topic_hooks`:
  - Purpose: Methods for inspecting TreeDB topics.
  - Example: Describing topics or retrieving metadata.

(mt_topic_size)=
- `mt_topic_size`:
   - Purpose: Methods for inspecting TreeDB topics.
   - Example: Describing topics or retrieving metadata.

(mt_shoot_snap)=
- `mt_shoot_snap`:
  - Purpose: Manage snapshots in the TreeDB.
  - Example: Creating or restoring data snapshots.

(mt_activate_snap)=
- `mt_activate_snap`:
  - Purpose: Manage snapshots in the TreeDB.
  - Example: Creating or restoring data snapshots.

(mt_list_snaps)=
- `mt_list_snaps`:
   - Purpose: Manage snapshots in the TreeDB.
   - Example: Creating or restoring data snapshots.

(mapping_gmethods)=
### Mapping

This section maps GObject API functions to the `GMETHODS` they invoke. It provides a detailed overview of how `GMETHODS` are utilized within the GObject lifecycle.

This mapping provides a detailed and structured view of how `GMETHODS` are utilized across various GObject API functions, covering attributes, events, TreeDB, lifecycle, and more. Each method plays a specific role in the modular design of the GClass.


#### Creation and Destruction
- **Creation Operations**:
    - `gobj_create2()`:
        - `mt_create2`: Initializes the GObject with additional parameters.
        - `mt_create`: Performs basic GObject initialization.
        - `mt_child_added`: Invoked when a child GObject is added.
        - `mt_gobj_created`: Special handling for `__yuno__` when a GObject is created.

- **Destroy Operations**:
    - `gobj_destroy()`:
        - `mt_child_removed`: Invoked when a child GObject is removed.
        - `mt_destroy`: Cleans up resources when a GObject is destroyed.

---

#### Attribute Management
- **Read Operations**:
    - `gobj_read_bool_attr()`:
    - `gobj_read_integer_attr()`:
    - `gobj_read_real_attr()`:
    - `gobj_read_str_attr()`:
    - `gobj_read_json_attr()`:
    - `gobj_read_pointer_attr()`:
        - `mt_reading`: Retrieves the requested attribute dynamically.

- **Write Operations**:
    - `gobj_write_str_attr()`:
    - `gobj_write_bool_attr()`:
    - `gobj_write_integer_attr()`:
    - `gobj_write_real_attr()`:
    - `gobj_write_json_attr()`:
    - `gobj_write_new_json_attr()`:
    - `gobj_write_pointer_attr()`:
        - `mt_writing`: Updates the specified attribute dynamically.

---

#### Lifecycle Management
- `gobj_start()`:
    - `mt_start`: Starts the GObject's operation.
- `gobj_stop()`:
    - `mt_stop`: Stops the GObject's operation.
- `gobj_play()`:
    - `mt_play`: Resumes the GObject after being paused.
- `gobj_pause()`:
    - `mt_pause`: Pauses the GObject's operation.
- `gobj_disable()`:
    - `mt_disable`: Disables the GObject.
- `gobj_enable()`:
    - `mt_enable`: Enables the GObject.

---

#### Event and State Management
- `gobj_send_event()`:
    - `mt_inject_event`: Processes an event directly.
- `gobj_change_state()`:
    - `mt_state_changed`: Handles state transitions.

---

#### Subscription Management
- `gobj_subscribe_event()`:
    - `mt_subscription_added`: Called when a subscription is added.
- `_delete_subscription()`:
    - `mt_subscription_deleted`: Called when a subscription is removed.

---

#### Resource Management
- `gobj_create_resource()`:
    - `mt_create_resource`: Called when a resource is created.
- `gobj_save_resource()`:
    - `mt_save_resource`: Called when a resource is saved.
- `gobj_delete_resource()`:
    - `mt_delete_resource`: Called when a resource is deleted.
- `gobj_list_resource()`:
    - `mt_list_resource`: Called when a resource is list.
- `gobj_get_resource()`:
    - `mt_get_resource`: Called when a resource is get.

---

#### Publishing Events
- `gobj_publish_event()`:
    - `mt_publish_event`: Manages the publication of events.
    - `mt_publication_pre_filter`: Applies pre-filters to events before publishing.
    - `mt_publication_filter`: Filters events before final publishing.

---

#### Commands and Statistics
- `gobj_command()`:
    - `mt_command_parser`: Executes a command related to the GClass.
- `gobj_stats()`:
    - `mt_stats`: Provides statistics for the GObject.

---

#### Authorization
- `gobj_user_has_authz()`:
    - `mt_authz_checker`: Verifies if the user has the necessary permissions.

---

#### TreeDB Management
- `gobj_treedbs()`:
    - `mt_treedbs`: Lists the TreeDBs associated with the GObject.
- `gobj_treedb_topics()`:
    - `mt_treedb_topics`: Lists the topics within a TreeDB.
- `gobj_topic_desc()`:
    - `mt_topic_desc`: Describes a topic in the TreeDB.
- `gobj_topic_links()`:
    - `mt_topic_links`: Lists the links for a topic.
- `gobj_topic_hooks()`:
    - `mt_topic_hooks`: Lists the hooks for a topic.
- `gobj_create_node()`:
    - `mt_create_node`: Creates a new node in the TreeDB.
- `gobj_update_node()`:
    - `mt_update_node`: Updates an existing node in the TreeDB.
- `gobj_delete_node()`:
    - `mt_delete_node`: Deletes a node from the TreeDB.
- `gobj_get_node()`:
    - `mt_get_node`: Retrieves a specific node.
- `gobj_list_nodes()`:
    - `mt_list_nodes`: Lists all nodes in a topic.
- `gobj_list_instances()`:
    - `mt_list_instances`: Lists instances of nodes.
- `gobj_node_parents()`:
    - `mt_node_parents`: Lists the parent nodes of a specific node.
- `gobj_node_childs()`:
    - `mt_node_childs`: Lists the child nodes of a specific node.
- `gobj_topic_jtree()`:
    - `mt_topic_jtree`: Retrieves the JSON representation of a topic's tree structure.
- `gobj_node_tree()`:
    - `mt_node_tree`: Retrieves the tree structure of nodes.

---

#### Snapshots
- `gobj_shoot_snap()`:
    - `mt_shoot_snap`: Creates a snapshot.
- `gobj_activate_snap()`:
    - `mt_activate_snap`: Activates a snapshot.
- `gobj_list_snaps()`:
    - `mt_list_snaps`: Lists all snapshots.

---

#### Tracing
- `gobj_set_gobj_trace()`:
    - `mt_trace_on`: Enables tracing for the GObject.
    - `mt_trace_off`: Disables tracing for the GObject.

---



(LMETHOD)=
## LMETHOD

The `LMETHOD` structure in the Yuneta framework defines the local methods 
that can be implemented by a `GClass`. 
These methods are specific to the internal operation of the `GClass` 
and are not intended to be invoked dynamically via 
the [`gobj_local_method()`](#gobj_local_method()) API, 
unlike the global methods (`GMETHODS`).


(event_type_t)=
## Event Table

- A list of public and private events that the `GClass` can handle or emit.
- Events are categorized with flags, such as `EVF_OUTPUT_EVENT`.

In C:
```C
typedef struct event_type_s {
    gobj_event_t event;
    event_flag_t event_flag;
} event_type_t;
```


(states_t)=
## States Table

Defines the behavior of the `GClass` in terms of states, events, and transitions:
    - **States**: Predefined states (e.g., `ST_CLOSED`, `ST_OPENED`).
    - **Events**: Triggers for state transitions (e.g., `EV_ON_OPEN`, `EV_ON_CLOSE`).
    - **Actions**: Functions executed during transitions between states.

In C:
```C
typedef struct states_s {
    gobj_state_t state_name;
    ev_action_t *state;
} states_t;
```


(authz_table)=
## Authorizations

The `authz_table` in a GClass defines the permissions required to access specific operations or resources. Each entry in the `authz_table` describes a distinct authorization, including its parameters, description, and validation schema. The `authz_table` is implemented using `sdata_desc_t` structures.

---

### Key Features of Authorizations

#### 1. **Authorization Levels**
Each authorization defines a level or type of access, such as:
- `create`: Permission to create resources.
- `update`: Permission to modify existing resources.
- `read`: Permission to view resources.
- `delete`: Permission to remove resources.

#### 2. **Validation Parameters**
Each authorization level can specify a schema that describes the parameters required for validation. For example:
- Parameters can include resource names, IDs, or paths.
- Parameters may reference specific authorization paths (e.g., `"record`id"`).

#### 3. **Authorization Paths**
Authorization paths define the logic for validating access. Paths may:
- Point to specific fields or metadata within a resource (e.g., `"__md_treedb__`treedb_name"`).
- Be empty (`""`) for unrestricted or default access.

#### 4. **Descriptions**
Each authorization includes a description that clearly defines its purpose. These descriptions serve as documentation and help developers and administrators understand the access requirements.

---

### Purpose of `authz_table`

The `authz_table` enforces access control within a GClass. By defining and validating authorizations, the GClass ensures that operations are performed only by users or systems with the necessary permissions.

---

### Validation of Authorizations

The `authz_table` works with APIs like `gobj_user_has_authz` to validate permissions dynamically. These APIs:
1. Check if the current user or system has the required authorization.
2. Validate the input parameters against the schema defined for the authorization.
3. Respond with an appropriate error or success message.

#### High-Level Process
1. **Permission Check:** Specify the desired permission (e.g., `"create"`).
2. **Validation:** The `gobj_user_has_authz` function validates the authorization based on:
  - The current user's roles and permissions.
  - The input parameters provided to the function.
  - The schema and paths defined in the `authz_table`.
3. **Action Execution:** If the validation passes, the requested action is performed. Otherwise, an error is returned.

---

### Benefits of `authz_table`

- **Granular Access Control:** Permissions can be tailored for specific actions and resources.
- **Validation and Consistency:** Using schemas ensures that authorization checks are structured and reliable.
- **Reusability:** Authorization definitions can be reused across different operations or resources within a GClass.
- **Security:** Centralized definitions minimize the risk of unauthorized access.

---

### Example Use Cases

#### Authorization Levels
- **Create:** Permission to add new nodes or resources.
- **Update:** Permission to modify existing data.
- **Read:** Permission to view specific data.
- **Delete:** Permission to remove resources.

#### Parameterized Authorizations
- Authorizations can specify required parameters, such as resource names, IDs, or metadata paths, for validation.
- For example, a `read` permission might require a `treedb_name` and `id` to validate access to a specific node.


(command_table)=
## Command Table

The `command_table` in a GClass defines the available commands and their corresponding behaviors. Each command is represented as an entry in the table, described with parameters, aliases, execution logic, and a human-readable description. This structured approach ensures consistency, flexibility, and clarity in how commands are defined and executed.

---

### Key Features of Commands

#### 1. **Command Names**
Each command is identified by a unique name, which is used to invoke the command programmatically. For example:
- `help`: Provides help or documentation for available commands.
- `view-channels`: Displays the current status of channels.
- `reset-stats-channel`: Resets statistics for a specific channel.

#### 2. **Aliases**
Commands can have alternative names (aliases) to provide flexibility and improve usability. For instance:
- The `help` command can have aliases like `h` or `?`.

#### 3. **Parameters**
Commands accept structured input, described using schemas. These schemas are defined in `sdata_desc_t` tables, specifying:
- **Parameter Name:** The name of the input parameter.
- **Type:** The type of the parameter (e.g., string, boolean, integer).
- **Flags:** Attributes of the parameter, such as required or optional.
- **Default Value:** The default value for the parameter if none is provided.
- **Description:** A description of the parameter’s purpose.

#### 4. **Handlers**
Each command is linked to a function that implements its logic. These handler functions:
- Receive the command name and input parameters.
- Perform the intended operation or behavior.
- Return results or error messages.

#### 5. **Descriptions**
Commands include a detailed description to document their purpose and functionality. This description is useful for both users and developers.

---

### Purpose of `command_table`

The `command_table` serves as a central registry for all commands available in a GClass. It provides:
- **Consistency:** All commands are defined in a structured and uniform manner.
- **Validation:** Ensures that commands receive the expected input, conforming to their schemas.
- **Documentation:** Built-in descriptions make commands self-explanatory.
- **Flexibility:** Commands can be extended, updated, or aliased without altering the overall structure.

---

### High-Level Overview of Command Definitions

| **Component**       | **Description**                                                                 |
|----------------------|---------------------------------------------------------------------------------|
| **Command Name**     | The unique identifier for the command.                                         |
| **Aliases**          | Alternative names for the command, improving usability.                       |
| **Parameters**       | Input schema defining the required and optional parameters for the command.    |
| **Handler Function** | A function that implements the command's logic.                               |
| **Description**      | Documentation describing the command's purpose and behavior.                  |

---

### Benefits of `command_table`

- **Extensibility:** Commands can be added, removed, or modified without affecting other components.
- **Reusability:** Shared schemas allow commands to reuse parameter definitions across multiple commands.
- **Validation:** Input parameters are rigorously validated using predefined schemas.
- **Self-Documentation:** Command descriptions and aliases make the system intuitive for users and developers.
- **Integration:** Commands are seamlessly integrated with authorization and other components of the GClass.


(trace_level_t)=
## Trace Levels

Trace levels allow precise control over debugging and diagnostic logging in a GClass. By defining a table of trace levels (`s_user_trace_level`), GClasses can manage which aspects of their behavior are logged, ensuring detailed, yet efficient, monitoring and debugging.

In C:
```C
typedef struct {
    const char *name;
    const char *description;
} trace_level_t;
```

---

### Key Concepts

#### Trace Level Table (`s_user_trace_level`)
The trace level table defines all available trace levels for a GClass. Each entry in the table includes:
- **Name:** A unique identifier for the trace level.
- **Description:** A detailed explanation of what the trace level represents.

#### Managing Trace Levels with GObj API
Trace levels are dynamically managed using the GObj API. These functions allow for:
- Retrieving lists of active trace levels.
- Setting or clearing specific trace levels at the global, GClass, or GObj instance levels.
- Checking if specific trace levels are active.

---

### Core API Functions

#### 1. **Listing Trace Levels**
Retrieve the list of all defined trace levels for a GClass:
- `gobj_trace_level_list(hgclass gclass)`: Returns a JSON list of all trace levels defined in the GClass.

#### 2. **Retrieving Active Trace Levels**
Retrieve the currently active trace levels at various scopes:
- **Global Levels:**
  - `gobj_get_global_trace_level()`: Get active global trace levels.
- **GClass Levels:**
  - `gobj_get_gclass_trace_level(hgclass gclass)`: Get active trace levels for a GClass.
  - `gobj_get_gclass_trace_no_level(hgclass gclass)`: Get trace levels explicitly disabled for a GClass.
- **GObj Instance Levels:**
  - `gobj_get_gobj_trace_level(hgobj gobj)`: Get active trace levels for a GObj instance.
  - `gobj_get_gobj_trace_no_level(hgobj gobj)`: Get trace levels explicitly disabled for a GObj instance.

#### 3. **Tree and Hierarchy Retrieval**
Retrieve trace levels across hierarchical scopes:
- `gobj_get_gclass_trace_level_list(hgclass gclass)`: Get active trace levels for a GClass and its hierarchy.
- `gobj_get_gobj_trace_level_tree(hgobj gobj)`: Get active trace levels for a GObj and its tree of children.

#### 4. **Querying and Checking Trace Levels**
Query specific trace levels for a GObj instance:
- `gobj_trace_level(hgobj gobj)`: Retrieve the active trace level bitmask for a GObj.
- `is_level_tracing(hgobj gobj, uint32_t level)`: Check if a specific trace level is active.
- `is_level_not_tracing(hgobj gobj, uint32_t level)`: Check if a specific trace level is explicitly disabled.

#### 5. **Setting Trace Levels**
Enable or disable trace levels dynamically at different scopes:
- **Global Scope:**
  - `gobj_set_global_trace(const char *level, BOOL set)`: Enable or disable specific global trace levels.
  - `gobj_set_global_no_trace(const char *level, BOOL set)`: Enable or disable "no trace" levels globally.
- **GClass Scope:**
  - `gobj_set_gclass_trace(hgclass gclass, const char *level, BOOL set)`: Enable or disable trace levels for a GClass.
- **GObj Instance Scope:**
  - `gobj_set_gobj_trace(hgobj gobj, const char *level, BOOL set, json_t *kw)`: Enable or disable trace levels for a specific GObj instance.

#### 6. **Deep Tracing**
Deep tracing enables comprehensive logging across all levels:
- `gobj_set_deep_tracing(int level)`: Enable deep tracing with different levels of detail.
- `gobj_get_deep_tracing()`: Retrieve the current deep tracing level.

---

### Practical Use Cases

#### 1. **Dynamic Logging**
During runtime, trace levels can be toggled on or off to focus on specific areas, such as:
- Connections (`connections`).
- Traffic (`traffic`).
- Security (`tls`).

This dynamic control reduces log clutter and enhances debugging efficiency.

#### 2. **Debugging Hierarchies**
Trace levels can propagate across GClass and GObj hierarchies. For instance:
- Enabling a trace level for a parent GClass or GObj can cascade to its children.
- Hierarchical queries can retrieve active trace levels across an entire tree of objects.

#### 3. **Selective Analysis**
Trace levels allow selective analysis of specific features or operations, improving troubleshooting accuracy. For example:
- Checking only traffic logs when debugging data flow issues.
- Enabling TLS logs during secure connection debugging.

---

### Benefits of Trace Levels

- **Granular Control:** Enable or disable logging for specific features or behaviors.
- **Dynamic Configuration:** Adjust trace levels at runtime without restarting the application.
- **Hierarchical Flexibility:** Manage trace levels across global, GClass, and GObj scopes.
- **Efficiency:** Minimize performance impact by logging only relevant details.
- **Clarity:** Descriptive trace level names and documentation improve usability.



(gclass_flag_t)=
## gclass_flag_t

The `gclass_flag_t` enumeration defines flags that can be applied to a `GClass` to modify its behavior. These flags are used to control specific operational aspects of the `GClass`.

Values of `gclass_flag_t` Flags

- `gcflag_manual_start`: (`0x0001`)
    - **Description**: Prevents the automatic start of the GObject tree. When this flag is set, `gobj_start_tree()` will not start the GObject automatically.
    - **Use Case**: Manual control over when a GObject tree is started.
- `gcflag_no_check_output_events`: (`0x0002`)
    - **Description**: Disables output event checking. When publishing events, it skips validation against the `output_event_list`.
    - **Use Case**: Optimization when output event checking is unnecessary.
- `gcflag_ignore_unknown_attrs`: (`0x0004`)
    - **Description**: Allows the creation of a GObject even if it contains attributes that are not defined in the `GClass`.
    - **Use Case**: Flexibility during dynamic object creation when unknown attributes may be present.
- `gcflag_required_start_to_play`: (`0x0008`)
    - **Description**: Prevents the GObject from entering the "play" state unless it has already been started.
    - **Use Case**: Ensures proper initialization before transitioning to "play."
- `gcflag_singleton`: (`0x0010`)
    - **Description**: Enforces that only one instance of the `GClass` can exist at a time.
    - **Use Case**: For `GClasses` that must maintain a single-instance constraint (e.g., a singleton pattern).


(__global_list_persistent_attrs_fn__)=
## __global_list_persistent_attrs_fn__

    TODO
