/****************************************************************************
 *          c_timer.c
 *
 *          GClass Timer
 *          Low level esp-idf
 *
 *          Copyright (c) 2023 Niyamaka.
 *          Copyright (c) 2024, ArtGins.
 *          All Rights Reserved.
 ****************************************************************************/
#include <time.h>

#ifdef ESP_PLATFORM
  #include <esp_timer.h>
  #include <esp_event.h>
#endif
#include <g_ev_kernel.h>
#include <g_st_kernel.h>
#include <kwid.h>
#include "c_esp_yuno.h"
#include "c_timer.h"

/***************************************************************
 *              Constants
 ***************************************************************/

/***************************************************************
 *              Prototypes
 ***************************************************************/
#ifdef ESP_PLATFORM
static void timer_callback(void* arg);
#endif

/***************************************************************
 *              Data
 ***************************************************************/
/*---------------------------------------------*
 *          Attributes
 *---------------------------------------------*/
PRIVATE const sdata_desc_t tattr_desc[] = {
/*-ATTR-type--------name----------------flag------------default-----description---------- */
SDATA (DTP_INTEGER, "subscriber",       0,              0,          "subscriber of output-events. Default if null is parent."),
SDATA (DTP_BOOLEAN, "periodic",         SDF_RD,         "0",        "True for periodic timeouts"),
SDATA (DTP_INTEGER, "msec",             SDF_RD,         "0",        "Timeout in miliseconds"),
SDATA_END()
};

/*---------------------------------------------*
 *              Private data
 *---------------------------------------------*/
typedef struct _PRIVATE_DATA {
    BOOL periodic;
#ifdef ESP_PLATFORM
    esp_timer_handle_t esp_timer_handle;
#endif
} PRIVATE_DATA;

PRIVATE hgclass gclass = 0;





                    /******************************
                     *      Framework Methods
                     ******************************/




/***************************************************************************
 *      Framework Method
 ***************************************************************************/
PRIVATE void mt_create(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    SET_PRIV(periodic,          gobj_read_bool_attr)
}

/***************************************************************************
 *      Framework Method writing
 ***************************************************************************/
PRIVATE void mt_writing(hgobj gobj, const char *path)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    IF_EQ_SET_PRIV(periodic,    gobj_read_bool_attr)
    END_EQ_SET_PRIV()
}

/***************************************************************************
 *      Framework Method
 ***************************************************************************/
PRIVATE int mt_start(hgobj gobj)
{
#ifdef ESP_PLATFORM
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    /*--------------------------------*
     *      Create timer
     *--------------------------------*/
    esp_timer_create_args_t esp_timer_create_args = {
        .callback = &timer_callback,
        .arg = gobj,
        .skip_unhandled_events = true
    };

    ESP_ERROR_CHECK(esp_timer_create(&esp_timer_create_args, &priv->esp_timer_handle));
#endif
    return 0;
}

/***************************************************************************
 *      Framework Method
 ***************************************************************************/
PRIVATE int mt_stop(hgobj gobj)
{
#ifdef ESP_PLATFORM
    PRIVATE_DATA *priv = gobj_priv_data(gobj);
    esp_err_t err = esp_timer_delete(priv->esp_timer_handle);
    if(err) {
        gobj_log_error(gobj, LOG_OPT_TRACE_STACK,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_SYSTEM_ERROR,
            "msg",          "%s", "esp_timer_delete() FAILED",
            "esp_error",    "%s", esp_err_to_name(err),
            NULL
        );
    }
#endif
    return 0;
}

/***************************************************************************
 *      Framework Method
 ***************************************************************************/
PRIVATE int mt_play(hgobj gobj)
{
#ifdef ESP_PLATFORM
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    json_int_t msec = gobj_read_integer_attr(gobj, "msec");
    BOOL periodic = gobj_read_bool_attr(gobj, "periodic");

    if(esp_timer_is_active(priv->esp_timer_handle)) {
        esp_err_t err = esp_timer_stop(priv->esp_timer_handle);
        if(err != ESP_OK) {
            gobj_log_error(gobj, 0,
                "function",     "%s", __FUNCTION__,
                "msgset",       "%s", MSGSET_SYSTEM_ERROR,
                "msg",          "%s", "esp_timer_stop() FAILED",
                "esp_error",    "%s", esp_err_to_name(err),
                NULL
            );
        }
    }

    if(periodic) {
        esp_err_t err = esp_timer_start_periodic(priv->esp_timer_handle, msec*1000); // timer in microseconds
        if(err != ESP_OK) {
            gobj_log_error(gobj, LOG_OPT_TRACE_STACK,
                "function",     "%s", __FUNCTION__,
                "msgset",       "%s", MSGSET_SYSTEM_ERROR,
                "msg",          "%s", "esp_timer_start_periodic() FAILED",
                "esp_error",    "%s", esp_err_to_name(err),
                NULL
            );
        }
    } else {
        esp_err_t err = esp_timer_start_once(priv->esp_timer_handle, msec*1000); // timer in microseconds
        if(err != ESP_OK) {
            gobj_log_error(gobj, LOG_OPT_TRACE_STACK,
                "function",     "%s", __FUNCTION__,
                "msgset",       "%s", MSGSET_SYSTEM_ERROR,
                "msg",          "%s", "esp_timer_start_once() FAILED",
                "esp_error",    "%s", esp_err_to_name(err),
                NULL
            );
        }
    }
#endif

    return 0;
}

/***************************************************************************
 *      Framework Method
 ***************************************************************************/
PRIVATE int mt_pause(hgobj gobj)
{
#ifdef ESP_PLATFORM
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    if(esp_timer_is_active(priv->esp_timer_handle)) {
        esp_err_t err = esp_timer_stop(priv->esp_timer_handle);
        if(err != ESP_OK) {
            gobj_log_error(gobj, 0,
                "function",     "%s", __FUNCTION__,
                "msgset",       "%s", MSGSET_SYSTEM_ERROR,
                "msg",          "%s", "esp_timer_stop() FAILED",
                "esp_error",    "%s", esp_err_to_name(err),
                NULL
            );
        }
    }
#endif

    return 0;
}

/***************************************************************************
 *      Framework Method destroy
 ***************************************************************************/
PRIVATE void mt_destroy(hgobj gobj)
{
}




                    /***************************
                     *      Local methods
                     ***************************/




/***************************************************************************
 *  Callback that will be executed when the timer period lapses.
 *  Posts the timer expiry event to the default event loop.
 ***************************************************************************/
#ifdef ESP_PLATFORM
static void timer_callback(void* arg)
{
    hgobj gobj = arg;
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    if(priv->periodic) {
        gobj_post_event(gobj, EV_TIMEOUT_PERIODIC, 0, gobj);
    } else {
        gobj_post_event(gobj, EV_TIMEOUT, 0, gobj);
    }
}
#endif




                    /***************************
                     *      Actions
                     ***************************/




/***************************************************************************
 *  Resend to the parent
 ***************************************************************************/
PRIVATE int ac_timeout(hgobj gobj, gobj_event_t event, json_t *kw, hgobj src)
{
    gobj_publish_event(gobj, event, json_incref(kw));

    JSON_DECREF(kw)
    return 0;
}




                    /***************************
                     *          FSM
                     ***************************/




/*---------------------------------------------*
 *          Global methods table
 *---------------------------------------------*/
PRIVATE const GMETHODS gmt = {
    .mt_create = mt_create,
    .mt_writing = mt_writing,
    .mt_destroy = mt_destroy,
    .mt_start = mt_start,
    .mt_stop = mt_stop,
    .mt_play = mt_play,
    .mt_pause = mt_pause,
};

/*------------------------*
 *      GClass name
 *------------------------*/
GOBJ_DEFINE_GCLASS(C_TIMER);

/*------------------------*
 *      States
 *------------------------*/

/*------------------------*
 *      Events
 *------------------------*/
// HACK: Systems events: defined in gobj.h
//GOBJ_DEFINE_EVENT(EV_TIMEOUT);
//GOBJ_DEFINE_EVENT(EV_TIMEOUT_PERIODIC);

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE int create_gclass(gclass_name_t gclass_name)
{
    static hgclass __gclass__ = 0;
    if(gclass) {
        gobj_log_error(0, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_INTERNAL_ERROR,
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
        {EV_TIMEOUT,              ac_timeout,         0},
        {EV_TIMEOUT_PERIODIC,     ac_timeout,         0},
        {0,0,0}
    };
    states_t states[] = {
        {ST_IDLE,       st_idle},
        {0, 0}
    };

    /*----------------------------------------*
     *          Create the gclass
     *----------------------------------------*/
    gclass = gclass_create(
        gclass_name,
        0,  // event_types
        states,
        &gmt,
        0,  // lmt,
        tattr_desc,
        sizeof(PRIVATE_DATA),
        0,  // authz_table,
        0,  // command_table,
        0,  // s_user_trace_level
        gcflag_manual_start // gclass_flag
    );
    if(!gclass) {
        // Error already logged
        return -1;
    }

    return 0;
}

/***************************************************************************
 *
 ***************************************************************************/
PUBLIC int register_c_timer(void)
{
    return create_gclass(C_TIMER);
}




                    /***************************
                     *      Helpers
                     ***************************/




/***************************************************************************
 *  Set timeout
 ***************************************************************************/
PUBLIC void set_timeout(hgobj gobj, json_int_t msec)
{
    uint32_t level = TRACE_TIMER_PERIODIC|TRACE_TIMER;
    BOOL tracea = gobj_is_level_tracing(gobj, level) && !gobj_is_level_not_tracing(gobj, level);

    if(tracea) {
        trace_machine("⏲ ✅ set_timeout %ld: %s",
            (long)msec,
            gobj_full_name(gobj)
        );
    }

    if(gobj_is_playing(gobj)) { // clear_timeout(gobj);
        gobj_pause(gobj);
    }

    gobj_write_integer_attr(gobj, "msec", msec);
    gobj_write_bool_attr(gobj, "periodic", FALSE);
    gobj_play(gobj);
}

/***************************************************************************
 *  Set periodic timeout
 ***************************************************************************/
PUBLIC void set_timeout_periodic(hgobj gobj, json_int_t msec)
{
    uint32_t level = TRACE_TIMER_PERIODIC|TRACE_TIMER;
    BOOL tracea = gobj_is_level_tracing(gobj, level) && !gobj_is_level_not_tracing(gobj, level);

    if(tracea) {
        trace_machine("⏲ ⏲ ✅ set_timeout_periodic %ld: %s",
            (long)msec,
            gobj_full_name(gobj)
        );
    }

    if(gobj_is_playing(gobj)) { // clear_timeout(gobj);
        gobj_pause(gobj);
    }

    gobj_write_integer_attr(gobj, "msec", msec);
    gobj_write_bool_attr(gobj, "periodic", TRUE);
    gobj_play(gobj);
}

/***************************************************************************
 *  Clear timeout
 ***************************************************************************/
PUBLIC void IRAM_ATTR clear_timeout(hgobj gobj)
{
    if(gobj_get_deep_tracing()>1) {
        trace_machine("⏲ ❎ clear_timeout: %s",
            gobj_full_name(gobj)
        );
    }

    if(gobj_is_playing(gobj)) {
        gobj_pause(gobj);
    }
}
