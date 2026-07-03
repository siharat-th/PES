#pragma once
// Auto Punch engine seams, shared by the native cxx facade (pes_ffi.cpp) and
// the wasm/embind web binding (wasm/pes_web.cpp) — same pattern as
// pes_satin_core.hpp.
//
// The tracing itself (color quantization + vectorization) runs in the
// frontend tracer wasm (tracer/ crate -> public/tracer/, driven by
// src/punch/tracerClient.ts). These seams turn its output — per-color SVG
// d-strings in trace-work pixel coordinates — into stitched engine objects
// (one object per thread color, optionally grouped), and import the source
// photo as a locked Background object.

#include <cstring>
#include <string>
#include <vector>

#include "json.hpp"
#include "pesData.hpp"
#include "pesDocument.hpp"
#include "pesPathUtility.hpp" // toPes

#include "include/core/SkMatrix.h"
#include "include/core/SkPath.h"
#include "include/utils/SkParsePath.h"
#include "skia-ext/pes_skpath_compat.h" // PesPath (SkPath is immutable in M150)

namespace pescore {

// Standard base64 decode (counterpart of pescore::base64 in pes_ffi_core.hpp).
// The background PNG crosses BOTH transports as base64 inside the JSON args —
// one identical code path, no extra embind export. Whitespace tolerated,
// anything else invalid -> empty vector.
inline std::vector<uint8_t> base64Decode(const std::string& s) {
    auto val = [](char c) -> int {
        if (c >= 'A' && c <= 'Z') return c - 'A';
        if (c >= 'a' && c <= 'z') return c - 'a' + 26;
        if (c >= '0' && c <= '9') return c - '0' + 52;
        if (c == '+') return 62;
        if (c == '/') return 63;
        return -1;
    };
    std::vector<uint8_t> out;
    out.reserve(s.size() / 4 * 3);
    uint32_t acc = 0;
    int bits = 0;
    for (char c : s) {
        if (c == '=' || c == '\n' || c == '\r' || c == ' ') continue;
        int v = val(c);
        if (v < 0) return {};
        acc = (acc << 6) | (uint32_t)v;
        bits += 6;
        if (bits >= 8) {
            bits -= 8;
            out.push_back((uint8_t)(acc >> bits));
        }
    }
    return out;
}

struct PunchResult {
    std::vector<int> newIndices;
    int groupId = -1; // -1 = no group was created
};

// "#rrggbb" -> pesColor (white on parse failure)
inline pesColor punchHexColor(const std::string& hex) {
    unsigned v = 0xffffff;
    if (hex.size() == 7 && hex[0] == '#') v = (unsigned)strtoul(hex.c_str() + 1, nullptr, 16);
    return pesColor((v >> 16) & 0xff, (v >> 8) & 0xff, v & 0xff);
}

// Build per-color embroidery fill objects from the tracer's output and append
// them (bottom-up, spec order) as ONE logical step — group creation happens
// in here, not as a separate command, so undo restores everything at once
// (same reasoning as duplicate_objects in commands.rs). `spec`:
//   {"imageSize": [w, h],            // trace working resolution, px
//    "outputWidthMm": 100.0,         // physical width; height follows aspect
//    "groupName": "Auto Punch",      // optional; absent/empty -> no group
//    "fillDensity": 2.5,             // lines/mm, engine default 2.5
//    "sewDirection": 0,
//    "objects": [{"paths": ["M...Z", ...],  // absolute px, holes = subpaths
//                 "rgb": "#a01830",
//                 "colorIndex": 27,   // Brother 1..65; <0 -> nearest match
//                 "fillType": 1}]}    // fillTypeIndex: 1=NORMAL, 0=NONE
// All objects share ONE uniform scale (image px -> engine 0.1mm units) and
// ONE translation (image center -> hoop origin); per-object recentering would
// misalign the color layers against each other.
inline PunchResult addPunchObjects(pesDocument* doc, const nlohmann::json& spec) {
    PunchResult res;
    if (!spec.contains("objects") || !spec["objects"].is_array()) return res;

    float imageW = 0.f, imageH = 0.f;
    if (spec.contains("imageSize") && spec["imageSize"].is_array() && spec["imageSize"].size() == 2) {
        imageW = spec["imageSize"][0].get<float>();
        imageH = spec["imageSize"][1].get<float>();
    }
    if (imageW <= 0.f || imageH <= 0.f) return res;

    float outputWidthMm = spec.value("outputWidthMm", 100.f);
    float density = spec.value("fillDensity", 2.5f);
    float sewDirection = spec.value("sewDirection", 0.f);
    float s = outputWidthMm * 10.f / imageW; // px -> engine units (0.1mm)

    for (const auto& jo : spec["objects"]) {
        if (!jo.contains("paths") || !jo["paths"].is_array()) continue;

        int colorIndex = jo.value("colorIndex", -1);
        pesColor rgb = punchHexColor(jo.value("rgb", std::string("#ffffff")));
        if (colorIndex < 0) colorIndex = pesGetNearestBrotherColorIndex(rgb);
        pesColor threadColor = pesGetBrotherColor(colorIndex);
        int fillTypeIndex = jo.value("fillType", 1);

        // Bake the shared frame into the path coordinates at parse time
        // (px -> engine units, image center -> hoop origin). pesData::scale()
        // can't be used here: it pins the object's own bbox min in place,
        // which would re-frame every color layer independently.
        SkMatrix frame = SkMatrix::Scale(s, s);
        frame.postTranslate(-imageW * s / 2.f, -imageH * s / 2.f);

        pesData pes;
        for (const auto& jd : jo["paths"]) {
            std::string d = jd.get<std::string>();
            if (d.empty()) continue;
            SkPath skp;
            if (!SkParsePath::FromSVGString(d.c_str(), &skp)) continue;
            // pesEMBFill fills with pesPath::fillRule = 0 (nonzero winding), so
            // hole subpaths must be opposite-wound. Interpret the tracer output
            // as even-odd (winding-agnostic for boundary+holes) and let pathops
            // Simplify rewrite it into a winding-correct, intersection-free path.
            PesPath norm(skp.makeTransform(frame));
            norm.setFillType(SkPathFillType::kEvenOdd);
            norm.simplify();
            pesPath pp = toPes(norm);
            pp.setFilled(true);
            pp.setFillColor(threadColor);
            pes.paths.push_back(pp);
        }
        if (pes.paths.empty()) continue;

        auto& param = pes.parameter;
        param.setType(pesData::OBJECT_TYPE_SCALABLE_SVG_FILE);
        param.useColorFromPicker = false;
        param.fillTypeIndex = fillTypeIndex;
        param.fillType = (pesData::FillType)(pesData::FILL_TYPE_NONE + fillTypeIndex);
        param.strokeTypeIndex = 0;
        param.strokeType = pesData::STROKE_TYPE_NONE;
        // fillColorIndex/colorIndex must agree with the path colors, or the
        // first set_parameter -> updateSvgObject would visibly recolor
        param.fillColorIndex = colorIndex;
        param.colorIndex = colorIndex;
        param.fill.density = density;
        param.fill.sewDirection = sewDirection;
        param.text = std::string("Punch ") + pesGetBrotherColorName(colorIndex);

        pes.applyFill();   // honors fillType (NONE -> plain vector, no stitches)
        pes.applyStroke(); // STROKE_TYPE_NONE -> no-op, keeps updateSvgObject parity
        pes.recalculate();

        doc->addObject(pes);
        res.newIndices.push_back(doc->getObjectCount() - 1);
    }

    std::string groupName = spec.value("groupName", std::string());
    if (!groupName.empty() && !res.newIndices.empty()) {
        res.groupId = doc->createGroup(groupName, 0);
        for (int i : res.newIndices) doc->setObjectGroup(i, res.groupId);
    }
    return res;
}

// PNG bytes -> locked Background object, moved to index 0 (back-most).
// PNG ONLY: the PPES writer dumps parameter.backgroundBuffer raw and the
// reader re-finds it by scanning for the PNG magic (pesDocument.cpp) — a JPEG
// would corrupt every save/undo snapshot. Returns the object's index (0) or
// -1 on failure.
inline int importBackground(pesDocument* doc, const std::vector<uint8_t>& bytes) {
    static const uint8_t PNG_MAGIC[8] = {0x89, 'P', 'N', 'G', 0x0d, 0x0a, 0x1a, 0x0a};
    if (bytes.size() < 8 || std::memcmp(bytes.data(), PNG_MAGIC, 8) != 0) return -1;

    pesBuffer buf((const char*)bytes.data(), bytes.size());
    pesData d;
    if (!d.loadBackgroundFromBuffer(buf, true)) return -1;
    doc->addObject(d);
    for (int cur = doc->getObjectCount() - 1; cur > 0 && doc->moveObjectBack(cur); --cur) {
    }
    return 0;
}

} // namespace pescore
