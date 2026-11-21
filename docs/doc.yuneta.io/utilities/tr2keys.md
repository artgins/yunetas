(util-tr2keys)=
# `tr2keys`

List the keys (primary ids) of a timeranger2 topic. It reads the same store
as [`tr2list`](tr2list.md), but prints only the keys.

## Usage

```bash
tr2keys PATH [options]
```

| Option | What it does |
|--------|--------------|
| `--key=KEY` | Show only this key, if the topic has it. |
| `--rkey=RKEY` | Show only the keys that match this regular expression (PCRE2). |
| `--recursive` / `-r` | List every topic under `PATH`. |
| `--list-databases` | List the databases under `PATH`. |

The topic and the database come from `PATH`. Without `--key` or `--rkey`,
`tr2keys` prints every key of the topic. With one of them, it prints only the
keys that match, and with `--recursive` only the topics that have a key that
matches. A pattern that does not compile stops `tr2keys` before it opens a
topic, with the cause.

```bash
# Does the topic have this key?
tr2keys /yuneta/store/myapp/treedb_myapp/devices --key=dev-0042

# The keys that start with "dev-00", in every topic of the database
tr2keys /yuneta/store/myapp/treedb_myapp -r --rkey='^dev-00'
```

`rkey` is for operators and tools like this one. A GUI that checks given
keys asks for them exactly, with the `key` parameter of `list-keys`.

## See also

- [`tr2list`](tr2list.md) — full records. [`tr2search`](tr2search.md) — content search.
- [`utils/c/tr2keys/README.md`](https://github.com/artgins/yunetas/blob/7.31.29/utils/c/tr2keys/README.md).
