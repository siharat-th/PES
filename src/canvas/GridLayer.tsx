import { Layer, Rect, Line } from "react-konva";
import { UNITS_PER_MM } from "../engine/types";
import type { ViewMode } from "../state/uiStore";
import { useView, layerTransform } from "./viewContext";

/** Hoop, 5 mm grid and center axes — port of drawNodes (PesHelper.js:838-860).
 *  Coordinates are engine units (0.1 mm) with the origin at hoop center.
 *  In stitch view the hoop becomes a dark fabric so threads + penetration
 *  dots read with real contrast. */
export default function GridLayer({
  hoopWMm,
  hoopHMm,
  mode,
}: {
  hoopWMm: number;
  hoopHMm: number;
  mode: ViewMode;
}) {
  const view = useView();
  const hw = hoopWMm * UNITS_PER_MM;
  const hh = hoopHMm * UNITS_PER_MM;
  const step = 50; // 5 mm

  // stitch + node-edit modes use the dark fabric so threads/handles pop
  const dark = mode !== "design";
  const c = dark
    ? {
        fabric: "#34373d",
        grid: "rgba(255,255,255,0.05)",
        axis: "rgba(255,255,255,0.13)",
        border: "#5b5f66",
      }
    : {
        fabric: "#ffffff",
        grid: "#e3e3e3",
        axis: "#b5b5b5",
        border: "#999999",
      };

  const vLines = [];
  for (let x = step; x < hw / 2; x += step) vLines.push(x, -x);
  const hLines = [];
  for (let y = step; y < hh / 2; y += step) hLines.push(y, -y);

  return (
    <Layer listening={false} {...layerTransform(view)}>
      <Rect
        x={-hw / 2}
        y={-hh / 2}
        width={hw}
        height={hh}
        fill={c.fabric}
        stroke={c.border}
        strokeWidth={1}
        strokeScaleEnabled={false}
      />
      {vLines.map((x) => (
        <Line
          key={`v${x}`}
          points={[x, -hh / 2, x, hh / 2]}
          stroke={c.grid}
          strokeWidth={1}
          strokeScaleEnabled={false}
        />
      ))}
      {hLines.map((y) => (
        <Line
          key={`h${y}`}
          points={[-hw / 2, y, hw / 2, y]}
          stroke={c.grid}
          strokeWidth={1}
          strokeScaleEnabled={false}
        />
      ))}
      {/* center axes */}
      <Line
        points={[0, -hh / 2, 0, hh / 2]}
        stroke={c.axis}
        strokeWidth={1.5}
        strokeScaleEnabled={false}
      />
      <Line
        points={[-hw / 2, 0, hw / 2, 0]}
        stroke={c.axis}
        strokeWidth={1.5}
        strokeScaleEnabled={false}
      />
    </Layer>
  );
}
