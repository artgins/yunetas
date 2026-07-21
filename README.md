# Yunetas

[![License Shield](https://img.shields.io/badge/license-MIT-orange)](https://github.com/artgins/yunetas/blob/main/LICENSE.txt)
[![API Docs](https://img.shields.io/badge/api-docs-informational.svg)](https://doc.yuneta.io/)
[![Ask DeepWiki](https://deepwiki.com/badge.svg)](https://deepwiki.com/artgins/yunetas)

<a href="https://yuneta.io/">
    <img src="https://github.com/artgins/yunetas/blob/main/docs/doc.yuneta.io/_static/yuneta-image.svg?raw=true" alt="Icon" width="200" /> <!-- Adjust the width as needed -->
</a>

# Yuneta Simplified

An Asynchronous Development Framework

For more details, see [doc.yuneta.io](https://doc.yuneta.io)

## Install

One command, on both distro families — Debian/Ubuntu (`.deb`) and
RHEL/Rocky/Alma (`.rpm`). It installs the **`yuneta-agent` package** (the
runtime: agent, CLI tools, bundled web server, plus the libraries, headers and
CMake toolchain under `/yuneta/development/yunetas/`) **and** the developer
toolchain, so the machine can both run yunos and build your own projects:

    curl -fsSL https://raw.githubusercontent.com/artgins/yunetas/main/install.sh | sudo sh

To install **only the `yuneta-agent` package**, without the developer toolchain
— a pure deployment node:

    curl -fsSL https://raw.githubusercontent.com/artgins/yunetas/main/install.sh | sudo sh -s -- --runtime-only

Both forms also set up certbot. To pin a published release instead of the
latest, pass its tag: `… | sudo sh -s -- 7.8.6`.

> [!WARNING]
> **The packages *run* anywhere. They only *build* on one distro each.**
>
> | Package | Node that can build against it | glibc |
> |---------|--------------------------------|-------|
> | `.rpm` (EL9)   | **Rocky Linux / AlmaLinux 9** | 2.34 |
> | `.deb` (amd64) | **Debian 13** (trixie)        | 2.41 |
>
> The shipped binaries are fully static, so the agent, the CLI tools and your
> yunos run on any Linux of the same architecture. But the package also carries
> a sparse SDK — prebuilt static archives under `outputs/` — and those archives
> reference glibc internals whose layout moves between releases. Linking your
> own code against them requires the node's glibc to match **exactly**.
>
> A mismatch does not fail the link. It **succeeds silently** and corrupts the
> heap at run time: SIGABRT inside glibc's allocator seconds after start, no
> Yuneta error logged first, and a stack pointing at innocent code. A build
> guard stops this at configure time — do not override it.
>
> Every other distro — Ubuntu (24.04 is glibc 2.39, 26.04 is 2.43), Debian 12
> (2.36) — is **runtime-only**. To develop there, either build on a matching
> node and push the binaries with `yunetas sync-binaries`, or build the
> framework from source (below) on that machine.

What the script does step by step, and the full list of verified distros:
[Installation](https://doc.yuneta.io/installation/).

## Build from source

To develop the framework itself, or to build it with different options
(TLS backend, modules, static/dynamic):

    git clone --recurse-submodules https://github.com/artgins/yunetas.git

[pypi-badge]: https://img.shields.io/pypi/v/yunetas


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
