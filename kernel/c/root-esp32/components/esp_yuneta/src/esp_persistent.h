/****************************************************************************
 *          loop.h
 *          GObj loop
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
 *              Prototypes
 ***************************************************************/
int dbesp_startup_persistent_attrs(void);
void dbesp_end_persistent_attrs(void);

/**rst**
   Load writable persistent attributes from simple file db
**rst**/
PUBLIC int dbesp_load_persistent_attrs(
    hgobj gobj,
    json_t *keys  // owned
);

/**rst**
   Save writable persistent attributes from simple file db
**rst**/
PUBLIC int dbesp_save_persistent_attrs(
    hgobj gobj,
    json_t *keys  // owned
);

/**rst**
   Remove writable persistent attributes from simple file db
**rst**/
PUBLIC int dbesp_remove_persistent_attrs(
    hgobj gobj,
    json_t *keys  // owned
);

/**rst**
   List persistent attributes from simple file db
**rst**/
PUBLIC json_t *dbesp_list_persistent_attrs(
    hgobj gobj,
    json_t *keys  // owned
);

#ifdef __cplusplus
}
#endif
