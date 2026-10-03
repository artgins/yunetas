# Yunetas

[![License Shield](https://img.shields.io/badge/license-MIT-orange)](https://github.com/artgins/yunetas/blob/main/LICENSE.txt)
[![API Docs](https://img.shields.io/badge/api-docs-informational.svg)](https://doc.yuneta.io/)
[![Ask DeepWiki](https://deepwiki.com/badge.svg)](https://deepwiki.com/artgins/yunetas)

<a href="https://yuneta.io/">
    <img src="https://github.com/artgins/yunetas/blob/main/docs/doc.yuneta.io/_static/yuneta-image.svg?raw=true" alt="Icon" width="200" /> <!-- Adjust the width as needed -->
</a>

# Yuneta Simplified

An Asynchronous Development Framework

<!-- yuneta-intro-start: also the introduction of doc.yuneta.io (get_started.md includes it) -->
`Yuneta Simplified` is a **development framework** focused on **messaging** and **services**, built on the
[event-driven](https://en.wikipedia.org/wiki/Event-driven_programming),
[automata-based](https://en.wikipedia.org/wiki/Automata-based_programming)
and [object-oriented](https://en.wikipedia.org/wiki/Object-oriented_programming)
programming paradigms, with heavy use of **time-series**, **key-value**, **flat-file** and **graph** concepts.

It is a **real-time system** that covers **development**, **testing** and **deployment**:
built for [Linux](https://en.wikipedia.org/wiki/Linux), and **deployable** on any **bare-metal** server.

It specializes in IoT data collection from any kind of device, and in data exchange and protocol adaptation
between systems: **publication/subscription** and querying of **messages** in **real time**, with **historical** storage.

Messages, **encrypted** or in plain text, can be persisted on disk or live only in transit or in the memory
of a service. All data is JSON.

Versions in **C** (the reference implementation) and **JavaScript** (browser/Node).
<!-- yuneta-intro-end -->

For more details, see [doc.yuneta.io](https://doc.yuneta.io)

## Install

> [!IMPORTANT]
> **Yuneta needs Linux 5.19 or later**: its event loop runs on io_uring. RHEL,
> Rocky and Alma **9** work too (Red Hat backports io_uring into their 5.14,
> disabled by default: set `kernel.io_uring_disabled=0`). **RHEL / Rocky / Alma
> 8 (kernel 4.18) have no io_uring and cannot run Yuneta.** Details:
> [Linux kernel](https://doc.yuneta.io/installation#kernel-required).

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
> yunos run on any Linux of the same architecture with the kernel above. But the package also carries
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

## Performance

A release that moves a lot of code ships a performance report: charts of what
the release does on one machine, the release before measured the same way, and every loss with its
reason.

- **7.26.0** (against 7.25.22): reading a timeranger2 history 16-17% faster,
  opening a treedb 20% faster, a tm query of a topic 7.25.22 had not marked 31x
  faster; one price said with its reason (a tm query of a topic 7.25.22 had
  marked, 7.5 -> 12.8 ms: the tm markers are gone).
  [Report](performance/reports/7.26.0.html) (the file) ·
  [rendered view](https://htmlpreview.github.io/?https://github.com/artgins/yunetas/blob/7.26.0/performance/reports/7.26.0.html) ·
  [raw figures](performance/reports/7.26.0.json)
- **7.25.22** (against 7.25.21): two reviews closed, the agents as systemd
  units, and a forced treedb delete 39% faster; nothing slower on a path this
  release changed.
  [Report](performance/reports/7.25.22.html) (the file) ·
  [rendered view](https://htmlpreview.github.io/?https://github.com/artgins/yunetas/blob/7.25.22/performance/reports/7.25.22.html) ·
  [raw figures](performance/reports/7.25.22.json)
- **7.25.21** (against 7.25.20): secrets masked in every trace, dump and kw,
  secret gbuffers, a reworked TCP server lifecycle, and nothing slower beyond
  the spread of its rounds.
  [Report](performance/reports/7.25.21.html) (the file) ·
  [rendered view](https://htmlpreview.github.io/?https://github.com/artgins/yunetas/blob/7.25.21/performance/reports/7.25.21.html) ·
  [raw figures](performance/reports/7.25.21.json)
- **7.25.20** (against 7.25.5): treedb writes 16-25% faster and the start of
  40 treedbs twice as fast, from the `SWITCHS` fix of 7.25.7; nothing slower.
  [Report](performance/reports/7.25.20.html) (the file) ·
  [rendered view](https://htmlpreview.github.io/?https://github.com/artgins/yunetas/blob/7.25.20/performance/reports/7.25.20.html) ·
  [raw figures](performance/reports/7.25.20.json)
- **7.25.5** (against 7.25.4): an agent audit record 11x cheaper, a large
  store opened in 13% less time, a treedb update in memory in 26% less time,
  appends at the same speed with four new checks each.
  [Report](performance/reports/7.25.5.html) (the file) ·
  [rendered view](https://htmlpreview.github.io/?https://github.com/artgins/yunetas/blob/7.25.5/performance/reports/7.25.5.html) ·
  [raw figures](performance/reports/7.25.5.json)
- Every report, and how one is made: [performance/reports/](performance/reports/README.md)
- The trend, release after release: [doc.yuneta.io/performance](https://doc.yuneta.io/performance/)
- The benchmarks: [performance/c/](performance/c/README.md)

## Build from source

To develop the framework itself, or to build it with different options
(TLS backend, modules, static/dynamic):

    git clone --recurse-submodules https://github.com/artgins/yunetas.git

[pypi-badge]: https://img.shields.io/pypi/v/yunetas
