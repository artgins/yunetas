# root-esp32

ESP32 port of the Yuneta runtime GClasses. Provides the same GClass catalogue as `root-linux` (TCP, timers, protocols, …) but adapted to **ESP-IDF** and the ESP32 hardware.

## Layout

- `components/esp_gobj/` — ESP-IDF component wrapping `kernel/c/gobj-c` (the core GObj framework)
- `components/esp_c_prot/` — ESP-IDF component wrapping the protocol / networking GClasses
- `common_components/` — shared ESP-IDF common code
- `src/` — ESP32-specific GClass implementations

## Build

Built with ESP-IDF's CMake component manager, **not** by `yunetas build`. Include the component from an ESP-IDF project and the ESP-IDF build system will handle it.

See the top-level `CLAUDE.md` and ESP-IDF documentation for integration details.
