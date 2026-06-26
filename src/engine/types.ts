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
  /** has real stitches → render as a PNG; false → render as a crisp vector */
  has_stitches: boolean;
  object_type: string;
  text: string;
  /** layer-group membership; 0 = ungrouped (see GroupSnapshot) */
  group_id: number;
}

/** One drawable path of a scalable object, as a Konva-ready SVG `d` + paint
 *  (absolute world coords, engine units). See engine `objectVectorJson`. */
export interface VectorPath {
  d: string;
  /** path bounding box in world coords [x, y, w, h] (for gradient placement) */
  bbox: [number, number, number, number];
  fillRule: "evenodd" | "nonzero";
  fill?: string;
  fillOpacity?: number;
  stroke?: string;
  strokeOpacity?: number;
  strokeWidth?: number;
}

export interface ObjectVector {
  paths: VectorPath[];
}

/** A layer group (folder-like; organizational metadata, not an object). */
export interface GroupSnapshot {
  id: number;
  parent_id: number;
  name: string;
  collapsed: boolean;
  order: number;
  /** derived: false if any member object is non-scalable */
  scalable: boolean;
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
  groups: GroupSnapshot[];
  can_undo: boolean;
  can_redo: boolean;
}

/** pesPath::Command::Type. */
export const PATH_CMD = {
  moveTo: 0,
  lineTo: 1,
  curveTo: 2,
  bezierTo: 3,
  quadBezierTo: 4,
  arc: 5,
  arcNegative: 6,
  close: 7,
} as const;

/** One editable path command, coordinates in engine units (world space). */
export interface PathNode {
  node_type: number;
  x: number;
  y: number;
  cp1x: number;
  cp1y: number;
  cp2x: number;
  cp2y: number;
}

/** A single needle point (world space) for StitchEdit. */
export interface StitchPoint {
  x: number;
  y: number;
  jump: boolean;
}

/** Stitch block kinds (which vector of an object holds the block). */
export const STITCH_KIND = { fill: 0, stroke: 1 } as const;

/** One stitch block for StitchEdit; `block_index` indexes its kind's vector. */
export interface StitchEditBlock {
  kind: number;
  block_index: number;
  hex: string;
  points: StitchPoint[];
}
