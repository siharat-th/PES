import { useEffect } from "react";
import { create } from "zustand";
import { useDocumentStore } from "../state/documentStore";
import { setPathNodeType } from "../engine/EngineClient";

/** Shared state for the path-node right-click menu. PathEditLayer lives inside
 *  the Konva Stage (which can't host HTML), so the menu is opened from there but
 *  rendered as a DOM sibling of the Stage via this store. */
interface MenuState {
  open: boolean;
  x: number;
  y: number;
  pathIndex: number;
  nodeIndex: number;
  isCurve: boolean;
  show: (m: Omit<MenuState, "open" | "show" | "hide">) => void;
  hide: () => void;
}

export const usePathNodeMenu = create<MenuState>((set) => ({
  open: false,
  x: 0,
  y: 0,
  pathIndex: 0,
  nodeIndex: 0,
  isCurve: false,
  show: (m) => set({ open: true, ...m }),
  hide: () => set({ open: false }),
}));

function MenuItem({
  active,
  glyph,
  label,
  onClick,
}: {
  active: boolean;
  glyph: string;
  label: string;
  onClick: () => void;
}) {
  return (
    <button
      type="button"
      onClick={onClick}
      className="flex w-full items-center gap-2 px-3 py-1.5 text-left hover:bg-indigo-600/80"
    >
      <span className="w-4 text-center text-base leading-none opacity-80">
        {glyph}
      </span>
      <span className="flex-1">{label}</span>
      {active && <span className="text-xs text-indigo-300">✓</span>}
    </button>
  );
}

/** Right-click menu for a path node: switch its incoming segment between a
 *  straight corner and a smooth curve. Render once near the canvas. */
export default function PathNodeMenu() {
  const open = usePathNodeMenu((s) => s.open);
  const x = usePathNodeMenu((s) => s.x);
  const y = usePathNodeMenu((s) => s.y);
  const pathIndex = usePathNodeMenu((s) => s.pathIndex);
  const nodeIndex = usePathNodeMenu((s) => s.nodeIndex);
  const isCurve = usePathNodeMenu((s) => s.isCurve);
  const hide = usePathNodeMenu((s) => s.hide);
  const selectedIndex = useDocumentStore((s) => s.selectedIndex);
  const applyPathEdit = useDocumentStore((s) => s.applyPathEdit);

  // dismiss on any outside press, scroll, or Escape
  useEffect(() => {
    if (!open) return;
    const onKey = (e: KeyboardEvent) => {
      if (e.key === "Escape") hide();
    };
    window.addEventListener("mousedown", hide);
    window.addEventListener("wheel", hide);
    window.addEventListener("keydown", onKey);
    return () => {
      window.removeEventListener("mousedown", hide);
      window.removeEventListener("wheel", hide);
      window.removeEventListener("keydown", onKey);
    };
  }, [open, hide]);

  if (!open) return null;

  const convert = (toCurve: boolean) => {
    hide();
    if (toCurve === isCurve) return; // already this type
    if (useDocumentStore.getState().busy) return;
    void applyPathEdit(() =>
      setPathNodeType(selectedIndex, pathIndex, nodeIndex, toCurve),
    );
  };

  return (
    <div
      className="fixed z-50 min-w-[170px] overflow-hidden rounded-md border border-neutral-700 bg-neutral-900/95 py-1 text-sm text-neutral-100 shadow-xl backdrop-blur"
      style={{ left: x, top: y }}
      // keep the opening/closing mousedown from bubbling to the dismiss handler
      onMouseDown={(e) => e.stopPropagation()}
      onContextMenu={(e) => e.preventDefault()}
    >
      <div className="px-3 py-1 text-[11px] uppercase tracking-wide text-neutral-500">
        ชนิดจุด / Node type
      </div>
      <MenuItem
        active={!isCurve}
        glyph="◇"
        label="มุม (Corner)"
        onClick={() => convert(false)}
      />
      <MenuItem
        active={isCurve}
        glyph="○"
        label="เส้นโค้ง (Curve)"
        onClick={() => convert(true)}
      />
    </div>
  );
}
