# Get Started

**Current version: [7.26.0](https://github.com/artgins/yunetas/tree/7.26.0)** ·
*Documentation updated: 2026-10-04*

## What is Yuneta

```{include} ../../README.md
:start-after: <!-- yuneta-intro-start: also the introduction of doc.yuneta.io (get_started.md includes it) -->
:end-before: <!-- yuneta-intro-end -->
```

Install Yuneta, prepare your build environment, and bring a node online.
Start here if this is your first contact with the framework.

## In this section

- [Installation](installation.md) — system prerequisites, the apt dependency
  list, and the one-time environment setup ([`menuconfig`](installation.md#configure-menuconfig), [`yunetas-env.sh`](https://github.com/artgins/yunetas/blob/7.26.0/yunetas-env.sh),
  building the external libraries).
- [Activate the environment](#activate-environment) — `yunetas-env.sh`, in
  every shell that builds or deploys.
- [Starting and stopping the agent](#yuneta-agent-systemd) — the systemd units
  of the two agents.
- [Changelog](CHANGELOG.md) — release history.

The build and deploy steps below are driven by the
[`yunetas` CLI](yunetas-cli.md) — its own page is the full command reference.
