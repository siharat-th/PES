use std::path::PathBuf;

fn main() {
    tauri_build::build();

    // Location of the SkiaApps tree (provides Skia + pesEngine headers and
    // the prebuilt static libs under out/<platform>-<arch>-release).
    let skiaapps = std::env::var("SKIAAPPS_DIR")
        .map(PathBuf::from)
        .unwrap_or_else(|_| {
            PathBuf::from(env!("CARGO_MANIFEST_DIR")).join("../../SkiaApps")
        });
    let skiaapps = skiaapps
        .canonicalize()
        .expect("SkiaApps tree not found; set SKIAAPPS_DIR");

    let out_dir = if cfg!(target_os = "macos") {
        let arch = if cfg!(target_arch = "aarch64") { "arm64" } else { "x64" };
        skiaapps.join(format!("out/macos-{arch}-release"))
    } else if cfg!(target_os = "windows") {
        skiaapps.join("out/win-x64-release")
    } else {
        skiaapps.join("out/linux-release")
    };
    assert!(
        out_dir.join("libpes.a").exists() || out_dir.join("pes.lib").exists(),
        "libpes not built at {out_dir:?}; run: ninja -C <out> skia modules/pes:pes"
    );

    let apps2_utils = skiaapps.join("apps2/1080_PES5Template/src/Utils");
    let mut bridge = cxx_build::bridge("src/engine.rs");
    bridge
        .file("cpp/pes_ffi.cpp")
        .file("cpp/pes_resources.cpp")
        // PPEF text shaping (native SQLiteCpp) from the old app layer
        .file(apps2_utils.join("PesPPEFUtils.cpp"))
        .file(apps2_utils.join("PesUnicodeUtils.cpp"))
        .include("cpp")
        .include(&skiaapps) // Skia headers as include/core/..., modules/...
        .include(skiaapps.join("modules/pes/include"))
        .include(&apps2_utils)
        .include(skiaapps.join("third_party/sqlitecpp/include"))
        .include(skiaapps.join("third_party/sqlitecpp/sqlite3"))
        .flag_if_supported("-std=c++17")
        .flag_if_supported("-Wno-deprecated-declarations");
    // ABI/layout-affecting defines MUST match the GN build of libskia/libpes
    // (`gn desc <out> //modules/pes:pes defines`), otherwise types like sk_sp
    // are passed with a different calling convention and crash at runtime.
    for (k, v) in [
        ("NDEBUG", None),
        ("SK_TRIVIAL_ABI", Some("[[clang::trivial_abi]]")),
        ("SK_GAMMA_APPLY_TO_A8", None),
        ("SK_ALLOW_STATIC_GLOBAL_INITIALIZERS", Some("1")),
        ("SK_GL", None),
        ("SK_METAL", None),
        ("SK_SUPPORT_GPU", Some("1")),
        ("SK_SUPPORT_PDF", None),
        ("SK_CODEC_DECODES_JPEG", None),
        ("SK_ENCODE_JPEG", None),
        ("SK_CODEC_DECODES_PNG", None),
        ("SK_ENCODE_PNG", None),
        ("SK_CODEC_DECODES_WEBP", None),
        ("SK_ENCODE_WEBP", None),
        ("SK_HAS_WUFFS_LIBRARY", None),
        ("SK_XML", None),
        ("SK_ENABLE_SKSL", None),
        ("SK_ENABLE_PRECOMPILE", None),
        ("SK_ASSUME_GL", Some("1")),
        ("SK_ENABLE_API_AVAILABLE", None),
    ] {
        bridge.define(k, v);
    }
    bridge.compile("pes_ffi");

    println!("cargo:rustc-link-search=native={}", out_dir.display());
    // Order roughly matters for some linkers: app code -> pes -> skia -> third-party.
    for lib in [
        "pes", "skia", "skshaper", "skunicode", "svg", "skresources",
        "harfbuzz", "icu", "expat", "png", "zlib", "jpeg", "webp",
        "webp_sse41", "skcms", "wuffs", "piex", "dng_sdk", "sqlitecpp",
    ] {
        let archive = out_dir.join(format!("lib{lib}.a"));
        if archive.exists() {
            println!("cargo:rustc-link-lib=static={lib}");
        }
    }

    if cfg!(target_os = "macos") {
        for fw in [
            "CoreFoundation", "CoreGraphics", "CoreText", "CoreServices",
            "Metal", "MetalKit", "Foundation", "QuartzCore",
        ] {
            println!("cargo:rustc-link-lib=framework={fw}");
        }
        println!("cargo:rustc-link-lib=c++");
    }

    println!("cargo:rerun-if-changed=cpp/pes_resources.cpp");
    println!("cargo:rerun-if-changed=cpp/pes_ffi.cpp");
    println!("cargo:rerun-if-changed=cpp/pes_ffi.h");
    println!("cargo:rerun-if-env-changed=SKIAAPPS_DIR");
}
