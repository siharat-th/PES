# pes-rs

Cross-platform desktop editor for **Brother PES** embroidery designs — a Tauri 2
port (macOS + Windows) of a production embroidery design tool. It keeps the
proven C++ embroidery engine via **native FFI (no WASM)** and rebuilds the whole
canvas/UI on **React + Konva**, replacing the old Cordova/Electron app whose
engine ran as `canvaskit.wasm`.

> Status: active port. See [`PLAN.md`](PLAN.md) for the phase-by-phase plan,
> current status, and lessons learned — read it before large changes.

## Architecture

Three layers over a single C++ engine singleton:

1. **React frontend (`src/`)** — never touches FFI directly.
   - `engine/EngineClient.ts` — the only place Tauri commands are invoked.
   - `state/documentStore.ts` (zustand) — holds a plain-data `DocumentSnapshot`;
     the C++ engine is the source of truth (every mutation returns a fresh
     snapshot).
   - `canvas/` — Konva stage (world coords = engine units, 0.1 mm). Transforms
     preview locally and **commit on release**.
   - `panels/` — properties / layers, type-switched per object.
2. **Rust (`src-tauri/src/`)** — `commands.rs` (thin async wrappers),
   `engine.rs` (the `cxx` bridge; all access via `with_engine()` mutex),
   `history.rs` (undo/redo as whole-document PPES snapshots).
3. **C++ facade + engine (`src-tauri/cpp/`)** — `pes_ffi.cpp` is a headless API
   over the engine. The engine itself lives in `cpp/pes/{src,include}` and is
   **compiled from source** by `build.rs`, so the PPES format/engine can be
   edited here and `cargo build` recompiles them.

The current PPES project format is **v504** (adds layer groups); v504 files stay
readable by the old app (unknown keys ignored).

## Prerequisites

- **Node.js** 20+ (developed on 22) and **npm**
- **Rust** stable (developed on 1.95) + the platform C/C++ toolchain
  (Xcode CLT on macOS; MSVC on Windows)
- Two **sibling checkouts** next to this repo (reference + build inputs):

  ```
  <parent>/
  ├── pes-rs/            ← this repo
  ├── SkiaApps/          ← Skia fork: engine source + prebuilt Skia/3rd-party libs
  └── Victor-frontend/   ← old Cordova app (PES5) — behavior/asset reference
  ```

- **Prebuilt Skia + third-party static libraries.** These are large
  (~900 MB, `libskia.a` alone is ~470 MB) and are **not committed** to any repo —
  they are build inputs produced in the SkiaApps tree. `build.rs` locates them
  via `SKIAAPPS_DIR` (default `../../SkiaApps` from `src-tauri/`) under
  `out/<platform>-<arch>-release/`. The build gate is `libskia.a`.

  > A fresh clone of `pes-rs` alone will **not** build until SkiaApps is present
  > and these libs are built.

### Build the Skia + third-party libs (once)

```bash
cd ../SkiaApps
# macOS (arm64)
bin/gn gen out/macos-arm64-release \
  --args='is_debug=false target_cpu="arm64" skia_use_metal=true'
ninja -C out/macos-arm64-release skia third_party/sqlitecpp:sqlitecpp
```

The pes engine itself is compiled from `src-tauri/cpp/pes/` in this repo, so
`libpes.a` is **not** linked — only Skia + third-party archives are.

## Run & build

```bash
npm install                              # frontend deps (first time)

npm run tauri dev                        # run the app (Vite + cargo, auto-rebuild)
npm run tauri build                      # release bundle

npx tsc --noEmit                         # type-check the frontend
cd src-tauri && cargo build              # rebuild the Rust/C++ bridge only
cd src-tauri && cargo test --lib engine  # engine FFI tests (round-trip + PPEF)
```

If SkiaApps lives elsewhere, point the build at it:

```bash
SKIAAPPS_DIR=/abs/path/to/SkiaApps npm run tauri dev
```

## Resources

`src-tauri/resources/` holds assets the engine loads at startup (stitch textures
in `texture/`, 136 PPEF fonts in `PPEF/`). **The engine crashes in `loadAssets()`
if the textures are missing.**

## Notes / gotchas

- **ABI defines must match the GN build of Skia** (e.g. `SK_TRIVIAL_ABI`,
  `NDEBUG`); a mismatch crashes with a misleading `EXC_BAD_ACCESS`. `build.rs`
  mirrors `gn desc <out> //modules/pes:pes defines`.
- Keep all `pesBuffer`/`sk_sp` lifetimes inside `pes_ffi.cpp`; only plain data
  (cxx structs, JSON strings, byte vectors) crosses to Rust.
- The SkiaApps tree carries local patches (`libpng/pngpriv.h`, `zlib/zutil.h`)
  for newer macOS SDKs — re-syncing its deps reverts them and breaks the build.

## Tech stack

Tauri 2 · Rust · cxx · React 19 · react-konva / Konva · zustand · Vite ·
Tailwind CSS · Skia (engine rendering) · C++ embroidery engine.
