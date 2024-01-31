#!/usr/bin/nodejs

var utils = require('../src/utils');
var SMachine = require('../src/smachine').SMachine;
var GObj = require('../src/gobj').GObj;
var Router = require('../src/router').Router;
var Yuno = require('../src/yuno').Yuno;

var testing = true;
if(testing) {

    function ac_set_timer(gobj, event, kw, src) {
        gobj.set_timeout(1*1000);
        gobj.change_state('ST_WAIT');
        console.log("ac_set_timer");
        return 1;
    }

    function ac_do_something(gobj, event, kw, src) {
        //alert("TIME!!!");
        console.log("TIME!!!!!, gobj: " + gobj.name);
        return 1;
    }
    var TEST_FSM = {
        'event_list': [
            'EV_SET_TIMER: top input',
            'EV_TIMEOUT: top output',
        ],
        'state_list': ['ST_IDLE', 'ST_WAIT'],
        'machine': {
            'ST_IDLE':
            [
                ['EV_SET_TIMER', ac_set_timer, undefined],
            ],
            'ST_WAIT':
            [
                ['EV_TIMEOUT', ac_do_something, 'ST_IDLE'],
            ]
        }
    };

    var TEST_CONFIG = {kaka:1, vaca:2};

    var Test_GObj = GObj.__makeSubclass__();
    Test_GObj.prototype.__init__= function(name, parent, kw, yuno) {
        this.name = name || '';  // set before super(), to put the same smachine name
        GObj.prototype.__init__.call(this, TEST_FSM, TEST_CONFIG);
        //this.yuno = yuno;
        //this.parent = parent;
        __update_dict__(this.config, kw || {});
        //**********************************
        //  mt_create
        //**********************************
        return this;
    };

    var yuno = new Yuno('yuno', ['xx'], {});

    var obj = yuno.gobj_create('xxx', Test_GObj, null, null);
    obj.set_machine_trace(true);
    console.log("obj: instanceof GObj: " + (obj instanceof GObj));
    console.log("obj: instanceof Test_GObj: " + (obj instanceof Test_GObj));

    obj.send_event(obj, 'EV_SET_TIMER', {pepe:1, juan:2});
}

var testing2 = true;
if(testing2) {

    //########################################################
    //#       Child
    //########################################################

    function ac_query_and_direct_response(self, event, kw, src) {
        var data = kw.data;
        if (data === 'query1') {
            return self.send_event(self.parent, 'EV_RESP_OK', {response:'OK'});
        } else {
            return self.send_event(self.parent, 'EV_RESP_ERROR', {response:'ERROR'});
        }
    }

    function ac_query_and_broadcast_response(self, event, kw, src) {
        data = kw.data;
        if (data === 'query3') {
            self.publish_event('EV_RESP_OK', {response:'OK'});
        } else {
            self.publish_event('EV_RESP_ERROR', {response:'ERROR'});
        }
        event.event_name = 'EV_RESP_OK';
        self.send_event(self.parent, event, kw);
    }

    CHILD_FSM = {
        'event_list': [
            'EV_QUERY_BY_DIRECT',
            'EV_QUERY_BY_BROADCAST',
            'EV_RESP_OK: output',
            'EV_RESP_ERROR: output'
        ],
        'state_list': ['ST_IDLE'],
        'machine': {
            'ST_IDLE':
            [
                ['EV_QUERY_BY_DIRECT',    ac_query_and_direct_response,    undefined],
                ['EV_QUERY_BY_BROADCAST', ac_query_and_broadcast_response, undefined]
            ]
        }
    }

    var ChildGClass = GObj.__makeSubclass__();
    ChildGClass.prototype.__init__= function(name, parent, kw, yuno) {
        this.name = name || '';  // set before super(), to put the same smachine name
        GObj.prototype.__init__.call(this, CHILD_FSM, {});
        __update_dict__(this.config, kw || {});
        return this;
    };


    //########################################################
    //#       Parent
    //########################################################

    function ac_response_ok(self, event, kw, src) {
        self.response = kw.response;
        return 'Done';
    }

    function ac_response_error(self, event, kw, src) {
        self.response = kw.response;
        return 'Done';
    }

    function ac_consult(self, event, kw, src) {
        return self.send_event(self.cons, event, kw);
    }

    PARENT_FSM = {
        'event_list': [
            'EV_QUERY_BY_DIRECT',
            'EV_QUERY_BY_BROADCAST',
            'EV_RESP_OK',
            'EV_RESP_ERROR'
        ],
        'state_list': [
            'ST_IDLE',
            'ST_WAIT_RESP'
        ],
        'machine': {
            'ST_IDLE':
            [
                ['EV_QUERY_BY_DIRECT',      ac_consult,     'ST_WAIT_RESP'],
                ['EV_QUERY_BY_BROADCAST',   ac_consult,     'ST_WAIT_RESP']
            ],

            'ST_WAIT_RESP':
            [
                ['EV_RESP_OK',    ac_response_ok,           'ST_IDLE'],
                ['EV_RESP_ERROR', ac_response_error,        'ST_IDLE']
            ]
        }
    }

    var ParentGClass = GObj.__makeSubclass__();
    ParentGClass.prototype.__init__= function(name, parent, kw, yuno) {
        this.name = name || '';  // set before super(), to put the same smachine name
        GObj.prototype.__init__.call(this, PARENT_FSM, {});
        __update_dict__(this.config, kw || {});
        return this;
    };
    ParentGClass.prototype.mt_create= function() {
        //**********************************
        //  mt_create
        //**********************************
        var self = this;
        self.cons = self.yuno.gobj_create('child', ChildGClass, {}, self);
    }

    //########################################################
    //#       Check Parent and child
    //########################################################

    var is_chrome = false;
    if (typeof GLOBAL === 'undefined') {
        is_chrome = navigator.userAgent.toLowerCase().indexOf('chrome') > -1;
    }
    function assertEqual(var1, var2) {
        if (var1 === var2) {

        } else {
            var msg = "=====> ERROR TESTING assert: '" + var1 +
                "' NOT equal to '" + var2 + "'";
            if (is_chrome) {
                console.log(msg);
            } else {
                throw msg;
            }
        }
    }

    var yuno = new Yuno('yuno', ['xxx'], {});
    var gobj_parent = yuno.gobj_create('parent', ParentGClass);
    gobj_parent.set_machine_trace(true);
    gobj_parent.cons.set_machine_trace(true);

    // test_send_event_to_itself
    var ret = gobj_parent.send_event(
        gobj_parent,
        'EV_QUERY_BY_DIRECT',
        {data:'query1'}
    );
    assertEqual(ret, 'Done');
    assertEqual(gobj_parent.response, 'OK');

    ret = gobj_parent.send_event(
        gobj_parent,
        'EV_QUERY_BY_DIRECT',
        {data:'query2'});
    assertEqual(ret, 'Done');
    assertEqual(gobj_parent.response, 'ERROR');

    // test_send_event_name_to_child
    ret = gobj_parent.send_event(
        gobj_parent.cons,
        'EV_QUERY_BY_DIRECT',
        {data:'query1'}
    );
    assertEqual(ret, -1);

    ret = gobj_parent.send_event(
        gobj_parent.cons,
        'EV_QUERY_BY_DIRECT',
        {data:'query2'}
    );
    assertEqual(ret, -1);

    // test_send_event_name_to_none
    try {
        ret = gobj_parent.send_event(
            null,
            'EV_QUERY_BY_DIRECT',
            {data:'query2'}
        );
    } catch (e) {
        assertEqual(e, "ERROR send_event('EV_QUERY_BY_DIRECT') NO DESTINATION.");
    }

    // test_publish_event1
    gobj_parent.cons.subscribe_event(
        'EV_RESP_OK',
        {},
        gobj_parent);

    var ln = gobj_parent.cons._dl_subscriptions.length;
    assertEqual(ln, 1);

    gobj_parent.cons.subscribe_event(
        'EV_RESP_OK', {},
        gobj_parent);
    ln = gobj_parent.cons._dl_subscriptions.length;
    assertEqual(ln, 1);

    return;
    //# AKI se enbucla
    gobj_parent.send_event(
        gobj_parent,
        'EV_QUERY_BY_BROADCAST',
        {data:'query3'}
    );
    assertEqual(gobj_parent.response, 'OK');

    gobj_parent.cons.delete_subscription(
        'EV_RESP_OK',
        gobj_parent
    );
    ln = gobj_parent.cons._dl_subscriptions.length;
    assertEqual(ln, 0);

    // test_publish_event2
    gobj_parent.cons.subscribe_event(
        ['EV_RESP_OK','EV_RESP_ERROR'], {},
        gobj_parent
    );
    gobj_parent.send_event(gobj_parent,
        'EV_QUERY_BY_BROADCAST',
        data='query4'
    );
    assertEqual(gobj_parent.response, 'ERROR')
    gobj_parent.cons.delete_subscription(
        ['EV_RESP_OK','EV_RESP_ERROR'],
        gobj_parent
    );

    ln = gobj_parent.cons._dl_subscriptions.length;
    assertEqual(ln, 0);

    // test_publish_event3
    gobj_parent.cons.subscribe_event(undefined, {}, gobj_parent);
    gobj_parent.send_event(
        gobj_parent,
        'EV_QUERY_BY_BROADCAST',
        {data:'query4'}
    );
    assertEqual(gobj_parent.response, 'ERROR')
    gobj_parent.cons.delete_subscription(undefined, gobj_parent);

    // test_publish_event4(self)
    gobj_parent.cons.subscribe_event(
        [null], {},
        gobj_parent
    );
    gobj_parent.send_event(
        gobj_parent,
        'EV_QUERY_BY_BROADCAST',
        {data:'query4'}
    );
    assertEqual(gobj_parent.response, 'ERROR');
    ln = gobj_parent.cons._dl_subscriptions.length;
    assertEqual(ln, 1);
    gobj_parent.cons.delete_subscription([null], gobj_parent);
    ln = gobj_parent.cons._dl_subscriptions.length;
    assertEqual(ln, 0);

    // test_subscribe_event_and_delete_subscription
    try {
        gobj_parent.cons.subscribe_event('EV_RESP_OK', {}, 1);
    } catch (e) {
        assertEqual(e, "ERROR GObj.subscribe_event(): BAD TYPE subscriber_gobj");
    }
    try {
        gobj_parent.cons.subscribe_event('EV_XXX', {}, gobj_parent);
    } catch (e) {
        assertEqual(e, "ERROR GObj.subscribe_event(): EV_XXX not in output-event list");
    }

    gobj_parent.cons.subscribe_event('EV_RESP_OK', {}, gobj_parent);
    gobj_parent.cons.subscribe_event(['EV_RESP_OK', 'EV_RESP_ERROR'], {}, gobj_parent);
    ln = gobj_parent.cons._dl_subscriptions.length;
    assertEqual(ln, 2);

    gobj_parent.cons.delete_subscription('EV_RESP_OK', gobj_parent);
    ln = gobj_parent.cons._dl_subscriptions.length;
    assertEqual(ln, 1);

    gobj_parent.cons.delete_subscription(['EV_RESP_ERROR', 'EV_RESP_OK'], gobj_parent);
    ln = gobj_parent.cons._dl_subscriptions.length;
    assertEqual(ln, 0);

}
