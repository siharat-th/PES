//! FFI bridge to the C++ pesEngine (libpes + Skia, built from the SkiaApps tree).
//!
//! pesDocument is a process-wide singleton on the C++ side, so all access
//! must go through [`with_engine`] which serializes calls behind a mutex.

use std::sync::Mutex;

#[cxx::bridge(namespace = "pesffi")]
mod ffi {
    /// Plain-data view of one document object (engine units = 0.1 mm).
    #[derive(Debug, Clone, Serialize)]
    struct ObjectSnapshot {
        index: i32,
        x: f32,
        y: f32,
        width: f32,
        height: f32,
        rotate_degree: f32,
        visible: bool,
        locked: bool,
        scalable: bool,
        object_type: String,
        text: String,
    }

    /// One vector path inside an object (colors as #RRGGBB).
    #[derive(Debug, Clone, Serialize)]
    struct PathInfo {
        index: i32,
        path_id: String,
        is_fill: bool,
        is_stroke: bool,
        fill_type: i32,
        fill_color: String,
        stroke_color: String,
        stroke_width: f32,
        visible: bool,
    }

    /// Brother thread color (palette index 1..=65).
    #[derive(Debug, Clone, Serialize)]
    struct BrotherColor {
        index: i32,
        hex: String,
        name: String,
    }

    /// One thread color block (fillBlocks) of an object.
    #[derive(Debug, Clone, Serialize)]
    struct ColorBlockInfo {
        index: i32,
        hex: String,
        brother_index: i32,
        stitch_count: i32,
    }

    /// One run of continuous needle stitches (jumps break runs).
    #[derive(Debug, Clone, Serialize)]
    struct StitchSegment {
        hex: String,
        /// offset (in points = pairs) into the flat coordinate buffer
        start: u32,
        /// number of points (x,y pairs) in this segment
        count: u32,
    }

    /// Whole-document stitch geometry. `coords` is x,y pairs in engine units
    /// (0.1mm); segments index into it. Total points = simulator length.
    #[derive(Debug, Clone)]
    struct StitchData {
        segments: Vec<StitchSegment>,
        coords: Vec<f32>,
        total_points: u32,
    }

    /// One path command for node editing. Coordinates are WORLD units (the
    /// object's display rotation is already applied). `node_type` mirrors
    /// pesPath::Command::Type (0=moveTo,1=lineTo,2=curveTo,3=bezierTo,
    /// 4=quadBezierTo,5=arc,6=arcNegative,7=close). cp1/cp2 are only
    /// meaningful for bezier/quad commands.
    #[derive(Debug, Clone, Serialize)]
    struct PathNode {
        node_type: i32,
        x: f32,
        y: f32,
        cp1x: f32,
        cp1y: f32,
        cp2x: f32,
        cp2y: f32,
    }

    /// One editable needle point (WORLD units; object rotation folded in).
    #[derive(Debug, Clone, Serialize)]
    struct StitchPoint {
        x: f32,
        y: f32,
        /// jump/trim point (machine moves without stitching) rather than a needle drop
        jump: bool,
    }

    /// One stitch block for StitchEdit. `kind`: 0 = fill, 1 = stroke;
    /// `block_index` indexes into that object's fill/stroke block vector.
    #[derive(Debug, Clone, Serialize)]
    struct StitchBlock {
        kind: i32,
        block_index: i32,
        hex: String,
        points: Vec<StitchPoint>,
    }

    unsafe extern "C++" {
        include!("pes_ffi.h");

        fn set_resource_path(path: &str);

        fn new_document();
        fn set_hoop_size_mm(w: f32, h: f32);
        fn get_hoop_width_mm() -> f32;
        fn get_hoop_height_mm() -> f32;

        fn load_ppes(data: &[u8]) -> bool;
        fn import_pes(data: &[u8]) -> bool;
        fn import_svg(data: &[u8]) -> bool;

        fn get_object_count() -> i32;
        fn get_object_snapshot(index: i32) -> ObjectSnapshot;
        fn get_object_image_png(index: i32) -> Vec<u8>;

        fn translate_object(index: i32, dx: f32, dy: f32);
        fn scale_object(index: i32, sx: f32, sy: f32);
        fn set_object_rotation(index: i32, degree: f32);
        fn set_object_visible(index: i32, visible: bool);
        fn set_object_locked(index: i32, locked: bool);
        fn delete_object(index: i32) -> bool;
        fn duplicate_object(index: i32) -> bool;
        fn move_object_front(index: i32) -> bool;
        fn move_object_back(index: i32) -> bool;
        fn move_object_to(from: i32, to: i32) -> bool;

        fn export_as(format: &str) -> Vec<u8>;
        fn get_thumbnail_png(wmax: i32, hmax: i32, index: i32) -> Vec<u8>;

        fn get_path_count(obj_index: i32) -> i32;
        fn get_path_info(obj_index: i32, path_index: i32) -> PathInfo;
        fn set_path_fill_color(obj_index: i32, path_index: i32, brother_index: i32);
        fn set_path_stroke_color(obj_index: i32, path_index: i32, brother_index: i32);
        fn set_path_stroke_width(obj_index: i32, path_index: i32, width: f32);
        fn get_brother_palette() -> Vec<BrotherColor>;

        fn get_stitch_data(obj_index: i32) -> StitchData;
        fn get_color_blocks(obj_index: i32) -> Vec<ColorBlockInfo>;
        fn set_color_block(obj_index: i32, block_index: i32, brother_index: i32);
        fn swap_color_block(obj_index: i32, block_index: i32, dir: i32) -> bool;
        fn flip_object(obj_index: i32, horizontal: bool);

        fn get_parameter_json(obj_index: i32) -> String;
        fn update_ppef_text(obj_index: i32) -> bool;
        fn update_ttf_text(obj_index: i32) -> bool;
        fn get_path_nodes(obj_index: i32, path_index: i32) -> Vec<PathNode>;
        fn move_path_node(
            obj_index: i32,
            path_index: i32,
            node_index: i32,
            world_dx: f32,
            world_dy: f32,
        ) -> bool;
        fn move_path_handle(
            obj_index: i32,
            path_index: i32,
            cmd_index: i32,
            cp_slot: i32,
            world_dx: f32,
            world_dy: f32,
        ) -> bool;
        fn insert_path_node(obj_index: i32, path_index: i32, node_index: i32, t: f32) -> bool;
        fn delete_path_node(obj_index: i32, path_index: i32, node_index: i32) -> bool;

        fn get_stitch_points(obj_index: i32) -> Vec<StitchBlock>;
        fn move_stitch_point(
            obj_index: i32,
            kind: i32,
            block_index: i32,
            point_index: i32,
            world_dx: f32,
            world_dy: f32,
        ) -> bool;
        fn insert_stitch_point(
            obj_index: i32,
            kind: i32,
            block_index: i32,
            point_index: i32,
        ) -> bool;
        fn insert_stitch_point_at(
            obj_index: i32,
            kind: i32,
            block_index: i32,
            after_index: i32,
            world_x: f32,
            world_y: f32,
        ) -> bool;
        fn delete_stitch_point(
            obj_index: i32,
            kind: i32,
            block_index: i32,
            point_index: i32,
        ) -> bool;

        fn path_op(obj_index: i32, path_index: i32, op: &str, value: f32) -> bool;
        fn set_param_num(obj_index: i32, key: &str, value: f32) -> bool;
        fn set_param_bool(obj_index: i32, key: &str, value: bool) -> bool;
        fn set_param_str(obj_index: i32, key: &str, value: &str) -> bool;
    }
}

pub use ffi::{
    BrotherColor, ColorBlockInfo, ObjectSnapshot, PathInfo, PathNode, StitchBlock, StitchData,
    StitchPoint,
};

static ENGINE_LOCK: Mutex<()> = Mutex::new(());

/// Serialized access to the C++ engine singleton.
pub struct Engine<'a> {
    _guard: std::sync::MutexGuard<'a, ()>,
}

pub fn with_engine<R>(f: impl FnOnce(&Engine) -> R) -> R {
    let guard = ENGINE_LOCK.lock().unwrap_or_else(|e| e.into_inner());
    f(&Engine { _guard: guard })
}

impl Engine<'_> {
    pub fn set_resource_path(&self, path: &str) {
        ffi::set_resource_path(path);
    }

    pub fn new_document(&self) {
        ffi::new_document();
    }

    pub fn set_hoop_size_mm(&self, w: f32, h: f32) {
        ffi::set_hoop_size_mm(w, h);
    }

    pub fn hoop_size_mm(&self) -> (f32, f32) {
        (ffi::get_hoop_width_mm(), ffi::get_hoop_height_mm())
    }

    pub fn load_ppes(&self, data: &[u8]) -> bool {
        ffi::load_ppes(data)
    }

    pub fn import_pes(&self, data: &[u8]) -> bool {
        ffi::import_pes(data)
    }

    pub fn import_svg(&self, data: &[u8]) -> bool {
        ffi::import_svg(data)
    }

    pub fn object_count(&self) -> i32 {
        ffi::get_object_count()
    }

    pub fn object_snapshot(&self, index: i32) -> ObjectSnapshot {
        ffi::get_object_snapshot(index)
    }

    pub fn object_snapshots(&self) -> Vec<ObjectSnapshot> {
        (0..self.object_count())
            .map(|i| self.object_snapshot(i))
            .collect()
    }

    pub fn object_image_png(&self, index: i32) -> Vec<u8> {
        ffi::get_object_image_png(index)
    }

    pub fn translate_object(&self, index: i32, dx: f32, dy: f32) {
        ffi::translate_object(index, dx, dy);
    }

    pub fn scale_object(&self, index: i32, sx: f32, sy: f32) {
        ffi::scale_object(index, sx, sy);
    }

    pub fn set_object_rotation(&self, index: i32, degree: f32) {
        ffi::set_object_rotation(index, degree);
    }

    pub fn set_object_visible(&self, index: i32, visible: bool) {
        ffi::set_object_visible(index, visible);
    }

    pub fn set_object_locked(&self, index: i32, locked: bool) {
        ffi::set_object_locked(index, locked);
    }

    pub fn delete_object(&self, index: i32) -> bool {
        ffi::delete_object(index)
    }

    pub fn duplicate_object(&self, index: i32) -> bool {
        ffi::duplicate_object(index)
    }

    pub fn move_object_front(&self, index: i32) -> bool {
        ffi::move_object_front(index)
    }

    pub fn move_object_back(&self, index: i32) -> bool {
        ffi::move_object_back(index)
    }

    pub fn move_object_to(&self, from: i32, to: i32) -> bool {
        ffi::move_object_to(from, to)
    }

    pub fn export_as(&self, format: &str) -> Vec<u8> {
        ffi::export_as(format)
    }

    pub fn path_infos(&self, obj_index: i32) -> Vec<PathInfo> {
        (0..ffi::get_path_count(obj_index))
            .map(|i| ffi::get_path_info(obj_index, i))
            .collect()
    }

    pub fn set_path_fill_color(&self, obj_index: i32, path_index: i32, brother_index: i32) {
        ffi::set_path_fill_color(obj_index, path_index, brother_index);
    }

    pub fn set_path_stroke_color(&self, obj_index: i32, path_index: i32, brother_index: i32) {
        ffi::set_path_stroke_color(obj_index, path_index, brother_index);
    }

    pub fn set_path_stroke_width(&self, obj_index: i32, path_index: i32, width: f32) {
        ffi::set_path_stroke_width(obj_index, path_index, width);
    }

    pub fn brother_palette(&self) -> Vec<BrotherColor> {
        ffi::get_brother_palette()
    }

    pub fn color_blocks(&self, obj_index: i32) -> Vec<ColorBlockInfo> {
        ffi::get_color_blocks(obj_index)
    }

    pub fn stitch_data(&self, obj_index: i32) -> StitchData {
        ffi::get_stitch_data(obj_index)
    }

    pub fn set_color_block(&self, obj_index: i32, block_index: i32, brother_index: i32) {
        ffi::set_color_block(obj_index, block_index, brother_index);
    }

    pub fn swap_color_block(&self, obj_index: i32, block_index: i32, dir: i32) -> bool {
        ffi::swap_color_block(obj_index, block_index, dir)
    }

    pub fn flip_object(&self, obj_index: i32, horizontal: bool) {
        ffi::flip_object(obj_index, horizontal);
    }

    pub fn parameter_json(&self, obj_index: i32) -> String {
        ffi::get_parameter_json(obj_index)
    }

    /// Rebuild paths/stitches of a PPEF text object from its parameters.
    /// No-op (returns false) for other object types.
    pub fn update_ppef_text(&self, obj_index: i32) -> bool {
        ffi::update_ppef_text(obj_index)
    }

    /// Rebuild a TTF text object's path from its parameters (no-op otherwise).
    pub fn update_ttf_text(&self, obj_index: i32) -> bool {
        ffi::update_ttf_text(obj_index)
    }

    pub fn path_nodes(&self, obj_index: i32, path_index: i32) -> Vec<PathNode> {
        ffi::get_path_nodes(obj_index, path_index)
    }

    pub fn move_path_node(
        &self,
        obj_index: i32,
        path_index: i32,
        node_index: i32,
        dx: f32,
        dy: f32,
    ) -> bool {
        ffi::move_path_node(obj_index, path_index, node_index, dx, dy)
    }

    pub fn move_path_handle(
        &self,
        obj_index: i32,
        path_index: i32,
        cmd_index: i32,
        cp_slot: i32,
        dx: f32,
        dy: f32,
    ) -> bool {
        ffi::move_path_handle(obj_index, path_index, cmd_index, cp_slot, dx, dy)
    }

    pub fn insert_path_node(&self, obj_index: i32, path_index: i32, node_index: i32, t: f32) -> bool {
        ffi::insert_path_node(obj_index, path_index, node_index, t)
    }

    pub fn delete_path_node(&self, obj_index: i32, path_index: i32, node_index: i32) -> bool {
        ffi::delete_path_node(obj_index, path_index, node_index)
    }

    pub fn stitch_points(&self, obj_index: i32) -> Vec<StitchBlock> {
        ffi::get_stitch_points(obj_index)
    }

    pub fn move_stitch_point(
        &self,
        obj_index: i32,
        kind: i32,
        block_index: i32,
        point_index: i32,
        dx: f32,
        dy: f32,
    ) -> bool {
        ffi::move_stitch_point(obj_index, kind, block_index, point_index, dx, dy)
    }

    pub fn insert_stitch_point(
        &self,
        obj_index: i32,
        kind: i32,
        block_index: i32,
        point_index: i32,
    ) -> bool {
        ffi::insert_stitch_point(obj_index, kind, block_index, point_index)
    }

    pub fn insert_stitch_point_at(
        &self,
        obj_index: i32,
        kind: i32,
        block_index: i32,
        after_index: i32,
        x: f32,
        y: f32,
    ) -> bool {
        ffi::insert_stitch_point_at(obj_index, kind, block_index, after_index, x, y)
    }

    pub fn delete_stitch_point(
        &self,
        obj_index: i32,
        kind: i32,
        block_index: i32,
        point_index: i32,
    ) -> bool {
        ffi::delete_stitch_point(obj_index, kind, block_index, point_index)
    }

    pub fn path_op(&self, obj_index: i32, path_index: i32, op: &str, value: f32) -> bool {
        ffi::path_op(obj_index, path_index, op, value)
    }

    pub fn set_param_num(&self, obj_index: i32, key: &str, value: f32) -> bool {
        ffi::set_param_num(obj_index, key, value)
    }

    pub fn set_param_bool(&self, obj_index: i32, key: &str, value: bool) -> bool {
        ffi::set_param_bool(obj_index, key, value)
    }

    pub fn set_param_str(&self, obj_index: i32, key: &str, value: &str) -> bool {
        ffi::set_param_str(obj_index, key, value)
    }

    pub fn thumbnail_png(&self, wmax: i32, hmax: i32, index: i32) -> Vec<u8> {
        ffi::get_thumbnail_png(wmax, hmax, index)
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    fn setup_resources(eng: &Engine) {
        let res = concat!(env!("CARGO_MANIFEST_DIR"), "/resources");
        eng.set_resource_path(res);
    }

    /// Load the PPEF-text fixture and return the index of the PPEF Text object.
    fn load_ppef_fixture(eng: &Engine) -> i32 {
        setup_resources(eng);
        let bytes = std::fs::read(concat!(
            env!("CARGO_MANIFEST_DIR"),
            "/../testdata/ppef-text.ppes"
        ))
        .expect("missing testdata/ppef-text.ppes");
        eng.new_document();
        assert!(eng.load_ppes(&bytes), "loadPPES failed");
        (0..eng.object_count())
            .find(|&i| eng.object_snapshot(i).object_type == "PPEF Text")
            .expect("fixture has no PPEF Text object")
    }

    fn bbox(eng: &Engine, idx: i32) -> (f32, f32) {
        let s = eng.object_snapshot(idx);
        (s.width, s.height)
    }

    fn param_json(eng: &Engine, idx: i32) -> serde_json::Value {
        serde_json::from_str(&eng.parameter_json(idx)).unwrap()
    }

    #[test]
    fn ppef_update_is_idempotent() {
        with_engine(|eng| {
            let idx = load_ppef_fixture(eng);
            let (w0, h0) = bbox(eng, idx);
            for round in 0..3 {
                assert!(eng.update_ppef_text(idx), "update failed");
                let (w, h) = bbox(eng, idx);
                assert!(
                    (w - w0).abs() / w0 < 0.05 && (h - h0).abs() / h0 < 0.05,
                    "round {round}: bbox drifted {w0}x{h0} -> {w}x{h}"
                );
            }
        });
    }

    #[test]
    fn ppef_font_size_scales_bbox() {
        with_engine(|eng| {
            let idx = load_ppef_fixture(eng);
            let size0 = param_json(eng, idx)["fontSize"].as_f64().unwrap() as f32;
            assert!(eng.update_ppef_text(idx));
            let (_, h0) = bbox(eng, idx);

            eng.set_param_num(idx, "fontSize", size0 * 2.0);
            assert!(eng.update_ppef_text(idx));
            let (_, h1) = bbox(eng, idx);
            let ratio = h1 / h0;
            assert!(
                (ratio - 2.0).abs() < 0.2,
                "doubling font size {size0} should ~double height: {h0} -> {h1} (ratio {ratio})"
            );

            // back to original — no cumulative drift
            eng.set_param_num(idx, "fontSize", size0);
            assert!(eng.update_ppef_text(idx));
            let (_, h2) = bbox(eng, idx);
            assert!(
                (h2 - h0).abs() / h0 < 0.05,
                "size restore drifted: {h0} -> {h2}"
            );
        });
    }

    #[test]
    fn ttf_update_regenerates_and_scales() {
        with_engine(|eng| {
            setup_resources(eng);
            let bytes = std::fs::read(concat!(
                env!("CARGO_MANIFEST_DIR"),
                "/../testdata/ttf-text.ppes"
            ))
            .expect("missing testdata/ttf-text.ppes");
            eng.new_document();
            assert!(eng.load_ppes(&bytes), "loadPPES failed");
            let idx = (0..eng.object_count())
                .find(|&i| eng.object_snapshot(i).object_type == "TTF Text")
                .expect("fixture has no TTF Text object");

            // idempotent re-shape
            assert!(eng.update_ttf_text(idx), "update_ttf_text failed");
            let (_, h0) = bbox(eng, idx);
            for _ in 0..2 {
                assert!(eng.update_ttf_text(idx));
                let (_, h) = bbox(eng, idx);
                assert!((h - h0).abs() / h0 < 0.05, "drifted {h0} -> {h}");
            }

            // font size doubles height, restore returns
            let size0 = param_json(eng, idx)["fontSize"].as_f64().unwrap() as f32;
            eng.set_param_num(idx, "fontSize", size0 * 2.0);
            assert!(eng.update_ttf_text(idx));
            let (_, h1) = bbox(eng, idx);
            let ratio = h1 / h0;
            assert!((ratio - 2.0).abs() < 0.2, "ratio {ratio} ({h0} -> {h1})");
            eng.set_param_num(idx, "fontSize", size0);
            assert!(eng.update_ttf_text(idx));
            let (_, h2) = bbox(eng, idx);
            assert!((h2 - h0).abs() / h0 < 0.05, "restore drifted {h0} -> {h2}");

            // text change regenerates (width changes for longer text)
            let (w0, _) = bbox(eng, idx);
            eng.set_param_str(idx, "text", "Hello World Wide");
            assert!(eng.update_ttf_text(idx));
            let (w1, _) = bbox(eng, idx);
            assert!(w1 > w0, "longer text should widen bbox: {w0} -> {w1}");
        });
    }

    #[test]
    fn stitch_data_has_runs() {
        with_engine(|eng| {
            let idx = load_ppef_fixture(eng);
            let _ = idx;
            let d = eng.stitch_data(-1);
            // sanity: real stitches exist, split into many short runs
            assert!(d.total_points > 100, "too few stitches: {}", d.total_points);
            assert!(!d.segments.is_empty(), "no stitch segments");
            let pts: u32 = d.segments.iter().map(|s| s.count).sum();
            assert!(pts <= d.total_points);
            let avg = pts as f32 / d.segments.len() as f32;
            eprintln!(
                "stitch_data: {} pts, {} segments, avg {:.1} pts/seg",
                d.total_points,
                d.segments.len(),
                avg
            );
        });
    }

    #[test]
    fn ppef_border_toggle() {
        with_engine(|eng| {
            let idx = load_ppef_fixture(eng);
            assert!(eng.update_ppef_text(idx));
            let paths0 = eng.path_infos(idx).len();
            let (_, h0) = bbox(eng, idx);

            eng.set_param_bool(idx, "border", true);
            assert!(eng.update_ppef_text(idx), "update with border failed");
            let paths1 = eng.path_infos(idx).len();
            assert!(
                paths1 > paths0,
                "border on should add paths: {paths0} -> {paths1}"
            );

            eng.set_param_bool(idx, "border", false);
            assert!(eng.update_ppef_text(idx));
            let paths2 = eng.path_infos(idx).len();
            let (_, h2) = bbox(eng, idx);
            assert_eq!(paths2, paths0, "border off should restore path count");
            assert!(
                (h2 - h0).abs() / h0 < 0.05,
                "border toggle drifted bbox: {h0} -> {h2}"
            );
        });
    }

    #[test]
    fn path_node_move_shifts_anchor() {
        with_engine(|eng| {
            setup_resources(eng);
            let bytes = std::fs::read(concat!(
                env!("CARGO_MANIFEST_DIR"),
                "/../testdata/ttf-text.ppes"
            ))
            .expect("missing testdata/ttf-text.ppes");
            eng.new_document();
            assert!(eng.load_ppes(&bytes), "loadPPES failed");

            // an object with editable vector paths (TTF text glyph outline)
            let idx = (0..eng.object_count())
                .find(|&i| !eng.path_infos(i).is_empty())
                .expect("no object with editable paths");
            // fixture is unrotated, so a world delta maps 1:1 to the anchor
            assert_eq!(eng.object_snapshot(idx).rotate_degree, 0.0);

            let nodes0 = eng.path_nodes(idx, 0);
            assert!(!nodes0.is_empty(), "path 0 has no nodes");
            let ni = nodes0
                .iter()
                .position(|n| n.node_type != 7) // skip _close
                .expect("only close nodes");
            let (x0, y0) = (nodes0[ni].x, nodes0[ni].y);
            let stitches0 = eng.stitch_data(idx).total_points;
            assert!(stitches0 > 0, "fixture has no stitches to begin with");

            assert!(
                eng.move_path_node(idx, 0, ni as i32, 123.0, -45.0),
                "move_path_node failed"
            );

            let nodes1 = eng.path_nodes(idx, 0);
            assert_eq!(nodes1.len(), nodes0.len(), "node count changed");
            let (x1, y1) = (nodes1[ni].x, nodes1[ni].y);
            assert!(
                (x1 - (x0 + 123.0)).abs() < 0.5 && (y1 - (y0 - 45.0)).abs() < 0.5,
                "anchor did not move as expected: ({x0},{y0}) -> ({x1},{y1})"
            );
            // the edit must not wipe the stitches (regression: "ลายปักหาย")
            let stitches1 = eng.stitch_data(idx).total_points;
            assert!(
                stitches1 > 0,
                "stitches vanished after node move: {stitches0} -> {stitches1}"
            );
        });
    }

    /// Regression for "ลายปักหาย": editing a PPEF-text node must regenerate
    /// via applyPPEFFill (not applyFill, which wipes the fill to 0 stitches).
    #[test]
    fn ppef_node_move_keeps_stitches() {
        with_engine(|eng| {
            let idx = load_ppef_fixture(eng);
            assert!(eng.update_ppef_text(idx));
            let s0 = eng.stitch_data(idx).total_points;
            assert!(s0 > 0, "fixture has no stitches to begin with");

            let nodes = eng.path_nodes(idx, 0);
            assert!(!nodes.is_empty(), "PPEF path 0 has no nodes");
            let ni = nodes.iter().position(|n| n.node_type != 7).unwrap_or(0);

            assert!(eng.move_path_node(idx, 0, ni as i32, 20.0, 0.0), "move failed");
            let s1 = eng.stitch_data(idx).total_points;
            assert!(
                s1 > 0,
                "PPEF stitches vanished after node move: {s0} -> {s1}"
            );
        });
    }

    /// In this engine a _quadBezierTo (node_type 4) stores its START point in
    /// cp1, which must equal the previous command's endpoint. Edits that don't
    /// maintain this gap the curve.
    fn quad_start_invariant_holds(nodes: &[PathNode]) -> bool {
        for i in 1..nodes.len() {
            if nodes[i].node_type == 4 {
                let p = &nodes[i - 1];
                if (nodes[i].cp1x - p.x).abs() > 0.5 || (nodes[i].cp1y - p.y).abs() > 0.5 {
                    return false;
                }
            }
        }
        true
    }

    #[test]
    fn ttf_quad_edits_preserve_start_invariant() {
        with_engine(|eng| {
            setup_resources(eng);
            let bytes = std::fs::read(concat!(
                env!("CARGO_MANIFEST_DIR"),
                "/../testdata/ttf-text.ppes"
            ))
            .expect("missing testdata/ttf-text.ppes");
            eng.new_document();
            assert!(eng.load_ppes(&bytes), "loadPPES failed");
            let idx = (0..eng.object_count())
                .find(|&i| eng.object_snapshot(i).object_type == "TTF Text")
                .expect("no TTF Text object");
            assert!(eng.update_ttf_text(idx));

            let n0 = eng.path_nodes(idx, 0);
            assert!(
                n0.iter().any(|n| n.node_type == 4),
                "TTF fixture path 0 has no quads to exercise"
            );
            assert!(quad_start_invariant_holds(&n0), "initial invariant violated");

            // INSERT on a quad segment → new quad must start at the split point
            let k = n0
                .iter()
                .enumerate()
                .position(|(j, n)| j >= 1 && n.node_type == 4)
                .expect("no quad segment");
            assert!(eng.insert_path_node(idx, 0, k as i32, 0.5), "insert failed");
            let n1 = eng.path_nodes(idx, 0);
            assert_eq!(n1.len(), n0.len() + 1);
            assert!(quad_start_invariant_holds(&n1), "invariant broken after insert");
            assert!(eng.stitch_data(idx).total_points > 0, "stitches vanished");

            // MOVE the split node (its successor is a quad whose start must track)
            assert!(eng.move_path_node(idx, 0, k as i32, 15.0, 7.0), "move failed");
            let n2 = eng.path_nodes(idx, 0);
            assert!(quad_start_invariant_holds(&n2), "invariant broken after move");

            // DELETE an interior node → following quad's start must be repaired
            let dk = n2
                .iter()
                .enumerate()
                .position(|(j, n)| {
                    j >= 1 && j + 1 < n2.len() && n.node_type != 0 && n.node_type != 7
                })
                .expect("no deletable interior node");
            assert!(eng.delete_path_node(idx, 0, dk as i32), "delete failed");
            let n3 = eng.path_nodes(idx, 0);
            assert!(quad_start_invariant_holds(&n3), "invariant broken after delete");
            assert!(eng.stitch_data(idx).total_points > 0, "stitches vanished");
        });
    }

    #[test]
    fn ppef_node_insert_keeps_stitches_adds_node() {
        with_engine(|eng| {
            let idx = load_ppef_fixture(eng);
            assert!(eng.update_ppef_text(idx));
            let s0 = eng.stitch_data(idx).total_points;
            assert!(s0 > 0, "fixture has no stitches");
            let n0 = eng.path_nodes(idx, 0);
            // a segment end command: not the leading moveTo(0), not close(7)
            let k = n0
                .iter()
                .enumerate()
                .position(|(j, n)| j >= 1 && n.node_type != 0 && n.node_type != 7)
                .expect("no interpolatable segment");
            assert!(eng.insert_path_node(idx, 0, k as i32, 0.5), "insert failed");
            let n1 = eng.path_nodes(idx, 0);
            assert_eq!(n1.len(), n0.len() + 1, "insert should add exactly one node");
            let s1 = eng.stitch_data(idx).total_points;
            assert!(s1 > 0, "PPEF stitches vanished after insert: {s0} -> {s1}");
        });
    }

    #[test]
    fn ppef_node_delete_keeps_stitches_removes_node() {
        with_engine(|eng| {
            let idx = load_ppef_fixture(eng);
            assert!(eng.update_ppef_text(idx));
            let s0 = eng.stitch_data(idx).total_points;
            assert!(s0 > 0);
            let n0 = eng.path_nodes(idx, 0);
            // an interior anchor with a predecessor and successor
            let k = n0
                .iter()
                .enumerate()
                .position(|(j, n)| {
                    j >= 1 && j + 1 < n0.len() && n.node_type != 0 && n.node_type != 7
                })
                .expect("no deletable interior node");
            assert!(eng.delete_path_node(idx, 0, k as i32), "delete failed");
            let n1 = eng.path_nodes(idx, 0);
            assert_eq!(n1.len(), n0.len() - 1, "delete should remove exactly one node");
            let s1 = eng.stitch_data(idx).total_points;
            assert!(s1 > 0, "PPEF stitches vanished after delete: {s0} -> {s1}");
        });
    }

    /// StitchEdit: moving/inserting/deleting a needle point edits the block
    /// in place (no regeneration) and keeps the object's stitches intact.
    #[test]
    fn stitch_point_edit_move_insert_delete() {
        with_engine(|eng| {
            let idx = load_ppef_fixture(eng);
            assert!(eng.update_ppef_text(idx));
            // fixture is unrotated, so world coords == stored vertices 1:1
            assert_eq!(eng.object_snapshot(idx).rotate_degree, 0.0);

            let blocks0 = eng.stitch_points(idx);
            assert!(!blocks0.is_empty(), "no stitch blocks to edit");
            // the richest block (most points) — kind/index address it stably
            let b = blocks0
                .iter()
                .max_by_key(|b| b.points.len())
                .expect("no blocks");
            assert!(b.points.len() >= 3, "block too small to exercise");
            let (kind, block) = (b.kind, b.block_index);
            let n0 = b.points.len();
            let s0 = eng.stitch_data(idx).total_points;
            let (x1, y1) = (b.points[1].x, b.points[1].y);

            // MOVE point 1
            assert!(eng.move_stitch_point(idx, kind, block, 1, 50.0, -30.0));
            let after = |eng: &Engine| {
                eng.stitch_points(idx)
                    .into_iter()
                    .find(|x| x.kind == kind && x.block_index == block)
                    .expect("block vanished")
            };
            let bm = after(eng);
            assert_eq!(bm.points.len(), n0, "move must not change count");
            assert!(
                (bm.points[1].x - (x1 + 50.0)).abs() < 0.5
                    && (bm.points[1].y - (y1 - 30.0)).abs() < 0.5,
                "point did not move as expected"
            );
            assert_eq!(
                eng.stitch_data(idx).total_points,
                s0,
                "move must not add/drop stitches"
            );

            // INSERT near point 1 → exactly one more point in the block
            assert!(eng.insert_stitch_point(idx, kind, block, 1));
            assert_eq!(after(eng).points.len(), n0 + 1, "insert should add one");

            // DELETE point 1 → back to original count
            assert!(eng.delete_stitch_point(idx, kind, block, 1));
            assert_eq!(after(eng).points.len(), n0, "delete should remove one");
            assert!(eng.stitch_data(idx).total_points > 0, "stitches vanished");

            // INSERT-AT a world position after point 0 (double-click on a line)
            let b0 = after(eng);
            let (tx, ty) = (b0.points[0].x + 5.0, b0.points[0].y - 7.0);
            assert!(eng.insert_stitch_point_at(idx, kind, block, 0, tx, ty));
            let bz = after(eng);
            assert_eq!(bz.points.len(), n0 + 1, "insert_at should add one");
            assert!(
                (bz.points[1].x - tx).abs() < 0.5 && (bz.points[1].y - ty).abs() < 0.5,
                "insert_at point not placed at the requested world position"
            );
        });
    }

    #[test]
    fn load_pes_roundtrip() {
        let sample = std::env::var("PES_SAMPLE")
            .unwrap_or_else(|_| "../testdata/sample.pes".to_string());
        let bytes = std::fs::read(&sample)
            .unwrap_or_else(|e| panic!("cannot read sample {sample}: {e}"));

        with_engine(|eng| {
            eng.new_document();
            assert_eq!(eng.object_count(), 0, "new document should be empty");

            assert!(eng.import_pes(&bytes), "import_pes failed");
            assert_eq!(eng.object_count(), 1);

            let snap = eng.object_snapshot(0);
            assert!(snap.width > 0.0 && snap.height > 0.0, "empty bbox: {snap:?}");
            assert!(snap.visible);

            let png = eng.object_image_png(0);
            assert!(!png.is_empty(), "object image PNG is empty");
            assert_eq!(&png[1..4], b"PNG");

            let exported = eng.export_as("PES");
            assert!(!exported.is_empty(), "PES export is empty");
            assert_eq!(&exported[..4], b"#PES", "PES magic missing");
        });
    }
}
