# **Basic Concepts**

# **What is Yuneta?**

Yuneta is a **function-based development framework** that implements an independent **class system**, abstracted from the programming language. Using functions, classes are created with a core and interface composed of three primary elements: `attributes`, `commands`, and `messages`.

Yuneta's architecture, centered around [`gclass`](gclass), [`gobj`](gobj), and [`yuno`](yuno), allows developers to create powerful, event-driven, and scalable applications tailored to complex systems.

---

## **Core Features**

### **1. Class System**
- Yuneta's class system operates independently of language-specific class definitions, ensuring compatibility across different programming environments.
- Classes ( [`gclass`](gclass)) define the structure, behavior, and interaction model of reusable components.

### **2. Interaction Model of GClasses and Gobjs**
The interaction or **interface** of a gclass or gobj is based primarily on the  `attributes`, `commands`, and `messages` defined by the gclass:

- **Attributes**: 
  - Represent the configuration and runtime behavior of objects.
  - Can be modified dynamically to change the behavior of a gobj during execution.
  
- **Commands**: 
  - Provide direct, high-level interaction.
  - Executed independently of the current state of the gobj's FSM.
  - Can include parameters for more complex operations.
  
- **Messages**: 
  - Define how a gobj communicates with others. These are:
    - **Input Messages**: Trigger actions or state transitions.
    - **Output Messages**: Publish information to other components.
  - Messages are structured as **key-value pairs**, where:
    - **Event Name**: Identifies the purpose of the message.
    - **KW**: A dictionary or object containing the payload.

---

## **Key Features of a Gclass**

1. **No Traditional Inheritance**:
    - Gclasses do not support inheritance in the traditional object-oriented sense.
    - Instead, gclasses instantiate objects into a **hierarchical tree of gobjs** within a binary, called a **yuno**.
    - The root object of this tree is referred to as `__yuno__` or `__root__`.

2. **Bottom Gobj Mechanism**:
    - Gclasses implement a specific kind of inheritance through the `bottom gobj` mechanism:
        - A gobj can designate a child object as its `bottom`.
        - Certain elements, such as `attributes`, are inherited from the `bottom gobj`.
    - **Attribute Lookup**:
        - If a gobj does not define an attribute, it attempts to retrieve the attribute from its `bottom gobj`.
        - This process continues up the `bottom` chain until a gobj with the attribute is found.

3. **Event-Driven Design**:
    - Gclasses define the events their gobjs can handle and emit.
    - Events are used for communication and triggering state transitions.

4. **FSM-Based Behavior**:
    - Each gclass defines a finite state machine (FSM) that governs the behavior of its gobjs.

5. **Interface Through Attributes, Commands, and Messages**:
    - Gclasses define attributes for configuration, commands for direct interaction, and messages for communication.

---

## **Applications of Yuneta**
Yuneta is suited for:
- **IoT**: Managing interconnected devices and sensors.
- **Networking**: Building scalable and modular communication systems.
- **Monitoring**: Real-time data collection and analysis.
- **Complex Event Processing (CEP)**: Handling asynchronous and distributed events.

---

# `gclass`
A `gclass` (Generic Class) is the **template or blueprint** for creating gobjs. It defines the structure, behavior, and lifecycle of gobjs. Unlike traditional classes in languages like C, Python, or JavaScript, a gclass is implemented manually using the [](gclass_create) function, as Yuneta is a function-based development framework.

A `gclass` provides a complete definition of the structure, behavior, and lifecycle of [`gobjs`](gobj). By combining attributes, commands, events, states, and lifecycle methods, it establishes a powerful and flexible framework for developing event-driven, modular systems.

```{figure} ../_static/gclass_diagram.svg
---
alt: gclass_diagram
name: gclass_diagram
target: ../_static/gclass_diagram.svg
---
```

## **Key Features of a Gclass**:
1. **Independent Implementation**:
    - A gclass is not tied to any specific language's class structure and is manually defined for consistency across environments.

2. **Components of a Gclass**:
    - `gclass_name`: A unique identifier for the gclass. It must be distinct across all gclasses in the system.

    - `event_types`: A list of events supported by the gclass. Each event is defined with a name and associated flags. Events, along with commands and attributes, form the primary interface of a gobj. Events define how a gobj interacts with other gobjs and responds to external stimuli.

    - `states`: A simple finite state machine (FSM) defining:
        - States of the gobj.
        - Events supported in each state.
        - State transitions triggered by specific events.

    - `gmt`: Global Methods Table, a table of class methods executed by Yuneta's framework during specific lifecycle events.

    - `lmt`: Local Methods Table, A table of private methods that can be explicitly invoked by users using [](gobj_local_method). Should only be used when necessary. The natural interface for interacting with gobjs is through events, commands, and attributes.

    - `tattr_desc`: Attribute Table, defines the attributes of the gclass. See [](attributes). Attributes form a crucial part of the gobj's interface. They configure the gobj and can dynamically modify its behavior at runtime. Changes to attributes can trigger monitoring or additional actions.

    - `priv_size`: Specifies the size of private data for the gobj. A memory buffer of this size is allocated for each gobj instance to store its private data (in C).

    - `authz_table`: Authorization Table, defines access restrictions for specific actions or commands based on user permissions. Enhances security by controlling access to sensitive operations.

    - `command_table`: Defines commands supported by the gclass. Commands can include parameters and operate independently of the FSM. Commands are parsed using an internal parser, which can be replaced in [](gobj_start_up) with a custom parser. Commands are a key part of the gobj's interface, allowing external systems or users to interact with the gobj directly.

    - `s_user_trace_level`: Defines trace levels for the gclass. These trace levels can be dynamically activated during runtime to log the activity and behavior of gobjs. Facilitates debugging and monitoring of gobj operations.

    - `gclass_flag`: A modifier for the gclass. Its behavior is defined by the [](gclass_flag_t) type.
      May specify flags that alter class-level behavior, such as enabling or disabling specific features.

# `gobj`
A `gobj` (Generic Object) is an **instance** of a  [](gclass) (Generic Class) within the Yuneta framework. It is a modular, reusable, and event-driven component that encapsulates data, behavior, and state.

## **Key Features of a gobj**:
1. **Event-Driven Design**:
    - Gobjs interact through external and internal events to trigger actions or state transitions.
    - They can publish and subscribe to events based on authorization rules.

2. **Hierarchical Structure**:
    - Organized in a parent-child hierarchy where a gobj has one parent and can manage multiple children.

3. **Finite State Machine (FSM)**:
    - Each gobj is governed by an FSM that defines its states, events, and state-specific actions.

4. **Modularity and Autonomy**:
    - Gobjs are independent units designed for reusability and scalability.

(yuno)=
# `yuno`

**What is a Yuno?**

Once a system of gclasses is created, we can build a **yuno**, which is a **single-threaded, asynchronous binary** composed of a **hierarchical tree of gobjs**.

## **Key Characteristics of a Yuno**:
1. **Monolithic Binary**:
    - A yuno encapsulates all functionality within a single binary, simplifying deployment and management.

2. **Asynchronous Execution**:
    - Operates in a non-blocking, event-driven manner to efficiently manage tasks.

3. **Hierarchical Structure**:
    - Built as a tree of gobjs, where:
        - Each gobj is a node in the hierarchy.
        - Parent gobjs manage their children, enabling structured interaction and modular design.

4. **Root Object**:
    - The root of the gobj tree is referred to as the `__yuno__` or `__root__`.

5. **Scalability and Modularity**:
    - A yuno can be expanded by adding new gclasses and combining them hierarchically.
