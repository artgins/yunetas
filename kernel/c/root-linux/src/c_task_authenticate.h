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
 *       — full URLs, skips discovery (one fewer round-trip).
 *
 *    2. 'issuer' attr — full issuer URL.  The task chain prepends
 *       a GET of <issuer>/.well-known/openid-configuration before
 *       action_get_token, caches the resolved endpoints in priv,
 *       and continues the auth flow on the same connection.
 *
 *          Copyright (c) 2021 Niyamaka.
 *          Copyright (c) 2024-2026, ArtGins.
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
