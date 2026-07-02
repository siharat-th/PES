#pragma once
// Transport-agnostic Smart Satin seams, shared by the native cxx facade
// (pes_ffi.cpp) and the wasm/embind web binding (wasm/pes_web.cpp) — same
// pattern as pes_text_core.hpp.
//
// Smart Satin (TTF/SVG outline -> satin columns) is a two-layer feature:
// the GEOMETRY (straight skeleton -> centerline -> column rails) runs in the
// vendored JS core (public/satin/satin-core.js, from the old app's
// api-satin-helper.js) and stays byte-identical to production; the ENGINE
// seams below replace its CanvasKit boundary calls:
//
//   satinSource()      <- pesPath clone/scale/simplify/getOutline prep
//                         (api-satin-helper.js:7860-7958)
//   simplifyPolygons() <- CanvasKit MakeFromSVGString+simplify+toCanvas trick
//                         (api-satin-helper.js:8009-8017)
//   addSatinObjects()  <- apiWorkerAddSatinColumnToLayer (:7737-7826) +
//                         the density/pull params from autoSmartSatin
//                         (appinit.js:5609)
//
// A satin-column object is a pesData whose paths are RAIL PAIRS in order —
// pesData::applyFill pairs them consecutively (paths[0]+paths[1], ...) and
// zigzags between the rails (pesData.cpp FILL_TYPE_SATIN_COLUMN branch).

#include <algorithm>
#include <cmath>
#include <string>

#include "json.hpp"
#include "pesData.hpp"
#include "pesDocument.hpp"
#include "pesPathUtility.hpp" // toPes(SkPath)
#include "pes_ffi_core.hpp"   // colorToHex

#include "include/core/SkPath.h"
#include "include/utils/SkParsePath.h"
#include "skia-ext/pes_skpath_compat.h" // PesPath (SkPath is immutable in M150)

namespace pescore {

// Per-path prep for the JS geometry core — port of the input side of
// apiWorkerConvertLayerToSatinColumn: clone each visible+filled path, nudge
// one axis by 1.002 (avoids degenerate skeletons on axis-aligned edges),
// pathops-simplify, then flatten via the engine's own getOutline() so both
// targets (and the old app) see identical polygons.
// Returns {"istext", "rotateDegree", "paths": [{"polygons", "colorHex",
// "center", "scale", "simplifyValue"}]}.
inline nlohmann::json satinSource(pesData& data) {
    using nlohmann::json;
    bool istext = data.parameter.type == pesData::OBJECT_TYPE_SCALABLE_TTF_TEXT;
    json jpaths = json::array();

    for (auto& pp : data.paths) {
        if (!pp.bVisible || !pp.isFill())
            continue;

        pesPath ppclone = pp;
        pesRectangle bb = ppclone.getBoundingBox();
        float minside = std::min(bb.width, bb.height);
        pesVec2f center = bb.getCenter();

        float scalew = 1.f, scaleh = 1.f;
        if (istext) {
            if (minside == bb.height) scalew = 1.002f; else scaleh = 1.002f;
        } else {
            if (minside == bb.height) scaleh = 1.002f; else scalew = 1.002f;
        }

        // +(minside ** 0.2 / 10).toFixed(1), clamped for non-text
        float simplifyValue = std::round(std::pow(minside, 0.2f) / 10.f * 10.f) / 10.f;
        if (istext) {
            simplifyValue = 0.1f;
        } else {
            simplifyValue -= 0.1f;
            simplifyValue = std::min(1.0f, std::max(0.1f, simplifyValue));
        }

        ppclone.scale(scalew, scaleh);
        ppclone.simplify(0, true);

        json jpolys = json::array();
        for (const auto& ol : ppclone.getOutline()) {
            json ring = json::array();
            for (const auto& v : ol.getVertices())
                ring.push_back({v.x, v.y});
            jpolys.push_back(std::move(ring));
        }
        if (jpolys.empty())
            continue;

        jpaths.push_back({
            {"polygons", std::move(jpolys)},
            {"colorHex", colorToHex(pp.getFillColor())},
            {"center", {center.x, center.y}},
            {"scale", {scalew, scaleh}},
            {"simplifyValue", simplifyValue},
        });
    }

    return json{{"istext", istext},
                {"rotateDegree", data.parameter.rotateDegree},
                {"paths", std::move(jpaths)}};
}

// Pathops-simplify a set of rings and return the resulting subpaths as rings —
// replaces the JS trick of replaying a simplified CanvasKit path into a fake
// canvas ctx (moveTo starts a ring, closePath re-appends the first vertex).
// Input/output: [[[x,y], ...], ...].
inline nlohmann::json simplifyPolygons(const nlohmann::json& rings) {
    using nlohmann::json;
    PesPath p;
    for (const auto& ring : rings) {
        if (!ring.is_array() || ring.size() < 2) continue;
        p.moveTo(ring[0][0].get<float>(), ring[0][1].get<float>());
        for (size_t i = 1; i < ring.size(); i++)
            p.lineTo(ring[i][0].get<float>(), ring[i][1].get<float>());
    }
    p.setFillType(SkPathFillType::kWinding);
    p.simplify();

    json out = json::array();
    json cur = json::array();
    SkPath flat = p; // simplified line-only path
    SkPath::Iter iter(flat, false);
    SkPoint pts[4];
    for (SkPath::Verb verb; (verb = iter.next(pts)) != SkPath::kDone_Verb;) {
        switch (verb) {
            case SkPath::kMove_Verb:
                if (!cur.empty()) { out.push_back(cur); cur = json::array(); }
                cur.push_back({pts[0].x(), pts[0].y()});
                break;
            case SkPath::kLine_Verb:
                cur.push_back({pts[1].x(), pts[1].y()});
                break;
            case SkPath::kClose_Verb:
                if (!cur.empty()) {
                    auto& first = cur[0];
                    auto& last = cur[cur.size() - 1];
                    if (first[0] != last[0] || first[1] != last[1])
                        cur.push_back({first[0].get<float>(), first[1].get<float>()});
                }
                break;
            default: // simplify of line input emits no curves
                break;
        }
    }
    if (!cur.empty()) out.push_back(cur);
    return out;
}

// Build satin-column objects from the JS core's output and append them to the
// document. `objects` is an array of
//   {"rails": [[d0, d1], ...],          // SVG path d-string rail pairs
//    "colorIndex": 11,
//    "center": [x, y], "scale": [sx, sy], "rotateDegree": 0,
//    "density": 2.5, "pullCompensate": 0, "noneOverlap": false}
// Returns the number of objects added.
inline int addSatinObjects(pesDocument* doc, const nlohmann::json& objects) {
    int added = 0;
    for (const auto& jo : objects) {
        pesData pes;

        for (const auto& rail : jo.value("rails", nlohmann::json::array())) {
            if (!rail.is_array() || rail.size() != 2) continue;
            std::string d0 = rail[0].get<std::string>();
            std::string d1 = rail[1].get<std::string>();
            if (d0.empty() || d1.empty()) continue;
            SkPath skp0, skp1;
            if (!SkParsePath::FromSVGString(d0.c_str(), &skp0)) continue;
            if (!SkParsePath::FromSVGString(d1.c_str(), &skp1)) continue;
            pesPath pp0 = toPes(skp0);
            pesPath pp1 = toPes(skp1);
            pp0.setStrokeWidth(1.5f);
            pp1.setStrokeWidth(1.5f);
            pp0.setStrokeHexColor(0x6494ed); // rail A (cornflower)
            pp1.setStrokeHexColor(0xffa500); // rail B (orange)
            pes.paths.push_back(pp0);
            pes.paths.push_back(pp1);
        }
        if (pes.paths.size() < 2)
            continue;

        auto& param = pes.parameter;
        param.setType(pesData::OBJECT_TYPE_SCALABLE_SATINCOLUMN);
        param.isSatinColumnPath = true; // setType only flags PPEF/TTF
        param.fillType = pesData::FILL_TYPE_SATIN_COLUMN;
        param.strokeType = pesData::STROKE_TYPE_NONE;
        param.text = "Smart";
        param.rotateDegree = jo.value("rotateDegree", 0.f);
        param.fillColorIndex = jo.value("colorIndex", 11);
        param.fill.density = jo.value("density", 2.5f);
        param.pullCompensate = jo.value("pullCompensate", 0.f);
        param.autoSatinFill.noneOverlap = jo.value("noneOverlap", false);

        // undo the 1.002 prep nudge and restore the source path's center
        if (jo.contains("scale") && jo.contains("center")) {
            float sx = jo["scale"][0].get<float>();
            float sy = jo["scale"][1].get<float>();
            if (sx != 0.f && sy != 0.f)
                pes.scale(1.f / sx, 1.f / sy);
            pesVec2f nc = pes.getBoundingBox().getCenter();
            pes.translate(jo["center"][0].get<float>() - nc.x,
                          jo["center"][1].get<float>() - nc.y);
        }

        pes.applyFill();
        doc->addObject(pes);
        added++;
    }
    return added;
}

} // namespace pescore
