import { useEffect, useRef, useState } from "react";
import { Layer, Circle, Line, Rect } from "react-konva";
import Konva from "konva";
import { useDocumentStore } from "../state/documentStore";
import {
  getObjectPaths,
  getPathNodes,
  movePathNode,
  movePathHandle,
  insertPathNode,
  deletePathNode,
} from "../engine/EngineClient";
import { PATH_CMD } from "../engine/types";
import type { PathNode } from "../engine/types";
import { useView, layerTransform } from "./viewContext";
import { usePathNodeMenu } from "./PathNodeMenu";

interface PathNodes {
  pathIndex: number;
  nodes: PathNode[];
}

interface Handle {
  x: number;
  y: number;
  ax: number; // anchor (control-arm start)
  ay: number;
  cmdIndex: number;
  cpSlot: 1 | 2;
}

const isCurve = (t: number) =>
  t === PATH_CMD.bezierTo || t === PATH_CMD.quadBezierTo;

/** The bezier handles attached to anchor `i`: the incoming control (cp2 of
 *  command i) and the outgoing one (next command's cp1, or cp2 for a quad). */
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

/** Point at s∈[0,1] on the segment from anchor `a` to anchor `b` (b carries the
 *  segment's command type/handles); null if b isn't a drawable segment end. */
function ptOnNodes(
  a: PathNode,
  b: PathNode,
  s: number,
): { x: number; y: number } | null {
  if (!a || !b) return null;
  if (b.node_type === PATH_CMD.lineTo) {
    return { x: a.x + (b.x - a.x) * s, y: a.y + (b.y - a.y) * s };
  }
  const u = 1 - s;
  if (b.node_type === PATH_CMD.bezierTo) {
    return {
      x: u * u * u * a.x + 3 * u * u * s * b.cp1x + 3 * u * s * s * b.cp2x + s * s * s * b.x,
      y: u * u * u * a.y + 3 * u * u * s * b.cp1y + 3 * u * s * s * b.cp2y + s * s * s * b.y,
    };
  }
  if (b.node_type === PATH_CMD.quadBezierTo) {
    return {
      x: u * u * a.x + 2 * u * s * b.cp2x + s * s * b.x,
      y: u * u * a.y + 2 * u * s * b.cp2y + s * s * b.y,
    };
  }
  return null; // moveTo / close / arc → not a drawable segment
}

function segPoints(a: PathNode, b: PathNode, samples = 18): number[] {
  const pts: number[] = [];
  for (let j = 0; j <= samples; j++) {
    const pt = ptOnNodes(a, b, j / samples);
    if (pt) pts.push(pt.x, pt.y);
  }
  return pts;
}

/** A copy of `n` with a delta added to the chosen fields. */
function shifted(
  n: PathNode,
  dx: number,
  dy: number,
  anchor: boolean,
  cp1: boolean,
  cp2: boolean,
): PathNode {
  return {
    ...n,
    x: anchor ? n.x + dx : n.x,
    y: anchor ? n.y + dy : n.y,
    cp1x: cp1 ? n.cp1x + dx : n.cp1x,
    cp1y: cp1 ? n.cp1y + dy : n.cp1y,
    cp2x: cp2 ? n.cp2x + dx : n.cp2x,
    cp2y: cp2 ? n.cp2y + dy : n.cp2y,
  };
}

const SAMPLES = 18;

/** PathEdit mode: draggable anchors + bezier handles over the selected object.
 *  The path outline updates live during a drag (the engine recomputes the
 *  stitch fill on release). Double-click a segment to insert a node; Delete
 *  removes the selected node. */
export default function PathEditLayer() {
  const view = useView();
  const doc = useDocumentStore((s) => s.doc);
  const imageVersion = useDocumentStore((s) => s.imageVersion);
  const selectedIndex = useDocumentStore((s) => s.selectedIndex);
  const applyPathEdit = useDocumentStore((s) => s.applyPathEdit);
  const busy = useDocumentStore((s) => s.busy);
  const showNodeMenu = usePathNodeMenu((s) => s.show);

  const [paths, setPaths] = useState<PathNodes[]>([]);
  const [sel, setSel] = useState<{ p: number; i: number } | null>(null);
  const dragStart = useRef<{ x: number; y: number } | null>(null);
  const armRefs = useRef<(Konva.Line | null)[]>([]);
  const handleRefs = useRef<(Konva.Circle | null)[]>([]);
  const segRefs = useRef<Map<string, Konva.Line>>(new Map());

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

  // Delete/Backspace deletes the selected node (App.tsx defers in pathEdit).
  useEffect(() => {
    if (!sel) return;
    const onKey = (e: KeyboardEvent) => {
      const tag = (e.target as HTMLElement)?.tagName;
      if (tag === "INPUT" || tag === "TEXTAREA") return;
      if (e.key === "Delete" || e.key === "Backspace") {
        e.preventDefault();
        if (useDocumentStore.getState().busy) return; // edit in flight
        const cur = sel;
        void applyPathEdit(() => deletePathNode(selectedIndex, cur.p, cur.i));
        setSel({ p: cur.p, i: Math.max(0, cur.i - 1) });
      }
    };
    window.addEventListener("keydown", onKey);
    return () => window.removeEventListener("keydown", onKey);
  }, [sel, selectedIndex, applyPathEdit]);

  if (selectedIndex < 0 || !obj) return null;

  const r = 4.5 / view.zoom;
  const sw = 1.2 / view.zoom;
  const hr = 4 / view.zoom; // handle radius
  const arm = 1 / view.zoom;

  const selPath = sel ? paths.find((p) => p.pathIndex === sel.p) : undefined;
  const handles = selPath ? handlesFor(selPath.nodes, sel!.i) : [];

  // editable segments (faint guide outline + double-click target to insert)
  const segments: {
    p: number;
    k: number;
    i: number;
    nodes: PathNode[];
    pts: number[];
  }[] = [];
  for (const pth of paths) {
    for (let i = 0; i < pth.nodes.length - 1; i++) {
      if (!ptOnNodes(pth.nodes[i], pth.nodes[i + 1], 0)) continue; // not a segment
      segments.push({
        p: pth.pathIndex,
        k: i + 1,
        i,
        nodes: pth.nodes,
        pts: segPoints(pth.nodes[i], pth.nodes[i + 1], SAMPLES),
      });
    }
  }

  const setSegPts = (p: number, k: number, a: PathNode, b: PathNode) =>
    segRefs.current.get(`${p}:${k}`)?.points(segPoints(a, b, SAMPLES));

  // live redraw of the two segments around a dragged anchor, plus its handles
  const onAnchorDragMove = (
    e: Konva.KonvaEventObject<DragEvent>,
    p: number,
    i: number,
  ) => {
    const ds = dragStart.current;
    if (!ds) return;
    const dx = e.target.x() - ds.x;
    const dy = e.target.y() - ds.y;
    const pth = paths.find((pp) => pp.pathIndex === p);
    if (!pth) return;
    const nodes = pth.nodes;
    // moved anchor carries its incoming control (cp2) with it
    const mi = shifted(nodes[i], dx, dy, true, false, true);
    if (i >= 1) setSegPts(p, i, nodes[i - 1], mi); // segment ending at i
    const nx = nodes[i + 1];
    if (nx) {
      const cubic = nx.node_type === PATH_CMD.bezierTo;
      const quad = nx.node_type === PATH_CMD.quadBezierTo;
      // outgoing control moves with the anchor (quad also carries its start cp1)
      const mNext = shifted(nx, dx, dy, false, cubic || quad, quad);
      setSegPts(p, i + 1, mi, mNext); // segment starting at i
    }
    // drag the selected node's handle dots + arms along too
    if (sel?.p === p && sel?.i === i) {
      const lx = e.target.x();
      const ly = e.target.y();
      handles.forEach((h, k) => {
        const hx = h.x + dx;
        const hy = h.y + dy;
        handleRefs.current[k]?.position({ x: hx, y: hy });
        armRefs.current[k]?.points([lx, ly, hx, hy]);
      });
    }
    e.target.getLayer()?.batchDraw();
  };

  // shared drag/select handlers for an anchor (square or circle)
  const anchorHandlers = (p: number, i: number, nodeType: number) => ({
    draggable: true,
    onClick: () => setSel({ p, i }),
    onTap: () => setSel({ p, i }),
    onContextMenu: (e: Konva.KonvaEventObject<MouseEvent>) => {
      e.evt.preventDefault();
      e.cancelBubble = true;
      // a subpath start (moveTo) / close marker has no incoming segment to convert
      if (nodeType === PATH_CMD.moveTo || nodeType === PATH_CMD.close) {
        usePathNodeMenu.getState().hide();
        return;
      }
      setSel({ p, i });
      showNodeMenu({
        x: e.evt.clientX,
        y: e.evt.clientY,
        pathIndex: p,
        nodeIndex: i,
        isCurve: isCurve(nodeType),
      });
    },
    onDragStart: (e: Konva.KonvaEventObject<DragEvent>) => {
      setSel({ p, i });
      dragStart.current = { x: e.target.x(), y: e.target.y() };
    },
    onDragMove: (e: Konva.KonvaEventObject<DragEvent>) =>
      onAnchorDragMove(e, p, i),
    onDragEnd: (e: Konva.KonvaEventObject<DragEvent>) => {
      const s = dragStart.current;
      dragStart.current = null;
      if (!s) return;
      const dx = e.target.x() - s.x;
      const dy = e.target.y() - s.y;
      if (dx === 0 && dy === 0) return;
      void applyPathEdit(() => movePathNode(selectedIndex, p, i, dx, dy));
    },
    onMouseEnter: (e: Konva.KonvaEventObject<MouseEvent>) => {
      const st = e.target.getStage();
      if (st) st.container().style.cursor = "move";
    },
    onMouseLeave: (e: Konva.KonvaEventObject<MouseEvent>) => {
      const st = e.target.getStage();
      if (st) st.container().style.cursor = "default";
    },
  });

  return (
    // ignore pointer input while an edit is in flight → no gesture on stale nodes
    <Layer {...layerTransform(view)} listening={!busy}>
      {/* editable path outline; double-click a segment to insert a node */}
      {segments.map((sg) => (
        <Line
          key={`seg-${sg.p}-${sg.k}`}
          ref={(node) => {
            const key = `${sg.p}:${sg.k}`;
            if (node) segRefs.current.set(key, node);
            else segRefs.current.delete(key);
          }}
          points={sg.pts}
          stroke="rgba(255,255,255,0.18)"
          strokeWidth={1 / view.zoom}
          hitStrokeWidth={9 / view.zoom}
          onDblClick={(e) => {
            const w = e.target.getLayer()?.getRelativePointerPosition();
            if (!w) return;
            let bestS = 0.5;
            let bestD = Infinity;
            const M = 40;
            for (let j = 0; j <= M; j++) {
              const s = j / M;
              const pt = ptOnNodes(sg.nodes[sg.i], sg.nodes[sg.k], s);
              if (!pt) continue;
              const ddx = pt.x - w.x;
              const ddy = pt.y - w.y;
              const d = ddx * ddx + ddy * ddy;
              if (d < bestD) {
                bestD = d;
                bestS = s;
              }
            }
            if (bestS < 0.06 || bestS > 0.94) return; // too close to an anchor
            void applyPathEdit(() =>
              insertPathNode(selectedIndex, sg.p, sg.k, bestS),
            );
            setSel({ p: sg.p, i: sg.k }); // inserted node lands at index k
          }}
        />
      ))}

      {/* control arms + handles for the selected node (under the anchors) */}
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
          ref={(node) => {
            handleRefs.current[k] = node;
          }}
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
            // arm follows the handle, and the segment it controls redraws live
            armRefs.current[k]?.points([h.ax, h.ay, e.target.x(), e.target.y()]);
            if (selPath && sel) {
              const a0 = selPath.nodes[h.cmdIndex - 1];
              const b0 = selPath.nodes[h.cmdIndex];
              if (a0 && b0) {
                const b = { ...b0 };
                if (h.cpSlot === 1) {
                  b.cp1x = e.target.x();
                  b.cp1y = e.target.y();
                } else {
                  b.cp2x = e.target.x();
                  b.cp2y = e.target.y();
                }
                setSegPts(sel.p, h.cmdIndex, a0, b);
              }
            }
            e.target.getLayer()?.batchDraw();
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

      {/* anchors for every visible path — square = corner, circle = curve */}
      {paths.map((p) =>
        p.nodes.map((n, i) => {
          if (n.node_type === PATH_CMD.close) return null;
          const isSel = sel?.p === p.pathIndex && sel?.i === i;
          const rad = isSel ? r * 1.35 : r;
          const handlers = anchorHandlers(p.pathIndex, i, n.node_type);
          const style = {
            fill: isCurve(n.node_type) ? "#6464ff" : "#ffc800",
            stroke: isSel ? "#ff2dff" : "#1f2937",
            strokeWidth: isSel ? sw * 1.8 : sw,
          };
          if (isCurve(n.node_type)) {
            return (
              <Circle
                key={`${p.pathIndex}-${i}`}
                x={n.x}
                y={n.y}
                radius={rad}
                {...style}
                {...handlers}
              />
            );
          }
          const side = rad * 2;
          return (
            <Rect
              key={`${p.pathIndex}-${i}`}
              x={n.x}
              y={n.y}
              width={side}
              height={side}
              offsetX={side / 2}
              offsetY={side / 2}
              {...style}
              {...handlers}
            />
          );
        }),
      )}
    </Layer>
  );
}
