# c_node_paged_nodes test

Tests the paging of `C_NODE`'s `nodes` command.

A treedb lives in memory, so walking it is not what costs: serializing every
node and pushing it through a websocket is. `nodes` therefore cuts the answer
on the way out, with the same contract `list-keys` of `C_TRANGER` uses:

- No `limit` → the plain list it always answered, so every client
  written before this keeps working.
- A `limit` → the envelope `{total_rows, pages, data}`, the same one
  `get-page` uses, so a client pages nodes exactly as it pages records.

`from` is 1-based.

## What it pins

1. No limit answers the plain list, unchanged.
2. A limit answers the envelope, with the right totals and the first page.
3. `from` is 1-based (`from=4` starts at the fourth node).
4. The last page holds what is left, and a page past the end is empty
   without lying about the total.
5. `from` / `limit` survive when they arrive as strings. That is how they arrive
   through the agent's `command-yuno` forwarding, which does not coerce
   `SDF_REQUIRED` / `DTP_INTEGER` parameters. That is what `KW_WILD_NUMBER` is
   there for, and it is the half of this that a JSON-only test misses.

## Run

```bash
ctest -R test_c_node_paged_nodes --output-on-failure --test-dir build
```
