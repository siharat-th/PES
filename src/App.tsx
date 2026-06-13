import { useEffect, useState } from "react";
import { open, save } from "@tauri-apps/plugin-dialog";
import { getCurrentWebview } from "@tauri-apps/api/webview";
import {
  FilePlus2,
  FolderOpen,
  Download,
  Copy,
  Trash2,
  Undo2,
  Redo2,
  Maximize,
  X,
  PenTool,
  Sparkles,
} from "lucide-react";
import EmbroideryStage from "./canvas/EmbroideryStage";
import Sidebar from "./panels/Sidebar";
import SimulatorBar from "./panels/SimulatorBar";
import { useDocumentStore } from "./state/documentStore";
import { useViewportStore } from "./state/viewportStore";
import { useUiStore } from "./state/uiStore";
import * as engine from "./engine/EngineClient";
import "./App.css";

const EXPORT_FORMATS = [
  { label: "PES", format: "PES", ext: "pes" },
  { label: "DST", format: "DST", ext: "dst" },
  { label: "EXP", format: "EXP", ext: "exp" },
  { label: "JEF", format: "JEF", ext: "jef" },
  { label: "XXX", format: "XXX", ext: "xxx" },
  { label: "PNG", format: "PNG", ext: "png" },
  { label: "PPES (project)", format: "PPES", ext: "ppes" },
];

const OPENABLE = ["ppes", "ppes5", "pes", "svg"];

export default function App() {
  const { doc, busy, error, selectedIndex, clearError } = useDocumentStore();
  const newDocument = useDocumentStore((s) => s.newDocument);
  const openFile = useDocumentStore((s) => s.openFile);
  const refresh = useDocumentStore((s) => s.refresh);
  const deleteSelected = useDocumentStore((s) => s.deleteSelected);
  const duplicateSelected = useDocumentStore((s) => s.duplicateSelected);
  const undo = useDocumentStore((s) => s.undo);
  const redo = useDocumentStore((s) => s.redo);
  const zoom = useViewportStore((s) => s.zoom);
  const requestFit = useViewportStore((s) => s.requestFit);
  const viewMode = useUiStore((s) => s.viewMode);
  const setViewMode = useUiStore((s) => s.setViewMode);
  const [dragOver, setDragOver] = useState(false);

  useEffect(() => {
    void refresh();
  }, [refresh]);

  // OS file drag & drop (Tauri intercepts HTML5 drop events)
  useEffect(() => {
    const unlisten = getCurrentWebview().onDragDropEvent((event) => {
      if (event.payload.type === "enter" || event.payload.type === "over") {
        setDragOver(true);
      } else if (event.payload.type === "leave") {
        setDragOver(false);
      } else if (event.payload.type === "drop") {
        setDragOver(false);
        const path = event.payload.paths.find((p) =>
          OPENABLE.includes(p.split(".").pop()?.toLowerCase() ?? ""),
        );
        if (path) void openFile(path);
      }
    });
    return () => {
      void unlisten.then((fn) => fn());
    };
  }, [openFile]);

  const handleOpen = async () => {
    const path = await open({
      multiple: false,
      filters: [{ name: "Embroidery", extensions: OPENABLE }],
    });
    if (typeof path === "string") await openFile(path);
  };

  const handleExport = async (format: string, ext: string) => {
    const path = await save({
      filters: [{ name: format, extensions: [ext] }],
      defaultPath: `untitled.${ext}`,
    });
    if (path) await engine.exportFile(path, format);
  };

  // keyboard shortcuts
  useEffect(() => {
    const onKey = (e: KeyboardEvent) => {
      const tag = (e.target as HTMLElement)?.tagName;
      const typing = tag === "INPUT" || tag === "TEXTAREA";
      const mod = e.metaKey || e.ctrlKey;
      if (!typing && (e.key === "Delete" || e.key === "Backspace")) {
        void deleteSelected();
      } else if (mod && e.key.toLowerCase() === "z") {
        e.preventDefault();
        if (e.shiftKey) void redo();
        else void undo();
      } else if (mod && e.key.toLowerCase() === "d") {
        e.preventDefault();
        void duplicateSelected();
      } else if (mod && e.key === "0") {
        e.preventDefault();
        requestFit();
      }
    };
    window.addEventListener("keydown", onKey);
    return () => window.removeEventListener("keydown", onKey);
  }, [deleteSelected, duplicateSelected, undo, redo, requestFit]);

  const selectedObj = doc?.objects.find((o) => o.index === selectedIndex);

  return (
    <div className="flex h-screen flex-col bg-neutral-100 text-sm text-neutral-800">
      {/* Toolbar */}
      <div className="flex items-center gap-1 border-b border-neutral-200 bg-white px-2 py-1.5 shadow-sm">
        <span className="mx-2 select-none text-base font-bold tracking-wide text-blue-600">
          PES
        </span>
        <ToolButton
          label="New"
          icon={<FilePlus2 size={16} />}
          onClick={() => void newDocument(100, 100)}
        />
        <ToolButton
          label="Open"
          icon={<FolderOpen size={16} />}
          onClick={() => void handleOpen()}
        />
        <div className="group relative">
          <ToolButton
            label="Export"
            icon={<Download size={16} />}
            disabled={!doc || doc.objects.length === 0}
          />
          <div className="absolute left-0 top-full z-10 hidden min-w-40 flex-col rounded-md border border-neutral-200 bg-white py-1 shadow-lg group-hover:flex">
            {EXPORT_FORMATS.map((f) => (
              <button
                key={f.format}
                className="px-3 py-1.5 text-left hover:bg-blue-50"
                onClick={() => void handleExport(f.format, f.ext)}
              >
                {f.label}
              </button>
            ))}
          </div>
        </div>

        <Divider />
        <ToolButton
          label="Undo"
          icon={<Undo2 size={16} />}
          disabled={!doc?.can_undo}
          onClick={() => void undo()}
        />
        <ToolButton
          label="Redo"
          icon={<Redo2 size={16} />}
          disabled={!doc?.can_redo}
          onClick={() => void redo()}
        />

        <Divider />
        <ToolButton
          label="Duplicate"
          icon={<Copy size={16} />}
          disabled={selectedIndex < 0}
          onClick={() => void duplicateSelected()}
        />
        <ToolButton
          label="Delete"
          icon={<Trash2 size={16} />}
          disabled={selectedIndex < 0}
          onClick={() => void deleteSelected()}
        />

        <Divider />
        <ToolButton
          label="Fit"
          icon={<Maximize size={16} />}
          onClick={requestFit}
        />
        <ToolButton
          label={viewMode === "design" ? "Stitch View" : "Design View"}
          icon={
            viewMode === "design" ? (
              <Sparkles size={16} />
            ) : (
              <PenTool size={16} />
            )
          }
          active={viewMode === "stitch"}
          disabled={!doc || doc.objects.length === 0}
          onClick={() =>
            setViewMode(viewMode === "design" ? "stitch" : "design")
          }
        />

        {busy && (
          <span className="ml-auto mr-2 animate-pulse text-xs text-blue-500">
            กำลังประมวลผล…
          </span>
        )}
      </div>

      {/* Error banner */}
      {error && (
        <div className="flex items-center justify-between bg-red-50 px-3 py-1.5 text-red-700">
          <span className="text-xs">{error}</span>
          <button className="rounded p-0.5 hover:bg-red-100" onClick={clearError}>
            <X size={14} />
          </button>
        </div>
      )}

      {/* Workspace */}
      <div className="relative flex min-h-0 flex-1">
        <div className="flex min-w-0 flex-1 flex-col">
          <div className="min-h-0 flex-1">
            <EmbroideryStage />
          </div>
          {viewMode === "stitch" && <SimulatorBar />}
        </div>
        <Sidebar />
        {dragOver && (
          <div className="pointer-events-none absolute inset-0 z-20 flex items-center justify-center border-4 border-dashed border-blue-400 bg-blue-100/60">
            <span className="rounded-lg bg-white px-4 py-2 text-base font-medium text-blue-600 shadow">
              วางไฟล์ .pes / .ppes / .svg ที่นี่
            </span>
          </div>
        )}
      </div>

      {/* Status bar */}
      <div className="flex items-center gap-4 border-t border-neutral-200 bg-white px-3 py-1 text-[11px] text-neutral-500">
        <span>
          Hoop {doc?.hoop_width_mm ?? "–"}×{doc?.hoop_height_mm ?? "–"} mm
        </span>
        <span>{doc?.objects.length ?? 0} objects</span>
        {selectedObj && (
          <span className="text-blue-600">
            {selectedObj.object_type}
            {selectedObj.text ? ` “${selectedObj.text}”` : ""} ·{" "}
            {(selectedObj.width / 10).toFixed(1)}×
            {(selectedObj.height / 10).toFixed(1)} mm
            {selectedObj.rotate_degree
              ? ` · ${selectedObj.rotate_degree.toFixed(0)}°`
              : ""}
          </span>
        )}
        <span className="ml-auto">zoom {(zoom * 100).toFixed(0)}%</span>
      </div>
    </div>
  );
}

function ToolButton({
  label,
  icon,
  onClick,
  disabled,
  active,
}: {
  label: string;
  icon: React.ReactNode;
  onClick?: () => void;
  disabled?: boolean;
  active?: boolean;
}) {
  return (
    <button
      className={`flex flex-col items-center gap-0.5 rounded-md px-2.5 py-1 text-[10px] hover:bg-neutral-100 active:bg-neutral-200 disabled:opacity-35 disabled:hover:bg-transparent ${
        active ? "bg-blue-50 text-blue-600" : "text-neutral-600"
      }`}
      onClick={onClick}
      disabled={disabled}
      title={label}
    >
      {icon}
      <span>{label}</span>
    </button>
  );
}

function Divider() {
  return <div className="mx-1 h-6 w-px bg-neutral-200" />;
}
