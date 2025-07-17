/****************************************************************************
 *          C_PTY.H
 *          Pty GClass.
 *
 *          Pseudoterminal uv-mixin.
 *
 *          Copyright (c) 2021 Niyamaka.
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
#define GCLASS_PTY_NAME "Pty"
#define GCLASS_PTY gclass_pty()

/***************************************************************
 *              Prototypes
 ***************************************************************/
PUBLIC GCLASS *gclass_pty(void);

#ifdef __cplusplus
}
#endif
