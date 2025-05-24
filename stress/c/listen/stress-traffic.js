#!/usr/bin/env node
/* jshint -W083 */

// TODO WARNING remember exec "ulimit -n 200000"

const net = require('net');
const fs = require('fs');
const { argv } = require('process');

const args = require('minimist')(argv.slice(2), {
    string: ['host', 'message', 'msgfile'],
    integer: ['port', 'connections', 'rate', 'count'],
    alias: {
        h: 'host',
        p: 'port',
        c: 'connections',
        r: 'rate',
        m: 'message',
        f: 'msgfile',
        n: 'count',
        help: 'help'
    },
    default: {
        host: '127.0.0.1',
        port: 7779,
        connections: 100,
        rate: 1,
        count: 0,  // 0 = unlimited
    },
    boolean: ['help']
});

if(args.help) {
    console.log(`
Usage: stress.js [options]

Options:
  -h, --host         Target host (default: 127.0.0.1)
  -p, --port         Target port (default: 7779)
  -c, --connections  Number of concurrent connections (default: 100)
  -r, --rate         Messages per second per connection (can be < 1) (default: 1)
  -n, --count        Max messages per connection (0 = unlimited) (default: 0)
  -m, --message      JSON message string to send
  -f, --msgfile      Path to JSON file to load message from
      --help         Show this help message

Each message is sent with a 4-byte binary length header (network byte order),
where the length includes the header itself.
`);
    process.exit(0);
}

// Load or default the base message
let baseMessage;

if(args.msgfile) {
    try {
        const content = fs.readFileSync(args.msgfile, 'utf8');
        baseMessage = JSON.parse(content);
    } catch(err) {
        console.error(`Error loading JSON from file: ${err.message}`);
        process.exit(1);
    }
} else if(args.message) {
    try {
        baseMessage = JSON.parse(args.message);
    } catch(err) {
        console.error('Error: Invalid JSON in --message');
        process.exit(1);
    }
} else {
    baseMessage = {
        id: "DVES_000000",
        event: "trace",
        device_type: "tele/tasmota_XXXXXX/STATE",
        tm: Math.floor(Date.now() / 1000),
        version: "1.0.0.0",
        temperature: 27.8,
        power_on: "ON",
        voltage: 230.0,
        total: 8.29,
        yesterday: 0.041,
        today: 0.01,
        period: 0.0,
        power: 2.0,
        apparentPower: 8.0,
        reactivePower: 8.0,
        factor: 0.21,
        current: 0.035,
        yuno: "C_GATE_ENCHUFE^gate_enchufe"
    };
}

const start = process.hrtime.bigint();

let max_connections = args.connections;
let cur_connections = 0;

// Launch N connections
for(let i = 0; i < args.connections; i++) {
    const client = new net.Socket();

    client.connect(args.port, args.host, () => {
        const start2 = process.hrtime.bigint();
        cur_connections++;
        const peer = `${client.remoteAddress}:${client.remotePort}`;
        const local = `${client.localAddress}:${client.localPort}`;

        if(cur_connections >= max_connections) {
            let end =  process.hrtime.bigint();
            let total = Number(end - start)/1000000000;
            console.log(`Connected ${i}: peer=${peer}, local=${local}, Total Duration: ${total} sec, ops ${max_connections/total}`);
        } else {
            let end2 =  process.hrtime.bigint();
            let total = Number(end2 - start2)/1000000000;
            console.log(`Connected ${i}: peer=${peer}, local=${local}, Duration: ${total} sec, ops ${1/total}`);
        }

        const thisMsg = JSON.parse(JSON.stringify(baseMessage));
        const hexId = i.toString(16).padStart(6, '0').toUpperCase();
        thisMsg.id = `DVES_${hexId}`;
        thisMsg.device_type = `tele/tasmota_${hexId}/STATE`;

        const intervalMs = 1000 / Math.max(args.rate, 0.000001);
        let messageCount = 0;

        const interval = setInterval(() => {
            if(args.count > 0 && messageCount >= args.count) {
                clearInterval(interval);
                client.end();
                return;
            }

            thisMsg.tm = Math.floor(Date.now() / 1000);

            const jsonPayload = Buffer.from(JSON.stringify(thisMsg));
            const totalLength = jsonPayload.length + 4;
            const buffer = Buffer.alloc(totalLength);
            buffer.writeUInt32BE(totalLength, 0); // 4-byte header in network byte order
            jsonPayload.copy(buffer, 4);
            client.write(buffer);

            messageCount++;
        }, intervalMs);
    });

    client.on('error', (err) => {
        console.error(`Connection ${i} error: ${err.message}`);
    });
}
