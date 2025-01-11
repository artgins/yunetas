# GObj

The main api for the Yuneta framework. It defines the GObj (Generic Object) API, which includes the GObj system, event-driven architecture, finite state machines, hierarchical object trees, and additional utilities.

This API provides the prototypes, structures, enums, and macros needed to work with the Yuneta framework's GObj system.

- **GObj Features**:
    - Event-driven model using input/output events.
    - Simple finite state machine to manage object states.
    - Hierarchical organization of GObjs forming a tree structure (Yuno).
    - Communication between objects through events (key-value messages).
    - Built-in logging, buffering, and JSON handling.
- **Internal Systems**:
    - Log system.
    - Buffer system.
    - JSON system based on the [`Jansson`](http://jansson.readthedocs.io/en/latest/) library.


---

## Start up functions

```{toctree}
:caption: Functions
:maxdepth: 2

api_gobj_start_up
api_gobj_end

```
