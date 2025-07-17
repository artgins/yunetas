/****************************************************************************
 *          YUNO_WATCHFS.H
 *          Watchfs yuno.
 *
 *          Copyright (c) 2016 Niyamaka.
 *          All Rights Reserved.
 ****************************************************************************/
#ifndef _YUNO_WATCHFS_H
#define _YUNO_WATCHFS_H 1

#include <yuneta.h>

#ifdef __cplusplus
extern "C"{
#endif

/*********************************************************************
 *      Prototypes
 *********************************************************************/
PUBLIC void register_yuno_watchfs(void);

#define GCLASS_YUNO_WATCHFS_NAME "YWatchfs"
#define ROLE_WATCHFS "watchfs"

#ifdef __cplusplus
}
#endif


#endif
