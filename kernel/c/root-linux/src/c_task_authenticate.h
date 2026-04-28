/****************************************************************************
 *          c_task_authenticate.h
 *          Task_authenticate GClass.
 *
 *  Task to authenticate against an OIDC IdP using OAuth2 ROPC
 *  (Resource Owner Password Credentials).
 *
 *  IdP configuration (priority order — see c_task_authenticate.c):
 *
 *    1. Explicit 'token_endpoint' + 'end_session_endpoint' attrs
 *       — full URLs, skips discovery.
 *
 *    2. 'issuer' attr — full issuer URL.  The task chain prepends
 *       a GET of <issuer>/.well-known/openid-configuration before
 *       action_get_token, caches the resolved endpoints in priv,
 *       and continues the auth flow on the same connection.
 *
 *    3. Legacy 'auth_url' (DEPRECATED) — Keycloak path scheme.
 *       Emits a startup warning; will be removed in a future
 *       release.  Six callers still use this path:
 *       yuno_cli, ybatch, ystats, ytests, ycommand, mqtt_tui.
 *
 *  The 'auth_system' attr is preserved for back-compat but no
 *  longer routes flow selection.
 *
 *          Copyright (c) 2021 Niyamaka.
 *          Copyright (c) 2024, ArtGins.
 *          All Rights Reserved.
 ****************************************************************************/
#pragma once

#include <gobj.h>

#ifdef __cplusplus
extern "C"{
#endif

/***************************************************************
 *              FSM
 ***************************************************************/
/*------------------------*
 *      GClass name
 *------------------------*/
GOBJ_DECLARE_GCLASS(C_TASK_AUTHENTICATE);

/*------------------------*
 *      States
 *------------------------*/

/*------------------------*
 *      Events
 *------------------------*/
GOBJ_DECLARE_EVENT(EV_ON_TOKEN);

/***************************************************************
 *              Prototypes
 ***************************************************************/
PUBLIC int register_c_task_authenticate(void);

#ifdef __cplusplus
}
#endif
