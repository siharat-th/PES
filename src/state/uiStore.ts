import { create } from "zustand";

export type ViewMode =
  | "design"
  | "stitch"
  | "pathEdit"
  | "stitchEdit"
  | "satinDraw";

/** which คลัง (design library) tab is open, or null if the panel is closed */
export type LibraryKind = "svg" | "pes" | "ppes";

interface UiState {
  viewMode: ViewMode;
  /** simulator playhead: number of stitches revealed; -1 = show all */
  simIndex: number;
  simPlaying: boolean;
  simSpeed: number; // stitches advanced per frame
  /** whether the right-hand panel (Properties/Layers) is shown */
  rightPanelOpen: boolean;
  libraryOpen: LibraryKind | null;
  /** Auto Punch (image → embroidery) dialog */
  punchOpen: boolean;
  /** image handed to the Auto Punch dialog from outside (drag-drop) —
   *  consumed (cleared) by AutoPunchPanel on pickup */
  punchSeed: { name: string; bytes: Uint8Array } | null;
  /** show jump/trim travel moves as dashed lines (design view).
   *  Stitch View always shows them regardless of this flag. */
  showJumpStitches: boolean;

  setViewMode: (m: ViewMode) => void;
  setSimIndex: (i: number) => void;
  setSimPlaying: (p: boolean) => void;
  setSimSpeed: (s: number) => void;
  toggleRightPanel: () => void;
  setLibraryOpen: (k: LibraryKind | null) => void;
  setPunchOpen: (open: boolean) => void;
  setPunchSeed: (seed: { name: string; bytes: Uint8Array } | null) => void;
  toggleShowJumpStitches: () => void;
}

export const useUiStore = create<UiState>((set) => ({
  viewMode: "design",
  simIndex: -1,
  simPlaying: false,
  simSpeed: 2,
  rightPanelOpen: true,
  libraryOpen: null,
  punchOpen: false,
  punchSeed: null,
  showJumpStitches: false,

  toggleRightPanel: () =>
    set((s) => ({ rightPanelOpen: !s.rightPanelOpen })),
  setLibraryOpen: (libraryOpen) => set({ libraryOpen }),
  setPunchOpen: (punchOpen) => set({ punchOpen }),
  setPunchSeed: (punchSeed) => set({ punchSeed }),
  toggleShowJumpStitches: () =>
    set((s) => ({ showJumpStitches: !s.showJumpStitches })),

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
