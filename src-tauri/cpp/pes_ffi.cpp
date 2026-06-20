#include "pes_ffi.h"
#include "pes/src/engine.rs.h" // cxx-generated shared structs (ObjectSnapshot)

#include "pesDocument.hpp"
#include "pesData.hpp"
#include "pesBuffer.hpp"
#include "pesColor.hpp"
#include "pesEffect.hpp"
#include "pesPathUtility.hpp"
#include "json.hpp"

#include "include/core/SkFont.h"
#include "include/core/SkPaint.h"
#include "include/core/SkPathUtils.h"
#include "include/core/SkTypeface.h"
#include "include/pathops/SkPathOps.h"
#include "include/utils/SkTextUtils.h"

#include "PesPPEFUtils.hpp" // apps2/1080_PES5Template/src/Utils (native SQLiteCpp)

#include <cmath>
#include <cstdio>
#include <memory>

#include "include/core/SkData.h"
#include "include/core/SkImage.h"
#include "include/core/SkString.h"

namespace {

pesDocument* doc() {
    return pesDocument::getInstance();
}

pesBuffer toPesBuffer(rust::Slice<const uint8_t> data) {
    return pesBuffer(reinterpret_cast<const char*>(data.data()), data.size());
}

rust::Vec<uint8_t> toRustVec(const pesBuffer& buf) {
    rust::Vec<uint8_t> out;
    out.reserve(buf.size());
    const char* p = buf.getData();
    for (std::size_t i = 0; i < buf.size(); ++i)
        out.push_back(static_cast<uint8_t>(p[i]));
    return out;
}

rust::Vec<uint8_t> toRustVec(const sk_sp<SkData>& data) {
    rust::Vec<uint8_t> out;
    if (!data)
        return out;
    out.reserve(data->size());
    const uint8_t* p = data->bytes();
    for (std::size_t i = 0; i < data->size(); ++i)
        out.push_back(p[i]);
    return out;
}

// Mirrors PES5_ObjectTypeToString (apps2/1080_PES5Template/src/PES5Command.cpp).
std::string objectTypeToString(int type) {
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

} // namespace

void SetResourcePath(const char* path); // pes_resources.cpp
SkString GetResourcePath(const char* resource);
sk_sp<SkData> GetResourceAsData(const char* resource);

namespace pesffi {

void set_resource_path(rust::Str path) {
    SetResourcePath(std::string(path).c_str());
}

void new_document() {
    doc()->newDocument();
}

void set_hoop_size_mm(float w, float h) {
    doc()->setHoopSizeInMM(w, h);
}

float get_hoop_width_mm() {
    return doc()->getHoopSizeInMM().x;
}

float get_hoop_height_mm() {
    return doc()->getHoopSizeInMM().y;
}

bool load_ppes(rust::Slice<const uint8_t> data) {
    pesBuffer buf = toPesBuffer(data);
    return doc()->loadPPESFromBuffer(buf);
}

bool import_pes(rust::Slice<const uint8_t> data) {
    pesBuffer buf = toPesBuffer(data);
    pesData d;
    if (!d.loadPESFromBuffer(buf, true))
        return false;
    doc()->addObject(d);
    return true;
}

bool import_svg(rust::Slice<const uint8_t> data) {
    pesBuffer buf = toPesBuffer(data);
    pesData d;
    if (!d.loadSVGFromBuffer(buf, true))
        return false;
    doc()->addObject(d);
    return true;
}

int32_t get_object_count() {
    return doc()->getObjectCount();
}

ObjectSnapshot get_object_snapshot(int32_t index) {
    ObjectSnapshot s{};
    s.index = index;
    if (index < 0 || index >= doc()->getObjectCount())
        return s;

    auto data = doc()->getDataObject(index);
    auto& param = doc()->getDataParameter(index);
    pesRectangle bound = data->getBoundingBox();

    s.x = bound.x;
    s.y = bound.y;
    s.width = bound.width;
    s.height = bound.height;
    s.rotate_degree = param.rotateDegree;
    s.visible = param.visible;
    s.locked = param.locked;
    s.scalable = data->isScalable();
    s.object_type = objectTypeToString(param.type);
    s.text = std::string(param.text);
    return s;
}

rust::Vec<uint8_t> get_object_image_png(int32_t index) {
    if (index < 0 || index >= doc()->getObjectCount())
        return {};
    sk_sp<SkImage> img = doc()->makePesImageSnapshot(index);
    if (!img)
        return {};
    return toRustVec(SkImageToPngData(img));
}

void translate_object(int32_t index, float dx, float dy) {
    if (index < 0 || index >= doc()->getObjectCount())
        return;
    doc()->getDataObject(index)->translate(dx, dy);
}

void scale_object(int32_t index, float sx, float sy) {
    if (index < 0 || index >= doc()->getObjectCount())
        return;
    auto data = doc()->getDataObject(index);
    if (!data->isScalable()) // "Stitch" etc. must never scale
        return;
    data->scale(sx, sy);
}

void set_object_rotation(int32_t index, float degree) {
    if (index < 0 || index >= doc()->getObjectCount())
        return;
    // Display rotation is non-destructive, kept in the parameter block —
    // same model as the old app (PesHelper.js mouseReleased rotate path).
    while (degree > 180.f) degree -= 360.f;
    while (degree < -180.f) degree += 360.f;
    doc()->getDataParameter(index).rotateDegree = degree;
}

void set_object_visible(int32_t index, bool visible) {
    if (index < 0 || index >= doc()->getObjectCount())
        return;
    doc()->getDataParameter(index).visible = visible;
}

void set_object_locked(int32_t index, bool locked) {
    if (index < 0 || index >= doc()->getObjectCount())
        return;
    doc()->getDataParameter(index).locked = locked;
}

bool delete_object(int32_t index) {
    return doc()->deleteObject(index);
}

bool duplicate_object(int32_t index) {
    return doc()->duplicateObject(index);
}

bool move_object_front(int32_t index) {
    return doc()->moveObjectFront(index);
}

bool move_object_back(int32_t index) {
    return doc()->moveObjectBack(index);
}

rust::Vec<uint8_t> export_as(rust::Str format) {
    pesBuffer buf = doc()->exportBufferAs(std::string(format));
    return toRustVec(buf);
}

rust::Vec<uint8_t> get_thumbnail_png(int32_t wmax, int32_t hmax, int32_t index) {
    pesBuffer buf = doc()->getThumbnailPNGBuffer(wmax, hmax, index);
    return toRustVec(buf);
}

namespace {

std::string colorToHex(const pesColor& c) {
    char sz[8];
    std::snprintf(sz, sizeof sz, "#%02X%02X%02X", c.r, c.g, c.b);
    return sz;
}

bool validPath(int32_t objIndex, int32_t pathIndex) {
    if (objIndex < 0 || objIndex >= doc()->getObjectCount())
        return false;
    auto data = doc()->getDataObject(objIndex);
    return pathIndex >= 0 && pathIndex < (int32_t)data->paths.size();
}

// Mirrors PES5Template::hasStitch — stitches exist, so edits must reapply.
bool hasStitch(int32_t objIndex) {
    auto data = doc()->getDataObject(objIndex);
    return !data->fillBlocks.empty() || !data->strokeBlocks.empty();
}

} // namespace

StitchData get_stitch_data(int32_t obj_index) {
    StitchData out;
    out.total_points = 0;
    int count = doc()->getObjectCount();

    auto appendObject = [&](int idx) {
        auto data = doc()->getDataObject(idx);
        if (!data->parameter.visible)
            return;
        pesStitchBlockList blocks;
        data->getStitchBlockList(blocks);
        for (auto& block : blocks) {
            std::string hex = colorToHex(block.color);
            size_t n = block.polyline.size();
            // split into runs of normal stitches; jumps break the thread line
            size_t i = 0;
            while (i < n) {
                bool jump = i < block.types.size() && block.types[i] != 0;
                if (jump) { // jump points still count toward simulator length
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
                    StitchSegment seg;
                    seg.hex = hex;
                    seg.start = start;
                    seg.count = segCount;
                    out.segments.push_back(std::move(seg));
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

int32_t get_path_count(int32_t obj_index) {
    if (obj_index < 0 || obj_index >= doc()->getObjectCount())
        return 0;
    return (int32_t)doc()->getDataObject(obj_index)->paths.size();
}

PathInfo get_path_info(int32_t obj_index, int32_t path_index) {
    PathInfo info{};
    info.index = path_index;
    if (!validPath(obj_index, path_index))
        return info;
    pesPath& p = doc()->getDataObject(obj_index)->paths[path_index];
    info.path_id = std::string(p.path_id);
    info.is_fill = p.isFill();
    info.is_stroke = p.isStroke();
    info.fill_type = p.fillType;
    info.fill_color = colorToHex(p.getFillColor());
    info.stroke_color = colorToHex(p.getStrokeColor());
    info.stroke_width = p.getStrokeWidth();
    info.visible = p.bVisible;
    return info;
}

void set_path_fill_color(int32_t obj_index, int32_t path_index, int32_t brother_index) {
    if (!validPath(obj_index, path_index))
        return;
    auto data = doc()->getDataObject(obj_index);
    data->paths[path_index].setFillColor(pesGetBrotherColor(brother_index));
    if (hasStitch(obj_index))
        data->applyFill();
}

void set_path_stroke_color(int32_t obj_index, int32_t path_index, int32_t brother_index) {
    if (!validPath(obj_index, path_index))
        return;
    auto data = doc()->getDataObject(obj_index);
    data->paths[path_index].setStrokeColor(pesGetBrotherColor(brother_index));
    if (hasStitch(obj_index))
        data->applyStroke();
}

void set_path_stroke_width(int32_t obj_index, int32_t path_index, float width) {
    if (!validPath(obj_index, path_index))
        return;
    auto data = doc()->getDataObject(obj_index);
    data->paths[path_index].setStrokeWidth(width);
    if (hasStitch(obj_index))
        data->applyStroke();
}

rust::Vec<BrotherColor> get_brother_palette() {
    rust::Vec<BrotherColor> out;
    for (int i = 1; i <= 65; ++i) { // valid Brother thread indices (pesColor.cpp)
        BrotherColor c{};
        c.index = i;
        c.hex = colorToHex(pesGetBrotherColor(i));
        c.name = std::string(pesGetBrotherColorName(i));
        out.push_back(std::move(c));
    }
    return out;
}

// Thread color blocks (= pesData::fillBlocks), the unit shown in the old
// "Stitch"/PES properties panel (Prop_PESHandler.js).
rust::Vec<ColorBlockInfo> get_color_blocks(int32_t obj_index) {
    rust::Vec<ColorBlockInfo> out;
    if (obj_index < 0 || obj_index >= doc()->getObjectCount())
        return out;
    auto data = doc()->getDataObject(obj_index);
    for (size_t i = 0; i < data->fillBlocks.size(); ++i) {
        auto& block = data->fillBlocks[i];
        ColorBlockInfo info{};
        info.index = (int32_t)i;
        info.hex = colorToHex(block.color);
        info.brother_index = block.colorIndex;
        info.stitch_count = (int32_t)block.size();
        out.push_back(std::move(info));
    }
    return out;
}

void set_color_block(int32_t obj_index, int32_t block_index, int32_t brother_index) {
    if (obj_index < 0 || obj_index >= doc()->getObjectCount())
        return;
    auto data = doc()->getDataObject(obj_index);
    if (block_index < 0 || block_index >= (int32_t)data->fillBlocks.size())
        return;
    data->fillBlocks[block_index].setColorFromIndex(brother_index);
}

bool swap_color_block(int32_t obj_index, int32_t block_index, int32_t dir) {
    if (obj_index < 0 || obj_index >= doc()->getObjectCount())
        return false;
    auto data = doc()->getDataObject(obj_index);
    int32_t other = block_index + dir;
    if (block_index < 0 || other < 0 ||
        block_index >= (int32_t)data->fillBlocks.size() ||
        other >= (int32_t)data->fillBlocks.size())
        return false;
    std::swap(data->fillBlocks[block_index], data->fillBlocks[other]);
    return true;
}

void flip_object(int32_t obj_index, bool horizontal) {
    if (obj_index < 0 || obj_index >= doc()->getObjectCount())
        return;
    auto data = doc()->getDataObject(obj_index);
    if (horizontal)
        data->horizontalFlip();
    else
        data->verticalFlip();
}

// Parameter access for the properties panels. JSON keeps the FFI surface
// small; field set mirrors what the old Prop_* handlers read.
rust::String get_parameter_json(int32_t obj_index) {
    if (obj_index < 0 || obj_index >= doc()->getObjectCount())
        return rust::String("{}");
    auto& p = doc()->getDataParameter(obj_index);
    nlohmann::json j{
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
    return rust::String(j.dump());
}

bool set_param_num(int32_t obj_index, rust::Str key, float value) {
    if (obj_index < 0 || obj_index >= doc()->getObjectCount())
        return false;
    auto* d = doc();
    std::string k(key);
    int iv = (int)value;
    if (k == "fontSize") d->setDataParameterFontSize(obj_index, iv);
    else if (k == "textEffect") d->setDataParameterTextEffect(obj_index, iv);
    else if (k == "textEffectAngle") d->setDataParameterTextEffectAngle(obj_index, value);
    else if (k == "textEffectRadius") d->setDataParameterTextEffectRadius(obj_index, value);
    else if (k == "fillColor") d->setDataParameterFillColor(obj_index, iv);
    else if (k == "fillType") d->setDataParameterFillTypeIndex(obj_index, iv);
    else if (k == "strokeColor") d->setDataParameterStrokeColor(obj_index, iv);
    else if (k == "strokeType") d->setDataParameterStrokeTypeIndex(obj_index, iv);
    else if (k == "textDensity") d->setDataParameterTextDensity(obj_index, value);
    else if (k == "textPullCompensate") d->setDataParameterTextPullCompensate(obj_index, value);
    else if (k == "extraLetterSpace") d->setDataParameterTextExtraLetterSpace(obj_index, iv);
    else if (k == "extraSpace") d->setDataParameterTextExtraSpace(obj_index, iv);
    else if (k == "borderGapX") d->setDataParameterTextBorderGapX(obj_index, iv);
    else if (k == "borderGapY") d->setDataParameterTextBorderGapY(obj_index, iv);
    else if (k == "borderColor") d->setDataParameterTextBorderColor(obj_index, iv);
    else if (k == "strokeRunPitch") d->setDataParameterStrokeRunPitch(obj_index, value);
    else if (k == "strokeWidth") d->setDataParameterStrokeWidth(obj_index, value);
    else if (k == "strokeDensity") d->setDataParameterStrokeDensity(obj_index, value);
    else if (k == "strokeRunningInset") d->setDataParameterStrokeRunningInset(obj_index, value);
    else if (k == "fillDensity") d->setDataParameterFillDensity(obj_index, value);
    else if (k == "fillDirection") d->setDataParameterFillDirection(obj_index, value);
    else return false;
    return true;
}

bool set_param_bool(int32_t obj_index, rust::Str key, bool value) {
    if (obj_index < 0 || obj_index >= doc()->getObjectCount())
        return false;
    auto* d = doc();
    std::string k(key);
    if (k == "border") d->setDataParameterTextBorder(obj_index, value);
    else if (k == "italic") d->setDataParameterTextItalic(obj_index, value);
    else if (k == "fillUnderlay") d->setDataParameterFillUnderlay(obj_index, value);
    else if (k == "fillPatternUnderlay") d->setDataParameterFillPatternUnderlay(obj_index, value);
    else return false;
    return true;
}

// Rebuild a PPEF text object's paths/stitches from its parameter block.
// Port of updatePPEFText (Victor-frontend cordova/www/pes5.html:125-310),
// using the native PPEF_Reader (SQLiteCpp) instead of sql.js.
bool update_ppef_text(int32_t obj_index) {
    if (obj_index < 0 || obj_index >= doc()->getObjectCount())
        return false;
    auto data = doc()->getDataObject(obj_index);
    auto& param = data->parameter;
    if (param.type != pesData::OBJECT_TYPE_SCALABLE_PPEF_TEXT)
        return false;

    std::string text = param.text;
    while (!text.empty() && text.back() == ' ') text.pop_back();
    while (!text.empty() && text.front() == ' ') text.erase(0, 1);
    if (text.empty())
        return false;

    std::string fontName = param.fontName.empty() ? "Thai001" : param.fontName;
    SkString fontPath = GetResourcePath(("PPEF/" + fontName + ".ppef").c_str());

    pesVec2f oldCenter = data->getBoundingBox().getCenter();

    PPEF_Reader ppef(fontPath.c_str());
    ppef.readPPEFConfig();
    // scale=1.0 matches the native reference (PES5_AddPPEFText #else branch)
    std::vector<pesPath> shapes = ppef.getStringAsShapes(
        text, 1.0f, param.extraLetterSpace, param.extraSpace);
    if (shapes.empty())
        return false;

    // Effect selection mirrors pes5.html shapeIndex mapping (0..15).
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
    if (effect) {
        effect->bItalic = param.italic;
        effect->bCreateBorder = param.border;
        effect->borderGap = 100 + param.borderGap;
        effect->borderGapY = 100 + param.borderGapY;
        effect->applyPaths(shapes);
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

    data->paths = shapes;
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
    data->scale(s * paramCopy.ppefScaleX, s * paramCopy.ppefScaleY);
    data->parameter = paramCopy;

    data->applyPPEFFill();

    pesVec2f newCenter = data->getBoundingBox().getCenter();
    data->translate(oldCenter.x - newCenter.x, oldCenter.y - newCenter.y);
    return true;
}

namespace {

// Port of PES5Template::SkiaPathStroke (bindings line ~690)
SkPath skiaPathStroke(const SkPath& skPath, float value) {
    SkPath newPath;
    SkPaint paint;
    paint.setStyle(SkPaint::Style::kStroke_Style);
    paint.setStrokeWidth(value * 2);
    paint.setStrokeCap(SkPaint::Cap::kButt_Cap);
    paint.setStrokeJoin(SkPaint::Join::kMiter_Join);
    skpathutils::FillPathWithPaint(skPath, paint, &newPath);
    return newPath;
}

// Replace paths[idx] with a new SkPath, preserving style attributes.
void replacePath(pesData* pes, int idx, const SkPath& skPath) {
    pesPath& tpath = pes->paths[idx];
    bool isFill = tpath.isFill();
    float strokeWidth = tpath.getStrokeWidth();
    pesColor strokeColor = tpath.getStrokeColor();
    pesColor fillColor = tpath.getFillColor();
    std::string path_id = tpath.path_id;
    std::string group_id = tpath.group_id;

    pesPath path = toPes(skPath);
    path.setStrokeColor(strokeColor);
    path.setFillColor(fillColor);
    path.setStrokeWidth(strokeWidth);
    path.setFilled(isFill);
    path.path_id = path_id;
    path.group_id = group_id;

    pes->paths[idx].clear();
    pes->paths[idx] = path;
}

// Regenerate stitches after a geometry change. The fill regenerator is
// type-dependent — using applyFill() on PPEF/Monogram text wipes the fill
// (0 stitches, colors reset). Mirrors PES5_StopPathEditInput.
void reapplyStitches(int32_t objIndex, pesData* pes) {
    if (!hasStitch(objIndex))
        return;
    switch (pes->parameter.type) {
        case pesData::OBJECT_TYPE_SCALABLE_PPEF_TEXT:
            pes->applyPPEFFill();
            break;
        case pesData::OBJECT_TYPE_SCALABLE_PPEF_TEXT_V2:
            pes->applyPPEF_V2_Fill();
            break;
        default:
            pes->applyFill();
            break;
    }
    pes->applyStroke();
}

} // namespace

// Rebuild a TTF text object's path from its parameters — port of
// PES5_ReplaceTTFText (PES5Command.cpp:2081-2136), loading the typeface from
// bundled resources (TTF/<fontName>.ttf) instead of the sk_ui font combobox.
bool update_ttf_text(int32_t obj_index) {
    if (obj_index < 0 || obj_index >= doc()->getObjectCount())
        return false;
    auto data = doc()->getDataObject(obj_index);
    pesData* pes = data.get();
    auto& param = pes->parameter;
    if (param.type != pesData::OBJECT_TYPE_SCALABLE_TTF_TEXT)
        return false;
    if (param.text.empty() || pes->paths.empty())
        return false;

    sk_sp<SkData> fontData =
        GetResourceAsData(("TTF/" + param.fontName + ".ttf").c_str());
    if (!fontData)
        return false; // font file not bundled — leave object untouched
    sk_sp<SkTypeface> typeface = SkTypeface::MakeFromData(fontData);
    if (!typeface)
        return false;

    pesVec2f oldCenter = pes->getBoundingBox().getCenter();
    std::string str(param.text);
    SkScalar ptSize = param.fontSize * 10;

    { // normalize so glyph "0" is exactly ptSize tall (old behavior)
        SkFont font(typeface, ptSize);
        SkPath path;
        SkTextUtils::GetPath("0", 1, SkTextEncoding::kUTF8, 0, 0, font, &path);
        auto h = std::abs(path.getBounds().height());
        if (h > 0)
            ptSize = ptSize * (ptSize / h);
    }

    SkFont font(typeface, ptSize);
    SkPath path;
    SkTextUtils::GetPath(str.c_str(), str.length(), SkTextEncoding::kUTF8, 0, 0,
                         font, &path);
    pesPath pes_path = toPes(path);
    pes_path.setFilled(true);
    pes_path.setStrokeWidth(2);
    pes_path.setFillColor(pesGetBrotherColor(param.fillColorIndex));
    pes_path.setStrokeColor(pesGetBrotherColor(param.colorIndex));
    pes_path.fillRule = pes->paths[0].fillRule;
    pes->paths.clear();
    pes->paths.push_back(pes_path);

    if (param.lastFontSize != param.fontSize) {
        param.lastFontSize = param.fontSize;
        param.ppefScaleX = param.ppefScaleY = 1.f;
    }
    // pesData::scale mutates ppefScaleX/Y cumulatively — backup & rollback
    float sx = param.ppefScaleX;
    float sy = param.ppefScaleY;
    pes->scale(param.ppefScaleX, param.ppefScaleY);
    param.ppefScaleX = sx;
    param.ppefScaleY = sy;

    // refresh dynamic stroke types (same trick as the old code)
    pes->scale(1.f, 1.f);
    param.ppefScaleX = sx;
    param.ppefScaleY = sy;

    pesVec2f newCenter = pes->getBoundingBox().getCenter();
    pes->translate(oldCenter.x - newCenter.x, oldCenter.y - newCenter.y);
    return true;
}

// Path operations — ported from PES5Template_bindings.cpp (lines 508-880).
bool path_op(int32_t obj_index, int32_t path_index, rust::Str op_, float value) {
    if (obj_index < 0 || obj_index >= doc()->getObjectCount())
        return false;
    pesData* pes = doc()->getDataObject(obj_index).get();
    int n = (int)pes->paths.size();
    std::string op(op_);

    if (op == "up") {
        if (path_index < 1 || path_index >= n) return false;
        std::swap(pes->paths[path_index], pes->paths[path_index - 1]);
        reapplyStitches(obj_index, pes);
        return true;
    }
    if (op == "down") {
        if (path_index < 0 || path_index >= n - 1) return false;
        std::swap(pes->paths[path_index], pes->paths[path_index + 1]);
        reapplyStitches(obj_index, pes);
        return true;
    }
    if (op == "inset" || op == "outset") {
        if (path_index < 0 || path_index >= n) return false;
        SkPath skPath = toSk(pes->paths[path_index]);
        SkPath outlinePath = skiaPathStroke(skPath, value);
        Simplify(outlinePath, &outlinePath);
        Op(outlinePath, skPath,
           op == "inset" ? SkPathOp::kReverseDifference_SkPathOp
                         : SkPathOp::kDifference_SkPathOp,
           &outlinePath);
        replacePath(pes, path_index, outlinePath);
        reapplyStitches(obj_index, pes);
        return true;
    }
    if (op == "simplify") {
        if (path_index < 0 || path_index >= n) return false;
        pesPath& tpath = pes->paths[path_index];
        std::vector<pesPath> subpath = tpath.getSubPath();
        SkPath skPath = toSk(tpath);
        if (!subpath.empty())
            skPath = toSk(subpath[0]);
        Simplify(skPath, &skPath);
        replacePath(pes, path_index, skPath);
        reapplyStitches(obj_index, pes);
        return true;
    }
    if (op == "unite_next") {
        if (path_index < 0 || path_index >= n - 1) return false;
        SkPath path1 = toSk(pes->paths[path_index]);
        SkPath path2 = toSk(pes->paths[path_index + 1]);
        Op(path1, path2, SkPathOp::kUnion_SkPathOp, &path1);
        replacePath(pes, path_index, path1);
        pes->paths[path_index + 1].clear();
        pes->paths.erase(pes->paths.begin() + (path_index + 1));
        reapplyStitches(obj_index, pes);
        return true;
    }
    if (op == "separate") {
        if (path_index < 0 || path_index >= n) return false;
        pesPath& tpath = pes->paths[path_index];
        bool isFill = tpath.isFill();
        float strokeWidth = tpath.getStrokeWidth();
        pesColor strokeColor = tpath.getStrokeColor();
        pesColor fillColor = tpath.getFillColor();
        std::vector<pesPath> subpaths = tpath.getSubPath();
        for (size_t i = 0; i < subpaths.size(); ++i) {
            pesPath path = subpaths[i];
            path.setStrokeColor(strokeColor);
            path.setFillColor(fillColor);
            path.setStrokeWidth(strokeWidth);
            path.setFilled(isFill);
            pes->paths.insert(pes->paths.begin() + (path_index + i + 1), path);
        }
        reapplyStitches(obj_index, pes);
        return true;
    }
    if (op == "erase_under") {
        if (path_index < 1 || path_index >= n) return false;
        SkPath eraserPath = toSk(pes->paths[path_index]);
        for (int ip = 0; ip < path_index; ++ip) {
            pesPath& tpath = pes->paths[ip];
            if (!tpath.bVisible)
                continue;
            bool isFill = tpath.isFill();
            bool isStroke = tpath.isStroke();
            pesColor strokeColor = tpath.getStrokeColor();
            SkPath targetPath = toSk(tpath);
            if (isStroke && !isFill) {
                SkPath outlinePath = skiaPathStroke(
                    targetPath,
                    tpath.getStrokeWidth() / 2 * pes->parameter.ppefScaleX);
                Op(outlinePath, eraserPath, SkPathOp::kDifference_SkPathOp,
                   &targetPath);
                pesPath path = toPes(targetPath);
                path.setStrokeColor(strokeColor);
                path.setFillColor(strokeColor);
                path.setStrokeWidth(0);
                path.setFilled(true);
                pes->paths[ip].clear();
                pes->paths[ip] = path;
            } else {
                Op(eraserPath, targetPath, SkPathOp::kReverseDifference_SkPathOp,
                   &targetPath);
                replacePath(pes, ip, targetPath);
            }
        }
        reapplyStitches(obj_index, pes);
        return true;
    }
    return false;
}

bool set_param_str(int32_t obj_index, rust::Str key, rust::Str value) {
    if (obj_index < 0 || obj_index >= doc()->getObjectCount())
        return false;
    auto* d = doc();
    std::string k(key);
    if (k == "text") d->setDataParameterText(obj_index, std::string(value));
    else if (k == "font") d->setDataParameterFont(obj_index, std::string(value));
    else return false;
    return true;
}

// --- PathEdit ---------------------------------------------------------------
// Path command points are stored unrotated; the object's display rotation
// (parameter.rotateDegree, non-destructive) is applied around the bbox center
// at render time. We translate node coordinates to/from world space here so
// the frontend only ever deals with world units (matching the canvas).

rust::Vec<PathNode> get_path_nodes(int32_t obj_index, int32_t path_index) {
    rust::Vec<PathNode> out;
    if (!validPath(obj_index, path_index))
        return out;
    auto data = doc()->getDataObject(obj_index);
    float angle = doc()->getDataParameter(obj_index).rotateDegree;
    pesVec2f center = data->getBoundingBox().getCenter();
    auto toWorld = [&](float x, float y) -> pesVec2f {
        pesVec2f v(x, y);
        if (std::abs(angle) > 1e-4f)
            v.rotate(angle, center);
        return v;
    };
    for (const auto& c : data->paths[path_index].getCommands()) {
        PathNode n{};
        n.node_type = (int32_t)c.type;
        pesVec2f to = toWorld(c.to.x, c.to.y);
        pesVec2f cp1 = toWorld(c.cp1.x, c.cp1.y);
        pesVec2f cp2 = toWorld(c.cp2.x, c.cp2.y);
        n.x = to.x;   n.y = to.y;
        n.cp1x = cp1.x; n.cp1y = cp1.y;
        n.cp2x = cp2.x; n.cp2y = cp2.y;
        out.push_back(n);
    }
    return out;
}

bool move_path_node(int32_t obj_index, int32_t path_index, int32_t node_index,
                    float world_dx, float world_dy) {
    if (!validPath(obj_index, path_index))
        return false;
    pesData* pes = doc()->getDataObject(obj_index).get();
    auto& cmds = pes->paths[path_index].getCommands();
    if (node_index < 0 || node_index >= (int32_t)cmds.size())
        return false;

    // world delta → local delta (undo the object's display rotation)
    pesVec2f d(world_dx, world_dy);
    float angle = doc()->getDataParameter(obj_index).rotateDegree;
    if (std::abs(angle) > 1e-4f)
        d.rotate(-angle); // rotate the vector about the origin

    using Cmd = pesPath::Command;
    auto& c = cmds[node_index];
    c.to.x += d.x; c.to.y += d.y;
    // the curve arriving at this node keeps its shape → move its near handle
    if (c.type == Cmd::_bezierTo || c.type == Cmd::_quadBezierTo) {
        c.cp2.x += d.x; c.cp2.y += d.y;
    }
    // the curve leaving this node lives on the next command → move its handle
    if (node_index + 1 < (int32_t)cmds.size()) {
        auto& nx = cmds[node_index + 1];
        if (nx.type == Cmd::_bezierTo) {
            nx.cp1.x += d.x; nx.cp1.y += d.y;
        } else if (nx.type == Cmd::_quadBezierTo) {
            nx.cp2.x += d.x; nx.cp2.y += d.y;
        }
    }
    pes->paths[path_index].flagShapeChanged();
    reapplyStitches(obj_index, pes);
    return true;
}

bool move_path_handle(int32_t obj_index, int32_t path_index, int32_t cmd_index,
                      int32_t cp_slot, float world_dx, float world_dy) {
    if (!validPath(obj_index, path_index))
        return false;
    pesData* pes = doc()->getDataObject(obj_index).get();
    auto& cmds = pes->paths[path_index].getCommands();
    if (cmd_index < 0 || cmd_index >= (int32_t)cmds.size())
        return false;

    pesVec2f d(world_dx, world_dy);
    float angle = doc()->getDataParameter(obj_index).rotateDegree;
    if (std::abs(angle) > 1e-4f)
        d.rotate(-angle);

    auto& c = cmds[cmd_index];
    if (cp_slot == 1) {
        c.cp1.x += d.x; c.cp1.y += d.y;
    } else if (cp_slot == 2) {
        c.cp2.x += d.x; c.cp2.y += d.y;
    } else {
        return false;
    }
    pes->paths[path_index].flagShapeChanged();
    reapplyStitches(obj_index, pes);
    return true;
}

} // namespace pesffi
