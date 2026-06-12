import { Layer, Rect, Line } from "react-konva";
import { UNITS_PER_MM } from "../engine/types";
import { useView, layerTransform } from "./viewContext";

/** Hoop, 5 mm grid and center axes — port of drawNodes (PesHelper.js:838-860).
 *  Coordinates are engine units (0.1 mm) with the origin at hoop center. */
export default function GridLayer({
  hoopWMm,
  hoopHMm,
}: {
  hoopWMm: number;
  hoopHMm: number;
}) {
  const view = useView();
  const hw = hoopWMm * UNITS_PER_MM;
  const hh = hoopHMm * UNITS_PER_MM;
  const step = 50; // 5 mm

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
        fill="#ffffff"
        stroke="#999999"
        strokeWidth={1}
        strokeScaleEnabled={false}
      />
      {vLines.map((x) => (
        <Line
          key={`v${x}`}
          points={[x, -hh / 2, x, hh / 2]}
          stroke="#e3e3e3"
          strokeWidth={1}
          strokeScaleEnabled={false}
        />
      ))}
      {hLines.map((y) => (
        <Line
          key={`h${y}`}
          points={[-hw / 2, y, hw / 2, y]}
          stroke="#e3e3e3"
          strokeWidth={1}
          strokeScaleEnabled={false}
        />
      ))}
      {/* center axes */}
      <Line
        points={[0, -hh / 2, 0, hh / 2]}
        stroke="#b5b5b5"
        strokeWidth={1.5}
        strokeScaleEnabled={false}
      />
      <Line
        points={[-hw / 2, 0, hw / 2, 0]}
        stroke="#b5b5b5"
        strokeWidth={1.5}
        strokeScaleEnabled={false}
      />
    </Layer>
  );
}
