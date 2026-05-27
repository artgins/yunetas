/****************************************************************************
 *          c_smtp_session.h
 *          Smtp_session GClass.
 *
 *          SMTP client protocol over C_TCP.
 *          Implements the SMTP submission FSM (banner, EHLO, AUTH PLAIN,
 *          MAIL FROM, RCPT TO, DATA, QUIT) on top of an implicit-TLS
 *          transport (smtps://, SMTPS). Sits in the C_PROT_* slot of
 *          the canonical __output_side__ stack:
 *
 *              C_MQIOGATE > C_QIOGATE > C_IOGATE > C_CHANNEL
 *                  > C_SMTP_SESSION  <- this gclass
 *                      > C_TCP
 *
 *          Copyright (c) 2026, ArtGins.
 *          All Rights Reserved.
 ****************************************************************************/
#pragma once

#include <yunetas.h>

#ifdef __cplusplus
extern "C"{
#endif

/***************************************************************
 *              FSM
 ***************************************************************/
/*------------------------*
 *      GClass name
 *------------------------*/
GOBJ_DECLARE_GCLASS(C_SMTP_SESSION);

/*------------------------*
 *      States
 *------------------------*/

/*------------------------*
 *      Events
 *------------------------*/

/***************************************************************
 *              Prototypes
 ***************************************************************/
PUBLIC int register_c_smtp_session(void);

#ifdef __cplusplus
}
#endif
