/****************************************************************************
 *          c_yuno.h
 *
 *          GClass __yuno__
 *          Low level
 *
 *          Copyright (c) 2023 Niyamaka.
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
 *
 *  PROCESS-level, not gclass interface: the loop is one per process and every
 *  gclass that opens a fd asks for it. An attribute read would work and would
 *  be worse -- it hides a process global behind an instance lookup, on hot
 *  paths.
 */
PUBLIC void *yuno_event_loop(void);
PUBLIC void yuno_event_destroy(void);

/*
 *  End this process, orderly: flush the log, set the exit code and leave the
 *  event loop. PROCESS-level too, and the reason it is not an event: every
 *  caller is ending its OWN process (a signal handler, a CLI that was told to
 *  quit, a test yuno that finished), not ordering another gobj about. An
 *  event would be a second door to the same thing.
 */
PUBLIC void set_yuno_must_die(void);

/*--------------------------------------------------*
 *  Allowed ips for authz without jwt
 *      (in addition to local ip with yuneta user)
 *  Denied ips for authz without jwt (prevalence over allowed)
 *--------------------------------------------------*/
/*
 *  Only the READ half is exported: c_tcp_s asks it once per accepted
 *  connection and c_authz once per login, and a local method would cost a kw
 *  on those paths. Writing goes through the interface like anything else --
 *  the `allowed_ips`/`denied_ips` attributes (SDF_PERSIST) and the
 *  add-allowed-ip / remove-allowed-ip / add-denied-ip / remove-denied-ip
 *  commands.
 */
PUBLIC BOOL is_ip_allowed(const char *peername);
PUBLIC BOOL is_ip_denied(const char *peername);

/*--------------------------------------------------*
 *  Sending signals to any yuno (10 SIGUSR1) (12 SIGUSR2)
    killall -10 <yuno_name>
        rotating between
            TRACE_GLOBAL_LEVEL0,
            TRACE_GLOBAL_LEVEL1,
            TRACE_GLOBAL_LEVEL2,
            nothing

    killall -12 <yuno_name>
        rotating between
            gobj_set_deep_tracing(1)
            gobj_set_deep_tracing(0)
 *--------------------------------------------------*/

#ifdef __cplusplus
}
#endif
