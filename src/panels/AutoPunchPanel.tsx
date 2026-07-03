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

// sRGB hex -> CIELAB (D65). Thread matching uses CIEDE2000 (below), the
// perceptual gold standard: plain RGB sends a muddy cream to gray, and naive
// Oklab distance sends a yellow-green to brown — CIEDE2000 gets both right.
const hexToLab = (hex: string): [number, number, number] => {
  const { r, g, b } = parseHex(hex);
  const lin = (c: number) => {
    c /= 255;
    return c <= 0.04045 ? c / 12.92 : Math.pow((c + 0.055) / 1.055, 2.4);
  };
  const R = lin(r),
    G = lin(g),
    B = lin(b);
  const x = (R * 0.4124564 + G * 0.3575761 + B * 0.1804375) / 0.95047;
  const y = R * 0.2126729 + G * 0.7151522 + B * 0.072175;
  const z = (R * 0.0193339 + G * 0.119192 + B * 0.9503041) / 1.08883;
  const f = (t: number) => (t > 0.008856 ? Math.cbrt(t) : 7.787 * t + 16 / 116);
  const fx = f(x),
    fy = f(y),
    fz = f(z);
  return [116 * fy - 16, 500 * (fx - fy), 200 * (fy - fz)];
};

// CIEDE2000 colour difference between two CIELAB colours.
const DEG = Math.PI / 180;
const ciede2000 = (
  c1: [number, number, number],
  c2: [number, number, number],
) => {
  const [L1, a1, b1] = c1;
  const [L2, a2, b2] = c2;
  const C1 = Math.hypot(a1, b1);
  const C2 = Math.hypot(a2, b2);
  const aC = (C1 + C2) / 2;
  const G = 0.5 * (1 - Math.sqrt(aC ** 7 / (aC ** 7 + 25 ** 7)));
  const a1p = a1 * (1 + G);
  const a2p = a2 * (1 + G);
  const C1p = Math.hypot(a1p, b1);
  const C2p = Math.hypot(a2p, b2);
  const aCp = (C1p + C2p) / 2;
  let h1 = Math.atan2(b1, a1p) / DEG;
  if (h1 < 0) h1 += 360;
  let h2 = Math.atan2(b2, a2p) / DEG;
  if (h2 < 0) h2 += 360;
  const dLp = L2 - L1;
  const dCp = C2p - C1p;
  let dhp = 0;
  if (C1p * C2p !== 0) {
    dhp = h2 - h1;
    if (dhp > 180) dhp -= 360;
    else if (dhp < -180) dhp += 360;
  }
  const dHp = 2 * Math.sqrt(C1p * C2p) * Math.sin((dhp * DEG) / 2);
  const aLp = (L1 + L2) / 2;
  let ahp: number;
  if (C1p * C2p === 0) ahp = h1 + h2;
  else if (Math.abs(h1 - h2) > 180) ahp = (h1 + h2 + 360) / 2;
  else ahp = (h1 + h2) / 2;
  const T =
    1 -
    0.17 * Math.cos((ahp - 30) * DEG) +
    0.24 * Math.cos(2 * ahp * DEG) +
    0.32 * Math.cos((3 * ahp + 6) * DEG) -
    0.2 * Math.cos((4 * ahp - 63) * DEG);
  const dTheta = 30 * Math.exp(-(((ahp - 275) / 25) ** 2));
  const Rc = 2 * Math.sqrt(aCp ** 7 / (aCp ** 7 + 25 ** 7));
  const Sl = 1 + (0.015 * (aLp - 50) ** 2) / Math.sqrt(20 + (aLp - 50) ** 2);
  const Sc = 1 + 0.045 * aCp;
  const Sh = 1 + 0.015 * aCp * T;
  const Rt = -Math.sin(2 * dTheta * DEG) * Rc;
  return Math.sqrt(
    (dLp / Sl) ** 2 +
      (dCp / Sc) ** 2 +
      (dHp / Sh) ** 2 +
      Rt * (dCp / Sc) * (dHp / Sh),
  );
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
  const [smoothing, setSmoothing] = useState(3);
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
          smoothing,
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
  }, [open, img, maxColors, outWidthMm, despeckleMm2, smoothing, cornerDeg, stacked]);

  // Brother thread swatches in CIELAB, computed once.
  const paletteLab = useMemo(
    () => palette.map((p) => ({ p, lab: hexToLab(p.hex) })),
    [palette],
  );

  // traced color → nearest Brother thread by CIEDE2000 (display AND commit use
  // this index, so preview == result)
  const threads = useMemo(() => {
    if (!result || paletteLab.length === 0) return [];
    return result.colors.map((c) => {
      const lab = hexToLab(c.rgb);
      let best = paletteLab[0].p;
      let bestD = Infinity;
      for (const { p, lab: plab } of paletteLab) {
        const d = ciede2000(lab, plab);
        if (d < bestD) {
          bestD = d;
          best = p;
        }
      }
      return best;
    });
  }, [result, paletteLab]);

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

            <label className="flex flex-col gap-1">
              <span className="flex justify-between text-neutral-600">
                ความเนียน (ลด noise) <b>{smoothing}</b>
              </span>
              <input
                type="range"
                min={0}
                max={4}
                value={smoothing}
                onChange={(e) => setSmoothing(Number(e.target.value))}
              />
              <span className="text-[10px] text-neutral-400">
                สูงขึ้น = สีเรียบ แยกตัววัตถุกับพื้นหลังชัดขึ้น (เหมาะกับภาพถ่าย)
              </span>
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
