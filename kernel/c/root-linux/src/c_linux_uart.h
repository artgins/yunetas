/****************************************************************************
 *          c_linux_uart.h
 *
 *          GClass Uart
 *          Low level linux TODO no sé si hace falta todavia
 *
 *          Copyright (c) 2023 Niyamaka, 2024 ArtGins.
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
GOBJ_DECLARE_GCLASS(C_LINUX_UART);

/*------------------------*
 *      States
 *------------------------*/

/*------------------------*
 *      Events
 *------------------------*/

/***************************************************************
 *              Prototypes
 ***************************************************************/
PUBLIC int register_c_linux_uart(void);

#ifdef __cplusplus
}
#endif
