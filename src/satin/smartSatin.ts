// Smart Satin driver — TypeScript port of apiWorkerConvertLayerToSatinColumn
// (Victor-frontend api-satin-helper.js:7829-8084) + the parameter copying from
// autoSmartSatin (appinit.js:5609).
//
// The regression-prone GEOMETRY (multipolygon -> straight skeleton ->
// centerline -> satin column rails) is NOT ported: it runs verbatim in the
// vendored classic scripts under public/satin/ (satin-core.js + d3 +
// straight-skeleton-v2 wasm), loaded here by <script> tag exactly like the
// engine wasm (webEngine.ts). This driver only replaces the old CanvasKit
// boundary with the engine seams (getSatinSource / simplifyPolygons /
// addSatinObjects — pes_satin_core.hpp), so desktop and web run the same code.
import * as engine from "../engine/EngineClient";
import type { DocumentSnapshot } from "../engine/types";

type Ring = [number, number][];

/** Handles exported by public/satin/satin-core.js (see its overrides tail). */
interface SatinCore {
  apiWorkerMakeMultipolygon(
    polygons: Ring[],
    value: number,
    istext: boolean,
    invert: boolean,
  ): SatinPolygonData | null;
  apiWorkerGetCenterline(
    polygondata: SatinPolygonData,
    istext: boolean,
    option0: boolean,
  ): Promise<SatinCenterline | null>;
  apiWorkerGetSatinColumnDS(
    centerline: SatinCenterline,
    istext: boolean,
  ): [string, string][];
  simplifyDouglasPeucker(points: Ring, sqTolerance: number): Ring;
  ringarea(ring: Ring): number;
  fnAddMiterJoinToTriangle(poly: Ring): void;
  findNearestBrotherColorIndex(r: number, g: number, b: number): number;
  hexToRgb(hex: string): [number, number, number];
}

interface SatinPolygonData {
  coordinates: unknown[];
  colorindex?: number;
  strHexColor?: string;
  transforms?: [[number, number], [number, number], number];
}

interface SatinCenterline {
  idx: number;
}

let corePromise: Promise<SatinCore> | null = null;

function loadScript(src: string): Promise<void> {
  return new Promise((resolve, reject) => {
    const s = document.createElement("script");
    s.src = src;
    s.async = false; // preserve d3 -> skeleton -> core order
    s.onload = () => resolve();
    s.onerror = () => reject(new Error(`failed to load ${src}`));
    document.head.appendChild(s);
  });
}

/** Load (once) the vendored Smart Satin scripts; classic scripts, same
 *  pattern as the engine wasm glue. */
function loadSatinCore(): Promise<SatinCore> {
  if (!corePromise) {
    corePromise = (async () => {
      await loadScript("/satin/d3.v7.min.js");
      await loadScript("/satin/straight-skeleton-v2/index.js");
      await loadScript("/satin/satin-core.js");
      const core = (globalThis as { __pesSatinCore?: SatinCore }).__pesSatinCore;
      if (!core) throw new Error("__pesSatinCore global missing");
      return core;
    })();
  }
  return corePromise;
}

const delay = (ms: number) => new Promise((res) => setTimeout(res, ms));

/**
 * Convert a TTF Text / SVG object's filled outlines into satin-column objects
 * (appended after the last object; the source object is left untouched, like
 * the old app). Returns the fresh snapshot, or null when the object yields no
 * columns.
 */
export async function convertObjectToSmartSatin(
  index: number,
): Promise<DocumentSnapshot | null> {
  const core = await loadSatinCore();
  const source = await engine.getSatinSource(index);
  if (!source.paths?.length) return null;

  const { istext, rotateDegree } = source;
  const polygondatas: (SatinPolygonData | null)[] = [];

  for (let i = 0; i < source.paths.length; i++) {
    const sp = source.paths[i];

    // per-polygon cleanup (api-satin-helper.js:7961-8028): area prefilter ->
    // Douglas-Peucker -> pathops simplify (engine) -> area filter -> miter fix
    const polygons: Ring[] = [];
    for (const polygon of sp.polygons) {
      if (!polygon || polygon.length < 3) continue;
      if (Math.abs(core.ringarea(polygon)) < 5) continue;

      const dp =
        polygon.length > 5
          ? core.simplifyDouglasPeucker(polygon, sp.simplifyValue)
          : polygon;
      let rings = await engine.simplifyPolygons([dp]);
      if (rings.length > 1) {
        rings = rings.filter((v) => Math.abs(core.ringarea(v)) >= 5);
      }
      for (const ring of rings) {
        if (ring) {
          core.fnAddMiterJoinToTriangle(ring);
          polygons.push(ring);
        }
      }
    }

    const mp = core.apiWorkerMakeMultipolygon(polygons, i, istext, false);
    if (mp) {
      mp.strHexColor = sp.colorHex;
      mp.colorindex = core.findNearestBrotherColorIndex(
        ...core.hexToRgb(sp.colorHex),
      );
      mp.transforms = [sp.center, sp.scale, rotateDegree];
    }
    polygondatas.push(mp);
  }

  // satin params copied from the source object (autoSmartSatin quirks kept:
  // density is effectively always 2.5)
  const param = await engine.getParameter(index);
  const density = 2.5;
  const pullCompensate = Math.min(2, Math.max(-0.5, param.pullCompensate ?? 0));
  const noneOverlap = false;

  let nlayers = 0;
  const centerlines: (SatinCenterline | null)[] = [];
  for (const polygondata of polygondatas) {
    if (!polygondata) continue;
    if (polygondata.coordinates?.length) nlayers++;
    centerlines.push(await core.apiWorkerGetCenterline(polygondata, istext, false));
    await delay(10);
  }

  const objects: engine.SatinObjectSpec[] = [];
  for (const centerline of centerlines) {
    if (!centerline) continue;
    const ds = core.apiWorkerGetSatinColumnDS(centerline, istext);
    if (!ds?.length) continue;
    const polygondata = polygondatas[centerline.idx];
    if (!polygondata?.transforms) continue;

    // multi-part objects: rotation is applied per new object, so zero it out
    // beyond the first layer (api-satin-helper.js:8058-8063)
    if (nlayers > 1 && polygondata.transforms[2] !== 0) {
      polygondata.transforms[2] = 0;
    }
    const [center, scale, rotate] = polygondata.transforms;
    objects.push({
      rails: ds.filter(
        (pair) => pair?.length === 2 && !!pair[0]?.length && !!pair[1]?.length,
      ),
      colorIndex: polygondata.colorindex ?? 11,
      center,
      scale,
      rotateDegree: rotate,
      density,
      pullCompensate,
      noneOverlap,
    });
    await delay(10);
  }

  if (!objects.length) return null;
  return engine.addSatinObjects(objects);
}
