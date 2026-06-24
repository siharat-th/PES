#include "pes_ffi.h"
#include "pes/src/engine.rs.h" // cxx-generated shared structs (ObjectSnapshot)

#include "pesDocument.hpp"
#include "pesData.hpp"
#include "pesBuffer.hpp"
#include "pesColor.hpp"
#include "pesEffect.hpp"
#include "pesPathUtility.hpp"
#include "json.hpp"
#include "pes_ffi_core.hpp" // shared extraction logic (also used by wasm/pes_web.cpp)
#include "pes_edit_core.hpp" // shared PathEdit/StitchEdit/path-op logic (ditto)

#include "include/core/SkFont.h"
#include "include/core/SkPaint.h"
#include "include/core/SkPathUtils.h"
#include "include/core/SkTypeface.h"
#include "include/core/SkFontMgr.h"
#include "include/pathops/SkPathOps.h"

// SkTypeface::MakeFromData was removed in Skia M150; build typefaces from font
// data through the platform font manager (native facade only).
#if defined(__APPLE__)
#include "include/ports/SkFontMgr_mac_ct.h"
static sk_sp<SkFontMgr> pesSystemFontMgr() {
    static sk_sp<SkFontMgr> mgr = SkFontMgr_New_CoreText(nullptr);
    return mgr;
}
#elif defined(_WIN32)
#include "include/ports/SkTypeface_win.h"
static sk_sp<SkFontMgr> pesSystemFontMgr() {
    static sk_sp<SkFontMgr> mgr = SkFontMgr_New_DirectWrite();
    return mgr;
}
#endif
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

using pescore::objectTypeToString; // shared (pes_ffi_core.hpp)

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
    s.group_id = param.groupId;
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

// Move object from `from` to `to` via adjacent swaps so the whole drag is a
// single engine mutation (one undo step). Indices are list positions
// (0 = back-most, count-1 = front-most).
bool move_object_to(int32_t from, int32_t to) {
    int32_t count = doc()->getObjectCount();
    if (from < 0 || from >= count || to < 0 || to >= count || from == to)
        return false;
    int32_t cur = from;
    while (cur < to && doc()->moveObjectFront(cur)) cur++;
    while (cur > to && doc()->moveObjectBack(cur)) cur--;
    return cur == to;
}

// --- Layer groups ---------------------------------------------------------

rust::Vec<GroupSnapshot> get_groups() {
    rust::Vec<GroupSnapshot> out;
    for (const auto& g : doc()->getGroups()) {
        GroupSnapshot s{};
        s.id = g.id;
        s.parent_id = g.parentId;
        s.name = g.name;
        s.collapsed = g.collapsed;
        s.order = g.order;
        s.scalable = doc()->isGroupScalable(g.id);
        out.push_back(std::move(s));
    }
    return out;
}

int32_t create_group(rust::Str name, int32_t parent_id) {
    return doc()->createGroup(std::string(name), parent_id);
}

bool rename_group(int32_t id, rust::Str name) {
    return doc()->renameGroup(id, std::string(name));
}

bool delete_group(int32_t id) {
    return doc()->deleteGroup(id);
}

bool set_group_collapsed(int32_t id, bool collapsed) {
    return doc()->setGroupCollapsed(id, collapsed);
}

bool set_group_order(int32_t id, int32_t order) {
    return doc()->setGroupOrder(id, order);
}

void set_object_group(int32_t index, int32_t group_id) {
    doc()->setObjectGroup(index, group_id);
}

// Cascade visibility/lock from a group header to all its member objects.
void set_group_visible(int32_t id, bool visible) {
    auto& objs = doc()->getDataObjects();
    for (auto& d : objs)
        if (d->parameter.groupId == id) d->parameter.visible = visible;
}

void set_group_locked(int32_t id, bool locked) {
    auto& objs = doc()->getDataObjects();
    for (auto& d : objs)
        if (d->parameter.groupId == id) d->parameter.locked = locked;
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

using pescore::colorToHex; // shared (pes_ffi_core.hpp)

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
    pescore::StitchGeom geom = pescore::buildStitchData(doc(), obj_index);
    StitchData out;
    out.total_points = geom.total_points;
    out.coords.reserve(geom.coords.size());
    for (float f : geom.coords)
        out.coords.push_back(f);
    for (auto& s : geom.segments) {
        StitchSegment seg;
        seg.hex = s.hex;
        seg.start = s.start;
        seg.count = s.count;
        out.segments.push_back(std::move(seg));
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
    return rust::String(pescore::parameterToJson(doc()->getDataParameter(obj_index)).dump());
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

// skiaPathStroke / replacePath / reapplyStitches now live in pes_edit_core.hpp
// (namespace pescore) so desktop and web share one copy.

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
    sk_sp<SkTypeface> typeface = pesSystemFontMgr()->makeFromData(fontData);
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

// Path operations — see pescore::pathOp (pes_edit_core.hpp).
bool path_op(int32_t obj_index, int32_t path_index, rust::Str op_, float value) {
    return pescore::pathOp(doc(), obj_index, path_index, std::string(op_), value);
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
    for (const auto& n : pescore::getPathNodes(doc(), obj_index, path_index)) {
        PathNode pn{};
        pn.node_type = n.node_type;
        pn.x = n.x;     pn.y = n.y;
        pn.cp1x = n.cp1x; pn.cp1y = n.cp1y;
        pn.cp2x = n.cp2x; pn.cp2y = n.cp2y;
        out.push_back(pn);
    }
    return out;
}

bool move_path_node(int32_t obj_index, int32_t path_index, int32_t node_index,
                    float world_dx, float world_dy) {
    return pescore::movePathNode(doc(), obj_index, path_index, node_index, world_dx, world_dy);
}

bool move_path_handle(int32_t obj_index, int32_t path_index, int32_t cmd_index,
                      int32_t cp_slot, float world_dx, float world_dy) {
    return pescore::movePathHandle(doc(), obj_index, path_index, cmd_index, cp_slot,
                                   world_dx, world_dy);
}

// Insert a node on the segment whose end command is node_index, subdividing at
// t. Commands are stored unrotated and t is rotation-invariant, so this needs
// no world<->local conversion (unlike the move ops which receive a delta).
bool insert_path_node(int32_t obj_index, int32_t path_index, int32_t node_index,
                      float t) {
    return pescore::insertPathNode(doc(), obj_index, path_index, node_index, t);
}

bool delete_path_node(int32_t obj_index, int32_t path_index, int32_t node_index) {
    return pescore::deletePathNode(doc(), obj_index, path_index, node_index);
}

// Convert a node's incoming segment between a straight corner (lineTo) and a
// smooth cubic curve (bezierTo). corner->curve seeds collinear 1/3 & 2/3 handles
// so the shape is preserved until a handle is dragged; curve->corner drops to a
// lineTo (a quad collapses to a line too). moveTo/close have no editable
// incoming segment and are rejected. Like move/insert, t-free so no rotation
// conversion is needed (handles are derived from already-stored local coords).
bool set_path_node_type(int32_t obj_index, int32_t path_index, int32_t node_index,
                        bool to_curve) {
    return pescore::setPathNodeType(doc(), obj_index, path_index, node_index, to_curve);
}

// --- StitchEdit -------------------------------------------------------------
// Port of PesStitchEdit (apps2/1080_PES5Template/src/Utils/PesSatinColumn.cpp)
// + the PES5_StitchEdit* command flow. The object's needle points live in
// fillBlocks (kind 0) / strokeBlocks (kind 1); each block is a polyline of
// pesVec2f plus a parallel `types` array (jump/normal flags). Stitches are
// stored unrotated, so we fold the display rotation in/out around the bbox
// center — exactly like the PathEdit ops above. The blocks are the source of
// truth (no regeneration), so an edit just mutates them and recalculates the
// cached totals/bbox.

rust::Vec<StitchBlock> get_stitch_points(int32_t obj_index) {
    rust::Vec<StitchBlock> out;
    for (auto& b : pescore::getStitchPoints(doc(), obj_index)) {
        StitchBlock sb;
        sb.kind = b.kind;
        sb.block_index = b.block_index;
        sb.hex = b.hex;
        for (auto& p : b.points) {
            StitchPoint sp{};
            sp.x = p.x; sp.y = p.y; sp.jump = p.jump;
            sb.points.push_back(sp);
        }
        out.push_back(std::move(sb));
    }
    return out;
}

bool move_stitch_point(int32_t obj_index, int32_t kind, int32_t block_index,
                       int32_t point_index, float world_dx, float world_dy) {
    return pescore::moveStitchPoint(doc(), obj_index, kind, block_index, point_index,
                                    world_dx, world_dy);
}

bool insert_stitch_point(int32_t obj_index, int32_t kind, int32_t block_index,
                         int32_t point_index) {
    return pescore::insertStitchPoint(doc(), obj_index, kind, block_index, point_index);
}

bool insert_stitch_point_at(int32_t obj_index, int32_t kind, int32_t block_index,
                            int32_t after_index, float world_x, float world_y) {
    return pescore::insertStitchPointAt(doc(), obj_index, kind, block_index, after_index,
                                        world_x, world_y);
}

bool delete_stitch_point(int32_t obj_index, int32_t kind, int32_t block_index,
                         int32_t point_index) {
    return pescore::deleteStitchPoint(doc(), obj_index, kind, block_index, point_index);
}

} // namespace pesffi
