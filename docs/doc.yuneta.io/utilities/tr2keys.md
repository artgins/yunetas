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
| `--key=KEY` | Show only this key, if the topic has it. It wins over `--rkey`. |
| `--rkey=RKEY` | Show only the keys that match this regular expression (PCRE2). |
| `--recursive` / `-r` | List every topic under `PATH`. |
| `--list-databases` | List the databases under `PATH`. |

`tr2keys` takes the topic and the database from `PATH`. Without `--key` or `--rkey`,
`tr2keys` prints every key of the topic. With one of them, it prints only the
keys that match, and with `--recursive` only the topics that have a key that
matches. `--key` looks up the key. `--rkey` is judged as `tr2list --rkey` is
judged ([`tranger2_list_keys_by_rkey()`](<#tranger2_list_keys_by_rkey>)), so
the two tools choose the same keys. A pattern that the judgement refuses (it
does not compile, it has a refused form, or its judgements pass 2 s) prints the
cause for each topic, and `tr2keys` exits 1:

```bash
tr2keys /yuneta/store/myapp/treedb_myapp/devices --rkey='a(*COMMIT)b'
# Cannot list the keys of treedb_myapp/devices: Bad rkey regex 'a(*COMMIT)b': (*COMMIT) is refused in an rkey
```

7.31.29 had no `--key` and no `--rkey`: `tr2keys` printed every key.

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
- [`utils/c/tr2keys/README.md`](https://github.com/artgins/yunetas/blob/7.32.1/utils/c/tr2keys/README.md).
