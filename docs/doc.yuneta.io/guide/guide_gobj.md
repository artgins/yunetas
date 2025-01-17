(gobj)=
# **GObj**

Gobjs are modular components that form the building blocks of Yuneta applications. The creation and configuration of gobjs are flexible, allowing developers to instantiate individual gobjs or entire hierarchies using structured APIs and configurations.

---

## GObj Creation APIs

### 1. **Creating Individual Gobjs**

The primary APIs for creating gobjs include:

| **API**                     | **Description**                                                                                   |
|-----------------------------|---------------------------------------------------------------------------------------------------|
| `gobj_create`               | Creates a GObj with the specified name, GClass, and attributes.                                   |
| `gobj_create2`              | Similar to `gobj_create`, but allows specifying flags to control the GObj's behavior.             |
| `gobj_create_yuno`          | Creates the root GObj of a Yuno, typically used to initialize the application.                    |
| `gobj_create_service`       | Creates a service GObj that exposes commands and events for interaction with other components.    |
| `gobj_create_default_service` | Creates a default service GObj with `autostart` enabled and `autoplay` disabled.                 |
| `gobj_create_volatil`       | Creates a volatile GObj, which is not persisted.                                                  |
| `gobj_create_pure_child`    | Creates a pure child GObj that directly sends events to its parent.                               |

### 2. **Creating GObj Trees**

Entire hierarchies of gobjs can be created using structured configurations:
- **`gobj_create_tree0`**: Creates a GObj tree based on a JSON structure (`jn_tree`).
- **`gobj_create_tree`**: Parses a tree configuration and calls `gobj_create_tree0` to instantiate the hierarchy.

---

(gobj_flag_t)=
## GObj Flags (`gobj_flag_t`)

Flags control the behavior and characteristics of gobjs during their creation. They can be combined to define multiple properties.

| **Flag**                  | **Value**   | **Description**                                                                 |
|---------------------------|-------------|---------------------------------------------------------------------------------|
| `gobj_flag_yuno`          | `0x0001`    | Marks the GObj as a Yuno root.                                                 |
| `gobj_flag_default_service` | `0x0002`  | Sets the GObj as the default service for the Yuno, with `autostart` enabled.    |
| `gobj_flag_service`       | `0x0004`    | Indicates that the GObj is a service, exposing public commands and events.     |
| `gobj_flag_volatil`       | `0x0008`    | Marks the GObj as volatile, meaning it is not persistent.                      |
| `gobj_flag_pure_child`    | `0x0010`    | Sets the GObj as a pure child, sending events directly to its parent.          |
| `gobj_flag_autostart`     | `0x0020`    | Automatically starts the GObj after creation.                                  |
| `gobj_flag_autoplay`      | `0x0040`    | Automatically plays the GObj after creation.                                   |

---

## JSON Tree Configuration for GObj Creation

GObj hierarchies can be defined using JSON structures, making it easy to specify attributes, children, and behavioral flags. A typical JSON tree structure includes:

| **Key**          | **Description**                                                                                  |
|-------------------|--------------------------------------------------------------------------------------------------|
| `gclass`         | The name of the GClass for the GObj.                                                             |
| `name`           | The name of the GObj.                                                                            |
| `default_service` | Boolean indicating if the GObj is the default service.                                           |
| `as_service`      | Boolean indicating if the GObj should act as a service.                                          |
| `autostart`       | Boolean indicating if the GObj should start automatically.                                       |
| `autoplay`        | Boolean indicating if the GObj should play automatically.                                        |
| `disable`         | Boolean indicating if the GObj should be disabled.                                               |
| `pure_child`      | Boolean indicating if the GObj is a pure child.                                                  |
| `kw`             | A dictionary of attributes to configure the GObj.                                                |
| `zchilds`        | A list of child GObjs with their respective configurations.                                       |

### Hierarchy Rules:
1. If a `subscriber` is not specified, it defaults to the parent GObj.
2. If there is only one child in `zchilds`, it is automatically set as the "bottom" GObj.

---

## Rules for Proper GObj Usage

- **Pure Child:** Automatically sends events to its parent using `gobj_send_event`.
- **Service:** Publishes events externally using `gobj_publish_event`.
- **Default Service:** Starts automatically but requires explicit play through the Yuno's play method.

---

## Benefits of GObj Creation APIs

- **Flexibility:** Allows creation of individual gobjs or entire hierarchies.
- **Modularity:** Enables clear separation of components through services and child-parent relationships.
- **Dynamic Behavior:** Flags and attributes provide fine-grained control over the GObj's functionality.
- **Scalability:** JSON tree configurations simplify the management of large, complex GObj hierarchies.
