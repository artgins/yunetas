#!/usr/bin/nodejs
 
var utils = require('../src/utils');
var SMachine = require('../src/smachine').SMachine;

function ac_set_timer(fsm, event, kw, src) {
    SMachine.set_timeout(fsm, 1*1000);
    SMachine.change_state(fsm, 'ST_WAIT');
    console.log("ac_set_timer");
    return 1;
}
function ac_do_something(fsm, event, kw, src) {
    //alert("TIME!!!");
    console.log("TIME!!!!! smachine");
    return 1;
}
var TEST_FSM = {
    'name': 'Test-machine',
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

SMachine.set_machine_trace(true);
var fsm = SMachine.create(TEST_FSM);
fsm.name = 'testo';
fsm.gclass_name = 'Testo';
//console.log("inside: " + SMachine._inside);
//console.log("fsm: %j", fsm);

SMachine.inject_event(fsm, 'EV_SET_TIMER', [1,2,3]);

/*
var cur_state = SMachine.current_state(fsm);
console.log("cur_state: " + cur_state);
SMachine.set_timeout(fsm, 1*1000);
SMachine.change_state(fsm, "ST_WAIT");
cur_state = SMachine.current_state(fsm);
console.log("cur_state: " + cur_state);
var list = SMachine.get_input_event_list(fsm);
console.log("event_list: " + list);
var list = SMachine.get_output_event_list(fsm);
console.log("output_event_list: " + list);
*/
