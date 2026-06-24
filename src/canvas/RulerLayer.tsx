import type { ReactElement } from "react";
import { Layer, Rect, Line, Text } from "react-konva";
import { UNITS_PER_MM } from "../engine/types";
import type { ViewMode } from "../state/uiStore";
import { useView } from "./viewContext";

/** Top + left rulers (mm), drawn in screen space so they stay pinned while the
 *  canvas pans/zooms. World units are 0.1 mm with the origin at hoop center, so
 *  0 sits on the center axes and labels go negative left/up, positive right/down. */

const RULER = 18; // band thickness, px
const MAJOR_TICK = RULER; // long tick (labeled)
const MINOR_TICK = 6; // short tick

/** Pick a "nice" mm step (1/2/5 × 10ⁿ) so labeled ticks sit ~targetPx apart. */
function niceStepMm(mmPerPx: number, targetPx: number) {
  const raw = mmPerPx * targetPx;
  const pow = Math.pow(10, Math.floor(Math.log10(raw)));
  const norm = raw / pow;
  const mult = norm < 1.5 ? 1 : norm < 3 ? 2 : norm < 7 ? 5 : 10;
  return mult * pow;
}

export default function RulerLayer({ mode }: { mode: ViewMode }) {
  const { zoom, centerX, centerY, x, y } = useView();
  const width = centerX * 2;
  const height = centerY * 2;
  if (width === 0 || height === 0) return null;

  const dark = mode !== "design";
  const c = dark
    ? { band: "#2a2c30", border: "#4a4d53", tick: "#8a8f96", label: "#c2c6cc" }
    : { band: "#f7f7f8", border: "#c9c9cc", tick: "#8a8a8a", label: "#555555" };

  const pxPerMm = zoom * UNITS_PER_MM;
  const majorMm = niceStepMm(1 / pxPerMm, 64);
  const minorMm = majorMm / 5;
  const decimals = Math.max(0, -Math.floor(Math.log10(majorMm) + 1e-9));
  const fmt = (m: number) => {
    const s = (Math.abs(m) < minorMm / 2 ? 0 : m).toFixed(decimals);
    return s === `-${(0).toFixed(decimals)}` ? (0).toFixed(decimals) : s;
  };

  // world(mm) -> screen px
  const sx = (mm: number) => mm * pxPerMm + centerX + x;
  const sy = (mm: number) => mm * pxPerMm + centerY + y;

  const hTicks: ReactElement[] = [];
  const startIX = Math.ceil(((0 - centerX - x) / pxPerMm) / minorMm);
  const endIX = Math.floor(((width - centerX - x) / pxPerMm) / minorMm);
  for (let i = startIX; i <= endIX; i++) {
    const mm = i * minorMm;
    const px = sx(mm);
    const major = i % 5 === 0;
    hTicks.push(
      <Line
        key={`ht${i}`}
        points={[px, RULER, px, RULER - (major ? MAJOR_TICK : MINOR_TICK)]}
        stroke={c.tick}
        strokeWidth={1}
        listening={false}
      />,
    );
    if (major)
      hTicks.push(
        <Text
          key={`hl${i}`}
          text={fmt(mm)}
          x={px + 2}
          y={2}
          fontSize={9}
          fill={c.label}
          listening={false}
        />,
      );
  }

  const vTicks: ReactElement[] = [];
  const startIY = Math.ceil(((0 - centerY - y) / pxPerMm) / minorMm);
  const endIY = Math.floor(((height - centerY - y) / pxPerMm) / minorMm);
  for (let i = startIY; i <= endIY; i++) {
    const mm = i * minorMm;
    const py = sy(mm);
    const major = i % 5 === 0;
    vTicks.push(
      <Line
        key={`vt${i}`}
        points={[RULER, py, RULER - (major ? MAJOR_TICK : MINOR_TICK), py]}
        stroke={c.tick}
        strokeWidth={1}
        listening={false}
      />,
    );
    if (major) {
      const label = fmt(mm);
      vTicks.push(
        <Text
          key={`vl${i}`}
          text={label}
          x={2}
          y={py}
          offsetX={(label.length * 5.4) / 2}
          rotation={-90}
          fontSize={9}
          fill={c.label}
          listening={false}
        />,
      );
    }
  }

  return (
    <Layer listening={false}>
      {/* top band */}
      <Rect x={0} y={0} width={width} height={RULER} fill={c.band} />
      {hTicks}
      {/* left band (drawn after, so it masks the start of the top ticks) */}
      <Rect x={0} y={0} width={RULER} height={height} fill={c.band} />
      {vTicks}
      {/* edge borders */}
      <Line points={[0, RULER + 0.5, width, RULER + 0.5]} stroke={c.border} strokeWidth={1} />
      <Line points={[RULER + 0.5, 0, RULER + 0.5, height]} stroke={c.border} strokeWidth={1} />
      {/* corner */}
      <Rect x={0} y={0} width={RULER} height={RULER} fill={c.band} />
      <Line points={[0, RULER + 0.5, RULER + 0.5, RULER + 0.5]} stroke={c.border} strokeWidth={1} />
      <Line points={[RULER + 0.5, 0, RULER + 0.5, RULER + 0.5]} stroke={c.border} strokeWidth={1} />
      <Text text="mm" x={2} y={4} fontSize={8} fill={c.tick} listening={false} />
    </Layer>
  );
}
