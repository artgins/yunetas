---
title: Executable notebook demo
subtitle: MyST Markdown + Jupyter — replacement for myst_nb
kernelspec:
  name: python3
  display_name: Python 3
---

This page demonstrates **mystmd's native Jupyter integration** — the feature
that motivated evaluating the new stack as a successor to Sphinx's
`myst_nb` extension.

When this page is rendered, every `{code-cell}` below is executed by a
real Jupyter kernel; outputs (numbers, tables, plots) are captured and
embedded in the HTML. Execution uses mystmd's built-in caching.

## A tiny time-series example

Yuneta is heavy on time-series data. Here's a trivial example generating
and plotting a synthetic signal — the kind of snippet we'd want to embed
in architecture docs to illustrate concepts without shipping a separate
notebook file.

```{code-cell} python
:tags: [remove-stderr]

import numpy as np
import matplotlib.pyplot as plt

rng = np.random.default_rng(seed=42)
t = np.linspace(0, 4 * np.pi, 500)
signal = np.sin(t) + 0.2 * rng.standard_normal(t.shape)

fig, ax = plt.subplots(figsize=(7, 3))
ax.plot(t, signal, linewidth=1)
ax.set_xlabel("time")
ax.set_ylabel("value")
ax.grid(alpha=0.3)
plt.tight_layout()
plt.show()
```

## What this replaces

| Sphinx stack feature | mystmd equivalent |
|---|---|
| `myst_nb` execution of `.md`/`.ipynb` | Native `{code-cell}` directive in `.md` |
| `nb_execution_mode = "cache"` | Built-in execution cache |
| `sphinx-book-theme` | `book-theme` (same visual family, reimplemented) |
| `{tab-set}` / `{tab-item}` / `{list-table}` / `{dropdown}` | Native (identical syntax) |
| `{toctree}` | `project.toc` in `myst.yml` |
| `matplotlib` figure embedding | Native |

:::{tip}
To build this page locally you need:

```bash
npm install -g mystmd
pip install jupyter matplotlib numpy
cd docs/doc.yuneta.io-mystmd
myst start   # live-reloading dev server at http://localhost:3000
```

See https://mystmd.org/guide/quickstart for more.
:::
