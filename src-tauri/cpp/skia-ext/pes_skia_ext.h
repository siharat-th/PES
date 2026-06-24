// pes_skia_ext.h — PES extensions over PRISTINE upstream Skia.
//
// Concept: we never patch Skia's own source. The few places the old
// SkiaApps fork modified Skia core that PES actually relies on are
// re-implemented here as standalone helpers that use only Skia's public
// and (build-visible) private headers. Engine call sites call into these
// instead of the fork's in-tree patches, so official Skia stays untouched.
//
// Surface (traced from real call sites — everything else in the fork's
// Skia divergence is dead code for PES):
//   * getVerb        — SkPath::getVerb(int)           [pesData, pesSatinColumn]
//   * encodePngWithDpi — SkPngEncoder::Encode(...,ppm) [pesDocument]
//   * fixWinding / resolveBuilder — SkOpBuilder::resolve()'s "+FixWinding"
//                                                       [pesData x6]
#pragma once

#include <cstdint>

class SkPath;
class SkPixmap;
class SkOpBuilder;
class SkWStream;

namespace pes_skia {

// Verb at `index`, or SkPath::kDone_Verb when out of range.
// Faithful replacement for the fork's SkPath::getVerb(int) using SkPathPriv.
uint8_t getVerb(const SkPath& path, int index);

// Normalise contour winding directions in place. Exact relocation of the
// stock (unmodified) SkOpBuilder::FixWinding — which the fork's resolve()
// called but upstream's does not. Idempotent on already-consistent paths.
bool fixWinding(SkPath* path);

// resolve() + fixWinding(): reproduces the fork's patched
// SkOpBuilder::resolve() behaviour without touching Skia. Returns the
// result of resolve(); winding is fixed only when resolve() succeeds.
bool resolveBuilder(SkOpBuilder& builder, SkPath* result);

// PNG-encode `src` and stamp a pHYs (physical pixel) chunk so the image
// carries DPI, where `ppm` is pixels-per-metre (0 = no pHYs, plain encode).
// Replaces the fork's SkPngEncoder::Encode(...,ppm) overload.
bool encodePngWithDpi(SkWStream* dst, const SkPixmap& src, uint32_t ppm);

}  // namespace pes_skia
