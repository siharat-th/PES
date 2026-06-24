#pragma once
// Transport-agnostic PathEdit / StitchEdit / path-op logic, shared by the native
// cxx facade (pes_ffi.cpp) and the wasm/embind web binding (wasm/pes_web.cpp).
// These ops were the source of subtle "ลายปักหาย" (vanishing-stitch) regressions,
// so they MUST be identical on desktop and web — hence one copy here. Functions
// take a pesDocument* and return plain data / bool; no rust:: or emscripten::
// types. Coordinates that cross to the UI are WORLD units (the object's display
// rotation folded in around the bbox center), matching the canvas.

#include <cmath>
#include <string>
#include <vector>

#include "pes_ffi_core.hpp" // colorToHex + pesData/pesDocument/pesColor/pesStitchBlock
#include "pesPathUtility.hpp" // toSk / toPes
#include "include/core/SkPathUtils.h"
#include "include/pathops/SkPathOps.h"

namespace pescore {

// ---- plain-data views handed to the UI -------------------------------------
struct PathNodeData {
    int node_type;
    float x, y, cp1x, cp1y, cp2x, cp2y;
};
struct EditStitchPoint {
    float x, y;
    bool jump;
};
struct EditStitchBlock {
    int kind, block_index;
    std::string hex;
    std::vector<EditStitchPoint> points;
};

// ---- shared helpers (were pes_ffi.cpp local) -------------------------------
inline bool hasStitch(pesDocument* doc, int objIndex) {
    auto data = doc->getDataObject(objIndex);
    return !data->fillBlocks.empty() || !data->strokeBlocks.empty();
}

inline bool validPath(pesDocument* doc, int objIndex, int pathIndex) {
    if (objIndex < 0 || objIndex >= doc->getObjectCount())
        return false;
    auto data = doc->getDataObject(objIndex);
    return pathIndex >= 0 && pathIndex < (int)data->paths.size();
}

inline SkPath skiaPathStroke(const SkPath& skPath, float value) {
    SkPaint paint;
    paint.setStyle(SkPaint::Style::kStroke_Style);
    paint.setStrokeWidth(value * 2);
    paint.setStrokeCap(SkPaint::Cap::kButt_Cap);
    paint.setStrokeJoin(SkPaint::Join::kMiter_Join);
    return skpathutils::FillPathWithPaint(skPath, paint);
}

inline void replacePath(pesData* pes, int idx, const SkPath& skPath) {
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
// type-dependent — applyFill() on PPEF/Monogram text wipes the fill.
inline void reapplyStitches(pesDocument* doc, int objIndex, pesData* pes) {
    if (!hasStitch(doc, objIndex))
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

inline pesStitchBlockList* stitchBlockListOf(pesData* pes, int kind) {
    if (kind == 0) return &pes->fillBlocks;
    if (kind == 1) return &pes->strokeBlocks;
    return nullptr;
}

inline pesStitchBlock* stitchBlockAt(pesDocument* doc, int objIndex, int kind, int blockIndex) {
    if (objIndex < 0 || objIndex >= doc->getObjectCount())
        return nullptr;
    pesData* pes = doc->getDataObject(objIndex).get();
    pesStitchBlockList* list = stitchBlockListOf(pes, kind);
    if (!list || blockIndex < 0 || blockIndex >= (int)list->size())
        return nullptr;
    return &(*list)[blockIndex];
}

// ---- PathEdit ---------------------------------------------------------------
inline std::vector<PathNodeData> getPathNodes(pesDocument* doc, int objIndex, int pathIndex) {
    std::vector<PathNodeData> out;
    if (!validPath(doc, objIndex, pathIndex))
        return out;
    auto data = doc->getDataObject(objIndex);
    float angle = doc->getDataParameter(objIndex).rotateDegree;
    pesVec2f center = data->getBoundingBox().getCenter();
    auto toWorld = [&](float x, float y) -> pesVec2f {
        pesVec2f v(x, y);
        if (std::abs(angle) > 1e-4f)
            v.rotate(angle, center);
        return v;
    };
    for (const auto& c : data->paths[pathIndex].getCommands()) {
        pesVec2f to = toWorld(c.to.x, c.to.y);
        pesVec2f cp1 = toWorld(c.cp1.x, c.cp1.y);
        pesVec2f cp2 = toWorld(c.cp2.x, c.cp2.y);
        out.push_back(PathNodeData{(int)c.type, to.x, to.y, cp1.x, cp1.y, cp2.x, cp2.y});
    }
    return out;
}

inline bool movePathNode(pesDocument* doc, int objIndex, int pathIndex, int nodeIndex,
                         float worldDx, float worldDy) {
    if (!validPath(doc, objIndex, pathIndex))
        return false;
    pesData* pes = doc->getDataObject(objIndex).get();
    auto& cmds = pes->paths[pathIndex].getCommands();
    if (nodeIndex < 0 || nodeIndex >= (int)cmds.size())
        return false;

    pesVec2f d(worldDx, worldDy);
    float angle = doc->getDataParameter(objIndex).rotateDegree;
    if (std::abs(angle) > 1e-4f)
        d.rotate(-angle);

    using Cmd = pesPath::Command;
    auto& c = cmds[nodeIndex];
    c.to.x += d.x; c.to.y += d.y;
    if (c.type == Cmd::_bezierTo || c.type == Cmd::_quadBezierTo) {
        c.cp2.x += d.x; c.cp2.y += d.y;
    }
    if (nodeIndex + 1 < (int)cmds.size()) {
        auto& nx = cmds[nodeIndex + 1];
        if (nx.type == Cmd::_bezierTo) {
            nx.cp1.x += d.x; nx.cp1.y += d.y;
        } else if (nx.type == Cmd::_quadBezierTo) {
            nx.cp1.x += d.x; nx.cp1.y += d.y;
            nx.cp2.x += d.x; nx.cp2.y += d.y;
        }
    }
    pes->paths[pathIndex].flagShapeChanged();
    reapplyStitches(doc, objIndex, pes);
    return true;
}

inline bool movePathHandle(pesDocument* doc, int objIndex, int pathIndex, int cmdIndex,
                           int cpSlot, float worldDx, float worldDy) {
    if (!validPath(doc, objIndex, pathIndex))
        return false;
    pesData* pes = doc->getDataObject(objIndex).get();
    auto& cmds = pes->paths[pathIndex].getCommands();
    if (cmdIndex < 0 || cmdIndex >= (int)cmds.size())
        return false;

    pesVec2f d(worldDx, worldDy);
    float angle = doc->getDataParameter(objIndex).rotateDegree;
    if (std::abs(angle) > 1e-4f)
        d.rotate(-angle);

    auto& c = cmds[cmdIndex];
    if (cpSlot == 1) {
        c.cp1.x += d.x; c.cp1.y += d.y;
    } else if (cpSlot == 2) {
        c.cp2.x += d.x; c.cp2.y += d.y;
    } else {
        return false;
    }
    pes->paths[pathIndex].flagShapeChanged();
    reapplyStitches(doc, objIndex, pes);
    return true;
}

inline bool insertPathNode(pesDocument* doc, int objIndex, int pathIndex, int nodeIndex, float t) {
    if (!validPath(doc, objIndex, pathIndex))
        return false;
    pesData* pes = doc->getDataObject(objIndex).get();
    auto& cmds = pes->paths[pathIndex].getCommands();
    const int k = nodeIndex;
    if (k < 1 || k >= (int)cmds.size())
        return false;

    using Cmd = pesPath::Command;
    auto& seg = cmds[k];
    if (seg.type != Cmd::_lineTo && seg.type != Cmd::_bezierTo &&
        seg.type != Cmd::_quadBezierTo)
        return false;

    if (t < 1e-3f) t = 1e-3f;
    if (t > 1.f - 1e-3f) t = 1.f - 1e-3f;

    auto lerp = [t](const pesVec3f& a, const pesVec3f& b) {
        pesVec3f r;
        r.x = a.x + (b.x - a.x) * t;
        r.y = a.y + (b.y - a.y) * t;
        r.z = a.z + (b.z - a.z) * t;
        return r;
    };

    const pesVec3f P0 = cmds[k - 1].to;

    if (seg.type == Cmd::_lineTo) {
        Cmd mid = seg;
        mid.to = lerp(P0, seg.to);
        cmds.insert(cmds.begin() + k, mid);
    } else if (seg.type == Cmd::_bezierTo) {
        const pesVec3f A = lerp(P0, seg.cp1);
        const pesVec3f B = lerp(seg.cp1, seg.cp2);
        const pesVec3f C = lerp(seg.cp2, seg.to);
        const pesVec3f AB = lerp(A, B);
        const pesVec3f BC = lerp(B, C);
        const pesVec3f M = lerp(AB, BC);
        Cmd left = seg;  left.to = M;       left.cp1 = A;  left.cp2 = AB;
        Cmd right = seg; right.to = seg.to; right.cp1 = BC; right.cp2 = C;
        cmds[k] = left;
        cmds.insert(cmds.begin() + k + 1, right);
    } else { // _quadBezierTo: cp1 = START, cp2 = control, to = end
        const pesVec3f A = lerp(P0, seg.cp2);
        const pesVec3f Bq = lerp(seg.cp2, seg.to);
        const pesVec3f M = lerp(A, Bq);
        Cmd left = seg;  left.to = M;       left.cp1 = P0; left.cp2 = A;
        Cmd right = seg; right.to = seg.to; right.cp1 = M;  right.cp2 = Bq;
        cmds[k] = left;
        cmds.insert(cmds.begin() + k + 1, right);
    }

    pes->paths[pathIndex].flagShapeChanged();
    reapplyStitches(doc, objIndex, pes);
    return true;
}

inline bool deletePathNode(pesDocument* doc, int objIndex, int pathIndex, int nodeIndex) {
    if (!validPath(doc, objIndex, pathIndex))
        return false;
    pesData* pes = doc->getDataObject(objIndex).get();
    auto& cmds = pes->paths[pathIndex].getCommands();
    const int k = nodeIndex;
    if (k < 0 || k >= (int)cmds.size())
        return false;

    using Cmd = pesPath::Command;
    if (cmds[k].type == Cmd::_moveTo || cmds[k].type == Cmd::_close)
        return false;

    int start = k;
    while (start > 0 && cmds[start].type != Cmd::_moveTo)
        --start;
    int end = k + 1;
    while (end < (int)cmds.size() && cmds[end].type != Cmd::_moveTo)
        ++end;
    int anchors = 0;
    for (int j = start; j < end; ++j)
        if (cmds[j].type != Cmd::_close)
            ++anchors;
    if (anchors <= 2)
        return false;

    cmds.erase(cmds.begin() + k);
    if (k >= 1 && k < (int)cmds.size() && cmds[k].type == Cmd::_quadBezierTo)
        cmds[k].cp1 = cmds[k - 1].to;
    pes->paths[pathIndex].flagShapeChanged();
    reapplyStitches(doc, objIndex, pes);
    return true;
}

inline bool setPathNodeType(pesDocument* doc, int objIndex, int pathIndex, int nodeIndex,
                            bool toCurve) {
    if (!validPath(doc, objIndex, pathIndex))
        return false;
    pesData* pes = doc->getDataObject(objIndex).get();
    auto& cmds = pes->paths[pathIndex].getCommands();
    const int k = nodeIndex;
    if (k < 1 || k >= (int)cmds.size())
        return false;

    using Cmd = pesPath::Command;
    auto& c = cmds[k];
    const pesVec3f P0 = cmds[k - 1].to;

    if (toCurve) {
        if (c.type == Cmd::_bezierTo)
            return false;
        if (c.type != Cmd::_lineTo && c.type != Cmd::_quadBezierTo)
            return false;
        auto along = [&](float f) {
            pesVec3f r;
            r.x = P0.x + (c.to.x - P0.x) * f;
            r.y = P0.y + (c.to.y - P0.y) * f;
            r.z = P0.z + (c.to.z - P0.z) * f;
            return r;
        };
        c.cp1 = along(1.f / 3.f);
        c.cp2 = along(2.f / 3.f);
        c.type = Cmd::_bezierTo;
    } else {
        if (c.type == Cmd::_lineTo)
            return false;
        if (c.type != Cmd::_bezierTo && c.type != Cmd::_quadBezierTo)
            return false;
        c.type = Cmd::_lineTo;
    }
    pes->paths[pathIndex].flagShapeChanged();
    reapplyStitches(doc, objIndex, pes);
    return true;
}

// ---- StitchEdit -------------------------------------------------------------
inline std::vector<EditStitchBlock> getStitchPoints(pesDocument* doc, int objIndex) {
    std::vector<EditStitchBlock> out;
    if (objIndex < 0 || objIndex >= doc->getObjectCount())
        return out;
    auto data = doc->getDataObject(objIndex);
    float angle = doc->getDataParameter(objIndex).rotateDegree;
    pesVec2f center = data->getBoundingBox().getCenter();
    auto toWorld = [&](const pesVec2f& v) -> pesVec2f {
        pesVec2f w(v.x, v.y);
        if (std::abs(angle) > 1e-4f)
            w.rotate(angle, center);
        return w;
    };
    auto appendList = [&](pesStitchBlockList& list, int kind) {
        for (size_t bi = 0; bi < list.size(); ++bi) {
            auto& block = list[bi];
            auto& verts = block.polyline.getVertices();
            if (verts.empty())
                continue;
            EditStitchBlock sb;
            sb.kind = kind;
            sb.block_index = (int)bi;
            sb.hex = colorToHex(block.color);
            for (size_t i = 0; i < verts.size(); ++i) {
                pesVec2f w = toWorld(verts[i]);
                bool jump = i < block.types.size() && block.types[i] != 0;
                sb.points.push_back(EditStitchPoint{w.x, w.y, jump});
            }
            out.push_back(std::move(sb));
        }
    };
    appendList(data->fillBlocks, 0);
    appendList(data->strokeBlocks, 1);
    return out;
}

inline bool moveStitchPoint(pesDocument* doc, int objIndex, int kind, int blockIndex,
                            int pointIndex, float worldDx, float worldDy) {
    pesStitchBlock* block = stitchBlockAt(doc, objIndex, kind, blockIndex);
    if (!block)
        return false;
    auto& verts = block->polyline.getVertices();
    if (pointIndex < 0 || pointIndex >= (int)verts.size())
        return false;
    pesVec2f d(worldDx, worldDy);
    float angle = doc->getDataParameter(objIndex).rotateDegree;
    if (std::abs(angle) > 1e-4f)
        d.rotate(-angle);
    verts[pointIndex].translate(d);
    doc->getDataObject(objIndex)->recalculate();
    return true;
}

inline bool insertStitchPoint(pesDocument* doc, int objIndex, int kind, int blockIndex,
                              int pointIndex) {
    pesStitchBlock* block = stitchBlockAt(doc, objIndex, kind, blockIndex);
    if (!block)
        return false;
    auto& verts = block->polyline.getVertices();
    const int n = (int)verts.size();
    if (n < 2 || pointIndex < 0 || pointIndex >= n)
        return false;
    int leftIndex, rightIndex;
    if (pointIndex == n - 1) {
        leftIndex = pointIndex - 1;
        rightIndex = pointIndex;
    } else {
        leftIndex = pointIndex;
        rightIndex = pointIndex + 1;
    }
    pesVec2f mid = (verts[leftIndex] + verts[rightIndex]) / 2.0f;
    verts.insert(verts.begin() + rightIndex, mid);
    if (rightIndex <= (int)block->types.size())
        block->types.insert(block->types.begin() + rightIndex, (uint8_t)NORMAL_STITCH);
    doc->getDataObject(objIndex)->recalculate();
    return true;
}

inline bool insertStitchPointAt(pesDocument* doc, int objIndex, int kind, int blockIndex,
                                int afterIndex, float worldX, float worldY) {
    pesStitchBlock* block = stitchBlockAt(doc, objIndex, kind, blockIndex);
    if (!block)
        return false;
    auto& verts = block->polyline.getVertices();
    if (afterIndex < 0 || afterIndex >= (int)verts.size())
        return false;
    auto data = doc->getDataObject(objIndex);
    pesVec2f p(worldX, worldY);
    float angle = doc->getDataParameter(objIndex).rotateDegree;
    if (std::abs(angle) > 1e-4f)
        p.rotate(-angle, data->getBoundingBox().getCenter());
    const int at = afterIndex + 1;
    verts.insert(verts.begin() + at, p);
    if (at <= (int)block->types.size())
        block->types.insert(block->types.begin() + at, (uint8_t)NORMAL_STITCH);
    data->recalculate();
    return true;
}

inline bool deleteStitchPoint(pesDocument* doc, int objIndex, int kind, int blockIndex,
                              int pointIndex) {
    pesStitchBlock* block = stitchBlockAt(doc, objIndex, kind, blockIndex);
    if (!block)
        return false;
    auto& verts = block->polyline.getVertices();
    if (pointIndex < 0 || pointIndex >= (int)verts.size())
        return false;
    verts.erase(verts.begin() + pointIndex);
    if (pointIndex < (int)block->types.size())
        block->types.erase(block->types.begin() + pointIndex);
    doc->getDataObject(objIndex)->recalculate();
    return true;
}

// ---- path operations (inset/outset/simplify/unite/separate/erase/up/down) ---
inline bool pathOp(pesDocument* doc, int objIndex, int pathIndex, const std::string& op,
                   float value) {
    if (objIndex < 0 || objIndex >= doc->getObjectCount())
        return false;
    pesData* pes = doc->getDataObject(objIndex).get();
    int n = (int)pes->paths.size();

    if (op == "up") {
        if (pathIndex < 1 || pathIndex >= n) return false;
        std::swap(pes->paths[pathIndex], pes->paths[pathIndex - 1]);
        reapplyStitches(doc, objIndex, pes);
        return true;
    }
    if (op == "down") {
        if (pathIndex < 0 || pathIndex >= n - 1) return false;
        std::swap(pes->paths[pathIndex], pes->paths[pathIndex + 1]);
        reapplyStitches(doc, objIndex, pes);
        return true;
    }
    if (op == "inset" || op == "outset") {
        if (pathIndex < 0 || pathIndex >= n) return false;
        SkPath skPath = toSk(pes->paths[pathIndex]);
        SkPath outlinePath = skiaPathStroke(skPath, value);
        Simplify(outlinePath, &outlinePath);
        Op(outlinePath, skPath,
           op == "inset" ? SkPathOp::kReverseDifference_SkPathOp
                         : SkPathOp::kDifference_SkPathOp,
           &outlinePath);
        replacePath(pes, pathIndex, outlinePath);
        reapplyStitches(doc, objIndex, pes);
        return true;
    }
    if (op == "simplify") {
        if (pathIndex < 0 || pathIndex >= n) return false;
        pesPath& tpath = pes->paths[pathIndex];
        std::vector<pesPath> subpath = tpath.getSubPath();
        SkPath skPath = toSk(tpath);
        if (!subpath.empty())
            skPath = toSk(subpath[0]);
        Simplify(skPath, &skPath);
        replacePath(pes, pathIndex, skPath);
        reapplyStitches(doc, objIndex, pes);
        return true;
    }
    if (op == "unite_next") {
        if (pathIndex < 0 || pathIndex >= n - 1) return false;
        SkPath path1 = toSk(pes->paths[pathIndex]);
        SkPath path2 = toSk(pes->paths[pathIndex + 1]);
        Op(path1, path2, SkPathOp::kUnion_SkPathOp, &path1);
        replacePath(pes, pathIndex, path1);
        pes->paths[pathIndex + 1].clear();
        pes->paths.erase(pes->paths.begin() + (pathIndex + 1));
        reapplyStitches(doc, objIndex, pes);
        return true;
    }
    if (op == "separate") {
        if (pathIndex < 0 || pathIndex >= n) return false;
        pesPath& tpath = pes->paths[pathIndex];
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
            pes->paths.insert(pes->paths.begin() + (pathIndex + i + 1), path);
        }
        reapplyStitches(doc, objIndex, pes);
        return true;
    }
    if (op == "erase_under") {
        if (pathIndex < 1 || pathIndex >= n) return false;
        SkPath eraserPath = toSk(pes->paths[pathIndex]);
        for (int ip = 0; ip < pathIndex; ++ip) {
            pesPath& tpath = pes->paths[ip];
            if (!tpath.bVisible)
                continue;
            bool isFill = tpath.isFill();
            bool isStroke = tpath.isStroke();
            pesColor strokeColor = tpath.getStrokeColor();
            SkPath targetPath = toSk(tpath);
            if (isStroke && !isFill) {
                SkPath outlinePath = skiaPathStroke(
                    targetPath, tpath.getStrokeWidth() / 2 * pes->parameter.ppefScaleX);
                Op(outlinePath, eraserPath, SkPathOp::kDifference_SkPathOp, &targetPath);
                pesPath path = toPes(targetPath);
                path.setStrokeColor(strokeColor);
                path.setFillColor(strokeColor);
                path.setStrokeWidth(0);
                path.setFilled(true);
                pes->paths[ip].clear();
                pes->paths[ip] = path;
            } else {
                Op(eraserPath, targetPath, SkPathOp::kReverseDifference_SkPathOp, &targetPath);
                replacePath(pes, ip, targetPath);
            }
        }
        reapplyStitches(doc, objIndex, pes);
        return true;
    }
    return false;
}

}  // namespace pescore
