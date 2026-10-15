// Copyright (c) 2026 Borja Vazquez Cuesta. All rights reserved.

// swift svg2png.swift IN.svg OUT.png SIZE [opaque] — an SVG drawn by AppKit into a SIZE×SIZE PNG
// (tools/icons/app_icon.py). With opaque, the PNG has no alpha channel: App Store Connect rejects an icon that has one,
// even when every pixel is opaque. (AppKit draws only into a bitmap with alpha, so it is drawn so, then copied into
// one without: what was transparent comes out black.)
import AppKit
let a = CommandLine.arguments
guard a.count == 4 || a.count == 5, let img = NSImage(contentsOfFile: a[1]), let n = Int(a[3]) else { print("usage / unreadable"); exit(1) }
let opaque = a.count == 5 && a[4] == "opaque"
let rep = NSBitmapImageRep(bitmapDataPlanes: nil, pixelsWide: n, pixelsHigh: n, bitsPerSample: 8, samplesPerPixel: 4, hasAlpha: true, isPlanar: false, colorSpaceName: .deviceRGB, bytesPerRow: 0, bitsPerPixel: 0)!
rep.size = NSSize(width: n, height: n)
NSGraphicsContext.saveGraphicsState()
NSGraphicsContext.current = NSGraphicsContext(bitmapImageRep: rep)
NSGraphicsContext.current?.imageInterpolation = .high
img.draw(in: NSRect(x: 0, y: 0, width: n, height: n))
NSGraphicsContext.restoreGraphicsState()
var out = rep
if opaque {
    let flat = CGContext(data: nil, width: n, height: n, bitsPerComponent: 8, bytesPerRow: 0, space: CGColorSpaceCreateDeviceRGB(),
                         bitmapInfo: CGImageAlphaInfo.noneSkipLast.rawValue)!
    flat.draw(rep.cgImage!, in: CGRect(x: 0, y: 0, width: n, height: n))
    out = NSBitmapImageRep(cgImage: flat.makeImage()!)
}
try! out.representation(using: .png, properties: [:])!.write(to: URL(fileURLWithPath: a[2]))
