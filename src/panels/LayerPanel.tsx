import { useEffect, useRef, useState } from "react";
import { invoke } from "@tauri-apps/api/core";
import {
  Eye,
  EyeOff,
  Lock,
  LockOpen,
  ChevronUp,
  ChevronDown,
} from "lucide-react";
import { useDocumentStore } from "../state/documentStore";

/** Right-side layer list — port of Prop_LayerHandler.js:
 *  thumbnail, type/text label, visibility & lock toggles, reorder, select. */
export default function LayerPanel() {
  const doc = useDocumentStore((s) => s.doc);
  const imageVersion = useDocumentStore((s) => s.imageVersion);
  const selectedIndices = useDocumentStore((s) => s.selectedIndices);
  const select = useDocumentStore((s) => s.select);
  const setVisible = useDocumentStore((s) => s.setVisible);
  const setLocked = useDocumentStore((s) => s.setLocked);
  const reorder = useDocumentStore((s) => s.reorder);
  const reorderTo = useDocumentStore((s) => s.reorderTo);

  // Pointer-driven reordering. We can't use HTML5 drag-and-drop: Tauri's
  // native file drag-drop handler intercepts it (lighting up the whole window
  // as a file drop zone and swallowing the row `drop` event), so we drive the
  // reorder with pointer events and limit the drop area to the layer rows.
  const [dragIdx, setDragIdx] = useState<number | null>(null);
  const [overIdx, setOverIdx] = useState<number | null>(null);
  // cursor position for the floating drag preview (viewport coords)
  const [dragPos, setDragPos] = useState<{ x: number; y: number } | null>(null);
  const dragStart = useRef<{ idx: number; y: number } | null>(null);
  const didDrag = useRef(false);

  const objects = doc?.objects ?? [];
  // top of the list = front-most (drawn last) — matches the old app
  const rows = [...objects].reverse();
  const draggedObj =
    dragIdx !== null ? objects.find((o) => o.index === dragIdx) : undefined;

  /** Engine index of the layer row under a screen point, or null if the point
   *  is outside the layer list (keeps the drop area within the panel). */
  const rowAtPoint = (x: number, y: number): number | null => {
    const el = document.elementFromPoint(x, y) as HTMLElement | null;
    const row = el?.closest("[data-layer-index]") as HTMLElement | null;
    return row ? Number(row.dataset.layerIndex) : null;
  };

  const onRowPointerDown = (e: React.PointerEvent, idx: number) => {
    if (e.button !== 0) return;
    // let the eye/lock/chevron toggles handle their own clicks
    if ((e.target as HTMLElement).closest("button")) return;
    dragStart.current = { idx, y: e.clientY };
    didDrag.current = false;
    e.currentTarget.setPointerCapture(e.pointerId);
  };

  const onRowPointerMove = (e: React.PointerEvent) => {
    const st = dragStart.current;
    if (!st) return;
    if (!didDrag.current) {
      if (Math.abs(e.clientY - st.y) < 4) return; // ignore tiny jitter
      didDrag.current = true;
      setDragIdx(st.idx);
    }
    setDragPos({ x: e.clientX, y: e.clientY });
    setOverIdx(rowAtPoint(e.clientX, e.clientY));
  };

  const onRowPointerUp = (e: React.PointerEvent) => {
    const st = dragStart.current;
    dragStart.current = null;
    if (e.currentTarget.hasPointerCapture?.(e.pointerId))
      e.currentTarget.releasePointerCapture(e.pointerId);
    if (st && didDrag.current) {
      const target = rowAtPoint(e.clientX, e.clientY);
      if (target !== null && target !== st.idx) void reorderTo(st.idx, target);
    }
    setDragIdx(null);
    setOverIdx(null);
    setDragPos(null);
  };

  return (
    <div className="flex h-full flex-col">
      <div className="min-h-0 flex-1 overflow-y-auto">
        {rows.length === 0 && (
          <div className="px-3 py-4 text-xs text-neutral-400">
            ยังไม่มี object — เปิดไฟล์หรือ import ก่อน
          </div>
        )}
        {rows.map((obj) => (
          <div
            key={`${obj.index}`}
            data-layer-index={obj.index}
            title="ลากเพื่อจัดลำดับ"
            className={[
              "flex cursor-grab touch-none select-none items-center gap-2 border-b border-neutral-100 px-2 py-1.5 active:cursor-grabbing",
              selectedIndices.includes(obj.index)
                ? "bg-blue-50 ring-1 ring-inset ring-blue-300"
                : "hover:bg-neutral-50",
              dragIdx === obj.index ? "opacity-40" : "",
              overIdx === obj.index && dragIdx !== obj.index
                ? "ring-2 ring-inset ring-blue-400"
                : "",
            ].join(" ")}
            onClick={(e) => {
              if (didDrag.current) {
                didDrag.current = false;
                return; // this "click" was the end of a drag — don't select
              }
              select(obj.index, e.shiftKey);
            }}
            onPointerDown={(e) => onRowPointerDown(e, obj.index)}
            onPointerMove={onRowPointerMove}
            onPointerUp={onRowPointerUp}
          >
            <LayerThumb index={obj.index} version={imageVersion} />
            <div className="min-w-0 flex-1">
              <div className="truncate text-xs font-medium">
                {obj.object_type}
              </div>
              {obj.text && (
                <div className="truncate text-[11px] text-neutral-500">
                  {obj.text}
                </div>
              )}
            </div>
            <button
              className={`icon-btn ${obj.visible ? "text-neutral-500" : "text-neutral-300"}`}
              title={obj.visible ? "ซ่อน" : "แสดง"}
              onClick={(e) => {
                e.stopPropagation();
                void setVisible(obj.index, !obj.visible);
              }}
            >
              {obj.visible ? <Eye size={14} /> : <EyeOff size={14} />}
            </button>
            <button
              className={`icon-btn ${obj.locked ? "text-amber-500" : "text-neutral-300"}`}
              title={obj.locked ? "ปลดล็อก" : "ล็อก"}
              onClick={(e) => {
                e.stopPropagation();
                void setLocked(obj.index, !obj.locked);
              }}
            >
              {obj.locked ? <Lock size={14} /> : <LockOpen size={14} />}
            </button>
            <div className="flex flex-col">
              <button
                className="icon-btn text-neutral-400"
                title="เลื่อนขึ้น (มาหน้า)"
                disabled={obj.index >= objects.length - 1}
                onClick={(e) => {
                  e.stopPropagation();
                  void reorder(obj.index, +1);
                }}
              >
                <ChevronUp size={13} />
              </button>
              <button
                className="icon-btn text-neutral-400"
                title="เลื่อนลง (ไปหลัง)"
                disabled={obj.index <= 0}
                onClick={(e) => {
                  e.stopPropagation();
                  void reorder(obj.index, -1);
                }}
              >
                <ChevronDown size={13} />
              </button>
            </div>
          </div>
        ))}
      </div>

      {/* floating preview of the dragged layer, following the cursor */}
      {draggedObj && dragPos && (
        <div
          className="pointer-events-none fixed z-50 flex w-60 items-center gap-2 rounded border border-blue-300 bg-white/95 px-2 py-1.5 shadow-xl"
          style={{ left: dragPos.x + 12, top: dragPos.y + 8 }}
        >
          <LayerThumb index={draggedObj.index} version={imageVersion} />
          <div className="min-w-0 flex-1">
            <div className="truncate text-xs font-medium">
              {draggedObj.object_type}
            </div>
            {draggedObj.text && (
              <div className="truncate text-[11px] text-neutral-500">
                {draggedObj.text}
              </div>
            )}
          </div>
        </div>
      )}
    </div>
  );
}

function LayerThumb({ index, version }: { index: number; version: number }) {
  const [url, setUrl] = useState<string | null>(null);
  const urlRef = useRef<string | null>(null);

  useEffect(() => {
    let cancelled = false;
    (async () => {
      try {
        const buf = await invoke<ArrayBuffer>("get_object_image", { index });
        if (cancelled || !buf || buf.byteLength === 0) return;
        const next = URL.createObjectURL(
          new Blob([buf], { type: "image/png" }),
        );
        if (urlRef.current) URL.revokeObjectURL(urlRef.current);
        urlRef.current = next;
        setUrl(next);
      } catch {
        /* object may be gone after reorder/delete */
      }
    })();
    return () => {
      cancelled = true;
    };
  }, [index, version]);

  useEffect(
    () => () => {
      if (urlRef.current) URL.revokeObjectURL(urlRef.current);
    },
    [],
  );

  return (
    <div className="flex h-10 w-10 shrink-0 items-center justify-center rounded border border-neutral-200 bg-neutral-50">
      {url ? (
        <img src={url} className="max-h-9 max-w-9 object-contain" alt="" />
      ) : (
        <span className="text-[10px] text-neutral-300">…</span>
      )}
    </div>
  );
}
