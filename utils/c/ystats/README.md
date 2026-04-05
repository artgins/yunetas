# ystats

Subscribe to and display real-time statistics from a Yuneta service. Supports a refresh interval, filtering by stat/attribute name, and multiple concurrent subscriptions.

## Usage

```bash
ystats --url ws://host:port [--interval <s>] [--stats <names>] [--attrs <names>]
```

Authentication options are the same as `ycommand`. Run `ystats --help` for details.
