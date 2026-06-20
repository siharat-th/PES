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

### ▶ Milestone ถัดไป (Phase 1 ต่อ)
1. ตรวจ fidelity transform กับแอปเดิม (scale semantics ของ pesData.scale ที่ไม่ scalable, rotate + stitch regen)
2. Undo/Redo (Rust command stack ตาม PESUndoRedoCommand)
3. Property tabs (StrokeFill, PathOps, Color, PES, SVG) + multi-select
4. Drag & drop ไฟล์ลงหน้าต่าง (onDragDropEvent), recent files, dirty tracking
5. Text (PPEF native sqlite + TTF) → Tools → PathEdit/StitchEdit → simulator
6. Windows build (GN clang-cl)
7. Smart satin: port makesatincolumn.js (~8.9K LOC: straight skeleton + Catmull-Rom + zigzag) เป็น crate Rust (`geo`, `cavalier_contours`, `geo-buffer`) — golden test เทียบ JS เดิม

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
