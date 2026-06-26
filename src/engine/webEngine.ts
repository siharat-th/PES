// Browser engine transport: loads the embind wasm module (pes_web) and exposes
// an invoke() shaped exactly like the Tauri command layer, so EngineClient can
// run unchanged in a browser. The module is built by scripts/build-web.sh into
// public/wasm/ and served at /wasm by vite.
import type { DocumentSnapshot } from "./types";

export interface PesModule {
  pes_call(cmd: string, argsJson: string): string;
  load_input(kind: string, bytes: Uint8Array): string;
  object_png(index: number): Uint8Array;
  export_bytes(format: string): Uint8Array;
  set_resource_path(path: string): void;
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

/** Tauri-shaped invoke backed by the wasm engine. */
export async function webInvoke<T = unknown>(
  cmd: string,
  args?: Record<string, unknown>,
): Promise<T> {
  const m = await loadPesModule();
  const a = args ?? {};
  switch (cmd) {
    case "get_object_image": {
      const u8 = m.object_png((a.index as number) ?? 0);
      // object_png returns a fresh JS Uint8Array sized exactly to the PNG.
      return u8.buffer as unknown as T;
    }
    case "open_file":
      throw new Error("เปิดไฟล์บนเว็บ: ใช้ openDocumentBytes()");
    case "export_file":
      throw new Error("บันทึกไฟล์บนเว็บ: ใช้ exportDocumentBytes()");
    case "list_ppef_fonts":
    case "list_ttf_fonts":
      return [] as unknown as T; // fonts arrive in the web text/font phase
    default:
      return parse<T>(m.pes_call(cmd, JSON.stringify(a)));
  }
}

/** Web file-open: load raw bytes of a picked file into the engine. */
export async function webLoadInput(
  kind: "ppes" | "pes" | "svg",
  bytes: Uint8Array,
): Promise<DocumentSnapshot> {
  const m = await loadPesModule();
  return parse<DocumentSnapshot>(m.load_input(kind, bytes));
}

/** Web file-save: get exported bytes for a format (PES/PPES/...). */
export async function webExportBytes(format: string): Promise<Uint8Array> {
  const m = await loadPesModule();
  return m.export_bytes(format);
}
