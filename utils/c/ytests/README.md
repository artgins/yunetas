# ytests

Runner for Yuneta test suites against a live service. Supports repeat/interval options for soak testing, and the same authentication mechanisms as `ycommand` / `ybatch`.

## Usage

```bash
ytests --url ws://host:port --config <test.json> [--repeat <n>] [--interval <ms>]
```

Run `ytests --help` for the complete option list.
