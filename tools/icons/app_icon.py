#!/usr/bin/env python3
# Copyright (c) 2026 Borja Vazquez Cuesta. All rights reserved.

# A study app's icon, from its platform/ios/AppIcon.svg: the App Store's
# AppIcon-1024.png and the launch screen's LaunchIcon@2x/@3x.png (the icon
# rounded as iOS rounds it on the home screen), into its Assets.xcassets.
#
#   python3 -m pip install pillow                       # once
#   python3 tools/icons/app_icon.py apps/hanzi/platform/ios
#
# Needs a Mac: svg2png.swift draws the SVG with AppKit.
import os, subprocess, sys, tempfile
from PIL import Image, ImageDraw

HERE = os.path.dirname(os.path.abspath(__file__))

def render(svg, out, size):
    subprocess.run(['swift', os.path.join(HERE, 'svg2png.swift'), svg, out, str(size)], check=True)

def main(ios):
    svg    = os.path.join(ios, 'AppIcon.svg')
    assets = os.path.join(ios, 'Assets.xcassets')
    with tempfile.TemporaryDirectory() as tmp:
        render(svg, os.path.join(tmp, 'icon.png'), 1024)
        Image.open(os.path.join(tmp, 'icon.png')).convert('RGB').save(os.path.join(assets, 'AppIcon.appiconset', 'AppIcon-1024.png'))
        # The launch icon: drawn three times larger, rounded (a 22.4% corner), then scaled down for smooth edges.
        for size, name in ((320, 'LaunchIcon@2x.png'), (480, 'LaunchIcon@3x.png')):
            big = os.path.join(tmp, 'launch.png')
            render(svg, big, size * 3)
            im = Image.open(big).convert('RGBA')
            n = im.size[0]
            mask = Image.new('L', (n, n), 0)
            ImageDraw.Draw(mask).rounded_rectangle((0, 0, n - 1, n - 1), radius=int(n * 0.224), fill=255)
            im.putalpha(mask)
            im.resize((size, size), Image.LANCZOS).save(os.path.join(assets, 'LaunchIcon.imageset', name))
    print('wrote', assets)

if __name__ == '__main__':
    main(sys.argv[1])
