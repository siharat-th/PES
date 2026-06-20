import { useCallback, useEffect, useRef, useState } from "react";
import { Stage } from "react-konva";
import Konva from "konva";
import GridLayer from "./GridLayer";
import ObjectsLayer from "./ObjectsLayer";
import StitchLayer from "./StitchLayer";
import PathEditLayer from "./PathEditLayer";
import StitchEditLayer from "./StitchEditLayer";
import { ViewContext } from "./viewContext";
import { useDocumentStore } from "../state/documentStore";
import { useViewportStore } from "../state/viewportStore";
import { useUiStore } from "../state/uiStore";
import { UNITS_PER_MM } from "../engine/types";

const MIN_ZOOM = 0.05;
const MAX_ZOOM = 40;

/** Workspace canvas. World coordinates = engine units (0.1 mm), origin at
 *  hoop center. Wheel = zoom about cursor, middle-drag or space+drag = pan. */
export default function EmbroideryStage() {
  const containerRef = useRef<HTMLDivElement>(null);
  const stageRef = useRef<Konva.Stage>(null);
  const [size, setSize] = useState({ width: 0, height: 0 });
  const view = useViewportStore();
  const setView = useViewportStore((s) => s.setView);
  const panState = useRef<{ startX: number; startY: number } | null>(null);
  const spaceDown = useRef(false);

  const doc = useDocumentStore((s) => s.doc);
  const select = useDocumentStore((s) => s.select);
  const viewMode = useUiStore((s) => s.viewMode);
  const hoopWMm = doc?.hoop_width_mm ?? 100;
  const hoopHMm = doc?.hoop_height_mm ?? 100;

  useEffect(() => {
    const el = containerRef.current;
    if (!el) return;
    const ro = new ResizeObserver(() =>
      setSize({ width: el.clientWidth, height: el.clientHeight }),
    );
    ro.observe(el);
    setSize({ width: el.clientWidth, height: el.clientHeight });
    return () => ro.disconnect();
  }, []);

  const fitToHoop = useCallback(() => {
    if (size.width === 0 || size.height === 0) return;
    const hw = hoopWMm * UNITS_PER_MM;
    const hh = hoopHMm * UNITS_PER_MM;
    const zoom = Math.min(size.width / (hw * 1.15), size.height / (hh * 1.15));
    setView({ zoom: Math.min(Math.max(zoom, MIN_ZOOM), MAX_ZOOM), x: 0, y: 0 });
  }, [size, hoopWMm, hoopHMm]);

  // fit when the document/hoop changes, on first layout, or on request
  const fitKey = `${doc ? 1 : 0}-${hoopWMm}x${hoopHMm}-${size.width > 0}-${view.fitRequest}`;
  const lastFitKey = useRef("");
  useEffect(() => {
    if (lastFitKey.current !== fitKey) {
      lastFitKey.current = fitKey;
      fitToHoop();
    }
  }, [fitKey, fitToHoop]);

  useEffect(() => {
    const down = (e: KeyboardEvent) => {
      if (e.code === "Space") spaceDown.current = true;
    };
    const up = (e: KeyboardEvent) => {
      if (e.code === "Space") spaceDown.current = false;
    };
    window.addEventListener("keydown", down);
    window.addEventListener("keyup", up);
    return () => {
      window.removeEventListener("keydown", down);
      window.removeEventListener("keyup", up);
    };
  }, []);

  const handleWheel = (e: Konva.KonvaEventObject<WheelEvent>) => {
    e.evt.preventDefault();
    const pointer = stageRef.current?.getPointerPosition();
    if (!pointer) return;

    const oldZoom = view.zoom;
    const factor = e.evt.deltaY > 0 ? 1 / 1.08 : 1.08;
    const zoom = Math.min(Math.max(oldZoom * factor, MIN_ZOOM), MAX_ZOOM);

    // keep the world point under the cursor fixed
    const cx = size.width / 2 + view.x;
    const cy = size.height / 2 + view.y;
    const worldX = (pointer.x - cx) / oldZoom;
    const worldY = (pointer.y - cy) / oldZoom;
    setView({
      zoom,
      x: pointer.x - worldX * zoom - size.width / 2,
      y: pointer.y - worldY * zoom - size.height / 2,
    });
  };

  const handleMouseDown = (e: Konva.KonvaEventObject<MouseEvent>) => {
    if (e.evt.button === 1 || spaceDown.current) {
      e.evt.preventDefault();
      panState.current = {
        startX: e.evt.clientX - view.x,
        startY: e.evt.clientY - view.y,
      };
      return;
    }
    // keep the selection while editing nodes/stitches (the edit layers need it)
    if (e.target === e.target.getStage() && viewMode === "design") select(-1);
  };

  const handleMouseMove = (e: Konva.KonvaEventObject<MouseEvent>) => {
    const pan = panState.current;
    if (!pan) return;
    setView({
      zoom: view.zoom,
      x: e.evt.clientX - pan.startX,
      y: e.evt.clientY - pan.startY,
    });
  };

  return (
    <div
      ref={containerRef}
      className={`h-full w-full overflow-hidden transition-colors ${
        viewMode === "design" ? "bg-neutral-300" : "bg-neutral-800"
      }`}
    >
      <ViewContext.Provider
        value={{
          ...view,
          centerX: size.width / 2,
          centerY: size.height / 2,
        }}
      >
        <Stage
          ref={stageRef}
          width={size.width}
          height={size.height}
          onWheel={handleWheel}
          onMouseDown={handleMouseDown}
          onMouseMove={handleMouseMove}
          onMouseUp={() => (panState.current = null)}
          onMouseLeave={() => (panState.current = null)}
        >
          <GridLayer hoopWMm={hoopWMm} hoopHMm={hoopHMm} mode={viewMode} />
          {viewMode === "design" && <ObjectsLayer />}
          {viewMode === "stitch" && <StitchLayer />}
          {viewMode === "pathEdit" && (
            <>
              <ObjectsLayer readOnly />
              <PathEditLayer />
            </>
          )}
          {/* StitchEdit shows the stitches as editable thread lines (no texture) */}
          {viewMode === "stitchEdit" && <StitchEditLayer />}
        </Stage>
      </ViewContext.Provider>
    </div>
  );
}
