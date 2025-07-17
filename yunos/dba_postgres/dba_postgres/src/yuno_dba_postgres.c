/***********************************************************************
 *          YUNO_DBA_POSTGRES.C
 *          Dba_postgres yuno.
 *
 *          Copyright (c) 2021 by Niyamaka.
 *          All Rights Reserved.
 ***********************************************************************/
#include <string.h>
#include "c_dba_postgres.h"
#include "yuno_dba_postgres.h"

/****************************************************************
 *          Constants
 ****************************************************************/

/****************************************************************
 *          Data
 ****************************************************************/

/***************************************************************************
 *      Framework Method create
 ***************************************************************************/
PRIVATE void mt_create(hgobj gobj)
{
    GCLASS *gclass =  gobj_gclass(gobj);

    gclass->base->gmt.mt_create(gobj);
}

/***************************************************************************
 *      Framework Method start
 ***************************************************************************/
PRIVATE int mt_start(hgobj gobj)
{
    GCLASS *gclass =  gobj_gclass(gobj);

    int ret = gclass->base->gmt.mt_start(gobj);

    /*
     *  HACK Start here the services or gobjs with no autostart
     */

    return ret;
}

/***************************************************************************
 *      Framework Method stop
 ***************************************************************************/
PRIVATE int mt_stop(hgobj gobj)
{
    GCLASS *gclass =  gobj_gclass(gobj);

    gobj_stop_services();

    return gclass->base->gmt.mt_stop(gobj);
}

/***************************************************************************
 *      Framework Method play
 ***************************************************************************/
PRIVATE int mt_play(hgobj gobj)
{
    /*
     *  This play order can became from yuneta_agent or autoplay config option
     *
     *  Organize the gobj's play as you want.
     */
    return gobj_play(gobj_default_service());
}

/***************************************************************************
 *      Framework Method pause
 ***************************************************************************/
PRIVATE int mt_pause(hgobj gobj)
{
    /*
     *  This pause order can became from yuneta_agent or from yuno stop
     *
     *  Organize the gobj's pause as you want.
     */
    return gobj_pause(gobj_default_service());
}

/***************************************************************************
 *          Register
 ***************************************************************************/
PUBLIC void register_yuno_dba_postgres(void)
{
    GCLASS *gclass;

    /*
     *  Subclass default yuno
     */
    gclass = gobj_subclass_gclass(gclass_default_yuno(), GCLASS_YUNO_DBA_POSTGRES_NAME);

    /*
     *  HACK Override your methods
     *  (the only one inheritance type in gclass)
     */
    gclass->gmt.mt_create = mt_create;
    gclass->gmt.mt_start = mt_start;
    gclass->gmt.mt_stop = mt_stop;
    gclass->gmt.mt_play = mt_play;
    gclass->gmt.mt_pause = mt_pause;

    gobj_register_yuno(ROLE_DBA_POSTGRES, gclass, TRUE);
}
