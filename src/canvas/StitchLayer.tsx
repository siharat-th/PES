import { useEffect, useRef, useState } from "react";
import { Layer, Shape, Circle, Line } from "react-konva";
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
        }}
      />
      {needle && (
        <NeedleCrosshair x={needle[0]} y={needle[1]} zoom={view.zoom} />
      )}
    </Layer>
  );
}

/** Current-stitch marker: a red crosshair with a centre gap so the exact needle
 *  point stays visible (unlike a solid dot, which hides the stitch under it).
 *  Sizes are fixed screen pixels (÷zoom → world units). */
function NeedleCrosshair({ x, y, zoom }: { x: number; y: number; zoom: number }) {
  const arm = 11 / zoom; // half-length of each crosshair line
  const gap = 3.5 / zoom; // clear centre so the point shows through
  const sw = 1.5 / zoom; // ~1.5 px stroke
  const red = "#ff2d2d";
  const line = (points: number[], key: string) => (
    <Line key={key} points={points} stroke={red} strokeWidth={sw} lineCap="round" />
  );
  return (
    <>
      {line([x - arm, y, x - gap, y], "l")}
      {line([x + gap, y, x + arm, y], "r")}
      {line([x, y - arm, x, y - gap], "t")}
      {line([x, y + gap, x, y + arm], "b")}
      <Circle x={x} y={y} radius={1.3 / zoom} fill={red} />
    </>
  );
}
