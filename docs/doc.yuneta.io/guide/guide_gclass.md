(gclass)=
# **GClass**

# What is a GClass?

A `GClass` (short for GObject Class) is a core construct in the Yuneta framework. It defines the behavior, attributes, and structure of GObjects (runtime instances of a `GClass`). The `GClass` encapsulates the functionality and state machine of a particular object type, serving as a blueprint for creating and managing GObjects.

## Components of a GClass

A `GClass` consists of several key components that define its operation:

1. **Name**
    - Each `GClass` has a unique name (e.g., `C_CHANNEL`) to identify it.

2. **Attributes**
    - A description of the data fields or properties associated with objects of the `GClass`.
    - Defined using a table of descriptors (`tattr_desc`), specifying the type, name, flags, default value, and description for each attribute.

3. **Methods (GMETHODS)**
    - A set of global methods (`gmt`) that the `GClass` supports, such as:

   > - `mt_create`: Initialize the GObject.
   > - `mt_start`: Start the GObject's operation.
   > - `mt_stop`: Stop the GObject's operation.
   > - `mt_reading` and `mt_writing`: Access or modify attributes dynamically.

    - These methods are dynamically dispatched through the GObject API.

4. **Finite State Machine (FSM)**
    - Defines the behavior of the `GClass` in terms of states, events, and transitions:

   > - **States**: Predefined states (e.g., `ST_CLOSED`, `ST_OPENED`).
   > - **Events**: Triggers for state transitions (e.g., `EV_ON_OPEN`, `EV_ON_CLOSE`).
   > - **Actions**: Functions executed during transitions between states.

5. **Event Table**
    - A list of public and private events that the `GClass` can handle or emit.
    - Events are categorized with flags, such as `EVF_OUTPUT_EVENT`.

6. **Trace Levels**
    - Defines trace levels for debugging and monitoring specific operations of the `GClass`.

7. **Private Data**
    - A structure (`PRIVATE_DATA`) for storing runtime-specific data for each GObject instance of the `GClass`.

8. **Authorization and Commands (Optional)**
    - **Authorization Table**: Specifies access control rules.
    - **Command Table**: Describes executable commands for the GClass.

## How a GClass Works

1. **Definition**:
    - A `GClass` is created using the `gclass_create()` function, combining its attributes, methods, states, events, and other metadata.
2. **Instantiation**:
    - A GObject instance is created from a `GClass` using the GObject API.
3. **Interaction**:
    - The GObject API (e.g., `gobj_start`, `gobj_stop`) dynamically invokes the global methods (`gmt`) defined in the `GClass`.
4. **Extensibility**:
    - The modular design of `GClass` allows new behaviors and features to be added by defining additional methods, events, or states.

## Summary

A `GClass` in Yuneta is the foundation for defining the behavior and structure of objects in the framework. It encapsulates attributes, methods, and finite state machines, enabling the creation of reusable, modular, and extensible components.



(GMETHODS)=
## GMETHODS

The `GMETHODS` structure in Yuneta defines the global methods that a `GClass` can implement. These methods encapsulate key behaviors and operations associated with the lifecycle, state management, and functionality of GObjects.

Each method serves a specific purpose and is invoked through the GObject API, enabling dynamic and modular behavior across different `GClasses`.

-----------------------
GMETHODS Overview
-----------------------

- **`mt_create`**
   - Purpose: Initializes the GObject. Called when a GObject is instantiated.
   - Example: Allocating memory or setting default attribute values.

- **`mt_create2`**
   - Purpose: Provides extended initialization capabilities with additional parameters.
   - Example: Custom initialization logic based on specific input.

- **`mt_destroy`**
   - Purpose: Cleans up and destroys the GObject. Called when the GObject is deleted.
   - Example: Freeing resources or stopping child GObjects.

- **`mt_start`**
   - Purpose: Starts the operation of the GObject.
   - Example: Transitioning to a running state or starting timers.

- **`mt_stop`**
   - Purpose: Stops the operation of the GObject.
   - Example: Halting operations, stopping timers, or freeing temporary resources.

- **`mt_play`**
   - Purpose: Resumes the GObject's operation after being paused.
   - Example: Resuming services or processes.

- **`mt_pause`**
   - Purpose: Pauses the GObject's operation.
   - Example: Temporarily halting services without stopping them.

- **`mt_writing`**
   - Purpose: Handles attribute write operations dynamically.
   - Example: Validating or processing the value being written.

- **`mt_reading`**
   - Purpose: Handles attribute read operations dynamically.
   - Example: Adjusting or filtering the value before returning it.

- **`mt_subscription_added`**
   - Purpose: Called when a subscription to an event is added.
   - Example: Managing event listeners or counting subscriptions.

- **`mt_subscription_deleted`**
   - Purpose: Called when a subscription to an event is removed.
   - Example: Cleaning up resources or notifying other components.

- **`mt_child_added`**
   - Purpose: Called after a child GObject is created and added.
   - Example: Managing hierarchical relationships.

- **`mt_child_removed`**
   - Purpose: Called before a child GObject is removed.
   - Example: Cleaning up references or handling dependencies.

- **`mt_stats`**
   - Purpose: Returns statistics related to the GObject in a JSON format.
   - Example: Providing performance or usage metrics.

- **`mt_command_parser`**
   - Purpose: Parses and executes user commands specific to the GClass.
   - Example: Implementing custom command handling logic.

- **`mt_inject_event`**
   - Purpose: Handles events manually, bypassing the built-in state machine.
   - Example: Custom event-processing logic.

- **`mt_create_resource`**
   - Purpose: Creates a new resource managed by the GObject.
   - Example: Initializing data structures or external dependencies.

- **`mt_list_resource`**
   - Purpose: Lists resources managed by the GObject.
   - Example: Returning an iterator or JSON representation.

- **`mt_save_resource`**
   - Purpose: Saves or updates a resource.
   - Example: Persisting data to storage.

- **`mt_delete_resource`**
   - Purpose: Deletes a resource.
   - Example: Removing an entry from a database.

- **`mt_state_changed`**
   - Purpose: Handles transitions between states.
   - Example: Logging state changes or triggering side effects.

- **`mt_authenticate`**
   - Purpose: Authenticates users or services.
   - Example: Verifying credentials or tokens.

- **`mt_list_childs`**
   - Purpose: Lists the child GObjects of the current GObject.
   - Example: Returning hierarchical information.

- **`mt_stats_updated`**
   - Purpose: Notifies that statistics have been updated.
   - Example: Refreshing metrics in real time.

- **`mt_disable`**
   - Purpose: Disables the GObject.
   - Example: Preventing further operations until re-enabled.

- **`mt_enable`**
   - Purpose: Enables the GObject.
   - Example: Allowing operations after being disabled.

- **`mt_trace_on`**
   - Purpose: Enables tracing for the GObject.
   - Example: Activating debug or log output.

- **`mt_trace_off`**
   - Purpose: Disables tracing for the GObject.
   - Example: Deactivating debug or log output.

- **`mt_gobj_created`**
   - Purpose: Special method invoked when a GObject is created. Typically for the root object (`__yuno__`).
   - Example: Custom initialization for the Yuno.

- **`mt_publish_event`**
   - Purpose: Manages the publication of events.
   - Example: Filtering or modifying events before they are emitted.

- **`mt_authz_checker`**
   - Purpose: Checks authorization for specific actions or events.
   - Example: Enforcing access control policies.

- **`mt_create_node`, `mt_update_node`, `mt_delete_node`, `mt_link_nodes`, `mt_unlink_nodes`**
   - Purpose: Methods for managing nodes in TreeDB.
   - Example: Adding, updating, deleting, or linking nodes.

- **`mt_topic_desc`, `mt_topic_links`, `mt_topic_hooks`, `mt_topic_size`**
   - Purpose: Methods for inspecting TreeDB topics.
   - Example: Describing topics or retrieving metadata.

- **`mt_shoot_snap`, `mt_activate_snap`, `mt_list_snaps`**
   - Purpose: Manage snapshots in the TreeDB.
   - Example: Creating or restoring data snapshots.

-----------------------
Conclusion
-----------------------

The `GMETHODS` structure enables flexible, modular behavior in the Yuneta framework. Each method is optional, allowing `GClasses` to implement only the functionality they need.



(LMETHOD)=
## LMETHOD

(gclass_flag_t)=
## gclass_flag_t

# `gclass_flag_t` Values

The `gclass_flag_t` enumeration defines flags that can be applied to a `GClass` to modify its behavior. These flags are used to control specific operational aspects of the `GClass`.

## List of `gclass_flag_t` Flags

- **`gcflag_manual_start`** (`0x0001`)
  - **Description**: Prevents the automatic start of the GObject tree. When this flag is set, `gobj_start_tree()` will not start the GObject automatically.
  - **Use Case**: Manual control over when a GObject tree is started.
- **`gcflag_no_check_output_events`** (`0x0002`)
  - **Description**: Disables output event checking. When publishing events, it skips validation against the `output_event_list`.
  - **Use Case**: Optimization when output event checking is unnecessary.
- **`gcflag_ignore_unknown_attrs`** (`0x0004`)
  - **Description**: Allows the creation of a GObject even if it contains attributes that are not defined in the `GClass`.
  - **Use Case**: Flexibility during dynamic object creation when unknown attributes may be present.
- **`gcflag_required_start_to_play`** (`0x0008`)
  - **Description**: Prevents the GObject from entering the "play" state unless it has already been started.
  - **Use Case**: Ensures proper initialization before transitioning to "play."
- **`gcflag_singleton`** (`0x0010`)
  - **Description**: Enforces that only one instance of the `GClass` can exist at a time.
  - **Use Case**: For `GClasses` that must maintain a single-instance constraint (e.g., a singleton pattern).

## Summary

The `gclass_flag_t` flags provide fine-grained control over the behavior of `GClasses` and their instances, enhancing flexibility and enabling specialized configurations.
