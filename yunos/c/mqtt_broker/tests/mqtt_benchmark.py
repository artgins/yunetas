#!/usr/bin/env python3
"""
MQTT Broker Speed Benchmark
Measures maximum message throughput for QoS 0, 1, and 2.
Uses raw sockets for zero overhead — measures actual broker performance.

Usage:
    python3 mqtt_benchmark.py [--host HOST] [--port PORT] [--count N]
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


def run_subscriber(host, port, topic, qos, count, result, stop_event):
    """Subscriber thread: connects, subscribes, counts received messages."""
    try:
        sock = mqtt_connect(host, port, f'bench-sub-{qos}-{int(time.time())}')
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
                        # Extract mid from publish
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


def run_publisher(host, port, topic, qos, count, payload_data, result):
    """Publisher: connects, publishes count messages, measures time."""
    try:
        sock = mqtt_connect(host, port, f'bench-pub-{qos}-{int(time.time())}')
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


def run_benchmark(host, port, topic, qos, count, payload_size):
    payload_data = (b'X' * payload_size)

    sub_result = {'received': 0, 'ready': False}
    pub_result = {}
    stop_event = threading.Event()

    # Start subscriber
    sub_thread = threading.Thread(
        target=run_subscriber,
        args=(host, port, topic, qos, count, sub_result, stop_event)
    )
    sub_thread.start()

    # Wait for subscriber to be ready
    deadline = time.monotonic() + 5.0
    while not sub_result.get('ready') and time.monotonic() < deadline:
        time.sleep(0.05)
    if not sub_result.get('ready'):
        stop_event.set()
        sub_thread.join(timeout=3)
        return {'error': 'Subscriber failed to connect'}

    time.sleep(0.2)  # brief settle

    # Run publisher (blocking)
    run_publisher(host, port, topic, qos, count, payload_data, pub_result)

    # Wait for subscriber to finish receiving
    if qos == 0:
        time.sleep(2)
    elif qos == 1:
        time.sleep(3)
    else:
        time.sleep(4)

    stop_event.set()
    sub_thread.join(timeout=5)

    return {
        'qos': qos,
        'count': count,
        'payload_size': payload_size,
        'published': pub_result.get('published', 0),
        'pub_time': pub_result.get('pub_time', 0),
        'received': sub_result.get('received', 0),
        'first_recv': sub_result.get('first_recv_time'),
        'last_recv': sub_result.get('last_recv_time'),
        'pub_error': pub_result.get('error'),
        'sub_error': sub_result.get('error'),
    }


def main():
    parser = argparse.ArgumentParser(description='MQTT Broker Speed Benchmark')
    parser.add_argument('--host', default='127.0.0.1', help='Broker host')
    parser.add_argument('--port', type=int, default=1810, help='Broker port')
    parser.add_argument('--count', type=int, default=5000, help='Messages per QoS level')
    parser.add_argument('--payload', type=int, default=64, help='Payload size in bytes')
    parser.add_argument('--qos', type=int, default=-1, help='Test only this QoS (-1 = all)')
    args = parser.parse_args()

    print(f"MQTT Broker Speed Benchmark")
    print(f"  Broker  : {args.host}:{args.port}")
    print(f"  Messages: {args.count} per QoS level")
    print(f"  Payload : {args.payload} bytes")
    print()

    qos_levels = [args.qos] if args.qos >= 0 else [0, 1, 2]
    all_pass = True
    results = []

    for qos in qos_levels:
        topic = f"bench/speed/q{qos}/{int(time.time())}"
        r = run_benchmark(args.host, args.port, topic, qos, args.count, args.payload)
        results.append(r)

        if r.get('pub_error') or r.get('sub_error'):
            err = r.get('pub_error') or r.get('sub_error')
            print(f"  QoS {qos}: ERROR - {err}")
            all_pass = False
            continue

        pub_time_ms = r['pub_time'] * 1000
        pub_rate = int(r['published'] / r['pub_time']) if r['pub_time'] > 0 else 0
        received = r['received']
        loss = r['count'] - received
        loss_pct = (loss * 100 // r['count']) if r['count'] > 0 else 0

        # End-to-end rate based on subscriber receive window
        e2e_time = 0
        e2e_rate = 0
        if r.get('first_recv') and r.get('last_recv') and received > 1:
            e2e_time = r['last_recv'] - r['first_recv']
            e2e_rate = int(received / e2e_time) if e2e_time > 0 else received

        # Pass/fail thresholds
        if qos == 0:
            passed = received >= (r['count'] * 80 // 100)
        else:
            passed = received >= (r['count'] * 95 // 100)

        status = "PASS" if passed else "FAIL"
        if not passed:
            all_pass = False

        print(f"  [{status}] QoS {qos}: "
              f"pub {pub_rate:,} msg/s ({pub_time_ms:,.0f} ms) | "
              f"recv {received:,}/{r['count']:,} ({loss_pct}% loss) | "
              f"e2e {e2e_rate:,} msg/s")

    print()

    # Machine-readable summary for the shell test
    for r in results:
        qos = r['qos']
        pub_rate = int(r.get('published', 0) / r['pub_time']) if r.get('pub_time', 0) > 0 else 0
        received = r.get('received', 0)
        loss_pct = ((r['count'] - received) * 100 // r['count']) if r['count'] > 0 else 0
        e2e_rate = 0
        if r.get('first_recv') and r.get('last_recv') and received > 1:
            e2e_time = r['last_recv'] - r['first_recv']
            e2e_rate = int(received / e2e_time) if e2e_time > 0 else 0
        if qos == 0:
            passed = received >= (r['count'] * 80 // 100)
        else:
            passed = received >= (r['count'] * 95 // 100)
        status = "PASS" if passed else "FAIL"
        print(f"RESULT:qos={qos}:status={status}:pub_rate={pub_rate}:"
              f"received={received}:count={r['count']}:loss_pct={loss_pct}:"
              f"e2e_rate={e2e_rate}:pub_ms={int(r.get('pub_time', 0) * 1000)}")

    sys.exit(0 if all_pass else 1)


if __name__ == '__main__':
    main()
