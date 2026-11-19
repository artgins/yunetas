# ytestconfig

`ytestconfig` reads a JSON configuration file and reports each syntax or structural error. Run it before you start a yuno with that file.

## Usage

```bash
ytestconfig <config.json>
```

If the parse fails, the exit code is not zero.
