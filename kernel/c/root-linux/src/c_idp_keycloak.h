/****************************************************************************
 *          c_idp_keycloak.h
 *          IdpKeycloak GClass.
 *
 *          Provision accounts in a Keycloak identity provider
 *
 *          Copyright (c) 2026, ArtGins.
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
GOBJ_DECLARE_GCLASS(C_IDP_KEYCLOAK);

/*------------------------*
 *      States
 *------------------------*/

/*------------------------*
 *      Events
 *------------------------*/

/***************************************************************
 *              Prototypes
 ***************************************************************/
PUBLIC int register_c_idp_keycloak(void);

#ifdef __cplusplus
}
#endif
