---
title: Yuneta Simplified
---

# Yuneta Simplified

::::{grid}
:reverse:
:gutter: 2 1 1 1
:margin: 4 4 1 1

:::{grid-item}
:columns: 4

```{image} ./_static/yuneta-image.svg
:width: 150px
```
:::

:::{grid-item}
:columns: 8
:class: sd-fs-3

An Asynchronous Development Framework 

::::

`Yuneta Simplified` is a **development framework** focused on **messaging** and **services**, based on 
[Event-driven](https://en.wikipedia.org/wiki/Event-driven_programming), 
[Automata-based](https://en.wikipedia.org/wiki/Automata-based_programming) 
and [Object-oriented](https://en.wikipedia.org/wiki/Object-oriented_programming) 
programming paradigms. 
Heavy use of JSON, **serie-time**, **key-value**, **flat-files** and **graphs** concepts.

[Yuneta Simplified](https://yuneta.io) is a **real-time system** RTS that includes **development**, **testing**, and **deployment** features. Built for Linux, and **deployable** on any **bare-metal** server. Too it has been ported successfully to [ESP-IDF](https://www.espressif.com/en/products/sdks/esp-idf), a basic version.

Specialized in IoT data collection, all types of devices, and data exchange and protocol adaptation between systems, including collection, **publication/subscription**, and querying of **messages** in **real time**, with **historical** data storage. 

The messages (**encrypted** or plain text) circulating within the Yuneta system can be persistent in disk or exist only while in transit or in the memory of a service. All data in JSON.

For [Linux](https://en.wikipedia.org/wiki/Linux) and [RTOS/ESP32](https://www.espressif.com/en/products/sdks/esp-idf). 

Versions in C, partially in JavaScript. Todo in Python.

---

# Index


:::{toctree}
:maxdepth: 1
:caption: Get Started

installation
activating
CHANGELOG
:::

:::{toctree}
:caption: Philosophy
:maxdepth: 1

philosophy/philosophy.md

:::

:::{toctree}
:caption: Guides
:maxdepth: 1

guide/basic_concepts.md
guide/guide_gclass.md
guide/guide_gobj.md
guide/guide_sdata.md
guide/guide_kwid.md
guide/guide_gbuffer.md
guide/folders.md
guide/persistent_attrs.md
guide/parser_command.md
guide/parser_stats.md
guide/settings.md
guide/guide_authz.md
guide/guide_timeranger2.md
guide/guide_yev_loop.md
guide/guide_ytls.md

:::

:::{toctree}
:caption: GObj API
:maxdepth: 1


api/gobj/api_gobj_startup.md
api/gobj/api_gobj_gclass.md
api/gobj/api_gobj_creation.md
api/gobj/api_gobj_attrs.md
api/gobj/api_gobj_op.md
api/gobj/api_gobj_events_state.md
api/gobj/api_gobj_resource.md
api/gobj/api_gobj_publish.md
api/gobj/api_gobj_authz.md
api/gobj/api_gobj_info.md
api/gobj/api_gobj_stats.md
api/gobj/api_gobj_node.md
api/gobj/api_gobj_memory.md
api/gobj/api_gobj_log.md
api/gobj/api_gobj_trace.md
api/gobj/api_gobj_gbuffer.md
api/gobj/api_gobj_dl.md

:::

:::{toctree}
:caption: Helpers API
:maxdepth: 1

api/helpers/api_file_system.md
api/helpers/api_string_helper.md
api/helpers/api_json_helper.md
api/helpers/api_directory_walk.md
api/helpers/api_time_date.md
api/helpers/api_misc.md
api/helpers/api_common_protocol.md
api/helpers/api_daemon_launcher.md
api/helpers/api_url_parsing.md
api/helpers/api_backtrace.md
api/helpers/api_http_parser.md
api/helpers/api_istream.md

:::

:::{toctree}
:caption: GObj Auxiliary API
:maxdepth: 1

api/kwid/api_kwid.md
api/command_parser/api_command_parser.md
api/stats_parser/api_stats_parser.md
api/log_udp_handler/api_log_udp_handler.md
api/rotatory/api_rotatory.md
api/testing/api_testing.md

:::

:::{toctree}
:caption: Timeranger2 API
:maxdepth: 1

api/timeranger2/api_timeranger2
api/timeranger2/api_fs_watcher
api/timeranger2/api_tr_msg
api/timeranger2/api_tr_msg2db
api/timeranger2/api_tr_queue
api/timeranger2/api_treedb

:::

:::{toctree}
:caption: Event Loop API
:maxdepth: 1

api/yev_loop/api_yev_loop

:::

:::{toctree}
:caption: TLS API
:maxdepth: 1

api/ytls/api_ytls

:::


:::{toctree}
:caption: Glossary 
:maxdepth: 1

glossary

:::
