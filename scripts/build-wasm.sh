#!/usr/bin/env bash
# WASM smoke build for the PES core.
#
# Proves the engine + skia-ext compile to wasm and link against the STANDALONE
# pristine Skia-wasm (built by `build-skia.sh wasm`). Skia is never baked in —
# it is a separate set of static libs we link, exactly like the native build.
# This does NOT build the web app; it links a tiny wasm_smoke main to validate
# the dual-target architecture. The real web binding (embind) comes later.
set -euo pipefail

REPO="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
SKIA="$REPO/third_party/skia"
WOUT="$SKIA/out/wasm-release"
SQ="$REPO/third_party/sqlitecpp"
SRC="$REPO/src-tauri/cpp"
OBJ="$REPO/build/wasm-obj"
OUTDIR="$REPO/build/wasm"

# emsdk (reuse the toolchain that Skia-wasm was built with)
source "$SKIA/third_party/externals/emsdk/emsdk_env.sh" 2>/dev/null

ls "$WOUT"/libskia.wasm.a >/dev/null 2>&1 || { echo "Skia-wasm missing; run: build-skia.sh wasm" >&2; exit 1; }
mkdir -p "$OBJ" "$OUTDIR"

# wasm Skia ABI defines (from `gn desc out/wasm-release //:skia defines`, Skia
# M150). MUST match what libskia.wasm.a was built with (minus SKIA_IMPLEMENTATION,
# which is build-only). Note vs M112: SK_SUPPORT_GPU -> SK_GANESH, SK_ENCODE_* ->
# SK_CODEC_ENCODES_*, freetype-empty fontmgr; no SK_TRIVIAL_ABI on wasm.
DEFS="-DNDEBUG -DSK_GANESH -DSK_GL -DSK_ASSUME_WEBGL=1 -DSK_GAMMA_APPLY_TO_A8 \
-DSK_ENABLE_PRECOMPILE -DSKVX_DISABLE_SIMD -DSK_FORCE_8_BYTE_ALIGNMENT \
-DSK_DISABLE_TRACING -DSK_USE_PARTITION_ALLOC -DSK_ENABLE_AVX512_OPTS \
-DSK_TYPEFACE_FACTORY_FREETYPE -DSK_FONTMGR_FREETYPE_EMPTY_AVAILABLE \
-DSK_SUPPORT_PDF -DSK_XML -DSK_HAS_WUFFS_LIBRARY \
-DSK_CODEC_DECODES_BMP -DSK_CODEC_DECODES_WBMP -DSK_CODEC_DECODES_GIF \
-DSK_CODEC_DECODES_ICO -DSK_CODEC_DECODES_PNG -DSK_CODEC_DECODES_PNG_WITH_LIBPNG \
-DSK_CODEC_ENCODES_PNG -DSK_CODEC_ENCODES_PNG_WITH_LIBPNG \
-DSK_CODEC_DECODES_JPEG -DSK_CODEC_ENCODES_JPEG \
-DSK_CODEC_DECODES_WEBP -DSK_CODEC_ENCODES_WEBP"
INCS="-I$SRC -I$SRC/pes/include -I$SRC/ppef -I$SKIA -I$SQ/include -I$SQ/sqlite3"
CXXFLAGS="-O2 -std=c++17 -fexceptions $DEFS $INCS"

CORE_OBJS=()   # everything except the smoke main -> archived for on-demand link
SMOKE_OBJ=""
# incremental: recompile only when the object is missing or older than its source
cxx() {
  if [ ! -f "$2" ] || [ "$1" -nt "$2" ]; then echo "  cc $(basename "$1")"; em++ $CXXFLAGS -c "$1" -o "$2"; fi
  CORE_OBJS+=("$2")
}

echo "[1/4] sqlite3 (C)"
if [ ! -f "$OBJ/sqlite3.o" ] || [ "$SQ/sqlite3/sqlite3.c" -nt "$OBJ/sqlite3.o" ]; then
  emcc -O2 -c "$SQ/sqlite3/sqlite3.c" -o "$OBJ/sqlite3.o"
fi
CORE_OBJS+=("$OBJ/sqlite3.o")

echo "[2/4] pes engine"
for s in UnicodeHelper clipper pesAutoBranch pesBuffer pesClipper pesColor \
  pesCubicSuperPath pesData pesDocument pesEMBClassify pesEMBFill pesEffect \
  pesEncoder pesGcode pesMath pesPath pesPathUtility pesPolyline pesRectangle \
  pesSVG pesSatinColumn pesSatinOutline pesSkPath pesStitchBlock pesUtility \
  pesVec2f pugixml; do
  cxx "$SRC/pes/src/$s.cpp" "$OBJ/$s.o"
done

echo "[3/4] skia-ext + ppef + SQLiteCpp"
for s in pes_skpath_ext pes_png_ext pes_pathops_ext; do cxx "$SRC/skia-ext/$s.cpp" "$OBJ/$s.o"; done
for s in PesPPEFUtils PesUnicodeUtils; do cxx "$SRC/ppef/$s.cpp" "$OBJ/ppef_$s.o"; done
for s in Backup Column Database Exception Statement Transaction; do cxx "$SQ/src/$s.cpp" "$OBJ/sqlcpp_$s.o"; done
SMOKE_OBJ="$OBJ/wasm_smoke.o"
if [ ! -f "$SMOKE_OBJ" ] || [ "$SRC/wasm/wasm_smoke.cpp" -nt "$SMOKE_OBJ" ]; then
  echo "  cc wasm_smoke.cpp"; em++ $CXXFLAGS -c "$SRC/wasm/wasm_smoke.cpp" -o "$SMOKE_OBJ"
fi

echo "[4/4] link -> $OUTDIR/pes_smoke.js"
# Archive the core so wasm-ld pulls members on demand (matches the native cxx
# build). UnicodeHelper.cpp and PesUnicodeUtils.cpp both define th_is* Thai
# helpers; on-demand loading avoids the duplicate-symbol clash unless both are
# actually referenced (a real dedupe TODO for the full PPEF web build).
emar rcs "$OUTDIR/libpescore.a" "${CORE_OBJS[@]}"
em++ -O2 -fexceptions -sDISABLE_EXCEPTION_CATCHING=0 -sALLOW_MEMORY_GROWTH=1 \
  -sENVIRONMENT=web,node -sEXIT_RUNTIME=1 \
  "$SMOKE_OBJ" "$OUTDIR/libpescore.a" "$WOUT"/*.wasm.a -o "$OUTDIR/pes_smoke.js"

echo "=== run ==="
node "$OUTDIR/pes_smoke.js"
