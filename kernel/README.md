# Yuneta Kernel

The Yuneta kernel has an implementation in C and an implementation in JavaScript.

## c

The C implementation is the reference implementation. It is complete. CMake builds it.

See [`c/README.md`](c/README.md) for the full C API reference.

## js

The JavaScript (ES6) implementation is complete. Vite builds it.

| Package | Description |
|---------|-------------|
| [`gobj-js`](js/gobj-js/) | Core GObject + FSM runtime (`@yuneta/gobj-js`) |
| [`gobj-ui`](js/gobj-ui/) | Reusable UI component library (`@yuneta/gobj-ui`) |

See [`js/README.md`](js/README.md) for an overview of the JavaScript packages.
