# PES — Port แอปออกแบบลายปัก → Tauri 2 + React (C++ native FFI, ไม่มี WASM)

แผนฉบับเต็ม: `~/.claude/plans/recursive-yawning-lightning.md` (อนุมัติ 2026-06-12)

## สรุปสถาปัตยกรรม

```
Tauri 2 (macOS + Windows)
├─ WebView: React 18 + TS + Vite + Tailwind + zustand + react-konva
│   ├─ Canvas: Konva (objects/gizmos/PathEdit/StitchEdit/tools)
│   └─ engine/EngineClient.ts — เรียก Tauri commands
└─ Rust backend
    ├─ src/engine.rs: cxx FFI → pes engine (compile จาก source ใน cpp/pes/) + libskia.a (Skia ยัง prebuilt จาก SkiaApps ด้วย GN)
    ├─ cpp/pes/{src,include}: pesEngine ก๊อปเข้ารีโป — แก้ format/engine แล้ว cargo build recompile ได้เลย (ไม่ link libpes.a แล้ว)
    ├─ cpp/pes_ffi.{h,cpp}: facade headless ห่อ pesDocument/pesData
    ├─ cpp/pes_resources.cpp: shim แทน tools/Resources ของ Skia
    └─ Phase ถัดไป: undo/redo (Rust), depth map (ort), spot UV (image+tiff)
```

- Engine = pesEngine C++ เดิม (`SkiaApps/modules/pes`) link ตรงเข้า Rust — ตัด WASM/EM_ASM/ชั้น view ทิ้ง
- Phase ท้าย: แทน C++ ด้วย Rust ทีละส่วน (skia-safe + clipper2) ด้วย golden tests

## สถานะ

### ✅ Phase 0 — Build foundation (เสร็จ 2026-06-12)
- Build `libskia.a` + `libpes.a` + `libsqlitecpp.a` (release, arm64) ที่ `SkiaApps/out/macos-arm64-release`
  - `bin/fetch-gn && bin/gn gen out/macos-arm64-release --args='is_debug=false target_cpu="arm64" skia_use_metal=true'`
  - `ninja -C out/macos-arm64-release skia modules/pes:pes third_party/sqlitecpp:sqlitecpp`
- cxx bridge + facade ขั้นต่ำ: newDocument / loadPPES / importPES / importSVG / objectCount / exportAs / thumbnailPNG
- Round-trip test ผ่าน: `cargo test --lib engine` (import burger_king.pes → export #PES → PNG)

**บทเรียนสำคัญที่ค้นพบ (ห้ามลืม):**
1. **ABI defines ต้องตรงกับ GN build** — โดยเฉพาะ `SK_TRIVIAL_ABI=[[clang::trivial_abi]]` + `NDEBUG`: ถ้าไม่ตรง `sk_sp` ถูกส่งคนละ calling convention → crash (EXC_BAD_ACCESS, pc misaligned) ดูรายการเต็มใน `src-tauri/build.rs` (มาจาก `gn desc <out> //modules/pes:pes defines`)
2. **libpes ต้องการ `GetResourceAsData`** (จาก Skia tools) — implement เองใน `cpp/pes_resources.cpp` + ต้อง bundle `resources/texture/lineStitch-*.png` ไม่งั้น engine crash ตอน `loadAssets()`
3. SDK macOS ใหม่ (Xcode 26.5) ชน vendored libs เก่า — patch แล้วใน SkiaApps tree: `third_party/externals/libpng/pngpriv.h` และ `third_party/externals/zlib/zutil.h` (เงื่อนไข `TARGET_OS_MAC` ยุคคลาสสิก) — **ถ้า sync deps ใหม่ patch จะหาย**
4. SkiaApps path กำหนดผ่าน env `SKIAAPPS_DIR` (default: `../../SkiaApps` จาก src-tauri)

### ✅ Phase 1 milestone 2-3 (บางส่วน) — แอปรันได้จริง (2026-06-12)
- Rust API: snapshots (bbox/rotate/type/visible/locked), object PNG, transforms (commit-on-release + center compensation), delete/duplicate/visible/locked/reorder, export 7 formats
- Konva canvas: hoop + grid 5mm + แกน, zoom ที่ cursor, pan (กลาง/space), fit-to-hoop
- ObjectsLayer (ภาพ stitch ต่อ object + cache ตาม imageVersion) + Transformer (drag/scale/rotate) + Delete/Cmd-D
- LayerPanel: thumbnail, type/text, ตา/กุญแจ, ลำดับขึ้นลง
- Toolbar: New/Open/Export/Duplicate/Delete + error banner; เปิดผ่าน dialog (.ppes/.pes/.svg)

### ✅ เพิ่มเติม (2026-06-12/13)
- Properties panel แบบ type-switched ครบ (PPEF/TTF/SVG/Stitch/Satin) + undo/redo + drag&drop + multi-select (บทเรียน: Transformer ยิง dragend ทุก node — commit ครั้งเดียวต่อ gesture)
- **PPEF text editing แบบ native** (PesPPEFUtils + SQLiteCpp + pesEffect 16 แบบ, 136 ฟอนต์) + regression tests (idempotence/fontsize/border)
- **TTF text editing แบบ native** (SkTypeface::MakeFromData + SkTextUtils::GetPath, 426 ฟอนต์, ต้อง link skshaper+skunicode) + test
- Path operations (inset/outset/simplify/unite/separate/erase under)

### ✅ Slice 3 — PathEdit (เสร็จ 2026-06-20)
- โหมดแก้ node ของ vector path: ลาก anchor + bezier handle, double-click แทรก node, Delete ลบ node (live drag preview, regen stitch ตอนปล่อย)

### ✅ Slice 4 — StitchEdit (เสร็จ 2026-06-20)
- โหมดแก้จุดเข็มดิบราย object: อ่าน/ย้าย/แทรก/ลบจุดใน `fillBlocks`(kind 0)/`strokeBlocks`(kind 1)
- FFI: `get_stitch_points` / `move_stitch_point` / `insert_stitch_point` / `delete_stitch_point` (พิกัด world, fold display-rotation รอบ bbox center เหมือน PathEdit) — port จาก `PesStitchEdit` + `PES5_StitchEdit*` (PesSatinColumn.cpp / PES5Command.cpp)
- บล็อก = ตัวข้อมูลจริง → **ไม่ regen** แค่ mutate + `recalculate()`; insert คงค่า `types[]` ให้ขนาน (จุดใหม่ = NORMAL_STITCH)
- UI: `StitchEditLayer` (Shape เดียววาดเส้น+จุดทั้งหมด, Rect โปร่งใสรับคลิกเลือกจุดใกล้สุด, handle ลากได้, hover ring); toolbar "Edit Stitches"; แก้ undoable 1 step/gesture
- Test: `stitch_point_edit_move_insert_delete`

### ✅ Slice 5 — Layer groups + engine compiled from source (เสร็จ 2026-06-20)
- **เลิก link `libpes.a`**: ก๊อป `modules/pes/{src,include}` → `src-tauri/cpp/pes/` แล้ว `build.rs` compile 27 ไฟล์เอง(เหลือ link เฉพาะ `libskia.a` + third-party). แก้ format C++ ในรีโปนี้แล้ว `cargo build` recompile ได้เลย. include ชี้ `cpp/pes/include` ตัวเดียว (ทุก TU ใช้ layout `Parameter`/`pesData` ชุดเดียว ไม่งั้น ABI พัง). แก้ 2 include แบบ `modules/pes/include/...` → bare.
- **PPES bump 503 → 504**: เพิ่ม `pesData::Parameter::groupId` + `pesGroup` registry (`__pesGroups`/`__nextGroupId`) ใน `pesDocument`. เขียน `numGroups`/`Group.*` ใน **header** (ก่อน object blocks ไม่ใช่ก่อน `[PINNDATA]` — กัน '#' ในชื่อกลุ่มไปชนตัว scan magic byte ของ binary loader) + `groupId=` ต่อ object. key ใหม่ทั้งหมดเป็น unknown ของ reader เก่า → เปิดไฟล์ 504 ในแอปเดิมได้ (กลุ่มถูกมองข้าม)
- group เป็น metadata ล้วน (ไม่ใช่ pesData object) — **ไม่ใช้** `OBJECT_TYPE_SCALABLE_CONTAINER`. `group.scalable` เป็น derived (AND ของ member `isScalable()`) ไม่เก็บลงไฟล์; กลุ่มที่มี Stitch จะ scale ไม่ได้ (Transformer ปิด anchor ผ่าน `selectionScalable` เดิม)
- FFI/commands: `create_group`/`rename_group`/`ungroup`/`add_to_group`/`remove_from_group`/`set_group_visible|locked` (undoable, cascade ไปลูก) + `set_group_collapsed` (ไม่ undoable แต่ persist). `DocumentSnapshot.groups` + `ObjectSnapshot.group_id`
- UI: `LayerPanel` เป็น tree (header ย่อ/ขยาย, rename inline, เลือกกลุ่ม=เลือกทุก member, ปุ่มซ่อน/ล็อกทั้งกลุ่ม, hint กลุ่มสเกลไม่ได้, ปุ่ม "กลุ่มใหม่"), drag: ข้ามขอบกลุ่ม=ย้าย membership / ในกลุ่มเดิม=reorder
- Test: `group_roundtrip_survives_ppes` (กลุ่มเปล่า+มีสมาชิก, collapsed, membership, nextGroupId monotonic, scalable derive)

### ✅ Slice 6 — PPEF/TTF Text creation + web text (เสร็จ 2026-07-02)
- **แยก logic PPEF text เป็น `cpp/pes_text_core.hpp`** (`pescore::rebuildPpefText`/`makePpefTextObject`/`makePpefEffect`) ใช้ร่วม native+web เหมือน `pes_edit_core.hpp`; `pes_ffi.cpp::update_ppef_text` เป็น wrapper บางๆ
- **`add_ppef_text(text, fontName)`** — สร้าง object ใหม่กลางสะดึง (port `PES5_AddPPEFText`: satin column, Deep Gold, default "ภิญญ์จักรปัก"/Thai001) ครบทั้ง native (commands.rs, undoable) และ web (`pes_web.cpp`); ปุ่ม "ข้อความปัก (PPEF Text)" ใน RadialToolMenu ใช้งานได้แล้ว
- **PPEF บน web แบบ on-demand**: ฟอนต์ไม่ preload (42MB/136 ไฟล์) — `build-web.sh` sync ไป `public/resources/PPEF/` + เขียน `fonts.json` (ใช้เป็น `list_ppef_fonts`); engine ตอบ `{"missing_font": name}` → `webEngine.ts` fetch → `load_ppef_font(name, bytes)` เขียนลง MEMFS → retry คำสั่งเดิม. `set_parameter` บน web regenerate PPEF/TTF ผ่าน core เดียวกับ native แล้ว
- **บทเรียน**: (1) ฟอนต์หาย → SQLiteCpp โยน exception ข้าม FFI = SIGSEGV — ต้องเช็คไฟล์+try/catch ใน core (บั๊กแฝงเดิมของ update_ppef_text ด้วย); (2) `emar r` อัปเดต archive เก่าโดยคงลำดับ member เดิม — ต้อง `rm -f` ก่อน; (3) ลำดับ member ใน archive สำคัญ: `ppef_PesUnicodeUtils.o` ต้องมาก่อน `UnicodeHelper.o` ไม่งั้น wasm-ld ดึงทั้งคู่แล้ว `th_is*` ซ้ำ; (4) web `setParamStr` ต้องมี key `"font"` ให้ตรง native
- Test: `add_ppef_text_creates_centered_object` (สร้าง+กลางสะดึง+rebuild ไม่ drift+ฟอนต์หายไม่ crash); verify จริงบน browser (headless chromium): สร้าง, แก้ text, สลับฟอนต์ Thai004 (fetch on demand) ผ่านหมด
- **TTF Text ครบทั้งสร้าง+re-shape ทุก target**: ย้าย `update_ttf_text` ไป `pescore::rebuildTtfText` + เพิ่ม `makeTtfTextObject`/`add_ttf_text` (port `PES5_AddTTFText`: outline เดียว fill Deep Gold + stroke Dark Grey, default "ภิญญ์จักรปัก"/JS-Boaboon, เริ่มแบบ vector-only ไม่มี stitch เหมือน shape) — font manager ต่อ platform อยู่ใน core (CoreText/DirectWrite/`SkFontMgr_New_Custom_Empty` FreeType บน wasm ซึ่ง build ด้วย `SK_FONTMGR_FREETYPE_EMPTY_AVAILABLE` อยู่แล้ว)
- **TTF บน web on-demand เหมือน PPEF**: `.ttf` 209MB/421 ไฟล์ sync ไป `public/resources/TTF/` + `fonts.json` (เป็น `list_ttf_fonts` บน web แล้ว — Properties panel มี dropdown ฟอนต์); missing-font handshake เพิ่ม `font_kind: "ppef"|"ttf"` → `webEngine.ts` เลือก loader (`load_ttf_font` เขียน MEMFS); ปุ่ม "ข้อความ TTF (TTF Text)" ใน RadialToolMenu ใช้งานได้แล้ว
- Test: `add_ttf_text_creates_centered_object` + node smoke ของ wasm (handshake→load→สร้างกลางสะดึง→fontSize x2 สูงขึ้น x2.00→แก้ text กว้างขึ้น→ฟอนต์หาย error สะอาด) ผ่านหมด

### ✅ Slice 7 — Smart Satin (TTF/SVG → Satin Column → Fill) จังหวะที่ 1: vendor JS เดิม (เสร็จ 2026-07-02)
- **ยุทธศาสตร์สองจังหวะ** (แทนแผนเดิม "port เป็น crate Rust" — Rust ไม่อยู่ใน build ฝั่ง web เลยตกไป): จังหวะ 1 = รัน geometry JS เดิมของ production แบบ vendored (ถูกต้องโดยนิยาม, ship เร็ว), จังหวะ 2 (อนาคต, เมื่อคุ้ม) = port ลง C++ `pes_satin_core.hpp` หลัง command เดิม โดยใช้ผลจังหวะ 1 เป็น golden test
- **Vendored `public/satin/`**: `satin-core.js` (= `api-satin-helper.js` บรรทัด 1-7733 ของแอปเก่า — multipolygon → straight skeleton → centerline → rails — **ห้ามแก้**, ต้อง byte-identical กับ production) + `d3.v7.min.js` + `straight-skeleton-v2/` (wasm อีกตัว, โหลด lazy ตอนกดแปลงครั้งแรก) ท้าย satin-core มี overrides: `USE_WORKER=false` + export handles ผ่าน `globalThis.__pesSatinCore` (top-level const ของ classic script ไม่โผล่บน window)
- **Engine seams ใหม่ใน `cpp/pes_satin_core.hpp`** (แชร์ native+web, แทนรอยต่อ CanvasKit เดิม 1:1): `get_satin_source(index)` (clone→nudge 1.002 แกนเดียว→pathops simplify→`getOutline()` flatten — engine ตัวเดียวกับที่ binding เก่าเรียก), `simplify_polygons` (แทน trick MakeFromSVGString+simplify+toCanvas), `add_satin_objects` (คู่ราง SVG d-string → `SkParsePath` → pesData `SCALABLE_SATINCOLUMN` + `applyFill()`, undoable ก้อนเดียว) — **satin object = paths เป็นคู่รางเรียงลำดับ** (applyFill จับคู่ paths[0]+[1], [2]+[3], ... แล้ว zigzag ระหว่างราง)
- **Driver TS `src/satin/smartSatin.ts`**: port ของ `apiWorkerConvertLayerToSatinColumn` + พารามิเตอร์จาก `autoSmartSatin` (density 2.5, pullCompensate clamp, quirk เดิม: nlayers>1 → rotate=0); ปุ่ม "แปลงเป็นซาติน (Smart Satin)" ในเมนูแก้ไข เปิดเมื่อเลือก TTF Text/SVG; object ต้นฉบับคงอยู่ (ไม่มี stitch เหมือนแอปเก่า)
- Test: `smart_satin_seams_roundtrip` (rails→object มี stitch + recenter, TTF→source polygons, bowtie simplify) + node e2e smoke รัน pipeline เต็ม ("ภิญ" → 3 polygons → 10 คู่ราง → Satin Column มี stitch, bbox ตรงต้นฉบับ) + render PNG ยืนยันด้วยตา ("ภิญญ์จักรปัก" เป็นลายปักซาตินสวยตรงกับ vector)
- **บทเรียน**: (1) `type:module` ใน package.json ทำให้ require() UMD/emscripten glue ใน repo กลายเป็น ESM — smoke ต้อง copy ออกไปนอก repo; (2) shim `window`/`self` ก่อนโหลด pes_web.js ทำให้ emscripten detect เป็น web แล้วพังใน node — ต้องโหลด engine ก่อนค่อย shim; (3) จุดตัด vendor สำคัญ: CanvasKit ทั้ง 17 จุดกระจุกท้ายไฟล์ (บรรทัด 7744+) geometry ล้วนสะอาด

### ▶ Milestone ถัดไป (Phase 1 ต่อ)
1. ตรวจ fidelity transform กับแอปเดิม (scale semantics ของ pesData.scale ที่ไม่ scalable, rotate + stitch regen)
2. Undo/Redo (Rust command stack ตาม PESUndoRedoCommand)
3. Property tabs (StrokeFill, PathOps, Color, PES, SVG) + multi-select
4. Drag & drop ไฟล์ลงหน้าต่าง (onDragDropEvent), recent files, dirty tracking
5. Text (PPEF native sqlite + TTF) → Tools → PathEdit/StitchEdit → simulator
6. Windows build (GN clang-cl)
7. Smart satin จังหวะที่ 2 (เมื่อคุ้มค่า): port geometry core จาก vendored JS ลง C++ (`pes_satin_core.hpp` ต่อยอด seams เดิม, command ไม่เปลี่ยน — frontend ไม่ต้องแก้) ใช้ output ของจังหวะ 1 เป็น golden test (~~แผนเดิม crate Rust ตกไป — Rust ไม่อยู่ใน build ฝั่ง web~~)

## คำสั่งที่ใช้บ่อย
- Test engine: `cd src-tauri && cargo test --lib engine`
- Dev app: `npm run tauri dev`
- แก้ engine/format: แก้ใน `src-tauri/cpp/pes/` แล้ว `cargo build` (recompile เอง). ต้องมี `libskia.a` พร้อมที่ `SkiaApps/out/...` — rebuild Skia: `cd ../SkiaApps && ninja -C out/macos-arm64-release skia`

## Reference หลัก (โค้ดเดิม)
- API spec: `SkiaApps/apps2/1080_PES5Template/src/PES5Template_bindings.cpp`, `SkiaApps/modules/canvaskit/pes_bindings.cpp`
- Logic operations/tools: `SkiaApps/apps2/1080_PES5Template/src/PES5Command.cpp`
- Canvas behaviors: `SkiaApps/apps2/1080_PES5Template/src/GUI/PES5DocumentView.cpp`
- Undo/redo semantics: `SkiaApps/apps2/1080_PES5Template/src/PESUndoRedoCommand.cpp`
- UI panels spec: `Victor-frontend/PES5/client/js/*` (27 handlers)
- Assets: `Victor-frontend/PES5/cordova/www/PES2025_Data`, `Victor-frontend/PES5/res`
