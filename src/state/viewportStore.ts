import { create } from "zustand";

interface ViewportState {
  zoom: number;
  x: number;
  y: number;
  /** bumped to ask the stage to re-fit the hoop */
  fitRequest: number;
  setView: (v: { zoom: number; x: number; y: number }) => void;
  requestFit: () => void;
}

export const useViewportStore = create<ViewportState>((set) => ({
  zoom: 1,
  x: 0,
  y: 0,
  fitRequest: 0,
  setView: (v) => set(v),
  requestFit: () => set((s) => ({ fitRequest: s.fitRequest + 1 })),
}));
