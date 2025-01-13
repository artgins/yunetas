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
:maxdepth: 1
:caption: Get Started

installation
activating
CHANGELOG
```

```{toctree}
:caption: Philosophy
:maxdepth: 1

philosophy/philosophy.md

```

```{toctree}
:caption: Guides
:maxdepth: 1

guide/basic_concepts.md
guide/guide_gclass.md
guide/guide_gobj.md
guide/guide_kwid.md
guide/folders.md
guide/persistent_attrs.md
guide/parser_command.md
guide/parser_stats.md
guide/settings.md
guide/guide_authz.md

```

```{toctree}
:caption: API Reference
:maxdepth: 2

api/api_gobj.md
api/api_kwid.md

```

```{toctree}
:caption: Glossary 
:maxdepth: 1

glossary

```
