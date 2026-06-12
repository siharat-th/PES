//! Tauri commands — thin async wrappers over the engine.
//! Engine work runs on blocking threads (stitch ops can be slow).

use crate::engine::{with_engine, BrotherColor, ColorBlockInfo, ObjectSnapshot, PathInfo};
use crate::history;
use serde::Serialize;
use tauri::ipc::Response;

#[derive(Debug, Clone, Serialize)]
pub struct DocumentSnapshot {
    pub hoop_width_mm: f32,
    pub hoop_height_mm: f32,
    pub objects: Vec<ObjectSnapshot>,
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
        let ok = history::run_undoable(|eng| {
            let ok = match &value {
                ParamValue::Bool(b) => eng.set_param_bool(index, &key, *b),
                ParamValue::Num(n) => eng.set_param_num(index, &key, *n),
                ParamValue::Str(s) => eng.set_param_str(index, &key, s),
            };
            if ok {
                // text-type objects regenerate from parameters (no-op otherwise)
                eng.update_ppef_text(index);
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

#[tauri::command]
pub async fn list_ppef_fonts() -> Result<Vec<String>, String> {
    run_blocking(|| {
        let dir = RESOURCE_DIR
            .get()
            .ok_or("resource dir not initialized")?
            .join("PPEF");
        let mut fonts: Vec<String> = std::fs::read_dir(&dir)
            .map_err(|e| format!("read {dir:?}: {e}"))?
            .filter_map(|e| e.ok())
            .filter_map(|e| {
                let p = e.path();
                (p.extension().and_then(|x| x.to_str()) == Some("ppef"))
                    .then(|| p.file_stem()?.to_str().map(String::from))
                    .flatten()
            })
            .collect();
        fonts.sort();
        Ok(fonts)
    })
    .await?
}

#[tauri::command]
pub async fn apply_path_op(
    index: i32,
    path_index: i32,
    op: String,
    value: f32,
) -> Result<DocumentSnapshot, String> {
    run_blocking(move || {
        let ok = history::run_undoable(|eng| eng.path_op(index, path_index, &op, value));
        if !ok {
            return Err(format!("path op ล้มเหลว: {op}"));
        }
        Ok(document_snapshot())
    })
    .await?
}

/// Move several objects by the same delta as ONE undo step (multi-select drag).
#[tauri::command]
pub async fn translate_objects(
    indices: Vec<i32>,
    dx: f32,
    dy: f32,
) -> Result<DocumentSnapshot, String> {
    run_blocking(move || {
        history::run_undoable(|eng| {
            for &i in &indices {
                eng.translate_object(i, dx, dy);
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
