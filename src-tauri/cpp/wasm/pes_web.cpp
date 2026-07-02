// pes_web.cpp — embind web binding over the pes engine core.
//
// The browser counterpart of the native cxx facade (pes_ffi.cpp) + Rust
// orchestration (commands.rs/history.rs). Same engine (pesDocument/pesData,
// compiled to wasm), same shared extraction logic (pes_ffi_core.hpp), same
// command contract as the Tauri layer — so the React frontend can talk to it
// through an invoke()-shaped transport with no UI changes.
//
// Surface (embind):
//   set_resource_path(path)               -> void
//   pes_call(cmd, argsJson)               -> resultJson   (all JSON in/out cmds)
//   load_input(kind, Uint8Array)          -> resultJson    ("ppes"|"pes"|"svg")
//   object_png(index)                     -> Uint8Array
//   export_bytes(format)                  -> Uint8Array
//
// Commands map 1:1 to the Tauri command names; args use the SAME camelCase keys
// the frontend already sends (see EngineClient.ts). DocumentSnapshot-returning
// commands return the snapshot JSON; errors return {"__error": "..."}.

#include <emscripten/bind.h>
#include <emscripten/val.h>

#include <sys/stat.h> // mkdir (MEMFS font dir)

#include <algorithm>
#include <cstdio>
#include <string>
#include <vector>

#include "json.hpp"
#include "pes_ffi_core.hpp"
#include "pes_edit_core.hpp" // shared PathEdit/StitchEdit/path-op logic
#include "pes_text_core.hpp" // shared PPEF/TTF text creation/rebuild
#include "pes_satin_core.hpp" // shared Smart Satin engine seams

#include "pesBuffer.hpp"
#include "pesColor.hpp"
#include "pesData.hpp"
#include "pesDocument.hpp"

#include "include/core/SkData.h"
#include "include/core/SkImage.h"

using emscripten::val;
using nlohmann::json;

// Resource API implemented in pes_resources.cpp.
void SetResourcePath(const char* path);

namespace {

pesDocument* doc() { return pesDocument::getInstance(); }

// parameter setters (mirror pes_ffi.cpp set_param_*), defined at end of file.
bool setParamNum(int idx, const std::string& k, float value);
bool setParamBool(int idx, const std::string& k, bool value);
bool setParamStr(int idx, const std::string& k, const std::string& value);

// ---- byte <-> JS typed array ------------------------------------------------
std::vector<uint8_t> valToBytes(const val& v) {
    unsigned len = v["length"].as<unsigned>();
    std::vector<uint8_t> out(len);
    if (len) {
        val view = val(emscripten::typed_memory_view(len, out.data()));
        view.call<void>("set", v);
    }
    return out;
}

val bytesToVal(const uint8_t* data, size_t n) {
    val u8 = val::global("Uint8Array").new_(n);
    if (n) {
        val view = val(emscripten::typed_memory_view(n, data));
        u8.call<void>("set", view);
    }
    return u8;
}

val pesBufferToVal(const pesBuffer& buf) {
    return bytesToVal(reinterpret_cast<const uint8_t*>(buf.getData()), buf.size());
}

// ---- snapshot-based undo/redo (mirrors history.rs) --------------------------
constexpr size_t kMaxDepth = 50;
std::vector<std::string> g_undo, g_redo;

std::string snapshotPPES() {
    pesBuffer buf = doc()->exportBufferAs("PPES");
    return std::string(buf.getData(), buf.size());
}
void restorePPES(const std::string& buf) {
    doc()->newDocument();
    pesBuffer b(buf.data(), buf.size());
    doc()->loadPPESFromBuffer(b);
}
void pushUndo(std::string before) {
    g_undo.push_back(std::move(before));
    if (g_undo.size() > kMaxDepth) g_undo.erase(g_undo.begin());
    g_redo.clear();
}
void clearHistory() { g_undo.clear(); g_redo.clear(); }

// ---- snapshot JSON (mirrors commands.rs document_snapshot) ------------------
json objectSnapshotJson(int index) {
    json s;
    s["index"] = index;
    if (index < 0 || index >= doc()->getObjectCount()) return s;
    auto data = doc()->getDataObject(index);
    auto& p = doc()->getDataParameter(index);
    pesRectangle b = data->getBoundingBox();
    s["x"] = b.x;
    s["y"] = b.y;
    s["width"] = b.width;
    s["height"] = b.height;
    s["rotate_degree"] = p.rotateDegree;
    s["visible"] = p.visible;
    s["locked"] = p.locked;
    s["scalable"] = data->isScalable();
    s["has_stitches"] = pescore::objectHasStitches(*data);
    s["object_type"] = pescore::objectTypeToString(p.type);
    s["text"] = std::string(p.text);
    s["group_id"] = p.groupId;
    return s;
}

json groupsJson() {
    json arr = json::array();
    for (const auto& g : doc()->getGroups()) {
        arr.push_back({
            {"id", g.id},
            {"parent_id", g.parentId},
            {"name", g.name},
            {"collapsed", g.collapsed},
            {"order", g.order},
            {"scalable", doc()->isGroupScalable(g.id)},
        });
    }
    return arr;
}

json documentSnapshotJson() {
    int n = doc()->getObjectCount();
    json objs = json::array();
    for (int i = 0; i < n; ++i) objs.push_back(objectSnapshotJson(i));
    auto hoop = doc()->getHoopSizeInMM();
    return json{
        {"hoop_width_mm", hoop.x},
        {"hoop_height_mm", hoop.y},
        {"objects", objs},
        {"groups", groupsJson()},
        {"can_undo", !g_undo.empty()},
        {"can_redo", !g_redo.empty()},
    };
}

bool inRange(int idx) { return idx >= 0 && idx < doc()->getObjectCount(); }

// group members (object indices) sorted ascending
std::vector<int> groupMembers(int id) {
    std::vector<int> m;
    int n = doc()->getObjectCount();
    for (int i = 0; i < n; ++i)
        if (doc()->getDataParameter(i).groupId == id) m.push_back(i);
    return m;
}

bool moveObjectTo(int from, int to) {
    int count = doc()->getObjectCount();
    if (from < 0 || from >= count || to < 0 || to >= count || from == to) return false;
    int cur = from;
    while (cur < to && doc()->moveObjectFront(cur)) cur++;
    while (cur > to && doc()->moveObjectBack(cur)) cur--;
    return cur == to;
}

json errorJson(const std::string& msg) { return json{{"__error", msg}}; }

// Fonts are fetched on demand (not preloaded into pes_web.data). When a
// command needs a font that is not in MEMFS yet, return this instead of
// mutating anything — webEngine.ts fetches the font (.ppef or .ttf, picked by
// font_kind), calls load_ppef_font/load_ttf_font, and retries the same command.
json missingFontJson(const std::string& fontName, const char* kind = "ppef") {
    return json{{"__error", std::string(kind) + " font not loaded: " + fontName},
                {"missing_font", fontName},
                {"font_kind", kind}};
}

// ============================================================================
// pes_call — JSON dispatch. `a` is the parsed args object (camelCase keys, as
// the frontend sends them). Returns a JSON value (snapshot, array, etc.).
// ============================================================================
json dispatch(const std::string& cmd, const json& a) {
    auto iarg = [&](const char* k, int def = 0) -> int {
        return a.contains(k) && !a[k].is_null() ? a[k].get<int>() : def;
    };
    auto farg = [&](const char* k, float def = 0.f) -> float {
        return a.contains(k) && !a[k].is_null() ? a[k].get<float>() : def;
    };
    auto barg = [&](const char* k, bool def = false) -> bool {
        return a.contains(k) && !a[k].is_null() ? a[k].get<bool>() : def;
    };
    auto sarg = [&](const char* k) -> std::string {
        return a.contains(k) && a[k].is_string() ? a[k].get<std::string>() : std::string();
    };
    auto undoable = [&](auto&& fn) { std::string b = snapshotPPES(); fn(); pushUndo(std::move(b)); };
    auto undoableChecked = [&](auto&& fn) -> bool {
        std::string b = snapshotPPES();
        bool changed = fn();
        if (changed) pushUndo(std::move(b));
        return changed;
    };

    // ---- document lifecycle ----
    if (cmd == "new_document") {
        doc()->newDocument();
        doc()->setHoopSizeInMM(farg("hoopWMm", 100.f), farg("hoopHMm", 100.f));
        clearHistory();
        return documentSnapshotJson();
    }
    if (cmd == "get_document") return documentSnapshotJson();

    // ---- queries (no undo) ----
    if (cmd == "get_object_paths") {
        int idx = iarg("index");
        json arr = json::array();
        if (inRange(idx)) {
            auto data = doc()->getDataObject(idx);
            for (size_t i = 0; i < data->paths.size(); ++i) {
                pesPath& p = data->paths[i];
                arr.push_back({
                    {"index", (int)i},
                    {"path_id", std::string(p.path_id)},
                    {"is_fill", p.isFill()},
                    {"is_stroke", p.isStroke()},
                    {"fill_type", p.fillType},
                    {"fill_color", pescore::colorToHex(p.getFillColor())},
                    {"stroke_color", pescore::colorToHex(p.getStrokeColor())},
                    {"stroke_width", p.getStrokeWidth()},
                    {"visible", p.bVisible},
                });
            }
        }
        return arr;
    }
    if (cmd == "get_brother_palette") {
        json arr = json::array();
        for (int i = 1; i <= 65; ++i)
            arr.push_back({{"index", i},
                           {"hex", pescore::colorToHex(pesGetBrotherColor(i))},
                           {"name", std::string(pesGetBrotherColorName(i))}});
        return arr;
    }
    if (cmd == "get_color_blocks") {
        int idx = iarg("index");
        json arr = json::array();
        if (inRange(idx)) {
            auto data = doc()->getDataObject(idx);
            for (size_t i = 0; i < data->fillBlocks.size(); ++i) {
                auto& blk = data->fillBlocks[i];
                arr.push_back({{"index", (int)i},
                               {"hex", pescore::colorToHex(blk.color)},
                               {"brother_index", blk.colorIndex},
                               {"stitch_count", (int)blk.size()}});
            }
        }
        return arr;
    }
    if (cmd == "get_parameter") {
        int idx = iarg("index");
        if (!inRange(idx)) return std::string("{}");
        // returns a STRING (the frontend JSON.parses it, matching Tauri)
        return pescore::parameterToJson(doc()->getDataParameter(idx)).dump();
    }
    if (cmd == "get_object_vector") {
        int idx = iarg("index");
        if (!inRange(idx)) return std::string("{\"paths\":[]}");
        // STRING result (frontend JSON.parses it, matching Tauri)
        return pescore::objectVectorJson(*doc()->getDataObject(idx)).dump();
    }
    if (cmd == "get_stitch_data") {
        int idx = a.contains("index") ? iarg("index", -1) : -1;
        pescore::StitchGeom g = pescore::buildStitchData(doc(), idx);
        json segs = json::array();
        for (auto& s : g.segments)
            segs.push_back({{"hex", s.hex}, {"start", s.start}, {"count", s.count}});
        // coords as base64 LE f32, matching the Tauri StitchDataDto
        std::vector<uint8_t> bytes;
        bytes.reserve(g.coords.size() * 4);
        for (float f : g.coords) {
            uint8_t* p = reinterpret_cast<uint8_t*>(&f);
            bytes.insert(bytes.end(), p, p + 4);
        }
        return json{{"segments", segs},
                    {"total_points", g.total_points},
                    {"coords_b64", pescore::base64(bytes.data(), bytes.size())}};
    }

    // ---- transforms / object edits (undoable) ----
    if (cmd == "transform_object") {
        int idx = iarg("index");
        float sx = farg("sx", 1.f), sy = farg("sy", 1.f);
        float dx = farg("dx"), dy = farg("dy"), rot = farg("rotateDegree");
        undoable([&] {
            if (inRange(idx)) {
                auto data = doc()->getDataObject(idx);
                if ((sx != 1.f || sy != 1.f) && data->isScalable()) data->scale(sx, sy);
                if (dx != 0.f || dy != 0.f) data->translate(dx, dy);
                float r = rot;
                while (r > 180.f) r -= 360.f;
                while (r < -180.f) r += 360.f;
                doc()->getDataParameter(idx).rotateDegree = r;
            }
        });
        return documentSnapshotJson();
    }
    if (cmd == "translate_objects") {
        undoable([&] {
            for (auto& m : a["moves"]) {
                int i = m["index"].get<int>();
                if (inRange(i)) doc()->getDataObject(i)->translate(m["dx"].get<float>(), m["dy"].get<float>());
            }
        });
        return documentSnapshotJson();
    }
    if (cmd == "delete_object") {
        int idx = iarg("index");
        undoable([&] { doc()->deleteObject(idx); });
        return documentSnapshotJson();
    }
    if (cmd == "delete_objects") {
        std::vector<int> idx;
        for (auto& v : a["indices"]) idx.push_back(v.get<int>());
        std::sort(idx.begin(), idx.end(), std::greater<int>());
        idx.erase(std::unique(idx.begin(), idx.end()), idx.end());
        undoable([&] { for (int i : idx) doc()->deleteObject(i); });
        return documentSnapshotJson();
    }
    if (cmd == "duplicate_object") {
        int idx = iarg("index");
        undoable([&] { doc()->duplicateObject(idx); });
        return documentSnapshotJson();
    }
    if (cmd == "add_shape") {
        int shapeIndex = iarg("shapeIndex");
        undoable([&] {
            pesData d = pescore::makeShapeObject(shapeIndex);
            doc()->addObject(d);
        });
        return documentSnapshotJson();
    }
    if (cmd == "add_ppef_text") {
        std::string text = sarg("text");
        std::string fontName = sarg("fontName");
        if (fontName.empty()) fontName = "Thai001";
        if (!pescore::ppefFontAvailable(fontName)) return missingFontJson(fontName);
        bool ok = undoableChecked([&]() -> bool {
            pesData d;
            if (!pescore::makePpefTextObject(d, text, fontName)) return false;
            doc()->addObject(d);
            return true;
        });
        if (!ok) return errorJson("สร้างข้อความ PPEF ไม่สำเร็จ (ฟอนต์ " + fontName + ")");
        return documentSnapshotJson();
    }
    if (cmd == "add_ttf_text") {
        std::string text = sarg("text");
        std::string fontName = sarg("fontName");
        if (fontName.empty()) fontName = "JS-Boaboon";
        if (!pescore::ttfFontAvailable(fontName)) return missingFontJson(fontName, "ttf");
        bool ok = undoableChecked([&]() -> bool {
            pesData d;
            if (!pescore::makeTtfTextObject(d, text, fontName)) return false;
            doc()->addObject(d);
            return true;
        });
        if (!ok) return errorJson("สร้างข้อความ TTF ไม่สำเร็จ (ฟอนต์ " + fontName + ")");
        return documentSnapshotJson();
    }

    // ---- Smart Satin seams (pes_satin_core.hpp) ----
    if (cmd == "get_satin_source") {
        int idx = iarg("index");
        if (!inRange(idx)) return std::string("{}");
        // returns a STRING (the frontend JSON.parses it, matching Tauri)
        return pescore::satinSource(*doc()->getDataObject(idx)).dump();
    }
    if (cmd == "simplify_polygons") {
        try {
            auto rings = json::parse(sarg("polygonsJson"));
            return pescore::simplifyPolygons(rings).dump();
        } catch (...) {
            return std::string("[]");
        }
    }
    if (cmd == "add_satin_objects") {
        json objects;
        try {
            objects = json::parse(sarg("objectsJson"));
        } catch (...) {
            return errorJson("add_satin_objects: invalid JSON");
        }
        bool ok = undoableChecked([&]() -> bool {
            return pescore::addSatinObjects(doc(), objects) > 0;
        });
        if (!ok) return errorJson("สร้าง Satin Column ไม่สำเร็จ");
        return documentSnapshotJson();
    }
    if (cmd == "satin_column_rails") {
        try {
            auto in = json::parse(sarg("railsJson"));
            return pescore::satinColumnRails(in).dump();
        } catch (...) {
            return std::string("{}");
        }
    }
    if (cmd == "duplicate_objects") {
        std::vector<int> src;
        for (auto& v : a["indices"]) src.push_back(v.get<int>());
        std::sort(src.begin(), src.end());
        src.erase(std::unique(src.begin(), src.end()), src.end());
        std::string groupName = a.contains("groupName") && a["groupName"].is_string()
                                    ? a["groupName"].get<std::string>() : std::string();
        bool hasGroup = a.contains("groupName") && a["groupName"].is_string();
        std::vector<int> newIdx;
        int groupId = -1;
        {
            std::string b = snapshotPPES();
            int inserted = 0;
            for (int s : src) {
                int cur = s + inserted;
                if (doc()->duplicateObject(cur)) { newIdx.push_back(cur + 1); inserted++; }
            }
            if (hasGroup && !newIdx.empty()) {
                groupId = doc()->createGroup(groupName, 0);
                for (int i : newIdx) doc()->setObjectGroup(i, groupId);
            }
            pushUndo(std::move(b));
        }
        return json{{"snapshot", documentSnapshotJson()},
                    {"new_indices", newIdx},
                    {"group_id", groupId}};
    }
    if (cmd == "set_object_visible") {
        int idx = iarg("index"); bool v = barg("visible");
        undoable([&] { if (inRange(idx)) doc()->getDataParameter(idx).visible = v; });
        return documentSnapshotJson();
    }
    if (cmd == "set_object_locked") {
        int idx = iarg("index"); bool v = barg("locked");
        undoable([&] { if (inRange(idx)) doc()->getDataParameter(idx).locked = v; });
        return documentSnapshotJson();
    }
    if (cmd == "reorder_object") {
        int idx = iarg("index"), dir = iarg("dir");
        undoable([&] { if (dir > 0) doc()->moveObjectFront(idx); else doc()->moveObjectBack(idx); });
        return documentSnapshotJson();
    }
    if (cmd == "reorder_object_to") {
        int from = iarg("from"), to = iarg("to");
        undoable([&] { moveObjectTo(from, to); });
        return documentSnapshotJson();
    }
    if (cmd == "flip_object") {
        int idx = iarg("index"); bool h = barg("horizontal");
        undoable([&] {
            if (inRange(idx)) { auto d = doc()->getDataObject(idx); if (h) d->horizontalFlip(); else d->verticalFlip(); }
        });
        return documentSnapshotJson();
    }

    // ---- color / path edits (undoable) ----
    if (cmd == "set_path_fill_color" || cmd == "set_path_stroke_color" || cmd == "set_path_stroke_width") {
        int idx = iarg("index"), pi = iarg("pathIndex");
        undoable([&] {
            if (!inRange(idx)) return;
            auto data = doc()->getDataObject(idx);
            if (pi < 0 || pi >= (int)data->paths.size()) return;
            bool hasStitch = !data->fillBlocks.empty() || !data->strokeBlocks.empty();
            if (cmd == "set_path_fill_color") {
                data->paths[pi].setFillColor(pesGetBrotherColor(iarg("brotherIndex")));
                if (hasStitch) data->applyFill();
            } else if (cmd == "set_path_stroke_color") {
                data->paths[pi].setStrokeColor(pesGetBrotherColor(iarg("brotherIndex")));
                if (hasStitch) data->applyStroke();
            } else {
                data->paths[pi].setStrokeWidth(farg("width"));
                if (hasStitch) data->applyStroke();
            }
        });
        return documentSnapshotJson();
    }
    if (cmd == "set_color_block") {
        int idx = iarg("index"), bi = iarg("blockIndex"), br = iarg("brotherIndex");
        undoable([&] {
            if (!inRange(idx)) return;
            auto data = doc()->getDataObject(idx);
            if (bi >= 0 && bi < (int)data->fillBlocks.size()) data->fillBlocks[bi].setColorFromIndex(br);
        });
        return documentSnapshotJson();
    }
    if (cmd == "swap_color_block") {
        int idx = iarg("index"), bi = iarg("blockIndex"), dir = iarg("dir");
        undoable([&] {
            if (!inRange(idx)) return;
            auto data = doc()->getDataObject(idx);
            int other = bi + dir;
            if (bi >= 0 && other >= 0 && bi < (int)data->fillBlocks.size() && other < (int)data->fillBlocks.size())
                std::swap(data->fillBlocks[bi], data->fillBlocks[other]);
        });
        return documentSnapshotJson();
    }
    if (cmd == "set_parameter") {
        int idx = iarg("index");
        std::string key = sarg("key");
        // Text re-shapes after every parameter change (mirrors the native
        // set_parameter command), which reads the font from MEMFS — check the
        // font BEFORE mutating so the frontend can fetch it and retry cleanly.
        if (inRange(idx)) {
            auto& p = doc()->getDataObject(idx)->parameter;
            // "font" is the set_parameter key the panel sends (matches
            // native set_param_str); the value is the new font's name.
            std::string font = (key == "font" && a.contains("value") && a["value"].is_string())
                                   ? a["value"].get<std::string>()
                                   : std::string(p.fontName);
            if (p.type == pesData::OBJECT_TYPE_SCALABLE_PPEF_TEXT) {
                if (font.empty()) font = "Thai001";
                if (!pescore::ppefFontAvailable(font)) return missingFontJson(font);
            } else if (p.type == pesData::OBJECT_TYPE_SCALABLE_TTF_TEXT) {
                if (font.empty()) font = "JS-Boaboon";
                if (!pescore::ttfFontAvailable(font)) return missingFontJson(font, "ttf");
            }
        }
        bool ok = undoableChecked([&]() -> bool {
            if (!inRange(idx)) return false;
            auto& jv = a["value"];
            bool applied = false;
            if (jv.is_boolean()) applied = setParamBool(idx, key, jv.get<bool>());
            else if (jv.is_string()) applied = setParamStr(idx, key, jv.get<std::string>());
            else if (jv.is_number()) applied = setParamNum(idx, key, jv.get<float>());
            if (applied) {
                auto data = doc()->getDataObject(idx);
                // type-specific regeneration (mirrors commands.rs): PPEF/TTF
                // text re-shapes (shared core); SVG re-colors paths +
                // regenerates stitches.
                if (!pescore::rebuildPpefText(*data) && !pescore::rebuildTtfText(*data))
                    pescore::updateSvgObject(*data);
            }
            return applied;
        });
        if (!ok) return errorJson("unknown parameter key: " + key);
        return documentSnapshotJson();
    }

    // ---- layer groups (undoable) ----
    if (cmd == "create_group") {
        std::string name = sarg("name");
        std::vector<int> members;
        if (a.contains("memberIndices")) for (auto& v : a["memberIndices"]) members.push_back(v.get<int>());
        undoable([&] { int id = doc()->createGroup(name, 0); for (int i : members) doc()->setObjectGroup(i, id); });
        return documentSnapshotJson();
    }
    if (cmd == "rename_group") {
        int id = iarg("id"); std::string name = sarg("name");
        undoable([&] { doc()->renameGroup(id, name); });
        return documentSnapshotJson();
    }
    if (cmd == "ungroup") {
        int id = iarg("id");
        undoable([&] {
            auto members = groupMembers(id);
            if (members.size() >= 2) {
                int k = (int)members.size();
                int top = members[k - 1];
                for (int j = k - 2; j >= 0; --j) moveObjectTo(members[j], top - (k - 1 - j));
            }
            doc()->deleteGroup(id);
        });
        return documentSnapshotJson();
    }
    if (cmd == "delete_group") {
        int id = iarg("id");
        undoable([&] {
            auto members = groupMembers(id);
            for (auto it = members.rbegin(); it != members.rend(); ++it) doc()->deleteObject(*it);
            doc()->deleteGroup(id);
        });
        return documentSnapshotJson();
    }
    if (cmd == "add_to_group") {
        int id = iarg("id");
        undoable([&] { for (auto& v : a["indices"]) doc()->setObjectGroup(v.get<int>(), id); });
        return documentSnapshotJson();
    }
    if (cmd == "remove_from_group") {
        undoable([&] { for (auto& v : a["indices"]) doc()->setObjectGroup(v.get<int>(), 0); });
        return documentSnapshotJson();
    }
    if (cmd == "set_group_collapsed") {
        int id = iarg("id"); bool c = barg("collapsed");
        doc()->setGroupCollapsed(id, c); // not undoable (UI metadata), matches commands.rs
        return documentSnapshotJson();
    }
    if (cmd == "set_group_visible" || cmd == "set_group_locked") {
        int id = iarg("id");
        bool val_ = cmd == "set_group_visible" ? barg("visible") : barg("locked");
        undoable([&] {
            for (auto& d : doc()->getDataObjects())
                if (d->parameter.groupId == id) {
                    if (cmd == "set_group_visible") d->parameter.visible = val_;
                    else d->parameter.locked = val_;
                }
        });
        return documentSnapshotJson();
    }

    // ---- PathEdit (world coords; pescore folds in the object rotation) ----
    if (cmd == "get_path_nodes") {
        json arr = json::array();
        for (auto& n : pescore::getPathNodes(doc(), iarg("index"), iarg("pathIndex")))
            arr.push_back({{"node_type", n.node_type}, {"x", n.x}, {"y", n.y},
                           {"cp1x", n.cp1x}, {"cp1y", n.cp1y}, {"cp2x", n.cp2x}, {"cp2y", n.cp2y}});
        return arr;
    }
    if (cmd == "move_path_node") {
        int i = iarg("index"), p = iarg("pathIndex"), ni = iarg("nodeIndex");
        float dx = farg("dx"), dy = farg("dy");
        undoableChecked([&] { return pescore::movePathNode(doc(), i, p, ni, dx, dy); });
        return documentSnapshotJson();
    }
    if (cmd == "move_path_handle") {
        int i = iarg("index"), p = iarg("pathIndex"), ci = iarg("cmdIndex"), cs = iarg("cpSlot");
        float dx = farg("dx"), dy = farg("dy");
        undoableChecked([&] { return pescore::movePathHandle(doc(), i, p, ci, cs, dx, dy); });
        return documentSnapshotJson();
    }
    if (cmd == "insert_path_node") {
        int i = iarg("index"), p = iarg("pathIndex"), ni = iarg("nodeIndex");
        float t = farg("t");
        undoableChecked([&] { return pescore::insertPathNode(doc(), i, p, ni, t); });
        return documentSnapshotJson();
    }
    if (cmd == "delete_path_node") {
        int i = iarg("index"), p = iarg("pathIndex"), ni = iarg("nodeIndex");
        undoableChecked([&] { return pescore::deletePathNode(doc(), i, p, ni); });
        return documentSnapshotJson();
    }
    if (cmd == "set_path_node_type") {
        int i = iarg("index"), p = iarg("pathIndex"), ni = iarg("nodeIndex");
        bool tc = barg("toCurve");
        undoableChecked([&] { return pescore::setPathNodeType(doc(), i, p, ni, tc); });
        return documentSnapshotJson();
    }

    // ---- StitchEdit (needle points; world coords) ----
    if (cmd == "get_stitch_points") {
        json arr = json::array();
        for (auto& b : pescore::getStitchPoints(doc(), iarg("index"))) {
            json pts = json::array();
            for (auto& p : b.points) pts.push_back({{"x", p.x}, {"y", p.y}, {"jump", p.jump}});
            arr.push_back({{"kind", b.kind}, {"block_index", b.block_index}, {"hex", b.hex}, {"points", pts}});
        }
        return arr;
    }
    if (cmd == "move_stitch_point") {
        int i = iarg("index"), k = iarg("kind"), bi = iarg("blockIndex"), pi = iarg("pointIndex");
        float dx = farg("dx"), dy = farg("dy");
        undoableChecked([&] { return pescore::moveStitchPoint(doc(), i, k, bi, pi, dx, dy); });
        return documentSnapshotJson();
    }
    if (cmd == "insert_stitch_point") {
        int i = iarg("index"), k = iarg("kind"), bi = iarg("blockIndex"), pi = iarg("pointIndex");
        undoableChecked([&] { return pescore::insertStitchPoint(doc(), i, k, bi, pi); });
        return documentSnapshotJson();
    }
    if (cmd == "insert_stitch_point_at") {
        int i = iarg("index"), k = iarg("kind"), bi = iarg("blockIndex"), ai = iarg("afterIndex");
        float x = farg("x"), y = farg("y");
        undoableChecked([&] { return pescore::insertStitchPointAt(doc(), i, k, bi, ai, x, y); });
        return documentSnapshotJson();
    }
    if (cmd == "delete_stitch_point") {
        int i = iarg("index"), k = iarg("kind"), bi = iarg("blockIndex"), pi = iarg("pointIndex");
        undoableChecked([&] { return pescore::deleteStitchPoint(doc(), i, k, bi, pi); });
        return documentSnapshotJson();
    }

    // ---- path operations (inset/outset/simplify/unite/separate/erase/up/down) ----
    if (cmd == "apply_path_op") {
        int i = iarg("index"), p = iarg("pathIndex");
        std::string op = sarg("op"); float v = farg("value");
        bool ok = undoableChecked([&] { return pescore::pathOp(doc(), i, p, op, v); });
        if (!ok) return errorJson("path op ล้มเหลว: " + op);
        return documentSnapshotJson();
    }

    // ---- undo / redo ----
    if (cmd == "undo") {
        if (g_undo.empty()) return documentSnapshotJson();
        std::string buf = g_undo.back(); g_undo.pop_back();
        g_redo.push_back(snapshotPPES());
        restorePPES(buf);
        return documentSnapshotJson();
    }
    if (cmd == "redo") {
        if (g_redo.empty()) return documentSnapshotJson();
        std::string buf = g_redo.back(); g_redo.pop_back();
        g_undo.push_back(snapshotPPES());
        restorePPES(buf);
        return documentSnapshotJson();
    }

    return errorJson("unknown command: " + cmd);
}

}  // namespace

// ---- embind exports ---------------------------------------------------------
static void web_set_resource_path(std::string path) { SetResourcePath(path.c_str()); }

static std::string web_pes_call(std::string cmd, std::string argsJson) {
    json a = json::object();
    if (!argsJson.empty()) {
        try { a = json::parse(argsJson); } catch (...) { a = json::object(); }
        if (!a.is_object()) a = json::object();
    }
    try {
        return dispatch(cmd, a).dump();
    } catch (const std::exception& e) {
        return errorJson(std::string("exception: ") + e.what()).dump();
    } catch (...) {
        return errorJson("unknown C++ exception").dump();
    }
}

static std::string web_load_input(std::string kind, val bytes) {
    std::vector<uint8_t> data = valToBytes(bytes);
    pesBuffer buf(reinterpret_cast<const char*>(data.data()), data.size());
    bool ok = false;
    if (kind == "ppes") {
        doc()->newDocument();
        ok = doc()->loadPPESFromBuffer(buf);
        clearHistory();
    } else {
        std::string before = snapshotPPES();
        pesData d;
        if (kind == "pes") ok = d.loadPESFromBuffer(buf, true);
        else if (kind == "svg") ok = d.loadSVGFromBuffer(buf, true);
        if (ok) { doc()->addObject(d); pushUndo(std::move(before)); }
    }
    if (!ok) return errorJson("load failed for kind: " + kind).dump();
    return documentSnapshotJson().dump();
}

static val web_object_png(int index) {
    if (!inRange(index)) return bytesToVal(nullptr, 0);
    sk_sp<SkImage> img = doc()->makePesImageSnapshot(index);
    if (!img) return bytesToVal(nullptr, 0);
    sk_sp<SkData> png = SkImageToPngData(img);
    if (!png) return bytesToVal(nullptr, 0);
    return bytesToVal(png->bytes(), png->size());
}

static val web_export_bytes(std::string format) {
    return pesBufferToVal(doc()->exportBufferAs(format));
}

// Write a fetched font into MEMFS — the on-demand counterpart of the preloaded
// stitch textures. Called by webEngine.ts when a command answers
// {"missing_font": ..., "font_kind": ...}.
static std::string writeFontFile(const char* dir, const std::string& path,
                                 const val& bytes, const std::string& name) {
    std::vector<uint8_t> data = valToBytes(bytes);
    if (data.empty()) return errorJson("empty font data: " + name).dump();
    ::mkdir(GetResourcePath(dir).c_str(), 0755); // no-op if it exists
    std::FILE* f = std::fopen(path.c_str(), "wb");
    if (!f) return errorJson("cannot write " + path).dump();
    std::fwrite(data.data(), 1, data.size(), f);
    std::fclose(f);
    return json{{"ok", true}}.dump();
}

// .ppef → read by PPEF_Reader (sqlite); .ttf → read by makeTtfTypeface.
static std::string web_load_ppef_font(std::string name, val bytes) {
    return writeFontFile("PPEF", pescore::ppefFontPath(name), bytes, name);
}
static std::string web_load_ttf_font(std::string name, val bytes) {
    return writeFontFile("TTF", pescore::ttfFontPath(name), bytes, name);
}

EMSCRIPTEN_BINDINGS(pes_web) {
    emscripten::function("set_resource_path", &web_set_resource_path);
    emscripten::function("pes_call", &web_pes_call);
    emscripten::function("load_input", &web_load_input);
    emscripten::function("object_png", &web_object_png);
    emscripten::function("export_bytes", &web_export_bytes);
    emscripten::function("load_ppef_font", &web_load_ppef_font);
    emscripten::function("load_ttf_font", &web_load_ttf_font);
}

// ---- parameter setters (verbatim from pes_ffi.cpp set_param_*) ---------------
namespace {
bool setParamNum(int obj_index, const std::string& k, float value) {
    if (!inRange(obj_index)) return false;
    auto* d = doc();
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
bool setParamBool(int obj_index, const std::string& k, bool value) {
    if (!inRange(obj_index)) return false;
    auto* d = doc();
    if (k == "border") d->setDataParameterTextBorder(obj_index, value);
    else if (k == "italic") d->setDataParameterTextItalic(obj_index, value);
    else if (k == "fillUnderlay") d->setDataParameterFillUnderlay(obj_index, value);
    else if (k == "fillPatternUnderlay") d->setDataParameterFillPatternUnderlay(obj_index, value);
    else return false;
    return true;
}
bool setParamStr(int obj_index, const std::string& k, const std::string& value) {
    if (!inRange(obj_index)) return false;
    if (k == "text") { doc()->setDataParameterText(obj_index, value); return true; }
    if (k == "font") { doc()->setDataParameterFont(obj_index, value); return true; }
    return false;
}
}  // namespace
