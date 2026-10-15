#!/usr/bin/env python3
# Copyright (c) 2026 Borja Vazquez Cuesta. All rights reserved.

# Any app's icon, from its platform/ios/AppIcon.svg, into its Assets.xcassets:
#   - AppIcon-1024.png, the App Store's and Android's: opaque (App Store Connect rejects an alpha channel);
#   - LaunchIcon@2x/@3x.png, the launch screen's: the icon rounded as iOS rounds it on the home screen (a 22.4%
#     corner), transparent outside.
#
#   python3 tools/icons/app_icon.py apps/hanzi/platform/ios
#
# The study apps' SVGs are drawn by hand or by tools/icons/script_icon.py; the canvas apps' by
# tools/icons/infini_icons.py, which then calls write_icons. Needs a Mac: svg2png.swift draws the SVG with AppKit.
import os, re, subprocess, sys, tempfile

HERE = os.path.dirname(os.path.abspath(__file__))

def render(svg_path, out, size, opaque=False):
    subprocess.run(['swift', os.path.join(HERE, 'svg2png.swift'), svg_path, out, str(size)] + (['opaque'] if opaque else []), check=True)

# The SVG with everything in it clipped to the home screen's rounded square: what the root <svg> holds, wrapped in a
# clip the size of its viewBox (any SVG, whatever its layout).
def rounded(svg_text):
    start = svg_text.index('<svg')
    open_end = svg_text.index('>', start) + 1
    box = re.search(r'viewBox="([-\d.]+)[ ,]+([-\d.]+)[ ,]+([\d.]+)[ ,]+([\d.]+)"', svg_text[start:open_end])
    x, y, w, h = (float(v) for v in box.groups()) if box else (0.0, 0.0, 1024.0, 1024.0)
    clip = (f'<defs><clipPath id="home-round"><rect x="{x:g}" y="{y:g}" width="{w:g}" height="{h:g}" rx="{round(0.224 * w)}"/>'
            '</clipPath></defs><g clip-path="url(#home-round)">')
    close = svg_text.rindex('</svg>')
    return svg_text[:open_end] + clip + svg_text[open_end:close] + '</g>' + svg_text[close:]

def write_icons(ios):
    svg    = os.path.join(ios, 'AppIcon.svg')
    assets = os.path.join(ios, 'Assets.xcassets')
    render(svg, os.path.join(assets, 'AppIcon.appiconset', 'AppIcon-1024.png'), 1024, opaque=True)
    text = open(svg).read()
    with tempfile.TemporaryDirectory() as tmp:
        launch = os.path.join(tmp, 'launch.svg')
        open(launch, 'w').write(rounded(text))
        for size, name in ((320, 'LaunchIcon@2x.png'), (480, 'LaunchIcon@3x.png')):
            render(launch, os.path.join(assets, 'LaunchIcon.imageset', name), size)
    print('wrote', assets)

if __name__ == '__main__':
    write_icons(sys.argv[1])
