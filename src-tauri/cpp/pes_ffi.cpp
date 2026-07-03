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
#include "pes_text_core.hpp" // shared PPEF/TTF text creation/rebuild (ditto)
#include "pes_satin_core.hpp" // shared Smart Satin engine seams (ditto)

#include "include/core/SkFont.h"
#include "include/core/SkPaint.h"
#include "include/core/SkPathUtils.h"
#include "include/core/SkTypeface.h"
#include "include/core/SkFontMgr.h"
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
    if (!doc()->loadPPESFromBuffer(buf))
        return false;
    pescore::migratePes2TextObjects(doc());
    return true;
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
    s.has_stitches = pescore::objectHasStitches(*data);
    s.object_type = objectTypeToString(param.type);
    s.text = std::string(param.text);
    s.group_id = param.groupId;
    return s;
}

rust::Vec<uint8_t> get_object_image_png(int32_t index, float scale) {
    sk_sp<SkImage> img = pescore::makeObjectPreviewImage(doc(), index, scale);
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

// Drop a ready-made scalable SVG-path shape at the hoop center (see
// pescore::makeShapeObject — shared with the web binding). shape_index:
// 0=line, 1=triangle, 2=rect, 8=ellipse. Returns the new object's index.
int32_t add_shape(int32_t shape_index) {
    pesData d = pescore::makeShapeObject(shape_index);
    doc()->addObject(d);
    return doc()->getObjectCount() - 1;
}

// Vector geometry (SVG paths + paint) for crisp Konva rendering of shapes.
rust::String get_object_vector_json(int32_t index) {
    if (index < 0 || index >= doc()->getObjectCount())
        return rust::String("{\"paths\":[]}");
    return rust::String(pescore::objectVectorJson(*doc()->getDataObject(index)).dump());
}

// Re-apply an SVG object's parameters to its paths + regenerate stitches (the
// SVG counterpart of update_ppef_text). No-op for non-SVG objects.
bool update_svg(int32_t index) {
    if (index < 0 || index >= doc()->getObjectCount())
        return false;
    return pescore::updateSvgObject(*doc()->getDataObject(index));
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
// The logic lives in pescore::rebuildPpefText (pes_text_core.hpp, shared with
// the web binding).
bool update_ppef_text(int32_t obj_index) {
    if (obj_index < 0 || obj_index >= doc()->getObjectCount())
        return false;
    return pescore::rebuildPpefText(*doc()->getDataObject(obj_index));
}

// Drop a fresh PPEF text object at the hoop center (port of PES5_AddPPEFText,
// shared with the web binding). Returns the new object's index, or -1 when the
// font is missing or shaping produced no glyphs.
int32_t add_ppef_text(rust::Str text, rust::Str font_name) {
    pesData d;
    if (!pescore::makePpefTextObject(d, std::string(text), std::string(font_name)))
        return -1;
    doc()->addObject(d);
    return doc()->getObjectCount() - 1;
}

namespace {

// skiaPathStroke / replacePath / reapplyStitches now live in pes_edit_core.hpp
// (namespace pescore) so desktop and web share one copy.

} // namespace

// Rebuild a TTF text object's path from its parameters — see
// pescore::rebuildTtfText (pes_text_core.hpp, shared with the web binding).
bool update_ttf_text(int32_t obj_index) {
    if (obj_index < 0 || obj_index >= doc()->getObjectCount())
        return false;
    return pescore::rebuildTtfText(*doc()->getDataObject(obj_index));
}

// Drop a fresh TTF text object at the hoop center (port of PES5_AddTTFText,
// shared with the web binding). Returns the new object's index, or -1 when the
// font is missing or the text produced no outline.
int32_t add_ttf_text(rust::Str text, rust::Str font_name) {
    pesData d;
    if (!pescore::makeTtfTextObject(d, std::string(text), std::string(font_name)))
        return -1;
    doc()->addObject(d);
    return doc()->getObjectCount() - 1;
}

// Path operations — see pescore::pathOp (pes_edit_core.hpp).
bool path_op(int32_t obj_index, int32_t path_index, rust::Str op_, float value) {
    return pescore::pathOp(doc(), obj_index, path_index, std::string(op_), value);
}

// ---- Smart Satin seams (pes_satin_core.hpp) --------------------------------

rust::String get_satin_source(int32_t obj_index) {
    if (obj_index < 0 || obj_index >= doc()->getObjectCount())
        return rust::String("{}");
    return rust::String(pescore::satinSource(*doc()->getDataObject(obj_index)).dump());
}

rust::String simplify_polygons(rust::Str polygons_json) {
    try {
        auto rings = nlohmann::json::parse(std::string(polygons_json));
        return rust::String(pescore::simplifyPolygons(rings).dump());
    } catch (...) {
        return rust::String("[]");
    }
}

int32_t add_satin_objects(rust::Str objects_json) {
    try {
        auto objects = nlohmann::json::parse(std::string(objects_json));
        return pescore::addSatinObjects(doc(), objects);
    } catch (...) {
        return 0;
    }
}

rust::String satin_column_rails(rust::Str rails_json) {
    try {
        auto in = nlohmann::json::parse(std::string(rails_json));
        return rust::String(pescore::satinColumnRails(in).dump());
    } catch (...) {
        return rust::String("{}");
    }
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
