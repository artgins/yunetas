# Guides

Task-oriented how-to guides for the core framework concepts — from the basic
object model through SData schemas, persistence, parsers, authorization, the
event loop, and TLS.

## In this section

- [Basic Concepts](guide_basic_concepts.md) — the vocabulary: yuno, gclass, gobj,
  event, attribute.
- [GClass](guide_gclass.md) — defining a class: states, events, attributes,
  commands.
- [GObj](guide_gobj.md) — creating and wiring object instances.
- [SData](guide_sdata.md) — the typed schema system (`SDATA()` descriptors).
- [kwid](guide_kwid.md) — addressing data inside JSON `kw` trees.
- [GBuffer](guide_gbuffer.md) — the growable byte buffer.
- [Folders](guide_folders.md) — the repository and on-disk directory layout.
- [Persistent Attributes](guide_persistent_attrs.md) — saving and restoring gobj
  state.
- [Command Parser](guide_parser_command.md) — exposing commands to the control plane.
- [Stats Parser](guide_parser_stats.md) — exposing statistics.
- [Settings](guide_settings.md) — configuration merging and overrides.
- [Authorization](guide_authz.md) — [`C_AUTHZ`](#gclass-c-authz), roles, and permissions.
- [Timeranger2](guide_timeranger2.md) — the append-only time-series store.
- [Event Loop](guide_yev_loop.md) — the io_uring-based async loop.
- [ytls](guide_tls.md) — the runtime-selectable TLS abstraction.
- [Certificate Management](guide_cert_management.md) — issuing, reloading, and
  syncing certificates.
- [OAuth2 PKCE BFF](guide_oauth2_pkce_bff.md) — the browser auth flow with
  `auth_bff`.
