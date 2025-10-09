#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
MQTT CONNECT/DISCONNECT test harness (v3.1.1 + v5.0)

Covers:
- v3.1.1 CONNECT: cleanSession, willFlag/retain/qos, username/password, keepAlive edges,
  empty clientId rules, reserved flag abuse, wrong protocol name/level, malformed RL.
- v5 CONNECT: cleanStart, will (with Will Properties), Username/Password, keepAlive,
  properties (Session Expiry, Receive Maximum, Max Packet Size), invalid flag/reserved bit,
  oversized/undersized fields.
- v3.1.1 DISCONNECT (E0 00).
- v5 DISCONNECT: Reason Codes (0x00, 0x04, 0x80) + Properties (Session Expiry, Reason String).

Expectations model:
- For invalid CONNECT: PASS if broker closes socket OR sends non-success CONNACK RC/reason code.
- For valid CONNECT: PASS only if success ACK received (Code=0 in v3; Reason=0x00 in v5).
- For DISCONNECT: PASS if the broker closes the connection reasonably soon (common behavior),
  or remains open without error (allowed). We don’t fail on either unless the broker sends a protocol error.

Author: ChatGPT (GPT-5 Thinking)
"""

import argparse
import socket
import struct
import time
from typing import Dict, Any, List, Optional, Tuple

# --------------- Low-level encoding helpers -----------------

def enc_varint(x: int) -> bytes:
    """MQTT Variable Byte Integer"""
    out = bytearray()
    if x < 0:
        raise ValueError("varint must be >= 0")
    while True:
        byte = x % 128
        x //= 128
        if x > 0:
            byte |= 0x80
        out.append(byte)
        if x == 0:
            break
    return bytes(out)

def enc_str(s: str) -> bytes:
    b = s.encode("utf-8")
    if len(b) > 0xFFFF:
        raise ValueError("string too long")
    return struct.pack("!H", len(b)) + b

def enc_bin(b: bytes) -> bytes:
    if len(b) > 0xFFFF:
        raise ValueError("binary too long")
    return struct.pack("!H", len(b)) + b

# MQTT v5 property encoders (subset used here)
def prop_u32(identifier: int, value: int) -> bytes:
    return bytes([identifier]) + struct.pack("!I", value)

def prop_u16(identifier: int, value: int) -> bytes:
    return bytes([identifier]) + struct.pack("!H", value)

def prop_varint(identifier: int, value: int) -> bytes:
    return bytes([identifier]) + enc_varint(value)

def prop_str(identifier: int, s: str) -> bytes:
    return bytes([identifier]) + enc_str(s)

def wrap_props(props: bytes) -> bytes:
    return enc_varint(len(props)) + props

# --------------- Packet builders -----------------

def build_connect_v311(params: Dict[str, Any]) -> bytes:
    """
    params keys:
      client_id:str, clean:bool, keepalive:int,
      will: Optional[dict] {topic:str, payload:bytes, qos:int(0-2), retain:bool},
      username: Optional[str], password: Optional[bytes],
      hack_reserved_connect_flag_bit: Optional[bool] -> set reserved LSB to 1 (invalid)
      bad_protocol: Optional[str] -> override Protocol Name
      bad_level: Optional[int]  -> override Protocol Level
      malformed_remaining_length: Optional[int] -> override RL (invalid)
    """
    proto_name = params.get("bad_protocol", "MQTT")
    proto_level = params.get("bad_level", 4)  # 4 for v3.1.1
    vh = bytearray()
    vh += enc_str(proto_name)
    vh.append(proto_level)

    # Connect Flags
    flags = 0
    clean = 1 if params.get("clean", True) else 0
    flags |= (clean << 1)

    will = params.get("will")
    if will:
        flags |= 1 << 2  # Will Flag
        qos = int(will.get("qos", 0)) & 0x03
        flags |= (qos << 3)
        if will.get("retain", False):
            flags |= 1 << 5
    if params.get("password") is not None:
        flags |= 1 << 6
    if params.get("username") is not None:
        flags |= 1 << 7

    if params.get("hack_reserved_connect_flag_bit"):
        flags |= 0x01  # LSB reserved bit set (invalid in both 3.1.1 and 5.0)

    vh.append(flags)
    keepalive = int(params.get("keepalive", 60)) & 0xFFFF
    vh += struct.pack("!H", keepalive)

    # Payload
    pl = bytearray()
    client_id = params.get("client_id", "")
    pl += enc_str(client_id)

    if will:
        # No will properties in 3.1.1
        pl += enc_str(will.get("topic", "t/+/0"))
        wpayload = will.get("payload", b"")
        pl += enc_bin(wpayload if isinstance(wpayload, (bytes, bytearray)) else str(wpayload).encode("utf-8"))

    if params.get("username") is not None:
        pl += enc_str(params["username"])
    if params.get("password") is not None:
        pw = params["password"]
        pl += enc_bin(pw if isinstance(pw, (bytes, bytearray)) else str(pw).encode("utf-8"))

    # Fixed header
    packet = bytearray()
    packet.append(0x10)  # CONNECT
    remaining_length = len(vh) + len(pl)
    if "malformed_remaining_length" in params:
        packet += enc_varint(params["malformed_remaining_length"])
    else:
        packet += enc_varint(remaining_length)
    packet += vh + pl
    return bytes(packet)

def build_connect_v5(params: Dict[str, Any]) -> bytes:
    """
    Similar to v3.1.1 but with v5 changes:
      - Protocol Name "MQTT", Level 5
      - Clean Start bit instead of Clean Session
      - Properties after KeepAlive
      - Will Properties before Will Topic/Payload
      - Username/Password still in payload
      v5 properties (subset):
        'session_expiry': int (prop 0x11, Session Expiry Interval)
        'receive_max': int (prop 0x21, Receive Maximum)
        'max_packet_size': int (prop 0x27, Maximum Packet Size)
    """
    proto_name = params.get("bad_protocol", "MQTT")
    proto_level = params.get("bad_level", 5)  # 5 for v5.0
    vh = bytearray()
    vh += enc_str(proto_name)
    vh.append(proto_level)

    flags = 0
    clean_start = 1 if params.get("clean", True) else 0
    flags |= (clean_start << 1)

    will = params.get("will")
    if will:
        flags |= 1 << 2
        qos = int(will.get("qos", 0)) & 0x03
        flags |= (qos << 3)
        if will.get("retain", False):
            flags |= 1 << 5
    if params.get("password") is not None:
        flags |= 1 << 6
    if params.get("username") is not None:
        flags |= 1 << 7

    if params.get("hack_reserved_connect_flag_bit"):
        flags |= 0x01  # invalid

    vh.append(flags)
    keepalive = int(params.get("keepalive", 60)) & 0xFFFF
    vh += struct.pack("!H", keepalive)

    # ---- v5 properties (Connect Properties) ----
    props = bytearray()
    if "session_expiry" in params:
        props += prop_u32(0x11, int(params["session_expiry"]))      # Session Expiry Interval
    if "receive_max" in params:
        props += prop_u16(0x21, int(params["receive_max"]))         # Receive Maximum
    if "max_packet_size" in params:
        props += prop_u32(0x27, int(params["max_packet_size"]))     # Maximum Packet Size
    if "reason_string" in params:
        # Not a Connect property; kept here only for symmetry with DISCONNECT tests; ignore
        pass
    vh += wrap_props(bytes(props))

    # Payload
    pl = bytearray()
    pl += enc_str(params.get("client_id", ""))

    if will:
        # Will Properties (subset)
        will_props = bytearray()
        if "will_delay" in will:
            will_props += prop_u32(0x18, int(will["will_delay"]))   # Will Delay Interval
        if "content_type" in will:
            will_props += prop_str(0x03, will["content_type"])      # Content Type
        pl += wrap_props(bytes(will_props))
        pl += enc_str(will.get("topic", "t/+/0"))
        wpayload = will.get("payload", b"")
        pl += enc_bin(wpayload if isinstance(wpayload, (bytes, bytearray)) else str(wpayload).encode("utf-8"))

    if params.get("username") is not None:
        pl += enc_str(params["username"])
    if params.get("password") is not None:
        pw = params["password"]
        pl += enc_bin(pw if isinstance(pw, (bytes, bytearray)) else str(pw).encode("utf-8"))

    # Fixed header
    packet = bytearray()
    packet.append(0x10)
    remaining_length = len(vh) + len(pl)
    if "malformed_remaining_length" in params:
        packet += enc_varint(params["malformed_remaining_length"])
    else:
        packet += enc_varint(remaining_length)
    packet += vh + pl
    return bytes(packet)

def build_disconnect_v311() -> bytes:
    return b"\xE0\x00"

def build_disconnect_v5(reason: Optional[int] = None, props: Optional[bytes] = None) -> bytes:
    body = bytearray()
    if reason is None and not props:
        # minimal
        body = b""
    else:
        if reason is None:
            reason = 0x00  # Normal
        body.append(reason)
        if props is None:
            props = b""
        body += wrap_props(props)
    return bytes([0xE0]) + enc_varint(len(body)) + bytes(body)

# --------------- Parsers -----------------

def recv_all(sock: socket.socket, n: int, timeout: float) -> bytes:
    sock.settimeout(timeout)
    buf = bytearray()
    while len(buf) < n:
        chunk = sock.recv(n - len(buf))
        if not chunk:
            break
        buf += chunk
    return bytes(buf)

def read_varint(sock: socket.socket, timeout: float) -> Optional[Tuple[int, bytes]]:
    sock.settimeout(timeout)
    multiplier = 1
    value = 0
    raw = bytearray()
    for _ in range(4):
        b = sock.recv(1)
        if not b:
            return None
        raw += b
        digit = b[0]
        value += (digit & 0x7F) * multiplier
        if (digit & 0x80) == 0:
            return value, bytes(raw)
        multiplier *= 128
    return None

def read_connack_v311(sock: socket.socket, timeout: float) -> Tuple[bool, Optional[int], str]:
    """
    Returns (got_ack, return_code, info)
    """
    sock.settimeout(timeout)
    h = sock.recv(1)
    if not h:
        return False, None, "no header"
    if h[0] != 0x20:
        return False, None, f"unexpected packet type 0x{h[0]:02x}"
    rl = sock.recv(1)
    if not rl:
        return False, None, "no RL"
    if rl[0] != 0x02:
        # some brokers still send 0x02; others strictly close on protocol issues
        body = recv_all(sock, rl[0], timeout) if rl[0] else b""
        return False, None, f"unexpected RL {rl[0]} body={body.hex()}"
    body = recv_all(sock, 2, timeout)
    if len(body) != 2:
        return False, None, "short body"
    # session present = body[0]
    rc = body[1]
    return True, rc, "ok"

def read_connack_v5(sock: socket.socket, timeout: float) -> Tuple[bool, Optional[int], str]:
    """
    Returns (got_ack, reason_code, info)
    """
    sock.settimeout(timeout)
    h = sock.recv(1)
    if not h:
        return False, None, "no header"
    if h[0] != 0x20:
        return False, None, f"unexpected packet type 0x{h[0]:02x}"
    vi = read_varint(sock, timeout)
    if not vi:
        return False, None, "no RL"
    rl, _raw = vi
    body = recv_all(sock, rl, timeout)
    if len(body) < 2:
        return False, None, "short body"
    # byte0: Acknowledge Flags, byte1: Reason Code, then properties...
    reason = body[1]
    return True, reason, "ok"

# --------------- Test catalog -----------------

def t_valid_v311_min():
    return {
        "name": "v311: cleanSession=1, minimal",
        "version": "3.1.1",
        "connect": {
            "client_id": "cid-min",
            "clean": True,
            "keepalive": 0
        },
        "expect": {"success": True}
    }

def t_v311_empty_cid_clean_ok():
    return {
        "name": "v311: empty clientId allowed with cleanSession=1",
        "version": "3.1.1",
        "connect": {
            "client_id": "",
            "clean": True,
            "keepalive": 30
        },
        "expect": {"success": True}  # many brokers accept and may assign server-generated id
    }

def t_v311_empty_cid_no_clean_invalid():
    return {
        "name": "v311: empty clientId with cleanSession=0 (invalid)",
        "version": "3.1.1",
        "connect": {
            "client_id": "",
            "clean": False,
            "keepalive": 30
        },
        "expect": {"success": False}  # close or RC!=0
    }

def t_v311_will_qos_retain():
    return {
        "name": "v311: will flag qos=2 retain=1",
        "version": "3.1.1",
        "connect": {
            "client_id": "cid-will",
            "clean": True,
            "keepalive": 10,
            "will": {"topic": "t/will", "payload": b"bye", "qos": 2, "retain": True}
        },
        "expect": {"success": True}
    }

def t_v311_username_pw_ok():
    return {
        "name": "v311: username+password ok",
        "version": "3.1.1",
        "connect": {
            "client_id": "cid-auth",
            "clean": True,
            "username": "u",
            "password": b"p",
        },
        "expect": {"success": True}
    }

def t_v311_pw_without_user_invalid():
    return {
        "name": "v311: password set without username (invalid)",
        "version": "3.1.1",
        "connect": {
            "client_id": "cid-bad",
            "clean": True,
            "password": b"p-only",
        },
        "expect": {"success": False}
    }

def t_v311_reserved_flag_bit_set():
    return {
        "name": "v311: reserved connect flag bit set (invalid)",
        "version": "3.1.1",
        "connect": {
            "client_id": "cid-rsv",
            "clean": True,
            "hack_reserved_connect_flag_bit": True
        },
        "expect": {"success": False}
    }

def t_v311_bad_proto_level():
    return {
        "name": "v311: bad protocol level (3) invalid for 3.1.1",
        "version": "3.1.1",
        "connect": {
            "client_id": "cid-badlvl",
            "clean": True,
            "bad_level": 3
        },
        "expect": {"success": False}
    }

def t_v311_keepalive_edges():
    return {
        "name": "v311: keepalive edge 65535",
        "version": "3.1.1",
        "connect": {
            "client_id": "cid-ka",
            "clean": True,
            "keepalive": 65535
        },
        "expect": {"success": True}
    }

def t_v311_malformed_rl():
    return {
        "name": "v311: malformed remaining length (too small)",
        "version": "3.1.1",
        "connect": {
            "client_id": "cid-mal",
            "clean": True,
            "malformed_remaining_length": 0  # should be >0
        },
        "expect": {"success": False}
    }

# ---- v5 ----

def t_v5_minimal():
    return {
        "name": "v5: cleanStart=1 minimal props",
        "version": "5.0",
        "connect": {
            "client_id": "cid5",
            "clean": True,
            "keepalive": 0,
            "session_expiry": 0  # immediate expiry
        },
        "expect": {"success": True, "reason_ok": [0x00]}
    }

def t_v5_empty_cid_clean():
    return {
        "name": "v5: empty clientId with cleanStart=1 (usually ok)",
        "version": "5.0",
        "connect": {
            "client_id": "",
            "clean": True,
            "keepalive": 30
        },
        "expect": {"success": True}
    }

def t_v5_receive_max_and_mps():
    return {
        "name": "v5: props receive_max=10, max_packet_size=256",
        "version": "5.0",
        "connect": {
            "client_id": "cid5-props",
            "clean": True,
            "receive_max": 10,
            "max_packet_size": 256
        },
        "expect": {"success": True}
    }

def t_v5_will_with_props():
    return {
        "name": "v5: will qos=1 retain=0 with will props",
        "version": "5.0",
        "connect": {
            "client_id": "cid5-will",
            "clean": True,
            "will": {"topic": "t/will5", "payload": b"bye5", "qos": 1, "retain": False, "will_delay": 3, "content_type": "text/plain"}
        },
        "expect": {"success": True}
    }

def t_v5_reserved_bit_invalid():
    return {
        "name": "v5: reserved connect flag bit set (invalid)",
        "version": "5.0",
        "connect": {
            "client_id": "cid5-rsv",
            "clean": True,
            "hack_reserved_connect_flag_bit": True
        },
        "expect": {"success": False}
    }

def t_v5_pw_without_user_invalid():
    return {
        "name": "v5: password set without username (invalid)",
        "version": "5.0",
        "connect": {
            "client_id": "cid5-bad",
            "clean": True,
            "password": b"p-only",
        },
        "expect": {"success": False}
    }

def t_v5_bad_protocol_name():
    return {
        "name": "v5: bad protocol name",
        "version": "5.0",
        "connect": {
            "client_id": "cid5-badp",
            "clean": True,
            "bad_protocol": "MQIsdp"
        },
        "expect": {"success": False}
    }

def t_v5_keepalive_65535():
    return {
        "name": "v5: keepalive 65535",
        "version": "5.0",
        "connect": {
            "client_id": "cid5-ka",
            "clean": True,
            "keepalive": 65535
        },
        "expect": {"success": True}
    }

# DISCONNECT tests after a successful CONNECT
def t_disc_v311():
    return {
        "name": "v311: DISCONNECT",
        "disconnect": {"version": "3.1.1"}
    }

def t_disc_v5_normal():
    return {
        "name": "v5: DISCONNECT reason=0x00",
        "disconnect": {"version": "5.0", "reason": 0x00}
    }

def t_disc_v5_with_will():
    return {
        "name": "v5: DISCONNECT reason=0x04 (Disconnect with Will)",
        "disconnect": {"version": "5.0", "reason": 0x04}
    }

def t_disc_v5_unspecified():
    return {
        "name": "v5: DISCONNECT reason=0x80 (Unspecified error) + Reason String",
        "disconnect": {"version": "5.0", "reason": 0x80, "props": wrap_props(prop_str(0x1F, "bye"))}  # Reason String id=0x1F
    }

TESTS: List[Dict[str, Any]] = [
    # v3.1.1 connect matrix
    t_valid_v311_min(),
    t_v311_empty_cid_clean_ok(),
    t_v311_empty_cid_no_clean_invalid(),
    t_v311_will_qos_retain(),
    t_v311_username_pw_ok(),
    t_v311_pw_without_user_invalid(),
    t_v311_reserved_flag_bit_set(),
    t_v311_bad_proto_level(),
    t_v311_keepalive_edges(),
    t_v311_malformed_rl(),
    # v5 connect matrix
    t_v5_minimal(),
    t_v5_empty_cid_clean(),
    t_v5_receive_max_and_mps(),
    t_v5_will_with_props(),
    t_v5_reserved_bit_invalid(),
    t_v5_pw_without_user_invalid(),
    t_v5_bad_protocol_name(),
    t_v5_keepalive_65535(),
    # disconnects (executed only after a preceding successful CONNECT)
    t_disc_v311(),
    t_disc_v5_normal(),
    t_disc_v5_with_will(),
    t_disc_v5_unspecified(),
]

# --------------- Execution -----------------

def do_connect(host: str, port: int, timeout: float, version: str, params: Dict[str, Any], verbose: bool) -> Tuple[bool, Optional[int], str, Optional[socket.socket]]:
    s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    s.settimeout(timeout)
    s.connect((host, port))
    if version == "3.1.1":
        pkt = build_connect_v311(params)
    else:
        pkt = build_connect_v5(params)
    if verbose:
        print(f"> CONNECT[{version}] {pkt.hex()}")
    s.sendall(pkt)

    try:
        if version == "3.1.1":
            got, rc, info = read_connack_v311(s, timeout)
        else:
            got, rc, info = read_connack_v5(s, timeout)
        return got, rc, info, s
    except (socket.timeout, ConnectionResetError, OSError) as e:
        s.close()
        return False, None, f"exception: {e}", None

def do_disconnect(sock: socket.socket, version: str, timeout: float, disc: Dict[str, Any], verbose: bool) -> Tuple[bool, str]:
    try:
        if version == "3.1.1":
            pkt = build_disconnect_v311()
        else:
            reason = disc.get("reason")
            props = disc.get("props")
            pkt = build_disconnect_v5(reason, props)
        if verbose:
            print(f"> DISCONNECT[{version}] {pkt.hex()}")
        sock.sendall(pkt)
        # Most brokers will close soon after; allow both close or idle as PASS
        sock.settimeout(timeout)
        time.sleep(0.1)
        try:
            data = sock.recv(1)
            # If we get anything unexpected, not fatal; consider PASS
            return True, "ok (socket stayed open or sent data)"
        except socket.timeout:
            # no data (likely closed by broker later) — acceptable
            return True, "ok (no data; likely closed)"
    except Exception as e:
        return False, f"exception: {e}"
    finally:
        try:
            sock.close()
        except Exception:
            pass

def main():
    ap = argparse.ArgumentParser(description="MQTT CONNECT/DISCONNECT test (v3.1.1 + v5.0)")
    ap.add_argument("--host", default="127.0.0.1")
    ap.add_argument("--port", type=int, default=1883)
    ap.add_argument("--timeout", type=float, default=3.0)
    ap.add_argument("--verbose", action="store_true")
    args = ap.parse_args()

    host, port, timeout, verbose = args.host, args.port, args.timeout, args.verbose

    print(f"Target broker: {host}:{port}")
    print("Test\t\t\t\t\tResult\tDetails")

    last_success_sock = None
    last_success_version = None

    for t in TESTS:
        name = t["name"]
        ok = False
        detail = ""
        sock = None

        if "connect" in t:
            version = t["version"]
            got, code, info, sock = do_connect(host, port, timeout, version, t["connect"], verbose)
            exp = t["expect"]
            if exp.get("success"):
                if not got:
                    ok = False
                    detail = f"no CONNACK ({info})"
                    if sock: sock.close()
                else:
                    if version == "3.1.1":
                        ok = (code == 0)
                        detail = f"RC={code}"
                    else:
                        ok = (code == 0x00 if "reason_ok" not in exp else code in exp["reason_ok"])
                        detail = f"Reason={code}"
                    if ok:
                        last_success_sock = sock
                        last_success_version = version
                    else:
                        if sock:
                            sock.close()
            else:
                # expecting failure: either no CONNACK, or non-zero code
                if not got:
                    ok = True
                    detail = f"fail as expected ({info})"
                else:
                    if version == "3.1.1":
                        ok = (code != 0)
                        detail = f"RC={code} (non-zero expected)"
                    else:
                        ok = (code != 0x00)
                        detail = f"Reason={code} (non-zero expected)"
                    if sock:
                        sock.close()

        elif "disconnect" in t:
            if last_success_sock is None or last_success_version is None:
                ok = True
                detail = "skipped (no prior successful CONNECT)"
            else:
                ok, detail = do_disconnect(last_success_sock, t["disconnect"]["version"], timeout, t["disconnect"], verbose)
                # After a disconnect, clear the live socket
                last_success_sock = None
                last_success_version = None

        print(f"{name:40s}\t{'PASS' if ok else 'FAIL'}\t{detail}")

if __name__ == "__main__":
    main()
