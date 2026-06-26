/** Shared inline icons that lucide-react doesn't provide. */

/** Sewing/embroidery machine (จักรปัก) — ported from the old app's Stitch
 *  Simulator toolbar button so Stitch View reads as the machine preview. */
export function SewingMachineIcon({ size = 16 }: { size?: number }) {
  return (
    <svg
      width={size}
      height={size}
      viewBox="0 0 32 32"
      fill="currentColor"
      aria-hidden="true"
    >
      <path d="M31,22v4c0,1.1-0.9,2-2,2H3c-1.1,0-2-0.9-2-2v-4c0-1.1,0.9-2,2-2h26C30.1,20,31,20.9,31,22z M30,5c-0.55,0-1,0.45-1,1v6c0,0.55,0.45,1,1,1s1-0.45,1-1V6C31,5.45,30.55,5,30,5z M3,9V5c0-1.1,0.9-2,2-2h21c1.1,0,2,0.9,2,2v14H18v-4c0-1.1-0.892-2.119-1.982-2.264l-5.024-0.67C10.958,13.135,10.077,14,9,14H8v2.182c0,0.276-0.224,0.5-0.5,0.5S7,16.458,7,16.182V14H6c-1.1,0-2-0.9-2-2v-1.141C3.406,10.433,3,9.738,3,9z M21,9c0,1.105,0.895,2,2,2s2-0.895,2-2c0-1.105-0.895-2-2-2S21,7.895,21,9z M21,15c0,1.105,0.895,2,2,2s2-0.895,2-2c0-1.105-0.895-2-2-2S21,13.895,21,15z M23,10c0.552,0,1-0.448,1-1c0-0.552-0.448-1-1-1s-1,0.448-1,1C22,9.552,22.448,10,23,10z M23,16c0.552,0,1-0.448,1-1c0-0.552-0.448-1-1-1s-1,0.448-1,1C22,15.552,22.448,16,23,16z" />
    </svg>
  );
}
