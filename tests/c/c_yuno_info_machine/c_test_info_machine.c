/****************************************************************************
 *          C_TEST_INFO_MACHINE.C
 *
 *          Asks its yuno for `info-machine` (c_yuno.c, cmd_info_machine())
 *          and checks the answer:
 *              - `ifs` has one entry per interface name, in the order of
 *                the first time getifaddrs() names it, each with the
 *                IPv4/IPv6 addresses getifaddrs() gives for it;
 *              - with a hostname that is not utf-8 (get_hostname() is
 *                wrapped, -Wl,--wrap=get_hostname, for one call) the answer
 *                cannot be built: result -1, and one ERROR. Up to 7.31.29
 *                the answer went with result 0, no data and no log, after
 *                a read of the os info that json_pack() had freed.
 *
 *          Copyright (c) 2026, ArtGins.
 *          All Rights Reserved.
 ***********************************************************************/
#include <string.h>
#include <errno.h>
#include <ifaddrs.h>
#include <sys/socket.h>

#include "c_test_info_machine.h"

/***************************************************************************
 *              Constants
 ***************************************************************************/
#define BAD_HOSTNAME    "host\xff\xfe-not-utf8"

/***************************************************************************
 *              Structures
 ***************************************************************************/

/***************************************************************************
 *              Prototypes
 ***************************************************************************/
const char *__real_get_hostname(void);
const char *__wrap_get_hostname(void);

/***************************************************************************
 *          Data: config, public data, private data
 ***************************************************************************/
PRIVATE BOOL s_bad_hostname_once = FALSE;   /* the next get_hostname() is not utf-8 */

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
typedef struct _PRIVATE_DATA {
    hgobj timer;
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
}

/***************************************************************************
 *      Framework Method start
 ***************************************************************************/
PRIVATE int mt_start(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    gobj_start(priv->timer);
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
    return 0;
}

/***************************************************************************
 *      Framework Method play
 ***************************************************************************/
PRIVATE int mt_play(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    set_timeout(priv->timer, 100);
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
}




                    /***************************
                     *      Commands
                     ***************************/




                    /***************************
                     *      Local Methods
                     ***************************/




/***************************************************************************
 *  The hostname of the machine, but the one call after
 *  s_bad_hostname_once is set: a name that is not utf-8
 ***************************************************************************/
const char *__wrap_get_hostname(void)
{
    if(s_bad_hostname_once) {
        s_bad_hostname_once = FALSE;
        return BAD_HOSTNAME;
    }
    return __real_get_hostname();
}

/***************************************************************************
 *  An error of the check, said
 ***************************************************************************/
PRIVATE void check_failed(hgobj gobj, const char *what, const char *detail)
{
    gobj_log_error(gobj, 0,
        "function",     "%s", __FUNCTION__,
        "msgset",       "%s", MSGSET_INTERNAL,
        "msg",          "%s", "info-machine check FAILED",
        "what",         "%s", what,
        "detail",       "%s", detail? detail : "",
        NULL
    );
}

/***************************************************************************
 *  The interfaces as getifaddrs() names them: [{name, n_addresses}], in
 *  the order of the first time each name comes
 ***************************************************************************/
PRIVATE json_t *expected_interfaces(hgobj gobj)
{
    struct ifaddrs *ifaddr;
    if(getifaddrs(&ifaddr) == -1) {
        check_failed(gobj, "getifaddrs() of the test", strerror(errno));
        return NULL;
    }
    json_t *jn_list = json_array();
    json_t *jn_by_name = json_object();
    for(struct ifaddrs *ifa = ifaddr; ifa != NULL; ifa = ifa->ifa_next) {
        if(!ifa->ifa_name) {
            continue;
        }
        json_t *jn_iface = json_object_get(jn_by_name, ifa->ifa_name);
        if(!jn_iface) {
            jn_iface = json_pack("{s:s, s:i}", "name", ifa->ifa_name, "n_addresses", 0);
            json_array_append_new(jn_list, jn_iface);
            json_object_set(jn_by_name, ifa->ifa_name, jn_iface);
        }
        if(ifa->ifa_addr && (ifa->ifa_addr->sa_family == AF_INET ||
                ifa->ifa_addr->sa_family == AF_INET6)) {
            json_object_set_new(jn_iface, "n_addresses", json_integer(
                json_integer_value(json_object_get(jn_iface, "n_addresses")) + 1));
        }
    }
    JSON_DECREF(jn_by_name)
    freeifaddrs(ifaddr);
    return jn_list;
}

/***************************************************************************
 *  The `ifs` of the answer, against getifaddrs()
 ***************************************************************************/
PRIVATE void check_interfaces(hgobj gobj, json_t *jn_ifs)
{
    json_t *jn_expected = expected_interfaces(gobj);
    if(!jn_expected) {
        return;     // Error already logged
    }
    if(!json_is_array(jn_ifs) || json_array_size(jn_ifs) != json_array_size(jn_expected)) {
        check_failed(gobj, "ifs: not one entry per interface name", NULL);
        JSON_DECREF(jn_expected)
        return;
    }
    size_t idx;
    json_t *jn_exp;
    json_array_foreach(jn_expected, idx, jn_exp) {
        json_t *jn_iface = json_array_get(jn_ifs, idx);
        const char *name = json_string_value(json_object_get(jn_exp, "name"));
        const char *got = json_string_value(json_object_get(jn_iface, "name"));
        if(!got || strcmp(got, name) != 0) {
            check_failed(gobj, "ifs: an interface out of the order of getifaddrs()", name);
        }
        if((json_int_t)json_array_size(json_object_get(jn_iface, "addresses")) !=
                json_integer_value(json_object_get(jn_exp, "n_addresses"))) {
            check_failed(gobj, "ifs: the addresses of an interface", name);
        }
    }
    JSON_DECREF(jn_expected)
}

/***************************************************************************
 *  info-machine, with the hostname of the machine and with one that is
 *  not utf-8
 ***************************************************************************/
PRIVATE void check_info_machine(hgobj gobj)
{
    json_t *resp = gobj_command(gobj_yuno(), "info-machine", 0, gobj);
    json_t *jn_data = kw_get_dict(gobj, resp, "data", 0, 0);
    if(kw_get_int(gobj, resp, "result", -1, 0) != 0 || !jn_data) {
        check_failed(gobj, "info-machine answered no data", NULL);
    } else {
        const char *hostname = json_string_value(json_object_get(jn_data, "hostname"));
        if(!hostname || strcmp(hostname, __real_get_hostname()) != 0) {
            check_failed(gobj, "info-machine: the hostname", hostname);
        }
        check_interfaces(gobj, json_object_get(jn_data, "ifs"));
    }
    JSON_DECREF(resp)

    s_bad_hostname_once = TRUE;
    resp = gobj_command(gobj_yuno(), "info-machine", 0, gobj);
    if(s_bad_hostname_once) {
        s_bad_hostname_once = FALSE;
        check_failed(gobj, "info-machine did not read the hostname", NULL);
    }
    if(kw_get_int(gobj, resp, "result", 0, 0) != -1) {
        check_failed(gobj, "info-machine with a hostname not utf-8: not result -1", NULL);
    }
    if(json_is_object(json_object_get(resp, "data"))) {
        check_failed(gobj, "info-machine with a hostname not utf-8: data", NULL);
    }
    JSON_DECREF(resp)

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
    check_info_machine(gobj);

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
GOBJ_DEFINE_GCLASS(C_TEST_INFO_MACHINE);

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
PUBLIC int register_c_test_info_machine(void)
{
    return create_gclass(C_TEST_INFO_MACHINE);
}
