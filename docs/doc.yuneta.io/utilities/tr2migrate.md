(util-tr2migrate)=
# `tr2migrate`

One-shot bulk migration of legacy **timeranger (v1)** on-disk data into
**timeranger2**.

## Usage

```bash
tr2migrate PATH_TRANGER PATH_TRANGER2 [--verbose <level>]
```

- `PATH_TRANGER` — source (old timeranger) — required.
- `PATH_TRANGER2` — destination (new timeranger2) — required.
- `--verbose` / `-l` — verbosity level.

## See also

- [`utils/c/tr2migrate/README.md`](https://github.com/artgins/yunetas/blob/7.8.2/utils/c/tr2migrate/README.md).
