/****************************************************************************
 *          yuno_config_file.c
 *
 *          The configuration files the agent writes for a yuno. See
 *          yuno_config_file.h.
 *
 *          Copyright (c) 2026, ArtGins.
 *          All Rights Reserved.
 ****************************************************************************/
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <sys/stat.h>

#include "yuno_config_file.h"

/***************************************************************************
 *  See yuno_config_file.h
 ***************************************************************************/
PUBLIC int write_yuno_config_file(
    hgobj gobj,
    gbuffer_t *gbuf,
    const char *path
)
{
    int fd = newfile(path, YUNO_CONFIG_FILE_PERMISSION, TRUE);
    if(fd<0) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_SYSTEM,
            "msg",          "%s", "Cannot create the configuration file of a yuno",
            "path",         "%s", path,
            "errno",        "%d", errno,
            "strerror",     "%s", strerror(errno),
            NULL
        );
        gbuffer_decref(gbuf);
        return -1;
    }

    /*
     *  open() keeps the mode of a file that exists: a wider one would
     *  leave the secrets of the configuration readable
     */
    if(fchmod(fd, YUNO_CONFIG_FILE_PERMISSION)<0) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_SYSTEM,
            "msg",          "%s", "Cannot narrow the mode of the configuration file of a yuno, not written",
            "path",         "%s", path,
            "mode",         "0%o", YUNO_CONFIG_FILE_PERMISSION,
            "errno",        "%d", errno,
            "strerror",     "%s", strerror(errno),
            NULL
        );
        close(fd);
        gbuffer_decref(gbuf);
        return -1;
    }

    int ret = 0;
    size_t len;
    while((len=gbuffer_chunk(gbuf))>0) {
        char *p = gbuffer_get(gbuf, len);
        if(write(fd, p, len) != (ssize_t)len) {
            gobj_log_error(gobj, 0,
                "function",     "%s", __FUNCTION__,
                "msgset",       "%s", MSGSET_SYSTEM,
                "msg",          "%s", "Cannot write the configuration file of a yuno",
                "path",         "%s", path,
                "errno",        "%d", errno,
                "strerror",     "%s", strerror(errno),
                NULL
            );
            ret = -1;
            break;
        }
    }
    close(fd);
    gbuffer_decref(gbuf);
    return ret;
}
