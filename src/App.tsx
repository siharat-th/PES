import { useEffect, useRef, useState } from "react";
import { open, save } from "@tauri-apps/plugin-dialog";
import { getCurrentWebview } from "@tauri-apps/api/webview";
import {
  FilePlus2,
  FolderOpen,
  Download,
  Save,
  SaveAll,
  Copy,
  Trash2,
  Undo2,
  Redo2,
  Maximize,
  X,
  Spline,
  Waypoints,
  PanelRightClose,
  PanelRightOpen,
} from "lucide-react";
import pesLogo from "./assets/pes-logo.png";
import { SewingMachineIcon } from "./icons";
import EmbroideryStage from "./canvas/EmbroideryStage";
import Sidebar from "./panels/Sidebar";
import SimulatorBar from "./panels/SimulatorBar";
import RadialToolMenu from "./panels/RadialToolMenu";
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
];

const OPENABLE = ["ppes", "ppes5", "pes", "svg"];

export default function App() {
  const { doc, busy, error, selectedIndex, clearError } = useDocumentStore();
  const newDocument = useDocumentStore((s) => s.newDocument);
  const openFile = useDocumentStore((s) => s.openFile);
  const openBytes = useDocumentStore((s) => s.openBytes);
  const saveProject = useDocumentStore((s) => s.saveProject);
  const projectPath = useDocumentStore((s) => s.projectPath);
  const refresh = useDocumentStore((s) => s.refresh);
  const deleteSelected = useDocumentStore((s) => s.deleteSelected);
  const duplicateSelected = useDocumentStore((s) => s.duplicateSelected);
  const undo = useDocumentStore((s) => s.undo);
  const redo = useDocumentStore((s) => s.redo);
  const zoom = useViewportStore((s) => s.zoom);
  const requestFit = useViewportStore((s) => s.requestFit);
  const viewMode = useUiStore((s) => s.viewMode);
  const setViewMode = useUiStore((s) => s.setViewMode);
  const rightPanelOpen = useUiStore((s) => s.rightPanelOpen);
  const toggleRightPanel = useUiStore((s) => s.toggleRightPanel);
  const [dragOver, setDragOver] = useState(false);
  const [exportMenuOpen, setExportMenuOpen] = useState(false);
  const exportRef = useRef<HTMLDivElement>(null);
  const fileInputRef = useRef<HTMLInputElement>(null);

  useEffect(() => {
    void refresh();
  }, [refresh]);

  // close the Export menu on outside click or Escape
  useEffect(() => {
    if (!exportMenuOpen) return;
    const onDown = (e: MouseEvent) => {
      if (!exportRef.current?.contains(e.target as Node)) {
        setExportMenuOpen(false);
      }
    };
    const onEsc = (e: KeyboardEvent) => {
      if (e.key === "Escape") setExportMenuOpen(false);
    };
    window.addEventListener("mousedown", onDown);
    window.addEventListener("keydown", onEsc);
    return () => {
      window.removeEventListener("mousedown", onDown);
      window.removeEventListener("keydown", onEsc);
    };
  }, [exportMenuOpen]);

  // OS file drag & drop (Tauri intercepts HTML5 drop events)
  useEffect(() => {
    // Browser preview (npm run dev, no Tauri): the webview API is absent.
    if (!("__TAURI_INTERNALS__" in window)) return;
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

  // Browser download helper (web export/save has no filesystem path).
  const downloadBytes = (bytes: Uint8Array, filename: string) => {
    const blob = new Blob([bytes as BlobPart], {
      type: "application/octet-stream",
    });
    const url = URL.createObjectURL(blob);
    const a = document.createElement("a");
    a.href = url;
    a.download = filename;
    a.click();
    setTimeout(() => URL.revokeObjectURL(url), 1000);
  };

  const handleOpen = async () => {
    if (!engine.IS_TAURI) {
      fileInputRef.current?.click(); // web: native <input type=file> picker
      return;
    }
    const path = await open({
      multiple: false,
      filters: [{ name: "Embroidery", extensions: OPENABLE }],
    });
    if (typeof path === "string") await openFile(path);
  };

  const onFileInputChange = async (e: React.ChangeEvent<HTMLInputElement>) => {
    const file = e.target.files?.[0];
    e.target.value = ""; // allow re-picking the same file
    if (!file) return;
    const bytes = new Uint8Array(await file.arrayBuffer());
    await openBytes(file.name, bytes);
  };

  const handleExport = async (format: string, ext: string) => {
    if (!engine.IS_TAURI) {
      const bytes = await engine.exportDocumentBytes(format);
      if (bytes.length) downloadBytes(bytes, `untitled.${ext}`);
      return;
    }
    const path = await save({
      filters: [{ name: format, extensions: [ext] }],
      defaultPath: `untitled.${ext}`,
    });
    if (path) await engine.exportFile(path, format);
  };

  const handleSaveAs = async () => {
    if (!engine.IS_TAURI) {
      const bytes = await engine.exportDocumentBytes("PPES");
      if (bytes.length) downloadBytes(bytes, projectPath ?? "untitled.ppes");
      return;
    }
    const path = await save({
      filters: [{ name: "PES Project", extensions: ["ppes"] }],
      defaultPath: projectPath ?? "untitled.ppes",
    });
    if (path) await saveProject(path);
  };

  const handleSave = async () => {
    if (projectPath) await saveProject(projectPath);
    else await handleSaveAs();
  };

  // keyboard shortcuts
  useEffect(() => {
    const onKey = (e: KeyboardEvent) => {
      const tag = (e.target as HTMLElement)?.tagName;
      const typing = tag === "INPUT" || tag === "TEXTAREA";
      const mod = e.metaKey || e.ctrlKey;
      if (!typing && (e.key === "Delete" || e.key === "Backspace")) {
        // the edit layers handle Delete (remove node / stitch point / satin knot)
        if (
          viewMode === "pathEdit" ||
          viewMode === "stitchEdit" ||
          viewMode === "satinDraw"
        )
          return;
        void deleteSelected();
      } else if (mod && e.key.toLowerCase() === "z") {
        e.preventDefault();
        if (e.shiftKey) void redo();
        else void undo();
      } else if (mod && e.key.toLowerCase() === "d") {
        e.preventDefault();
        void duplicateSelected();
      } else if (mod && e.key.toLowerCase() === "s") {
        e.preventDefault();
        if (e.shiftKey) void handleSaveAs();
        else void handleSave();
      } else if (mod && e.key === "0") {
        e.preventDefault();
        requestFit();
      }
    };
    window.addEventListener("keydown", onKey);
    return () => window.removeEventListener("keydown", onKey);
    // eslint-disable-next-line react-hooks/exhaustive-deps
  }, [
    deleteSelected,
    duplicateSelected,
    undo,
    redo,
    requestFit,
    viewMode,
    projectPath,
  ]);

  const selectedObj = doc?.objects.find((o) => o.index === selectedIndex);

  return (
    <div className="flex h-screen flex-col bg-neutral-100 text-sm text-neutral-800">
      {/* web file-open picker (desktop uses the native Tauri dialog) */}
      <input
        ref={fileInputRef}
        type="file"
        accept=".ppes,.ppes5,.pes,.svg"
        className="hidden"
        onChange={onFileInputChange}
      />
      {/* Toolbar */}
      <div className="flex items-center gap-1 border-b border-neutral-200 bg-white px-2 py-1.5 shadow-sm">
        <img
          src={pesLogo}
          alt="PES"
          draggable={false}
          className="mx-1.5 h-7 w-7 select-none rounded-md"
        />
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
        <div className="relative" ref={exportRef}>
          <ToolButton
            label="Export"
            icon={<Download size={16} />}
            active={exportMenuOpen}
            disabled={!doc || doc.objects.length === 0}
            onClick={() => setExportMenuOpen((v) => !v)}
          />
          {exportMenuOpen && (
            <div className="absolute left-0 top-full z-10 mt-1 flex min-w-36 flex-col rounded-md border border-neutral-200 bg-white py-1 shadow-lg">
              {EXPORT_FORMATS.map((f) => (
                <button
                  key={f.format}
                  className="px-3 py-1.5 text-left hover:bg-blue-50"
                  onClick={() => {
                    setExportMenuOpen(false);
                    void handleExport(f.format, f.ext);
                  }}
                >
                  {f.label}
                </button>
              ))}
            </div>
          )}
        </div>

        <Divider />
        <ToolButton
          label="Save"
          icon={<Save size={16} />}
          disabled={!doc}
          onClick={() => void handleSave()}
        />
        <ToolButton
          label="Save As"
          icon={<SaveAll size={16} />}
          disabled={!doc}
          onClick={() => void handleSaveAs()}
        />

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
          label="Edit Nodes"
          icon={<Spline size={16} />}
          active={viewMode === "pathEdit"}
          disabled={selectedIndex < 0}
          onClick={() =>
            setViewMode(viewMode === "pathEdit" ? "design" : "pathEdit")
          }
        />
        <ToolButton
          label="Edit Stitches"
          icon={<Waypoints size={16} />}
          active={viewMode === "stitchEdit"}
          disabled={selectedIndex < 0}
          onClick={() =>
            setViewMode(viewMode === "stitchEdit" ? "design" : "stitchEdit")
          }
        />
        <ToolButton
          label="Stitch View"
          icon={<SewingMachineIcon size={17} />}
          active={viewMode === "stitch"}
          disabled={!doc || doc.objects.length === 0}
          onClick={() =>
            setViewMode(viewMode === "stitch" ? "design" : "stitch")
          }
        />

        {busy && (
          <span className="ml-auto mr-2 animate-pulse text-xs text-blue-500">
            กำลังประมวลผล…
          </span>
        )}
        <ToolButton
          label="Panel"
          icon={
            rightPanelOpen ? (
              <PanelRightClose size={16} />
            ) : (
              <PanelRightOpen size={16} />
            )
          }
          active={rightPanelOpen}
          onClick={toggleRightPanel}
          className={busy ? "" : "ml-auto"}
        />
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
        <div className="relative flex min-w-0 flex-1 flex-col">
          <div className="min-h-0 flex-1">
            <EmbroideryStage />
          </div>
          <RadialToolMenu />
          {viewMode === "stitch" && <SimulatorBar />}
          {viewMode === "stitchEdit" && (
            <div className="pointer-events-none absolute bottom-4 left-1/2 z-10 -translate-x-1/2 rounded-full bg-neutral-900/80 px-4 py-1.5 text-[11px] text-neutral-200 shadow-lg ring-1 ring-white/10">
              ลากจุดเพื่อย้าย · <b>ดับเบิลคลิกที่เส้น</b> แทรกจุด · <b>Delete</b> ลบจุด
            </div>
          )}
          {viewMode === "satinDraw" && (
            <div className="pointer-events-none absolute bottom-4 left-1/2 z-10 -translate-x-1/2 rounded-full bg-neutral-900/80 px-4 py-1.5 text-[11px] text-neutral-200 shadow-lg ring-1 ring-white/10">
              <b>คลิก</b> จุดมุม · <b>Shift+คลิก</b> จุดโค้ง · สลับซ้าย–ขวาเป็นคู่ ·{" "}
              <b>Enter</b> เสร็จ · <b>Backspace</b> ลบจุด · <b>Esc</b> ยกเลิก
            </div>
          )}
        </div>
        <div
          className={`shrink-0 overflow-hidden transition-[width] duration-300 ease-in-out ${
            rightPanelOpen ? "w-72" : "w-0"
          }`}
        >
          <div
            className={`h-full w-72 transition-transform duration-300 ease-in-out ${
              rightPanelOpen ? "translate-x-0" : "translate-x-full"
            }`}
          >
            <Sidebar />
          </div>
        </div>
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
  className = "",
}: {
  label: string;
  icon: React.ReactNode;
  onClick?: () => void;
  disabled?: boolean;
  active?: boolean;
  className?: string;
}) {
  return (
    // group/tip wrapper so the tooltip shows even on disabled buttons and works
    // in the Tauri WKWebView (which doesn't render the native `title` tooltip).
    <span className={`group relative inline-flex ${className}`}>
      <button
        className={`flex items-center justify-center rounded-md p-2 hover:bg-neutral-100 active:bg-neutral-200 disabled:opacity-35 disabled:hover:bg-transparent ${
          active ? "bg-blue-50 text-blue-600" : "text-neutral-600"
        }`}
        onClick={onClick}
        disabled={disabled}
        aria-label={label}
      >
        {icon}
      </button>
      <span
        role="tooltip"
        className="pointer-events-none absolute left-1/2 top-full z-50 mt-1.5 -translate-x-1/2 translate-y-1 whitespace-nowrap rounded-md bg-neutral-800 px-2 py-1 text-[11px] font-medium text-white opacity-0 shadow-lg ring-1 ring-black/5 transition-all duration-150 group-hover:translate-y-0 group-hover:opacity-100"
      >
        {label}
      </span>
    </span>
  );
}

function Divider() {
  return <div className="mx-1 h-6 w-px bg-neutral-200" />;
}
