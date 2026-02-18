#!/usr/bin/env python3
"""
MQTT Broker Performance Test
Measures message throughput with configurable publishers, subscribers, and QoS.
Uses raw sockets for zero overhead — measures actual broker performance.

Scenarios:
    Single connection throughput:  --count 1000 --qos 0
    Fan-out (1 pub to N subs):    --count 50 --subs 20 --qos 0
    Burst (N pubs to 1 sub):      --count 50 --pubs 10 --qos 0
    Full benchmark (all QoS):     --count 5000  (default: iterates QoS 0,1,2)

Usage:
    python3 mqtt_benchmark.py [--host HOST] [--port PORT] [--count N]
                              [--pubs N] [--subs N] [--qos Q] [--payload N]
"""

import socket
import struct
import threading
import time
import sys
import argparse

# MQTT packet types
CONNECT     = 0x10
CONNACK     = 0x20
PUBLISH     = 0x30
PUBACK      = 0x40
PUBREC      = 0x50
PUBREL      = 0x60
PUBCOMP     = 0x70
SUBSCRIBE   = 0x80
SUBACK      = 0x90
DISCONNECT  = 0xE0


def encode_remaining_length(length):
    out = bytearray()
    while True:
        byte = length % 128
        length //= 128
        if length > 0:
            byte |= 0x80
        out.append(byte)
        if length == 0:
            break
    return bytes(out)


def decode_remaining_length(sock):
    multiplier = 1
    value = 0
    while True:
        b = sock.recv(1)
        if not b:
            return None
        encoded = b[0]
        value += (encoded & 0x7F) * multiplier
        if (encoded & 0x80) == 0:
            break
        multiplier *= 128
    return value


def encode_utf8(s):
    encoded = s.encode('utf-8')
    return struct.pack('!H', len(encoded)) + encoded


def build_connect(client_id, keepalive=60):
    # Variable header: protocol name + level + flags + keepalive
    var_header = encode_utf8('MQTT')           # protocol name
    var_header += struct.pack('!B', 4)          # protocol level (4 = v3.1.1)
    var_header += struct.pack('!B', 0x02)       # flags: clean session
    var_header += struct.pack('!H', keepalive)  # keepalive
    # Payload
    payload = encode_utf8(client_id)
    remaining = var_header + payload
    return bytes([CONNECT]) + encode_remaining_length(len(remaining)) + remaining


def build_subscribe(mid, topic, qos):
    var_header = struct.pack('!H', mid)
    payload = encode_utf8(topic) + struct.pack('!B', qos)
    remaining = var_header + payload
    return bytes([SUBSCRIBE | 0x02]) + encode_remaining_length(len(remaining)) + remaining


def build_publish(topic, payload_data, qos=0, mid=0):
    flags = qos << 1
    var_header = encode_utf8(topic)
    if qos > 0:
        var_header += struct.pack('!H', mid)
    remaining = var_header + payload_data
    return bytes([PUBLISH | flags]) + encode_remaining_length(len(remaining)) + remaining


def build_puback(mid):
    return bytes([PUBACK, 2]) + struct.pack('!H', mid)


def build_pubrec(mid):
    return bytes([PUBREC, 2]) + struct.pack('!H', mid)


def build_pubrel(mid):
    return bytes([PUBREL | 0x02, 2]) + struct.pack('!H', mid)


def build_pubcomp(mid):
    return bytes([PUBCOMP, 2]) + struct.pack('!H', mid)


def build_disconnect():
    return bytes([DISCONNECT, 0])


def recv_packet(sock, timeout=5.0):
    sock.settimeout(timeout)
    try:
        header = sock.recv(1)
        if not header:
            return None, None
        pkt_type = header[0] & 0xF0
        remaining_len = decode_remaining_length(sock)
        if remaining_len is None:
            return None, None
        data = b''
        while len(data) < remaining_len:
            chunk = sock.recv(remaining_len - len(data))
            if not chunk:
                return None, None
            data += chunk
        return pkt_type, data
    except socket.timeout:
        return None, None
    except (ConnectionResetError, BrokenPipeError, OSError):
        return None, None


def mqtt_connect(host, port, client_id):
    sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    sock.setsockopt(socket.IPPROTO_TCP, socket.TCP_NODELAY, 1)
    sock.connect((host, port))
    sock.sendall(build_connect(client_id))
    pkt_type, data = recv_packet(sock)
    if pkt_type != CONNACK:
        raise Exception(f"Expected CONNACK, got {pkt_type}")
    return sock


def run_subscriber(host, port, topic, qos, result, stop_event, idx=0):
    """Subscriber thread: connects, subscribes, counts received messages."""
    try:
        cid = f'bench-sub-{idx}-{qos}-{int(time.time()*1000) % 100000}'
        sock = mqtt_connect(host, port, cid)
        sock.sendall(build_subscribe(1, topic, qos))
        pkt_type, data = recv_packet(sock)
        if pkt_type != SUBACK:
            result['error'] = f"Expected SUBACK, got {pkt_type}"
            return

        result['ready'] = True
        received = 0
        sock.settimeout(0.5)

        while not stop_event.is_set():
            try:
                header = sock.recv(1)
                if not header:
                    break
                pkt_type_byte = header[0]
                pkt_type = pkt_type_byte & 0xF0
                msg_qos = (pkt_type_byte >> 1) & 0x03
                remaining_len = decode_remaining_length(sock)
                if remaining_len is None:
                    break
                data = b''
                while len(data) < remaining_len:
                    chunk = sock.recv(remaining_len - len(data))
                    if not chunk:
                        break
                    data += chunk

                if pkt_type == PUBLISH:
                    received += 1
                    if received == 1:
                        result['first_recv_time'] = time.monotonic()
                    result['last_recv_time'] = time.monotonic()

                    # Handle QoS acknowledgments
                    if msg_qos == 1:
                        topic_len = struct.unpack('!H', data[0:2])[0]
                        mid = struct.unpack('!H', data[2 + topic_len:4 + topic_len])[0]
                        sock.sendall(build_puback(mid))
                    elif msg_qos == 2:
                        topic_len = struct.unpack('!H', data[0:2])[0]
                        mid = struct.unpack('!H', data[2 + topic_len:4 + topic_len])[0]
                        sock.sendall(build_pubrec(mid))
                elif pkt_type == PUBREL:
                    mid = struct.unpack('!H', data[0:2])[0]
                    sock.sendall(build_pubcomp(mid))

            except socket.timeout:
                continue
            except (ConnectionResetError, BrokenPipeError, OSError):
                break

        result['received'] = received
        try:
            sock.sendall(build_disconnect())
            sock.close()
        except Exception:
            pass
    except Exception as e:
        result['error'] = str(e)


def run_publisher(host, port, topic, qos, count, payload_data, result, idx=0):
    """Publisher: connects, publishes count messages, measures time."""
    try:
        cid = f'bench-pub-{idx}-{qos}-{int(time.time()*1000) % 100000}'
        sock = mqtt_connect(host, port, cid)
        sock.settimeout(5.0)

        mid = 0
        t_start = time.monotonic()

        for i in range(count):
            if qos > 0:
                mid = (mid % 65535) + 1

            pkt = build_publish(topic, payload_data, qos, mid)
            sock.sendall(pkt)

            if qos == 1:
                # Wait for PUBACK
                pkt_type, data = recv_packet(sock, timeout=10.0)
                if pkt_type != PUBACK:
                    result['error'] = f"Expected PUBACK at msg {i}, got {pkt_type}"
                    break
            elif qos == 2:
                # Wait for PUBREC
                pkt_type, data = recv_packet(sock, timeout=10.0)
                if pkt_type != PUBREC:
                    result['error'] = f"Expected PUBREC at msg {i}, got {pkt_type}"
                    break
                # Send PUBREL
                sock.sendall(build_pubrel(mid))
                # Wait for PUBCOMP
                pkt_type, data = recv_packet(sock, timeout=10.0)
                if pkt_type != PUBCOMP:
                    result['error'] = f"Expected PUBCOMP at msg {i}, got {pkt_type}"
                    break

        t_end = time.monotonic()
        result['pub_time'] = t_end - t_start
        result['published'] = count if 'error' not in result else i

        sock.sendall(build_disconnect())
        sock.close()
    except Exception as e:
        result['error'] = str(e)


def run_benchmark(host, port, topic_base, qos, count, payload_size,
                  num_pubs=1, num_subs=1):
    """Run a benchmark with configurable publishers and subscribers."""
    payload_data = (b'X' * payload_size)
    topic = f"{topic_base}/q{qos}/{int(time.time())}"
    total_msgs = count * num_pubs  # total messages published

    # Start subscribers
    sub_results = []
    sub_threads = []
    stop_event = threading.Event()

    for i in range(num_subs):
        sub_result = {'received': 0, 'ready': False}
        sub_results.append(sub_result)
        t = threading.Thread(
            target=run_subscriber,
            args=(host, port, topic, qos, sub_result, stop_event, i)
        )
        t.start()
        sub_threads.append(t)

    # Wait for all subscribers to be ready
    deadline = time.monotonic() + 10.0
    while time.monotonic() < deadline:
        if all(r.get('ready') for r in sub_results):
            break
        time.sleep(0.05)

    not_ready = [i for i, r in enumerate(sub_results) if not r.get('ready')]
    if not_ready:
        stop_event.set()
        for t in sub_threads:
            t.join(timeout=3)
        return {'error': f'Subscribers {not_ready} failed to connect',
                'qos': qos, 'count': count, 'num_pubs': num_pubs,
                'num_subs': num_subs}

    time.sleep(0.2)  # brief settle

    # Start publishers
    pub_results = []
    pub_threads = []
    t_wall_start = time.monotonic()

    for i in range(num_pubs):
        pub_result = {}
        pub_results.append(pub_result)
        if num_pubs == 1:
            # Single publisher: run blocking (no thread overhead)
            run_publisher(host, port, topic, qos, count, payload_data,
                          pub_result, i)
        else:
            t = threading.Thread(
                target=run_publisher,
                args=(host, port, topic, qos, count, payload_data,
                      pub_result, i)
            )
            t.start()
            pub_threads.append(t)

    for t in pub_threads:
        t.join(timeout=60)

    t_wall_end = time.monotonic()
    wall_time = t_wall_end - t_wall_start

    # Wait for subscribers to receive remaining messages
    if qos == 0:
        time.sleep(2)
    elif qos == 1:
        time.sleep(3)
    else:
        time.sleep(4)

    stop_event.set()
    for t in sub_threads:
        t.join(timeout=5)

    # Aggregate results
    total_published = sum(r.get('published', 0) for r in pub_results)
    total_received = sum(r.get('received', 0) for r in sub_results)
    total_expected = total_msgs * num_subs  # each sub should get all msgs

    pub_errors = [r.get('error') for r in pub_results if r.get('error')]
    sub_errors = [r.get('error') for r in sub_results if r.get('error')]

    # Compute e2e timing across all subscribers
    first_recv = None
    last_recv = None
    for r in sub_results:
        ft = r.get('first_recv_time')
        lt = r.get('last_recv_time')
        if ft and (first_recv is None or ft < first_recv):
            first_recv = ft
        if lt and (last_recv is None or lt > last_recv):
            last_recv = lt

    return {
        'qos': qos,
        'count': count,
        'num_pubs': num_pubs,
        'num_subs': num_subs,
        'payload_size': payload_size,
        'total_published': total_published,
        'total_expected': total_expected,
        'total_received': total_received,
        'wall_time': wall_time,
        'first_recv': first_recv,
        'last_recv': last_recv,
        'pub_errors': pub_errors,
        'sub_errors': sub_errors,
    }


def main():
    parser = argparse.ArgumentParser(description='MQTT Broker Performance Test')
    parser.add_argument('--host', default='127.0.0.1', help='Broker host')
    parser.add_argument('--port', type=int, default=1810, help='Broker port')
    parser.add_argument('--count', type=int, default=5000,
                        help='Messages per publisher')
    parser.add_argument('--payload', type=int, default=64,
                        help='Payload size in bytes')
    parser.add_argument('--qos', type=int, default=-1,
                        help='Test only this QoS (-1 = all 0,1,2)')
    parser.add_argument('--pubs', type=int, default=1,
                        help='Number of publisher connections')
    parser.add_argument('--subs', type=int, default=1,
                        help='Number of subscriber connections')
    args = parser.parse_args()

    print(f"MQTT Broker Performance Test")
    print(f"  Broker  : {args.host}:{args.port}")
    print(f"  Messages: {args.count} per publisher")
    print(f"  Payload : {args.payload} bytes")
    if args.pubs > 1 or args.subs > 1:
        print(f"  Pubs    : {args.pubs}")
        print(f"  Subs    : {args.subs}")
    print()

    qos_levels = [args.qos] if args.qos >= 0 else [0, 1, 2]
    all_pass = True
    results = []

    for qos in qos_levels:
        topic = f"bench/perf/{int(time.time())}"
        r = run_benchmark(args.host, args.port, topic, qos, args.count,
                          args.payload, num_pubs=args.pubs, num_subs=args.subs)
        results.append(r)

        if r.get('error'):
            print(f"  QoS {qos}: ERROR - {r['error']}")
            all_pass = False
            continue

        if r.get('pub_errors') or r.get('sub_errors'):
            errs = r.get('pub_errors', []) + r.get('sub_errors', [])
            print(f"  QoS {qos}: ERROR - {'; '.join(str(e) for e in errs)}")
            all_pass = False
            continue

        total_received = r['total_received']
        total_expected = r['total_expected']
        wall_ms = r['wall_time'] * 1000
        pub_rate = int(r['total_published'] / r['wall_time']) \
            if r['wall_time'] > 0 else 0
        loss = total_expected - total_received
        loss_pct = (loss * 100 // total_expected) if total_expected > 0 else 0

        # End-to-end rate
        e2e_rate = 0
        if r.get('first_recv') and r.get('last_recv') and total_received > 1:
            e2e_time = r['last_recv'] - r['first_recv']
            e2e_rate = int(total_received / e2e_time) if e2e_time > 0 \
                else total_received

        # Pass/fail thresholds
        if qos == 0:
            passed = total_received >= (total_expected * 80 // 100)
        else:
            passed = total_received >= (total_expected * 95 // 100)

        status = "PASS" if passed else "FAIL"
        if not passed:
            all_pass = False

        print(f"  [{status}] QoS {qos}: "
              f"pub {pub_rate:,} msg/s ({wall_ms:,.0f} ms) | "
              f"recv {total_received:,}/{total_expected:,} "
              f"({loss_pct}% loss) | "
              f"e2e {e2e_rate:,} msg/s")

    print()

    # Machine-readable summary for the shell test
    for r in results:
        qos = r.get('qos', -1)
        if r.get('error'):
            print(f"RESULT:qos={qos}:status=ERROR:error={r['error']}")
            continue

        total_received = r['total_received']
        total_expected = r['total_expected']
        pub_rate = int(r['total_published'] / r['wall_time']) \
            if r.get('wall_time', 0) > 0 else 0
        loss_pct = ((total_expected - total_received) * 100 // total_expected) \
            if total_expected > 0 else 0
        e2e_rate = 0
        if r.get('first_recv') and r.get('last_recv') and total_received > 1:
            e2e_time = r['last_recv'] - r['first_recv']
            e2e_rate = int(total_received / e2e_time) if e2e_time > 0 else 0

        if qos == 0:
            passed = total_received >= (total_expected * 80 // 100)
        else:
            passed = total_received >= (total_expected * 95 // 100)
        status = "PASS" if passed else "FAIL"
        print(f"RESULT:qos={qos}:status={status}:pub_rate={pub_rate}:"
              f"received={total_received}:expected={total_expected}:"
              f"loss_pct={loss_pct}:e2e_rate={e2e_rate}:"
              f"wall_ms={int(r.get('wall_time', 0) * 1000)}:"
              f"pubs={r['num_pubs']}:subs={r['num_subs']}")

    sys.exit(0 if all_pass else 1)


if __name__ == '__main__':
    main()
