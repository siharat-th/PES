// Browser engine transport: loads the embind wasm module (pes_web) and exposes
// an invoke() shaped exactly like the Tauri command layer, so EngineClient can
// run unchanged in a browser. The module is built by scripts/build-web.sh into
// public/wasm/ and served at /wasm by vite.
import type { DocumentSnapshot } from "./types";

export interface PesModule {
  pes_call(cmd: string, argsJson: string): string;
  load_input(kind: string, bytes: Uint8Array): string;
  object_png(index: number, scale: number): Uint8Array;
  export_bytes(format: string): Uint8Array;
  set_resource_path(path: string): void;
  load_ppef_font(name: string, bytes: Uint8Array): string;
  load_ttf_font(name: string, bytes: Uint8Array): string;
}

type PesFactory = (opts: Record<string, unknown>) => Promise<PesModule>;

let modPromise: Promise<PesModule> | null = null;

// The engine wasm is rebuilt often during development (scripts/build-web.sh).
// Browsers keep the previously fetched/compiled module, so a stale wasm answers
// "unknown command" for newly added engine commands. In dev, bust the cache so
// each page load fetches the freshly built module; prod keeps cacheable URLs.
const CACHE_BUST = import.meta.env.DEV ? `?t=${Date.now()}` : "";

/** Inject the emscripten glue as a classic <script>. It lives in public/wasm
 *  (served verbatim) and can't go through vite's module graph, so we load it by
 *  tag — which sets the global factory `createPesModule`. */
function loadScript(src: string): Promise<void> {
  return new Promise((resolve, reject) => {
    const s = document.createElement("script");
    s.src = src;
    s.async = true;
    s.onload = () => resolve();
    s.onerror = () => reject(new Error(`failed to load ${src}`));
    document.head.appendChild(s);
  });
}

/** Load (once) and initialize the wasm engine module. */
export function loadPesModule(): Promise<PesModule> {
  if (!modPromise) {
    modPromise = (async () => {
      await loadScript("/wasm/pes_web.js" + CACHE_BUST);
      const factory = (globalThis as { createPesModule?: PesFactory })
        .createPesModule;
      if (!factory) throw new Error("createPesModule global missing");
      const m = await factory({
        locateFile: (p: string) => "/wasm/" + p + CACHE_BUST,
      });
      m.set_resource_path("/resources"); // stitch textures preloaded into MEMFS
      return m;
    })();
  }
  return modPromise;
}

function parse<T>(res: string): T {
  const v = JSON.parse(res);
  if (v && typeof v === "object" && "__error" in v) {
    throw new Error((v as { __error: string }).__error);
  }
  return v as T;
}

// ---- fonts (fetched on demand) ----------------------------------------------
// The wasm module preloads only stitch textures; fonts (.ppef 42MB / 136
// files, .ttf 209MB / 400+ files) are served under /resources/{PPEF,TTF} and
// written into MEMFS the first time the engine needs them (it answers
// {"missing_font": name, "font_kind": "ppef" | "ttf"}).
type FontKind = "ppef" | "ttf";
const FONT_DIR: Record<FontKind, string> = { ppef: "PPEF", ttf: "TTF" };
const loadedFonts = new Set<string>();

async function loadFont(
  m: PesModule,
  kind: FontKind,
  name: string,
): Promise<void> {
  const key = `${kind}:${name}`;
  if (loadedFonts.has(key)) return;
  const dir = FONT_DIR[kind];
  const res = await fetch(
    `/resources/${dir}/${encodeURIComponent(name)}.${kind}`,
  );
  if (!res.ok) {
    throw new Error(
      `โหลดฟอนต์ ${dir} "${name}" ไม่สำเร็จ (HTTP ${res.status})`,
    );
  }
  const bytes = new Uint8Array(await res.arrayBuffer());
  parse(kind === "ppef" ? m.load_ppef_font(name, bytes) : m.load_ttf_font(name, bytes));
  loadedFonts.add(key);
}

const fontLists: Partial<Record<FontKind, Promise<string[]>>> = {};

/** Font names from the manifest build-web.sh writes next to the fonts. */
function listFontsWeb(kind: FontKind): Promise<string[]> {
  let list = fontLists[kind];
  if (!list) {
    list = fetch(`/resources/${FONT_DIR[kind]}/fonts.json`)
      .then((r) => (r.ok ? (r.json() as Promise<string[]>) : []))
      .catch(() => []);
    fontLists[kind] = list;
  }
  return list;
}

/** Tauri-shaped invoke backed by the wasm engine. */
export async function webInvoke<T = unknown>(
  cmd: string,
  args?: Record<string, unknown>,
): Promise<T> {
  const m = await loadPesModule();
  const a = args ?? {};
  switch (cmd) {
    case "get_object_image": {
      const u8 = m.object_png((a.index as number) ?? 0, (a.scale as number) ?? 1);
      // object_png returns a fresh JS Uint8Array sized exactly to the PNG.
      return u8.buffer as unknown as T;
    }
    case "open_file":
      throw new Error("เปิดไฟล์บนเว็บ: ใช้ openDocumentBytes()");
    case "export_file":
      throw new Error("บันทึกไฟล์บนเว็บ: ใช้ exportDocumentBytes()");
    case "list_ppef_fonts":
      return listFontsWeb("ppef") as Promise<unknown> as Promise<T>;
    case "list_ttf_fonts":
      return listFontsWeb("ttf") as Promise<unknown> as Promise<T>;
    default: {
      const argsJson = JSON.stringify(a);
      let v = JSON.parse(m.pes_call(cmd, argsJson)) as unknown;
      // font-miss loop: fetch the .ppef/.ttf the engine asked for, retry the
      // same command (bounded — each round loads a different font or throws)
      for (let round = 0; round < 3; round++) {
        const miss =
          v && typeof v === "object" && "missing_font" in v
            ? (v as { missing_font: string; font_kind?: string })
            : null;
        if (!miss) break;
        await loadFont(
          m,
          miss.font_kind === "ttf" ? "ttf" : "ppef",
          miss.missing_font,
        );
        v = JSON.parse(m.pes_call(cmd, argsJson)) as unknown;
      }
      if (v && typeof v === "object" && "__error" in v) {
        throw new Error((v as { __error: string }).__error);
      }
      return v as T;
    }
  }
}

/** Web file-open: load raw bytes of a picked file into the engine.
 *  A .ppes may contain legacy PES2_TEXT placeholders the engine migrates to
 *  live PPEF text on load (see pescore::migratePes2TextObjects); that needs
 *  "Thai001.ppef", which — like all fonts — isn't preloaded into MEMFS. On a
 *  miss the engine re-parses from scratch next call, so retrying re-sends the
 *  same original bytes (same font-miss loop shape as webInvoke's default case). */
export async function webLoadInput(
  kind: "ppes" | "pes" | "svg",
  bytes: Uint8Array,
): Promise<DocumentSnapshot> {
  const m = await loadPesModule();
  let v = JSON.parse(m.load_input(kind, bytes)) as unknown;
  for (let round = 0; round < 3; round++) {
    const miss =
      v && typeof v === "object" && "missing_font" in v
        ? (v as { missing_font: string; font_kind?: string })
        : null;
    if (!miss) break;
    await loadFont(m, miss.font_kind === "ttf" ? "ttf" : "ppef", miss.missing_font);
    v = JSON.parse(m.load_input(kind, bytes)) as unknown;
  }
  if (v && typeof v === "object" && "__error" in v) {
    throw new Error((v as { __error: string }).__error);
  }
  return v as DocumentSnapshot;
}

/** Web file-save: get exported bytes for a format (PES/PPES/...). */
export async function webExportBytes(format: string): Promise<Uint8Array> {
  const m = await loadPesModule();
  return m.export_bytes(format);
}
