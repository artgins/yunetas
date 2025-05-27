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
  -j, --jobs         Load the connection tube with a job, a loop of console.log("Nothing") 'job' times (default: 0)
  -d, --disconnect   Seconds before closing each connection (default: 60, 0=no-disconnect)
      --help         Show this help message

Each connection logs its local and remote address when connected,
and closes automatically after the timeout. If -d > 0, connections are reopened.
`);
    process.exit(0);
}

const start = process.hrtime.bigint();
let cur_connections = 0;
const max_connections = args.connections;
const jobs = args.jobs;
const disconnect = args.disconnect;

function connect(i) {
    const client = new net.Socket();

    const start2 = process.hrtime.bigint();

    client.connect(args.port, args.host, () => {
        cur_connections++;
        const peer = `${client.remoteAddress}:${client.remotePort}`;
        const local = `${client.localAddress}:${client.localPort}`;

        if(jobs > 0) {
            for(let j = 0; j < jobs; j++) {
                console.log("Nothing\r");
            }
        }

        if(cur_connections >= max_connections) {
            const end = process.hrtime.bigint();
            const total = Number(end - start) / 1e9;
            console.log(`Connected ${i}: peer=${peer}, local=${local}, Jobs ${jobs}, Total Duration: ${total} sec, ops ${(max_connections / total).toFixed(2)}`);
        } else {
            const end2 = process.hrtime.bigint();
            const total = Number(end2 - start2) / 1e9;
            console.log(`Connected ${i}: peer=${peer}, local=${local}, Jobs ${jobs}, Duration: ${total} sec, ops ${(1 / total).toFixed(2)}`);
        }

        if(disconnect > 0) {
            setTimeout(() => {
                console.log(`Close Connection ${i}`);
                client.end();
            }, disconnect * 1000);
        }
    });

    client.on('error', (err) => {
        console.error(`Connection ${i} error: ${err.message}`);
    });

    client.on('close', () => {
        console.log(`Connection ${i} closed`);
        if(disconnect > 0) {
            setTimeout(() => connect(i), 0);  // immediate restart
        }
    });
}

// Launch all initial connections
for(let i = 0; i < max_connections; i++) {
    connect(i);
}
