---
title: 'Appendix: Diagram Index'
description: >-
  Visual index of every diagram in the documentation, grouped by theme,
  each linking to the section where it appears in context.
---

# Appendix: Diagram Index

Every diagram in the documentation, grouped by theme. Click a card to jump to
the section where the diagram appears in context. All are hand-authored SVG
(they scale crisply and render on GitHub too).

## The domain model

::::{grid} 1 2 2 3

:::{card}
:link: ./philosophy/domain_model.md#crudlu
![CRUDLU](./_static/crudlu_model.svg)

**CRUDLU** — CRUD on entities, Link / Unlink on relationships.
:::

:::{card}
:link: ./philosophy/domain_model.md#entity-relationship-model
![Entity/Relationship](./_static/entity_relationship.svg)

**Entity / Relationship** — reality as entities + the relationships between them.
:::

:::{card}
:link: ./philosophy/domain_model.md#entities
![Entities](./_static/entities.svg)

**Entities** — one Role (class / topic) and its many instances.
:::

:::{card}
:link: ./philosophy/domain_model.md#relationships
![Relationships](./_static/relationships.svg)

**Relationships** — the Parent/Child and Client/Service pair models.
:::

:::{card}
:link: ./philosophy/typed_graph_model.md#the-unit-is-not-node-edge-it-is-typed-instance-with-typed-bindings
![treedb graph](./_static/treedb_graph.svg)

**TreeDB graph** — topics, nodes, hooks and fkeys; port colour = linked topic.
:::

:::{card}
:link: ./philosophy/typed_graph_model.md#what-this-lets-you-model
![node is its history](./_static/node_history.svg)

**Node is its history** — `node`ⁿ as an append timeline; a link is an event.
:::

::::

## Runtime: gobj, lifecycle & scaffolding

::::{grid} 1 2 2 3

:::{card}
:link: ../../yunos/c/yuno_agent/GOBJ.md#id-6-the-runtime-tree
![gobj tree](./_static/gobj_tree.svg)

**Gobj runtime tree** — `__yuno__` children + the bottom chain for attributes.
:::

:::{card}
:link: ../../yunos/c/yuno_agent/ENTRY_POINT.md#id-1-the-picture
![process tree](./_static/process_tree.svg)

**Process tree** — grandparent / watcher / yuno; two pids per yuno.
:::

:::{card}
:link: ../../yunos/c/yuno_agent/YUNO_LIFECYCLE.md#id-4-1-state-machine-of-a-single-yuno-as-the-agent-sees-it
![lifecycle FSM](./_static/yuno_lifecycle_fsm.svg)

**Yuno lifecycle FSM** — create → run → play ⇄ pause → kill → delete.
:::

:::{card}
:link: ./guide/guide_gclass.md#components-of-a-gclass
![gclass anatomy](./_static/gclass_anatomy.svg)

**GClass anatomy** — the eleven components, grouped.
:::

:::{card}
:link: ../../yunos/c/yuno_agent/SCAFFOLDING.md#id-1-which-template-to-pick
![scaffolding tree](./_static/scaffolding_tree.svg)

**Scaffolding decision tree** — which `yuno-skeleton` template to pick.
:::

::::

## Messaging & IPC

::::{grid} 1 2 2 3

:::{card}
:link: ../../yunos/c/yuno_agent/IPC.md#id-6-gates-how-external-traffic-becomes-events
![message pipeline](./_static/message_pipeline.svg)

**Message pipeline** — bytes climb the gate stack to a service; traces per layer.
:::

:::{card}
:link: ../../yunos/c/yuno_agent/IPC.md#id-4-1-the-two-ends
![ievent WS stack](./_static/ievent_stack.svg)

**ievent stack across the WS boundary** — two yunos, CLI ⇄ SRV.
:::

::::

## Persistence (timeranger2 / treedb)

::::{grid} 1 2 2 3

:::{card}
:link: ../../yunos/c/yuno_agent/YUNO_TREEDB.md#id-2-2-records-and-the-md2-index
![md2 record](./_static/md2_record.svg)

**md2 record + O(1) lookup** — rowid × 32 → `.md2` → offset/size → `.json`.
:::

:::{card}
:link: ../../yunos/c/yuno_agent/YUNO_TREEDB.md#id-2-1-the-on-disk-layout
![on-disk layout](./_static/treedb_ondisk.svg)

**On-disk layout** — `keys/` real files vs `disks/` hardlink slots.
:::

::::

## Auth & TLS

::::{grid} 1 2 2 3

:::{card}
:link: ./guide/guide_oauth2_pkce_bff.md#complete-authentication-flow
![auth flow](./_static/auth_flow.svg)

**OIDC / PKCE auth flow** — login + per-request, with the disabled authz gate.
:::

:::{card}
:link: ./guide/guide_authz.md#authorization-workflow
![authz decision](./_static/authz_decision.svg)

**Authz decision** — checker precedence → TRUE / FALSE.
:::

:::{card}
:link: ./guide/guide_cert_management.md#how-live-sessions-survive-the-swap
![cert hot-swap](./_static/cert_hotswap.svg)

**Cert hot-swap** — `old_ctx` kept alive by live sessions until they close.
:::

:::{card}
:link: ./guide/guide_cert_management.md#the-three-layer-defense-in-depth
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
:link: ./yunos/emailsender.md#delivery-semantics
![emailsender flow](./_static/emailsender_flow.svg)

**Emailsender retry / dead-letter** — queue → SMTP → sent / retry / failed.
:::

:::{card}
:link: ./yunos/logcenter.md#what-it-does-with-each-record
![logcenter pipeline](./_static/logcenter_pipeline.svg)

**logcenter ingest** — UDP datagram → verify → count / rotatory / summary.
:::

:::{card}
:link: ./tools/sync_binaries.md#rebuild-lifecycle-is-automated-the-bump-path-is-not
![deploy/sync](./_static/deploy_sync.svg)

**Deploy / sync cycle** — build → diff → REBUILD hot-patch by start_priority.
:::

:::{card}
:link: ./guide/folders.md#kernel
![build stack](./_static/build_stack.svg)

**Build dependency stack** — gobj-c → … → yunos → tests.
:::

:::{card}
:link: ./guide/guide_gbuffer.md#structure-overview
![gbuffer layout](./_static/gbuffer_layout.svg)

**gbuffer layout** — independent `curp` (read) / `tail` (write) cursors.
:::

::::
