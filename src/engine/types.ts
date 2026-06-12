/** Engine units are 0.1 mm ("pes units"): 10 units = 1 mm. */
export const UNITS_PER_MM = 10;

export interface ObjectSnapshot {
  index: number;
  x: number;
  y: number;
  width: number;
  height: number;
  rotate_degree: number;
  visible: boolean;
  locked: boolean;
  scalable: boolean;
  object_type: string;
  text: string;
}

export interface PathInfo {
  index: number;
  path_id: string;
  is_fill: boolean;
  is_stroke: boolean;
  fill_type: number;
  fill_color: string;
  stroke_color: string;
  stroke_width: number;
  visible: boolean;
}

export interface BrotherColor {
  index: number;
  hex: string;
  name: string;
}

export interface ColorBlockInfo {
  index: number;
  hex: string;
  brother_index: number;
  stitch_count: number;
}

export interface DocumentSnapshot {
  hoop_width_mm: number;
  hoop_height_mm: number;
  objects: ObjectSnapshot[];
  can_undo: boolean;
  can_redo: boolean;
}
