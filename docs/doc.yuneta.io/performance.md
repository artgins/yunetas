(performance-history)=
# Performance

Every Yuneta release is measured before it is tagged, and a release that moves
a lot of code ships a performance report. This page keeps the history: the
trend of the main figures, report after report, and a link to each report.

Two kinds of figures are kept:

- **A/B figures.** The same benchmark, built with the module of the new release
  and with the module of the release before, the two binaries run alternately
  on one machine. They say what the release changed. A change inside the spread
  of its rounds is noise, and it is called noise. A study compares the release
  with the last release that has a study, never with a tag nobody measured.
  When releases without a study sit between the two, the tags between them are
  built too, and a figure that got slower is measured on each of them until
  the release (and, where it is cheap, the commit) where it appeared is named.
- **Absolute figures.** The benchmarks run on the build of the release at tag
  time. They say what one machine does with the release: records stored per
  second, messages per second, what one operation costs.

A figure that got slower is never left out. It is shown with its reason, as the
CHANGELOG states it: most are the price of a guarantee (a file that survives a
power cut, a record that reaches the disk before a command runs), kept by
decision.

## Reports

| Release | Against | Report | Rendered | Raw figures |
|---------|---------|--------|----------|-------------|
| 7.29.3 | 7.29.2 | no page (a fix release) | - | [7.29.3.json](https://github.com/artgins/yunetas/blob/7.29.3/performance/reports/7.29.3.json) |
| 7.29.2 | 7.29.0 | no page (a fix release) | - | [7.29.2.json](https://github.com/artgins/yunetas/blob/7.29.3/performance/reports/7.29.2.json) |
| 7.29.0 | 7.28.8 | [7.29.0.html](https://github.com/artgins/yunetas/blob/7.29.0/performance/reports/7.29.0.html) | [view](https://htmlpreview.github.io/?https://github.com/artgins/yunetas/blob/7.29.0/performance/reports/7.29.0.html) | [7.29.0.json](https://github.com/artgins/yunetas/blob/7.29.0/performance/reports/7.29.0.json) |
| 7.28.8 | 7.28.5 | no page (a short study) | - | [7.28.8.json](https://github.com/artgins/yunetas/blob/7.28.8/performance/reports/7.28.8.json) |
| 7.28.5 | 7.28.4 | no page (a short study) | - | [7.28.5.json](https://github.com/artgins/yunetas/blob/7.28.5/performance/reports/7.28.5.json) |
| 7.28.4 | 7.28.2 | no page (a fix release) | - | [7.28.4.json](https://github.com/artgins/yunetas/blob/7.28.4/performance/reports/7.28.4.json) |
| 7.28.2 | 7.28.0 | no page (a fix release) | - | [7.28.2.json](https://github.com/artgins/yunetas/blob/7.28.2/performance/reports/7.28.2.json) |
| 7.28.0 | 7.27.11 | [7.28.0.html](https://github.com/artgins/yunetas/blob/7.28.0/performance/reports/7.28.0.html) | [view](https://htmlpreview.github.io/?https://github.com/artgins/yunetas/blob/7.28.0/performance/reports/7.28.0.html) | [7.28.0.json](https://github.com/artgins/yunetas/blob/7.28.0/performance/reports/7.28.0.json) |
| 7.27.11 | 7.27.10 | no page (a fix release) | - | [7.27.11.json](https://github.com/artgins/yunetas/blob/7.27.11/performance/reports/7.27.11.json) |
| 7.27.10 | 7.27.7 | [7.27.10.html](https://github.com/artgins/yunetas/blob/7.27.10/performance/reports/7.27.10.html) | [view](https://htmlpreview.github.io/?https://github.com/artgins/yunetas/blob/7.27.10/performance/reports/7.27.10.html) | [7.27.10.json](https://github.com/artgins/yunetas/blob/7.27.10/performance/reports/7.27.10.json) |
| 7.27.7 | 7.27.5 | [7.27.7.html](https://github.com/artgins/yunetas/blob/7.27.7/performance/reports/7.27.7.html) | [view](https://htmlpreview.github.io/?https://github.com/artgins/yunetas/blob/7.27.7/performance/reports/7.27.7.html) | [7.27.7.json](https://github.com/artgins/yunetas/blob/7.27.7/performance/reports/7.27.7.json) |
| 7.27.5 | 7.27.0 | [7.27.5.html](https://github.com/artgins/yunetas/blob/7.27.8/performance/reports/7.27.5.html) | [view](https://htmlpreview.github.io/?https://github.com/artgins/yunetas/blob/7.27.8/performance/reports/7.27.5.html) | [7.27.5.json](https://github.com/artgins/yunetas/blob/7.27.8/performance/reports/7.27.5.json) |
| 7.27.0 | 7.26.9 | [7.27.0.html](https://github.com/artgins/yunetas/blob/7.27.0/performance/reports/7.27.0.html) | [view](https://htmlpreview.github.io/?https://github.com/artgins/yunetas/blob/7.27.0/performance/reports/7.27.0.html) | [7.27.0.json](https://github.com/artgins/yunetas/blob/7.27.0/performance/reports/7.27.0.json) |
| 7.26.9 | 7.26.0 | [7.26.9.html](https://github.com/artgins/yunetas/blob/7.27.8/performance/reports/7.26.9.html) | [view](https://htmlpreview.github.io/?https://github.com/artgins/yunetas/blob/7.27.8/performance/reports/7.26.9.html) | [7.26.9.json](https://github.com/artgins/yunetas/blob/7.27.8/performance/reports/7.26.9.json) |
| 7.26.0 | 7.25.22 | [7.26.0.html](https://github.com/artgins/yunetas/blob/7.26.0/performance/reports/7.26.0.html) | [view](https://htmlpreview.github.io/?https://github.com/artgins/yunetas/blob/7.26.0/performance/reports/7.26.0.html) | [7.26.0.json](https://github.com/artgins/yunetas/blob/7.26.0/performance/reports/7.26.0.json) |
| 7.25.22 | 7.25.21 | [7.25.22.html](https://github.com/artgins/yunetas/blob/7.25.22/performance/reports/7.25.22.html) | [view](https://htmlpreview.github.io/?https://github.com/artgins/yunetas/blob/7.25.22/performance/reports/7.25.22.html) | [7.25.22.json](https://github.com/artgins/yunetas/blob/7.25.22/performance/reports/7.25.22.json) |
| 7.25.21 | 7.25.20 | [7.25.21.html](https://github.com/artgins/yunetas/blob/7.25.21/performance/reports/7.25.21.html) | [view](https://htmlpreview.github.io/?https://github.com/artgins/yunetas/blob/7.25.21/performance/reports/7.25.21.html) | [7.25.21.json](https://github.com/artgins/yunetas/blob/7.25.21/performance/reports/7.25.21.json) |
| 7.25.20 | 7.25.5 | [7.25.20.html](https://github.com/artgins/yunetas/blob/7.25.20/performance/reports/7.25.20.html) | [view](https://htmlpreview.github.io/?https://github.com/artgins/yunetas/blob/7.25.20/performance/reports/7.25.20.html) | [7.25.20.json](https://github.com/artgins/yunetas/blob/7.25.20/performance/reports/7.25.20.json) |
| 7.25.5 | 7.25.4 | [7.25.5.html](https://github.com/artgins/yunetas/blob/7.25.5/performance/reports/7.25.5.html) | [view](https://htmlpreview.github.io/?https://github.com/artgins/yunetas/blob/7.25.5/performance/reports/7.25.5.html) | [7.25.5.json](https://github.com/artgins/yunetas/blob/7.25.5/performance/reports/7.25.5.json) |

The report is one self-contained page (no script, no external file). GitHub
shows it as source code; the "view" link renders it. How a report is made, and
the schema of its `.json`, are in
[`performance/reports/README.md`](https://github.com/artgins/yunetas/blob/7.29.3/performance/reports/README.md).

## The trend

```{figure} ./_static/perf/perf_trend.svg
:alt: Ten small charts, one per figure, each from 7.25.4 through 7.25.5, 7.25.20, 7.25.21, 7.25.22, 7.26.0, 7.26.9, 7.27.0, 7.27.5, 7.27.7, 7.27.10, 7.27.11, 7.28.0, 7.28.2 and 7.28.4: appends per second 220,120, 221,588, 226,720, 225,883, 224,185, 223,864, 223,770, 229,502, 226,311, 232,605, 225,186, 228,739, 422,850, 434,385, 422,318; open of a store as master 92.5, 80.8, 78.2, 81.9, 79.6, 81.9, 79.9, 82.2, 81.1, 78.8, 79.3, 79.0, 77.4, 76.5, 81.8 ms; a tm query of one minute 12.7, 7.4, 7.35, 7.66, 7.47, 12.8, 12.5, 12.8, 13.2, 12.9, 13.2, 12.9, 12.9, 13.0, 13.4 ms (from 7.26.0 every row is read: no tm markers); a treedb update in memory 3.92, 2.92, 2.32, 2.32, 2.41, 2.49, 2.29, 2.46, 2.36, 2.36, 2.37, 2.43, 1.14, 1.14, 1.22 us; a saved treedb update 11.75, 10.81, 7.66, 7.77, 7.75, 7.80, 7.48, 7.84, 7.62, 7.59, 7.70, 7.90, 4.05, 4.07, 4.25 us; link or unlink 11.27, 11.09, 8.65, 8.71, 8.63, 8.65, 7.54, 7.76, 7.48, 7.38, 7.58, 7.94, 4.59, 4.61, 4.81 us; the open of 40 treedbs with an unchanged schema 1.84, 0.649, 0.318, 0.340, 0.333, 0.335, 0.359, 0.383, 0.374, 0.331, 0.344, 0.357, 0.256, 0.253, 0.325 s; one agent audit record 6194, 554, 572, 593, 568, 570, 579, 578, 573, 572, 559, 564, 564, 558, 603 ns; the event loop echo 152.2, 150.5, 150.1, 147.1, 150.5, 149.0, 146.5, 144.9, 143.1, 148.3, 145.3, 147.1, 151.4, 147.6, 143.2 K messages per second; the gobj TCP echo 38,706, 39,344, 38,191, 39,715, 38,292, 39,511, 39,034, 38,744, 38,301, 39,107, 38,877, 39,035, 63,478, 62,915, 58,567 round trips per second. 7.28.5, a short study, adds a point only to the panels of the treedb: 1.16 us in memory, 4.10 us saved, 4.70 us link or unlink, and 0.266 s for the open of 40 treedbs. 7.29.0 adds a point to every panel but the event loop echo: 430,866 appends per second, 78.5 ms for the master open, 12.9 ms for the tm query, 1.18 us in memory, 4.19 us saved, 4.59 us link or unlink, 0.267 s for the open of 40 treedbs, 567 ns for the audit record and 61,798 TCP round trips per second. 7.29.2 adds a point to the same panels: 429,111 appends per second, 76.0 ms for the master open, 12.9 ms for the tm query, 1.17 us in memory, 4.27 us saved, 4.72 us link or unlink, 0.272 s for the open of 40 treedbs, 571 ns for the audit record and 60,964 TCP round trips per second. 7.29.3 adds a point to the same panels: 419,306 appends per second, 77.1 ms for the master open, 12.6 ms for the tm query, 1.18 us in memory, 4.36 us saved, 4.78 us link or unlink, 0.269 s for the open of 40 treedbs, 577 ns for the audit record and 61,740 TCP round trips per second.
:width: 100%

The main A/B figures, one panel each, on their own scale from zero. The first
report (7.25.5) gives two points: 7.25.4, its baseline, and 7.25.5. Each new
report adds one point.
```

The C_TREEDB open of 7.25.5 compares two separate runs: 7.25.4 (medians of an
earlier run) and 7.25.5 as shipped (the tag-time run). Every other point of the
table comes from one alternated A/B run. Each point is the figure its own
report measured, so two neighbouring points come from two runs, days apart:
the A/B of 7.25.20 measured 7.25.5 again, the A/B of 7.25.21 measured
7.25.20 again, the A/B of 7.25.22 measured 7.25.21 again, the A/B of 7.26.0 measured 7.25.22 again, the A/B of 7.26.9 measured 7.26.0 again (7.26.1 to 7.26.8 shipped no report; it was redone after the release), the A/B of 7.27.0 measured 7.26.9 again, the A/B of 7.27.5 measured 7.27.0 again (7.27.1 to 7.27.4 shipped no report; 7.27.5's was written the day after the release), the A/B of 7.27.7 measured 7.27.5 again, with 7.27.6 (no report) in the same rotation, the A/B of 7.27.10 measured 7.27.7 again (7.27.8 and 7.27.9 changed no measured code), the A/B of 7.27.11 measured 7.27.10 again, the A/B of 7.28.0 measured 7.27.11 again (7.27.12 to 7.27.21, lite releases of the agent and the controlcenter, are inside it), the A/B of 7.28.2 measured 7.28.0 again (7.28.1, the agents' pid file, is inside it), the A/B of 7.28.4 measured 7.28.2 again (7.28.3, the stop of the agent, is inside it), the short A/B of 7.28.5 measured 7.28.4 again, only the figures of the treedb and of the inter-yuno events, and the short A/B of 7.28.8 measured 7.28.5 again, only the inter-yuno events (7.28.6 and 7.28.7 changed no measured code), the A/B of 7.29.0 measured 7.28.8 again (7.28.9 to 7.28.11 shipped no study and are inside it), the A/B of 7.29.2 measured 7.29.0 again (7.29.1 changed no C of the libraries or yunos, only a comment in `helpers.c` and a new test), and the A/B of 7.29.3 measured 7.29.2 again; their figures are in the sections below. Between two such
reports a figure can move by a few percent with no change of the code (the
master open of 7.25.20 measured 78.2 ms in its own report and 82.4 ms in the
A/B of 7.25.21, another day): read a trend against the A/B of its report.

| Figure | Unit | 7.25.4 | 7.25.5 | 7.25.20 | 7.25.21 | 7.25.22 | 7.26.0 | 7.26.9 | 7.27.0 | 7.27.5 | 7.27.7 | 7.27.10 | 7.27.11 | 7.28.0 | 7.28.2 | 7.28.4 | 7.28.5 | 7.29.0 | 7.29.2 | 7.29.3 |
|--------|------|--------|--------|---------|---------|---------|--------|--------|--------|--------|--------|---------|---------|--------|--------|--------|--------|--------|--------|--------|
| Appends per second (`test_topic_pkey_integer`, 4 link layouts) | appends/s | 220,120 | 221,588 | 226,720 | 225,883 | 224,185 | 223,864 | 223,770 | 229,502 | 226,311 | 232,605 | 225,186 | 228,739 | 422,850 | 434,385 | 422,318 | - | 430,866 | 429,111 | 419,306 |
| Open a store as master, 20 000 md2 files (`perf_timeranger2`) | ms | 92.5 | 80.8 | 78.2 | 81.9 | 79.6 | 81.9 | 79.9 | 82.2 | 81.1 | 78.8 | 79.3 | 79.0 | 77.4 | 76.5 | 81.8 | - | 78.5 | 76.0 | 77.1 |
| A tm query of one minute of a key | ms | 12.7 | 7.4 (migrated) | 7.35 | 7.66 | 7.47 | 12.8 (every row) | 12.5 | 12.8 | 13.2 | 12.9 | 13.2 | 12.9 | 12.9 | 13.0 | 13.4 | - | 12.9 | 12.9 | 12.6 |
| treedb: update a node in memory (`perf_tr_treedb`) | us | 3.92 | 2.92 | 2.32 | 2.32 | 2.41 | 2.49 | 2.29 | 2.46 | 2.36 | 2.36 | 2.37 | 2.43 | 1.14 | 1.14 | 1.22 | 1.16 | 1.18 | 1.17 | 1.18 |
| treedb: update a node and save it | us | 11.75 | 10.81 | 7.66 | 7.77 | 7.75 | 7.80 | 7.48 | 7.84 | 7.62 | 7.59 | 7.70 | 7.90 | 4.05 | 4.07 | 4.25 | 4.10 | 4.19 | 4.27 | 4.36 |
| treedb: link or unlink two nodes | us | 11.27 | 11.09 | 8.65 | 8.71 | 8.63 | 8.65 | 7.54 | 7.76 | 7.48 | 7.38 | 7.58 | 7.94 | 4.59 | 4.61 | 4.81 | 4.70 | 4.59 | 4.72 | 4.78 |
| C_TREEDB: open 40 treedbs whose schema did not change (`perf_c_treedb`) | s | 1.84 | 0.649 | 0.318 | 0.340 | 0.333 | 0.335 | 0.359 | 0.383 | 0.374 | 0.331 | 0.344 | 0.357 | 0.256 | 0.253 | 0.325 | 0.266 | 0.267 | 0.272 | 0.269 |
| One agent audit record of 300 bytes (`perf_rotatory`) | ns | 6,194 | 554 | 572 | 593 | 568 | 570 | 579 | 578 | 573 | 572 | 559 | 564 | 564 | 558 | 603 | - | 567 | 571 | 577 |
| Event loop echo, 1 KB messages (`perf_yev_ping_pong`) | K msg/s | 152.2 | 150.5 | 150.1 | 147.1 | 150.5 | 149.0 | 146.5 | 144.9 | 143.1 | 148.3 | 145.3 | 147.1 | 151.4 | 147.6 | 143.2 | - | - | - | - |
| TCP echo through the gobj stack (`perf_tcp_test4`) | round trips/s | 38,706 | 39,344 | 38,191 | 39,715 | 38,292 | 39,511 | 39,034 | 38,744 | 38,301 | 39,107 | 38,877 | 39,035 | 63,478 | 62,915 | 58,567 | - | 61,798 | 60,964 | 61,740 |

A `-` in the 7.28.5 column is a figure its short study did not measure: no code
of 7.28.5 is on its path, and its last measure is the column of 7.28.4. The
event loop echo of 7.29.0, 7.29.2 and 7.29.3 is not in the A/B for the same
reason: no code of 7.28.9 to 7.29.3 is on its path (the tag-time runs of 7.29.2
and 7.29.3 measured 147.1 and 147.4 K messages per second).

The appends of 7.25.20, 7.25.21, 7.25.22, 7.26.0, 7.26.9, 7.27.0, 7.27.5, 7.27.7, 7.27.10, 7.27.11, 7.28.0, 7.28.2 and 7.28.4 are measured on whole-release builds (see
below), not on the four padded links of 7.25.5. The tm query of 7.25.5 to
7.25.22 is the one of a topic its release had marked; 7.26.0 has no tm markers
and reads every row of the key (a topic 7.25.22 had not marked took 402 ms).

## What each figure means

| Figure | What it tells a user |
|--------|----------------------|
| Appends per second | How many records one yuno stores per second in a key-indexed timeranger2 topic. It is the write rate of every history, queue and log that Yuneta keeps on disk. |
| Open of a store | The start-up time of a yuno that owns a store (master) or follows one that another yuno writes (replica). |
| tm query | A read of a time range of a key: what a dashboard or a report pays to load one minute of data. |
| Iterator open, page (master, replica) | What a client of a store pays to start reading a key and to pull one page of it, on the yuno that owns the store (master) and on one that follows it (replica). From 7.27.5 on (`perf_timeranger2 read`). |
| treedb update, link, create, delete | The cost of one change in the graph database: a node changed in memory, a node changed and saved, a relation made or undone, a node created or removed. The CPU time of the process, per operation. |
| C_TREEDB open | The start-up of a yuno with many treedbs. With an unchanged schema it is the everyday case; the first open (seed) and the open with a newer schema are one-time costs. |
| Agent audit record | What every command sent to the agent pays to be audited. |
| Log record | What a yuno pays for one line of its log file. |
| Event loop echo | The ceiling of the io_uring event loop alone: 1 KB messages echoed per second, no gobj layer. |
| gobj TCP and TLS echo | Round trips of a JSON message through the whole stack (`C_IOGATE`, `C_TCP_S`, `C_PROT_TCP4H`, `C_CHANNEL`), plain and encrypted, with and without a timeranger2 append per message. |
| treedb reads | What a `list-nodes` or a `get-node` pays: the open of a treedb of 8 000 users, the list of a topic, the view of a node with its links collapsed into references, and its json text. From 7.28.0 on (`perf_tr_treedb_read`). |
| Inter-yuno events | An event sent to a service of another yuno and its answer published back, over a websocket: what every command, every subscription between yunos and every SPA message pays. From 7.28.0 on (`perf_ievent`). |
| MQTT QoS 0 | A publication through the embedded broker and its delivery back to the client that subscribes the topic. From 7.28.0 on (`perf_mqtt`). |
| Publish | `gobj_publish_event()`: one event delivered to N subscribers of one gobj, the in-process message bus. |
| Binary size | Each yuno is one fully static executable. The size includes OpenSSL and the whole framework. |

## 7.29.3 against 7.29.2

7.29.3 is the fixes of a review round and a Modbus master that answers every
request. In C: `build_path()` of gobj-c returns NULL and logs when the path
does not fit (every path of timeranger2 is assembled with it), the date
reader of gobj-c reads again four more forms, the ids of the lists of a treedb take
a long yuno name whole (`list_id_yuno()` of `tr_treedb`), and the archive of
retention (`archive_purged_files()`) and the old files of the rotatory check
the paths they build. The rest is in modules and yunos no benchmark runs
(Modbus, M-Bus, the agent, the controlcenter, `archive_keeper`, the cloners).
Of the library objects, only `helpers.c`, `rotatory.c`, `timeranger2.c` and
`tr_treedb.c` have other machine code. Measured against 7.29.2, the last
release with a study, before the tag. The same 47 figures as 7.29.2, 8
rounds, its figures here and in its `.json`, with no report page of its own.

```{figure} ./_static/perf/perf_change_7.29.3.svg
:alt: The change of the time one operation takes, 7.29.3 against 7.29.2, for 47 figures. No price: 46 inside their noise; the appends with a live reader, -3.7% of appends, are outside it in the rotation and inside it when measured again, on code that did not change.
:width: 100%

The change of the time one operation takes, left of zero is faster. Grey is
within the noise of its rounds.
```

```{figure} ./_static/perf/perf_throughput_7.29.3.svg
:alt: Operations per second on one core in 7.29.3: 419,306 appends, 339,214 appends with a live reader, 569,927 records read, 409,285 records read page by page, 147,385 event loop echoes, 97,446 with an append, 61,740 TCP echoes, 46,990 with an append, 45,034 TLS echoes, 36,376 with an append, 13,053 BFF logins, 28,474 inter-yuno event round trips and 36,338 MQTT QoS 0 round trips.
:width: 100%

Operations per second on one core, as each benchmark counts them.
```

No price. 46 of the 47 figures stay inside their noise, and no round is left
out. One figure moved outside its noise, on code that did not change:

| Figure | 7.29.2 | 7.29.3 | Change | Why |
|--------|--------|--------|--------|-----|
| Appends with a live reader (`test_topic_pkey_integer`) | 352,364 | 339,214 | -3.7% (t = -4.7) | Measured again: -1.82% (t = -2.3) with 7.29.2 and 7.29.3 alternated, 10 rounds, and -2.47% (t = -3.6) in a rotation of five builds, 10 rounds. The 7.29.3 binary relinked with the libraries of 7.29.2: +0.94%. Relinked with only one of them: +0.57% (gobj-c), -1.52% (timeranger2). Placement. |

The append path (`tranger2_append_record()`, `append_content()`, the
in-memory realtime lists) has the same machine code in both releases. Of
`timeranger2.c` only `purge_key_files()` changed, where
`archive_purged_files()` is inlined. Of `helpers.c` only `build_path()` (+52
instructions), the date reader and two functions no append runs changed. In
a run of the benchmark (360,000 appends) `build_path()` is called 82 times,
at the opens, and the date reader never. In the binary, 3,003 of the 3,237
functions moved, most by 1,536 or 64 bytes, and 21 of them, none on the
append path, to another 64-byte alignment. The appends without a reader, in
the same binary, measured -2.37% (t = -2.6), inside their noise; measured
again, -0.74% and -0.89%, and +0.20% relinked with the libraries of 7.29.2.
The TCP echoes, slower by about 2% in 7.29.2 by placement, are 1.5% faster
here (t = 2.2 and 2.7), on the same machine code. The code is the same, so
the cause is where the code is placed, not what it does. The yunos are 0 to
4 KB larger (stripped).

## 7.29.2 against 7.29.0

7.29.2 is the fixes of a review round. In C: the permission check of
`C_AUTHZ` no longer takes roles from the client's kw, the date reader of
gobj-c reads again the forms it read before 7.28.5, a text append that a
realtime list cannot parse still reaches the lists after it
(`append_content()` of timeranger2), a treedb whose name is too long for the
ids of its lists is refused at the open, the websocket reads a short
`getrandom()` to its end, and `publish-stats` refuses a service name that no
service can have. The rest is in yunos and tools no benchmark runs (the
agent, the controlcenter, `archive_keeper`, M-Bus, the cloners). Measured
against 7.29.0, the last release with a study, before the tag; 7.29.1
changed no C of the libraries or yunos (a comment in `helpers.c` and a new
test). The same 47 figures as 7.29.0, 8 rounds, its figures here and in its
`.json`, with no report page of its own.

```{figure} ./_static/perf/perf_change_7.29.2.svg
:alt: The change of the time one operation takes, 7.29.2 against 7.29.0, for 47 figures. No price: 45 inside their noise; the TCP echo -1.5% and the TCP echo with an append -2.7% of round trips are outside it in the rotation and inside it when measured again, on code that did not change.
:width: 100%

The change of the time one operation takes, left of zero is faster. Grey is
within the noise of its rounds.
```

```{figure} ./_static/perf/perf_throughput_7.29.2.svg
:alt: Operations per second on one core in 7.29.2: 429,111 appends, 350,966 appends with a live reader, 563,751 records read, 403,975 records read page by page, 147,096 event loop echoes, 96,903 with an append, 60,964 TCP echoes, 46,171 with an append, 44,387 TLS echoes, 36,002 with an append, 12,740 BFF logins, 27,836 inter-yuno event round trips and 36,246 MQTT QoS 0 round trips.
:width: 100%

Operations per second on one core, as each benchmark counts them.
```

No price. 45 of the 47 figures stay inside their noise. Round 3 of the
rotation was slow on both builds at once (the appends 35% slower on both),
so it is left out and a ninth round takes its place. Two figures moved
outside their noise, both on code that did not change:

| Figure | 7.29.0 | 7.29.2 | Change | Why |
|--------|--------|--------|--------|-----|
| TCP echo (`perf_tcp_test4`) | 61,893 | 60,964 | -1.5% (t = -3.7) | Measured again: -1.69% (t = -2.0) with 7.29.0 and 7.29.2 alternated, 10 rounds, and -2.11% (t = -2.3) in a rotation of six builds, 10 rounds. The 7.29.2 binary relinked with the libraries of 7.29.0: +0.24%. Relinked with only one of them: -1.28% (root-linux), -2.50% (gobj-c), -1.37% (timeranger2). Placement. |
| TCP echo with an append (`perf_tcp_test5`) | 47,452 | 46,171 | -2.7% (t = -6.5) | Measured again: -0.98% (t = -0.5) and -2.82% (t = -2.4). Relinked with the libraries of 7.29.0: +0.36%. With only one of them: -2.51% (root-linux), -0.61% (gobj-c), +0.07% (timeranger2). Placement. |

The objects on the path of the echoes (`C_TCP`, `C_PROT_TCP4H`,
`C_CHANNEL`, `C_IOGATE`, `gobj.c`, `yev_loop`, `ytls`) have the same machine
code in both releases. Only the six objects that 7.29.2 changes differ, and
the one function of them on a measured path, `append_content()`, is 23
instructions shorter. The appends benchmark that runs its loop measured
-0.09% (t = -0.1). In the binary of the echo, 17,672 of the 17,880 functions
moved by about 700 bytes, and 412 of them to another 64-byte alignment. The
TLS echo, -2.68% (t = -2.8), stays inside its noise and moves the same way:
-2.18% and -1.24% when measured again, +0.79% relinked with the libraries of
7.29.0. Over the three rotations (28 rounds) the echoes are about 2% slower.
The code is the same, so the cause is where the code is placed, not what it
does. The yunos are 0 to 8 KB larger (stripped).

## 7.29.0 against 7.28.8

7.29.0 lets retention keep what it takes (`tranger2_archive_key_files()`), adds
the yunos that ship the archive to a backup (`archive_sender`,
`archive_keeper`), an incremental sha256 to gobj-c, and the certificate of the
peer to ytls and `C_TCP` (mutual TLS). Measured against 7.28.8, the last
release with a study: 7.28.9 to 7.28.11 shipped none and are inside the
comparison (they changed the date reading of gobj-c, `C_WEBSOCKET` and
`C_YUNO`). Every benchmark whose binary links that code ran, 47 figures, 8
rounds; the raw event loop and MQTT run none of it. The report:
[7.29.0.html](https://github.com/artgins/yunetas/blob/7.29.0/performance/reports/7.29.0.html)
([view](https://htmlpreview.github.io/?https://github.com/artgins/yunetas/blob/7.29.0/performance/reports/7.29.0.html)).

```{figure} ./_static/perf/perf_change_7.29.0.svg
:alt: The change of the time one operation takes, 7.29.0 against 7.28.8, for 47 figures; the largest moves are the treedb save +4.3% of time, the TLS echo -3.1% and the TCP echo -2.7% of round trips, and the read with an iterator +3.0%, all on code that did not change.
:width: 100%

The change of the time one operation takes, left of zero is faster. Grey is
within the noise of its rounds.
```

```{figure} ./_static/perf/perf_throughput_7.29.0.svg
:alt: Operations per second on one core in 7.29.0: 430,866 appends, 354,461 appends with a live reader, 573,331 records read, 409,551 records read page by page, 61,798 TCP echoes, 47,606 with an append, 45,296 TLS echoes, 36,371 with an append, 12,908 BFF logins, 28,015 inter-yuno event round trips.
:width: 100%

Operations per second on one core, as each benchmark counts them.
```

No price. 41 of the 47 figures stay inside their noise. Six moved outside it,
all on code that did not change in the measured path:

| Figure | 7.28.8 | 7.29.0 | Change | Why |
|--------|--------|--------|--------|-----|
| TCP echo (`perf_tcp_test4`) | 63,506 | 61,798 | -2.7% (t = -4.3) | The TCP path in the clear is the same code (`C_TCP` changed only after a TLS handshake). The 7.29.0 binary relinked with the libraries of 7.28.11: 63,373 against 62,190 (10 rounds); with only the root-linux of 7.28.11, whose code on that path is the same: 61,505. Placement. |
| TLS echo (`perf_tcps_test4`) | 46,759 | 45,296 | -3.1% (t = -3.6) | Five builds in one rotation (10 rounds): 45,875 in 7.28.8, 45,910 in 7.28.9, 45,353 in 7.28.10, 45,434 in 7.28.11, 44,785 in 7.29.0: steps of about 1%, none outside its noise. The TLS echo that also appends every message, the same TLS code, measured +1.3%. Placement. |
| treedb: update a node and save it | 4.014 us | 4.186 us | +4.3% (t = 5.5) | `tr_treedb.c` and the append of timeranger2 are the same code. Relinked with the libraries of 7.28.11: 4.112 against 4.147 us (+0.85%); with only the timeranger2 of 7.28.11: 4.205 us, slower. Placement. |
| Appends with a live reader | 342,402 | 354,461 | +3.5% (t = 3.8) | The same append code. Not claimed. |
| Records read with an iterator | 556,692 | 573,331 | +3.0% (t = 3.5) | The same read code. Not claimed. |
| 400 000 appends into 20 000 files (`build_appends`) | 1,301 ms | 1,274 ms | -2.1% (t = -3.4) | The same append code. Not claimed. |

The yunos are 0.2% larger (stripped): the new code, off the measured paths.

## 7.28.8 against 7.28.5

7.28.8 makes `json2gbuf()` say when it cannot dump a json whole (it now looks
at the result of the dump), stops msg2db freeing the defaults of its schema,
and indents the machine trace four spaces per level, formatting its line a
third faster. Of that, only `json2gbuf()` is on the path of a benchmark: it
dumps every message between yunos, which `perf_ievent` runs. The trace runs
only with a trace on, and msg2db has no benchmark. So this is a short study:
`perf_ievent` alone, against 7.28.5, the last release with a study (7.28.6 and
7.28.7 changed the controlcenter and the scaffolding tools, which no
benchmark runs), before the tag. 8 rounds, its figure here and in its
`.json`, with no report page of its own.

```{figure} ./_static/perf/perf_change_7.28.8.svg
:alt: The change of the time one inter-yuno round trip takes, 7.28.8 against 7.28.5: within its noise, -1.24% round trips per second (t = -1.9).
:width: 100%

The change of the time one operation takes, left of zero is faster. Grey is
within the noise of its rounds.
```

No price. The inter-yuno round trips measured 27,537 per second against
27,881, -1.24% (t = -1.9), inside the noise of their rounds.

## 7.28.5 against 7.28.4

7.28.5 writes again the code of others that the MIT license of the SDK cannot
carry: the date reader of `helpers.c`, the string switch of `helpers.h`
(`SWITCHS`), `run_process2()`, `copyfile()`, `replace_string()` and the SHA-1
and base64 of the websocket handshake. Two of them are on the path of a
benchmark: `SWITCHS`, which `tr_treedb` runs, and the handshake, which an
inter-yuno connection runs once. So this is a short study: only the
benchmarks of the treedb (`perf_tr_treedb`, `perf_tr_treedb_read`,
`perf_c_treedb`) and of the inter-yuno events (`perf_ievent`), 18 figures,
against 7.28.4, the last release with a study, before the tag. 8 rounds, its
figures here and in its `.json`, with no report page of its own. The other
figures run no changed code; their last measure is the study of 7.28.4.

```{figure} ./_static/perf/perf_change_7.28.5.svg
:alt: The change of the time one operation takes, 7.28.5 against 7.28.4, one bar per figure, for the 18 figures of the treedb and of the inter-yuno events. No price and no gain: every one within its noise; the largest change is the link or unlink of two nodes, +1.67% (t = 2.4).
:width: 100%

One bar per figure: the change of the time one operation takes, left of zero
is faster. Grey is within the noise of its rounds.
```

No price. Every one of the 18 figures stays inside its noise: the largest
change is the link or unlink of two nodes, +1.67% (t = 2.4), and the
inter-yuno round trips measured +0.28% (t = 0.3).

## 7.28.4 against 7.28.2

7.28.4 changes three things in C. A queue gate (`C_QIOGATE`) refuses a message
its queue cannot save and goes on, instead of aborting its yuno: it checks the
free space of its disk once a second. Every yuno reads its memory and the free
space of its disks once a second, for its stats. And the controlcenter sends
the steps of a stop, a pause or a report at once. None of them is on the path
of a benchmark. It is measured against 7.28.2, the last release with a study,
before the tag; 7.28.3 (the stop of the agent) is inside it. A fix release: 8
rounds, 12 for the appends and the reads, its figures here and in its
`.json`, with no report page of its own.

```{figure} ./_static/perf/perf_change_7.28.4.svg
:alt: The change of the time one operation takes, 7.28.4 against 7.28.2, one bar per figure. No price and no gain: every one of the 50 figures within its noise. The TLS echo measured +1.87% round trips (t = 3.5) in the rotation and +0.99% (t = 1.0) in a controlled re-run of 10 rounds.
:width: 100%

One bar per figure: the change of the time one operation takes, left of zero
is faster. Grey is within the noise of its rounds.
```

No price. Every one of the 50 figures stays inside its noise. One crossed it
in the rotation of the study: the TLS echo, +1.87% round trips (t = 3.5, 8
rounds). Run again alone, 7.28.2 and 7.28.4 alternated for 10 rounds, it
measured +0.99% (t = 1.0); the changed code is not on its path. The yunos are
4 KB bigger stripped, the controlcenter 12 KB.

This run measured 7.28.2 again, and lower than its own report: 419,617 appends
per second against 434,385, the TCP echo 57,933 round trips per second against
62,915, the open of 40 treedbs 0.322 s against 0.253 s. The code is the same,
so it is the machine on that day. That is why the trend above has a dip at
7.28.4: read the change of a release against the A/B of its own report, where
both sides ran on the same day.

## 7.28.2 against 7.28.0

7.28.2 makes every yuno read its signals again when that read ends while the
yuno runs: up to 7.28.1 it went deaf to SIGQUIT and the rest, and only a
SIGKILL ended it (`read_signals_again()` in `c_yuno.c`). The change runs once
per signal. It is measured against 7.28.0, the last release with a study,
before the tag; 7.28.1 (the agents' pid file) is inside it. A fix release: a
smaller study (8 rounds, 12 for the appends and the reads), its figures here
and in its `.json`, with no report page of its own.

```{figure} ./_static/perf/perf_change_7.28.2.svg
:alt: The change of the time one operation takes, 7.28.2 against 7.28.0, one bar per figure. No price and no gain: every one of the 50 figures within its noise. The TCP echo with an append measured -0.94% (t = -5.3) in the rotation and +0.64% (t = 0.6) in a controlled re-run of 10 rounds.
:width: 100%

One bar per figure: the change of the time one operation takes, left of zero
is faster. Grey is within the noise of its rounds.
```

No price. Every one of the 50 figures stays inside its noise. One crossed it
in the rotation of the study: the TCP echo with an append per message,
-0.94% (t = -5.3, 8 rounds). Run again alone, 7.28.0 and 7.28.2 alternated
for 10 rounds, it measured +0.64% (t = 0.6); the changed code is not on its
path. The yunos are the same size (stripped: +0 or +4 KB).

## 7.28.0 against 7.27.11

7.28.0 is a performance round over the whole C SDK, taken from an audit
and reviewed commit by commit, and it makes jansson Yuneta's own JSON library
(`github.com/artgins/jansson`, tag `2.15.1-yuneta.1`). It is measured against
7.27.11, the last release with a study, before the tag. The releases between
them (7.27.12 to 7.27.21, lite releases of the agent and the controlcenter)
changed only `c_authz.c` in the kernel and are inside the same comparison; no
figure got slower, so no tag between them had to be measured on its own.
Each release is built with its own jansson: 7.27.11 with upstream v2.15.1 and
the old patch (atomic reference counts), 7.28.0 with the fork, built with
`JANSSON_THREAD_SAFE_REFCOUNT=OFF`. The report, with its charts:
[7.28.0.html](https://htmlpreview.github.io/?https://github.com/artgins/yunetas/blob/7.28.0/performance/reports/7.28.0.html).

```{figure} ./_static/perf/perf_change_7.28.0.svg
:alt: The change of the time one operation takes, 7.28.0 against 7.27.11, one bar per figure. No price. 40 of 50 figures faster outside their noise: appends per second 1.9 times (225,745 to 422,850), records read per second 2.5 times (216,577 to 549,780), the reopen of a treedb 17 times (277.7 to 16.6 us per node), a treedb update saved -45%, link or unlink -37%, the TCP echo -37% and the TLS echo -35% of time, inter-yuno event round trips 2.1 times (13,226 to 27,726 per second), MQTT QoS 0 -45%, the BFF logins -34%. Within their noise: the opens of a store, the tm query, the creation of topics, the change of a topic_version, the event loop alone, one treedb list with a filter, and the rotatory (the same code, moved).
:width: 100%

One bar per figure: the change of the time one operation takes, left of zero
is faster. Blue is faster outside the noise of its rounds, grey within it.
```

```{figure} ./_static/perf/perf_throughput_7.28.0.svg
:alt: Operations per second on one core in 7.28.0: 422,850 appends, 344,421 appends with a live reader, 549,780 records read, 406,169 records read page by page, 151,437 event loop echoes, 102,548 echoes with an append, 63,478 TCP echoes, 48,648 with an append, 46,810 TLS echoes, 36,937 with an append, 13,170 BFF logins, 27,726 inter-yuno event round trips, 35,770 MQTT QoS 0 round trips.
:width: 100%

Operations per second on one core, as each benchmark counts them.
```

No price. 40 of the 50 figures are faster outside their noise; the other ten
stay inside it. The event loop alone (`perf_yev_ping_pong`: io_uring, no gobj,
no json) is where it was, +0.5% (t = 1.2): the gain is in the layers above it.

**Where the gain comes from.** A third build ran in the same rotation: the C
code of 7.28.0 with the jansson of 7.27.11 (8 rounds, 12 for the appends and
the reads). What it leaves of a gain is jansson's:

| Figure | 7.27.11 | 7.28.0 code, old jansson | 7.28.0 | jansson's share |
|--------|---------|--------------------------|--------|-----------------|
| Appends per second | 232,182 | 247,960 | 434,110 | 86% |
| Records read per second (iterator) | 221,213 | 241,600 | 560,963 | 86% |
| A page of 100 rows, master (us) | 260.3 | 243.5 | 189.7 | 76% |
| The json text of a treedb view (us) | 2.30 | 2.18 | 0.918 | 91% |
| Reopen a treedb (us per node) | 277.3 | 19.28 | 16.68 | 1% |
| Update a treedb node and save it (us) | 7.33 | 4.84 | 4.06 | 24% |
| TCP echo through the gobj stack (round trips/s) | 40,056 | 61,144 | 63,426 | 6% | - |
| TLS echo (round trips/s) | 30,663 | 45,123 | 47,553 | 10% |
| Inter-yuno event round trips (/s) | 13,639 | 19,527 | 28,829 | 43% |
| MQTT QoS 0 round trips (/s) | 20,009 | 33,212 | 36,826 | 13% |
| OAuth2 BFF logins (/s) | 8,773 | 10,098 | 13,319 | 62% |

timeranger2 owes most of its gain to jansson: a record is a json, dumped when
it is written and parsed when it is read (the dump gathered in a buffer, a
short string in one block, the lexer reading runs of plain bytes). The treedb
and the network owe most of theirs to the rest of the round: the open of a
treedb reads the newest record of each key only and rebuilds its hooks
indexed; a publication reads its subscriptions from a C record, and an
`EV_STATE_CHANGED` that nobody would see is not built; a websocket server
frame goes in one write. The share is of the time one operation takes.

**The rotatory.** Two of its figures measured slower in the rotation of the
study: the log record +1.0% (t = 3.4) and the audit record with its retention
+1.8% (t = 8.0). `rotatory.c` and the path of a record are the same in both
releases (`rotatory.o` has the same machine code; `rotatory_write()` moved
7,296 bytes). A controlled rotation of five binaries, 16 rounds, ran 7.27.11,
7.28.0, and the same 7.28.0 relinked with 64, 128 and 192 bytes of pad before
the libraries: against 7.27.11 the log record measured +1.45% for 7.28.0 and
+0.47%, +0.55% and +0.32% for the moved copies, the audit record with
retention +2.16% and +0.74%, +1.06% and +0.06%. The same code moves with its
placement: code placement, not code.

**The new benchmarks.** `perf_ievent`, `perf_mqtt` and `perf_tr_treedb_read`
are new in 7.28.0; they were built in the tree of 7.27.11 too, with its
allocators, so they have both sides. `perf_tr_treedb_read` stopped at its
own N (8 000) on a build with `CONFIG_DEBUG_TRACK_MEMORY` (*"REACHED
MAX_SYSTEM_MEMORY"*: the default 64 MB of gbmem); it raises the limit now.

The yunos are 0.3% larger stripped (+29 KB to +51 KB).

## 7.27.11 against 7.27.10

7.27.11 puts the yuno in the ids of a treedb's lists. A replica names a
directory of the store after each id, and the id was unique inside one
process only, so every yuno of a realm that read the realm's authz store as a
replica shared one directory. The id is built when a treedb or a topic is
opened, never per record. It is measured against 7.27.10, the last release
with a study, before the tag. A fix release: its figures are here and in its
`.json`, with no report page of its own.

```{figure} ./_static/perf/perf_change_7.27.11.svg
:alt: The change of the time one operation takes, 7.27.11 against 7.27.10, one bar per figure. No price and no gain outside the noise: every one of the 41 figures within its noise, the appends of test_topic_pkey_integer among them (+0.03%, t = 0.1, 48 rounds). The BFF logins are the figures of a controlled three-way re-run, +0.08% (t = 0.05, 24 rounds).
:width: 100%

One bar per figure: the change of the time one operation takes, left of zero
is faster. Grey is within the noise of its rounds.
```

No price. Every one of the 41 figures stays inside its noise. One figure
crossed it in the rotation of the study and was measured again: the BFF
logins, -1.69% (t = -3.3, 32 rounds). `perf_auth_bff` never calls the changed
functions (no breakpoint of them is hit under `gdb`), but the change moved its
hot code (the event loop, gobj, llhttp, jansson) 128 bytes. A controlled
re-run of 24 rounds rotated three binaries: 7.27.10, 7.27.11, and the code of
7.27.10 relinked with the placement of 7.27.11 (a pad of 120 bytes in
`tr_treedb.o`; 17,258 of 17,926 functions at the addresses of 7.27.11). It
measured 7.27.11 +0.08% (t = 0.05). Without one slow round of 7.27.10 (5,888
against ~8,300) it is -1.15% (t = -1.6), and the code of 7.27.10 with the new
placement -1.38% (t = -1.8), the same: what moves follows the placement of
the code, not the code. Those are the figures of this report.

The treedb operations are 2-7% faster and the C_TREEDB opens 2-3.5% slower,
all within their noise (|t| < 1.5): their spread is wide at 8 and 24 rounds.
The yunos are the same size (stripped: +4 KB for `auth_bff`, -4 KB for the
agent, 0 for the others).

## 7.27.10 against 7.27.7

7.27.10 gives timeranger2 an append for a record that is already json text,
`tranger2_append_record_text()`, and gobj-c a reader of the members of a json
object from its text, `json_scan_object_members()`, with the verdict of
jansson and no node built. It is measured against 7.27.7, the last release
with a study, before the tag. 7.27.8 and 7.27.9 changed `.c` files only in
the modbus and mbus modules, which no measured binary links, so they were not
built: their measured code is the code of 7.27.7.

What the new append is for: a writer of raw history that receives frames as
text (an application's `db_events`) parsed each frame whole to read three of its
members, and dumped it again to store it. With the text path, on the same
machine and with the same SDK, it stores **2.2 times more frames per second**
(24,736 -> 53,308, 100 000 frames, 4 alternated rounds). On a Xeon E-2386G
it holds 20 000 records per second for 10.5 minutes at
55% of one core, ~27.6 us a frame (it was ~42 us); reading three members of a
frame of ~1 KB costs 0.75 us there, against 19.85 us for the parse, delete,
dump and free of the whole.

```{figure} ./_static/perf/perf_change_7.27.10.svg
:alt: The change of the time one operation takes, 7.27.10 against 7.27.7, one bar per figure. No price. Faster outside the noise: 600 000 appends into one key -2.8% of time (t = -5.8) and 400 000 appends into 20 000 files -1.6% (t = -3.5). Every other figure within its noise, the appends of test_topic_pkey_integer among them (-0.2%, t = -0.4, 48 rounds), after the fix described below.
:width: 100%

One bar per figure: the change of the time one operation takes, left of zero
is faster. Green is faster outside the noise, grey is within the noise of its
rounds.
```

No price. The one loss the study found is fixed in the release: before the
tag, the json append of `test_topic_pkey_integer` measured -0.87% appends per
second (t = -2.6) and -1.18% with a live reader (t = -2.8) after 48 rounds. An
A/B with only `timeranger2.c` changed placed it in commit `8d7e2a85f`, the
split of the append in two (-1.6%, t = -3.2; -2.0% with a reader, t = -4.0):
`append_content()` read the `system_flag` of the topic json a second time on
every append. Passing it the flag the append had already read removed it
(+0.07%, t = 0.1); every benchmark that appends was measured again with the
fix, and those are the figures of this report.

Faster outside the noise, without a change aimed at it: the appends of
`perf_timeranger2`, 600 000 into one key (-2.8% of time, t = -5.8) and
400 000 into 20 000 files (-1.6%, t = -3.5), since the same commit; the cause
was not isolated. Four figures sit at the edge of their noise after 24
rounds without crossing it, and their code did not change: the open of a
store as a replica and as the master (+1.1%), link or unlink (+1.4%), a tm
query (+1.5%). The yunos are 0.08-0.16% larger (stripped, 8-16 KB: the
scanner and the text append).

## 7.27.7 against 7.27.5

7.27.7 closes three reviews of the retention of 7.27.5: the
bounds of an iterator on a replica whose numbering a purge moved, the order
of a purge and the links of its feeds, a base read again after a failure
that passes, the reads an rt_disk follower owes after `EMFILE`, the parts of
a multi-key `C_TRANGER` iterator, `tr2list`. It is measured against 7.27.5,
the last release with a study, before the tag. 7.27.6 changed `.c` files
and shipped no study, so it ran in the same rotation: a figure that moved is
placed in the release where it moved.

```{figure} ./_static/perf/perf_change_7.27.7.svg
:alt: The change of the time one operation takes, 7.27.7 against 7.27.5, one bar per figure. One price: a forced treedb delete, +21.1% of CPU time (43.6 to 52.8 us), which appeared in 7.27.6. Every other figure within its noise: the read path of the master and of a replica (a page of 1 row on a replica +0.6%, an iterator open +1.1%), the appends +0.5%, the opens of a store, the treedb writes, the agent audit, the event loop, TCP, TLS and the BFF logins.
:width: 100%

One bar per figure: the change of the time one operation takes, left of zero
is faster. Red is a price, grey is within the noise of its rounds.
```

| Price | 7.27.5 | 7.27.7 | Change | Appeared in | What it buys |
|-------|--------|--------|--------|-------------|--------------|
| treedb: forced delete of a node (CPU per operation) | 43.6 us | 52.8 us | +21.1% (t = 41) | 7.27.6, commit `503c23efb` (7.27.6: 52.7 us; 7.27.7 +0.3% on it, t = 0.8) | Every key a delete removes is first moved out of `keys/` whole, by a `rename()`, then removed. A removal that failed half way left part of the key's files: the key, read again from them, moved the rows after the hole, and its next appends took `g_rowid`s that followers and clients had seen. 7.27.5 moved only a key with a base. |

The 9.2 us split in two, measured with an attribution build of 7.27.7 that
reads the start time of the process once (never shipped): 47.7 us, so about
5 us is `process_start_time()` -- a read of `/proc/self/stat`, made at every
delete to name the moved directory -- and about 4 us the `rename()`. The
start time of a process does not change, so that half can be paid once per
process.

Every other figure moved inside its spread (|t| < 3): the read path of the
master and of a replica (48 rounds; a page of 1 row on a replica 6.27 ->
6.31 us), the appends (+0.5%), the opens of a store, the treedb writes, the
agent audit, the event loop, TCP (+2.4% round trips, t = 2.3, 24 rounds),
TLS and the BFF logins. The yunos are 0-0.08% larger (stripped, at most
8 KB).

## 7.27.5 against 7.27.0

7.27.5 gives timeranger2 retention: a store deletes the old files of a key and
keeps the `g_rowid` of every row that stays. This study compares it with
7.27.0, the last release with a study, so it covers 7.27.1 to 7.27.5. It
replaces the one written the day after the release against 7.27.4, a release
nobody had measured. Each move is placed in the release where it appeared: the
read path ran once more on every tag from 7.27.0 to 7.27.5 and on two commits
of 7.27.5. It adds the read path of an iterator to the benchmarks
(`perf_timeranger2 read`), because the price of this release is there.

```{figure} ./_static/perf/perf_change_7.27.5.svg
:alt: The change of the time one operation takes, 7.27.5 against 7.27.0, one bar per figure. Prices, all of them from 7.27.5: on a replica, a page of 1 row +18.0% (5.25 to 6.20 us), a filtered page of 1 row +21.5% (4.08 to 4.95 us), an iterator open +6.9% (16.5 to 17.7 us), a filtered open +8.5%, a page of 100 rows +1.5%; a forced treedb delete +3.9%. Every other figure within its noise, the master's read path among them.
:width: 100%

One bar per figure: the change of the time one operation takes, left of zero
is faster. Red is a price, grey is within the noise of its rounds.
```

| Price | 7.27.0 | 7.27.5 | Change | Appeared in | What it buys |
|-------|--------|--------|--------|-------------|--------------|
| A page of 1 row, as a replica | 5.25 us | 6.20 us | +18.0% (t = 17) | 7.27.5, commit `83a771089` | Before every iterator open and every page, a replica `stat()`s the first md2 file of the key (`check_purge_before_read()`), purged or not: it notices a purge of the master whose signal it did not hear (an overflow of its watch, a follower down while the master purged, a replica without a follower) before the read meets files that are gone. About 1 us per call. |
| A filtered page of 1 row, as a replica | 4.08 us | 4.95 us | +21.5% (t = 40) | 7.27.5, `83a771089` | The same check. |
| Open an iterator, as a replica | 16.5 us | 17.7 us | +6.9% (t = 9.4) | 7.27.5, `83a771089` | The same check, made once by the open (a multi-key iterator pays it per key). |
| Open a filtered iterator, as a replica | 11.8 us | 12.8 us | +8.5% (t = 13) | 7.27.5, `83a771089` | The same check. |
| A page of 100 rows, as a replica | 264.1 us | 268.1 us | +1.5% (t = 3.5) | 7.27.5, commit `bd3fdce51` | A page of an unfiltered iterator counts its rows as positions among the live rows (`g_rowid` = purged + position), so the base of the key is read for every row. The master's page moves the same way (+1.1% here, t = 1.7). |
| treedb: forced delete of a node (CPU) | 42.9 us | 44.5 us | +3.9% (t = 2.5) | 7.27.5 (7.27.4: 42.0 us) | A key delete first looks for the key's base (`.purged`, one `access()`): a key with a base is moved out of `keys/` whole before it is removed, so a crash never separates its files from its base. |

How they were placed, the read phase alone, 24 rounds, every build in one
rotation (us per operation):

| Case | 7.27.0 | 7.27.1 | 7.27.2 | 7.27.3 | 7.27.4 | `bd3fdce51` | `83a771089` | 7.27.5 |
|------|--------|--------|--------|--------|--------|-------------|-------------|--------|
| A page of 1 row, replica | 5.13 | 5.12 | 5.16 | 5.22 | 5.12 | 5.19 | 6.03 | 6.10 |
| A filtered page of 1 row, replica | 3.98 | 3.98 | 3.98 | 4.00 | 3.98 | 4.02 | 4.79 | 4.89 |
| Iterator open, replica | 16.1 | 16.1 | 16.1 | 16.2 | 16.2 | 16.4 | 17.2 | 17.5 |
| Filtered iterator open, replica | 11.5 | 11.4 | 11.5 | 11.7 | 11.5 | 11.7 | 12.6 | 12.7 |
| A page of 100 rows, replica | 261.0 | 260.3 | 264.6 | 260.6 | 261.7 | 266.4 | 267.0 | 265.9 |
| A page of 100 rows, master | 257.9 | 258.8 | 262.8 | 259.6 | 261.7 | 265.7 | 265.7 | 264.6 |

7.27.1 to 7.27.4 read at 7.27.0's speed. The check of a replica comes with
`83a771089` (*the followers hear a purge of old key files*); a page of 100
rows moves with `bd3fdce51` (*`tranger2_delete_key_files()`, the master side
of retention*), on the master too. The check stays: skipping it while the
replica seems to have missed nothing leaves a window (a signal not yet read
from the event queue) in which a read opens a deleted file.

The master's read path, the appends (+1.1%), the opens of a store, the treedb
writes, the agent audit, the event loop, TCP, TLS and the BFF logins moved
inside their noise: the gobj and TCP changes of 7.27.2 to 7.27.4 cost
nothing measurable. 7.27.1 changed only the external libraries (OpenSSL
3.6.3 -> 3.6.5, PCRE2); both sides here run with those of 1.24, and that
change was measured on its own when it shipped (the TLS echo -0.9%, t =
-0.6). The yunos are 0.4-0.5% larger than in the 7.27.0 report (stripped,
about 50 KB): retention.

## 7.27.0 against 7.26.9

7.27.0 is a hardening release (nineteen more rounds of review): the command
parser types every parameter, secrets are masked in more places of the logs,
the walks of the gobj tree survive a callback that destroys a child, and a NUL
in an id, a pkey2 value, an fkey reference or a string of a record is refused.
It holds its speed: 30 of the 31 A/B figures stay inside the noise of their
rounds, and the one outside it is a price with its cause.

```{figure} ./_static/perf/perf_change_7.27.0.svg
:alt: The change of the time one operation takes, 7.27.0 against 7.26.9, one bar per figure. Price: a treedb update in memory, +6.5% of CPU time (2.31 to 2.46 us), the NUL checks of each write. Every other figure within its noise: the appends +1.0% (t = 2.3), the reads, the opens of a store, the event loop, TCP and TLS through the gobj stack, the BFF logins and the agent audit.
:width: 100%

One bar per figure: the change of the time one operation takes, left of zero
is faster. Red is a price, grey is within the noise of its rounds.
```

| Price | 7.26.9 | 7.27.0 | Change | What it buys |
|-------|--------|--------|--------|--------------|
| treedb: update a node in memory (CPU per operation) | 2.31 us | 2.46 us | +6.5% (t = 5.4) | The NUL checks of every treedb write: the id and each pkey2 value, and a walk of the kw for a NUL before any fkey reference is read. A NUL cut a name where C read it, so a node was indexed under one name and saved with another. Attributed by relinking (`perf` is not permitted on the machine): the 7.26.9 timeranger2 library with the 7.27.0 gobj-c is not slower, and the 7.27.0 `tr_treedb.c` with only those two checks taken out runs at 2.328 us against 2.327 us of 7.26.9. | 1.16 |

The saved update moved +3.9% (t = 1.7), inside its noise; the attribution run,
16 quieter rounds, measured +3.6% (t = 4.4) with the checks and -0.4% without
them: the same price, about 0.1 us of CPU per write. Everything else moved
inside its spread (|t| < 3), the price of 7.26.9 included (the event loop echo
that appends every message, +0.7% against 7.26.9). The yunos are 1.3-1.4%
larger (stripped, about 140 KB): the masking of secrets, the typed command
parser and the new checks.

## 7.26.9 against 7.26.0

7.26.9 is a hardening release (sixteen rounds of review): the kw readers, the
names that come from data, two name rules, the logger and what C_NODE accepts
from a peer. This study compares it with 7.26.0, the last release with a
study, so it covers 7.26.1 to 7.26.9; it replaces the one made at the tag
against 7.26.8, a release nobody had measured. Each move is placed in its
release, and in its commit where that was cheap, by building the tags and
commits between them. One of its changes is a gain: the treedb looks its
topics, hooks and fkeys up by key (`KWID_GET_BY_KEYS()`) instead of formatting
a path and splitting it again on every call.

```{figure} ./_static/perf/perf_change_7.26.9.svg
:alt: The change of the time one operation takes, 7.26.9 against 7.26.0, one bar per figure. Faster: a treedb link or unlink -12.2%, deleting a parent -10.1%, reopening a treedb -9.2%, a forced delete -3.7%. Prices: the first open of 40 treedbs +6.7% (7.26.7, the __icons__ topic), 600 000 appends to one key +4.1% and 400 000 appends into 20 000 files +2.3% (7.26.9). Every other figure within its noise, the read path, the event loop and the network among them.
:width: 100%

One bar per figure: the change of the time one operation takes, left of zero
is faster. Blue is faster, red is a price, grey is within the noise of its
rounds.
```

| Faster | 7.26.0 | 7.26.9 | Change | Appeared in | Why |
|--------|--------|--------|--------|-------------|-----|
| treedb: link or unlink two nodes (CPU per operation) | 8.59 us | 7.54 us | -12.2% (t = -18) | 7.26.9, mostly `f4be7d750` (7.26.8 8.62, `9d8f7c584` 8.28, `f4be7d750` 7.53 us) | The hook map and the fkey column looked up by key, one hash lookup per segment. | 4.70 |
| treedb: delete a parent with its children | 2,536 us | 2,281 us | -10.1% (t = -6.5) | 7.26.9, mostly `f4be7d750` | The same lookups, once per child. |
| treedb: open (per node loaded) | 311.6 us | 282.8 us | -9.2% (t = -11) | 7.26.9, mostly by `9d8f7c584` (309.5 -> 286.7 us) | The same lookups, for every node linked again; most of it comes with the commits that read a name from data as a key. |
| treedb: forced delete | 44.3 us | 42.6 us | -3.7% (t = -3.1) | 7.26.9 | The same lookups of the hooks it walks. |

| Price | 7.26.0 | 7.26.9 | Change | Appeared in | What it buys |
|-------|--------|--------|--------|-------------|--------------|
| C_TREEDB: first open of 40 treedbs (`perf_c_treedb` seed) | 10.29 s | 10.97 s | +6.7% (t = 5.7) | 7.26.7, commit `50c6cd78f` (7.26.6 10.33 s, 7.26.7 10.98 s) | Every treedb has a system topic `__icons__` for the icons a user adds, created at its first open: 40 topics more, each with its fsyncs. A one-time cost; the everyday open did not move (+1.2%, t = 0.4). |
| 600 000 appends to one key (`perf_timeranger2` `tm_build_appends`) | 1,604 ms | 1,669 ms | +4.1% (t = 7.3) | 7.26.9, in steps over its commits | About 0.11 us per append: the kw calls of each append, hardened -- the readers that answer what the kw holds (`df6c5f06a`, `ac9165b82`: +1.0%), a name from data read as a key (`6193b002c`: +1.7%), `9d8f7c584` +0.6%, the rest after. |
| 400 000 appends into 20 000 files (`build_appends`) | 1,550 ms | 1,586 ms | +2.3% (t = 4.4) | 7.26.9, the same steps | The same. |

The main append benchmark (`test_topic_pkey_integer`, 4 link layouts) moved
+0.7% (t = 0.6). The price the study at the tag reported and could not trace,
the event loop echo that appends every message (-3.9% against 7.26.8), is not
there against 7.26.0 (-0.2%, t = -0.2): 7.26.8 had measured high that day.
Every other figure moved inside its spread (|t| < 3), the read path of the
master and of a replica among them. The binaries are 0.16-0.24% larger
(stripped) than in the report of 7.26.0.

## 7.26.0 against 7.25.22

7.26.0 changes how timeranger2 reads and how its followers hear deletes: the
md2 rows of a scan are read 1024 at a time with one `pread()`, and the match
condition is parsed once per scan; the tm markers of 7.25.5..7.25.22 are gone,
so a tm condition filters every row of any topic; a key delete reaches the
rt_disk followers with the master's delete sequence; and a link in a treedb
publishes the relationship (`with_link_events` on by default), not the parent
collapsed whole.

```{figure} ./_static/perf/perf_change_7.26.0.svg
:alt: The change of the time one operation takes, 7.26.0 against 7.25.22, one bar per figure. Faster and claimed: a tm query of a topic 7.25.22 had not marked, -96.8% of time (402 to 12.8 ms); reading a history, -14% of time (+16.7% records per second) and page by page (+16.1%); opening a treedb, -20.3%; opening a store as a replica, -12.6%. Prices: a tm query of a topic 7.25.22 had marked, +69.9% (7.55 to 12.8 ms); creating 10 topics, +96.7%; the first open of 40 treedbs, +55.4%. Every other figure within its noise.
:width: 100%

One bar per figure: the change of the time one operation takes, left of zero
is faster. Blue is faster, red is a price paid on purpose, grey is within the
noise of its rounds.
```

| Faster | 7.25.22 | 7.26.0 | Change | Why |
|--------|---------|--------|--------|-----|
| A tm query of one minute, a topic 7.25.22 had not marked (1 key, 30 files x 20 000 rows) | 401.6 ms | 12.8 ms | -96.8%, 31x (t = -45) | The md2 rows read 1024 at a time with one `pread()` (an `lseek()` and a `read()` each before), and the match condition parsed into a C struct once per scan (~15 json lookups per row before). | - |
| Records read per second, every record of 2 keys (`test_topic_pkey_integer_iterator2`) | 184,338 | 215,157 | +16.7% (t = 17) | The same two changes. |
| Records read per second, page by page (`test_topic_pkey_integer_iterator5`) | 166,017 | 192,697 | +16.1% (t = 19) | The same two changes, on the read of a GUI. |
| treedb: open (per node loaded, `perf_tr_treedb` `reopen`) | 390.3 us | 311.2 us | -20.3% (t = -15) | A treedb loads its topics through the same scans. |
| Open a store as a replica, 20 000 md2 files | 108.4 ms | 94.8 ms | -12.6% (t = -8.7) | The load no longer looks for a tm marker beside each md2 file: the markers are gone. The master's open did not move. |

| Price | 7.25.22 | 7.26.0 | Change | What it buys |
|-------|---------|--------|--------|--------------|
| A tm query of one minute, a topic 7.25.22 had marked | 7.55 ms | 12.8 ms | +69.9% (t = 16) | The tm markers are gone (an API removal): a tm condition filters every row and no file is skipped by its tm range, correct on any topic and after any rollback. The block reads keep it at 1.7x the marked topic, not the 53x of an unmarked one. | - |

Fixed before the tag: a master that opens a topic makes its `delete_seq.json`
(the record of the delete sequence, which a follower takes as the sign that
its master signals each delete in order). Written durably -- a file fsync, a
rename and a directory fsync per topic -- it doubled the creation of 10
topics (131.4 -> 258.5 ms) and made the first open of 40 treedbs (400 topics)
55% longer (10.42 -> 16.19 s). It holds 0 and is made again if lost, so it is
written NOT durably now: 134.0 ms and 10.33 s, within the noise of 7.25.22
(the 7.26.0 side measured again after the fix, 8 rounds).

Not measured: a key delete on a topic with an rt_disk feed. No benchmark
deletes keys under a feed; by construction such a delete writes the record of
its sequence first, two fsyncs and a rename more than 7.25.22 did. A topic
without feeds pays nothing.

Every other figure moved inside its spread (|t| < 3), on paths that changed
or not. The binaries are 0.04-0.15% larger (stripped).

The method: 7.25.22 and 7.26.0 each built from their own tree (a git worktree
of the 7.25.22 tag with the same `.config` and compiler, where only the kernel
libraries, the modules, `performance/c/` and the three ctest binaries were
built), the two binaries of each benchmark run alternately, the order flipped
every round, with a `sync` and a 3 s pause before each run: 8 rounds, 24 for
the three timeranger2 ctest binaries. `outputs_ext` was shared: linux-ext-libs
did not change. The treedb figures are CPU time of the process.

## What one machine does with 7.26.0

The 7.26.0 side of the same run: 8 rounds (24 for the ctest binaries), mean
of the rounds, on the machine described below.

```{figure} ./_static/perf/perf_throughput_7.26.0.svg
:alt: Operations per second on one core with Yuneta 7.26.0. timeranger2 appends 224 K, appends with a live reader 190 K, timeranger2 reads 215 K, reads page by page 193 K, the io_uring event loop alone 149 K, the event loop with a timeranger2 append per message 86.1 K, the full gobj stack over TCP 39.5 K, TCP with an append 30.4 K, TLS 29.8 K, TLS with an append 23.9 K, OAuth2 BFF logins 8.6 K.
:width: 100%

Operations per second on one core, as each benchmark counts them.
```

| Figure | Benchmark | 7.26.0 |
|--------|-----------|--------|
| Records stored per second, one key-indexed topic | `test_topic_pkey_integer` | 223,864 +- 12,198 |
| The same, with a live reader (an rt list) | `test_topic_pkey_integer` | 189,539 +- 7,784 |
| Records stored per second, one key, 600 000 appends | `perf_timeranger2` `tm_build_appends` | 354,343 +- 10,491 |
| Records read per second, every record of 2 keys, one callback each | `test_topic_pkey_integer_iterator2` | 215,157 +- 7,268 |
| Records read per second, page by page | `test_topic_pkey_integer_iterator5` | 192,697 +- 4,480 |
| Open a store as master, 20 000 md2 files | `perf_timeranger2` | 81.9 ms | - |
| A tm query of one minute of a key (every row read) | `perf_timeranger2` | 12.8 ms | - |
| treedb: update a node in memory / saved | `perf_tr_treedb` | 2.49 us / 7.80 us | 1.16 |
| treedb: link or unlink / create / forced delete | `perf_tr_treedb` | 8.65 us / 65.6 us / 43.7 us |
| C_TREEDB: open 40 treedbs, schema unchanged | `perf_c_treedb` | 0.335 s | 0.266 |
| One log line of 300 bytes | `perf_rotatory` | 374 ns |
| One agent audit record / flushed | `perf_rotatory` | 570 ns / 1,257 ns | - |
| Event loop echo, 1 KB messages / with an append | `perf_yev_ping_pong`, `perf_yev_ping_pong2` | 149.0 K / 86.1 K msg/s | - |
| gobj stack, TCP round trips / with an append | `perf_tcp_test4`, `perf_tcp_test5` | 39,511 / 30,378 per s |
| gobj stack, TLS (OpenSSL 3.6.3) round trips / with an append | `perf_tcps_test4`, `perf_tcps_test5` | 29,762 / 23,907 per s |
| OAuth2 BFF logins (HTTP, 5 clients, mock IdP) | `perf_auth_bff` | 8,622 per s |
| Binary size of a yuno, fully static with OpenSSL | `outputs/yunos/*` | 10.2-10.6 MB stripped, +0.04% to +0.15% against 7.25.22 |

## 7.25.22 against 7.25.21

7.25.22 closes two reviews -- of 7.25.21, and of the fixes made for it -- and
the open defects of the list it inherited: the agents run as native systemd
units; a connection can be dropped from any callback of TLS or TCP without a
use of freed memory; a yuno alive but not connected can be killed; the rt_disk
followers of timeranger2 hear what they used to lose; C_AUTHZ and the IdP stop
handing roles and keys to whoever asks; a stats reset resets.

```{figure} ./_static/perf/perf_change_7.25.22.svg
:alt: The change of the time one operation takes, 7.25.22 against 7.25.21, one bar per figure. Faster and claimed: a forced treedb delete, -39% of time, because a delete no longer walks every open key. Moved outside their spread on code that did not change, not claimed: the appends of a store of many keys (-8.5% of time), 600 000 appends (+1.9%, +1.6% with a live reader) and a treedb update in memory (+5.0%). Every other figure within its noise.
:width: 100%

One bar per figure: the change of the time one operation takes, left of zero
is faster. Blue is faster, grey is within the noise of its rounds, or
placement.
```

| Faster | 7.25.21 | 7.25.22 | Change | Why |
|--------|---------|---------|--------|-----|
| treedb: forced delete (2000 deletes, 100 000 nodes) | 69.8 us | 42.5 us | -39.1% (t = -62) | A delete no longer walks every key with an open file to close the descriptors of the one deleted. |

Four figures moved outside their spread on code that did not change in this
release, and none is claimed:

| Moved | 7.25.21 | 7.25.22 | Change | Why it is not claimed |
|-------|---------|---------|--------|-----------------------|
| Appends per second (`test_topic_pkey_integer`) | 228,532 | 224,185 | -1.9% (t = -4.6) | `tranger2_append_record()` and everything it calls are the same in both releases. | - |
| The same, with a live reader | 194,479 | 191,447 | -1.6% (t = -3.3) | The same append, and the rt_mem feed did not change. |
| Appends to a store of many keys (`build_appends`) | 1,709 ms | 1,564 ms | -8.5% (t = -9.9) | The same append code, faster here and slower above. |
| treedb: update a node in memory | 2.30 us | 2.41 us | +5.0% (t = 8.2) | `tr_treedb.c`, `kwid.c` and jansson did not change; the gobj-c functions it could reach changed only in their trace branches. The same figure moved -5.2% the other way in the A/B of 7.25.21, on unchanged code too. | 1.16 |

Both releases are built whole, so the code lands at other addresses, which
alone moves a figure by a few percent: that is placement, not speed, and it is
said with its figure. The binaries are 0.4-0.5% larger (stripped).

The method: 7.25.21 and 7.25.22 each built from their own tree (a git worktree
of the 7.25.21 tag with the same `.config` and compiler, where only the kernel
libraries, `modules/c/test`, `performance/c/` and the three ctest binaries were
built), the two binaries of each benchmark run alternately, the order flipped
every round, with a `sync` and a 3 s pause before each run: 8 rounds, 24 for
the three timeranger2 ctest binaries. `outputs_ext` was shared: linux-ext-libs
did not change.

## What one machine does with 7.25.22

The 7.25.22 side of the same run: 8 rounds (24 for the ctest binaries), mean
of the rounds, on the machine described below.

```{figure} ./_static/perf/perf_throughput_7.25.22.svg
:alt: Operations per second on one core with Yuneta 7.25.22. timeranger2 appends 224 K, appends with a live reader 191 K, timeranger2 reads 189 K, reads page by page 169 K, the io_uring event loop alone 150 K, the event loop with a timeranger2 append per message 84.8 K, the full gobj stack over TCP 38.3 K, TCP with an append 30.6 K, TLS 29.7 K, TLS with an append 24.5 K, OAuth2 BFF logins 8.6 K.
:width: 100%

Operations per second on one core, as each benchmark counts them.
```

| Figure | Benchmark | 7.25.22 |
|--------|-----------|---------|
| Records stored per second, one key-indexed topic | `test_topic_pkey_integer` | 224,185 +- 3,339 |
| The same, with a live reader (an rt list) | `test_topic_pkey_integer` | 191,447 +- 3,677 |
| Records stored per second, one key, 600 000 appends | `perf_timeranger2` `tm_build_appends` | 362,758 +- 6,468 |
| Records read per second, every record of 2 keys, one callback each | `test_topic_pkey_integer_iterator2` | 189,390 +- 3,336 |
| Records read per second, page by page | `test_topic_pkey_integer_iterator5` | 169,121 +- 3,810 |
| Open a store as master, 20 000 md2 files | `perf_timeranger2` | 79.6 ms | - |
| A tm query of one minute of a key (migrated topic) | `perf_timeranger2` | 7.47 ms | - |
| treedb: update a node in memory / saved | `perf_tr_treedb` | 2.41 us / 7.75 us | 1.16 |
| treedb: link or unlink / create / forced delete | `perf_tr_treedb` | 8.63 us / 63.3 us / 42.5 us |
| C_TREEDB: open 40 treedbs, schema unchanged | `perf_c_treedb` | 0.333 s | 0.266 |
| One log line of 300 bytes | `perf_rotatory` | 369 ns |
| One agent audit record / flushed | `perf_rotatory` | 568 ns / 1,232 ns | - |
| Event loop echo, 1 KB messages / with an append | `perf_yev_ping_pong`, `perf_yev_ping_pong2` | 150.5 K / 84.8 K msg/s | - |
| gobj stack, TCP round trips / with an append | `perf_tcp_test4`, `perf_tcp_test5` | 38,292 / 30,571 per s |
| gobj stack, TLS (OpenSSL 3.6.3) round trips / with an append | `perf_tcps_test4`, `perf_tcps_test5` | 29,663 / 24,528 per s |
| OAuth2 BFF logins (HTTP, 5 clients, mock IdP) | `perf_auth_bff` | 8,581 per s |
| Binary size of a yuno, fully static with OpenSSL | `outputs/yunos/*` | 10.2-10.6 MB stripped, +0.4% to +0.5% against 7.25.21 |

## 7.25.21 against 7.25.20

7.25.21 is a security and correctness release: credentials are masked in
every trace, in every kw the kernel dumps, in the traffic dumps and in
`view-config`; a gbuffer can be marked secret (hidden from the dumps, wiped
when it is freed, never serialized); `C_TCP` and `C_TCP_S` stop and start
again cleanly and count their connections; the rt_disk feeds of timeranger2
hear every delete. None of it is on the paths that run every second at the
default trace levels: the masking runs only when a trace prints. On those
paths a secret gbuffer costs the test of one flag, and a TCP send one key
lookup (the `__secret__` flag of `EV_TX_DATA`).

```{figure} ./_static/perf/perf_change_7.25.21.svg
:alt: The change of the time one operation takes, 7.25.21 against 7.25.20, one bar per figure. Every bar is within the noise of its rounds or explained as code placement: the TCP echo through the gobj stack (+4.8% round trips), the BFF logins (+3.1%) and 600 000 appends to one key (-3.7% of time) moved outside their spread, faster, and are not claimed. Nothing slower.
:width: 100%

One bar per figure: the change of the time one operation takes, left of zero
is faster. Grey is within the noise of its rounds, or placement. No bar is
red: nothing is slower beyond its noise, and no bar is blue: nothing is
claimed faster.
```

Nothing got slower beyond the spread of its rounds. Three figures moved
outside it, all of them faster, and none is claimed as a gain:

| Moved | 7.25.20 | 7.25.21 | Change | Why it is not claimed |
|-------|---------|---------|--------|-----------------------|
| TCP echo through the gobj stack | 37,909 | 39,715 round trips/s | +4.8% (t = 6.2) | No change takes work off this path (a TCP send gained a key lookup), and the TLS echo, which runs the same `C_TCP` code, did not move (-0.7%). | - |
| OAuth2 BFF logins | 8,350 | 8,606 per s | +3.1% (t = 3.2) | `c_auth_bff.c` changed by one line; the HTTP path did not change. |
| 600 000 appends to one key (`tm_build_appends`) | 1,720 | 1,656 ms | -3.7% (t = -3.2) | `tranger2_append_record()` is the same in both releases; the same benchmark moved +3.6% the other way in the 7.25.20 report. |

Both releases are built whole, so the code lands at other addresses, which
alone moves a figure by a few percent (2.5% on an append, measured for
7.25.5): that is placement, not speed. The main append benchmark,
`test_topic_pkey_integer`, 24 rounds, moved +1.3% (223,031 -> 225,883
appends/s), inside its spread. The binaries are 0.6-0.9% larger (stripped),
the code of the masking.

The method: 7.25.20 and 7.25.21 each built from their own tree (a git
worktree of the 7.25.20 tag with the same `.config` and compiler, where only
the kernel libraries, `modules/c/test`, `performance/c/` and the three ctest
binaries were built), the two binaries of each benchmark run alternately, the
order flipped every round, with a `sync` and a 3 s pause before each run: 8
rounds, 24 for the three timeranger2 ctest binaries. `outputs_ext` was shared:
linux-ext-libs moved from 1.22 to 1.23 in this release, but only in how
openresty is downloaded, and no library changed version.

## What one machine does with 7.25.21

The benchmarks of `performance/c/` on the build of 7.25.21 at tag time, 5
rounds; the appends and reads of the ctest binaries are the 7.25.21 side of
the A/B run (24 rounds). Mean of the rounds, on the machine described below.

```{figure} ./_static/perf/perf_throughput_7.25.21.svg
:alt: Operations per second on one core with Yuneta 7.25.21. timeranger2 appends 226 K, appends with a live reader 192 K, timeranger2 reads 186 K, reads page by page 169 K, the io_uring event loop alone 149 K, the event loop with a timeranger2 append per message 85.4 K, the full gobj stack over TCP 39.2 K, TCP with an append 30.3 K, TLS 30.0 K, TLS with an append 23.7 K, OAuth2 BFF logins 8.7 K.
:width: 100%

Operations per second on one core, as each benchmark counts them.
```

| Figure | Benchmark | 7.25.21 |
|--------|-----------|---------|
| Records stored per second, one key-indexed topic | `test_topic_pkey_integer` | 225,883 +- 6,043 |
| The same, with a live reader (an rt list) | `test_topic_pkey_integer` | 191,672 +- 5,799 |
| Records stored per second, one key, 600 000 appends | `perf_timeranger2` `tm_build_appends` | 365,419 +- 10,488 |
| Records read per second, every record of 2 keys, one callback each | `test_topic_pkey_integer_iterator2` | 186,284 +- 4,672 |
| Records read per second, page by page | `test_topic_pkey_integer_iterator5` | 168,516 +- 3,912 |
| Open a store as master, 20 000 md2 files | `perf_timeranger2` | 80.1 ms | - |
| A tm query of one minute of a key (migrated topic) | `perf_timeranger2` | 7.50 ms | - |
| treedb: update a node in memory / saved | `perf_tr_treedb` | 2.31 us / 7.74 us | 1.16 |
| treedb: link or unlink / create / forced delete | `perf_tr_treedb` | 8.68 us / 64.4 us / 69.3 us |
| C_TREEDB: open 40 treedbs, schema unchanged | `perf_c_treedb` | 0.327 s | 0.266 |
| One log line of 300 bytes | `perf_rotatory` | 372 ns |
| One agent audit record / flushed | `perf_rotatory` | 582 ns / 1,230 ns | - |
| Event loop echo, 1 KB messages / with an append | `perf_yev_ping_pong`, `perf_yev_ping_pong2` | 149.4 K / 85.4 K msg/s | - |
| gobj stack, TCP round trips / with an append | `perf_tcp_test4`, `perf_tcp_test5` | 39,231 / 30,273 per s |
| gobj stack, TLS (OpenSSL 3.6.3) round trips / with an append | `perf_tcps_test4`, `perf_tcps_test5` | 29,999 / 23,718 per s |
| OAuth2 BFF logins (HTTP, 5 clients, mock IdP) | `perf_auth_bff` | 8,660 per s |
| Binary size of a yuno, fully static with OpenSSL | `outputs/yunos/*` | 10.2-10.5 MB stripped, +0.6% to +0.9% against 7.25.20 |

## 7.25.20 against 7.25.5

The fifteen releases between the two reports fixed the agent, the file
watcher, the TCP writes, the `SWITCHS` macro and the masking of secrets. None
of them was meant to change speed, and one of them is the largest gain of the
round: since 7.25.7 `SWITCHS` no longer compiles a regular expression each
time a switch is entered (it leaked it on every `return` from a case, commit
`3e8ff0508`). `tr_treedb` switches on the type of every column, so every
treedb write paid a `regcomp(".*")` and a `regfree`; `tr_treedb.c` and
`c_treedb.c` did not change between the two releases.

```{figure} ./_static/perf/perf_change_7.25.20.svg
:alt: The change of the time one operation takes, 7.25.20 against 7.25.5, one bar per figure. Faster: the open of 40 treedbs with an unchanged schema (2.1 times), a saved treedb update (-25%), link or unlink (-20%), the delete of a parent with 200 children (-17%), a treedb update in memory (-16%), appends with a live reader (-11% of time per append, +12.8% appends per second), the first open of 40 treedbs (-8%), a treedb create (-7%). Within noise: every other figure, the appends, the reads, the opens, the tm queries, the audit and log records, the event loop, TCP, TLS and the BFF logins. Nothing slower.
:width: 100%

One bar per figure: the change of the time one operation takes, left of zero
is faster. Blue is faster, grey is within the noise of its rounds. No bar is
red: nothing is slower beyond its noise.
```

| Faster | 7.25.5 | 7.25.20 | Change | Why |
|--------|--------|---------|--------|-----|
| C_TREEDB: open 40 treedbs, schema unchanged | 0.667 s | 0.318 s | x2.1 | `SWITCHS`: the open checks every column of each schema and loads every node through those switches. | 0.266 |
| treedb: update a node and save it | 10.25 us | 7.66 us | -25.2% | `SWITCHS` in the validation of each column. | 4.10 |
| treedb: link or unlink two nodes | 10.85 us | 8.65 us | -20.3% | The same. | 4.70 |
| treedb: forced delete of a parent with 200 children | 2,996 us | 2,478 us | -17.3% | The same. |
| treedb: update a node in memory | 2.76 us | 2.32 us | -16.2% | The same. | 1.16 |
| Appends per second with a live reader | 169,624 | 191,284 | +12.8% | The benchmark's own rt callback switches on the key with `SWITCHS` once per record: the fix seen from a caller. timeranger2's rt path did not change. |
| C_TREEDB: first open of 40 treedbs (seed) | 11.09 s | 10.19 s | -8.1% | `SWITCHS`. |
| treedb: create a node | 69.7 us | 64.9 us | -7.0% | `SWITCHS`. |

Nothing got slower beyond the spread of its rounds. The closest,
`tm_build_appends` (600 000 appends to one key, 1,632 -> 1,691 ms, +3.6%, at
the edge of the spread), runs append code that is the same in both releases:
`timeranger2.c` changed only in its rt-disk rescan. Both releases are built
whole, so the code lands at other addresses, which alone moves an append by up
to 2.5% (measured for 7.25.5); the main append benchmark,
`test_topic_pkey_integer`, 24 rounds, moved +0.3%.

The method: 7.25.5 and 7.25.20 each built whole from their own tree (a git
worktree of the tag for 7.25.5; the same `.config`, compiler and external
libraries), the two binaries of each benchmark run alternately, the order
flipped every round, with a `sync` and a 3 s pause before each run: 8 rounds,
24 for the three timeranger2 ctest binaries.

## What one machine does with 7.25.20

The 7.25.20 side of the same run: 8 rounds (24 for the ctest binaries), mean of
the rounds, on the machine described below.

```{figure} ./_static/perf/perf_throughput_7.25.20.svg
:alt: Operations per second on one core with Yuneta 7.25.20. timeranger2 appends 227 K, appends with a live reader 191 K, timeranger2 reads 189 K, reads page by page 169 K, the io_uring event loop alone 150 K, the event loop with a timeranger2 append per message 84.3 K, the full gobj stack over TCP 38.2 K, TCP with an append 30.1 K, TLS 27.8 K, TLS with an append 23.3 K, OAuth2 BFF logins 8.3 K.
:width: 100%

Operations per second on one core, as each benchmark counts them.
```

| Figure | Benchmark | 7.25.20 |
|--------|-----------|---------|
| Records stored per second, one key-indexed topic | `test_topic_pkey_integer` | 226,720 +- 4,468 |
| The same, with a live reader (an rt list) | `test_topic_pkey_integer` | 191,284 +- 4,132 |
| Records stored per second, one key, 600 000 appends | `perf_timeranger2` `tm_build_appends` | 355,059 +- 8,509 |
| Records read per second, every record of 2 keys, one callback each | `test_topic_pkey_integer_iterator2` | 188,895 +- 4,283 |
| Records read per second, page by page | `test_topic_pkey_integer_iterator5` | 168,796 +- 3,212 |
| Open a store as master, 20 000 md2 files | `perf_timeranger2` | 78.2 ms | - |
| A tm query of one minute of a key (migrated topic) | `perf_timeranger2` | 7.35 ms | - |
| treedb: update a node in memory / saved | `perf_tr_treedb` | 2.32 us / 7.66 us | 1.16 |
| treedb: link or unlink / create / forced delete | `perf_tr_treedb` | 8.65 us / 64.9 us / 68.5 us |
| C_TREEDB: open 40 treedbs, schema unchanged | `perf_c_treedb` | 0.318 s | 0.266 |
| One log line of 300 bytes | `perf_rotatory` | 375 ns |
| One agent audit record / flushed | `perf_rotatory` | 572 ns / 1,244 ns | - |
| Event loop echo, 1 KB messages / with an append | `perf_yev_ping_pong`, `perf_yev_ping_pong2` | 150.1 K / 84.3 K msg/s | - |
| gobj stack, TCP round trips / with an append | `perf_tcp_test4`, `perf_tcp_test5` | 38,191 / 30,073 per s |
| gobj stack, TLS (OpenSSL 3.6.3) round trips / with an append | `perf_tcps_test4`, `perf_tcps_test5` | 27,849 / 23,271 per s |
| OAuth2 BFF logins (HTTP, 5 clients, mock IdP) | `perf_auth_bff` | 8,347 per s |
| Binary size of a yuno, fully static with OpenSSL | `outputs/yunos/*` | 10.1-10.4 MB stripped, as 7.25.5 (+0.1% to +0.4%) |

## 7.25.5 against 7.25.4

```{figure} ./_static/perf/perf_change_7.25.5.svg
:alt: The change of the time one operation takes, 7.25.5 against 7.25.4, one bar per figure. Faster: the agent audit record (11 times), the audit record with a flush (5.2 times), the open of 40 treedbs with an unchanged schema (2.8 times), a migrated tm query (-42%), a treedb update in memory (-26%), a master open (-13%), a saved treedb update (-8%), the delete of a parent with 200 children (-6%). Within noise: the appends, the links, the creates, the reopen, the event loop and the TCP echo. Slower, by decision: the create of a topic (33 times), a topic_version change (136 times), a tm query on a topic not yet migrated (31 times), a publish to 100 subscribers with __global__ (2.1 times at 250 bytes, +27% at 20 KB), a replica open (+19%), the first open and the newer-schema open of a treedb (+10% and +9%), a forced delete (+1.8%).
:width: 100%

One bar per figure: the change of the time one operation takes (a rate is
turned into time per operation), left of zero is faster. Blue is faster, grey
is within the noise of its rounds, red is slower and kept by decision.
```

The prices of 7.25.5, and what each one buys:

| Slower | By | Why |
|--------|----|-----|
| Create a topic (timeranger2) | 3.6 -> 118.9 ms for 10 | 2 fsyncs per topic: a power cut never leaves a topic whose files are not on disk. |
| Change a `topic_version` | 1.7 -> 231.5 ms for 10 | 4 fsyncs per change: never a new `topic_version` over a `topic_cols.json` that is not on disk. |
| A tm query on a topic of 7.25.4 or earlier | 12.7 -> 391.6 ms | Until `mark-tm-order` migrates the topic (19 ms, once): the old topic does not say that its tm order holds. After the migration the query takes 7.4 ms. (From 7.26.0 the tm markers and `mark-tm-order` are gone: a tm query is a filter on every row again; with 7.26.0's block reads and once-parsed condition it takes ~12.8 ms on the benchmark's store, where a topic marked by 7.25.5..7.25.22 took 7.5 ms.) |
| Open a store as a replica | 89.7 -> 106.5 ms | One more `stat()` per md2 file: a replica looks for the order markers after reading each file, so it cannot miss one the master writes during the open. |
| C_TREEDB: first open, newer schema | +10%, +9% | The record of the projection in progress, written whole with its fsyncs: a crash in the middle is finished at the next open. |
| Publish with `__global__` or `__local__` | +0.23 us a delivery | Each such subscription gets its own `kw_twin()` of the event, so a peer's subscription cannot change the event of every later subscriber. Every remote (`C_IEVENT_SRV`) subscription carries `__global__`. |
| treedb forced delete | +1.8% (~1 us) | The delete holds its events and keeps the column that lets a refused delete change nothing. |
| The agent audit record, built | +2-3% | Names are judged with their JSON escapes decoded, and a write-attr is looked for in every string: no secret reaches the audit file. |

Every A/B of 7.25.5, change by change, with its rounds and spread, is in
[`performance/c/README.md`](https://github.com/artgins/yunetas/blob/7.29.3/performance/c/README.md)
and in the [CHANGELOG](CHANGELOG.md) ("Performance, against 7.25.4").

## What one machine does with 7.25.5

Measured at tag time on the build of 7.25.5: 5 rounds (3 for `perf_c_treedb`
and `perf_auth_bff`), mean of the rounds. One process; the event loop runs on
one core, and Yuneta scales by running one yuno per core.

```{figure} ./_static/perf/perf_throughput_7.25.5.svg
:alt: Operations per second on one core with Yuneta 7.25.5. timeranger2 appends 224 K, appends with a live reader 167 K, timeranger2 reads 188 K, reads page by page 165 K, the io_uring event loop alone 150 K, the event loop with a timeranger2 append per message 84.9 K, the full gobj stack over TCP 38.6 K, TCP with an append 29.7 K, TLS 29.2 K, TLS with an append 23.4 K, OAuth2 BFF logins 8.4 K.
:width: 100%

Operations per second on one core, as each benchmark counts them: a record
stored or read, a message echoed, a round trip, a login.
```

| Figure | Benchmark | 7.25.5 |
|--------|-----------|--------|
| Records stored per second, one key-indexed topic | `test_topic_pkey_integer` | 224,455 +- 2,585 |
| The same, with a live reader (an rt list) | `test_topic_pkey_integer` | 166,654 +- 1,748 |
| Records stored per second, one key, 600 000 appends | `perf_timeranger2` `tm_build_appends` | 365,459 +- 5,836 |
| Records read per second, every record of 2 keys, one callback each | `test_topic_pkey_integer_iterator2` | 187,914 +- 3,379 |
| Records read per second, page by page | `test_topic_pkey_integer_iterator5` | 165,301 +- 1,971 |
| Open a store as master, 20 000 md2 files | `perf_timeranger2` | 79.8 ms | - |
| A tm query of one minute of a key (migrated topic) | `perf_timeranger2` | 7.2 ms | - |
| treedb: update a node in memory / saved | `perf_tr_treedb` | 2.76 us / 10.37 us | 1.16 |
| treedb: link or unlink / create / forced delete | `perf_tr_treedb` | 11.02 us / 71.0 us / 69.2 us |
| C_TREEDB: open 40 treedbs, schema unchanged | `perf_c_treedb` | 0.649 s | 0.266 |
| One log line of 300 bytes | `perf_rotatory` | 381 ns |
| One agent audit record / flushed | `perf_rotatory` | 570 ns / 1,263 ns | - |
| Event loop echo, 1 KB messages / with an append | `perf_yev_ping_pong`, `perf_yev_ping_pong2` | 149.6 K / 84.9 K msg/s | - |
| gobj stack, TCP round trips / with an append | `perf_tcp_test4`, `perf_tcp_test5` | 38,587 / 29,699 per s |
| gobj stack, TLS (OpenSSL 3.6.3) round trips / with an append | `perf_tcps_test4`, `perf_tcps_test5` | 29,166 / 23,363 per s |
| OAuth2 BFF logins (HTTP, 5 clients, mock IdP) | `perf_auth_bff` | 8,446 per s |
| Binary size of a yuno, fully static with OpenSSL | `outputs/yunos/*` | 10.1-10.4 MB stripped, 33.9-34.7 MB with debug information |

TLS with OpenSSL costs 24% of the round trips of plain TCP through the gobj
stack (29,166 against 38,587). mbedTLS is not built on the measuring machine,
so its figures are not in the report.

## How the figures are measured

- **The machine.** One laptop, the same for every report until a report says
  otherwise: an Acer Nitro AN517-53 with an 11th Gen Intel Core i7-11370H
  (4 cores, 8 threads, up to 4.8 GHz), 31 GiB of RAM, a Samsung 990 PRO NVMe
  with ext4, Linux 7.0.0-34-generic, Ubuntu 26.04.1 LTS, glibc 2.43, gcc 15.2.0.
  Absolute figures compare only between reports of the same machine.
- **The build.** RelWithDebInfo (`-O2 -g`), gcc, fully static, OpenSSL, with
  `CONFIG_DEBUG_TRACK_MEMORY` on (every allocation is tracked).
- **A/B.** The module of the old release is compiled with the headers of the
  new one and linked before the same libraries; the two binaries run
  alternately, with a `sync` and a pause before each run. For timeranger2 the
  modules are padded to one size and linked four times at four addresses,
  because the address of the libraries alone moves an append by up to 2.5%.
  That is the A/B of 7.25.5. The A/B of 7.25.20 and of 7.25.21 builds each
  release whole from its own tree and runs the two binaries alternately, the order flipped every
  round: simpler, and blind to a change under ~2.5% on an append, which it says
  where it matters.
- **Tag time.** The benchmarks of `performance/c/`, in rounds, on the build of
  the release.

The benchmarks themselves are described in [Benchmarks & Stress Tests](benchmarks.md).
