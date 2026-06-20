import { create } from "zustand";

export type ViewMode = "design" | "stitch" | "pathEdit" | "stitchEdit";

interface UiState {
  viewMode: ViewMode;
  /** simulator playhead: number of stitches revealed; -1 = show all */
  simIndex: number;
  simPlaying: boolean;
  simSpeed: number; // stitches advanced per frame
  /** whether the right-hand panel (Properties/Layers) is shown */
  rightPanelOpen: boolean;

  setViewMode: (m: ViewMode) => void;
  setSimIndex: (i: number) => void;
  setSimPlaying: (p: boolean) => void;
  setSimSpeed: (s: number) => void;
  toggleRightPanel: () => void;
}

export const useUiStore = create<UiState>((set) => ({
  viewMode: "design",
  simIndex: -1,
  simPlaying: false,
  simSpeed: 2,
  rightPanelOpen: true,

  toggleRightPanel: () =>
    set((s) => ({ rightPanelOpen: !s.rightPanelOpen })),

  setViewMode: (viewMode) =>
    set(
      viewMode === "stitch"
        ? { viewMode }
        : { viewMode, simPlaying: false, simIndex: -1 },
    ),
  setSimIndex: (simIndex) => set({ simIndex }),
  setSimPlaying: (simPlaying) => set({ simPlaying }),
  setSimSpeed: (simSpeed) => set({ simSpeed }),
}));
