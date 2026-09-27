# Get Started

**Current version: [7.25.9](https://github.com/artgins/yunetas/tree/7.25.9)** ·
*Documentation updated: 2026-09-27*

## What is Yuneta

`Yuneta Simplified` is a **development framework** focused on **messaging** and **services**, based on
[Event-driven](https://en.wikipedia.org/wiki/Event-driven_programming),
[Automata-based](https://en.wikipedia.org/wiki/Automata-based_programming)
and [Object-oriented](https://en.wikipedia.org/wiki/Object-oriented_programming)
programming paradigms.
Heavy use of JSON, **time-series**, **key-value**, **flat-files** and **graphs** concepts.

[Yuneta Simplified](https://yuneta.io) is a **real-time system** RTS that includes **development**, **testing**, and **deployment** features. Built for Linux, and **deployable** on any **bare-metal** server.

Specialized in IoT data collection, all types of devices, and data exchange and protocol adaptation between systems, including collection, **publication/subscription**, and querying of **messages** in **real time**, with **historical** data storage.

The messages (**encrypted** or plain text) circulating within the Yuneta system can be persistent on disk or exist only while in transit or in the memory of a service. All data in JSON.

For [Linux](https://en.wikipedia.org/wiki/Linux).

Versions in **C** (reference implementation) and **JavaScript** (browser/Node).

Install Yuneta, prepare your build environment, and bring a node online.
Start here if this is your first contact with the framework.

## In this section

- [Installation](installation.md) — system prerequisites, the apt dependency
  list, and the one-time environment setup ([`menuconfig`](installation.md#configure-menuconfig), [`yunetas-env.sh`](https://github.com/artgins/yunetas/blob/7.25.9/yunetas-env.sh),
  building the external libraries).
- [Activating](activating.md) — initialize, build, and run the agent, then
  launch your first yuno.
- [Changelog](CHANGELOG.md) — release history.

The build and deploy steps below are driven by the
[`yunetas` CLI](yunetas-cli.md) — its own page is the full command reference.
