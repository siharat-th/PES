import { createContext, useContext } from "react";

/** World→screen transform: screen = world * zoom + (centerX + x, centerY + y). */
export interface ViewTransform {
  x: number; // pan offset px (from stage center)
  y: number;
  zoom: number;
  centerX: number; // stage center px
  centerY: number;
}

export const ViewContext = createContext<ViewTransform>({
  x: 0,
  y: 0,
  zoom: 1,
  centerX: 0,
  centerY: 0,
});

export const useView = () => useContext(ViewContext);

/** Konva Layer props that place world-coordinate children on screen. */
export function layerTransform(v: ViewTransform) {
  return {
    x: v.centerX + v.x,
    y: v.centerY + v.y,
    scaleX: v.zoom,
    scaleY: v.zoom,
  };
}
