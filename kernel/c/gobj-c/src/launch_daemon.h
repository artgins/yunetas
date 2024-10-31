/****************************************************************************
 *          launch_async_program.h
 *
 *          Launch a program without waiting his output
 *
 *          Copyright (c) 2024, ArtGins.
 *          All Rights Reserved.
 ****************************************************************************/
#pragma once

#include "gobj.h"

#ifdef __cplusplus
extern "C"{
#endif

/***************************************************
 *              Prototypes
 **************************************************/
PUBLIC int launch_daemon(const char *program, ...); // Return pid of daemon

#ifdef __cplusplus
}
#endif
