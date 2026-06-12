import { useEffect, useRef, useState } from "react";
import { Layer, Image as KonvaImage, Transformer } from "react-konva";
import Konva from "konva";
import { useDocumentStore } from "../state/documentStore";
import { getObjectImageBitmap } from "../engine/EngineClient";
import type { ObjectSnapshot } from "../engine/types";
import { useView, layerTransform } from "./viewContext";

interface CachedImage {
  version: number;
  bitmap: ImageBitmap;
}

/** Embroidery objects as cached bitmap images + selection transformer.
 *  Mirrors the old model: image rendered unrotated by the engine, rotation
 *  applied visually; gestures preview locally and commit on release.
 *  Multi-select: shift-click; dragging any selected node moves the group. */
export default function ObjectsLayer() {
  const view = useView();
  const doc = useDocumentStore((s) => s.doc);
  const imageVersion = useDocumentStore((s) => s.imageVersion);
  const selectedIndices = useDocumentStore((s) => s.selectedIndices);
  const select = useDocumentStore((s) => s.select);
  const commitTransform = useDocumentStore((s) => s.commitTransform);
  const translateSelected = useDocumentStore((s) => s.translateSelected);

  const cacheRef = useRef<Map<number, CachedImage>>(new Map());
  const [, forceRender] = useState(0);
  const trRef = useRef<Konva.Transformer>(null);
  const nodeRefs = useRef<Map<number, Konva.Image>>(new Map());
  const dragStart = useRef<Map<number, { x: number; y: number }> | null>(null);

  const objects = doc?.objects ?? [];

  // (Re)fetch object images when the document content changes.
  useEffect(() => {
    let cancelled = false;
    (async () => {
      for (const obj of objects) {
        const cached = cacheRef.current.get(obj.index);
        if (cached && cached.version === imageVersion) continue;
        const bitmap = await getObjectImageBitmap(obj.index);
        if (cancelled) return;
        if (bitmap) {
          cacheRef.current.set(obj.index, { version: imageVersion, bitmap });
          forceRender((n) => n + 1);
        }
      }
      for (const key of [...cacheRef.current.keys()]) {
        if (!objects.some((o) => o.index === key)) cacheRef.current.delete(key);
      }
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
      .filter((n): n is Konva.Image => !!n);
    tr.nodes(nodes);
    tr.getLayer()?.batchDraw();
  }, [selectedIndices, objects, imageVersion]);

  const commitNode = (obj: ObjectSnapshot, node: Konva.Image) => {
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

  const handleDragStart = (obj: ObjectSnapshot) => {
    // dragging an unselected object selects it alone
    if (!selectedIndices.includes(obj.index)) select(obj.index);
    const positions = new Map<number, { x: number; y: number }>();
    const ids = selectedIndices.includes(obj.index)
      ? selectedIndices
      : [obj.index];
    for (const i of ids) {
      const n = nodeRefs.current.get(i);
      if (n) positions.set(i, { x: n.x(), y: n.y() });
    }
    dragStart.current = positions;
  };

  const handleDragMove = (obj: ObjectSnapshot, node: Konva.Image) => {
    const start = dragStart.current;
    if (!start || start.size <= 1) return;
    const s0 = start.get(obj.index);
    if (!s0) return;
    const dx = node.x() - s0.x;
    const dy = node.y() - s0.y;
    for (const [i, p] of start) {
      if (i === obj.index) continue;
      nodeRefs.current.get(i)?.position({ x: p.x + dx, y: p.y + dy });
    }
  };

  const handleDragEnd = (obj: ObjectSnapshot, node: Konva.Image) => {
    const start = dragStart.current;
    dragStart.current = null;
    if (start && start.size > 1) {
      const s0 = start.get(obj.index);
      if (!s0) return;
      void translateSelected(node.x() - s0.x, node.y() - s0.y);
    } else {
      commitNode(obj, node);
    }
  };

  const primaryScalable =
    selectedIndices.length > 0 &&
    selectedIndices.every(
      (i) => objects.find((o) => o.index === i)?.scalable,
    );

  return (
    <Layer {...layerTransform(view)}>
      {objects
        .filter((o) => o.visible)
        .map((obj) => {
          const img = cacheRef.current.get(obj.index)?.bitmap;
          if (!img) return null;
          return (
            <KonvaImage
              key={obj.index}
              ref={(n) => {
                if (n) nodeRefs.current.set(obj.index, n);
                else nodeRefs.current.delete(obj.index);
              }}
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
              draggable={!obj.locked}
              onClick={(e) => select(obj.index, e.evt.shiftKey)}
              onTap={() => select(obj.index)}
              onDragStart={() => handleDragStart(obj)}
              onDragMove={(e) => handleDragMove(obj, e.target as Konva.Image)}
              onDragEnd={(e) => handleDragEnd(obj, e.target as Konva.Image)}
              onTransformEnd={(e) => commitNode(obj, e.target as Konva.Image)}
            />
          );
        })}
      <Transformer
        ref={trRef}
        rotateEnabled
        centeredScaling={false}
        keepRatio
        enabledAnchors={
          primaryScalable
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
      />
    </Layer>
  );
}
