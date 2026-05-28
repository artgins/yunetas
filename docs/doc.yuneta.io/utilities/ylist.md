(util-ylist)=
# `ylist`

`ylist` lists the Yuneta processes (yunos) running on the **local** machine,
with their process information. Use it for a quick status check, or `--pids` for
scripted pid lookups.

## Usage

```bash
ylist              # full listing
ylist --pids       # pids only (for scripting)
```

## See also

- [`ycommand -c 'list-yunos'`](ycommand.md) — the agent's view of managed yunos
  (id, role, status), as opposed to `ylist`'s local-process view.
- [`utils/c/ylist/README.md`](https://github.com/artgins/yunetas/blob/main/utils/c/ylist/README.md).
