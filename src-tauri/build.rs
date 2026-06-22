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
    // Windows canonicalize() returns a \\?\ verbatim path; clang-cl can't combine
    // that prefix with forward-slash relative includes (e.g. "include/core/..."),
    // so strip it back to a plain Z:\... path.
    let skiaapps = {
        let s = skiaapps.to_string_lossy();
        PathBuf::from(s.strip_prefix(r"\\?\").unwrap_or(&s).to_string())
    };

    let out_dir = if cfg!(target_os = "macos") {
        let arch = if cfg!(target_arch = "aarch64") { "arm64" } else { "x64" };
        skiaapps.join(format!("out/macos-{arch}-release"))
    } else if cfg!(target_os = "windows") {
        skiaapps.join("out/win-x64-release")
    } else {
        skiaapps.join("out/linux-release")
    };
    // Skia + third-party stay prebuilt; only the pes engine is compiled from
    // source in-repo (below), so the gate is libskia, not libpes.
    assert!(
        out_dir.join("libskia.a").exists() || out_dir.join("skia.lib").exists(),
        "libskia not built at {out_dir:?}; run: ninja -C <out> skia modules/pes:pes"
    );

    let apps2_utils = skiaapps.join("apps2/1080_PES5Template/src/Utils");
    let mut bridge = cxx_build::bridge("src/engine.rs");
    bridge
        .file("cpp/pes_ffi.cpp")
        .file("cpp/pes_resources.cpp")
        // PPEF text shaping (native SQLiteCpp) from the old app layer
        .file(apps2_utils.join("PesPPEFUtils.cpp"))
        .file(apps2_utils.join("PesUnicodeUtils.cpp"));
    // The pes engine, compiled from our in-repo copy (formerly the prebuilt
    // libpes.a). This is what lets us edit the PPES format here and recompile
    // with `cargo build`. Source list mirrors ../SkiaApps/modules/pes/pes.gni;
    // headers come from cpp/pes/include (NOT the SkiaApps copy) so every TU —
    // facade, PPEF utils, and engine — shares one Parameter/pesData layout.
    for src in [
        "UnicodeHelper", "clipper", "pesAutoBranch", "pesBuffer", "pesClipper",
        "pesColor", "pesCubicSuperPath", "pesData", "pesDocument", "pesEMBClassify",
        "pesEMBFill", "pesEffect", "pesEncoder", "pesGcode", "pesMath", "pesPath",
        "pesPathUtility", "pesPolyline", "pesRectangle", "pesSVG", "pesSatinColumn",
        "pesSatinOutline", "pesSkPath", "pesStitchBlock", "pesUtility", "pesVec2f",
        "pugixml",
    ] {
        bridge.file(format!("cpp/pes/src/{src}.cpp"));
    }
    bridge
        .include("cpp")
        .include("cpp/pes/include") // our editable copy of the pes headers
        .include(&skiaapps) // Skia headers as include/core/..., modules/...
        .include(&apps2_utils)
        .include(skiaapps.join("third_party/sqlitecpp/include"))
        .include(skiaapps.join("third_party/sqlitecpp/sqlite3"))
        .flag_if_supported("-Wno-deprecated-declarations");
    // ABI/layout-affecting defines MUST match the GN build of libskia
    // (`gn desc <out> //:skia defines`), otherwise types like sk_sp are passed
    // with a different calling convention and crash at runtime. NOTE: these
    // govern the WHOLE pes engine (compiled above), not just the facade — they
    // must stay in lockstep with the prebuilt libskia.
    for (k, v) in [
        ("NDEBUG", None),
        ("SK_TRIVIAL_ABI", Some("[[clang::trivial_abi]]")),
        ("SK_GAMMA_APPLY_TO_A8", None),
        ("SK_ALLOW_STATIC_GLOBAL_INITIALIZERS", Some("1")),
        ("SK_GL", None),
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
    ] {
        bridge.define(k, v);
    }
    if cfg!(target_os = "windows") {
        // Windows libskia is GL-only (no Metal) and carries these extra defines
        // (`gn desc //:skia defines`). It is built with clang-cl + the static
        // CRT (/MT), so the facade/engine must use the same: MSVC cl.exe would
        // silently drop [[clang::trivial_abi]] and change the sk_sp calling
        // convention, and a dynamic CRT would clash with libskia's libcmt.
        for (k, v) in [
            ("GR_TEST_UTILS", Some("1")),
            ("SKVM_JIT_WHEN_POSSIBLE", None),
            ("_CRT_SECURE_NO_WARNINGS", None),
            ("WIN32_LEAN_AND_MEAN", None),
            ("NOMINMAX", None),
        ] {
            bridge.define(k, v);
        }
        let clang_cl = std::env::var("CLANG_CL")
            .unwrap_or_else(|_| "C:/Program Files/LLVM/bin/clang-cl.exe".to_string());
        bridge
            .compiler(clang_cl)
            .static_crt(true)
            .flag("/std:c++17")
            .flag("/bigobj")
            .flag_if_supported("-Wno-unknown-attributes");
    } else {
        // macOS/Linux: Metal backend + Apple availability shims, GNU-style flag.
        for (k, v) in [
            ("SK_METAL", None),
            ("SK_ASSUME_GL", Some("1")),
            ("SK_ENABLE_API_AVAILABLE", None),
        ] {
            bridge.define(k, v);
        }
        bridge.flag_if_supported("-std=c++17");
    }
    bridge.compile("pes_ffi");

    println!("cargo:rustc-link-search=native={}", out_dir.display());

    if cfg!(target_os = "windows") {
        // Link every static archive Skia + its third-party deps produced in the
        // out root (skia.lib, libpng.lib, harfbuzz.lib, ...). Linking the whole
        // set avoids hand-maintaining the transitive closure; the MSVC/lld
        // linker resolves across all of them regardless of order.
        let mut names: Vec<String> = std::fs::read_dir(&out_dir)
            .expect("read win out_dir")
            .filter_map(|e| e.ok())
            .filter_map(|e| {
                let p = e.path();
                if p.extension().and_then(|x| x.to_str()) == Some("lib") {
                    p.file_stem().and_then(|s| s.to_str()).map(str::to_string)
                } else {
                    None
                }
            })
            .collect();
        names.sort();
        for n in &names {
            println!("cargo:rustc-link-lib=static={n}");
        }
        // System libs Skia needs on Windows (`gn desc <out> //:skia libs`).
        for sys in [
            "Ole32", "OleAut32", "User32", "Usp10", "Gdi32", "OpenGL32",
            "FontSub", "Advapi32", "Shell32",
        ] {
            println!("cargo:rustc-link-lib=dylib={sys}");
        }
    } else {
        // Order roughly matters for some linkers: app code -> pes -> skia -> third-party.
        for lib in [
            "skia", "skshaper", "skunicode", "svg", "skresources",
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
    }

    println!("cargo:rerun-if-changed=cpp/pes_resources.cpp");
    println!("cargo:rerun-if-changed=cpp/pes_ffi.cpp");
    println!("cargo:rerun-if-changed=cpp/pes_ffi.h");
    println!("cargo:rerun-if-changed=cpp/pes/src");
    println!("cargo:rerun-if-changed=cpp/pes/include");
    println!("cargo:rerun-if-env-changed=SKIAAPPS_DIR");
}
