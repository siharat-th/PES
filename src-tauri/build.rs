use std::path::PathBuf;

fn main() {
    tauri_build::build();

    let manifest = PathBuf::from(env!("CARGO_MANIFEST_DIR"));

    // PRISTINE upstream Skia lives in-repo as a git submodule (third_party/skia),
    // built by scripts/build-skia.sh into out/<platform>-<arch>-release. Skia's
    // source is NEVER modified; the few fork Skia-core patches PES relied on are
    // re-implemented as standalone helpers under cpp/skia-ext (compiled below).
    // No external SkiaApps checkout is required anymore.
    let skia = manifest
        .join("../third_party/skia")
        .canonicalize()
        .expect("third_party/skia submodule missing; run: git submodule update --init");
    // Windows canonicalize() returns a \\?\ verbatim path; clang-cl can't combine
    // that prefix with forward-slash relative includes (e.g. "include/core/..."),
    // so strip it back to a plain Z:\... path.
    let skia = {
        let s = skia.to_string_lossy();
        PathBuf::from(s.strip_prefix(r"\\?\").unwrap_or(&s).to_string())
    };

    let out_dir = if cfg!(target_os = "macos") {
        let arch = if cfg!(target_arch = "aarch64") { "arm64" } else { "x64" };
        skia.join(format!("out/macos-{arch}-release"))
    } else if cfg!(target_os = "windows") {
        skia.join("out/win-x64-release")
    } else {
        skia.join("out/linux-release")
    };
    assert!(
        out_dir.join("libskia.a").exists() || out_dir.join("skia.lib").exists(),
        "Skia not built at {out_dir:?}; run: scripts/build-skia.sh"
    );

    // Vendored third-party also kept in-repo (was pulled from the SkiaApps tree).
    let sqlitecpp = manifest.join("../third_party/sqlitecpp");

    let is_windows = cfg!(target_os = "windows");
    let clang_cl = std::env::var("CLANG_CL")
        .unwrap_or_else(|_| "C:/Program Files/LLVM/bin/clang-cl.exe".to_string());

    // sqlite3 amalgamation is C — compile it on its own (the C++ flags/defines the
    // bridge needs would mis-compile a large C TU). cc auto-emits its link line.
    {
        let mut c = cc::Build::new();
        c.file(sqlitecpp.join("sqlite3/sqlite3.c"))
            .include(sqlitecpp.join("sqlite3"))
            .warnings(false)
            .flag_if_supported("-Wno-return-type");
        if is_windows {
            c.compiler(&clang_cl).static_crt(true);
        }
        c.compile("sqlite3");
    }

    let mut bridge = cxx_build::bridge("src/engine.rs");
    bridge
        .file("cpp/pes_ffi.cpp")
        .file("cpp/pes_resources.cpp")
        // PES extensions over PRISTINE Skia: the few fork Skia-core patches PES
        // relies on, re-implemented as standalone helpers (Skia untouched).
        .file("cpp/skia-ext/pes_skpath_ext.cpp")
        .file("cpp/skia-ext/pes_png_ext.cpp")
        .file("cpp/skia-ext/pes_pathops_ext.cpp")
        // PPEF Thai text shaping (vendored from the old app layer).
        .file("cpp/ppef/PesPPEFUtils.cpp")
        .file("cpp/ppef/PesUnicodeUtils.cpp")
        // Vendored SQLiteCpp C++ wrappers (sqlite3.c compiled separately above).
        .file(sqlitecpp.join("src/Backup.cpp"))
        .file(sqlitecpp.join("src/Column.cpp"))
        .file(sqlitecpp.join("src/Database.cpp"))
        .file(sqlitecpp.join("src/Exception.cpp"))
        .file(sqlitecpp.join("src/Statement.cpp"))
        .file(sqlitecpp.join("src/Transaction.cpp"));
    // The pes engine, compiled from our in-repo copy. This is what lets us edit
    // the PPES format here and recompile with `cargo build`. Source list mirrors
    // modules/pes/pes.gni; headers come from cpp/pes/include so every TU — facade,
    // PPEF utils, skia-ext, and engine — shares one Parameter/pesData layout.
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
        .include("cpp/ppef") // PesPPEFUtils.hpp + utf8cpp/
        .include(&skia) // pristine Skia headers: include/core/..., src/..., modules/...
        .include(sqlitecpp.join("include"))
        .include(sqlitecpp.join("sqlite3"))
        .flag_if_supported("-Wno-deprecated-declarations");
    // ABI/layout-affecting defines MUST match the GN build of libskia
    // (`gn desc <out> //:skia defines`), otherwise types like sk_sp are passed
    // with a different calling convention and crash at runtime. NOTE: these
    // govern the WHOLE pes engine (compiled above), not just the facade — they
    // must stay in lockstep with the prebuilt libskia.
    // Mirror `gn desc <out> //:skia defines` for Skia M150 (minus
    // SKIA_IMPLEMENTATION, which is build-Skia-only and controls SK_API export).
    // NOTE vs M112: NO SK_TRIVIAL_ABI (M150 dropped it), SK_SUPPORT_GPU -> SK_GANESH,
    // and SK_ENCODE_* / SK_CODEC_DECODES_* were renamed to SK_CODEC_ENCODES_* etc.
    for (k, v) in [
        ("NDEBUG", None),
        ("SK_GAMMA_APPLY_TO_A8", None),
        ("SK_ALLOW_STATIC_GLOBAL_INITIALIZERS", Some("1")),
        ("SK_GANESH", None),
        ("SK_GL", None),
        ("SK_ASSUME_GL", Some("1")),
        ("SK_SUPPORT_PDF", None),
        ("SK_XML", None),
        ("SK_ENABLE_PRECOMPILE", None),
        ("SK_HAS_WUFFS_LIBRARY", None),
        ("SK_USE_PERFETTO", None),
        ("SK_USE_PARTITION_ALLOC", None),
        ("SK_ENABLE_AVX512_OPTS", None),
        ("GPU_TEST_UTILS", Some("1")),
        ("SK_ENABLE_ANDROID_UTILS", None),
        ("SK_CODEC_DECODES_BMP", None),
        ("SK_CODEC_DECODES_WBMP", None),
        ("SK_CODEC_DECODES_GIF", None),
        ("SK_CODEC_DECODES_ICO", None),
        ("SK_CODEC_DECODES_PNG", None),
        ("SK_CODEC_DECODES_PNG_WITH_LIBPNG", None),
        ("SK_CODEC_ENCODES_PNG", None),
        ("SK_CODEC_ENCODES_PNG_WITH_LIBPNG", None),
        ("SK_CODEC_DECODES_JPEG", None),
        ("SK_CODEC_DECODES_JPEG_GAINMAPS", None),
        ("SK_CODEC_ENCODES_JPEG", None),
        ("SK_CODEC_DECODES_WEBP", None),
        ("SK_CODEC_ENCODES_WEBP", None),
        ("SK_CODEC_DECODES_RAW", None),
    ] {
        bridge.define(k, v);
    }
    if is_windows {
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
        bridge
            .compiler(&clang_cl)
            .static_crt(true)
            .flag("/std:c++17")
            .flag("/bigobj")
            .flag_if_supported("-Wno-unknown-attributes");
    } else {
        // macOS/Linux: Metal backend + Apple availability + CoreText fontmgr.
        for (k, v) in [
            ("SK_METAL", None),
            ("SK_ENABLE_API_AVAILABLE", None),
            ("SK_TYPEFACE_FACTORY_CORETEXT", None),
            ("SK_FONTMGR_CORETEXT_AVAILABLE", None),
        ] {
            bridge.define(k, v);
        }
        bridge.flag_if_supported("-std=c++17");
    }
    bridge.compile("pes_ffi");

    println!("cargo:rustc-link-search=native={}", out_dir.display());

    if is_windows {
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
        // Link every static archive Skia + deps produced (lib*.a). Names/splits
        // change between milestones (e.g. M150 split skunicode into _core/_icu and
        // added partition_alloc/perfetto), so globbing beats a hand-kept list;
        // macOS ld64 resolves across the set regardless of order.
        // (sqlitecpp is compiled from vendored source above, not linked here.)
        let mut names: Vec<String> = std::fs::read_dir(&out_dir)
            .expect("read out_dir")
            .filter_map(|e| e.ok())
            .filter_map(|e| {
                let p = e.path();
                let stem = p.file_stem()?.to_str()?;
                if p.extension().and_then(|x| x.to_str()) == Some("a")
                    && stem.starts_with("lib")
                {
                    Some(stem[3..].to_string())
                } else {
                    None
                }
            })
            .collect();
        names.sort();
        for n in &names {
            println!("cargo:rustc-link-lib=static={n}");
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
    println!("cargo:rerun-if-changed=cpp/pes_ffi_core.hpp");
    println!("cargo:rerun-if-changed=cpp/pes_edit_core.hpp");
    println!("cargo:rerun-if-changed=cpp/pes_text_core.hpp");
    println!("cargo:rerun-if-changed=cpp/pes_satin_core.hpp");
    println!("cargo:rerun-if-changed=cpp/pes_punch_core.hpp");
    println!("cargo:rerun-if-changed=cpp/pes/src");
    println!("cargo:rerun-if-changed=cpp/pes/include");
    println!("cargo:rerun-if-changed=cpp/skia-ext");
    println!("cargo:rerun-if-changed=cpp/ppef");
}
