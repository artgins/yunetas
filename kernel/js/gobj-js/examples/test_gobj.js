#!/usr/bin/nodejs

var utils = require('../src/utils');
var SMachine = require('../src/smachine').SMachine;
var GObj = require('../src/gobj').GObj;

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
Test_GObj.prototype.__init__= function(name, parent, kw) {
    this.name = name || '';  // set before super(), to put the same smachine name
    GObj.prototype.__init__.call(this, TEST_FSM, TEST_CONFIG);
    this.parent = parent;
    this.kw = kw || {};
};

var obj = new Test_GObj('prueba-gobj', null, null);
obj.set_machine_trace(true);
console.log("obj: instanceof GObj: " + (obj instanceof GObj));
console.log("obj: instanceof Test_GObj: " + (obj instanceof Test_GObj));

obj.send_event(obj, 'EV_SET_TIMER', {pepe:1, juan:2});
