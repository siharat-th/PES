import { useEffect, useRef, useState } from "react";
import { Layer, Group, Path, Image as KonvaImage, Transformer } from "react-konva";
import Konva from "konva";
import { useDocumentStore } from "../state/documentStore";
import { getObjectImageBitmap, getObjectVector } from "../engine/EngineClient";
import type { ObjectSnapshot, ObjectVector, VectorPath } from "../engine/types";
import { lookupGradient, type SvgGradient } from "../engine/svgGradients";
import { useView, layerTransform } from "./viewContext";

interface CachedImage {
  version: number;
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

  // (Re)fetch each object's render resource when the document content changes:
  // a vector (shape) or a bitmap (stitched), whichever applies now.
  useEffect(() => {
    let cancelled = false;
    (async () => {
      for (const obj of objects) {
        if (isVector(obj)) {
          const cached = vecRef.current.get(obj.index);
          if (cached && cached.version === imageVersion) continue;
          const vector = await getObjectVector(obj.index);
          if (cancelled) return;
          vecRef.current.set(obj.index, { version: imageVersion, vector });
          forceRender((n) => n + 1);
        } else {
          const cached = cacheRef.current.get(obj.index);
          if (cached && cached.version === imageVersion) continue;
          const bitmap = await getObjectImageBitmap(obj.index);
          if (cancelled) return;
          if (bitmap) {
            cacheRef.current.set(obj.index, { version: imageVersion, bitmap });
            forceRender((n) => n + 1);
          }
        }
      }
      // drop caches for objects that no longer exist
      for (const key of [...cacheRef.current.keys()])
        if (!objects.some((o) => o.index === key)) cacheRef.current.delete(key);
      for (const key of [...vecRef.current.keys()])
        if (!objects.some((o) => o.index === key)) vecRef.current.delete(key);
    })();
    return () => {
      cancelled = true;
    };
  }, [objects, imageVersion]);

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
