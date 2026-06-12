import { create } from "zustand";
import * as engine from "../engine/EngineClient";
import type { DocumentSnapshot } from "../engine/types";

interface DocumentState {
  doc: DocumentSnapshot | null;
  /** bumped whenever object pixels may have changed → ObjectsLayer refetches */
  imageVersion: number;
  /** multi-selection; last entry is the primary (shown in Properties) */
  selectedIndices: number[];
  selectedIndex: number;
  busy: boolean;
  error: string | null;

  newDocument: (wMm?: number, hMm?: number) => Promise<void>;
  openFile: (path: string) => Promise<void>;
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
  translateSelected: (dx: number, dy: number) => Promise<void>;
  deleteSelected: () => Promise<void>;
  duplicateSelected: () => Promise<void>;
  setVisible: (index: number, visible: boolean) => Promise<void>;
  setLocked: (index: number, locked: boolean) => Promise<void>;
  reorder: (index: number, dir: number) => Promise<void>;
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

  return {
    doc: null,
    imageVersion: 0,
    selectedIndices: [],
    selectedIndex: -1,
    busy: false,
    error: null,

    newDocument: (wMm = 100, hMm = 100) =>
      run(true, () => engine.newDocument(wMm, hMm)),

    openFile: (path) => run(true, () => engine.openFile(path)),

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

    translateSelected: async (dx, dy) => {
      const { selectedIndices } = get();
      if (!selectedIndices.length) return;
      await run(true, () => engine.translateObjects(selectedIndices, dx, dy));
    },

    deleteSelected: async () => {
      const { selectedIndices } = get();
      if (!selectedIndices.length) return;
      await run(true, () => engine.deleteObjects(selectedIndices));
      set({ selectedIndices: [], selectedIndex: -1 });
    },

    duplicateSelected: async () => {
      const { selectedIndex } = get();
      if (selectedIndex < 0) return;
      await run(true, () => engine.duplicateObject(selectedIndex));
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

    undo: () => run(true, () => engine.undo()),
    redo: () => run(true, () => engine.redo()),

    applyPathEdit: (fn) => run(true, fn),

    clearError: () => set({ error: null }),
  };
});
