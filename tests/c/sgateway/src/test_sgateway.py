#!/usr/bin/env python3
#
#   Run the real sgateway and drive both of its sides
#
#   A listener of this script is the output target of the sgateway
#   (OUTPUT_PORT) and a client of this script connects to its input port
#   (INPUT_PORT). Then:
#
#     1. bytes go input -> output and output -> input;
#     2. the client goes away and comes back: bytes flow again (the input
#        side takes the new connection);
#     3. the output peer drops the connection: the sgateway connects again
#        by itself, and bytes flow again;
#     4. an orderly stop (SIGINT) ends it with exit code 0.
#
#   Its log (stdout) must have no ERROR, no "Event NOT DEFINED", no
#   WARNING, and nothing left at the gbmem audit of the orderly end
#   ("system memory not free", CONFIG_DEBUG_TRACK_MEMORY).
#
#   Each step waits for what the sgateway says in its log ("Side open",
#   "Side closed") before the bytes go: a connection accepted by the
#   kernel is not yet a side open in the sgateway, and bytes sent to a
#   side not open are dropped (with a WARNING).
#
#   Usage: test_sgateway.py <sgateway binary>
#   Exit 77 (skipped) when the binary is not there.
#
#   Copyright (c) 2026, ArtGins.
#   All Rights Reserved.
#
import json
import os
import re
import shutil
import signal
import socket
import subprocess
import sys
import tempfile
import time

PORTS = {'input_port': 18960, 'output_port': 18961}    # as check_test_ports.py reads them
INPUT_PORT = PORTS['input_port']
OUTPUT_PORT = PORTS['output_port']
TIMEOUT = 10


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


def recv_exactly(sock, n, what):
    sock.settimeout(TIMEOUT)
    data = b''
    while len(data) < n:
        try:
            chunk = sock.recv(n - len(data))
        except socket.timeout:
            raise Failed(f'{what}: {len(data)} of {n} bytes in {TIMEOUT} s: {data!r}')
        if not chunk:
            raise Failed(f'{what}: connection closed after {len(data)} of {n} bytes: {data!r}')
        data += chunk
    return data


def expect(sock, payload, what):
    data = recv_exactly(sock, len(payload), what)
    if data != payload:
        raise Failed(f'{what}: got {data!r}, expected {payload!r}')
    print(f'  ok: {what} ({len(payload)} bytes)')


def connect_input(proc):
    t = time.monotonic() + TIMEOUT
    while time.monotonic() < t:
        if proc.poll() is not None:
            raise Failed(f'sgateway ended (exit {proc.returncode}) before its input port listened')
        try:
            return socket.create_connection(('127.0.0.1', INPUT_PORT), timeout=TIMEOUT)
        except OSError:
            time.sleep(0.05)
    raise Failed(f'the input port {INPUT_PORT} did not listen in {TIMEOUT} s')


def accept_output(listener, what):
    listener.settimeout(TIMEOUT)
    try:
        conn, _ = listener.accept()
    except socket.timeout:
        raise Failed(f'{what}: the sgateway did not connect to its output in {TIMEOUT} s')
    return conn


#
#   The run
#
def run(binary, work):
    log_path = os.path.join(work, 'sgateway.log')
    config_path = os.path.join(work, 'sgateway.json')
    config = {
        'environment': {
            'work_dir': work,
            'domain_dir': 'sgateway',
        },
        'yuno': {
            'autoplay': True,
        },
        'global': {
            'sgateway.input_url': f'tcp://127.0.0.1:{INPUT_PORT}',
            'sgateway.output_url': f'tcp://127.0.0.1:{OUTPUT_PORT}',
        },
    }
    with open(config_path, 'w') as f:
        json.dump(config, f, indent=4)

    listener = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    listener.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
    listener.bind(('127.0.0.1', OUTPUT_PORT))
    listener.listen(4)

    log_file = open(log_path, 'wb')
    proc = subprocess.Popen(
        [binary, f'--config-file={config_path}'],
        stdout=log_file,
        stderr=subprocess.STDOUT,
        cwd=work,
    )
    failure = None
    exit_code = None
    try:
        print('1. both ways')
        out1 = accept_output(listener, 'first output connection')
        wait_side(log_path, 'Side open', 'output', 1, proc)
        client1 = connect_input(proc)
        client1.sendall(b'input-1 hello')
        expect(out1, b'input-1 hello', 'input -> output')
        wait_side(log_path, 'Side open', 'input', 1, proc)
        out1.sendall(b'output-1 world')
        expect(client1, b'output-1 world', 'output -> input')

        print('2. the input client goes and comes back')
        client1.close()
        wait_side(log_path, 'Side closed', 'input', 1, proc)
        client2 = connect_input(proc)
        client2.sendall(b'input-2 again')
        expect(out1, b'input-2 again', 'input -> output, second client')
        wait_side(log_path, 'Side open', 'input', 2, proc)
        out1.sendall(b'output-2 again')
        expect(client2, b'output-2 again', 'output -> input, second client')

        print('3. the output peer drops the connection: the sgateway connects again')
        out1.close()
        wait_side(log_path, 'Side closed', 'output', 1, proc)
        out2 = accept_output(listener, 'output connection again')
        out2.sendall(b'output-3 back')
        expect(client2, b'output-3 back', 'output -> input, after the reconnect')
        wait_side(log_path, 'Side open', 'output', 2, proc)
        client2.sendall(b'input-3 back')
        expect(out2, b'input-3 back', 'input -> output, after the reconnect')

        print('4. orderly stop')
        proc.send_signal(signal.SIGINT)
        try:
            exit_code = proc.wait(timeout=15)
        except subprocess.TimeoutExpired:
            raise Failed('the sgateway did not end in 15 s after SIGINT')
        client2.close()
        out2.close()

    except Failed as e:
        failure = str(e)
    finally:
        if proc.poll() is None:
            proc.kill()
            proc.wait()
        log_file.close()
        listener.close()

    #
    #   The log
    #
    text = log_text(log_path)
    problems = []
    if failure:
        problems.append(failure)
    if exit_code is not None and exit_code != 0:
        problems.append(f'exit code {exit_code} after SIGINT (not 0)')
    for line in text.splitlines():
        if 'ERROR' in line or 'WARNING' in line or 'Event NOT DEFINED' in line or \
                'system memory not free' in line:
            problems.append('log: ' + line[:400])
    if problems:
        print('--- sgateway log ---')
        print(text[-20000:])
        print('--- end ---')
        for p in problems:
            print('TEST FAILED: ' + p)
        return 1

    print('TEST PASSED')
    return 0


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
        return run(binary, work)
    finally:
        shutil.rmtree(work, ignore_errors=True)


if __name__ == '__main__':
    sys.exit(main())
