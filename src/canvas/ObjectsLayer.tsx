import { useCallback, useEffect, useRef, useState } from "react";
import { Layer, Group, Path, Image as KonvaImage, Transformer } from "react-konva";
import Konva from "konva";
import { useDocumentStore } from "../state/documentStore";
import { getObjectImageBitmap, getObjectVector } from "../engine/EngineClient";
import type { ObjectSnapshot, ObjectVector, VectorPath } from "../engine/types";
import { lookupGradient, type SvgGradient } from "../engine/svgGradients";
import { useView, layerTransform, type ViewTransform } from "./viewContext";

interface CachedImage {
  version: number;
  scale: number; // LOD bucket the bitmap was rasterized at (1,2,4,8)
  bitmap: ImageBitmap;
}
interface CachedVector {
  version: number;
  vector: ObjectVector;
}

/** An object with no stitches yet (a freshly added shape / un-filled SVG) is
 *  drawn as a crisp Konva vector — no raster, no stroke clipping, instant
 *  recolor. Everything with real stitches stays a cached engine PNG. */
const isVector = (o: ObjectSnapshot) => o.scalable && !o.has_stitches;

// ── Zoom-proportional LOD ────────────────────────────────────────────────────
// A stitched object's PNG is baked at the engine's 1px-per-0.1mm base, so Konva
// upsamples it on zoom → blur. Instead we re-request it at a power-of-two scale
// ≥ zoom, so Konva only ever DOWNSAMPLES the raster (crisp). Buckets keep the
// re-raster count to a handful of boundaries across the whole zoom range, and
// culling means only on-screen objects pay for the high-res render.
const MAX_LOD = 8; // ceiling (~2000 DPI); beyond this deep zoom stays a bit soft
const MAX_RASTER_PX = 2048; // per-object cap so one huge object can't blow memory
const VIEW_DEBOUNCE_MS = 140; // re-raster only after zoom/pan settles

/** Power-of-two scale bucket covering a zoom level (1, 2, 4, 8). */
function lodForZoom(zoom: number): number {
  if (zoom <= 1) return 1;
  return Math.min(2 ** Math.ceil(Math.log2(zoom)), MAX_LOD);
}

/** LOD for one object, lowered so its raster stays under MAX_RASTER_PX. */
function lodForObject(zoom: number, o: ObjectSnapshot): number {
  let lod = lodForZoom(zoom);
  const maxDim = Math.max(o.width, o.height) + 6; // +engine margin
  while (lod > 1 && lod * maxDim > MAX_RASTER_PX) lod /= 2;
  return lod;
}

interface WorldRect {
  x0: number;
  y0: number;
  x1: number;
  y1: number;
}

/** Visible world-coordinate rectangle for the current view transform. */
function visibleWorldRect(v: ViewTransform): WorldRect {
  const w = v.centerX * 2;
  const h = v.centerY * 2;
  const ox = v.centerX + v.x;
  const oy = v.centerY + v.y;
  return {
    x0: -ox / v.zoom,
    y0: -oy / v.zoom,
    x1: (w - ox) / v.zoom,
    y1: (h - oy) / v.zoom,
  };
}

/** Does the object's (axis-aligned) bbox overlap the viewport? A margin covers
 *  rotation and lets objects just off-screen stay ready when panning. */
function objVisible(o: ObjectSnapshot, r: WorldRect): boolean {
  const m = Math.max(o.width, o.height) * 0.5 + 20;
  return !(
    o.x + o.width < r.x0 - m ||
    o.x > r.x1 + m ||
    o.y + o.height < r.y0 - m ||
    o.y > r.y1 + m
  );
}

/** "#rrggbb" + opacity → an rgba() string Konva understands. */
function rgba(hex?: string, opacity = 1): string | undefined {
  if (!hex) return undefined;
  const h = hex.replace("#", "");
  const r = parseInt(h.slice(0, 2), 16);
  const g = parseInt(h.slice(2, 4), 16);
  const b = parseInt(h.slice(4, 6), 16);
  return `rgba(${r}, ${g}, ${b}, ${opacity})`;
}

/** Konva fill props for a vector path: a real linear/radial gradient if the
 *  fill is a remembered SVG-gradient sentinel, else a solid fill. Gradient
 *  coords map over the path bbox (objectBoundingBox; userSpaceOnUse approx). */
function fillProps(p: VectorPath): Record<string, unknown> {
  const grad: SvgGradient | undefined = lookupGradient(p.fill);
  if (!grad) return { fill: rgba(p.fill, p.fillOpacity) };
  const [bx, by, bw, bh] = p.bbox;
  const stops = grad.stops.flatMap((s) => [s.offset, s.color]);
  if (grad.type === "linear") {
    const o = grad.objectBBox;
    const x1 = o ? grad.x1 : 0;
    const y1 = o ? grad.y1 : 0;
    const x2 = o ? grad.x2 : 1;
    const y2 = o ? grad.y2 : 1;
    return {
      fillPriority: "linear-gradient",
      fillLinearGradientStartPoint: { x: bx + x1 * bw, y: by + y1 * bh },
      fillLinearGradientEndPoint: { x: bx + x2 * bw, y: by + y2 * bh },
      fillLinearGradientColorStops: stops,
    };
  }
  const cx = grad.objectBBox ? grad.cx : 0.5;
  const cy = grad.objectBBox ? grad.cy : 0.5;
  const center = { x: bx + cx * bw, y: by + cy * bh };
  return {
    fillPriority: "radial-gradient",
    fillRadialGradientStartPoint: center,
    fillRadialGradientEndPoint: center,
    fillRadialGradientStartRadius: 0,
    fillRadialGradientEndRadius: grad.r * Math.max(bw, bh),
    fillRadialGradientColorStops: stops,
  };
}

/** Embroidery objects as cached bitmaps (stitched) or crisp vectors (shapes) +
 *  selection transformer. Image/vector rendered unrotated by the engine,
 *  rotation applied visually; gestures preview locally and commit on release.
 *  Multi-select: shift-click; dragging any selected node moves the group. */
export default function ObjectsLayer({ readOnly = false }: { readOnly?: boolean }) {
  const view = useView();
  const doc = useDocumentStore((s) => s.doc);
  const imageVersion = useDocumentStore((s) => s.imageVersion);
  const selectedIndices = useDocumentStore((s) => s.selectedIndices);
  const select = useDocumentStore((s) => s.select);
  const commitTransform = useDocumentStore((s) => s.commitTransform);
  const translateObjects = useDocumentStore((s) => s.translateObjects);

  const cacheRef = useRef<Map<number, CachedImage>>(new Map());
  const vecRef = useRef<Map<number, CachedVector>>(new Map());
  const [renderTick, forceRender] = useState(0);
  const trRef = useRef<Konva.Transformer>(null);
  const nodeRefs = useRef<Map<number, Konva.Node>>(new Map());
  const dragIds = useRef<number[]>([]);

  const objects = doc?.objects ?? [];

  // Latest values for the async render passes (which outlive one render).
  const viewRef = useRef(view);
  viewRef.current = view;
  const objectsRef = useRef(objects);
  objectsRef.current = objects;
  const versionRef = useRef(imageVersion);
  versionRef.current = imageVersion;
  const passRef = useRef(0); // per-pass id; a stale pass bails after its await

  // (Re)fetch each object's render resource: a crisp vector (shape) or a
  // zoom-proportional bitmap (stitched). Only visible objects rasterize at full
  // LOD, and a bitmap is never downgraded — so pan/zoom stays smooth and deep
  // zoom stays sharp. Runs on content change, and debounced when the view settles.
  const ensureImages = useCallback(async () => {
    const myPass = ++passRef.current;
    const objs = objectsRef.current;
    const ver = versionRef.current;
    const v = viewRef.current;
    const rect = v.centerX > 0 && v.centerY > 0 ? visibleWorldRect(v) : null;

    for (const obj of objs) {
      if (isVector(obj)) {
        const cached = vecRef.current.get(obj.index);
        if (cached && cached.version === ver) continue;
        const vector = await getObjectVector(obj.index);
        if (passRef.current !== myPass) return;
        vecRef.current.set(obj.index, { version: ver, vector });
        forceRender((n) => n + 1);
        continue;
      }
      const visible = !rect || objVisible(obj, rect);
      const wantLod = visible ? lodForObject(v.zoom, obj) : 1;
      const cached = cacheRef.current.get(obj.index);
      if (cached && cached.version === ver) {
        // fresh content already cached: re-raster only to gain sharpness for a
        // visible object; never downgrade an off-screen or already-crisp one.
        if (!visible || cached.scale >= wantLod) continue;
      }
      const bitmap = await getObjectImageBitmap(obj.index, wantLod);
      if (passRef.current !== myPass) return;
      if (bitmap) {
        cached?.bitmap.close();
        cacheRef.current.set(obj.index, { version: ver, scale: wantLod, bitmap });
        forceRender((n) => n + 1);
      }
    }
    // drop caches for objects that no longer exist
    for (const key of [...cacheRef.current.keys()])
      if (!objs.some((o) => o.index === key)) {
        cacheRef.current.get(key)?.bitmap.close();
        cacheRef.current.delete(key);
      }
    for (const key of [...vecRef.current.keys()])
      if (!objs.some((o) => o.index === key)) vecRef.current.delete(key);
  }, []);

  // content / version change → refresh right away
  useEffect(() => {
    void ensureImages();
  }, [objects, imageVersion, ensureImages]);

  // view change → re-raster visible objects at the new zoom, once it settles
  useEffect(() => {
    const t = window.setTimeout(() => void ensureImages(), VIEW_DEBOUNCE_MS);
    return () => clearTimeout(t);
  }, [view.zoom, view.x, view.y, view.centerX, view.centerY, ensureImages]);

  // Attach transformer to all selected nodes.
  useEffect(() => {
    const tr = trRef.current;
    if (!tr) return;
    const nodes = selectedIndices
      .map((i) => nodeRefs.current.get(i))
      .filter((n): n is Konva.Node => !!n);
    tr.nodes(nodes);
    tr.getLayer()?.batchDraw();
    // `renderTick` re-runs this as each object's resource loads and its Konva
    // node mounts — without it, a freshly duplicated group attaches only the
    // copies whose resources happened to be ready, leaving the rest unselected.
  }, [selectedIndices, objects, imageVersion, renderTick]);

  const commitNode = (obj: ObjectSnapshot, node: Konva.Node) => {
    const cx0 = obj.x + obj.width / 2;
    const cy0 = obj.y + obj.height / 2;
    // non-scalable objects (e.g. imported "Stitch") never scale
    const sx = obj.scalable ? node.scaleX() : 1;
    const sy = obj.scalable ? node.scaleY() : 1;
    const rot = node.rotation();
    // Engine scales from the bbox left-top corner; compensate so the visual
    // center stays where Konva shows it (old app: translate(-dw/2,-dh/2)).
    const dx = node.x() - cx0 - ((sx - 1) * obj.width) / 2;
    const dy = node.y() - cy0 - ((sy - 1) * obj.height) / 2;
    node.scale({ x: 1, y: 1 });
    void commitTransform(obj.index, dx, dy, sx, sy, rot);
  };

  // The multi-node Transformer group-drags peers natively, so we only need
  // to record who participates and commit per-node absolute deltas at the end.
  const handleDragStart = (obj: ObjectSnapshot) => {
    if (!selectedIndices.includes(obj.index)) select(obj.index);
    dragIds.current = selectedIndices.includes(obj.index)
      ? [...selectedIndices]
      : [obj.index];
  };

  const handleDragEnd = (obj: ObjectSnapshot, node: Konva.Node) => {
    // Group drag fires dragend on EVERY attached node — only the first one
    // commits the gesture; later ones must not re-translate (double-move bug).
    const ids = dragIds.current;
    dragIds.current = [];
    if (ids.length === 0) return;
    if (ids.length > 1) {
      const moves = ids.flatMap((i) => {
        const o = objects.find((x) => x.index === i);
        const n = nodeRefs.current.get(i);
        if (!o || !n) return [];
        return [
          {
            index: i,
            dx: n.x() - (o.x + o.width / 2),
            dy: n.y() - (o.y + o.height / 2),
          },
        ];
      });
      void translateObjects(moves);
    } else {
      commitNode(obj, node);
    }
  };

  // The whole selection is scalable only if EVERY selected object is — so a
  // group containing a non-scalable "Stitch" shows no scale handles.
  const selectionScalable =
    selectedIndices.length > 0 &&
    selectedIndices.every((i) => objects.find((o) => o.index === i)?.scalable);

  const setNodeRef = (index: number) => (n: Konva.Node | null) => {
    if (n) nodeRefs.current.set(index, n);
    else nodeRefs.current.delete(index);
  };

  return (
    <Layer {...layerTransform(view)}>
      {objects
        .filter((o) => o.visible)
        .map((obj) => {
          const common = {
            draggable: !obj.locked && !readOnly,
            listening: !readOnly,
            onClick: (e: Konva.KonvaEventObject<MouseEvent>) =>
              select(obj.index, e.evt.shiftKey),
            onTap: () => select(obj.index),
            onDragStart: () => handleDragStart(obj),
            onDragEnd: (e: Konva.KonvaEventObject<DragEvent>) =>
              handleDragEnd(obj, e.target),
            onTransformEnd: (e: Konva.KonvaEventObject<Event>) =>
              commitNode(obj, e.currentTarget as Konva.Node),
          };

          if (isVector(obj)) {
            const vec = vecRef.current.get(obj.index)?.vector;
            if (!vec) return null;
            const cx = obj.x + obj.width / 2;
            const cy = obj.y + obj.height / 2;
            return (
              <Group
                key={obj.index}
                ref={setNodeRef(obj.index)}
                x={cx}
                y={cy}
                offsetX={cx}
                offsetY={cy}
                rotation={obj.rotate_degree}
                {...common}
              >
                {vec.paths.map((p, i) => (
                  <Path
                    key={i}
                    data={p.d}
                    {...fillProps(p)}
                    stroke={rgba(p.stroke, p.strokeOpacity)}
                    strokeWidth={p.strokeWidth}
                  />
                ))}
              </Group>
            );
          }

          const img = cacheRef.current.get(obj.index)?.bitmap;
          if (!img) return null;
          return (
            <KonvaImage
              key={obj.index}
              ref={setNodeRef(obj.index)}
              image={img}
              x={obj.x + obj.width / 2}
              y={obj.y + obj.height / 2}
              offsetX={obj.width / 2}
              offsetY={obj.height / 2}
              width={obj.width}
              height={obj.height}
              rotation={obj.rotate_degree}
              scaleX={1}
              scaleY={1}
              {...common}
            />
          );
        })}
      {!readOnly && <Transformer
        ref={trRef}
        rotateEnabled
        centeredScaling={false}
        keepRatio
        enabledAnchors={
          selectionScalable
            ? [
                "top-left",
                "top-right",
                "bottom-left",
                "bottom-right",
                "middle-right",
                "bottom-center",
              ]
            : [] // มี "Stitch" ในกลุ่ม: ลาก/หมุนได้ แต่ scale ไม่ได้
        }
        anchorSize={9}
        anchorStroke="#6464ff"
        anchorFill="#ffffff"
        borderStroke="#6464ff"
        rotateAnchorOffset={30}
        ignoreStroke
      />}
    </Layer>
  );
}
