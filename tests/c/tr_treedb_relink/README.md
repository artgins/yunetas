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
   alone, refused (*"Cannot unlink, the child does not hang from that parent"*):
   no `EV_TREEDB_NODE_UNLINKED`, and the child is not saved again.
3. The old parent is deleted without `force`.
4. A forced delete of an old parent does not touch the child.
5. After a reload the child hangs from the last parent it was linked to.

## Cycles

A hook holds the child NODE, so a cycle of links is a cycle of json
references. Before this test covered it, a cycle was accepted, survived a
reload, and leaked at every `treedb_close_db()` (15 676 bytes for two nodes).
A cycle in ONE hook also sent `children recursive=1` and `jtree` into endless
recursion, with no visited set and no depth limit. The test checks:

6. A link that would hang a node from its own descendant through the SAME
   hook is refused (*"Cannot link, the link would close a cycle in the hook"*).
   A tree is a tree.
7. A cycle through TWO hooks is data, and is accepted: a department manages a
   department it contains. The second hook, as written in the schema:

   ```c
   'manager': {
       'header': 'Manager',
       'fillspace': 20,
       'type': 'array',
       'flag': ['fkey']
   },
   'managers': {
       'header': 'Managers',
       'fillspace': 20,
       'type': 'object',
       'flag': ['hook'],
       'hook': {
           'departments': 'manager'
       }
   }
   ```

8. A store that already holds a cycle in one hook (written as a raw record,
   the way an old store has it) reloads, `jtree` and `children recursive` end
   and say *"Cycle in the hook, node not followed again"*, and the close frees
   every node: the memory check at the end of the test is the assertion.
