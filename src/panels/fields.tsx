import { useEffect, useRef, useState } from "react";
import { Minus, Plus } from "lucide-react";
import type { BrotherColor } from "../engine/types";

/** Shared form controls for property panels — visual style follows the old
 *  app's label-left / control-right rows with ± stepper inputs. */

export function Section({ title }: { title: string }) {
  return (
    <div className="border-b border-neutral-100 bg-neutral-50 px-3 py-1 text-xs font-semibold text-neutral-500">
      {title}
    </div>
  );
}

export function Row({
  label,
  children,
}: {
  label: string;
  children: React.ReactNode;
}) {
  return (
    <div className="flex items-center justify-between gap-2 px-3 py-1.5">
      <span className="w-1/2 text-xs text-neutral-600">{label}</span>
      <div className="flex w-1/2 items-center justify-end">{children}</div>
    </div>
  );
}

export function NumberField({
  value,
  min,
  max,
  step,
  onCommit,
}: {
  value: number;
  min: number;
  max: number;
  step: number;
  onCommit: (v: number) => void;
}) {
  const [text, setText] = useState(String(value));
  useEffect(() => setText(String(value)), [value]);

  const clamp = (v: number) => Math.min(max, Math.max(min, v));
  const commit = (v: number) => {
    if (!isNaN(v) && v !== value) onCommit(clamp(v));
    else setText(String(value));
  };

  return (
    <div className="flex h-6 items-stretch overflow-hidden rounded border border-neutral-300">
      <button
        className="bg-neutral-100 px-1 text-neutral-500 hover:bg-neutral-200"
        onClick={() => commit(value - step)}
      >
        <Minus size={11} />
      </button>
      <input
        className="w-14 bg-white text-center text-xs outline-none"
        value={text}
        onChange={(e) => setText(e.target.value)}
        onBlur={() => commit(parseFloat(text))}
        onKeyDown={(e) => {
          if (e.key === "Enter") (e.target as HTMLInputElement).blur();
        }}
      />
      <button
        className="bg-neutral-100 px-1 text-neutral-500 hover:bg-neutral-200"
        onClick={() => commit(value + step)}
      >
        <Plus size={11} />
      </button>
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
      className="h-6 w-full rounded border border-neutral-300 bg-white px-1 text-xs"
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
        className="flex h-6 w-full items-center gap-1.5 rounded border border-neutral-300 bg-white px-1.5 text-xs hover:border-blue-300"
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
