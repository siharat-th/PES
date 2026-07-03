//! PES Auto Punch tracer.
//!
//! Pipeline (matches the vtracer web app, then adds thread-count control):
//!   RGBA image -> vtracer native gradient clustering (color_precision +
//!   layer_difference) -> clean vector regions with averaged colours ->
//!   reduce those region colours to at most N thread colours (quantette,
//!   k-means in Oklab, area-weighted) -> per-colour SVG d-strings in absolute
//!   pixel coordinates, ready for SkParsePath::FromSVGString engine-side.
//!
//! Doing the colour reduction AFTER clustering (not a hard posterize before)
//! is what keeps regions large and smooth — a fixed-palette posterize of a
//! photo fragments every noisy patch into its own speckle.
//!
//! `trace_impl` is plain Rust (natively testable); `trace` is the
//! wasm-bindgen wrapper taking/returning JSON.

mod convert;

use convert::{trace_regions, ClusterConfig, TracedCluster};
use quantette::deps::palette::{IntoColor, Oklab, Srgb};
use quantette::{PaletteSize, Pipeline, QuantizeMethod};
use serde::{Deserialize, Serialize};
use visioncortex::{ColorImage, PathSimplifyMode};
use wasm_bindgen::prelude::*;

#[derive(Deserialize)]
#[serde(rename_all = "camelCase", default)]
pub struct TraceOptions {
    /// max thread colours after reduction; the UI offers 2..=16
    pub max_colors: u32,
    /// minimum cluster area in *work* pixels (the frontend converts mm²)
    pub despeckle_area_px: u32,
    /// vtracer color_precision (significant bits/channel, web default 6)
    pub color_precision: i32,
    /// vtracer layer_difference / gradient step (web default 16)
    pub layer_difference: i32,
    pub corner_threshold_deg: f64,
    /// "cutout" (default — regions don't overlap, stitched once) | "stacked"
    pub hierarchical: String,
    /// "spline" (default) | "polygon" | "none"
    pub mode: String,
    pub length_threshold: f64,
    pub splice_threshold_deg: f64,
    pub max_iterations: usize,
    pub path_precision: u32,
}

impl Default for TraceOptions {
    fn default() -> Self {
        Self {
            max_colors: 6,
            despeckle_area_px: 16,
            color_precision: 6,
            layer_difference: 16,
            corner_threshold_deg: 60.0,
            hierarchical: "cutout".into(),
            mode: "spline".into(),
            length_threshold: 4.0,
            splice_threshold_deg: 45.0,
            max_iterations: 10,
            path_precision: 2,
        }
    }
}

#[derive(Serialize)]
#[serde(rename_all = "camelCase")]
pub struct ColorLayer {
    /// "#rrggbb"
    pub rgb: String,
    /// total pixel count of this color's clusters (UI sorts/filters by it)
    pub area_px: u64,
    /// one d-string per traced cluster; holes are subpaths of the same string
    pub paths: Vec<String>,
}

#[derive(Serialize)]
#[serde(rename_all = "camelCase")]
pub struct TraceResult {
    pub width: u32,
    pub height: u32,
    /// bottom-up: colors[0] is the background-most layer — stitch in order.
    /// These are reduced to at most `max_colors` thread colours.
    pub colors: Vec<ColorLayer>,
    /// the FULL-resolution emergent clusters in their true colours (no
    /// reduction) — the "like the web app" reference preview, not embroidered
    pub web_colors: Vec<ColorLayer>,
}

fn deg2rad(deg: f64) -> f64 {
    deg / 180.0 * std::f64::consts::PI
}

fn srgb_to_oklab(c: Srgb<u8>) -> Oklab {
    let f: Srgb<f32> = c.into_format();
    f.into_color()
}

fn oklab_dist2(a: Oklab, b: Oklab) -> f32 {
    let dl = a.l - b.l;
    let da = a.a - b.a;
    let db = a.b - b.b;
    dl * dl + da * da + db * db
}

/// Reduce a set of area-weighted region colours to at most `max_colors`
/// distinct thread colours. Runs k-means (Oklab) on an area-weighted sample of
/// the region colours, then merges near-identical palette entries (ΔRGB < 8).
fn reduce_palette(clusters: &[TracedCluster], max_colors: u32) -> Result<Vec<Srgb<u8>>, String> {
    if clusters.is_empty() {
        return Ok(vec![]);
    }
    // Sample region colours weighted by SQRT of area (not area) so a small but
    // distinct subject — the dog's fur against a big grass background — still
    // earns palette slots instead of being swamped by the dominant colour.
    let total_w: f64 = clusters.iter().map(|c| (c.area.max(1) as f64).sqrt()).sum();
    let mut samples: Vec<Srgb<u8>> = Vec::new();
    for c in clusters {
        let col = Srgb::new(c.color.r, c.color.g, c.color.b);
        let frac = (c.area.max(1) as f64).sqrt() / total_w.max(1.0);
        let w = ((frac * 2048.0).round() as u64).clamp(1, 4096);
        for _ in 0..w {
            samples.push(col);
        }
    }

    let size = PaletteSize::try_from(max_colors.clamp(1, 64) as u16)
        .map_err(|e| format!("bad palette size: {e:?}"))?;
    let (palette, counts) = Pipeline::new()
        .palette_size(size)
        .quantize_method(QuantizeMethod::kmeans())
        .input_slice(&samples)
        .map_err(|e| format!("quantize input: {e:?}"))?
        .output_srgb8_palette_and_counts();

    // merge near-identical entries into the higher-count one
    let mut keep: Vec<(Srgb<u8>, u32)> = Vec::new();
    for (c, n) in palette.into_vec().into_iter().zip(counts.into_vec()) {
        let near = keep.iter_mut().find(|(k, _)| {
            let dr = k.red as f32 - c.red as f32;
            let dg = k.green as f32 - c.green as f32;
            let db = k.blue as f32 - c.blue as f32;
            (dr * dr + dg * dg + db * db).sqrt() < 8.0
        });
        match near {
            Some((k, kn)) => {
                if n > *kn {
                    *k = c;
                    *kn = n;
                }
            }
            None => keep.push((c, n)),
        }
    }
    Ok(keep.into_iter().map(|(c, _)| c).collect())
}

fn nearest_oklab(pal_ok: &[Oklab], c: &visioncortex::Color) -> usize {
    let ok = srgb_to_oklab(Srgb::new(c.r, c.g, c.b));
    let mut best = 0usize;
    let mut best_d = f32::MAX;
    for (i, &p) in pal_ok.iter().enumerate() {
        let d = oklab_dist2(ok, p);
        if d < best_d {
            best_d = d;
            best = i;
        }
    }
    best
}

pub fn trace_impl(
    rgba: &[u8],
    width: u32,
    height: u32,
    opts: &TraceOptions,
) -> Result<TraceResult, String> {
    let (w, h) = (width as usize, height as usize);
    if rgba.len() != w * h * 4 {
        return Err(format!(
            "buffer size {} does not match {}x{}x4",
            rgba.len(),
            width,
            height
        ));
    }
    let empty = TraceResult {
        width,
        height,
        colors: vec![],
        web_colors: vec![],
    };

    // 1. build the ColorImage from the raw (downscaled) pixels — NO posterize.
    //    Mark transparent pixels (alpha 0) so vtracer keys them out.
    let mut pixels = rgba.to_vec();
    let mut has_transparency = false;
    for px in pixels.chunks_exact_mut(4) {
        if px[3] < 128 {
            px[3] = 0;
            has_transparency = true;
        } else {
            px[3] = 255;
        }
    }
    let img = ColorImage {
        pixels,
        width: w,
        height: h,
    };

    // 2. vtracer native clustering -> clean regions with averaged colours
    let mode = match opts.mode.as_str() {
        "polygon" => PathSimplifyMode::Polygon,
        "none" => PathSimplifyMode::None,
        _ => PathSimplifyMode::Spline,
    };
    let cfg = ClusterConfig {
        filter_speckle_area: opts.despeckle_area_px as usize,
        color_precision_loss: (8 - opts.color_precision).clamp(0, 8),
        layer_difference: opts.layer_difference.max(0),
        corner_threshold: deg2rad(opts.corner_threshold_deg),
        length_threshold: opts.length_threshold,
        splice_threshold: deg2rad(opts.splice_threshold_deg),
        max_iterations: opts.max_iterations,
        cutout: opts.hierarchical != "stacked",
        mode,
        path_precision: Some(opts.path_precision),
    };
    let clusters: Vec<TracedCluster> = trace_regions(img, has_transparency, &cfg)?;
    if clusters.is_empty() {
        return Ok(empty);
    }

    // 2b. web reference: every emergent cluster in its TRUE colour, grouped by
    //     exact hex (first-seen bottom-up order). This is what vtracer's web app
    //     shows — full colour, no thread reduction.
    let mut web_index: std::collections::HashMap<u32, usize> = std::collections::HashMap::new();
    let mut web_colors: Vec<ColorLayer> = Vec::new();
    for cl in &clusters {
        let key = ((cl.color.r as u32) << 16) | ((cl.color.g as u32) << 8) | cl.color.b as u32;
        let idx = *web_index.entry(key).or_insert_with(|| {
            web_colors.push(ColorLayer {
                rgb: format!("#{:02x}{:02x}{:02x}", cl.color.r, cl.color.g, cl.color.b),
                area_px: 0,
                paths: vec![],
            });
            web_colors.len() - 1
        });
        web_colors[idx].area_px += cl.area as u64;
        web_colors[idx].paths.push(cl.d.clone());
    }

    // 3. reduce region colours to at most N thread colours, then group each
    //    region under its nearest thread colour (bottom-up first-seen order)
    let palette = reduce_palette(&clusters, opts.max_colors)?;
    if palette.is_empty() {
        return Ok(empty);
    }
    let pal_ok: Vec<Oklab> = palette.iter().map(|&c| srgb_to_oklab(c)).collect();

    let mut layers: Vec<Option<ColorLayer>> = (0..palette.len()).map(|_| None).collect();
    let mut order: Vec<usize> = Vec::new();
    for cl in &clusters {
        let idx = nearest_oklab(&pal_ok, &cl.color);
        let layer = layers[idx].get_or_insert_with(|| {
            order.push(idx);
            let p = palette[idx];
            ColorLayer {
                rgb: format!("#{:02x}{:02x}{:02x}", p.red, p.green, p.blue),
                area_px: 0,
                paths: vec![],
            }
        });
        layer.area_px += cl.area as u64;
        layer.paths.push(cl.d.clone());
    }
    let colors = order
        .into_iter()
        .filter_map(|i| layers[i].take())
        .collect();

    Ok(TraceResult {
        width,
        height,
        colors,
        web_colors,
    })
}

/// wasm entry point. `opts_json` may be "" or "{}" for defaults.
/// Returns TraceResult JSON, or `{"error": "..."}` on failure.
#[wasm_bindgen]
pub fn trace(rgba: &[u8], width: u32, height: u32, opts_json: &str) -> String {
    let opts: TraceOptions = if opts_json.trim().is_empty() {
        TraceOptions::default()
    } else {
        match serde_json::from_str(opts_json) {
            Ok(o) => o,
            Err(e) => return format!("{{\"error\":\"bad options: {}\"}}", e),
        }
    };
    match trace_impl(rgba, width, height, &opts) {
        Ok(res) => serde_json::to_string(&res)
            .unwrap_or_else(|e| format!("{{\"error\":\"serialize: {}\"}}", e)),
        Err(e) => serde_json::to_string(&serde_json::json!({ "error": e }))
            .unwrap_or_else(|_| "{\"error\":\"unknown\"}".into()),
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    fn make_rgba(w: usize, h: usize, f: impl Fn(usize, usize) -> [u8; 4]) -> Vec<u8> {
        let mut v = vec![0u8; w * h * 4];
        for y in 0..h {
            for x in 0..w {
                v[(y * w + x) * 4..(y * w + x) * 4 + 4].copy_from_slice(&f(x, y));
            }
        }
        v
    }

    #[test]
    fn two_colors_split() {
        let (w, h) = (64, 64);
        let rgba = make_rgba(w, h, |x, _| {
            if x < w / 2 {
                [200, 30, 30, 255]
            } else {
                [30, 30, 200, 255]
            }
        });
        let opts = TraceOptions {
            max_colors: 2,
            ..Default::default()
        };
        let res = trace_impl(&rgba, w as u32, h as u32, &opts).unwrap();
        assert_eq!(res.colors.len(), 2, "expected two color layers");
        for c in &res.colors {
            assert!(!c.paths.is_empty(), "layer {} has no paths", c.rgb);
            assert!(c.area_px > 1000, "layer {} too small: {}", c.rgb, c.area_px);
            assert!(c.paths[0].starts_with('M'), "d-string must be absolute: {}", c.paths[0]);
        }
    }

    #[test]
    fn donut_has_hole() {
        let (w, h) = (96, 96);
        let (cx, cy) = (48.0f64, 48.0f64);
        let rgba = make_rgba(w, h, |x, y| {
            let d = ((x as f64 - cx).powi(2) + (y as f64 - cy).powi(2)).sqrt();
            if (15.0..=30.0).contains(&d) {
                [10, 10, 10, 255]
            } else {
                [245, 245, 245, 255]
            }
        });
        let opts = TraceOptions {
            max_colors: 2,
            ..Default::default() // cutout
        };
        let res = trace_impl(&rgba, w as u32, h as u32, &opts).unwrap();
        assert_eq!(res.colors.len(), 2);
        let dark = res
            .colors
            .iter()
            .find(|c| c.rgb < "#800000".to_string())
            .expect("no dark layer");
        let m_count = dark.paths.iter().map(|d| d.matches('M').count()).sum::<usize>();
        assert!(
            m_count >= 2,
            "ring should carry a hole subpath, got {} M in {:?}",
            m_count,
            dark.paths
        );
    }

    #[test]
    fn transparent_background_dropped() {
        let (w, h) = (64, 64);
        let rgba = make_rgba(w, h, |x, y| {
            if (22..42).contains(&x) && (22..42).contains(&y) {
                [30, 180, 60, 255]
            } else {
                [0, 0, 0, 0]
            }
        });
        for hier in ["cutout", "stacked"] {
            let opts = TraceOptions {
                max_colors: 4,
                hierarchical: hier.into(),
                ..Default::default()
            };
            let res = trace_impl(&rgba, w as u32, h as u32, &opts).unwrap();
            assert_eq!(
                res.colors.len(),
                1,
                "{hier}: transparent bg should yield one layer, got {:?}",
                res.colors.iter().map(|c| &c.rgb).collect::<Vec<_>>()
            );
            assert!(res.colors[0].area_px >= 350 && res.colors[0].area_px <= 450);
        }
    }

    #[test]
    fn photo_reduces_to_target_colors() {
        // a smooth 3-band gradient + noise: native clustering should yield a
        // handful of big regions, reduced to <= max_colors threads
        let (w, h) = (80, 80);
        let rgba = make_rgba(w, h, |x, y| {
            let n = (((x * 7 + y * 13) % 11) as i32 - 5) as i32; // ±5 noise
            let base = if y < h / 3 {
                [200, 60, 60]
            } else if y < 2 * h / 3 {
                [60, 180, 90]
            } else {
                [70, 90, 200]
            };
            [
                (base[0] + n).clamp(0, 255) as u8,
                (base[1] + n).clamp(0, 255) as u8,
                (base[2] + n).clamp(0, 255) as u8,
                255,
            ]
        });
        let opts = TraceOptions {
            max_colors: 4,
            ..Default::default()
        };
        let res = trace_impl(&rgba, w as u32, h as u32, &opts).unwrap();
        assert!(
            res.colors.len() >= 3 && res.colors.len() <= 4,
            "expected 3-4 threads, got {}",
            res.colors.len()
        );
        // no speckle explosion: the biggest layer covers a real band
        let biggest = res.colors.iter().map(|c| c.area_px).max().unwrap();
        assert!(biggest > 1500, "regions too fragmented: max area {biggest}");
    }

    #[test]
    fn json_entry_point() {
        let (w, h) = (16, 16);
        let rgba = make_rgba(w, h, |_, _| [255, 0, 0, 255]);
        let out = trace(&rgba, w as u32, h as u32, "{\"maxColors\":2}");
        assert!(out.contains("\"colors\""), "bad output: {out}");
        assert!(!out.contains("\"error\""), "unexpected error: {out}");
    }
}
