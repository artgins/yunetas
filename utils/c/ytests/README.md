# ytests

`ytests` runs Yuneta test suites against a live service. It has repeat and interval options for soak tests (long tests under continuous load). It uses the same authentication mechanisms as `ycommand` and `ybatch`.

## Usage

```bash
ytests --url ws://host:port --config <test.json> [--repeat <n>] [--interval <ms>]
```

Run `ytests --help` for the complete option list.
