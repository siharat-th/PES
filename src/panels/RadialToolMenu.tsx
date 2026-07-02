import { useEffect, useState } from "react";
import {
  MousePointer2,
  Spline,
  Waypoints,
  Maximize,
  Copy,
  Trash2,
  Plus,
  X,
  ArrowLeft,
  Shapes,
  PenTool,
  Eye,
  FolderOpen,
  Type,
  TypeOutline,
  Columns3,
  Square,
  Circle,
  Triangle,
  Minus,
  LibraryBig,
} from "lucide-react";
import { SewingMachineIcon } from "../icons";
import { SHAPE } from "../engine/EngineClient";
import { useDocumentStore } from "../state/documentStore";
import { useViewportStore } from "../state/viewportStore";
import { useUiStore } from "../state/uiStore";

interface Item {
  id: string;
  label: string;
  icon: React.ReactNode;
  onClick?: () => void;
  active?: boolean;
  disabled?: boolean;
  /** not wired to the engine yet — shown dimmed with a "coming soon" hint */
  soon?: boolean;
}

interface Category {
  id: string;
  label: string;
  icon: React.ReactNode;
  tools: Item[];
}

// Items fan along an arc from straight-up (90°) to just past straight-left
// (185°), so the wheel always opens up-and-left of the bottom-right anchor and
// never sweeps off the right/bottom edge. Screen y is down, hence dy is negated.
const ARC_START = 90;
const ARC_END = 185;
const BASE_R = 96; // smallest ring radius (few items)
const MIN_CHORD = 54; // min center-to-center spacing → auto-grows radius so no overlap
const ITEM = 44; // button diameter (px)

/** A ring of buttons fanning out from the shared center anchor. `show` drives
 *  the open/collapse animation (expanded to the arc vs. collapsed into center). */
function Ring({ items, show }: { items: Item[]; show: boolean }) {
  const n = items.length;
  const step = n > 1 ? (ARC_END - ARC_START) / (n - 1) : 0;
  const stepRad = (step * Math.PI) / 180;
  // radius grows until adjacent buttons are at least MIN_CHORD apart
  const radius =
    n > 1
      ? Math.max(BASE_R, MIN_CHORD / (2 * Math.sin(stepRad / 2)))
      : BASE_R;

  return (
    <>
      {items.map((item, i) => {
        const angleDeg = n > 1 ? ARC_START + step * i : (ARC_START + ARC_END) / 2;
        const a = (angleDeg * Math.PI) / 180;
        const dx = Math.cos(a) * radius;
        const dy = -Math.sin(a) * radius;
        // stagger; closing collapses in reverse so it folds back inward smoothly
        const delay = show ? i * 28 : (n - 1 - i) * 20;
        // dimmed = not actionable, but keep it hoverable (no `disabled` attr) so
        // the tooltip — incl. the "เร็วๆ นี้" hint — still shows on hover.
        const dim = item.disabled || item.soon;
        return (
          <button
            key={item.id}
            onClick={dim ? undefined : item.onClick}
            aria-label={item.label}
            aria-disabled={dim || undefined}
            style={{
              width: ITEM,
              height: ITEM,
              transform: show
                ? `translate(calc(-50% + ${dx}px), calc(-50% + ${dy}px)) scale(1)`
                : "translate(-50%, -50%) scale(0.2)",
              opacity: show ? undefined : 0,
              transitionDelay: `${delay}ms`,
            }}
            className={`group/tool absolute left-1/2 top-1/2 z-0 flex items-center justify-center rounded-full shadow-lg ring-1 backdrop-blur-sm transition-[transform,opacity,background-color,color] duration-300 ease-[cubic-bezier(0.34,1.56,0.64,1)] hover:z-50 focus-visible:z-50 ${
              item.active
                ? "bg-blue-600 text-white ring-blue-700/30"
                : dim
                  ? "cursor-not-allowed bg-white/70 text-neutral-300 ring-black/5 hover:text-neutral-400"
                  : "bg-white/90 text-neutral-600 ring-black/5 hover:bg-white hover:text-blue-600"
            } ${show ? "" : "pointer-events-none"}`}
          >
            {item.icon}
            <span
              role="tooltip"
              className="pointer-events-none absolute right-full mr-2.5 whitespace-nowrap rounded-md bg-neutral-800 px-2 py-1 text-[11px] font-medium text-white opacity-0 shadow-lg ring-1 ring-black/5 transition-opacity duration-150 group-hover/tool:opacity-100"
            >
              {item.label}
              {item.soon && <span className="ml-1 text-neutral-400">· เร็วๆ นี้</span>}
            </span>
          </button>
        );
      })}
    </>
  );
}

export default function RadialToolMenu() {
  const [open, setOpen] = useState(false);
  const [activeCat, setActiveCat] = useState<string | null>(null);

  const selectedIndex = useDocumentStore((s) => s.selectedIndex);
  const objectCount = useDocumentStore((s) => s.doc?.objects.length ?? 0);
  const deleteSelected = useDocumentStore((s) => s.deleteSelected);
  const duplicateSelected = useDocumentStore((s) => s.duplicateSelected);
  const addShape = useDocumentStore((s) => s.addShape);
  const addPpefText = useDocumentStore((s) => s.addPpefText);
  const addTtfText = useDocumentStore((s) => s.addTtfText);
  const convertToSatin = useDocumentStore((s) => s.convertToSatin);
  const selectedType = useDocumentStore(
    (s) => s.doc?.objects[s.selectedIndex]?.object_type,
  );
  const requestFit = useViewportStore((s) => s.requestFit);
  const viewMode = useUiStore((s) => s.viewMode);
  const setViewMode = useUiStore((s) => s.setViewMode);

  const hasSelection = selectedIndex >= 0;

  const close = () => {
    setOpen(false);
    setActiveCat(null);
  };

  // Esc steps back a level, then closes
  useEffect(() => {
    if (!open) return;
    const onEsc = (e: KeyboardEvent) => {
      if (e.key !== "Escape") return;
      if (activeCat) setActiveCat(null);
      else close();
    };
    window.addEventListener("keydown", onEsc);
    return () => window.removeEventListener("keydown", onEsc);
  }, [open, activeCat]);

  // run a tool action, then collapse the whole wheel
  const run = (fn: () => void) => () => {
    fn();
    close();
  };

  const soon = (id: string, label: string, icon: React.ReactNode): Item => ({
    id,
    label,
    icon,
    soon: true,
  });

  const categories: Category[] = [
    {
      id: "create",
      label: "สร้าง",
      icon: <Shapes size={20} />,
      tools: [
        {
          id: "rect",
          label: "สี่เหลี่ยม (Rectangle)",
          icon: <Square size={18} />,
          onClick: run(() => void addShape(SHAPE.rect)),
        },
        {
          id: "ellipse",
          label: "วงรี (Ellipse)",
          icon: <Circle size={18} />,
          onClick: run(() => void addShape(SHAPE.ellipse)),
        },
        {
          id: "triangle",
          label: "สามเหลี่ยม (Triangle)",
          icon: <Triangle size={18} />,
          onClick: run(() => void addShape(SHAPE.triangle)),
        },
        {
          id: "line",
          label: "เส้น (Line)",
          icon: <Minus size={18} />,
          onClick: run(() => void addShape(SHAPE.line)),
        },
        {
          id: "ppefText",
          label: "ข้อความปัก (PPEF Text)",
          icon: <TypeOutline size={18} />,
          onClick: run(() => void addPpefText()),
        },
        {
          id: "ttfText",
          label: "ข้อความ TTF (TTF Text)",
          icon: <Type size={18} />,
          onClick: run(() => void addTtfText()),
        },
        soon("satin", "Satin Column", <Columns3 size={18} />),
      ],
    },
    {
      id: "edit",
      label: "แก้ไข",
      icon: <PenTool size={20} />,
      tools: [
        {
          id: "pathEdit",
          label: "แก้ไขเส้น (Edit Nodes)",
          icon: <Spline size={18} />,
          active: viewMode === "pathEdit",
          disabled: !hasSelection,
          onClick: run(() =>
            setViewMode(viewMode === "pathEdit" ? "design" : "pathEdit"),
          ),
        },
        {
          id: "smartSatin",
          label: "แปลงเป็นซาติน (Smart Satin)",
          icon: <Columns3 size={18} />,
          disabled: !["TTF Text", "SVG"].includes(selectedType ?? ""),
          onClick: run(() => void convertToSatin(selectedIndex)),
        },
        {
          id: "stitchEdit",
          label: "แก้ไขฝีเข็ม (Edit Stitches)",
          icon: <Waypoints size={18} />,
          active: viewMode === "stitchEdit",
          disabled: !hasSelection,
          onClick: run(() =>
            setViewMode(viewMode === "stitchEdit" ? "design" : "stitchEdit"),
          ),
        },
        {
          id: "duplicate",
          label: "ทำสำเนา (Duplicate)",
          icon: <Copy size={18} />,
          disabled: !hasSelection,
          onClick: run(() => void duplicateSelected()),
        },
        {
          id: "delete",
          label: "ลบ (Delete)",
          icon: <Trash2 size={18} />,
          disabled: !hasSelection,
          onClick: run(() => void deleteSelected()),
        },
      ],
    },
    {
      id: "view",
      label: "มุมมอง",
      icon: <Eye size={20} />,
      tools: [
        {
          id: "select",
          label: "เลือก (Select)",
          icon: <MousePointer2 size={18} />,
          active: viewMode === "design",
          onClick: run(() => setViewMode("design")),
        },
        {
          id: "fit",
          label: "พอดีจอ (Fit)",
          icon: <Maximize size={18} />,
          onClick: run(requestFit),
        },
        {
          id: "stitchView",
          label: "ดูการปัก (Stitch View)",
          icon: <SewingMachineIcon size={19} />,
          active: viewMode === "stitch",
          disabled: objectCount === 0,
          onClick: run(() =>
            setViewMode(viewMode === "stitch" ? "design" : "stitch"),
          ),
        },
      ],
    },
    {
      id: "library",
      label: "คลัง",
      icon: <FolderOpen size={20} />,
      tools: [
        soon("ppefLib", "คลังลายปัก (PPEF)", <LibraryBig size={18} />),
        soon("pesLib", "คลัง PES", <Shapes size={18} />),
        soon("svgLib", "คลัง SVG", <PenTool size={18} />),
      ],
    },
  ];

  // level-1 ring: the category hubs
  const catItems: Item[] = categories.map((c) => ({
    id: c.id,
    label: c.label,
    icon: c.icon,
    active: activeCat === c.id,
    onClick: () => setActiveCat(c.id),
  }));

  // center button: + (closed) → × (categories) → ← (inside a category)
  const centerIcon = !open ? (
    <Plus size={26} strokeWidth={2.5} />
  ) : activeCat ? (
    <ArrowLeft size={24} strokeWidth={2.5} />
  ) : (
    <X size={26} strokeWidth={2.5} />
  );
  const onCenter = () => {
    if (!open) setOpen(true);
    else if (activeCat) setActiveCat(null);
    else close();
  };

  return (
    <div className="absolute bottom-6 right-6 z-30">
      {open && (
        <div
          className="fixed inset-0 -z-10"
          onClick={close}
          aria-hidden="true"
        />
      )}

      <div className="relative h-14 w-14">
        {/* level 1 — category hubs (hidden once a category is open) */}
        <Ring items={catItems} show={open && !activeCat} />
        {/* level 2 — each category's tools (only the active one expands) */}
        {categories.map((c) => (
          <Ring key={c.id} items={c.tools} show={open && activeCat === c.id} />
        ))}

        {/* center toggle */}
        <button
          onClick={onCenter}
          aria-label={!open ? "เปิดเครื่องมือ" : activeCat ? "ย้อนกลับ" : "ปิด"}
          aria-expanded={open}
          className={`relative flex h-14 w-14 items-center justify-center rounded-full text-white shadow-xl ring-1 ring-black/10 transition-[transform,background-color] duration-300 ease-out ${
            open
              ? "bg-neutral-800 hover:bg-neutral-700"
              : "rotate-0 bg-blue-600 hover:bg-blue-500"
          } ${open && !activeCat ? "rotate-90" : ""}`}
        >
          {centerIcon}
        </button>
      </div>
    </div>
  );
}
