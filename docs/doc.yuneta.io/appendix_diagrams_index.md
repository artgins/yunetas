---
title: 'Appendix: Diagram Index'
description: >-
  Visual index of every diagram in the documentation, grouped by theme,
  each linking to the page where it appears in context.
---

# Appendix: Diagram Index

Every diagram in the documentation, grouped by theme. Click a card to open the
page where the diagram appears in context. All are hand-authored SVG (they
scale crisply and render on GitHub too).

## The domain model

::::{grid} 1 2 2 3

:::{card}
:link: ./philosophy/domain_model.md
![CRUDLU](./_static/crudlu_model.svg)

**CRUDLU** — CRUD on entities, Link / Unlink on relationships.
:::

:::{card}
:link: ./philosophy/domain_model.md
![Entity/Relationship](./_static/entity_relationship.svg)

**Entity / Relationship** — reality as entities + the relationships between them.
:::

:::{card}
:link: ./philosophy/domain_model.md
![Entities](./_static/entities.svg)

**Entities** — one Role (class / topic) and its many instances.
:::

:::{card}
:link: ./philosophy/domain_model.md
![Relationships](./_static/relationships.svg)

**Relationships** — the Parent/Child and Client/Service pair models.
:::

:::{card}
:link: ./philosophy/typed_graph_model.md
![treedb graph](./_static/treedb_graph.svg)

**TreeDB graph** — topics, nodes, hooks and fkeys; port colour = linked topic.
:::

:::{card}
:link: ./philosophy/typed_graph_model.md
![node is its history](./_static/node_history.svg)

**Node is its history** — `node`ⁿ as an append timeline; a link is an event.
:::

::::

## Runtime: gobj, lifecycle & scaffolding

::::{grid} 1 2 2 3

:::{card}
:link: ../../yunos/c/yuno_agent/GOBJ.md
![gobj tree](./_static/gobj_tree.svg)

**Gobj runtime tree** — `__yuno__` children + the bottom chain for attributes.
:::

:::{card}
:link: ../../yunos/c/yuno_agent/ENTRY_POINT.md
![process tree](./_static/process_tree.svg)

**Process tree** — grandparent / watcher / yuno; two pids per yuno.
:::

:::{card}
:link: ../../yunos/c/yuno_agent/YUNO_LIFECYCLE.md
![lifecycle FSM](./_static/yuno_lifecycle_fsm.svg)

**Yuno lifecycle FSM** — create → run → play ⇄ pause → kill → delete.
:::

:::{card}
:link: ./guide/guide_gclass.md
![gclass anatomy](./_static/gclass_anatomy.svg)

**GClass anatomy** — the eleven components, grouped.
:::

:::{card}
:link: ../../yunos/c/yuno_agent/SCAFFOLDING.md
![scaffolding tree](./_static/scaffolding_tree.svg)

**Scaffolding decision tree** — which `yuno-skeleton` template to pick.
:::

::::

## Messaging & IPC

::::{grid} 1 2 2 3

:::{card}
:link: ../../yunos/c/yuno_agent/IPC.md
![message pipeline](./_static/message_pipeline.svg)

**Message pipeline** — bytes climb the gate stack to a service; traces per layer.
:::

:::{card}
:link: ../../yunos/c/yuno_agent/IPC.md
![ievent WS stack](./_static/ievent_stack.svg)

**ievent stack across the WS boundary** — two yunos, CLI ⇄ SRV.
:::

::::

## Persistence (timeranger2 / treedb)

::::{grid} 1 2 2 3

:::{card}
:link: ../../yunos/c/yuno_agent/YUNO_TREEDB.md
![md2 record](./_static/md2_record.svg)

**md2 record + O(1) lookup** — rowid × 32 → `.md2` → offset/size → `.json`.
:::

:::{card}
:link: ../../yunos/c/yuno_agent/YUNO_TREEDB.md
![on-disk layout](./_static/treedb_ondisk.svg)

**On-disk layout** — `keys/` real files vs `disks/` hardlink slots.
:::

::::

## Auth & TLS

::::{grid} 1 2 2 3

:::{card}
:link: ../../yunos/c/yuno_agent/YUNO_AUTH.md
![auth flow](./_static/auth_flow.svg)

**OIDC / PKCE auth flow** — login + per-request, with the disabled authz gate.
:::

:::{card}
:link: ./guide/guide_authz.md
![authz decision](./_static/authz_decision.svg)

**Authz decision** — checker precedence → TRUE / FALSE.
:::

:::{card}
:link: ./guide/guide_cert_management.md
![cert hot-swap](./_static/cert_hotswap.svg)

**Cert hot-swap** — `old_ctx` kept alive by live sessions until they close.
:::

:::{card}
:link: ./guide/guide_cert_management.md
![cert defense](./_static/cert_defense.svg)

**Cert defense in depth** — three layers → `ytls_reload_certificates()`.
:::

::::

## Operations, build & buffers

::::{grid} 1 2 2 3

:::{card}
:link: ./yunos/controlcenter.md
![control-center topology](./_static/controlcenter_topology.svg)

**Control-center topology** — nodes dial in; `command-agent` routed back.
:::

:::{card}
:link: ./yunos/emailsender.md
![emailsender flow](./_static/emailsender_flow.svg)

**Emailsender retry / dead-letter** — queue → SMTP → sent / retry / failed.
:::

:::{card}
:link: ./yunos/logcenter.md
![logcenter pipeline](./_static/logcenter_pipeline.svg)

**logcenter ingest** — UDP datagram → verify → count / rotatory / summary.
:::

:::{card}
:link: ./tools/sync_binaries.md
![deploy/sync](./_static/deploy_sync.svg)

**Deploy / sync cycle** — build → diff → REBUILD hot-patch by start_priority.
:::

:::{card}
:link: ./guide/folders.md
![build stack](./_static/build_stack.svg)

**Build dependency stack** — gobj-c → … → yunos → tests.
:::

:::{card}
:link: ./guide/guide_gbuffer.md
![gbuffer layout](./_static/gbuffer_layout.svg)

**gbuffer layout** — independent `curp` (read) / `tail` (write) cursors.
:::

::::
