/****************************************************************************
 *          C_PROT_HTTP_SRV.H
 *          Prot_http_srv GClass.
 *
 *          Protocol http as server
 *
 *          Copyright (c) 2018-2021 Niyamaka.
 *          All Rights Reserved.
 ****************************************************************************/
#pragma once

#include <ginsfsm.h>
#include "ghttp_parser.h"
#include "c_timer.h"
#include "c_connex.h"

#ifdef __cplusplus
extern "C"{
#endif

/***************************************************************
 *              Constants
 ***************************************************************/
#define GCLASS_PROT_HTTP_SRV_NAME "Prot_http_srv"
#define GCLASS_PROT_HTTP_SRV gclass_prot_http_srv()

/***************************************************************
 *              Prototypes
 ***************************************************************/
PUBLIC GCLASS *gclass_prot_http_srv(void);

#ifdef __cplusplus
}
#endif
