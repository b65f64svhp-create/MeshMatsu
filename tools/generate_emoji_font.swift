// Rasterize the OFL Noto Emoji font only; never include system fallback glyphs.
// swift -module-cache-path /tmp/meshdeck-swift-cache tools/generate_emoji_font.swift FONT EMOJI_TEST OUTPUT
import Foundation
import CoreText
import CoreGraphics

let args = CommandLine.arguments
guard args.count == 4 else { fatalError("Expected font, emoji-test.txt, output .inc") }
let fontURL = URL(fileURLWithPath: args[1])
let descriptors = CTFontManagerCreateFontDescriptorsFromURL(fontURL as CFURL) as! [CTFontDescriptor]
let font = CTFontCreateWithFontDescriptor(descriptors[0], 32, nil)
let name = CTFontCopyPostScriptName(font) as String
guard name.contains("NotoEmoji") else { fatalError("Only Noto Emoji is permitted") }
var sequences = Set<String>()
for line in try String(contentsOfFile: args[2], encoding: .utf8).components(separatedBy: .newlines) {
    let fields = line.components(separatedBy: ";")
    guard fields.count > 1, fields[1].trimmingCharacters(in: .whitespaces).hasPrefix("fully-qualified") else { continue }
    let scalars = fields[0].split(separator: " ").compactMap { UInt32($0, radix: 16) }.compactMap(UnicodeScalar.init)
    sequences.insert(String(String.UnicodeScalarView(scalars.filter { $0.value != 0xfe0f })))
}
var entries: [(String, Int)] = []
var pixels: [UInt8] = []
var pool: [Data: Int] = [:]
var skipped = 0
for text in sequences.sorted(by: { Array($0.utf8).lexicographicallyPrecedes(Array($1.utf8)) }) {
    let attributes = [kCTFontAttributeName: font, kCTForegroundColorAttributeName: CGColor(gray: 1, alpha: 1)] as CFDictionary
    let attributed = CFAttributedStringCreate(nil, text as CFString, attributes)!
    let line = CTLineCreateWithAttributedString(attributed)
    let runs = CTLineGetGlyphRuns(line) as! [CTRun]
    let valid = runs.allSatisfy { run in
        let attrs = CTRunGetAttributes(run) as NSDictionary
        let runFont = attrs[kCTFontAttributeName] as! CTFont
        return (CTFontCopyPostScriptName(runFont) as String).contains("NotoEmoji")
    }
    guard valid, CTLineGetGlyphCount(line) == 1 else { skipped += 1; continue }
    var bitmap = [UInt8](repeating: 0, count: 400)
    bitmap.withUnsafeMutableBytes { bytes in
        let ctx = CGContext(data: bytes.baseAddress, width: 20, height: 20, bitsPerComponent: 8,
                            bytesPerRow: 20, space: CGColorSpaceCreateDeviceGray(), bitmapInfo: 0)!
        ctx.setShouldAntialias(true)
        let bounds = CTLineGetBoundsWithOptions(line, .useGlyphPathBounds)
        let scale = min(18 / max(bounds.width, 1), 18 / max(bounds.height, 1))
        ctx.translateBy(x: 10, y: 10)
        ctx.scaleBy(x: scale, y: scale)
        ctx.translateBy(x: -bounds.midX, y: -bounds.midY)
        ctx.textPosition = .zero
        CTLineDraw(line, ctx)
    }
    var packed = [UInt8](repeating: 0, count: 100)
    for y in 0..<20 { for x in 0..<20 {
        let index = y * 20 + x
        let alpha = (Int(bitmap[y * 20 + x]) * 3 + 127) / 255
        packed[index / 4] |= UInt8(alpha << ((3 - index % 4) * 2))
    }}
    guard packed.contains(where: { $0 != 0 }) else { skipped += 1; continue }
    let data = Data(packed)
    let offset: Int
    if let existing = pool[data] { offset = existing }
    else { offset = pixels.count; pixels += packed; pool[data] = offset }
    entries.append((text, offset))
}
var output = "// Generated from Noto Emoji (SIL OFL 1.1); see licenses/NotoEmoji-OFL.txt.\n"
output += "static const uint8_t emoji_pixels[] = {\n"
for start in stride(from: 0, to: pixels.count, by: 32) {
    output += pixels[start..<min(start + 32, pixels.count)].map { String(format: "0x%02x", $0) }.joined(separator: ",") + ",\n"
}
output += "};\nstatic const struct { const char *text; uint32_t offset; } emoji_glyphs[] = {\n"
for (text, offset) in entries {
    let escaped = text.utf8.map { String(format: "\\x%02x", $0) }.joined()
    output += "{\"\(escaped)\",\(offset)},\n"
}
output += "};\n"
try output.write(toFile: args[3], atomically: true, encoding: .utf8)
print("Generated \(entries.count) sequences, \(pool.count) distinct glyphs, \(pixels.count) pixel bytes; skipped \(skipped) unsupported sequences")
