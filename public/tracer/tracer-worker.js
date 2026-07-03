// Auto Punch tracer worker. Classic worker (not a module) so it can
// importScripts the wasm-pack `no-modules` glue, same public/-asset pattern
// as the engine wasm. Messages:
//   in : { id, rgba: ArrayBuffer, width, height, opts }
//   out: { id, result } | { id, error }
importScripts("/tracer/pes_tracer.js"); // sets global `wasm_bindgen`

const ready = wasm_bindgen({ module_or_path: "/tracer/pes_tracer_bg.wasm" });

onmessage = async (e) => {
  const { id, rgba, width, height, opts } = e.data;
  try {
    await ready;
    const json = wasm_bindgen.trace(
      new Uint8Array(rgba),
      width,
      height,
      JSON.stringify(opts || {})
    );
    const result = JSON.parse(json);
    if (result.error) postMessage({ id, error: result.error });
    else postMessage({ id, result });
  } catch (err) {
    postMessage({ id, error: String(err && err.message ? err.message : err) });
  }
};
