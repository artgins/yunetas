# NODE_SEALING.md

**The black-box node: no inbound SSH, access only inward-out through the
controlcenter.**

> **Status: DESIGN PROPOSAL — not yet implemented.**
> This document is the validated mental model for a node-sealing lifecycle.
> The commands, the agent22↔agent heartbeat and the node-state topic described
> here **do not exist in the code yet**. Read the
> [What exists today](#1-what-exists-today-vs-what-this-proposes) table before
> assuming any of it ships. When a piece lands, move it from the "proposed"
> column to the "exists" column and link the commit.

---

## 0. The goal

A provisioned node is a **black box**: no operator can reach it by SSH. The only
way in is *inward-out* — the agent (and its redundant twin, `yuneta_agent22`)
hold an outbound connection to a controlcenter, and the controlcenter opens an
interactive **PTY** back down that connection. The forward door (SSH) is closed;
the reverse door (PTY over the agent's outbound link) is the only door.

The break-glass of last resort is **the hosting provider's out-of-band console**
(OVH rescue mode / KVM / IPMI / serial). There is deliberately **no automatic
un-seal**: a sealed box stays sealed even if both reverse channels die, and is
recovered only from the provider console. This is the security/robustness trade
chosen for OVH-hosted nodes — see [§6](#6-break-glass-the-provider-console-is-the-contract).

---

## 1. What exists today vs what this proposes

| Piece                                   | Status | Where |
|-----------------------------------------|--------|-------|
| `C_PTY` pseudoterminal gclass           | **Exists** | [`kernel/c/root-linux/src/c_pty.c`](../../../kernel/c/root-linux/src/c_pty.c) |
| Agent/agent22 create a PTY for the CC   | **Exists** | [`yuno_agent22/src/c_agent22.c`](../yuno_agent22/src/c_agent22.c) (`gobj_create_service(name, C_PTY, …)`) |
| Outbound controlcenter client (`tcps://`) | **Exists** | both `main.c` (`controlcenter` service, `C_IEVENT_CLI` → `C_PROT_TCP4H`) |
| Dual-homing (agent `:1994` / agent22 `:1995`, `.com`/`.ovh`) | **Exists** | both `main.c` `__output_url__` |
| `node_owner != "none"` gates the CC link | **Exists** | [`c_agent.c`](src/c_agent.c) `mt_start`, [`c_agent22.c`](../yuno_agent22/src/c_agent22.c) `mt_play` |
| Long-lived controlcenter JWT + cert auto-sync | **Exists** | agent config `controlcenter.jwt`, `cert_sync_*` (see [`YUNO_AUTH.md`](YUNO_AUTH.md)) |
| **agent22 survives an empty `node_owner`** | **Proposed** (Hardening #0) | today it `exit(0)`s — [§5.1](#51-hardening-0-agent22-must-never-suicide) |
| **agent22 → agent CC-connection heartbeat** | **Proposed** | [§5.2](#52-the-agent22agent-heartbeat) |
| **`seal-node` / `unseal-node` / `node-seal-status`** | **Proposed** | [§3](#3-command-surface), [§4](#4-the-seal-is-self-proving-and-self-validating) |
| **`/yuneta/bin/yuneta-unseal-local`** (console break-glass) | **Proposed** | [§6](#6-break-glass-the-provider-console-is-the-contract) |
| **Node-state topic + fleet view** | **Proposed** (phase 2) | [§7](#7-node-state-phase-2) |
| **SSH-dependency ledger** — what still needs SSH, and at which tier | **Living inventory** | [§5.4](#54-the-ssh-dependency-ledger) |

---

## 2. The governing property: the OS enforces the seal, not the agent

For a real black box, **the seal must hold even when the agent is dead.** So
every seal step is a *persistent OS-level* change, not something the agent
re-applies on each boot:

| Step | Mechanism | Why it persists |
|------|-----------|-----------------|
| 1 · stop SSH | `systemctl disable --now ssh` | systemd will not start it on boot |
| 2 · firewall :22 | nftables rule dropping inbound `:22`, saved | survives reboot |
| 3 · keys | move every `~/.ssh/authorized_keys` → `…/authorized_keys.sealed`, `passwd -l` the bootstrap user | on-disk, survives reboot |
| 4 · agent port | nftables rule dropping inbound `:1993` (the agent's wss+OAuth2 listener, bound `0.0.0.0` today), saved | forces every operation through the reverse channel; survives reboot |

None of these depend on a running agent. The agent is needed **only to
un-seal in-band** (over the reverse PTY). If the agent crashes, is SIGKILLed,
or fails to start, the box **stays sealed**. Step 1 (`disable sshd`) is the
primary lever — it cuts every user at once, not key by key; the key-move is
secondary defence in depth.

Corollary: sealing is durable across reboot *by construction*. The agent does
**not** re-seal on startup; the OS already holds the line.

### 2.1 The access doors: three SSH-less paths, and the mutual-update rule

Verified 2026-07-24 against the live artgins fleet. Excluding SSH (which is only
for hand-over / provisioning — "the machine is given to us"), a node is
reachable **three** ways, and they are **not** equivalent:

| # | Door | Endpoint | Reaches | Surface |
|---|------|----------|---------|---------|
| 1 | Direct to the agent | `wss://<node>:1993` (OAuth2) — **inbound** | the node's own `yuneta_agent` | full `C_AGENT` |
| 2 | Controlcenter, agent plane | `wss://<cc>.com:1996` → `command-agent agent_id=<node> cmd2agent=…` | `yuneta_agent` (`C_AGENT`) | **full**: deploy, lifecycle, realms, configs, snaps, `command-yuno`, `dir-*`, `read-*`, cert-sync, `open-console`, … |
| 3 | Controlcenter, agent22 plane | `wss://<cc>.ovh:1997` → `command-agent …` | `yuneta_agent22` (`C_AGENT22`) | **minimal**: `help`, `list/open/close-console`, `write-tty` — essentially **just the root shell** |

- **Door 1 (`:1993`) is inbound** and open to `0.0.0.0` today — it is the one
  the seal must close (step 4 in the table above). Doors 2 and 3 are the node's
  **outbound** reverse channels to the controlcenter and are what survives the
  seal: you never connect *to* a sealed node, it reaches out and you ride its
  channel back down through the controlcenter.
- **The two controlcenter planes reach different agents with different
  surfaces.** `.com`/`agent` is full node management; `.ovh`/`agent22` is a
  deliberately minimal escape hatch that offers **only the console**. So the
  redundancy is "a full agent + a bare-shell twin", not two equal copies. (On
  our 7.8.x nodes the homing is clean: `.com`→`yuneta_agent`,
  `.ovh`→`yuneta_agent22`; legacy 6.x nodes may home differently.)
- **The controlcenter is a multi-tenant hub.** One CC (artgins) is the
  reverse-channel endpoint for *many* independent fleets (start-fleet, the
  normedan hospital nodes, a CESGA cluster of ~15, raspz, …). Any operator it
  authenticates with the `command-agent` permission can address every agent
  registered there. **Under a seal the controlcenter's security IS the whole
  fleet's security** — it needs the strongest hardening in the network. (This is
  compounded by the per-command gate being off on the nodes — see [§5.4](#54-the-ssh-dependency-ledger):
  authenticated ⇒ full non-gated surface on every node.)

#### The mutual-update rule — never saw off your own branch

The two agents exist so **each can maintain the other**. The agent is a
standalone daemon (not a managed yuno), so updating an agent binary is a
`--stop` / replace / `--start` run **on the box, from a root console** — and you
must open that console through the *other* agent's plane:

- To update / restart **`yuneta_agent`** → open a console through **agent22**
  (`.ovh:1997`) and do the stop/replace/start there. If instead you entered
  through the agent's own plane (`.com:1996`) and killed it, you kill the very
  reverse channel your session rides on and **lose the node**.
- To update **`yuneta_agent22`** → open a console through **`yuneta_agent`**
  (`.com:1996`).
- Rule: **you can never kill or restart the agent whose reverse channel carries
  your session.** Cross over — maintain each agent from the other's plane. This
  is the concrete operational reason for the two-agent design (mutual,
  cross-channel maintenance) and the sharp edge behind "never stop or upgrade
  both agents at once": doing so, or killing the one you came in on, strands the
  node with no in-band channel, recoverable only from the provider console
  ([§6](#6-break-glass-the-provider-console-is-the-contract)).

---

## 3. Command surface

Three agent commands, invoked **from the controlcenter over the reverse
channel** (and, for `node-seal-status`, locally over `:1991`):

| Command | What it does |
|---------|--------------|
| `seal-node [force=false]` | Run the [precondition gate](#4-the-seal-is-self-proving-and-self-validating), then apply the three OS-persistent steps. Records which steps it applied. |
| `unseal-node` | Reverse exactly the recorded steps. Only reachable while the reverse channel is alive. |
| `node-seal-status` | Report current seal state, the steps applied, and the last verification snapshot. |

`unseal-node` is **not** a recovery path for a dead channel — if the reverse
channel is down there is no way to deliver it. That case is the provider
console ([§6](#6-break-glass-the-provider-console-is-the-contract)).

---

## 4. The seal is self-proving AND self-validating

Two independent safety layers, because there is no automatic recovery:

**Self-proving.** `seal-node` arrives *through* the controlcenter→agent reverse
channel. If it executes and ACKs back, the channel demonstrably works at the
instant of sealing. You only ever close the forward door by sending a command
through the door that replaces it.

**Self-validating (the hard gate).** `seal-node` **refuses** unless *all* hold:

1. `node_owner != "none"`.
2. `controlcenter.jwt` present and **not expired** (the agent introspects its
   own `exp` claim).
3. The agent's own controlcenter link is established (connected session, not
   just configured).
4. **agent22 is also connected** to its controlcenter — redundancy is present.
   See the heartbeat in [§5.2](#52-the-agent22agent-heartbeat); this is the one
   piece of new plumbing the gate needs.
5. `authz.jwks` present and the `tcps` client certs present.
6. A recent PTY self-test round-trip succeeded (or the live PTY session the
   command was issued from).

`force=true` bypasses the gate but logs *who* and *why* (the
[no-silent-errors axiom](../../../CLAUDE.md) applies). With no safety net,
`force` is "on your head".

---

## 5. The plumbing this exposes (must land first)

### 5.1 Hardening #0: agent22 must never suicide

Today [`c_agent22.c`](../yuno_agent22/src/c_agent22.c) `mt_create` does
`exit(0)` when `node_owner` is empty. The primary agent in the same situation
writes `"none"` and **keeps running** ([`c_agent.c`](src/c_agent.c) `mt_start`).
For a black box this asymmetry is dangerous: if a provisioning mistake clears
`node_owner`, the escape hatch kills itself **exactly when you need it most**.

**Fix:** agent22 must behave like the primary — fall back to standalone
`"none"` mode (quiet, no controlcenter) and **stay alive**. The escape hatch
never exits on a config value it can default. This is self-contained and
low-risk; it is the prerequisite for everything else here.

### 5.2 The agent22↔agent heartbeat

The seal gate needs to know "is agent22 connected to its controlcenter?".
agent22 is **outbound-only** (it opens no listening port), and the two agents
talk to *different* controlcenters (`.com` vs `.ovh`), so the primary's
controlcenter cannot answer the question. The signal must be **node-local**:

> agent22 writes a small heartbeat — its CC-connection state + a timestamp —
> to a node-local sink the primary agent reads (a file under `/yuneta/agent/`
> or a record in the agent treedb). The primary derives "agent22 connected =
> last_seen within N seconds".

This mirrors the existing in-memory-presence pattern used elsewhere in the
fleet (touch-a-timestamp, derive liveness). It is the only new inter-process
channel the design introduces.

### 5.3 Expiry is a guaranteed brick

Sealed box + short-lived JWT or cert = a guaranteed eventual brick: the reverse
channel dies on expiry and the only way back is the console. Design rule for a
sealed node: **`controlcenter.jwt` and the `tcps` certs must be long-lived or
auto-renewed**, never short. (Today's agent JWT carries `exp` in 2038 —
effectively perpetual — and `cert_sync_*` already auto-renews the Let's Encrypt
path; keep it that way. See [`YUNO_AUTH.md`](YUNO_AUTH.md).)

### 5.4 The SSH-dependency ledger

**The seal is blocked by whatever still needs SSH.** Sealing a node is not a
switch to flip — it is the moment every remaining SSH dependency turns into an
outage. So this ledger is a standing inventory, kept current: *policy is
agent-first — if the agent can do it, do it through the agent, and every time
something genuinely cannot be done through the agent, it gets recorded here.*

**Verified end-to-end SSH-less (2026-07-24).** The whole reverse-channel path
was exercised with a real non-owner operator identity (`claudia@artgins.com`,
root in the controlcenter, **not** in any node's authz), no SSH anywhere:
operator → `wss://<cc>:1996` (OAuth2 password grant, client `yunetacontrol`) →
`command-agent agent_id=<node> cmd2agent=<cmd>` → node agent. Results on the
wattyzer agent:

- `list-agents`, and `command-agent … cmd2agent=list-yunos` → **worked**
  ("Command sent to 1 nodes" + the table). So **every Tier-1 command runs for
  any operator the controlcenter authenticates** — because the per-command
  `SDF_AUTHZ_X` gate is **off** on the node (`enable_command_authz` absent), so
  authentication alone suffices; the node's authz list is not consulted.
- `command-agent … cmd2agent=open-console` → **`No permission to 'open-console'
  in service 'agent'`**. The *only* command that refused, because
  `cmd_open_console` calls `gobj_user_has_authz` unconditionally and claudia is
  not in the node's authz.

**Two consequences for the seal.** (1) With the per-command gate off, the
security boundary of a sealed node collapses to *"who can authenticate to the
controlcenter"* plus *"who is in the node authz for `open-console`"* — the node
authz list governs the kitchen and nothing else. (2) To give a principal full
"to-the-kitchen" access you must add them to **each node's** authz (see the
identity model in [`IPC.md`](IPC.md) §4.7); `yuneta_admin@artgins.com`, a seed
immutable admin on every node, already has it, so it is today's working
full-access operator over the reverse channel.

The distinction below is **not** "possible / impossible" but which of three
tiers a task falls in:

**Tier 1 — a structured agent command exists.** Safe under a seal today.
Verified against [`c_agent.c`](src/c_agent.c) (2026-07-22 command surface):

- Yuno lifecycle: `run-yuno`, `kill-yuno`, `play-yuno`, `pause-yuno`,
  `enable-yuno`, `disable-yuno`, `create-yuno`, `delete-yuno`,
  `find-new-yunos`.
- Binaries: `install-binary`, `update-binary`, `delete-binary`,
  `replicate-binaries`.
- Configs / realms: `create-config`, `edit-config`, `update-config`,
  `view-config`, `delete-config`, and the `*-realm` family.
- Snapshots: `shoot-snap`, `activate-snap`, `deactivate-snap`, `snaps`,
  `snap-content`.
- Observability: `read-file`, `read-binary-file`, `read-json`, the `dir-*`
  family (`dir-logs`, `dir-realms`, `dir-store`, …), `top`, `stats-yuno`,
  `trace-on-yuno` / `trace-off-yuno`, `command-yuno`.
- Fleet: `replicate-node`, `upgrade-node` — note these replicate **realms'
  yunos**, they are *not* an OS package upgrade.
- Certs: `cert-sync-now`, `cert-sync-status`.

The deploy tooling already lives in this tier: `sync_binaries`, `sync_configs`
and `set_start_priorities` (`yunetas.agent_tools.*`) drive a remote `wss://`
agent through `ycommand` with a single OAuth2 password-grant login, reusing the
JWT via `-j`. They were written for SSH-disabled nodes on purpose.

**Tier 2 — only through the agent's PTY** (`open-console` + `write-tty`, the
same channel the `gui_agent` Terminal workspace uses). Reachable under a seal,
but as free-form shell text: no schema, no authz granularity beyond the
`open-console` permission, nothing an operator can audit as a command. Anything
here is a candidate for promotion to Tier 1:

- Writing an **arbitrary file** to the node. There is `read-file` but **no
  `write-file`**; only binaries and configs have structured write commands. So
  certs, `/etc/security/limits.d/*.conf`, `resolv.conf`, nginx vhosts, secret
  overlays all fall here.
- OS administration: `systemctl`, `nftables`, `sysctl`, `tmpfiles.d`,
  `core_pattern`/apport, certbot renewals, nginx/openresty reload.
- Installing or upgrading the **`.deb`/`.rpm` package itself** (SDK + agent
  binaries). `upgrade-node` does not do this.
- Upgrading **the agent binary**. The agent is a standalone daemon, not a
  managed yuno: `update-binary` does not reach it, and it is restarted with
  `yuneta_agent --config-file=<json> --stop` then `--start`. The intended
  agent-side answer is the twin (each agent can replace the other — never both
  at once), but there is no command for it today.

**Tier 2 is reachable — and it is root.** Resolved on `e.com` 2026-07-22:

- [`c_pty.c`](../../../kernel/c/root-linux/src/c_pty.c) spawns the shell with
  `forkpty()` + `execv()` and **no `setuid`/`setgid`** — the PTY child inherits
  the agent's own uid.
- On that node both agents run as **`yuneta`**, and `yuneta` has
  `(ALL) NOPASSWD: ALL`.

So the PTY is a passwordless-root shell, and every OS-level item above really
is doable under a seal: a sealed node can be *maintained*, not merely observed.
The price is that **`open-console` is effectively root on the box** — under a
seal it is the only door, and that door is total. Two consequences:

- The `open-console` permission is the entire security boundary of a sealed
  node. It must never be granted as a convenience.
- Promoting Tier 2 items to Tier 1 is worth real effort — a structured
  `write-file`, a package-upgrade command — because each promotion is one
  fewer reason to hand out root.

Verified on `e.com` only; confirm per node before sealing, especially the uid
the agent runs as (the local dev node, for one, runs the primary agent as the
developer's own user).

**The two authz paths are not the same, and the asymmetry is deliberate:**
`cmd_open_console` calls `gobj_user_has_authz(gobj, "open-console", …)`
**unconditionally** ([`c_agent.c`](src/c_agent.c)), so the console is gated by
the `Authz.initial_load` user list whatever else is configured. The generic
per-command gate is different: `SDF_AUTHZ_X` runs only when the yuno sets
`enable_command_authz` (see
[`command_parser.c`](../../../kernel/c/gobj-c/src/command_parser.c)), and that
attr is **absent on `e.com`** — so the rest of the surface (`install-binary`,
`kill-yuno`, `read-file`, …) is **authenticated but not authorized**: any
principal holding a valid realm token can drive it. Before sealing, decide
whether that is acceptable, because a sealed node's only remaining defence is
this gate. Tracked with the wider role-matrix work in
[`YUNO_AUTH.md`](YUNO_AUTH.md) §4.5.

**Tier 3 — genuinely SSH-only.** Nothing is known to be here yet, *because*
Tier 2 exists: the PTY is a shell, so a shell-shaped task is never impossible,
only unauditable. Treat this tier as the alarm — an entry here means the seal
cannot be applied to that node until it is resolved or accepted as a
console-only (break-glass) operation.

**Who reaches the node through the reverse channel:** the operator, not the
system user. The agent's own JWT identity (`yuneta_agent@…`) authenticates the
*link* into the controlcenter, but the operator's `__username__` is propagated
end to end and is what the node's `authz_checker` evaluates — so every
principal expected to run authz-gated commands on a sealed node must exist in
**that node's** authz list, not only in the controlcenter's. Full chain,
measurement method and the client-side trust asymmetry in
[`IPC.md`](IPC.md) §4.7.

**Known SSH-shaped leftover:** the CLI's node registry
(`~/.yuneta/nodes.json`) stores only an `ssh` string per node
(`yuneta@<host>`), with no agent URL. The registry assumes SSH even though the
`sync_*` tools it ships with do not. It needs an agent-URL field before a
sealed node can be registered.

---

## 6. Break-glass: the provider console is the contract

Because recovery is out-of-band by choice, it must be a documented,
tooled procedure — not folklore:

- **`/yuneta/bin/yuneta-unseal-local`** — a standalone script an operator runs
  as root *from the provider console* (KVM / rescue / serial). It reverses the
  three seal steps **without needing the agent or the network**: re-enable
  sshd, drop the nftables block, restore `authorized_keys.sealed`, unlock the
  bootstrap user. It reads the same recorded step-list `seal-node` wrote.
- **Per-provider runbook** (OVH first): boot into rescue mode → mount the
  system disk → either run `yuneta-unseal-local` chrooted, or restore
  `authorized_keys.sealed` and `systemctl enable ssh` by hand.

There is **no** dead-man's-switch and **no** auto-unseal watchdog. A sustained
controlcenter outage does **not** reopen SSH. The box stays black; the console
is the only fallback. (The alternative — auto-unseal after N hours offline —
was considered and rejected for OVH nodes: it trades the brick risk for a
recurring SSH-reopen security window, and the OVH console is trusted.)

---

## 7. Node state (phase 2)

Persist a node lifecycle state in the agent treedb so the controlcenter can
show a **fleet view** — which nodes are still SSH-open vs sealed:

```
bootstrapped  →  provisioned  →  verified  →  sealed
  (host SSH)      (project        (CC + PTY    (SSH closed,
                   config on        proven on    reverse PTY
                   both agents)     both)        only)
```

Record: `{ state, sealed_at, sealed_by, steps_applied, last_verify }`.
`seal-node` is the `verified → sealed` transition; the controlcenter should only
issue it once it has independently seen **both** agents registered and a PTY
round-trip succeed. The human never decides "is it safe to close SSH" — the
controlcenter proves it, then seals.

---

## 8. End-to-end flow

```
1 bootstrap   install.sh over the hosting's SSH/user (cloud-init ideal)
              → agents run standalone (node_owner=none): black, no reverse channel
2 provision   push the project agent JSONs to agent AND agent22 → restart
              → both dial their controlcenter (.com:1994 / .ovh:1995)
3 verify      controlcenter sees BOTH registered + a PTY round-trip OK
4 seal        controlcenter issues seal-node → gate passes → OS seals → ACK
              → from here: reverse PTY only; recovery = OVH console
```

The bootstrap key is **ephemeral by construction**: it exists only to run
install + provision (ideally via cloud-init user-data, so no human SSHes in for
a normal node), and the `seal` step retires it. The forward door is temporary
by design, not by discipline.

---

## 9. Build order

0. **Keep the SSH-dependency ledger current** ([§5.4](#54-the-ssh-dependency-ledger)) — continuous, not a step: it is the readiness gate for every node. Work agent-first and record each new SSH-only need as it appears.
1. **Hardening #0** — agent22 stops suiciding on empty `node_owner` ([§5.1](#51-hardening-0-agent22-must-never-suicide)). Self-contained, unblocks the rest.
2. **agent22↔agent heartbeat** ([§5.2](#52-the-agent22agent-heartbeat)) — the gate's precondition #4.
3. **`seal-node` / `unseal-node` / `node-seal-status`** ([§3](#3-command-surface)) — the three OS-persistent steps + the gate ([§4](#4-the-seal-is-self-proving-and-self-validating)).
4. **`yuneta-unseal-local`** + the OVH break-glass runbook ([§6](#6-break-glass-the-provider-console-is-the-contract)).
5. **Node-state topic + fleet view** ([§7](#7-node-state-phase-2)) — phase 2.

---

## See also

- [`YUNO_AUTH.md`](YUNO_AUTH.md) — controlcenter JWT, `C_AUTHZ`, cert auto-sync (the credentials a sealed node lives or dies by).
- [`IPC.md`](IPC.md) — `C_IEVENT_CLI` outbound, the gate stack, how the reverse channel is built.
- [`../yuno_agent22/`](../yuno_agent22/) — the redundant escape-hatch agent.
- [`ENTRY_POINT.md`](ENTRY_POINT.md) — the `ydaemon.c` watcher (the autonomous-survival kernel the heartbeat and seal sit on top of).
