// Tracing core adapted from vtracer's cmdapp/src/converter.rs
// (https://github.com/visioncortex/vtracer, MIT License, (c) visioncortex).
// This mirrors vtracer's OWN colour clustering (color_precision +
// layer_difference) rather than posterizing to a fixed palette first — that is
// what gives the web app its clean, large, smooth regions. The thread-count
// reduction happens AFTER, in lib.rs, on the already-clean vector regions.
// Other changes vs upstream: output is per-cluster d-strings in absolute pixel
// coordinates instead of an SvgFile; no file I/O.

use visioncortex::color_clusters::{KeyingAction, Runner, RunnerConfig, HIERARCHICAL_MAX};
use visioncortex::{
    Color, ColorImage, CompoundPath, CompoundPathElement, PathSimplifyMode, PointF64, PointI32,
};

pub struct ClusterConfig {
    pub filter_speckle_area: usize,
    /// vtracer color_precision_loss = 8 - color_precision (web default 6 -> 2)
    pub color_precision_loss: i32,
    /// vtracer layer_difference / "gradient step" (web default 16)
    pub layer_difference: i32,
    /// radians
    pub corner_threshold: f64,
    pub length_threshold: f64,
    /// radians
    pub splice_threshold: f64,
    pub max_iterations: usize,
    pub cutout: bool,
    pub mode: PathSimplifyMode,
    pub path_precision: Option<u32>,
}

pub struct TracedCluster {
    pub color: Color,
    /// pixel count of the cluster
    pub area: usize,
    /// single SVG d-string, absolute pixel coordinates; holes are subpaths
    pub d: String,
}

fn color_exists_in_image(img: &ColorImage, color: Color) -> bool {
    for y in 0..img.height {
        for x in 0..img.width {
            let p = img.get_pixel(x, y);
            if p.r == color.r && p.g == color.g && p.b == color.b {
                return true;
            }
        }
    }
    false
}

// Deterministic replacement for vtracer's random search: the six saturated
// primaries, then the gray ramp (least likely to occur in a real photo).
fn find_unused_color_in_image(img: &ColorImage) -> Result<Color, String> {
    let special_colors = [
        Color::new(255, 0, 0),
        Color::new(0, 255, 0),
        Color::new(0, 0, 255),
        Color::new(255, 255, 0),
        Color::new(0, 255, 255),
        Color::new(255, 0, 255),
    ];
    let gray_ramp = (1u8..=254).map(|v| Color::new(v, v, v));
    for color in special_colors.into_iter().chain(gray_ramp) {
        if !color_exists_in_image(img, color) {
            return Ok(color);
        }
    }
    Err(String::from(
        "unable to find unused color in image to use as key",
    ))
}

/// Serialize a CompoundPath as ONE d-string in absolute pixel coordinates.
/// (CompoundPath::to_svg_string shifts everything relative to the first point
/// and returns a translate offset; serializing each element with a zero offset
/// keeps the coordinates absolute, which is what SkParsePath needs.)
pub fn compound_path_to_d(paths: &CompoundPath, precision: Option<u32>) -> String {
    paths
        .iter()
        .map(|p| match p {
            CompoundPathElement::PathI32(p) => {
                p.to_svg_string(true, &PointI32::default(), precision)
            }
            CompoundPathElement::PathF64(p) => {
                p.to_svg_string(true, &PointF64::default(), precision)
            }
            CompoundPathElement::Spline(p) => {
                p.to_svg_string(true, &PointF64::default(), precision)
            }
        })
        .collect::<String>()
}

/// Cluster + trace an RGBA image with vtracer's native gradient clustering.
/// Transparent pixels (alpha 0) are keyed out. Returns clusters bottom-up
/// (background first), each with its averaged colour and absolute-px d-string.
pub fn trace_regions(
    mut img: ColorImage,
    has_transparency: bool,
    cfg: &ClusterConfig,
) -> Result<Vec<TracedCluster>, String> {
    let width = img.width;
    let height = img.height;
    if width == 0 || height == 0 {
        return Ok(vec![]);
    }

    let key_color = if has_transparency {
        let key_color = find_unused_color_in_image(&img)?;
        for y in 0..height {
            for x in 0..width {
                if img.get_pixel(x, y).a == 0 {
                    img.set_pixel(x, y, &key_color);
                }
            }
        }
        key_color
    } else {
        // all-zero = visioncortex special value meaning "no keying"
        Color::default()
    };

    let runner = Runner::new(
        RunnerConfig {
            diagonal: cfg.layer_difference == 0,
            hierarchical: HIERARCHICAL_MAX,
            batch_size: 25600,
            good_min_area: cfg.filter_speckle_area,
            good_max_area: width * height,
            is_same_color_a: cfg.color_precision_loss,
            is_same_color_b: 1,
            deepen_diff: cfg.layer_difference,
            hollow_neighbours: 1,
            key_color,
            keying_action: if cfg.cutout {
                KeyingAction::Keep
            } else {
                KeyingAction::Discard
            },
        },
        img,
    );

    let mut clusters = runner.run();

    if cfg.cutout {
        let view = clusters.view();
        let image = view.to_color_image();
        let runner = Runner::new(
            RunnerConfig {
                diagonal: false,
                hierarchical: 64,
                batch_size: 25600,
                good_min_area: 0,
                good_max_area: image.width * image.height,
                is_same_color_a: 0,
                is_same_color_b: 1,
                deepen_diff: 0,
                hollow_neighbours: 0,
                key_color,
                keying_action: KeyingAction::Discard,
            },
            image,
        );
        clusters = runner.run();
    }

    let view = clusters.view();

    let mut out = Vec::new();
    for &cluster_index in view.clusters_output.iter().rev() {
        let cluster = view.get_cluster(cluster_index);
        let paths = cluster.to_compound_path(
            &view,
            false,
            cfg.mode,
            cfg.corner_threshold,
            cfg.length_threshold,
            cfg.max_iterations,
            cfg.splice_threshold,
        );
        let d = compound_path_to_d(&paths, cfg.path_precision);
        if d.is_empty() {
            continue;
        }
        out.push(TracedCluster {
            color: cluster.residue_color(),
            area: cluster.area(),
            d,
        });
    }
    Ok(out)
}
