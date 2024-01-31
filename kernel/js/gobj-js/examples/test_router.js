#!/usr/bin/nodejs

/*-----------------------------------------------*
 *  You need yuno matador running!
 *-----------------------------------------------*/

var WebSocket = require('./node_modules/ws');
var utils = require('../src/utils');
var SMachine = require('../src/smachine').SMachine;
var GObj = require('../src/gobj').GObj;
var Router = require('../src/router').Router;
var Yuno = require('../src/yuno').Yuno;

GLOBAL.WebSocket = WebSocket;

(function(exports) {
    'use strict';

    SMachine.set_machine_trace(true);  // global trace
    console.log("STARTING yuno wibterm");
    var kw = {
        trace_creation: true,
        router_limit_pending_output: 64,
        trace_router: true
    };
    var yuno_wibterm = new Yuno('', ['wibterm'], kw);
    yuno_wibterm.send_event(
        yuno_wibterm.router,
        'EV_ADD_STATIC_ROUTE',
        {
            name: '',
            role: 'matadort',
            urls: [
            'ws://localhost:9990'
            ],
        }
    );

    // TBD
    console.log("======> Start RUNNING");
    //yuno_wibterm.ibterm = yuno_wibterm.gobj_create(
    //    'ibterm',
    //    Ibterm,
    //    {
    //        parent_dom_id:'pages-container'
    //    },
    //    yuno_wibterm
    //);


    /************************************************
     *          Expose to the global object
     ************************************************/
    exports.yuno_wibterm = yuno_wibterm;
})(this);
