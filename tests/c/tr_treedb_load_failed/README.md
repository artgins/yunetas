# tr_treedb_load_failed test

A treedb topic whose keys cannot all be read. treedb loads each topic with
`tranger2_open_list()` of every key. A key whose history does not load whole
is named in `load_failed_keys`; treedb loads the other keys and remembers the
names, and its guards refuse what they would answer wrong.

| Case | What it checks |
|---|---|
| 1 | The topic loads every key it can read, and its realtime feed is open. |
| 2 | A create of an id that did not load is refused. A create of another id is accepted. |
| 3 | A `__snaps__` that did not load whole: `shoot-snap`, `activate-snap` and the delete of a node refuse, because which snap is active, or holds the node, is unknown. |
| 4a | A md2 of 0 rows beside a content file that is not empty (an append that was never acknowledged) as the newest file of `k2`: the file is ignored with a warning, `k2` is in memory with its previous version, and nothing is flagged. |
| 4b | The same file between two good versions of `k2`: `k2` is in memory with the newest one. |
| 5 | After a restart, the md2 of `k2` cannot be read (mode 000): `k2` is not in memory, and a create of it is refused. |
| 6 | After a restart, the content of `k2` cut to 0 bytes: the same. A record whose content cannot be read does not become a node with id `""`. |
| 7 | After a restart, a `__snaps__` md2 that cannot be read (mode 000): shoot, activate and delete refuse. |
| 8 | The recovery: after the key is deleted, a create of its id is accepted without a reopen of the treedb. |
| 9 | A parent whose child topic did not load whole (`things`, key `t2` cut) is not deleted, forced or not, with or without children in memory: a child that did not load may hang from it. It used to unlink the children in memory, delete the parent, and leave `t2` naming a parent that is gone. |

Cases 5 and 7 are skipped as root, because root can read a file of mode 000.
A md2 whose last row is torn is not a case here: it is not damage, and a
master cuts it back (timeranger2's `test_torn_md2_tail`).

The operator's side of these cases (how to find the file and repair it) is in
the treedb API page, "A topic that did not load whole".

## Run

```bash
ctest -R '^test_tr_treedb_load_failed$' --output-on-failure --test-dir build
```
