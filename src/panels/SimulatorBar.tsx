import { useEffect, useMemo, useRef, useState } from "react";
import {
  Play,
  Pause,
  SkipBack,
  SkipForward,
  Rewind,
  FastForward,
  Minus,
  Plus,
} from "lucide-react";
import { useUiStore } from "../state/uiStore";
import { useDocumentStore } from "../state/documentStore";
import { getStitchData } from "../engine/EngineClient";
import type { StitchData, StitchSegment } from "../engine/EngineClient";

// stitches advanced per 60fps frame → ×60 ≈ stitches/sec (0.25× ≈ 15/sec)
const SPEEDS = [0.25, 0.5, 1, 2, 5, 10, 30, 80, 200];

interface ColorBlock {
  hex: string;
  start: number; // first revealed-point index of this color
  end: number; // one past the last point index
  count: number;
}

/** Group the stitch runs into contiguous same-colour blocks (a colour change
 *  starts a new block; jumps inside one colour stay in the same block). */
function colorBlocks(segments: StitchSegment[]): ColorBlock[] {
  const blocks: ColorBlock[] = [];
  for (const s of segments) {
    const end = s.start + s.count;
    const last = blocks[blocks.length - 1];
    if (last && last.hex === s.hex) {
      last.end = end;
      last.count += s.count;
    } else {
      blocks.push({ hex: s.hex, start: s.start, end, count: s.count });
    }
  }
  return blocks;
}

/** Bottom bar shown in stitch view: transport controls + a colour-segmented
 *  timeline you can scrub, with per-colour jumps and a live colour readout. */
export default function SimulatorBar() {
  const imageVersion = useDocumentStore((s) => s.imageVersion);
  const simIndex = useUiStore((s) => s.simIndex);
  const simPlaying = useUiStore((s) => s.simPlaying);
  const simSpeed = useUiStore((s) => s.simSpeed);
  const setSimIndex = useUiStore((s) => s.setSimIndex);
  const setSimPlaying = useUiStore((s) => s.setSimPlaying);
  const setSimSpeed = useUiStore((s) => s.setSimSpeed);

  const [data, setData] = useState<StitchData | null>(null);
  const raf = useRef<number | null>(null);
  const trackRef = useRef<HTMLDivElement>(null);

  useEffect(() => {
    let cancelled = false;
    void getStitchData(-1).then((d) => {
      if (!cancelled) setData(d);
    });
    return () => {
      cancelled = true;
    };
  }, [imageVersion]);

  const total = data?.totalPoints ?? 0;
  const blocks = useMemo(
    () => (data ? colorBlocks(data.segments) : []),
    [data],
  );

  // playback loop
  useEffect(() => {
    if (!simPlaying || total === 0) return;
    let last = performance.now();
    // Precise fractional playhead kept in the closure — simIndex is floored
    // for display, so re-reading it each frame would drop the fraction and
    // stall any speed below 1 stitch/frame (0.25×, 0.5×). Accumulate here.
    const cur = useUiStore.getState().simIndex;
    let pos = cur < 0 ? 0 : cur;
    const tick = (now: number) => {
      const dt = (now - last) / 16.67; // frames at 60fps
      last = now;
      pos += simSpeed * dt;
      if (pos >= total) {
        setSimIndex(total);
        setSimPlaying(false);
        return;
      }
      setSimIndex(Math.floor(pos));
      raf.current = requestAnimationFrame(tick);
    };
    raf.current = requestAnimationFrame(tick);
    return () => {
      if (raf.current) cancelAnimationFrame(raf.current);
    };
  }, [simPlaying, simSpeed, total, setSimIndex, setSimPlaying]);

  const shown = simIndex < 0 ? total : simIndex;
  const pct = total > 0 ? (shown / total) * 100 : 0;

  // which colour block the playhead currently sits in
  const curIdx = blocks.findIndex((b) => shown < b.end);
  const colorNo = curIdx < 0 ? blocks.length : curIdx + 1;
  const curHex =
    blocks[Math.min(blocks.length - 1, Math.max(0, colorNo - 1))]?.hex ??
    "#888888";

  const play = () => {
    if (simIndex < 0 || simIndex >= total) setSimIndex(0);
    setSimPlaying(true);
  };

  const nextColor = () => {
    const b = blocks.find((bl) => bl.start > shown + 1);
    setSimIndex(b ? b.start : total);
  };

  const prevColor = () => {
    const ci = curIdx < 0 ? blocks.length - 1 : curIdx;
    const curStart = blocks[ci]?.start ?? 0;
    // not yet at this colour's start → snap to it; otherwise go to previous
    setSimIndex(shown > curStart + 1 ? curStart : (blocks[ci - 1]?.start ?? 0));
  };

  const speedIdx = Math.max(0, SPEEDS.indexOf(simSpeed));
  const slower = () => setSimSpeed(SPEEDS[Math.max(0, speedIdx - 1)] ?? 0.25);
  const faster = () =>
    setSimSpeed(SPEEDS[Math.min(SPEEDS.length - 1, speedIdx + 1)] ?? 200);

  const scrubTo = (clientX: number) => {
    const el = trackRef.current;
    if (!el || total === 0) return;
    const rect = el.getBoundingClientRect();
    const ratio = Math.min(1, Math.max(0, (clientX - rect.left) / rect.width));
    setSimIndex(Math.round(ratio * total));
  };

  return (
    <div className="flex items-center gap-2 border-t border-neutral-200 bg-white px-3 py-1.5">
      <button
        className="icon-btn"
        title="ไปต้น"
        onClick={() => {
          setSimPlaying(false);
          setSimIndex(0);
        }}
      >
        <SkipBack size={16} />
      </button>
      <button className="icon-btn" title="สีก่อนหน้า" onClick={prevColor}>
        <Rewind size={16} />
      </button>
      {simPlaying ? (
        <button className="icon-btn" title="หยุด" onClick={() => setSimPlaying(false)}>
          <Pause size={18} />
        </button>
      ) : (
        <button className="icon-btn" title="เล่น" onClick={play}>
          <Play size={18} />
        </button>
      )}
      <button className="icon-btn" title="สีถัดไป" onClick={nextColor}>
        <FastForward size={16} />
      </button>
      <button
        className="icon-btn"
        title="ไปท้าย"
        onClick={() => {
          setSimPlaying(false);
          setSimIndex(total);
        }}
      >
        <SkipForward size={16} />
      </button>

      {/* colour-segmented timeline */}
      <div
        ref={trackRef}
        className="relative h-3.5 min-w-0 flex-1 cursor-pointer touch-none overflow-hidden rounded-full bg-neutral-200 shadow-inner"
        onPointerDown={(e) => {
          setSimPlaying(false);
          e.currentTarget.setPointerCapture(e.pointerId);
          scrubTo(e.clientX);
        }}
        onPointerMove={(e) => {
          if (e.buttons & 1) scrubTo(e.clientX);
        }}
      >
        <div className="flex h-full w-full">
          {blocks.map((b, i) => (
            <div
              key={i}
              style={{ flexGrow: b.count, background: b.hex }}
              className="h-full"
            />
          ))}
        </div>
        {/* dim the not-yet-stitched portion */}
        <div
          className="absolute inset-y-0 right-0 bg-white/60"
          style={{ left: `${pct}%` }}
        />
        {/* playhead */}
        <div
          className="absolute inset-y-0 -ml-px w-0.5 bg-neutral-900/80"
          style={{ left: `${pct}%` }}
        />
      </div>

      {/* current colour */}
      <div className="flex items-center gap-1.5" title="สีที่กำลังปัก">
        <span
          className="h-3.5 w-3.5 rounded-sm border border-black/15 shadow-sm"
          style={{ background: curHex }}
        />
        <span className="w-10 text-[11px] tabular-nums text-neutral-500">
          {colorNo}/{blocks.length}
        </span>
      </div>

      <span className="w-28 text-right text-[11px] tabular-nums text-neutral-500">
        {shown.toLocaleString()} / {total.toLocaleString()}
      </span>
      {/* speed: step down / up (clamped) */}
      <div className="flex items-center gap-0.5">
        <button
          className="icon-btn"
          title="ช้าลง"
          onClick={slower}
          disabled={speedIdx <= 0}
        >
          <Minus size={14} />
        </button>
        <span className="w-12 text-center text-[11px] tabular-nums text-neutral-500">
          {simSpeed}×
        </span>
        <button
          className="icon-btn"
          title="เร็วขึ้น"
          onClick={faster}
          disabled={speedIdx >= SPEEDS.length - 1}
        >
          <Plus size={14} />
        </button>
      </div>
    </div>
  );
}
