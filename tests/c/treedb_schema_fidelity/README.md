# treedb_schema_fidelity test

Every real schema that ships in this tree, through the projection into the
`__system__` treedb and back: each one is opened from its own projection, and
**every attribute the literal declares must come back with the same value**.
Attributes the literal does not declare are ignored — the projection is
allowed to fill in the defaults a column gets anyway.

It exists because what the projection drops, it drops in silence. A column
that loses its `enum` list keeps its `enum` **flag**, so it declares an
enumeration it no longer has and every value passes. `enum`, `template`,
`pkey2s` and scalar `default`s were all being lost that way.

Covered: `authzs`, `mqtt_broker`, `controlcenter`, `yuneta_agent` — between
them they use `enum`, `template`, scalar defaults, hooks, fkeys and `pkey2s`.
The schemas of the project repos (wattyzer, estadodelaire, hidraulia,
yunovatios) cannot be reached from here, and must not be: the SDK does not
depend on projects.

**Each schema gets its own store, on purpose.** `__system__` keys its `topics`
topic by the bare topic name, so two treedbs sharing one (`users` is in three
of these four) collide in a single store — a real limit recorded in
`TODO.md`, and not what this sweep measures.

The log capture is at WARNING and above: opening four treedbs is noisy, and
what matters is that a clean sweep says nothing at all.

## Run

```bash
ctest -R test_treedb_schema_fidelity --output-on-failure --test-dir build
```
