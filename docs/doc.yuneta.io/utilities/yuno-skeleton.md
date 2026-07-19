(util-yuno-skeleton)=
# `yuno-skeleton`

Instantiate a new yuno (or gclass) from a named template in the skeleton
library.

## Usage

```bash
yuno-skeleton --list                 # list available skeletons
yuno-skeleton <SKELETON>             # instantiate one
```

| Option | Purpose |
|--------|---------|
| `<SKELETON>` | Template to instantiate (positional) |
| `--list` / `-l` | List available skeletons |
| `--skeletons-path` / `-p` | Skeleton directory (default `/yuneta/bin/skeletons`) |

Run with no skeleton to print the available list. The templates are the ones
under `utils/c/yuno-skeleton/skeletons/`.

## See also

- [Scaffolding new yunos](../../../yunos/c/yuno_agent/SCAFFOLDING.md).
- [`utils/c/yuno-skeleton/README.md`](https://github.com/artgins/yunetas/blob/7.8.4/utils/c/yuno-skeleton/README.md).
