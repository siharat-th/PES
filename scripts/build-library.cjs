// build-library.cjs — generate the design-library payload for the web app.
//
// Walks the three legacy asset trees (SVG / PES / PPES templates), then per
// library: copies the design files, writes a manifest.json (the old
// PES5_Data/{SVG,PES,PPES}.json tree format + per-file `path`/`thumb`), and
// bakes a content-cropped PNG thumbnail per design through the SAME wasm
// engine the web app runs (thumbnail_png = pesDocument::getThumbnailPNGBuffer,
// the renderer behind the old app's shadowdoc.getThumbnailPNG).
//
// Run via scripts/build-library.sh — it copies the emscripten glue OUTSIDE the
// repo first (package.json `type:module` would make require() of the UMD glue
// parse as ESM — see PLAN.md Slice 7 lessons) and pre-bundles
// src/engine/svgGradients.ts to CJS (the engine cannot resolve SVG gradients;
// thumbs must run the same preprocess as documentStore.openBytes).
//
//   node scripts/build-library.cjs <workDir> [--only svg,pes,ppes] [--limit N]
//
// Output -> public/library/<lib>/{manifest.json, files/**, thumbs/**}
// Resumable: existing thumbs are skipped; corrupt inputs are logged and skipped.

"use strict";
const fs = require("node:fs");
const path = require("node:path");

const REPO = path.resolve(__dirname, "..");
const OUT = path.join(REPO, "public", "library");
const THUMB_MAX = 256; // engine cap is 400; 256 keeps retina grids crisp

// Legacy asset trees (reference checkout, NOT a build input — see CLAUDE.md).
const RES = process.env.PES_LIBRARY_SRC || path.resolve(REPO, "..", "Victor-frontend", "PES5", "res");
const LIBS = [
  { key: "svg",  src: path.join(RES, "_svg_pro"),       exts: [".svg"] },
  { key: "pes",  src: path.join(RES, "_pes"),           exts: [".pes"] },
  { key: "ppes", src: path.join(RES, "_pes4_tpl_ppes"), exts: [".ppes", ".ppes5"] },
];

// ---- args -------------------------------------------------------------------
const [workDir, ...rest] = process.argv.slice(2);
if (!workDir) { console.error("usage: node build-library.cjs <workDir> [--only svg,pes] [--limit N]"); process.exit(1); }
const only = rest.includes("--only") ? rest[rest.indexOf("--only") + 1].split(",") : null;
const limit = rest.includes("--limit") ? Number(rest[rest.indexOf("--limit") + 1]) : Infinity;

// ---- tree walk (old PES5_Data manifest shape) --------------------------------
// {type:"directory",name,contents:[...]} / {type:"file",name,path,thumb}
function walk(dir, rel, exts, files) {
  const contents = [];
  for (const e of fs.readdirSync(dir, { withFileTypes: true }).sort((a, b) => a.name.localeCompare(b.name, "th"))) {
    if (e.name.startsWith(".")) continue;
    const abs = path.join(dir, e.name);
    const r = rel ? `${rel}/${e.name}` : e.name;
    if (e.isDirectory()) {
      const sub = walk(abs, r, exts, files);
      if (sub.contents.length) contents.push(sub);
    } else if (exts.includes(path.extname(e.name).toLowerCase())) {
      const thumbRel = r.replace(/\.[^.]+$/, ".png");
      const node = { type: "file", name: e.name, path: `files/${r}`, thumb: `thumbs/${thumbRel}` };
      contents.push(node);
      files.push({ abs, rel: r, thumbRel, node });
    }
  }
  return { type: "directory", name: path.basename(dir), contents };
}

// ---- engine (lazy: only when thumbs are missing) ------------------------------
let enginePromise = null;
function engine() {
  if (!enginePromise) {
    const createPesModule = require(path.join(workDir, "pes_web.js"));
    // node resolves .wasm/.data relative to CWD unless told otherwise
    enginePromise = createPesModule({ locateFile: (p) => path.join(workDir, p) }).then((m) => {
      m.set_resource_path("/resources"); // stitch textures preloaded in pes_web.data
      return m;
    });
  }
  return enginePromise;
}
const { preprocessSvgGradients } = require(path.join(workDir, "svgGradients.cjs"));

async function bake(m, job, kind) {
  let bytes = fs.readFileSync(job.abs);
  if (kind === "svg") // same preprocess as documentStore.openBytes — engine can't resolve gradients
    bytes = Buffer.from(preprocessSvgGradients(bytes.toString("utf8")));
  m.pes_call("new_document", "{}"); // fresh doc + clearHistory (undo stack must not grow across 5k bakes)
  const kindArg = kind === "ppes" ? "ppes" : kind; // .ppes/.ppes5 both load as ppes
  const res = JSON.parse(m.load_input(kindArg, bytes));
  if (res.__error) throw new Error(res.__error);
  const png = m.thumbnail_png(THUMB_MAX, THUMB_MAX);
  if (!png || png.length === 0) throw new Error("empty thumbnail");
  return Buffer.from(png); // copy out of wasm heap before the next bake reuses it
}

// ---- main ---------------------------------------------------------------------
(async () => {
  const failures = [];
  for (const lib of LIBS) {
    if (only && !only.includes(lib.key)) continue;
    if (!fs.existsSync(lib.src)) { console.warn(`skip ${lib.key}: missing ${lib.src}`); continue; }

    const files = [];
    const tree = walk(lib.src, "", lib.exts, files);
    tree.name = lib.key;
    const outDir = path.join(OUT, lib.key);

    // copy design files (only new/updated — the tree is 288MB total)
    let copied = 0;
    for (const f of files.slice(0, limit)) {
      const dst = path.join(outDir, "files", f.rel);
      const st = fs.statSync(f.abs);
      if (!fs.existsSync(dst) || fs.statSync(dst).mtimeMs < st.mtimeMs) {
        fs.mkdirSync(path.dirname(dst), { recursive: true });
        fs.copyFileSync(f.abs, dst);
        copied++;
      }
    }

    // bake missing thumbs (resumable: existing thumbs are the checkpoint)
    const jobs = files.slice(0, limit).filter((f) => !fs.existsSync(path.join(outDir, "thumbs", f.thumbRel)));
    let baked = 0;
    if (jobs.length) {
      const m = await engine();
      for (const f of jobs) {
        try {
          const png = await bake(m, f, lib.key);
          const dst = path.join(outDir, "thumbs", f.thumbRel);
          fs.mkdirSync(path.dirname(dst), { recursive: true });
          fs.writeFileSync(dst, png);
          baked++;
        } catch (err) {
          failures.push(`${lib.key}/${f.rel}: ${err.message}`);
          f.node.thumb = null; // manifest marks it — UI shows a placeholder
        }
        if ((baked + failures.length) % 50 === 0)
          console.log(`  [${lib.key}] ${baked + failures.length}/${jobs.length} baked...`);
      }
    }

    // manifest last, so a killed run re-bakes before re-publishing
    if (limit !== Infinity) tree.contents = pruneToLimit(tree.contents, files.slice(0, limit));
    fs.mkdirSync(outDir, { recursive: true });
    fs.writeFileSync(path.join(outDir, "manifest.json"),
      JSON.stringify({ generated: new Date().toISOString(), count: Math.min(files.length, limit), tree }));
    console.log(`${lib.key}: ${Math.min(files.length, limit)} files (${copied} copied, ${baked} thumbs baked, ${failures.length} failures total)`);
  }
  if (failures.length) {
    const logPath = path.join(OUT, "bake-failures.log");
    fs.writeFileSync(logPath, failures.join("\n") + "\n");
    console.warn(`${failures.length} bake failures -> ${logPath}`);
  }
  process.exit(0); // emscripten runtime keeps the event loop alive
})().catch((err) => { console.error(err); process.exit(1); });

// keep only file nodes in the limited set (subset test runs)
function pruneToLimit(contents, kept) {
  const keep = new Set(kept.map((f) => f.node));
  return contents
    .map((n) => (n.type === "directory" ? { ...n, contents: pruneToLimit(n.contents, kept) } : n))
    .filter((n) => (n.type === "directory" ? n.contents.length : keep.has(n)));
}
