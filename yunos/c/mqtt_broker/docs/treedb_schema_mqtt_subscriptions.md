# `treedb_mqtt_subscriptions` schema diagram

The literal is in [`treedb_schema_mqtt_subscriptions.c`](treedb_schema_mqtt_subscriptions.c), alone, so it can be replaced whole with a schema-editor export.

```
 *  TreeDB Schema for MQTT Broker Subscriptions Model (Mosquitto-style)
 *
    {}  dict hook   (N unique children)
    []  list hook   (n not-unique children)
    (↖) 1 fkey      (1 parent)
    [↖] n fkeys     (n parents)

    * field required

                        clients
            ┌───────────────────────────┐
            │* id (client_id)           │
            │                           │
            │          subscriptions {} │ ◀─┐N
            │                           │   │
            │  protocol_version         │
            │  clean_start              │
            │  username                 │
            │  keep_alive               │
            │  session_expiry_interval  │
            │  connected                │
            │  will_topic               │
            │  will_payload             │
            │  will_qos                 │
            │  will_retain              │
            │  will_delay_interval      │
            │                           │
            │  _geometry                │
            └───────────────────────────┘
                                            │
                                            │
                    subscriptions           │
            ┌───────────────────────────┐   │
            │* id (topic_filter)        │   │
            │                           │   │
            │              client_id (↖)│ ──┘1
            │                           │
            │* qos                      │
            │  no_local                 │
            │  retain_as_published      │
            │  retain_handling          │
            │  identifier               │
            │                           │
            │          shared_group [↖] │ ──┐n
            │                           │   │
            │  _geometry                │   │
            └───────────────────────────┘   │
                                            │
                                            │
                    shared_groups           │
            ┌───────────────────────────┐   │
            │* id (group_name)          │   │
            │                           │   │
            │          subscriptions {} │ ◀─┘N
            │                           │
            │* topic_filter             │
            │  _geometry                │
            └───────────────────────────┘


                    retained_msgs
            ┌───────────────────────────┐
            │* id (topic)               │
            │                           │
            │  payload                  │
            │  qos                      │
            │  time                     │
            │  message_expiry_interval  │
            │  payload_format_indicator │
            │  content_type             │
            │  response_topic           │
            │  correlation_data         │
            │  user_properties          │
            │                           │
            │  _geometry                │
            └───────────────────────────┘


```
