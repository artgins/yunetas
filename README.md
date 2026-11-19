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
`Yuneta Simplified` is an _open source_ development framework for messaging and services. It uses the
[event-driven](https://en.wikipedia.org/wiki/Event-driven_programming),
[automata-based](https://en.wikipedia.org/wiki/Automata-based_programming)
and [object-oriented](https://en.wikipedia.org/wiki/Object-oriented_programming)
programming paradigms. It makes heavy use of time-series, key-value, flat-file and graph concepts.

It is a real-time system that covers development, testing and deployment.
It is built for [Linux](https://en.wikipedia.org/wiki/Linux), and you can deploy it on any bare-metal server.

It specializes in IoT data collection from any kind of device, and in data exchange and protocol adaptation
between systems. It gives publication/subscription and queries of messages in real time, with historical storage.

A message can be encrypted or in plain text. It can be kept on disk, or it can live only in transit or in the memory
of a service. All data is JSON.

There are versions in C (the reference implementation) and in JavaScript (browser/Node).
<!-- yuneta-intro-end -->

For more details, see [doc.yuneta.io](https://doc.yuneta.io).

## Install

> [!IMPORTANT]
> Yuneta needs Linux 5.19 or later, because its event loop runs on io_uring.
> RHEL, Rocky and Alma 9 also work. Red Hat backports io_uring into their 5.14,
> but it is disabled by default: set `kernel.io_uring_disabled=0`. RHEL / Rocky /
> Alma 8 (kernel 4.18) have no io_uring and cannot run Yuneta. Details:
> [Linux kernel](https://doc.yuneta.io/installation#kernel-required).

One command works on both distribution families: Debian/Ubuntu (`.deb`) and
RHEL/Rocky/Alma (`.rpm`). It installs the `yuneta-agent` package and the
developer toolchain. The package is the runtime: the agent, the CLI tools, a
bundled web server, and the libraries, headers and CMake toolchain under
`/yuneta/development/yunetas/`. With both, the machine can run yunos and build
your own projects:

    curl -fsSL https://raw.githubusercontent.com/artgins/yunetas/main/install.sh | sudo sh

To install only the `yuneta-agent` package, without the developer toolchain
(a pure deployment node), run:

    curl -fsSL https://raw.githubusercontent.com/artgins/yunetas/main/install.sh | sudo sh -s -- --runtime-only

Both forms also install certbot. To pin a published release and not the
latest, give its tag: `… | sudo sh -s -- 7.8.6`.

> [!WARNING]
> The packages *run* anywhere. They *build* only on one distribution each.
>
> | Package | Node that can build against it | glibc |
> |---------|--------------------------------|-------|
> | `.rpm` (EL9)   | **Rocky Linux / AlmaLinux 9** | 2.34 |
> | `.deb` (amd64) | **Debian 13** (trixie)        | 2.41 |
>
> The shipped binaries are fully static. Thus the agent, the CLI tools and your
> yunos run on any Linux of the same architecture with the kernel above. But the
> package also carries a sparse SDK: prebuilt static archives under `outputs/`.
> Those archives reference glibc internals, and the layout of those internals
> moves between releases. If you link your own code against them, the glibc of
> the node must match exactly.
>
> A mismatch does not fail the link. The link succeeds silently and corrupts the
> heap at run time. The result is a SIGABRT inside the glibc allocator seconds
> after start, with no Yuneta error logged first, and a stack that points at
> innocent code. A build guard stops this at configure time. Do not override it.
>
> Every other distribution is runtime-only: Ubuntu (24.04 is glibc 2.39, 26.04 is
> 2.43), Debian 12 (2.36). To develop there, build on a matching node and push
> the binaries with `yunetas sync-binaries`. Or build the framework from source
> (below) on that machine.

[Installation](https://doc.yuneta.io/installation/) tells what the script does
step by step, and gives the full list of verified distributions.

## See it running

The JavaScript version runs live in the browser, with no backend and no login:

- [demo.yuneta.io](https://demo.yuneta.io): the UI library itself. It shows every
  `C_YUI_NAV` layout and the responsive model of each zone. One JSON file
  declares them, and the shell builds them.

Two shipped apps run on the same shell (`C_YUI_SHELL` + `C_YUI_NAV`): a
music player and a video player for the files that are already on the phone or
the computer. You authorize a folder. The app reads it on the device, and it
copies or uploads nothing. You can install the apps, they work offline, and
they have ten languages. In all of them, every tap reaches a state machine as
an event.

## Performance

A release that changes a lot of code ships a performance report. The report
has charts of what the release does on one machine, and of the release before,
measured the same way. It gives every loss with its reason.

- 7.31.0 (against 7.30.0, the last release with a study, with 7.30.1 to
  7.30.26 inside it): the agent, the control center, the MQTT broker and gates,
  the transports and the inter-yuno events hardened against peers that lie,
  flood or do not read, and consoles with flow control. All 66 figures are inside
  their noise: no price.
  [Report](performance/reports/7.31.0.html) (the file) ·
  [rendered view](https://htmlpreview.github.io/?https://github.com/artgins/yunetas/blob/7.31.0/performance/reports/7.31.0.html) ·
  [raw figures](performance/reports/7.31.0.json)
- 7.30.0 (against 7.29.3): an HTTP parser in linear time with limits, a
  server that holds a pipeline, a BFF that serves a session only to its
  `Origin`, and a rewritten date reader. One header of 7.8 MB holds the event
  loop 603 times less (5.3 s -> 8.9 ms), and 64 KB is the limit by default.
  Prices, each with its reason: an HTTP round trip +0.7 us (-5.1%), a BFF login
  +6.8 us (-8.2%), one `stat()` per date text with `TZ` unset, and seven system
  calls per conversion to a named zone. Of the other 49 benchmarks, 39 stay
  inside their noise and 10 move by placement, five each way.
  [Report](performance/reports/7.30.0.html) (the file) ·
  [rendered view](https://htmlpreview.github.io/?https://github.com/artgins/yunetas/blob/7.30.0/performance/reports/7.30.0.html) ·
  [raw figures](performance/reports/7.30.0.json)
- 7.29.0 (against 7.28.8, the last release with a study, with 7.28.9 to
  7.28.11 inside it): retention that keeps what it takes, shipped to a backup
  by two new yunos, with the content of each file checked and mutual TLS. The
  new code is off the measured paths: 41 of 47 figures are inside their noise.
  The six that move (2-4%, both ways) are on code that did not change, and they
  moved as much when only its placement changed. No price.
  [Report](performance/reports/7.29.0.html) (the file) ·
  [rendered view](https://htmlpreview.github.io/?https://github.com/artgins/yunetas/blob/7.29.0/performance/reports/7.29.0.html) ·
  [raw figures](performance/reports/7.29.0.json)
- 7.28.0 (against 7.27.11, the last release with a study, with 7.27.12 to
  7.27.21 inside it): a performance round over the C SDK, and jansson as
  Yuneta's own JSON library. 1.9 times the appends per second (225,745 ->
  422,850), 2.5 times the records read (216,577 -> 549,780), a treedb opened 17
  times faster, inter-yuno events and their answers 2.1 times faster, TCP and
  TLS echoes +50-58%. 40 of 50 figures are faster outside their noise, none slower.
  [Report](performance/reports/7.28.0.html) (the file) ·
  [rendered view](https://htmlpreview.github.io/?https://github.com/artgins/yunetas/blob/7.28.0/performance/reports/7.28.0.html) ·
  [raw figures](performance/reports/7.28.0.json)
- 7.27.10 (against 7.27.7, the last release with a study. 7.27.8 and
  7.27.9 changed no measured code): a store that writes json text as it
  arrives. With `tranger2_append_record_text()` and `json_scan_object_members()`,
  a writer of raw history skips the parse and the dump of every frame.
  The db_events yuno of an application stores 2.2 times more frames per second
  (24,700 -> 53,300), and holds 20,000 records/s on a Xeon E-2386G at 55% of one
  core. Each of the 41 figures is inside its noise or faster. The study found
  one loss (-1.6% of the json append), and the release fixes it.
  [Report](performance/reports/7.27.10.html) (the file) ·
  [rendered view](https://htmlpreview.github.io/?https://github.com/artgins/yunetas/blob/7.27.10/performance/reports/7.27.10.html) ·
  [raw figures](performance/reports/7.27.10.json)
- 7.27.7 (against 7.27.5, the last release with a study, with 7.27.6 in the
  same rotation): three reviews of retention at no measurable cost. 40 of 41
  figures are inside their noise, the read path of a replica among them. There
  is one price, and it appeared in 7.27.6: a forced treedb delete takes 21% more
  CPU (43.6 -> 52.8 us). The reason is that a deleted key moves out of `keys/`
  whole before its removal. Thus a delete that fails half way never renumbers it.
  [Report](performance/reports/7.27.7.html) (the file) ·
  [rendered view](https://htmlpreview.github.io/?https://github.com/artgins/yunetas/blob/7.27.7/performance/reports/7.27.7.html) ·
  [raw figures](performance/reports/7.27.7.json)
- 7.27.5 (against 7.27.0, the last release with a study. It was measured after
  the release, and it covers 7.27.1 to 7.27.5): the retention of timeranger2 costs
  a replica about 1 us for each read call. A page of 1 row is 18% slower, a
  filtered one 22%, the open of an iterator 7%. The cause is the stat() that
  notices a purge whose signal the replica missed, placed in 7.27.5, commit
  83a771089. A page of 100 rows is 1.5% slower, a forced treedb delete 3.9%. The
  master and the appends stay inside their noise, and 7.27.1 to 7.27.4 cost
  nothing measurable.
  [Report](performance/reports/7.27.5.html) (the file) ·
  [rendered view](https://htmlpreview.github.io/?https://github.com/artgins/yunetas/blob/7.27.8/performance/reports/7.27.5.html) ·
  [raw figures](performance/reports/7.27.5.json)
- 7.27.0 (against 7.26.9): a hardening release that holds its speed.
  30 of 31 figures are inside their noise. There is one price, given with its
  cause: a treedb update in memory takes 6.5% more CPU (2.31 -> 2.46 us),
  because of the NUL checks of each write.
  [Report](performance/reports/7.27.0.html) (the file) ·
  [rendered view](https://htmlpreview.github.io/?https://github.com/artgins/yunetas/blob/7.27.0/performance/reports/7.27.0.html) ·
  [raw figures](performance/reports/7.27.0.json)
- 7.26.9 (against 7.26.0, the last release with a study. It was done again
  after the release, and it covers 7.26.1 to 7.26.9): a hardening release that
  made the treedb faster. A link or unlink is 12% faster, the delete of a parent
  10%, a reopen 9% (the names looked up by key). There are two prices, each
  placed in its release. The first open of 40 treedbs takes 6.7% longer (7.26.7,
  the `__icons__` topic). The appends of a store take 2-4% longer (7.26.9, the
  hardened kw calls).
  [Report](performance/reports/7.26.9.html) (the file) ·
  [rendered view](https://htmlpreview.github.io/?https://github.com/artgins/yunetas/blob/7.27.8/performance/reports/7.26.9.html) ·
  [raw figures](performance/reports/7.26.9.json)
- 7.26.0 (against 7.25.22): the read of a timeranger2 history is 16-17% faster,
  the open of a treedb 20% faster. A tm query of a topic that 7.25.22 did not
  mark is 31x faster. There is one price, given with its reason: a tm query of
  a topic that 7.25.22 marked goes from 7.5 to 12.8 ms, because the tm markers
  are gone.
  [Report](performance/reports/7.26.0.html) (the file) ·
  [rendered view](https://htmlpreview.github.io/?https://github.com/artgins/yunetas/blob/7.26.0/performance/reports/7.26.0.html) ·
  [raw figures](performance/reports/7.26.0.json)
- 7.25.22 (against 7.25.21): two reviews closed, the agents as systemd
  units, and a forced treedb delete 39% faster. Nothing is slower on a path that
  this release changed.
  [Report](performance/reports/7.25.22.html) (the file) ·
  [rendered view](https://htmlpreview.github.io/?https://github.com/artgins/yunetas/blob/7.25.22/performance/reports/7.25.22.html) ·
  [raw figures](performance/reports/7.25.22.json)
- 7.25.21 (against 7.25.20): secrets masked in every trace, dump and kw,
  secret gbuffers, a reworked TCP server lifecycle, and nothing slower beyond
  the spread of its rounds.
  [Report](performance/reports/7.25.21.html) (the file) ·
  [rendered view](https://htmlpreview.github.io/?https://github.com/artgins/yunetas/blob/7.25.21/performance/reports/7.25.21.html) ·
  [raw figures](performance/reports/7.25.21.json)
- 7.25.20 (against 7.25.5): treedb writes 16-25% faster and the start of
  40 treedbs two times as fast, from the `SWITCHS` fix of 7.25.7. Nothing is slower.
  [Report](performance/reports/7.25.20.html) (the file) ·
  [rendered view](https://htmlpreview.github.io/?https://github.com/artgins/yunetas/blob/7.25.20/performance/reports/7.25.20.html) ·
  [raw figures](performance/reports/7.25.20.json)
- 7.25.5 (against 7.25.4): an agent audit record 11x cheaper, a large
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
(TLS backend, modules, static/dynamic), clone the repository with its submodules:

    git clone --recurse-submodules https://github.com/artgins/yunetas.git

[pypi-badge]: https://img.shields.io/pypi/v/yunetas
