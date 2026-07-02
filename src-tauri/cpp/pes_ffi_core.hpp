#pragma once
// Transport-agnostic extraction helpers over the pes engine, shared by BOTH the
// native cxx facade (pes_ffi.cpp) and the wasm/embind web binding
// (wasm/pes_web.cpp). The divergence-prone bits — object-type names, the
// parameter JSON field set, and the stitch run-splitting — live here ONCE so the
// desktop and web builds can never drift apart. No rust:: or emscripten:: types
// here: only std + nlohmann::json + engine types.

#include <cstdint>
#include <cstdio>
#include <string>
#include <vector>

#include "json.hpp"
#include "pesColor.hpp"
#include "pesData.hpp"
#include "pesDocument.hpp"
#include "pesStitchBlock.hpp"
#include "pesPathUtility.hpp" // toSk(pesPath) -> SkPath

#include "include/utils/SkParsePath.h" // SkParsePath::ToSVGString
#include "include/core/SkString.h"

namespace pescore {

inline std::string objectTypeToString(int type) {
    switch (type) {
        case pesData::OBJECT_TYPE_PES2_TEXT:
        case pesData::OBJECT_TYPE_PES:
        case pesData::OBJECT_TYPE_SHAPE: return "Stitch";
        case pesData::OBJECT_TYPE_BACKGROUND: return "Background";
        case pesData::OBJECT_TYPE_SCALABLE_SVG_FILE: return "SVG";
        case pesData::OBJECT_TYPE_SCALABLE_PPEF_TEXT: return "PPEF Text";
        case pesData::OBJECT_TYPE_SCALABLE_TTF_TEXT: return "TTF Text";
        case pesData::OBJECT_TYPE_SCALABLE_SATINCOLUMN: return "Satin Column";
        case pesData::OBJECT_TYPE_SCALABLE_PPEF_TEXT_V2: return "Monogram";
        case pesData::OBJECT_TYPE_SCALABLE_CONTAINER: return "Group";
        default: return "Unknown";
    }
}

// Per-object preview PNG source, shared by get_object_image_png (native) and
// object_png (web). makePesImageSnapshot() only draws scalable paths + stitch
// blocks — a Background object (embedded material-photo texture, no paths, no
// stitches) renders fully transparent through it. The document-level
// composite renderer (pesDocument.cpp's fndrawpes) already special-cases this
// via makePesBackgroundImageSnapshot(); the per-object getters need the same
// dispatch so a Background layer's thumbnail/canvas image isn't blank.
inline sk_sp<SkImage> makeObjectPreviewImage(pesDocument* doc, int index) {
    if (index < 0 || index >= doc->getObjectCount()) return nullptr;
    if (doc->getDataParameter(index).type == pesData::OBJECT_TYPE_BACKGROUND)
        return doc->makePesBackgroundImageSnapshot(index);
    return doc->makePesImageSnapshot(index);
}

inline std::string colorToHex(const pesColor& c) {
    char sz[8];
    std::snprintf(sz, sizeof sz, "#%02X%02X%02X", c.r, c.g, c.b);
    return sz;
}

// Standard base64 (same alphabet/padding as commands.rs base64_encode) — used to
// ship the f32 stitch-coord buffer through the JSON dispatch on the web side.
inline std::string base64(const uint8_t* data, size_t len) {
    static const char* T =
        "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    std::string out;
    out.reserve((len + 2) / 3 * 4);
    for (size_t i = 0; i < len; i += 3) {
        uint32_t n = (uint32_t)data[i] << 16;
        if (i + 1 < len) n |= (uint32_t)data[i + 1] << 8;
        if (i + 2 < len) n |= (uint32_t)data[i + 2];
        out.push_back(T[(n >> 18) & 63]);
        out.push_back(T[(n >> 12) & 63]);
        out.push_back(i + 1 < len ? T[(n >> 6) & 63] : '=');
        out.push_back(i + 2 < len ? T[n & 63] : '=');
    }
    return out;
}

// Properties-panel parameter view (mirrors the old Prop_* handlers). Keeping the
// field set here means desktop and web read identical parameter JSON.
inline nlohmann::json parameterToJson(const pesData::Parameter& p) {
    return nlohmann::json{
        {"text", p.text},
        {"fontName", p.fontName},
        {"fontSize", p.fontSize},
        {"colorIndex", p.colorIndex},
        {"borderColorIndex", p.borderColorIndex},
        {"shapeIndex", p.shapeIndex},
        {"angleValue", p.angleValue},
        {"radiusValue", p.radiusValue},
        {"italic", p.italic},
        {"border", p.border},
        {"borderGap", p.borderGap},
        {"borderGapY", p.borderGapY},
        {"extraLetterSpace", p.extraLetterSpace},
        {"extraSpace", p.extraSpace},
        {"density", p.density},
        {"pullCompensate", p.pullCompensate},
        {"fillTypeIndex", p.fillTypeIndex},
        {"fillColorIndex", p.fillColorIndex},
        {"fillUnderlay", p.fill.underlay},
        {"fillDensity", p.fill.density},
        {"fillDirection", p.fill.sewDirection},
        {"strokeTypeIndex", p.strokeTypeIndex},
        {"strokeRunPitch", p.strokeRunPitch},
        {"strokeWidth", p.strokeWidth},
        {"strokeDensity", p.strokeDensity},
        {"strokeRunningInset", p.strokeRunningInset},
    };
}

// Build a ready-made scalable shape as an SVG-path object centered at the origin
// (= hoop center). These are plain editable vector paths: they scale with the
// Transformer and open in PathEdit/StitchEdit. They carry NO stitches yet
// (fill/stroke types NONE) — the canvas renders the path as a flat shape and the
// user converts it to stitches later. Shared by the native facade and the web
// binding so both targets create byte-identical objects.
// shape_index: 0=line, 1=triangle, 2=rect, 8=ellipse.
inline pesData makeShapeObject(int shape_index) {
    pesData d;
    auto& param = d.parameter;
    param.type = pesData::OBJECT_TYPE_SCALABLE_SVG_FILE;
    param.useColorFromPicker = false;
    param.text = "Shape";
    param.fillType = pesData::FILL_TYPE_NONE;   // no stitches yet
    param.strokeType = pesData::STROKE_TYPE_NONE;

    const float w = 500.f, h = 500.f; // ~50 mm (engine units = 0.1mm)
    // A translucent fill + a solid outline so it reads as a vector shape.
    const pesColor fillColor(79, 157, 222, 0x59);  // #4F9DDE @ ~35%
    const pesColor strokeColor(31, 86, 138, 0xFF); // #1F568A solid
    const float strokeW = 12.f;                    // ~1.2 mm
    const bool isLine = (shape_index == 0);

    pesPath path;
    if (shape_index == 0) { // line
        path.moveTo(-w * 0.5f, 0.f);
        path.lineTo(w * 0.5f, 0.f);
    } else if (shape_index == 1) { // triangle
        path.moveTo(0.f, -h * 0.5f);
        path.lineTo(w * 0.5f, h * 0.5f);
        path.lineTo(-w * 0.5f, h * 0.5f);
        path.close();
    } else if (shape_index == 8) { // ellipse — 4 cubic béziers (toSk converts
        // these; pesPath::arc emits an _arc command that toSk does NOT render)
        const float rx = w * 0.5f, ry = h * 0.5f;
        const float k = 0.5522847498f; // circle bézier constant (4/3·tan(π/8))
        path.moveTo(rx, 0.f);
        path.bezierTo(pesVec2f(rx, -ry * k), pesVec2f(rx * k, -ry), pesVec2f(0.f, -ry));
        path.bezierTo(pesVec2f(-rx * k, -ry), pesVec2f(-rx, -ry * k), pesVec2f(-rx, 0.f));
        path.bezierTo(pesVec2f(-rx, ry * k), pesVec2f(-rx * k, ry), pesVec2f(0.f, ry));
        path.bezierTo(pesVec2f(rx * k, ry), pesVec2f(rx, ry * k), pesVec2f(rx, 0.f));
        path.close();
    } else { // rect (2)
        path.rectangle(-w * 0.5f, -h * 0.5f, w, h);
    }

    if (isLine) {
        path.setFilled(false);
        path.setStrokeColor(strokeColor);
        path.setStrokeWidth(strokeW);
    } else {
        path.setFilled(true);
        path.setFillColor(fillColor);
        path.setStrokeColor(strokeColor); // visible outline
        path.setStrokeWidth(strokeW);
    }
    param.colorIndex = 20; // black (used once converted to stitches)

    d.paths.push_back(path);
    d.recalculate(); // compute bbox from the path (no stitch generation)
    return d;
}

// True if the object carries any actual stitches (non-empty stitch blocks).
// Drives the "render as crisp vector vs. as a stitched PNG" decision: an SVG
// shape with no fill/stroke assigned yet has none.
inline bool objectHasStitches(pesData& d) {
    for (auto& b : d.fillBlocks)
        if (b.size() > 0) return true;
    for (auto& b : d.strokeBlocks)
        if (b.size() > 0) return true;
    return false;
}

// Vector geometry for the frontend to draw scalable shapes as crisp Konva paths
// (no raster, no stroke clipping). Each path → an SVG `d` string in ABSOLUTE
// world coords (engine units) plus fill/stroke paint. Shared so desktop/web emit
// identical geometry.
inline nlohmann::json objectVectorJson(pesData& d) {
    nlohmann::json paths = nlohmann::json::array();
    for (auto& p : d.paths) {
        if (!p.bVisible) continue;
        SkPath sk = toSk(p);
        nlohmann::json jp;
        jp["d"] = std::string(SkParsePath::ToSVGString(sk).c_str());
        // path bbox in world coords — lets the frontend place an SVG gradient
        // (objectBoundingBox units) over the shape.
        SkRect r = sk.getBounds();
        jp["bbox"] = {r.x(), r.y(), r.width(), r.height()};
        jp["fillRule"] = (p.fillRule == 1) ? "evenodd" : "nonzero";
        if (p.isFill()) {
            pesColor c = p.getFillColor();
            jp["fill"] = colorToHex(c);
            jp["fillOpacity"] = c.a / 255.0;
        }
        if (p.isStroke()) {
            pesColor c = p.getStrokeColor();
            jp["stroke"] = colorToHex(c);
            jp["strokeOpacity"] = c.a / 255.0;
            jp["strokeWidth"] = p.getStrokeWidth();
        }
        paths.push_back(jp);
    }
    return nlohmann::json{{"paths", paths}};
}

// Apply a parameter change to a scalable SVG object: push the palette fill/stroke
// colors down onto its paths (the fill keeps its design alpha) and regenerate
// fill/stroke stitches per the current fill/stroke types (NONE → stays a plain
// vector shape; NORMAL/etc. → real stitches). This is the SVG counterpart of
// update_ppef_text / update_ttf_text. Returns false for non-SVG objects.
inline bool updateSvgObject(pesData& d) {
    auto& param = d.parameter;
    if (param.type != pesData::OBJECT_TYPE_SCALABLE_SVG_FILE) return false;
    pesColor fillC = pesGetBrotherColor(param.fillColorIndex);
    pesColor strokeC = pesGetBrotherColor(param.colorIndex);
    for (auto& p : d.paths) {
        if (p.isFill()) {
            pesColor c = fillC;
            c.a = p.getFillColor().a; // preserve the shape's translucency
            p.setFillColor(c);
        }
        if (p.isStroke()) p.setStrokeColor(strokeC);
    }
    d.applyFill();   // honors param.fillType (NONE → no fill stitches)
    d.applyStroke(); // honors param.strokeType (NONE → no stroke stitches)
    d.recalculate();
    return true;
}

// Ordered stitch geometry. `coords` is x,y pairs (engine units, 0.1mm); each
// segment indexes into it. Jumps/trims break a thread line into runs but still
// count toward the simulator's total length.
struct StitchSeg {
    std::string hex;
    uint32_t start;  // point offset (pair index) into coords
    uint32_t count;  // number of points in this run
};
struct StitchGeom {
    std::vector<StitchSeg> segments;
    std::vector<float> coords;
    uint32_t total_points = 0;
};

inline StitchGeom buildStitchData(pesDocument* doc, int obj_index) {
    StitchGeom out;
    int count = doc->getObjectCount();

    auto appendObject = [&](int idx) {
        auto data = doc->getDataObject(idx);
        if (!data->parameter.visible)
            return;
        pesStitchBlockList blocks;
        data->getStitchBlockList(blocks);
        for (auto& block : blocks) {
            std::string hex = colorToHex(block.color);
            size_t n = block.polyline.size();
            size_t i = 0;
            while (i < n) {
                bool jump = i < block.types.size() && block.types[i] != 0;
                if (jump) {  // jump points still count toward simulator length
                    out.coords.push_back(block.polyline[(int)i].x);
                    out.coords.push_back(block.polyline[(int)i].y);
                    out.total_points += 1;
                    ++i;
                    continue;
                }
                uint32_t start = (uint32_t)(out.coords.size() / 2);
                uint32_t segCount = 0;
                while (i < n &&
                       !(i < block.types.size() && block.types[i] != 0)) {
                    out.coords.push_back(block.polyline[(int)i].x);
                    out.coords.push_back(block.polyline[(int)i].y);
                    ++segCount;
                    ++i;
                }
                if (segCount > 0) {
                    out.segments.push_back(StitchSeg{hex, start, segCount});
                    out.total_points += segCount;
                }
            }
        }
    };

    if (obj_index >= 0 && obj_index < count) {
        appendObject(obj_index);
    } else {
        for (int i = 0; i < count; ++i)
            appendObject(i);
    }
    return out;
}

}  // namespace pescore
