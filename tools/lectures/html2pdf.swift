// Copyright (c) 2026 Borja Vazquez Cuesta. All rights reserved.

// HTML laid out as an A4 PDF by macOS's own text system (AppKit), its pages
// numbered at the foot — for the lectures built from wiki pages (phrasebook.py):
//   swift html2pdf.swift IN.html OUT.pdf [--no-numbers]
// A tool for building the app's assets on a Mac, not part of the app.
import AppKit

let args = CommandLine.arguments
guard args.count >= 3 else {
    FileHandle.standardError.write("usage: swift html2pdf.swift IN.html OUT.pdf [--no-numbers]\n".data(using: .utf8)!)
    exit(1)
}
let numbered = !args.contains("--no-numbers")
_ = NSApplication.shared

let html = try Data(contentsOf: URL(fileURLWithPath: args[1]))
let text = try NSAttributedString(data: html,
                                  options: [.documentType: NSAttributedString.DocumentType.html,
                                            .characterEncoding: String.Encoding.utf8.rawValue],
                                  documentAttributes: nil)

// A4, with margins a hand can hold the page by.
let paper  = NSSize(width: 595, height: 842)
let margin = (top: CGFloat(54), bottom: CGFloat(60), side: CGFloat(56))
let width  = paper.width - 2 * margin.side

// The page's number at its foot.
final class PagedText: NSTextView {
    var numbered = true
    override var pageFooter: NSAttributedString {
        guard numbered, let op = NSPrintOperation.current else { return NSAttributedString() }
        let style = NSMutableParagraphStyle()
        style.alignment = .center
        return NSAttributedString(string: "\(op.currentPage)",
                                  attributes: [.font: NSFont(name: "Helvetica Neue", size: 9) ?? NSFont.systemFont(ofSize: 9),
                                               .foregroundColor: NSColor.darkGray, .paragraphStyle: style])
    }
    override var pageHeader: NSAttributedString { NSAttributedString() }
}

let view = PagedText(frame: NSRect(x: 0, y: 0, width: width, height: 100))
view.numbered = numbered
view.isVerticallyResizable = true
view.textContainer?.containerSize = NSSize(width: width, height: .greatestFiniteMagnitude)
view.textContainer?.widthTracksTextView = true
view.textStorage?.setAttributedString(text)
view.drawsBackground = false
if let layout = view.layoutManager, let container = view.textContainer {
    layout.ensureLayout(for: container)
    view.frame = NSRect(x: 0, y: 0, width: width, height: ceil(layout.usedRect(for: container).height) + 8)
}

let info = NSPrintInfo()
info.paperSize              = paper
info.topMargin              = margin.top
info.bottomMargin           = margin.bottom
info.leftMargin             = margin.side
info.rightMargin            = margin.side
info.horizontalPagination   = .clip
info.verticalPagination     = .automatic
info.isHorizontallyCentered = false
info.isVerticallyCentered   = false
info.jobDisposition         = .save
info.dictionary()[NSPrintInfo.AttributeKey.jobSavingURL]    = URL(fileURLWithPath: args[2])
info.dictionary()[NSPrintInfo.AttributeKey.headerAndFooter] = numbered

let op = NSPrintOperation(view: view, printInfo: info)
op.showsPrintPanel    = false
op.showsProgressPanel = false
exit(op.run() ? 0 : 1)
