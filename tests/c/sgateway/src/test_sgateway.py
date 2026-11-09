#!/usr/bin/env python3
#
#   Run the real sgateway and drive both of its sides
#
#   A listener of this script is the output target of the sgateway
#   (OUTPUT_PORT) and a client of this script connects to its input port
#   (INPUT_PORT). The first run, with the `connections` trace of
#   C_SGATEWAY on:
#
#     1. bytes go input -> output and output -> input;
#     2. one output connection per client: the client goes away and the
#        sgateway closes its output connection; a second client gets an
#        output connection of its own, and nothing of the first one;
#     3. the output peer drops the connection: the sgateway drops the
#        client; a new client gets a new output connection;
#     4. a slow side, both ways: 8 MB against a peer that reads slowly
#        arrive whole and in order (up to 7.30.18 the read buffer of one
#        side was relayed to the other while it was read into again:
#        corrupted data and one ERROR per message);
#     5. a side that never reads: the sgateway stops reading the other
#        side (its memory does not grow, its log does not either), and
#        when that peer goes away the client is dropped;
#     6. a client that leaves while its output cannot connect: it is not
#        read meanwhile, so its leaving is seen when the output opens --
#        what it sent is delivered first, then its output is closed;
#     7. an orderly stop (SIGINT) with a client that does not read ends it
#        with exit code 0, and opens no output connection while it stops
#        (in 7.30.19 the stop of the output started it again for the
#        client that was still being closed: a session of seconds with the
#        backend, and "Destroying a RUNNING gobj" at the end).
#
#   The second run, with the default traces, a TLS input (tcps://, its
#   `input_crypto` attribute) and a TLS output (tcps://, its `crypto`
#   attribute): 20 clients, each one with its own output connection, relay
#   bytes both ways, and the log does not grow with them. Up to 7.30.19 a
#   TLS input could not be configured.
#
#   Each log must have no ERROR, no "Event NOT DEFINED", no WARNING, and
#   nothing left at the gbmem audit of the orderly end ("system memory not
#   free", CONFIG_DEBUG_TRACK_MEMORY).
#
#   Usage: test_sgateway.py <sgateway binary>
#   Exit 77 (skipped) when the binary is not there.
#
#   Copyright (c) 2026, ArtGins.
#   All Rights Reserved.
#
import json
import os
import random
import re
import shutil
import signal
import socket
import ssl
import subprocess
import sys
import tempfile
import threading
import time

PORTS = {'input_port': 18960, 'output_port': 18961}    # as check_test_ports.py reads them
INPUT_PORT = PORTS['input_port']
OUTPUT_PORT = PORTS['output_port']
TIMEOUT = 10
BIG = 8 * 1024 * 1024
NEVER_READ_SECONDS = 3
RSS_GROWTH_MAX_KB = 32 * 1024
QUIET_CLIENTS = 20
QUIET_LOG_LINES_MAX = 10
CERT = '/yuneta/agent/certs/localhost.crt'
KEY = '/yuneta/agent/certs/localhost.key'


#
#   Helpers
#
class Failed(Exception):
    pass


def log_text(log_path):
    with open(log_path, 'rb') as f:
        return f.read().decode('utf-8', errors='replace')


def count_side(log_path, msg, side):
    pattern = re.compile(r'"msg": "%s", "side": "%s"' % (re.escape(msg), side))
    return len(pattern.findall(log_text(log_path)))


def wait_side(log_path, msg, side, count, proc):
    t = time.monotonic() + TIMEOUT
    while time.monotonic() < t:
        if count_side(log_path, msg, side) >= count:
            return
        if proc.poll() is not None:
            raise Failed(f'sgateway ended (exit {proc.returncode}) waiting for "{msg}" of {side}')
        time.sleep(0.02)
    raise Failed(f'no "{msg}" #{count} of the {side} side in {TIMEOUT} s')


def recv_exactly(sock, n, what, slow=False):
    sock.settimeout(TIMEOUT)
    chunks = []
    got = 0
    while got < n:
        try:
            chunk = sock.recv(min(16384 if slow else 1048576, n - got))
        except socket.timeout:
            raise Failed(f'{what}: {got} of {n} bytes in {TIMEOUT} s')
        except ConnectionError as e:
            raise Failed(f'{what}: {e} after {got} of {n} bytes')
        if not chunk:
            raise Failed(f'{what}: connection closed after {got} of {n} bytes')
        chunks.append(chunk)
        got += len(chunk)
        if slow:
            time.sleep(0.001)
    return b''.join(chunks)


def expect(sock, payload, what, slow=False):
    data = recv_exactly(sock, len(payload), what, slow)
    if data != payload:
        first = next((i for i in range(len(data)) if data[i] != payload[i]), None)
        if len(payload) > 64:
            raise Failed(f'{what}: {len(payload)} bytes, the first different one is the {first}')
        raise Failed(f'{what}: got {data!r}, expected {payload!r}')
    print(f'  ok: {what} ({len(payload)} bytes)')


def expect_nothing(sock, what, seconds=0.3):
    sock.settimeout(seconds)
    try:
        data = sock.recv(65536)
    except socket.timeout:
        print(f'  ok: {what}')
        return
    raise Failed(f'{what}: got {data!r}')


def expect_closed(sock, what):
    """The peer closes the connection: EOF or a reset, and no data before"""
    sock.settimeout(TIMEOUT)
    try:
        data = sock.recv(65536)
    except socket.timeout:
        raise Failed(f'{what}: the connection is still open after {TIMEOUT} s')
    except ConnectionError:
        print(f'  ok: {what} (reset)')
        return
    if data:
        raise Failed(f'{what}: got {data!r} instead of the end of the connection')
    print(f'  ok: {what}')


def connect_input(proc, tls_context=None):
    t = time.monotonic() + TIMEOUT
    while time.monotonic() < t:
        if proc.poll() is not None:
            raise Failed(f'sgateway ended (exit {proc.returncode}) before its input port listened')
        try:
            sock = socket.create_connection(('127.0.0.1', INPUT_PORT), timeout=TIMEOUT)
        except OSError:
            time.sleep(0.05)
            continue
        if tls_context:
            try:
                sock = tls_context.wrap_socket(sock, server_hostname='localhost')
            except (ssl.SSLError, OSError) as e:
                sock.close()
                raise Failed(f'TLS handshake with the input port: {e}')
        return sock
    raise Failed(f'the input port {INPUT_PORT} did not listen in {TIMEOUT} s')


def make_listener():
    listener = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    listener.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
    listener.bind(('127.0.0.1', OUTPUT_PORT))
    listener.listen(4)
    return listener


def expect_no_connection(listener, what, seconds=0.5):
    listener.settimeout(seconds)
    try:
        conn, _ = listener.accept()
    except socket.timeout:
        print(f'  ok: {what}')
        return
    conn.close()
    raise Failed(f'{what}: the sgateway opened an output connection')


def accept_output(listener, what, tls_context=None):
    listener.settimeout(TIMEOUT)
    try:
        conn, _ = listener.accept()
    except socket.timeout:
        raise Failed(f'{what}: the sgateway did not connect to its output in {TIMEOUT} s')
    if tls_context:
        conn.settimeout(TIMEOUT)
        try:
            conn = tls_context.wrap_socket(conn, server_side=True)
        except (ssl.SSLError, OSError) as e:
            raise Failed(f'{what}: TLS handshake: {e}')
    return conn


def rss_kb(pid):
    with open(f'/proc/{pid}/status') as f:
        for line in f:
            if line.startswith('VmRSS:'):
                return int(line.split()[1])
    return 0


def start_sgateway(binary, work, name, input_url, output_url, extra):
    log_path = os.path.join(work, f'{name}.log')
    config_path = os.path.join(work, f'{name}.json')
    config = {
        'environment': {
            'work_dir': work,
            'domain_dir': name,
        },
        'yuno': {
            'autoplay': True,
        },
        'global': {
            'sgateway.input_url': input_url,
            'sgateway.output_url': output_url,
        },
    }
    for k, v in extra.items():
        config.setdefault(k, {}).update(v)
    with open(config_path, 'w') as f:
        json.dump(config, f, indent=4)
    log_file = open(log_path, 'wb')
    proc = subprocess.Popen(
        [binary, f'--config-file={config_path}'],
        stdout=log_file,
        stderr=subprocess.STDOUT,
        cwd=work,
    )
    return proc, log_file, log_path


def stop_sgateway(proc):
    proc.send_signal(signal.SIGINT)
    try:
        return proc.wait(timeout=15)
    except subprocess.TimeoutExpired:
        raise Failed('the sgateway did not end in 15 s after SIGINT')


def check_log(log_path, failure, exit_code, problems):
    if failure:
        problems.append(failure)
    if exit_code is not None and exit_code != 0:
        problems.append(f'exit code {exit_code} after SIGINT (not 0)')
    text = log_text(log_path)
    for line in text.splitlines():
        if 'ERROR' in line or 'WARNING' in line or 'Event NOT DEFINED' in line or \
                'system memory not free' in line:
            problems.append('log: ' + line[:400])
    if problems:
        print(f'--- {os.path.basename(log_path)} ---')
        print(text[-20000:])
        print('--- end ---')


#
#   The first run: plain output, connections traced
#
def run_main(binary, work):
    listener = make_listener()

    proc, log_file, log_path = start_sgateway(
        binary, work, 'sgateway',
        f'tcp://127.0.0.1:{INPUT_PORT}',
        f'tcp://127.0.0.1:{OUTPUT_PORT}',
        {'yuno': {'trace_levels': {'C_SGATEWAY': ['connections']}}}
    )
    failure = None
    exit_code = None
    socks = []
    try:
        print('1. both ways')
        client_a = connect_input(proc)
        socks.append(client_a)
        out_a = accept_output(listener, 'output connection of the first client')
        socks.append(out_a)
        client_a.sendall(b'input-1 hello')
        expect(out_a, b'input-1 hello', 'input -> output')
        out_a.sendall(b'SECRET-REPLY-FOR-A')
        expect(client_a, b'SECRET-REPLY-FOR-A', 'output -> input')

        print('2. one output connection per client')
        client_a.close()
        expect_closed(out_a, 'the client went away: its output connection is closed')
        wait_side(log_path, 'Side closed', 'input', 1, proc)
        client_b = connect_input(proc)
        socks.append(client_b)
        out_b = accept_output(listener, 'output connection of the second client')
        socks.append(out_b)
        expect_nothing(client_b, 'the second client gets nothing of the first one')
        client_b.sendall(b'input-2 again')
        expect(out_b, b'input-2 again', 'input -> output, second client')
        out_b.sendall(b'output-2 again')
        expect(client_b, b'output-2 again', 'output -> input, second client')

        print('3. the output peer drops the connection: the client is dropped')
        out_b.close()
        expect_closed(client_b, 'the output went away: its client is dropped')
        wait_side(log_path, 'Side closed', 'input', 2, proc)
        client_c = connect_input(proc)
        socks.append(client_c)
        out_c = accept_output(listener, 'output connection of the third client')
        socks.append(out_c)
        client_c.sendall(b'input-3 back')
        expect(out_c, b'input-3 back', 'input -> output, third client')
        out_c.sendall(b'output-3 back')
        expect(client_c, b'output-3 back', 'output -> input, third client')

        print('4. a slow side, both ways')
        data = random.Random(70).randbytes(BIG)
        sender = threading.Thread(target=client_c.sendall, args=(data,), daemon=True)
        sender.start()
        expect(out_c, data, 'input -> output, a slow output peer', slow=True)
        sender.join(TIMEOUT)
        data = random.Random(71).randbytes(BIG)
        sender = threading.Thread(target=out_c.sendall, args=(data,), daemon=True)
        sender.start()
        expect(client_c, data, 'output -> input, a slow client', slow=True)
        sender.join(TIMEOUT)

        print('5. an output peer that never reads')
        rss_before = rss_kb(proc.pid)
        chunk = b'x' * 65536
        sent = 0
        client_c.setblocking(False)
        t_end = time.monotonic() + NEVER_READ_SECONDS
        while time.monotonic() < t_end:
            try:
                sent += client_c.send(chunk)
            except BlockingIOError:
                time.sleep(0.01)
        client_c.setblocking(True)
        rss_after = rss_kb(proc.pid)
        print(f'  the client got {sent} bytes taken in {NEVER_READ_SECONDS} s, '
              f'the sgateway grew {rss_after - rss_before} KB')
        if rss_after - rss_before > RSS_GROWTH_MAX_KB:
            raise Failed(f'the sgateway grew {rss_after - rss_before} KB against a peer that does not read')
        out_c.close()
        expect_closed(client_c, 'the output that never read went away: its client is dropped')
        wait_side(log_path, 'Side closed', 'input', 3, proc)

        print('6. a client that leaves while its output cannot connect')
        listener.close()
        client_d = connect_input(proc)
        socks.append(client_d)
        client_d.sendall(b'input-4 left early')
        client_d.close()
        time.sleep(0.5)
        listener = make_listener()
        out_d = accept_output(listener, 'output connection of the client that left')
        socks.append(out_d)
        expect(out_d, b'input-4 left early', 'what it sent is delivered when its output opens')
        expect_closed(out_d, 'then its output is closed')
        wait_side(log_path, 'Side closed', 'input', 4, proc)

        print('7. orderly stop with a client that does not read')
        client_e = connect_input(proc)
        socks.append(client_e)
        out_e = accept_output(listener, 'output connection of the client that does not read')
        socks.append(out_e)
        out_e.setblocking(False)
        blocked_since = None
        t_end = time.monotonic() + TIMEOUT
        while time.monotonic() < t_end:
            try:
                out_e.send(chunk)
                blocked_since = None
            except BlockingIOError:
                if blocked_since is None:
                    blocked_since = time.monotonic()
                elif time.monotonic() - blocked_since > 0.5:
                    break
                time.sleep(0.01)
        out_e.setblocking(True)
        if blocked_since is None:
            raise Failed('the client that does not read never held the output peer back')
        exit_code = stop_sgateway(proc)
        expect_no_connection(listener, 'no output connection opened while it stopped')

    except Failed as e:
        failure = str(e)
    finally:
        if proc.poll() is None:
            proc.kill()
            proc.wait()
        log_file.close()
        for s in socks:
            s.close()
        listener.close()

    problems = []
    check_log(log_path, failure, exit_code, problems)
    return problems


#
#   The second run: TLS output, default traces
#
def run_quiet_tls(binary, work):
    tls_context = ssl.SSLContext(ssl.PROTOCOL_TLS_SERVER)
    tls_context.load_cert_chain(CERT, KEY)
    # The test certificate is self-signed, with no CA flag and no SAN: trusted
    # as it is, its CN not matched (python matches the SAN only)
    tls_client_context = ssl.create_default_context(cafile=CERT)
    tls_client_context.check_hostname = False
    tls_client_context.verify_flags &= ~ssl.VERIFY_X509_STRICT

    listener = make_listener()

    proc, log_file, log_path = start_sgateway(
        binary, work, 'sgateway_tls',
        f'tcps://127.0.0.1:{INPUT_PORT}',
        f'tcps://127.0.0.1:{OUTPUT_PORT}',
        {'global': {
            'sgateway.crypto': {
                'ssl_trusted_certificate': CERT,
                'ssl_server_name': 'localhost',
            },
            'sgateway.input_crypto': {
                'ssl_certificate': CERT,
                'ssl_certificate_key': KEY,
            },
        }}
    )
    failure = None
    exit_code = None
    socks = []
    try:
        print(f'8. a TLS input and a TLS output, default traces: {QUIET_CLIENTS} clients')
        client = connect_input(proc, tls_client_context)
        socks.append(client)
        out = accept_output(listener, 'TLS output connection of the first client', tls_context)
        socks.append(out)
        client.sendall(b'first')
        expect(out, b'first', 'TLS input -> TLS output')
        out.sendall(b'first back')
        expect(client, b'first back', 'TLS output -> TLS input')
        client.close()
        expect_closed(out, 'the client went away: its TLS output connection is closed')
        lines_before = len(log_text(log_path).splitlines())

        for i in range(QUIET_CLIENTS):
            client = connect_input(proc, tls_client_context)
            socks.append(client)
            out = accept_output(listener, f'TLS output connection of client {i}', tls_context)
            socks.append(out)
            payload = f'client {i}'.encode()
            client.sendall(payload)
            recv_exactly(out, len(payload), f'client {i} -> TLS output')
            out.sendall(payload + b' back')
            recv_exactly(client, len(payload) + 5, f'TLS output -> client {i}')
            client.close()
            expect_closed(out, f'client {i} went away: its TLS output is closed')
        print(f'  ok: {QUIET_CLIENTS} clients relayed through TLS')

        grown = len(log_text(log_path).splitlines()) - lines_before
        if grown > QUIET_LOG_LINES_MAX:
            raise Failed(f'the log grew {grown} lines with {QUIET_CLIENTS} clients (default traces)')
        print(f'  ok: the log grew {grown} lines with {QUIET_CLIENTS} clients')

        print('9. orderly stop')
        exit_code = stop_sgateway(proc)

    except Failed as e:
        failure = str(e)
    finally:
        if proc.poll() is None:
            proc.kill()
            proc.wait()
        log_file.close()
        for s in socks:
            s.close()
        listener.close()

    problems = []
    check_log(log_path, failure, exit_code, problems)
    return problems


def main():
    if len(sys.argv) != 2:
        print('Usage: test_sgateway.py <sgateway binary>')
        return 2
    binary = os.path.abspath(sys.argv[1])
    if not os.access(binary, os.X_OK):
        print(f'SKIPPED: no sgateway binary at {binary!r} (built by yunetas build)')
        return 77
    work = tempfile.mkdtemp(prefix='test_sgateway-')
    try:
        problems = run_main(binary, work)
        problems += run_quiet_tls(binary, work)
    finally:
        shutil.rmtree(work, ignore_errors=True)

    if problems:
        for p in problems:
            print('TEST FAILED: ' + p)
        return 1
    print('TEST PASSED')
    return 0


if __name__ == '__main__':
    sys.exit(main())
