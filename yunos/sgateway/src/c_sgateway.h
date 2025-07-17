/****************************************************************************
 *          C_SGATEWAY.H
 *          Sgateway GClass.
 *
 *          Simple Gateway
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
#define GCLASS_SGATEWAY_NAME "Sgateway"
#define GCLASS_SGATEWAY gclass_sgateway()

/***************************************************************
 *              Prototypes
 ***************************************************************/
PUBLIC GCLASS *gclass_sgateway(void);

#ifdef __cplusplus
}
#endif
