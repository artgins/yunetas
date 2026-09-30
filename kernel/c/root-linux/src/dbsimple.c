/***********************************************************************
 *          dbsimple.c
 *
 *          Simple DB file for persistent attributes
 *
 *          Copyright (c) 2015 Niyamaka.
 *          Copyright (c) 2024-2026, ArtGins.
 *          All Rights Reserved.
***********************************************************************/
#include <string.h>
#include <unistd.h>
#include <limits.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/stat.h>

#include <kwid.h>
#include "yunetas_environment.h"
#include "dbsimple.h"

/***************************************************************
 *              Constants
 ***************************************************************/

/***************************************************************
 *              Structures
 ***************************************************************/

/***************************************************************
 *              Prototypes
 ***************************************************************/

/***************************************************************
 *              Data
 ***************************************************************/

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE char *get_persist_filename(
    hgobj gobj,
    char *bf,
    int bflen,
    const char *label,
    BOOL create_directories)
{
    char filename[NAME_MAX];
    snprintf(filename, sizeof(filename), "%s-%s-%s.json",
        gobj_gclass_name(gobj),
        gobj_name(gobj),
        label
    );

    return yuneta_realm_file(bf, bflen, "data", filename, create_directories);
}

/***************************************************************************
 *  Make the persistent attrs file 0600, the mode save_json() gives it.
 *  Return 0 when it is 0600, -1 when it is not a file to use at all,
 *  -2 when it is a regular file that stays wider than 0600.
 ***************************************************************************/
PRIVATE int narrow_file_mode(hgobj gobj, int fd, const char *filename)
{
    struct stat st;
    if(fstat(fd, &st) < 0) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_SYSTEM,
            "msg",          "%s", "Cannot stat the persistent attrs file",
            "path",         "%s", filename,
            "errno",        "%d", errno,
            "serrno",       "%s", strerror(errno),
            NULL
        );
        return -1;
    }
    if(!S_ISREG(st.st_mode)) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_SYSTEM,
            "msg",          "%s", "The persistent attrs file is not a regular file",
            "path",         "%s", filename,
            NULL
        );
        return -1;
    }
    if((st.st_mode & 07777) == 0600) {
        return 0;
    }
    char mode[16];
    snprintf(mode, sizeof(mode), "0%o", (unsigned)(st.st_mode & 07777));
    if(fchmod(fd, 0600) < 0) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_SYSTEM,
            "msg",          "%s", "Cannot make the persistent attrs file 0600",
            "path",         "%s", filename,
            "mode",         "%s", mode,
            "errno",        "%d", errno,
            "serrno",       "%s", strerror(errno),
            NULL
        );
        return -2;
    }
    gobj_log_info(gobj, 0,
        "function",     "%s", __FUNCTION__,
        "msgset",       "%s", MSGSET_SYSTEM,
        "msg",          "%s", "Persistent attrs file made 0600",
        "path",         "%s", filename,
        "old_mode",     "%s", mode,
        NULL
    );
    return 0;
}

/***************************************************************************
 *  O_NOFOLLOW: the data dirs are 02775, so a member of the group could
 *  plant a symlink in place of the file.
 ***************************************************************************/
PRIVATE int open_persist_file(hgobj gobj, const char *filename, int flags)
{
    int fd = open(filename, flags|O_NOFOLLOW|O_CLOEXEC, 0600);
    if(fd < 0) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_SYSTEM,
            "msg",          "%s", errno==ELOOP?
                "Refused the persistent attrs file: it is a symlink" :
                "Cannot open the persistent attrs file",
            "path",         "%s", filename,
            "errno",        "%d", errno,
            "serrno",       "%s", strerror(errno),
            NULL
        );
        return -1;
    }
    return fd;
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE json_t *load_json(
    hgobj gobj
)
{
    char filename[PATH_MAX];
    get_persist_filename(gobj, filename, sizeof(filename), "persistent-attrs", FALSE);

    struct stat st;
    if(lstat(filename, &st) < 0) {
        if(errno != ENOENT) {
            gobj_log_error(gobj, 0,
                "function",     "%s", __FUNCTION__,
                "msgset",       "%s", MSGSET_SYSTEM,
                "msg",          "%s", "Cannot stat the persistent attrs file",
                "path",         "%s", filename,
                "errno",        "%d", errno,
                "serrno",       "%s", strerror(errno),
                NULL
            );
        }
        // No persistent attrs saved
        return 0;
    }

    int fd = open_persist_file(gobj, filename, O_RDONLY|O_NONBLOCK);
    if(fd < 0) {
        // Error already logged
        return 0;
    }

    /*
     *  A file written before 7.25.19 is 0664 with the secrets in it, and a
     *  secret set once (the emailsender password) is never saved again.
     *  One that stays wide is still loaded: refusing it would not hide it.
     */
    if(narrow_file_mode(gobj, fd, filename) == -1) {
        // Error already logged
        close(fd);
        return 0;
    }

    size_t flags = 0;
    json_error_t error;
    json_t *jn_device = json_loadfd(fd, flags, &error);
    close(fd);
    if(!jn_device) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_JSON,
            "msg",          "%s", "Cannot load device json database",
            "path",         "%s", filename,
            "error",        "%s", error.text,
            "line",         "%d", error.line,
            NULL
        );
    }
    return jn_device;
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE int save_json(
    hgobj gobj,
    json_t *jn // owned
)
{
    char filename[PATH_MAX];
    get_persist_filename(gobj, filename, sizeof(filename), "persistent-attrs", TRUE);

    /*
     *  0600: a persistent attr can be a secret (the SMTP password of the
     *  emailsender, set with set-email-user). Up to 7.25.18 json_dump_file()
     *  created it with the process umask -- 0666 on every node. A file that
     *  cannot be made 0600 (not ours) is not written: the secret would land
     *  in a file others read. Truncated only after that, so a refused save
     *  leaves the file as it was.
     */
    int fd = open_persist_file(gobj, filename, O_WRONLY|O_CREAT|O_NONBLOCK);
    if(fd < 0) {
        // Error already logged
        JSON_DECREF(jn)
        return -1;
    }
    if(narrow_file_mode(gobj, fd, filename) != 0) {
        // Error already logged
        close(fd);
        JSON_DECREF(jn)
        return -1;
    }
    if(ftruncate(fd, 0) < 0) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_SYSTEM,
            "msg",          "%s", "Cannot truncate the persistent attrs file",
            "path",         "%s", filename,
            "errno",        "%d", errno,
            "serrno",       "%s", strerror(errno),
            NULL
        );
        close(fd);
        JSON_DECREF(jn)
        return -1;
    }

    int ret = json_dumpfd(jn, fd, JSON_INDENT(4));
    if(ret < 0) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_JSON,
            "msg",          "%s", "Cannot save device json database",
            "path",         "%s", filename,
            NULL
        );
        close(fd);
        JSON_DECREF(jn)
        return -1;
    }
    if(close(fd) < 0) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_SYSTEM,
            "msg",          "%s", "Cannot close the persistent attrs file",
            "path",         "%s", filename,
            "errno",        "%d", errno,
            "serrno",       "%s", strerror(errno),
            NULL
        );
        JSON_DECREF(jn)
        return -1;
    }
    JSON_DECREF(jn)
    return 0;
}

/***************************************************************************
   Load writable persistent attributes from simple file db
 ***************************************************************************/
PUBLIC int db_load_persistent_attrs(
    hgobj gobj,
    json_t *keys  // owned
) {
    json_t *jn_file = load_json(gobj);
    if(jn_file) {
        json_t *attrs = kw_clone_by_keys(
            gobj,
            jn_file,    // owned
            keys,       // owned
            FALSE
        );

        gobj_write_attrs(gobj, attrs, SDF_PERSIST, 0);
    } else {
        JSON_DECREF(keys)
    }

    return 0;
}

/***************************************************************************
 *
 ***************************************************************************/
PUBLIC int db_save_persistent_attrs(
    hgobj gobj,
    json_t *keys  // owned
) {
    json_t *jn_attrs = gobj_read_attrs(gobj, SDF_PERSIST, 0);
    json_t *attrs = kw_clone_by_keys(
        gobj,
        jn_attrs,   // owned
        keys,       // owned
        FALSE
    );

    json_t *jn_file = load_json(gobj);
    if(jn_file) {
        json_object_update_missing(attrs, jn_file);
        JSON_DECREF(jn_file)
    }

    return save_json(
        gobj,
        attrs  // owned
    );
}

/***************************************************************************
 *
 ***************************************************************************/
PUBLIC int db_remove_persistent_attrs(
    hgobj gobj,
    json_t *keys  // owned
) {
    json_t *jn_file = load_json(gobj);

    json_t *attrs = kw_clone_by_not_keys(
        gobj,
        jn_file,    // owned
        keys,       // owned
        FALSE
    );

    return save_json(
        gobj,
        attrs  // owned
    );
}

/***************************************************************************
 *
 ***************************************************************************/
PUBLIC json_t *db_list_persistent_attrs(
    hgobj gobj,
    json_t *keys  // owned
) {
    json_t *jn_file = load_json(gobj);

    json_t *attrs = kw_clone_by_keys(
        gobj,
        jn_file,    // owned
        keys,       // owned
        FALSE
    );
    return attrs;
}
