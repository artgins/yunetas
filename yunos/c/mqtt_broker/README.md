# mqtt_broker

`mqtt_broker` is the Yuneta MQTT broker yuno. It speaks MQTT v3.1, v3.1.1, and
v5.0. It has persistent sessions, retained messages, shared subscriptions,
multi-tenant isolation, and OAuth2/JWT authentication through the standard
`authz` service.

The yuno uses the [MQTT module](../../../modules/c/mqtt/)
(`C_MQTT_BROKER` + `C_PROT_MQTT2`). One yuno is one broker instance.

## Build & deploy

The yuno uses the standard `yunetas` flow. The quick steps are:

```bash
cd yunos/c/mqtt_broker/build
make install                                        # 1. build
ycommand -c 'install-binary content64=$$(mqtt_broker)'  # 2. upload to agent
ycommand -c 'create-yuno realm_id=<realm> yuno_role=mqtt_broker yuno_name=<name>'
ycommand -c 'run-yuno'
```

For the canonical deploy recipes, see
[`YUNO_LIFECYCLE.md`](../yuno_agent/YUNO_LIFECYCLE.md) §6.

## Commands

`C_MQTT_BROKER` defines the commands (see the
[MQTT module README](../../../modules/c/mqtt/README.md)): `list-channels`,
`list-sessions`, `list-queues`, `normal-subs`, `shared-subs`,
`flatten-subs`, `list-retains`, `remove-retains`, `clean-queues`,
`authzs`. Send them to the broker with `ycommand`:

```bash
ycommand -c 'command-yuno id=<id> service=mqtt_broker command=list-sessions'
ycommand -c 'command-yuno id=<id> service=mqtt_broker command=list-retains'
```

## Default services

From [`src/main.c`](src/main.c):

| Service       | gclass          | Notes                                                                |
|---------------|-----------------|----------------------------------------------------------------------|
| `authz`       | `C_AUTHZ`       | Authentication / authorization (users, roles, JWT). `autoplay`.      |
| `mqtt_broker` | `C_MQTT_BROKER` | The broker itself. `default_service`, `autostart`, `autoplay=false` (the broker is *played* when authz is ready). |

The initial configuration (`variable_config` in `main.c`) has one
`deny_subscribes` entry, `["test/nosubscribe"]`, as a placeholder. A real
deployment overrides the attrs of the broker in the external configuration
file. [`YUNO_LIFECYCLE.md`](../yuno_agent/YUNO_LIFECYCLE.md) §2.2 tells how
the agent materializes the configuration files.

## Memory tuning

`main.c` declares that the yuno handles large workloads:

```
MEM_MIN_BLOCK         512 B
MEM_MAX_BLOCK         1 GiB
MEM_SUPERBLOCK        1 GiB
MEM_MAX_SYSTEM_MEMORY 16 GiB
```

If you run more than one broker on a host, or if the device has little
memory, change these values.

## Logging

`variable_config` connects the standard handlers:

- File handler (`mqtt_broker-W.log` under
  `/yuneta/logs/<role_plus_name>/`).
- UDP handler to `udp://127.0.0.1:1992` (the
  [`logcenter`](../logcenter/) yuno).
- `to_stdout` only in console mode.

For the log infrastructure, see [`DEBUGGING.md`](../yuno_agent/DEBUGGING.md).

## Tests

`tests/` has a black-box test suite. For the matrix, see
[`tests/README.md`](tests/README.md).
