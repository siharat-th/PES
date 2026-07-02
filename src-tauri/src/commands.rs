//! Tauri commands — thin async wrappers over the engine.
//! Engine work runs on blocking threads (stitch ops can be slow).

use crate::engine::{
    with_engine, BrotherColor, ColorBlockInfo, GroupSnapshot, ObjectSnapshot, PathInfo, PathNode,
    StitchBlock,
};
use crate::history;
use serde::Serialize;
use tauri::ipc::Response;

#[derive(Debug, Clone, Serialize)]
pub struct DocumentSnapshot {
    pub hoop_width_mm: f32,
    pub hoop_height_mm: f32,
    pub objects: Vec<ObjectSnapshot>,
    pub groups: Vec<GroupSnapshot>,
    pub can_undo: bool,
    pub can_redo: bool,
}

fn document_snapshot() -> DocumentSnapshot {
    with_engine(|eng| {
        let (w, h) = eng.hoop_size_mm();
        DocumentSnapshot {
            hoop_width_mm: w,
            hoop_height_mm: h,
            objects: eng.object_snapshots(),
            groups: eng.groups(),
            can_undo: history::can_undo(),
            can_redo: history::can_redo(),
        }
    })
}

async fn run_blocking<R: Send + 'static>(
    f: impl FnOnce() -> R + Send + 'static,
) -> Result<R, String> {
    tauri::async_runtime::spawn_blocking(f)
        .await
        .map_err(|e| e.to_string())
}

#[tauri::command]
pub async fn new_document(hoop_w_mm: f32, hoop_h_mm: f32) -> Result<DocumentSnapshot, String> {
    run_blocking(move || {
        with_engine(|eng| {
            eng.new_document();
            eng.set_hoop_size_mm(hoop_w_mm, hoop_h_mm);
        });
        history::clear();
        document_snapshot()
    })
    .await
}

#[tauri::command]
pub async fn open_file(path: String) -> Result<DocumentSnapshot, String> {
    run_blocking(move || {
        let bytes = std::fs::read(&path).map_err(|e| format!("read {path}: {e}"))?;
        let ext = std::path::Path::new(&path)
            .extension()
            .and_then(|e| e.to_str())
            .unwrap_or("")
            .to_ascii_lowercase();

        let is_import = matches!(ext.as_str(), "pes" | "svg");
        let ok = if is_import {
            // adding into the current document is undoable
            history::run_undoable(|eng| match ext.as_str() {
                "pes" => eng.import_pes(&bytes),
                _ => eng.import_svg(&bytes),
            })
        } else {
            let ok = with_engine(|eng| match ext.as_str() {
                "ppes" | "ppes5" => {
                    eng.new_document();
                    eng.load_ppes(&bytes)
                }
                _ => false,
            });
            history::clear();
            ok
        };
        if !ok {
            return Err(format!(
                "ไม่สามารถเปิดไฟล์ {path} ได้ (format ไม่รองรับหรือไฟล์เสีย)"
            ));
        }
        Ok(document_snapshot())
    })
    .await?
}

#[tauri::command]
pub async fn get_document() -> Result<DocumentSnapshot, String> {
    run_blocking(document_snapshot).await
}

#[tauri::command]
pub async fn get_object_image(index: i32) -> Result<Response, String> {
    let png = run_blocking(move || with_engine(|eng| eng.object_image_png(index))).await?;
    Ok(Response::new(png))
}

#[derive(Serialize)]
pub struct StitchSegmentDto {
    pub hex: String,
    pub start: u32,
    pub count: u32,
}

#[derive(Serialize)]
pub struct StitchDataDto {
    pub segments: Vec<StitchSegmentDto>,
    pub total_points: u32,
    /// flat x,y pairs in engine units (0.1mm), base64-encoded little-endian f32
    pub coords_b64: String,
}

#[tauri::command]
pub async fn get_stitch_data(index: i32) -> Result<StitchDataDto, String> {
    run_blocking(move || {
        let data = with_engine(|eng| eng.stitch_data(index));
        let mut bytes = Vec::with_capacity(data.coords.len() * 4);
        for f in &data.coords {
            bytes.extend_from_slice(&f.to_le_bytes());
        }
        StitchDataDto {
            segments: data
                .segments
                .iter()
                .map(|s| StitchSegmentDto {
                    hex: s.hex.clone(),
                    start: s.start,
                    count: s.count,
                })
                .collect(),
            total_points: data.total_points,
            coords_b64: base64_encode(&bytes),
        }
    })
    .await
}

/// Minimal standard base64 (avoids an extra dependency for one call site).
fn base64_encode(data: &[u8]) -> String {
    const T: &[u8; 64] =
        b"ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    let mut out = String::with_capacity((data.len() + 2) / 3 * 4);
    for chunk in data.chunks(3) {
        let b = [
            chunk[0],
            *chunk.get(1).unwrap_or(&0),
            *chunk.get(2).unwrap_or(&0),
        ];
        let n = (b[0] as u32) << 16 | (b[1] as u32) << 8 | b[2] as u32;
        out.push(T[(n >> 18 & 63) as usize] as char);
        out.push(T[(n >> 12 & 63) as usize] as char);
        out.push(if chunk.len() > 1 {
            T[(n >> 6 & 63) as usize] as char
        } else {
            '='
        });
        out.push(if chunk.len() > 2 {
            T[(n & 63) as usize] as char
        } else {
            '='
        });
    }
    out
}

#[tauri::command]
pub async fn transform_object(
    index: i32,
    dx: f32,
    dy: f32,
    sx: f32,
    sy: f32,
    rotate_degree: f32,
) -> Result<DocumentSnapshot, String> {
    run_blocking(move || {
        history::run_undoable(|eng| {
            if sx != 1.0 || sy != 1.0 {
                eng.scale_object(index, sx, sy);
            }
            if dx != 0.0 || dy != 0.0 {
                eng.translate_object(index, dx, dy);
            }
            eng.set_object_rotation(index, rotate_degree);
        });
        document_snapshot()
    })
    .await
}

#[tauri::command]
pub async fn export_file(path: String, format: String) -> Result<(), String> {
    run_blocking(move || {
        let bytes = with_engine(|eng| eng.export_as(&format));
        if bytes.is_empty() {
            return Err(format!("export {format} ได้ข้อมูลว่าง"));
        }
        std::fs::write(&path, bytes).map_err(|e| format!("write {path}: {e}"))
    })
    .await?
}

#[tauri::command]
pub async fn delete_object(index: i32) -> Result<DocumentSnapshot, String> {
    run_blocking(move || {
        history::run_undoable(|eng| eng.delete_object(index));
        document_snapshot()
    })
    .await
}

#[tauri::command]
pub async fn duplicate_object(index: i32) -> Result<DocumentSnapshot, String> {
    run_blocking(move || {
        history::run_undoable(|eng| eng.duplicate_object(index));
        document_snapshot()
    })
    .await
}

/// Drop a ready-made parametric shape at the hoop center (0=line, 1=triangle,
/// 2=rect, 8=ellipse). The new object is appended last; the UI selects it.
#[tauri::command]
pub async fn add_shape(shape_index: i32) -> Result<DocumentSnapshot, String> {
    run_blocking(move || {
        history::run_undoable(|eng| eng.add_shape(shape_index));
        document_snapshot()
    })
    .await
}

/// Add a fresh PPEF text object at the hoop center (port of PES5_AddPPEFText).
/// The frontend passes the default text/font ("ภิญญ์จักรปัก" / Thai001). The
/// new object is appended last; the UI selects it.
#[tauri::command]
pub async fn add_ppef_text(text: String, font_name: String) -> Result<DocumentSnapshot, String> {
    run_blocking(move || {
        let ok = history::run_undoable_checked(|eng| eng.add_ppef_text(&text, &font_name) >= 0);
        if !ok {
            return Err(format!("สร้างข้อความ PPEF ไม่สำเร็จ (ฟอนต์ {font_name})"));
        }
        Ok(document_snapshot())
    })
    .await?
}

/// Add a fresh TTF text object at the hoop center (port of PES5_AddTTFText).
/// The frontend passes the default text/font ("ภิญญ์จักรปัก" / JS-Boaboon). The
/// new object is appended last; the UI selects it.
#[tauri::command]
pub async fn add_ttf_text(text: String, font_name: String) -> Result<DocumentSnapshot, String> {
    run_blocking(move || {
        let ok = history::run_undoable_checked(|eng| eng.add_ttf_text(&text, &font_name) >= 0);
        if !ok {
            return Err(format!("สร้างข้อความ TTF ไม่สำเร็จ (ฟอนต์ {font_name})"));
        }
        Ok(document_snapshot())
    })
    .await?
}

#[derive(Serialize)]
pub struct DuplicateResult {
    pub snapshot: DocumentSnapshot,
    pub new_indices: Vec<i32>,
    /// id of the freshly created group (when `group_name` was given), else -1.
    pub group_id: i32,
}

/// Duplicate every object in `indices` as ONE undo step. When `group_name` is
/// given, the new copies are reassigned into a fresh group of that name — so
/// duplicating a whole group yields a new group instead of inheriting the
/// originals' group. Returns the new copies' indices so the UI can select them.
#[tauri::command]
pub async fn duplicate_objects(
    indices: Vec<i32>,
    group_name: Option<String>,
) -> Result<DuplicateResult, String> {
    run_blocking(move || {
        let (new_indices, group_id) = history::run_undoable(|eng| {
            // Duplicate in ascending order: duplicateObject inserts the copy
            // right after its source, so each later source shifts up by the
            // number of insertions already made below it.
            let mut src = indices.clone();
            src.sort_unstable();
            src.dedup();
            let mut new_indices = Vec::with_capacity(src.len());
            let mut inserted = 0i32;
            for &s in &src {
                let cur = s + inserted;
                if eng.duplicate_object(cur) {
                    new_indices.push(cur + 1);
                    inserted += 1;
                }
            }
            // Copies inherit the source groupId; overwrite it for a new group.
            let mut group_id = -1;
            if let Some(name) = &group_name {
                if !new_indices.is_empty() {
                    let id = eng.create_group(name, 0);
                    for &i in &new_indices {
                        eng.set_object_group(i, id);
                    }
                    group_id = id;
                }
            }
            (new_indices, group_id)
        });
        DuplicateResult {
            snapshot: document_snapshot(),
            new_indices,
            group_id,
        }
    })
    .await
}

#[tauri::command]
pub async fn set_object_visible(index: i32, visible: bool) -> Result<DocumentSnapshot, String> {
    run_blocking(move || {
        history::run_undoable(|eng| eng.set_object_visible(index, visible));
        document_snapshot()
    })
    .await
}

#[tauri::command]
pub async fn set_object_locked(index: i32, locked: bool) -> Result<DocumentSnapshot, String> {
    run_blocking(move || {
        history::run_undoable(|eng| eng.set_object_locked(index, locked));
        document_snapshot()
    })
    .await
}

/// dir > 0 moves toward front (drawn later), dir < 0 toward back.
#[tauri::command]
pub async fn reorder_object(index: i32, dir: i32) -> Result<DocumentSnapshot, String> {
    run_blocking(move || {
        history::run_undoable(|eng| {
            if dir > 0 {
                eng.move_object_front(index);
            } else {
                eng.move_object_back(index);
            }
        });
        document_snapshot()
    })
    .await
}

/// Move an object to an arbitrary list position in one undoable step
/// (used by drag-and-drop reordering). Indices are list positions:
/// 0 = back-most, count-1 = front-most.
#[tauri::command]
pub async fn reorder_object_to(from: i32, to: i32) -> Result<DocumentSnapshot, String> {
    run_blocking(move || {
        history::run_undoable(|eng| {
            eng.move_object_to(from, to);
        });
        document_snapshot()
    })
    .await
}

#[tauri::command]
pub async fn get_object_paths(index: i32) -> Result<Vec<PathInfo>, String> {
    run_blocking(move || with_engine(|eng| eng.path_infos(index))).await
}

#[tauri::command]
pub async fn get_brother_palette() -> Result<Vec<BrotherColor>, String> {
    run_blocking(|| with_engine(|eng| eng.brother_palette())).await
}

#[tauri::command]
pub async fn set_path_fill_color(
    index: i32,
    path_index: i32,
    brother_index: i32,
) -> Result<DocumentSnapshot, String> {
    run_blocking(move || {
        history::run_undoable(|eng| eng.set_path_fill_color(index, path_index, brother_index));
        document_snapshot()
    })
    .await
}

#[tauri::command]
pub async fn set_path_stroke_color(
    index: i32,
    path_index: i32,
    brother_index: i32,
) -> Result<DocumentSnapshot, String> {
    run_blocking(move || {
        history::run_undoable(|eng| eng.set_path_stroke_color(index, path_index, brother_index));
        document_snapshot()
    })
    .await
}

#[tauri::command]
pub async fn set_path_stroke_width(
    index: i32,
    path_index: i32,
    width: f32,
) -> Result<DocumentSnapshot, String> {
    run_blocking(move || {
        history::run_undoable(|eng| eng.set_path_stroke_width(index, path_index, width));
        document_snapshot()
    })
    .await
}

#[tauri::command]
pub async fn get_color_blocks(index: i32) -> Result<Vec<ColorBlockInfo>, String> {
    run_blocking(move || with_engine(|eng| eng.color_blocks(index))).await
}

#[tauri::command]
pub async fn set_color_block(
    index: i32,
    block_index: i32,
    brother_index: i32,
) -> Result<DocumentSnapshot, String> {
    run_blocking(move || {
        history::run_undoable(|eng| eng.set_color_block(index, block_index, brother_index));
        document_snapshot()
    })
    .await
}

#[tauri::command]
pub async fn swap_color_block(
    index: i32,
    block_index: i32,
    dir: i32,
) -> Result<DocumentSnapshot, String> {
    run_blocking(move || {
        history::run_undoable(|eng| eng.swap_color_block(index, block_index, dir));
        document_snapshot()
    })
    .await
}

#[tauri::command]
pub async fn flip_object(index: i32, horizontal: bool) -> Result<DocumentSnapshot, String> {
    run_blocking(move || {
        history::run_undoable(|eng| eng.flip_object(index, horizontal));
        document_snapshot()
    })
    .await
}

#[tauri::command]
pub async fn get_parameter(index: i32) -> Result<String, String> {
    run_blocking(move || with_engine(|eng| eng.parameter_json(index))).await
}

/// Smart Satin input prep (flattened polygons + colors, JSON string).
#[tauri::command]
pub async fn get_satin_source(index: i32) -> Result<String, String> {
    run_blocking(move || with_engine(|eng| eng.satin_source(index))).await
}

/// Pathops-simplify rings for the Smart Satin JS core (JSON in/out).
#[tauri::command]
pub async fn simplify_polygons(polygons_json: String) -> Result<String, String> {
    run_blocking(move || with_engine(|eng| eng.simplify_polygons(&polygons_json))).await
}

/// Smooth manual satin-column rails (clicked knots -> two rail d-strings +
/// center) for the interactive draw tool's preview and commit (read-only).
#[tauri::command]
pub async fn satin_column_rails(rails_json: String) -> Result<String, String> {
    run_blocking(move || with_engine(|eng| eng.satin_column_rails(&rails_json))).await
}

/// Append satin-column objects built by the Smart Satin JS core (undoable).
#[tauri::command]
pub async fn add_satin_objects(objects_json: String) -> Result<DocumentSnapshot, String> {
    run_blocking(move || {
        let ok = history::run_undoable_checked(|eng| eng.add_satin_objects(&objects_json) > 0);
        if !ok {
            return Err("สร้าง Satin Column ไม่สำเร็จ".to_string());
        }
        Ok(document_snapshot())
    })
    .await?
}

/// Vector geometry (SVG paths + paint) for crisp Konva rendering of shapes.
#[tauri::command]
pub async fn get_object_vector(index: i32) -> Result<String, String> {
    run_blocking(move || with_engine(|eng| eng.object_vector_json(index))).await
}

#[derive(serde::Deserialize)]
#[serde(untagged)]
pub enum ParamValue {
    Bool(bool),
    Num(f32),
    Str(String),
}

#[tauri::command]
pub async fn set_parameter(
    index: i32,
    key: String,
    value: ParamValue,
) -> Result<DocumentSnapshot, String> {
    run_blocking(move || {
        let ok = history::run_undoable_checked(|eng| {
            let ok = match &value {
                ParamValue::Bool(b) => eng.set_param_bool(index, &key, *b),
                ParamValue::Num(n) => eng.set_param_num(index, &key, *n),
                ParamValue::Str(s) => eng.set_param_str(index, &key, s),
            };
            if ok {
                // type-specific regeneration from parameters (each no-ops for
                // the wrong type): PPEF/TTF text re-shape, SVG re-colors paths
                // and regenerates fill/stroke stitches.
                if !eng.update_ppef_text(index) && !eng.update_ttf_text(index) {
                    eng.update_svg(index);
                }
            }
            ok
        });
        if !ok {
            return Err(format!("unknown parameter key: {key}"));
        }
        Ok(document_snapshot())
    })
    .await?
}

/// Resource dir, set once at startup (lib.rs) — used for font listings.
pub static RESOURCE_DIR: std::sync::OnceLock<std::path::PathBuf> = std::sync::OnceLock::new();

fn list_fonts(subdir: &str, ext: &str) -> Result<Vec<String>, String> {
    let dir = RESOURCE_DIR
        .get()
        .ok_or("resource dir not initialized")?
        .join(subdir);
    let mut fonts: Vec<String> = std::fs::read_dir(&dir)
        .map_err(|e| format!("read {dir:?}: {e}"))?
        .filter_map(|e| e.ok())
        .filter_map(|e| {
            let p = e.path();
            (p.extension().and_then(|x| x.to_str()) == Some(ext))
                .then(|| p.file_stem()?.to_str().map(String::from))
                .flatten()
        })
        .collect();
    fonts.sort();
    Ok(fonts)
}

#[tauri::command]
pub async fn list_ppef_fonts() -> Result<Vec<String>, String> {
    run_blocking(|| list_fonts("PPEF", "ppef")).await?
}

#[tauri::command]
pub async fn list_ttf_fonts() -> Result<Vec<String>, String> {
    run_blocking(|| list_fonts("TTF", "ttf")).await?
}

#[tauri::command]
pub async fn get_path_nodes(index: i32, path_index: i32) -> Result<Vec<PathNode>, String> {
    run_blocking(move || with_engine(|eng| eng.path_nodes(index, path_index))).await
}

/// Move one path node (anchor + its attached bezier handles) by a world delta,
/// then regenerate stitches. One undo step per drag.
#[tauri::command]
pub async fn move_path_node(
    index: i32,
    path_index: i32,
    node_index: i32,
    dx: f32,
    dy: f32,
) -> Result<DocumentSnapshot, String> {
    run_blocking(move || {
        history::run_undoable_checked(|eng| {
            eng.move_path_node(index, path_index, node_index, dx, dy)
        });
        document_snapshot()
    })
    .await
}

/// Move one bezier control point (cp_slot: 1=cp1, 2=cp2 of cmd_index) by a
/// world delta, then regenerate stitches. One undo step per drag.
#[tauri::command]
pub async fn move_path_handle(
    index: i32,
    path_index: i32,
    cmd_index: i32,
    cp_slot: i32,
    dx: f32,
    dy: f32,
) -> Result<DocumentSnapshot, String> {
    run_blocking(move || {
        history::run_undoable_checked(|eng| {
            eng.move_path_handle(index, path_index, cmd_index, cp_slot, dx, dy)
        });
        document_snapshot()
    })
    .await
}

/// Insert a node on the segment ending at node_index, at parameter t. Errors
/// (keeping the selection) if the segment isn't interpolatable.
#[tauri::command]
pub async fn insert_path_node(
    index: i32,
    path_index: i32,
    node_index: i32,
    t: f32,
) -> Result<DocumentSnapshot, String> {
    run_blocking(move || {
        // refusal (non-interpolatable segment) is a benign no-op, not an error
        history::run_undoable_checked(|eng| eng.insert_path_node(index, path_index, node_index, t));
        document_snapshot()
    })
    .await
}

/// Delete the node at node_index. Errors if it's a subpath start/close or would
/// collapse the subpath.
#[tauri::command]
pub async fn delete_path_node(
    index: i32,
    path_index: i32,
    node_index: i32,
) -> Result<DocumentSnapshot, String> {
    run_blocking(move || {
        // refusal (subpath would collapse, or moveTo/close) is a benign no-op
        history::run_undoable_checked(|eng| eng.delete_path_node(index, path_index, node_index));
        document_snapshot()
    })
    .await
}

/// Convert the node's incoming segment between corner (line) and curve (bezier).
#[tauri::command]
pub async fn set_path_node_type(
    index: i32,
    path_index: i32,
    node_index: i32,
    to_curve: bool,
) -> Result<DocumentSnapshot, String> {
    run_blocking(move || {
        // refusal (moveTo/close, or already the target type) is a benign no-op
        history::run_undoable_checked(|eng| {
            eng.set_path_node_type(index, path_index, node_index, to_curve)
        });
        document_snapshot()
    })
    .await
}

#[tauri::command]
pub async fn get_stitch_points(index: i32) -> Result<Vec<StitchBlock>, String> {
    run_blocking(move || with_engine(|eng| eng.stitch_points(index))).await
}

/// Move one needle point by a world delta. One undo step per drag.
#[tauri::command]
pub async fn move_stitch_point(
    index: i32,
    kind: i32,
    block_index: i32,
    point_index: i32,
    dx: f32,
    dy: f32,
) -> Result<DocumentSnapshot, String> {
    run_blocking(move || {
        history::run_undoable_checked(|eng| {
            eng.move_stitch_point(index, kind, block_index, point_index, dx, dy)
        });
        document_snapshot()
    })
    .await
}

/// Insert a needle point near point_index (refusal is a benign no-op).
#[tauri::command]
pub async fn insert_stitch_point(
    index: i32,
    kind: i32,
    block_index: i32,
    point_index: i32,
) -> Result<DocumentSnapshot, String> {
    run_blocking(move || {
        history::run_undoable_checked(|eng| {
            eng.insert_stitch_point(index, kind, block_index, point_index)
        });
        document_snapshot()
    })
    .await
}

/// Insert a needle point at a world position right after after_index
/// (double-click on a thread line).
#[tauri::command]
pub async fn insert_stitch_point_at(
    index: i32,
    kind: i32,
    block_index: i32,
    after_index: i32,
    x: f32,
    y: f32,
) -> Result<DocumentSnapshot, String> {
    run_blocking(move || {
        history::run_undoable_checked(|eng| {
            eng.insert_stitch_point_at(index, kind, block_index, after_index, x, y)
        });
        document_snapshot()
    })
    .await
}

/// Delete the needle point at point_index (refusal is a benign no-op).
#[tauri::command]
pub async fn delete_stitch_point(
    index: i32,
    kind: i32,
    block_index: i32,
    point_index: i32,
) -> Result<DocumentSnapshot, String> {
    run_blocking(move || {
        history::run_undoable_checked(|eng| {
            eng.delete_stitch_point(index, kind, block_index, point_index)
        });
        document_snapshot()
    })
    .await
}

#[tauri::command]
pub async fn apply_path_op(
    index: i32,
    path_index: i32,
    op: String,
    value: f32,
) -> Result<DocumentSnapshot, String> {
    run_blocking(move || {
        let ok = history::run_undoable_checked(|eng| eng.path_op(index, path_index, &op, value));
        if !ok {
            return Err(format!("path op ล้มเหลว: {op}"));
        }
        Ok(document_snapshot())
    })
    .await?
}

#[derive(serde::Deserialize)]
pub struct ObjectMove {
    pub index: i32,
    pub dx: f32,
    pub dy: f32,
}

/// Move several objects (per-object deltas) as ONE undo step (multi-select drag).
#[tauri::command]
pub async fn translate_objects(moves: Vec<ObjectMove>) -> Result<DocumentSnapshot, String> {
    run_blocking(move || {
        history::run_undoable(|eng| {
            for m in &moves {
                eng.translate_object(m.index, m.dx, m.dy);
            }
        });
        document_snapshot()
    })
    .await
}

/// Delete several objects as ONE undo step (highest index first).
#[tauri::command]
pub async fn delete_objects(mut indices: Vec<i32>) -> Result<DocumentSnapshot, String> {
    run_blocking(move || {
        indices.sort_unstable_by(|a, b| b.cmp(a));
        indices.dedup();
        history::run_undoable(|eng| {
            for &i in &indices {
                eng.delete_object(i);
            }
        });
        document_snapshot()
    })
    .await
}

// --- Layer groups ---------------------------------------------------------

/// Create a group and (optionally) move the given objects into it — ONE undo step.
#[tauri::command]
pub async fn create_group(
    name: String,
    member_indices: Vec<i32>,
) -> Result<DocumentSnapshot, String> {
    run_blocking(move || {
        history::run_undoable(|eng| {
            let id = eng.create_group(&name, 0);
            for &i in &member_indices {
                eng.set_object_group(i, id);
            }
        });
        document_snapshot()
    })
    .await
}

#[tauri::command]
pub async fn rename_group(id: i32, name: String) -> Result<DocumentSnapshot, String> {
    run_blocking(move || {
        history::run_undoable(|eng| {
            eng.rename_group(id, &name);
        });
        document_snapshot()
    })
    .await
}

/// Ungroup: drop the group, members revert to ungrouped (objects are kept).
#[tauri::command]
pub async fn ungroup(id: i32) -> Result<DocumentSnapshot, String> {
    run_blocking(move || {
        history::run_undoable(|eng| {
            // Members may be non-contiguous in the flat list (a duplicated group
            // interleaves copies with originals, drag-reorder can split a group).
            // The panel hides this by clustering on group_id, but ungrouping
            // exposes the raw order. Compact the members into one contiguous
            // block first — preserving their relative z-order and anchoring at
            // the front-most (highest-index) member — so the order stays right.
            let mut members: Vec<i32> = eng
                .object_snapshots()
                .iter()
                .filter(|o| o.group_id == id)
                .map(|o| o.index)
                .collect();
            members.sort_unstable();
            if members.len() >= 2 {
                let k = members.len();
                let top = members[k - 1];
                // High→low: each lower member slides up to just below the block
                // already assembled at the top. A lower member keeps its index
                // until moved, since each move only touches higher positions.
                for j in (0..k - 1).rev() {
                    let dest = top - (k as i32 - 1 - j as i32);
                    eng.move_object_to(members[j], dest);
                }
            }
            eng.delete_group(id);
        });
        document_snapshot()
    })
    .await
}

/// Delete a group AND all of its member objects — ONE undo step. (Distinct from
/// `ungroup`, which keeps the objects.)
#[tauri::command]
pub async fn delete_group(id: i32) -> Result<DocumentSnapshot, String> {
    run_blocking(move || {
        history::run_undoable(|eng| {
            // Delete members high-index → low so earlier indices stay valid.
            let mut members: Vec<i32> = eng
                .object_snapshots()
                .iter()
                .filter(|o| o.group_id == id)
                .map(|o| o.index)
                .collect();
            members.sort_unstable();
            for &i in members.iter().rev() {
                eng.delete_object(i);
            }
            eng.delete_group(id);
        });
        document_snapshot()
    })
    .await
}

#[tauri::command]
pub async fn add_to_group(id: i32, indices: Vec<i32>) -> Result<DocumentSnapshot, String> {
    run_blocking(move || {
        history::run_undoable(|eng| {
            for &i in &indices {
                eng.set_object_group(i, id);
            }
        });
        document_snapshot()
    })
    .await
}

#[tauri::command]
pub async fn remove_from_group(indices: Vec<i32>) -> Result<DocumentSnapshot, String> {
    run_blocking(move || {
        history::run_undoable(|eng| {
            for &i in &indices {
                eng.set_object_group(i, 0);
            }
        });
        document_snapshot()
    })
    .await
}

/// Collapse/expand is UI metadata: it persists into the file but does NOT push
/// an undo entry (mirrors the lightweight feel of toggling visibility).
#[tauri::command]
pub async fn set_group_collapsed(id: i32, collapsed: bool) -> Result<DocumentSnapshot, String> {
    run_blocking(move || {
        with_engine(|eng| {
            eng.set_group_collapsed(id, collapsed);
        });
        document_snapshot()
    })
    .await
}

/// Cascade visibility to all members — ONE undo step.
#[tauri::command]
pub async fn set_group_visible(id: i32, visible: bool) -> Result<DocumentSnapshot, String> {
    run_blocking(move || {
        history::run_undoable(|eng| {
            eng.set_group_visible(id, visible);
        });
        document_snapshot()
    })
    .await
}

/// Cascade lock to all members — ONE undo step.
#[tauri::command]
pub async fn set_group_locked(id: i32, locked: bool) -> Result<DocumentSnapshot, String> {
    run_blocking(move || {
        history::run_undoable(|eng| {
            eng.set_group_locked(id, locked);
        });
        document_snapshot()
    })
    .await
}

#[tauri::command]
pub async fn undo() -> Result<DocumentSnapshot, String> {
    run_blocking(|| {
        history::undo();
        document_snapshot()
    })
    .await
}

#[tauri::command]
pub async fn redo() -> Result<DocumentSnapshot, String> {
    run_blocking(|| {
        history::redo();
        document_snapshot()
    })
    .await
}
