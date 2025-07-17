/****************************************************************************
 *          C_CONTROLCENTER.H
 *          Controlcenter GClass.
 *
 *          Control Center of Yuneta Systems
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
#define GCLASS_CONTROLCENTER_NAME "Controlcenter"
#define GCLASS_CONTROLCENTER gclass_controlcenter()

/***************************************************************
 *              Prototypes
 ***************************************************************/
PUBLIC GCLASS *gclass_controlcenter(void);

#ifdef __cplusplus
}
#endif
