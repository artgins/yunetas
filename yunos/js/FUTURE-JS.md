# JS Library Evaluation Report — Browser/Mobile Webapp (2025/2026)

## Summary Table

| Library | Min+Gzip | Mobile | Large Dataset Perf | Verdict |
|---|---|---|---|---|
| @antv/g6 v5 | ~800 KB min / ~200-300 KB gz (est.) | Partial - touch/pinch works, no dedicated mobile build | Canvas + WebGL + WASM layouts; handles thousands of nodes well | Keep if you need enterprise graph viz; consider Cytoscape.js for simpler cases |
| @antv/s2 v2 | ~500 KB min / ~150 KB gz (est.) | Poor - pointer events disabled on mobile | 1M rows claim; Canvas-based; sub-4s full render | Niche pivot/cross-analysis use only; poor mobile story |
| bootstrap-table v1.25 | ~100 KB min / ~30 KB gz (core only) | Adequate - responsive modes built in | Struggles beyond ~10k rows without virtualization | Drop it - requires jQuery; Tabulator does more with less |
| bulma v1.0.4 | ~202 KB min / ~24 KB gz | Good - flexbox-based, CSS-only | N/A (CSS framework) | Reasonable if you already use it; Tailwind v4 is smaller and more powerful |
| jquery v3.7 | ~87 KB min / ~30 KB gz (full) / ~24 KB gz (slim) | Good - cross-browser | N/A | Avoid for new projects; Bootstrap 5 and bootstrap-table are progressively removing it |
| tabulator-tables v6 | ~320 KB min / ~99 KB gz | Good - responsive layouts built in | Virtual DOM; solid at 50k rows; slower than AG Grid at 100k+ | Strong free-tier choice; AG Grid Community edges it at extreme scale |
| uplot v1.6 | ~47 KB min / ~20 KB gz | Good - Canvas-based, renders well on mobile | 166k pts in 25ms cold start; 10% CPU @ 60fps streaming | Excellent - best perf/size ratio for time series |
| tom-select v2.4 | ~50 KB min / ~16 KB gz | Good - keyboard + touch | N/A | Good choice, no significant better alternative in this niche |
| maplibre-gl v5 | ~2.5 MB min / ~600 KB gz (est.) | Good - first-class touch/pinch | GPU-accelerated WebGL; scales to millions of features | Dominant open-source map lib; no strong alternative unless you need 2D-only (Leaflet) |
| vanilla-jsoneditor | ~400 KB min / ~120 KB gz (est., standalone) | Adequate - works on mobile, not optimized | N/A | Fine for JSON editing; no major competitor |
| luxon v3 | ~70 KB min / ~23 KB gz | Good | N/A | Heavy if you only need formatting; Day.js (~6 KB gz) or date-fns (~14 KB gz) are leaner |

---

## Per-Library Deep Dive

### 1. @antv/g6 v5 (Graph Visualization)

**Bundle Size:** G6 v5 is a large framework. The npm package is several megabytes unpacked. No precise gzip figure is consistently published, but given it bundles Canvas, SVG, and WebGL renderers from `@antv/g`, the runtime is estimated at 800 KB+ minified, ~200-300 KB gzipped. The library supports tree-shaking via ES modules and a plugin architecture — unused modules are excluded from the final build, which can reduce this significantly for simple use cases.

**Mobile Support:** Touch events and pinch-to-zoom work through the `zoom-canvas` built-in behavior. The `@antv/g6-mobile` package exists but has been unmaintained for 5 years (last version 0.1.2), so dedicated mobile support is purely through the main package's touch event handling. Pinch-zoom works via `event.ctrlKey` detection. There is no "mobile-first" mode.

**Large Dataset Performance:**
- G6 v4 could handle ~60k–70k primitives before degradation; interactive smooth operation required keeping below 30k.
- G6 v5 introduces: WebGL rendering, Rust+WASM layout computation, WebGPU-accelerated layouts, and a "Transient Canvas" for interaction overlays (avoids repainting the main canvas on every hover/drag). These changes make thousands of nodes tractable.
- For 10k+ nodes with force-directed layouts, the WASM/GPU acceleration is meaningfully useful.

**Practical concern:** G6's primary documentation is in Chinese. English docs are translated but often lag behind.

---

### 2. @antv/s2 v2 (Pivot/Spreadsheet Table)

**Bundle Size:** Estimated ~500 KB minified / ~150 KB gzipped for `@antv/s2` core (depends on `@antv/g` rendering engine). React/Vue wrappers add ~15-25 KB. The standalone package has no external peer dependencies beyond the renderer.

**Mobile Support:** Poor by design. The source code explicitly sets `supportsPointerEvents = !isMobile(device)` — pointer events are disabled on mobile to avoid conflicts with native touch scrolling (documented in issue #2857). Known bugs include: mobile tap events firing multiple times (fixed in v2.4.0-beta.1), CSS transform breaking canvas hit detection (issue #2879), and scrollbar touch area glitches. S2 is architected for desktop analytics dashboards.

**Large Dataset Performance:**
- Official claim: full million-row rendering under 4 seconds.
- Virtual scrolling ensures each scroll frame renders only the viewport, not the full dataset.
- Canvas-based (not DOM), so it avoids the DOM recycling bottleneck that plagues HTML table-based solutions.
- Serves 90%+ of Ant Group's core data dashboards — production-proven at scale in controlled desktop environments.

**Assessment:** S2 is the right tool specifically if you need Excel-style pivot/cross-analysis tables with grouping, drill-down, and aggregation. It is not a general-purpose data grid and is **not suitable for mobile-first interfaces**.

---

### 3. bootstrap-table v1.25 (Table)

**Bundle Size:** The core `bootstrap-table.js` is approximately 150-200 KB minified / ~30-35 KB gzipped. However, it requires jQuery (~87 KB min / ~30 KB gz) and Bootstrap CSS (~58 KB min / ~16 KB gz), making the total dependency weight significant.

**Mobile Support:** Responsive behavior via column priority collapsing, card view mode, and Bootstrap's responsive grid. Adequate for basic use. v1.25 (released October 2025) added `aria-sort` on sortable headers. v1.26 refactored into separate modules. jQuery dependency is being reduced across releases (removed from the `utils` module, `DOMHelper` abstraction added).

**Performance:** No virtualization by default. For large datasets (10k+ rows), performance degrades as all rows are rendered in the DOM. Pagination is the primary mitigation. Not suitable for 100k rows.

**Verdict:** This library is legacy infrastructure. It requires jQuery (deprecated in Bootstrap 5 itself), lacks DOM virtualization, and Tabulator provides a strict superset of features with better performance and no jQuery requirement. The only reason to keep it is an existing jQuery-based codebase.

---

### 4. bulma v1.0.4 (CSS Framework)

**Bundle Size:** Full build: ~202 KB minified / ~24 KB gzipped. With the modular import system, you can import only the components you use and reduce this substantially.

**Mobile Support:** Good. Flexbox-first, with responsive column classes (`is-mobile`, `is-tablet`, etc.) and mobile-first breakpoints. Pure CSS, no JavaScript.

**v1.0 changes (March 2025):** CSS custom properties (variables) throughout, official dark mode support, improved theming system.

**Verdict:** Bulma v1.0 is a clean, JavaScript-free CSS framework. At ~24 KB gz for the full build it is competitive. It lacks Tailwind's utility-first flexibility and has no component JavaScript. For a modern app with a build system, Tailwind CSS v4 is lighter (under 10 KB gz after purging) and more widely adopted (31M vs. ~2M weekly downloads). Bulma makes sense if you prefer semantic class names and don't want to configure Tailwind.

---

### 5. jquery v3.7 (DOM Library)

**Bundle Size:** Full build: ~87 KB minified / ~30 KB gzipped. Slim build (no AJAX, no effects): ~67 KB min / ~24 KB gz.

**Mobile Support:** Good cross-browser compatibility including mobile browsers. But jQuery's mobile support is largely irrelevant now — all modern mobile browsers support the standard DOM APIs jQuery abstracts.

**Performance:** jQuery adds overhead for every DOM operation. Not meaningful for static pages but accumulates in high-frequency update scenarios.

**Verdict:** Avoid for new projects. Bootstrap 5 dropped jQuery. Tabulator, uPlot, MapLibre, and all other libraries in this list work without it. The only reason jQuery appears here is as a bootstrap-table dependency. Adding jQuery solely for bootstrap-table costs 24-30 KB gzipped for a library you could replace with Tabulator for the same bundle cost with no jQuery dependency.

---

### 6. tabulator-tables v6 (Table)

**Bundle Size:** ~320 KB minified / ~99 KB gzipped for the full package. This is the entire Tabulator with all modules. You can import individual modules to reduce size.

**Mobile Support:** Good. Responsive layout system collapses columns, card-view mode for small screens, touch scrolling works natively. Responsive column visibility with `responsive` layout mode.

**Large Dataset Performance:**
- Uses row virtualization (virtual DOM rendering) — only visible rows are rendered in the DOM.
- Solid for 50k rows; some users report ~50ms initial render for ~10k rows without locked columns, but ~2200ms with locked columns (a known perf regression that should be checked in v6).
- At 100k rows with filtering, sorting, and grouping simultaneously, it begins to show latency vs. AG Grid Community.
- Supports server-side pagination/sorting/filtering for truly large remote datasets.

**Verdict:** Strong choice for a free, full-featured, framework-agnostic table. The 99 KB gz cost is reasonable for what it delivers.

---

### 7. uplot v1.6 (Time Series Charts)

**Bundle Size:** ~47 KB minified / ~20 KB gzipped. This is the entire library including all chart types.

**Mobile Support:** Canvas-based rendering works well on mobile. Touch events are not natively handled for pan/zoom by default — you need to implement touch gesture handling or use a plugin. The core library draws correctly on high-DPI/Retina displays via `devicePixelRatio`.

**Large Dataset Performance (benchmarks):**
- Cold start: 166,650 data points rendered in 25ms.
- Streaming at 60 fps updating 3,600 points: **10% CPU, 12.3 MB RAM**.
- Comparison: Chart.js at same workload: 40% CPU, 77 MB RAM. ECharts: 70% CPU, 85 MB RAM.
- Bundle size vs. ECharts: uPlot 47 KB vs. ECharts ~1,000 KB.
- Scales linearly at ~100k pts/ms after cold start.

**Limitations:**
- Documentation is sparse ("Spartan") — primarily TypeScript types and examples.
- Limited built-in chart types: line, area, bars, OHLC, bands.
- No built-in zoom/pan UI; must implement interactions manually.
- Struggles beyond 100k in-view data points at 60fps (Canvas 2D limitation).

**Verdict:** The best perf/size ratio for time series in Canvas 2D. Excellent for monitoring dashboards, sensor data, and real-time feeds.

---

### 8. tom-select v2.4 (Select/Autocomplete)

**Bundle Size:** ~50 KB minified / ~16 KB gzipped for the full build. A smaller `tom-select.base.js` (without plugins) saves ~4 KB.

**Mobile Support:** Good. Works with both mouse and touch events. Keyboard navigation works on mobile with software keyboards. No known mobile-specific issues.

**Performance:** Handles large option lists (10k+ items) via virtual scrolling in the dropdown. Forked from Selectize.js but modernized and framework-agnostic.

**Alternatives:** Choices.js (~19 KB gz), Select2 (requires jQuery). Tom Select is the best maintained vanilla option in 2025. No significantly better alternative.

---

### 9. maplibre-gl v5 (Maps)

**Bundle Size:** MapLibre GL JS is large by necessity — it bundles WebGL shaders, tile rendering, style parsing, geographic math, and worker thread code. Estimated: ~2.5 MB minified / ~550-650 KB gzipped. The library uses Web Workers for tile decoding (the worker code is inlined as a blob), so Bundlephobia's reported size is an overestimate of what actually blocks the main thread.

**Mobile Support:** First-class. Touch/pinch-to-zoom, touch-pan, rotation with two fingers, bearing control — all built in. MapLibre GL JS is tested on iOS Safari and Android Chrome as primary targets.

**Performance:** GPU-accelerated WebGL rendering. Vector tiles are decoded in Web Workers off the main thread. Can render millions of points via clustering and data-driven styling. Frame rate is typically 60fps for normal map interactions.

**Alternatives:** Leaflet (~40 KB gz) for simple 2D tile maps without WebGL. Deck.gl for extreme-scale data overlays (built on top of MapLibre or Mapbox). No open-source alternative matches MapLibre GL's feature set for WebGL map rendering. Mapbox GL JS is the commercial predecessor but has a non-OSS license since December 2020.

---

### 10. vanilla-jsoneditor (JSON Editor)

**Bundle Size:** The ES module import (recommended for bundlers) pulls in lodash-es and Ajv as peer dependencies — when tree-shaken, the net cost is approximately 200-400 KB minified / 80-120 KB gzipped depending on which validators and lodash functions are used. The standalone bundle includes all dependencies and is larger.

**Mobile Support:** Works on mobile browsers. The interface (tree view, text editor with line numbers) is not optimized for small screens but is functional.

**Alternatives:** JSON Editor (josdejong/json-editor) is older and less actively maintained. CodeMirror 6 can edit JSON with a syntax plugin but requires more setup. vanilla-jsoneditor is the best maintained option for a drop-in JSON viewer/editor in 2025.

---

### 11. luxon v3 (Date/Time)

**Bundle Size:** ~70 KB minified / ~23 KB gzipped. Built on the native `Intl` API, so it bundles zero locale data — 50 locales cost 0 additional bytes.

**Mobile Support:** Pure JavaScript, works identically on mobile. The `Intl` API is available in all modern mobile browsers.

**Performance:** Slower than date-fns or Day.js because every formatting/parsing operation goes through `Intl`, which has JIT-compilation overhead on first call.

**Alternative Comparison:**

| Library | Min+Gzip | Locale overhead | TZ support | API style |
|---|---|---|---|---|
| Day.js | ~6 KB | Plugins (+3-5 KB each) | Plugin needed | OOP (Moment compat) |
| date-fns | ~14 KB (tree-shaken) | +80 KB all bundled | `date-fns-tz` plugin | Functional |
| Luxon v3 | ~23 KB | 0 (uses Intl) | Built-in | OOP / Chainable |
| Moment.js | ~67 KB | ~330 KB (all locales) | Plugin | Deprecated |

**Verdict:** If you need robust time zone support and internationalization without bundling locale data, Luxon is the right choice and the ~23 KB gz is justified. If you only need date formatting without time zones, Day.js at ~6 KB gz is the clear winner.

---

## Alternative Comparisons

### Graph Visualization: G6 v5 vs. Cytoscape.js vs. vis-network

| | G6 v5 | Cytoscape.js v3 | vis-network v10 |
|---|---|---|---|
| Bundle (min+gz) | ~200-300 KB (est.) | ~112 KB | ~150-200 KB (est.) |
| Mobile/touch | Partial (pinch-zoom works, no mobile-first design) | Excellent (built-in tap/pinch/drag) | Basic (mouse-first) |
| Large graph perf | High (WebGL + WASM layouts) | Moderate (Canvas only) | Low-Moderate |
| Customization | Very high | High (stylesheet system) | Moderate |
| Graph algorithms | Good | Rich built-in (PageRank, centrality, A*) | Limited |
| Ease of use | Moderate | Moderate | Easiest |
| English docs | Moderate (primary docs in Chinese) | Excellent | Good |
| Active maintenance | Yes (Ant Group) | Yes (monthly releases, v3.33.0 in July 2025) | Slow (v10.0.2, 5 months old) |
| License | MIT | MIT | Apache 2.0 |

**Recommendation:**
- **Cytoscape.js** is the strongest alternative to G6 for general-purpose graph/network visualization. It is smaller (~112 KB gz), has first-class mobile touch support out of the box, excellent English documentation, rich built-in graph algorithms, and active monthly releases. The main disadvantage vs. G6 is no WebGL renderer (Canvas only) and slightly lower ceiling for 10k+ node graphs.
- **G6 v5** is the better choice if: you need WebGL rendering for very large graphs (10k+ nodes), you need WASM-accelerated layouts, or you need 3D graph visualization.
- **vis-network** is not recommended: slowest performance on large graphs, least active development, no significant advantages over Cytoscape.js.

**For most web+mobile graph apps: Cytoscape.js is the better practical choice.** For enterprise-scale graphs (5k+ nodes, complex layouts, 3D): G6 v5.

---

### Table: Tabulator v6 vs. AG Grid Community

| | Tabulator v6 | AG Grid Community v33 |
|---|---|---|
| Bundle (min+gz) | ~99 KB | ~300-520 KB (varies by modules used; v33 promises ~40% reduction) |
| Mobile/responsive | Built-in responsive layout, card view | Built-in, more config required |
| 100k row performance | Good with virtual scroll; slower than AG Grid at extreme scale | Best-in-class virtualization |
| Row grouping | Free | Enterprise only |
| Tree data | Free | Enterprise only |
| Export (CSV, XLSX, PDF) | Free (all formats) | Limited in Community |
| Inline editing | Free | Free |
| Server-side data | Free | Free |
| Ease of use | Simpler API | Steeper learning curve |
| License | MIT | MIT (Community) / Commercial (Enterprise) |

**Recommendation:** For 100k rows with mixed filtering/sorting/grouping on mobile:
- **AG Grid Community** is the performance leader at extreme scale. However, v33's bundle starts at ~300 KB gz even with modules, which is 3× Tabulator's footprint. Key features (row grouping, tree data, Excel export) are Enterprise-only.
- **Tabulator v6** at ~99 KB gz is significantly leaner, includes grouping, tree data, and full export in the free tier, and has a friendlier API. For most real-world 100k-row use cases it is sufficient, especially with server-side data loading.

**Verdict:** If 100k rows are server-paginated/filtered, Tabulator is the better pick (smaller bundle, more free features). If you need client-side 100k+ row operations (full sort, filter, grouping on the client), AG Grid Community handles it better but costs you ~200-400 KB additional bundle size and loses grouping/tree features unless you pay for Enterprise.

---

### Time Series: uPlot v1.6 vs. TradingView Lightweight Charts v4

| | uPlot v1.6 | Lightweight Charts v4 |
|---|---|---|
| Bundle (min+gz) | ~20 KB | ~45 KB |
| Rendering | Canvas 2D | Canvas 2D |
| Cold-start perf | 166k pts in 25ms | Not independently benchmarked; comparable |
| CPU @ 60fps | 10% (3,600 pts) | Not published |
| Chart types | Line, area, bars, OHLC, bands | Line, area, bar, candlestick, histogram |
| Financial-specific | Basic | Purpose-built (crosshair, price scale, time scale) |
| Real-time streaming | Excellent | Excellent |
| Documentation | Sparse (TypeScript types + examples) | Good |
| Plugin system | Minimal | Rich (v4 plugin API) |
| License | MIT | Apache 2.0 |
| Mobile touch | Manual implementation needed | Manual implementation needed |

**Recommendation:**
- For general time-series monitoring (server metrics, IoT, sensor data): **uPlot** — smaller, faster, less opinionated.
- For financial/trading charts (OHLC, candlesticks, price scales, crosshair tools): **Lightweight Charts** — purpose-built, better developer experience for finance UIs.
- For extreme real-time data beyond Canvas 2D limits (500k+ in-view points at 60fps): Consider WebGL-based alternatives: `huww98/TimeChart` (WebGL), `danchitnis/webgl-plot`, or LightningChart JS (commercial).

For "very fast real-time + historical time series charts" without financial/OHLC requirements, **uPlot is the recommendation** — its 10% CPU vs. 40-70% for competitors at 60fps is a decisive advantage for real-time dashboards.

---

## Total Bundle Weight Estimate

If you include all 11 libraries in a single app (worst case, no tree-shaking):

| Library | Approx. min+gz |
|---|---|
| @antv/g6 v5 | ~250 KB |
| @antv/s2 v2 | ~150 KB |
| bootstrap-table + jQuery | ~65 KB |
| bulma | ~24 KB |
| tabulator-tables | ~99 KB |
| uplot | ~20 KB |
| tom-select | ~16 KB |
| maplibre-gl | ~600 KB |
| vanilla-jsoneditor | ~100 KB |
| luxon | ~23 KB |
| **Total** | **~1.35 MB gz** |

For a mobile-optimized app this is heavy. The three largest contributors are maplibre-gl (~600 KB gz), @antv/g6 (~250 KB), and @antv/s2 (~150 KB). Code splitting (lazy-loading the map, graph, and pivot table views) would reduce the initial bundle significantly.

---

## Updated Final Verdict

| Library | Status | Key reason |
|---------|--------|------------|
| `@antv/g6` v5 | Keep (or evaluate Cytoscape.js) | G6 for large/complex graphs; Cytoscape.js for simpler graphs + better mobile + smaller bundle + English docs |
| `@antv/s2` v2 | Remove if mobile matters | Pointer events explicitly disabled on mobile (`supportsPointerEvents = !isMobile(device)`) |
| `bootstrap-table` | Remove | No virtualization, redundant, requires jQuery |
| `jquery` | Remove | Legacy, only present for bootstrap-table |
| `tabulator-tables` | Keep | Best free table for 100k rows, ~99 KB gz |
| `uplot` | Keep | Fastest time series, 10% CPU @ 60fps vs 40-70% for alternatives |
| `bulma` | Keep | 24 KB gz, CSS-only, mobile-first |
| `maplibre-gl` | Keep (lazy-loaded) | Dominant open-source WebGL map, load only when needed |
| `tom-select` | Keep | Best maintained vanilla select, ~16 KB gz |
| `vanilla-jsoneditor` | Keep | Best maintained JSON editor |
| `luxon` | Keep (or swap Day.js) | Only if TZ support needed; otherwise Day.js saves ~17 KB gz |
| `i18next` | Keep | Standard i18n |

---

## Sources

- [uPlot GitHub benchmarks](https://github.com/leeoniya/uPlot)
- [G6 v5 announcement](https://yanyanwang93.medium.com/g6-5-0-a-professional-and-elegant-graph-visualization-engine-11bba453ff4d)
- [G6 ZoomCanvas behavior docs](https://g6.antv.antgroup.com/en/manual/behavior/zoom-canvas)
- [AntV S2 introduction](https://s2.antv.antgroup.com/en/manual/introduction)
- [S2 mobile issues — GitHub](https://github.com/antvis/S2/issues)
- [AG Grid v33 release blog](https://blog.ag-grid.com/whats-new-in-ag-grid-33/)
- [Tabulator v6.3.1 — Bundlephobia](https://bundlephobia.com/package/tabulator-tables)
- [bootstrap-table changelog](https://github.com/wenzhixin/bootstrap-table/blob/develop/CHANGELOG.md)
- [Tom Select GitHub](https://github.com/orchidjs/tom-select)
- [Bulma vs Tailwind comparison](https://blog.logrocket.com/bulma-vs-tailwind-css-better-bootstrap-alternative/)
- [Luxon vs date-fns vs Day.js comparison](https://www.dhiwise.com/post/luxon-vs-date-fns-whats-the-best-for-managing-dates)
- [Graph library comparison — Cylynx](https://www.cylynx.io/blog/a-comparison-of-javascript-graph-network-visualisation-libraries/)
- [Cytoscape.js WebGL preview (Jan 2025)](https://blog.js.cytoscape.org/2025/01/13/webgl-preview/)
- [JS Data Grid Comparison — DZone](https://dzone.com/articles/javascript-data-grids-top-options)
- [MapLibre GL JS npm](https://www.npmjs.com/package/maplibre-gl)
- [TradingView Lightweight Charts](https://www.tradingview.com/lightweight-charts/)
