import { useEffect, useRef, useState } from "react";
import { ChevronRight, ChevronUp, ChevronDown } from "lucide-react";
import type { BrotherColor } from "../engine/types";

/** Shared form controls for property panels — collapsible groups with
 *  right-aligned labels on the left and macOS-style stacked spinners,
 *  matching the reference design. */

/** Collapsible section. Header shows a rotating chevron; click toggles its
 *  body. `defaultOpen` controls the initial state. */
export function Group({
  title,
  defaultOpen = true,
  children,
}: {
  title: string;
  defaultOpen?: boolean;
  children: React.ReactNode;
}) {
  const [open, setOpen] = useState(defaultOpen);
  return (
    <div className="border-b border-neutral-100">
      <button
        className="flex w-full items-center gap-1 bg-neutral-50 px-2.5 py-1.5 text-left text-xs font-semibold text-neutral-500 hover:bg-neutral-100"
        onClick={() => setOpen((o) => !o)}
      >
        <ChevronRight
          size={13}
          className={`shrink-0 transition-transform ${open ? "rotate-90" : ""}`}
        />
        <span>{title}</span>
      </button>
      {open && <div className="py-1">{children}</div>}
    </div>
  );
}

/** Fixed width of the right-hand control column — shared so checkbox rows
 *  line up with dropdowns/spinners. */
const CONTROL_COL = "w-[150px] shrink-0";

export function Row({
  label,
  children,
}: {
  label: string;
  children: React.ReactNode;
}) {
  return (
    <div className="flex items-center gap-2 px-2.5 py-1">
      <span className="flex-1 text-right text-xs text-neutral-500">
        {label}:
      </span>
      <div className={`flex items-center justify-end ${CONTROL_COL}`}>
        {children}
      </div>
    </div>
  );
}

/** Checkbox whose box aligns with the left edge of the control column
 *  (same x as dropdowns/spinners), rather than the panel's right edge. */
export function CheckRow({
  label,
  checked,
  onChange,
}: {
  label: string;
  checked: boolean;
  onChange: (v: boolean) => void;
}) {
  return (
    <div className="flex items-center gap-2 px-2.5 py-1">
      <span className="flex-1" />
      <div className={CONTROL_COL}>
        <CheckboxField label={label} checked={checked} onChange={onChange} />
      </div>
    </div>
  );
}

export function NumberField({
  value,
  min,
  max,
  step,
  unit,
  onCommit,
}: {
  value: number;
  min: number;
  max: number;
  step: number;
  unit?: string;
  onCommit: (v: number) => void;
}) {
  const [text, setText] = useState(String(value));
  useEffect(() => setText(String(value)), [value]);

  const clamp = (v: number) => Math.min(max, Math.max(min, v));
  // keep the displayed precision in step with the step size
  const round = (v: number) => {
    const dp = (String(step).split(".")[1] ?? "").length;
    return parseFloat(v.toFixed(dp));
  };
  const commit = (v: number) => {
    if (!isNaN(v) && v !== value) onCommit(clamp(round(v)));
    else setText(String(value));
  };

  return (
    <div className="flex h-7 w-full items-stretch overflow-hidden rounded border border-neutral-300 bg-white focus-within:border-blue-400">
      <input
        className="min-w-0 flex-1 bg-transparent pl-2 text-right text-xs tabular-nums outline-none"
        value={text}
        onChange={(e) => setText(e.target.value)}
        onBlur={() => commit(parseFloat(text))}
        onKeyDown={(e) => {
          if (e.key === "Enter") (e.target as HTMLInputElement).blur();
          else if (e.key === "ArrowUp") {
            e.preventDefault();
            commit(value + step);
          } else if (e.key === "ArrowDown") {
            e.preventDefault();
            commit(value - step);
          }
        }}
      />
      {unit && (
        <span className="flex items-center px-1 text-[11px] text-neutral-400">
          {unit}
        </span>
      )}
      <div className="flex w-4 flex-col border-l border-neutral-200">
        <button
          tabIndex={-1}
          className="flex flex-1 items-center justify-center text-neutral-500 hover:bg-neutral-100"
          onClick={() => commit(value + step)}
        >
          <ChevronUp size={10} />
        </button>
        <button
          tabIndex={-1}
          className="flex flex-1 items-center justify-center border-t border-neutral-200 text-neutral-500 hover:bg-neutral-100"
          onClick={() => commit(value - step)}
        >
          <ChevronDown size={10} />
        </button>
      </div>
    </div>
  );
}

export function SelectField<T extends number | string>({
  value,
  options,
  onChange,
}: {
  value: T;
  options: { value: T; label: string }[];
  onChange: (v: T) => void;
}) {
  return (
    <select
      className="h-7 w-full rounded border border-neutral-300 bg-white px-1.5 text-xs"
      value={String(value)}
      onChange={(e) => {
        const raw = e.target.value;
        const opt = options.find((o) => String(o.value) === raw);
        if (opt) onChange(opt.value);
      }}
    >
      {options.map((o) => (
        <option key={String(o.value)} value={String(o.value)}>
          {o.label}
        </option>
      ))}
    </select>
  );
}

export function CheckboxField({
  label,
  checked,
  onChange,
}: {
  label: string;
  checked: boolean;
  onChange: (v: boolean) => void;
}) {
  return (
    <label className="flex cursor-pointer items-center gap-1.5 text-xs text-neutral-700">
      <input
        type="checkbox"
        className="h-4 w-4 accent-blue-600"
        checked={checked}
        onChange={(e) => onChange(e.target.checked)}
      />
      {label}
    </label>
  );
}

export function ColorField({
  palette,
  brotherIndex,
  onPick,
}: {
  palette: BrotherColor[];
  brotherIndex: number;
  onPick: (c: BrotherColor) => void;
}) {
  const [open, setOpen] = useState(false);
  const current = palette.find((c) => c.index === brotherIndex);
  return (
    <div className="relative w-full">
      <button
        className="flex h-7 w-full items-center gap-1.5 rounded border border-neutral-300 bg-white px-1.5 text-xs hover:border-blue-300"
        onClick={() => setOpen(!open)}
      >
        <span
          className="h-3.5 w-3.5 rounded-sm border border-neutral-300"
          style={{ backgroundColor: current?.hex ?? "#fff" }}
        />
        <span className="truncate text-neutral-600">
          {current?.name ?? "—"}
        </span>
      </button>
      {open && (
        <PaletteGrid
          palette={palette}
          current={brotherIndex}
          onPick={(c) => {
            setOpen(false);
            onPick(c);
          }}
          onClose={() => setOpen(false)}
        />
      )}
    </div>
  );
}

export function PaletteGrid({
  palette,
  current,
  onPick,
  onClose,
}: {
  palette: BrotherColor[];
  current?: number;
  onPick: (c: BrotherColor) => void;
  onClose: () => void;
}) {
  const ref = useRef<HTMLDivElement>(null);
  useEffect(() => {
    const onDown = (e: MouseEvent) => {
      if (ref.current && !ref.current.contains(e.target as Node)) onClose();
    };
    window.addEventListener("mousedown", onDown);
    return () => window.removeEventListener("mousedown", onDown);
  }, [onClose]);

  return (
    <div
      ref={ref}
      className="absolute right-0 top-7 z-30 w-56 rounded-lg border border-neutral-200 bg-white p-2 shadow-xl"
    >
      <div className="grid grid-cols-8 gap-1">
        {palette.map((c) => (
          <button
            key={c.index}
            className={`h-6 w-6 rounded-sm border transition-transform hover:scale-125 hover:border-blue-400 ${
              c.index === current
                ? "border-2 border-blue-500"
                : "border-neutral-300"
            }`}
            style={{ backgroundColor: c.hex }}
            title={`${c.index}. ${c.name}`}
            onClick={() => onPick(c)}
          />
        ))}
      </div>
    </div>
  );
}
