/****************************************************************************
 *          c_yuno.h
 *
 *          GClass __yuno__
 *          Low level
 *
 *          Copyright (c) 2023 Niyamaka.
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
GOBJ_DECLARE_GCLASS(C_YUNO);

/*------------------------*
 *      States
 *------------------------*/

/*------------------------*
 *      Events
 *------------------------*/

/***************************************************************
 *              Prototypes
 ***************************************************************/
PUBLIC int register_c_yuno(void);

// PUBLIC int gobj_post_event(
//     hgobj dst,
//     gobj_event_t event,
//     json_t *kw,  // owned
//     hgobj src
// );

/*
 *  Get yuno event loop
 *  Return void * to hide #include <yev_loop.h> dependency
 */
PUBLIC void *yuno_event_loop(void);

PUBLIC void set_yuno_must_die(void);

/*--------------------------------------------------*
 *  Allowed ips for authz without jwt
 *      (in addition to local ip with yuneta user)
 *  Denied ips for authz without jwt (prevalence over allowed)
 *--------------------------------------------------*/
PUBLIC BOOL is_ip_allowed(const char *peername);
PUBLIC int add_allowed_ip(const char *ip, BOOL allowed); // allowed: TRUE to allow, FALSE to not allow
PUBLIC int remove_allowed_ip(const char *ip); // Remove from interna list

PUBLIC BOOL is_ip_denied(const char *peername);
PUBLIC int add_denied_ip(const char *ip, BOOL denied); // denied: TRUE to deny, FALSE to not deny
PUBLIC int remove_denied_ip(const char *ip); // Remove from interna list

PUBLIC unsigned long free_ram_in_kb(void);

#ifdef __cplusplus
}
#endif
