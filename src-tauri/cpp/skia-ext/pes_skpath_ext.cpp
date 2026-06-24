// pesGetVerb — faithful replacement for the SkiaApps fork's
// SkPath::getVerb(int). M150 exposes verbs as a public span (SkPath::verbs(),
// SkSpan<const SkPathVerb>), so we read it directly — Skia needs no patch.
#include "skia-ext/pes_skia_ext.h"

#include "include/core/SkPath.h"
#include "include/core/SkPathTypes.h"

namespace pes_skia {

uint8_t getVerb(const SkPath& path, int index) {
    const auto verbs = path.verbs();
    if ((unsigned)index < (unsigned)verbs.size()) {
        return (uint8_t)verbs[index];
    }
    return (uint8_t)SkPath::kDone_Verb;
}

}  // namespace pes_skia
