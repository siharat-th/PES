import { useEffect, useRef, useState } from "react";
import { Layer, Circle, Line } from "react-konva";
import Konva from "konva";
import { useDocumentStore } from "../state/documentStore";
import {
  getObjectPaths,
  getPathNodes,
  movePathNode,
  movePathHandle,
} from "../engine/EngineClient";
import { PATH_CMD } from "../engine/types";
import type { PathNode } from "../engine/types";
import { useView, layerTransform } from "./viewContext";

interface PathNodes {
  pathIndex: number;
  nodes: PathNode[];
}

interface Handle {
  x: number;
  y: number;
  ax: number; // anchor (for the control arm)
  ay: number;
  cmdIndex: number;
  cpSlot: 1 | 2;
}

const isCurve = (t: number) =>
  t === PATH_CMD.bezierTo || t === PATH_CMD.quadBezierTo;

/** The bezier handles attached to anchor `i`: the incoming control point
 *  (cp2 of command i) and the outgoing one (next command's cp1, or cp2 for
 *  a quad). Coordinates are world units, matching the anchors. */
function handlesFor(nodes: PathNode[], i: number): Handle[] {
  const n = nodes[i];
  if (!n) return [];
  const out: Handle[] = [];
  if (isCurve(n.node_type)) {
    out.push({ x: n.cp2x, y: n.cp2y, ax: n.x, ay: n.y, cmdIndex: i, cpSlot: 2 });
  }
  const nx = nodes[i + 1];
  if (nx) {
    if (nx.node_type === PATH_CMD.bezierTo) {
      out.push({ x: nx.cp1x, y: nx.cp1y, ax: n.x, ay: n.y, cmdIndex: i + 1, cpSlot: 1 });
    } else if (nx.node_type === PATH_CMD.quadBezierTo) {
      out.push({ x: nx.cp2x, y: nx.cp2y, ax: n.x, ay: n.y, cmdIndex: i + 1, cpSlot: 2 });
    }
  }
  return out;
}

/** PathEdit mode: draggable anchor handles over the selected object's path
 *  nodes. Click an anchor to reveal its bezier control handles. Drags preview
 *  locally and commit a world delta on release; the engine moves the point
 *  and regenerates stitches. */
export default function PathEditLayer() {
  const view = useView();
  const doc = useDocumentStore((s) => s.doc);
  const imageVersion = useDocumentStore((s) => s.imageVersion);
  const selectedIndex = useDocumentStore((s) => s.selectedIndex);
  const applyPathEdit = useDocumentStore((s) => s.applyPathEdit);

  const [paths, setPaths] = useState<PathNodes[]>([]);
  const [sel, setSel] = useState<{ p: number; i: number } | null>(null);
  const dragStart = useRef<{ x: number; y: number } | null>(null);
  const armRefs = useRef<(Konva.Line | null)[]>([]);

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

  // a different object → forget the node selection
  useEffect(() => {
    setSel(null);
  }, [selectedIndex]);

  if (selectedIndex < 0 || !obj) return null;

  const r = 4.5 / view.zoom;
  const sw = 1.2 / view.zoom;
  const hr = 4 / view.zoom; // handle radius
  const arm = 1 / view.zoom;

  const selPath = sel ? paths.find((p) => p.pathIndex === sel.p) : undefined;
  const handles = selPath ? handlesFor(selPath.nodes, sel!.i) : [];

  return (
    <Layer {...layerTransform(view)}>
      {/* control arms + handles for the selected node (drawn under anchors) */}
      {handles.map((h, k) => (
        <Line
          key={`arm-${k}`}
          ref={(node) => {
            armRefs.current[k] = node;
          }}
          points={[h.ax, h.ay, h.x, h.y]}
          stroke="#d000d0"
          strokeWidth={arm}
          listening={false}
        />
      ))}
      {handles.map((h, k) => (
        <Circle
          key={`h-${k}`}
          x={h.x}
          y={h.y}
          radius={hr}
          fill="#ff2dff"
          stroke="#ffffff"
          strokeWidth={sw}
          draggable
          onDragStart={(e) => {
            dragStart.current = { x: e.target.x(), y: e.target.y() };
          }}
          onDragMove={(e) => {
            // keep the control arm glued to the handle while dragging
            const ln = armRefs.current[k];
            if (ln) {
              ln.points([h.ax, h.ay, e.target.x(), e.target.y()]);
              ln.getLayer()?.batchDraw();
            }
          }}
          onDragEnd={(e) => {
            const s = dragStart.current;
            dragStart.current = null;
            if (!s || !sel) return;
            const dx = e.target.x() - s.x;
            const dy = e.target.y() - s.y;
            if (dx === 0 && dy === 0) return;
            void applyPathEdit(() =>
              movePathHandle(selectedIndex, sel.p, h.cmdIndex, h.cpSlot, dx, dy),
            );
          }}
        />
      ))}

      {/* anchors for every visible path */}
      {paths.map((p) =>
        p.nodes.map((n, i) => {
          if (n.node_type === PATH_CMD.close) return null;
          const isSel = sel?.p === p.pathIndex && sel?.i === i;
          return (
            <Circle
              key={`${p.pathIndex}-${i}`}
              x={n.x}
              y={n.y}
              radius={isSel ? r * 1.35 : r}
              fill={isCurve(n.node_type) ? "#6464ff" : "#ffc800"}
              stroke={isSel ? "#ff2dff" : "#1f2937"}
              strokeWidth={isSel ? sw * 1.8 : sw}
              draggable
              onClick={() => setSel({ p: p.pathIndex, i })}
              onTap={() => setSel({ p: p.pathIndex, i })}
              onDragStart={(e) => {
                setSel({ p: p.pathIndex, i });
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
                const st = e.target.getStage();
                if (st) st.container().style.cursor = "move";
              }}
              onMouseLeave={(e) => {
                const st = e.target.getStage();
                if (st) st.container().style.cursor = "default";
              }}
            />
          );
        }),
      )}
    </Layer>
  );
}
