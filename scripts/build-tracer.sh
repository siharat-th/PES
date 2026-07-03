#!/usr/bin/env bash
# Build the Auto Punch tracer (tracer/ crate: quantette + visioncortex) to wasm.
#
# Output -> public/tracer/pes_tracer.js + pes_tracer_bg.wasm  (served verbatim
# by vite at /tracer, loaded by public/tracer/tracer-worker.js via importScripts)
#
#   scripts/build-tracer.sh
#
# --target no-modules: classic-script output (global `wasm_bindgen`), the same
# public/-asset pattern as the engine wasm — works in classic Web Workers.
set -euo pipefail

REPO="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
OUTDIR="$REPO/public/tracer"

command -v wasm-pack >/dev/null 2>&1 || { echo "wasm-pack missing (brew install wasm-pack)" >&2; exit 1; }

wasm-pack build "$REPO/tracer" --release --target no-modules \
  --out-dir "$OUTDIR" --out-name pes_tracer --no-pack

# wasm-pack drops a .gitignore that would hide the output from vite deploys
rm -f "$OUTDIR/.gitignore"

ls -lh "$OUTDIR"/pes_tracer_bg.wasm
