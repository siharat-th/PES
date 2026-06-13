import { useEffect, useMemo, useRef, useState } from "react";
import { Layer, Line } from "react-konva";
import { useDocumentStore } from "../state/documentStore";
import { useUiStore } from "../state/uiStore";
import { getStitchData } from "../engine/EngineClient";
import type { StitchData } from "../engine/EngineClient";
import { useView, layerTransform } from "./viewContext";

/** Real stitch view: each continuous needle run drawn as a thin Konva.Line.
 *  The simulator reveals stitches progressively (simIndex = playhead). */
export default function StitchLayer() {
  const view = useView();
  const imageVersion = useDocumentStore((s) => s.imageVersion);
  const docExists = useDocumentStore((s) => !!s.doc);
  const simIndex = useUiStore((s) => s.simIndex);

  const [data, setData] = useState<StitchData | null>(null);

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

  // Build per-segment point arrays, clipped to the simulator playhead.
  const lines = useMemo(() => {
    if (!data) return [];
    const limit = simIndex < 0 ? data.totalPoints : simIndex;
    const out: { key: number; points: number[]; stroke: string }[] = [];
    for (let s = 0; s < data.segments.length; s++) {
      const seg = data.segments[s];
      if (seg.start >= limit) break; // segments are in stitch order
      const visible = Math.min(seg.count, limit - seg.start);
      if (visible < 2) continue;
      const pts: number[] = new Array(visible * 2);
      for (let i = 0; i < visible; i++) {
        pts[i * 2] = data.coords[(seg.start + i) * 2];
        pts[i * 2 + 1] = data.coords[(seg.start + i) * 2 + 1];
      }
      out.push({ key: s, points: pts, stroke: seg.hex });
    }
    return out;
  }, [data, simIndex]);

  // Needle position marker during simulation
  const needle = useMemo(() => {
    if (!data || simIndex < 0 || simIndex < 1 || simIndex > data.totalPoints)
      return null;
    const i = simIndex - 1;
    return [data.coords[i * 2], data.coords[i * 2 + 1]];
  }, [data, simIndex]);

  const layerRef = useRef<import("konva").default.Layer>(null);

  return (
    <Layer ref={layerRef} listening={false} {...layerTransform(view)}>
      {lines.map((l) => (
        <Line
          key={l.key}
          points={l.points}
          stroke={l.stroke}
          strokeWidth={3}
          strokeScaleEnabled={false}
          lineCap="round"
          lineJoin="round"
          perfectDrawEnabled={false}
        />
      ))}
      {needle && (
        <Line
          points={[needle[0] - 18, needle[1], needle[0] + 18, needle[1]]}
          stroke="#ff2d2d"
          strokeWidth={2}
          strokeScaleEnabled={false}
        />
      )}
    </Layer>
  );
}
