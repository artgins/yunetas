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

[//]: # (Versions in[C]&#40;https://en.wikipedia.org/wiki/C_&#40;programming_language&#41;&#41;, [Python]&#40;https://www.python.org/&#41; and Javascript.)

Versions in C, Python and Javascript.

**Dependencies**: development for C and Linux:
- [Jansson](http://jansson.readthedocs.io/en/latest/)
- [libjwt](https://github.com/benmcollins/libjwt)
- [liburing](https://github.com/axboe/liburing)
- [mbedtls](https://www.trustedfirmware.org/projects/mbed-tls/) (future)
- [openssl](https://www.openssl.org/)
- [pcre2](https://github.com/PCRE2Project/pcre2)

**Dependencies**: deploy for Linux: 
- [nginx](https://nginx.org)
- [openresty](https://openresty.org/)


```{toctree}
:maxdepth: 1
:caption: Get Started

installation
activating
CHANGELOG
```

```{toctree}
:caption: Philosophy
:maxdepth: 2

philosophy/philosophy.md

```

```{toctree}
:caption: Api Reference
:maxdepth: 3

api/api_gobj
api/api_kwid
api/api_glossary

```
