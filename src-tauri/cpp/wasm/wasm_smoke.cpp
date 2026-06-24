// WASM smoke test — proves the PES *core* (engine + skia-ext) compiles to wasm
// and links against a STANDALONE pristine Skia-wasm (Skia built separately, no
// pes baked in). This is the dual-target validation: the same engine code that
// the native Tauri FFI uses also builds under emscripten. It deliberately does
// NOT include pes_ffi.cpp (that's the native cxx binding); the real web binding
// (embind) is a separate layer built on this same core.
#include <cstdio>

#include "include/core/SkPath.h"
#include "skia-ext/pes_skia_ext.h"
#include "skia-ext/pes_skpath_compat.h"

#include "pesData.hpp"
#include "pesPath.hpp"

int main() {
    // 1) skia-ext over pristine Skia-wasm: getVerb + fixWinding (the latter
    //    links Skia's internal pathops symbols — the real link risk).
    //    Skia M150 made SkPath immutable; build through PesPath (SkPathBuilder
    //    wrapper) then snapshot to a real SkPath for the read/pathops calls.
    PesPath builder;
    builder.moveTo(0, 0).lineTo(100, 0).lineTo(100, 100).close();
    SkPath p = builder;
    printf("[skia-ext] countVerbs=%d verb0=%d\n", p.countVerbs(), pes_skia::getVerb(p, 0));
    pes_skia::fixWinding(&p);
    printf("[skia-ext] fixWinding ok (fillType=%d)\n", (int)p.getFillType());

    // 2) pes engine types link + run under wasm.
    pesPath path;
    path.moveTo(0, 0);
    path.lineTo(50, 0);
    path.lineTo(50, 50);
    path.close();
    printf("[engine] pesPath commands=%d\n", (int)path.getCommands().size());

    pesData data;
    printf("[engine] pesData constructed\n");

    printf("PES WASM CORE OK\n");
    return 0;
}
