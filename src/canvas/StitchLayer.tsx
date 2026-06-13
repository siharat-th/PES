import { useEffect, useRef, useState } from "react";
import { Layer, Shape, Circle } from "react-konva";
import Konva from "konva";
import { useDocumentStore } from "../state/documentStore";
import { useUiStore } from "../state/uiStore";
import { getStitchData } from "../engine/EngineClient";
import type { StitchData } from "../engine/EngineClient";
import { useView, layerTransform } from "./viewContext";

/** Real stitch view: needle runs as 1px thread lines plus penetration dots,
 *  drawn in one Shape (sceneFunc) so it stays fast for large stitch counts.
 *  The simulator reveals stitches progressively (simIndex = playhead). */
export default function StitchLayer() {
  const view = useView();
  const imageVersion = useDocumentStore((s) => s.imageVersion);
  const docExists = useDocumentStore((s) => !!s.doc);
  const simIndex = useUiStore((s) => s.simIndex);

  const [data, setData] = useState<StitchData | null>(null);
  const shapeRef = useRef<Konva.Shape>(null);

  useEffect(() => {
    if (!docExists) {
      setData(null);
      return;
    }
    let cancelled = false;
    void getStitchData(-1).then((d) => {
      if (!cancelled) setData(d);
    });
    return () => {
      cancelled = true;
    };
  }, [docExists, imageVersion]);

  // redraw when playhead or zoom changes (line/dot sizes depend on zoom)
  useEffect(() => {
    shapeRef.current?.getLayer()?.batchDraw();
  }, [simIndex, view.zoom, data]);

  const limit = data
    ? simIndex < 0
      ? data.totalPoints
      : simIndex
    : 0;
  // penetration dots get expensive past this; lines still draw
  const showDots = limit <= 60000;

  const needle =
    data && simIndex >= 1 && simIndex <= data.totalPoints
      ? [data.coords[(simIndex - 1) * 2], data.coords[(simIndex - 1) * 2 + 1]]
      : null;

  return (
    <Layer listening={false} {...layerTransform(view)}>
      <Shape
        ref={shapeRef}
        sceneFunc={(konvaCtx) => {
          if (!data) return;
          const ctx = konvaCtx._context; // raw CanvasRenderingContext2D
          const { coords, segments } = data;
          const px = 1 / view.zoom; // 1 screen pixel in world units
          // thread lines
          ctx.lineCap = "round";
          ctx.lineJoin = "round";
          ctx.lineWidth = px;
          for (const seg of segments) {
            if (seg.start >= limit) break;
            const vis = Math.min(seg.count, limit - seg.start);
            if (vis < 2) continue;
            ctx.beginPath();
            ctx.moveTo(coords[seg.start * 2], coords[seg.start * 2 + 1]);
            for (let i = 1; i < vis; i++) {
              ctx.lineTo(
                coords[(seg.start + i) * 2],
                coords[(seg.start + i) * 2 + 1],
              );
            }
            ctx.strokeStyle = seg.hex;
            ctx.stroke();
          }
          // needle penetration dots
          if (showDots) {
            const r = px * 1.3; // ~1.3px on screen
            ctx.fillStyle = "rgba(255,255,255,0.85)";
            for (const seg of segments) {
              if (seg.start >= limit) break;
              const vis = Math.min(seg.count, limit - seg.start);
              for (let i = 0; i < vis; i++) {
                ctx.beginPath();
                ctx.arc(
                  coords[(seg.start + i) * 2],
                  coords[(seg.start + i) * 2 + 1],
                  r,
                  0,
                  Math.PI * 2,
                );
                ctx.fill();
              }
            }
          }
        }}
      />
      {needle && (
        <Circle x={needle[0]} y={needle[1]} radius={6 / view.zoom} fill="#ff2d2d" />
      )}
    </Layer>
  );
}
