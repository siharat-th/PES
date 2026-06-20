import ReactDOM from "react-dom/client";
import App from "./App";

// NOTE: no React.StrictMode — its dev-only double-invoke leaves orphaned
// duplicate Konva nodes (react-konva drives the canvas imperatively), which
// surfaced as doubled control-arm lines in PathEdit.
ReactDOM.createRoot(document.getElementById("root") as HTMLElement).render(
  <App />,
);
