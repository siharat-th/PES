// PesPath — drop-in for code that built MUTABLE SkPaths before Skia M150 made
// SkPath immutable (moveTo/lineTo/addPath/... were removed; all building now
// goes through SkPathBuilder). PesPath wraps an SkPathBuilder, exposes the old
// mutating API, and converts to SkPath implicitly so existing read sites
// (Op/Simplify/getBounds/drawPath, which take const SkPath&) keep working.
//
// Notes / intentional limitations:
//  * The SkPath ctor is explicit so PesPath<->SkPath don't form ambiguous
//    implicit-conversion pairs in overloaded calls. Construct from a path with
//    `PesPath p(sk);` or assign `p = sk;`.
//  * In-out patterns like `Simplify(p, &p)` / `Op(a, b, op, &p)` cannot take
//    `&p` (it is PesPath*, not SkPath*). Use the member helpers p.simplify() /
//    p.op(other, mode) instead — those sites are rewritten during the port.
#pragma once

#include "include/core/SkPath.h"
#include "include/core/SkPathBuilder.h"
#include "include/core/SkPathTypes.h"
#include "include/core/SkPoint.h"
#include "include/core/SkRect.h"
#include "include/pathops/SkPathOps.h"
#include "src/core/SkPathPriv.h"

class PesPath {
public:
    SkPathBuilder b;

    PesPath() = default;
    explicit PesPath(const SkPath& p) { *this = p; }
    PesPath(const PesPath&) = default;
    PesPath& operator=(const PesPath&) = default;
    PesPath& operator=(const SkPath& p) {
        b.reset();
        b.setFillType(p.getFillType());
        if (!p.isEmpty()) b.addPath(p);
        return *this;
    }

    // Implicit view as a real (immutable) SkPath for read-only consumers.
    operator SkPath() const { return b.snapshot(); }
    SkPath skpath() const { return b.snapshot(); }

    // ---- building (delegates to SkPathBuilder) ----
    PesPath& moveTo(SkPoint p)                 { b.moveTo(p); return *this; }
    PesPath& moveTo(SkScalar x, SkScalar y)    { b.moveTo(x, y); return *this; }
    PesPath& lineTo(SkPoint p)                 { b.lineTo(p); return *this; }
    PesPath& lineTo(SkScalar x, SkScalar y)    { b.lineTo(x, y); return *this; }
    PesPath& quadTo(SkPoint a, SkPoint c)      { b.quadTo(a, c); return *this; }
    PesPath& quadTo(SkScalar x1, SkScalar y1, SkScalar x2, SkScalar y2) {
        b.quadTo(x1, y1, x2, y2); return *this; }
    PesPath& conicTo(SkPoint a, SkPoint c, SkScalar w) { b.conicTo(a, c, w); return *this; }
    PesPath& conicTo(SkScalar x1, SkScalar y1, SkScalar x2, SkScalar y2, SkScalar w) {
        b.conicTo(x1, y1, x2, y2, w); return *this; }
    PesPath& cubicTo(SkPoint a, SkPoint c, SkPoint d) { b.cubicTo(a, c, d); return *this; }
    PesPath& cubicTo(SkScalar x1, SkScalar y1, SkScalar x2, SkScalar y2, SkScalar x3, SkScalar y3) {
        b.cubicTo(x1, y1, x2, y2, x3, y3); return *this; }
    PesPath& close()                           { b.close(); return *this; }
    PesPath& addRect(const SkRect& r)          { b.addRect(r); return *this; }
    PesPath& addRect(SkScalar l, SkScalar t, SkScalar r, SkScalar bot) {
        b.addRect(SkRect::MakeLTRB(l, t, r, bot)); return *this; }
    PesPath& addOval(const SkRect& r)          { b.addOval(r); return *this; }
    PesPath& addCircle(SkScalar x, SkScalar y, SkScalar r) { b.addCircle(x, y, r); return *this; }
    PesPath& addPath(const SkPath& s)          { b.addPath(s); return *this; }
    PesPath& offset(SkScalar dx, SkScalar dy)  { b.offset(dx, dy); return *this; }
    PesPath& reset()                           { b.reset(); return *this; }
    PesPath& setFillType(SkPathFillType ft)    { b.setFillType(ft); return *this; }

    // Old SkPath::reverseAddPath added each src contour reversed as a NEW
    // contour. SkPathPriv::ReversePathTo continues from the current point, so
    // open a fresh contour at src's last point first.
    PesPath& reverseAddPath(const SkPath& s) {
        auto lastPt = s.getLastPt();
        if (lastPt.has_value()) {
            b.moveTo(*lastPt);
            SkPathPriv::ReversePathTo(&b, s);
        }
        return *this;
    }

    // ---- in-place pathops (replace `Simplify(p,&p)` / `Op(a,b,op,&p)`) ----
    bool simplify() {
        SkPath r;
        bool ok = Simplify(this->skpath(), &r);
        *this = r;
        return ok;
    }
    bool op(const SkPath& other, SkPathOp mode) {
        SkPath r;
        bool ok = Op(this->skpath(), other, mode, &r);
        *this = r;
        return ok;
    }

    // ---- reads (snapshot-delegated) ----
    SkPathFillType getFillType() const { return b.fillType(); }
    bool isEmpty() const               { return this->skpath().isEmpty(); }
    int countVerbs() const             { return this->skpath().countVerbs(); }
    bool contains(SkScalar x, SkScalar y) const { return this->skpath().contains(x, y); }
    SkRect getBounds() const           { return this->skpath().getBounds(); }
    SkRect computeTightBounds() const  { return this->skpath().computeTightBounds(); }
};
