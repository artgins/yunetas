# mqtt_broker

Yuneta MQTT broker yuno. Speaks **MQTT v3.1 / v3.1.1 / v5.0** with
persistent sessions, retained messages, shared subscriptions, multi-
tenant isolation, and OAuth2/JWT authentication via the standard
`authz` service.

Built on top of the [MQTT module](../../../modules/c/mqtt/)
(`C_MQTT_BROKER` + `C_PROT_MQTT2`). One yuno = one broker instance.

## Default services

From [`src/main.c`](src/main.c):

| Service       | gclass          | Notes                                                                |
|---------------|-----------------|----------------------------------------------------------------------|
| `authz`       | `C_AUTHZ`       | Authentication / authorisation (users, roles, JWT). `autoplay`.      |
| `mqtt_broker` | `C_MQTT_BROKER` | The broker itself. `default_service`, `autostart`, `autoplay=false` (the broker is *played* once authz is ready). |

Initial config (from `variable_config` in `main.c`): one `deny_subscribes`
entry `["test/nosubscribe"]` as a placeholder. Real deployments override
the broker's attrs via the external config file (see
[`YUNO_LIFECYCLE.md`](../yuno_agent/YUNO_LIFECYCLE.md) §2.2 for how the agent
materialises configs).

## Commands

Inherited from `C_MQTT_BROKER` (see the
[MQTT module README](../../../modules/c/mqtt/README.md)): `list-channels`,
`list-sessions`, `list-queues`, `normal-subs`, `shared-subs`,
`flatten-subs`, `list-retains`, `remove-retains`, `clean-queues`,
`authzs`. Talk to the broker via `ycommand`:

```bash
ycommand -c 'command-yuno id=<id> service=mqtt_broker command=list-sessions'
ycommand -c 'command-yuno id=<id> service=mqtt_broker command=list-retains'
```

## Memory tuning

`main.c` declares the yuno expects to handle large workloads:

```
MEM_MIN_BLOCK         512 B
MEM_MAX_BLOCK         1 GiB
MEM_SUPERBLOCK        1 GiB
MEM_MAX_SYSTEM_MEMORY 16 GiB
```

Adjust if you're running multiple brokers per host or on a constrained
device.

## Logging

Standard handlers wired in `variable_config`:

- File handler (`mqtt_broker-W.log` under
  `/yuneta/logs/<role_plus_name>/`).
- UDP handler to `udp://127.0.0.1:1992` (the
  [`logcenter`](../logcenter/) yuno).
- `to_stdout` only in console mode.

See [`DEBUGGING.md`](../yuno_agent/DEBUGGING.md) for the log
infrastructure.

## Tests

`tests/` ships a black-box test suite — see
[`tests/README.md`](tests/README.md) for the matrix.

## Build & deploy

Standard `yunetas` flow. Quick steps:

```bash
cd yunos/c/mqtt_broker/build
make install                                        # 1. build
ycommand -c 'install-binary content64=$$(mqtt_broker)'  # 2. upload to agent
ycommand -c 'create-yuno realm_id=<realm> yuno_role=mqtt_broker yuno_name=<name>'
ycommand -c 'run-yuno'
```

See [`YUNO_LIFECYCLE.md`](../yuno_agent/YUNO_LIFECYCLE.md) §6 for the canonical
deploy recipes.
