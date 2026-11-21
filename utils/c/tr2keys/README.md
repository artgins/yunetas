# tr2keys

`tr2keys` lists the keys (primary ids) of a timeranger2 topic. It is like `tr2list`, but it prints only the set of keys. Use it to audit a topic or to give the keys to a batch job.

## Usage

```bash
tr2keys <PATH> [options]
```

`PATH` is a positional argument: the directory of a topic, or of a database with `--recursive`.

| Option | What it does |
|--------|--------------|
| `--key=KEY` | Show only this key, if the topic has it. It wins over `--rkey`. |
| `--rkey=RKEY` | Show only the keys that match this regular expression (PCRE2). |
| `--recursive` / `-r` | List every topic under `PATH`. |
| `--list-databases` | List the databases under `PATH`. |

Without `--key` or `--rkey`, `tr2keys` prints every key of the topic. With one of them, it prints only the keys that match, and with `--recursive` only the topics that have a key that matches. `--key` looks up the key. `--rkey` is judged by timeranger2 as `tr2list --rkey` is judged (`tranger2_list_keys_by_rkey()`), so the two tools choose the same keys. A pattern that the judgement refuses (it does not compile, it has a refused form such as `(*COMMIT)`, or its judgements pass 2 s) prints the cause for each topic, and `tr2keys` exits 1. Up to 7.31.29 `tr2keys` compiled `--rkey` with no options and no limits.

## Examples

```bash
# Every key of a topic
tr2keys /yuneta/store/myapp/treedb_myapp/devices

# Does the topic have this key?
tr2keys /yuneta/store/myapp/treedb_myapp/devices --key=dev-0042

# The keys that start with "dev-00"
tr2keys /yuneta/store/myapp/treedb_myapp/devices --rkey='^dev-00'

# In every topic of the database
tr2keys /yuneta/store/myapp/treedb_myapp -r --rkey='^dev-00'
```
