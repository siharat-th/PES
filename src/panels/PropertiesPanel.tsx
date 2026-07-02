import { useEffect, useState } from "react";
import {
  ChevronUp,
  ChevronDown,
  FlipHorizontal2,
  FlipVertical2,
} from "lucide-react";
import { useDocumentStore } from "../state/documentStore";
import * as engine from "../engine/EngineClient";
import type { ObjectParameter } from "../engine/EngineClient";
import type { BrotherColor, ColorBlockInfo } from "../engine/types";
import {
  Group,
  Row,
  NumberField,
  SelectField,
  CheckRow,
  ColorField,
  PaletteGrid,
} from "./fields";

/** Type-switched properties panel — mirrors the old PropertyBoxHandler:
 *  which controls appear depends on GetLayerTypeString (object_type). */

const FONT_SIZES = [4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 22, 24, 26, 28, 30];

// shapeIndex mapping per pes5.html updatePPEFText (0..15 contiguous)
const TEXT_EFFECTS = [
  { value: 0, label: "Plain Text" },
  { value: 1, label: "Arch Up" },
  { value: 2, label: "Arch Down" },
  { value: 3, label: "Circle" },
  { value: 4, label: "Wave" },
  { value: 5, label: "Chevron Up" },
  { value: 6, label: "Chevron Down" },
  { value: 7, label: "Slant Up" },
  { value: 8, label: "Slant Down" },
  { value: 9, label: "Triangle Up" },
  { value: 10, label: "Triangle Down" },
  { value: 11, label: "Fade Right" },
  { value: 12, label: "Fade Left" },
  { value: 13, label: "Fade Up" },
  { value: 14, label: "Fade Down" },
  { value: 15, label: "Inflate" },
];

// Prop_StrokeFillHandler.js lines 4-15 (visible entries only)
const FILL_TYPES = [
  { value: 0, label: "None" },
  { value: 1, label: "Stitch" },
  { value: 2, label: "Pattern" },
  { value: 3, label: "Motif" },
  { value: 7, label: "Cross Stitch" },
  { value: 9, label: "Dynamic" },
];

// Prop_StrokeFillHandler.js lines 33-44
const STROKE_TYPES = [
  { value: 0, label: "None" },
  { value: 1, label: "Running" },
  { value: 2, label: "Double Running" },
  { value: 3, label: "Tripple Running" },
  { value: 4, label: "Bean Running" },
  { value: 5, label: "Satin" },
  { value: 6, label: "Satin outter" },
  { value: 7, label: "Satin inner" },
  { value: 8, label: "Motif" },
  { value: 9, label: "Dynamic" },
];

const isRunningStroke = (t: number) => t >= 1 && t <= 4;
const isSatinStroke = (t: number) => t >= 5 && t <= 7;

export default function PropertiesPanel() {
  const doc = useDocumentStore((s) => s.doc);
  const selectedIndex = useDocumentStore((s) => s.selectedIndex);
  const imageVersion = useDocumentStore((s) => s.imageVersion);
  const applyPathEdit = useDocumentStore((s) => s.applyPathEdit);
  const deleteSelected = useDocumentStore((s) => s.deleteSelected);
  const convertToSatin = useDocumentStore((s) => s.convertToSatin);
  const busy = useDocumentStore((s) => s.busy);

  const [param, setParam] = useState<ObjectParameter | null>(null);
  const [blocks, setBlocks] = useState<ColorBlockInfo[]>([]);
  const [palette, setPalette] = useState<BrotherColor[]>([]);
  const [ppefFonts, setPpefFonts] = useState<string[]>([]);
  const [activeBlock, setActiveBlock] = useState(-1);
  const [blockPickerOpen, setBlockPickerOpen] = useState(false);
  const [textDraft, setTextDraft] = useState("");
  const [pathCount, setPathCount] = useState(0);
  const [activePath, setActivePath] = useState(0);
  const [insetMm, setInsetMm] = useState(1);

  const obj = doc?.objects.find((o) => o.index === selectedIndex);

  const [ttfFonts, setTtfFonts] = useState<string[]>([]);

  useEffect(() => {
    void engine.getBrotherPalette().then(setPalette);
    void engine.listPpefFonts().then(setPpefFonts);
    void engine.listTtfFonts().then(setTtfFonts);
  }, []);

  useEffect(() => {
    setBlockPickerOpen(false);
    setActiveBlock(-1);
    if (selectedIndex < 0) {
      setParam(null);
      setBlocks([]);
      return;
    }
    let cancelled = false;
    void Promise.all([
      engine.getParameter(selectedIndex),
      engine.getColorBlocks(selectedIndex),
      engine.getObjectPaths(selectedIndex),
    ]).then(([p, b, paths]) => {
      if (cancelled) return;
      setParam(p);
      setBlocks(b);
      setTextDraft(p.text);
      setPathCount(paths.length);
      setActivePath((i) => Math.min(i, Math.max(paths.length - 1, 0)));
    });
    return () => {
      cancelled = true;
    };
  }, [selectedIndex, imageVersion]);

  if (!obj || !param) {
    return (
      <div className="px-3 py-4 text-xs text-neutral-400">
        เลือก object บน canvas หรือใน Layers เพื่อดู properties
      </div>
    );
  }

  const set = (key: string, value: number | boolean | string) =>
    void applyPathEdit(() => engine.setParameter(selectedIndex, key, value));

  const type = obj.object_type;
  const isText = ["PPEF Text", "TTF Text", "Monogram"].includes(type);
  const showFillStroke = ["TTF Text", "SVG"].includes(type);

  return (
    <div className="relative flex h-full flex-col overflow-y-auto text-sm">
      {/* header */}
      <div className="border-b border-neutral-100 px-2.5 py-2">
        <div className="font-medium">{type}</div>
        <div className="mt-0.5 text-[11px] text-neutral-400">
          {(obj.width / 10).toFixed(1)} × {(obj.height / 10).toFixed(1)} mm
          {obj.rotate_degree ? ` · ${obj.rotate_degree.toFixed(0)}°` : ""}
        </div>
      </div>

      {/* text editing (prop_ppeftext.ejs / prop_ttf.ejs) */}
      {isText && (
        <Group title="Text">
          <div className="flex gap-1.5 px-2.5 py-1">
            <input
              className="h-7 min-w-0 flex-1 rounded border border-neutral-300 px-2 text-xs"
              value={textDraft}
              onChange={(e) => setTextDraft(e.target.value)}
              onKeyDown={(e) => {
                if (e.key === "Enter" && textDraft !== param.text)
                  set("text", textDraft);
              }}
            />
            <button
              className="btn text-xs"
              disabled={textDraft === param.text}
              onClick={() => set("text", textDraft)}
            >
              Update
            </button>
          </div>
          {obj.object_type === "PPEF Text" && ppefFonts.length > 0 && (
            <Row label="Font">
              <SelectField
                value={param.fontName || "Thai001"}
                options={ppefFonts.map((f) => ({ value: f, label: f }))}
                onChange={(v) => set("font", v)}
              />
            </Row>
          )}
          {obj.object_type === "TTF Text" && ttfFonts.length > 0 && (
            <Row label="Font">
              <SelectField
                value={param.fontName}
                options={[
                  // current font may be a legacy display name not in the list
                  ...(ttfFonts.includes(param.fontName)
                    ? []
                    : [{ value: param.fontName, label: param.fontName }]),
                  ...ttfFonts.map((f) => ({ value: f, label: f })),
                ]}
                onChange={(v) => set("font", v)}
              />
            </Row>
          )}
          <Row label="Font size">
            <SelectField
              value={param.fontSize}
              options={FONT_SIZES.map((s) => ({ value: s, label: `${s} mm` }))}
              onChange={(v) => set("fontSize", v)}
            />
          </Row>
        </Group>
      )}

      {/* PPEF effects (prop_ppeftext.ejs lines 140-243) */}
      {type === "PPEF Text" && (
        <Group title="Effect">
          <Row label="Style">
            <SelectField
              value={param.shapeIndex}
              options={TEXT_EFFECTS}
              onChange={(v) => set("textEffect", v)}
            />
          </Row>
          {(param.shapeIndex === 1 || param.shapeIndex === 2) && (
            <>
              <Row label="Angle">
                <NumberField
                  value={Math.round(param.angleValue)}
                  min={15}
                  max={90}
                  step={1}
                  unit="°"
                  onCommit={(v) => set("textEffectAngle", v)}
                />
              </Row>
              <Row label="Radius">
                <NumberField
                  value={Math.round(param.radiusValue)}
                  min={100}
                  max={5000}
                  step={10}
                  onCommit={(v) => set("textEffectRadius", v)}
                />
              </Row>
            </>
          )}
          <CheckRow
            label="Border"
            checked={param.border}
            onChange={(v) => set("border", v)}
          />
          <CheckRow
            label="Italic"
            checked={param.italic}
            onChange={(v) => set("italic", v)}
          />
          <Row label="Letter gap">
            <NumberField
              value={param.extraLetterSpace}
              min={-50}
              max={50}
              step={10}
              onCommit={(v) => set("extraLetterSpace", v)}
            />
          </Row>
          <Row label="Word gap">
            <NumberField
              value={param.extraSpace}
              min={-200}
              max={200}
              step={10}
              onCommit={(v) => set("extraSpace", v)}
            />
          </Row>
          {param.border && (
            <>
              <Row label="Border gap X">
                <NumberField
                  value={param.borderGap}
                  min={-100}
                  max={1000}
                  step={10}
                  onCommit={(v) => set("borderGapX", v)}
                />
              </Row>
              <Row label="Border gap Y">
                <NumberField
                  value={param.borderGapY}
                  min={-100}
                  max={1000}
                  step={10}
                  onCommit={(v) => set("borderGapY", v)}
                />
              </Row>
              <Row label="Border color">
                <ColorField
                  palette={palette}
                  brotherIndex={param.borderColorIndex}
                  onPick={(c) => set("borderColor", c.index)}
                />
              </Row>
            </>
          )}
        </Group>
      )}

      {/* thread density (comp_thread.ejs) */}
      {(isText || type === "Satin Column") && (
        <Group title="Thread">
          <Row label="Density">
            <NumberField
              value={param.fillDensity}
              min={1}
              max={5}
              step={0.1}
              unit="l/mm"
              onCommit={(v) => set("textDensity", v)}
            />
          </Row>
          <Row label="Pull comp.">
            <NumberField
              value={param.pullCompensate}
              min={-0.5}
              max={2}
              step={0.25}
              unit="mm"
              onCommit={(v) => set("textPullCompensate", v)}
            />
          </Row>
        </Group>
      )}

      {/* Fill / Stroke (prop_ttf.ejs / prop_svg.ejs) */}
      {showFillStroke && (
        <>
          <Group title="Fill">
            <Row label="Type">
              <SelectField
                value={param.fillTypeIndex}
                options={FILL_TYPES}
                onChange={(v) => set("fillType", v)}
              />
            </Row>
            <Row label="Color">
              <ColorField
                palette={palette}
                brotherIndex={param.fillColorIndex}
                onPick={(c) => set("fillColor", c.index)}
              />
            </Row>
            {param.fillTypeIndex === 1 && (
              <>
                <CheckRow
                  label="Underlay"
                  checked={param.fillUnderlay}
                  onChange={(v) => set("fillUnderlay", v)}
                />
                <Row label="Density">
                  <NumberField
                    value={param.fillDensity}
                    min={0.5}
                    max={10}
                    step={0.5}
                    unit="l/mm"
                    onCommit={(v) => set("fillDensity", v)}
                  />
                </Row>
                <Row label="Direction">
                  <NumberField
                    value={param.fillDirection}
                    min={-90}
                    max={90}
                    step={5}
                    unit="°"
                    onCommit={(v) => set("fillDirection", v)}
                  />
                </Row>
              </>
            )}
          </Group>

          <Group title="Stroke">
            <Row label="Type">
              <SelectField
                value={param.strokeTypeIndex}
                options={STROKE_TYPES}
                onChange={(v) => set("strokeType", v)}
              />
            </Row>
            <Row label="Color">
              <ColorField
                palette={palette}
                brotherIndex={param.colorIndex}
                onPick={(c) => set("strokeColor", c.index)}
              />
            </Row>
            {isRunningStroke(param.strokeTypeIndex) && (
              <>
                <Row label="Run pitch">
                  <NumberField
                    value={param.strokeRunPitch}
                    min={0.2}
                    max={5}
                    step={0.1}
                    unit="mm"
                    onCommit={(v) => set("strokeRunPitch", v)}
                  />
                </Row>
                <Row label="Inset">
                  <NumberField
                    value={param.strokeRunningInset}
                    min={-2}
                    max={10}
                    step={0.1}
                    unit="mm"
                    onCommit={(v) => set("strokeRunningInset", v)}
                  />
                </Row>
              </>
            )}
            {isSatinStroke(param.strokeTypeIndex) && (
              <>
                <Row label="Width">
                  <NumberField
                    value={param.strokeWidth}
                    min={0.25}
                    max={10}
                    step={0.25}
                    unit="mm"
                    onCommit={(v) => set("strokeWidth", v)}
                  />
                </Row>
                <Row label="Density">
                  <NumberField
                    value={param.strokeDensity}
                    min={0.25}
                    max={10}
                    step={0.25}
                    unit="l/mm"
                    onCommit={(v) => set("strokeDensity", v)}
                  />
                </Row>
              </>
            )}
          </Group>
        </>
      )}

      {/* Satin column color (prop_satincolumn.ejs) */}
      {type === "Satin Column" && (
        <Group title="Color">
          <Row label="Color">
            <ColorField
              palette={palette}
              brotherIndex={param.colorIndex}
              onPick={(c) => set("strokeColor", c.index)}
            />
          </Row>
        </Group>
      )}

      {/* Path operations (Prop_PathOpsHandler) — geometry editing per path */}
      {obj.object_type !== "Stitch" && pathCount > 0 && (
        <Group title="Path operations" defaultOpen={false}>
          <Row label={`Path (${pathCount})`}>
            <SelectField
              value={activePath}
              options={Array.from({ length: pathCount }, (_, i) => ({
                value: i,
                label: `#${i + 1}`,
              }))}
              onChange={setActivePath}
            />
          </Row>
          <Row label="Inset / Outset">
            <NumberField
              value={insetMm}
              min={0.1}
              max={20}
              step={0.1}
              unit="mm"
              onCommit={setInsetMm}
            />
          </Row>
          <div className="grid grid-cols-2 gap-1.5 px-2.5 py-1.5">
            <button
              className="btn text-xs"
              onClick={() =>
                void applyPathEdit(() =>
                  engine.applyPathOp(selectedIndex, activePath, "inset", insetMm * 10),
                )
              }
            >
              Inset
            </button>
            <button
              className="btn text-xs"
              onClick={() =>
                void applyPathEdit(() =>
                  engine.applyPathOp(selectedIndex, activePath, "outset", insetMm * 10),
                )
              }
            >
              Outset
            </button>
            <button
              className="btn text-xs"
              onClick={() =>
                void applyPathEdit(() =>
                  engine.applyPathOp(selectedIndex, activePath, "simplify"),
                )
              }
            >
              Simplify
            </button>
            <button
              className="btn text-xs"
              disabled={activePath >= pathCount - 1}
              onClick={() =>
                void applyPathEdit(() =>
                  engine.applyPathOp(selectedIndex, activePath, "unite_next"),
                )
              }
            >
              Unite Next
            </button>
            <button
              className="btn text-xs"
              onClick={() =>
                void applyPathEdit(() =>
                  engine.applyPathOp(selectedIndex, activePath, "separate"),
                )
              }
            >
              Separate
            </button>
            <button
              className="btn text-xs"
              disabled={activePath < 1}
              onClick={() =>
                void applyPathEdit(() =>
                  engine.applyPathOp(selectedIndex, activePath, "erase_under"),
                )
              }
            >
              Erase Under
            </button>
          </div>
        </Group>
      )}

      {/* Transform: flip (prop_pes.ejs) */}
      <Group title="Transform">
        <div className="flex gap-2 px-2.5 py-1">
          <button
            className="btn flex items-center gap-1.5 text-xs"
            onClick={() =>
              void applyPathEdit(() => engine.flipObject(selectedIndex, false))
            }
          >
            <FlipVertical2 size={14} /> Vertical Flip
          </button>
          <button
            className="btn flex items-center gap-1.5 text-xs"
            onClick={() =>
              void applyPathEdit(() => engine.flipObject(selectedIndex, true))
            }
          >
            <FlipHorizontal2 size={14} /> Horizontal Flip
          </button>
        </div>
      </Group>

      <Group
        title={`Thread colors${blocks.length ? ` (${blocks.length})` : ""}`}
      >
        <div className="relative">
          {blocks.length === 0 && (
            <div className="px-2.5 py-1 text-[11px] text-neutral-400">
              object นี้ยังไม่มี stitch block
            </div>
          )}
          {blocks.map((b) => (
            <div
              key={b.index}
              className={`flex cursor-pointer items-center gap-2 border-b border-neutral-100 px-2.5 py-1.5 ${
                b.index === activeBlock
                  ? "bg-blue-50 ring-1 ring-inset ring-blue-300"
                  : "hover:bg-neutral-50"
              }`}
              onClick={() => setActiveBlock(b.index)}
            >
              <button
                className="h-6 w-6 shrink-0 rounded-sm border border-neutral-300 hover:scale-110 hover:border-blue-400"
                style={{ backgroundColor: b.hex }}
                title="เปลี่ยนสี"
                onClick={(e) => {
                  e.stopPropagation();
                  setActiveBlock(b.index);
                  setBlockPickerOpen(true);
                }}
              />
              <div className="min-w-0 flex-1">
                <div className="truncate text-xs">
                  {palette.find((c) => c.index === b.brother_index)?.name ?? b.hex}
                </div>
                <div className="text-[10px] text-neutral-400">
                  {b.stitch_count.toLocaleString()} stitches
                </div>
              </div>
            </div>
          ))}
          {blocks.length > 1 && (
            <div className="flex justify-center gap-2 px-2.5 py-2">
              <button
                className="btn flex items-center gap-1 text-xs"
                disabled={activeBlock <= 0}
                onClick={() => {
                  void applyPathEdit(() =>
                    engine.swapColorBlock(selectedIndex, activeBlock, -1),
                  );
                  setActiveBlock((i) => i - 1);
                }}
              >
                <ChevronUp size={14} /> Move Up
              </button>
              <button
                className="btn flex items-center gap-1 text-xs"
                disabled={activeBlock < 0 || activeBlock >= blocks.length - 1}
                onClick={() => {
                  void applyPathEdit(() =>
                    engine.swapColorBlock(selectedIndex, activeBlock, +1),
                  );
                  setActiveBlock((i) => i + 1);
                }}
              >
                <ChevronDown size={14} /> Move Down
              </button>
            </div>
          )}
          {blockPickerOpen && activeBlock >= 0 && (
            <div className="absolute inset-x-2 top-2 z-20">
              <PaletteGrid
                palette={palette}
                current={blocks[activeBlock]?.brother_index}
                onPick={(c) => {
                  setBlockPickerOpen(false);
                  void applyPathEdit(() =>
                    engine.setColorBlock(selectedIndex, activeBlock, c.index),
                  );
                }}
                onClose={() => setBlockPickerOpen(false)}
              />
            </div>
          )}
        </div>
      </Group>

      {/* Smart Satin: TTF/SVG outline -> satin columns (src/satin/smartSatin.ts) */}
      {showFillStroke && (
        <Group title="Smart Satin">
          <div className="px-2.5 py-1.5">
            <button
              className="w-full rounded bg-amber-500 py-1.5 text-xs font-medium text-white hover:bg-amber-600 disabled:cursor-not-allowed disabled:opacity-50"
              disabled={busy}
              onClick={() => void convertToSatin(selectedIndex)}
            >
              {busy ? "กำลังแปลง..." : "แปลงเป็นซาติน (Smart Satin)"}
            </button>
            <div className="mt-1 text-[10px] leading-4 text-neutral-400">
              แปลงเส้นขอบเป็น Satin Column พร้อมฝีเข็ม (object เดิมคงอยู่)
            </div>
          </div>
        </Group>
      )}

      {/* delete */}
      <div className="mt-auto px-2.5 py-3">
        <button
          className="w-full rounded bg-red-500 py-1.5 text-xs font-medium text-white hover:bg-red-600"
          onClick={() => void deleteSelected()}
        >
          Delete {isText ? "Text" : "Object"}
        </button>
      </div>
    </div>
  );
}
