#!/usr/bin/env node
/* jshint -W083 */

const net = require('net');
const { argv } = require('process');

const args = require('minimist')(argv.slice(2), {
    string: ['host'],
    integer: ['port', 'connections', 'disconnect'],
    alias: {
        h: 'host',
        p: 'port',
        c: 'connections',
        d: 'disconnect',
        help: 'help'
    },
    default: {
        host: '127.0.0.1',
        port: 7778,
        connections: 10,
        disconnect: 60  // seconds before disconnect
    },
    boolean: ['help']
});

if(args.help) {
    console.log(`
Usage: stress.js [options]

Options:
  -h, --host         Target host (default: 127.0.0.1)
  -p, --port         Target port (default: 2002)
  -c, --connections  Number of concurrent connections (default: 10)
  -d, --disconnect   Seconds before closing each connection (default: 60)
      --help         Show this help message

Each connection logs its local and remote address when connected,
and closes automatically after the timeout.
`);
    process.exit(0);
}

function createConnection(i) {
    const client = new net.Socket();

    client.connect(args.port, args.host, () => {
        const peer = `${client.remoteAddress}:${client.remotePort}`;
        const local = `${client.localAddress}:${client.localPort}`;
        console.log(`Connected ${i}: peer=${peer}, local=${local}`);

        setTimeout(() => {
            console.log(`Close Connection ${i}`);
            client.end();
        }, args.disconnect * 1000);
    });

    client.on('error', (err) => {
        console.error(`Connection ${i} error: ${err.message}`);
    });

    client.on('close', () => {
        console.log(`Connection ${i} closed`);
    });
}

// Throttle with interval (e.g., 1ms per connection)
let i = 0;
const throttleIntervalMs = 100;

const throttler = setInterval(() => {
    if(i >= args.connections) {
        clearInterval(throttler);
        return;
    }
    createConnection(i++);
}, throttleIntervalMs);
