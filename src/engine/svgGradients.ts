// SVG gradient support for the editor — WEB import path.
//
// The C++ SVG parser (pesSVG) doesn't resolve gradients, so a `fill="url(#g)"`
// would import as black. Instead we parse gradients with the browser's own SVG
// DOM at import time, rewrite each gradient fill to a unique SENTINEL solid
// color (so the engine still imports clean, editable geometry), and remember the
// gradient under that sentinel. ObjectsLayer then renders the path with a real
// Konva gradient whenever its fill matches a remembered sentinel.
//
// Geometry stays the engine's (fully editable / PathEdit). Limitations: web
// import only (desktop reads files directly), and gradients are session-only —
// a saved .ppes keeps the sentinel solid color, not the gradient (yet).

export interface GradientStop {
  offset: number; // 0..1
  color: string; // final CSS color (opacity already baked in)
}

export interface SvgGradient {
  type: "linear" | "radial";
  /** objectBoundingBox coords are fractions of the shape bbox (the common case);
   *  userSpaceOnUse is approximated (mapped across the bbox) for now. */
  objectBBox: boolean;
  stops: GradientStop[];
  // linear (fractions when objectBoundingBox)
  x1: number;
  y1: number;
  x2: number;
  y2: number;
  // radial
  cx: number;
  cy: number;
  r: number;
}

// session map: sentinel hex (lowercase "#rrggbb") -> gradient
const gradients = new Map<string, SvgGradient>();

export function lookupGradient(fill?: string): SvgGradient | undefined {
  return fill ? gradients.get(fill.toLowerCase()) : undefined;
}

const hex2 = (n: number) => Math.max(0, Math.min(255, n)).toString(16).padStart(2, "0");

function parseHex(c: string): [number, number, number] | null {
  const m = /^#([0-9a-f]{6})$/i.exec(c.trim());
  if (!m) return null;
  const v = parseInt(m[1], 16);
  return [(v >> 16) & 255, (v >> 8) & 255, v & 255];
}

/** Bake a stop's opacity into its color (hex → rgba; others passed through). */
function bakeOpacity(color: string, opacity: number): string {
  if (opacity >= 1) return color;
  const rgb = parseHex(color);
  if (!rgb) return color; // named/rgb() color with opacity<1: best effort
  return `rgba(${rgb[0]}, ${rgb[1]}, ${rgb[2]}, ${opacity})`;
}

/** A unique sentinel hex for this gradient — starts at its first stop color
 *  (so a lost session degrades to a sensible solid) and nudges blue until free. */
function makeSentinel(grad: SvgGradient): string {
  const base = parseHex(grad.stops[0]?.color ?? "") ?? [128, 128, 128];
  for (let d = 0; d < 256; d++) {
    for (const sign of [1, -1]) {
      const b = base[2] + sign * d;
      if (b < 0 || b > 255) continue;
      const hex = `#${hex2(base[0])}${hex2(base[1])}${hex2(b)}`;
      if (!gradients.has(hex.toLowerCase())) return hex;
    }
  }
  return `#${hex2(base[0])}${hex2(base[1])}${hex2(base[2])}`;
}

const coord = (v: string | null, dflt: number): number => {
  if (v == null || v === "") return dflt;
  return v.trim().endsWith("%") ? parseFloat(v) / 100 : parseFloat(v);
};

function readStops(el: Element): GradientStop[] {
  const stops: GradientStop[] = [];
  el.querySelectorAll("stop").forEach((s) => {
    const off = s.getAttribute("offset");
    const style = s.getAttribute("style") ?? "";
    const styleColor = /stop-color:\s*([^;]+)/i.exec(style)?.[1];
    const styleOpacity = /stop-opacity:\s*([^;]+)/i.exec(style)?.[1];
    const color = (s.getAttribute("stop-color") ?? styleColor ?? "#000000").trim();
    const opacity = parseFloat(s.getAttribute("stop-opacity") ?? styleOpacity ?? "1");
    stops.push({
      offset: off ? coord(off, 0) : stops.length === 0 ? 0 : 1,
      color: bakeOpacity(color, isNaN(opacity) ? 1 : opacity),
    });
  });
  return stops;
}

function resolveGradient(el: Element, defs: Map<string, Element>): SvgGradient | null {
  // stops may be inherited via href from another gradient (Illustrator pattern)
  let stops = readStops(el);
  if (!stops.length) {
    const href =
      el.getAttribute("href") ?? el.getAttribute("xlink:href") ?? "";
    const ref = href.startsWith("#") ? defs.get(href.slice(1)) : undefined;
    if (ref) stops = readStops(ref);
  }
  if (!stops.length) return null;

  const objectBBox =
    (el.getAttribute("gradientUnits") ?? "objectBoundingBox") !==
    "userSpaceOnUse";

  if (el.nodeName === "radialGradient") {
    return {
      type: "radial",
      objectBBox,
      stops,
      x1: 0, y1: 0, x2: 0, y2: 0,
      cx: coord(el.getAttribute("cx"), 0.5),
      cy: coord(el.getAttribute("cy"), 0.5),
      r: coord(el.getAttribute("r"), 0.5),
    };
  }
  return {
    type: "linear",
    objectBBox,
    stops,
    x1: coord(el.getAttribute("x1"), 0),
    y1: coord(el.getAttribute("y1"), 0),
    x2: coord(el.getAttribute("x2"), 1),
    y2: coord(el.getAttribute("y2"), 0),
    cx: 0, cy: 0, r: 0,
  };
}

/** Rewrite gradient fills to sentinel solids and remember them. Returns the
 *  modified SVG text to hand to the engine (or the original on any parse miss). */
export function preprocessSvgGradients(svgText: string): string {
  let dom: Document;
  try {
    dom = new DOMParser().parseFromString(svgText, "image/svg+xml");
  } catch {
    return svgText;
  }
  const root = dom.documentElement;
  if (!root || root.nodeName === "parsererror") return svgText;

  const defs = new Map<string, Element>();
  root.querySelectorAll("linearGradient, radialGradient").forEach((g) => {
    const id = g.getAttribute("id");
    if (id) defs.set(id, g);
  });
  if (!defs.size) return svgText;

  let rewrote = false;
  // any element painted with a gradient (fill attribute or inline style)
  root.querySelectorAll<SVGElement>("*").forEach((el) => {
    const styleFill = /(?:^|;)\s*fill:\s*([^;]+)/i.exec(
      el.getAttribute("style") ?? "",
    )?.[1];
    const fill = (el.getAttribute("fill") ?? styleFill ?? "").trim();
    const m = /^url\(["']?#(.+?)["']?\)$/.exec(fill);
    if (!m) return;
    const gEl = defs.get(m[1]);
    if (!gEl) return;
    const grad = resolveGradient(gEl, defs);
    if (!grad) return;
    const sentinel = makeSentinel(grad);
    gradients.set(sentinel.toLowerCase(), grad);
    el.setAttribute("fill", sentinel);
    rewrote = true;
  });
  if (!rewrote) return svgText;
  return new XMLSerializer().serializeToString(root);
}
