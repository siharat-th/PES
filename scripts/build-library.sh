#!/usr/bin/env bash
# Build the design library (คลัง SVG / PES / PPES) for the web app.
#
# Orchestrates scripts/build-library.cjs: copies the emscripten glue to a work
# dir OUTSIDE the repo (package.json `type:module` would make require() of the
# UMD glue parse as ESM — PLAN.md Slice 7 lesson), bundles svgGradients.ts to
# CJS for the bake harness, then walks the legacy asset trees, writes
# manifest.json per library, copies the design files, and bakes thumbnails
# through the wasm engine (thumbnail_png).
#
#   scripts/build-library.sh                    # all three libraries
#   scripts/build-library.sh --only svg         # subset by library
#   scripts/build-library.sh --limit 20         # subset by count (smoke test)
#
# Source trees: ../Victor-frontend/PES5/res/{_svg_pro,_pes,_pes4_tpl_ppes}
# (override with PES_LIBRARY_SRC). Output -> public/library/ (gitignored; dev
# serves it at /library, prod uploads it to the CDN behind VITE_LIBRARY_BASE).
# Resumable — existing thumbs are skipped, so rerun after a crash or to update.
set -euo pipefail

REPO="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
GLUE="$REPO/public/wasm"

ls "$GLUE"/pes_web.js >/dev/null 2>&1 || { echo "web engine missing; run: scripts/build-web.sh" >&2; exit 1; }
# embind names live in the wasm binary, not the JS glue
grep -aq thumbnail_png "$GLUE/pes_web.wasm" || { echo "pes_web.wasm predates thumbnail_png; rerun: scripts/build-web.sh" >&2; exit 1; }

WORK="$(mktemp -d "${TMPDIR:-/tmp}/pes-library.XXXXXX")"
trap 'rm -rf "$WORK"' EXIT

echo "[1/3] engine glue -> $WORK (outside repo, so require() stays CJS)"
cp "$GLUE"/pes_web.{js,wasm,data} "$WORK/"

echo "[2/3] bundle svgGradients.ts -> CJS (same gradient preprocess as the app)"
"$REPO/node_modules/.bin/esbuild" "$REPO/src/engine/svgGradients.ts" \
  --bundle --format=cjs --platform=node --outfile="$WORK/svgGradients.cjs" --log-level=warning

echo "[3/3] manifest + files + thumbnails -> public/library/"
node "$REPO/scripts/build-library.cjs" "$WORK" "$@"
