# tr_treedb_relink test

Tests a link into a **single-valued** fkey (a `string` column with the `fkey`
flag), where a new link replaces the old one.

Before the fix, `_link_nodes()` wrote the new reference into the child and
left the child in the hook of the parent it hung from before. That parent kept
a phantom child: a delete without `force` was refused (*"has down links"*),
and a delete with `force` "unlinked" the phantom, which cleared the child's
reference, the one naming the NEW parent, and saved it. After a reload the
child hung from nobody. `_unlink_nodes()` made that possible: it emptied a
string reference without checking that it named the parent being unlinked.

Now a link first unlinks the child from the parent its string names, which
also emits `EV_TREEDB_NODE_UNLINKED` for it, and an unlink only clears a string
reference that names that parent.

The column, as a C schema literal:

```c
'department_id': {
    'header': 'Top Department',
    'fillspace': 20,
    'type': 'string',
    'flag': ['fkey']
}
```

The test checks:

1. A second link moves the child: the old parent's hook no longer holds it.
2. An unlink from a parent the child does not hang from leaves its reference
   alone (*"Parent ref not found in string child data"*).
3. The old parent is deleted without `force`.
4. A forced delete of an old parent does not touch the child.
5. After a reload the child hangs from the last parent it was linked to.
