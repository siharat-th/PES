import { useCallback, useEffect, useMemo, useRef, useState } from "react";
import {
  ImagePlus,
  Loader2,
  Sparkles,
  X,
} from "lucide-react";
import * as engine from "../engine/EngineClient";
import type { BrotherColor } from "../engine/types";
import { traceImage, type TraceResult } from "../punch/tracerClient";
import { useDocumentStore } from "../state/documentStore";
import { useUiStore } from "../state/uiStore";

// Auto Punch (แกะลายจากรูป): PNG/JPG → quantize → trace → per-color fill
// objects. Modal patterned on LibraryPanel. Tracing runs in the tracer worker
// (public/tracer/) so the UI never blocks; every param change re-traces the
// live preview after a short debounce, stale results are dropped by job seq.

interface SourceImage {
  el: HTMLImageElement;
  url: string; // object URL (revoked on replace/unmount)
  name: string;
}

/** work resolution: ~5 px/mm (0.2 mm/px ≈ half the minimum stitchable
 *  feature), capped so quantize+trace stays interactive */
const workDims = (img: HTMLImageElement, outWidthMm: number) => {
  const maxDim = Math.min(Math.max(Math.round(5 * outWidthMm), 256), 1024);
  const k = Math.min(maxDim / img.naturalWidth, maxDim / img.naturalHeight, 1);
  return {
    w: Math.max(1, Math.round(img.naturalWidth * k)),
    h: Math.max(1, Math.round(img.naturalHeight * k)),
  };
};

const parseHex = (hex: string) => {
  const v = parseInt(hex.replace("#", ""), 16);
  return { r: (v >> 16) & 0xff, g: (v >> 8) & 0xff, b: v & 0xff };
};

// sRGB hex -> Oklab [L, a, b]. Thread matching is done in Oklab (perceptual):
// plain RGB distance sends a cream to a gray and an olive-green to a brown.
const hexToOklab = (hex: string): [number, number, number] => {
  const { r, g, b } = parseHex(hex);
  const lin = (c: number) => {
    c /= 255;
    return c <= 0.04045 ? c / 12.92 : Math.pow((c + 0.055) / 1.055, 2.4);
  };
  const lr = lin(r),
    lg = lin(g),
    lb = lin(b);
  const l = Math.cbrt(0.4122214708 * lr + 0.5363325363 * lg + 0.0514459929 * lb);
  const m = Math.cbrt(0.2119034982 * lr + 0.6806995451 * lg + 0.1073969566 * lb);
  const s = Math.cbrt(0.0883024619 * lr + 0.2817188376 * lg + 0.6299787005 * lb);
  return [
    0.2104542553 * l + 0.793617785 * m - 0.0040720468 * s,
    1.9779984951 * l - 2.428592205 * m + 0.4505937099 * s,
    0.0259040371 * l + 0.7827717662 * m - 0.808675766 * s,
  ];
};

// Weight chroma (a, b) ~3x more than lightness (L). This keeps hue fidelity:
// a warm cream matches Beige not Gray, an olive-green matches Moss Green not
// Russet Brown, while genuinely desaturated tones still fall to a neutral.
const LIGHTNESS_WEIGHT = 0.35;
const oklabDist2 = (
  a: [number, number, number],
  b: [number, number, number],
) => {
  const dl = a[0] - b[0];
  const da = a[1] - b[1];
  const db = a[2] - b[2];
  return LIGHTNESS_WEIGHT * dl * dl + da * da + db * db;
};

const checker: React.CSSProperties = {
  backgroundImage:
    "linear-gradient(45deg,#e5e5e5 25%,transparent 25%,transparent 75%,#e5e5e5 75%),linear-gradient(45deg,#e5e5e5 25%,transparent 25%,transparent 75%,#e5e5e5 75%)",
  backgroundSize: "16px 16px",
  backgroundPosition: "0 0, 8px 8px",
};

export default function AutoPunchPanel() {
  const open = useUiStore((s) => s.punchOpen);
  const setPunchOpen = useUiStore((s) => s.setPunchOpen);
  const autoPunch = useDocumentStore((s) => s.autoPunch);
  const busy = useDocumentStore((s) => s.busy);
  const hoopWidthMm = useDocumentStore((s) => s.doc?.hoop_width_mm ?? 100);

  const [img, setImg] = useState<SourceImage | null>(null);
  const [palette, setPalette] = useState<BrotherColor[]>([]);
  const [maxColors, setMaxColors] = useState(6);
  const [outWidthMm, setOutWidthMm] = useState(() =>
    Math.round(hoopWidthMm * 0.6),
  );
  const [despeckleMm2, setDespeckleMm2] = useState(0.5);
  const [cornerDeg, setCornerDeg] = useState(60);
  const [stacked, setStacked] = useState(false);
  const [withBackground, setWithBackground] = useState(false);
  const [result, setResult] = useState<TraceResult | null>(null);
  const [tracing, setTracing] = useState(false);
  const [disabled, setDisabled] = useState<Set<number>>(new Set());
  const [error, setError] = useState<string | null>(null);
  // shared zoom/pan for all three previews (same scale + offset)
  const [view, setView] = useState({ scale: 1, tx: 0, ty: 0 });

  const seq = useRef(0);
  const fileRef = useRef<HTMLInputElement>(null);
  const gridRef = useRef<HTMLDivElement>(null);
  const drag = useRef<{ x: number; y: number; tx: number; ty: number } | null>(
    null,
  );
  const punchSeed = useUiStore((s) => s.punchSeed);
  const setPunchSeed = useUiStore((s) => s.setPunchSeed);

  useEffect(() => {
    if (open && palette.length === 0)
      void engine.getBrotherPalette().then(setPalette);
  }, [open, palette.length]);

  useEffect(() => {
    if (!open) return;
    const onEsc = (e: KeyboardEvent) => {
      if (e.key === "Escape") setPunchOpen(false);
    };
    window.addEventListener("keydown", onEsc);
    return () => window.removeEventListener("keydown", onEsc);
  }, [open, setPunchOpen]);

  const pickFile = useCallback((file: File) => {
    if (!/image\/(png|jpeg)/.test(file.type)) {
      setError("รองรับเฉพาะไฟล์ PNG / JPG");
      return;
    }
    const url = URL.createObjectURL(file);
    const el = new Image();
    el.onload = () => {
      setImg((prev) => {
        if (prev) URL.revokeObjectURL(prev.url);
        return { el, url, name: file.name };
      });
      setView({ scale: 1, tx: 0, ty: 0 }); // reset zoom for the new image
    };
    el.onerror = () => {
      URL.revokeObjectURL(url);
      setError("อ่านไฟล์รูปไม่สำเร็จ");
    };
    el.src = url;
    setError(null);
  }, []);

  // wheel-zoom as a NON-passive listener so preventDefault suppresses page
  // scroll; zooms toward the cursor and updates the shared view (all 3 panels)
  useEffect(() => {
    const grid = gridRef.current;
    if (!grid) return;
    const onWheel = (e: WheelEvent) => {
      const panel = (e.target as HTMLElement).closest("[data-view-panel]");
      if (!panel) return;
      e.preventDefault();
      const rect = panel.getBoundingClientRect();
      const cx = e.clientX - rect.left - rect.width / 2;
      const cy = e.clientY - rect.top - rect.height / 2;
      setView((v) => {
        const scale = Math.min(12, Math.max(1, v.scale * Math.exp(-e.deltaY * 0.0015)));
        const f = scale / v.scale;
        if (scale <= 1.001) return { scale: 1, tx: 0, ty: 0 };
        const maxX = (rect.width * (scale - 1)) / 2;
        const maxY = (rect.height * (scale - 1)) / 2;
        const nx = cx - f * (cx - v.tx);
        const ny = cy - f * (cy - v.ty);
        return {
          scale,
          tx: Math.min(maxX, Math.max(-maxX, nx)),
          ty: Math.min(maxY, Math.max(-maxY, ny)),
        };
      });
    };
    grid.addEventListener("wheel", onWheel, { passive: false });
    return () => grid.removeEventListener("wheel", onWheel);
  }, [open]); // re-attach when the dialog (and thus the grid) mounts

  // image handed over from an app-level drag-drop (App.tsx punchImage)
  useEffect(() => {
    if (!open || !punchSeed) return;
    const mime = punchSeed.name.toLowerCase().endsWith(".png")
      ? "image/png"
      : "image/jpeg";
    pickFile(
      new File([punchSeed.bytes as BlobPart], punchSeed.name, { type: mime }),
    );
    setPunchSeed(null);
  }, [open, punchSeed, pickFile, setPunchSeed]);

  // live preview: re-trace on image/param change (300ms debounce, drop stale)
  useEffect(() => {
    if (!open || !img) return;
    const job = ++seq.current;
    setTracing(true);
    const t = setTimeout(() => {
      try {
        const { w, h } = workDims(img.el, outWidthMm);
        const canvas = document.createElement("canvas");
        canvas.width = w;
        canvas.height = h;
        const ctx = canvas.getContext("2d")!;
        ctx.drawImage(img.el, 0, 0, w, h);
        const data = ctx.getImageData(0, 0, w, h);
        const pxPerMm = w / outWidthMm;
        const despeckleAreaPx = Math.max(
          1,
          Math.round(despeckleMm2 * pxPerMm * pxPerMm),
        );
        void traceImage(data.data.buffer as ArrayBuffer, w, h, {
          maxColors,
          despeckleAreaPx,
          cornerThresholdDeg: cornerDeg,
          hierarchical: stacked ? "stacked" : "cutout",
        })
          .then((res) => {
            if (seq.current !== job) return; // stale
            setResult(res);
            setDisabled(new Set());
            setError(null);
          })
          .catch((e) => {
            if (seq.current !== job) return;
            setError(`เทรซไม่สำเร็จ: ${e.message ?? e}`);
          })
          .finally(() => {
            if (seq.current === job) setTracing(false);
          });
      } catch (e) {
        setError(`เทรซไม่สำเร็จ: ${e}`);
        setTracing(false);
      }
    }, 300);
    return () => clearTimeout(t);
  }, [open, img, maxColors, outWidthMm, despeckleMm2, cornerDeg, stacked]);

  // Brother thread swatches in perceptual (Oklab) space, computed once.
  const paletteOk = useMemo(
    () => palette.map((p) => ({ p, ok: hexToOklab(p.hex) })),
    [palette],
  );

  // traced color → nearest Brother thread in weighted Oklab (display AND commit
  // use this index, so preview == result)
  const threads = useMemo(() => {
    if (!result || paletteOk.length === 0) return [];
    return result.colors.map((c) => {
      const ok = hexToOklab(c.rgb);
      let best = paletteOk[0].p;
      let bestD = Infinity;
      for (const { p, ok: pok } of paletteOk) {
        const d = oklabDist2(ok, pok);
        if (d < bestD) {
          bestD = d;
          best = p;
        }
      }
      return best;
    });
  }, [result, paletteOk]);

  const outHeightMm = useMemo(() => {
    if (!result) return 0;
    return Math.round((outWidthMm * result.height) / result.width);
  }, [result, outWidthMm]);

  const create = async () => {
    if (!result || !img) return;
    const objects = result.colors
      .map((c, i) => ({ c, i }))
      .filter(({ i }) => !disabled.has(i))
      .map(({ c, i }) => ({
        paths: c.paths,
        rgb: c.rgb,
        colorIndex: threads[i]?.index ?? -1,
        fillType: 1, // NORMAL — stitches baked immediately
      }));
    if (objects.length === 0) {
      setError("ไม่มีสีที่เลือกไว้");
      return;
    }
    let bg: string | undefined;
    if (withBackground) {
      // Background sizing rule: 1 px = 1 engine unit = 0.1 mm → raster the
      // source at exactly outWidthMm × 10 px. Engine requires PNG (PPES
      // round-trip scans for the PNG magic).
      const bw = Math.max(1, Math.round(outWidthMm * 10));
      const bh = Math.max(
        1,
        Math.round((bw * img.el.naturalHeight) / img.el.naturalWidth),
      );
      const canvas = document.createElement("canvas");
      canvas.width = bw;
      canvas.height = bh;
      canvas.getContext("2d")!.drawImage(img.el, 0, 0, bw, bh);
      bg = canvas.toDataURL("image/png").split(",")[1];
    }
    const ok = await autoPunch(
      {
        imageSize: [result.width, result.height],
        outputWidthMm: outWidthMm,
        groupName: "Auto Punch",
        fillDensity: 2.5,
        sewDirection: 0,
        objects,
      },
      bg,
    );
    if (ok) setPunchOpen(false);
  };

  // ---- shared zoom / pan for the three preview panels ----------------------
  const clamp = (n: number, lo: number, hi: number) =>
    Math.min(hi, Math.max(lo, n));

  const clampPan = (scale: number, tx: number, ty: number, rect: DOMRect) => {
    if (scale <= 1.001) return { scale: 1, tx: 0, ty: 0 };
    const maxX = (rect.width * (scale - 1)) / 2;
    const maxY = (rect.height * (scale - 1)) / 2;
    return { scale, tx: clamp(tx, -maxX, maxX), ty: clamp(ty, -maxY, maxY) };
  };

  const onViewPointerDown = (e: React.PointerEvent) => {
    if (!img || view.scale <= 1) return;
    e.currentTarget.setPointerCapture(e.pointerId);
    drag.current = { x: e.clientX, y: e.clientY, tx: view.tx, ty: view.ty };
  };
  const onViewPointerMove = (e: React.PointerEvent) => {
    if (!drag.current) return;
    const rect = e.currentTarget.getBoundingClientRect();
    setView((v) =>
      clampPan(
        v.scale,
        drag.current!.tx + (e.clientX - drag.current!.x),
        drag.current!.ty + (e.clientY - drag.current!.y),
        rect,
      ),
    );
  };
  const onViewPointerUp = () => {
    drag.current = null;
  };

  // wheel zoom is attached as a NON-passive native listener (see the effect
  // below) so preventDefault works; pointer/dblclick can stay React props.
  const viewHandlers = {
    onPointerDown: onViewPointerDown,
    onPointerMove: onViewPointerMove,
    onPointerUp: onViewPointerUp,
    onPointerCancel: onViewPointerUp,
    onDoubleClick: () => setView({ scale: 1, tx: 0, ty: 0 }),
  };
  const viewTransform: React.CSSProperties = {
    transform: `translate(${view.tx}px, ${view.ty}px) scale(${view.scale})`,
    transformOrigin: "center",
    cursor: view.scale > 1 ? (drag.current ? "grabbing" : "grab") : "default",
  };

  if (!open) return null;

  const sameThreadIndices = new Set(
    threads.filter((t, i) => threads.some((u, j) => j !== i && u.index === t.index)).map((t) => t.index),
  );

  // One traced SVG.
  //  "web"    → the FULL-resolution emergent clusters in their true colours
  //             (vtracer's raw output, like the web app) — reference only.
  //  "thread" → the reduced thread layers painted with their nearest Brother
  //             colour — the actual embroidery result (honours the color list's
  //             enable/disable).
  const tracedSvg = (mode: "web" | "thread") => {
    if (!result)
      return (
        <span className="text-xs text-neutral-300">
          {img ? "กำลังเทรซ…" : "ตัวอย่างลายจะแสดงที่นี่"}
        </span>
      );
    const layers =
      mode === "web"
        ? result.webColors.map((c) => ({ d: c.paths.join(" "), fill: c.rgb }))
        : result.colors.flatMap((c, i) =>
            disabled.has(i)
              ? []
              : [{ d: c.paths.join(" "), fill: threads[i]?.hex ?? c.rgb }],
          );
    return (
      <svg
        viewBox={`0 0 ${result.width} ${result.height}`}
        className="max-h-full max-w-full"
        style={{
          aspectRatio: `${result.width} / ${result.height}`,
          ...viewTransform,
        }}
      >
        {layers.map((l, i) => (
          <path
            key={i}
            d={l.d}
            fill={l.fill}
            // stroke = fill closes the sub-pixel gaps between adjacent cutout
            // regions (a preview-only artifact; real stitches fill solidly)
            stroke={l.fill}
            strokeWidth={0.6}
            fillRule="evenodd"
          />
        ))}
      </svg>
    );
  };

  const tracedPanel = (mode: "web" | "thread", label: string) => (
    <div
      data-view-panel
      className="relative flex touch-none select-none items-center justify-center overflow-hidden rounded-lg border border-neutral-200 bg-white"
      style={checker}
      {...viewHandlers}
    >
      {tracedSvg(mode)}
      {tracing && (
        <div className="absolute inset-0 flex items-center justify-center bg-white/60">
          <Loader2 size={24} className="animate-spin text-blue-600" />
        </div>
      )}
      <span className="absolute left-2 top-2 rounded bg-white/80 px-1.5 py-0.5 text-[10px] text-neutral-500">
        {label}
      </span>
    </div>
  );

  return (
    <div
      className="fixed inset-0 z-40 flex items-center justify-center bg-black/40 p-4 backdrop-blur-[2px]"
      onClick={() => setPunchOpen(false)}
    >
      <div
        className="flex h-[84vh] w-full max-w-5xl flex-col overflow-hidden rounded-xl bg-neutral-50 shadow-2xl ring-1 ring-black/10"
        onClick={(e) => e.stopPropagation()}
      >
        {/* header */}
        <div className="flex items-center gap-3 border-b border-neutral-200 bg-white px-4 py-2.5">
          <Sparkles size={16} className="text-blue-600" />
          <h2 className="text-sm font-semibold text-neutral-800">
            แกะลายจากรูป (Auto Punch)
          </h2>
          {img && (
            <span className="truncate text-xs text-neutral-400">{img.name}</span>
          )}
          <button
            onClick={() => setPunchOpen(false)}
            className="ml-auto rounded-md p-1.5 text-neutral-400 transition hover:bg-neutral-100 hover:text-neutral-700"
            aria-label="ปิด"
          >
            <X size={16} />
          </button>
        </div>

        <div className="flex min-h-0 flex-1">
          {/* previews: source | true (web) colours | Brother thread colours */}
          <div ref={gridRef} className="grid min-w-0 flex-1 grid-cols-3 gap-3 p-3">
            {/* source */}
            <div
              data-view-panel
              className="relative flex touch-none select-none items-center justify-center overflow-hidden rounded-lg border border-dashed border-neutral-300 bg-white"
              style={img ? checker : undefined}
              {...(img ? viewHandlers : {})}
            >
              {img ? (
                <img
                  src={img.url}
                  alt="ต้นฉบับ"
                  className="max-h-full max-w-full object-contain"
                  draggable={false}
                  style={viewTransform}
                />
              ) : (
                <button
                  onClick={() => fileRef.current?.click()}
                  className="flex flex-col items-center gap-2 p-8 text-neutral-400 transition hover:text-blue-600"
                >
                  <ImagePlus size={40} strokeWidth={1.2} />
                  <span className="text-sm">เลือกรูป PNG / JPG</span>
                  <span className="text-xs text-neutral-300">
                    หรือลากไฟล์มาวางที่นี่
                  </span>
                </button>
              )}
              <div
                className="absolute inset-0"
                onDragOver={(e) => e.preventDefault()}
                onDrop={(e) => {
                  e.preventDefault();
                  const f = e.dataTransfer.files?.[0];
                  if (f) pickFile(f);
                }}
                style={{ pointerEvents: img ? "none" : undefined }}
              />
              {img && (
                <button
                  onClick={() => fileRef.current?.click()}
                  className="absolute bottom-2 right-2 rounded-md bg-white/90 px-2 py-1 text-xs text-neutral-600 shadow ring-1 ring-black/10 transition hover:text-blue-600"
                >
                  เปลี่ยนรูป…
                </button>
              )}
              <span className="absolute left-2 top-2 rounded bg-white/80 px-1.5 py-0.5 text-[10px] text-neutral-500">
                ต้นฉบับ
              </span>
            </div>

            {/* full-colour vtracer output (like the web app), before reduction */}
            {tracedPanel(
              "web",
              result ? `สีจริง (เว็บ) · ${result.webColors.length} สี` : "สีจริง (เว็บ)",
            )}

            {/* Brother thread colours — the actual embroidery result */}
            {tracedPanel(
              "thread",
              result ? `สีด้าย · ${outWidthMm}×${outHeightMm} มม.` : "สีด้าย Brother",
            )}
          </div>

          {/* controls */}
          <div className="flex w-72 flex-col gap-3 overflow-y-auto border-l border-neutral-200 bg-white p-3 text-xs">
            <label className="flex flex-col gap-1">
              <span className="flex justify-between text-neutral-600">
                จำนวนสี (เส้นด้าย) <b>{maxColors}</b>
              </span>
              <input
                type="range"
                min={2}
                max={16}
                value={maxColors}
                onChange={(e) => setMaxColors(Number(e.target.value))}
              />
            </label>

            <label className="flex items-center justify-between gap-2 text-neutral-600">
              ขนาดกว้าง (มม.)
              <input
                type="number"
                min={10}
                max={1000}
                value={outWidthMm}
                onChange={(e) =>
                  setOutWidthMm(Math.max(10, Number(e.target.value) || 10))
                }
                className="w-20 rounded-md border border-neutral-200 px-2 py-1 text-right outline-none focus:border-blue-400"
              />
            </label>

            <label className="flex flex-col gap-1">
              <span className="flex justify-between text-neutral-600">
                ลบจุดเล็ก (มม.²) <b>{despeckleMm2.toFixed(1)}</b>
              </span>
              <input
                type="range"
                min={0}
                max={5}
                step={0.1}
                value={despeckleMm2}
                onChange={(e) => setDespeckleMm2(Number(e.target.value))}
              />
            </label>

            <details className="rounded-md border border-neutral-100 p-2">
              <summary className="cursor-pointer select-none text-neutral-500">
                ขั้นสูง
              </summary>
              <div className="mt-2 flex flex-col gap-3">
                <label className="flex flex-col gap-1">
                  <span className="flex justify-between text-neutral-600">
                    ความโค้งมุม (°) <b>{cornerDeg}</b>
                  </span>
                  <input
                    type="range"
                    min={10}
                    max={120}
                    value={cornerDeg}
                    onChange={(e) => setCornerDeg(Number(e.target.value))}
                  />
                </label>
                <label className="flex items-center gap-2 text-neutral-600">
                  <input
                    type="checkbox"
                    checked={stacked}
                    onChange={(e) => setStacked(e.target.checked)}
                  />
                  โหมดซ้อนชั้น (เย็บพื้นเต็มใต้สีบน)
                </label>
              </div>
            </details>

            {/* color list */}
            <div className="min-h-0 flex-1">
              <div className="mb-1 text-neutral-500">
                สีที่เทรซได้ → เส้นด้าย Brother
              </div>
              <div className="flex flex-col gap-1">
                {result?.colors.map((c, i) => (
                  <label
                    key={i}
                    className={`flex items-center gap-2 rounded-md border px-2 py-1 transition ${
                      disabled.has(i)
                        ? "border-neutral-100 opacity-40"
                        : "border-neutral-200"
                    }`}
                  >
                    <input
                      type="checkbox"
                      checked={!disabled.has(i)}
                      onChange={() =>
                        setDisabled((s) => {
                          const n = new Set(s);
                          if (n.has(i)) n.delete(i);
                          else n.add(i);
                          return n;
                        })
                      }
                    />
                    <span
                      className="h-4 w-4 rounded-sm ring-1 ring-black/10"
                      style={{ background: c.rgb }}
                    />
                    <span className="text-neutral-300">→</span>
                    <span
                      className="h-4 w-4 rounded-sm ring-1 ring-black/10"
                      style={{ background: threads[i]?.hex }}
                    />
                    <span className="min-w-0 flex-1 truncate text-neutral-600">
                      {threads[i]?.name ?? c.rgb}
                    </span>
                    {threads[i] && sameThreadIndices.has(threads[i].index) && (
                      <span
                        className="text-[10px] text-amber-500"
                        title="มีหลายสีที่ใช้เส้นด้ายเดียวกัน"
                      >
                        ด้ายซ้ำ
                      </span>
                    )}
                  </label>
                ))}
              </div>
            </div>

            <label className="flex items-center gap-2 text-neutral-600">
              <input
                type="checkbox"
                checked={withBackground}
                onChange={(e) => setWithBackground(e.target.checked)}
              />
              ใส่รูปต้นฉบับเป็น Background
            </label>

            {error && <div className="text-red-500">{error}</div>}

            <button
              onClick={() => void create()}
              disabled={!result || tracing || busy}
              className="flex items-center justify-center gap-2 rounded-lg bg-blue-600 px-3 py-2 text-sm font-medium text-white shadow transition hover:bg-blue-700 disabled:opacity-50"
            >
              {busy ? (
                <Loader2 size={16} className="animate-spin" />
              ) : (
                <Sparkles size={16} />
              )}
              สร้างลายปัก
            </button>
          </div>
        </div>

        <input
          ref={fileRef}
          type="file"
          accept="image/png,image/jpeg"
          className="hidden"
          onChange={(e) => {
            const f = e.target.files?.[0];
            if (f) pickFile(f);
            e.target.value = "";
          }}
        />
      </div>
    </div>
  );
}
