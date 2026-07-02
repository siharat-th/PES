#pragma once
// Transport-agnostic PPEF/TTF text creation/rebuild, shared by the native cxx
// facade (pes_ffi.cpp) and the wasm/embind web binding (wasm/pes_web.cpp) —
// same pattern as pes_edit_core.hpp: ONE copy so desktop and web re-shape text
// identically. Port of updatePPEFText (Victor-frontend pes5.html:125-310),
// PES5_AddPPEFText (PES5Command.cpp:1541), PES5_AddTTFText (:1632) and
// PES5_ReplaceTTFText (:2081), using the native PPEF_Reader (SQLiteCpp)
// instead of sql.js. On wasm, fonts are read from MEMFS — the frontend fetches
// missing .ppef/.ttf files on demand (see webEngine.ts).

#include <cmath>
#include <cstdio>
#include <memory>
#include <string>
#include <vector>

#include "pesData.hpp"
#include "pesEffect.hpp"
#include "pesPathUtility.hpp" // toPes(SkPath)
#include "PesPPEFUtils.hpp"   // PPEF_Reader

#include "include/core/SkData.h"
#include "include/core/SkFont.h"
#include "include/core/SkFontMgr.h"
#include "include/core/SkString.h"
#include "include/core/SkTypeface.h"
#include "include/utils/SkTextUtils.h"

// SkTypeface::MakeFromData was removed in Skia M150; typefaces are built from
// font bytes through a font manager. Native uses the platform manager; wasm
// uses the FreeType "empty" manager (no system fonts, data-only) — the wasm
// Skia is built with SK_FONTMGR_FREETYPE_EMPTY_AVAILABLE (see build-web.sh).
#if defined(__EMSCRIPTEN__)
#include "include/ports/SkFontMgr_empty.h"
#elif defined(__APPLE__)
#include "include/ports/SkFontMgr_mac_ct.h"
#elif defined(_WIN32)
#include "include/ports/SkTypeface_win.h"
#endif

// Resource API implemented in pes_resources.cpp (both targets).
SkString GetResourcePath(const char* resource);
sk_sp<SkData> GetResourceAsData(const char* resource);

namespace pescore {

inline std::string ppefFontPath(const std::string& fontName) {
    return GetResourcePath(("PPEF/" + fontName + ".ppef").c_str()).c_str();
}

// The web binding checks this before any command that re-shapes text, so the
// frontend can fetch the font into MEMFS and retry (PPEF fonts are NOT
// preloaded into pes_web.data — 42MB across 136 files).
inline bool ppefFontAvailable(const std::string& fontName) {
    std::FILE* f = std::fopen(ppefFontPath(fontName).c_str(), "rb");
    if (!f) return false;
    std::fclose(f);
    return true;
}

// Effect selection mirrors pes5.html shapeIndex mapping (0..15 contiguous).
inline std::unique_ptr<pesEffect> makePpefEffect(const pesData::Parameter& param) {
    std::unique_ptr<pesEffect> effect;
    switch (param.shapeIndex) {
        case 0: effect = std::make_unique<pesEffectNormal>(); break;
        case 1: {
            auto e = std::make_unique<pesEffectArchTop>();
            e->angle = param.angleValue;
            e->radius = param.radiusValue;
            effect = std::move(e);
            break;
        }
        case 2: {
            auto e = std::make_unique<pesEffectArchBottom>();
            e->angle = param.angleValue;
            e->radius = param.radiusValue;
            effect = std::move(e);
            break;
        }
        case 3: effect = std::make_unique<pesEffectCircle>(); break;
        case 4: {
            auto e = std::make_unique<pesEffectSineWave>();
            e->magnitude = param.waveMagnitude;
            effect = std::move(e);
            break;
        }
        case 5: effect = std::make_unique<pesEffectChevron>(true); break;
        case 6: effect = std::make_unique<pesEffectChevron>(false); break;
        case 7: {
            auto e = std::make_unique<pesEffectSlant>(true);
            e->angle = param.slantUpAngle;
            effect = std::move(e);
            break;
        }
        case 8: {
            auto e = std::make_unique<pesEffectSlant>(false);
            e->angle = param.slantDownAngle;
            effect = std::move(e);
            break;
        }
        case 9: effect = std::make_unique<pesEffectTriangleUp>(); break;
        case 10: effect = std::make_unique<pesEffectTriangleDown>(); break;
        case 11: effect = std::make_unique<pesEffectFadeRight>(); break;
        case 12: effect = std::make_unique<pesEffectFadeLeft>(); break;
        case 13: {
            auto e = std::make_unique<pesEffectFadeUp>();
            e->slantFactor = param.fadeUpSlant;
            effect = std::move(e);
            break;
        }
        case 14: {
            auto e = std::make_unique<pesEffectFadeDown>();
            e->slantFactor = param.fadeDownSlant;
            effect = std::move(e);
            break;
        }
        case 15: effect = std::make_unique<pesEffectInflate>(); break;
        default: effect = std::make_unique<pesEffectNormal>(); break;
    }
    return effect;
}

// Rebuild a PPEF text object's paths/stitches from its parameter block,
// preserving the bbox center. Returns false for non-PPEF objects, empty text,
// or a missing/unshapeable font.
inline bool rebuildPpefText(pesData& data) {
    auto& param = data.parameter;
    if (param.type != pesData::OBJECT_TYPE_SCALABLE_PPEF_TEXT)
        return false;

    std::string text = param.text;
    while (!text.empty() && text.back() == ' ') text.pop_back();
    while (!text.empty() && text.front() == ' ') text.erase(0, 1);
    if (text.empty())
        return false;

    std::string fontName = param.fontName.empty() ? "Thai001" : param.fontName;
    if (!ppefFontAvailable(fontName))
        return false; // SQLite would throw across the FFI boundary otherwise

    pesVec2f oldCenter = data.getBoundingBox().getCenter();

    std::vector<pesPath> shapes;
    try {
        PPEF_Reader ppef(ppefFontPath(fontName).c_str());
        ppef.readPPEFConfig();
        // scale=1.0 matches the native reference (PES5_AddPPEFText #else branch)
        shapes = ppef.getStringAsShapes(
            text, 1.0f, param.extraLetterSpace, param.extraSpace);
    } catch (...) {
        return false; // unreadable/corrupt .ppef — leave the object untouched
    }
    if (shapes.empty())
        return false;

    std::unique_ptr<pesEffect> effect = makePpefEffect(param);
    if (effect) {
        effect->bItalic = param.italic;
        effect->bCreateBorder = param.border;
        effect->borderGap = 100 + param.borderGap;
        effect->borderGapY = 100 + param.borderGapY;
        effect->applyPaths(shapes);
        // arch effects normalize angle/radius while applying — persist them
        if (param.shapeIndex == 1) {
            auto* arch = static_cast<pesEffectArchTop*>(effect.get());
            param.angleValue = arch->angle;
            param.radiusValue = arch->radius;
        } else if (param.shapeIndex == 2) {
            auto* arch = static_cast<pesEffectArchBottom*>(effect.get());
            param.angleValue = arch->angle;
            param.radiusValue = arch->radius;
        }
    }

    data.paths = shapes;
    // pesData::scale multiplies parameter.ppefScaleX/Y cumulatively, so work
    // on a copy and write it back afterwards — exactly like the JS original
    // (pes5.html:256-263) — otherwise every update corrupts the scale state.
    pesData::Parameter paramCopy = param;
    const float unit_per_mm = 10.f;
    float s = (1.f / 300.f) * (paramCopy.fontSize * unit_per_mm);
    if (paramCopy.lastFontSize != paramCopy.fontSize) {
        paramCopy.lastFontSize = paramCopy.fontSize;
        paramCopy.ppefScaleX = paramCopy.ppefScaleY = 1.0f;
    }
    data.scale(s * paramCopy.ppefScaleX, s * paramCopy.ppefScaleY);
    data.parameter = paramCopy;

    data.applyPPEFFill();

    pesVec2f newCenter = data.getBoundingBox().getCenter();
    data.translate(oldCenter.x - newCenter.x, oldCenter.y - newCenter.y);
    return true;
}

// Build a fresh PPEF text object centered at the hoop origin — port of
// PES5_AddPPEFText (PES5Command.cpp:1541 #else branch): satin-column fill in
// Deep Gold (Brother index 11), no stroke. `out` must be default-constructed.
inline bool makePpefTextObject(pesData& out, const std::string& text,
                               const std::string& fontName) {
    auto& param = out.parameter;
    param.setType(pesData::OBJECT_TYPE_SCALABLE_PPEF_TEXT); // sets isPPEFPath
    param.text = text;
    param.fontName = fontName;
    param.fillColorIndex = 11; // Deep Gold
    param.fillType = pesData::FILL_TYPE_SATIN_COLUMN;
    param.strokeType = pesData::STROKE_TYPE_NONE;
    if (!rebuildPpefText(out))
        return false;
    pesVec2f c = out.getBoundingBox().getCenter();
    if (c.x != 0.f || c.y != 0.f)
        out.translate(-c.x, -c.y);
    return true;
}

// ---- TTF text (SkTypeface outline) -----------------------------------------

inline std::string ttfFontPath(const std::string& fontName) {
    return GetResourcePath(("TTF/" + fontName + ".ttf").c_str()).c_str();
}

// Same contract as ppefFontAvailable: the web binding checks this before any
// command that shapes TTF text, so the frontend can fetch the .ttf into MEMFS
// and retry (the 209MB TTF tree is NOT preloaded into pes_web.data).
inline bool ttfFontAvailable(const std::string& fontName) {
    std::FILE* f = std::fopen(ttfFontPath(fontName).c_str(), "rb");
    if (!f) return false;
    std::fclose(f);
    return true;
}

inline sk_sp<SkFontMgr> systemFontMgr() {
#if defined(__EMSCRIPTEN__)
    static sk_sp<SkFontMgr> mgr = SkFontMgr_New_Custom_Empty();
#elif defined(__APPLE__)
    static sk_sp<SkFontMgr> mgr = SkFontMgr_New_CoreText(nullptr);
#elif defined(_WIN32)
    static sk_sp<SkFontMgr> mgr = SkFontMgr_New_DirectWrite();
#endif
    return mgr;
}

inline sk_sp<SkTypeface> makeTtfTypeface(const std::string& fontName) {
    sk_sp<SkData> fontData = GetResourceAsData(("TTF/" + fontName + ".ttf").c_str());
    if (!fontData)
        return nullptr;
    return systemFontMgr()->makeFromData(fontData);
}

// Text outline as one filled pesPath, normalized so glyph "0" is exactly
// fontSize mm tall (the old app's sizing trick — ptSize alone is font-relative).
inline pesPath makeTtfPath(const sk_sp<SkTypeface>& typeface, const std::string& text,
                           float fontSize, int fillColorIndex, int strokeColorIndex) {
    SkScalar ptSize = fontSize * 10;
    {
        SkFont font(typeface, ptSize);
        SkPath zero;
        SkTextUtils::GetPath("0", 1, SkTextEncoding::kUTF8, 0, 0, font, &zero);
        auto h = std::abs(zero.getBounds().height());
        if (h > 0)
            ptSize = ptSize * (ptSize / h);
    }
    SkFont font(typeface, ptSize);
    SkPath skPath;
    SkTextUtils::GetPath(text.c_str(), text.length(), SkTextEncoding::kUTF8, 0, 0,
                         font, &skPath);
    pesPath path = toPes(skPath);
    path.setFilled(true);
    path.setStrokeWidth(2);
    path.setFillColor(pesGetBrotherColor(fillColorIndex));
    path.setStrokeColor(pesGetBrotherColor(strokeColorIndex));
    return path;
}

// Rebuild a TTF text object's path from its parameters — port of
// PES5_ReplaceTTFText (PES5Command.cpp:2081-2136), loading the typeface from
// bundled resources (TTF/<fontName>.ttf) instead of the sk_ui font combobox.
inline bool rebuildTtfText(pesData& data) {
    auto& param = data.parameter;
    if (param.type != pesData::OBJECT_TYPE_SCALABLE_TTF_TEXT)
        return false;
    if (param.text.empty() || data.paths.empty())
        return false;

    sk_sp<SkTypeface> typeface = makeTtfTypeface(param.fontName);
    if (!typeface)
        return false; // font file missing — leave object untouched

    pesVec2f oldCenter = data.getBoundingBox().getCenter();
    pesPath path = makeTtfPath(typeface, std::string(param.text), param.fontSize,
                               param.fillColorIndex, param.colorIndex);
    path.fillRule = data.paths[0].fillRule;
    data.paths.clear();
    data.paths.push_back(path);

    if (param.lastFontSize != param.fontSize) {
        param.lastFontSize = param.fontSize;
        param.ppefScaleX = param.ppefScaleY = 1.f;
    }
    // pesData::scale mutates ppefScaleX/Y cumulatively — backup & rollback
    float sx = param.ppefScaleX;
    float sy = param.ppefScaleY;
    data.scale(param.ppefScaleX, param.ppefScaleY);
    param.ppefScaleX = sx;
    param.ppefScaleY = sy;

    // refresh dynamic stroke types (same trick as the old code)
    data.scale(1.f, 1.f);
    param.ppefScaleX = sx;
    param.ppefScaleY = sy;

    pesVec2f newCenter = data.getBoundingBox().getCenter();
    data.translate(oldCenter.x - newCenter.x, oldCenter.y - newCenter.y);
    return true;
}

// Build a fresh TTF text object centered at the hoop origin — port of
// PES5_AddTTFText (PES5Command.cpp:1632): one filled outline path in Deep Gold
// with a Dark Grey stroke. Like shapes, the object starts as vector-only (no
// stitches yet). `out` must be default-constructed.
inline bool makeTtfTextObject(pesData& out, const std::string& text,
                              const std::string& fontName) {
    if (text.empty())
        return false;
    sk_sp<SkTypeface> typeface = makeTtfTypeface(fontName);
    if (!typeface)
        return false;

    auto& param = out.parameter;
    param.setType(pesData::OBJECT_TYPE_SCALABLE_TTF_TEXT); // sets isTTFPath
    param.text = text;
    param.fontName = fontName;
    param.fillColorIndex = 11; // Deep Gold
    param.colorIndex = 23;     // Dark Grey
    pesPath path = makeTtfPath(typeface, text, param.fontSize,
                               param.fillColorIndex, param.colorIndex);
    if (path.getCommands().empty())
        return false; // font has none of the requested glyphs
    out.paths.push_back(path);
    param.lastFontSize = param.fontSize;

    pesVec2f c = out.getBoundingBox().getCenter();
    if (c.x != 0.f || c.y != 0.f)
        out.translate(-c.x, -c.y);
    return true;
}

} // namespace pescore
