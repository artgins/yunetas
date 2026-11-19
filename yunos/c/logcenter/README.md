# logcenter

logcenter is the yuno that collects the logs of the other yunos. It receives them by UDP and by file handlers, and applies rotation and filters. You can then look at the collected data.

The configuration of each yuno usually adds the `to_udp` log handler (`udp://127.0.0.1:1992`) to send its logs to logcenter.
