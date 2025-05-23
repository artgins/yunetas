#!/usr/bin/env node
/* jshint -W083 */

const net = require('net');
const { argv } = require('process');

const args = require('minimist')(argv.slice(2), {
    string: ['host'],
    integer: ['port', 'connections', 'jobs', 'disconnect'],
    alias: {
        h: 'host',
        p: 'port',
        c: 'connections',
        j: 'jobs',
        d: 'disconnect',
        help: 'help'
    },
    default: {
        host: '127.0.0.1',
        port: 7779,
        connections: 10,
        jobs: 0,
        disconnect: 60  // seconds before disconnect
    },
    boolean: ['help']
});

if(args.help) {
    console.log(`
Usage: stress.js [options]

Options:
  -h, --host         Target host (default: 127.0.0.1)
  -p, --port         Target port (default: 7779)
  -c, --connections  Number of concurrent connections (default: 10)
  -j, --job        Load the connection tube with a job, a loop of console.log("Nothing" 'job' times) (default: 0)
  -d, --disconnect   Seconds before closing each connection (default: 60, 0=no-disconnect)
      --help         Show this help message

Each connection logs its local and remote address when connected,
and closes automatically after the timeout.
`);
    process.exit(0);
}

// Launch N connections
let jobs = args.jobs;

const start = process.hrtime.bigint();

let max_connections = args.connections;
let cur_connections = 0;

for(let i = 0; i < args.connections; i++) {
    ((i) => {
        const client = new net.Socket();

        client.connect(args.port, args.host, () => {
            const start2 = process.hrtime.bigint();
            cur_connections++;
            const peer = `${client.remoteAddress}:${client.remotePort}`;
            const local = `${client.localAddress}:${client.localPort}`;
            if(jobs) {
                for(let j = 0; j<jobs; j++) {
                    if(j < -1) {
                        console.log("Nothing\r");
                    }
                }
            }

            if(cur_connections >= max_connections) {
                let end =  process.hrtime.bigint();
                let total = Number(end - start)/1000000000;
                console.log(`Connected ${i}: peer=${peer}, local=${local}, Jobs ${jobs}, Total Duration: ${total} sec, ops ${max_connections/total}`);
            } else {
                let end2 =  process.hrtime.bigint();
                let total = Number(end2 - start2)/1000000000;
                console.log(`Connected ${i}: peer=${peer}, local=${local}, Jobs ${jobs}, Duration: ${total} sec, ops ${1/total}`);
            }
            if(args.disconnect) {
                setTimeout(() => {
                    console.log(`Close Connection ${i}`);
                    client.end();
                }, args.disconnect * 1000);
            }
        });

        client.on('error', (err) => {
            console.error(`Connection ${i} error: ${err.message}`);
        });

        client.on('close', () => {
            console.log(`Connection ${i} closed`);
        });
    })(i);
}
