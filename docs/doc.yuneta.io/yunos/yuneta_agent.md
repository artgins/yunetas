(yuno-yuneta_agent)=
# `yuneta_agent`

The primary agent — the supervisor of a Yuneta node. It manages the lifecycle
of every other yuno (create / run / kill / update / delete), owns the realms
and their on-disk layout, exposes the control plane, and coordinates inter-yuno
communication. [`ycommand`](#util-ycommand), [`ystats`](#util-ystats), [`ylist`](#util-ylist) and [`ybatch`](#util-ybatch) talk to this yuno by
default.

## Control plane ports

| URL | Purpose |
|-----|---------|
| `ws://127.0.0.1:1991` | Local, plaintext control plane (default for `ycommand`) |
| `wss://0.0.0.0:1993` | Secure control plane (TLS + OAuth2) |

## Connecting out to a control center

When the node has an owner (`node_owner` ≠ `"none"`), the agent starts an
outbound [`C_IEVENT_CLI`](#gclass-c-ievent-cli) client service named `controlcenter` and dials the
control center. The remote URL is assembled from config variables:

```
tcps://(^^__sys_machine__^^).(^^__node_owner__^^).(^^__output_url__^^)
```

i.e. `tcps://<machine>.<node_owner>.<output_url>`, where `__output_url__`
defaults to `yunetacontrol.com:1994`. With an empty/`none` owner the agent does
not dial out. See [`controlcenter`](controlcenter.md) for the operator side.

## Redundancy

Every node also runs [`yuneta_agent22`](yuneta_agent22.md), a minimal second
agent kept alive as an escape hatch so a node is never left unmanageable.

## Deep dive

The agent is documented at length under **Operating Yuneta**:

- [Entry point (main + watcher)](../../../yunos/c/yuno_agent/ENTRY_POINT.md)
- [Yuno lifecycle](../../../yunos/c/yuno_agent/YUNO_LIFECYCLE.md)
- [Debugging a yuno](../../../yunos/c/yuno_agent/DEBUGGING.md)
- [Inter-process communication](../../../yunos/c/yuno_agent/IPC.md)
- [Realms (multi-tenancy)](../../../yunos/c/yuno_agent/REALMS.md)
- [Scaffolding new yunos](../../../yunos/c/yuno_agent/SCAFFOLDING.md)
- [Auth, permissions, TLS](../../../yunos/c/yuno_agent/YUNO_AUTH.md)
- [gobj framework crash course](../../../yunos/c/yuno_agent/GOBJ.md)
- [timeranger2 + treedb crash course](../../../yunos/c/yuno_agent/YUNO_TREEDB.md)
