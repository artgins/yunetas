#!/usr/bin/env python3
#
#   make_report.py -- the performance report of a Yuneta release
#
#   Reads performance/reports/<version>.json (schema "yuneta-perf-report/1",
#   documented in performance/reports/README.md) and writes:
#
#       python3 make_report.py 7.25.5       -> performance/reports/7.25.5.html
#       python3 make_report.py --docs       -> docs/doc.yuneta.io/_static/perf/*.svg
#
#   The .html is self-contained: inline CSS, inline SVG, no script, no font
#   or image from outside. It follows the reader's light/dark setting.
#   The docs charts are standalone SVG files with their own background, so
#   they read the same in the light and the dark theme of the site.
#
#   Copyright (c) 2026, ArtGins.
#
import glob
import html
import json
import math
import os
import re
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
REPO = os.path.abspath(os.path.join(HERE, '..', '..'))
DOCS_SVG = os.path.join(REPO, 'docs', 'doc.yuneta.io', '_static', 'perf')
GITHUB = 'https://github.com/artgins/yunetas'


#
#   Helpers
#
def esc(s):
    return html.escape(str(s), quote=True)


def md_code(s):
    # `x` -> <code>x</code>, the only markup the json texts carry
    return re.sub(r'`([^`]+)`', r'<code>\1</code>', esc(s))


def vkey(v):
    return tuple(int(x) for x in re.findall(r'\d+', v))


def load_reports():
    reports = {}
    for p in glob.glob(os.path.join(HERE, '*.json')):
        with open(p) as f:
            d = json.load(f)
        if d.get('schema', '').startswith('yuneta-perf-report/'):
            reports[d['version']] = d
    return dict(sorted(reports.items(), key=lambda kv: vkey(kv[0])))


def fmt(v, unit=None):
    if v is None:
        return '--'
    a = abs(v)
    if a >= 100000:
        s = '{:,.0f}'.format(v)
    elif a >= 1000:
        s = '{:,.0f}'.format(v)
    elif a >= 100:
        s = '{:.1f}'.format(v)
    elif a >= 10:
        s = '{:.2f}'.format(v)
    elif a >= 1:
        s = '{:.2f}'.format(v)
    else:
        s = '{:.3f}'.format(v)
    if '.' in s and a >= 100:
        s = s.rstrip('0').rstrip('.')
    if unit == 'ns':
        s = '{:,.0f}'.format(v)
    if unit:
        s += ' ' + UNIT.get(unit, unit)
    return s


UNIT = {'us': '\u00b5s'}


def fmt_rate(v):
    if v >= 1e6:
        return '{:.2f} M'.format(v / 1e6)
    if v >= 1e3:
        return '{:.0f} K'.format(v / 1e3) if v >= 1e5 else '{:.1f} K'.format(v / 1e3)
    return '{:.0f}'.format(v)


def pct(v):
    return ('+' if v > 0 else '') + '{:.1f}%'.format(v)


def time_change(e):
    # change of the time an operation takes (negative = faster), for any unit
    p = e['previous']['mean']
    c = e['current']['mean']
    if e['better'] == 'higher':
        return (p / c - 1.0) * 100.0
    return (c / p - 1.0) * 100.0


def time_change_sd(e):
    # one standard deviation of that change, from the two relative spreads
    p = e['previous']
    c = e['current']
    if p.get('sd') is None or c.get('sd') is None:
        return None
    r = math.sqrt((p['sd'] / p['mean']) ** 2 + (c['sd'] / c['mean']) ** 2)
    ratio = time_change(e) / 100.0 + 1.0
    return ratio * r * 100.0


def ratio_text(e):
    p = e['previous']['mean']
    c = e['current']['mean']
    if e['better'] == 'higher':
        t = p / c
    else:
        t = c / p
    if t >= 2:
        return 'x{:.0f} slower'.format(t) if t >= 10 else 'x{:.1f} slower'.format(t)
    if t <= 0.5:
        return 'x{:.0f} faster'.format(1 / t) if 1 / t >= 10 else 'x{:.1f} faster'.format(1 / t)
    return pct(time_change(e))


VERDICT = {
    'gain': ('faster', 'v-gain'),
    'noise': ('within noise', 'v-noise'),
    'price': ('price paid on purpose', 'v-price'),
    'price-until-migrated': ('price until migrated', 'v-price'),
}


def ab_by_id(rep):
    return {e['id']: e for e in rep.get('ab', [])}


def fig_by_id(rep):
    return {e['id']: e for e in rep.get('figures', [])}


#
#   CSS of the html report
#
CSS = r"""
:root {
  color-scheme: light;
  --page: #f9f9f7;
  --surface: #fcfcfb;
  --card: #ffffff;
  --ink: #0b0b0b;
  --ink-2: #52514e;
  --muted: #6f6d68;
  --grid: #e1e0d9;
  --axis: #c3c2b7;
  --ring: rgba(11,11,11,0.10);
  --cur: #2a78d6;
  --prev: #9b9991;
  --gain: #2a78d6;
  --price: #e34948;
  --noise: #b9b7ae;
  --accent-ink: #1c5cab;
  --price-ink: #b3302f;
  --wash: #eef4fc;
}
@media (prefers-color-scheme: dark) {
  :root:where(:not([data-theme="light"])) {
    color-scheme: dark;
    --page: #0d0d0d;
    --surface: #1a1a19;
    --card: #1f1f1e;
    --ink: #ffffff;
    --ink-2: #c3c2b7;
    --muted: #a3a198;
    --grid: #2c2c2a;
    --axis: #4a4a46;
    --ring: rgba(255,255,255,0.10);
    --cur: #3987e5;
    --prev: #7a786f;
    --gain: #3987e5;
    --price: #e66767;
    --noise: #5c5b56;
    --accent-ink: #86b6ef;
    --price-ink: #f08a8a;
    --wash: #16263a;
  }
}
:root[data-theme="dark"] {
  color-scheme: dark;
  --page: #0d0d0d; --surface: #1a1a19; --card: #1f1f1e; --ink: #ffffff;
  --ink-2: #c3c2b7; --muted: #a3a198; --grid: #2c2c2a; --axis: #4a4a46;
  --ring: rgba(255,255,255,0.10); --cur: #3987e5; --prev: #7a786f;
  --gain: #3987e5; --price: #e66767; --noise: #5c5b56; --accent-ink: #86b6ef;
  --price-ink: #f08a8a; --wash: #16263a;
}
* { box-sizing: border-box; }
html { -webkit-text-size-adjust: 100%; }
body {
  margin: 0; background: var(--page); color: var(--ink);
  font: 16px/1.55 system-ui, -apple-system, "Segoe UI", Roboto, sans-serif;
}
main { max-width: 1040px; margin: 0 auto; padding: 32px 16px 64px; }
a { color: var(--accent-ink); }
code { font: 0.88em ui-monospace, SFMono-Regular, Menlo, Consolas, monospace; }
h1 { font-size: 2rem; line-height: 1.2; margin: 0 0 8px; letter-spacing: -0.01em; }
h2 { font-size: 1.35rem; margin: 48px 0 6px; }
h3 { font-size: 1.05rem; margin: 22px 0 6px; }
p.lead { font-size: 1.1rem; color: var(--ink-2); margin: 0 0 20px; max-width: 46rem; }
p.sub { color: var(--ink-2); margin: 0 0 18px; max-width: 48rem; }
.small { font-size: 0.85rem; color: var(--muted); }
.card {
  background: var(--card); border: 1px solid var(--ring); border-radius: 12px;
  padding: 18px 18px 14px; margin: 14px 0;
}
.machine { font-size: 0.9rem; color: var(--ink-2); }
.machine b { color: var(--ink); font-weight: 600; }

/* hero */
.hero { display: grid; grid-template-columns: minmax(0, 1.2fr) minmax(0, 2fr); gap: 16px; align-items: stretch; }
.hero .big { background: var(--wash); border-radius: 12px; padding: 22px; }
.hero .big .v { font-size: 3.6rem; font-weight: 650; line-height: 1; color: var(--ink); }
.hero .big .l { margin-top: 10px; color: var(--ink-2); }
.tiles { display: grid; grid-template-columns: repeat(auto-fill, minmax(200px, 1fr)); gap: 12px; }
.tile { background: var(--card); border: 1px solid var(--ring); border-radius: 12px; padding: 14px 16px; }
.tile .l { font-size: 0.85rem; color: var(--ink-2); }
.tile .v { font-size: 1.7rem; font-weight: 650; line-height: 1.2; margin: 4px 0 2px; }
.tile .d { font-size: 0.85rem; color: var(--muted); }
.tile .d .up { color: var(--accent-ink); font-weight: 600; }
.tile .d .down { color: var(--price-ink); font-weight: 600; }

/* bar rows: label | track | value */
.chart { margin: 8px 0 4px; }
.row { display: grid; grid-template-columns: minmax(0, 17rem) minmax(0, 1fr) 7.5rem; gap: 10px; align-items: center; min-height: 26px; }
.row .lab { font-size: 0.88rem; color: var(--ink); overflow-wrap: anywhere; }
.row .lab .m { color: var(--muted); font-size: 0.8rem; }
.row .val { font-size: 0.85rem; color: var(--ink-2); text-align: right; font-variant-numeric: tabular-nums; white-space: nowrap; }
.row svg { display: block; width: 100%; height: 22px; overflow: visible; }
.grp { font-size: 0.78rem; font-weight: 650; text-transform: uppercase; letter-spacing: 0.05em; color: var(--muted); margin: 14px 0 2px; }
.axisrow .val, .axisrow .lab { visibility: hidden; }
.axisrow svg { height: 18px; }
.axisrow text { fill: var(--muted); font-size: 10.5px; }
.legend { display: flex; flex-wrap: wrap; gap: 6px 18px; font-size: 0.85rem; color: var(--ink-2); margin: 4px 0 8px; }
.legend i { display: inline-block; width: 12px; height: 12px; border-radius: 3px; margin-right: 6px; vertical-align: -1px; }
.sw-cur { background: var(--cur); } .sw-prev { background: var(--prev); }
.sw-gain { background: var(--gain); } .sw-price { background: var(--price); } .sw-noise { background: var(--noise); }
.bar-cur { fill: var(--cur); } .bar-prev { fill: var(--prev); }
.bar-gain { fill: var(--gain); } .bar-price { fill: var(--price); } .bar-noise { fill: var(--noise); }
.whisk { stroke: var(--ink-2); stroke-width: 1.5; }
.zero { stroke: var(--axis); stroke-width: 1; }
.gridl { stroke: var(--grid); stroke-width: 1; }
.clip { fill: var(--ink-2); font-size: 11px; font-weight: 600; }
.v-price { color: var(--price-ink); } .v-gain { color: var(--accent-ink); }

/* small multiples */
.multi { display: grid; grid-template-columns: repeat(auto-fill, minmax(300px, 1fr)); gap: 14px; }
.multi .card { margin: 0; }
.multi h3 { margin: 0 0 6px; font-size: 0.98rem; }
.multi .row { grid-template-columns: 6.6rem minmax(0, 1fr) 5.6rem; }
.multi p { font-size: 0.86rem; color: var(--ink-2); margin: 8px 0 0; }
.multi .ch { font-size: 0.85rem; font-weight: 650; }

table { border-collapse: collapse; width: 100%; font-size: 0.85rem; }
th, td { text-align: left; padding: 6px 8px; border-bottom: 1px solid var(--grid); vertical-align: top; }
th { color: var(--ink-2); font-weight: 600; }
td.n { text-align: right; font-variant-numeric: tabular-nums; white-space: nowrap; }
.tablewrap { overflow-x: auto; }
details { margin: 12px 0; }
summary { cursor: pointer; font-weight: 600; }
footer { margin-top: 48px; font-size: 0.85rem; color: var(--muted); }

@media (max-width: 720px) {
  .hero { grid-template-columns: minmax(0, 1fr); }
  .row { grid-template-columns: minmax(0, 1fr) 6.5rem; row-gap: 0; margin-top: 10px; }
  .axisrow { margin-top: 0; }
  .row .lab { grid-column: 1 / -1; }
  .row svg { grid-column: 1; }
  .axisrow .lab { display: none; }
  h1 { font-size: 1.6rem; }
  .hero .big .v { font-size: 2.8rem; }
}
@media (forced-colors: active) {
  .bar-cur, .bar-gain { fill: Highlight; } .bar-prev, .bar-noise { fill: GrayText; } .bar-price { fill: Mark; }
}
"""


#
#   Building blocks
#
def svg_track(content, label):
    return ('<svg role="img" aria-label="%s" preserveAspectRatio="none">%s</svg>'
            % (esc(label), content))


def xpos(v, lo, hi):
    return max(0.0, min(100.0, (v - lo) / (hi - lo) * 100.0))


def diverging_chart(entries, lo=-100.0, hi=40.0):
    """
    One row per entry: the change of the time an operation takes, 7.25.x against
    the release before, colored by verdict, with a whisker of +-1 sd.
    """
    ticks = [t for t in range(int(lo), int(hi) + 1, 25)]
    out = ['<div class="chart">']
    out.append('<div class="legend">'
               '<span><i class="sw-gain"></i>faster</span>'
               '<span><i class="sw-noise"></i>within noise</span>'
               '<span><i class="sw-price"></i>slower: a price paid on purpose</span>'
               '<span>whisker: &plusmn;1 standard deviation of the change</span></div>')
    group = None
    for e in entries:
        if e['group'] != group:
            group = e['group']
            out.append('<div class="grp">%s</div>' % esc(group))
        ch = time_change(e)
        sd = time_change_sd(e)
        cls = {'gain': 'bar-gain', 'noise': 'bar-noise'}.get(e['verdict'], 'bar-price')
        z = xpos(0, lo, hi)
        x = xpos(ch, lo, hi)
        a, b = min(z, x), max(z, x)
        parts = []
        for t in ticks:
            parts.append('<line class="gridl" x1="%.2f%%" x2="%.2f%%" y1="0" y2="22"/>' % (xpos(t, lo, hi), xpos(t, lo, hi)))
        tip = '%s: %s -> %s %s (%s, n = %s)' % (
            e['label'], fmt(e['previous']['mean']), fmt(e['current']['mean']), e['unit'],
            ratio_text(e), e['n'])
        parts.append('<rect class="%s" x="%.2f%%" y="6" width="%.2f%%" height="10" rx="2"><title>%s</title></rect>'
                     % (cls, a, max(b - a, 0.4), esc(tip)))
        if sd is not None and ch <= hi:
            w1 = xpos(ch - sd, lo, hi)
            w2 = xpos(ch + sd, lo, hi)
            parts.append('<line class="whisk" x1="%.2f%%" x2="%.2f%%" y1="11" y2="11"/>' % (w1, w2))
            parts.append('<line class="whisk" x1="%.2f%%" x2="%.2f%%" y1="7" y2="15"/>' % (w1, w1))
            parts.append('<line class="whisk" x1="%.2f%%" x2="%.2f%%" y1="7" y2="15"/>' % (w2, w2))
        parts.append('<line class="zero" x1="%.2f%%" x2="%.2f%%" y1="0" y2="22"/>' % (z, z))
        mark = ' <span class="m">&dagger;</span>' if 'separate' in e.get('scope', '') else ''
        out.append('<div class="row"><div class="lab">%s%s</div>%s<div class="val %s">%s</div></div>' % (
            esc(e['label']), mark, svg_track(''.join(parts), tip), VERDICT[e['verdict']][1], esc(ratio_text(e))))
    # axis
    parts = []
    for t in ticks:
        anchor = 'middle'
        x = xpos(t, lo, hi)
        if x < 3:
            anchor = 'start'
        parts.append('<text x="%.2f%%" y="12" text-anchor="%s">%s</text>' % (x, anchor, ('+' if t > 0 else '') + '%d%%' % t))
    out.append('<div class="row axisrow"><div class="lab">.</div><svg role="presentation" aria-hidden="true">%s</svg><div class="val">.</div></div>'
               % ''.join(parts))
    out.append('<p class="small">Change of the time one operation takes (a throughput is turned into '
               'time per operation), 7.25.5 against 7.25.4. Left of zero is faster. '
               'Bars past +40% are cut; the right column gives the real factor. '
               '&dagger; release against release from two separate runs, not one alternated run.</p>')
    out.append('</div>')
    return '\n'.join(out)


def paired_card(title, rows, unit, reason, change=None, better='lower'):
    """
    rows: [(label, value, css)]; one scale per card, starting at zero.
    """
    hi = max(v for _, v, _ in rows) * 1.02
    out = ['<div class="card"><h3>%s</h3>' % esc(title)]
    if change:
        out.append('<div class="ch v-price">%s</div>' % esc(change))
    for lab, v, css in rows:
        w = xpos(v, 0, hi)
        tip = '%s: %s %s' % (lab, fmt(v), unit)
        out.append('<div class="row"><div class="lab">%s</div>%s<div class="val">%s</div></div>' % (
            esc(lab),
            svg_track('<rect class="%s" x="0" y="5" width="%.2f%%" height="12" rx="3"><title>%s</title></rect>'
                      % (css, max(w, 0.5), esc(tip)), tip),
            esc(fmt(v, unit))))
    out.append('<p>%s</p></div>' % md_code(reason))
    return '\n'.join(out)


def hbar_chart(rows, unit_note, log=False, lo=None, hi=None, css='bar-cur'):
    """
    rows: [(label, sublabel, value, value_text, tip)]
    """
    vals = [r[2] for r in rows]
    if log:
        lo = lo or 10 ** math.floor(math.log10(min(vals)))
        hi = hi or 10 ** math.ceil(math.log10(max(vals)))
        f = lambda v: xpos(math.log10(v), math.log10(lo), math.log10(hi))
        ticks = [10 ** k for k in range(int(math.log10(lo)), int(math.log10(hi)) + 1)]
    else:
        lo = 0
        hi = hi or max(vals) * 1.05
        f = lambda v: xpos(v, lo, hi)
        step = 10 ** math.floor(math.log10(hi))
        if hi / step < 3:
            step /= 2
        ticks = [k * step for k in range(0, int(hi / step) + 1)]
    out = ['<div class="chart">']
    for lab, sub, v, vt, tip in rows:
        parts = ['<line class="gridl" x1="%.2f%%" x2="%.2f%%" y1="0" y2="22"/>' % (f(t), f(t)) for t in ticks]
        parts.append('<rect class="%s" x="0" y="5" width="%.2f%%" height="12" rx="3"><title>%s</title></rect>'
                     % (css, max(f(v), 0.5), esc(tip)))
        out.append('<div class="row"><div class="lab">%s%s</div>%s<div class="val">%s</div></div>' % (
            esc(lab), (' <span class="m">%s</span>' % esc(sub)) if sub else '',
            svg_track(''.join(parts), tip), esc(vt)))
    parts = []
    for t in ticks:
        x = f(t)
        anchor = 'start' if x < 2 else ('end' if x > 98 else 'middle')
        parts.append('<text x="%.2f%%" y="12" text-anchor="%s">%s</text>' % (x, anchor, esc(tick_text(t, log))))
    out.append('<div class="row axisrow"><div class="lab">.</div><svg role="presentation" aria-hidden="true">%s</svg><div class="val">.</div></div>'
               % ''.join(parts))
    out.append('<p class="small">%s</p></div>' % unit_note)
    return '\n'.join(out)


def tick_text(t, log):
    if t >= 1e6:
        return '%gM' % (t / 1e6)
    if t >= 1000:
        return '%gk' % (t / 1000.0)
    return '%g' % t


def tile(label, value, delta_html, note=''):
    return ('<div class="tile"><div class="l">%s</div><div class="v">%s</div><div class="d">%s%s</div></div>'
            % (esc(label), esc(value), delta_html, (' ' + esc(note)) if note else ''))


def mean_sd(st):
    if st.get('sd'):
        return '%s &plusmn; %s' % (esc(fmt(st['mean'])), esc(fmt(st['sd'])))
    return esc(fmt(st['mean']))


#
#   The html report
#
def build_html(rep, reports):
    version = rep['version']
    prev = rep.get('previous', '')
    AB = ab_by_id(rep)
    FG = fig_by_id(rep)
    m = rep['machine']
    b = rep['build']

    def ab_get(i):
        return AB.get(i)

    def fg(i):
        return FG.get(i)

    machine_line = '%s, %d cores / %d threads (up to %.1f GHz), %s RAM, %s (%s), %s, %s, glibc %s, %s' % (
        m['cpu'], m['cores'], m['threads'], m['cpu_max_ghz'], m['ram'], m['disk'], m['filesystem'],
        m['kernel'], m['distro'], m['glibc'], b['compiler'])

    H = []
    H.append('<!DOCTYPE html>\n<html lang="en">\n<head>\n<meta charset="utf-8">\n'
             '<meta name="viewport" content="width=device-width, initial-scale=1">\n'
             '<title>Yuneta %s performance</title>\n'
             '<meta name="description" content="Performance report of Yuneta %s against %s: '
             'timeranger2, treedb, the event loop, TLS, the agent audit and binary sizes, measured on one machine.">\n'
             '<style>%s</style>\n</head>\n<body>\n<main>\n' % (esc(version), esc(version), esc(prev), CSS))

    # ---- header
    H.append('<p class="small">Yuneta Simplified &middot; performance report &middot; %s</p>' % esc(rep['date']))
    H.append('<h1>Yuneta %s: faster where it runs every second, and every price named</h1>' % esc(version))
    H.append('<p class="lead">%s is a correctness release: crash safety of the stores and the schemas, '
             'a hardened agent audit, a stricter message bus. This page shows what that did to speed, '
             'measured against %s on the same machine, with the same benchmarks, the two releases run '
             'alternately. The hot paths got faster or stayed where they were. The few operations that got '
             'slower are listed with the reason, because each one buys a guarantee.</p>' % (esc(version), esc(prev)))

    # ---- hero
    aud = ab_get('rotatory.audit_record')
    hero_v = '%.0fx' % (aud['previous']['mean'] / aud['current']['mean']) if aud else ''
    tiles = []

    def tile_ab(i, label, invert=False):
        e = ab_get(i)
        if not e:
            return
        ch = time_change(e)
        cls = 'up' if ch < 0 else 'down'
        word = 'faster' if ch < 0 else 'slower'
        amount = pct(abs(ch)).lstrip('+')
        word = 'less time' if ch < 0 else 'more time'
        if ch <= -33.4:
            amount = '%.1fx' % (100.0 / (100.0 + ch))
            word = 'faster'
        tiles.append(tile(label, fmt(e['current']['mean'], e['unit']),
                          '<span class="%s">%s %s</span> than %s (%s)' % (
                              cls, esc(amount), word, esc(prev), esc(fmt(e['previous']['mean'], e['unit'])))))

    tile_ab('tr2.open_master', 'Open a large store (20 000 files)')
    tile_ab('treedb.update_memory', 'treedb: update a node in memory')
    tile_ab('treedb.update_saved', 'treedb: update a node and save it')
    tile_ab('ctreedb.same_literal_release', 'Start 40 treedbs, schema unchanged')
    tile_ab('tr2.tm_query', 'Query one minute of a key (migrated)')
    e = ab_get('tr2.append_rate')
    if e:
        tiles.append(tile('Appends per second', fmt(e['current']['mean']),
                          'same as %s (%s), with four new checks per append' % (esc(prev), esc(fmt(e['previous']['mean'])))))
    H.append('<section class="hero">')
    if aud:
        H.append('<div class="big"><div class="v">%s</div><div class="l">cheaper: one agent audit record costs '
                 '<b>%s ns</b>, down from %s ns. Every command sent to the agent is audited, so this is paid on every '
                 'command. The agent now also flushes each record, and still pays less than %s did without '
                 'flushing.</div></div>' % (esc(hero_v), esc(fmt(aud['current']['mean'])), esc(fmt(aud['previous']['mean'])), esc(prev)))
    H.append('<div class="tiles">%s</div></section>' % ''.join(tiles))

    H.append('<div class="card machine"><b>Measured on</b> %s. Build: %s, %s, fully static, TLS %s. '
             '%s</div>' % (esc(machine_line), esc(b['build_type']), esc(b['compiler']), esc(b['tls']), esc(b['note'])))

    # ---- absolute figures
    H.append('<h2>What one machine does with %s</h2>' % esc(version))
    H.append('<p class="sub">Absolute figures of the %s build, measured at tag time on the laptop above: '
             'one process, one core for the event loop (Yuneta scales by running one yuno per core). '
             'Mean of the rounds; the spread and the rounds are in the table at the end.</p>' % esc(version))
    t2 = []

    def tile_fig(i, label, rate=False, unit=None, note=''):
        f = fg(i)
        if not f:
            return
        mean = f['mean'] * (1000.0 if f['unit'] == 'K msg/s' else 1.0)
        if rate:
            v = (fmt_rate(mean) + ' ' + unit).strip()
        else:
            v = fmt(mean, unit or f['unit'])
        per_s = ''
        if f['unit'] in ('us', 'ns') and not rate:
            ops = (1e6 if f['unit'] == 'us' else 1e9) / mean
            per_s = '&asymp; %s per second' % esc(fmt_rate(ops))
        t2.append(tile(label, v, per_s, note))

    tile_fig('tr2.append_rate', 'Records stored per second', rate=True, unit='', note='one key-indexed topic, 180 000 appends')
    tile_fig('tr2.read_rate_iterator', 'Records read per second', rate=True, unit='', note='from disk, parsed, one callback each')
    tile_fig('tr2.read_rate_pages', 'Records read per second, page by page', rate=True, unit='', note='as a GUI pages a history')
    tile_fig('treedb.update_saved', 'treedb: update + save a node')
    tile_fig('treedb.link_unlink', 'treedb: link or unlink two nodes')
    tile_fig('net.yev_ping_pong', 'Event loop echoes per second, 1 KB', rate=True, unit='', note='io_uring, no gobj layer')
    tile_fig('net.tls_echo', 'TLS round trips per second', rate=True, unit='', note='full gobj stack, OpenSSL')
    tile_fig('rotatory.log_record', 'One log line (300 bytes)')
    H.append('<div class="tiles">%s</div>' % ''.join(t2))

    # throughput through the stack
    rows = []
    for i, lab, sub in [
        ('tr2.append_rate', 'timeranger2 appends', 'records stored'),
        ('tr2.append_rate_rt', 'appends with a live reader', ''),
        ('tr2.read_rate_iterator', 'timeranger2 reads', 'records loaded'),
        ('tr2.read_rate_pages', 'reads, page by page', ''),
        ('net.yev_ping_pong', 'io_uring event loop alone', '1 KB echo'),
        ('net.yev_ping_pong_persist', 'event loop + timeranger2 append', 'every message stored'),
        ('net.tcp_echo', 'full gobj stack, TCP', 'JSON message round trip'),
        ('net.tcp_echo_persist', 'gobj stack, TCP + append', ''),
        ('net.tls_echo', 'gobj stack, TLS (OpenSSL)', ''),
        ('net.tls_echo_persist', 'gobj stack, TLS + append', ''),
        ('net.auth_bff_login', 'OAuth2 BFF logins', 'HTTP, 5 clients'),
    ]:
        f = fg(i)
        if not f:
            continue
        v = f['mean'] * (1000.0 if f['unit'] == 'K msg/s' else 1.0)
        rows.append((lab, sub, v, fmt_rate(v) + '/s',
                     '%s: %s per second (mean of %d runs, sd %s)' % (lab, fmt_rate(v), f['n'], fmt(f['sd']))))
    if rows:
        H.append('<h3>Operations per second on one core</h3>')
        H.append(hbar_chart(rows, 'As each benchmark counts them: a record stored or read, a message '
                                  'echoed, a round trip, a login. The gobj stack pays for what the bare loop '
                                  'does not do: framing, JSON, the FSM, the routing to a channel.'))

    # cost of one operation
    rows = []
    for i, lab in [
        ('rotatory.log_record', 'log line (300 B)'),
        ('rotatory.audit_record', 'agent audit record'),
        ('rotatory.audit_record_flush', 'audit record, flushed'),
        ('treedb.update_memory', 'treedb update, in memory'),
        ('treedb.update_saved', 'treedb update, saved'),
        ('treedb.link_unlink', 'treedb link / unlink'),
        ('treedb.delete_force', 'treedb forced delete'),
        ('treedb.create_link_half', 'treedb create'),
        ('treedb.delete_parent', 'delete a parent of 200 children'),
    ]:
        f = fg(i)
        if not f:
            continue
        us = f['mean'] / 1000.0 if f['unit'] == 'ns' else f['mean']
        rows.append((lab, '', us, fmt(us, 'us'),
                     '%s: %s us (%s per second), mean of %d runs' % (lab, fmt(us), fmt_rate(1e6 / us), f['n'])))
    if rows:
        H.append('<h3>What one operation costs</h3>')
        H.append(hbar_chart(rows, 'Microseconds per operation, logarithmic scale: each grid step is ten times '
                                  'the one before. treedb figures are CPU time of the process; the log and audit '
                                  'figures are wall time per record.', log=True))

    # ---- A/B
    H.append('<h2>%s against %s</h2>' % (esc(version), esc(prev)))
    H.append('<p class="sub">Each benchmark linked twice, with the module of each release, and the two '
             'binaries run alternately (8 to 80 rounds, see the table). A change inside its whisker is noise. '
             'Everything is on one axis: the change of the time an operation takes.</p>')
    order = [
        'tr2.append_rate', 'tr2.append_rate_rt', 'tr2.build_appends', 'tr2.tm_build_appends', 'tr2.open_master',
        'tr2.tm_query', 'tr2.open_replica', 'tr2.create_topic', 'tr2.topic_version_change', 'tr2.tm_query_unmigrated',
        'treedb.update_memory', 'treedb.update_saved', 'treedb.link_unlink', 'treedb.create_link_half',
        'treedb.reopen', 'treedb.delete_force', 'treedb.delete_parent',
        'ctreedb.same_literal_release', 'ctreedb.same_literal', 'ctreedb.seed', 'ctreedb.newer_literal',
        'rotatory.audit_record', 'rotatory.audit_record_flush',
        'net.yev_ping_pong', 'net.tcp_echo', 'net.tcp_echo_persist',
        'publish.250b.plain.1', 'publish.250b.plain.100', 'publish.20kb.plain.100',
        'publish.250b.global.100', 'publish.20kb.global.100',
    ]
    H.append('<div class="card">%s</div>' % diverging_chart([AB[i] for i in order if i in AB]))

    # ---- prices
    H.append('<h2>Prices paid on purpose</h2>')
    H.append('<p class="sub">What got slower, how much, and what it buys. None of these is on the path of '
             'an ordinary append, read or update: they are one-time costs (a new topic, a schema change, the '
             'first start of a treedb), a migration step, or the cost of a guarantee a remote peer relies on.</p>')
    cards = []

    def price_card(i, title, extra_rows=None, prev_label=None, cur_label=None):
        e = ab_get(i)
        if not e:
            return
        rows_ = [(prev_label or prev, e['previous']['mean'], 'bar-prev'),
                 (cur_label or version, e['current']['mean'], 'bar-cur')]
        if extra_rows:
            rows_ += extra_rows
        cards.append(paired_card(title, rows_, e['unit'], e.get('reason', ''), ratio_text(e)))

    price_card('tr2.create_topic', 'Create 10 topics (timeranger2)')
    price_card('tr2.topic_version_change', 'Change the topic_version of 10 topics')
    price_card('tr2.open_replica', 'Open a store as a replica')
    mig = ab_get('tr2.tm_query')
    price_card('tr2.tm_query_unmigrated', 'A tm query before and after mark-tm-order',
               extra_rows=[('migrated', mig['current']['mean'], 'bar-cur')] if mig else None,
               cur_label='not migrated')
    price_card('ctreedb.seed', 'First open of 40 treedbs (C_TREEDB)')
    price_card('ctreedb.newer_literal', 'Open 40 treedbs with a newer schema')
    price_card('ctreedb.same_literal_fix', 'Everyday open: the last schema fixes', prev_label='before', cur_label='after')
    price_card('treedb.delete_force', 'Forced delete of a node (treedb)')
    price_card('publish.250b.global.100', 'Publish to 100 subscribers with __global__ (250 B)')
    price_card('publish.20kb.global.100', 'Publish to 100 subscribers with __global__ (20 KB)')
    price_card('audit.build.run_yuno', 'Build the audit record of run-yuno', prev_label='before', cur_label='after')
    price_card('ctest.treedb_schema_fidelity', 'A test that creates four stores', prev_label=prev + ' era', cur_label=version)
    H.append('<div class="multi">%s</div>' % '\n'.join(cards))

    # ---- binary sizes
    bins = [x for x in rep.get('binaries', []) if x['kind'] in ('yuno', 'agent')]
    if bins:
        bins.sort(key=lambda x: -x['bytes'])
        H.append('<h2>Binary sizes</h2>')
        H.append('<p class="sub">Every yuno is one fully static executable: no shared library, no runtime to '
                 'install, runs on any Linux of the same architecture. The sizes below include OpenSSL %s, '
                 'jansson, liburing and the whole framework. On disk they carry their debug information '
                 '(RelWithDebInfo); the stripped size is what the code and data take.</p>'
                 % esc(b['tls'].split(' ')[1] if b['tls'].startswith('OpenSSL') else ''))
        H.append('<div class="legend"><span><i class="sw-cur"></i>stripped</span><span><i class="sw-prev"></i>debug information (on disk only)</span></div>')
        hi = max(x['bytes'] for x in bins) / 1e6 * 1.02
        out = ['<div class="chart">']
        for x in bins:
            mb = x['bytes'] / 1e6
            smb = x['stripped_bytes'] / 1e6
            tip = '%s: %.1f MB on disk, %.1f MB stripped, static: %s' % (x['name'], mb, smb, 'yes' if x['static'] else 'no')
            track = ('<rect class="bar-prev" x="0" y="5" width="%.2f%%" height="12" rx="3"><title>%s</title></rect>'
                     '<rect class="bar-cur" x="0" y="5" width="%.2f%%" height="12" rx="3"><title>%s</title></rect>'
                     % (xpos(mb, 0, hi), esc(tip), xpos(smb, 0, hi), esc(tip)))
            out.append('<div class="row"><div class="lab">%s <span class="m">%s</span></div>%s<div class="val">%.1f / %.1f MB</div></div>'
                       % (esc(x['name']), esc(x['kind']), svg_track(track, tip), smb, mb))
        out.append('<p class="small">MB = 10<sup>6</sup> bytes; the column gives stripped / on disk. Yuneta can also be '
                   'built with mbedTLS instead of OpenSSL; mbedTLS is not built on this machine, so those '
                   'binaries are not in this report.</p></div>')
        H.append('<div class="card">%s</div>' % '\n'.join(out))

    # ---- method
    H.append('<h2>How it was measured</h2>')
    H.append('<div class="card"><p><b>%s against %s.</b> %s</p><p><b>Absolute figures.</b> %s</p>'
             '<p><b>Machine.</b> %s. %s. %s.</p>'
             '<p><b>Build.</b> %s, %s, fully static: %s, TLS: %s. %s</p>'
             '<p>The benchmarks are in <a href="%s/tree/%s/performance/c">performance/c/</a>; the raw '
             'figures of this page are in <a href="%s/blob/%s/performance/reports/%s.json">%s.json</a>; '
             'every A/B, change by change, is in <a href="%s/blob/%s/performance/c/README.md">performance/c/README.md</a> '
             'and in the <a href="%s/blob/%s/CHANGELOG.md">CHANGELOG</a>.</p></div>' % (
                 esc(version), esc(prev), md_code(rep['method']['ab']), md_code(rep['method']['tag_run']),
                 esc(machine_line), esc(m['model']), esc(m.get('shared', '')),
                 esc(b['build_type']), esc(b['compiler']), 'yes' if b['fully_static'] else 'no', esc(b['tls']), esc(b['note']),
                 GITHUB, esc(version), GITHUB, esc(version), esc(version), esc(version),
                 GITHUB, esc(version), GITHUB, esc(version)))

    # ---- tables
    H.append('<details><summary>Every A/B figure (%d)</summary><div class="tablewrap"><table>'
             '<tr><th>Figure</th><th>Unit</th><th>%s</th><th>%s</th><th>Change</th><th>n</th><th>Scope</th><th>Verdict</th></tr>'
             % (len(rep['ab']), esc(prev), esc(version)))
    for e in rep['ab']:
        H.append('<tr><td>%s<br><span class="small">%s%s</span></td><td>%s</td><td class="n">%s</td><td class="n">%s</td>'
                 '<td class="n">%s</td><td class="n">%s</td><td>%s</td><td class="%s">%s</td></tr>' % (
                     esc(e['label']), esc(e['bench']), (' &middot; ' + esc(e['commit'])) if e.get('commit') else '',
                     esc(e['unit']), mean_sd(e['previous']), mean_sd(e['current']), esc(pct(e['change_pct'])),
                     esc(e['n']), esc(e['scope']), VERDICT[e['verdict']][1], esc(VERDICT[e['verdict']][0])))
    H.append('</table></div></details>')
    H.append('<details><summary>Every absolute figure of %s (%d)</summary><div class="tablewrap"><table>'
             '<tr><th>Figure</th><th>Unit</th><th>Mean &plusmn; sd</th><th>Median</th><th>Min .. max</th><th>Rounds</th></tr>'
             % (esc(version), len(rep['figures'])))
    for f in rep['figures']:
        H.append('<tr><td>%s<br><span class="small">%s %s</span></td><td>%s</td><td class="n">%s</td><td class="n">%s</td>'
                 '<td class="n">%s .. %s</td><td class="n">%s</td></tr>' % (
                     esc(f['label']), esc(f['bench']), esc(f.get('case', '')), esc(f['unit']), mean_sd(f),
                     esc(fmt(f['median'])), esc(fmt(f['min'])), esc(fmt(f['max'])), esc(f['n'])))
    H.append('</table></div></details>')

    H.append('<footer>Yuneta Simplified %s &middot; <a href="%s">github.com/artgins/yunetas</a> &middot; '
             '<a href="https://doc.yuneta.io/performance">doc.yuneta.io/performance</a> (every release) &middot; '
             'generated by performance/reports/make_report.py from %s.json</footer>'
             % (esc(version), GITHUB, esc(version)))
    H.append('</main>\n</body>\n</html>\n')
    return '\n'.join(H)


#
#   Standalone SVG charts for doc.yuneta.io
#
SVG_STYLE = """<style>
  .bg { fill: #fcfcfb; stroke: rgba(11,11,11,0.10); }
  .t { fill: #0b0b0b; font-size: 15px; font-weight: 600; }
  .l { fill: #0b0b0b; font-size: 12.5px; }
  .m { fill: #6f6d68; font-size: 11.5px; }
  .g { fill: #6f6d68; font-size: 11px; font-weight: 700; letter-spacing: 0.06em; }
  .v { fill: #52514e; font-size: 12px; }
  .grid { stroke: #e1e0d9; stroke-width: 1; }
  .zero { stroke: #a3a198; stroke-width: 1; }
  .gain { fill: #2a78d6; } .price { fill: #e34948; } .noise { fill: #b9b7ae; }
  .cur { fill: #2a78d6; } .prev { fill: #9b9991; }
  .curl { stroke: #2a78d6; stroke-width: 2; fill: none; }
  .w { stroke: #52514e; stroke-width: 1.5; }
  @media (prefers-color-scheme: dark) {
    .bg { fill: #1a1a19; stroke: rgba(255,255,255,0.10); }
    .t, .l { fill: #ffffff; } .m, .g { fill: #a3a198; } .v { fill: #c3c2b7; }
    .grid { stroke: #2c2c2a; } .zero { stroke: #5c5b56; }
    .gain, .cur { fill: #3987e5; } .price { fill: #e66767; } .noise { fill: #5c5b56; } .prev { fill: #7a786f; }
    .curl { stroke: #3987e5; } .w { stroke: #c3c2b7; }
  }
</style>"""


def svg_doc(w, h, title, body):
    return ('<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 %d %d" width="%d" '
            'font-family="system-ui, -apple-system, \'Segoe UI\', Roboto, sans-serif" role="img" aria-label="%s">\n'
            '<title>%s</title>\n%s\n<rect class="bg" x="0.5" y="0.5" width="%d" height="%d" rx="10"/>\n%s\n</svg>\n'
            % (w, h, w, esc(title), esc(title), SVG_STYLE, w - 1, h - 1, body))


def svg_change_chart(rep, order):
    AB = ab_by_id(rep)
    entries = [AB[i] for i in order if i in AB]
    W = 900
    LX, TX, TW, VX = 20, 330, 440, 880
    lo, hi = -100.0, 40.0
    row_h = 22
    y = 58
    body = []
    title = 'Yuneta %s against %s: change of the time one operation takes' % (rep['version'], rep['previous'])
    body.append('<text class="t" x="%d" y="30">%s</text>' % (LX, esc(title)))
    body.append('<text class="m" x="%d" y="48">left of zero is faster &#183; blue faster, grey within noise, '
                'red a price paid on purpose &#183; whisker &#177;1 sd &#183; bars past +40%% are cut (factor on the right)</text>' % LX)
    X = lambda v: TX + xpos(v, lo, hi) / 100.0 * TW
    y += 8
    group = None
    rows = []
    for e in entries:
        if e['group'] != group:
            group = e['group']
            y += 8
            rows.append('<text class="g" x="%d" y="%d">%s</text>' % (LX, y + 10, esc(group.upper())))
            y += 18
        ch = time_change(e)
        sd = time_change_sd(e)
        cls = {'gain': 'gain', 'noise': 'noise'}.get(e['verdict'], 'price')
        a, bb = sorted([X(0), X(ch)])
        rows.append('<text class="l" x="%d" y="%d">%s</text>' % (LX, y + 14, esc(e.get('short', e['label']))))
        rows.append('<rect class="%s" x="%.1f" y="%d" width="%.1f" height="10" rx="2"><title>%s: %s</title></rect>'
                    % (cls, a, y + 5, max(bb - a, 1.0), esc(e['label']), esc(ratio_text(e))))
        if sd is not None and ch <= hi:
            w1, w2 = X(ch - sd), X(ch + sd)
            rows.append('<path class="w" d="M%.1f %d H%.1f M%.1f %d V%d M%.1f %d V%d"/>'
                        % (w1, y + 10, w2, w1, y + 6, y + 14, w2, y + 6, y + 14))
        rows.append('<text class="v" x="%d" y="%d" text-anchor="end">%s</text>' % (VX, y + 14, esc(ratio_text(e))))
        y += row_h
    top = 66
    grid = []
    for t in range(int(lo), int(hi) + 1, 25):
        grid.append('<line class="grid" x1="%.1f" x2="%.1f" y1="%d" y2="%d"/>' % (X(t), X(t), top, y))
        grid.append('<text class="m" x="%.1f" y="%d" text-anchor="middle">%s</text>' % (X(t), y + 16, ('+' if t > 0 else '') + '%d%%' % t))
    grid.append('<line class="zero" x1="%.1f" x2="%.1f" y1="%d" y2="%d"/>' % (X(0), X(0), top, y))
    H = y + 32
    return svg_doc(W, H, title, '\n'.join(body + grid + rows))


TREND_METRICS = [
    ('tr2.append_rate', 'Appends per second'),
    ('tr2.open_master', 'Open a store as master (ms)'),
    ('tr2.tm_query', 'tm query of one minute (ms)'),
    ('treedb.update_memory', 'treedb update in memory (us)'),
    ('treedb.update_saved', 'treedb update, saved (us)'),
    ('treedb.link_unlink', 'treedb link / unlink (us)'),
    ('ctreedb.same_literal_release', 'Open 40 treedbs, same schema (s)'),
    ('rotatory.audit_record', 'Agent audit record (ns)'),
    ('net.yev_ping_pong', 'Event loop echo (K msg/s)'),
    ('net.tcp_echo', 'gobj TCP echo (round trips/s)'),
]


def trend_series(reports, mid):
    """
    Points (version, mean) of one A/B metric across every report: the
    "previous" of the oldest report, then the "current" of each report.
    """
    pts = []
    first = True
    for v, rep in reports.items():
        e = ab_by_id(rep).get(mid)
        if not e:
            continue
        if first and rep.get('previous'):
            pts.append((rep['previous'], e['previous']['mean']))
        first = False
        pts.append((v, e['current']['mean']))
    return pts


def svg_trend(reports):
    cols, cw, ch = 2, 430, 150
    n = len(TREND_METRICS)
    rows_ = (n + cols - 1) // cols
    W = 20 + cols * cw + 20
    H = 70 + rows_ * ch + 10
    body = ['<text class="t" x="20" y="30">Yuneta performance, release after release</text>',
            '<text class="m" x="20" y="48">one panel per figure, each on its own scale from zero &#183; '
            'the A/B figures of each release report (7.25.4 is the baseline of 7.25.5)</text>']
    for k, (mid, lab) in enumerate(TREND_METRICS):
        pts = trend_series(reports, mid)
        cx = 20 + (k % cols) * cw
        cy = 66 + (k // cols) * ch
        body.append('<text class="l" x="%d" y="%d">%s</text>' % (cx, cy + 14, esc(lab)))
        if not pts:
            continue
        px0, px1 = cx + 10, cx + cw - 70
        py0, py1 = cy + 110, cy + 30
        hi = max(v for _, v in pts) * 1.15
        body.append('<line class="grid" x1="%d" x2="%d" y1="%d" y2="%d"/>' % (px0, px1 + 50, py0, py0))
        xs = [px0 + (px1 - px0) * (i / max(len(pts) - 1, 1)) for i in range(len(pts))]
        if len(pts) == 1:
            xs = [(px0 + px1) / 2]
        ys = [py0 - (py0 - py1) * (v / hi) for _, v in pts]
        if len(pts) > 1:
            body.append('<polyline class="curl" points="%s"/>' % ' '.join('%.1f,%.1f' % xy for xy in zip(xs, ys)))
        for i, ((ver, v), x, yy) in enumerate(zip(pts, xs, ys)):
            last = i == len(pts) - 1
            body.append('<circle class="%s" cx="%.1f" cy="%.1f" r="4.5" stroke-width="2"><title>%s: %s</title></circle>'
                        % ('cur' if last else 'prev', x, yy, esc(ver), esc(fmt(v))))
            body.append('<text class="v" x="%.1f" y="%.1f" text-anchor="start">%s</text>'
                        % (x + 8, yy - 8 if not last else yy + 4, esc(fmt(v))))
            body.append('<text class="m" x="%.1f" y="%d" text-anchor="middle">%s</text>' % (x, py0 + 16, esc(ver)))
    return svg_doc(W, H, 'Yuneta performance, release after release', '\n'.join(body))


def svg_absolute(rep):
    FG = fig_by_id(rep)
    items = [
        ('tr2.append_rate', 'timeranger2 appends (records stored)'),
        ('tr2.append_rate_rt', 'appends with a live reader'),
        ('tr2.read_rate_iterator', 'timeranger2 reads (records loaded)'),
        ('tr2.read_rate_pages', 'reads, page by page'),
        ('net.yev_ping_pong', 'io_uring event loop alone (1 KB echo)'),
        ('net.yev_ping_pong_persist', 'event loop + timeranger2 append'),
        ('net.tcp_echo', 'full gobj stack, TCP'),
        ('net.tcp_echo_persist', 'gobj stack, TCP + append'),
        ('net.tls_echo', 'gobj stack, TLS (OpenSSL)'),
        ('net.tls_echo_persist', 'gobj stack, TLS + append'),
        ('net.auth_bff_login', 'OAuth2 BFF logins (HTTP, 5 clients)'),
    ]
    rows = []
    for i, lab in items:
        f = FG.get(i)
        if f:
            rows.append((lab, f['mean'] * (1000.0 if f['unit'] == 'K msg/s' else 1.0)))
    if not rows:
        return None
    W, LX, TX, TW = 900, 20, 330, 470
    hi = max(v for _, v in rows) * 1.05
    title = 'Yuneta %s: operations per second on one core' % rep['version']
    body = ['<text class="t" x="%d" y="30">%s</text>' % (LX, esc(title)),
            '<text class="m" x="%d" y="48">as each benchmark counts them: a record stored or read, a message echoed, a round trip, a login</text>' % LX]
    y = 66
    step = 25000 if hi < 200000 else 50000
    grid = []
    for k in range(0, int(hi / step) + 1):
        x = TX + k * step / hi * TW
        grid.append('<line class="grid" x1="%.1f" x2="%.1f" y1="%d" y2="%d"/>' % (x, x, y, y + len(rows) * 26))
        grid.append('<text class="m" x="%.1f" y="%d" text-anchor="middle">%s</text>' % (x, y + len(rows) * 26 + 16, esc(tick_text(k * step, False) if k else '0')))
    for lab, v in rows:
        body.append('<text class="l" x="%d" y="%d">%s</text>' % (LX, y + 16, esc(lab)))
        body.append('<rect class="cur" x="%d" y="%d" width="%.1f" height="12" rx="3"/>' % (TX, y + 6, v / hi * TW))
        body.append('<text class="v" x="%.1f" y="%d">%s/s</text>' % (TX + v / hi * TW + 6, y + 16, esc(fmt_rate(v))))
        y += 26
    return svg_doc(W, y + 36, title, '\n'.join(grid + body))


AB_ORDER = [
    'tr2.append_rate', 'tr2.append_rate_rt', 'tr2.build_appends', 'tr2.open_master', 'tr2.tm_query',
    'tr2.open_replica', 'tr2.create_topic', 'tr2.topic_version_change', 'tr2.tm_query_unmigrated',
    'treedb.update_memory', 'treedb.update_saved', 'treedb.link_unlink', 'treedb.create_link_half',
    'treedb.reopen', 'treedb.delete_force', 'treedb.delete_parent',
    'ctreedb.same_literal_release', 'ctreedb.seed', 'ctreedb.newer_literal',
    'rotatory.audit_record', 'rotatory.audit_record_flush',
    'net.yev_ping_pong', 'net.tcp_echo', 'net.tcp_echo_persist',
    'publish.250b.plain.100', 'publish.250b.global.100', 'publish.20kb.global.100',
]


def build_docs(reports):
    os.makedirs(DOCS_SVG, exist_ok=True)
    latest = list(reports.values())[-1]
    written = []
    p = os.path.join(DOCS_SVG, 'perf_trend.svg')
    open(p, 'w').write(svg_trend(reports))
    written.append(p)
    for v, rep in reports.items():
        p = os.path.join(DOCS_SVG, 'perf_change_%s.svg' % v)
        open(p, 'w').write(svg_change_chart(rep, AB_ORDER))
        written.append(p)
        s = svg_absolute(rep)
        if s:
            p = os.path.join(DOCS_SVG, 'perf_throughput_%s.svg' % v)
            open(p, 'w').write(s)
            written.append(p)
    return written


def main():
    reports = load_reports()
    if len(sys.argv) > 1 and sys.argv[1] == '--docs':
        for p in build_docs(reports):
            print('wrote', os.path.relpath(p, REPO))
        return 0
    if len(sys.argv) < 2 or sys.argv[1] not in reports:
        print('usage: make_report.py <version> | --docs   (versions: %s)' % ', '.join(reports), file=sys.stderr)
        return 1
    rep = reports[sys.argv[1]]
    out = os.path.join(HERE, '%s.html' % rep['version'])
    with open(out, 'w') as f:
        f.write(build_html(rep, reports))
    print('wrote', os.path.relpath(out, REPO))
    return 0


if __name__ == '__main__':
    sys.exit(main())
