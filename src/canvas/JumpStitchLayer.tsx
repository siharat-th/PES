import { useEffect, useMemo, useRef, useState } from "react";
import { Layer, Shape } from "react-konva";
import Konva from "konva";
import { useDocumentStore } from "../state/documentStore";
import { useUiStore } from "../state/uiStore";
import { getStitchData } from "../engine/EngineClient";
import type { StitchData } from "../engine/EngineClient";
import { useView, layerTransform } from "./viewContext";

/** Jump/trim travel moves drawn as dashed lines, colored by their stitch
 *  block's thread color. buildStitchData() pushes jump points into `coords`
 *  but leaves them out of every drawn segment, so any point index NOT covered
 *  by a segment is a jump destination — the dashed line goes from the previous
 *  needle point to it, and its color = the block it belongs to (the next run's
 *  hex, matching how the engine groups a leading jump with its block).
 *  Overlays ObjectsLayer (design view, toggle) and StitchLayer (always). */
export default function JumpStitchLayer() {
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

  // jump point indices (uncovered points), grouped by thread color. Each point
  // i draws a dashed line coords[i-1] -> coords[i]; its color is the next run's
  // hex (the block that owns the jump), falling back to the last run's hex.
  const groups = useMemo(() => {
    if (!data) return null;
    const segs = data.segments; // ordered ascending by start
    const covered = new Uint8Array(data.totalPoints);
    for (const seg of segs)
      for (let k = 0; k < seg.count; k++) covered[seg.start + k] = 1;

    const byHex = new Map<string, number[]>();
    const fallback = segs.length ? segs[segs.length - 1].hex : "#888888";
    let segPtr = 0;
    for (let i = 1; i < data.totalPoints; i++) {
      if (covered[i]) continue;
      while (segPtr < segs.length && segs[segPtr].start <= i) segPtr++;
      const hex = segPtr < segs.length ? segs[segPtr].hex : fallback;
      let arr = byHex.get(hex);
      if (!arr) byHex.set(hex, (arr = []));
      arr.push(i);
    }
    return Array.from(byHex, ([hex, idxs]) => ({ hex, idxs }));
  }, [data]);

  useEffect(() => {
    shapeRef.current?.getLayer()?.batchDraw();
  }, [simIndex, view.zoom, groups]);

  const limit = data ? (simIndex < 0 ? data.totalPoints : simIndex) : 0;

  return (
    <Layer listening={false} {...layerTransform(view)}>
      <Shape
        ref={shapeRef}
        sceneFunc={(konvaCtx) => {
          if (!data || !groups || groups.length === 0) return;
          const ctx = konvaCtx._context; // raw CanvasRenderingContext2D
          const { coords } = data;
          const px = 1 / view.zoom; // 1 screen pixel in world units
          ctx.lineWidth = px;
          ctx.lineCap = "butt";
          ctx.setLineDash([px * 3, px * 3]);
          for (const { hex, idxs } of groups) {
            ctx.strokeStyle = hex;
            ctx.beginPath();
            for (const i of idxs) {
              if (i >= limit) break; // respect the simulator playhead (idxs ↑)
              ctx.moveTo(coords[(i - 1) * 2], coords[(i - 1) * 2 + 1]);
              ctx.lineTo(coords[i * 2], coords[i * 2 + 1]);
            }
            ctx.stroke();
          }
          ctx.setLineDash([]);
        }}
      />
    </Layer>
  );
}
