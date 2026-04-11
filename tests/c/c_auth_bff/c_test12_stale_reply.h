/****************************************************************************
 *          C_TEST12_STALE_REPLY.H
 *
 *          Driver GClass for test12_stale_reply: cross-user token leak
 *          race reproducer.  Connection A POSTs /auth/login, aborts
 *          before Keycloak replies, Connection B opens on the same
 *          persistent BFF channel BEFORE the stale A reply arrives,
 *          and B must NOT receive A's token.
 *
 *          Gates the per-task browser generation fix added in the
 *          previous commit.  Without that fix the test fails because
 *          the stale reply for A's task is forwarded to B's socket.
 *
 *          Copyright (c) 2026, ArtGins.
 *          All Rights Reserved.
 ****************************************************************************/
#pragma once

#include <yunetas.h>

#ifdef __cplusplus
extern "C" {
#endif

GOBJ_DECLARE_GCLASS(C_TEST12_STALE_REPLY);

PUBLIC int register_c_test12_stale_reply(void);

#ifdef __cplusplus
}
#endif
