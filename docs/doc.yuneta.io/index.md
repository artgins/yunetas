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

[Yuneta Simplified](https://yuneta.io) is a development framework focused on messaging and services, based on 
[Event-driven](https://en.wikipedia.org/wiki/Event-driven_programming), 
[Automata-based](https://en.wikipedia.org/wiki/Automata-based_programming) 
and [Object-oriented](https://en.wikipedia.org/wiki/Object-oriented_programming) 
programming paradigms. 
Heavy use of json, serie-time, key-value, flat-files and graphs concepts.

[Yuneta Simplified](https://yuneta.io) is a real-time system RTS that includes development, testing, and deployment features. Built for Linux, and deployable on any bare-metal server.

Specialized in IoT data collection, all types of devices, and data exchange and protocol adaptation between systems, including collection, publication, and querying of messages in real time, with historical data storage. 

The messages (encrypted or plain text) circulating within the Yuneta system can be persistent in disk or exist only while in transit or in the memory of a service. All data in json.

For [Linux](https://en.wikipedia.org/wiki/Linux) and [RTOS/ESP32](https://www.espressif.com/en/products/sdks/esp-idf). 

Versions in C, partially in JavaScript. Todo in Python.

---

# Index


```{toctree}
:maxdepth: 3
:caption: Get Started

installation
activating
CHANGELOG
```

```{toctree}
:caption: Philosophy
:maxdepth: 3

philosophy/philosophy.md

```

```{toctree}
:caption: Guides
:maxdepth: 3

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

```

```{toctree}
:caption: GObj API
:maxdepth: 3

api/gobj/startup/api_gobj_startup.md
api/gobj/gclass/api_gobj_gclass.md
api/gobj/creation/api_gobj_creation.md
api/gobj/attrs/api_gobj_attrs.md
api/gobj/op/api_gobj_op.md
api/gobj/events_state/api_gobj_events_state.md
api/gobj/publish/api_gobj_publish.md
api/gobj/authz/api_gobj_authz.md
api/gobj/info/api_gobj_info.md
api/gobj/stats/api_gobj_stats.md
api/gobj/node/api_gobj_node.md
api/gobj/memory/api_gobj_memory.md
api/gobj/log/api_gobj_log.md
api/gobj/trace/api_gobj_trace.md
api/gobj/gbuffer/api_gobj_gbuffer.md
api/gobj/dl/api_gobj_dl.md

```

```{toctree}
:caption: Kwid API
:maxdepth: 3

api/kwid/api_kwid.md

```

```{toctree}
:caption: Helpers API
:maxdepth: 3

api/helpers/api_helpers.md

```

```{toctree}
:caption: Command Parser API
:maxdepth: 3

api/command_parser/api_command_parser.md

```

```{toctree}
:caption: Statistic Parser API
:maxdepth: 3

api/stats_parser/api_stats_parser.md

```

```{toctree}
:caption: Log UPD handler API
:maxdepth: 3

api/log_udp_handler/api_log_udp_handler.md

```

```{toctree}
:caption: Rotatory API
:maxdepth: 3

api/rotatory/api_rotatory.md

```

```{toctree}
:caption: Testing API
:maxdepth: 3

api/testing/api_testing.md

```


```{toctree}
:caption: Glossary 
:maxdepth: 3

glossary

```
