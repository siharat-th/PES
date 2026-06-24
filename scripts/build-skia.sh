#!/usr/bin/env bash
# Build PRISTINE upstream Skia (the third_party/skia submodule) for PES.
#
#   build-skia.sh [native|wasm]      (default: native)
#
# Skia's own source is NEVER modified — this only generates a build tree under
# third_party/skia/out/. PES's additions live in src-tauri/cpp/skia-ext and are
# compiled separately (build.rs for native, build-wasm.sh for web); they link
# against the libs produced here. Skia is a standalone dependency in BOTH
# targets — pes is never baked into it.
#
# To bump Skia: `git -C third_party/skia checkout <newer-commit>` then re-run.
set -euo pipefail

TARGET="${1:-native}"
REPO="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
SKIA="$REPO/third_party/skia"
SIB="$REPO/../SkiaApps"   # optional sibling checkout: reuse its gn + externals
cd "$SKIA"

# --- gn toolchain (reuse sibling SkiaApps if present, else fetch) ---
if [ ! -x ./bin/gn ]; then
  if [ -x "$SIB/bin/gn" ]; then
    cp "$SIB/bin/gn" ./bin/gn && chmod +x ./bin/gn
  else
    python3 bin/fetch-gn
  fi
fi

# --- third-party externals (symlink sibling to skip the ~12GB git-sync-deps) ---
if [ ! -e third_party/externals ]; then
  if [ -d "$SIB/third_party/externals" ]; then
    ln -s "$(cd "$SIB/third_party/externals" && pwd)" third_party/externals
  else
    python3 tools/git-sync-deps
  fi
fi

build() {  # $1 = out dir, $2 = lib name suffix (".wasm" for wasm cpu, "" native)
  ./bin/gn gen "$1"
  # M150 split skunicode into _core/_icu; build.rs / build-wasm.sh link every
  # lib*.a produced. For target_cpu="wasm", GN suffixes outputs with ".wasm".
  ninja -C "$1" "libskia$2.a" "libskshaper$2.a" "libskunicode_core$2.a" "libskunicode_icu$2.a"
  echo "OK: pristine Skia ($TARGET) -> $SKIA/$1"
}

case "$TARGET" in
  native)
    case "$(uname -s)" in
      Darwin)
        ARCH=$([ "$(uname -m)" = arm64 ] && echo arm64 || echo x64)
        OUT="out/macos-$ARCH-release" ;;
      *) echo "build-skia.sh native: only macOS wired up so far" >&2; exit 1 ;;
    esac
    mkdir -p "$OUT"
    cat > "$OUT/args.gn" <<EOF
is_debug = false
target_cpu = "$ARCH"
skia_use_metal = true
EOF
    build "$OUT" ""
    ;;

  wasm)
    # emscripten toolchain (emcc/em++ in PATH for gn's wasm toolchain + ninja)
    source "$SKIA/third_party/externals/emsdk/emsdk_env.sh" 2>/dev/null
    OUT="out/wasm-release"
    mkdir -p "$OUT"
    cat > "$OUT/args.gn" <<'EOF'
is_debug = false
is_official_build = true
is_component_build = false
target_cpu = "wasm"

skia_use_angle = false
skia_use_dng_sdk = false
skia_use_dawn = false
skia_use_webgl = true
skia_use_webgpu = false
skia_use_expat = true
skia_use_system_expat = false
skia_use_fontconfig = false
skia_use_freetype = true
skia_use_libheif = false
skia_use_libjpeg_turbo_decode = true
skia_use_libjpeg_turbo_encode = true
skia_use_libpng_decode = true
skia_use_libpng_encode = true
skia_use_libwebp_decode = true
skia_use_libwebp_encode = true
skia_use_lua = false
skia_use_piex = false
skia_use_system_freetype2 = false
skia_use_system_libjpeg_turbo = false
skia_use_system_libpng = false
skia_use_system_libwebp = false
skia_use_system_zlib = false
skia_use_vulkan = false
skia_use_wuffs = true
skia_use_zlib = true
skia_enable_gpu = true

skia_use_icu = true
skia_use_system_icu = false
skia_use_harfbuzz = true
skia_use_system_harfbuzz = false
skia_enable_skshaper = true
skia_use_freetype_woff2 = true

# Headless fonts: engine makes typefaces from data via freetype; no platform
# fontmgr (avoids the android parser and its expat.h include).
skia_enable_fontmgr_android = false
skia_enable_fontmgr_custom_directory = false
skia_enable_fontmgr_custom_embedded = false
skia_enable_fontmgr_custom_empty = true
skia_fontmgr_factory = ":fontmgr_custom_empty_factory"
EOF
    build "$OUT" ".wasm"
    ;;

  *) echo "usage: build-skia.sh [native|wasm]" >&2; exit 1 ;;
esac
