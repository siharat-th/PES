import { invoke, IS_TAURI } from "./transport";
import { webLoadInput, webExportBytes } from "./webEngine";
import type {
  BrotherColor,
  ColorBlockInfo,
  DocumentSnapshot,
  ObjectVector,
  PathInfo,
  PathNode,
  StitchEditBlock,
} from "./types";

export { IS_TAURI };

/** Web file-open: load a picked file's bytes into the engine (no filesystem
 *  path on the web — the desktop build uses openFile(path) instead). */
export async function openDocumentBytes(
  filename: string,
  bytes: Uint8Array,
): Promise<DocumentSnapshot> {
  const ext = filename.split(".").pop()?.toLowerCase() ?? "";
  const kind = ext === "pes" ? "pes" : ext === "svg" ? "svg" : "ppes";
  return webLoadInput(kind, bytes);
}

/** Web file-save: exported bytes for a format, for a browser download. */
export async function exportDocumentBytes(format: string): Promise<Uint8Array> {
  return webExportBytes(format);
}

export async function newDocument(
  hoopWMm = 100,
  hoopHMm = 100,
): Promise<DocumentSnapshot> {
  return invoke("new_document", { hoopWMm, hoopHMm });
}

export async function openFile(path: string): Promise<DocumentSnapshot> {
  return invoke("open_file", { path });
}

export async function getDocument(): Promise<DocumentSnapshot> {
  return invoke("get_document");
}

/** PNG bytes of one object's rendered stitches. */
export async function getObjectImageBitmap(
  index: number,
): Promise<ImageBitmap | null> {
  const buf = await invoke<ArrayBuffer>("get_object_image", { index });
  if (!buf || buf.byteLength === 0) return null;
  return createImageBitmap(new Blob([buf], { type: "image/png" }));
}

/** Commit a gesture: deltas in engine units, absolute rotation in degrees. */
export async function transformObject(
  index: number,
  dx: number,
  dy: number,
  sx: number,
  sy: number,
  rotateDegree: number,
): Promise<DocumentSnapshot> {
  return invoke("transform_object", { index, dx, dy, sx, sy, rotateDegree });
}

export async function exportFile(path: string, format: string): Promise<void> {
  return invoke("export_file", { path, format });
}

export async function deleteObject(index: number): Promise<DocumentSnapshot> {
  return invoke("delete_object", { index });
}

export async function duplicateObject(
  index: number,
): Promise<DocumentSnapshot> {
  return invoke("duplicate_object", { index });
}

/** Brother shape indices accepted by add_shape (see SHAPE_* in the engine). */
export const SHAPE = {
  line: 0,
  triangle: 1,
  rect: 2,
  ellipse: 8,
} as const;

/** Drop a ready-made parametric shape at the hoop center; returns the fresh
 *  snapshot (the new object is the last one). */
export async function addShape(shapeIndex: number): Promise<DocumentSnapshot> {
  return invoke("add_shape", { shapeIndex });
}

/** Vector geometry (SVG paths + paint) for crisp Konva rendering of a scalable
 *  shape that has no stitches yet. */
export async function getObjectVector(index: number): Promise<ObjectVector> {
  const json = await invoke<string>("get_object_vector", { index });
  return JSON.parse(json);
}

export interface DuplicateResult {
  snapshot: DocumentSnapshot;
  new_indices: number[];
  /** id of the new group when `groupName` was passed, else -1 */
  group_id: number;
}

/** Duplicate several objects in one undo step. Pass `groupName` to place the
 *  copies into a fresh group (duplicating a whole group). */
export async function duplicateObjects(
  indices: number[],
  groupName?: string,
): Promise<DuplicateResult> {
  return invoke("duplicate_objects", {
    indices,
    groupName: groupName ?? null,
  });
}

export async function setObjectVisible(
  index: number,
  visible: boolean,
): Promise<DocumentSnapshot> {
  return invoke("set_object_visible", { index, visible });
}

export async function setObjectLocked(
  index: number,
  locked: boolean,
): Promise<DocumentSnapshot> {
  return invoke("set_object_locked", { index, locked });
}

/** dir > 0 = toward front (drawn later), dir < 0 = toward back. */
export async function reorderObject(
  index: number,
  dir: number,
): Promise<DocumentSnapshot> {
  return invoke("reorder_object", { index, dir });
}

/** Move an object to an arbitrary list position (drag-and-drop reorder).
 *  Indices are list positions: 0 = back-most, count-1 = front-most. */
export async function reorderObjectTo(
  from: number,
  to: number,
): Promise<DocumentSnapshot> {
  return invoke("reorder_object_to", { from, to });
}

// --- Layer groups ---------------------------------------------------------

/** Create a group; if memberIndices is non-empty, move those objects into it. */
export async function createGroup(
  name: string,
  memberIndices: number[] = [],
): Promise<DocumentSnapshot> {
  return invoke("create_group", { name, memberIndices });
}

export async function renameGroup(
  id: number,
  name: string,
): Promise<DocumentSnapshot> {
  return invoke("rename_group", { id, name });
}

/** Ungroup: drop the group, members revert to ungrouped (objects kept). */
export async function ungroup(id: number): Promise<DocumentSnapshot> {
  return invoke("ungroup", { id });
}

/** Delete a group and all of its member objects. */
export async function deleteGroup(id: number): Promise<DocumentSnapshot> {
  return invoke("delete_group", { id });
}

export async function addToGroup(
  id: number,
  indices: number[],
): Promise<DocumentSnapshot> {
  return invoke("add_to_group", { id, indices });
}

export async function removeFromGroup(
  indices: number[],
): Promise<DocumentSnapshot> {
  return invoke("remove_from_group", { indices });
}

export async function setGroupCollapsed(
  id: number,
  collapsed: boolean,
): Promise<DocumentSnapshot> {
  return invoke("set_group_collapsed", { id, collapsed });
}

export async function setGroupVisible(
  id: number,
  visible: boolean,
): Promise<DocumentSnapshot> {
  return invoke("set_group_visible", { id, visible });
}

export async function setGroupLocked(
  id: number,
  locked: boolean,
): Promise<DocumentSnapshot> {
  return invoke("set_group_locked", { id, locked });
}

export async function getObjectPaths(index: number): Promise<PathInfo[]> {
  return invoke("get_object_paths", { index });
}

/** Path command nodes (world coords) for PathEdit mode. */
export async function getPathNodes(
  index: number,
  pathIndex: number,
): Promise<PathNode[]> {
  return invoke("get_path_nodes", { index, pathIndex });
}

/** Move one path node by a world-space delta; returns the fresh snapshot. */
export async function movePathNode(
  index: number,
  pathIndex: number,
  nodeIndex: number,
  dx: number,
  dy: number,
): Promise<DocumentSnapshot> {
  return invoke("move_path_node", { index, pathIndex, nodeIndex, dx, dy });
}

/** Move one bezier control point (cpSlot: 1=cp1, 2=cp2 of cmdIndex). */
export async function movePathHandle(
  index: number,
  pathIndex: number,
  cmdIndex: number,
  cpSlot: number,
  dx: number,
  dy: number,
): Promise<DocumentSnapshot> {
  return invoke("move_path_handle", { index, pathIndex, cmdIndex, cpSlot, dx, dy });
}

/** Insert a node on the segment ending at nodeIndex, at parameter t in (0,1). */
export async function insertPathNode(
  index: number,
  pathIndex: number,
  nodeIndex: number,
  t: number,
): Promise<DocumentSnapshot> {
  return invoke("insert_path_node", { index, pathIndex, nodeIndex, t });
}

/** Delete the node at nodeIndex. */
export async function deletePathNode(
  index: number,
  pathIndex: number,
  nodeIndex: number,
): Promise<DocumentSnapshot> {
  return invoke("delete_path_node", { index, pathIndex, nodeIndex });
}

/** Convert a node's incoming segment between corner (line) and curve (bezier).
 *  toCurve=true seeds shape-preserving handles; false drops to a straight line. */
export async function setPathNodeType(
  index: number,
  pathIndex: number,
  nodeIndex: number,
  toCurve: boolean,
): Promise<DocumentSnapshot> {
  return invoke("set_path_node_type", { index, pathIndex, nodeIndex, toCurve });
}

/** Needle points of an object's stitch blocks (world coords) for StitchEdit. */
export async function getStitchPoints(
  index: number,
): Promise<StitchEditBlock[]> {
  return invoke("get_stitch_points", { index });
}

/** Move one needle point by a world delta (kind 0=fill, 1=stroke). */
export async function moveStitchPoint(
  index: number,
  kind: number,
  blockIndex: number,
  pointIndex: number,
  dx: number,
  dy: number,
): Promise<DocumentSnapshot> {
  return invoke("move_stitch_point", {
    index,
    kind,
    blockIndex,
    pointIndex,
    dx,
    dy,
  });
}

/** Insert a needle point near pointIndex. */
export async function insertStitchPoint(
  index: number,
  kind: number,
  blockIndex: number,
  pointIndex: number,
): Promise<DocumentSnapshot> {
  return invoke("insert_stitch_point", { index, kind, blockIndex, pointIndex });
}

/** Insert a needle point at a world position, right after afterIndex. */
export async function insertStitchPointAt(
  index: number,
  kind: number,
  blockIndex: number,
  afterIndex: number,
  x: number,
  y: number,
): Promise<DocumentSnapshot> {
  return invoke("insert_stitch_point_at", {
    index,
    kind,
    blockIndex,
    afterIndex,
    x,
    y,
  });
}

/** Delete the needle point at pointIndex. */
export async function deleteStitchPoint(
  index: number,
  kind: number,
  blockIndex: number,
  pointIndex: number,
): Promise<DocumentSnapshot> {
  return invoke("delete_stitch_point", { index, kind, blockIndex, pointIndex });
}

export async function getBrotherPalette(): Promise<BrotherColor[]> {
  return invoke("get_brother_palette");
}

export async function setPathFillColor(
  index: number,
  pathIndex: number,
  brotherIndex: number,
): Promise<DocumentSnapshot> {
  return invoke("set_path_fill_color", { index, pathIndex, brotherIndex });
}

export async function setPathStrokeColor(
  index: number,
  pathIndex: number,
  brotherIndex: number,
): Promise<DocumentSnapshot> {
  return invoke("set_path_stroke_color", { index, pathIndex, brotherIndex });
}

export async function setPathStrokeWidth(
  index: number,
  pathIndex: number,
  width: number,
): Promise<DocumentSnapshot> {
  return invoke("set_path_stroke_width", { index, pathIndex, width });
}

export async function getColorBlocks(index: number): Promise<ColorBlockInfo[]> {
  return invoke("get_color_blocks", { index });
}

export interface StitchSegment {
  hex: string;
  start: number; // point offset into coords
  count: number; // number of points
}

export interface StitchData {
  segments: StitchSegment[];
  totalPoints: number;
  /** flat x,y pairs in engine units (0.1mm) */
  coords: Float32Array;
}

export async function getStitchData(index = -1): Promise<StitchData> {
  const dto = await invoke<{
    segments: StitchSegment[];
    total_points: number;
    coords_b64: string;
  }>("get_stitch_data", { index });
  const bin = atob(dto.coords_b64);
  const bytes = new Uint8Array(bin.length);
  for (let i = 0; i < bin.length; i++) bytes[i] = bin.charCodeAt(i);
  return {
    segments: dto.segments,
    totalPoints: dto.total_points,
    coords: new Float32Array(bytes.buffer),
  };
}

export async function setColorBlock(
  index: number,
  blockIndex: number,
  brotherIndex: number,
): Promise<DocumentSnapshot> {
  return invoke("set_color_block", { index, blockIndex, brotherIndex });
}

export async function swapColorBlock(
  index: number,
  blockIndex: number,
  dir: number,
): Promise<DocumentSnapshot> {
  return invoke("swap_color_block", { index, blockIndex, dir });
}

export async function flipObject(
  index: number,
  horizontal: boolean,
): Promise<DocumentSnapshot> {
  return invoke("flip_object", { index, horizontal });
}

export interface ObjectParameter {
  text: string;
  fontName: string;
  fontSize: number;
  colorIndex: number;
  borderColorIndex: number;
  shapeIndex: number;
  angleValue: number;
  radiusValue: number;
  italic: boolean;
  border: boolean;
  borderGap: number;
  borderGapY: number;
  extraLetterSpace: number;
  extraSpace: number;
  density: number;
  pullCompensate: number;
  fillTypeIndex: number;
  fillColorIndex: number;
  fillUnderlay: boolean;
  fillDensity: number;
  fillDirection: number;
  strokeTypeIndex: number;
  strokeRunPitch: number;
  strokeWidth: number;
  strokeDensity: number;
  strokeRunningInset: number;
}

export async function getParameter(index: number): Promise<ObjectParameter> {
  const json = await invoke<string>("get_parameter", { index });
  return JSON.parse(json);
}

export async function setParameter(
  index: number,
  key: string,
  value: number | boolean | string,
): Promise<DocumentSnapshot> {
  return invoke("set_parameter", { index, key, value });
}

export async function listPpefFonts(): Promise<string[]> {
  return invoke("list_ppef_fonts");
}

export async function listTtfFonts(): Promise<string[]> {
  return invoke("list_ttf_fonts");
}

export type PathOp =
  | "inset"
  | "outset"
  | "simplify"
  | "unite_next"
  | "separate"
  | "erase_under"
  | "up"
  | "down";

export async function applyPathOp(
  index: number,
  pathIndex: number,
  op: PathOp,
  value = 0,
): Promise<DocumentSnapshot> {
  return invoke("apply_path_op", { index, pathIndex, op, value });
}

export interface ObjectMove {
  index: number;
  dx: number;
  dy: number;
}

export async function translateObjects(
  moves: ObjectMove[],
): Promise<DocumentSnapshot> {
  return invoke("translate_objects", { moves });
}

export async function deleteObjects(
  indices: number[],
): Promise<DocumentSnapshot> {
  return invoke("delete_objects", { indices });
}

export async function undo(): Promise<DocumentSnapshot> {
  return invoke("undo");
}

export async function redo(): Promise<DocumentSnapshot> {
  return invoke("redo");
}
