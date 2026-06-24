// pesFixWinding / pesResolveBuilder.
//
// Upstream's SkOpBuilder::resolve() does not normalise winding on its fast
// path; the SkiaApps fork patched it to call the (private, otherwise
// unmodified) static SkOpBuilder::FixWinding. To keep Skia pristine we relocate
// that stock FixWinding here verbatim (plus the file-local one_contour and
// ReversePath helpers) and call it ourselves after resolve(). Its internal
// pathops dependencies (SkOpEdgeBuilder, FindSortableTop, SkPathWriter, ...) are
// global symbols in libskia, so this links against unmodified upstream Skia.
//
// Bodies copied unchanged from src/pathops/SkOpBuilder.cpp (Skia M150) so
// embroidery fill output matches the fork bit-for-bit.
#include "skia-ext/pes_skia_ext.h"

#include "include/core/SkPath.h"
#include "include/core/SkPathBuilder.h"
#include "include/core/SkPathTypes.h"
#include "include/core/SkPoint.h"
#include "include/core/SkTypes.h"
#include "include/pathops/SkPathOps.h"
#include "include/private/base/SkTo.h"
#include "src/base/SkArenaAlloc.h"
#include "src/core/SkPathEnums.h"
#include "src/core/SkPathPriv.h"
#include "src/pathops/SkOpContour.h"
#include "src/pathops/SkOpEdgeBuilder.h"
#include "src/pathops/SkOpSegment.h"
#include "src/pathops/SkOpSpan.h"
#include "src/pathops/SkPathOpsCommon.h"
#include "src/pathops/SkPathOpsTypes.h"
#include "src/pathops/SkPathWriter.h"

#include <cstdint>
#include <optional>

namespace {

// Relocated from SkOpBuilder.cpp (file-local statics in Skia M150).
bool one_contour(const SkPath& path) {
    const auto raw = SkPathPriv::Raw(path, SkResolveConvexity::kNo);
    if (!raw) {
        return false;
    }
    const auto verbs = raw->verbs();
    for (size_t i = 1; i < verbs.size(); ++i) {
        if (verbs[i] == SkPathVerb::kMove) {
            return false;
        }
    }
    return true;
}

void reversePath(SkPath* path) {
    auto lastPt = path->getLastPt();
    SkASSERT(lastPt.has_value());
    SkPathBuilder temp;
    temp.moveTo(*lastPt);
    SkPathPriv::ReversePathTo(&temp, *path);
    temp.close();
    *path = temp.detach();
}

}  // namespace

namespace pes_skia {

bool fixWinding(SkPath* path) {
    SkPathFillType fillType = path->getFillType();
    if (fillType == SkPathFillType::kInverseEvenOdd) {
        fillType = SkPathFillType::kInverseWinding;
    } else if (fillType == SkPathFillType::kEvenOdd) {
        fillType = SkPathFillType::kWinding;
    }
    if (one_contour(*path)) {
        SkPathFirstDirection dir = SkPathPriv::ComputeFirstDirection(*path);
        if (dir != SkPathFirstDirection::kUnknown) {
            if (dir == SkPathFirstDirection::kCW) {
                reversePath(path);
            }
            path->setFillType(fillType);
            return true;
        }
    }
    SkSTArenaAlloc<4096> allocator;
    SkOpContourHead contourHead;
    SkOpGlobalState globalState(&contourHead, &allocator SkDEBUGPARAMS(false)
                                        SkDEBUGPARAMS(nullptr));
    SkOpEdgeBuilder builder(*path, &contourHead, &globalState);
    if (builder.unparseable() || !builder.finish()) {
        return false;
    }
    if (!contourHead.count()) {
        return true;
    }
    if (!contourHead.next()) {
        return false;
    }
    contourHead.joinAllSegments();
    contourHead.resetReverse();
    bool writePath = false;
    SkOpSpan* topSpan;
    globalState.setPhase(SkOpPhase::kFixWinding);
    while ((topSpan = FindSortableTop(&contourHead))) {
        SkOpSegment* topSegment = topSpan->segment();
        SkOpContour* topContour = topSegment->contour();
        SkASSERT(topContour->isCcw() >= 0);
        if ((globalState.nested() & 1) != SkToBool(topContour->isCcw())) {
            topContour->setReverse();
            writePath = true;
        }
        topContour->markAllDone();
        globalState.clearNested();
    }
    if (!writePath) {
        path->setFillType(fillType);
        return true;
    }
    SkPathWriter woundPath(fillType);
    SkOpContour* test = &contourHead;
    do {
        if (!test->count()) {
            continue;
        }
        if (test->reversed()) {
            test->toReversePath(&woundPath);
        } else {
            test->toPath(&woundPath);
        }
    } while ((test = test->next()));
    *path = woundPath.nativePath();
    return true;
}

bool resolveBuilder(SkOpBuilder& builder, SkPath* result) {
    if (!builder.resolve(result)) {
        return false;
    }
    fixWinding(result);
    return true;
}

}  // namespace pes_skia
