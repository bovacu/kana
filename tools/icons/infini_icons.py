#!/usr/bin/env python3
# Copyright (c) 2026 Borja Vazquez Cuesta. All rights reserved.

# InfiNote's and InfiniWorkshop's icons, drawn as SVG (docs/product_split.md): their platform/ios/AppIcon.svg, from
# which tools/icons/svg2png.swift draws the App Store's and Android's 1024 px icon and the launch screen's (rounded):
#
#   python3 tools/icons/infini_icons.py
#
# InfiNote: a notebook page, an infinity written across it with a fountain pen (thin along the nib, broad across it),
# and a small one inside its right loop — what is inside, zoomed into. InfiniWorkshop: a blueprint, an infinity whose
# right loop is a gear and whose left loop runs circuit traces to their pads — mechanisms and circuits.
import math, os, subprocess, sys, tempfile

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.normpath(os.path.join(HERE, '..', '..'))

def lem(a, t):
    d = 1 + math.sin(t) ** 2
    return a * math.cos(t) / d, a * math.sin(t) * math.cos(t) / d

def nib_stroke(cx, cy, a, base, amp, nib_deg, color, n=420):
    # A fountain pen's line: the width set by the nib's angle to the way it goes (thin along it, broad across).
    out = []
    pts = [lem(a, 2 * math.pi * i / n) for i in range(n + 1)]
    nib = math.radians(nib_deg)
    for i in range(n):
        x0, y0 = pts[i]; x1, y1 = pts[i + 1]
        ang = math.atan2(y1 - y0, x1 - x0)
        w = base + amp * abs(math.sin(ang - nib))
        out.append(f'<line x1="{cx+x0:.1f}" y1="{cy+y0:.1f}" x2="{cx+x1:.1f}" y2="{cy+y1:.1f}" stroke="{color}" stroke-width="{w:.1f}" stroke-linecap="round"/>')
    return out

def svg(body, defs=''):
    return ('<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 1024 1024" width="1024" height="1024">\n'
            f'<defs>{defs}</defs>\n' + '\n'.join(body) + '\n</svg>\n')

# ---- InfiNote: a notebook page, an infinity written across it with a fountain pen, a small one inside its loop ----
def infinote():
    defs = ('<linearGradient id="bg" x1="0" y1="0" x2="1" y2="1"><stop offset="0" stop-color="#4C80DE"/><stop offset="1" stop-color="#1D3A84"/></linearGradient>'
            '<linearGradient id="pg" x1="0" y1="0" x2="0" y2="1"><stop offset="0" stop-color="#FFFBF2"/><stop offset="1" stop-color="#F6EEDC"/></linearGradient>')
    b = ['<rect width="1024" height="1024" fill="url(#bg)"/>']
    g = ['<g transform="rotate(-6 512 520)">']
    g.append('<rect x="262" y="196" width="520" height="664" rx="34" fill="#10275C" opacity="0.38" transform="translate(16 24)"/>')
    g.append('<rect x="262" y="196" width="520" height="664" rx="34" fill="url(#pg)"/>')
    for y in range(316, 840, 62):
        g.append(f'<line x1="290" y1="{y}" x2="754" y2="{y}" stroke="#BFD0EA" stroke-width="6" stroke-linecap="round"/>')
    g.append('<line x1="338" y1="222" x2="338" y2="834" stroke="#E8A3A3" stroke-width="6"/>')
    g.append('</g>')
    b += g
    b += nib_stroke(512, 532, 300, 20, 44, 40, '#17264A')
    # (in the right loop, a small one: what is inside, zoomed into)
    b += nib_stroke(512 + 300 * 0.62, 532, 60, 5, 9, 40, '#3D6FD1')
    return svg(b, defs)

# ---- InfiniWorkshop: a blueprint, an infinity whose right loop is a gear and whose left loop runs circuit traces ----
def workshop():
    s = 0.86                      # (the drawing kept to the middle: a launcher's mask clear of it)
    def P(x, y): return 512 + (x - 512) * s, 512 + (y - 512) * s
    defs = '<linearGradient id="bg" x1="0" y1="0" x2="1" y2="1"><stop offset="0" stop-color="#2D63A8"/><stop offset="1" stop-color="#0E2B55"/></linearGradient>'
    b = ['<rect width="1024" height="1024" fill="url(#bg)"/>']
    for k in range(64, 1024, 64):
        w, op = (5, 0.55) if k % 256 == 0 else (2.5, 0.45)
        b.append(f'<line x1="{k}" y1="0" x2="{k}" y2="1024" stroke="#5B8BC9" stroke-width="{w}" opacity="{op}"/>')
        b.append(f'<line x1="0" y1="{k}" x2="1024" y2="{k}" stroke="#5B8BC9" stroke-width="{w}" opacity="{op}"/>')
    amber, dark = '#F8A72E', '#0E2B55'
    r, d, sw = 168.0, 382.0, 84.0
    cl, cr = (512 - d / 2, 512.0), (512 + d / 2, 512.0)
    alpha = math.acos(r / (d / 2))            # (the tangent through the middle: its point's angle from the line of centres)
    def tp(c, ang): return c[0] + r * math.cos(ang), c[1] + r * math.sin(ang)
    # (SVG's y down: the left loop's upper and lower points by the middle, the right loop's lower and upper)
    l_up, l_dn = tp(cl, -alpha), tp(cl, alpha)
    r_dn, r_up = tp(cr, math.pi - alpha), tp(cr, math.pi + alpha)
    def fmt(p): x, y = P(*p); return f'{x:.1f} {y:.1f}'
    R = r * s
    # gear teeth round the right loop (under the line)
    teeth = 14
    for i in range(teeth):
        ang = 2 * math.pi * i / teeth + math.pi / teeth
        cx, cy = P(cr[0] + (r + sw / 2 + 22) * math.cos(ang), cr[1] + (r + sw / 2 + 22) * math.sin(ang))
        deg = math.degrees(ang)
        b.append(f'<rect x="{cx - 26*s:.1f}" y="{cy - 30*s:.1f}" width="{52*s:.1f}" height="{60*s:.1f}" rx="{10*s:.1f}" fill="{amber}" transform="rotate({deg:.1f} {cx:.1f} {cy:.1f})"/>')
    # circuit traces off the left loop, each to a pad
    for deg, bend in ((150, 1), (180, 0), (210, -1)):
        ang = math.radians(deg)
        x0, y0 = cl[0] + (r + sw / 2 - 6) * math.cos(ang), cl[1] + (r + sw / 2 - 6) * math.sin(ang)
        x1, y1 = x0 + 52 * math.cos(ang), y0 + 52 * math.sin(ang)
        x2, y2 = (x1 - 46, y1 - 46 * bend) if bend else (x1 - 60, y1)
        a0, a1, a2 = P(x0, y0), P(x1, y1), P(x2, y2)
        b.append(f'<polyline points="{a0[0]:.1f},{a0[1]:.1f} {a1[0]:.1f},{a1[1]:.1f} {a2[0]:.1f},{a2[1]:.1f}" fill="none" stroke="{amber}" stroke-width="{24*s:.1f}" stroke-linecap="round" stroke-linejoin="round"/>')
        b.append(f'<circle cx="{a2[0]:.1f}" cy="{a2[1]:.1f}" r="{30*s:.1f}" fill="{amber}"/><circle cx="{a2[0]:.1f}" cy="{a2[1]:.1f}" r="{12*s:.1f}" fill="{dark}"/>')
    # the infinity: across the middle, round each loop
    # (down across the middle to the right loop, round it the long way by its far side, up across the middle, round
    # the left loop the long way back)
    path = (f'M {fmt(l_up)} L {fmt(r_dn)} A {R:.1f} {R:.1f} 0 1 0 {fmt(r_up)} '
            f'L {fmt(l_dn)} A {R:.1f} {R:.1f} 0 1 1 {fmt(l_up)} Z')
    b.append(f'<path d="{path}" fill="none" stroke="{amber}" stroke-width="{sw*s:.1f}" stroke-linejoin="round"/>')
    # the gear's hub and the loop's centre mark
    hx, hy = P(*cr)
    b.append(f'<circle cx="{hx:.1f}" cy="{hy:.1f}" r="{34*s:.1f}" fill="none" stroke="{amber}" stroke-width="{16*s:.1f}"/>')
    return svg(b, defs)

def render(svg_path, out, size):
    subprocess.run(['swift', os.path.join(HERE, 'svg2png.swift'), svg_path, out, str(size)], check=True)

def rounded(svg_text):
    # (the launch screen's: the icon rounded as iOS rounds it on the home screen, a 22.4% corner)
    return svg_text.replace('<defs>', '<defs><clipPath id="round"><rect width="1024" height="1024" rx="229"/></clipPath>', 1) \
                   .replace('</defs>\n', '</defs>\n<g clip-path="url(#round)">\n', 1).replace('\n</svg>', '\n</g>\n</svg>')

def make(app, svg_text):
    ios = os.path.join(ROOT, 'apps', app, 'platform', 'ios')
    open(os.path.join(ios, 'AppIcon.svg'), 'w').write(svg_text)
    assets = os.path.join(ios, 'Assets.xcassets')
    render(os.path.join(ios, 'AppIcon.svg'), os.path.join(assets, 'AppIcon.appiconset', 'AppIcon-1024.png'), 1024)
    with tempfile.TemporaryDirectory() as tmp:
        launch = os.path.join(tmp, 'launch.svg')
        open(launch, 'w').write(rounded(svg_text))
        for size, name in ((320, 'LaunchIcon@2x.png'), (480, 'LaunchIcon@3x.png')):
            render(launch, os.path.join(assets, 'LaunchIcon.imageset', name), size)
    print('wrote', ios)

if __name__ == '__main__':
    make('infinote', infinote())
    make('infiniworkshop', workshop())
