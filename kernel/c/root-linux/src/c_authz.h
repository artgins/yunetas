/****************************************************************************
 *          c_authz.h
 *          Authz GClass.
 *
 *          Authorization Manager
 *
 *          Copyright (c) 2020 Niyamaka, 2024- ArtGins.
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

/***************************************************************
 *              Prototypes
 ***************************************************************/
PUBLIC int register_c_authz(void);

PUBLIC BOOL authz_checker(hgobj gobj_to_check, const char *authz, json_t *kw, hgobj src);
PUBLIC json_t *authenticate_parser(hgobj gobj_service, json_t *kw, hgobj src);

#ifdef __cplusplus
}
#endif
