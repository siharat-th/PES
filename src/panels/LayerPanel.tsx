import { useEffect, useRef, useState } from "react";
import { createPortal } from "react-dom";
import { invoke } from "../engine/transport";
import {
  Eye,
  EyeOff,
  Lock,
  LockOpen,
  ChevronUp,
  ChevronDown,
  ChevronRight,
  FolderPlus,
  Maximize2,
  Ungroup,
  Trash2,
} from "lucide-react";
import { useDocumentStore } from "../state/documentStore";
import type { ObjectSnapshot, GroupSnapshot } from "../engine/types";

// Only genuine text objects carry a meaningful `text`; imported "Stitch"/PES2
// layers keep a leftover source string (e.g. "ภิญญ์จักรปัก สวัสดีค่ะ") that is
// noise in the layer list, so we only show the subtitle for these types.
const TEXT_TYPES = new Set(["PPEF Text", "TTF Text", "Monogram"]);
const showsText = (o: ObjectSnapshot) => TEXT_TYPES.has(o.object_type) && !!o.text;

/** Right-side layer list — port of Prop_LayerHandler.js, now with layer groups:
 *  folder-like headers (collapse/expand, rename, cascade hide/lock, select all),
 *  with ungrouped objects and groups projected into one tree. */
export default function LayerPanel() {
  const doc = useDocumentStore((s) => s.doc);
  const imageVersion = useDocumentStore((s) => s.imageVersion);
  const selectedIndices = useDocumentStore((s) => s.selectedIndices);
  const select = useDocumentStore((s) => s.select);
  const setVisible = useDocumentStore((s) => s.setVisible);
  const setLocked = useDocumentStore((s) => s.setLocked);
  const reorder = useDocumentStore((s) => s.reorder);
  const reorderTo = useDocumentStore((s) => s.reorderTo);
  const selectGroup = useDocumentStore((s) => s.selectGroup);
  const createGroup = useDocumentStore((s) => s.createGroup);
  const renameGroup = useDocumentStore((s) => s.renameGroup);
  const ungroup = useDocumentStore((s) => s.ungroup);
  const deleteGroup = useDocumentStore((s) => s.deleteGroup);
  const addToGroup = useDocumentStore((s) => s.addToGroup);
  const removeFromGroup = useDocumentStore((s) => s.removeFromGroup);
  const setGroupCollapsed = useDocumentStore((s) => s.setGroupCollapsed);
  const setGroupVisible = useDocumentStore((s) => s.setGroupVisible);
  const setGroupLocked = useDocumentStore((s) => s.setGroupLocked);

  // Pointer-driven reordering / grouping. We can't use HTML5 drag-and-drop:
  // Tauri's native file drag-drop handler intercepts it, so we drive it with
  // pointer events and limit the drop area to the layer rows/headers.
  const [dragIdx, setDragIdx] = useState<number | null>(null);
  const [overIdx, setOverIdx] = useState<number | null>(null);
  const [overGroup, setOverGroup] = useState<number | null>(null);
  const [dragPos, setDragPos] = useState<{ x: number; y: number } | null>(null);
  const dragStart = useRef<{
    idx: number;
    y: number;
    left: number;
    width: number;
    grabDy: number;
  } | null>(null);
  const didDrag = useRef(false);

  // inline group rename
  const [renamingId, setRenamingId] = useState<number | null>(null);
  const [renameText, setRenameText] = useState("");

  const objects = doc?.objects ?? [];
  const groups = doc?.groups ?? [];
  const draggedObj =
    dragIdx !== null ? objects.find((o) => o.index === dragIdx) : undefined;

  // ---- Tree projection (back-most first, matching the old PES app) -------
  // The old app's layer list shows object index 0 (back-most, stitched first)
  // at the TOP and the front-most layer at the BOTTOM. Each group clusters its
  // members under one header; ungrouped objects stay at top level. A group
  // anchors at its back-most member (empty groups pin to the top so they stay
  // visible/droppable).
  const groupIds = new Set(groups.map((g) => g.id));
  const membersOf = (id: number) =>
    objects
      .filter((o) => o.group_id === id)
      .sort((a, b) => a.index - b.index);

  type Entry =
    | { kind: "object"; key: string; sort: number; obj: ObjectSnapshot }
    | {
        kind: "group";
        key: string;
        sort: number;
        group: GroupSnapshot;
        members: ObjectSnapshot[];
      };

  const entries: Entry[] = [];
  for (const o of objects) {
    if (!o.group_id || !groupIds.has(o.group_id)) {
      entries.push({ kind: "object", key: `o${o.index}`, sort: o.index, obj: o });
    }
  }
  for (const g of groups) {
    const members = membersOf(g.id);
    const sort = members.length
      ? Math.min(...members.map((m) => m.index))
      : -1;
    entries.push({ kind: "group", key: `g${g.id}`, sort, group: g, members });
  }
  entries.sort((a, b) => a.sort - b.sort);

  // ---- Drag helpers -----------------------------------------------------
  /** Resolve what's under a screen point: the layer row index and/or the
   *  enclosing group container (null group = ungrouped area). */
  const resolveDrop = (x: number, y: number) => {
    const el = document.elementFromPoint(x, y) as HTMLElement | null;
    const rowEl = el?.closest("[data-layer-index]") as HTMLElement | null;
    const groupEl = el?.closest("[data-group-id]") as HTMLElement | null;
    return {
      overIndex: rowEl ? Number(rowEl.dataset.layerIndex) : null,
      overGroupId: groupEl ? Number(groupEl.dataset.groupId) : null,
    };
  };

  const onRowPointerDown = (e: React.PointerEvent, idx: number) => {
    if (e.button !== 0) return;
    if ((e.target as HTMLElement).closest("button,input")) return;
    const rect = e.currentTarget.getBoundingClientRect();
    dragStart.current = {
      idx,
      y: e.clientY,
      left: rect.left,
      width: rect.width,
      grabDy: e.clientY - rect.top,
    };
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
    const { overIndex, overGroupId } = resolveDrop(e.clientX, e.clientY);
    setOverIdx(overIndex);
    setOverGroup(overGroupId);
  };

  const onRowPointerUp = (e: React.PointerEvent) => {
    const st = dragStart.current;
    dragStart.current = null;
    if (e.currentTarget.hasPointerCapture?.(e.pointerId))
      e.currentTarget.releasePointerCapture(e.pointerId);
    if (st && didDrag.current) {
      const from = st.idx;
      const { overIndex, overGroupId } = resolveDrop(e.clientX, e.clientY);
      const dragged = objects.find((o) => o.index === from);
      const curGroup = dragged?.group_id ?? 0;
      const targetGroup = overGroupId ?? 0;
      // One op per drop: crossing a group boundary changes membership;
      // staying inside the same group reorders z-order.
      if (targetGroup !== curGroup) {
        if (targetGroup === 0) void removeFromGroup([from]);
        else void addToGroup(targetGroup, [from]);
      } else if (overIndex !== null && overIndex !== from) {
        void reorderTo(from, overIndex);
      }
    }
    setDragIdx(null);
    setOverIdx(null);
    setOverGroup(null);
    setDragPos(null);
  };

  // ---- New group --------------------------------------------------------
  const handleNewGroup = async () => {
    const before = new Set(
      (useDocumentStore.getState().doc?.groups ?? []).map((g) => g.id),
    );
    await createGroup("กลุ่มใหม่", [...selectedIndices]);
    const fresh = (useDocumentStore.getState().doc?.groups ?? []).find(
      (g) => !before.has(g.id),
    );
    if (fresh) {
      setRenamingId(fresh.id);
      setRenameText(fresh.name);
    }
  };

  const commitRename = () => {
    if (renamingId !== null) {
      const name = renameText.trim();
      if (name) void renameGroup(renamingId, name);
    }
    setRenamingId(null);
  };

  // ---- Row renderers ----------------------------------------------------
  const objectRow = (obj: ObjectSnapshot, inGroup: boolean) => (
    <div
      key={`o${obj.index}`}
      data-layer-index={obj.index}
      title="ลากเพื่อจัดลำดับ หรือลากเข้า/ออกกลุ่ม"
      className={[
        "flex cursor-grab touch-none select-none items-center gap-2 border-b border-neutral-100 py-1.5 pr-2 active:cursor-grabbing",
        inGroup ? "pl-7" : "pl-2",
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
          return;
        }
        select(obj.index, e.shiftKey);
      }}
      onPointerDown={(e) => onRowPointerDown(e, obj.index)}
      onPointerMove={onRowPointerMove}
      onPointerUp={onRowPointerUp}
    >
      <LayerThumb index={obj.index} version={imageVersion} />
      <div className="min-w-0 flex-1">
        <div className="truncate text-xs font-medium">{obj.object_type}</div>
        {showsText(obj) && (
          <div className="truncate text-[11px] text-neutral-500">{obj.text}</div>
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
          title="เลื่อนขึ้น (ไปหลัง)"
          disabled={obj.index <= 0}
          onClick={(e) => {
            e.stopPropagation();
            void reorder(obj.index, -1);
          }}
        >
          <ChevronUp size={13} />
        </button>
        <button
          className="icon-btn text-neutral-400"
          title="เลื่อนลง (มาหน้า)"
          disabled={obj.index >= objects.length - 1}
          onClick={(e) => {
            e.stopPropagation();
            void reorder(obj.index, +1);
          }}
        >
          <ChevronDown size={13} />
        </button>
      </div>
    </div>
  );

  const groupHeader = (group: GroupSnapshot, members: ObjectSnapshot[]) => {
    const memberIdx = members.map((m) => m.index);
    const allVisible = members.length > 0 && members.every((m) => m.visible);
    const allLocked = members.length > 0 && members.every((m) => m.locked);
    const allSelected =
      members.length > 0 && memberIdx.every((i) => selectedIndices.includes(i));
    const isDropTarget = overGroup === group.id && dragIdx !== null;
    return (
      <div
        data-group-header
        className={[
          "flex select-none items-center gap-1.5 border-b border-neutral-200 bg-neutral-50/80 py-1.5 pl-2 pr-2",
          allSelected ? "bg-blue-50 ring-1 ring-inset ring-blue-300" : "",
          isDropTarget ? "ring-2 ring-inset ring-blue-400" : "",
        ].join(" ")}
        onClick={(e) => {
          if ((e.target as HTMLElement).closest("button,input")) return;
          selectGroup(group.id, e.shiftKey);
        }}
      >
        <button
          className="icon-btn text-neutral-500"
          title={group.collapsed ? "ขยาย" : "ย่อ"}
          onClick={(e) => {
            e.stopPropagation();
            void setGroupCollapsed(group.id, !group.collapsed);
          }}
        >
          {group.collapsed ? (
            <ChevronRight size={14} />
          ) : (
            <ChevronDown size={14} />
          )}
        </button>
        <div className="min-w-0 flex-1">
          {renamingId === group.id ? (
            <input
              autoFocus
              className="w-full rounded border border-blue-300 px-1 py-0.5 text-xs outline-none"
              value={renameText}
              onChange={(e) => setRenameText(e.target.value)}
              onBlur={commitRename}
              onKeyDown={(e) => {
                if (e.key === "Enter") commitRename();
                else if (e.key === "Escape") setRenamingId(null);
              }}
              onClick={(e) => e.stopPropagation()}
            />
          ) : (
            <div
              className="flex items-center gap-1.5"
              onDoubleClick={(e) => {
                e.stopPropagation();
                setRenamingId(group.id);
                setRenameText(group.name);
              }}
              title="ดับเบิลคลิกเพื่อเปลี่ยนชื่อ"
            >
              <span className="truncate text-xs font-semibold text-neutral-700">
                {group.name || "กลุ่ม"}
              </span>
              <span className="text-[10px] text-neutral-400">
                {members.length}
              </span>
              {!group.scalable && (
                <Maximize2
                  size={11}
                  className="text-neutral-300"
                  aria-label="no-scale"
                />
              )}
            </div>
          )}
        </div>
        <button
          className={`icon-btn ${allVisible ? "text-neutral-500" : "text-neutral-300"}`}
          title={allVisible ? "ซ่อนทั้งกลุ่ม" : "แสดงทั้งกลุ่ม"}
          onClick={(e) => {
            e.stopPropagation();
            void setGroupVisible(group.id, !allVisible);
          }}
        >
          {allVisible ? <Eye size={14} /> : <EyeOff size={14} />}
        </button>
        <button
          className={`icon-btn ${allLocked ? "text-amber-500" : "text-neutral-300"}`}
          title={allLocked ? "ปลดล็อกทั้งกลุ่ม" : "ล็อกทั้งกลุ่ม"}
          onClick={(e) => {
            e.stopPropagation();
            void setGroupLocked(group.id, !allLocked);
          }}
        >
          {allLocked ? <Lock size={14} /> : <LockOpen size={14} />}
        </button>
        <button
          className="icon-btn text-neutral-400 hover:text-neutral-600"
          title="ยุบกลุ่ม (เก็บ object ไว้)"
          onClick={(e) => {
            e.stopPropagation();
            void ungroup(group.id);
          }}
        >
          <Ungroup size={14} />
        </button>
        <button
          className="icon-btn text-neutral-400 hover:text-red-500"
          title="ลบทั้งกลุ่ม (รวม object ข้างใน)"
          onClick={(e) => {
            e.stopPropagation();
            void deleteGroup(group.id);
          }}
        >
          <Trash2 size={14} />
        </button>
      </div>
    );
  };

  return (
    <div className="flex h-full flex-col">
      <div className="flex items-center justify-between border-b border-neutral-200 px-2 py-1.5">
        <span className="text-[11px] font-semibold text-neutral-500">Layers</span>
        <button
          className="flex items-center gap-1 rounded px-1.5 py-1 text-[11px] text-neutral-600 hover:bg-neutral-100"
          title={
            selectedIndices.length
              ? "สร้างกลุ่มจาก object ที่เลือก"
              : "สร้างกลุ่มเปล่า"
          }
          onClick={() => void handleNewGroup()}
        >
          <FolderPlus size={14} />
          กลุ่มใหม่
        </button>
      </div>

      <div className="min-h-0 flex-1 overflow-y-auto">
        {entries.length === 0 && (
          <div className="px-3 py-4 text-xs text-neutral-400">
            ยังไม่มี object — เปิดไฟล์หรือ import ก่อน
          </div>
        )}
        {entries.map((e) =>
          e.kind === "object" ? (
            objectRow(e.obj, false)
          ) : (
            <div key={e.key} data-group-id={e.group.id}>
              {groupHeader(e.group, e.members)}
              {!e.group.collapsed &&
                e.members.map((m) => objectRow(m, true))}
            </div>
          ),
        )}
      </div>

      {/* Floating preview of the dragged layer, following the cursor. Portaled
          to <body> so it escapes the sliding panel's transform/overflow. */}
      {draggedObj &&
        dragPos &&
        dragStart.current &&
        createPortal(
          <div
            className="pointer-events-none fixed z-50 flex items-center gap-2 rounded border border-blue-300 bg-white/95 px-2 py-1.5 shadow-xl"
            style={{
              left: dragStart.current.left,
              width: dragStart.current.width,
              top: dragPos.y - dragStart.current.grabDy,
            }}
          >
            <LayerThumb index={draggedObj.index} version={imageVersion} />
            <div className="min-w-0 flex-1">
              <div className="truncate text-xs font-medium">
                {draggedObj.object_type}
              </div>
              {showsText(draggedObj) && (
                <div className="truncate text-[11px] text-neutral-500">
                  {draggedObj.text}
                </div>
              )}
            </div>
          </div>,
          document.body,
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
    <div className="flex h-14 w-14 shrink-0 items-center justify-center rounded-md border border-neutral-200 bg-neutral-50">
      {url ? (
        <img src={url} className="max-h-[52px] max-w-[52px] object-contain" alt="" />
      ) : (
        <span className="text-[10px] text-neutral-300">…</span>
      )}
    </div>
  );
}
