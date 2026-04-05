# yuno_agent

Primary Yuneta agent. Manages the lifecycle of yunos on the local machine (start/stop/update), exposes a WebSocket control interface, handles realms and authentication, and coordinates inter-yuno communication.

This is the yuno that `ycommand`, `ystats`, `ybatch` and friends talk to by default.

## Entry points

- Local socket: used by local CLI tools
- WebSocket (realm-specific URL): used for remote management and `controlcenter`
