// Single invoke() seam: route engine commands to Tauri (desktop) or the wasm
// module (browser). EngineClient imports invoke() from here, so every command
// works in both targets with no per-call branching.
import { invoke as tauriInvoke } from "@tauri-apps/api/core";
import { webInvoke } from "./webEngine";

/** True when running inside the Tauri desktop shell (vs a plain browser). */
export const IS_TAURI =
  typeof window !== "undefined" && "__TAURI_INTERNALS__" in window;

export function invoke<T = unknown>(
  cmd: string,
  args?: Record<string, unknown>,
): Promise<T> {
  return IS_TAURI ? tauriInvoke<T>(cmd, args) : webInvoke<T>(cmd, args);
}
