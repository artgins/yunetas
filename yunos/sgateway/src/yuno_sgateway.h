/****************************************************************************
 *          YUNO_SGATEWAY.H
 *          Sgateway yuno.
 *
 *          Copyright (c) 2020 Niyamaka.
 *          All Rights Reserved.
 ****************************************************************************/
#pragma once

#include <yuneta.h>

#ifdef __cplusplus
extern "C"{
#endif

/***************************************************************
 *              Constants
 ***************************************************************/
#define GCLASS_YUNO_SGATEWAY_NAME "YSgateway"
#define ROLE_SGATEWAY "sgateway"

/***************************************************************
 *              Prototypes
 ***************************************************************/
PUBLIC void register_yuno_sgateway(void);

#ifdef __cplusplus
}
#endif
