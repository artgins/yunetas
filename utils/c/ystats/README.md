# ystats

ystats subscribes to the statistics of a Yuneta service and shows them in real time. It has a refresh interval and a filter by statistic or attribute name. It can keep more than one subscription at the same time.

## Usage

```bash
ystats --url ws://host:port [--interval <s>] [--stats <names>] [--attrs <names>]
```

The authentication options are the same as those of `ycommand`. Run `ystats --help` to see all the options.
