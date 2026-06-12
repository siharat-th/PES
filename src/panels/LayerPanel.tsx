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

  const objects = doc?.objects ?? [];
  // top of the list = front-most (drawn last) — matches the old app
  const rows = [...objects].reverse();

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
            className={`flex cursor-pointer items-center gap-2 border-b border-neutral-100 px-2 py-1.5 ${
              selectedIndices.includes(obj.index)
                ? "bg-blue-50 ring-1 ring-inset ring-blue-300"
                : "hover:bg-neutral-50"
            }`}
            onClick={(e) => select(obj.index, e.shiftKey)}
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
