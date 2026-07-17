# Realms

This document explains what a **realm** is in Yuneta, how the agent stores
and manages them, how they partition the on-disk layout, and what is *not*
scoped per realm (which surprises people).

Sibling to [`YUNO_LIFECYCLE.md`](YUNO_LIFECYCLE.md), [`DEBUGGING.md`](DEBUGGING.md),
[`IPC.md`](IPC.md). The unit of deployment (the yuno) is documented there;
the unit of *tenancy* (the realm) is documented here.

---

## 1. Mental model

A realm is a **logical partition** of a Yuneta host:

```
                  ┌─────────────────────────────────┐
                  │           Yuneta host           │
                  │     (one yuno_agent process)    │
                  └────────────────┬────────────────┘
                                   │ owns
                                   ▼
        ┌──────────────────────────────────────────────────┐
        │                  realms topic                    │
        │  id = "<name>.<role>.<env>"  (the composed URL)  │
        │  realm_owner  │  realm_role  │  realm_name  │ env│
        └──────────────────────────────────────────────────┘
                ▲                                     │
                │  parent_realm_id                    │  hook: yunos
                │  (self-fkey, hierarchical)          ▼
        ┌──────────────────────────────────────────────────┐
        │                   yunos topic                    │
        │  realm_id  ───►  (the realm above)               │
        └──────────────────────────────────────────────────┘
```

Three things live inside a realm: yunos, their working directories, and a
`bind_ip`. Things that look like they should be per-realm but **aren't**:
the port pool, the cert sync directory, and most authzs. Section 7 has the
full list.

The classic deployment pattern: one realm per project/customer (`wattyzer`,
`estadodelaire`…), all sharing the same agent on a
host.

---

## 2. Data model — the `realms` topic

Defined in [`treedb_schema_yuneta_agent.c`](https://github.com/artgins/yunetas/blob/7.8.0/yunos/c/yuno_agent/src/treedb_schema_yuneta_agent.c). Required fields (all
strings unless noted):

| Field            | Notes                                                            |
|------------------|------------------------------------------------------------------|
| `id`             | **pkey**. Format: `<realm_name>.<realm_role>.<realm_env>` — composed at create time, never edited. |
| `realm_owner`    | Tenant identifier. Used in the on-disk path. Immutable.          |
| `realm_role`     | Logical "role" of the realm (e.g. `prod`, `api`). Immutable.     |
| `realm_name`     | Human-readable name. Immutable.                                  |
| `realm_env`      | Environment label (e.g. `prod`, `staging`, `dev`). Immutable.    |

Optional fields:

| Field                | Notes                                                          |
|----------------------|----------------------------------------------------------------|
| `bind_ip`            | IP the realm's yunos listen on. Default `"127.0.0.1"`.         |
| `realm_disabled`     | Boolean, persistent. Currently advisory — see §10.5.           |
| `parent_realm_id`    | fkey → another realm. Enables hierarchical realms.             |

Two fields kept in the schema for historical reasons but flagged
**deprecated** (treedb_schema_yuneta_agent.c around the realms topic):

- `range_ports`
- `last_port`

These now live on the **agent**, not on the realm — see §7.

Hooks (relations to other topics):

| Hook         | Joined topic | Inverse fkey on the other side                |
|--------------|--------------|-----------------------------------------------|
| `realms`     | realms       | `parent_realm_id` (parent → children)         |
| `yunos`      | yunos        | `realm_id` (realm → its yunos, 1:N)           |

---

## 3. The composed URL is the identity

The `id` is **not just a label** — it is constructed at create time as

```
<realm_name>.<realm_role>.<realm_env>
```

and used as the realm's URL. Where you'll see it:

- [`c_agent.c`](https://github.com/artgins/yunetas/blob/7.8.0/yunos/c/yuno_agent/src/c_agent.c) — builds the URL when computing a yuno's working
  directory:

  ```c
  snprintf(url, sizeof(url), "%s.%s.%s", realm_name, realm_role, realm_env);
  ```

- On disk: the URL is the middle segment of the path (see §4).
- In ievent stack entries: `dst_yuno` carries the yuno name, but the realm
  is implied by which `yuno_agent` the request reached.

Because the four parts (`name`, [`role`](https://github.com/artgins/yunetas/blob/7.8.0/utils/c/yuno-skeleton/make_skeleton.c#L195), `env`, plus `owner`) are
immutable and effectively make up the identity, **the only mutable field
on a realm is `bind_ip`** (see §5).

---

## 4. On-disk layout

Built by [`build_yuno_private_domain()`](https://github.com/artgins/yunetas/blob/7.8.0/yunos/c/yuno_agent/src/c_agent.c#L7503) at [`c_agent.c`](https://github.com/artgins/yunetas/blob/7.8.0/yunos/c/yuno_agent/src/c_agent.c):

```
/yuneta/realms/<realm_owner>/<realm_name>.<realm_role>.<realm_env>/<yuno_role>_<yuno_name>/
  ├── bin/     ← materialised configuration JSON files (see YUNO_LIFECYCLE.md §2.2)
  └── logs/    ← per-yuno log files (see DEBUGGING.md §5.1)
```

The realm owns the middle segment. Each yuno gets its own
`<yuno_role>_<yuno_name>/` subdirectory.

**Important nuance**: `create-realm` does **not** create
`/yuneta/realms/<owner>/<url>/`. It only writes the treedb record. The
directory is materialised by [`mkrdir()`](#mkrdir) inside
`build_yuno_private_domain()` the first time a yuno of that realm is
started. Empty realms therefore leave no on-disk trace.

The binary repository at `/yuneta/repos/<tags>/<role>/<version>/<role>`
(see [`YUNO_LIFECYCLE.md`](YUNO_LIFECYCLE.md) §2.1) is **realm-agnostic**. A binary
is shared across all realms on the host.

---

## 5. CRUD commands

Registered in the agent's command table at [`c_agent.c`](https://github.com/artgins/yunetas/blob/7.8.0/yunos/c/yuno_agent/src/c_agent.c). Same
patterns as the other agent commands (`pm_<name>` permission schemas,
[`gobj_create_node`](#gobj_create_node) / [`gobj_update_node`](#gobj_update_node) / [`gobj_delete_node`](#gobj_delete_node) against the
treedb).

### 5.1 `create-realm`

Handler at [`c_agent.c`](https://github.com/artgins/yunetas/blob/7.8.0/yunos/c/yuno_agent/src/c_agent.c). Schema `pm_create_realm`.

Required: `realm_owner`, `realm_role`, `realm_name`, `realm_env`.
Optional: `bind_ip` (default `127.0.0.1`).

Flow:

1. Validate that none of the four required fields is empty.
2. Compose the `id` (`<name>.<role>.<env>`).
3. Check if a row with that `id` already exists. If
   yes, return *"Realm already exists"*.
4. `gobj_create_node()` — write the row.

No on-disk directory is created. No bootstrap of a "default" realm exists
(see §10.4).

### 5.2 `update-realm`

Handler at [`c_agent.c`](https://github.com/artgins/yunetas/blob/7.8.0/yunos/c/yuno_agent/src/c_agent.c). Schema `pm_update_realm`.

**The only mutable field is `bind_ip`.** The four pkey components are
not in the param schema; you cannot mutate them. To "rename" a realm,
you must delete and re-create — which is messy because yunos reference
it by `realm_id` (the immutable composed URL).

### 5.3 `delete-realm`

Handler at [`c_agent.c`](https://github.com/artgins/yunetas/blob/7.8.0/yunos/c/yuno_agent/src/c_agent.c).

**Refusal conditions**:

- If the realm has any child yunos in its `yunos` hook (iterate the
  hook and bail if `json_array_size > 0`). The check is
  *existence*, not *running* — you must `delete-yuno` every yuno first.

On success: removes the treedb row (`gobj_delete_node()`). The
on-disk directory `/yuneta/realms/<owner>/<url>/`, if it ever was
created, is **not** removed automatically — orphan directories survive
realm deletion. Clean up by hand if it matters.

---

## 6. Realm ↔ yunos

The link is the `yunos.realm_id` fkey
([`treedb_schema_yuneta_agent.c`](https://github.com/artgins/yunetas/blob/7.8.0/yunos/c/yuno_agent/src/treedb_schema_yuneta_agent.c)). One realm has N yunos
(`realms.yunos` hook); one yuno belongs to exactly one realm.

At `run-yuno` time the agent reads the realm record to compute the
working directory ([`c_agent.c`](https://github.com/artgins/yunetas/blob/7.8.0/yunos/c/yuno_agent/src/c_agent.c)):

```c
json_t *realm   = get_yuno_realm(gobj, yuno);
realm_owner    = kw_get_str(gobj, realm, "realm_owner", …);
realm_role     = kw_get_str(gobj, realm, "realm_role",  …);
realm_name     = kw_get_str(gobj, realm, "realm_name",  …);
realm_env      = kw_get_str(gobj, realm, "realm_env",   …);
snprintf(url, …, "%s.%s.%s", realm_name, realm_role, realm_env);
build_path(bf, bfsize, "realms", realm_owner, url, role_plus_id, NULL);
```

Implication: `update-realm` cannot change the working-dir path because
the four contributing fields are immutable. The path is stable for the
realm's lifetime.

---

## 7. What is **not** scoped per realm

This is where production gotchas live. The fact that some things look
realm-scoped but aren't is the most common cause of surprises.

### 7.1 Port pool (`range_ports` / `last_port`)

Agent-level attributes ([`c_agent.c`](https://github.com/artgins/yunetas/blob/7.8.0/yunos/c/yuno_agent/src/c_agent.c)):

```
range_ports = "[[11100,11199]]"   ← default range
last_port   = 0
```

Allocation in [`get_new_service_port()`](https://github.com/artgins/yunetas/blob/7.8.0/yunos/c/yuno_agent/src/c_agent.c#L9207) at [`c_agent.c`](https://github.com/artgins/yunetas/blob/7.8.0/yunos/c/yuno_agent/src/c_agent.c) reads the
agent's `range_ports`, advances `last_port` globally, and hands out the
next port. **All realms on the host share the same pool.** Two realms
asking for an automatic port get adjacent ports from the same range,
period.

The deprecated `range_ports`/`last_port` columns still listed in the
realms schema (see §2) are vestigial — the runtime ignores them. Don't
write code that reads them.

### 7.2 Cert sync directory

Agent-level ([`c_agent.c`](https://github.com/artgins/yunetas/blob/7.8.0/yunos/c/yuno_agent/src/c_agent.c)): a single
`cert_sync_store_dir` (default `/yuneta/store/certs`) is watched.
Changes broadcast a `reload-certs` event to **every** yuno on the host,
regardless of realm.

If two realms need disjoint cert sets, you cannot get that from the
agent's cert-sync mechanism — you have to ship the certs directly into
each yuno's config and skip cert-sync.

### 7.3 `__username__` / authzs

The agent has one `__username__` attribute ([`c_agent.c`](https://github.com/artgins/yunetas/blob/7.8.0/yunos/c/yuno_agent/src/c_agent.c)) with
"permission for all" semantics. Per-command `pm_<name>` schemas gate
access, but the gating is against the *caller's* user, not against the
realm. There is no built-in *"user X may read realm R but not realm S"*
predicate in the agent itself. If you need that, the `auth_bff` yuno is
the place to enforce it (see the upcoming YUNO_AUTH.md).

### 7.4 The binary repository

`/yuneta/repos/<tags>/<role>/<version>/<role>` is host-global. A binary
uploaded for one realm is visible to (and runnable by) yunos in every
realm. The `binaries` topic does not have a realm fkey.

### 7.5 Audit log

`use_audit_command_file` + `max_megas_audit_file` are agent-level
attributes — one audit file per host, not per realm.

---

## 8. `bind_ip` — the realm's listen address

The one truly per-realm operational knob. Default `127.0.0.1`
(loopback only). On production hosts you almost always want this changed
to the public/internal IP that the realm should listen on.

`bind_ip` is the IP **for the yunos in this realm to bind their
listening sockets to**, not a routing rule. Each yuno reads it from its
realm record at start and uses it where its config refers to `bind_ip`
(typically by templating a `tcp://${bind_ip}:${port}` URL).

Set at create time, mutable via `update-realm`:

```bash
ycommand -c 'update-realm id=<name>.<role>.<env> bind_ip=10.0.0.5'
ycommand -c 'kill-yuno yuno_role=<role>'   # for each affected yuno
ycommand -c 'run-yuno'                     # pick up the new bind_ip
```

A change to `bind_ip` does **not** restart anything automatically.

---

## 9. Hierarchical realms (`parent_realm_id`)

The `realms` topic has a self-fkey `parent_realm_id` and a hook back to
the same topic. This permits a tree of realms (a parent realm with child
realms). Current runtime usage is limited — the field is mainly
informational (`controlcenter` displays the tree). Nothing in
`build_yuno_private_domain` walks the parent chain when computing paths;
the working dir uses only the realm's own four immutable fields.

Treat it as metadata. Don't build infrastructure that assumes a child
realm inherits anything operational from its parent.

---

## 10. Sharp edges

### 10.1 The four pkey components are immutable

`realm_owner`, `realm_role`, `realm_name`, `realm_env`. There is no
rename. If you got one wrong, you must delete the realm (after deleting
its yunos) and re-create it.

### 10.2 `delete-realm` only refuses on *existence* of yunos, not on
*running* state

It iterates the `yunos` hook and bails if any rows exist. If you
`disable-yuno` everything and forget to `delete-yuno` first,
`delete-realm` still refuses. The error message tells you, but it can
be confusing the first time.

### 10.3 The on-disk realm directory is never cleaned up

Both at `delete-realm` and on a host migration, the directory under
`/yuneta/realms/<owner>/<url>/` is left behind. If you reuse the same
realm coordinates later, you'll inherit whatever was in there. Sweep by
hand when retiring a realm.

### 10.4 No default realm bootstrap

The agent does not create a realm on first run. A brand-new agent with
no realm cannot run any yuno (the yuno create requires `realm_id`).
First action on a new host is always `create-realm`.

### 10.5 `realm_disabled` is advisory

The field exists in the schema and you can set it via the treedb tools.
Nothing in [`c_agent.c`](https://github.com/artgins/yunetas/blob/7.8.0/yunos/c/yuno_agent/src/c_agent.c)'s start path checks it before running yunos of a
disabled realm. Don't trust it as a kill switch — it's information for
operators, not enforcement.

### 10.6 The deprecated `range_ports`/`last_port` on realms are *traps*

They're still in the schema. Code that "reads the realm's port range"
will read stale data. The runtime ignores those fields. The real port
pool is on the agent ([`c_agent.c`](https://github.com/artgins/yunetas/blob/7.8.0/yunos/c/yuno_agent/src/c_agent.c)). If you find code reading
`realm.range_ports`, that's a bug — fix it to read the agent's.

### 10.7 One agent per host means one port pool per host

If you need disjoint port pools for security/audit reasons, you need
**two agents** on the host (or two hosts). You cannot achieve it through
realms alone.

### 10.8 The cert sync `reload-certs` event is broadcast to all yunos

A cert change is not silently filtered by realm — every yuno receives
the reload event and decides what to do with it. Yunos that don't use
TLS just ignore the event; yunos that do, reload. There's no way to
scope a cert change to one realm without partitioning yunos differently
(e.g. shipping cert paths via per-yuno config, bypassing cert sync).

---

## 11. Recipes

### 11.1 Create a realm for a new project

```bash
# 1. create the realm
ycommand -c 'create-realm realm_owner=acme realm_role=prod realm_name=widgets realm_env=production bind_ip=10.0.0.7'

# 2. list to confirm
ycommand -c 'list-realms'

# 3. … now you can create yunos against it
ycommand -c 'create-yuno realm_id=widgets.prod.production yuno_role=my_service yuno_name=instance_01'
```

### 11.2 Move all yunos out of a realm before deleting it

```bash
REALM_ID=widgets.prod.production

# disable each yuno
for y in $(ycommand -j -c "list-yunos realm_id=$REALM_ID" | jq -r '.[].id'); do
    ycommand -c "kill-yuno    id=$y"
    ycommand -c "disable-yuno id=$y"
    ycommand -c "delete-yuno  id=$y"
done

# now the realm is empty
ycommand -c "delete-realm id=$REALM_ID"

# sweep the leftover directory by hand (the agent does not)
sudo rm -rf "/yuneta/realms/acme/$REALM_ID"
```

### 11.3 Change the bind_ip of a realm

```bash
REALM_ID=widgets.prod.production
ycommand -c "update-realm id=$REALM_ID bind_ip=10.0.0.99"

# update takes effect on next yuno start
for y in $(ycommand -j -c "list-yunos realm_id=$REALM_ID" | jq -r '.[].id'); do
    ycommand -c "kill-yuno id=$y"
done
ycommand -c "run-yuno"
```

### 11.4 List which yunos belong to which realm

```bash
ycommand -c 'list-realms'                                 # all realms
ycommand -c 'list-yunos realm_id=widgets.prod.production' # one realm
ycommand -c 'list-yunos'                                  # all yunos with their realm_id
```

---

## 12. Code pointers

| What                                              | Where                                                              |
|---------------------------------------------------|--------------------------------------------------------------------|
| `realms` topic schema                             | [`treedb_schema_yuneta_agent.c`](https://github.com/artgins/yunetas/blob/7.8.0/yunos/c/yuno_agent/src/treedb_schema_yuneta_agent.c)                             |
| `create-realm` handler                            | [`yunos/c/yuno_agent/src/c_agent.c`](https://github.com/artgins/yunetas/blob/7.8.0/yunos/c/yuno_agent/src/c_agent.c)                       |
| `update-realm` handler                            | [`c_agent.c`](https://github.com/artgins/yunetas/blob/7.8.0/yunos/c/yuno_agent/src/c_agent.c)                                              |
| `delete-realm` handler                            | [`c_agent.c`](https://github.com/artgins/yunetas/blob/7.8.0/yunos/c/yuno_agent/src/c_agent.c)                                              |
| Realm URL composition (`<name>.<role>.<env>`)     | [`c_agent.c`](https://github.com/artgins/yunetas/blob/7.8.0/yunos/c/yuno_agent/src/c_agent.c)                                              |
| Yuno working-dir from realm                       | [`c_agent.c`](https://github.com/artgins/yunetas/blob/7.8.0/yunos/c/yuno_agent/src/c_agent.c) (`build_yuno_private_domain`)                |
| Agent's port pool                                 | [`c_agent.c`](https://github.com/artgins/yunetas/blob/7.8.0/yunos/c/yuno_agent/src/c_agent.c)                                     |
| Agent's cert sync attrs                           | [`c_agent.c`](https://github.com/artgins/yunetas/blob/7.8.0/yunos/c/yuno_agent/src/c_agent.c)                                                |
| `bind_ip` default                                 | [`treedb_schema_yuneta_agent.c`](https://github.com/artgins/yunetas/blob/7.8.0/yunos/c/yuno_agent/src/treedb_schema_yuneta_agent.c) (realms topic, `bind_ip` field)     |
| `parent_realm_id` self-fkey                       | [`treedb_schema_yuneta_agent.c`](https://github.com/artgins/yunetas/blob/7.8.0/yunos/c/yuno_agent/src/treedb_schema_yuneta_agent.c)                                 |
| `realms.yunos` hook                               | [`treedb_schema_yuneta_agent.c`](https://github.com/artgins/yunetas/blob/7.8.0/yunos/c/yuno_agent/src/treedb_schema_yuneta_agent.c)                                 |
