import { create } from "zustand";
import * as engine from "../engine/EngineClient";
import { preprocessSvgGradients } from "../engine/svgGradients";
import type { DocumentSnapshot, ObjectSnapshot } from "../engine/types";

// Some legacy AutoSewing PPES templates ship with every real stitch layer
// hidden by default — only a blank "Real Material" background swatch and a
// sub-millimeter marker stub are left visible (a quirk baked into the source
// file itself, reproduced verbatim by the port's engine). Sized in engine
// units (10 = 1mm); real stitch art is easily two orders of magnitude bigger
// than the marker stubs, so this comfortably excludes only those stubs.
const REVEAL_MIN_UNITS = 50;
// `has_stitches` excludes the (visible-by-default, but blank) "Real Material"
// Background swatch — size alone doesn't, since it's bigger than any stub.
const hasVisibleStitchArt = (o: ObjectSnapshot) =>
  o.has_stitches && Math.max(o.width, o.height) >= REVEAL_MIN_UNITS;

interface DocumentState {
  doc: DocumentSnapshot | null;
  /** bumped whenever object pixels may have changed → ObjectsLayer refetches */
  imageVersion: number;
  /** path of the open .ppes project (null = unsaved/new); drives Save vs Save As */
  projectPath: string | null;
  /** multi-selection; last entry is the primary (shown in Properties) */
  selectedIndices: number[];
  selectedIndex: number;
  busy: boolean;
  error: string | null;

  newDocument: (wMm?: number, hMm?: number) => Promise<void>;
  openFile: (path: string) => Promise<void>;
  /** web file-open: load a picked file's bytes (browser has no path) */
  openBytes: (filename: string, bytes: Uint8Array) => Promise<void>;
  /** write the whole document as a .ppes project to `path` and remember it */
  saveProject: (path: string) => Promise<void>;
  refresh: () => Promise<void>;
  select: (index: number, additive?: boolean) => void;
  commitTransform: (
    index: number,
    dx: number,
    dy: number,
    sx: number,
    sy: number,
    rotateDegree: number,
  ) => Promise<void>;
  translateObjects: (moves: engine.ObjectMove[]) => Promise<void>;
  deleteSelected: () => Promise<void>;
  duplicateSelected: () => Promise<void>;
  /** drop a ready-made parametric shape and select it */
  addShape: (shapeIndex: number) => Promise<void>;
  /** add a PPEF text object (default Thai sample text) and select it */
  addPpefText: () => Promise<void>;
  /** add a TTF text object (default Thai sample text) and select it */
  addTtfText: () => Promise<void>;
  /** commit a manually-drawn satin column (two rails of clicked knots) — the
   *  engine smooths the rails, then it's added + selected. Returns true if a
   *  column was created (needs ≥2 knots per rail, equal counts). */
  addSatinColumn: (rails: engine.SatinKnot[][]) => Promise<boolean>;
  /** Smart Satin: convert a TTF/SVG object's outlines into satin columns */
  convertToSatin: (index: number) => Promise<void>;
  /** Auto Punch: optionally add the source photo as a Background object, then
   *  the traced per-color fill objects as one group. Selects the new objects.
   *  Returns true on success. */
  autoPunch: (
    spec: engine.PunchSpec,
    backgroundPngBase64?: string,
  ) => Promise<boolean>;
  setVisible: (index: number, visible: boolean) => Promise<void>;
  setLocked: (index: number, locked: boolean) => Promise<void>;
  reorder: (index: number, dir: number) => Promise<void>;
  reorderTo: (from: number, to: number) => Promise<void>;
  /** select every object in a group (group header click) */
  selectGroup: (id: number, additive?: boolean) => void;
  createGroup: (name: string, memberIndices?: number[]) => Promise<void>;
  renameGroup: (id: number, name: string) => Promise<void>;
  ungroup: (id: number) => Promise<void>;
  /** delete the group AND all its member objects */
  deleteGroup: (id: number) => Promise<void>;
  addToGroup: (id: number, indices: number[]) => Promise<void>;
  removeFromGroup: (indices: number[]) => Promise<void>;
  setGroupCollapsed: (id: number, collapsed: boolean) => Promise<void>;
  setGroupVisible: (id: number, visible: boolean) => Promise<void>;
  setGroupLocked: (id: number, locked: boolean) => Promise<void>;
  undo: () => Promise<void>;
  redo: () => Promise<void>;
  /** run any engine mutation that returns a fresh DocumentSnapshot */
  applyPathEdit: (fn: () => Promise<DocumentSnapshot>) => Promise<void>;
  clearError: () => void;
}

export const useDocumentStore = create<DocumentState>((set, get) => {
  const applyDoc = (doc: DocumentSnapshot, invalidateImages: boolean) =>
    set((s) => {
      const valid = s.selectedIndices.filter((i) => i < doc.objects.length);
      return {
        doc,
        imageVersion: invalidateImages ? s.imageVersion + 1 : s.imageVersion,
        selectedIndices: valid,
        selectedIndex: valid.length ? valid[valid.length - 1] : -1,
      };
    });

  const run = async (
    invalidateImages: boolean,
    fn: () => Promise<DocumentSnapshot>,
  ) => {
    set({ busy: true, error: null });
    try {
      applyDoc(await fn(), invalidateImages);
    } catch (e) {
      set({ error: String(e) });
    } finally {
      set({ busy: false });
    }
  };

  // After a load, if nothing substantial ended up visible (see
  // REVEAL_MIN_UNITS), reveal the first substantial stitch layer so the
  // document doesn't land on a blank canvas.
  const revealIfBlank = async () => {
    const doc = get().doc;
    if (!doc) return;
    if (doc.objects.some((o) => o.visible && hasVisibleStitchArt(o))) return;
    const hidden = doc.objects.find(hasVisibleStitchArt);
    if (hidden) await run(false, () => engine.setObjectVisible(hidden.index, true));
  };

  return {
    doc: null,
    imageVersion: 0,
    projectPath: null,
    selectedIndices: [],
    selectedIndex: -1,
    busy: false,
    error: null,

    newDocument: async (wMm = 100, hMm = 100) => {
      await run(true, () => engine.newDocument(wMm, hMm));
      set({ projectPath: null });
    },

    openFile: async (path) => {
      await run(true, () => engine.openFile(path));
      // Only project files become the active project; .pes/.svg are imports
      // merged into the current document and must not claim the save path.
      const ext = path.split(".").pop()?.toLowerCase();
      if (!get().error && (ext === "ppes" || ext === "ppes5")) {
        set({ projectPath: path });
        await revealIfBlank();
      }
    },

    openBytes: async (filename, bytes) => {
      let data = bytes;
      // resolve SVG gradients in the browser (engine can't) → sentinel fills
      // that ObjectsLayer paints as real Konva gradients.
      if (filename.toLowerCase().endsWith(".svg")) {
        const svg = preprocessSvgGradients(new TextDecoder().decode(bytes));
        data = new TextEncoder().encode(svg);
      }
      await run(true, () => engine.openDocumentBytes(filename, data));
      const ext = filename.split(".").pop()?.toLowerCase();
      if (!get().error && (ext === "ppes" || ext === "ppes5")) {
        set({ projectPath: filename });
        await revealIfBlank();
      }
    },

    saveProject: async (path) => {
      set({ busy: true, error: null });
      try {
        await engine.exportFile(path, "PPES");
        set({ projectPath: path });
      } catch (e) {
        set({ error: String(e) });
      } finally {
        set({ busy: false });
      }
    },

    refresh: () => run(false, () => engine.getDocument()),

    select: (index, additive = false) =>
      set((s) => {
        if (index < 0) return { selectedIndices: [], selectedIndex: -1 };
        let list: number[];
        if (additive) {
          list = s.selectedIndices.includes(index)
            ? s.selectedIndices.filter((i) => i !== index)
            : [...s.selectedIndices, index];
        } else {
          list = [index];
        }
        return {
          selectedIndices: list,
          selectedIndex: list.length ? list[list.length - 1] : -1,
        };
      }),

    commitTransform: (index, dx, dy, sx, sy, rotateDegree) =>
      run(true, () =>
        engine.transformObject(index, dx, dy, sx, sy, rotateDegree),
      ),

    translateObjects: async (moves) => {
      if (!moves.length) return;
      await run(true, () => engine.translateObjects(moves));
    },

    deleteSelected: async () => {
      const { selectedIndices } = get();
      if (!selectedIndices.length) return;
      await run(true, () => engine.deleteObjects(selectedIndices));
      set({ selectedIndices: [], selectedIndex: -1 });
    },

    duplicateSelected: async () => {
      const { selectedIndices, doc } = get();
      if (!selectedIndices.length || !doc) return;
      // If every selected object belongs to one (non-ungrouped) group, the
      // copies form a fresh group — "duplicate group". Otherwise just copy
      // the objects (copies keep whatever group they were in).
      const objs = selectedIndices
        .map((i) => doc.objects.find((o) => o.index === i))
        .filter((o): o is NonNullable<typeof o> => !!o);
      const gid = objs[0]?.group_id ?? 0;
      const sameGroup = gid !== 0 && objs.every((o) => o.group_id === gid);
      const groupName = sameGroup
        ? `${doc.groups.find((g) => g.id === gid)?.name ?? "Group"} copy`
        : undefined;
      set({ busy: true, error: null });
      try {
        const res = await engine.duplicateObjects(selectedIndices, groupName);
        set((s) => ({
          doc: res.snapshot,
          imageVersion: s.imageVersion + 1,
          selectedIndices: res.new_indices,
          selectedIndex: res.new_indices.length
            ? res.new_indices[res.new_indices.length - 1]
            : -1,
        }));
      } catch (e) {
        set({ error: String(e) });
      } finally {
        set({ busy: false });
      }
    },

    addShape: async (shapeIndex) => {
      set({ busy: true, error: null });
      try {
        const doc = await engine.addShape(shapeIndex);
        // the new object is appended last → select it
        const last = doc.objects.length - 1;
        set((s) => ({
          doc,
          imageVersion: s.imageVersion + 1,
          selectedIndices: last >= 0 ? [last] : [],
          selectedIndex: last,
        }));
      } catch (e) {
        set({ error: String(e) });
      } finally {
        set({ busy: false });
      }
    },

    addPpefText: async () => {
      set({ busy: true, error: null });
      try {
        const doc = await engine.addPpefText();
        // the new object is appended last → select it
        const last = doc.objects.length - 1;
        set((s) => ({
          doc,
          imageVersion: s.imageVersion + 1,
          selectedIndices: last >= 0 ? [last] : [],
          selectedIndex: last,
        }));
      } catch (e) {
        set({ error: String(e) });
      } finally {
        set({ busy: false });
      }
    },

    addTtfText: async () => {
      set({ busy: true, error: null });
      try {
        const doc = await engine.addTtfText();
        // the new object is appended last → select it
        const last = doc.objects.length - 1;
        set((s) => ({
          doc,
          imageVersion: s.imageVersion + 1,
          selectedIndices: last >= 0 ? [last] : [],
          selectedIndex: last,
        }));
      } catch (e) {
        set({ error: String(e) });
      } finally {
        set({ busy: false });
      }
    },

    addSatinColumn: async (rails) => {
      const [railA = [], railB = []] = rails;
      // same guard as the old app (PES5_StopSatinColumnInput): ≥2 knots per
      // rail and equal counts, so every rung has both ends.
      if (railA.length < 2 || railA.length !== railB.length) {
        set({ error: "ต้องมีจุดอย่างน้อย 2 คู่ และรางทั้งสองเท่ากัน" });
        return false;
      }
      set({ busy: true, error: null });
      try {
        // engine smooths the clicked knots into rail d-strings (its own
        // cubic-superpath — identical to the old app) + the bbox center, so the
        // column lands exactly where it was drawn. Then reuse addSatinObjects.
        const { rails: [dA, dB], center } = await engine.satinColumnRails(rails);
        if (!dA || !dB) {
          set({ error: "สร้าง Satin Column ไม่สำเร็จ" });
          return false;
        }
        const spec: engine.SatinObjectSpec = {
          rails: [[dA, dB]],
          colorIndex: 11, // Deep Gold — matches new PPEF/TTF text
          center,
          scale: [1, 1],
          rotateDegree: 0,
          density: 2.5,
          pullCompensate: 0,
          noneOverlap: false,
        };
        const doc = await engine.addSatinObjects([spec]);
        // the new object is appended last → select it
        const last = doc.objects.length - 1;
        set((s) => ({
          doc,
          imageVersion: s.imageVersion + 1,
          selectedIndices: last >= 0 ? [last] : [],
          selectedIndex: last,
        }));
        return true;
      } catch (e) {
        set({ error: String(e) });
        return false;
      } finally {
        set({ busy: false });
      }
    },

    convertToSatin: async (index) => {
      set({ busy: true, error: null });
      try {
        // heavy geometry (straight skeleton) — lazy-load the vendored core
        const { convertObjectToSmartSatin } = await import("../satin/smartSatin");
        const doc = await convertObjectToSmartSatin(index);
        if (!doc) {
          set({ error: "แปลงเป็น Satin ไม่สำเร็จ (ไม่พบเส้นที่แปลงได้)" });
          return;
        }
        // new satin objects are appended last → select the last one
        const last = doc.objects.length - 1;
        set((s) => ({
          doc,
          imageVersion: s.imageVersion + 1,
          selectedIndices: last >= 0 ? [last] : [],
          selectedIndex: last,
        }));
      } catch (e) {
        set({ error: String(e) });
      } finally {
        set({ busy: false });
      }
    },

    autoPunch: async (spec, backgroundPngBase64) => {
      set({ busy: true, error: null });
      try {
        // two undo steps when the background is on (both commands stay
        // independently useful; documented in the plan)
        if (backgroundPngBase64) await engine.importBackground(backgroundPngBase64);
        const res = await engine.addPunchObjects(spec);
        set((s) => ({
          doc: res.snapshot,
          imageVersion: s.imageVersion + 1,
          selectedIndices: res.new_indices,
          selectedIndex: res.new_indices.length
            ? res.new_indices[res.new_indices.length - 1]
            : -1,
        }));
        return true;
      } catch (e) {
        set({ error: String(e) });
        return false;
      } finally {
        set({ busy: false });
      }
    },

    setVisible: (index, visible) =>
      run(false, () => engine.setObjectVisible(index, visible)),

    setLocked: (index, locked) =>
      run(false, () => engine.setObjectLocked(index, locked)),

    reorder: async (index, dir) => {
      await run(true, () => engine.reorderObject(index, dir));
      const count = get().doc?.objects.length ?? 0;
      const next = Math.min(Math.max(index + dir, 0), Math.max(count - 1, 0));
      set({ selectedIndices: [next], selectedIndex: next });
    },

    reorderTo: async (from, to) => {
      if (from === to) return;
      await run(true, () => engine.reorderObjectTo(from, to));
      // the dragged object now lives at `to`; keep it selected
      set({ selectedIndices: [to], selectedIndex: to });
    },

    selectGroup: (id, additive = false) =>
      set((s) => {
        const members = (s.doc?.objects ?? [])
          .filter((o) => o.group_id === id)
          .map((o) => o.index);
        if (!members.length) {
          return additive ? {} : { selectedIndices: [], selectedIndex: -1 };
        }
        let list: number[];
        if (additive) {
          const cur = new Set(s.selectedIndices);
          const allIn = members.every((m) => cur.has(m));
          list = allIn
            ? s.selectedIndices.filter((i) => !members.includes(i)) // toggle off
            : [...new Set([...s.selectedIndices, ...members])];
        } else {
          list = members;
        }
        return {
          selectedIndices: list,
          selectedIndex: list.length ? list[list.length - 1] : -1,
        };
      }),

    // Group structure changes don't alter pixels, so they don't bump
    // imageVersion (run(false)) — the panel re-renders from `doc` regardless.
    createGroup: (name, memberIndices = []) =>
      run(false, () => engine.createGroup(name, memberIndices)),
    renameGroup: (id, name) => run(false, () => engine.renameGroup(id, name)),
    ungroup: (id) => run(false, () => engine.ungroup(id)),
    // Deleting members changes pixels/indices → invalidate images, drop selection.
    deleteGroup: async (id) => {
      await run(true, () => engine.deleteGroup(id));
      set({ selectedIndices: [], selectedIndex: -1 });
    },
    addToGroup: (id, indices) =>
      run(false, () => engine.addToGroup(id, indices)),
    removeFromGroup: (indices) =>
      run(false, () => engine.removeFromGroup(indices)),
    setGroupCollapsed: (id, collapsed) =>
      run(false, () => engine.setGroupCollapsed(id, collapsed)),
    setGroupVisible: (id, visible) =>
      run(false, () => engine.setGroupVisible(id, visible)),
    setGroupLocked: (id, locked) =>
      run(false, () => engine.setGroupLocked(id, locked)),

    undo: () => run(true, () => engine.undo()),
    redo: () => run(true, () => engine.redo()),

    applyPathEdit: (fn) => run(true, fn),

    clearError: () => set({ error: null }),
  };
});
