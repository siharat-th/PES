pub mod commands;
pub mod engine;
pub mod history;

use tauri::Manager;

#[cfg_attr(mobile, tauri::mobile_entry_point)]
pub fn run() {
    tauri::Builder::default()
        .plugin(tauri_plugin_opener::init())
        .plugin(tauri_plugin_dialog::init())
        .setup(|app| {
            // Stitch textures etc. live in bundled resources (resources/texture/...).
            let resource_dir = app
                .path()
                .resource_dir()
                .ok()
                .map(|p| p.join("resources"))
                .filter(|p| p.join("texture").exists())
                // dev fallback: src-tauri/resources
                .unwrap_or_else(|| {
                    std::path::PathBuf::from(env!("CARGO_MANIFEST_DIR")).join("resources")
                });
            let _ = commands::RESOURCE_DIR.set(resource_dir.clone());
            engine::with_engine(|eng| {
                eng.set_resource_path(resource_dir.to_string_lossy().as_ref());
                eng.new_document();
            });
            Ok(())
        })
        .invoke_handler(tauri::generate_handler![
            commands::new_document,
            commands::open_file,
            commands::get_document,
            commands::get_object_image,
            commands::get_stitch_data,
            commands::transform_object,
            commands::export_file,
            commands::delete_object,
            commands::duplicate_object,
            commands::set_object_visible,
            commands::set_object_locked,
            commands::reorder_object,
            commands::reorder_object_to,
            commands::undo,
            commands::redo,
            commands::get_object_paths,
            commands::get_brother_palette,
            commands::set_path_fill_color,
            commands::set_path_stroke_color,
            commands::set_path_stroke_width,
            commands::get_color_blocks,
            commands::set_color_block,
            commands::swap_color_block,
            commands::flip_object,
            commands::get_parameter,
            commands::set_parameter,
            commands::list_ppef_fonts,
            commands::list_ttf_fonts,
            commands::apply_path_op,
            commands::get_path_nodes,
            commands::move_path_node,
            commands::move_path_handle,
            commands::insert_path_node,
            commands::delete_path_node,
            commands::get_stitch_points,
            commands::move_stitch_point,
            commands::insert_stitch_point,
            commands::insert_stitch_point_at,
            commands::delete_stitch_point,
            commands::translate_objects,
            commands::delete_objects,
            commands::create_group,
            commands::rename_group,
            commands::ungroup,
            commands::add_to_group,
            commands::remove_from_group,
            commands::set_group_collapsed,
            commands::set_group_visible,
            commands::set_group_locked,
        ])
        .run(tauri::generate_context!())
        .expect("error while running tauri application");
}
