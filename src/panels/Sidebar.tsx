import { useState } from "react";
import LayerPanel from "./LayerPanel";
import PropertiesPanel from "./PropertiesPanel";

const TABS = [
  { id: "properties", label: "Properties" },
  { id: "layers", label: "Layers" },
] as const;

type TabId = (typeof TABS)[number]["id"];

export default function Sidebar() {
  const [tab, setTab] = useState<TabId>("layers");

  return (
    <div className="flex h-full w-72 flex-col border-l border-neutral-200 bg-white">
      <div className="flex border-b border-neutral-200">
        {TABS.map((t) => (
          <button
            key={t.id}
            className={`flex-1 px-3 py-2 text-xs font-medium ${
              tab === t.id
                ? "border-b-2 border-blue-500 text-blue-600"
                : "text-neutral-400 hover:text-neutral-600"
            }`}
            onClick={() => setTab(t.id)}
          >
            {t.label}
          </button>
        ))}
      </div>
      <div className="min-h-0 flex-1 overflow-hidden">
        {tab === "layers" ? <LayerPanel /> : <PropertiesPanel />}
      </div>
    </div>
  );
}
