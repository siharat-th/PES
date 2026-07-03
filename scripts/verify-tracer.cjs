#!/usr/bin/env node
// Headless smoke test for the Auto Punch tracer wasm (public/tracer/*).
// Drives the wasm-pack `no-modules` glue in node (same idea as
// verify-wasm/build-library.cjs for the engine): synthesize a 2-color 32x32
// RGBA image, trace it, assert the JSON shape.
//
//   node scripts/verify-tracer.cjs
const fs = require("fs");
const path = require("path");

const dir = path.resolve(__dirname, "..", "public", "tracer");
const glue = fs.readFileSync(path.join(dir, "pes_tracer.js"), "utf8");
// no-modules glue declares a module-local `let wasm_bindgen`; evaluate and grab it
const wasm_bindgen = new Function(`${glue}; return wasm_bindgen;`)();

async function main() {
  await wasm_bindgen({
    module_or_path: fs.readFileSync(path.join(dir, "pes_tracer_bg.wasm")),
  });

  const [w, h] = [32, 32];
  const rgba = new Uint8Array(w * h * 4);
  for (let y = 0; y < h; y++)
    for (let x = 0; x < w; x++) {
      const i = (y * w + x) * 4;
      const left = x < w / 2;
      rgba[i] = left ? 200 : 30;
      rgba[i + 1] = 30;
      rgba[i + 2] = left ? 30 : 200;
      rgba[i + 3] = 255;
    }

  const out = JSON.parse(
    wasm_bindgen.trace(rgba, w, h, JSON.stringify({ maxColors: 2, despeckleAreaPx: 4 }))
  );
  if (out.error) throw new Error(`trace error: ${out.error}`);
  if (out.width !== w || out.height !== h) throw new Error(`bad dims: ${out.width}x${out.height}`);
  if (!Array.isArray(out.colors) || out.colors.length !== 2)
    throw new Error(`expected 2 colors, got ${JSON.stringify(out.colors?.map((c) => c.rgb))}`);
  for (const c of out.colors) {
    if (!/^#[0-9a-f]{6}$/.test(c.rgb)) throw new Error(`bad rgb: ${c.rgb}`);
    if (!c.paths.length || !c.paths[0].startsWith("M"))
      throw new Error(`bad paths for ${c.rgb}: ${JSON.stringify(c.paths).slice(0, 120)}`);
    if (!(c.areaPx > 300)) throw new Error(`area too small for ${c.rgb}: ${c.areaPx}`);
  }
  console.log(
    "tracer OK:",
    out.colors.map((c) => `${c.rgb} area=${c.areaPx} paths=${c.paths.length}`).join(", ")
  );
}

main().catch((e) => {
  console.error("verify-tracer FAILED:", e.message);
  process.exit(1);
});
