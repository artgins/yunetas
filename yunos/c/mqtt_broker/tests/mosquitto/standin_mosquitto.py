#!/usr/bin/env python3
#
#   Copyright (c) 2026, ArtGins.
#
#   Stand-in for the mosquitto binary of mosquitto's test suite: it runs the
#   yuneta mqtt_broker on the ports the test asks for, translating the
#   mosquitto options that have a yuneta equivalent.
#
#   - One C_TCP_S per listener, each with its OWN pool of C_CHANNEL /
#     C_PROT_MQTT2 (child_tree_filter __prefix_gobj_name__ "p<port>-"), so the
#     per-listener options (allow_anonymous, max_qos, max_connections...) are
#     kept per listener.
#   - Options without equivalent are said on stderr as
#     "STANDIN: unsupported option ..." (the broker log the tests print).
#   - "persistence true": the store is kept across restarts of the same test
#     (keyed by the test process and the persistence file), otherwise a clean
#     store per run.
#
#   run_all.sh installs this file as <work>/root/src/mosquitto (the suite runs
#   $BUILD_ROOT/src/mosquitto), next to <work>/mqtt_broker (the broker under
#   test) and <work>/mqtt_broker.json (the template config, ../mqtt_broker.json).
#
import hashlib, json, os, shutil, signal, subprocess, sys, tempfile

HERE = os.path.dirname(os.path.abspath(__file__))
BASE = os.path.dirname(os.path.dirname(HERE))
BROKER = os.path.join(BASE, 'mqtt_broker')
TEMPLATE = os.path.join(BASE, 'mqtt_broker.json')

POOL_SIZE = 100     # channels per listener when max_connections is not set

#
#   Translation tables
#
# mosquitto option -> (yuneta C_PROT_MQTT2 attr, type)
PROT_GLOBAL = {
    'max_keepalive':          ('max_keepalive', int),
    'message_size_limit':     ('message_size_limit', int),
    'max_inflight_messages':  ('max_inflight_messages', int),
    'max_inflight_bytes':     ('max_inflight_bytes', int),
    'max_queued_messages':    ('max_queued_messages', int),
    'max_queued_bytes':       ('max_queued_bytes', int),
    'max_packet_size':        ('max_packet_size', int),
    'retain_available':       ('retain_available', bool),
}
# listener-scoped in mosquitto (follow the last `listener` line)
PROT_LISTENER = {
    'max_qos':                ('max_qos', int),
    'maximum_qos':            ('max_qos', int),
    'max_topic_alias':        ('max_topic_alias', int),
    'listener_allow_anonymous': ('allow_anonymous', bool),
}
# per_listener_settings-scoped in mosquitto
PROT_PLS = {
    'allow_anonymous':            ('allow_anonymous', bool),
    'allow_zero_length_clientid': ('allow_zero_length_clientid', bool),
}
# mosquitto defaults where the yuneta default differs and the yuneta attr exists
PROT_MOSQ_DEFAULTS = {
    'max_keepalive': 65535,                 # yuneta 60
    'max_subscriptions_per_client': 0,      # yuneta 100, mosquitto no limit
    'max_topic_filter_length': 0,           # yuneta 256, mosquitto 65535
    'max_packet_size': 2000000,             # mosquitto 2.1 default (announced in CONNACK)
}
IGNORED = {     # no behaviour to translate (logging, tuning, files of mosquitto)
    'log_type', 'log_dest', 'log_timestamp', 'log_timestamp_format',
    'connection_messages', 'pid_file', 'user', 'set_tcp_nodelay',
    'websockets_log_level', 'websockets_headers_size', 'memory_limit',
    'autosave_interval', 'autosave_on_changes', 'persistence',
    'persistence_file', 'persistence_location', 'per_listener_settings',
    'listener', 'port', 'socket_domain', 'bind_interface',
}

def as_bool(v):
    return v.lower() in ('true', '1', 'yes', 'on')

def unsupported(msg):
    sys.stderr.write("STANDIN: unsupported option %s\n" % msg)

#
#   Arguments
#
cli_ports = []
conf = None
args = sys.argv[1:]
i = 0
while i < len(args):
    if args[i] == '-p':
        cli_ports.append(int(args[i+1]))
        i += 2
        continue
    if args[i] == '-c':
        conf = args[i+1]
        i += 2
        continue
    if args[i] in ('-v', '-d', '--verbose'):
        i += 1
        continue
    unsupported("command line argument '%s'" % args[i])
    i += 1

#
#   Parse the config file
#
lines = []
def read_conf(path):
    for line in open(path, encoding='utf-8', errors='replace'):
        line = line.rstrip('\r\n')
        w = line.split(None, 1)
        if not w or w[0].startswith('#'):
            continue
        if w[0] == 'include_dir' and len(w) > 1:
            d = w[1].strip()
            if os.path.isdir(d):
                for f in sorted(os.listdir(d)):
                    if f.endswith('.conf'):
                        read_conf(os.path.join(d, f))
            continue
        lines.append((w[0], w[1].strip() if len(w) > 1 else ''))

if conf and os.path.exists(conf):
    read_conf(conf)

per_listener_settings = any(k == 'per_listener_settings' and as_bool(v) for k, v in lines)

listeners = []          # list of dicts: port, kw, opts
glob_prot = {}
glob_pls = {}
persistence = False
persistence_key = ''
persistence_file = 'mosquitto.db'
persistence_location = ''
max_connections_global = None
global_max_connections = None
conf_has_listener = False
cur = None

def new_listener(port):
    l = {'port': port, 'prot': {}, 'pls': {}, 'max_connections': None, 'ok': True}
    listeners.append(l)
    return l

for k, v in lines:
    if k in ('listener', 'port'):
        conf_has_listener = True
        w = v.split()
        if not w or not w[0].isdigit() or int(w[0]) == 0:
            unsupported("'%s %s' (unix socket listener)" % (k, v))
            cur = new_listener(None)
            cur['ok'] = False
            continue
        if len(w) > 1:
            unsupported("listener bind address '%s' (listens on 0.0.0.0)" % w[1])
        port = int(w[0])
        if k == 'port':
            # `port` changes the default listener
            for l in listeners:
                if l['port'] == port:
                    cur = l
                    break
            else:
                cur = new_listener(port)
        else:
            cur = new_listener(port)
        continue
    if k == 'protocol':
        if v.strip() != 'mqtt':
            unsupported("protocol %s" % v)
            if cur is not None:
                cur['ok'] = False
        continue
    if k in PROT_GLOBAL:
        a, t = PROT_GLOBAL[k]
        glob_prot[a] = as_bool(v) if t is bool else int(v)
        continue
    if k in PROT_LISTENER:
        a, t = PROT_LISTENER[k]
        val = as_bool(v) if t is bool else int(v)
        if cur is None:
            glob_prot[a] = val
        else:
            cur['prot'][a] = val
        continue
    if k in PROT_PLS:
        a, t = PROT_PLS[k]
        val = as_bool(v) if t is bool else int(v)
        if per_listener_settings and cur is not None:
            cur['pls'][a] = val
        else:
            glob_pls[a] = val
        continue
    if k == 'max_connections':
        n = int(v)
        if cur is None:
            max_connections_global = n
        else:
            cur['max_connections'] = n
        continue
    if k == 'global_max_connections':
        global_max_connections = int(v)
        continue
    if k == 'persistence':
        persistence = as_bool(v)
        continue
    if k in ('persistence_file', 'persistence_location'):
        persistence_key += '|' + v
        if k == 'persistence_file':
            persistence_file = v
        else:
            persistence_location = v
        continue
    if k == 'auto_id_prefix' and v == 'auto-':
        continue
    if k in IGNORED:
        continue
    unsupported("'%s %s'" % (k, v))

for p in cli_ports:
    if not any(l['port'] == p for l in listeners):
        new_listener(p)
listeners = [l for l in listeners if l['ok']]
if not listeners and not conf_has_listener:
    new_listener(1883)

# mosquitto 2: anonymous allowed by default only when the config file
# defines no listener (local only mode, -p or the default port)
default_anon = not conf_has_listener

#
#   Build the yuneta config
#
if persistence:
    key = hashlib.sha1(("%d%s%s" % (os.getppid(), persistence_key, conf or '')).encode()).hexdigest()[:12]
    work = os.path.join(BASE, 'runs', 'ybroker-persist-' + key)
    # A test starts clean by deleting its persistence file: no file = a fresh
    # store. It is created here, so a restart inside the same test keeps the
    # store (the yuneta broker keeps it in its own directory, not in the file)
    pfile = os.path.join(persistence_location, persistence_file)
    if not os.path.exists(pfile):
        shutil.rmtree(work, ignore_errors=True)
        open(pfile, 'a').close()
    os.makedirs(work, exist_ok=True)
else:
    work = tempfile.mkdtemp(prefix='ybroker-', dir=os.path.join(BASE, 'runs'))

c = json.load(open(TEMPLATE))
c['environment']['work_dir'] = work
for h in c['environment'].get('console_log_handlers', {}).values():
    h['handler_options'] = 255
c['global']['Authz.allow_anonymous_in_localhost'] = False

def channel_pool(prefix, size, prot):
    pool = []
    for n in range(1, size + 1):
        name = "%s%d" % (prefix, n)
        pool.append(
            {"name": name, "gclass": "C_CHANNEL",
             "children": [
                 {"name": name, "gclass": "C_PROT_MQTT2", "kw": dict(prot),
                  "children": [{"gclass": "C_TCP"}]}
             ]}
        )
    return pool

children = []
shared_pool = None
for l in listeners:
    port = l['port']
    prot = {"iamServer": True}
    prot.update(PROT_MOSQ_DEFAULTS)
    prot.update(glob_prot)
    prot['allow_anonymous'] = default_anon
    prot.update(glob_pls)
    prot.update(l['pls'])
    prot.update(l['prot'])
    size = l['max_connections'] if l['max_connections'] is not None else max_connections_global
    if size is None or size < 0:
        size = POOL_SIZE
    if global_max_connections is not None and global_max_connections > 0:
        # one pool shared by every listener: the per-listener options of
        # the first listener apply to all
        prefix = "g-"
        if shared_pool is None:
            shared_pool = channel_pool(prefix, global_max_connections, prot)
            if len(listeners) > 1:
                unsupported("global_max_connections with several listeners: one pool, options of the first listener")
    else:
        prefix = "p%d-" % port
    children.append(
        {"name": "server_port_%d" % port, "gclass": "C_TCP_S",
         "kw": {"url": "mqtt://0.0.0.0:%d" % port, "backlog": 100, "use_dups": 0,
                "child_tree_filter": {"kw": {"__gclass_name__": "C_CHANNEL",
                                             "__prefix_gobj_name__": prefix,
                                             "__disabled__": False, "connected": False}}}}
    )
    if prefix != "g-":
        children.extend(channel_pool(prefix, size, prot))
    sys.stderr.write("STANDIN: listener %d pool %s prot %s\n" % (
        port, global_max_connections if prefix == "g-" else size, json.dumps(prot)))
if shared_pool:
    children.extend(shared_pool)

for s in c['services']:
    if s['name'] == '__input_side__':
        s['children'] = children
        s.pop('[^^children^^]', None)

if os.environ.get('YBROKER_TRACE'):
    # debugging aid: gclass trace levels, e.g. {"C_PROT_MQTT2": ["show-decode"]}
    c['yuno']['trace_levels'] = json.loads(os.environ['YBROKER_TRACE'])

cfg = os.path.join(work, 'mqtt_broker.json')
json.dump(c, open(cfg, 'w'), indent=4)

extra = os.environ.get('YBROKER_ARGS', '').split()
proc = subprocess.Popen([BROKER, '--config-file=' + cfg] + extra, stdout=sys.stderr, stderr=sys.stderr)

def stop(signum, frame):
    if proc.poll() is None:
        proc.send_signal(signal.SIGINT)
signal.signal(signal.SIGTERM, stop)
signal.signal(signal.SIGINT, stop)
signal.signal(signal.SIGHUP, signal.SIG_IGN)

rc = proc.wait()
if os.environ.get('YBROKER_KEEP') is None and not persistence:
    shutil.rmtree(work, ignore_errors=True)
sys.exit(0 if rc in (0, -signal.SIGINT) else rc)
