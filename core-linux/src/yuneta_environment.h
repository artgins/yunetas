/****************************************************************************
 *              YUNETA_ENVIRONMENT.H
 *              Includes
 *              Copyright (c) 2014-2015 Niyamaka.
 *              All Rights Reserved.
 ****************************************************************************/
#pragma once

#ifdef __cplusplus
extern "C"{
#endif

#include <gobj.h>

/*********************************************************************
 *  Prototypes
 *********************************************************************/
PUBLIC int register_yuneta_environment(
    const char *work_dir,
    const char *domain_dir,
    int xpermission,    // permission for directories and executable files
    int rpermission,    // permission for regular files
    json_t *jn_config
);
PUBLIC json_t *yuneta_json_config(void);
PUBLIC int yuneta_xpermission(void);    // permission for directories and executable files
PUBLIC int yuneta_rpermission(void);    // permission for regular files

PUBLIC const char *yuneta_work_dir(void); // from environment.work_dir json config
PUBLIC const char *yuneta_domain_dir(void);

/*
 *  Final user functions
 *  work_dir            from environment.work_dir json config
 *  domain_dir          from environment.domain_dir json config
 */
PUBLIC char *yuneta_realm_dir(
    char *bf,
    int bfsize,
    const char *subdomain,
    BOOL create
);
PUBLIC char *yuneta_realm_file(
    char *bf,
    int bfsize,
    const char *subdomain,
    const char *filename,
    BOOL create
);

PUBLIC char *yuneta_store_dir(
    char *bf,
    int bfsize,
    const char *dir,
    const char *subdir,
    BOOL create
);
PUBLIC char *yuneta_store_file(
    char *bf,
    int bfsize,
    const char *dir,
    const char *subdir,
    const char *filename,
    BOOL create
);

PUBLIC char *yuneta_log_dir(
    char *bf,
    int bfsize,
    BOOL create
);
PUBLIC char *yuneta_log_file(
    char *bf,
    int bfsize,
    const char *filename,
    BOOL create
);

PUBLIC char *yuneta_bin_dir(
    char *bf,
    int bfsize,
    BOOL create
);

PUBLIC char *yuneta_bin_file(
    char *bf,
    int bfsize,
    const char *filename,
    BOOL create
);

PUBLIC char *yuneta_realm_store_dir(
    char *bf,
    int bfsize,
    const char *service,
    const char *owner,
    const char *realm_id,
    const char *dir,
    BOOL create
);

#ifdef __cplusplus
}
#endif
