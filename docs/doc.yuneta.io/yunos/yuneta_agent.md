(yuno-yuneta_agent)=
# `yuneta_agent`

Primary Yuneta agent — manages the lifecycle of yunos on the local machine
(start/stop/update), exposes a WebSocket control interface, handles realms and
authentication, and coordinates inter-yuno communication. This is the yuno that
`ycommand`, `ystats`, and `ybatch` talk to by default.
