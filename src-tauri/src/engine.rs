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
        fn path_op(obj_index: i32, path_index: i32, op: &str, value: f32) -> bool;
        fn set_param_num(obj_index: i32, key: &str, value: f32) -> bool;
        fn set_param_bool(obj_index: i32, key: &str, value: bool) -> bool;
        fn set_param_str(obj_index: i32, key: &str, value: &str) -> bool;
    }
}

pub use ffi::{BrotherColor, ColorBlockInfo, ObjectSnapshot, PathInfo, StitchData};

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
