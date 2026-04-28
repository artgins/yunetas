/****************************************************************************
 *          c_task_authenticate.h
 *          Task_authenticate GClass.
 *
 *          Task to authenticate with OAuth2.
 *
 *  NOTE — Keycloak coupling:
 *  This gclass currently assumes the Keycloak path scheme
 *      <auth_url>/protocol/openid-connect/{token,logout}
 *  (see c_task_authenticate.c, action_get_token / action_logout).
 *  OIDC discovery + IdP-agnostic operation will be added in a future
 *  commit, mirroring the issuer/.well-known approach used by
 *  c_auth_bff (see c_auth_bff.h).  Until then, only Keycloak-style
 *  IdPs are supported here.
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
