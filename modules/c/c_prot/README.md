# c_prot — Protocol GClasses

Optional Yuneta module that groups protocol-layer GClasses.
Enabled/disabled at configure time via `menuconfig` (`CONFIG_C_PROT`).

## Source layout

| File | Role |
|------|------|
| `src/register_prot.c` / `.h` | Registers all enabled protocol GClasses with the Yuneta runtime |
| `src/yunetas_c_prot.h` | Convenience umbrella header — include this instead of individual headers |
| `src/c_prot_mqtt2.c` / `.h` | MQTT v3.1.1 + v5.0 protocol GClass (broker **and** client roles) |
| `src/mqtt_util.c` / `.h` | MQTT packet encode/decode helpers |
| `src/tr2q_mqtt.c` / `.h` | timeranger2-backed MQTT session/subscription persistence queue |
| `src/topic_tokenise.c` / `.h` | MQTT topic tokeniser used by the broker |

## Kconfig options

| Symbol | Default | Description |
|--------|---------|-------------|
| `CONFIG_C_PROT` | `y` | Build and install the whole `c_prot` library |
| `CONFIG_C_PROT_MQTT` | `y` | Include the MQTT protocol (`c_prot_mqtt2` + helpers). Visible only when `CONFIG_C_PROT=y` |

Configure with:

```bash
cd $YUNETAS_BASE
python tools/kconfig/menuconfig.py
```

## Build

The module follows the standard Yuneta build flow:

```bash
source yunetas-env.sh
yunetas build          # builds all layers in dependency order
```

Or manually:

```bash
cd modules/c/c_prot
mkdir build && cd build
cmake -GNinja -DCMAKE_BUILD_TYPE=Debug ..
ninja && ninja install
```

When `CONFIG_C_PROT_MQTT=n` the MQTT source files are excluded from the
library and their headers are not installed, so projects that include
`yunetas_c_prot.h` must also have `CONFIG_C_PROT_MQTT=y`.
