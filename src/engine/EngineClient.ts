import { invoke } from "@tauri-apps/api/core";
import type {
  BrotherColor,
  ColorBlockInfo,
  DocumentSnapshot,
  PathInfo,
} from "./types";

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

export async function getObjectPaths(index: number): Promise<PathInfo[]> {
  return invoke("get_object_paths", { index });
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
