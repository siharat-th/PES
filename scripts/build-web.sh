#!/usr/bin/env bash
# Build the PES engine as a browser wasm module (embind) for the web app.
#
# Same engine + skia-ext as the native Tauri build and the wasm smoke, compiled
# to wasm and linked against the STANDALONE pristine Skia-wasm (built by
# `build-skia.sh wasm`). Adds the embind binding `cpp/wasm/pes_web.cpp` and the
# shared `cpp/pes_ffi_core.hpp`, and preloads the stitch textures so the engine's
# loadAssets() succeeds. Output is an ES6 module the frontend loads at runtime.
#
#   scripts/build-web.sh        # incremental
#
# Output -> public/wasm/pes_web.{js,wasm,data}  (served verbatim by vite at /wasm)
set -euo pipefail

REPO="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
SKIA="$REPO/third_party/skia"
WOUT="$SKIA/out/wasm-release"
SQ="$REPO/third_party/sqlitecpp"
SRC="$REPO/src-tauri/cpp"
RES="$REPO/src-tauri/resources"
OBJ="$REPO/build/web-obj"
OUTDIR="$REPO/public/wasm"

source "$SKIA/third_party/externals/emsdk/emsdk_env.sh" 2>/dev/null

ls "$WOUT"/libskia.wasm.a >/dev/null 2>&1 || { echo "Skia-wasm missing; run: build-skia.sh wasm" >&2; exit 1; }
mkdir -p "$OBJ" "$OUTDIR"

# wasm Skia ABI defines (must match libskia.wasm.a — see build-wasm.sh).
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

CORE_OBJS=()
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

echo "[2/4] pes engine (+ ppef helpers)"
# ppef objects FIRST: PesUnicodeUtils defines the same th_is* helpers as the
# engine's UnicodeHelper *plus* UTF8Iterator. wasm-ld pulls archive members in
# order per unresolved symbol, so PesUnicodeUtils must satisfy PesPPEFUtils's
# th_is* refs before UnicodeHelper does — otherwise both members load and the
# th_is* definitions collide (ld64 on native resolves it the same way).
for s in PesPPEFUtils PesUnicodeUtils; do cxx "$SRC/ppef/$s.cpp" "$OBJ/ppef_$s.o"; done
for s in UnicodeHelper clipper pesAutoBranch pesBuffer pesClipper pesColor \
  pesCubicSuperPath pesData pesDocument pesEMBClassify pesEMBFill pesEffect \
  pesEncoder pesGcode pesMath pesPath pesPathUtility pesPolyline pesRectangle \
  pesSVG pesSatinColumn pesSatinOutline pesSkPath pesStitchBlock pesUtility \
  pesVec2f pugixml; do
  cxx "$SRC/pes/src/$s.cpp" "$OBJ/$s.o"
done

echo "[3/4] skia-ext + SQLiteCpp + web binding"
for s in pes_skpath_ext pes_png_ext pes_pathops_ext; do cxx "$SRC/skia-ext/$s.cpp" "$OBJ/$s.o"; done
for s in Backup Column Database Exception Statement Transaction; do cxx "$SQ/src/$s.cpp" "$OBJ/sqlcpp_$s.o"; done
cxx "$SRC/pes_resources.cpp" "$OBJ/pes_resources.o"
# embind binding compiled with --bind (recompile when any shared core changes)
WEB_OBJ="$OBJ/pes_web.o"
if [ ! -f "$WEB_OBJ" ] || [ "$SRC/wasm/pes_web.cpp" -nt "$WEB_OBJ" ] || [ "$SRC/pes_ffi_core.hpp" -nt "$WEB_OBJ" ] \
   || [ "$SRC/pes_edit_core.hpp" -nt "$WEB_OBJ" ] || [ "$SRC/pes_text_core.hpp" -nt "$WEB_OBJ" ] \
   || [ "$SRC/pes_satin_core.hpp" -nt "$WEB_OBJ" ]; then
  echo "  cc pes_web.cpp"; em++ $CXXFLAGS --bind -c "$SRC/wasm/pes_web.cpp" -o "$WEB_OBJ"
fi

echo "[4/4] link -> $OUTDIR/pes_web.js  (preloading textures)"
# On-demand archive (matches native + smoke) avoids the th_is* duplicate symbol.
# Recreate from scratch: `ar r` would UPDATE a stale archive in place and keep
# its old member order, defeating the ppef-first ordering above.
rm -f "$OUTDIR/libpesweb.a"
emar rcs "$OUTDIR/libpesweb.a" "${CORE_OBJS[@]}"
# -sFETCH=1: pesDocument references emscripten_fetch (remote-asset path); the
# Fetch API must be linked even though textures are preloaded into MEMFS.
em++ -O2 --bind -fexceptions -sDISABLE_EXCEPTION_CATCHING=0 \
  -sMODULARIZE=1 -sEXPORT_NAME=createPesModule \
  -sENVIRONMENT=web,node -sALLOW_MEMORY_GROWTH=1 -sEXIT_RUNTIME=0 \
  -sFORCE_FILESYSTEM=1 -sFETCH=1 -sINITIAL_MEMORY=67108864 \
  --preload-file "$RES/texture@/resources/texture" \
  "$WEB_OBJ" "$OUTDIR/libpesweb.a" "$WOUT"/*.wasm.a -o "$OUTDIR/pes_web.js"

rm -f "$OUTDIR/libpesweb.a"

# Fonts are NOT preloaded (PPEF 42MB / 136 files, TTF 209MB / 400+ files) —
# serve them next to the app so webEngine.ts can fetch each .ppef/.ttf on
# demand into MEMFS (load_ppef_font / load_ttf_font). fonts.json is the
# manifest behind list_ppef_fonts / list_ttf_fonts on web.
echo "[5/5] PPEF + TTF fonts -> public/resources (on-demand fetch at runtime)"
sync_fonts() { # $1 = subdir (PPEF|TTF), $2 = extension (ppef|ttf)
  local fontdir="$REPO/public/resources/$1"
  mkdir -p "$fontdir"
  rsync -a --include="*.$2" --exclude='*' "$RES/$1/" "$fontdir/"
  {
    printf '['
    local first=1 f
    for f in "$RES/$1"/*."$2"; do
      [ "$first" -eq 1 ] || printf ','
      printf '"%s"' "$(basename "$f" ".$2")"
      first=0
    done
    printf ']\n'
  } > "$fontdir/fonts.json"
}
sync_fonts PPEF ppef
sync_fonts TTF ttf

echo "OK: web engine -> $OUTDIR/pes_web.{js,wasm,data}"
