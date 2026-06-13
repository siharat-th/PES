import { useEffect, useRef, useState } from "react";
import { Play, Pause, SkipBack, SkipForward, Rewind, FastForward } from "lucide-react";
import { useUiStore } from "../state/uiStore";
import { useDocumentStore } from "../state/documentStore";
import { getStitchData } from "../engine/EngineClient";

const SPEEDS = [10, 30, 80, 200];

/** Bottom bar shown in stitch view: play/pause + scrubber over stitch order. */
export default function SimulatorBar() {
  const imageVersion = useDocumentStore((s) => s.imageVersion);
  const simIndex = useUiStore((s) => s.simIndex);
  const simPlaying = useUiStore((s) => s.simPlaying);
  const simSpeed = useUiStore((s) => s.simSpeed);
  const setSimIndex = useUiStore((s) => s.setSimIndex);
  const setSimPlaying = useUiStore((s) => s.setSimPlaying);
  const setSimSpeed = useUiStore((s) => s.setSimSpeed);

  const [total, setTotal] = useState(0);
  const raf = useRef<number | null>(null);

  useEffect(() => {
    let cancelled = false;
    void getStitchData(-1).then((d) => {
      if (!cancelled) setTotal(d.totalPoints);
    });
    return () => {
      cancelled = true;
    };
  }, [imageVersion]);

  // playback loop
  useEffect(() => {
    if (!simPlaying || total === 0) return;
    let last = performance.now();
    const tick = (now: number) => {
      const dt = (now - last) / 16.67; // frames at 60fps
      last = now;
      const cur = useUiStore.getState().simIndex;
      const from = cur < 0 ? 0 : cur;
      const next = from + simSpeed * dt;
      if (next >= total) {
        setSimIndex(total);
        setSimPlaying(false);
        return;
      }
      setSimIndex(Math.floor(next));
      raf.current = requestAnimationFrame(tick);
    };
    raf.current = requestAnimationFrame(tick);
    return () => {
      if (raf.current) cancelAnimationFrame(raf.current);
    };
  }, [simPlaying, simSpeed, total, setSimIndex, setSimPlaying]);

  const shown = simIndex < 0 ? total : simIndex;

  const play = () => {
    if (simIndex < 0 || simIndex >= total) setSimIndex(0);
    setSimPlaying(true);
  };

  return (
    <div className="flex items-center gap-3 border-t border-neutral-200 bg-white px-3 py-1.5">
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
      <button
        className="icon-btn"
        title="ช้าลง"
        onClick={() =>
          setSimSpeed(SPEEDS[Math.max(0, SPEEDS.indexOf(simSpeed) - 1)] ?? 10)
        }
      >
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
      <button
        className="icon-btn"
        title="เร็วขึ้น"
        onClick={() =>
          setSimSpeed(
            SPEEDS[Math.min(SPEEDS.length - 1, SPEEDS.indexOf(simSpeed) + 1)] ??
              200,
          )
        }
      >
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

      <input
        type="range"
        min={0}
        max={total}
        value={shown}
        className="min-w-0 flex-1 accent-blue-600"
        onChange={(e) => {
          setSimPlaying(false);
          setSimIndex(Number(e.target.value));
        }}
      />
      <span className="w-28 text-right text-[11px] tabular-nums text-neutral-500">
        {shown.toLocaleString()} / {total.toLocaleString()}
      </span>
      <span className="w-10 text-right text-[11px] text-neutral-400">
        {simSpeed}×
      </span>
    </div>
  );
}
