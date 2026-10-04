// swift tools/icons/flags.swift OUT_DIR — China's, South Korea's, Thailand's and India's flags
// as the language list draws the others (96x64, rounded 6, a 1-pixel 209 grey border):
// cn.png, kr.png, th.png, in.png; and Arabic's mark, ar.png (no one country's flag).
import AppKit
let W: CGFloat = 96, H: CGFloat = 64
func make(_ name: String, _ draw: (CGContext) -> Void) {
    let rep = NSBitmapImageRep(bitmapDataPlanes: nil, pixelsWide: Int(W), pixelsHigh: Int(H), bitsPerSample: 8, samplesPerPixel: 4, hasAlpha: true, isPlanar: false, colorSpaceName: .deviceRGB, bytesPerRow: 0, bitsPerPixel: 0)!
    NSGraphicsContext.saveGraphicsState()
    let g = NSGraphicsContext(bitmapImageRep: rep)!
    NSGraphicsContext.current = g
    let c = g.cgContext
    c.translateBy(x: 0, y: H); c.scaleBy(x: 1, y: -1)        // y down, as a flag's geometry is given
    let frame = CGPath(roundedRect: CGRect(x: 0.5, y: 0.5, width: W - 1, height: H - 1), cornerWidth: 5.5, cornerHeight: 5.5, transform: nil)
    c.saveGState(); c.addPath(frame); c.clip(); draw(c); c.restoreGState()
    c.addPath(frame); c.setStrokeColor(CGColor(red: 209/255, green: 209/255, blue: 209/255, alpha: 1)); c.setLineWidth(1); c.strokePath()
    NSGraphicsContext.restoreGraphicsState()
    try! rep.representation(using: .png, properties: [:])!.write(to: URL(fileURLWithPath: CommandLine.arguments[1] + "/" + name))
}
func rgb(_ h: UInt32) -> CGColor { CGColor(red: CGFloat((h >> 16) & 255) / 255, green: CGFloat((h >> 8) & 255) / 255, blue: CGFloat(h & 255) / 255, alpha: 1) }
func star(_ c: CGContext, _ x: CGFloat, _ y: CGFloat, _ r: CGFloat, _ toward: CGFloat) {   // a point at angle `toward` (y down)
    c.beginPath()
    for i in 0..<10 {
        let a = toward + CGFloat(i) * .pi / 5
        let rr = i % 2 == 0 ? r : r * 0.381966
        let p = CGPoint(x: x + rr * cos(a), y: y + rr * sin(a))
        if i == 0 { c.move(to: p) } else { c.addLine(to: p) }
    }
    c.closePath(); c.fillPath()
}
// China: red; on a 30x20 grid a star of radius 3 at (5,5), four of radius 1 each pointing at it.
make("cn.png") { c in
    c.setFillColor(rgb(0xDE2910)); c.fill(CGRect(x: 0, y: 0, width: W, height: H))
    let u = W / 30
    c.setFillColor(rgb(0xFFDE00))
    star(c, 5 * u, 5 * u, 3 * u, -.pi / 2)
    for (x, y) in [(10.0, 2.0), (12.0, 4.0), (12.0, 7.0), (10.0, 9.0)] as [(CGFloat, CGFloat)] {
        star(c, x * u, y * u, u, atan2(5 - y, 5 - x))
    }
}
// South Korea: white; the taegeuk (diameter half the height) tilted along the
// diagonal, red over blue; the four trigrams at the corners: ☰ top left, ☵ top
// right, ☲ bottom left, ☷ bottom right.
make("kr.png") { c in
    c.setFillColor(rgb(0xFFFFFF)); c.fill(CGRect(x: 0, y: 0, width: W, height: H))
    let cx = W / 2, cy = H / 2, D = H / 2, R = D / 2
    let th = atan2(H, W)
    let ux = cos(th), uy = sin(th)                     // towards the bottom right
    c.setFillColor(rgb(0x0047A0)); c.fillEllipse(in: CGRect(x: cx - R, y: cy - R, width: D, height: D))
    c.setFillColor(rgb(0xCD2E3A)); c.beginPath()
    for i in 0...64 {
        let a = th + .pi + CGFloat(i) / 64 * .pi        // the half above the axis
        let p = CGPoint(x: cx + R * cos(a), y: cy + R * sin(a))
        if i == 0 { c.move(to: p) } else { c.addLine(to: p) }
    }
    c.closePath(); c.fillPath()
    let r = R / 2
    c.fillEllipse(in: CGRect(x: cx - r * ux - r, y: cy - r * uy - r, width: 2 * r, height: 2 * r))
    c.setFillColor(rgb(0x0047A0)); c.fillEllipse(in: CGRect(x: cx + r * ux - r, y: cy + r * uy - r, width: 2 * r, height: 2 * r))
    // The trigrams: bars across their diagonal, inner to outer (true: solid).
    let T = H / 24, G = H / 48, L = H / 4, gap = max(2, L / 8)
    let first = R + D / 4 + T / 2                      // the inner bar's centre, from the taegeuk's
    c.setFillColor(rgb(0x000000))
    for (dx, dy, bars) in [(-ux, -uy, [true, true, true]), (ux, -uy, [false, true, false]), (-ux, uy, [true, false, true]), (ux, uy, [false, false, false])] {
        for (i, solid) in bars.enumerated() {
            let d = first + CGFloat(i) * (T + G)
            let bx = cx + dx * d, by = cy + dy * d
            let px = -dy, py = dx                      // along the bar
            let halves: [(CGFloat, CGFloat)] = solid ? [(-L / 2, L / 2)] : [(-L / 2, -gap / 2), (gap / 2, L / 2)]
            for (a, b) in halves {
                c.beginPath()
                c.move(to: CGPoint(x: bx + px * a - dx * T / 2, y: by + py * a - dy * T / 2))
                c.addLine(to: CGPoint(x: bx + px * b - dx * T / 2, y: by + py * b - dy * T / 2))
                c.addLine(to: CGPoint(x: bx + px * b + dx * T / 2, y: by + py * b + dy * T / 2))
                c.addLine(to: CGPoint(x: bx + px * a + dx * T / 2, y: by + py * a + dy * T / 2))
                c.closePath(); c.fillPath()
            }
        }
    }
}
// Thailand: five stripes, red, white, blue (twice as tall), white, red.
make("th.png") { c in
    let u = H / 6
    for (y, h, col) in [(0.0, 1.0, 0xA51931), (1.0, 1.0, 0xF4F5F8), (2.0, 2.0, 0x2D2A4A), (4.0, 1.0, 0xF4F5F8), (5.0, 1.0, 0xA51931)] as [(CGFloat, CGFloat, UInt32)] {
        c.setFillColor(rgb(col)); c.fill(CGRect(x: 0, y: y * u, width: W, height: h * u))
    }
}
// India: saffron, white, green in thirds; the Ashoka Chakra in navy at the middle,
// as wide as three quarters of the white band, with 24 spokes.
make("in.png") { c in
    let u = H / 3
    for (i, col) in [0xFF9933, 0xFFFFFF, 0x138808].enumerated() {
        c.setFillColor(rgb(UInt32(col))); c.fill(CGRect(x: 0, y: CGFloat(i) * u, width: W, height: u))
    }
    let cx = W / 2, cy = H / 2, R = u * 0.375
    c.setStrokeColor(rgb(0x000080)); c.setFillColor(rgb(0x000080))
    c.setLineWidth(max(1, R * 0.12)); c.strokeEllipse(in: CGRect(x: cx - R, y: cy - R, width: 2 * R, height: 2 * R))
    c.setLineWidth(max(0.6, R * 0.06))
    for i in 0..<24 {
        let a = CGFloat(i) * .pi / 12
        c.beginPath(); c.move(to: CGPoint(x: cx, y: cy)); c.addLine(to: CGPoint(x: cx + R * cos(a), y: cy + R * sin(a))); c.strokePath()
    }
    c.fillEllipse(in: CGRect(x: cx - R * 0.2, y: cy - R * 0.2, width: R * 0.4, height: R * 0.4))
}
// Arabic: no one country's flag (and the ones often used carry the shahada): a
// green field with the letter ع (ʿayn, of العربية) in white, from Geeza Pro.
make("ar.png") { c in
    c.setFillColor(rgb(0x1E6B45)); c.fill(CGRect(x: 0, y: 0, width: W, height: H))
    let font = CTFontCreateWithName("GeezaPro-Bold" as CFString, 46, nil)
    var ch: [UniChar] = [0x0639], glyph: [CGGlyph] = [0]
    CTFontGetGlyphsForCharacters(font, &ch, &glyph, 1)
    var flip = CGAffineTransform(scaleX: 1, y: -1)
    if let path = CTFontCreatePathForGlyph(font, glyph[0], &flip) {
        let b = path.boundingBoxOfPath
        c.translateBy(x: W / 2 - b.midX, y: H / 2 - b.midY)
        c.addPath(path); c.setFillColor(rgb(0xFFFFFF)); c.fillPath()
    }
}
