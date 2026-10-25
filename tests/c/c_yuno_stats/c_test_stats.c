/****************************************************************************
 *          C_TEST_STATS.C
 *
 *          The stats of a yuno tell its memory and the fullest filesystem it
 *          writes to (c_yuno.c, load_memory_stats() / load_disk_stats()).
 *          This gobj opens a C_TRANGER whose store is in /dev/shm, a tmpfs, so
 *          the yuno writes to two filesystems: /yuneta and that one. After
 *          a few readings of the stats it checks them:
 *              - mem_rss_kb, mem_node_total_mb > 0,
 *                mem_node_available_percent in 0..100;
 *              - disk_min_free_percent in 0..100, and its path on the
 *                fuller of the two filesystems, as statvfs() measures them
 *                here (a point of slack: the two readings are not at once).
 *          Up to 7.28.3 a yuno told only of /yuneta, and the queues of a
 *          simulator filled a ramdisk unseen.
 *
 *          Copyright (c) 2026, ArtGins.
 *          All Rights Reserved.
 ***********************************************************************/
#include <string.h>
#include <sys/statvfs.h>

#include "c_test_stats.h"

/***************************************************************************
 *              Constants
 ***************************************************************************/

/***************************************************************************
 *              Structures
 ***************************************************************************/

/***************************************************************************
 *              Prototypes
 ***************************************************************************/

/***************************************************************************
 *          Data: config, public data, private data
 ***************************************************************************/
/*---------------------------------------------*
 *      Attributes
 *---------------------------------------------*/
PRIVATE sdata_desc_t attrs_table[] = {
/*-ATTR-type------------name----------------flag----------------default-----description--*/
SDATA (DTP_POINTER,     "subscriber",       0,                  0,          "subscriber of output-events. Not a child gobj."),
SDATA_END()
};

/*---------------------------------------------*
 *      GClass trace levels
 *---------------------------------------------*/
enum {
    TRACE_MESSAGES  = 0x0001,
};
PRIVATE const trace_level_t s_user_trace_level[16] = {
{"messages",        "Trace messages"},
{0, 0},
};

/*---------------------------------------------*
 *              Private data
 *---------------------------------------------*/
#define STORE_ROOT  "/dev/shm/yuneta_test_c_yuno_stats"

typedef struct _PRIVATE_DATA {
    hgobj timer;
    hgobj gobj_tranger;
} PRIVATE_DATA;





                    /******************************
                     *      Framework Methods
                     ******************************/




/***************************************************************************
 *      Framework Method create
 ***************************************************************************/
PRIVATE void mt_create(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    priv->timer = gobj_create_pure_child(gobj_name(gobj), C_TIMER, 0, gobj);

    rmrdir(STORE_ROOT);
    mkrdir(STORE_ROOT, 02770);
    json_t *kw_tranger = json_pack("{s:s, s:s, s:b}",
        "path", STORE_ROOT,
        "database", "stats_store",
        "master", 1
    );
    priv->gobj_tranger = gobj_create_pure_child("stats_store", C_TRANGER, kw_tranger, gobj);
}

/***************************************************************************
 *      Framework Method start
 ***************************************************************************/
PRIVATE int mt_start(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    gobj_start(priv->timer);
    gobj_start(priv->gobj_tranger);
    return 0;
}

/***************************************************************************
 *      Framework Method stop
 ***************************************************************************/
PRIVATE int mt_stop(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    clear_timeout(priv->timer);
    gobj_stop(priv->timer);
    gobj_stop(priv->gobj_tranger);
    return 0;
}

/***************************************************************************
 *      Framework Method play
 ***************************************************************************/
PRIVATE int mt_play(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    set_timeout(priv->timer, 3000);    // the stats are read every second
    return 0;
}

/***************************************************************************
 *      Framework Method pause
 ***************************************************************************/
PRIVATE int mt_pause(hgobj gobj)
{
    return 0;
}

/***************************************************************************
 *      Framework Method destroy
 ***************************************************************************/
PRIVATE void mt_destroy(hgobj gobj)
{
    rmrdir(STORE_ROOT);
}




                    /***************************
                     *      Commands
                     ***************************/




                    /***************************
                     *      Local Methods
                     ***************************/




/***************************************************************************
 *  Free percent of the filesystem of path, -1 if it cannot be measured
 ***************************************************************************/
PRIVATE int free_percent_of(const char *path)
{
    struct statvfs st;
    if(statvfs(path, &st) != 0 || st.f_blocks == 0) {
        return -1;
    }
    return (int)((st.f_bavail * 100) / st.f_blocks);
}

/***************************************************************************
 *  An error of the check, said
 ***************************************************************************/
PRIVATE void check_failed(hgobj gobj, const char *what, json_int_t value)
{
    gobj_log_error(gobj, 0,
        "function",     "%s", __FUNCTION__,
        "msgset",       "%s", MSGSET_INTERNAL,
        "msg",          "%s", "yuno stats check FAILED",
        "what",         "%s", what,
        "value",        "%d", (int)value,
        NULL
    );
}

/***************************************************************************
 *  The stats of the yuno, against what the system says here
 ***************************************************************************/
PRIVATE void check_stats(hgobj gobj)
{
    hgobj yuno = gobj_yuno();

    json_int_t rss = gobj_read_integer_attr(yuno, "mem_rss_kb");
    json_int_t total = gobj_read_integer_attr(yuno, "mem_node_total_mb");
    json_int_t available = gobj_read_integer_attr(yuno, "mem_node_available_percent");
    if(rss <= 0) {
        check_failed(gobj, "mem_rss_kb", rss);
    }
    if(total <= 0) {
        check_failed(gobj, "mem_node_total_mb", total);
    }
    if(available < 0 || available > 100) {
        check_failed(gobj, "mem_node_available_percent", available);
    }

    json_int_t min_free = gobj_read_integer_attr(yuno, "disk_min_free_percent");
    const char *min_path = gobj_read_str_attr(yuno, "disk_min_free_path");
    if(min_free < 0 || min_free > 100) {
        check_failed(gobj, "disk_min_free_percent", min_free);
    }

    int free_yuneta = free_percent_of("/yuneta");
    int free_store = free_percent_of(STORE_ROOT);
    BOOL on_store = strncmp(min_path, STORE_ROOT, strlen(STORE_ROOT)) == 0;
    BOOL on_yuneta = strcmp(min_path, "/yuneta") == 0;
    if(!on_store && !on_yuneta) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_INTERNAL,
            "msg",          "%s", "yuno stats check FAILED",
            "what",         "%s", "disk_min_free_path is neither /yuneta nor the store",
            "path",         "%s", min_path,
            NULL
        );
    } else {
        int expected = on_store? free_store : free_yuneta;
        int other = on_store? free_yuneta : free_store;
        if(expected - other > 1) {
            check_failed(gobj, "disk_min_free_path is not on the fuller filesystem", min_free);
        }
        if(min_free - expected > 1 || expected - min_free > 1) {
            check_failed(gobj, "disk_min_free_percent against statvfs", min_free);
        }
    }

    set_yuno_must_die();
}




                    /***************************
                     *      Actions
                     ***************************/




/***************************************************************************
 *
 ***************************************************************************/
PRIVATE int ac_timeout(hgobj gobj, gobj_event_t event, json_t *kw, hgobj src)
{
    check_stats(gobj);

    JSON_DECREF(kw)
    return 0;
}




/***************************************************************************
 *                          FSM
 ***************************************************************************/
/*---------------------------------------------*
 *          Global methods table
 *---------------------------------------------*/
PRIVATE const GMETHODS gmt = {
    .mt_create = mt_create,
    .mt_destroy = mt_destroy,
    .mt_start = mt_start,
    .mt_stop = mt_stop,
    .mt_play = mt_play,
    .mt_pause = mt_pause,
};

/*------------------------*
 *      GClass name
 *------------------------*/
GOBJ_DEFINE_GCLASS(C_TEST_STATS);

/*------------------------*
 *      States
 *------------------------*/

/*------------------------*
 *      Events
 *------------------------*/

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE int create_gclass(gclass_name_t gclass_name)
{
    static hgclass __gclass__ = 0;
    if(__gclass__) {
        gobj_log_error(0, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_INTERNAL,
            "msg",          "%s", "GClass ALREADY created",
            "gclass",       "%s", gclass_name,
            NULL
        );
        return -1;
    }

    /*----------------------------------------*
     *          Define States
     *----------------------------------------*/
    ev_action_t st_idle[] = {
        {EV_TIMEOUT,                ac_timeout,                 0},
        {0,0,0}
    };

    states_t states[] = {
        {ST_IDLE,                   st_idle},
        {0, 0}
    };

    event_type_t event_types[] = {
        {EV_TIMEOUT,                0},
        {0, 0}
    };

    /*----------------------------------------*
     *          Create the gclass
     *----------------------------------------*/
    __gclass__ = gclass_create(
        gclass_name,
        event_types,
        states,
        &gmt,
        0,  // lmt,
        attrs_table,
        sizeof(PRIVATE_DATA),
        0,  // authz_table,
        0,  // command_table,
        s_user_trace_level,  // s_user_trace_level,
        0   // gcflag_t
    );
    if(!__gclass__) {
        // Error already logged
        return -1;
    }

    return 0;
}

/***************************************************************************
 *
 ***************************************************************************/
PUBLIC int register_c_test_stats(void)
{
    return create_gclass(C_TEST_STATS);
}
