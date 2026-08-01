/****************************************************************************
 *          c_authz.h
 *          Authz GClass.
 *
 *          Authorization Manager
 *
 *          Copyright (c) 2020 Niyamaka.
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
GOBJ_DECLARE_GCLASS(C_AUTHZ);

/*------------------------*
 *      States
 *------------------------*/

/*------------------------*
 *      Events
 *------------------------*/
GOBJ_DECLARE_EVENT(EV_ADD_USER);
GOBJ_DECLARE_EVENT(EV_REJECT_USER);
GOBJ_DECLARE_EVENT(EV_AUTHZ_USER_LOGIN);
GOBJ_DECLARE_EVENT(EV_AUTHZ_USER_LOGOUT);
GOBJ_DECLARE_EVENT(EV_AUTHZ_USER_NEW);

/*
 *  The seam between an identity provider and this plane: an IdP
 *  provisioner publishes it after creating an account, and every plane
 *  that keeps its own user record subscribes and writes its own.  It is
 *  declared here, with the other user-lifecycle events, so that no authz
 *  code has to include a provider header -- the dependency points the
 *  other way, from the provider to this plane.
 *
 *  Payload: {username, first_name, last_name, role, idp_user_id}
 *  A subscriber action returns 0 when it recorded the user; the publisher
 *  reports a negative return to the caller as a non-fatal warning.
 */
GOBJ_DECLARE_EVENT(EV_IDP_USER_CREATED);

/***************************************************************
 *              Prototypes
 ***************************************************************/
PUBLIC int register_c_authz(void);

PUBLIC BOOL authz_checker(hgobj gobj_to_check, const char *authz, json_t *kw, hgobj src);
PUBLIC json_t *authentication_parser(hgobj gobj_service, json_t *kw, hgobj src);

#ifdef __cplusplus
}
#endif
