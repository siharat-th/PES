// Client for the Auto Punch tracer worker (public/tracer/*). The wasm module
// (quantette color quantization + visioncortex tracing) runs in a classic Web
// Worker so tracing never blocks the UI. Each trace() call gets a job id;
// callers that only care about the latest result (debounced live preview)
// can drop stale resolutions by comparing ids themselves — trace() resolves
// every job it is asked for.

export interface TraceOptions {
  /** 2..16 in the UI (default 6) */
  maxColors?: number;
  /** minimum region area in work-pixels (frontend converts from mm²) */
  despeckleAreaPx?: number;
  cornerThresholdDeg?: number;
  /** "cutout" (default): regions never overlap — each stitched once */
  hierarchical?: "cutout" | "stacked";
  mode?: "spline" | "polygon" | "none";
  /** edge-preserving pre-smoothing radius (0 = off, 1..4) — flattens photo
   *  noise for cleaner colour separation without blurring edges */
  smoothing?: number;
}

export interface TraceColorLayer {
  rgb: string; // "#rrggbb"
  areaPx: number;
  /** absolute work-px d-strings; holes are subpaths of the same string */
  paths: string[];
}

export interface TraceResult {
  width: number;
  height: number;
  /** reduced thread colours (bottom-up: [0] is background-most) */
  colors: TraceColorLayer[];
  /** full-resolution emergent clusters in true colours — the "web app"
   *  reference preview, not embroidered */
  webColors: TraceColorLayer[];
}

type Job = {
  resolve: (r: TraceResult) => void;
  reject: (e: Error) => void;
};

let worker: Worker | null = null;
let nextId = 1;
const pending = new Map<number, Job>();

function getWorker(): Worker {
  if (worker) return worker;
  worker = new Worker("/tracer/tracer-worker.js");
  worker.onmessage = (e: MessageEvent) => {
    const { id, result, error } = e.data;
    const job = pending.get(id);
    if (!job) return;
    pending.delete(id);
    if (error) job.reject(new Error(error));
    else job.resolve(result as TraceResult);
  };
  worker.onerror = (e) => {
    const err = new Error(e.message || "tracer worker error");
    for (const job of pending.values()) job.reject(err);
    pending.clear();
    worker?.terminate();
    worker = null; // next trace() spawns a fresh worker
  };
  return worker;
}

/** Trace an RGBA image. The buffer is TRANSFERRED (unusable afterwards). */
export function traceImage(
  rgba: ArrayBuffer,
  width: number,
  height: number,
  opts: TraceOptions = {}
): Promise<TraceResult> {
  const id = nextId++;
  return new Promise<TraceResult>((resolve, reject) => {
    pending.set(id, { resolve, reject });
    getWorker().postMessage({ id, rgba, width, height, opts }, [rgba]);
  });
}

export function disposeTracer() {
  worker?.terminate();
  worker = null;
  const err = new Error("tracer disposed");
  for (const job of pending.values()) job.reject(err);
  pending.clear();
}
