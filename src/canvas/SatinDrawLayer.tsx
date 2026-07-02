import { useEffect, useRef, useState } from "react";
import { Layer, Rect, Line, Circle, Path } from "react-konva";
import { useDocumentStore } from "../state/documentStore";
import { useUiStore } from "../state/uiStore";
import { satinColumnRails } from "../engine/EngineClient";
import type { SatinKnot, SatinRails } from "../engine/EngineClient";
import { useView, layerTransform } from "./viewContext";

/** half-extent of the invisible click surface (engine units) */
const HIT = 200000;

// old-app colours (Utils/PesSatinColumn.cpp draw()/drawKnot()): rail A
// cornflower, rail B orange, rungs green, corner = yellow square, curve = blue
// circle. Kept for fidelity.
const RAIL_A = "#6494ed";
const RAIL_B = "#ffa500";
const RUNG = "#00c853";
const CORNER_FILL = "#ffdd00";
const CURVE_FILL = "#6494ed";
const KNOT_STROKE = "#1f2937";

/** even-indexed clicks build rail A, odd-indexed build rail B (the old app's
 *  `(sizeA+sizeB)%2` alternation). */
function splitRails(pts: SatinKnot[]): [SatinKnot[], SatinKnot[]] {
  const a: SatinKnot[] = [];
  const b: SatinKnot[] = [];
  pts.forEach((p, i) => (i % 2 === 0 ? a : b).push(p));
  return [a, b];
}

/**
 * Manual Satin Column draw tool. Click places a corner node, Shift+click a
 * curve node; nodes alternate between the two rails (A, B, A, B…), each pair a
 * rung. The engine smooths the rails live (its own cubic-superpath, identical
 * to the old app). Enter commits (needs ≥2 nodes per rail, equal counts);
 * Backspace removes the last node; Esc cancels.
 */
export default function SatinDrawLayer() {
  const view = useView();
  const busy = useDocumentStore((s) => s.busy);
  const addSatinColumn = useDocumentStore((s) => s.addSatinColumn);
  const setViewMode = useUiStore((s) => s.setViewMode);

  const [pts, setPts] = useState<SatinKnot[]>([]);
  const [preview, setPreview] = useState<SatinRails | null>(null);
  const [cursor, setCursor] = useState<{ x: number; y: number; shift: boolean } | null>(
    null,
  );
  const ptsRef = useRef(pts);
  ptsRef.current = pts;

  // Engine-smoothed rail d-strings. While the mouse is over the canvas, the
  // hover point is appended to the CURRENT rail (the one the next click will
  // extend), so that rail's trailing segment rubber-bands from its last node to
  // the cursor — a straight line into a corner node, a curve into a curve node
  // (Shift), exactly as clicking there would draw it. Latest result wins.
  useEffect(() => {
    if (pts.length === 0) {
      setPreview(null);
      return;
    }
    const [a, b] = splitRails(pts);
    if (cursor) {
      const hk: SatinKnot = { x: cursor.x, y: cursor.y, curve: cursor.shift };
      (pts.length % 2 === 0 ? a : b).push(hk); // next click extends this rail
    }
    let cancelled = false;
    satinColumnRails([a, b])
      .then((res) => {
        if (!cancelled) setPreview(res);
      })
      .catch(() => {
        /* preview is best-effort; keep the last good one */
      });
    return () => {
      cancelled = true;
    };
  }, [pts, cursor]);

  // Enter = commit, Esc = cancel, Backspace = drop last node. App.tsx defers
  // Delete/Backspace to this layer in satinDraw mode.
  useEffect(() => {
    const onKey = (e: KeyboardEvent) => {
      const tag = (e.target as HTMLElement)?.tagName;
      if (tag === "INPUT" || tag === "TEXTAREA") return;
      if (useDocumentStore.getState().busy) return;
      if (e.key === "Enter") {
        e.preventDefault();
        const cur = ptsRef.current;
        if (cur.length < 4) return; // <2 pairs → nothing to commit, stay in tool
        void addSatinColumn(splitRails(cur)).then((ok) => {
          if (ok) {
            setPts([]);
            setViewMode("design");
          }
        });
      } else if (e.key === "Escape") {
        e.preventDefault();
        setPts([]);
        setViewMode("design");
      } else if (e.key === "Backspace" || e.key === "Delete") {
        e.preventDefault();
        setPts((p) => p.slice(0, -1));
      }
    };
    window.addEventListener("keydown", onKey);
    return () => window.removeEventListener("keydown", onKey);
  }, [addSatinColumn, setViewMode]);

  const z = view.zoom;
  const r = 5 / z; // curve circle radius
  const sq = 9 / z; // corner square side
  const sw = 2 / z;
  const rungW = 1.5 / z;

  const rungCount = Math.floor(pts.length / 2);
  // where the next click lands (rail colour), and — if it will complete a rung —
  // the pending partner node
  const nextIsB = pts.length % 2 === 1;
  const pendingPartner = nextIsB ? pts[pts.length - 1] : null;

  return (
    <Layer {...layerTransform(view)} listening={!busy}>
      {/* click surface — place a node (Shift = curve); track cursor for the hint */}
      <Rect
        x={-HIT}
        y={-HIT}
        width={HIT * 2}
        height={HIT * 2}
        fill="rgba(0,0,0,0.001)"
        onClick={(e) => {
          if (e.evt.button !== 0) return; // left button only
          const w = e.target.getLayer()?.getRelativePointerPosition();
          if (!w) return;
          setPts((p) => [...p, { x: w.x, y: w.y, curve: e.evt.shiftKey }]);
        }}
        onMouseMove={(e) => {
          const w = e.target.getLayer()?.getRelativePointerPosition();
          if (!w) return;
          setCursor({ x: w.x, y: w.y, shift: e.evt.shiftKey });
        }}
        onMouseLeave={() => setCursor(null)}
      />

      {/* smoothed rails (engine cubic-superpath) */}
      {preview?.rails[0] && (
        <Path
          data={preview.rails[0]}
          stroke={RAIL_A}
          strokeWidth={sw}
          fillEnabled={false}
          lineCap="round"
          lineJoin="round"
          listening={false}
        />
      )}
      {preview?.rails[1] && (
        <Path
          data={preview.rails[1]}
          stroke={RAIL_B}
          strokeWidth={sw}
          fillEnabled={false}
          lineCap="round"
          lineJoin="round"
          listening={false}
        />
      )}

      {/* rungs between paired nodes */}
      {Array.from({ length: rungCount }, (_, k) => {
        const a = pts[2 * k];
        const b = pts[2 * k + 1];
        return (
          <Line
            key={`rung-${k}`}
            points={[a.x, a.y, b.x, b.y]}
            stroke={RUNG}
            strokeWidth={rungW}
            listening={false}
          />
        );
      })}

      {/* cursor hint: the pending rung to the partner rail (dashed) + a marker
          at the cursor showing the pending node's type (square = corner, circle
          = curve). The pending rail SEGMENT itself is the live rail path above,
          which already rubber-bands to the cursor. */}
      {cursor && pendingPartner && (
        <Line
          points={[pendingPartner.x, pendingPartner.y, cursor.x, cursor.y]}
          stroke={RUNG}
          strokeWidth={rungW}
          dash={[6 / z, 4 / z]}
          opacity={0.6}
          listening={false}
        />
      )}
      {cursor &&
        (cursor.shift ? (
          <Circle
            x={cursor.x}
            y={cursor.y}
            radius={r}
            fill={CURVE_FILL}
            opacity={0.55}
            listening={false}
          />
        ) : (
          <Rect
            x={cursor.x}
            y={cursor.y}
            width={sq}
            height={sq}
            offsetX={sq / 2}
            offsetY={sq / 2}
            fill={CORNER_FILL}
            opacity={0.55}
            listening={false}
          />
        ))}

      {/* node handles: yellow square = corner, blue circle = curve (by type,
          as the old app; the rail PATHS carry the A/B colour) */}
      {pts.map((n, i) =>
        n.curve ? (
          <Circle
            key={`k-${i}`}
            x={n.x}
            y={n.y}
            radius={r}
            fill={CURVE_FILL}
            stroke={KNOT_STROKE}
            strokeWidth={sw * 0.6}
            listening={false}
          />
        ) : (
          <Rect
            key={`k-${i}`}
            x={n.x}
            y={n.y}
            width={sq}
            height={sq}
            offsetX={sq / 2}
            offsetY={sq / 2}
            fill={CORNER_FILL}
            stroke={KNOT_STROKE}
            strokeWidth={sw * 0.6}
            listening={false}
          />
        ),
      )}
    </Layer>
  );
}
