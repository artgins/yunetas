/****************************************************************************
 *          dbsimple.h
 *
 *          Simple DB file for persistent attributes
 *
 *          Copyright (c) 2015 Niyamaka.
 *          Copyright (c) 2024, ArtGins.
 *          All Rights Reserved.
 ****************************************************************************/
#pragma once

#include <gobj.h>

#ifdef __cplusplus
extern "C"{
#endif

/***************************************************
 *              Prototypes
 **************************************************/
/**rst**
   Load writable persistent attributes from simple file db
**rst**/
PUBLIC int db_load_persistent_attrs(
    hgobj gobj,
    json_t *keys  // owned
);

/**rst**
   Save writable persistent attributes from simple file db
**rst**/
PUBLIC int db_save_persistent_attrs(
    hgobj gobj,
    json_t *keys  // owned
);

/**rst**
   Remove writable persistent attributes from simple file db
**rst**/
PUBLIC int db_remove_persistent_attrs(
    hgobj gobj,
    json_t *keys  // owned
);

/**rst**
   List persistent attributes from simple file db
**rst**/
PUBLIC json_t *db_list_persistent_attrs(
    hgobj gobj,
    json_t *keys  // owned
);


#ifdef __cplusplus
}
#endif
