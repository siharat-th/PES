import { useEffect, useRef, useState } from "react";
import { Layer, Shape, Circle, Rect } from "react-konva";
import Konva from "konva";
import { useDocumentStore } from "../state/documentStore";
import {
  getStitchPoints,
  moveStitchPoint,
  insertStitchPoint,
  insertStitchPointAt,
  deleteStitchPoint,
} from "../engine/EngineClient";
import type { StitchEditBlock } from "../engine/types";
import { useView, layerTransform } from "./viewContext";

/** Which needle point is selected — addressed by engine-stable ids so it
 *  survives block re-ordering/dropping across refetches. */
interface Sel {
  kind: number;
  block: number;
  pi: number;
}

/** colour marking a jump stitch (the dot + the travel line into it) */
const JUMP_COLOR = "#ff8a3d";
/** dots get expensive past this; lines + selection still work without them */
const DOT_CAP = 20000;
/** skip the per-move hover scan past this (selection on click still works) */
const HOVER_CAP = 8000;
/** half-extent of the invisible interaction surface (engine units) */
const HIT = 200000;

function blockOf(blocks: StitchEditBlock[], s: Sel | null) {
  if (!s) return undefined;
  return blocks.find((b) => b.kind === s.kind && b.block_index === s.block);
}

/** StitchEdit mode: edit an object's raw needle points (move/insert/delete).
 *  All points + thread paths draw in one Shape (fast for big stitch counts);
 *  a transparent Rect catches clicks (nearest-point pick) and the selected
 *  point gets a draggable handle. The object's blocks ARE the data, so each
 *  edit commits straight to the engine — no regeneration. */
export default function StitchEditLayer() {
  const view = useView();
  const imageVersion = useDocumentStore((s) => s.imageVersion);
  const selectedIndex = useDocumentStore((s) => s.selectedIndex);
  const applyEdit = useDocumentStore((s) => s.applyPathEdit);
  const busy = useDocumentStore((s) => s.busy);

  const [blocks, setBlocks] = useState<StitchEditBlock[]>([]);
  const [sel, setSel] = useState<Sel | null>(null);
  const shapeRef = useRef<Konva.Shape>(null);
  const hoverRef = useRef<Konva.Circle>(null);
  const handleRef = useRef<Konva.Circle>(null);
  // active press-drag: node id, its original pos (o*) and the press pos (s*)
  const drag = useRef<{ sel: Sel; ox: number; oy: number; sx: number; sy: number } | null>(
    null,
  );
  // live drag preview: the dragged point's overridden world position
  const dragPos = useRef<{ x: number; y: number } | null>(null);

  // (re)load the selected object's stitch points
  useEffect(() => {
    if (selectedIndex < 0) {
      setBlocks([]);
      return;
    }
    let cancelled = false;
    void getStitchPoints(selectedIndex).then((b) => {
      if (!cancelled) setBlocks(b);
    });
    return () => {
      cancelled = true;
    };
  }, [selectedIndex, imageVersion]);

  // a different object → forget the point selection
  useEffect(() => setSel(null), [selectedIndex]);

  // redraw when zoom changes (line/dot sizes depend on it)
  useEffect(() => {
    shapeRef.current?.getLayer()?.batchDraw();
  }, [view.zoom, blocks, sel]);

  // commit the active press-drag (also fires if the mouse releases off-canvas)
  const finishDrag = () => {
    const d = drag.current;
    drag.current = null;
    const dp = dragPos.current;
    dragPos.current = null;
    if (!d || !dp) return;
    const dx = dp.x - d.ox;
    const dy = dp.y - d.oy;
    if (dx === 0 && dy === 0) return;
    void applyEdit(() =>
      moveStitchPoint(selectedIndex, d.sel.kind, d.sel.block, d.sel.pi, dx, dy),
    );
  };
  useEffect(() => {
    window.addEventListener("mouseup", finishDrag);
    return () => window.removeEventListener("mouseup", finishDrag);
  });

  const selBlock = blockOf(blocks, sel);
  const selPt =
    selBlock && sel && sel.pi < selBlock.points.length
      ? selBlock.points[sel.pi]
      : undefined;

  // Insert/Delete on the selected point (App defers Delete in stitchEdit).
  useEffect(() => {
    if (!sel) return;
    const onKey = (e: KeyboardEvent) => {
      const tag = (e.target as HTMLElement)?.tagName;
      if (tag === "INPUT" || tag === "TEXTAREA") return;
      if (useDocumentStore.getState().busy) return; // edit in flight
      const b = blockOf(blocks, sel);
      if (!b) return;
      if (e.key === "Delete" || e.key === "Backspace") {
        e.preventDefault();
        void applyEdit(() =>
          deleteStitchPoint(selectedIndex, sel.kind, sel.block, sel.pi),
        );
        // selection lands on the previous point (engine semantics)
        setSel(b.points.length > 1 ? { ...sel, pi: Math.max(0, sel.pi - 1) } : null);
      } else if (
        e.key === "i" || e.key === "I" || e.key === "+" || e.key === "Enter"
      ) {
        e.preventDefault();
        const last = b.points.length - 1;
        const newPi = sel.pi === last ? sel.pi : sel.pi + 1;
        void applyEdit(() =>
          insertStitchPoint(selectedIndex, sel.kind, sel.block, sel.pi),
        );
        setSel({ ...sel, pi: newPi });
      }
    };
    window.addEventListener("keydown", onKey);
    return () => window.removeEventListener("keydown", onKey);
  }, [sel, blocks, selectedIndex, applyEdit]);

  if (selectedIndex < 0) return null;

  const px = 1 / view.zoom;
  const total = blocks.reduce((n, b) => n + b.points.length, 0);
  const showDots = total <= DOT_CAP;
  const pick = 7 / view.zoom; // selection radius in world units

  // nearest point to (wx,wy) within `pick`; prefer topmost (stroke over fill)
  const findNearest = (wx: number, wy: number): Sel | null => {
    let best: Sel | null = null;
    let bestD = pick * pick;
    for (let bi = blocks.length - 1; bi >= 0; bi--) {
      const b = blocks[bi];
      for (let pi = 0; pi < b.points.length; pi++) {
        const dx = b.points[pi].x - wx;
        const dy = b.points[pi].y - wy;
        const d = dx * dx + dy * dy;
        if (d < bestD) {
          bestD = d;
          best = { kind: b.kind, block: b.block_index, pi };
        }
      }
    }
    return best;
  };

  const matchesSel = (b: StitchEditBlock, pi: number) =>
    sel?.kind === b.kind && sel?.block === b.block_index && sel?.pi === pi;

  const ptOf = (s: Sel) => {
    const b = blockOf(blocks, s);
    return b && s.pi < b.points.length ? b.points[s.pi] : null;
  };

  // nearest drawn segment (i, i+1) to a click, with the projected point on it
  const findSegment = (
    wx: number,
    wy: number,
  ): { sel: Sel; x: number; y: number } | null => {
    let best: { sel: Sel; x: number; y: number } | null = null;
    const lim = 10 / view.zoom;
    let bestD = lim * lim;
    for (let bi = blocks.length - 1; bi >= 0; bi--) {
      const b = blocks[bi];
      const pts = b.points;
      for (let i = 0; i < pts.length - 1; i++) {
        // A segment is a solid thread line (insertable) when the travel INTO
        // its end point isn't a jump — matches the renderer, which draws the
        // move OUT of a jump (e.g. the lead-in jump → first stitch) as a normal
        // line. Keying on pts[i].jump too would wrongly skip that first segment.
        if (pts[i + 1].jump) continue; // skip dashed jump-travel segments
        const ax = pts[i].x, ay = pts[i].y;
        const dx = pts[i + 1].x - ax, dy = pts[i + 1].y - ay;
        const len2 = dx * dx + dy * dy;
        let t = len2 > 0 ? ((wx - ax) * dx + (wy - ay) * dy) / len2 : 0;
        t = t < 0 ? 0 : t > 1 ? 1 : t;
        const px = ax + dx * t, py = ay + dy * t;
        const d = (px - wx) ** 2 + (py - wy) ** 2;
        if (d < bestD) {
          bestD = d;
          best = { sel: { kind: b.kind, block: b.block_index, pi: i }, x: px, y: py };
        }
      }
    }
    return best;
  };

  // Highlight the node under the cursor (no cursor change — the move cursor
  // would cover the very node you're aiming at).
  const updateHover = (e: Konva.KonvaEventObject<MouseEvent>) => {
    const w = e.target.getLayer()?.getRelativePointerPosition();
    const hov = w && total <= HOVER_CAP ? findNearest(w.x, w.y) : null;
    const h = hoverRef.current;
    if (!h) return;
    const b = hov && blockOf(blocks, hov);
    const p = b?.points[hov!.pi];
    if (p) {
      h.position({ x: p.x, y: p.y });
      h.visible(true);
    } else {
      h.visible(false);
    }
    h.getLayer()?.batchDraw(); // overlay layer only — never the stitch Shape
  };

  return (
    <>
    <Layer {...layerTransform(view)} listening={!busy}>
      {/* transparent interaction surface: press a node to grab+drag it, drag
          empty space to deselect, double-click a thread line to insert */}
      <Rect
        x={-HIT}
        y={-HIT}
        width={HIT * 2}
        height={HIT * 2}
        fill="rgba(0,0,0,0.001)"
        onMouseDown={(e) => {
          const w = e.target.getLayer()?.getRelativePointerPosition();
          if (!w) return;
          const hit = findNearest(w.x, w.y);
          setSel(hit);
          if (!hit) return;
          const p = ptOf(hit);
          if (!p) return;
          // grab immediately → click-and-drag in one motion
          drag.current = { sel: hit, ox: p.x, oy: p.y, sx: w.x, sy: w.y };
          dragPos.current = { x: p.x, y: p.y };
          hoverRef.current?.visible(false);
          hoverRef.current?.getLayer()?.batchDraw();
        }}
        onMouseMove={(e) => {
          if (drag.current) {
            const w = e.target.getLayer()?.getRelativePointerPosition();
            if (!w) return;
            const d = drag.current;
            dragPos.current = { x: d.ox + (w.x - d.sx), y: d.oy + (w.y - d.sy) };
            handleRef.current?.position(dragPos.current);
            shapeRef.current?.getLayer()?.batchDraw();
          } else {
            updateHover(e);
          }
        }}
        onMouseUp={finishDrag}
        onDblClick={(e) => {
          if (useDocumentStore.getState().busy) return;
          const w = e.target.getLayer()?.getRelativePointerPosition();
          if (!w) return;
          const seg = findSegment(w.x, w.y);
          if (!seg) return;
          drag.current = null; // cancel the spurious press-drag from the clicks
          dragPos.current = null;
          void applyEdit(() =>
            insertStitchPointAt(
              selectedIndex,
              seg.sel.kind,
              seg.sel.block,
              seg.sel.pi,
              seg.x,
              seg.y,
            ),
          );
          setSel({ ...seg.sel, pi: seg.sel.pi + 1 }); // select the new point
        }}
        onMouseLeave={() => {
          hoverRef.current?.visible(false);
          hoverRef.current?.getLayer()?.batchDraw();
        }}
      />

      {/* all thread paths + needle dots, drawn once (visual only) */}
      <Shape
        ref={shapeRef}
        listening={false}
        sceneFunc={(konvaCtx) => {
          const ctx = konvaCtx._context as CanvasRenderingContext2D;
          // resolve a point's drawn position (selected point follows the drag)
          const at = (b: StitchEditBlock, pi: number) =>
            dragPos.current && matchesSel(b, pi) ? dragPos.current : b.points[pi];
          // thread paths — drawn continuously THROUGH jump points. The travel
          // INTO a jump node (prev → jump) is a dashed jump-colour line so the
          // jump reads as a jump; the move OUT (jump → next stitch) is a normal
          // thread line. A leading jump node has no incoming segment, so nothing
          // is drawn before it. Consecutive same-style segments batch into one
          // stroke to stay fast on big blocks.
          ctx.lineCap = "round";
          ctx.lineJoin = "round";
          ctx.lineWidth = px;
          const jumpDash = [4 * px, 3 * px];
          for (const b of blocks) {
            const pts = b.points;
            let i = 0;
            while (i < pts.length - 1) {
              const jumpSeg = pts[i + 1].jump; // travel into a jump point
              ctx.beginPath();
              const p0 = at(b, i);
              ctx.moveTo(p0.x, p0.y);
              let j = i;
              while (j < pts.length - 1 && pts[j + 1].jump === jumpSeg) {
                const p = at(b, j + 1);
                ctx.lineTo(p.x, p.y);
                j++;
              }
              if (jumpSeg) {
                ctx.strokeStyle = JUMP_COLOR;
                ctx.globalAlpha = 0.95;
                ctx.setLineDash(jumpDash);
              } else {
                ctx.strokeStyle = b.hex;
                ctx.globalAlpha = 0.9;
                ctx.setLineDash([]);
              }
              ctx.stroke();
              i = j;
            }
          }
          ctx.setLineDash([]);
          ctx.globalAlpha = 1;
          // needle dots
          if (showDots) {
            const r = px * 2.1;
            ctx.lineWidth = px * 0.5;
            for (const b of blocks) {
              for (let pi = 0; pi < b.points.length; pi++) {
                if (matchesSel(b, pi)) continue; // drawn as the handle on top
                const p = at(b, pi);
                ctx.beginPath();
                ctx.arc(p.x, p.y, r, 0, Math.PI * 2);
                if (b.points[pi].jump) {
                  ctx.strokeStyle = JUMP_COLOR;
                  ctx.stroke();
                } else {
                  ctx.fillStyle = b.hex;
                  ctx.fill();
                  ctx.strokeStyle = "rgba(255,255,255,0.75)";
                  ctx.stroke();
                }
              }
            }
          }
        }}
      />

      {/* selected point — visual handle (dragging is driven from the Rect) */}
      {selPt && sel && (
        <Circle
          ref={handleRef}
          key={`${sel.kind}-${sel.block}-${sel.pi}`}
          x={selPt.x}
          y={selPt.y}
          radius={4.5 / view.zoom}
          fill="#ff2dff"
          stroke="#ffffff"
          strokeWidth={1.3 / view.zoom}
          listening={false}
        />
      )}
    </Layer>

    {/* hover ring on its own layer → moving it never repaints the stitch Shape */}
    <Layer {...layerTransform(view)} listening={false}>
      <Circle
        ref={hoverRef}
        x={0}
        y={0}
        radius={5 / view.zoom}
        fill="rgba(253,224,71,0.2)"
        stroke="#fde047"
        strokeWidth={1.8 / view.zoom}
        visible={false}
        listening={false}
      />
    </Layer>
    </>
  );
}
