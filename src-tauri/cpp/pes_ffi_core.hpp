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
