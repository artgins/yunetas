# yuno_agent22

`yuno_agent22` is the secondary Yuneta agent. It is a backdoor management channel that `controlcenter` controls. It uses a PTY for advanced remote administration features (interactive shells, remote commands), independent of the primary `yuno_agent`.

Enable it only on hosts that a control center must reach.

`yuno_agent22` also replaces the binary of the main agent and restarts it in
its systemd unit, with `upgrade-agent content64=<binary>`. The command runs
only on a node that builds from source. On a package node,
`install-package url=<https url> sha256=<hex>` installs the `yuneta-agent`
package instead. Its root part is `/usr/libexec/yuneta/install-package`, a
program of the package: `agent22` writes only the URL and the sha256. The page
[`yuneta_agent22`](https://doc.yuneta.io/yuneta-agent22) gives the steps.
