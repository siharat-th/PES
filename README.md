# pes-rs

Editor for **Brother PES** embroidery designs — a port of a production
embroidery design tool that keeps the proven C++ embroidery engine and rebuilds
the whole canvas/UI on **React + Konva**. It ships two targets from one codebase:

- **Web (WebAssembly)** — the primary target. The C++ engine + Skia compiled to
  wasm, running in the browser, no install.
- **Desktop (Tauri 2, macOS + Windows)** — the same engine via native FFI.

> Status: active port. See [`PLAN.md`](PLAN.md) for the phase-by-phase plan and
> [`CLAUDE.md`](CLAUDE.md) for the architecture deep-dive and invariants — read
> them before large changes.

## Architecture

Three layers over a single C++ engine singleton (the engine is the source of
truth; every mutation returns a fresh `DocumentSnapshot`):

1. **React frontend (`src/`)** — never touches the engine directly.
   `engine/transport.ts` routes each `invoke(cmd, args)` to **Tauri** (desktop)
   or the **wasm engine** (browser) by sniffing `__TAURI_INTERNALS__`, so the
   same client code runs in both targets. `state/documentStore.ts` (zustand)
   holds the snapshot; `canvas/` is the Konva stage (world coords = engine
   units, 0.1 mm; transforms commit on release).
2. **Rust (`src-tauri/src/`)** — thin async command wrappers over the `cxx`
   bridge (`engine.rs`), undo/redo as whole-document PPES snapshots
   (`history.rs`). Desktop only.
3. **C++ facade + engine (`src-tauri/cpp/`)** — `pes_ffi.cpp` (native) and
   `cpp/wasm/pes_web.cpp` (embind) are two thin bindings over the same engine
   in `cpp/pes/`, sharing divergence-prone logic through header-only cores
   (`pes_*_core.hpp`) so desktop and web stay byte-identical.

The PPES project format is **v504** (adds layer groups); v504 files stay
readable by the old app (unknown keys ignored).

---

## Web version — build & run from scratch

Everything runs from the repo root. First build is slow (Skia); after that only
the layer you touch is rebuilt.

### Prerequisites

- **Node.js** 20+ (developed on 22) and **npm**
- **Rust** stable, plus the wasm target and **wasm-pack** (for the Auto Punch
  tracer):
  ```bash
  rustup target add wasm32-unknown-unknown
  cargo install wasm-pack          # or: brew install wasm-pack
  ```
- **Emscripten** (`em++`) — provided by Skia's externals tree, not installed
  separately. `build-web.sh` sources it from
  `third_party/skia/third_party/externals/emsdk`. If it can't find `em++`, run
  that emsdk's `./emsdk install latest && ./emsdk activate latest` once.
- **~8–12 GB free disk** for Skia's `third_party/externals`.

### 1. Fetch Skia (pristine upstream submodule)

```bash
git submodule update --init third_party/skia
```

`build-skia.sh` needs `gn` + `third_party/externals`. If a sibling `../SkiaApps`
checkout is present it reuses them (symlinks externals — instant); otherwise it
fetches `gn` and runs Skia's `git-sync-deps` (large download).

### 2. Build Skia for wasm (once)

```bash
scripts/build-skia.sh wasm
# -> third_party/skia/out/wasm-release/lib*.wasm.a
```

### 3. Build the engine wasm module

```bash
scripts/build-web.sh
# -> public/wasm/pes_web.{js,wasm,data}     (engine + Skia, embind)
# also syncs the fonts -> public/resources/{PPEF,TTF}/ (+ fonts.json),
# fetched on demand at runtime
```

### 4. Build the Auto Punch tracer wasm

```bash
scripts/build-tracer.sh
# -> public/tracer/pes_tracer.js + pes_tracer_bg.wasm   (vtracer + quantette)
# (the worker public/tracer/tracer-worker.js is committed source, not generated)
```

### 5. Install deps and run

```bash
npm install
npm run dev            # http://localhost:1420
```

Open the URL. To try **Auto Punch** (image → embroidery): bottom-right **+** →
“สร้าง” → “แกะลายจากรูป (Auto Punch)”, then drop a PNG/JPG. Start with the flat
-colour samples in [`demo/auto-punch/`](demo/auto-punch/) — auto-tracing shines
on logos/flat art; full photos are the hardest case.

### Production build

```bash
npm run build          # tsc + vite build -> dist/
npm run preview        # serve the built dist/ locally
```

`public/wasm/`, `public/tracer/pes_tracer*`, and `public/resources/` are
generated and git-ignored — run steps 2–4 before `vite build`. The resulting
`dist/` is a static site you can host anywhere (serve the whole folder;
`.wasm`/`.data` must keep their MIME types).

### Rebuilding after edits

| You changed… | Re-run |
|---|---|
| `src/**` (React/TS) | nothing — Vite hot-reloads |
| `cpp/pes/**`, `cpp/wasm/pes_web.cpp`, `cpp/pes_*_core.hpp` | `scripts/build-web.sh` |
| `tracer/**` (Rust tracer) | `scripts/build-tracer.sh` |
| Skia bump (`git -C third_party/skia checkout …`) | `scripts/build-skia.sh wasm`, then the two above |

### Verify without a browser (headless)

```bash
cd tracer && cargo test          # Auto Punch tracer unit tests (native, fast)
node scripts/verify-tracer.cjs   # drive the tracer wasm in Node
npx tsc --noEmit                 # type-check the frontend
```

---

## Desktop version (Tauri)

Same engine, native FFI instead of wasm.

### Prerequisites

Node + Rust as above, plus the platform C/C++ toolchain (Xcode CLT on macOS,
MSVC on Windows). No emscripten or wasm-pack needed.

### Build & run

```bash
git submodule update --init third_party/skia
scripts/build-skia.sh native     # -> third_party/skia/out/<platform>-<arch>-release/lib*.a
npm install

npm run tauri dev                # run the app (Vite + cargo, auto-rebuild)
npm run tauri build              # release bundle

cd src-tauri && cargo build              # rebuild the Rust/C++ bridge only
cd src-tauri && cargo test --lib engine  # engine FFI tests (round-trip + PPEF + Auto Punch)
```

`build.rs` compiles the engine, the skia-ext files, and the vendored helpers
from source and links every `lib*.a` under the Skia out dir — so format/engine
edits recompile with `cargo build` (no GN rebuild). **Pristine Skia must be
built once (`build-skia.sh native`) before `cargo build` works.**

---

## Resources

`src-tauri/resources/` holds assets the engine loads at startup — stitch
textures in `texture/` and PPEF fonts in `PPEF/`. **The engine crashes in
`loadAssets()` if the textures are missing.** For web, `build-web.sh` preloads
the textures into the wasm module and serves fonts from `public/resources/`
(fetched on demand).

## Notes / gotchas

- **ABI defines must match the Skia GN build.** `build.rs` and `build-web.sh`
  mirror `gn desc <out> //:skia defines` (minus `SKIA_IMPLEMENTATION`). A
  mismatch changes how `sk_sp` crosses the FFI boundary and crashes with a
  misleading `EXC_BAD_ACCESS`. Re-sync both after any Skia bump.
- **Never edit `third_party/skia`.** It is pristine upstream (M150). PES's
  additions live in `src-tauri/cpp/skia-ext/` and link against it.
- Keep all `pesBuffer`/`sk_sp` lifetimes inside the C++ bindings; only plain
  data (cxx structs, JSON strings, byte vectors) crosses to Rust.

## Tech stack

Web: WebAssembly (emscripten/embind) · React 19 · react-konva / Konva · zustand
· Vite · Tailwind CSS · Rust→wasm tracer (visioncortex/vtracer + quantette).
Desktop: Tauri 2 · Rust · cxx. Both: Skia (M150) · a C++ embroidery engine.
