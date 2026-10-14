/***********************************************************************
 *          C_TEST_DISPATCH.C
 *
 *          Test of what the kernel does when a gobj is destroyed while an
 *          event is being dispatched to it, or published by it.
 *
 *          gobj_send_event() and gobj_publish_event() look at their gobj
 *          again when the action, or a subscriber, returns: to print the
 *          return trace, and to stop the publication when the publisher is
 *          gone. Up to 7.26.9 they looked at the gobj's MEMORY for that,
 *          which is freed memory by then. They look at a frame of their
 *          own now, that gobj_destroy() marks.
 *
 *          Phase 1 -- a subscriber destroys the PARENT of the gobj that is
 *          publishing. That is the rule broken: the publisher's tree goes
 *          from under its own publication. It is an ERROR, with a stack,
 *          the destroy still happens, the publication stops, and nothing
 *          reads the freed publisher (run it under ASan to see the last).
 *
 *          Phase 2 -- what is NOT an error, with the machine trace on (the
 *          return trace of gobj_send_event() is what read the freed gobj):
 *            - a gobj that destroys itself in its own action;
 *            - a gobj that publishes its last event, then destroys itself;
 *            - a gobj destroyed by its host in its EV_STOPPED, the stop
 *              contract.
 *
 *          Phase 3 -- a subscriber of EV_STATE_CHANGED destroys the gobj
 *          whose FSM is moving it to the state of an event, before that
 *          event's action. The destroy is the ERROR of phase 1, and the
 *          action is not run on the destroyed gobj (an ERROR too): up to
 *          7.26.9 it ran, on freed memory.
 *
 *          Phase 4 -- a gobj that ends itself in its own mt_start, inside
 *          gobj_start_tree(), in its own mt_stop, inside gobj_stop_tree(),
 *          and in its own mt_play, which returns 0. The walks do not go
 *          down into the children of the gobj that is gone, and the play
 *          does not write into it (up to 7.26.9 both read or wrote freed
 *          memory) and fails: nothing is playing. No ERROR: each one is the
 *          gobj's own code. And a gobj played without a start, whose start
 *          is refused (a required attribute is empty): the play fails too,
 *          the gobj is not left playing (up to 7.26.9 it was, not running).
 *
 *          Phase 5 -- a gobj destroyed by an inner dispatch while an outer
 *          one is still running on it. Both are an ERROR, and the outer code
 *          is not read by the kernel:
 *            - a publisher that destroys ITSELF, in an action a subscriber
 *              sent it from inside its publication (its tree is publishing);
 *            - a gobj whose action sends to a peer, which sends it back an
 *              event whose action destroys it (one of its actions is still
 *              running) -- C_TASK's end inside a job's action, up to 7.26.9.
 *
 *          Phase 6 -- a RUNNING gobj destroyed, whose mt_stop publishes
 *          EV_STOPPED at once (a C_TCP with nothing in flight), and whose
 *          host destroys it there (the stop contract). gobj_destroy() stops
 *          it first, and the host's destroy, inside that stop, is the one
 *          that frees it: up to 7.26.9 the outer gobj_destroy() went on with
 *          the freed gobj (run it under ASan). The ERROR is the one that
 *          destroying a running gobj always gave.
 *
 *          Phase 7 -- the walks of the tree, when the callback destroys more
 *          than the gobj it was given:
 *            - stopping a child destroys its NEXT sibling, in a walk of
 *              gobj_stop_tree() and in one of gobj_stop_children();
 *            - stopping a child destroys the ROOT of the walk (a host that
 *              ends itself in the EV_STOPPED of its transport);
 *            - destroying a running child, inside gobj_destroy() of its
 *              parent, destroys its next sibling.
 *          Up to 7.26.9 each walk took its next gobj before the callback and
 *          went on with it, freed (run it under ASan). No ERROR: each
 *          destroy is the stop of the gobj destroyed, or its own code,
 *          except the running child destroyed with its parent.
 *
 *          Phase 8 -- what the farewell excuses. A gobj publishes EV_STOPPED,
 *          and its host, instead of destroying it there, sends it an event
 *          whose action ends in its destroy, further in. That action began
 *          INSIDE the farewell and does not know it: an ERROR (one of its
 *          actions is still running): the farewell excuses only the code
 *          it runs inside of.
 *
 *          Phase 9 -- a gobj whose own action walks its parent's children,
 *          and the walk destroys it: the walk's callback, a
 *          gobj_destroy_children() and a gobj_destroy_named_children(). No
 *          ERROR: the action is the innermost code that destroys, the
 *          watch frames of the walk above it run no code of a gclass.
 *
 *          Phase 10 -- a walk whose child destroys itself AND the next one,
 *          in gobj_start_tree(), gobj_start_children(), gobj_stop_tree() and
 *          gobj_stop_children(): the walk keeps its place, and the third
 *          child is started or stopped all the same. No ERROR.
 *
 *          Phase 11 -- a host that renews its transport in the transport's
 *          EV_STOPPED (destroys the pair, creates and starts a new one).
 *          gobj_stop_children() and gobj_stop_tree() stop the children they
 *          had and end: the new pair was created after the walk began. The
 *          first fix of 7.26.10 started again from the first child, and
 *          stopped the new pair for ever.
 *
 *          Phase 12 -- the same walks, with a child before the pair whose
 *          start is refused, or that stops itself in its mt_start, or that
 *          starts itself again in its mt_stop: none is started or stopped
 *          twice (the first fix of 7.26.10 started again from the first
 *          child, and did it twice).
 *
 *          Phase 13 -- the walks of the callers, in the six walk types, whose
 *          callback destroys the child AND the next one: each child left is
 *          given once (up to the first fix of 7.26.10 they ended there with
 *          an ERROR). And the time of gobj_stop_children() grows as the
 *          children do, not as their square.
 *
 *          Phase 14 -- gobj_destroy_children() while children are created:
 *            - of a LIVE host that renews its transport in the transport's
 *              EV_STOPPED: the children it had are destroyed, the new pair
 *              lives on (the host lives);
 *            - of a gobj being destroyed, whose running child creates a
 *              sibling in its mt_stop: the create is refused with an ERROR
 *              (a child of a dying parent would be freed with it).
 *          Before, both took the new children too, and never ended.
 *
 *          And in phase 9, gobj_destroy_named_children() without a name
 *          fails with an ERROR.
 *
 *          Copyright (c) 2026, ArtGins.
 *          All Rights Reserved.
 ***********************************************************************/
#include <string.h>

#include "c_test_node.h"
#include "c_test_dispatch.h"

/***************************************************************************
 *              Constants
 ***************************************************************************/

/***************************************************************************
 *              Structures
 ***************************************************************************/

/***************************************************************************
 *              Prototypes
 ***************************************************************************/
PRIVATE int run_phase1(hgobj gobj);
PRIVATE int run_phase2(hgobj gobj);
PRIVATE int run_phase3(hgobj gobj);
PRIVATE int run_phase4(hgobj gobj);
PRIVATE int run_phase5(hgobj gobj);
PRIVATE int run_phase6(hgobj gobj);
PRIVATE int run_phase7(hgobj gobj);
PRIVATE int run_phase8(hgobj gobj);
PRIVATE int run_phase9(hgobj gobj);
PRIVATE int run_phase10(hgobj gobj);
PRIVATE int run_phase11(hgobj gobj);
PRIVATE int run_phase12(hgobj gobj);
PRIVATE int run_phase13(hgobj gobj);
PRIVATE int run_phase14(hgobj gobj);
PRIVATE int cb_count_and_kill(hgobj child, void *user_data, void *user_data2, void *user_data3);
PRIVATE uint64_t time_stop_children(hgobj gobj, int n);
PRIVATE hgobj create_lost_place_trio(hgobj gobj, const char *name, BOOL on_start);
PRIVATE hgobj create_killer_pair(hgobj gobj, const char *name);
PRIVATE int cb_stop_running(hgobj child, void *user_data, void *user_data2, void *user_data3);
PRIVATE int check_gone(hgobj gobj, const char *name, const char *phase);

/***************************************************************************
 *          Data: config, public data, private data
 ***************************************************************************/
/*---------------------------------------------*
 *      Attributes
 *---------------------------------------------*/
PRIVATE sdata_desc_t attrs_table[] = {
/*-ATTR-type------------name----------------flag----------------default-----description--*/
SDATA (DTP_POINTER,     "subscriber",       0,                  0,          "Subscriber of output-events"),
SDATA_END()
};

/*---------------------------------------------*
 *      GClass trace levels
 *---------------------------------------------*/
PRIVATE const trace_level_t s_user_trace_level[16] = {
{0, 0},
};

/*---------------------------------------------*
 *      GClass authz levels
 *---------------------------------------------*/
PRIVATE sdata_desc_t authz_table[] = {
/*-AUTHZ-- type---------name----------------flag----alias---items---description--*/
SDATA_END()
};

/*---------------------------------------------*
 *              Private data
 *---------------------------------------------*/
typedef struct _PRIVATE_DATA {
    int phase;
    int byes;           // EV_TEST_BYE received in phase 2
    int farewells;      // EV_STOPPED received in phase 2
} PRIVATE_DATA;




                    /******************************
                     *      Framework Methods
                     ******************************/




/***************************************************************************
 *      Framework Method create
 ***************************************************************************/
PRIVATE void mt_create(hgobj gobj)
{
    /*
     *  SERVICE subscription model
     */
    hgobj subscriber = (hgobj)gobj_read_pointer_attr(gobj, "subscriber");
    if(subscriber) {
        gobj_subscribe_event(gobj, NULL, NULL, subscriber);
    }
}

/***************************************************************************
 *      Framework Method destroy
 ***************************************************************************/
PRIVATE void mt_destroy(hgobj gobj)
{
}

/***************************************************************************
 *      Framework Method start
 ***************************************************************************/
PRIVATE int mt_start(hgobj gobj)
{
    return 0;
}

/***************************************************************************
 *      Framework Method stop
 ***************************************************************************/
PRIVATE int mt_stop(hgobj gobj)
{
    return 0;
}

/***************************************************************************
 *      Framework Method play
 *
 *  The phases run in an action, as any work of a yuno does: there is a
 *  dispatch in progress for this gobj while they run.
 ***************************************************************************/
PRIVATE int mt_play(hgobj gobj)
{
    gobj_post_event(gobj, EV_TEST_RUN, 0, gobj);
    return 0;
}

/***************************************************************************
 *      Framework Method pause
 ***************************************************************************/
PRIVATE int mt_pause(hgobj gobj)
{
    return 0;
}




                    /***************************
                     *      Local Methods
                     ***************************/




/***************************************************************************
 *  A gobj that must be gone is not among the children any more.
 ***************************************************************************/
PRIVATE int check_gone(hgobj gobj, const char *name, const char *phase)
{
    if(!gobj_child_by_name(gobj, name)) {
        return 0;
    }

    gobj_log_error(gobj, 0,
        "function",     "%s", __FUNCTION__,
        "msgset",       "%s", MSGSET_INTERNAL,
        "msg",          "%s", "A gobj that destroyed itself is still there",
        "phase",        "%s", phase,
        "name",         "%s", name,
        NULL
    );
    return -1;
}

/***************************************************************************
 *  Phase 1: a subscriber destroys the parent of the publisher.
 *
 *      driver
 *        ├── host
 *        │     └── pub     publishes EV_TEST_PING to: driver, witness
 *        └── witness
 *
 *  The driver, first subscriber, destroys `host` (and `pub` with it). The
 *  witness, second subscriber, must not hear the event: its publisher is
 *  gone.
 ***************************************************************************/
PRIVATE int run_phase1(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    priv->phase = 1;

    hgobj host = gobj_create("host", C_TEST_NODE, 0, gobj);
    hgobj pub = gobj_create("pub", C_TEST_NODE,
        json_pack("{s:I}", "subscriber", (json_int_t)(uintptr_t)gobj),
        host
    );
    hgobj witness = gobj_create("witness", C_TEST_NODE, 0, gobj);
    if(!host || !pub || !witness) {
        // Error already logged
        return -1;
    }
    gobj_subscribe_event(pub, EV_TEST_PING, 0, witness);

    gobj_send_event(pub, EV_TEST_FIRE, 0, gobj);

    check_gone(gobj, "host", "phase 1");    // Error already logged

    if(gobj_read_integer_attr(witness, "pings") != 0) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_INTERNAL,
            "msg",          "%s", "A destroyed publisher went on publishing",
            "pings",        "%d", (int)gobj_read_integer_attr(witness, "pings"),
            NULL
        );
    }
    gobj_destroy(witness);

    gobj_log_info(gobj, 0,
        "msgset",       "%s", MSGSET_INFO,
        "msg",          "%s", "phase 1 ok",
        NULL
    );
    return 0;
}

/***************************************************************************
 *  Phase 2: the destroys that are legal. Not one ERROR may come out of
 *  here, and the machine trace is on for every one of them.
 ***************************************************************************/
PRIVATE int run_phase2(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    priv->phase = 2;

    hgclass gclass_node = gclass_find_by_name(C_TEST_NODE);
    gobj_set_gclass_trace(gclass_node, "machine", TRUE);

    /*
     *  Its own action destroys it
     */
    hgobj self = gobj_create("self", C_TEST_NODE, 0, gobj);
    gobj_send_event(self, EV_TEST_DIE, 0, gobj);
    check_gone(gobj, "self", "phase 2");    // Error already logged

    /*
     *  Its last publication, then its own destroy
     */
    hgobj bye = gobj_create("bye", C_TEST_NODE, 0, gobj);
    gobj_send_event(bye, EV_TEST_SAY_BYE, 0, gobj);
    check_gone(gobj, "bye", "phase 2");     // Error already logged

    /*
     *  Its host destroys it in its EV_STOPPED
     */
    hgobj farewell = gobj_create("farewell", C_TEST_NODE, 0, gobj);
    gobj_send_event(farewell, EV_TEST_STOP_ME, 0, gobj);
    check_gone(gobj, "farewell", "phase 2"); // Error already logged

    gobj_set_gclass_trace(gclass_node, "machine", FALSE);

    if(priv->byes != 1 || priv->farewells != 1) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_INTERNAL,
            "msg",          "%s", "A last publication did not arrive",
            "byes",         "%d", priv->byes,
            "farewells",    "%d", priv->farewells,
            NULL
        );
    }

    gobj_log_info(gobj, 0,
        "msgset",       "%s", MSGSET_INFO,
        "msg",          "%s", "phase 2 ok",
        NULL
    );
    return 0;
}

/***************************************************************************
 *  Phase 3: the change of state before an action destroys the gobj.
 ***************************************************************************/
PRIVATE int run_phase3(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    priv->phase = 3;

    /*
     *  Its parent hears all it publishes, EV_STATE_CHANGED included
     */
    hgobj mover = gobj_create("mover", C_TEST_NODE, 0, gobj);
    if(!mover) {
        // Error already logged
        return -1;
    }

    gobj_send_event(mover, EV_TEST_MOVE, 0, gobj);
    check_gone(gobj, "mover", "phase 3");   // Error already logged

    gobj_log_info(gobj, 0,
        "msgset",       "%s", MSGSET_INFO,
        "msg",          "%s", "phase 3 ok",
        NULL
    );
    return 0;
}




/***************************************************************************
 *  Phase 4: gobjs that end themselves in their own framework methods.
 *
 *      tree_start              gobj_start_tree()
 *        ├── starter           destroys itself in its mt_start
 *        │     └── grandchild
 *        └── sibling           must be started all the same
 *
 *      tree_stop               gobj_stop_tree()
 *        ├── stopper           destroys itself in its mt_stop
 *        │     └── grandchild
 *        └── sibling           must be stopped all the same
 *
 *      player                  destroys itself in its mt_play
 *
 *      unstartable             played without a start, which is refused
 ***************************************************************************/
PRIVATE int run_phase4(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    priv->phase = 4;

    hgobj tree_start = gobj_create("tree_start", C_TEST_NODE, 0, gobj);
    hgobj starter = gobj_create("starter", C_TEST_NODE,
        json_pack("{s:b}", "die_on_start", 1),
        tree_start
    );
    gobj_create("grandchild", C_TEST_NODE, 0, starter);
    hgobj sibling = gobj_create("sibling", C_TEST_NODE, 0, tree_start);

    gobj_start_tree(tree_start);
    check_gone(tree_start, "starter", "phase 4");     // Error already logged
    if(!gobj_is_running(sibling)) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_INTERNAL,
            "msg",          "%s", "The walk stopped at the gobj that destroyed itself in its mt_start",
            NULL
        );
    }
    gobj_stop_tree(tree_start);
    gobj_destroy(tree_start);

    hgobj tree_stop = gobj_create("tree_stop", C_TEST_NODE, 0, gobj);
    hgobj stopper = gobj_create("stopper", C_TEST_NODE,
        json_pack("{s:b}", "die_on_stop", 1),
        tree_stop
    );
    gobj_create("grandchild", C_TEST_NODE, 0, stopper);
    sibling = gobj_create("sibling", C_TEST_NODE, 0, tree_stop);

    gobj_start_tree(tree_stop);
    gobj_stop_tree(tree_stop);
    check_gone(tree_stop, "stopper", "phase 4");      // Error already logged
    if(gobj_is_running(sibling)) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_INTERNAL,
            "msg",          "%s", "The walk stopped at the gobj that destroyed itself in its mt_stop",
            NULL
        );
    }
    gobj_destroy(tree_stop);

    hgobj player = gobj_create("player", C_TEST_NODE,
        json_pack("{s:b}", "die_on_play", 1),
        gobj
    );
    gobj_start(player);
    if(gobj_play(player) != -1) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_INTERNAL,
            "msg",          "%s", "A play that destroyed its gobj did not fail",
            NULL
        );
    }
    check_gone(gobj, "player", "phase 4");            // Error already logged

    hgobj unstartable = gobj_create("unstartable", C_TEST_NODE,
        json_pack("{s:s}", "must_have", ""),
        gobj
    );
    if(gobj_play(unstartable) != -1 || gobj_is_playing(unstartable)) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_INTERNAL,
            "msg",          "%s", "A gobj whose start was refused is playing",
            NULL
        );
    }
    gobj_destroy(unstartable);

    gobj_log_info(gobj, 0,
        "msgset",       "%s", MSGSET_INFO,
        "msg",          "%s", "phase 4 ok",
        NULL
    );
    return 0;
}

/***************************************************************************
 *  Phase 5: an inner dispatch destroys a gobj that an outer one is running
 *  on.
 ***************************************************************************/
PRIVATE int run_phase5(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    priv->phase = 5;

    /*
     *  A subscriber asks the publisher to die, and the publisher's own
     *  action destroys it, inside its publication
     */
    hgobj reentrant = gobj_create("reentrant", C_TEST_NODE, 0, gobj);
    gobj_send_event(reentrant, EV_TEST_FIRE, 0, gobj);
    check_gone(gobj, "reentrant", "phase 5"); // Error already logged

    /*
     *  relay ---EV_TEST_BOUNCE--> peer ---EV_TEST_DIE--> relay
     *  (ac_relay still running)
     */
    hgobj peer = gobj_create("peer", C_TEST_NODE, 0, gobj);
    hgobj relay = gobj_create("relay", C_TEST_NODE,
        json_pack("{s:I}", "peer", (json_int_t)(uintptr_t)peer),
        gobj
    );
    gobj_send_event(relay, EV_TEST_RELAY, 0, gobj);
    check_gone(gobj, "relay", "phase 5");   // Error already logged
    gobj_destroy(peer);

    gobj_log_info(gobj, 0,
        "msgset",       "%s", MSGSET_INFO,
        "msg",          "%s", "phase 5 ok",
        NULL
    );
    return 0;
}

/***************************************************************************
 *  Phase 6: a running gobj destroyed, whose stop says farewell at once and
 *  is destroyed by its host in it.
 ***************************************************************************/
PRIVATE int run_phase6(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    priv->phase = 6;

    hgobj transport = gobj_create("transport", C_TEST_NODE,
        json_pack("{s:b}", "farewell_on_stop", 1),
        gobj
    );
    if(!transport) {
        // Error already logged
        return -1;
    }
    gobj_start(transport);

    gobj_destroy(transport);    // ERROR: a RUNNING gobj; its host frees it inside the stop
    check_gone(gobj, "transport", "phase 6");   // Error already logged

    gobj_log_info(gobj, 0,
        "msgset",       "%s", MSGSET_INFO,
        "msg",          "%s", "phase 6 ok",
        NULL
    );
    return 0;
}

/***************************************************************************
 *  A gobj with two children: `killer`, whose mt_stop destroys `victim`.
 ***************************************************************************/
PRIVATE hgobj create_killer_pair(hgobj gobj, const char *name)
{
    hgobj tree = gobj_create(name, C_TEST_NODE, 0, gobj);
    hgobj killer = gobj_create("killer", C_TEST_NODE, 0, tree);
    hgobj victim = gobj_create("victim", C_TEST_NODE, 0, tree);
    if(!tree || !killer || !victim) {
        // Error already logged
        return NULL;
    }
    gobj_write_pointer_attr(killer, "victim", victim);
    return tree;
}

/***************************************************************************
 *  A walk's callback: stop the child.
 ***************************************************************************/
PRIVATE int cb_stop_running(hgobj child, void *user_data, void *user_data2, void *user_data3)
{
    if(gobj_is_running(child)) {
        gobj_stop(child);
    }
    return 0;
}

/***************************************************************************
 *  Phase 7: walks whose callback destroys more than the gobj it was given.
 *
 *      sibling_tree            gobj_stop_tree()
 *        ├── killer            its mt_stop destroys `victim`
 *        └── victim            the next gobj of the walk
 *
 *      sibling_list            gobj_stop_children(), the same pair
 *
 *      root                    destroys itself in the EV_STOPPED of `first`
 *        ├── first             publishes EV_STOPPED in its mt_stop
 *        └── second            the next gobj of the walk
 *
 *      root_by_level           the same, in a walk WALK_BYLEVEL, which
 *                              goes on to the children of the children
 *
 *      parent                  gobj_destroy(): its children are destroyed
 *        ├── killer            RUNNING: stopped first, its mt_stop destroys
 *        └── victim            the next child to destroy
 ***************************************************************************/
PRIVATE int run_phase7(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    priv->phase = 7;

    /*
     *  The next sibling, in a walk of the tree and in one of the children
     */
    hgobj sibling_tree = create_killer_pair(gobj, "sibling_tree");
    if(sibling_tree) {
        gobj_start(sibling_tree);
        gobj_start(gobj_child_by_name(sibling_tree, "killer"));
        gobj_stop_tree(sibling_tree);
        check_gone(sibling_tree, "victim", "phase 7");  // Error already logged
        gobj_destroy(sibling_tree);
    }

    hgobj sibling_list = create_killer_pair(gobj, "sibling_list");
    if(sibling_list) {
        gobj_start(gobj_child_by_name(sibling_list, "killer"));
        gobj_stop_children(sibling_list);
        check_gone(sibling_list, "victim", "phase 7");  // Error already logged
        gobj_destroy(sibling_list);
    }

    /*
     *  The root of the walk
     */
    hgobj root = gobj_create("root", C_TEST_NODE,
        json_pack("{s:b}", "die_on_child_stopped", 1),
        gobj
    );
    hgobj first = gobj_create("first", C_TEST_NODE,
        json_pack("{s:b}", "farewell_on_stop", 1),
        root
    );
    hgobj second = gobj_create("second", C_TEST_NODE, 0, root);
    if(root && first && second) {
        gobj_start(root);
        gobj_start(first);
        gobj_stop_tree(root);
        check_gone(gobj, "root", "phase 7");    // Error already logged
    }

    root = gobj_create("root_by_level", C_TEST_NODE,
        json_pack("{s:b}", "die_on_child_stopped", 1),
        gobj
    );
    first = gobj_create("first", C_TEST_NODE,
        json_pack("{s:b}", "farewell_on_stop", 1),
        root
    );
    second = gobj_create("second", C_TEST_NODE, 0, root);
    if(root && first && second) {
        gobj_start(first);
        gobj_walk_gobj_children_tree(root, WALK_BYLEVEL|WALK_FIRST2LAST, cb_stop_running, 0, 0, 0);
        check_gone(gobj, "root_by_level", "phase 7");   // Error already logged
    }

    /*
     *  The next child of a gobj destroyed
     */
    hgobj parent = create_killer_pair(gobj, "parent");
    if(parent) {
        gobj_start(gobj_child_by_name(parent, "killer"));
        gobj_destroy(parent);   // ERROR: a RUNNING child
        check_gone(gobj, "parent", "phase 7");  // Error already logged
    }

    gobj_log_info(gobj, 0,
        "msgset",       "%s", MSGSET_INFO,
        "msg",          "%s", "phase 7 ok",
        NULL
    );
    return 0;
}


/***************************************************************************
 *  Phase 8: an action that begins inside the farewell of its gobj.
 *
 *  late ---EV_STOPPED--> driver ---EV_TEST_RELAY--> late
 *  late ---EV_TEST_BOUNCE--> peer ---EV_TEST_DIE--> late
 *  (ac_relay of late still running)
 ***************************************************************************/
PRIVATE int run_phase8(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    priv->phase = 8;

    hgobj peer = gobj_create("peer", C_TEST_NODE, 0, gobj);
    hgobj late = gobj_create("late", C_TEST_NODE,
        json_pack("{s:I}", "peer", (json_int_t)(uintptr_t)peer),
        gobj
    );
    if(!peer || !late) {
        // Error already logged
        return -1;
    }

    gobj_send_event(late, EV_TEST_STOP_ME, 0, gobj);
    check_gone(gobj, "late", "phase 8");    // Error already logged
    gobj_destroy(peer);

    gobj_log_info(gobj, 0,
        "msgset",       "%s", MSGSET_INFO,
        "msg",          "%s", "phase 8 ok",
        NULL
    );
    return 0;
}

/***************************************************************************
 *  Phase 9: a walk run by a gobj's own action destroys the gobj.
 *
 *      group
 *        ├── c1        its action walks the children of `group`
 *        ├── c2
 *        └── c3
 ***************************************************************************/
PRIVATE int run_phase9(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    priv->phase = 9;

    const char *hows[] = {"walk", "children", "named", 0};
    for(int i=0; hows[i]; i++) {
        hgobj group = gobj_create("group", C_TEST_NODE, 0, gobj);
        hgobj c1 = gobj_create("c1", C_TEST_NODE, 0, group);
        hgobj c2 = gobj_create("c2", C_TEST_NODE, 0, group);
        hgobj c3 = gobj_create("c3", C_TEST_NODE, 0, group);
        if(!group || !c1 || !c2 || !c3) {
            // Error already logged
            return -1;
        }

        gobj_send_event(c1, EV_TEST_WALK_DIE, json_pack("{s:s}", "how", hows[i]), gobj);
        check_gone(group, "c1", "phase 9");     // Error already logged

        int left = gobj_child_size(group);
        int expected = strcmp(hows[i], "children")==0? 0 : 2;
        if(left != expected) {
            gobj_log_error(gobj, 0,
                "function",     "%s", __FUNCTION__,
                "msgset",       "%s", MSGSET_INTERNAL,
                "msg",          "%s", "The walk destroyed what it should not, or not what it should",
                "how",          "%s", hows[i],
                "left",         "%d", left,
                "expected",     "%d", expected,
                NULL
            );
        }
        gobj_destroy(group);
    }

    /*
     *  No name: refused, with an ERROR (up to 7.26.9 a NULL crashed in strcmp)
     */
    hgobj group = gobj_create("group", C_TEST_NODE, 0, gobj);
    gobj_create("c1", C_TEST_NODE, 0, group);
    if(gobj_destroy_named_children(group, NULL) != -1 ||    // ERROR
            gobj_destroy_named_children(group, "") != -1 || // ERROR
            gobj_child_size(group) != 1) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_INTERNAL,
            "msg",          "%s", "gobj_destroy_named_children() took no name",
            NULL
        );
    }
    gobj_destroy(group);

    gobj_log_info(gobj, 0,
        "msgset",       "%s", MSGSET_INFO,
        "msg",          "%s", "phase 9 ok",
        NULL
    );
    return 0;
}

/***************************************************************************
 *  A gobj with three children: `c1`, whose start (on_start) or stop ends
 *  itself after destroying `c2`, and `c3`.
 ***************************************************************************/
PRIVATE hgobj create_lost_place_trio(hgobj gobj, const char *name, BOOL on_start)
{
    hgobj host = gobj_create(name, C_TEST_NODE, 0, gobj);
    hgobj c1 = gobj_create("c1", C_TEST_NODE,
        json_pack("{s:b}", on_start? "die_on_start" : "die_on_stop", 1),
        host
    );
    hgobj c2 = gobj_create("c2", C_TEST_NODE, 0, host);
    hgobj c3 = gobj_create("c3", C_TEST_NODE, 0, host);
    if(!host || !c1 || !c2 || !c3) {
        // Error already logged
        return NULL;
    }
    gobj_write_pointer_attr(c1, "victim", c2);
    return host;
}

/***************************************************************************
 *  Phase 10: the walks of start and stop keep their place.
 *
 *      host
 *        ├── c1        destroys c2, then itself, in its mt_start / mt_stop
 *        ├── c2        the next child of the walk (not running: its
 *                      destroy is not the point)
 *        └── c3        must be started / stopped all the same
 ***************************************************************************/
PRIVATE int run_phase10(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    priv->phase = 10;

    const char *walks[] = {"start_tree", "start_children", "stop_tree", "stop_children", 0};
    for(int i=0; walks[i]; i++) {
        BOOL on_start = strncmp(walks[i], "start", 5)==0;
        hgobj host = create_lost_place_trio(gobj, walks[i], on_start);
        if(!host) {
            // Error already logged
            return -1;
        }
        hgobj c3 = gobj_child_by_name(host, "c3");

        if(strcmp(walks[i], "start_tree")==0) {
            gobj_start_tree(host);
        } else if(strcmp(walks[i], "start_children")==0) {
            gobj_start_children(host);
        } else {
            gobj_start(gobj_child_by_name(host, "c1"));
            gobj_start(c3);
            if(strcmp(walks[i], "stop_tree")==0) {
                gobj_start(host);
                gobj_stop_tree(host);
            } else {
                gobj_stop_children(host);
            }
        }
        check_gone(host, "c1", "phase 10");     // Error already logged
        check_gone(host, "c2", "phase 10");     // Error already logged

        if(gobj_is_running(c3) != on_start) {
            gobj_log_error(gobj, 0,
                "function",     "%s", __FUNCTION__,
                "msgset",       "%s", MSGSET_INTERNAL,
                "msg",          "%s", "A walk left a child unvisited",
                "walk",         "%s", walks[i],
                NULL
            );
        }
        gobj_stop_tree(host);
        gobj_destroy(host);
    }

    gobj_log_info(gobj, 0,
        "msgset",       "%s", MSGSET_INFO,
        "msg",          "%s", "phase 10 ok",
        NULL
    );
    return 0;
}

/***************************************************************************
 *  Phase 11: a host renews its transport when it stops.
 *
 *      host        renew_on_child_stopped
 *        ├── ta    publishes EV_STOPPED in its mt_stop: the host destroys
 *        │         it and `tb`, and creates and starts a new pair
 *        ├── tb    the next child of the walk, destroyed with `ta`
 *        └── c3    must be stopped
 *
 *  The walk stops `c3` and ends: the new pair was created after the walk
 *  began, it is not one of the children it walks.
 ***************************************************************************/
PRIVATE int run_phase11(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    priv->phase = 11;

    const char *walks[] = {"stop_children", "stop_tree", 0};
    for(int i=0; walks[i]; i++) {
        hgobj host = gobj_create(walks[i], C_TEST_NODE,
            json_pack("{s:b}", "renew_on_child_stopped", 1),
            gobj
        );
        hgobj ta = gobj_create("ta", C_TEST_NODE,
            json_pack("{s:b}", "farewell_on_stop", 1),
            host
        );
        hgobj tb = gobj_create("tb", C_TEST_NODE, 0, host);
        hgobj c3 = gobj_create("c3", C_TEST_NODE, 0, host);
        if(!host || !ta || !tb || !c3) {
            // Error already logged
            return -1;
        }
        gobj_start(ta);
        gobj_start(c3);

        if(strcmp(walks[i], "stop_tree")==0) {
            gobj_start(host);
            gobj_stop_tree(host);
        } else {
            gobj_stop_children(host);
        }

        hgobj new_ta = gobj_child_by_name(host, "ta");
        if(gobj_read_integer_attr(host, "renewals") != 1 ||
                gobj_is_running(c3) || !new_ta || !gobj_is_running(new_ta)) {
            gobj_log_error(gobj, 0,
                "function",     "%s", __FUNCTION__,
                "msgset",       "%s", MSGSET_INTERNAL,
                "msg",          "%s", "A walk that stops the children stopped the new ones, or not the old ones",
                "walk",         "%s", walks[i],
                "renewals",     "%d", (int)gobj_read_integer_attr(host, "renewals"),
                "c3_running",   "%d", gobj_is_running(c3),
                NULL
            );
        }

        gobj_write_bool_attr(host, "renew_on_child_stopped", FALSE);
        gobj_stop_tree(host);
        gobj_destroy(host);
    }

    gobj_log_info(gobj, 0,
        "msgset",       "%s", MSGSET_INFO,
        "msg",          "%s", "phase 11 ok",
        NULL
    );
    return 0;
}

/***************************************************************************
 *  Phase 12: after a child destroys itself AND the next one, no child is
 *  started or stopped twice.
 *
 *      start host                  gobj_start_children(), gobj_start_tree()
 *        ├── c0        its start is refused: a required attribute empty
 *        ├── c0b       stops itself in its mt_start
 *        ├── c1        destroys c2, then itself, in its mt_start
 *        ├── c2
 *        └── c3        must be started
 *
 *      stop host                   gobj_stop_children(), gobj_stop_tree()
 *        ├── s0        starts itself again in its mt_stop
 *        ├── c1        destroys c2, then itself, in its mt_stop
 *        ├── c2
 *        └── c3        must be stopped
 *
 *  Up to the first fix of 7.26.10 the walk started again from the first
 *  child: c0's start was refused twice (two ERRORs), c0b and s0 got two
 *  starts and two stops.
 ***************************************************************************/
PRIVATE int run_phase12(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    priv->phase = 12;

    const char *walks[] = {"start_children", "start_tree", "stop_children", "stop_tree", 0};
    for(int i=0; walks[i]; i++) {
        BOOL on_start = strncmp(walks[i], "start", 5)==0;
        hgobj host = gobj_create(walks[i], C_TEST_NODE, 0, gobj);
        hgobj twice;
        if(on_start) {
            gobj_create("c0", C_TEST_NODE, json_pack("{s:s}", "must_have", ""), host);
            twice = gobj_create("c0b", C_TEST_NODE, json_pack("{s:b}", "stop_on_start", 1), host);
        } else {
            twice = gobj_create("s0", C_TEST_NODE, json_pack("{s:b}", "start_on_stop", 1), host);
        }
        hgobj c1 = gobj_create("c1", C_TEST_NODE,
            json_pack("{s:b}", on_start? "die_on_start" : "die_on_stop", 1),
            host
        );
        hgobj c2 = gobj_create("c2", C_TEST_NODE, 0, host);
        hgobj c3 = gobj_create("c3", C_TEST_NODE, 0, host);
        if(!host || !twice || !c1 || !c2 || !c3) {
            // Error already logged
            return -1;
        }
        gobj_write_pointer_attr(c1, "victim", c2);

        if(on_start) {
            if(strcmp(walks[i], "start_tree")==0) {
                gobj_start_tree(host);  // ERROR: c0 cannot start
            } else {
                gobj_start_children(host);  // ERROR: c0 cannot start
            }
        } else {
            gobj_start(twice);
            gobj_start(c1);
            gobj_start(c3);
            if(strcmp(walks[i], "stop_tree")==0) {
                gobj_start(host);
                gobj_stop_tree(host);
            } else {
                gobj_stop_children(host);
            }
        }
        check_gone(host, "c1", "phase 12");     // Error already logged
        check_gone(host, "c2", "phase 12");     // Error already logged

        int starts = (int)gobj_read_integer_attr(twice, "starts");
        int stops = (int)gobj_read_integer_attr(twice, "stops");
        if(starts != (on_start? 1 : 2) || stops != 1 || gobj_is_running(c3) != on_start) {
            gobj_log_error(gobj, 0,
                "function",     "%s", __FUNCTION__,
                "msgset",       "%s", MSGSET_INTERNAL,
                "msg",          "%s", "A walk started or stopped a child twice, or left one out",
                "walk",         "%s", walks[i],
                "starts",       "%d", starts,
                "stops",        "%d", stops,
                "c3_running",   "%d", gobj_is_running(c3),
                NULL
            );
        }
        gobj_stop_tree(host);
        gobj_destroy(host);
    }

    gobj_log_info(gobj, 0,
        "msgset",       "%s", MSGSET_INFO,
        "msg",          "%s", "phase 12 ok",
        NULL
    );
    return 0;
}

/***************************************************************************
 *  A walk's callback: count the child, and when it has a victim destroy
 *  the victim (the next child) and the child itself.
 ***************************************************************************/
PRIVATE int cb_count_and_kill(hgobj child, void *user_data, void *user_data2, void *user_data3)
{
    int *visits = user_data;
    (*visits)++;

    hgobj victim = (hgobj)gobj_read_pointer_attr(child, "victim");
    if(victim) {
        gobj_write_pointer_attr(child, "victim", NULL);
        gobj_destroy(victim);
        gobj_destroy(child);
    }
    return 0;
}

/***************************************************************************
 *  `n` running children, then n/2 pairs whose first child destroys the
 *  second and itself in its mt_stop: the time of gobj_stop_children().
 ***************************************************************************/
PRIVATE uint64_t time_stop_children(hgobj gobj, int n)
{
    hgobj host = gobj_create("timed", C_TEST_NODE, 0, gobj);
    if(!host) {
        // Error already logged
        return 0;
    }
    for(int i=0; i<n; i++) {
        hgobj c = gobj_create("c", C_TEST_NODE, json_pack("{s:b}", "silent", 1), host);
        gobj_start(c);
    }
    for(int i=0; i<n/2; i++) {
        hgobj k = gobj_create("k", C_TEST_NODE,
            json_pack("{s:b, s:b}", "die_on_stop", 1, "silent", 1),
            host
        );
        hgobj v = gobj_create("v", C_TEST_NODE, json_pack("{s:b}", "silent", 1), host);
        gobj_write_pointer_attr(k, "victim", v);
        gobj_start(k);
    }

    uint64_t t0 = time_in_milliseconds_monotonic();
    gobj_stop_children(host);
    uint64_t t = time_in_milliseconds_monotonic() - t0;

    gobj_destroy(host);
    return t;
}

/***************************************************************************
 *  Phase 13: a walk of the callers, whose callback destroys the child AND
 *  the next one, goes on, gives each child once, and costs the same per
 *  child however many there are.
 *
 *      host
 *        ├── s1..s3    plain
 *        ├── k, v      x4: k destroys v and itself
 *        └── z         must be visited
 ***************************************************************************/
PRIVATE int run_phase13(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    priv->phase = 13;

    const walk_type_t types[] = {
        WALK_FIRST2LAST,
        WALK_LAST2FIRST,
        WALK_TOP2BOTTOM,
        WALK_BOTTOM2TOP,
        WALK_BYLEVEL|WALK_FIRST2LAST,
        WALK_BYLEVEL|WALK_LAST2FIRST,
        0
    };
    for(int i=0; types[i]; i++) {
        hgobj host = gobj_create("host", C_TEST_NODE, 0, gobj);
        for(int j=0; j<3; j++) {
            gobj_create("s", C_TEST_NODE, 0, host);
        }
        for(int j=0; j<4; j++) {
            hgobj k = gobj_create("k", C_TEST_NODE, 0, host);
            hgobj v = gobj_create("v", C_TEST_NODE, 0, host);
            /*
             *  Backwards, the victim is the child BEFORE the killer
             */
            if(types[i] & (WALK_LAST2FIRST|WALK_BOTTOM2TOP)) {
                gobj_write_pointer_attr(v, "victim", k);
            } else {
                gobj_write_pointer_attr(k, "victim", v);
            }
        }
        gobj_create("z", C_TEST_NODE, 0, host);

        int visits = 0;
        if(types[i] & (WALK_TOP2BOTTOM|WALK_BOTTOM2TOP|WALK_BYLEVEL)) {
            gobj_walk_gobj_children_tree(host, types[i], cb_count_and_kill, &visits, 0, 0);
        } else {
            gobj_walk_gobj_children(host, types[i], cb_count_and_kill, &visits, 0, 0);
        }
        if(visits != 3 + 4 + 1 || gobj_child_size(host) != 3 + 1) {
            gobj_log_error(gobj, 0,
                "function",     "%s", __FUNCTION__,
                "msgset",       "%s", MSGSET_INTERNAL,
                "msg",          "%s", "A walk gave a child twice, or left one out",
                "walk_type",    "%d", (int)types[i],
                "visits",       "%d", visits,
                "left",         "%d", (int)gobj_child_size(host),
                NULL
            );
        }
        gobj_destroy(host);
    }

    /*
     *  Four times the children, four times the time: allow eight (a busy
     *  machine). The walk that started again from the first child took 16
     */
    uint64_t t_n = time_stop_children(gobj, 2000);
    uint64_t t_4n = time_stop_children(gobj, 8000);
    if(t_4n > 8 * (t_n + 1)) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_INTERNAL,
            "msg",          "%s", "A walk of the children does not take a time linear in the children",
            "ms_n",         "%d", (int)t_n,
            "ms_4n",        "%d", (int)t_4n,
            NULL
        );
    }

    gobj_log_info(gobj, 0,
        "msgset",       "%s", MSGSET_INFO,
        "msg",          "%s", "phase 13 ok",
        "ms_n",         "%d", (int)t_n,
        "ms_4n",        "%d", (int)t_4n,
        NULL
    );
    return 0;
}

/***************************************************************************
 *  Phase 14: gobj_destroy_children() while children are being created.
 *
 *      host        renews `ta`/`tb` in the EV_STOPPED of `ta`, no bound
 *        ├── ta    RUNNING, publishes EV_STOPPED in its mt_stop
 *        ├── tb
 *        └── c3
 *
 *      dying       gobj_destroy()
 *        └── s     RUNNING, creates and starts a sibling in its mt_stop
 ***************************************************************************/
PRIVATE int run_phase14(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    priv->phase = 14;

    hgobj host = gobj_create("host", C_TEST_NODE,
        json_pack("{s:b}", "renew_without_end", 1),
        gobj
    );
    hgobj ta = gobj_create("ta", C_TEST_NODE,
        json_pack("{s:b}", "farewell_on_stop", 1),
        host
    );
    hgobj tb = gobj_create("tb", C_TEST_NODE, 0, host);
    hgobj c3 = gobj_create("c3", C_TEST_NODE, 0, host);
    if(!host || !ta || !tb || !c3) {
        // Error already logged
        return -1;
    }
    gobj_start(ta);

    gobj_destroy_children(host);    // ERROR: `ta` is running

    hgobj new_ta = gobj_child_by_name(host, "ta");
    if(gobj_child_size(host) != 2 || !new_ta || !gobj_is_running(new_ta) ||
            gobj_child_by_name(host, "c3") ||
            gobj_read_integer_attr(host, "renewals") != 1) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_INTERNAL,
            "msg",          "%s", "gobj_destroy_children() of a live gobj took the children created meanwhile",
            "left",         "%d", (int)gobj_child_size(host),
            "renewals",     "%d", (int)gobj_read_integer_attr(host, "renewals"),
            NULL
        );
    }
    gobj_write_bool_attr(host, "renew_without_end", FALSE);
    gobj_stop_tree(host);
    gobj_destroy(host);

    hgobj dying = gobj_create("dying", C_TEST_NODE, 0, gobj);
    hgobj s = gobj_create("s", C_TEST_NODE, json_pack("{s:b}", "spawn_on_stop", 1), dying);
    if(!dying || !s) {
        // Error already logged
        return -1;
    }
    gobj_start(s);
    gobj_destroy(dying);    // ERROR: `s` is running; ERROR: its sibling is refused
    check_gone(gobj, "dying", "phase 14");  // Error already logged

    gobj_log_info(gobj, 0,
        "msgset",       "%s", MSGSET_INFO,
        "msg",          "%s", "phase 14 ok",
        NULL
    );
    return 0;
}




                    /***************************
                     *      Actions
                     ***************************/




/***************************************************************************
 *
 ***************************************************************************/
PRIVATE int ac_run(hgobj gobj, gobj_event_t event, json_t *kw, hgobj src)
{
    KW_DECREF(kw)

    run_phase1(gobj);
    run_phase2(gobj);
    run_phase3(gobj);
    run_phase4(gobj);
    run_phase5(gobj);
    run_phase6(gobj);
    run_phase7(gobj);
    run_phase8(gobj);
    run_phase9(gobj);
    run_phase10(gobj);
    run_phase11(gobj);
    run_phase12(gobj);
    run_phase13(gobj);
    run_phase14(gobj);

    set_yuno_must_die();
    return 0;
}

/***************************************************************************
 *  Phase 1: destroy the publisher's parent, from inside its publication.
 *  Phase 5: ask the publisher to destroy itself, from inside it.
 ***************************************************************************/
PRIVATE int ac_ping(hgobj gobj, gobj_event_t event, json_t *kw, hgobj src)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    KW_DECREF(kw)

    if(priv->phase == 1) {
        gobj_destroy(gobj_parent(src));
    } else {
        gobj_send_event(src, EV_TEST_DIE, 0, gobj);
    }
    return 0;
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE int ac_bye(hgobj gobj, gobj_event_t event, json_t *kw, hgobj src)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    priv->byes++;

    KW_DECREF(kw)
    return 0;
}

/***************************************************************************
 *  Phase 3: destroy the gobj whose state is changing.
 ***************************************************************************/
PRIVATE int ac_state_changed(hgobj gobj, gobj_event_t event, json_t *kw, hgobj src)
{
    KW_DECREF(kw)
    gobj_destroy(src);
    return 0;
}

/***************************************************************************
 *  The host of the stop contract: destroy it in its EV_STOPPED.
 *  Phase 8: send it an event instead, whose action ends in its destroy.
 ***************************************************************************/
PRIVATE int ac_stopped(hgobj gobj, gobj_event_t event, json_t *kw, hgobj src)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    priv->farewells++;

    KW_DECREF(kw)
    if(priv->phase == 8) {
        return gobj_send_event(src, EV_TEST_RELAY, 0, gobj);
    }
    gobj_destroy(src);
    return 0;
}

/***************************************************************************
 *                          FSM
 ***************************************************************************/
/*---------------------------------------------*
 *          Global methods table
 *---------------------------------------------*/
PRIVATE const GMETHODS gmt = {
    .mt_create  = mt_create,
    .mt_destroy = mt_destroy,
    .mt_start   = mt_start,
    .mt_stop    = mt_stop,
    .mt_play    = mt_play,
    .mt_pause   = mt_pause,
};

/*------------------------*
 *      GClass name
 *------------------------*/
GOBJ_DEFINE_GCLASS(C_TEST_DISPATCH);

/*------------------------*
 *      States
 *------------------------*/

/*------------------------*
 *      Events
 *------------------------*/
GOBJ_DEFINE_EVENT(EV_TEST_RUN);

/***************************************************************************
 *          Create the GClass
 ***************************************************************************/
PRIVATE int create_gclass(gclass_name_t gclass_name)
{
    static hgclass __gclass__ = 0;
    if(__gclass__) {
        gobj_log_error(0, 0,
            "function", "%s", __FUNCTION__,
            "msgset",   "%s", MSGSET_INTERNAL,
            "msg",      "%s", "GClass ALREADY created",
            "gclass",   "%s", gclass_name,
            NULL
        );
        return -1;
    }

    /*------------------------*
     *      States
     *------------------------*/
    ev_action_t st_idle[] = {
        {EV_TEST_RUN,               ac_run,                 0},
        {EV_TEST_PING,              ac_ping,                0},
        {EV_TEST_BYE,               ac_bye,                 0},
        {EV_STOPPED,                ac_stopped,             0},
        {EV_STATE_CHANGED,          ac_state_changed,       0},
        {0,0,0}
    };

    states_t states[] = {
        {ST_IDLE,       st_idle},
        {0, 0}
    };

    /*------------------------*
     *      Events
     *------------------------*/
    event_type_t event_types[] = {
        {EV_TEST_RUN,               0},
        {EV_TEST_PING,              0},
        {EV_TEST_BYE,               0},
        {EV_STOPPED,                0},
        {EV_STATE_CHANGED,          0},
        {NULL, 0}
    };

    /*----------------------------------------*
     *          Register GClass
     *----------------------------------------*/
    __gclass__ = gclass_create(
        gclass_name,
        event_types,
        states,
        &gmt,
        0, // local methods
        attrs_table,
        sizeof(PRIVATE_DATA),
        authz_table,
        0, // command_table
        s_user_trace_level,
        0 // gcflags
    );
    if(!__gclass__) {
        // Error already logged
        return -1;
    }

    return 0;
}

/***************************************************************************
 *              Public access
 ***************************************************************************/
PUBLIC int register_c_test_dispatch(void)
{
    return create_gclass(C_TEST_DISPATCH);
}
