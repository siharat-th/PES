import { useEffect, useRef, useState } from "react";
import { Layer, Circle } from "react-konva";
import { useDocumentStore } from "../state/documentStore";
import { getObjectPaths, getPathNodes, movePathNode } from "../engine/EngineClient";
import { PATH_CMD } from "../engine/types";
import type { PathNode } from "../engine/types";
import { useView, layerTransform } from "./viewContext";

interface PathNodes {
  pathIndex: number;
  nodes: PathNode[];
}

const isCurve = (t: number) =>
  t === PATH_CMD.bezierTo || t === PATH_CMD.quadBezierTo;

/** PathEdit mode: draggable anchor handles over the selected object's path
 *  nodes. Drag previews locally and commits the world delta on release; the
 *  engine moves the node (+ its bezier handles) and regenerates stitches. */
export default function PathEditLayer() {
  const view = useView();
  const doc = useDocumentStore((s) => s.doc);
  const imageVersion = useDocumentStore((s) => s.imageVersion);
  const selectedIndex = useDocumentStore((s) => s.selectedIndex);
  const applyPathEdit = useDocumentStore((s) => s.applyPathEdit);

  const [paths, setPaths] = useState<PathNodes[]>([]);
  const dragStart = useRef<{ x: number; y: number } | null>(null);

  const obj = doc?.objects.find((o) => o.index === selectedIndex);

  // (re)load the selected object's nodes when the selection or geometry changes
  useEffect(() => {
    if (selectedIndex < 0) {
      setPaths([]);
      return;
    }
    let cancelled = false;
    (async () => {
      const infos = await getObjectPaths(selectedIndex);
      const out: PathNodes[] = [];
      for (const info of infos) {
        if (!info.visible) continue;
        const nodes = await getPathNodes(selectedIndex, info.index);
        out.push({ pathIndex: info.index, nodes });
      }
      if (!cancelled) setPaths(out);
    })();
    return () => {
      cancelled = true;
    };
  }, [selectedIndex, imageVersion]);

  if (selectedIndex < 0 || !obj) return null;

  const r = 4.5 / view.zoom;
  const sw = 1.2 / view.zoom;

  return (
    <Layer {...layerTransform(view)}>
      {paths.map((p) =>
        p.nodes.map((n, i) => {
          if (n.node_type === PATH_CMD.close) return null;
          return (
            <Circle
              key={`${p.pathIndex}-${i}`}
              x={n.x}
              y={n.y}
              radius={r}
              fill={isCurve(n.node_type) ? "#6464ff" : "#ffc800"}
              stroke="#1f2937"
              strokeWidth={sw}
              draggable
              onDragStart={(e) => {
                dragStart.current = { x: e.target.x(), y: e.target.y() };
              }}
              onDragEnd={(e) => {
                const s = dragStart.current;
                dragStart.current = null;
                if (!s) return;
                const dx = e.target.x() - s.x;
                const dy = e.target.y() - s.y;
                if (dx === 0 && dy === 0) return;
                void applyPathEdit(() =>
                  movePathNode(selectedIndex, p.pathIndex, i, dx, dy),
                );
              }}
              onMouseEnter={(e) => {
                const stage = e.target.getStage();
                if (stage) stage.container().style.cursor = "move";
              }}
              onMouseLeave={(e) => {
                const stage = e.target.getStage();
                if (stage) stage.container().style.cursor = "default";
              }}
            />
          );
        }),
      )}
    </Layer>
  );
}
