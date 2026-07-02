// Vendored from Victor-frontend/PES5 cordova/www/api-satin-helper.js (lines 1-7733):
// the Smart Satin geometry core (multipolygon -> straight skeleton -> centerline
// -> satin column rails). The CanvasKit boundary functions that followed
// (apiWorkerAddSatinColumnToLayer / apiWorkerConvertLayerToSatinColumn) are
// re-implemented over the PES engine seams in src/satin/smartSatin.ts —
// DO NOT EDIT the core below; it must stay byte-identical to production.
/**
 * API Satin Helper - Extracted Smart Satin Column Functions
 *
 * This file contains satin column functions extracted from makesatincolumn.js
 * for use in headless browser API worker context.
 *
 * Main functions exported (with apiWorker prefix to avoid conflicts):
 * - apiWorkerMakeMultipolygon: Converts polygons to hierarchical multipolygon structure
 * - apiWorkerGetSatinColumnDS: Gets satin column path data from centerline
 * - apiWorkerGetCenterline: Computes centerline/skeleton from polygon data
 * - apiWorkerAddSatinColumnToLayer: Adds satin column to PES document layer
 * - apiWorkerConvertLayerToSatinColumn: Converts a PES layer to smart satin column
 *
 * Guard clauses that check for UI elements have been removed for API worker compatibility.
 */

// ============================================================================
// MATH CONSTANTS
// ============================================================================
const D2R = Math.PI/180;
const deg2rad = (d) => d*D2R;
const R2D = 180/Math.PI;
const rad2deg = (r) => r*R2D;

// Angle constants
const M_PI12 = Math.PI/12;    // 15deg
const M_PI6 = Math.PI/6;      // 30deg
const M_PI4 = Math.PI/4;      // 45deg
const M_PI3 = Math.PI/3;      // 60deg
const M_PI2 = Math.PI/2;      // 90deg
const M_PI  = Math.PI;        // 180deg
const M_1PI = Math.PI;        // 180deg
const M_2PI = 2*Math.PI;      // 360deg
const M_3PI = 3*Math.PI;      // 540deg
const M_4PI = 4*Math.PI;      // 720deg

const c180D_IN_R = M_1PI;
const c90D_IN_R =  M_PI2;

// ============================================================================
// D3 LINE GENERATORS & INTERPOLATION
// ============================================================================
const DEFAULT_RIDGE_PATH_DIGITS = 3;
const DEFAULT_RIDGE_CURVE = 0.5;

const fnInterpolateNumberArray = (a, b) => {
  if (!b) b = [];
  const n = a ? Math.min(b.length, a.length) : 0;
  return (t) => {
    const c = b.slice();
    for (let i = 0; i < n; ++i)
      c[i] = a[i] * (1 - t) + b[i] * t;
    return c;
  }
}

if(d3 && d3.interpolateNumberArray != fnInterpolateNumberArray) {
  d3.interpolateNumberArray = fnInterpolateNumberArray;
}

let d3linecurveCatmullRom = d3.line()
  .digits(DEFAULT_RIDGE_PATH_DIGITS)
  .curve(d3.curveCatmullRom.alpha(DEFAULT_RIDGE_CURVE))
;

let d3linecurveCardinal = d3.line()
  .digits(DEFAULT_RIDGE_PATH_DIGITS)
  .curve(d3.curveCardinal.tension(DEFAULT_RIDGE_CURVE))
;

let d3line = d3.line()
  .digits(DEFAULT_RIDGE_PATH_DIGITS)
;

// ============================================================================
// RIDGE SIMPLIFY CONSTANTS
// ============================================================================
const SCALE_XY = 1.0;
let DEFAULT_RIDGE_SIMPLIFY = -1.0;
let DEFAULT_RIDGE_SIMPLIFY_Z_FACTOR = 1.0;
let DEFAULT_RIDGE_SIMPLIFY_AUTO_FACTOR = 1.0;
let DEFAULT_RIDGE_SIMPLIFY_AUTO_FACTOR_FINAL = 1/3;
let DEFAULT_MARGIN_DISTANCE_FACTOR = 1/3;
let DEFAULT_RIDGE_SIMPLIFY_AUTO_MAX = 256;
let DEFAULT_RIDGE_ERROR_MIN = 1.05;

// ============================================================================
// WORKER AND SKELETON BUILDER SETUP
// ============================================================================
let USE_WORKER = true;
let USE_MULTI_WORKER = false;
let skeletonBuilderLoaded = false;
let globalskeletonworker;

const fnSkeletonBuilderInit = async () => {
  if(!skeletonBuilderLoaded) {
    await SkeletonBuilder.init()
    .then(() => skeletonBuilderLoaded = true)
    .catch((err) => console.debug('SkeletonBuilderInit error:', err));
  }
}

const buildSkeletonFromWorker = (worker, p) => {
  if(worker) {
    const promise = new Promise((resolve, reject) => {
        worker.onmessage = (e) => {
          resolve(e.data);
        };
    });
    worker.postMessage(p);
    return promise;
  }
  return Promise.resolve(null);
};

// ============================================================================
// UTILITY FUNCTIONS
// ============================================================================
const delay = (ms) => new Promise(res => setTimeout(res, ms));

const maketimelog = () => {
  const timelog = {
    startDate: null,
    prevDate: null,
    currDate: null,
    beginargs: null,
    diffsecs: 0,
    reset: function (...args) {
      this.startDate = this.prevDate = this.currDate = new Date();
    },
    begin: function (...args) {
      this.beginargs = args;
      this.prevDate = this.currDate = new Date();
      console.log('[', ((this.currDate.getTime() - this.startDate.getTime()) / 1000).toFixed(3), ']', 'begin:', ...args);
    },
    end: function (...args) {
      this.currDate = new Date();
      this.diffsecs = ((this.currDate.getTime() - this.prevDate.getTime()) / 1000).toFixed(3);
      this.prevDate = this.currDate;
      console.log('[', ((this.currDate.getTime() - this.startDate.getTime()) / 1000).toFixed(3), ']', 'end: ', this.beginargs[0] || '', this.diffsecs, 'sec', ...args);
    },
  };

  timelog.reset();

  return timelog;
}

const diffValue = (a, b) => (b - a) / a;

// ============================================================================
// COLOR CONVERSION UTILITIES
// ============================================================================
const u32ARGBtoRGBA = (argb) => (a = argb << 0 >>> 24, r = argb << 8 >>> 24, g = argb << 16 >>> 24, b = argb << 24 >>> 24, [r,g,b,a]);

function hexToRgb(hex) {
  let result = /^#?([a-f\d]{2})([a-f\d]{2})([a-f\d]{2})$/i.exec(hex);
  return result ? [
    parseInt(result[1], 16),
    parseInt(result[2], 16),
    parseInt(result[3], 16)
  ] : [0, 0, 0];
}

// ============================================================================
// PES COLOR LIST
// ============================================================================
let colorList = [
  [[168, 168, 168], "Default",         "005"], /* Index  0 */
  [[ 14,  31, 124], "Prussian Blue",   "007"], /* Index  1 */
  [[ 10,  85, 163], "Blue",            "405"], /* Index  2 */
  [[ 48, 135, 119], "Teal Green",      "534"], /* Index  3 */
  [[ 75, 107, 175], "Cornflower Blue", "070"], /* Index  4 */
  [[237,  23,  31], "Red",             "800"], /* Index  5 */
  [[209,  92,   0], "Reddish Brown",   "337"], /* Index  6 */
  [[145,  54, 151], "Magenta",         "620"], /* Index  7 */
  [[228, 154, 203], "Light Lilac",     "810"], /* Index  8 */
  [[145,  95, 172], "Lilac",           "612"], /* Index  9 */
  [[157, 214, 125], "Mint Green",      "515"], /* Index 10 */
  [[232, 169,   0], "Deep Gold",       "214"], /* Index 11 */
  [[254, 186,  53], "Orange",          "208"], /* Index 12 */
  [[255, 255,   0], "Yellow",          "205"], /* Index 13 */
  [[112, 188,  31], "Lime Green",      "513"], /* Index 14 */
  [[186, 152,   0], "Brass",           "328"], /* Index 15 */
  [[168, 168, 168], "Silver",          "005"], /* Index 16 */
  [[123, 111,   0], "Russet Brown",    "337"], /* Index 17 */
  [[255, 255, 179], "Cream Brown",     "010"], /* Index 18 */
  [[ 95, 101, 121], "Pewter",          "704"], /* Index 19 */
  [[  0,   0,   0], "Black",           "900"], /* Index 20 */
  [[ 11,  61, 145], "Ultramarine",     "406"], /* Index 21 */
  [[119,   1, 118], "Royal Purple",    "869"], /* Index 22 */
  [[ 41,  49,  51], "Dark Gray",       "707"], /* Index 23 */
  [[ 42,  19,   1], "Dark Brown",      "058"], /* Index 24 */
  [[246,  74, 138], "Deep Rose",       "086"], /* Index 25 */
  [[178, 118,  36], "Light Brown",     "323"], /* Index 26 */
  [[252, 187, 196], "Salmon Pink",     "079"], /* Index 27 */
  [[254,  55,  15], "Vermillion",      "030"], /* Index 28 */
  [[240, 240, 240], "White",           "001"], /* Index 29 */
  [[106,  28, 138], "Violet",          "613"], /* Index 30 */
  [[168, 221, 196], "Seacrest",        "542"], /* Index 31 */
  [[ 37, 132, 187], "Sky Blue",        "019"], /* Index 32 */
  [[254, 179,  67], "Pumpkin",         "126"], /* Index 33 */
  [[240, 231, 101], "Cream Yellow",    "812"], /* Index 34 */
  [[208, 166,  96], "Khaki",           "348"], /* Index 35 */
  [[209,  84,   0], "Clay Brown",      "339"], /* Index 36 */
  [[102, 186,  73], "Leaf Green",      "509"], /* Index 37 */
  [[ 19,  74,  70], "Peacock Blue",    "415"], /* Index 38 */
  [[110, 123, 119], "Gray",            "817"], /* Index 39 */
  [[216, 202, 198], "Warm Gray",       "399"], /* Index 40 */
  [[ 67,  86,   7], "Dark Olive",      "517"], /* Index 41 */
  [[240, 225, 198], "Linen",           "307"], /* Index 42 */
  [[249, 147, 188], "Pink",            "085"], /* Index 43 */
  [[  0,  56,  34], "Deep Green",      "808"], /* Index 44 */
  [[178, 175, 212], "Lavender",        "804"], /* Index 45 */
  [[104, 106, 176], "Wisteria Violet", "607"], /* Index 46 */
  [[239, 227, 185], "Beige",           "843"], /* Index 47 */
  [[247,  56, 102], "Carmine",         "807"], /* Index 48 */
  [[181,  76, 100], "Amber Red",       "333"], /* Index 49 */
  [[ 19,  43,  26], "Olive Green",     "519"], /* Index 50 */
  [[199,   1,  85], "Dark Fuschia",    "107"], /* Index 51 */
  [[254, 158,  50], "Tangerine",       "209"], /* Index 52 */
  [[168, 222, 235], "Light Blue",      "017"], /* Index 53 */
  [[  0, 103,  26], "Emerald Green",   "507"], /* Index 54 */
  [[ 78,  41, 144], "Purple",          "614"], /* Index 55 */
  [[ 47, 126,  32], "Moss Green",      "515"], /* Index 56 */
  [[254, 227, 197], "Flesh Pink",      "124"], /* Index 57 */
  [[255, 217,  17], "Harvest Gold",    "206"], /* Index 58 */
  [[  9,  91, 166], "Electric Blue",   "420"], /* Index 59 */
  [[240, 249, 112], "Lemon Yellow",    "202"], /* Index 60 */
  [[227, 243,  91], "Fresh Green",     "027"], /* Index 61 */
  [[160, 160, 160], "Applique material","x9"], /* Index 62 */
  [[160, 160, 160], "Applique position","x8"], /* Index 63 */
  [[160, 160, 160], "Applique",         "x7"], /* Index 64 */
  [[  0,   0,   0], "Original color",  "---"]  /* Index 65 */
];

function findNearestBrotherColorIndex(red, green, blue){
  let currentClosestValue = Number.MAX_SAFE_INTEGER;
  let closestIndex = -1;
  let deltaRed, deltaGreen, deltaBlue;
  let dist;
  for(let i = 0, ii = colorList.length - 1; i < ii; i++) {
    let c = colorList[i][0]
    deltaRed = red - c[0];
    deltaBlue = green - c[1];
    deltaGreen = blue - c[2];
    dist = Math.hypot(deltaRed, deltaBlue, deltaGreen);
    if(dist <= currentClosestValue) {
        currentClosestValue = dist;
        closestIndex = i;
    }
  }
  return closestIndex;
}

// ============================================================================
// GEOMETRIC HELPER FUNCTIONS
// ============================================================================
function getTangentAngleRad(p0, p1) {
  let angle = Math.atan2(p1[1] - p0[1], p1[0] - p0[0]);
  return angle;
}

function getTangentAngleDeg(p0, p1) {
  return getTangentAngleRad(p0, p1) * R2D;
}

function getAngleRad(p0, p1, p2) {
  let res = getTangentAngleRad(p1, p0) - getTangentAngleRad(p1, p2);
  if(res > M_1PI) {
    res -= M_2PI;
  }
  else if(res < -M_1PI) {
    res += M_2PI;
  }
  return res;
}

function getAngleDeg(p0, p1, p2) {
  return getAngleRad(p0, p1, p2) * R2D;
}

function getTurnAngleRad(p0, p1, p2) {
  let res = getTangentAngleRad(p1, p2) - getTangentAngleRad(p0, p1);
  if(res > M_1PI) {
    res -= M_2PI;
  }
  else if(res < -M_1PI) {
    res += M_2PI;
  }
  return res;
}

function getTurnAngleDeg(p0, p1, p2) {
  return getTurnAngleRad(p0, p1, p2) * R2D;
}

function getAbsAngleRad(p0, p1, p2) {
  return Math.abs(getAngleRad(p0, p1, p2));
}

function getAbsAngleDeg(p0, p1, p2) {
  return getAbsAngleRad(p0, p1, p2) * R2D;
}

function meanAngleRad(angleRads) {
  let y_part = 0, x_part = 0;
  let len = angleRads.length;
  for(let i = 0; i < len; i++) {
    x_part += Math.cos(angleRads[i]);
    y_part += Math.sin(angleRads[i]);
  }
  return Math.atan2 (y_part / len, x_part / len);
}

const diffAngleRad = (a, b) => Math.abs((Math.abs(a-b) + M_1PI) % M_2PI - M_1PI);
const diffAngleDeg = (a, b) => Math.abs((Math.abs(a-b) + 180) % 360 - 180);

const diffValueMean = (a, b) => Math.abs(a - b) / ((a + b) / 2);

let getdirectionDeg = (d) => {
  d %= 360;
  if(d < -180) return d + 360;
  if(d >  180) return d - 360;
  return d;
}

// ============================================================================
// DISTANCE UTILITIES
// ============================================================================
function getDist(p1, p2) {
  var dx = p1[0] - p2[0], dy = p1[1] - p2[1];
  return Math.hypot(dx, dy);
}

function getSqDist(p1, p2) {
  var dx = p1[0] - p2[0], dy = p1[1] - p2[1];
  return dx * dx + dy * dy;
}

function isCirInts(c1, c2) {
  var dx = c1[0] - c2[0]
    , dy = c1[1] - c2[1];

  return Math.hypot(dx, dy) <= (c1[2] + c2[2]);
}

// ============================================================================
// LINE INTERSECTION UTILITIES
// ============================================================================
function lineIntersect(x0, y0, x1, y1, x2, y2, x3, y3) {
  let dx0 = x1 - x0;
  let dy0 = y1 - y0;

  let dx1 = x3 - x2;
  let dy1 = y3 - y2;

  denominator = dy1 * dx0 - dx1 * dy0;

  if (denominator === 0) {
    return false;
  }

  let ua = (dx1 * (y0 - y2) - dy1 * (x0 - x2)) / denominator;
  let ub = (dx0 * (y0 - y2) - dy0 * (x0 - x2)) / denominator;

  if (ua < 0 || ua > 1 || ub < 0 || ub > 1) {
    return false;
  }

  let x = x0 + ua * dx0;
  let y = y0 + ua * dy0;

  return [x, y];
}

function getPointLineIntersectPolygon(line, polygon) {
  let x0, y0, x1, y1;
  [x0, y0] = line[0];
  [x1, y1] = line[1];
  let dx0 = x1 - x0;
  let dy0 = y1 - y0;

  let intersects = [];
  let x2, y2, x3, y3;
  for(let i = 0, j = 1, jj = polygon.length; j < jj; i++, j++) {
    [x2, y2] = polygon[i];
    [x3, y3] = polygon[j];

    let dx1 = x3 - x2;
    let dy1 = y3 - y2;

    denominator = dy1 * dx0 - dx1 * dy0;

    if (denominator === 0) {
      continue;
    }

    let ua = (dx1 * (y0 - y2) - dy1 * (x0 - x2)) / denominator;
    let ub = (dx0 * (y0 - y2) - dy0 * (x0 - x2)) / denominator;

    if (ua < 0 || ua > 1 || ub < 0 || ub > 1) {
      continue;
    }

    let x = x0 + ua * dx0;
    let y = y0 + ua * dy0;

    intersects.push([polygon[i], [x, y], polygon[j]]);
  }

  if(intersects.length > 1) {
    let minlength = Number.MAX_VALUE;
    let intersect = intersects[0];
    for(const inter of intersects) {
      const [x1, y1]  = inter[1];
      const dx = x0 - x1;
      const dy = y0 - y1;
      const length = Math.hypot(dx, dy);
      if(minlength > length) {
        minlength = length;
        intersect = inter;
      }
    }
    return intersect;
  }
  else if(intersects.length > 0) {
    return intersects[0];
  }
  else {
    return null;
  }
}

function getTerminalLineIntersectPolygon(line, polygon) {
  let x0, y0, x1, y1;
  [x0, y0] = line[0];
  [x1, y1] = line[1];
  let dx0 = x1 - x0;
  let dy0 = y1 - y0;

  let intersects = [];
  let x2, y2, x3, y3;
  for(let i = 0, j = 1, jj = polygon.length; j < jj; i++, j++) {
    [x2, y2] = polygon[i];
    [x3, y3] = polygon[j];

    let dx1 = x3 - x2;
    let dy1 = y3 - y2;

    denominator = dy1 * dx0 - dx1 * dy0;

    // lines are parallel
    if (denominator === 0) {
      continue;
    }

    let ua = (dx1 * (y0 - y2) - dy1 * (x0 - x2)) / denominator;
    let ub = (dx0 * (y0 - y2) - dy0 * (x0 - x2)) / denominator;

    // is the intersection along the segments
    if (ua < 0 || ua > 1 || ub < 0 || ub > 1) {
      continue;
    }

    // Return a object with the x and y coordinates of the intersection
    let x = x0 + ua * dx0;
    let y = y0 + ua * dy0;

    intersects.push([polygon[i], [x, y], polygon[j]]);
  }

  if(intersects.length > 1) {
    let minlength = Number.MAX_VALUE;
    let intersect = intersects[0];
    for(const inter of intersects) {
      const [x1, y1]  = inter[1];
      const dx = x0 - x1;
      const dy = y0 - y1;
      const length = Math.hypot(dx, dy);
      if(minlength > length) {
        minlength = length;
        intersect = inter;
      }
    }
    const p0 = intersect[0];
    const p1 = intersect[2];
    const midx = (p0[0] + p1[0]) / 2;
    const midy = (p0[1] + p1[1]) / 2;
    const dx = p0[0] - p1[0];
    const dy = p0[1] - p1[1];
    const radius = Math.hypot(dx, dy) / 2;
    const tangent = getTangentAngleRad(p0, p1) - M_PI2;
    const p = [midx, midy, radius, 1, 8, tangent];
    return p;
  }
  else if(intersects.length > 0) {
    const intersect = intersects[0];
    const p0 = intersect[0];
    const p1 = intersect[2];
    const midx = (p0[0] + p1[0]) / 2;
    const midy = (p0[1] + p1[1]) / 2;
    const dx = p0[0] - p1[0];
    const dy = p0[1] - p1[1];
    const radius = Math.hypot(dx, dy) / 2;
    const tangent = getTangentAngleRad(p0, p1) - M_PI2;
    const p = [midx, midy, radius, 1, 8, tangent];
    return p;
  }
  else {
    return null;
  }
}

function getInfosFromHit(pi, factor, thetas, polygons, centerlines) {
  const [xi, yi, zi] = pi;
  const linelength = zi * factor;
  let infos = [];
  if(thetas) {
    if(!Array.isArray(thetas)) {
      thetas = +(thetas);
      if(Number.isFinite(thetas)) {
        thetas = [thetas];
      }
    }
  }
  else {
    thetas = [];
  }

  const r75 = zi * 0.75;

  for(const theta of thetas) {
    let pn = [linelength * Math.cos(theta) + xi, linelength * Math.sin(theta) + yi];
    let line = [pi, pn];
    let intersect;
    let intersects = [];
    for(const polygon of polygons.toReversed()) {
      intersect = getPointLineIntersectPolygon(line, polygon);
      if(intersect) {
        intersects.push(intersect);
      }
    }
    for(const intersect of intersects) {
      intersect[3] = getDist(pi, intersect[1]);
    }
    let minIdx = d3.minIndex(intersects, intersect => intersect[3]);
    if(minIdx > -1) {
      let isHitPolygon = true;
      intersect = intersects[minIdx];
      intersect[4] = theta;
      intersect[5] = isHitPolygon;
      infos.push(intersect);

      let [xhit, yhit] = intersect[1];
      let dist = intersect[3];
      if(centerlines && centerlines.length && dist > r75) {
        intersects = [];
        let p75 = [r75 * Math.cos(theta) + xi, r75 * Math.sin(theta) + yi];
        line = [p75, [xhit, yhit]];
        for(const centerline of centerlines) {
          intersect = getPointLineIntersectPolygon(line, centerline);
          if(intersect) {
            intersects.push(intersect);
          }
        }
        for(const intersect of intersects) {
          intersect[3] = getDist(p75, intersect[1]);
        }
        minIdx = d3.minIndex(intersects, intersect => intersect[3]);
        if(minIdx > -1) {
          infos.pop();
          isHitPolygon = false;
          intersect = intersects[minIdx];
          intersect[4] = theta;
          intersect[5] = isHitPolygon;
          infos.push(intersect);
        }
      }
    }
    else {
      infos.push(null);
    }
  }
  return infos;
}

// ============================================================================
// SIMPLIFICATION UTILITIES
// ============================================================================
function simplifyDPStep(points, first, last, sqTolerance, simplified, offset) {
  var maxSqDist = sqTolerance, sqDist, index;

  if(offset == undefined) {
    offset = 0;
  }

  const xoffset = 0 + offset;
  const yoffset = 1 + offset;

  var x0 = points[first][xoffset]
    , y0 = points[first][yoffset]
    , x1 = points[last][xoffset]
    , y1 = points[last][yoffset]
    , dx = x1 - x0
    , dy = y1 - y0
    , x, y, t;

  if (dx !== 0 || dy !== 0) {
    for (var i = first + 1; i < last; i++) {
      x = points[i][xoffset];
      y = points[i][yoffset];
      t = ((x - x0) * dx + (y - y0) * dy) / (dx * dx + dy * dy);

      if (t > 1) {
        sqDist = (x-x1)**2 + (y-y1)**2;
      }
      else if (t > 0) {
        sqDist = (x-(x0 + dx*t))**2 + (y-(y0 + dy*t))**2;
      }
      else {
        sqDist = (x-x0)**2 + (y-y0)**2;
      }

      if (sqDist > maxSqDist) {
        index = i;
        maxSqDist = sqDist;
      }
    }
  }
  else {
    for (var i = first + 1; i < last; i++) {
      sqDist = (points[i][xoffset]-x0)**2 + (points[i][yoffset]-y0)**2;
      if (sqDist > maxSqDist) {
        index = i;
        maxSqDist = sqDist;
      }
    }
  }

  if (maxSqDist > sqTolerance) {
    if (index - first > 1)
      simplifyDPStep(points, first, index, sqTolerance, simplified, offset);
    simplified.push(points[index]);
    if (last - index > 1)
      simplifyDPStep(points, index, last, sqTolerance, simplified, offset);
  }
}

function simplifyDouglasPeucker(points, sqTolerance, offset) {
  var last = points.length - 1;
  var simplified = [points[0]];
  simplifyDPStep(points, 0, last, sqTolerance, simplified, offset);
  simplified.push(points[last]);
  return simplified;
}

// ============================================================================
// POLYGON UTILITIES
// ============================================================================
function ringarea(ring) {
  var i = 0, n = ring.length, area = ring[n - 1][1] * ring[0][0] - ring[n - 1][0] * ring[0][1];
  while (++i < n) area += ring[i - 1][1] * ring[i][0] - ring[i - 1][0] * ring[i][1];
  return area;
}

function iscw(ring) {
  return ring && ring.length > 2 && ringarea(ring) > 0;
}

function polygonCentroid(polygon) {
  var i = -1,
      n = polygon.length,
      x = 0,
      y = 0,
      a,
      b = polygon[n - 1],
      c,
      k = 0;

  while (++i < n) {
    a = b;
    b = polygon[i];
    k += c = a[0] * b[1] - b[0] * a[1];
    x += (a[0] + b[0]) * c;
    y += (a[1] + b[1]) * c;
  }

  return k *= 3, [x / k, y / k];
}

function ringContainsPoint(ring, point) {
  var x = point[0], y = point[1], contains = -1;
  for (var i = 0, n = ring.length, j = n - 1; i < n; j = i++) {
    var pi = ring[i], xi = pi[0], yi = pi[1], pj = ring[j], xj = pj[0], yj = pj[1];
    if (segmentContains(pi, pj, point)) return 0;
    if (((yi > y) !== (yj > y)) && ((x < (xj - xi) * (y - yi) / (yj - yi) + xi))) contains = -contains;
  }
  return contains;
}

function segmentContains(a, b, c) {
  var i; return iscollinearpoints(a, b, c) && within(a[i = +(a[0] === b[0])], c[i], b[i]);
}

function iscollinearpoints(a, b, c) {
  return (b[0] - a[0]) * (c[1] - a[1]) === (c[0] - a[0]) * (b[1] - a[1]);
}

function within(p, q, r) {
  return p <= q && q <= r || r <= q && q <= p;
}

function ringContainsHole(ring, hole) {
  if(ring.length > 3) {
    let ps = ring[0];
    let pe = ring[ring.length-1];
    if(ps && pe) {
      if(ps == pe || (ps[0] == pe[0] && ps[1] == pe[1]) ) {
        ring = ring.slice(0, -1);
      }
    }
  }

  if(hole.length > 3) {
    let ps = hole[0];
    let pe = hole[hole.length-1];
    if(ps && pe) {
      if(ps == pe || (ps[0] == pe[0] && ps[1] == pe[1]) ) {
        hole = hole.slice(0, -1);
      }
    }
  }

  return hole.every(p => ringContainsPoint(ring, p) >= 0);
}

function lexicographicOrder(a, b) {
  return a[0] - b[0] || a[1] - b[1];
}

function fnAddMiterJoinToTriangle(poly) {
  if(poly.length == 4) {
    poly.pop();
    let p0 = poly[0];
    let p1 = poly[1];
    let p2 = poly[2];

    let ass = [[getAbsAngleRad(p2, p0, p1), [p1, p2]], [getAbsAngleRad(p0, p1, p2), [p2, p0]], [getAbsAngleRad(p1, p2, p0), [p0, p1]]];
    let minIdx = d3.minIndex(ass, as => as[0]);
    if(minIdx > -1) {
      const pn = poly[minIdx];
      const [a, [s0, s1]] = ass[minIdx];
      const midx = (s0[0] + s1[0]) / 2;
      const midy = (s0[1] + s1[1]) / 2;
      let theta = getTangentAngleRad([midx, midy], pn);
      const x0 = 0.1 * Math.cos(theta) + pn[0];
      const y0 = 0.1 * Math.sin(theta) + pn[1];
      theta += M_PI;
      const x1 = 0.1 * Math.cos(theta) + pn[0];
      const y1 = 0.1 * Math.sin(theta) + pn[1];
      poly.splice(minIdx, 1, [x0,y0], [x1,y1]);
      p0 = poly[0];
      poly.push([p0[0], p0[1]]);
    }
  }
}

// ============================================================================
// MAIN FUNCTION: apiWorkerMakeMultipolygon
// ============================================================================
function apiWorkerMakeMultipolygon(polygons, value, istext, invert) {
  if(value == undefined) {
    value = 0;
  }

  istext = !!istext;
  invert = !!invert;

  const makedata = (polygon) => {
    if(polygon.length < 3) {
      return null;
    }

    polygon = polygon.slice();
    let area = ringarea(polygon);

    if(Math.abs(area) < 5) {
      return null;
    }

    if(area < 0) {
      area = -area;
      polygon.reverse();
    }

    return {
      polygon: polygon,
      area: area,
      centroid: polygonCentroid(polygon),
      children: [],
      type: "",
    };
  }

  const hierarchyorder = (data) => {
    let datas = data.children;

    for(let i = 0, ii = datas.length; i < ii; i++) {
      let outer = datas[i];
      if(!outer) {
        continue;
      }

      for(let j = i+1; j < ii; j++) {
        let inner = datas[j];
        if(!inner) {
          continue;
        }

        if (ringContainsHole(outer.polygon, inner.polygon)) {
          outer.children.push(inner);
          datas[j] = null;
        }
      }

      if(outer.children.length) {
        hierarchyorder(outer);
      }
    }

    data.children = datas.filter(v => v);
  }

  let datas = polygons.map(polygon => makedata(polygon)).filter(v => v);
  datas.sort((a, b) => b.area - a.area);

  const data = {
    children: datas,
  };

  hierarchyorder(data);

  let coordinates = [];
  const pushcoordinates = (data) => {
    const datas = data.children;
    datas.sort((a, b) => lexicographicOrder(a.centroid, b.centroid));
    for(const data of datas) {
      let polygons;
      if(!data.type) {
        data.type = "outer";
        let outer = data;
        let inners = data.children.map(data => {
          if(!data.type) {
            data.type = "inner";
            return data;
          }
        }).filter(v => v);
        polygons = [outer, ...inners];
      }
      pushcoordinates(data);
      if(polygons) {
        coordinates.push(polygons);
      }
    }
  }

  pushcoordinates(data);

  const route = (coordinates) => {
    let wx = 1;
    let wy = 1;

    if(istext) {
      wy = 0.5;
    }

    let routecoordinates = [];
    let polygons = coordinates.shift();
    while(polygons) {
      let centroid = polygons[0].centroid;
      routecoordinates.push(polygons);
      polygons = null;
      if(coordinates.length) {
        let distances = coordinates.map(polygons => (((centroid[0] - polygons[0].centroid[0])*wx)**2) + (((centroid[1] - polygons[0].centroid[1])*wy)**2));
        let minidx = d3.minIndex(distances);
        if(minidx > -1) {
          polygons = coordinates[minidx];
          coordinates[minidx] = null;
          coordinates = coordinates.filter(v => v);
        }
      }
    }
    return routecoordinates;
  }

  coordinates = route(coordinates);

  for(const polygons of coordinates) {
    polygons[0].polygon.reverse();
  }

  if(invert) {
    for(const polygons of coordinates) {
      for(const data of polygons) {
        data.polygon.reverse();
      }
    }
  }

  return {
    type: "MultiPolygon",
    value: value,
    coordinates: coordinates.map(polygons => {
      if(polygons && polygons.length > 1) {
        const maxholearea = polygons[0].area * 0.99;
        polygons = polygons.filter((data,i) => i == 0 || data.area <= maxholearea);
      }
      return polygons.map(data => data.polygon);
    }),
  };
}

// ============================================================================
// MAIN FUNCTION: apiWorkerGetSatinColumnDS
// ============================================================================
const apiWorkerGetSatinColumnDS = (centerline, istext, json) => {
  istext = !!istext;
  json = !!json;

  const route_in_out = (idx_in_outs) => {
    let route_idx_in_outs = [];
    let idx_in_out = idx_in_outs.shift();
    while(idx_in_out) {
      let out = idx_in_out.out;
      route_idx_in_outs.push(idx_in_out);
      idx_in_out = null;
      if(idx_in_outs.length) {
        let distances = idx_in_outs.map(idx_in_out => [getSqDist(out, idx_in_out.in), getSqDist(out, idx_in_out.out)]).flat();
        let minidx = d3.minIndex(distances);
        if(minidx > -1) {
          minidx = minidx / 2 | 0;
          idx_in_out = idx_in_outs[minidx];
          idx_in_outs[minidx] = null;
          idx_in_outs = idx_in_outs.filter(v => v);
        }
      }
    }
    return route_idx_in_outs;
  }

  const route_centroid = (idx_centroids) => {
    let wx = 1;
    let wy = 1;

    let route_idx_centroids = [];
    let idx_centroid = idx_centroids.shift();
    while(idx_centroid) {
      let centroid = idx_centroid.centroid;
      route_idx_centroids.push(idx_centroid);
      idx_centroid = null;
      if(idx_centroids.length) {
        let distances = idx_centroids.map(idx_centroid => (((centroid[0] - (idx_centroid.centroid[0] - -1000000))*wx)**2) + (((centroid[1] - idx_centroid.centroid[1])*wy)**2));
        let minidx = d3.minIndex(distances);
        if(minidx > -1) {
          idx_centroid = idx_centroids[minidx];
          idx_centroids[minidx] = null;
          idx_centroids = idx_centroids.filter(v => v);
        }
      }
    }
    return route_idx_centroids;
  }

  if(!centerline) {
    return json? '[]' : [];
  }

  let routed = pessatincolumnds = centerline
    .normals.map(polys =>
      polys.map(polysegments =>
        polysegments.map(polysegment_3packs =>
          [d3linecurveCatmullRom(polysegment_3packs[0]), d3linecurveCatmullRom(polysegment_3packs[1])]).flat())).flat().filter(v => v);

  if(istext) {
    let idx_centroids = centerline
      .normals.map(polys =>
        polys.map(polysegments =>
          polysegments.map(polysegment_3packs =>
            [...polysegment_3packs[0], ...polysegment_3packs[1]]).flat())).flat().filter(v => v)
            .map((pl, idx) => (
              { idx, centroid: d3.polygonCentroid(d3.polygonHull(pl)) }
            ));

    idx_centroids = route_centroid(idx_centroids);
    routed = idx_centroids.map(idx_centroid => pessatincolumnds[idx_centroid.idx]);
  }
  else {
    let idx_in_outs = centerline
      .normals.map(polys =>
        polys.map(polysegments =>
          polysegments.map(polysegment_3packs =>
            polysegment_3packs[2]).flat())).flat().filter(v => v)
            .map((pl, idx) => (
              { idx, in: pl[0], out: pl[pl.length-1] }
            ));

    idx_in_outs = route_in_out(idx_in_outs);
    routed = idx_in_outs.map(idx_in_out => pessatincolumnds[idx_in_out.idx]);
  }

  return json? JSON.stringify(routed) : routed;
}

// ============================================================================
// MAIN FUNCTION: apiWorkerGetCenterline
// ============================================================================
const apiWorkerGetCenterline = async (polygondata, istext, option0) => {
  istext = !!istext;
  option0 = !!option0;

  const hitpolygonmax = (typeof window !== 'undefined' && window.hitpolygonmax !== undefined) ? window.hitpolygonmax : 1.633;
  const hitpolygon = (p0, normal, polygons, factor) => {
    if(factor == undefined || !isFinite(factor) || factor > hitpolygonmax) {
      factor = hitpolygonmax;
    }
    let pi = d3.interpolateNumberArray(p0.slice(0, 2), normal.slice(0, 2))(factor);
    let line = [p0, pi];
    let tangent = getTangentAngleRad(p0, pi);
    let intersect;
    let intersects = [];
    for(const polygon of polygons.toReversed()) {
      intersect = getPointLineIntersectPolygon(line, polygon);
      if(intersect) {
        let diff = diffAngleRad(tangent, getTangentAngleRad(intersect[0], intersect[1])) * R2D;
        if(diff >= 30 && diff <= 150) {
          intersects.push(intersect);
        }
        //else {
        //  console.log('diff:', diff);
        //}
      }
      //else {
      //  console.log('line:', line);
      //}
    }
    
    for(const intersect of intersects) {
      intersect[3] = getDist(p0, intersect[1]);
    }
    
    let minIdx = d3.minIndex(intersects, intersect => intersect[3]);
    if(minIdx > -1) {
      let [xnormal, ynormal] = intersects[minIdx][1];
      //let dist = intersects[minIdx][3];
      //let r75 = p0[2] * 0.75;
      //if(centerlines && centerlines.length && dist > r75) {
      //  let p01 = d3.interpolateNumberArray(p0.slice(0, 2), [xnormal, ynormal])(r75/dist);
      //  let line = [p01, [xnormal, ynormal]];
      //  intersects = [];
      //  for(const centerline of centerlines) {
      //    intersect = getPointLineIntersectPolygon(line, centerline);
      //    if(intersect) {
      //      intersects.push(intersect);
      //    }
      //  }
      //
      //  for(const intersect of intersects) {
      //    intersect[3] = getDist(p0, intersect[1]);
      //  }
      //
      //  minIdx = d3.minIndex(intersects, intersect => intersect[3]);
      //  if(minIdx > -1) {
      //    [xnormal, ynormal] = intersects[minIdx][1];
      //  }
      //}
      normal[0] = xnormal;
      normal[1] = ynormal;
      return true;
    }
    return false;
  }
  
  /*
  window.interhitpolygonmax == undefined && (window.interhitpolygonmax = 1.414213);
  const interpolatepoints = (poly, addonce, factor, polygons) => {
    addonce = !!addonce;

    if(factor == undefined) {
      factor = DEFAULT_RIDGE_POINTDIST_AUTO_FACTOR;
    }
    
    if(factor <= 0) {
      return poly.map(polyline => polyline.map(point => point));
    }
    
    const points = [];
    const length = poly.length;

    let start = 0;
    let end = 0;
    
    while(start < length) {
      //end = poly.slice(start).findIndex(p => p[4] == 8);
      let s = poly[start];
      if(!s) {
        console.log('start:', start);
        break;
      }
      if(s[4] == 8) {
        points.push(s);
        start++;
        continue;
      }
      
      
      end = -1;
      for(let i = start; i < length; i++) {
        if(poly[i][4] == 8) {
          end = i;
          break;
        }
      }
      if(end < 0) {
        end = length
      }

      //if((end - start) < 1) {
      //  continue;
      //}

      let polyline = poly.slice(start, end);
      start = end;
      
      
      const pointdist = d3.mean(polyline, v => v[2]) * factor;
      
      let point = polyline[0];
      //if(!points.length) {
        points.push(point);
      //}
      let prvpoint = point;
      
      for(let dx = 0, dy = 0, dist = 0, i = 1, len = polyline.length; i < len; i++) {
        point = polyline[i];
        dx = point[0] - prvpoint[0];
        dy = point[1] - prvpoint[1];
        dist = Math.hypot(dx, dy);
        if(dist > pointdist && !(prvpoint[7] && point[7] && isCirInts(prvpoint, point))) {
          //dx /= dist;
          //dy /= dist;
          
          let addpoints = (dist / pointdist) | 0;
          if(addonce) {
            addpoints = 1;
          }
                    
          if(polygons) {
            const theta = getTangentAngleRad(prvpoint, point);
            const thetas = [theta + M_PI2, theta - M_PI2];
            for(const pi of [point]) {
              const [xi, yi, zi, walks, type] = pi;
              if(type == 4 || type == 8) {
                continue;
              }
              
              const linelength = zi * interhitpolygonmax;
              
              let rs = [];
              for(const theta of thetas) {
                let pn = [linelength * Math.cos(theta) + xi, linelength * Math.sin(theta) + yi];
                let line = [pi, pn];
                let tangent = getTangentAngleRad(pi, pn);
                let intersect;
                let intersects = [];
                for(const polygon of polygons.toReversed()) {
                  intersect = getPointLineIntersectPolygon(line, polygon);
                  //if(intersect) {
                  //  intersects.push(intersect);
                  //}
                  if(intersect) {
                    let diff = diffAngleRad(tangent, getTangentAngleRad(intersect[0], intersect[1])) * R2D;
                    if(diff >= 30 && diff <= 150) {
                      intersects.push(intersect);
                    }
                    //else {
                    //  console.log('interpolatepoints diff:', diff);
                    //}
                  }
                  //else {
                  //  console.log('interpolatepoints line:', line);
                  //}
                }
                
                for(const intersect of intersects) {
                  intersect[3] = getDist(pi, intersect[1]);
                }
                
                let minIdx = d3.minIndex(intersects, intersect => intersect[3]);
                if(minIdx > -1) {
                  //pi[2] = intersects[minIdx][3];
                  rs.push(intersects[minIdx]);
                }
              }

              let minIdx = d3.minIndex(rs, r => r[3]);
              if(minIdx > -1) {
                //let [xnormal, ynormal] = rs[minIdx][1];
                let dist = rs[minIdx][3];
                //let r75 = pi[2] * 0.75;
                //if(centerlines && centerlines.length && dist > r75) {
                //  let p01 = d3.interpolateNumberArray(pi.slice(0, 2), [xnormal, ynormal])(r75/dist);
                //  let line = [p01, [xnormal, ynormal]];
                //  intersects = [];
                //  for(const centerline of centerlines) {
                //    intersect = getPointLineIntersectPolygon(line, centerline);
                //    if(intersect) {
                //      intersects.push(intersect);
                //    }
                //  }
                //
                //  for(const intersect of intersects) {
                //    intersect[3] = getDist(pi, intersect[1]);
                //  }
                //
                //  minIdx = d3.minIndex(intersects, intersect => intersect[3]);
                //  if(minIdx > -1) {
                //    dist = intersects[minIdx][3];
                //  }
                //}
                pi[2] = dist;
              }
            }
            
            //interpolate = d3.interpolateNumberArray(prvpoint.slice(0, 3), ...pointinters, point.slice(0, 3));
            let interpolate = d3.interpolateNumberArray(prvpoint.slice(0, 3), point.slice(0, 3));
            
            for(let j = 1, jj = addpoints+1; j < jj; j++) {
              //const percentage = j / jj;
              const [x, y, z] = interpolate(j / jj);
              //interpoint[3] = interpoint[4] = 1;
              points.push([x, y, z, 1, 1]);
            }
          }
          else {
            let interpolate = d3.interpolateNumberArray(prvpoint.slice(0, 3), point.slice(0, 3));
            for(let j = 1, jj = addpoints+1; j < jj; j++) {
              //const percentage = j / jj;
              const [x, y, z] = interpolate(j / jj);
              //interpoint[3] = interpoint[4] = 1;
              points.push([x, y, z, 1, 1]);
            }
          }
        }
        points.push(point);
        prvpoint = point;
      }
    }
    return points;
  }
  */

  (window.finalSimplifyTolerance == undefined) && (window.finalSimplifyTolerance = 0.2);
  const ridge_simplify = (polyline, simplifyvalue, usecondition0, usecondition1, usecondition2, isfinal, skipsimdist) => {
    const getSqDistXYZ = (p1, p2) => {
      let dx = p1[0] - p2[0], dy = p1[1] - p2[1];
      let dz = ((p1[2] || 0)*DEFAULT_RIDGE_SIMPLIFY_Z_FACTOR) - ((p2[2] || 0)*DEFAULT_RIDGE_SIMPLIFY_Z_FACTOR);
      return dx * dx + dy * dy + dz * dz;
    }

    isfinal = !!isfinal;
    skipsimdist = !!skipsimdist;

    if(simplifyvalue == undefined) {
      simplifyvalue = DEFAULT_RIDGE_SIMPLIFY;
    }
    if(simplifyvalue < 0) {
      simplifyvalue = Math.min(DEFAULT_RIDGE_SIMPLIFY_AUTO_MAX, d3.mean(polyline.length > 2? polyline.slice(1, -1) : polyline, v => v[2]) * (isfinal? DEFAULT_RIDGE_SIMPLIFY_AUTO_FACTOR_FINAL : DEFAULT_RIDGE_SIMPLIFY_AUTO_FACTOR));
    }
    if(simplifyvalue > 0 && polyline.length > 2) {
      let sqTolerance = simplifyvalue;
      usecondition0 = !!usecondition0;
      usecondition1 = !!usecondition1;
      usecondition2 = !!usecondition2;

      let points = polyline;
      
      for(const tol of [sqTolerance/3, sqTolerance]) {
        let point = points[0];
        let prevPoint = point;
        let newPoints = [point];
        let rescondition0, rescondition1, rescondition2;
        
        if(skipsimdist) {
          newPoints = polyline.slice();
        }
        else {
          for (let i = 1, len = points.length; i < len; i++) {
            point = points[i];

            if(usecondition0) {
              rescondition0 = point[4] >= 8;
            }
            else {
              rescondition0 = false;
            }
            if(usecondition1) {
              rescondition1 = point[4] == 4;
            }
            else {
              rescondition1 = false;
            }
            if(usecondition2) {
              rescondition2 = point[7] && point[7].length;
            }
            else {
              rescondition2 = false;
            }

            if (rescondition0 || rescondition1 || rescondition2 || Math.sqrt(getSqDistXYZ(point, prevPoint)) > tol) {
              newPoints.push(point);
              prevPoint = point;
            }
          }

          if (prevPoint !== point) {
            if(newPoints.length > 1) {
              newPoints.pop();
            }
            newPoints.push(point);
          }
        }
        points = newPoints;
      }

      if (isfinal && points.length > 2 && (window.finalSimplifyTolerance >= 0)) {
        let conditions = true;
        if(points.length == 4 && points[1][4] == 8 && points[2][4] == 8) {
          conditions = false;
        }
        else if(points.length == 5 && points[1][4] == 8 && points[3][4] == 8) {
          conditions = false;
        }
        
        if(conditions) {
          // simplify skeleton [x, y, z] in 3D space
          const tolerance = sqTolerance * (window.finalSimplifyTolerance**2);
          
          // get cumulative sum of dists in 2D space
          let zd = points.map((p, i) => p.slice(0, 3));
          
          let fidx = 0;
          let lidx = points.length-1;
          if(points[fidx][4] == 8 && points[fidx][5]) {
            let diffangledeg = diffAngleDeg(points[fidx][5] * R2D, getTangentAngleDeg(points[fidx], points[fidx+1]));
            zd[fidx][2] = zd[fidx][2] * Math.cos(diffangledeg * D2R);
          }
          
          if(points[lidx][4] == 8 && points[lidx][5]) {
            let diffangledeg = diffAngleDeg(points[lidx][5] * R2D, getTangentAngleDeg(points[lidx-1], points[lidx]));
            zd[lidx][2] = zd[lidx][2] * Math.cos(diffangledeg * D2R);
          }

          let cumsumdist = 0;
          let prev = zd[0];
          for (const p of zd) {
            cumsumdist += getDist(prev, p);
            prev = p;
            p[3] = cumsumdist;
          }
          
          // [x, y] skeleton points in cartesian coordinate
          const xyoffset = 0;
          let simxy = simplifyDouglasPeucker(points, tolerance, xyoffset);
          let simxy_enableindexs = points.map(p => simxy.indexOf(p) > -1);
          if(usecondition2) {
            for (let i = 1, len = points.length-1; i < len; i++) {
              const point = points[i];
              if(point[7] && point[7].length) {
                simxy_enableindexs[i] = true;
              }
            }
          }
          
          // [z, d] height(line width or line thickness) and cumulative sum of distance (in 2D space) between skeleton points
          const zdoffset = 2;
          let simzd = simplifyDouglasPeucker(zd, tolerance, zdoffset);
          let simzd_enableindexs = zd.map(p => simzd.indexOf(p) > -1);
          
          // ready
          //points = points.filter( (v,i) => (v[4] == 8) || simxy_enableindexs[i] || simzd_enableindexs[i] );
          points = points.filter( (v,i) => simxy_enableindexs[i] || simzd_enableindexs[i] );
        }
      }
      polyline = points;
    }
    return polyline;
  }

  const markwalks = (pls) => {
    // filter out duplicated
    pls = pls.map(pl => (
      prev = null,
      pl.filter(p => (f = prev != p, prev = p, f))
    ));

    pls = pls.filter(pl => pl.length > 1);

    // reset walks, node-types (0: unknow, 1: link, 2: links, 4: branch, 8: terminal)
    for(const pl of pls) {
      for(const p of pl) {
        // walks
        p[3] = 0;
        // node-types
        p[4] = 0;
      }
    }

    // walking
    for(const pl of pls) {
      for(const p of pl) {
        // walks
        p[3]++;
      }
    }

    // set node-types (0: unknow, 1: link, 2: links, 4: branch)
    for(const pl of pls) {
      for(const p of pl) {
        if(p[3] > 2)
          p[4] = 4;
        else if(p[3] > 1)
          p[4] = 2;
        else if(p[3] > 0)
          p[4] = 1;
        else
          p[4] = 0;
      }
    }

    return pls;
  }

  const joinpolys = (pls) => {
    let retpls = [];
    let joinpls = [];
    let pl;

    while(pl = pls.shift()) {
      if(pl.length > 1) {
        let lastpoint = pl[pl.length-1];
        if(joinpls.length) {
          pl[0][3] = 1;
          pl[0][4] = 1;
          joinpls.push(pl.slice(1));
        }
        else {
          joinpls.push(pl);
        }
        // cut joinpls, if lastpoint is not links, or lastpoint != firstpoint of next line
        if(lastpoint[4] != 2 || (pls.length && pls[0] && pls[0].length && pls[0][0] != lastpoint)) {
          retpls.push(joinpls.flat());
          joinpls = [];
        }
      }
    }

    if(joinpls.length) {
      retpls.push(joinpls.flat());
      joinpls = [];
    }

    if(retpls.length > 1) {
      // first line
      let fl = retpls[0];
      // firstpoint of first line
      let fpfl = fl[0];
      // lastpoint of first line
      let lpfl = fl[fl.length-1];
      // if fpfl is links && fpfl != lpfl
      if(fpfl[4] == 2 && fpfl != lpfl) {
        pls = retpls;
        retpls = [];
        joinpls = [pls.shift()];
        while(pl = pls.shift()) {
          if(fpfl == pl[0]) {
            joinpls.unshift(pl.slice(1).toReversed());
            retpls.push(...pls);
            break;
          }
          if(fpfl == pl[pl.length-1]) {
            joinpls.unshift(pl.slice(0, -1));
            retpls.push(...pls);
            break;
          }
          retpls.push(pl);
        }
        retpls.unshift(joinpls.flat());
      }
    }

    return retpls;
  }

  // hi --> lo
  const normalizedirections = (pls) => {
    for(const pl of pls) {
      const halflen = pl.length / 2;
      const leftweight = pl.slice(0, halflen).reduce((acc, p) => acc+p[2], 0);
      const rightweight = pl.slice(halflen+0.5).reduce((acc, p) => acc+p[2], 0);
      // 1. consider weight
      // line start from havy --> light weight
      if(leftweight < rightweight) {
        pl.reverse();
      }
      // if line weight is symmetric
      else if(leftweight == rightweight) {
        // 2. consider terminals type
        // line start from branch node to leaf node
        if(pl[0][4] < pl[pl.length-1][4]) {
          pl.reverse();
        }
      }
    }
    return pls;
  }

  let centerline = {
    idx: polygondata && polygondata.value,
    coordinates: [],
    skeletons: [],
    multipolygons: [],
    multipolylines: [],
    centers: [],
    traces: [],
    reduceintersects: [],
    fixedcorners: [],
    normals: [],
    pessatincolumnds: [],
  }

  if(polygondata && polygondata.coordinates && polygondata.coordinates.length) {
    let useworker = USE_WORKER;
    let usemultiworker = USE_MULTI_WORKER;
    let worker;

    if(!useworker) {
      await fnSkeletonBuilderInit();
    }
    
    let shapelen = polygondata.coordinates.length;
    let polygonpointlen = polygondata.coordinates.flat(2).length;

    let timelog;
    let enabletimelog = window.enabletimelog;
    if(enabletimelog) {
      timelog = maketimelog();
      timelog.begin('make skeletons v2', 'shapes:', shapelen, 'points:', polygonpointlen);
    }
    
/*
    centerline.multipolylines = await Promise.all(polygondata.coordinates.map(async(polygons, idx) => {
      enabletimelog && console.debug('polygon:', idx);
      await delay(10);
      let fixedpoints = 0.0;

      !window.retryv2 && (window.retryv2 = { times:[0, 1, 2, 3], tolerances: [0, 0.12, 0.24, 1], fixedpoints: 0.000333 } );
      fixedpoints = 0.0;
      for(const ii of retryv2.times) {
        let p = polygons.slice();

        if(ii >= 0) {
          fixedpoints += retryv2.fixedpoints;
          p = polygons.map((polygon,iii) => {
            if(polygon.length >= 3) {
              let sim = simplifyDouglasPeucker(polygon.slice(), retryv2.tolerances[ii]);
              if(sim.length >= 3 && Math.abs(d3.polygonArea(sim)) > 0) {
                polygon = sim;
              }
              else {
                return null;
              }

              let fixedp = (iii&1) == 0 ? fixedpoints:-fixedpoints;

              polygon.pop();
              polygon = polygon.map((p,i) => (fp = (i&1) == 0 ? fixedp:-fixedp, [p[0] + fp, p[1] + fp]));
              polygon.push([polygon[0][0], polygon[0][1]]);
            }
            return polygon;
          } );

          if(p && p[0] == null) {
            console.debug(window.strdoing || '', 'polygon:', idx, 'v2 skip:', ii);
            continue;
          }
          p = p.filter(v => v);
        }
        
        //let has4 = p.some(v => v.length <= 4);
        //if(has4) {
        //  console.log(p);
        //}
        
        let skeletonv2 = SkeletonBuilder.buildFromPolygon(p);
        
        if(skeletonv2) {
          centerline.coordinates.push(p);
          // todo: can't fix bug z = Infinity on "ไ" font JS-Karabow
          if(skeletonv2.vertices.some(v => !isFinite(v[2]))) {
            //console.log('found Infinity:', JSON.stringify(JSON.stringify(skeletonv2.vertices)));
            //console.log('found Infinity:', window.strdoing || '', 'polygon:', idx, 'v2 retry:', ii);
            if((ii+1) < retryv2.times.length) {
              continue;
            }
          }

          for (const vertice of skeletonv2.vertices) {
            if(isFinite(vertice[2])) {
              vertice[0] = +(vertice[0] / SCALE_XY).toFixed(3); // x
              vertice[1] = +(vertice[1] / SCALE_XY).toFixed(3); // y
              vertice[2] = +(vertice[2] / SCALE_XY).toFixed(3); // z
            }
          }

          for(let j = 0, jj = skeletonv2.vertices.length; j < jj; j++) {
            const vertice = skeletonv2.vertices[j];
            if(vertice[3] == undefined) {
              for(let k = j+1; k < jj; k++) {
                const next = skeletonv2.vertices[k];
                //// dist < 0.001, diff > 0.9995
                //if(next[3] == undefined &&
                //  ((vertice[0] == next[0] && vertice[1] == next[1] && vertice[2] == next[2])
                //   ||
                //   (getDist(vertice, next) < 0.001 && (Math.min(vertice[2], next[2]) / Math.max(vertice[2], next[2])) > 0.9995)
                //  )
                //) {
                if(next[3] == undefined && vertice[0] == next[0] && vertice[1] == next[1] && vertice[2] == next[2]) {
                  skeletonv2.vertices[k] = vertice;
                }
              }
              // set walk
              vertice[3] = 1;
              // type: unknow
              vertice[4] = 0;
            }
          }

          skeletonv2.polygons = skeletonv2.polygons.map(polygon => (
            prev = null,
            polygon.filter(vidx => (curr = skeletonv2.vertices[vidx], f = prev != curr, prev = curr, f))
          ));

          //centerline.skeletons.push(skeletonv2);

          // fixed: bug when vertice is [0, 0, Infinity]
          return skeletonv2.polygons.map(polygon => polygon.map(vidx => skeletonv2.vertices[vidx]).filter(vertice => isFinite(vertice[2])).toReversed()).filter(polygon => polygon.length > 2);
        }

        //console.debug(window.strdoing || '', 'polygon:', idx, 'v2 retry:', ii, JSON.stringify(p));
        console.debug(window.strdoing || '', 'polygon:', idx, 'v2 retry:', ii);
      }
      centerline.coordinates.push([]);
      return [];
    }));
*/

    for(let idx = 0, len = polygondata.coordinates.length; idx < len; idx++) {
      const polygons = polygondata.coordinates[idx];
      centerline.multipolylines.push([]);
      centerline.coordinates.push([]);
      
      enabletimelog && console.debug('polygon:', idx);
      await delay(10);
      let fixedpoints = 0.0;

      !window.retryv2 && (window.retryv2 = { times:[0, 1, 2, 3], tolerances: [0, 0.12, 0.24, 1], fixedpoints: 0.000333 } );
      fixedpoints = 0.0;
      for(const ii of retryv2.times) {
        let p = polygons.slice();

        if(ii >= 0) {
          fixedpoints += retryv2.fixedpoints;
          p = polygons.map((polygon,iii) => {
            if(polygon.length >= 3) {
              let sim = simplifyDouglasPeucker(polygon.slice(), retryv2.tolerances[ii]);
              if(sim.length >= 3 && Math.abs(d3.polygonArea(sim)) > 0) {
                polygon = sim;
              }
              else {
                return null;
              }

              let fixedp = (iii&1) == 0 ? fixedpoints:-fixedpoints;

              polygon.pop();
              polygon = polygon.map((p,i) => (fp = (i&1) == 0 ? fixedp:-fixedp, [p[0] + fp, p[1] + fp]));
              polygon.push([polygon[0][0], polygon[0][1]]);
            }
            return polygon;
          } );

          if(p && p[0] == null) {
            console.debug(window.strdoing || '', 'polygon:', idx, 'v2 skip:', ii);
            continue;
          }
          p = p.filter(v => v);
        }
        
        let skeletonv2;
        
        if(useworker) {
          //skeletonv2 = await makeSkeletonFromWorker(p);
          if(usemultiworker) {
            if(!worker) {
              worker = new Worker("straight-skeleton-v2/worker.js");
            }
            skeletonv2 = await buildSkeletonFromWorker(worker, p);
            if(!skeletonv2) {
              if(worker) {
                worker.terminate();
                worker = undefined;
              }
            }
          }
          else {
            if(!globalskeletonworker) {
              globalskeletonworker = new Worker("straight-skeleton-v2/worker.js");
            }
            skeletonv2 = await buildSkeletonFromWorker(globalskeletonworker, p);
            if(!skeletonv2) {
              if(globalskeletonworker) {
                globalskeletonworker.terminate();
                globalskeletonworker = undefined;
              }
            }
          }
        }
        else {
          skeletonv2 = SkeletonBuilder.buildFromPolygon(p);
        }
        
        if(skeletonv2) {
          //centerline.coordinates.push(p);
          // todo: can't fix bug z = Infinity on "ไ" font JS-Karabow
          if(skeletonv2.vertices.some(v => !isFinite(v[2]))) {
            //console.log('found Infinity:', JSON.stringify(JSON.stringify(skeletonv2.vertices)));
            //console.log('found Infinity:', window.strdoing || '', 'polygon:', idx, 'v2 retry:', ii);
            if((ii+1) < retryv2.times.length) {
              continue;
            }
          }

          for (const vertice of skeletonv2.vertices) {
            if(isFinite(vertice[2])) {
              vertice[0] = +(vertice[0] / SCALE_XY).toFixed(3); // x
              vertice[1] = +(vertice[1] / SCALE_XY).toFixed(3); // y
              vertice[2] = +(vertice[2] / SCALE_XY).toFixed(3); // z
            }
          }

          for(let j = 0, jj = skeletonv2.vertices.length; j < jj; j++) {
            const vertice = skeletonv2.vertices[j];
            if(vertice[3] == undefined) {
              for(let k = j+1; k < jj; k++) {
                const next = skeletonv2.vertices[k];
                //// dist < 0.001, diff > 0.9995
                //if(next[3] == undefined &&
                //  ((vertice[0] == next[0] && vertice[1] == next[1] && vertice[2] == next[2])
                //   ||
                //   (getDist(vertice, next) < 0.001 && (Math.min(vertice[2], next[2]) / Math.max(vertice[2], next[2])) > 0.9995)
                //  )
                //) {
                if(next[3] == undefined && vertice[0] == next[0] && vertice[1] == next[1] && vertice[2] == next[2]) {
                  skeletonv2.vertices[k] = vertice;
                }
              }
              // set walk
              vertice[3] = 1;
              // type: unknow
              vertice[4] = 0;
            }
          }

          skeletonv2.polygons = skeletonv2.polygons.map(polygon => (
            prev = null,
            polygon.filter(vidx => (curr = skeletonv2.vertices[vidx], f = prev != curr, prev = curr, f))
          ));

          //centerline.skeletons.push(skeletonv2);

          // fixed: bug when vertice is [0, 0, Infinity]
          //return skeletonv2.polygons.map(polygon => polygon.map(vidx => skeletonv2.vertices[vidx]).filter(vertice => isFinite(vertice[2])).toReversed()).filter(polygon => polygon.length > 2);
          
          centerline.coordinates[idx] = p;
          centerline.multipolylines[idx] = skeletonv2.polygons.map(polygon => polygon.map(vidx => skeletonv2.vertices[vidx]).filter(vertice => isFinite(vertice[2])).toReversed()).filter(polygon => polygon.length > 2);
          break;
        }

        //console.debug(window.strdoing || '', 'polygon:', idx, 'v2 retry:', ii, JSON.stringify(p));
        console.debug(window.strdoing || '', 'polygon:', idx, 'v2 retry:', ii);
      }
    }
    
    if(useworker) {
      if(!usemultiworker) {
        if(worker) {
          worker.terminate();
          worker = undefined;
        }        
      }
    }
    
    if(enabletimelog) {
      timelog.end();
      console.log(polygonpointlen / timelog.diffsecs | 0, 'points/sec');
    }

    centerline.centers = centerline.multipolylines.map(polylines => polylines.map(polyline => {
      // [x, y, z, walks, type, tangent, actualtangent, error]
      // error: length of p0 -> p1 / radius
      let [x0, y0] = polyline[0];
      let [x1, y1, z1] = polyline[1];
      let dx = x1-x0, dy = y1-y0, dist = Math.hypot(dx, dy);
      let error = dist / z1;

      if(error > DEFAULT_RIDGE_ERROR_MIN) {
        if(polyline[0][7] == undefined) {
          polyline[0][7] = [[error, polyline[1]]];
        }
        else {
          polyline[0][7].push([error, polyline[1]]);
        }

        if(polyline[1][7] == undefined) {
          polyline[1][7] = [[error, polyline[0]]];
        }
        else {
          polyline[1][7].push([error, polyline[0]]);
        }
      }

      return polyline.slice(error < DEFAULT_RIDGE_ERROR_MIN, -1);
    }));

    //centerline.traces = centerline.centers.map((polylines, idx) =>  {
    //  let pls = polylines.map(polyline => polyline.filter(p => p[2] > 0));
    
    window.shrinkunit == undefined && (window.shrinkunit = 0.666);
    const maketerm = (p0, pi, factor, polygons) => {
      const theta = getTangentAngleRad(pi, p0);
      const infos = getInfosFromHit(p0, factor, [theta], polygons);
      if(infos && infos.length && infos.every(v => v != null)) {
        let [poly0, hitpoint, poly1, dist, tangent] = infos[0];
        let r = p0[2];
        let edgelength = getDist(poly0, poly1);
        let edgehalf = edgelength / 2;
        if(r > edgehalf) {
          r = edgehalf;
        }
        let edgetangent = getTangentAngleRad(poly0, poly1) - M_PI2;
        if(diffAngleRad(edgetangent, tangent) > M_PI2) {
          edgetangent += M_1PI;
        }
        const term = [hitpoint[0], hitpoint[1], r, 1, 8, edgetangent];
        if(dist > shrinkunit) {
          let shrink = d3.interpolateNumberArray([p0[0], p0[1]], [term[0], term[1]])((dist-shrinkunit)/dist);
          term[0] = shrink[0];
          term[1] = shrink[1];
        }
        return term;
      }
      return null;
    }

    centerline.traces = centerline.multipolylines.map((polylines, idx) =>  {
      let polygons = centerline.coordinates[idx];
      let newpolylines = polylines.map(polyline => polyline.map(point => point));
      let lengths = centerline.coordinates[idx].map(polygon => polygon.length-1);
      //let pls = lengths.map(length => newpolylines.splice(0, length).flat()).map(polyline => polyline.filter(p => p[2] > 0));
      let pls = lengths.map(length => {
        let polylines = newpolylines.splice(0, length);
        polylines.push(polylines.shift());
        let prev = polylines[polylines.length-1];
        for(const polyline of polylines) {
          if(polyline && polyline[0] && polyline[0][7]) {
            let angle = getAngleDeg(prev[0], polyline[0], polyline[polyline.length-1]);
            polyline[0][7][0][2] = angle;
          }
          prev = polyline;
        }
        return polylines.flat();
      }).map(polyline => {
        let prev = null;
        let res = polyline.filter(p => p && p[2] > 0).filter(p => (f = prev != p, prev = p, f));
        if(res.length == 1) {
          prev = null;
          res = polyline.filter(p => (f = prev != p, prev = p, f));
        }
        return res;
      });

      pls = markwalks(pls);
      
      // fixed: first markwalks
      // then, may not require fix fragment overlaps.
      //for(const pl of pls) {
      //  if(pl[1][4] == 4 || pl[pl.length-2][4] == 4) {
      //    pl[0][3] = 3;
      //    pl[0][4] = 1;
      //  }
      //}

      let polys = [];
      let poly = [];

      for(const pl of pls) {
        for(const p of pl) {
          if(p[3] > 0) {
            poly.push(p);

            // if node-type is link or branch
            if(p[4] & 5) {
              if(poly.length > 1) {
                // cut end line
                polys.push(poly);
                poly = [];

                // if node-type is branch
                if(p[4] & 4) {
                  // start new line from this branch
                  poly.push(p);
                }
              }
            }
          }
          else {
            if(poly.length) {
              if(poly.length > 1) {
                polys.push(poly);
              }
              poly = [];
            }
          }

          p[3] = p[3] < 3? 0 : p[3] - 1;
        }
        if(poly.length) {
          if(poly.length > 1) {
            polys.push(poly);
          }
          poly = [];
        }
      }

      polys = markwalks(polys);
      polys = joinpolys(polys);

      // hard fixed: fragment overlaps
      if(polys.length >= 2) {
        let foundfm = false;
        let removed = 0;
        let maybeflagments = polys.filter(poly => poly.length == 2 && poly[0][4] == 4 && poly[1][4] == 4);

        // remove duplicated, LINEij as same as LINEji
        for(let j = 0, jj = maybeflagments.length; j < jj; j++) {
          const fm0 = maybeflagments[j];
          if(fm0.length) {
            for(let k = j+1; k < jj; k++) {
              const fm1 = maybeflagments[k];
              if(fm1.length && fm0 != fm1) {
                if((fm0[0] == fm1[0] && fm0[1] == fm1[1]) || (fm0[0] == fm1[1] && fm0[1] == fm1[0])) {
                  fm1.length = 0;
                  foundfm = true;
                  //console.log('remove flagment:', ++removed);
                }
              }
            }
          }
        }

        polys = polys.filter(v => v).filter(pl => pl.length);
        if(foundfm) {
          polys = markwalks(polys);
          polys = joinpolys(polys);
        }
      }

      polys = markwalks(polys);
      normalizedirections(polys);

      let temppolys = polys.map(pl => pl.slice());

      window.removeunusualpercentages == undefined && (window.removeunusualpercentages = [0.265, 0.10, 1.5]);
      let removeunusualpercentage = window.removeunusualpercentages[istext? 0:1];
      let branchfactor = window.removeunusualpercentages[2];
      for(const i of [0]) {
        let branchinfos = [];

        polys = polys.filter(pl => {
          let keep = true;
          let fp = pl[0];
          let lp = pl[pl.length-1];
          // some is..., not any
          //if((fp[4] == 4 || lp[4] == 4) && (fp[4] != lp[4])) {
          if((fp[4] == 4 && lp[4] != 4) || (fp[4] != 4 && lp[4] == 4)) {
            if(lp[4] == 4) {
              pl = pl.toReversed();
            }

            // JS-Rapee-Bold: ฝ
            if(pl.length == 3) {
              if(isCirInts(pl[0], pl[1]) && isCirInts(pl[0], pl[2])) {
                if(pl[2][7] && pl[2][7].length == 2) {
                  return keep;
                }
              }
            }

            let [x0, y0, radius, walks, type] = pl[0];

            if(i == 0) {
              let basep = radius + radius;
              let basearea = 0.5 * basep * radius;
              let basegain = radius * radius * removeunusualpercentage;
              keep = !pl.slice(1).every(p => {
                let [x1, y1, z1] = p;
                let dx = x1-x0, dy = y1-y0;
                let dist = Math.hypot(dx, dy);
                let targetarea = 0.5 * basep * (dist + z1);
                let leftarea = targetarea - basearea;
                return leftarea < basegain;
              });

              if(!keep) {
                let decradius = radius * 0.9999;
                keep = pl.slice(1).some(p => {
                  if(p[2] >= decradius) {
                    return true;
                  }
                  if(p[7] && p[7].length > 1 && d3.mean(p[7], error => error[0]) > 1.25) {
                    return true;
                  }
                  return false;
                });
              }
            }
            else {
              keep = !pl.slice(1).every(p => ([x1, y1] = p, dx = x1-x0, dy = y1-y0, Math.hypot(dx, dy) <= radius));
            }
          }
          if(!keep) {
            let errors = pl.map(p => p[7]).flat().filter(v => !!v);
            if(errors.length) {
              pl[0][7] = errors;
            }
            branchinfos.push([pl[0], pl]);
          }
          return keep;
        });

        polys = markwalks(polys);

        // joins polylines
        for(const branchinfo of branchinfos) {
          const [branch, removedpl] = branchinfo;
          if(branch[4] == 2) {
            let retpls = [];
            let joinpls = [];
            let pl;
            while(pl = polys.shift()) {
              let fp = pl[0];
              let lp = pl[pl.length-1];
              if(joinpls.length < 2 && (branch == fp || branch == lp)) {
                if(joinpls.length == 0) {
                  if(branch == fp) {
                    pl = pl.toReversed();
                  }
                }
                else {
                  if(branch == lp) {
                    pl = pl.toReversed();
                  }
                  pl = pl.slice(1);
                }
                joinpls.push(pl);
              }
              else {
                retpls.push(pl);
              }
            }
            if(joinpls.length) {
              branch[3] = 1;
              branch[4] = 1;
              
              if(branch[7] && joinpls.length == 2 && (joinpls[0] && joinpls[0].length > 1) && (joinpls[1] && joinpls[1].length > 0)) {
                let found = false;
                let mainpl = removedpl.slice();
                // fixed: JS-Chusri-Normal: r, add perror if available
                let lp = removedpl[0] == branch? removedpl[removedpl.length-1] : removedpl[0];
                if(lp && lp[7]) {
                  mainpl.push(lp[7][0][1]);
                }
                
                if(!found && (joinpls[0][0][4] != 2) && (!joinpls[0][0][7] || joinpls[0][0][7].length < 2)) {
                  let points = [...mainpl, ...joinpls[0]];
                  let polyline = polylines.find(polyline => points.every(p => polyline.indexOf(p) > -1));
                  if(polyline) {
                    const p0 = polyline[0];
                    const p1 = polyline[polyline.length-1];
                    const midx = (p0[0] + p1[0]) / 2;
                    const midy = (p0[1] + p1[1]) / 2;
                    const radius = getDist(p0, p1) / 2;
                    
                    const target = [midx, midy, radius];
                    const dist = getDist(branch, target);
                    const turndeg = getTurnAngleDeg(target, branch, joinpls[1][0]);
                    
                    if((dist < branch[2] * branchfactor) && Math.abs(turndeg) < 36) {
                      const midp = d3.interpolateNumberArray(branch.slice(0, 3), target)(0.5);
                      joinpls[0] = [midp, branch];
                      found = true;
                    }
                  }
                }
                
                if(!found && (joinpls[1][joinpls[1].length-1][4] != 2) && (!joinpls[1][joinpls[1].length-1][7] || joinpls[1][joinpls[1].length-1][7].length < 2)) {
                  let points = [...mainpl, ...joinpls[1]];
                  let polyline = polylines.find(polyline => points.every(p => polyline.indexOf(p) > -1));
                  if(polyline) {
                    const p0 = polyline[0];
                    const p1 = polyline[polyline.length-1];
                    const midx = (p0[0] + p1[0]) / 2;
                    const midy = (p0[1] + p1[1]) / 2;
                    const radius = getDist(p0, p1) / 2;
                    
                    const target = [midx, midy, radius];
                    const dist = getDist(branch, target);
                    const turndeg = getTurnAngleDeg(target, branch, joinpls[0][joinpls[0].length-2]);
                    
                    if((dist < branch[2] * branchfactor) && Math.abs(turndeg) < 36) {
                      const midp = d3.interpolateNumberArray(branch.slice(0, 3), target)(0.5);
                      joinpls[1] = [midp];
                      found = true;
                    }
                  }
                }
              }
              
              retpls.push(joinpls.flat());
              
              joinpls = [];
            }
            polys = retpls;
          }
        }
        polys = markwalks(polys);
        normalizedirections(polys);
      }

      if(polys.length == 0) {
        //console.log('idx:', idx, temppolys);
        polys = [temppolys.flat()].map(pl => {
          let maxIdx = d3.maxIndex(pl, p => p[2]);
          if(maxIdx > -1) {
            let [x, y, z] = pl[maxIdx];
            return [[x-(z*0.95), y, z/3, 1, 8, 0, 0], [x, y, z*1.05, 1, 1], [x+(z*0.95), y, z/3, 1, 8, 0, 0]];
          }
        }).filter(v => v);
      }
      temppolys = null;
      
      for(const pl of polys) {
        // is loop & first point has error
        if(pl[0] == pl[pl.length-1] && pl[0][4] == 2 && pl[0][7]) {
          // rotate until first point no error
          let normalidx = pl.findIndex(p => p[4] == 1);
          if(normalidx > -1) {
            pl[0][4] = 1;
            pl.pop();
            for(let i = 0; i < normalidx; i++) {
              pl.push(pl.shift());
            }
            pl.push(pl[0]);
            pl[0][4] = 2;
          }
        }
      }
      
      let isround = false;
      if(!isround) {
        let pads = [...new Set(polys.flat())];
        if(pads.length) {
          let errs = pads.map(pad => pad[7]).filter(v => v).flat().filter(err => err[0] > 1.2);
          isround = (errs.length / pads.length) < 0.2;
          if(isround && pads.length > 1) {
            pads.sort((a,b) => b[2] - a[2]);
            let [x0, y0, radius, walks, type] = pads[0];
            let intersectall = pads.slice(1).every(p => ([x1, y1] = p, Math.hypot(x1-x0, y1-y0) <= radius));
            if(intersectall) {
              let p0 = pads[0];
              let p1 = pads[1];
              //let midx = (p0[0] + p1[0]) / 2;
              //let midy = (p0[1] + p1[1]) / 2;
              //let midz = (p0[2] + p1[2]) / 2;
              //let mid = [midx, midy, midz, 1, 1];
              
              let theta = getTangentAngleRad(p1, p0);
              let t = 0.7071067811865476;
              let shrink = 0;
              if(p0[2] > 6) {
                shrink = 1.50;
              }
              else if(p0[2] > 4) {
                shrink = 0.50;
              }
              
              if(Math.min(p0[2], p1[2]) / Math.max(p0[2], p1[2]) < 0.7) {
                let t0 = [(p0[2]-shrink) * Math.cos(theta) + p0[0], (p0[2]-shrink) * Math.sin(theta) + p0[1], p0[2] / 6, 1, 8, theta];
                let m0 = [t*(p0[2]-shrink) * Math.cos(theta) + p0[0], t*(p0[2]-shrink) * Math.sin(theta) + p0[1], t*((p0[2]-1.50)+(p0[2]/6)), 1, 8];
                theta += M_1PI;
                let t1 = [(p0[2]-shrink) * Math.cos(theta) + p0[0], (p0[2]-shrink) * Math.sin(theta) + p0[1], p0[2] / 6, 1, 8, theta];
                let m1 = [t*(p0[2]-shrink) * Math.cos(theta) + p0[0], t*(p0[2]-shrink) * Math.sin(theta) + p0[1], t*((p0[2]-1.50)+(p0[2]/6)), 1, 8];
                
                p0[3] = 1;
                p0[4] = 8;
                polys = [[t0, m0, p0, m1, t1]];
                //polys = interpolatepoints(polys, false, 0.9, polygons);
              }
              else {
                let t0 = [(p0[2]-shrink) * Math.cos(theta) + p0[0], (p0[2]-shrink) * Math.sin(theta) + p0[1], p0[2] / 6, 1, 8, theta];
                let m0 = [t*(p0[2]-shrink) * Math.cos(theta) + p0[0], t*(p0[2]-shrink) * Math.sin(theta) + p0[1], t*((p0[2]-1.50)+(p0[2]/6)), 1, 8];
                theta += M_1PI;
                let t1 = [(p1[2]-shrink) * Math.cos(theta) + p1[0], (p1[2]-shrink) * Math.sin(theta) + p1[1], p1[2] / 6, 1, 8, theta];
                let m1 = [t*(p1[2]-shrink) * Math.cos(theta) + p1[0], t*(p1[2]-shrink) * Math.sin(theta) + p1[1], t*((p1[2]-1.50)+(p1[2]/6)), 1, 8];
                
                p0[3] = 1;
                p0[4] = 8;
                p1[3] = 1;
                p1[4] = 8;
                polys = [[t0, m0, p0, p1, m1, t1]];
                //polys = interpolatepoints(polys, false, 0.9, polygons);
              }
            }
          }
        }
      }

      //polys = polys.map(pl => simplifyDist(pl, 0.002));
      
      // Note: referrence between error and angle
      // error < 1.4142135623730951 => angle > 90 degree
      // error = 1.4142135623730951 => angle = 90 degree
      // error > 1.4142135623730951 => angle < 90 degree
      //
      // ex1. error = 1.3
      // 99.60438018010856 = 180 - Math.atan2(1.3, 1/1.3*2) * R2D * 2
      //
      // ex2. error = 1.4142135623730951
      // 89.99999999999999 = 180 - Math.atan2(1.4142135623730951, 1/1.4142135623730951*2) * R2D * 2
      //
      // ex3. error = 1.6
      // 75.99746488500932 = 180 - Math.atan2(1.6, 1/1.6*2) * R2D * 2
      //
      if(!window.segmentshapes) {
        window.segmentshapes = {};
        window.segmentshapes.enabledebugerror = false;
        window.segmentshapes.enabledebug = false;
        window.segmentshapes.enable1 = true;
        window.segmentshapes.enable2 = true;
        window.segmentshapes.enable3 = true;
        window.segmentshapes.enable4 = true;
        window.segmentshapes.enable5 = true;
        window.segmentshapes.enable6 = true;
        window.segmentshapes.enable7 = true;
        window.segmentshapes.enable8 = true;
        window.segmentshapes.enable9 = true;
        window.segmentshapes.enable10 = true;
        window.segmentshapes.enable11 = true;
        window.segmentshapes.enable12 = true;
        window.segmentshapes.enable13 = true;
        window.segmentshapes.enable14 = true;
        window.segmentshapes.enable15 = true;
        window.segmentshapes.enable16 = true;
        window.segmentshapes.enable17 = true;
        window.segmentshapes.enable18 = true;
        window.segmentshapes.enable19 = true;
        window.segmentshapes.enable20 = true;
        window.segmentshapes.enable21 = true;
        window.segmentshapes.enable22 = true;
        window.segmentshapes.enable23 = true;
        window.segmentshapes.enable24 = true;
        window.segmentshapes.enable25 = true;
        window.segmentshapes.enable26 = true;
        window.segmentshapes.enable27 = true;
        window.segmentshapes.enable28 = true;
        window.segmentshapes.enable29 = true;
        window.segmentshapes.enable30 = true;
        window.segmentshapes.enable31 = true;
        window.segmentshapes.enable32 = true;
        window.segmentshapes.enable33 = true;
        window.segmentshapes.enable34 = false;
        window.segmentshapes.enable35 = true;
        window.segmentshapes.enable36 = true;
        window.segmentshapes.enable37 = true;
        window.segmentshapes.enable38 = true;
        window.segmentshapes.enable39 = true;
        window.segmentshapes.enable40 = true;
        window.segmentshapes.enable41 = true;
        window.segmentshapes.enable42 = true;
        window.segmentshapes.enable43 = false;
        window.segmentshapes.enable44 = false;
        window.segmentshapes.enable45 = true;
        window.segmentshapes.enable46 = true;
        window.segmentshapes.enable47 = true;
        window.segmentshapes.enable48 = true;
        window.segmentshapes.enable49 = true;
        window.segmentshapes.enable50 = true;
        window.segmentshapes.enable51 = true;
        window.segmentshapes.enable52 = true;
        window.segmentshapes.enable53 = true;
        window.segmentshapes.enable54 = true;
      }

      let newpolys = [];
      let branches = [];
      let specialbranches = [];
      for(const pl of polys) {
        let segments = [];
        let segmentstartidx = 0;
        
        if(pl.length < 4) {
          newpolys.push(pl);
          continue;
        }

        let cw = (pl.length > 2) && iscw(pl);
        
        let isbranchs = [];
        if(pl[0][4] == 4) {
          isbranchs.push(pl[0]);
        }
        if(pl[pl.length-1][4] == 4) {
          isbranchs.push(pl[pl.length-1]);
        }
        
        //console.log('cw:', cw, 'max error:', Math.max( ...pl.filter(p => !!p[7]).map(p => p[7]).flat().map(einfo => einfo[0])) );
        
        for(let i = 0, ilen = pl.length, lastidx = ilen - 1; i < ilen; i++) {
          if(i < segmentstartidx) {
            i = segmentstartidx;
          }
          if(i > 0 && i < lastidx) {
            let p = pl[i];
            let found = false;
            let prev  = pl[i-1];
            let prev1 = pl[i-2];
            let prev2 = pl[i-3];
            let prev3 = pl[i-4];
            let prev4 = pl[i-5];
            let prev5 = pl[i-6];
            let next  = pl[i+1];
            let next1 = pl[i+2];
            let next2 = pl[i+3];
            let next3 = pl[i+4];
            let next4 = pl[i+5];
            let next5 = pl[i+6];

            if(p && p[7]) {
              let errors = p[7];
              let maxidx = d3.maxIndex(errors, ([error, point]) => error);
              //let maxidx = 0;
              if(maxidx > -1) {
                let angledeg = 0;
                let [error, point] = errors[maxidx];
                if(point && point[7] && point[7][0] && point[7][0][2]) {
                  angledeg = point[7][0][2];
                }
                let nangledeg = 0;
                if(next && next[7] && next[7][0] && next[7][0][1]) {
                  let target = next[7][0][1];
                  nangledeg = target[7][0][2];
                }
                let n1angledeg = 0;
                if(next1 && next1[7] && next1[7][0] && next1[7][0][1]) {
                  let target = next1[7][0][1];
                  n1angledeg = target[7][0][2];
                }
                let n2angledeg = 0;
                if(next2 && next2[7] && next2[7][0] && next2[7][0][1]) {
                  let target = next2[7][0][1];
                  n2angledeg = target[7][0][2];
                }
                let pangledeg = 0;
                if(prev && prev[7] && prev[7][0] && prev[7][0][1]) {
                  let target = prev[7][0][1];
                  pangledeg = target[7][0][2];
                }
                let p1angledeg = 0;
                if(prev1 && prev1[7] && prev1[7][0] && prev1[7][0][1]) {
                  let target = prev1[7][0][1];
                  p1angledeg = target[7][0][2];
                }
                let p2angledeg = 0;
                if(prev2 && prev2[7] && prev2[7][0] && prev2[7][0][1]) {
                  let target = prev2[7][0][1];
                  p2angledeg = target[7][0][2];
                }
      
                window.segmentshapes.enabledebugerror && console.log('error:', error, 'angle:', angledeg);
                
                // Poller-One-Regular: t
                if(window.segmentshapes.enable53 && !found && error > 1.3 && error < 2.0 &&
                  next && next1 && next2 && next3 &&
                   next[7] &&  next[7][0][0] > 1.3 &&  next[7][0][0] < 2.0 &&
                  next1[7] && next1[7][0][0] > 1.3 && next1[7][0][0] < 2.0 &&
                  next2[7] && next2[7][0][0] > 1.3 && next2[7][0][0] < 2.0 &&
                  getDist(p, next) < Math.max(p[2], next[2]) &&
                  getDist(p, next1) < Math.max(p[2], next1[2]) &&
                  getDist(p, next2) < Math.max(p[2], next2[2]) &&
                  getDist(next, next1) < Math.max(next[2], next1[2]) &&
                  getDist(next, next2) < Math.max(next[2], next2[2]) &&
                  getDist(next1, next2) < Math.max(next1[2], next2[2])
                ) {
                  window.segmentshapes.enabledebug && console.log('idx:', idx, 'enable53:', error, 'angledegs:', angledeg, nangledeg, n1angledeg, n2angledeg);
                  segments.push(pl.slice(segmentstartidx, i+1));
                  segmentstartidx = i+3;
                  found = true;
                  i = segmentstartidx;
                }

                // JS-Kobori-Allcaps-Bold: น
                if(window.segmentshapes.enable54 && !found && error > 1.3 && error < 2.0 &&
                  prev && next && next1 && next2 &&
                  prev[4] == 4 &&
                   next[7] &&  next[7][0][0] > 1.30 &&  next[7][0][0] < 2.0 &&
                  next1[7] && next1[7][0][0] > 1.15 && next1[7][0][0] < 2.0 &&
                  getDist(prev, p) < Math.max(p[2], next[2]) &&
                  getDist(prev, next) < Math.max(prev[2], next[2]) &&
                  getDist(prev, next1) < Math.max(prev[2], next1[2]) &&
                  isCirInts(next, next1)
                ) {
                  window.segmentshapes.enabledebug && console.log('idx:', idx, 'enable54:', error, 'angledegs:', angledeg, nangledeg, n1angledeg, n2angledeg);
                  segmentstartidx = i+2;
                  found = true;
                  i = segmentstartidx;
                  
                  prev[3] = prev[4] = 1;
                }

                // JS-Karabow: น
                if(window.segmentshapes.enable51 && !found && error > 1.3 && error < 2.0 &&
                  angledeg < 0 && nangledeg == 0 &&
                  getDist(p,  next) < next[2]
                ) {
                  for(let j = i+2; j < ilen; j++) {
                    let nextpoint = pl[j];
                    if(nextpoint && nextpoint[7] && nextpoint[7][0] && nextpoint[7][0][1]) {
                      let target = nextpoint[7][0][1];
                      let nextangledeg = target[7][0][2];
                      if(nextangledeg < 0) {
                        if(getDist(p, nextpoint) < Math.min(p[2], nextpoint[2])) {
                          let isround = false;
                          let intersectall = false;
                          let pads = pl.slice(i+1, j);
                          if(pads.length) {
                            let errs = pads.map(pad => pad[7]).filter(v => v).flat().filter(err => err[0] > 1.2);
                            isround = (errs.length / pads.length) < 0.2;
                            if(isround && pads.length > 1) {
                              pads.sort((a,b) => b[2] - a[2]);
                              let [x0, y0, radius, walks, type] = pads[0];
                              intersectall = pads.slice(1).every(p => ([x1, y1] = p, Math.hypot(x1-x0, y1-y0) <= radius));
                              if(intersectall) {
                                let p0 = pads[0];
                                let p1 = pads[1];
                                
                                let theta = getTangentAngleRad(p1, p0);
                                let t = 0.7071067811865476;
                                let shrink = 0;
                                if(p0[2] > 6) {
                                  shrink = 1.50;
                                }
                                else if(p0[2] > 4) {
                                  shrink = 0.50;
                                }
                                
                                if(Math.min(p0[2], p1[2]) / Math.max(p0[2], p1[2]) < 0.7) {
                                  let t0 = [(p0[2]-shrink) * Math.cos(theta) + p0[0], (p0[2]-shrink) * Math.sin(theta) + p0[1], p0[2] / 6, 1, 8, theta];
                                  let m0 = [t*(p0[2]-shrink) * Math.cos(theta) + p0[0], t*(p0[2]-shrink) * Math.sin(theta) + p0[1], t*((p0[2]-1.50)+(p0[2]/6)), 1, 8];
                                  theta += M_1PI;
                                  let t1 = [(p0[2]-shrink) * Math.cos(theta) + p0[0], (p0[2]-shrink) * Math.sin(theta) + p0[1], p0[2] / 6, 1, 8, theta];
                                  let m1 = [t*(p0[2]-shrink) * Math.cos(theta) + p0[0], t*(p0[2]-shrink) * Math.sin(theta) + p0[1], t*((p0[2]-1.50)+(p0[2]/6)), 1, 8];
                                  
                                  p0[3] = 1;
                                  p0[4] = 8;
                                  pads = [t0, m0, p0, m1, t1];
                                }
                                else {
                                  let t0 = [(p0[2]-shrink) * Math.cos(theta) + p0[0], (p0[2]-shrink) * Math.sin(theta) + p0[1], p0[2] / 6, 1, 8, theta];
                                  let m0 = [t*(p0[2]-shrink) * Math.cos(theta) + p0[0], t*(p0[2]-shrink) * Math.sin(theta) + p0[1], t*((p0[2]-1.50)+(p0[2]/6)), 1, 8];
                                  theta += M_1PI;
                                  let t1 = [(p1[2]-shrink) * Math.cos(theta) + p1[0], (p1[2]-shrink) * Math.sin(theta) + p1[1], p1[2] / 6, 1, 8, theta];
                                  let m1 = [t*(p1[2]-shrink) * Math.cos(theta) + p1[0], t*(p1[2]-shrink) * Math.sin(theta) + p1[1], t*((p1[2]-1.50)+(p1[2]/6)), 1, 8];
                                  
                                  p0[3] = 1;
                                  p0[4] = 8;
                                  p1[3] = 1;
                                  p1[4] = 8;
                                  pads = [t0, m0, p0, p1, m1, t1];
                                }
                              }
                            }
                          }
                          
                          if(intersectall) {
                            window.segmentshapes.enabledebug && console.log('idx:', idx, 'enable51:', error, 'angledegs:', angledeg, nangledeg, n1angledeg, n2angledeg);
                            segments.push(pl.slice(segmentstartidx, i+1));
                            segments.push(pads);
                            segmentstartidx = j;
                            i = segmentstartidx;
                            found = true;
                          }
                        }
                      }
                    }
                  }
                }

                // JS-Chodok-Bold: v
                if(window.segmentshapes.enable50 && !found && error > 2.6 && error < 12.0 &&
                  angledeg < 0 && prev[4] == 4 &&
                  getDist(prev, p) < prev[2] &&
                  !isCirInts(p,  next)
                ) {
                  window.segmentshapes.enabledebug && console.log('idx:', idx, 'enable50:', error, 'angledegs:', angledeg, nangledeg, n1angledeg, n2angledeg);
                  p[3] = 1;
                  p[4] = 8;
                  
                  segmentstartidx = i;
                  found = true;
                  i = segmentstartidx;
                  
                  let branchprev = prev;
                  if(branchprev) {
                    branchprev[3]--;
                    if(branchprev[3] > 2)
                      branchprev[4] = 4;
                    else if(branchprev[3] > 1)
                      branchprev[4] = 2;
                    else
                      branchprev[4] = 1;
                    branches.push(branchprev);
                  }
                }

                // JS-Saksit-Bold-Italic: ท
                if(window.segmentshapes.enable49 && !found && error > 2.5 && error < 12.0 &&
                  angledeg < 0 && nangledeg > 0 && n1angledeg > 0 && n2angledeg == 0 &&
                  prev && next2 && isCirInts(p,  next1) &&
                   next[7] &&  next[7][0][0] > 1.2 &&  next[7][0][0] < 1.8 &&
                  next1[7] && next1[7][0][0] > 1.2 && next1[7][0][0] < 1.8
                ) {
                  window.segmentshapes.enabledebug && console.log('idx:', idx, 'enable49:', error, 'angledegs:', angledeg, nangledeg, n1angledeg, n2angledeg);
                  segments.push(pl.slice(segmentstartidx, i+1));
                  segmentstartidx = i+2;
                  found = true;
                  i = segmentstartidx;
                  
                  let t0 = maketerm(p, prev, 3.0, polygons);
                  if(t0) {
                    segments[segments.length-1].push(t0);
                  }

                  let t1 = maketerm(next1, next2, 3.0, polygons);
                  if(t1) {
                    segmentstartidx--;
                    pl[segmentstartidx] = t1;
                  }
                }
                
                // JS-Neeno-Normal: w, 2.7
                // Kt-smarn-Pirate-Italic: wm, error: 3.4
                // Kt-smarn-Pirate: wm, error: 3.4
                if(window.segmentshapes.enable48 && !found && error > 2.5 && error < 4.0 &&
                  ((angledeg > 0 && nangledeg < 0) || (angledeg < 0 && nangledeg > 0)) &&
                  prev && next && next1 && isCirInts(p,  next) &&
                  diffAngleDeg(getTangentAngleDeg(p, point), getTangentAngleDeg(next, next[7][0][1])) >= 170
                ) {
                  window.segmentshapes.enabledebug && console.log('idx:', idx, 'enable48:', error, 'angledegs:', angledeg, nangledeg, n1angledeg, n2angledeg);
                  pl[i][3] = -1;
                  pl[i+1][3] = -1;
                  segments.push(pl.slice(segmentstartidx, i+1));
                  segmentstartidx = i+1;
                  found = true;
                  i = segmentstartidx;
                  
                  let t0 = maketerm(p, prev, 2.5, polygons);
                  if(t0) {
                    // fixed r
                    let tangent = getTangentAngleRad(prev, p);
                    let difftheta = diffAngleRad(tangent, t0[5]);
                    if(difftheta > M_PI3) {
                      difftheta = M_PI3;
                    }
                    let maxr = t0[2] / Math.cos(difftheta);
                    if(angledeg > 0) {
                      let r = getDist(t0, point);
                      if(maxr > r) {
                        t0[2] = r;
                      }
                      else {
                        t0[2] = maxr;
                      }
                    }
                    else {
                      let r = getDist(t0, next[7][0][1]);
                      if(maxr > r) {
                        t0[2] = r;
                      }
                      else {
                        t0[2] = maxr;
                      }
                    }
                    segments[segments.length-1].push(t0);
                  }

                  let t1 = maketerm(next, next1, 2.5, polygons);
                  if(t1) {
                    segmentstartidx--;
                    // fixed r
                    let tangent = getTangentAngleRad(next1, next);
                    let difftheta = diffAngleRad(tangent, t1[5]);
                    if(difftheta > M_PI3) {
                      difftheta = M_PI3;
                    }
                    let maxr = t1[2] / Math.cos(difftheta);
                    if(angledeg > 0) {
                      let r = getDist(t1, point);
                      if(maxr > r) {
                        t1[2] = r;
                      }
                      else {
                        t1[2] = maxr;
                      }
                    }
                    else {
                      let r = getDist(t1, next[7][0][1]);
                      if(maxr > r) {
                        t1[2] = r;
                      }
                      else {
                        t1[2] = maxr;
                      }
                    }
                    pl[segmentstartidx] = t1;
                  }
                }

                // Chokokutai-Regular: t
                // hard code!!: ilen < 5 to fixed Kt-smarn-JAK-KA-JEE: 3, Kt-smarn-Ribbon: 3
                if(window.segmentshapes.enable47 && !found && error > 3.0 && error < 12.0 &&
                  angledeg > 0 && angledeg < 40 &&
                  pl[0] && pl[0][4] == 4 && isCirInts(pl[0], p) &&
                  ilen < 5
                ) {
                  let target = point;
                  let dist = getDist(p, target);
                  if(dist > 3) {
                    window.segmentshapes.enabledebug && console.log('idx:', idx, 'enable47:', error, 'angledegs:', angledeg, nangledeg, n1angledeg, n2angledeg);
                    segments.push(pl.slice(segmentstartidx, i+1));
                    segmentstartidx = i+1;
                    found = true;
                    i = segmentstartidx;
                    
                    let shrink = d3.interpolateNumberArray([p[0], p[1], p[2]], [target[0], target[1], target[2]])((dist-shrinkunit)/dist);
                    shrink[3] = 1;
                    shrink[4] = 1;
                    segments[segments.length-1].push(shrink);
                  }
                }
                
                // JS-Jindara-Bold-Italic: w, error: 1.22
                // Black-Han-Sans-Regular: W, error: 1.31
                if(window.segmentshapes.enable46 && !found && error > 1.20 && error < 1.50 &&
                  angledeg < 0 && nangledeg > 0 && n1angledeg > 0 && n2angledeg < 0 &&
                  prev && next3 && isCirInts(p,  next2)
                ) {
                  window.segmentshapes.enabledebug && console.log('idx:', idx, 'enable46:', error, 'angledegs:', angledeg, nangledeg, n1angledeg, n2angledeg);
                  segments.push(pl.slice(segmentstartidx, i+1));
                  segmentstartidx = i+3;
                  found = true;
                  i = segmentstartidx;
                  
                  let t0 = maketerm(p, prev, 3.0, polygons);
                  if(t0) {
                    segments[segments.length-1].push(t0);
                  }

                  let t1 = maketerm(next2, next3, 3.0, polygons);
                  if(t1) {
                    segmentstartidx--;
                    pl[segmentstartidx] = t1;
                  }
                  
                  if(pl[pl.length-1][4] == 4) {
                    pl[pl.length-1][3]--;
                    specialbranches.push(pl[pl.length-1]);
                  }
                  
                  if(pl[0][4] == 4) {
                    pl[0][3]--;
                    specialbranches.push(pl[0]);
                  }
                }
                
                // JS-Noklae-Normal, Mali-Bold, Mali-Italic: W w V v
                if(window.segmentshapes.enable42 && !found && angledeg > -60 && angledeg < 0 && error > 1.5 && error < 4.0 &&
                  isbranchs.every(isbranch => getDist(isbranch, p) > isbranch[2])
                ) {
                  if((error > 3.0 || error < 2.2) &&
                    prev && prev1 &&
                    next && next1 &&
                    isCirInts(prev,  p) &&
                    isCirInts(p,  next) &&
                    (!prev[7] || prev[7][0][0] < 1.1) && (!prev1[7] || prev1[7][0][0] < 1.1) &&
                    (!next[7] || next[7][0][0] < 1.1) && (!next1[7] || next1[7][0][0] < 1.1) &&
                    prev[2] > p[2] && prev1[2] > p[2] &&
                    next[2] > p[2] && next1[2] > p[2]
                  ) {
                    window.segmentshapes.enabledebug && console.log('idx:', idx, 'enable42-0:', error);
                    segments.push(pl.slice(segmentstartidx, i));
                    segmentstartidx = i;
                    let pt = [...p.slice(0, 3), 1, 1];
                    segments[segments.length-1].push(pt);
                    found = true;
                  }
                  else if(
                    prev && prev1 &&
                    next &&
                    isCirInts(prev,  p) &&
                    isCirInts(p,  next) &&
                    (!prev[7] || prev[7][0][0] < 1.1) && (!prev1[7] || prev1[7][0][0] < 1.1) &&
                    (!next[7] || next[7][0][0] < 1.1) &&
                    prev[2] > p[2] && prev1[2] > p[2] &&
                    next[2] > p[2]
                  ) {
                    window.segmentshapes.enabledebug && console.log('idx:', idx, 'enable42-1:', error);
                    segments.push(pl.slice(segmentstartidx, i));
                    segmentstartidx = i;
                    let pt = [...p.slice(0, 3), 1, 1];
                    segments[segments.length-1].push(pt);
                    found = true;
                  }
                }

                // Smokum-Regular: v
                if(window.segmentshapes.enable41 && !found && angledeg < 0 && ilen > 8 && error > 2.2 && error < 2.6 &&
                  prev && prev1 && prev2 &&
                  next && next1 && next2 &&
                  prev[7] && prev1[7] && prev2[7] &&
                  next[7] && next1[7] && next2[7] &&
                  prev[7][0][0] > 1.37 && prev1[7][0][0] > 1.37 &&
                  next[7][0][0] > 1.37 && next1[7][0][0] > 1.37 &&
                  isCirInts(prev2, next2) &&
                  isCirInts(prev1, next1) &&
                  isCirInts(prev,  next) &&
                  isCirInts(prev,  p) &&
                  isCirInts(p,  next)
                ) {
                  window.segmentshapes.enabledebug && console.log('idx:', idx, 'enable41:', error);
                  segments.push(pl.slice(segmentstartidx, i-2));
                  segmentstartidx = i+3;
                  let sm0 = pl.slice(i-2, i);
                  let sm1 = pl.slice(i+1, i+3);
                  
                  const maketerm = (p0, pi) => {
                    const theta = getTangentAngleRad(pi, p0);
                    const infos = getInfosFromHit(p0, 2, [theta], polygons);
                    if(infos && infos.length && infos.every(v => v != null)) {
                      let [poly0, hitpoint, poly1, dist, tangent] = infos[0];
                      let r = p0[2];
                      let edgetangent = getTangentAngleRad(poly0, poly1) - M_PI2;
                      if(diffAngleRad(edgetangent, tangent) > M_PI2) {
                        edgetangent += M_1PI;
                      }
                      const term = [hitpoint[0], hitpoint[1], r, 1, 8, edgetangent];
                      let shrink = d3.interpolateNumberArray([p0[0], p0[1]], [term[0], term[1]])((dist-shrinkunit)/dist);
                      term[0] = shrink[0];
                      term[1] = shrink[1];
                      return term;
                    }
                    return null;
                  }
                  
                  let sm00 = maketerm(...sm0);
                  let sm01 = maketerm(...sm0.toReversed());
                  sm0 = [sm00, ...sm0, sm01].filter(v => v);
                  
                  let sm10 = maketerm(...sm1);
                  let sm11 = maketerm(...sm1.toReversed());
                  sm1 = [sm10, ...sm1, sm11].filter(v => v);
                  
                  segments.push(sm0, sm1);
                  
                  found = true;
                }

                // find couple errors in opposite direction:
                if(window.segmentshapes.enable40 && !found && angledeg < 0 &&
                  isbranchs.every(isbranch => getDist(isbranch, p) > isbranch[2]) &&
                  ( (errors.length == 2 && !prev[7]  && !next[7] &&
                     errors[0][0] > 1.0 && errors[0][0] < 2.0 &&
                     errors[1][0] > 1.0 && errors[1][0] < 2.0 &&
                     diffAngleDeg(getTangentAngleDeg(p, errors[0][1]), getTangentAngleDeg(p, errors[1][1])) > 170
                    )
                    ||
                    (errors.length == 1 && error > 1.0 && error < 2.0 && prev && next &&
                      ( (prev1 && !prev1[7] &&
                         !next[7] && prev[7] && prev[7].length == 1 && prev[7][0][0] > 1.0 && prev[7][0][0] < 2.0 &&
                         getDist(p, prev) < 3 &&
                         diffAngleDeg(getTangentAngleDeg(prev, prev[7][0][1]), getTangentAngleDeg(p, point)) > 170
                        )
                        ||
                        (next1 && !next1[7] &&
                         !prev[7] && next[7] && next[7].length == 1 && next[7][0][0] > 1.0 && next[7][0][0] < 2.0 &&
                         getDist(p, next) < 3 &&
                         diffAngleDeg(getTangentAngleDeg(next, next[7][0][1]), getTangentAngleDeg(p, point)) > 170
                        )
                      )
                    )
                  )
                ) {
                  window.segmentshapes.enabledebug && console.log('idx:', idx, 'enable40:', error);
                  //if(prev && prev[4] == 4 && Math.hypot(p[0]-prev[0], p[1]-prev[1]) <= prev[2] && (prev[2]/p[2]) > 3) {
                  //  if(--prev[3] <= 1) {
                  //    prev[4] = 1;
                  //    //pl[0] = d3.interpolateNumberArray(prev.slice(0, 3), p.slice(0, 3))(0.5);
                  //    let lines = polys.filter(poly => (poly != pl && poly[0] == prev));
                  //    for(let line of lines) {
                  //      pl[0] = d3.interpolateNumberArray(line[0].slice(0, 3), line[1].slice(0, 3))(0.5);
                  //    }
                  //  }
                  //}

                  let slicto, p0, p1, prevtangent;
                  if(errors.length == 2) {
                    slicto = i;
                    segments.push(pl.slice(segmentstartidx, slicto));
                    segmentstartidx = i+1;
                    p0 = errors[0][1];
                    p1 = errors[1][1];
                    prevtangent = getTangentAngleRad(prev, p);
                  }
                  else {
                    if(!next[7]) {
                      slicto = i-1;
                      segments.push(pl.slice(segmentstartidx, slicto));
                      segmentstartidx = i+1;
                      p0 = prev[7][0][1];
                      p1 = point;
                      prevtangent = getTangentAngleRad(prev1, p);
                    }
                    else {
                      slicto = i;
                      segments.push(pl.slice(segmentstartidx, slicto));
                      segmentstartidx = i+2;
                      p0 = point;
                      p1 = next[7][0][1];;
                      prevtangent = getTangentAngleRad(prev, p);
                    }
                  }
                  
                  const midx = (p0[0] + p1[0]) / 2;
                  const midy = (p0[1] + p1[1]) / 2;
                  const dx = p0[0] - p1[0];
                  const dy = p0[1] - p1[1];
                  const radius = Math.hypot(dx, dy) / 2;
                  let tangent = getTangentAngleRad(p0, p1) - M_PI2;
                  if(diffAngleRad(prevtangent, tangent) > (90 * D2R)) {
                    tangent += M_PI;
                  }
                  
                  const p8 = [midx, midy, radius, 1, 8, tangent];
                  segments[segments.length-1].push(p8);
                  
                  segmentstartidx--;
                  pl[segmentstartidx] = [p8[0], p8[1], radius, 1, 8, tangent - M_PI];
                  found = true;
                  
                }

                // find couple simple errors in opposite direction:
                // ie. sara อุ อู อี
                if(window.segmentshapes.enable43 && !found && next && next[7] &&
                  isbranchs.every(isbranch => getDist(isbranch, p) > isbranch[2]) &&
                  errors.length == 1 && error > 1.0 && error < 2.0 &&
                  next[7].length == 1 && next[7][0][0] > 1.0 && next[7][0][0] < 2.0 &&
                  getDist(p, next) < Math.min(p[2], next[2]) &&
                  diffAngleDeg(getTangentAngleDeg(next, next[7][0][1]), getTangentAngleDeg(p, point)) > 170
                ) {
                  window.segmentshapes.enabledebug && console.log('idx:', idx, 'enable43:', error);

                  segments.push(pl.slice(segmentstartidx, i));
                  segmentstartidx = i+2;
                  found = true;

                  let p0 = point;
                  let p1 = next[7][0][1];;
                  let prevtangent = getTangentAngleRad(prev, p);
                  
                  const midx = (p0[0] + p1[0]) / 2;
                  const midy = (p0[1] + p1[1]) / 2;
                  const dx = p0[0] - p1[0];
                  const dy = p0[1] - p1[1];
                  const radius = Math.hypot(dx, dy) / 2;
                  let tangent = getTangentAngleRad(p0, p1) - M_PI2;
                  if(diffAngleRad(prevtangent, tangent) > (90 * D2R)) {
                    tangent += M_PI;
                  }
                  
                  const p8 = [midx, midy, radius, 1, 8, tangent];
                  p8[7] = [p[7][0], next[7][0]];
                  segments[segments.length-1].push(p8);
                  
                  segmentstartidx--;
                  i = segmentstartidx;
                  pl[segmentstartidx] = [p8[0], p8[1], radius, 1, 8, tangent - M_PI, tangent - M_PI, p8[7]];
                }

                // BaiJamjuree-BoldItalic: W, error: 3.4078519119420996 - 4.441911246503835, nexterror: 1.2 - 1.34. preverror: 1.18
                if(window.segmentshapes.enable38 && !found && angledeg < 0 && error > 3.0 && error < 5.0 && errors.length == 1 && prev && next && prev1 && next1 &&
                  getAbsAngleDeg(prev, p, next) < 110 && isCirInts(p, prev) && isCirInts(p, next) &&
                  ((!next[7] && prev[7] && prev[7].length == 1 && prev[7][0][0] > 1.15 && prev[7][0][0] < 1.35 &&
                    (angle = diffAngleDeg(getTangentAngleDeg(prev, prev[7][0][1]), getTangentAngleDeg(p, point)), angle > 130 && angle < 150)
                   )
                   ||
                   (!prev[7] && next[7] && next[7].length == 1 && next[7][0][0] > 1.15 && next[7][0][0] < 1.35 &&
                    (angle = diffAngleDeg(getTangentAngleDeg(next, next[7][0][1]), getTangentAngleDeg(p, point)), angle > 130 && angle < 150)
                   )
                  )
                ) {
                  window.segmentshapes.enabledebug && console.log('idx:', idx, 'enable38:', error);
                  segments.push(pl.slice(segmentstartidx, i));
                  segmentstartidx = i+1;
                  found = true;
                }

                // BaiJamjuree-BoldItalic: 4
                if(window.segmentshapes.enable37 && !found && angledeg < 0 && error > 1.5 && error < 1.6 && errors.length == 2 && prev1 && next1 &&
                   prev &&  prev[7] &&  prev[7].length == 1  &&  prev[7][0][0] > 1.5  &&  prev[7][0][0] < 1.6 &&
                   next &&  next[7] &&  next[7].length == 1  &&  next[7][0][0] > 1.08 &&  next[7][0][0] < 1.1 &&
                  isCirInts(p, prev) &&
                  (diffAngleRad(getTangentAngleRad(prev, prev[7][0][1]), getTangentAngleRad(p, point)) * R2D) > 170 &&
                  getAbsAngleDeg(prev, p, next) < 110 &&
                  getAbsAngleDeg(prev1, p, next1) < 45.0
                ) {
                  window.segmentshapes.enabledebug && console.log('idx:', idx, 'enable37:', error);
                  segments.push(pl.slice(segmentstartidx, i));
                  segmentstartidx = i+1;
                  found = true;
                  
                  const p0 = errors[0][1];
                  const p1 = errors[1][1];
                  const midx = (p0[0] + p1[0]) / 2;
                  const midy = (p0[1] + p1[1]) / 2;
                  const dx = p0[0] - p1[0];
                  const dy = p0[1] - p1[1];
                  const radius = Math.hypot(dx, dy) / 2;
                  let tangent = getTangentAngleRad(p0, p1) - M_PI2;
                  if(diffAngleRad(tangent, getTangentAngleRad(prev, p)) > (90 * D2R)) {
                    tangent += M_PI;
                  }
                  
                  const p8 = [midx, midy, radius, 1, 8, tangent];
                  
                  let lastsegment = segments[segments.length-1];
                  let px = lastsegment && lastsegment[lastsegment.length-1];
                  if(px) {
                    let dist = getDist(px, p8);
                    let shrink = d3.interpolateNumberArray([px[0], px[1]], [p8[0], p8[1]])((dist-shrinkunit)/dist);
                    p8[0] = shrink[0];
                    p8[1] = shrink[1];
                  }
                  
                  segments[segments.length-1].push(p8);
                  
                  segmentstartidx--;
                  pl[segmentstartidx] = [p8[0], p8[1], radius, 1, 8, tangent - M_PI];
                }

                // Aclonica-Regular: k
                if(window.segmentshapes.enable36 && !found && error > 2.2 && error < 2.4 &&
                   prev &&  prev[7] &&  prev[7].length == 2  &&  prev[7][0][0] > 1.35 &&  prev[7][0][0] < 1.45 &&  prev[7][1][0] > 1.35 &&  prev[7][1][0] < 1.45 &&
                   next &&  next[7] &&  next[7].length == 2  &&  next[7][0][0] > 1.35 &&  next[7][0][0] < 1.45 &&  next[7][1][0] > 1.35 &&  next[7][1][0] < 1.45 &&
                  next1 && next1[7] && next1[7][0][0] > 1.13 && next1[7][0][0] < 1.15 &&
                  isCirInts(p, next) &&
                  getAbsAngleDeg(prev, next, next1) < 30.0
                ) {
                  window.segmentshapes.enabledebug && console.log('idx:', idx, 'enable36:', error);
                  segments.push([prev, next]);
                  segmentstartidx = i+2;
                  found = true;
                }

                // NotoSansThai-SemiCondensed-Black: ผ, ฝ
                if(window.segmentshapes.enable22 && !found && angledeg < 0 && error > 1.4 &&
                  next && next[7] && next[7][0][0] > 1.4 &&
                  next1 && next1[7] && next1[7][0][0] > 1.15 && next1[7][0][0] < 1.25 &&
                  next2 && next2[7] && next2[7][0][0] > 1.15 && next2[7][0][0] < 1.25 &&
                  isCirInts(p, next) && isCirInts(next, next2) &&
                  prev && getAbsAngleDeg(prev, p, next2) < 60.0
                ) {
                  window.segmentshapes.enabledebug && console.log('idx:', idx, 'enable22:', error);
                  segments.push(pl.slice(segmentstartidx, i+1));
                  segmentstartidx = i+3;
                  found = true;
                  if(pl[pl.length-1][4] == 4) {
                    pl[pl.length-1][3]--;
                    specialbranches.push(pl[pl.length-1]);
                  }
                }

                // NotoSansThai-SemiCondensed-ExtraBold: ผ, ฝ
                if(window.segmentshapes.enable23 && !found && angledeg < 0 && error > 1.4 &&
                  prev && prev[7] && prev[7][0][0] > 1.4 &&
                  prev1 && prev1[7] && prev1[7][0][0] > 1.15 && prev1[7][0][0] < 1.25 &&
                  prev2 && prev2[7] && prev2[7][0][0] > 1.15 && prev2[7][0][0] < 1.25 &&
                  isCirInts(p, prev) && isCirInts(prev, prev2) &&
                  next && getAbsAngleDeg(prev2, p, next) < 60.0
                ) {
                  window.segmentshapes.enabledebug && console.log('idx:', idx, 'enable23:', error);
                  segments.push(pl.slice(segmentstartidx, i-2));
                  segmentstartidx = i;
                  found = true;

                  // JS-Giat-Bold: 5
                  if((pl.length - 1 - i < 3) && next && next[7] && next1 && next1[7] && next1[7].length == 2) {
                    segmentstartidx--;
                  }

                  if(pl[0][4] == 4) {
                    pl[0][3]--;
                    specialbranches.push(pl[0]);
                  }
                }
                
                // ก, ภ, ญ
                // add: JS-Giat-Bold, ก, ฎ
                if(window.segmentshapes.enable29 && !found &&
                  isCirInts(p, next) &&
                  (
                    (error > 1.3 && cw &&
                    angledeg > 0 && nangledeg < 0 && n1angledeg < 0 &&
                    prev &&
                    next && next[7] && next[7][0][0] > 1.3 &&
                    next1 && next1[7] && next1[7][0][0] > 1.4 &&
                    next2 && next2[4] == 4 &&
                    error < next[7][0][0] && error < next1[7][0][0] &&
                    getAbsAngleRad(next[7][0][1], next, next1[7][0][1]) > (120 * D2R) &&
                    isCirInts(p, next2) )
                    ||
                    (error > 1.3 && cw &&
                    angledeg > 0 && n1angledeg < 0 && n2angledeg < 0 &&
                    prev &&
                    next && !next[7] &&
                    next1 && next1[7] && next1[7][0][0] > 1.3 &&
                    next2 && next2[7] && next2[7][0][0] > 1.3 &&
                    next3 && next3[4] == 4 &&
                    getAbsAngleRad(next1[7][0][1], next1, next2[7][0][1]) > (120 * D2R) &&
                    isCirInts(p, next3) )
                    ||
                    (error > 1.3 && !cw &&
                    angledeg < 0 && nangledeg < 0 &&
                    prev1 && prev1[4] == 4 &&
                    prev  && !prev[7] &&
                    next  &&  next[7] &&  next[7][0][0] > 1.3 &&
                    next1 && next1[4] == 4 &&
                    getAbsAngleRad(point, p, next[7][0][1]) > (120 * D2R) &&
                    isCirInts(prev1, next1) )
                    ||
                    (error > 1.3 && cw &&
                    angledeg > 0 && nangledeg < 0 && n1angledeg < 0 &&
                    next && next[7] && next[7][0][0] > 1.3 &&
                    next1 && next1[7] && next1[7][0][0] > 1.4 &&
                    next2 && next2[7] && next2[7][0][0] > 1.15 &&
                    next2[7][0][0] < error &&
                    error < next[7][0][0] && error < next1[7][0][0] &&
                    getAbsAngleRad(p[7][0][1], p, next[7][0][1]) > (120 * D2R) &&
                    isCirInts(next, next2) )
                    ||
                    (error > 1.25 && cw &&
                    angledeg > 0 && nangledeg < 0 && n1angledeg > 0 && n2angledeg < 0 &&
                    next && next[7] && next[7][0][0] > 1.4 &&
                    next1 && next1[7] && next1[7].length > 1 &&
                    next2 && next2[7] && next2[7][0][0] > 1.7 &&
                    error < next[7][0][0] && next[7][0][0] < next2[7][0][0] &&
                    getAbsAngleRad(p[7][0][1], p, next[7][0][1]) > (120 * D2R) &&
                    isCirInts(next, next1) )
                  )
                ) {
                  window.segmentshapes.enabledebug && console.log('idx:', idx, 'enable29:', error);
                  segments.push(pl.slice(segmentstartidx, i+1));
                  segmentstartidx = i+3;
                  found = true;
                  
                  if(next2 && next2[4] == 4) {
                    let nearerror = next[7][0][1];
                    if(getDist(point, next1[7][0][1]) < getDist(point, next[7][0][1])) {
                      nearerror = next1[7][0][1];
                    }
                    let theta = getTangentAngleRad(point, nearerror) + M_PI2;
                    let linelength = p[2];
                    let pn = [linelength * Math.cos(theta) + p[0], linelength * Math.sin(theta) + p[1], p[2], 1, 8, theta];
                    
                    //let lastsegment = segments[segments.length-1];
                    //let px = lastsegment && lastsegment[lastsegment.length-1];
                    let px = prev;
                    if(px) {
                      let dist = getDist(px, pn);
                      let shrink = d3.interpolateNumberArray([px[0], px[1]], [pn[0], pn[1]])((dist-shrinkunit)/dist);
                      pn[0] = shrink[0];
                      pn[1] = shrink[1];
                    }
                      
                    segments[segments.length-1].push(pn);
                  }
                  else if(next3 && next3[4] == 4) {
                    // JS-Giat-Bold, ก
                    segmentstartidx++;
                    
                    let nearerror = next1[7][0][1];
                    if(getDist(point, next2[7][0][1]) < getDist(point, next1[7][0][1])) {
                      nearerror = next2[7][0][1];
                    }
                    let theta = getTangentAngleRad(point, nearerror) + M_PI2;
                    let linelength = p[2];
                    let pn = [linelength * Math.cos(theta) + p[0], linelength * Math.sin(theta) + p[1], p[2], 1, 8, theta];
                    
                    let px = prev;
                    if(px) {
                      let dist = getDist(px, pn);
                      let shrink = d3.interpolateNumberArray([px[0], px[1]], [pn[0], pn[1]])((dist-shrinkunit)/dist);
                      pn[0] = shrink[0];
                      pn[1] = shrink[1];
                    }

                    segments[segments.length-1].push(pn);
                  }
                  else if(prev1 && prev1[4] == 4) {
                    // JS-Giat-Bold, ฎ
                    segments.pop();
                  }
                  else {
                    let normalorder = !(next1[7].length > 1 && next2[7][0][0] > 1.7);
                    
                    let theta = getTangentAngleRad(point, normalorder? next1[7][0][1] : next2[7][0][1]) + M_PI2;
                    let linelength = p[2];
                    let pn = [linelength * Math.cos(theta) + p[0], linelength * Math.sin(theta) + p[1], p[2], 1, 8, theta];
                    
                    let lastsegment = segments[segments.length-1];
                    let px = lastsegment && lastsegment[lastsegment.length-1];
                    if(px) {
                      let dist = getDist(px, pn);
                      let shrink = d3.interpolateNumberArray([px[0], px[1]], [pn[0], pn[1]])((dist-shrinkunit)/dist);
                      pn[0] = shrink[0];
                      pn[1] = shrink[1];
                    }
                      
                    segments[segments.length-1].push(pn);
                                      
                    let p0, p1;
                    if(normalorder) {
                      if(next2[7].length > 1) {
                        p0 = next2[7][0][1];
                        p1 = next2[7][1][1];
                      }
                      else if(next3 && next3[7] && next3 != pl[lastidx] && getDist(next2, next3) < ((next2[2] + next3[2]) / 2)) {
                        p0 = next2[7][0][1];
                        p1 = next3[7][0][1];
                        segmentstartidx--;
                      }
                      else {
                        let t = getTangentAngleRad(next2[7][0][1], next[7][0][1]);
                        linelength = next2[2] * 2;
                        p0 = [linelength * Math.cos(t) + next2[7][0][1][0], linelength * Math.sin(t) + next2[7][0][1][1]];
                        p1 = next2[7][0][1];
                        segmentstartidx--;
                      }
                    }
                    else {
                      p0 = next1[7][0][1];
                      p1 = next1[7][1][1];
                    }
                    if(p0 && p1) {
                      let midx = (p0[0] + p1[0]) / 2;
                      let midy = (p0[1] + p1[1]) / 2;
                      let dx = p0[0] - p1[0];
                      let dy = p0[1] - p1[1];
                      let radius = Math.hypot(dx, dy) / 2;
                      let tangent = getTangentAngleRad(p0, p1) + M_PI2;
                      
                      let p8 = [midx, midy, radius, 1, 8, tangent];

                      let px = pl[segmentstartidx+1];
                      if(px) {
                        let dist = getDist(px, p8);
                        let shrink = d3.interpolateNumberArray([px[0], px[1]], [p8[0], p8[1]])((dist-shrinkunit)/dist);
                        p8[0] = shrink[0];
                        p8[1] = shrink[1];
                        
                      }

                      pl[segmentstartidx] = p8;
                    }
                  }
                  i = segmentstartidx;
                  i++;
                  while(pl[i] && pl[i][7]) {
                    i++;
                  }
                }

                // ก, ภ, ญ (invert direction)
                if(window.segmentshapes.enable30 && !found && !cw &&
                  error > 1.15 &&
                  next && next[7] && next[7][0][0] > 1.4 &&
                  next1 && next1[7] && next1[7][0][0] > 1.3 &&
                  next2 && next2[7] && next2[7][0][0] > 1.3 &&
                  error < next1[7][0][0] && error < next2[7][0][0] && next1[7][0][0] < next[7][0][0] && next2[7][0][0] < next[7][0][0] &&
                  getAbsAngleRad(next2[7][0][1], next2, next1[7][0][1]) > (120 * D2R) &&
                  isCirInts(p, next1) && isCirInts(next1, next2)
                ) {
                  window.segmentshapes.enabledebug && console.log('idx:', idx, 'enable30:', error);

                  let slicto = i;
                  let p0, p1;
                  if(p[7].length > 1) {
                    p0 = p[7][1][1];
                    p1 = p[7][0][1];
                  }
                  else if(prev && prev[7] && getDist(prev, p) < ((prev[2] + p[2]) / 2)) {
                    p0 = prev[7][0][1];
                    p1 =    p[7][0][1];
                    slicto--;
                  }
                  else {
                    let t = getTangentAngleRad(p[7][0][1], next1[7][0][1]);
                    let linelength = p[2] * 2;
                    p0 = p[7][0][1];
                    p1 = [linelength * Math.cos(t) + p[7][0][1][0], linelength * Math.sin(t) + p[7][0][1][1]];
                  }
                  
                  segments.push(pl.slice(segmentstartidx, slicto));
                  segmentstartidx = i+3;
                  found = true;

                  if(p0 && p1) {
                    let midx = (p0[0] + p1[0]) / 2;
                    let midy = (p0[1] + p1[1]) / 2;
                    let dx = p0[0] - p1[0];
                    let dy = p0[1] - p1[1];
                    let radius = Math.hypot(dx, dy) / 2;
                    let tangent = getTangentAngleRad(p0, p1) + M_PI2;
                    let p8 = [midx, midy, radius, 1, 8, tangent];

                    let lastsegment = segments[segments.length-1];
                    let px = lastsegment && lastsegment[lastsegment.length-1];
                    if(px) {
                      let dist = getDist(px, p8);
                      let shrink = d3.interpolateNumberArray([px[0], px[1]], [p8[0], p8[1]])((dist-shrinkunit)/dist);
                      p8[0] = shrink[0];
                      p8[1] = shrink[1];
                    }

                    segments[segments.length-1].push(p8);
                  }
                  
                  let theta = getTangentAngleRad(next[7][0][1], next2[7][0][1]) + M_PI2;
                  let linelength = -next2[2];
                  let pn = [linelength * Math.cos(theta) + next2[0], linelength * Math.sin(theta) + next2[1], next2[2], 1, 8, theta];

                  let px = pl[segmentstartidx];
                  if(px) {
                    let dist = getDist(px, pn);
                    let shrink = d3.interpolateNumberArray([px[0], px[1]], [pn[0], pn[1]])((dist-shrinkunit)/dist);
                    pn[0] = shrink[0];
                    pn[1] = shrink[1];
                  }

                  pl[--segmentstartidx] = pn;
                }
                
                // JS-Pitsamai-Italic: ภ ฎ, ณ
                if(window.segmentshapes.enable31 && !found && cw &&
                  (
                    (error > 1.07 &&
                    next && next[7] && next[7][0][0] > 1.4 &&
                    next1 && next1[7] && next1[7][0][0] > 2.5 &&
                    next2 && next2[4] == 4 &&
                    error < next[7][0][0] && next[7][0][0] < next1[7][0][0] &&
                    isCirInts(p, next) && isCirInts(next, next1) && isCirInts(next1, p) )
                  )
                  ||
                  (
                    (error > 1.1 &&
                    next && next[7] && next[7][0][0] > 3.5 &&
                    next1 && next1[7] && next1[7][0][0] > 1.4 &&
                    next2 && next2[7] && next2[7][0][0] > 2.0 &&
                    next3 && next3[4] == 4 &&
                    error < next1[7][0][0] && next1[7][0][0] < next2[7][0][0] && next2[7][0][0] < next[7][0][0] &&
                    isCirInts(p, next) && isCirInts(next, next1) && isCirInts(next1, p) )
                  )
                ) {
                  window.segmentshapes.enabledebug && console.log('idx:', idx, 'enable31:', error);
                  segments.push(pl.slice(segmentstartidx, i+1));
                  segmentstartidx = i+2;
                  found = true;
                }

                // Kanit-Black: u, b, n, p, d
                if(window.segmentshapes.enable32 && !found && angledeg < 0 && error > 1.25 && error < 3.0) {
                  if(
                    ( prev && !prev[7] && prev[4] == 1 && next && diffValue(p[2], next[2]) > 0.25 &&
                      (
                        (next && next[7] && next[7][0][0] > 1.08 && next[7][0][0] < 1.45 &&
                        next1 && next1[7] && next1[7][0][0] > 1.08 && next1[7][0][0] < 1.45 &&
                        next2 && next2[7] && next2[7][0][0] > 1.08 && next2[7][0][0] < 1.6 &&
                        isCirInts(p, next) && isCirInts(p, next1) && isCirInts(p, next2)
                        )
                        ||
                        (next && next[7] && next[7][0][0] > 1.08 && next[7][0][0] < 1.45 &&
                        next1 && next1[7] && next1[7].length == 2 &&
                        next1[7][0][0] > 1.08 && next1[7][0][0] < 1.45 &&
                        next1[7][1][0] > 1.08 && next1[7][1][0] < 1.6 &&
                        isCirInts(p, next) && isCirInts(p, next1)
                        )
                      )
                    )
                    ||
                    ( next && !next[7] && next[4] == 1 && prev && diffValue(p[2], prev[2]) > 0.25 &&
                      (
                        (prev && prev[7] && prev[7][0][0] > 1.08 && prev[7][0][0] < 1.45 &&
                        prev1 && prev1[7] && prev1[7][0][0] > 1.08 && prev1[7][0][0] < 1.6 &&
                        prev2 && prev2[7] && prev2[7][0][0] > 1.08 && prev2[7][0][0] < 1.6 &&
                        isCirInts(p, prev) && isCirInts(p, prev1) && isCirInts(p, prev2)
                        )
                        ||
                        (prev && prev[7] && prev[7][0][0] > 1.08 && prev[7][0][0] < 1.45 &&
                        prev1 && prev1[7] && prev1[7].length == 2 &&
                        prev1[7][0][0] > 1.08 && prev1[7][0][0] < 1.6 &&
                        prev1[7][1][0] > 1.08 && prev1[7][1][0] < 1.6 &&
                        isCirInts(p, prev) && isCirInts(p, prev1)
                        )
                      )
                    )
                  ) {
                    window.segmentshapes.enabledebug && console.log('idx:', idx, 'enable32:', error);
                    if(prev && !prev[7] && prev[4] == 1 && next1 && next1[7] && next2) {
                      segments.push(pl.slice(segmentstartidx, i+1));

                      if(next1[7].length == 2) {
                        segmentstartidx = i + 2;
                      }
                      else {
                        segmentstartidx = i + (next1[2] > next2[2]? 2 : 3);
                      }
                      found = true;

                      let theta = getTangentAngleRad(prev, p);
                      let linelength = p[2] / 2;
                      let p8 = [linelength * Math.cos(theta) + p[0], linelength * Math.sin(theta) + p[1], linelength, 1, 8, theta];

                      segments[segments.length-1].push(p8);
                    }
                    if(next && !next[7] && next[4] == 1 && prev1 && prev1[7] && prev2) {
                      let sliceto = i;
                      if(prev1[7].length == 2) {
                        sliceto = i - 2;
                      }
                      else {
                        sliceto = i - (prev1[2] > prev2[2]? 2 : 3);
                      }
                      segments.push(pl.slice(segmentstartidx, sliceto + 1));
                      segmentstartidx = i;
                      found = true;

                      let theta = getTangentAngleRad(p, prev);
                      let linelength = p[2] / 2;
                      let p8 = [linelength * Math.cos(theta) + p[0], linelength * Math.sin(theta) + p[1], linelength, 1, 8, theta];

                      pl[i-1] = p8;
                    }
                  }
                }
                
                // JS-Angsumalin: ฑ (before: enable1, enable2), Taviraj-MediumItalic: h, Trirong-ExtraBoldItalic: h, Trirong-BlackItalic: m
                if(window.segmentshapes.enable14 && !found && angledeg < 0 && error > 3.4 && prev && prev[7] && prev[7].length == 2 && prev[7][0][0] > 1.24 && prev[7][1][0] > 1.4 && isCirInts(p, prev)) {
                  window.segmentshapes.enabledebug && console.log('idx:', idx, 'enable14:', error);
                  segments.push(pl.slice(segmentstartidx, i));
                  segmentstartidx = i;
                  found = true;

                  let p0 = prev[7][1][1];
                  let p1 = prev[7][0][1];
                  let midx = (p0[0] + p1[0]) / 2;
                  let midy = (p0[1] + p1[1]) / 2;
                  let dx = p0[0] - p1[0];
                  let dy = p0[1] - p1[1];
                  let radius = Math.hypot(dx, dy) / 2;
                  let tangent = getTangentAngleRad(p0, p1) + M_PI2;

                  let p8_0 = [midx, midy, radius, 1, 8, tangent];

                  let lastsegment = segments[segments.length-1];
                  let px = lastsegment && lastsegment[lastsegment.length-1];
                  if(px) {
                    let dist = getDist(px, p8_0);
                    let shrink = d3.interpolateNumberArray([px[0], px[1]], [p8_0[0], p8_0[1]])((dist-shrinkunit)/dist);
                    p8_0[0] = shrink[0];
                    p8_0[1] = shrink[1];
                  }

                  segments[segments.length-1].push(p8_0);

                  let p8_1 = [midx, midy, radius, 1, 8, tangent - Math.PI];

                  segmentstartidx--;

                  px = pl[segmentstartidx+1];
                  if(px) {
                    let dist = getDist(px, p8_1);
                    let shrink = d3.interpolateNumberArray([px[0], px[1]], [p8_1[0], p8_1[1]])((dist-shrinkunit)/dist);
                    p8_1[0] = shrink[0];
                    p8_1[1] = shrink[1];
                  }

                  pl[segmentstartidx] = p8_1;
                }

                // Kt-smarn-Smooth: ฐ
                if(window.segmentshapes.enable27 && !found && angledeg < 0 && error > 2.6 && prev && prev[4] == 4 && next && next[7] && next[7].length == 2 && next[7][0][0] > 1.4 && next[7][1][0] > 1.4 && isCirInts(p, next)) {
                  window.segmentshapes.enabledebug && console.log('idx:', idx, 'enable27:', error);
                  segments.push(pl.slice(segmentstartidx, i+1));
                  segmentstartidx = i+2;
                  found = true;

                  let p0 = next[7][0][1];
                  let p1 = next[7][1][1];
                  let midx = (p0[0] + p1[0]) / 2;
                  let midy = (p0[1] + p1[1]) / 2;
                  let dx = p0[0] - p1[0];
                  let dy = p0[1] - p1[1];
                  let radius = Math.hypot(dx, dy) / 2;
                  let tangent = getTangentAngleRad(p0, p1) + M_PI2;

                  if(i > 0 && diffAngleRad(tangent, getTangentAngleRad(prev, p)) > (60 * D2R)) {
                    tangent -= Math.PI;
                  }

                  let p8 = [midx, midy, radius, 1, 8, tangent];

                  let lastsegment = segments[segments.length-1];
                  let px = lastsegment && lastsegment[lastsegment.length-1];
                  if(px) {
                    let dist = getDist(px, p8);
                    let shrink = d3.interpolateNumberArray([px[0], px[1]], [p8[0], p8[1]])((dist-shrinkunit)/dist);
                    p8[0] = shrink[0];
                    p8[1] = shrink[1];
                  }

                  segments[segments.length-1].push(p8);
                }

                //  Taviraj-ExtraBold: ส
                if(window.segmentshapes.enable28 && !found && angledeg < 0 && error > 2.2 && prev2 && prev2[4] == 4 && next && next[7] && next[7].length == 2 && next[7][0][0] > 1.4 && next[7][1][0] > 1.4 && isCirInts(p, next)) {
                  window.segmentshapes.enabledebug && console.log('idx:', idx, 'enable28:', error);
                  segments.push(pl.slice(segmentstartidx, i+1));
                  segmentstartidx = i+2;
                  found = true;

                  let p0 = next[7][0][1];
                  let p1 = next[7][1][1];
                  let midx = (p0[0] + p1[0]) / 2;
                  let midy = (p0[1] + p1[1]) / 2;
                  let dx = p0[0] - p1[0];
                  let dy = p0[1] - p1[1];
                  let radius = Math.hypot(dx, dy) / 2;
                  let tangent = getTangentAngleRad(p0, p1) + M_PI2;

                  if(i > 0 && diffAngleRad(tangent, getTangentAngleRad(prev, p)) > (60 * D2R)) {
                    tangent -= Math.PI;
                  }

                  let p8 = [midx, midy, radius, 1, 8, tangent];

                  let lastsegment = segments[segments.length-1];
                  let px = lastsegment && lastsegment[lastsegment.length-1];
                  if(px) {
                    let dist = getDist(px, p8);
                    let shrink = d3.interpolateNumberArray([px[0], px[1]], [p8[0], p8[1]])((dist-shrinkunit)/dist);
                    p8[0] = shrink[0];
                    p8[1] = shrink[1];
                  }

                  segments[segments.length-1].push(p8);
                }


                if(window.segmentshapes.enable1 && !found && angledeg < 0 && error > 1.65 && error < 20.0) {
                  let condition = true;

                  if(condition) {
                    //condition = prev && next && (prev[2] * 0.95) > p[2] && p[2] < (next[2] * 0.95);
                    condition = prev && next && prev[2] > p[2] && p[2] < next[2];
                  }

                  // Prompt-BlackItalic: k
                  // Zen-Kaku-Gothic-New-Regular: k, pl[1][7][0][0] > 2.45
                  if(condition && ilen == 4 && pl[0][4] == 4 && pl[3][4] == 4 && pl[1][7] && pl[2][7] && pl[1][7][0][0] > 2.45 && pl[2][7][0][0] > 2.5) {
                    condition = false;
                  }
                  
                  // ZCOOL-XiaoWei-Regular: 4
                  if(condition && ilen == 4 && pl[0][4] == 4 && pl[3][4] == 4 && pl[1][7] && pl[2][7] && pl[1][7][0][0] > 1.4 && pl[1][7][0][0] < 1.43 && pl[2][7][0][0] > 1.6 && pl[2][7][0][0] < 1.8) {
                    condition = false;
                  }
                  
                  // Kt-smarn-Cartoon: k error 2.5, x error 1.9-2.0
                  if(condition && error > 1.9 && error < 3.0 && ilen == 3 && ((!prev[7] && prev[4] == 4) || (!next[7] && next[4] == 4))) {
                    condition = false;
                  }

                  // in case 'x' shape, do not segments
                  // if prev point is branch
                  if(condition && ilen >= 3 && prev[4] == 4 && getDist(p, prev) <= ((p[2] + prev[2]) * 0.75)) {
                    condition = false;
                    // prove, if remove branch from line segment, terminal line will be hit polygon?
                    let r = Math.max(p[2], prev[2]);
                    let dist = getDist(next, p);
                    let pi = d3.interpolateNumberArray(next.slice(0, 2), p.slice(0, 2))((dist + r*2.0)/dist);
                    let line = [p, pi];
                    let intersect;
                    let tangent = getTangentAngleDeg(next, p);
                    for(const polygon of polygons.toReversed()) {
                      intersect = getPointLineIntersectPolygon(line, polygon);
                      if(intersect) {
                        let diff = diffAngleDeg(tangent, getTangentAngleRad(intersect[0], intersect[1]));
                        if(diff > 40 && diff < 140) {
                          condition = true;
                          break;
                        }
                      }
                    }
                  }

                  // in case 'x' shape, do not segments
                  // if next point is branch
                   if(condition && ilen >= 3 && next[4] == 4 && getDist(p, next) <= ((p[2] + next[2]) * 0.75)) {
                    condition = false;
                    // prove, if remove branch from line segment, terminal line will be hit polygon?
                    let r = Math.max(p[2], next[2]);
                    let dist = getDist(prev, p);
                    let pi = d3.interpolateNumberArray(prev.slice(0, 2), p.slice(0, 2))((dist + r*2.0)/dist);
                    let line = [p, pi];
                    let intersect;
                    let tangent = getTangentAngleDeg(prev, p);
                    for(const polygon of polygons.toReversed()) {
                      intersect = getPointLineIntersectPolygon(line, polygon);
                      if(intersect) {
                        let diff = diffAngleDeg(tangent, getTangentAngleRad(intersect[0], intersect[1]));
                        if(diff > 40 && diff < 140) {
                          condition = true;
                          break;
                        }
                      }
                    }
                  }

                  if(condition) {
                    let pr;
                    let prerror;
                    let error0;
                    let prangledeg = 0;
                    for(const pt of [prev, prev1, prev2]) {
                      if(pt) {
                        let isintersect = getDist(next, pt) < (next[2]*1.25 + pt[2]*1.25);
                        if(!isintersect) {
                          break;
                        }
                        prerror = pt[7];
                        if(prerror && prerror.length) {
                          error0 = prerror.toSorted((a,b) => b[0]-a[0])[0];
                          pr = pt;
                          let target = error0[1];
                          if(target && target[7] && target[7][0] && target[7][0][2]) {
                            prangledeg = target[7][0][2];
                          }
                          if(prangledeg < 0) {
                            error0 = undefined;
                            pr = undefined;
                          }
                          break;
                        }
                      }
                    }

                    let nx;
                    let nxerror;
                    let error1;
                    let nxangledeg = 0;
                    for(const pt of [next, next1, next2]) {
                      if(pt) {
                        let isintersect = getDist(prev, pt) < (prev[2]*1.25 + pt[2]*1.25);
                        if(!isintersect) {
                          break;
                        }
                        nxerror = pt[7];
                        if(nxerror && nxerror.length) {
                          error1 = nxerror.toSorted((a,b) => b[0]-a[0])[0];
                          nx = pt;
                          let target = error1[1];
                          if(target && target[7] && target[7][0] && target[7][0][2] < 0) {
                            nxangledeg = target[7][0][2];
                          }
                          if(nxangledeg < 0) {
                            error1 = undefined;
                            nx = undefined;
                          }
                          break;
                        }
                      }
                    }
                    
                    if(pl[ilen - 1] == nx) {
                      condition = false;
                    }
                    
                    if(pl[segmentstartidx] == pr) {
                      condition = false;
                    }
                    
                    if(condition && ((pr && error0) || (nx && error1)) && segmentstartidx < i) {
                      // not segment, if branch associated with line loop
                      if(condition && !prev[7] && prev[4] == 4) {
                        let branch = prev;
                        condition = !polys.some(poly => (branch == poly[0] && branch == poly[poly.length-1]) );
                      }
                      if(condition && !next[7] && next[4] == 4) {
                        let branch = next;
                        condition = !polys.some(poly => (branch == poly[0] && branch == poly[poly.length-1]) );
                      }

                      
                      if(condition && pr && error0) {
                        let angledeg = getAbsAngleDeg(error0[1], pr, p);
                        condition = 65 < angledeg && angledeg < 110;
                        //if(!condition) {
                          //segments.push(pl.slice(segmentstartidx, i).filter(v => v != p));
                          //segmentstartidx = i+1;
                          //console.debug(window.strdoing || '', 'skip enable1 condition0', 'polygon:', idx, 'i:', i, error0[0], error);
                        //}
                      }
                      
                      if(condition && nx && error1) {
                        let angledeg = getAbsAngleDeg(error1[1], nx, p);
                        condition = 65 < angledeg && angledeg < 110;
                        //if(!condition) {
                          //segments.push(pl.slice(segmentstartidx, i+2).filter(v => v != p));
                          //segmentstartidx = i+2;
                          //console.debug(window.strdoing || '', 'skip enable1 condition1', 'polygon:', idx, 'i:', i, error1[0], error);
                        //}
                      }
                      
                    }
                    
                    if(condition && (pr || nx) && segmentstartidx < i) {
                      window.segmentshapes.enabledebug && console.log('idx:', idx, 'enable1:', error);
                      
                      // fixed: JS-Oobboon-Normal: 7
                      let slicto = i;
                      if(prangledeg < 0) {
                        slicto++;
                      }
                      
                      segments.push(pl.slice(segmentstartidx, slicto));
                      segmentstartidx = i+1;
                      
                      // fixed: JS-Oobboon-Normal: 7
                      if(nxangledeg < 0) {
                        segmentstartidx--;
                      }
                      
                      let branchprev = null;
                      let branchnext = null;
                      if(!prev[7] && prev[4] == 4) {
                        branchprev = prev;
                        //console.log('branch: prev');
                      }
                      if(!next[7] && next[4] == 4) {
                        branchnext = next;
                        //console.log('branch: next');
                      }
                      found = true;

                      let tangent;

                      if(window.segmentshapes.enable2 && error0 && error0.length) {
                        const theta = getTangentAngleRad(prev, p);
                        const linelength = prev[2] * 2.5;
                        let pn = [linelength * Math.cos(theta) + p[0], linelength * Math.sin(theta) + p[1]];
                        let line = [p, pn];
                        let intersect;
                        let intersects = [];
                        for(const polygon of polygons.toReversed()) {
                          intersect = getPointLineIntersectPolygon(line, polygon);
                          if(intersect) {
                            intersects.push(intersect);
                          }
                        }
                        let minIdx = d3.minIndex(intersects, intersect => getDist(p, intersect[1]));
                        if(minIdx > -1) {
                          intersect = intersects[minIdx];
                          const p0 = error0[1];
                          if(getDist(p0, intersect[0]) < 0.05 || getDist(p0, intersect[2]) < 0.05) {
                            const p1 = intersect[1];
                            const midx = (p0[0] + p1[0]) / 2;
                            const midy = (p0[1] + p1[1]) / 2;
                            const dx = p0[0] - p1[0];
                            const dy = p0[1] - p1[1];
                            const radius = Math.hypot(dx, dy) / 2;
                            tangent = getTangentAngleRad(p0, p1) - M_PI2;
                            if(i > 1 && diffAngleRad(tangent, getTangentAngleRad(pl[i-2], prev)) > (90 * D2R)) {
                              tangent -= Math.PI;
                            }

                            const p8 = [midx, midy, radius, 1, 8, tangent];
                            
                            let lastsegment = segments[segments.length-1];
                            let px = lastsegment && lastsegment[lastsegment.length-1];
                            if(px) {
                              let dist = getDist(px, p8);
                              let shrink = d3.interpolateNumberArray([px[0], px[1]], [p8[0], p8[1]])((dist-shrinkunit)/dist);
                              p8[0] = shrink[0];
                              p8[1] = shrink[1];
                            }

                            segments[segments.length-1].push(p8);

                            branchprev = null;
                          }
                        }
                      }

                      if(window.segmentshapes.enable3 && error1 && error1.length) {
                        const theta = getTangentAngleRad(next, p);
                        const linelength = next[2] * 2.5;
                        let pn = [linelength * Math.cos(theta) + p[0], linelength * Math.sin(theta) + p[1]];
                        let line = [p, pn];
                        let intersect;
                        let intersects = [];
                        for(const polygon of polygons.toReversed()) {
                          intersect = getPointLineIntersectPolygon(line, polygon);
                          if(intersect) {
                            intersects.push(intersect);
                          }
                        }
                        let minIdx = d3.minIndex(intersects, intersect => getDist(p, intersect[1]));
                        if(minIdx > -1) {
                          intersect = intersects[minIdx];
                          const p0 = error1[1];
                          if(getDist(p0, intersect[0]) < 0.05 || getDist(p0, intersect[2]) < 0.05) {
                            const p1 = intersect[1];
                            const midx = (p0[0] + p1[0]) / 2;
                            const midy = (p0[1] + p1[1]) / 2;
                            const dx = p0[0] - p1[0];
                            const dy = p0[1] - p1[1];
                            const radius = Math.hypot(dx, dy) / 2;

                            tangent = getTangentAngleRad(p0, p1) - M_PI2;
                            if(i < pl.length-2 && diffAngleRad(tangent, getTangentAngleRad(next, pl[i+2])) > (90 * D2R)) {
                              tangent -= Math.PI;
                            }

                            const p8 = [midx, midy, radius, 1, 8, tangent];

                            if(nx == next) {
                              segmentstartidx--;
                              
                              let px = pl[segmentstartidx+1];
                              if(px) {
                                let dist = getDist(px, p8);
                                let shrink = d3.interpolateNumberArray([px[0], px[1]], [p8[0], p8[1]])((dist-shrinkunit)/dist);
                                p8[0] = shrink[0];
                                p8[1] = shrink[1];
                              }
                              
                              pl[segmentstartidx] = p8;
                              branchnext = null;
                            }
                            else if(nx == next1) {
                              //segmentstartidx--;
                              
                              let px = pl[segmentstartidx+1];
                              if(px) {
                                let dist = getDist(px, p8);
                                let shrink = d3.interpolateNumberArray([px[0], px[1]], [p8[0], p8[1]])((dist-shrinkunit)/dist);
                                p8[0] = shrink[0];
                                p8[1] = shrink[1];
                              }
                              
                              pl[segmentstartidx] = p8;
                              branchnext = null;
                            }
                            else if(nx == next2) {
                              segmentstartidx++;
                              
                              let px = pl[segmentstartidx+1];
                              if(px) {
                                let dist = getDist(px, p8);
                                let shrink = d3.interpolateNumberArray([px[0], px[1]], [p8[0], p8[1]])((dist-shrinkunit)/dist);
                                p8[0] = shrink[0];
                                p8[1] = shrink[1];
                              }
                              
                              pl[segmentstartidx] = p8;
                              branchnext = null;
                            }
                          }
                        }
                      }

                      if(branchprev) {
                        branchprev[3]--;
                        if(branchprev[3] > 2)
                          branchprev[4] = 4;
                        else if(branchprev[3] > 1)
                          branchprev[4] = 2;
                        else
                          branchprev[4] = 1;
                        branches.push(branchprev);
                      }

                      if(branchnext) {
                        branchnext[3]--;
                        if(branchnext[3] > 2)
                          branchnext[4] = 4;
                        else if(branchnext[3] > 1)
                          branchnext[4] = 2;
                        else
                          branchnext[4] = 1;
                        branches.push(branchnext);
                      }

                    }
                  }
                }

                // Fahkwang-SemiBold: r, Trirong-BlackItalic: r
                if(window.segmentshapes.enable15 && !found && angledeg < 0 && error > 3.5 && next && next[7] && next[7].length == 2 && next[7][0][0] > 1.24 && next[7][1][0] > 1.4 && isCirInts(p, next)) {
                  window.segmentshapes.enabledebug && console.log('idx:', idx, 'enable15:', error);
                  segments.push(pl.slice(segmentstartidx, i+1));
                  segmentstartidx = i+2;
                  found = true;

                  let p0 = next[7][1][1];
                  let p1 = next[7][0][1];
                  let midx = (p0[0] + p1[0]) / 2;
                  let midy = (p0[1] + p1[1]) / 2;
                  let dx = p0[0] - p1[0];
                  let dy = p0[1] - p1[1];
                  let radius = Math.hypot(dx, dy) / 2;
                  let tangent = getTangentAngleRad(p0, p1) + M_PI2;
                  
                  if(prev && diffAngleRad(tangent, getTangentAngleRad(prev, p) > (90 * D2R))) {
                    tangent -= Math.PI;
                  }

                  let p8_0 = [midx, midy, radius, 1, 8, tangent];

                  let lastsegment = segments[segments.length-1];
                  let px = lastsegment && lastsegment[lastsegment.length-1];
                  if(px) {
                    let dist = getDist(px, p8_0);
                    let shrink = d3.interpolateNumberArray([px[0], px[1]], [p8_0[0], p8_0[1]])((dist-shrinkunit)/dist);
                    p8_0[0] = shrink[0];
                    p8_0[1] = shrink[1];
                  }

                  segments[segments.length-1].push(p8_0);

                  let p8_1 = [midx, midy, radius, 1, 8, tangent - Math.PI];

                  segmentstartidx--;

                  px = pl[segmentstartidx+1];
                  if(px) {
                    let dist = getDist(px, p8_1);
                    let shrink = d3.interpolateNumberArray([px[0], px[1]], [p8_1[0], p8_1[1]])((dist-shrinkunit)/dist);
                    p8_1[0] = shrink[0];
                    p8_1[1] = shrink[1];
                  }

                  pl[segmentstartidx] = p8_1;
                }

                // Kt-smarn-Basic: u, Anakotmai-Bold: r
                if(window.segmentshapes.enable16 && !found && angledeg < 0 && error > 3.0 && next && next[7] && next[7].length == 2 && next[7][0][0] > 1.4 && next[7][1][0] > 1.4 && isCirInts(p, next)) {
                  window.segmentshapes.enabledebug && console.log('idx:', idx, 'enable16:', error);
                  segments.push(pl.slice(segmentstartidx, i+1));
                  segmentstartidx = i+2;
                  found = true;

                  let p0 = next[7][0][1];
                  let p1 = next[7][1][1];
                  let midx = (p0[0] + p1[0]) / 2;
                  let midy = (p0[1] + p1[1]) / 2;
                  let dx = p0[0] - p1[0];
                  let dy = p0[1] - p1[1];
                  let radius = Math.hypot(dx, dy) / 2;
                  let tangent = getTangentAngleRad(p0, p1) + M_PI2;

                  if(i > 0 && diffAngleRad(tangent, getTangentAngleRad(prev, p)) > (60 * D2R)) {
                    tangent -= Math.PI;
                  }

                  let p8_0 = [midx, midy, radius, 1, 8, tangent];
                  
                  let lastsegment = segments[segments.length-1];
                  let px = lastsegment && lastsegment[lastsegment.length-1];
                  if(px) {
                    let dist = getDist(px, p8_0);
                    let shrink = d3.interpolateNumberArray([px[0], px[1]], [p8_0[0], p8_0[1]])((dist-shrinkunit)/dist);
                    p8_0[0] = shrink[0];
                    p8_0[1] = shrink[1];
                  }
                  
                  segments[segments.length-1].push(p8_0);
                }

                // Kt-smarn-Basic: k, JS-Kobori-Allcaps-Bold: k
                if(window.segmentshapes.enable17 && !found && angledeg < 0 && error > 2.7 && prev && prev[7] && prev[7][0][0] > 1.4 && isCirInts(p, prev) && next && next[7] && next[7][0][0] > 1.4 && isCirInts(p, next) &&
                  // fixed bug: on JS-Saksit-Bold: 'ฟ' by adding more details about 'k'
                  prev1 && prev1[7] && prev1[7].length == 2 && prev1[7][0][0] > 1.4 && prev1[7][1][0] > 1.4 ) {
                  let lp = pl[pl.length-1];
                  // adding more details about 'k'
                  if(lp[4] == 4 && pl.length - i < 5) {
                    window.segmentshapes.enabledebug && console.log('idx:', idx, 'enable17:', error);
                    let p0 = prev1[7][0][1];
                    let p1 = prev1[7][1][1];
                    let midx = (p0[0] + p1[0]) / 2;
                    let midy = (p0[1] + p1[1]) / 2;
                    let dx = p0[0] - p1[0];
                    let dy = p0[1] - p1[1];
                    let radius = Math.hypot(dx, dy) / 2;
                    let tangent = getTangentAngleRad(p0, p1) - M_PI2;

                    let p8_0 = [midx, midy, radius, 1, 8, tangent];
                    
                    let px = pl[segmentstartidx];
                    if(px) {
                      let dist = getDist(px, p8_0);
                      let shrink = d3.interpolateNumberArray([px[0], px[1]], [p8_0[0], p8_0[1]])((dist-shrinkunit)/dist);
                      p8_0[0] = shrink[0];
                      p8_0[1] = shrink[1];
                    }

                    p0 = prev[7][0][1];
                    p1 = next[7][0][1];
                    midx = (p0[0] + p1[0]) / 2;
                    midy = (p0[1] + p1[1]) / 2;
                    dx = p0[0] - p1[0];
                    dy = p0[1] - p1[1];
                    radius = Math.hypot(dx, dy) / 2;
                    tangent = getTangentAngleRad(p0, p1) + M_PI2;

                    let p8_1 = [midx, midy, radius, 1, 8, tangent];
                    
                    px = pl[i-1];
                    if(px) {
                      let dist = getDist(px, p8_1);
                      let shrink = d3.interpolateNumberArray([px[0], px[1]], [p8_1[0], p8_1[1]])((dist-shrinkunit)/dist);
                      p8_1[0] = shrink[0];
                      p8_1[1] = shrink[1];
                    }

                    if(diffAngleRad(p8_0[5], p8_1[5]) > (90 * D2R)) {
                      p8_0[5] -= M_1PI;
                    }

                    segments.push([p8_0, ...pl.slice(segmentstartidx, i), p8_1]);
                    segmentstartidx = i+3;
                    found = true;

                    let branch = lp;
                    let lines = polys.filter(poly => ((poly != pl) && (branch == poly[0] || branch == poly[poly.length-1])) );

                    if(lines.length == 2 && lines[0][0] != branch && lines[1][0] != branch) {
                      lines[0].push(...(lines[1].toReversed().slice(1)))
                      lines[1].length = 0;
                      branch[2] = lines[0][0][2];
                      branch[3] = 1;
                      branch[4] = 8;
                      branch[5] = tangent - Math.PI;
                    }
                  }
                }

                // JS-Saksit-Bold: 'ฟ' (error > 6.9)
                if(window.segmentshapes.enable18 && !found && angledeg < 0 && error > 4.0 && prev && prev[7] && prev[7][0][0] > 1.4 && isCirInts(p, prev) && next && next[7] && next[7][0][0] > 1.4 && isCirInts(p, next) ) {
                  window.segmentshapes.enabledebug && console.log('idx:', idx, 'enable18:', error);
                  segments.push(pl.slice(segmentstartidx, i));
                  segmentstartidx = i+1;
                  found = true;
                }

                // Fahkwang-Regular, Fahkwang-SemiBoldItalic: ส
                if(window.segmentshapes.enable19 && !found && angledeg < 0 && error > 3.3 && next && next[7] && next[7].length == 2 && next[7][0][0] > 1.3 && next[7][1][0] > 1.3 && isCirInts(p, next)) {
                  window.segmentshapes.enabledebug && console.log('idx:', idx, 'enable19:', error);
                  let p0 = next[7][1][1];
                  let p1 = next[7][0][1];
                  let midx = (p0[0] + p1[0]) / 2;
                  let midy = (p0[1] + p1[1]) / 2;
                  let dx = p0[0] - p1[0];
                  let dy = p0[1] - p1[1];
                  let radius = Math.hypot(dx, dy) / 2;
                  let tangent = getTangentAngleRad(p0, p1) + M_PI2;

                  if(i > 0 && diffAngleRad(tangent, getTangentAngleRad(prev, p)) > (60 * D2R)) {
                    tangent -= Math.PI;
                  }

                  let p8 = [midx, midy, radius, 1, 8, tangent];
                  
                  let px = pl[i];
                  if(px) {
                    let dist = getDist(px, p8);
                    let shrink = d3.interpolateNumberArray([px[0], px[1]], [p8[0], p8[1]])((dist-shrinkunit)/dist);
                    p8[0] = shrink[0];
                    p8[1] = shrink[1];
                  }
                  
                  segments.push([...pl.slice(segmentstartidx, i+1), p8]);
                  segmentstartidx = i+1;
                  found = true;
                }

                // Fahkwang-SemiBold: ส , Trirong-Regular: ผ, ฝ
                if(window.segmentshapes.enable20 && !found && angledeg < 0 && error > 3.5 && prev && prev[7] && prev[7][0][0] > 1.3 && isCirInts(p, prev) && next && next[7] && next[7][0][0] > 1.3 && isCirInts(p, next)) {
                  window.segmentshapes.enabledebug && console.log('idx:', idx, 'enable20:', error);
                  let p0 = prev[7][0][1];
                  let p1 = next[7][0][1];
                  let midx = (p0[0] + p1[0]) / 2;
                  let midy = (p0[1] + p1[1]) / 2;
                  let dx = p0[0] - p1[0];
                  let dy = p0[1] - p1[1];
                  let radius = Math.hypot(dx, dy) / 2;
                  let tangent = getTangentAngleRad(p0, p1) + M_PI2;

                  if(prev1 && diffAngleRad(tangent, getTangentAngleRad(prev1, prev)) > (60 * D2R)) {
                    tangent -= Math.PI;
                  }

                  let p8 = [midx, midy, radius, 1, 8, tangent];

                  let px = pl[i];
                  if(px) {
                    let dist = getDist(px, p8);
                    let shrink = d3.interpolateNumberArray([px[0], px[1]], [p8[0], p8[1]])((dist-shrinkunit)/dist);
                    p8[0] = shrink[0];
                    p8[1] = shrink[1];
                  }

                  segments.push([...pl.slice(segmentstartidx, i+1), p8]);
                  segmentstartidx = i+1;
                  found = true;
                }

                // NotoSerifThai-Condensed-Black: ส
                if(window.segmentshapes.enable21 && !found && angledeg < 0 && error > 2.4 && next && next[7] && next[7].length == 3 && next[7][0][0] > 1.0 && next[7][1][0] > 1.1 && next[7][2][0] > 1.4 && isCirInts(p, next)) {
                  window.segmentshapes.enabledebug && console.log('idx:', idx, 'enable21:', error);
                  let p0 = next[7][1][1];
                  let p1 = next[7][2][1];
                  let midx = (p0[0] + p1[0]) / 2;
                  let midy = (p0[1] + p1[1]) / 2;
                  let dx = p0[0] - p1[0];
                  let dy = p0[1] - p1[1];
                  let radius = Math.hypot(dx, dy) / 2;
                  let tangent = getTangentAngleRad(p0, p1) + M_PI2;

                  let p8 = [midx, midy, radius, 1, 8, tangent];

                  let px = pl[i];
                  if(px) {
                    let dist = getDist(px, p8);
                    let shrink = d3.interpolateNumberArray([px[0], px[1]], [p8[0], p8[1]])((dist-shrinkunit)/dist);
                    p8[0] = shrink[0];
                    p8[1] = shrink[1];
                  }

                  segments.push([...pl.slice(segmentstartidx, i+1), p8]);
                  segmentstartidx = i+1;
                  found = true;
                }

                // JS-Kobori-Allcaps-Bold: N
                if(window.segmentshapes.enable33 && !found && angledeg < 0) {
                  if(error > 4 &&
                    next &&  next[7] &&  next[7][0][0] > 1.2 &&  next[7][0][0] < 1.45 &&
                    next1 && next1[7] && next1[7][0][0] > 1.2 && next1[7][0][0] < 1.45 &&
                    isCirInts(p, next) && isCirInts(next, next1) && isCirInts(next1, p)
                  ) {
                    window.segmentshapes.enabledebug && console.log('idx:', idx, 'enable33-0:', error);
                    segments.push(pl.slice(segmentstartidx, i+1));
                    segmentstartidx = i+2;
                    // JS-Kobori-Allcaps-Bold: สระอี
                    if(pl[segmentstartidx] && pl[segmentstartidx][4] == 2) {
                      pl[segmentstartidx][3] = pl[segmentstartidx][4] = 1;
                    }
                    found = true;
                    
                  }
                  else if(error > 4 &&
                    prev && prev[7] && prev[7][0][0] > 1.2 && prev[7][0][0] < 1.45 &&
                    next && next[7] && next[7][0][0] > 1.2 && next[7][0][0] < 1.45 &&
                    isCirInts(prev, p) && isCirInts(p, next) && isCirInts(next, prev)
                  ) {
                    window.segmentshapes.enabledebug && console.log('idx:', idx, 'enable33-1:', error);
                    segments.push(pl.slice(segmentstartidx, i));
                    segmentstartidx = i+1;
                    // JS-Kobori-Allcaps-Bold: สระอี
                    if(pl[segmentstartidx] && pl[segmentstartidx][4] == 2) {
                      pl[segmentstartidx][3] = pl[segmentstartidx][4] = 1;
                    }
                    found = true;
                    
                  }
                  else if(error > 4 &&
                     prev &&  prev[7] &&  prev[7][0][0] > 1.2 &&  prev[7][0][0] < 1.45 &&
                    prev1 && prev1[7] && prev1[7][0][0] > 1.2 && prev1[7][0][0] < 1.45 &&
                    isCirInts(prev1, prev) && isCirInts(prev, p) && isCirInts(p, prev1)
                  ) {
                    window.segmentshapes.enabledebug && console.log('idx:', idx, 'enable33-2:', error);
                    segments.push(pl.slice(segmentstartidx, i-1));
                    segmentstartidx = i;
                    found = true;
                    
                  }
                  else if(error > 1.95 &&
                     prev &&  prev[7] &&  prev[7][0][0] > 1.4 &&   prev[7][0][0] < 1.42 &&
                    prev1 && prev1[7] && prev1[7][0][0] > 1.05 && prev1[7][0][0] < 1.10 &&
                    isCirInts(prev1, prev) && isCirInts(prev, p) && isCirInts(p, prev1)
                  ) {
                    // JS-Karabow: ณ
                    window.segmentshapes.enabledebug && console.log('idx:', idx, 'enable33-3:', error);
                    segments.push(pl.slice(segmentstartidx, i-1));
                    segmentstartidx = i;
                    found = true;
                    
                  }
                }

                // JS-Angsumalin: ค (before: enable4, enable5)
                if(window.segmentshapes.enable13 && !found && angledeg < 0 && error > 3.4 && prev && prev[7] && prev[7].length == 2 && prev[7][0][0] > 1.4 && prev[7][1][0] > 1.4 && isCirInts(p, prev)) {
                  window.segmentshapes.enabledebug && console.log('idx:', idx, 'enable13:', error);
                  segments.push(pl.slice(segmentstartidx, i));
                  segmentstartidx = i;
                  found = true;

                  let p0 = prev[7][0][1];
                  let p1 = prev[7][1][1];
                  let midx = (p0[0] + p1[0]) / 2;
                  let midy = (p0[1] + p1[1]) / 2;
                  let dx = p0[0] - p1[0];
                  let dy = p0[1] - p1[1];
                  let radius = Math.hypot(dx, dy) / 2;
                  let tangent = getTangentAngleRad(p0, p1) - M_PI2;
                  let p8 = [midx, midy, radius, 1, 8, tangent];

                  let lastsegment = segments[segments.length-1];
                  let px = lastsegment && lastsegment[lastsegment.length-1];
                  if(px) {
                    let dist = getDist(px, p8);
                    let shrink = d3.interpolateNumberArray([px[0], px[1]], [p8[0], p8[1]])((dist-shrinkunit)/dist);
                    p8[0] = shrink[0];
                    p8[1] = shrink[1];
                  }

                  segments[segments.length-1].push(p8);

                  p0 = p8;
                  p1 = prev[7][0][1];
                  midx = (p0[0] + p1[0]) / 2;
                  midy = (p0[1] + p1[1]) / 2;
                  dx = p0[0] - p1[0];
                  dy = p0[1] - p1[1];
                  radius = Math.hypot(dx, dy) / 2;
                  //tangent += Math.PI;
                  p8 = [midx, midy, radius, 1, 8, tangent + Math.PI];
                  segmentstartidx--;

                  px = pl[segmentstartidx+1];
                  if(px) {
                    let dist = getDist(px, p8);
                    let shrink = d3.interpolateNumberArray([px[0], px[1]], [p8[0], p8[1]])((dist-shrinkunit)/dist);
                    p8[0] = shrink[0];
                    p8[1] = shrink[1];
                  }

                  pl[segmentstartidx] = p8;
                }

                // JS-Rapee-Bold: น. ,Fahkwang-Bold: 4, Kt-sman-Like-xxx: z
                if(window.segmentshapes.enable24 && !found && angledeg < 0 && error > 1.8 && prev && prev[7] && prev[7].length > 1 && isCirInts(prev, p) && i > 1) {
                  // update exclude: พ.ชะพลู
                  if((p[2] / prev[2]) < 0.9 && prev1 && prev1[4] == 4) {
                    window.segmentshapes.enabledebug && console.log('idx:', idx, 'enable24:', error);
                    segments.push(pl.slice(segmentstartidx, i));
                    segmentstartidx = i;
                    found = true;
                  }
                }

                // ร.รัฐ๓ูมิ (h->t), (h->t)
                if(window.segmentshapes.enable4 && !found && angledeg < 0 && error > 1.6) {
                  // ร.รัฐ๓ูมิ (h->t)
                  // ค. วิหค (h->t),
                  // ฟ.ฟารัต (h->t)
                  if(!found && i > 1 && prev && prev[7] && prev[7].length > 1 && isCirInts(prev, p) && prev1 && prev1[4] != 4) {
                    // update exclude: พ.ชะพลู
                    // update exclude: JS-Rapee-Bold: น.
                    // update include: Prompt-Black: 2 change 0.9 to 0.97
                    if((p[2] / prev[2]) < 0.97) {
                      window.segmentshapes.enabledebug && console.log('idx:', idx, 'enable4.1:', error);

                      // Kt-sman-Like: 4
                      let minIdx = d3.minIndex(prev[7], ([err, errpoint]) => getDist(p, errpoint));

                      const p0 = point;
                      const p1 = prev[7][minIdx][1];
                      const midx = (p0[0] + p1[0]) / 2;
                      const midy = (p0[1] + p1[1]) / 2;
                      const dx = p0[0] - p1[0];
                      const dy = p0[1] - p1[1];
                      const radius = Math.hypot(dx, dy) / 2;
                      let samples = [];
                      for(const po of [prev1, prev, p, next, next1]) {
                        if(po) {
                          samples.push(po);
                        }
                      }
                      let tangent = getTangentAngleRad(p0, p1);
                      if(samples.length > 2) {
                        if(iscw(samples)) {
                          tangent -= M_PI2;
                        }
                        else {
                          tangent += M_PI2;
                        }
                      }
                      else {
                        if(cw) {
                          tangent -= M_PI2;
                        }
                        else {
                          tangent += M_PI2;
                        }
                      }

                      //lineIntersect(x0, y0, x1, y1, x2, y2, x3, y3)
                      let sliceto = i;
                      let ii = i+1;
                      while(--ii > 0) {
                        if(lineIntersect(p0[0], p0[1], p1[0], p1[1], pl[ii][0], pl[ii][1], pl[ii-1][0], pl[ii-1][1])) {
                          sliceto = ii;
                          if(diffAngleRad(tangent, getTangentAngleRad(pl[ii-1], [midx, midy])) > (90 * D2R)) {
                            tangent -= Math.PI;
                          }
                          break;
                        }
                      }
                      if(ii == 0) {
                        // ร.รัฐ๓ูมิ (h->t)
                        segments.push(pl.slice(segmentstartidx, i));
                        segmentstartidx = i;
                        found = true;
                      }
                      else {
                        const p8 = [midx, midy, radius, 1, 8, tangent];
                        segments.push([...pl.slice(segmentstartidx, sliceto), p8]);
                        segmentstartidx = sliceto;
                        found = true;
                        pl[--segmentstartidx] = p8;
                      }
                    }
                  }
                  
                  // ร.รัฐ๓ูมิ (t->h)
                  // ค. วิหค (t->h),
                  // ฟ.ฟารัต (t->h)
                  if(!found && i < (ilen-2) && next && next[7] && next[7].length > 1 && isCirInts(next, p) && next1 && next1[4] != 4) {
                    // update exclude: พ.ชะพลู
                    // update exclude: JS-Rapee-Bold: น.
                    // update include: Prompt-Black: 2 change 0.9 to 0.97
                    if((p[2] / next[2]) < 0.97) {
                      window.segmentshapes.enabledebug && console.log('idx:', idx, 'enable4.2:', error);

                      // Kt-sman-Like: 4
                      let minIdx = d3.minIndex(next[7], ([err, errpoint]) => getDist(p, errpoint));

                      const p0 = point;
                      const p1 = next[7][minIdx][1];
                      const midx = (p0[0] + p1[0]) / 2;
                      const midy = (p0[1] + p1[1]) / 2;
                      const dx = p0[0] - p1[0];
                      const dy = p0[1] - p1[1];
                      const radius = Math.hypot(dx, dy) / 2;
                      let samples = [];
                      for(const po of [next1, next, p, prev, prev1]) {
                        if(po) {
                          samples.push(po);
                        }
                      }
                      let tangent = getTangentAngleRad(p0, p1);
                      if(samples.length > 2) {
                        if(iscw(samples)) {
                          tangent += M_PI2;
                        }
                        else {
                          tangent -= M_PI2;
                        }
                      }
                      else {
                        if(cw) {
                          tangent += M_PI2;
                        }
                        else {
                          tangent -= M_PI2;
                        }
                      }

                      //lineIntersect(x0, y0, x1, y1, x2, y2, x3, y3)
                      let sliceto = i;
                      let ii = i-1;
                      while(++ii < lastidx) {
                        if(lineIntersect(p0[0], p0[1], p1[0], p1[1], pl[ii][0], pl[ii][1], pl[ii+1][0], pl[ii+1][1])) {
                          sliceto = ii;
                          if(diffAngleRad(tangent, getTangentAngleRad(pl[ii-1], [midx, midy])) > (90 * D2R)) {
                            tangent -= Math.PI;
                          }
                          break;
                        }
                      }
                      if(ii == lastidx) {
                        // ร.รัฐ๓ูมิ (t->h)
                        segments.push(pl.slice(segmentstartidx, i+1));
                        segmentstartidx = i+1;
                        found = true;
                      }
                      else {
                        const p8 = [midx, midy, radius, 1, 8, tangent];
                        segments.push([...pl.slice(segmentstartidx, sliceto+1), p8]);
                        segmentstartidx = sliceto+1;
                        found = true;
                        pl[--segmentstartidx] = p8;
                      }
                    }
                  }
                }

                // Krub-Medium: ท
                if(window.segmentshapes.enable5 && !found && angledeg < 0 && error > 5 && prev && prev[7] && prev[7].length == 2 && prev[7][0][0] > 1.2 && prev[7][1][0] > 1.2 && isCirInts(prev, p) && i > 1) {
                  window.segmentshapes.enabledebug && console.log('idx:', idx, 'enable5:', error);
                  segments.push(pl.slice(segmentstartidx, i));
                  segmentstartidx = i;
                  found = true;
                }

                // Pridi-Bold: 2
                if(window.segmentshapes.enable26 && !found && angledeg < 0 && error > 3 && next && next[7] && next[7].length == 2 && next[7][0][0] > 1.05 && next[7][1][0] > 1.05 && isCirInts(next, p) && i > 0) {
                  window.segmentshapes.enabledebug && console.log('idx:', idx, 'enable26:', error);
                  segments.push(pl.slice(segmentstartidx, i+1));
                  segmentstartidx = i+1;
                  found = true;
                }

                // Pridi-Bold: z
                if(window.segmentshapes.enable25 && !found && angledeg < 0 && error > 2 && next && next[7] && next[7].length == 2 && next[7][0][0] > 1.05 && next[7][1][0] > 1.4 && isCirInts(next, p) && i > 0 &&
                  // add more about z, (has error every points after this)
                  next1 && next1[7] && next1[7].length == 2 && next2 && next2[7] && pl[i+4] && pl[i+4][7]
                ) {
                  window.segmentshapes.enabledebug && console.log('idx:', idx, 'enable25:', error);
                  segments.push(pl.slice(segmentstartidx, i+1));
                  segmentstartidx = i+1;
                  found = true;

                  let minIdx = d3.minIndex(next[7], ([err, errpoint]) => getDist(p, errpoint));

                  const p0 = point;
                  const p1 = next[7][minIdx][1];
                  const midx = (p0[0] + p1[0]) / 2;
                  const midy = (p0[1] + p1[1]) / 2;
                  const dx = p0[0] - p1[0];
                  const dy = p0[1] - p1[1];
                  const radius = Math.hypot(dx, dy) / 2;
                  let tangent = getTangentAngleRad(p0, p1) + M_PI2;

                  //lineIntersect(x0, y0, x1, y1, x2, y2, x3, y3)
                  let ii = i-1;
                  let len = pl.length-1;
                  while(++ii < len) {
                    if(lineIntersect(p0[0], p0[1], p1[0], p1[1], pl[ii][0], pl[ii][1], pl[ii+1][0], pl[ii+1][1])) {
                      //sliceto = ii;
                      if(diffAngleRad(tangent, getTangentAngleRad([midx, midy], pl[ii+1])) > (90 * D2R)) {
                        tangent -= Math.PI;
                      }
                      break;
                    }
                  }

                  const p8 = [midx, midy, radius, 1, 8, tangent];
                  
                  let px = pl[segmentstartidx+1];
                  if(px) {
                    let dist = getDist(px, p8);
                    let shrink = d3.interpolateNumberArray([px[0], px[1]], [p8[0], p8[1]])((dist-shrinkunit)/dist);
                    p8[0] = shrink[0];
                    p8[1] = shrink[1];
                  }
                  
                  pl[segmentstartidx] = p8;
                }

                // ร.รัฐภูมิ (t->h), (h->t)
                if(window.segmentshapes.enable6 && !found && angledeg < 0 && error > 1.65 && error < 2.2) {
                  // (t->h)
                  if(!found && i < (ilen-2) && next && next[7] && isCirInts(next, p) && next1 && next1[7] && isCirInts(next1, p)) {
                    let fp = pl[0];
                    if(fp && fp[7] && fp[7].length == 2) {
                      window.segmentshapes.enabledebug && console.log('idx:', idx, 'enable6.1:', error);
                      segments.push(pl.slice(segmentstartidx, i+1));
                      segmentstartidx = i+2;
                      found = true;
                    }
                  }
                  // (h->t)
                  if(!found && i > 1 && prev && prev[7] && isCirInts(prev, p) && prev1 && prev1[7] && isCirInts(prev1, p)) {
                    let lp = pl[lastidx];
                    if(lp && lp[7] && lp[7].length == 2) {
                      window.segmentshapes.enabledebug && console.log('idx:', idx, 'enable6.2:', error);
                      segments.push(pl.slice(segmentstartidx, i-1));
                      segmentstartidx = i;
                      found = true;
                    }
                  }
                }

                // JS-Obsaward-Italic: ก, ญ
                if(window.segmentshapes.enable7 && !found && angledeg < 0 && error > 1.8 && error < 2.2 && prev && next && prev[2] < p[2] && p[2] > next[2] && prev[7] && isCirInts(prev, p) && i > 1 && next[7] && isCirInts(next, p) && i < (pl.length-2)) {
                  window.segmentshapes.enabledebug && console.log('idx:', idx, 'enable7:', error);
                  segments.push(pl.slice(segmentstartidx, i+1));
                  segmentstartidx = i+1;
                  found = true;
                }

                // JS-75-Pumpuang, JS-Pumpuang-Normal: พ ผ
                // Vintage: Limelight-Regular: W
                if(window.segmentshapes.enable8 && !found && angledeg < 0 && error > 2.2 && next && isCirInts(p, next)) {
                  // find next error
                  let errors2 = next[7];
                  if(errors2) {
                    let maxidx = d3.maxIndex(errors2, ([error, point]) => error);
                    if(maxidx > -1) {
                      let [error2, point2] = errors2[maxidx];
                      if(error2 > 2.2) {
                        let diffz = Math.max(p[2], next[2]) / Math.min(p[2], next[2]);
                        //console.log(error, error2, diffz);
                        if(diffz > 2.1) {
                          window.segmentshapes.enabledebug && console.log('idx:', idx, 'enable8:', idx, error, error2, diffz);
                          segments.push(pl.slice(segmentstartidx, i+1));
                          segmentstartidx = i+1;
                          found = true;
                          if(p[2] > next[2]) {
                            const p8 = [point[0], point[1], 1.0, 1, 8, M_PI2];
                            segments[segments.length-1].push(p8);
                          }
                          else {
                            const p8 = [point2[0], point2[1], 1.0, 1, 8, -M_PI2];
                            segmentstartidx--;
                            pl[segmentstartidx] = p8;
                            
                          }
                        }
                      }
                    }
                  }
                }

                // JS-Obsaward-Italic, JS-Obsaward-Normal: ฟ, พ
                if(window.segmentshapes.enable9 && !found && angledeg < 0 && error > 8.0) {
                  // find next error
                  if((next && next[7] == undefined) && next1 && isCirInts(p, next1)) {
                    let errors2 = next1[7];
                    if(errors2) {
                      let maxidx = d3.maxIndex(errors2, ([error, point]) => error);
                      if(maxidx > -1) {
                        let [error2, point2] = errors2[maxidx];
                        if(error2 > 2.5) {
                          let diffz = Math.max(p[2], next1[2]) / Math.min(p[2], next1[2]);
                          //console.log('enable9:', error, error2, diffz);
                          if(diffz > 1.5) {
                            window.segmentshapes.enabledebug && console.log('idx:', idx, 'enable9-0:', error, error2, diffz);
                            segments.push(pl.slice(segmentstartidx, i+1));
                            segmentstartidx = i+2;
                            found = true;
                          }
                        }
                      }
                    }
                  }
                  else if(prev && prev[7] && isCirInts(p, prev)) {
                    let [error2, point2] = prev[7][0];
                    if(error2 > 2.5) {
                      let diffz = Math.max(p[2], prev[2]) / Math.min(p[2], prev[2]);
                      //console.log('enable9:', error, error2, diffz);
                      if(diffz > 1.05) {
                        window.segmentshapes.enabledebug && console.log('idx:', idx, 'enable9-1:', error, error2, diffz);
                        segments.push(pl.slice(segmentstartidx, i));
                        segmentstartidx = i;
                        found = true;
                      }
                    }
                  }
                }

                // JS-Obsaward-Italic, JS-Obsaward-Normal, JS-Yodthida, JS-Yodthida-Italic, Kt-smarn-Seesanthai, more..: ฟ, พ
                if(window.segmentshapes.enable10 && !found && angledeg < 0 && error > 8.0) {
                  // find next error
                  if(next && isCirInts(p, next)) {
                    let errors2 = next[7];
                    if(errors2) {
                      let maxidx = d3.maxIndex(errors2, ([error, point]) => error);
                      if(maxidx > -1) {
                        let [error2, point2] = errors2[maxidx];
                        if(error2 > 2.5) {
                          let diffz = Math.max(p[2], next[2]) / Math.min(p[2], next[2]);
                          //console.log('enable10:', error, error2, diffz);
                          if(diffz > 1.5) {
                            window.segmentshapes.enabledebug && console.log('idx:', idx, 'enable10:', error, error2, diffz);
                            segments.push(pl.slice(segmentstartidx, i+1));
                            segmentstartidx = i+1;
                            found = true;

                            let p0 = point2;
                            let p1 = next;
                            let midx = (p0[0] + p1[0]) / 2;
                            let midy = (p0[1] + p1[1]) / 2;
                            let dx = p0[0] - p1[0];
                            let dy = p0[1] - p1[1];
                            let radius = Math.hypot(dx, dy) / 2;
                            let tangent = getTangentAngleRad(p0, p1) + M_PI2;

                            const p8_0 = [midx, midy, radius, 1, 8, tangent];
                            segments[segments.length-1].push(p8_0);

                            p0 = p;
                            p1 = point2;
                            midx = (p0[0] + p1[0]) / 2;
                            midy = (p0[1] + p1[1]) / 2;
                            dx = p0[0] - p1[0];
                            dy = p0[1] - p1[1];
                            radius = Math.hypot(dx, dy) / 2;
                            tangent = getTangentAngleRad(p0, p1) + M_PI2;

                            const p8_1 = [midx, midy, radius, 1, 8, tangent];
                            segmentstartidx--;
                            pl[segmentstartidx] = p8_1;
                          }
                        }
                      }
                    }
                  }
                }

                // JS-Angsumalin: ผ, ฝ
                if(window.segmentshapes.enable11 && !found && angledeg < 0 && error > 2.5 && prev && prev1 && prev[7] && prev1[7] && isCirInts(p, prev1)) {
                  let [error1, point1] = prev[7][0];
                  let [error2, point2] = prev1[7][0];

                  if(error1 < error2 && error2 < error && p[2] < prev[2] && prev[2] < prev1[2]) {
                    window.segmentshapes.enabledebug && console.log('idx:', idx, 'enable11:', error, 'angledeg:', angledeg);
                    segments.push(pl.slice(segmentstartidx, i-1));
                    segmentstartidx = i;
                    found = true;
                  }
                }

                // JS-Chulee: พ, ฟ
                if(window.segmentshapes.enable12 && !found && angledeg < 0 && error > 8.5 && prev && next && !prev[7] && !next[7] && isCirInts(p, next)) {
                  if(next[4] == 4) {
                    let branch = next;
                    let lines = polys.filter(poly => ((poly != pl) && (branch == poly[0] || branch == poly[poly.length-1])) );
                    // fixed: JS-75-Pumpuang: จ
                    if(lines.length == 2 && lines[0][0] == branch && lines[1][0] == branch) {
                      window.segmentshapes.enabledebug && console.log('idx:', idx, 'enable12:', error, 'angledeg:', angledeg);
                      lines[0].reverse();
                      let fp = lines[0][0];
                      if(fp && fp[7] && fp[7][0] && fp[7][0][1]) {
                        lines[0].unshift(fp[7][0][1]);
                      }
                      lines[0].push(...(lines[1].slice(1)))
                      lines[1].length = 0;
                      branch[4] = 1;

                      segments.push(pl.slice(segmentstartidx, i+1));
                      segmentstartidx = i+1;
                      found = true;
                    }
                  }
                }

                if(window.segmentshapes.enable34 && !found && error > 1.25 && error < 2.0) {

                }

                if(window.segmentshapes.enable35 && !found && (l = ilen-i, l > 3 && l < 6) && angledeg < 0 && error > 1.1 && error < 12.0 && p[7].length == 1 &&
                  (
                    (nangledeg > 0 && next && next[7] && next[7].length == 1 && error < next[7][0][0] && (r = error / next[7][0][0], r > 0.55 && r < 0.61))
                    ||
                    (nangledeg == 0 && n1angledeg > 0 && next1 && next1[7] && next1[7].length == 1 && error < next1[7][0][0] && (r = error / next1[7][0][0], r > 0.55 && r < 0.61))
                  )
                ) {
                  let pe = pl[lastidx];
                  if(pe && pe[4] == 4 && !pe[7]) {
                    window.segmentshapes.enabledebug && console.log('idx:', idx, 'enable35:', error, 'angledeg:', angledeg, 'nangledeg:', nangledeg, 'n1angledeg:', n1angledeg);
                    let slicto = i+1;
                    segments.push(pl.slice(segmentstartidx, slicto));
                    if(nangledeg == 0 && n1angledeg > 0) {
                      slicto++;
                    }
                    segmentstartidx = slicto;
                    i = slicto;
                  }
                }
                
                // segments angledeg ~237° to ~358°
                if(window.segmentshapes.enable44 && !found && angledeg < 0 && error >= 1.05 && error <= 10.0 &&
                    prev &&
                    next &&
                    (prev[7] || next[7]) &&
                    (!prev[7] || prev[7][0][0] > 1.1) &&
                    (!next[7] || next[7][0][0] > 1.1)
                ) {
                  window.segmentshapes.enabledebug && console.log('idx:', idx, 'enable44:', error, 'angledeg:', angledeg);
                  let points = [prev, p, next];
                  let turndeg = getTurnAngleDeg(...points);
                  let polyline = polylines.find(polyline => points.every(p => polyline.indexOf(p) > -1));
                  if(polyline) {
                    let p0 = turndeg < 0? polyline[polyline.length-1] : polyline[0];
                    let p1 = turndeg < 0? polyline[0] : polyline[polyline.length-1];
                    let midx = (p0[0] + p1[0]) / 2;
                    let midy = (p0[1] + p1[1]) / 2;
                    let dx = p0[0] - p1[0];
                    let dy = p0[1] - p1[1];
                    let dist = Math.hypot(dx, dy);
                    let radius = dist / 2;
                    let tangent = getTangentAngleRad(p0, p1) - M_PI2;

                    
                    if(prev1 && prev[7]) {
                      let tangent0 = getTangentAngleRad(prev1, prev);
                      let px = [0.9 * prev[2] * Math.cos(tangent0) + prev[0], 0.9 * prev[2] * Math.sin(tangent0) + prev[1]];
                      if(ringContainsPoint(polyline, px) >= 0) {
                        segments.push(pl.slice(segmentstartidx, i));
                        segmentstartidx = i+1;
                        found = true;
                        
                        let pxx = d3.interpolateNumberArray(prev.slice(0, 2), px)(1.1);
                        let intersect = lineIntersect(p0, p1, px, pxx);
                        if(intersect) {
                          if(radius < prev[2]) {
                            const p8_0 = [midx, midy, radius, 1, 8, tangent];
                            segments[segments.length-1].push(p8_0);
                          }
                          else {
                            let p11 = d3.interpolateNumberArray(p0.slice(0, 2), p1.slice(0, 2))(prev[2]/radius);
                            let midx = (p0[0] + p11[0]) / 2;
                            let midy = (p0[1] + p11[1]) / 2;
                            const p8_0 = [midx, midy, prev[2], 1, 8, tangent];
                            segments[segments.length-1].push(p8_0);
                          }
                        }
                        else {
                          px[2] = prev[2] / 2;
                          px[3] = 1;
                          px[4] = 8;
                          segments[segments.length-1].push(px);
                        }
                      }
                    }
                    else {
                      //px[2] = prev[2] / 2;
                      //px[3] = 1;
                      //px[4] = 8;
                      //segments[segments.length-1].push(px);
                      
                      //prev[3] = 1;
                      //prev[4] = 8;
                      //
                      //segments.push(pl.slice(segmentstartidx, i));
                      //segmentstartidx = i+1;
                      //found = true;
                    }
                    
                    if(found) {
                      if(next1 && next[7]) {
                        let tangent1 = getTangentAngleRad(next1, next);
                        let nx = [0.9 * next[2] * Math.cos(tangent1) + next[0], 0.9 * next[2] * Math.sin(tangent1) + next[1]];
                        if(ringContainsPoint(polyline, nx) >= 0) {
                          segmentstartidx--;
                          
                          let nxx = d3.interpolateNumberArray(next.slice(0, 2), nx)(1.1);
                          let intersect = lineIntersect(p0, p1, nx, nxx);
                          if(intersect) {
                            if(radius < next[2]) {
                              const p8_0 = [midx, midy, radius, 1, 8, tangent-M_1PI];
                              pl[segmentstartidx] = p8_0;
                            }
                            else {
                              let p00 = d3.interpolateNumberArray(p1.slice(0, 2), p0.slice(0, 2))(next[2]/radius);
                              let midx = (p00[0] + p1[0]) / 2;
                              let midy = (p00[1] + p1[1]) / 2;
                              const p8_0 = [midx, midy, next[2], 1, 8, tangent-M_1PI];
                              pl[segmentstartidx] = p8_0;
                            }
                          }
                          else {
                            nx[2] = next[2] / 2;
                            nx[3] = 1;
                            nx[4] = 8;
                            pl[segmentstartidx] = nx;
                          }
                        }
                      }
                      else {
                        //nx[2] = next[2] / 2;
                        //nx[3] = 1;
                        //nx[4] = 8;
                        //pl[segmentstartidx] = nx;
                        segmentstartidx--;
                        p[3] = 1;
                        p[4] = 8;
                      }
                    }
                  }
                }
                  
                // segments with different angles and sizes
                if(window.segmentshapes.enable45 && !found && error >= 1.05 && error <= 10.0 &&
                    prev && next &&
                    Math.min(p[2], next[2]) / Math.max(p[2], next[2]) < 0.5
                    //&&
                    //next && next[7] && next[7][0][0] > 1.1 &&
                    //isbranchs.every(isbranch => getDist(isbranch, p) > isbranch[2])
                ) {
                  //let angledeg = 0;
                  //let [error, point] = errors[maxidx];
                  //if(point && point[7] && point[7][0] && point[7][0][2]) {
                  //  angledeg = point[7][0][2];
                  //}
                  //let nangledeg = 0;
                  //if(next && next[7] && next[7][0] && next[7][0][1]) {
                  //  let target = next[7][0][1];
                  //  nangledeg = target[7][0][2];
                  //}
                  
                  //let turndeg = getTurnAngleDeg(prev, p, next);
                  //if(Math.abs(turndeg) > 30) {
                  //  //let minspace = Math.hypot(next[0]-p[0], next[1]-p[1], next[2]-p[2]) * 0.7;
                  //  let tangent0 = getTangentAngleRad(prev, p);
                  //  let tangent1 = getTangentAngleRad(p, next);
                  //  let tangent = (tangent0 + tangent1) / 2;
                  //  let normal0 = tangent + M_1PI;
                  //  let normal1 = tangent1 + M_1PI;
                  //
                  //  let [x0, y0, z0] = p;
                  //  let pn00 = [z0 * Math.cos(normal0) + x0, z0 * Math.sin(normal0) + y0];
                  //  normal0 += M_PI2;
                  //  let pn01 = [z0 * Math.cos(normal0) + x0, z0 * Math.sin(normal0) + y0];
                  //  let ln0 = [pn00, pn01];
                  //
                  //  let [x1, y1, z1] = next;
                  //  let pn10 = [z1 * Math.cos(normal1) + x1, z1 * Math.sin(normal1) + y1];
                  //  normal1 += M_PI2;
                  //  let pn11 = [z1 * Math.cos(normal1) + x1, z1 * Math.sin(normal1) + y1];
                  //  let ln1 = [pn10, pn11];
                  //
                  //  let intersect = lineIntersect(...ln0, ...ln1);
                  //  if(intersect) {
                  //    window.segmentshapes.enabledebug && console.log('enable44.4:', angledeg);
                  //    segments.push(pl.slice(segmentstartidx, i+1));
                  //    segmentstartidx = i+1;
                  //    while(segmentstartidx < lastidx && getDist(p, pl[segmentstartidx]) < (Math.max(p[2], pl[segmentstartidx]) * 0.7)) {
                  //      segmentstartidx++;
                  //    }
                  //    found = true;
                  //  }
                  //}
                  
                  //let tangent0 = getTangentAngleRad(prev, p);
                  //let xlength = getDist(p, next) * 0.333;
                  //
                  //let [x0, y0, z0] = p;
                  //let px = [xlength * z0 * Math.cos(tangent0) + x0, xlength * z0 * Math.sin(tangent0) + y0];
                  //let turndeg = getTurnAngleDeg(p, px, next);
                  ////if(getDist(px, next) > next[2] && Math.abs(turndeg) > 80) {
                  //if(Math.abs(turndeg) > 87) {
                  //  window.segmentshapes.enabledebug && console.log('idx:', idx, 'enable45:', error, 'angledeg:', angledeg);
                  //  segments.push(pl.slice(segmentstartidx, i+1));
                  //  segmentstartidx = i+1;
                  //  found = true;
                  //
                  //  let minspace = z0 * 0.333;
                  //  while(segmentstartidx < lastidx && getDist(p, pl[segmentstartidx+1]) < minspace) {
                  //    segmentstartidx++;
                  //  }
                  //
                  //  pl[segmentstartidx][3] = 1;
                  //  pl[segmentstartidx][4] = 8;
                  //
                  //  let polyline = polylines.find(polyline => ringContainsPoint(polyline, px) >= 0);
                  //  if(polyline) {
                  //    let p0 = polyline[0];
                  //    let p1 = polyline[polyline.length-1];
                  //    let midx = (p0[0] + p1[0]) / 2;
                  //    let midy = (p0[1] + p1[1]) / 2;
                  //    let dx = p0[0] - p1[0];
                  //    let dy = p0[1] - p1[1];
                  //    let dist = Math.hypot(dx, dy);
                  //    let radius = dist / 2;
                  //    let tangent = getTangentAngleRad(p0, p1) - M_PI2;
                  //
                  //    //if(radius < z0) {
                  //    //  const p8_0 = [midx, midy, radius, 1, 8, tangent];
                  //    //  segments[segments.length-1].push(p8_0);
                  //    //}
                  //    //else
                  //    {
                  //      let points = [p0.slice(0, 2), p1.slice(0, 2)];
                  //      if(point == p1) {
                  //        points.reverse();
                  //      }
                  //      let p11 = d3.interpolateNumberArray(...points)(z0/radius);
                  //      let midx = (points[0][0] + p11[0]) / 2;
                  //      let midy = (points[0][1] + p11[1]) / 2;
                  //      const p8_0 = [midx, midy, z0, 1, 8, tangent];
                  //      segments[segments.length-1].push(p8_0);
                  //    }
                  //  }
                  //}
                  
                  let condition = true;
                  
                  if(condition && angledeg > 0 && nangledeg < 0 && n1angledeg > 0) {
                    let points = [p, next, next1];
                    let polyline = polylines.find(polyline => points.every(p => polyline.indexOf(p) > -1));
                    if(polyline) {
                      condition = false;
                    }
                    else {
                      condition = true;
                    }
                  }
                  
                  if(condition) {
                    let d0 = getDist(p, next);
                    let turndeg = getTurnAngleDeg(prev, p, next);
                    let absturndeg = Math.abs(turndeg);
                    let maxr = Math.sin(absturndeg*D2R) * d0 / 2;
                    condition = p[2] > maxr;
                  }

                  if(condition && angledeg > 0 && nangledeg < 0) {
                    let turndeg = getTurnAngleDeg(prev, p, point);
                    let absturndeg = Math.abs(turndeg);
                    if(absturndeg > 20 && absturndeg < 90) {
                      let turndeg = getTurnAngleDeg(prev, p, next);
                      let points = [point, p, next];
                      let polyline = polylines.find(polyline => points.every(p => polyline.indexOf(p) > -1));
                      if(polyline) {
                        let p0 = turndeg < 0? polyline[polyline.length-1] : polyline[0];
                        let p1 = turndeg < 0? polyline[0] : polyline[polyline.length-1];
                        let midx = (p0[0] + p1[0]) / 2;
                        let midy = (p0[1] + p1[1]) / 2;
                        let dx = p0[0] - p1[0];
                        let dy = p0[1] - p1[1];
                        let dist = Math.hypot(dx, dy);
                        let radius = dist / 2;
                        let tangent = getTangentAngleRad(p0, p1) - M_PI2;
                        
                        let tangent0 = getTangentAngleRad(prev, p);
                        let px = [1.5 * p[2] * Math.cos(tangent0) + p[0], 1.5 * p[2] * Math.sin(tangent0) + p[1]];
                        let lineintersect = lineIntersect(p0[0],p0[1], p1[0],p1[1], p[0],p[1], px[0],px[1]);
                        if(lineintersect) {
                          window.segmentshapes.enabledebug && console.log('idx:', idx, 'enable45.0:', error, 'angledeg:', angledeg, 'nangledeg:', nangledeg);
                          segments.push(pl.slice(segmentstartidx, i));
                          segmentstartidx = i+1;
                          found = true;
                          
                          let r = getDist(p0, lineintersect);
                          const p8 = [lineintersect[0], lineintersect[1], r, 1, 8, tangent];
                          segments[segments.length-1].push(p8);
                          
                          pl[segmentstartidx][3] = 1;
                          pl[segmentstartidx][4] = 8;
                        }
                      }
                    }
                  }

                  else if(condition && angledeg > 0 && pangledeg < 0 && i > segmentstartidx) {
                    let turndeg = getTurnAngleDeg(next, p, point);
                    let absturndeg = Math.abs(turndeg);
                    if(absturndeg > 20 && absturndeg < 90) {
                      let turndeg = getTurnAngleDeg(next, p, prev);
                      let points = [point, p, next];
                      let polyline = polylines.find(polyline => points.every(p => polyline.indexOf(p) > -1));
                      if(polyline) {
                        let p0 = turndeg < 0? polyline[polyline.length-1] : polyline[0];
                        let p1 = turndeg < 0? polyline[0] : polyline[polyline.length-1];
                        let midx = (p0[0] + p1[0]) / 2;
                        let midy = (p0[1] + p1[1]) / 2;
                        let dx = p0[0] - p1[0];
                        let dy = p0[1] - p1[1];
                        let dist = Math.hypot(dx, dy);
                        let radius = dist / 2;
                        let tangent = getTangentAngleRad(p0, p1) + M_PI2;
                        
                        let tangent0 = getTangentAngleRad(next, p);
                        let px = [1.5 * p[2] * Math.cos(tangent0) + p[0], 1.5 * p[2] * Math.sin(tangent0) + p[1]];
                        let lineintersect = lineIntersect(p0[0],p0[1], p1[0],p1[1], p[0],p[1], px[0],px[1]);
                        if(lineintersect) {
                          window.segmentshapes.enabledebug && console.log('idx:', idx, 'enable45.1:', error, 'angledeg:', angledeg, 'pangledeg:', pangledeg);
                          
                          pl[i-1][3] = 1;
                          pl[i-1][4] = 8;
                                        
                          segments.push(pl.slice(segmentstartidx, i));
                          segmentstartidx = i-1;
                          found = true;
                          
                          let r = getDist(p0, lineintersect);
                          const p8 = [lineintersect[0], lineintersect[1], r, 1, 8, tangent];
                          pl[segmentstartidx] = p8;
                        }
                      }
                    }
                  }
                }
                
                // JS-Boaboon: ม, น
                // JS-Kobori-Allcaps-Bold: สระอี
                if(window.segmentshapes.enable52 && !found && error >= 1.3 && error <= 1.55 &&
                  ((angledeg > 0 && nangledeg < 0) || (angledeg < 0 && nangledeg > 0)) &&
                  prev && next && next1 && (getDist(p, next) < Math.max(p[2], next[2])) &&
                  diffAngleDeg(getTangentAngleDeg(p, point), getTangentAngleDeg(next, next[7][0][1])) <= 12 &&
                  (diff = Math.max(p[2], next[2]) / Math.min(p[2], next[2]), diff > 1.2)
                ) {
                  let condition = true;
                  if(angledeg < 0) {
                    if(n1angledeg > 0 && n2angledeg < 0) {
                      if(isCirInts(next, next2)) {
                        condition = false;
                      }
                    }
                  }
                  
                  if(condition) {
                    window.segmentshapes.enabledebug && console.log('idx:', idx, 'enable52:', error, 'angledegs:', angledeg, nangledeg, n1angledeg, n2angledeg);

                    let p0 = p;
                    let [x0, y0, z0] = p0;
                    let pe0 = point;
                    let [xe0, ye0] = pe0;
                    
                    let p1 = next;
                    let [x1, y1, z1] = p1;
                    let pe1 = p1[7][0][1];
                    let [xe1, ye1] = pe1;
                    
                    let sliceto, theta;
                    if(p[2] < next[2]) {
                      // translate p & next to prev
                      liceto = i;
                      theta = getTangentAngleRad(pe1, pe0);
                    }
                    else {
                      // translate p & next to next1
                      sliceto = i+1;
                      theta = getTangentAngleRad(pe0, pe1);
                    }
                    const p8_0 = [z0 * Math.cos(theta) + xe0, z0 * Math.sin(theta) + ye0, z0, 1, 8];
                    const p8_1 = [z1 * Math.cos(theta) + xe1, z1 * Math.sin(theta) + ye1, z1, 1, 8];
                    
                    let turnrad = getTurnAngleRad(p0, p8_0, pe0);
                    if(turnrad > 0) {
                      p8_1[5] = p8_0[5] = theta + M_PI2;
                    }
                    else {
                      p8_1[5] = p8_0[5] = theta - M_PI2;
                    }
                      
                    segments.push(pl.slice(segmentstartidx, sliceto));
                    if(p[2] > next[2]) {
                      segments[segments.length-1].push(p8_0);
                    }
                    segmentstartidx = sliceto;
                    if(p[2] < next[2]) {
                      pl[segmentstartidx] = p8_1;
                    }
                    found = true;
                    i = i+1;
                  }
                }
                
                
              }
            }
          }
        }

        if(segments.length) {
          if(segmentstartidx < pl.length-1) {
            segments.push(pl.slice(segmentstartidx));
          }
          newpolys.push(...segments);
        }
        else {
          newpolys.push(pl.slice(segmentstartidx));
        }
      }
      
      for(const branch of branches) {
        let lines = newpolys.filter(poly => (branch == poly[0] || branch == poly[poly.length-1]) );
        //let minIdx = d3.minIndex(lines, line => d3.mean(line, p => p[2]));
        //if(minIdx > -1) {
        //  let removepoly = lines[minIdx];
        //
        //  let [x0, y0, radius] = branch;
        //  radius *= 1.42;
        //
        //  let underdomain = removepoly.slice(1).every(p => ([x1, y1] = p, dx = x1-x0, dy = y1-y0, Math.hypot(dx, dy) <= radius));
        //  if(underdomain) {
        //    branch[3]--;
        //    if(branch[3] > 2)
        //      branch[4] = 4;
        //    else if(branch[3] > 1)
        //      branch[4] = 2;
        //    else
        //      branch[4] = 1;
        //    lines = lines.filter(poly => poly != removepoly);
        //    newpolys = newpolys.filter(poly => poly != removepoly);
        //  }
        //}
        
        if(branch[4] == 2 && lines.length == 2) {
          let line0 = lines[0];
          let revdirection0 = line0[0] != branch;
          let line1 = lines[1];
          let revdirection1 = line1[0] != branch;
          
          // ?--line0--? + ?--line1--?
          // 4 cases lines join
          // 1. <--line0--0 + <--line1--0, !rev +  rev, OK
          // 2. <--line0--0 + 0--line1-->, !rev + !rev, reversed line1 to case 1
          // 3. 0--line0--> + 0--line1-->,  rev + !rev, OK
          // 4. 0--line0--> + <--line1--0,  rev +  rev, reversed line1 to case 3
          //
          if(revdirection0) {
            // case 3,4: 0--line0--> + ?--line1--?
            if(revdirection1) {
              // case 4: 0--line0--> + <--line1--0
              const idx = 0;
              if(line1[idx][4] == 8) {
                if(line1[idx][5] > Math.PI) {
                  line1[idx][5] -= Math.PI;
                }
                else {
                  line1[idx][5] += Math.PI;
                }
              }
              // case 3: 0--line0--> + 0--line1-->
              line1.reverse();
            }
            // line0.tail == line1.head
            if(line0[line0.length-1] == line1[0]) {
              // remove line1.head
              line1.shift();
            }
            // case 3: 0--line0--> + 0--line1-->
            line0.push(...line1);
          }
          else {
            // case 1,2: <--line0--0 + ?--line1--?
            if(!revdirection1) {
              // case 2: <--line0--0 + 0--line1-->
              const idx = line1.length-1;
              if(line1[idx][4] == 8) {
                if(line1[idx][5] > Math.PI) {
                  line1[idx][5] -= Math.PI;
                }
                else {
                  line1[idx][5] += Math.PI;
                }
              }
              // case 1: <--line0--0 + <--line1--0
              line1.reverse();
            }
            // line0.head == line1.tail
            if(line0[0] == line1[line1.length-1]) {
              // remove line1.tail
              line1.pop();
            }
            // case 1: <--line0--0 + <--line1--0
            line0.unshift(...line1);
          }

          // remove line1
          newpolys = newpolys.filter(pl => pl != line1);
          // decrease walks by 2
          branch[3] = branch[3] - 2;
          // set type
          if(branch[3] > 2)
            branch[4] = 4;
          else
            branch[4] = branch[3];
        }
        
        if(branch[4] == 1) {
          branch[3] = -1;
        }
      }

      polys = newpolys.map(poly => poly.filter(p => p.length)).filter(poly => poly.length > 1);

      // find and add terminal line-end
      !window.terminalmindiff && (window.terminalmindiff = 0.55);
      !window.terminalmaxlength && (window.terminalmaxlength = 2.6);
      !window.terminalbranchfactor && (window.terminalbranchfactor = 1.5);
      for(const pl of polys) {
        if(pl.length < 2) {
          continue;
        }

        // JS-Rapee-Bold: ฝ
        if(pl.length == 3 && (pl[0][4] == 4 || pl[2][4] == 4)) {
          if(isCirInts(pl[0], pl[1]) && isCirInts(pl[0], pl[2])) {
            let condition = false;
            if(pl[0][4] == 4) {
              condition = pl[2][7] && pl[2][7].length == 2;
            }
            else {
              if(pl[0][7] && pl[0][7].length == 2) {
                condition = true;
                pl.reverse();
              }
            }
            if(condition) {
              // are these edge points from the same polyline?
              let points = [pl[2][7][0][1], pl[2][7][1][1]];
              let polyline = polylines.find(polyline => points.every(p => polyline.indexOf(p) > -1));
              condition = !!polyline;
            }
            if(condition) {
              let p0 = pl[2][7][0][1];
              let p1 = pl[2][7][1][1];
              let midx = (p0[0] + p1[0]) / 2;
              let midy = (p0[1] + p1[1]) / 2;
              let dx = p0[0] - p1[0];
              let dy = p0[1] - p1[1];
              let radius = Math.hypot(dx, dy) / 2;
              let tangent = getTangentAngleRad(p0, p1) + M_PI2;
              if(diffAngleRad(tangent, getTangentAngleRad(pl[2], [midx, midy])) > (60 * D2R)) {
                tangent -= Math.PI;
              }

              const p8 = [midx, midy, radius, 1, 8, tangent];
              let px = pl[1];
              if(px) {
                let dist = getDist(px, p8);
                if(dist > shrinkunit) {
                  let shrink = d3.interpolateNumberArray([px[0], px[1]], [p8[0], p8[1]])((dist-shrinkunit)/dist);
                  p8[0] = shrink[0];
                  p8[1] = shrink[1];
                }
              }
              
              pl.push(p8);
              continue;
            }
          }
        }

        let pi, p0, p1;

        p0 = pl[0];
        if(p0 && p0[4] == 1) {
          pi = pl[1];
          while((pi[4] == 1) && (pl.length > 2)) {
            if(p0[7] && p0[7].length >= 1) {
              //if(p0[7][0][0] >= 2.0 || p0[7].length >= 1) {
              if(p0[7].length >= 1 || p0[7].some(errorinfo => errorinfo[0] >= 2)) {
                break;
              }
            }
            let rmin = Math.min(p0[2], pi[2]);
            let rmax = Math.max(p0[2], pi[2]);
            if((rmin / rmax) > window.terminalmindiff) {
              break;
            }
            let dx = p0[0] - pi[0];
            let dy = p0[1] - pi[1];
            let distance = Math.hypot(dx, dy);
            if(distance > (rmax * 0.7 + rmin)) {
              break;
            }

            let willbreak = (pi[7] && pi[7].length > 0);

            p0 = pi;
            pl.shift();
            pi = pl[1];

            if(willbreak) {
              break;
            }
          }

          const terminals = [];
            
          if(p0[7]) {
            let errorinfo = p0[7].find(v => v[0] > 3.00);
            if(errorinfo) {
              let [error, target] = errorinfo;
              let angledeg = target[7][0][2];
              if(angledeg > 0 && angledeg < 35) {
                let dist = getDist(p0, target);
                if(dist > 3) {
                  p0[3] = -1;
                  p0[4] = 8;
                  let shrink = d3.interpolateNumberArray([p0[0], p0[1], p0[2]], [target[0], target[1], target[2]])((dist-shrinkunit)/dist);
                  shrink[3] = -1;
                  shrink[4] = 8;
                  shrink[5] = getTangentAngleRad(p0, target);
                  terminals.push(shrink);
                }
              }
            }
          }
          
          // JS-Boaboon: ๕
          // Rathabhumi: S
          if(!terminals.length && p0 && (!p0[7] || p0[7].length == 1)) {
            let pii = pl[1];
            let pjj = pl[2];
            if(pii && pjj && isCirInts(p0, pii)) {
              if(!pii[7] && isCirInts(p0, pjj)) {
                pii = pl[2];
                pjj = pl[3];
              }
              if(pii && pjj && pii[7]) {
                let points = (p0[7] && p0[7].length == 1)? [p0[7][0][1], p0, pii, pii[7][0][1]] : [p0, pii, pii[7][0][1]];
                let polyline = polylines.find(polyline => points.every(p => polyline.indexOf(p) > -1));
                if(polyline) {
                  const p0 = polyline[0];
                  const p1 = polyline[polyline.length-1];
                  const midx = (p0[0] + p1[0]) / 2;
                  const midy = (p0[1] + p1[1]) / 2;
                  const radius = getDist(p0, p1) / 2;
                  
                  const target = [midx, midy, radius];
                  // Keania-One-Regular: w
                  const dist = getDist(pii, target);
                  if(dist < (pii[2] * terminalbranchfactor)) {
                    const turndeg = getTurnAngleDeg(target, pii, pjj);
                    if(Math.abs(turndeg) < 36) {
                      const midp = d3.interpolateNumberArray(pii.slice(0, 3), target)(0.5);
                      const term = maketerm(midp, pii.slice(0, 3), 0.666, polygons);
                      if(term) {
                        terminals.push(term);
                        pl.shift();
                        if(pi != pii) {
                          pl.shift();
                        }
                      }
                    }
                  }
                }
              }
            }
          }

          if(!terminals.length) {
            let theta = getTangentAngleRad(pi, p0);
            //if(isCirInts(pi, p0) && pl.length > 2) {
            //  let pj = pl[2];
            //  if(isCirInts(pi, pj)) {
            //    theta = meanAngleRad([theta, getTangentAngleRad(pj, pi)]);
            //  }
            //}
            
            if(pl.length > 2) {
              let maxmean = pl.length-2;
              if(maxmean > 3) {
                maxmean = 3;
              }
              let startidx = 1;
              let startp = pl[startidx++];
              while(maxmean--) {
                let nextp = pl[startidx++];
                if(getDist(p0, nextp) <= p0[2]) {
                  theta = meanAngleRad([theta, getTangentAngleRad(nextp, startp)]);
                  startp = nextp;
                  continue;
                }
                break;
              }             
            }
            
            let linelength = p0[2] * terminalmaxlength;
            let p1 = [linelength * Math.cos(theta) + p0[0], linelength * Math.sin(theta) + p0[1], p0[2] / 2];
            let line = [p0, p1];
            let intersect;
            let intersects = [];
            for(const polygon of polygons.toReversed()) {
              intersect = getTerminalLineIntersectPolygon(line, polygon);
              if(intersect) {
                intersects.push(intersect);
              }
            }
            let minIdx = d3.minIndex(intersects, intersect => getDist(p0, intersect));
            if(minIdx > -1) {
              intersect = intersects[minIdx];
              if(intersect[5] > Math.PI) {
                intersect[5] -= Math.PI;
              }
              else {
                intersect[5] += Math.PI;
              }

              let dist = getDist(p0, intersect);
              if(((intersect[2] / dist) > 2.0) || ((intersect[2] / p0[2]) > 2.0)) {
                //const [x0, y0, z0] = p0;
                //terminals.push([z0 * Math.cos(theta) + x0, z0 * Math.sin(theta) + y0, z0/2, 1, 1]);
                let t0 = maketerm(p0, pi, window.terminalmaxlength, polygons);
                if(t0) {
                  p0[3] = -1;
                  p0[4] = 8;
                  // fixed r
                  let tangent = getTangentAngleRad(pi, p0);
                  let difftheta = diffAngleRad(tangent, t0[5]);
                  if(difftheta > M_PI3) {
                    difftheta = M_PI3;
                  }
                  let maxr = t0[2] / Math.cos(difftheta);
                  t0[2] = maxr;
                  terminals.push(t0);
                }
                else {
                  const [x0, y0, z0] = p0;
                  p0[3] = -1;
                  p0[4] = 8;
                  terminals.push([z0 * Math.cos(theta) + x0, z0 * Math.sin(theta) + y0, z0/2, -1, 8]);
                }
              }
              else {
                if(dist > shrinkunit) {
                  let shrink = d3.interpolateNumberArray([p0[0], p0[1], p0[2]], [intersect[0], intersect[1], intersect[2]])((dist-shrinkunit)/dist);
                  intersect[0] = shrink[0];
                  intersect[1] = shrink[1];
                  intersect[2] = shrink[2];
                }
                terminals.push(intersect);
              }
            }
            else {
              //const [x0, y0, z0] = p0;
              //terminals.push([z0 * Math.cos(theta) + x0, z0 * Math.sin(theta) + y0, z0]);
              const [x0, y0, z0] = p0;
              const x = z0 * Math.cos(theta) + x0;
              const y = z0 * Math.sin(theta) + y0;
              let z = z0;
              if(p0[7]) {
                let [rerror, perror] = p0[7][0];
                if(rerror && perror) {
                  let anglerad = getAbsAngleRad(perror, p0, [x, y]);
                  if(anglerad < M_PI2) {
                    z = Math.sin(anglerad) * rerror * z0;
                    
                    // fixed self radius
                    const normals = [theta + M_PI2, theta - M_PI2];
                    const infos = getInfosFromHit(p0, 1.33, normals, polygons);
                    if(infos && infos.length && infos.every(v => v != null)) {
                      let newz = d3.mean(infos, info => info && info[3]);
                      p0[2] = newz;
                      p0[3] = -1;
                      p0[4] = 8;
                    }
                  }
                }
              }
              terminals.push([x, y, z, -1, 8]);
            }
          }

          
          pl.unshift(...terminals);
        }

        p0 = pl[pl.length-1];
        if(p0 && p0[4] == 1) {
          pi = pl[pl.length-2];
          while((pi[4] == 1) && (pl.length > 2)) {
            if(p0[7] && p0[7].length >= 1) {
              //if(p0[7][0][0] >= 2.0 || p0[7].length >= 1) {
              if(p0[7].length >= 1 || p0[7].some(errorinfo => errorinfo[0] >= 2)) {  
                break;
              }
            }
            let rmin = Math.min(p0[2], pi[2]);
            let rmax = Math.max(p0[2], pi[2]);
            if((rmin / rmax) > window.terminalmindiff) {
              break;
            }
            let dx = p0[0] - pi[0];
            let dy = p0[1] - pi[1];
            let distance = Math.hypot(dx, dy);
            if(distance > (rmax * 0.7 + rmin)) {
              break;
            }

            let willbreak = (pi[7] && pi[7].length > 0);

            p0 = pi;
            pl.pop();
            pi = pl[pl.length-2];

            if(willbreak) {
              break;
            }
          }

          const terminals = [];
          
          if(p0[7]) {
            let errorinfo = p0[7].find(v => v[0] > 3.00);
            if(errorinfo) {
              let [error, target] = errorinfo;
              let angledeg = target[7][0][2];
              if(angledeg > 0 && angledeg < 35) {
                let dist = getDist(p0, target);
                if(dist > 3) {
                  p0[3] = -1;
                  p0[4] = 8;
                  let shrink = d3.interpolateNumberArray([p0[0], p0[1], p0[2]], [target[0], target[1], target[2]])((dist-shrinkunit)/dist);
                  shrink[3] = -1;
                  shrink[4] = 8;
                  shrink[5] = getTangentAngleRad(p0, target);
                  terminals.push(shrink);
                }
              }
            }
          }

          // JS-Boaboon: ๕
          // Rathabhumi: S
          if(!terminals.length && p0 && (!p0[7] || p0[7].length == 1)) {
            let pii = pl[pl.length-2];
            let pjj = pl[pl.length-3];
            if(pii && pjj && isCirInts(p0, pii)) {
              if(!pii[7] && isCirInts(p0, pjj)) {
                pii = pl[pl.length-3];
                pjj = pl[pl.length-4];
              }
              if(pii && pjj && pii[7]) {
                let points = (p0[7] && p0[7].length == 1)? [p0[7][0][1], p0, pii, pii[7][0][1]] : [p0, pii, pii[7][0][1]];
                let polyline = polylines.find(polyline => points.every(p => polyline.indexOf(p) > -1));
                if(polyline) {
                  const p0 = polyline[0];
                  const p1 = polyline[polyline.length-1];
                  const midx = (p0[0] + p1[0]) / 2;
                  const midy = (p0[1] + p1[1]) / 2;
                  const radius = getDist(p0, p1) / 2;
                  
                  const target = [midx, midy, radius];
                  // Keania-One-Regular: w
                  const dist = getDist(pii, target);
                  if(dist < (pii[2] * terminalbranchfactor)) {
                    const turndeg = getTurnAngleDeg(target, pii, pjj);
                    if(Math.abs(turndeg) < 36) {
                      const midp = d3.interpolateNumberArray(pii.slice(0, 3), target)(0.5);
                      const term = maketerm(midp, pii.slice(0, 3), 0.666, polygons);
                      if(term) {
                        terminals.push(term);
                        pl.pop();
                        if(pi != pii) {
                          pl.pop();
                        }
                      }
                    }
                  }
                }
              }
            }
          }

          if(!terminals.length) {
            let theta = getTangentAngleRad(pi, p0);
            //if(isCirInts(pi, p0) && pl.length > 2) {
            //  let pj = pl[pl.length-3];
            //  if(isCirInts(pi, pj)) {
            //    theta = meanAngleRad([theta, getTangentAngleRad(pj, pi)]);
            //  }
            //}

            if(pl.length > 2) {
              let maxmean = pl.length-2;
              if(maxmean > 3) {
                maxmean = 3;
              }
              let startidx = pl.length-2;
              let startp = pl[startidx--];
              while(maxmean--) {
                let nextp = pl[startidx--];
                if(getDist(p0, nextp) <= p0[2]) {
                  theta = meanAngleRad([theta, getTangentAngleRad(nextp, startp)]);
                  startp = nextp;
                  continue;
                }
                break;
              }             
            }
            
            let linelength = p0[2] * terminalmaxlength;
            let p1 = [linelength * Math.cos(theta) + p0[0], linelength * Math.sin(theta) + p0[1], p0[2] / 2];
            let line = [p0, p1];
            let intersect;
            let intersects = [];
            for(const polygon of polygons.toReversed()) {
              intersect = getTerminalLineIntersectPolygon(line, polygon);
              if(intersect) {
                intersects.push(intersect);
              }
            }
            let minIdx = d3.minIndex(intersects, intersect => getDist(p0, intersect));
            if(minIdx > -1) {
              intersect = intersects[minIdx];
              let dist = getDist(p0, intersect);
              if(((intersect[2] / dist) > 2.0) || ((intersect[2] / p0[2]) > 2.0)) {
                //const [x0, y0, z0] = p0;
                //terminals.push([z0 * Math.cos(theta) + x0, z0 * Math.sin(theta) + y0, z0/2, 1, 1]);
                let t0 = maketerm(p0, pi, window.terminalmaxlength, polygons);
                if(t0) {
                  p0[3] = -1;
                  p0[4] = 8;
                  // fixed r
                  let tangent = getTangentAngleRad(pi, p0);
                  let difftheta = diffAngleRad(tangent, t0[5]);
                  if(difftheta > M_PI3) {
                    difftheta = M_PI3;
                  }
                  let maxr = t0[2] / Math.cos(difftheta);
                  t0[2] = maxr;
                  terminals.push(t0);
                }
                else {
                  const [x0, y0, z0] = p0;
                  p0[3] = -1;
                  p0[4] = 8;
                  terminals.push([z0 * Math.cos(theta) + x0, z0 * Math.sin(theta) + y0, z0/2, -1, 8]);
                }
              }
              else {
                if(dist > shrinkunit) {
                  let shrink = d3.interpolateNumberArray([p0[0], p0[1], p0[2]], [intersect[0], intersect[1], intersect[2]])((dist-shrinkunit)/dist);
                  intersect[0] = shrink[0];
                  intersect[1] = shrink[1];
                  intersect[2] = shrink[2];
                }
                terminals.push(intersect);
              }
            }
            else {
              //const [x0, y0, z0] = p0;
              //terminals.push([z0 * Math.cos(theta) + x0, z0 * Math.sin(theta) + y0, z0]);
              const [x0, y0, z0] = p0;
              const x = z0 * Math.cos(theta) + x0;
              const y = z0 * Math.sin(theta) + y0;
              let z = z0;
              if(p0[7]) {
                let [rerror, perror] = p0[7][0];
                if(rerror && perror) {
                  let anglerad = getAbsAngleRad(perror, p0, [x, y]);
                  if(anglerad < M_PI2) {
                    z = Math.sin(anglerad) * rerror * z0;
                    
                    // fixed self radius
                    const normals = [theta + M_PI2, theta - M_PI2];
                    const infos = getInfosFromHit(p0, 1.33, normals, polygons);
                    if(infos && infos.length && infos.every(v => v != null)) {
                      let newz = d3.mean(infos, info => info && info[3]);
                      p0[2] = newz;
                      p0[3] = -1;
                      p0[4] = 8;
                    }
                  }
                }
              }
              terminals.push([x, y, z, -1, 8]);
            }
          }
          
          pl.push(...terminals.toReversed());
        }
      }

      if(specialbranches.length) {
        specialbranches = [...new Set(specialbranches)].filter(p => p[3] == 1);
        for(const branch of specialbranches) {
          let lines = polys.filter(poly => (branch == poly[0] || branch == poly[poly.length-1]) );
          if(lines.length < 3) {
            //console.log(window.strdoing || '', 'idx:', idx, 'lines:', lines, 'branch:', branch);
            continue;
          }
          lines.sort((a,b) => a.length - b.length);
          let shortestline = lines.shift();
          let line0 = lines.shift();
          let line1 = lines.shift();
          if(line0[0] == branch) {
            if(shortestline[0] == branch) {
              line0.unshift(...shortestline.toReversed());
            }
            else {
              line0.unshift(...shortestline);
            }
          }
          else {
            if(shortestline[0] == branch) {
              line0.push(...shortestline);
            }
            else {
              line0.push(...shortestline.toReversed());
            }
          }
          if(line1[0] == branch) {
            if(shortestline[0] == branch) {
              line1.unshift(...shortestline.toReversed());
            }
            else {
              line1.unshift(...shortestline);
            }
          }
          else {
            if(shortestline[0] == branch) {
              line1.push(...shortestline);
            }
            else {
              line1.push(...shortestline.toReversed());
            }
          }
          branch[3] = branch[4] = 1;
          //branch[3] = -1;
          polys = polys.filter(poly => poly != shortestline).map(poly => poly.filter(p => p != branch));
        }
      }

      // remove wrong points
      polys = polys.map(pl => {
        let len = pl.length;
        if(len >= 3) {
          let found = false;
          let p0 = pl[0];
          let p1 = pl[1];
          let p2 = pl[2];
          if(p0 && p1 && p2 && p0[4] == 8 && p1[4] != 8 && p2[4] != 8) {
            if(getDist(p0, p2) < (Math.max(p0[2], p2[2]) + shrinkunit)) {
              found = true;
              pl[1] = null;
            }
          }

          p0 = pl[len-1];
          p1 = pl[len-2];
          p2 = pl[len-3];
          if(p0 && p1 && p2 && p0[4] == 8 && p1[4] != 8 && p2[4] != 8) {
            if(getDist(p0, p2) < (Math.max(p0[2], p2[2]) + shrinkunit)) {
              found = true;
              pl[len-2] = null;
            }
          }

          if(found) {
            return pl.filter(v => v);
          }
        }
        return pl;
      });

      polys = polys.map(pl => {
        let len = pl.length;
        if(len > 1) {
          let idx0 = len - 1;
          let idx1 = len - 2;
          if(pl[idx0][4] == 8 && pl[idx1][4] != 8) {
            if((!pl[idx1][7] || (pl[idx1][7] && pl[idx1][7][0][0] < 1.45)) && getDist(pl[idx0], pl[idx1]) < (pl[idx1][2] + 1.0)) {
            //if((!pl[idx1][7] || (pl[idx1][7] && pl[idx1][7].length == 1 && pl[idx1][7][0][0] < 1.45)) && getDist(pl[idx0], pl[idx1]) < (pl[idx1][2] + 1.0)) {
              if(pl[idx1][2] > (pl[idx0][2] * 1.05)) {
                let roundcap = d3.interpolateNumberArray([pl[idx1][0], pl[idx1][1], pl[idx0][2]], [pl[idx0][0], pl[idx0][1], pl[idx1][2]])(0.7071067811865476);
                roundcap[3] = 1;
                roundcap[4] = 8;
                pl[idx1][3] = 1;
                pl[idx1][4] = 8;
                let temp = pl.pop();
                pl.push(roundcap);
                pl.push(temp);
              }
            }
          }

          idx0 = 0;
          idx1 = 1;
          if(pl[idx0][4] == 8 && pl[idx1][4] != 8) {
            if((!pl[idx1][7] || (pl[idx1][7] && pl[idx1][7][0][0] < 1.45)) && getDist(pl[idx0], pl[idx1]) < (pl[idx1][2] + 1.0)) {
            //if((!pl[idx1][7] || (pl[idx1][7] && pl[idx1][7].length == 1 && pl[idx1][7][0][0] < 1.45)) && getDist(pl[idx0], pl[idx1]) < (pl[idx1][2] + 1.0)) {
              if(pl[idx1][2] > (pl[idx0][2] * 1.05)) {
                let roundcap = d3.interpolateNumberArray([pl[idx1][0], pl[idx1][1], pl[idx0][2]], [pl[idx0][0], pl[idx0][1], pl[idx1][2]])(0.7071067811865476);
                roundcap[3] = 1;
                roundcap[4] = 8;
                pl[idx1][3] = 1;
                pl[idx1][4] = 8;
                let temp = pl.shift();
                pl.unshift(roundcap);
                pl.unshift(temp);
              }
            }
          }
        }

        return pl;
      });


      return polys;
    });

    // reduce intersections
    window.paramreduceintersects == undefined && (window.paramreduceintersects = {enable: true, sim: false, sim0: false, sim1: false, simtol: 0.5, sim0tol: 0.5, sim1tol: 0.5, fnmax: true, fnmax0: true, fnmax1: true, diff: 120, diff0: 120, diff1: 85, retry: 1, retry0: 1, retry1: 1});
    centerline.reduceintersects = centerline.traces.map((polylines, idx) => {
      let polygons = centerline.coordinates[idx];

      polylines = polylines.map(polyline => polyline.map(point => point));
      if(!window.paramreduceintersects.enable) {
        return polylines;
      }
      
      // 1. find branches
      let branches = [...new Set(polylines.flat().filter(v => v[4] == 4))];
      
      // find inter-link, then remove nodes between link
      if(branches.length > 1) {
        for(const line of polylines) {
          if(line.length > 2) {
            const fp = line[0];
            const lp = line[line.length-1];
            if(fp != lp && fp[4] == 4 && lp[4] == 4 && isCirInts(fp, lp)) {
              let bodynodes = line.slice(1, -1);
              let underdomain = bodynodes.every(p => getDist(fp, p) <= fp[2]) && bodynodes.every(p => getDist(lp, p) <= lp[2]);
              if(underdomain) {
                //console.log('remove nodes between inter-link:', line);
                line.length = 0;
                line.push(fp, lp);
              }
            }
          }
        }
      }

      //console.log('0: polylines:', polylines.slice().map(pl => pl.slice()));
      let retry = window.paramreduceintersects.retry;
      while(retry--) {
        // find and join inter-links (line-head and line-tail is branch)
        let interlinks = polylines.filter(pl => pl[0][4] == 4 && pl[pl.length-1][4] == 4 && pl[0] != pl[pl.length-1]);
        let remainingbranches = branches.filter(branch => interlinks.some(pl => (branch == pl[0] || branch == pl[pl.length-1])));
        for(const branch of remainingbranches) {
          let radius = branch[2];

          // 2. find lines associated with branch
          let lines = polylines.filter(pl => (branch == pl[0] || branch == pl[pl.length-1]));

          let lineinfos = [];
          for(let originalline of lines) {
            let line = window.paramreduceintersects.sim? simplifyDist(originalline.slice(), window.paramreduceintersects.simtol) : originalline.slice();
            if(line.length < 2) {
              continue;
            }
            let fp = line[0];
            let lp = line[line.length-1];
            let loop = fp == lp;

            if(branch == fp) {
              let outidx = line.findIndex(p => getDist(branch, p) >= radius);
              if(outidx > -1) {
                if(window.paramreduceintersects.fnmax) {
                  for(let i = outidx - 1; i > 0; i--) {
                    if(line[i][7]) {
                      const p = line[i]
                      while(line.length > 2 && p != line[0]) {
                        line.shift();
                      }
                      break;
                    }
                  }
                }
                else {
                  for(let i = 0, ii = outidx; i < ii; i++) {
                    if(line[i][7]) {
                      const p = line[i]
                      while(line.length > 2 && p != line[0]) {
                        line.shift();
                      }
                      break;
                    }
                  }
                }
              }
              
              if(!line[0] || !line[1]) {
                continue;
              }
              
              let tangent = getTangentAngleRad(line[0], line[1]);
              let revdirection = false;
              lineinfos.push([tangent, line, revdirection, loop, branch, originalline]);
            }
            if(branch == lp) {
              let outidx = line.findLastIndex(p => getDist(branch, p) >= radius);
              if(outidx > -1) {
                if(window.paramreduceintersects.fnmax) {
                  for(let i = outidx + 1, ii = line.length - 1; i < ii; i++) {
                    if(line[i][7]) {
                      const p = line[i]
                      while(line.length > 2 && p != line[line.length-1]) {
                        line.pop();
                      }
                      break;
                    }
                  }
                }
                else {
                  for(let i = line.length - 1, ii = outidx; i > ii; i--) {
                    if(line[i][7]) {
                      const p = line[i]
                      while(line.length > 2 && p != line[line.length-1]) {
                        line.pop();
                      }
                      break;
                    }
                  }
                }
              }

              if(!line[line.length-1] || !line[line.length-2]) {
                continue;
              }

              let tangent = getTangentAngleRad(line[line.length-1], line[line.length-2]);
              let revdirection = true;
              lineinfos.push([tangent, line, revdirection, loop, branch, originalline]);
            }
          }
          
          if(lineinfos.length < 2) {
            continue;
          }

          // matchscore: [diffAngleRad, lineinfo0, lineinfo1]
          let matchscores = [];
          for(let i = 0, ii = lineinfos.length; i < ii; i++) {
            let lineinfo0 = lineinfos[i];
            for(let j = i+1; j < ii; j++) {
              let lineinfo1 = lineinfos[j];
              let diff = diffAngleRad(lineinfo0[0], lineinfo1[0]) * R2D;
              matchscores.push([diff, lineinfo0, lineinfo1]);
            }
          }

          let matchscoreIdx = d3.maxIndex(matchscores, v => v[0]);
          if(matchscoreIdx > -1) {
            let matchscore = matchscores[matchscoreIdx];
            let [diff, lineinfo0, lineinfo1] = matchscore;
            //console.log('0:', diff, lineinfo0, lineinfo1);
            
            // diff0 > 120 Deg.
            if(diff > window.paramreduceintersects.diff) {
              retry = 1;

              let [tangent0, line0, revdirection0, loop0, branch0, originalline0] = lineinfo0;
              
              let [tangent1, line1, revdirection1, loop1, branch1, originalline1] = lineinfo1;

              if(originalline0 == originalline1) {
                if(line0[0] == line0[line0.length-1]) {
                  line0.pop();
                }
                if(line0[0] == branch) {
                  line0.push(line0.shift());
                }
                line0.push(line0[0]);
              }
              else {
                ////trim tail
                //if(loop0) {
                //  if(!revdirection0) {
                //    if(branch == line0[line0.length-1]) {
                //      const p0 = line0.pop();
                //      const p1 = line0[line0.length-1];
                //      if(isCirInts(p0, p1)) {
                //        const midx = (p0[0] + p1[0]) / 2;
                //        const midy = (p0[1] + p1[1]) / 2;
                //        const midz = (p0[2] + p1[2]) / 2;
                //        line0.push([midx, midy, midz, -1, 8]);
                //      }
                //      else {
                //        const theta = getTangentAngleRad(p0, p1);
                //        const hr = p0[2] * 0.5;
                //        const dist = getDist(p0, p1);
                //        const t = (dist-hr) / dist;
                //        const interr = p0[2] * (1 - t) + p1[2] * t;
                //        line0.push([hr * Math.cos(theta) + p0[0], hr * Math.sin(theta) + p0[1], interr, -1, 8]);
                //      }
                //    }
                //  }
                //  else {
                //    if(branch == line0[0]) {
                //      const p0 = line0.shift();
                //      const p1 = line0[0];
                //      if(isCirInts(p0, p1)) {
                //        const midx = (p0[0] + p1[0]) / 2;
                //        const midy = (p0[1] + p1[1]) / 2;
                //        const midz = (p0[2] + p1[2]) / 2;
                //        line0.unshift([midx, midy, midz, -1, 8]);
                //      }
                //      else {
                //        const theta = getTangentAngleRad(p0, p1);
                //        const hr = p0[2] * 0.5;
                //        const dist = getDist(p0, p1);
                //        const t = (dist-hr) / dist;
                //        const interr = p0[2] * (1 - t) + p1[2] * t;
                //        line0.unshift([hr * Math.cos(theta) + p0[0], hr * Math.sin(theta) + p0[1], interr, -1, 8]);
                //      }
                //    }
                //  }
                //}
                //
                ////trim tail
                //if(loop1) {
                //  if(!revdirection1) {
                //    if(branch == line1[line1.length-1]) {
                //      const p0 = line1.pop();
                //      const p1 = line1[line1.length-1];
                //      if(isCirInts(p0, p1)) {
                //        let midx = (p0[0] + p1[0]) / 2;
                //        let midy = (p0[1] + p1[1]) / 2;
                //        let midz = (p0[2] + p1[2]) / 2;
                //        line1.push([midx, midy, midz, 1, 1]);
                //      }
                //      else {
                //        const theta = getTangentAngleRad(p0, p1);
                //        const hr = p0[2] * 0.5;
                //        const dist = getDist(p0, p1);
                //        const t = (dist-hr) / dist;
                //        const interr = p0[2] * (1 - t) + p1[2] * t;
                //        line1.push([hr * Math.cos(theta) + p0[0], hr * Math.sin(theta) + p0[1], interr, -1, 8]);
                //      }
                //    }
                //  }
                //  else {
                //    if(branch == line1[0]) {
                //      const p0 = line1.shift();
                //      const p1 = line1[0];
                //      if(isCirInts(p0, p1)) {
                //        let midx = (p0[0] + p1[0]) / 2;
                //        let midy = (p0[1] + p1[1]) / 2;
                //        let midz = (p0[2] + p1[2]) / 2;
                //        line1.unshift([midx, midy, midz, 1, 1]);
                //      }
                //      else {
                //        const theta = getTangentAngleRad(p0, p1);
                //        const hr = p0[2] * 0.5;
                //        const dist = getDist(p0, p1);
                //        const t = (dist-hr) / dist;
                //        const interr = p0[2] * (1 - t) + p1[2] * t;
                //        line1.unshift([hr * Math.cos(theta) + p0[0], hr * Math.sin(theta) + p0[1], interr, -1, 8]);
                //      }
                //    }
                //  }
                //}

                // ?--line0--? + ?--line1--?
                // 4 cases lines join
                // 1. <--line0--0 + <--line1--0, !rev +  rev, OK
                // 2. <--line0--0 + 0--line1-->, !rev + !rev, reversed line1 to case 1
                // 3. 0--line0--> + 0--line1-->,  rev + !rev, OK
                // 4. 0--line0--> + <--line1--0,  rev +  rev, reversed line1 to case 3
                //
                if(revdirection0) {
                  // case 3,4: 0--line0--> + ?--line1--?
                  if(revdirection1) {
                    // case 4: 0--line0--> + <--line1--0
                    const idx = 0;
                    if(line1[idx][4] == 8) {
                      if(line1[idx][5] > Math.PI) {
                        line1[idx][5] -= Math.PI;
                      }
                      else {
                        line1[idx][5] += Math.PI;
                      }
                    }
                    // case 3: 0--line0--> + 0--line1-->
                    line1.reverse();
                  }
                  // case 3: 0--line0--> + 0--line1-->
                  
                  // line0.tail == line1.head
                  if(line0[line0.length-1] == line1[0]) {
                    // remove line1.head
                    line1.shift();
                  }

                  let p0 = line0[line0.length-1];
                  let p1 = line1[0];
                  if(p0 != branch && p1 != branch && (p0[2] < getDist(p0, branch) || p1[2] < getDist(p1, branch))) {
                    const midx = (p0[0] + p1[0]) / 2;
                    const midy = (p0[1] + p1[1]) / 2;
                    //const midz = (p0[2] + p1[2]) / 2;
                    const midz = (p0[2] + getDist(p0, p1) + p1[2]) / 2;
                    let normal = getTangentAngleRad(p0, p1);
                    if(iscw([branch, p0, p1])) {
                      normal += M_1PI;
                    }
                    branch[8] = [midx, midy, midz, 1, 8, normal-M_PI2];
                  }
                  
                  line0.push(...line1);
                }
                else {
                  // case 1,2: <--line0--0 + ?--line1--?
                  if(!revdirection1) {
                    // case 2: <--line0--0 + 0--line1-->
                    const idx = line1.length-1;
                    if(line1[idx][4] == 8) {
                      if(line1[idx][5] > Math.PI) {
                        line1[idx][5] -= Math.PI;
                      }
                      else {
                        line1[idx][5] += Math.PI;
                      }
                    }
                    // case 1: <--line0--0 + <--line1--0
                    line1.reverse();
                  }
                  // case 1: <--line0--0 + <--line1--0

                  // line0.head == line1.tail
                  if(line0[0] == line1[line1.length-1]) {
                    // remove line1.tail
                    line1.pop();
                  }

                  let p0 = line0[0];
                  let p1 = line1[line1.length-1];
                  if(p0 != branch && p1 != branch && (p0[2] < getDist(p0, branch) || p1[2] < getDist(p1, branch))) {
                    const midx = (p0[0] + p1[0]) / 2;
                    const midy = (p0[1] + p1[1]) / 2;
                    //const midz = (p0[2] + p1[2]) / 2;
                    const midz = (p0[2] + getDist(p0, p1) + p1[2]) / 2;
                    let normal = getTangentAngleRad(p0, p1);
                    if(iscw([branch, p0, p1])) {
                      normal += M_1PI;
                    }
                    branch[8] = [midx, midy, midz, 1, 8, normal-M_PI2];
                  }

                  line0.unshift(...line1);
                }
              }
              
              // update originalline0
              originalline0.length = 0;
              originalline0.push(...line0);
              
              // remove originalline1
              if(originalline0 != originalline1) {
                polylines = polylines.filter(pl => pl != originalline1);
              }

              // decrease walks
              if(branch0[3] > 1) {
                branch0[3]--;
              }
              // set type
              if(branch0[3] > 2)
                branch0[4] = 4;
              else if(branch0[3] > 0)
                branch0[4] = branch0[3];
              
              // decrease walks
              if(branch1[3] > 1) {
                branch1[3]--;
              }
              // set type
              if(branch1[3] > 2)
                branch1[4] = 4;
              else if(branch1[3] > 0)
                branch1[4] = branch1[3];
            }
          }
          //console.log('0: polylines:', polylines.slice().map(pl => pl.slice()));
        }
      }
      
      retry = window.paramreduceintersects.retry0;
      while(retry--) {
        let remainingbranches = branches.filter(branch => polylines.some(pl => (branch == pl[0] || branch == pl[pl.length-1])));
        for(const branch of remainingbranches) {
          let radius = branch[2];

          // 2. find lines associated with branch
          let lines = polylines.filter(pl => (branch == pl[0] || branch == pl[pl.length-1]));

          let lineinfos = [];
          for(let originalline of lines) {
            let line = window.paramreduceintersects.sim0? simplifyDist(originalline.slice(), window.paramreduceintersects.sim0tol) : originalline.slice();
            if(line.length < 2) {
              continue;
            }
            let fp = line[0];
            let lp = line[line.length-1];
            let loop = fp == lp;

            if(branch == fp) {
              let outidx = line.findIndex(p => getDist(branch, p) >= radius);
              if(outidx > -1) {
                if(window.paramreduceintersects.fnmax0) {
                  for(let i = outidx - 1; i > 0; i--) {
                    if(line[i][7]) {
                      const p = line[i]
                      while(line.length > 2 && p != line[0]) {
                        line.shift();
                      }
                      break;
                    }
                  }
                }
                else {
                  for(let i = 0, ii = outidx; i < ii; i++) {
                    if(line[i][7]) {
                      const p = line[i]
                      while(line.length > 2 && p != line[0]) {
                        line.shift();
                      }
                      break;
                    }
                  }
                }
              }
              
              if(!line[0] || !line[1]) {
                continue;
              }
              
              let tangent = getTangentAngleRad(line[0], line[1]);
              let revdirection = false;
              lineinfos.push([tangent, line, revdirection, loop, branch, originalline]);
            }
            if(branch == lp) {
              let outidx = line.findLastIndex(p => getDist(branch, p) >= radius);
              if(outidx > -1) {
                if(window.paramreduceintersects.fnmax0) {
                  for(let i = outidx + 1, ii = line.length - 1; i < ii; i++) {
                    if(line[i][7]) {
                      const p = line[i]
                      while(line.length > 2 && p != line[line.length-1]) {
                        line.pop();
                      }
                      break;
                    }
                  }
                }
                else {
                  for(let i = line.length - 1, ii = outidx; i > ii; i--) {
                    if(line[i][7]) {
                      const p = line[i]
                      while(line.length > 2 && p != line[line.length-1]) {
                        line.pop();
                      }
                      break;
                    }
                  }
                }
              }

              if(!line[line.length-1] || !line[line.length-2]) {
                continue;
              }

              let tangent = getTangentAngleRad(line[line.length-1], line[line.length-2]);
              let revdirection = true;
              lineinfos.push([tangent, line, revdirection, loop, branch, originalline]);
            }
          }
          
          if(lineinfos.length < 2) {
            continue;
          }

          // matchscore: [diffAngleRad, lineinfo0, lineinfo1]
          let matchscores = [];
          for(let i = 0, ii = lineinfos.length; i < ii; i++) {
            let lineinfo0 = lineinfos[i];
            for(let j = i+1; j < ii; j++) {
              let lineinfo1 = lineinfos[j];
              let diff = diffAngleRad(lineinfo0[0], lineinfo1[0]) * R2D;
              matchscores.push([diff, lineinfo0, lineinfo1]);
            }
          }

          let matchscoreIdx = d3.maxIndex(matchscores, v => v[0]);
          if(matchscoreIdx > -1) {
            let matchscore = matchscores[matchscoreIdx];
            let [diff, lineinfo0, lineinfo1] = matchscore;
            //console.log('0:', diff, lineinfo0, lineinfo1);
            
            // diff0 > 120 Deg.
            if(diff > window.paramreduceintersects.diff0) {
              retry = 1;

              let [tangent0, line0, revdirection0, loop0, branch0, originalline0] = lineinfo0;
              
              let [tangent1, line1, revdirection1, loop1, branch1, originalline1] = lineinfo1;

              if(originalline0 == originalline1) {
                if(line0[0] == line0[line0.length-1]) {
                  line0.pop();
                }
                if(line0[0] == branch) {
                  line0.push(line0.shift());
                }
                line0.push(line0[0]);
              }
              else {
                ////trim tail
                //if(loop0) {
                //  if(!revdirection0) {
                //    if(branch == line0[line0.length-1]) {
                //      const p0 = line0.pop();
                //      const p1 = line0[line0.length-1];
                //      if(isCirInts(p0, p1)) {
                //        const midx = (p0[0] + p1[0]) / 2;
                //        const midy = (p0[1] + p1[1]) / 2;
                //        const midz = (p0[2] + p1[2]) / 2;
                //        line0.push([midx, midy, midz, -1, 8]);
                //      }
                //      else {
                //        const theta = getTangentAngleRad(p0, p1);
                //        const hr = p0[2] * 0.5;
                //        const dist = getDist(p0, p1);
                //        const t = (dist-hr) / dist;
                //        const interr = p0[2] * (1 - t) + p1[2] * t;
                //        line0.push([hr * Math.cos(theta) + p0[0], hr * Math.sin(theta) + p0[1], interr, -1, 8]);
                //      }
                //    }
                //  }
                //  else {
                //    if(branch == line0[0]) {
                //      const p0 = line0.shift();
                //      const p1 = line0[0];
                //      if(isCirInts(p0, p1)) {
                //        const midx = (p0[0] + p1[0]) / 2;
                //        const midy = (p0[1] + p1[1]) / 2;
                //        const midz = (p0[2] + p1[2]) / 2;
                //        line0.unshift([midx, midy, midz, -1, 8]);
                //      }
                //      else {
                //        const theta = getTangentAngleRad(p0, p1);
                //        const hr = p0[2] * 0.5;
                //        const dist = getDist(p0, p1);
                //        const t = (dist-hr) / dist;
                //        const interr = p0[2] * (1 - t) + p1[2] * t;
                //        line0.unshift([hr * Math.cos(theta) + p0[0], hr * Math.sin(theta) + p0[1], interr, -1, 8]);
                //      }
                //    }
                //  }
                //}
                //
                ////trim tail
                //if(loop1) {
                //  if(!revdirection1) {
                //    if(branch == line1[line1.length-1]) {
                //      const p0 = line1.pop();
                //      const p1 = line1[line1.length-1];
                //      if(isCirInts(p0, p1)) {
                //        let midx = (p0[0] + p1[0]) / 2;
                //        let midy = (p0[1] + p1[1]) / 2;
                //        let midz = (p0[2] + p1[2]) / 2;
                //        line1.push([midx, midy, midz, 1, 1]);
                //      }
                //      else {
                //        const theta = getTangentAngleRad(p0, p1);
                //        const hr = p0[2] * 0.5;
                //        const dist = getDist(p0, p1);
                //        const t = (dist-hr) / dist;
                //        const interr = p0[2] * (1 - t) + p1[2] * t;
                //        line1.push([hr * Math.cos(theta) + p0[0], hr * Math.sin(theta) + p0[1], interr, -1, 8]);
                //      }
                //    }
                //  }
                //  else {
                //    if(branch == line1[0]) {
                //      const p0 = line1.shift();
                //      const p1 = line1[0];
                //      if(isCirInts(p0, p1)) {
                //        let midx = (p0[0] + p1[0]) / 2;
                //        let midy = (p0[1] + p1[1]) / 2;
                //        let midz = (p0[2] + p1[2]) / 2;
                //        line1.unshift([midx, midy, midz, 1, 1]);
                //      }
                //      else {
                //        const theta = getTangentAngleRad(p0, p1);
                //        const hr = p0[2] * 0.5;
                //        const dist = getDist(p0, p1);
                //        const t = (dist-hr) / dist;
                //        const interr = p0[2] * (1 - t) + p1[2] * t;
                //        line1.unshift([hr * Math.cos(theta) + p0[0], hr * Math.sin(theta) + p0[1], interr, -1, 8]);
                //      }
                //    }
                //  }
                //}

                // ?--line0--? + ?--line1--?
                // 4 cases lines join
                // 1. <--line0--0 + <--line1--0, !rev +  rev, OK
                // 2. <--line0--0 + 0--line1-->, !rev + !rev, reversed line1 to case 1
                // 3. 0--line0--> + 0--line1-->,  rev + !rev, OK
                // 4. 0--line0--> + <--line1--0,  rev +  rev, reversed line1 to case 3
                //
                if(revdirection0) {
                  // case 3,4: 0--line0--> + ?--line1--?
                  if(revdirection1) {
                    // case 4: 0--line0--> + <--line1--0
                    const idx = 0;
                    if(line1[idx][4] == 8) {
                      if(line1[idx][5] > Math.PI) {
                        line1[idx][5] -= Math.PI;
                      }
                      else {
                        line1[idx][5] += Math.PI;
                      }
                    }
                    // case 3: 0--line0--> + 0--line1-->
                    line1.reverse();
                  }
                  // case 3: 0--line0--> + 0--line1-->
                  
                  // line0.tail == line1.head
                  if(line0[line0.length-1] == line1[0]) {
                    // remove line1.head
                    line1.shift();
                  }

                  let p0 = line0[line0.length-1];
                  let p1 = line1[0];
                  if(p0 != branch && p1 != branch && (p0[2] < getDist(p0, branch) || p1[2] < getDist(p1, branch))) {
                    const midx = (p0[0] + p1[0]) / 2;
                    const midy = (p0[1] + p1[1]) / 2;
                    //const midz = (p0[2] + p1[2]) / 2;
                    const midz = (p0[2] + getDist(p0, p1) + p1[2]) / 2;
                    let normal = getTangentAngleRad(p0, p1);
                    if(iscw([branch, p0, p1])) {
                      normal += M_1PI;
                    }
                    branch[8] = [midx, midy, midz, 1, 8, normal-M_PI2];
                  }
                  
                  line0.push(...line1);
                }
                else {
                  // case 1,2: <--line0--0 + ?--line1--?
                  if(!revdirection1) {
                    // case 2: <--line0--0 + 0--line1-->
                    const idx = line1.length-1;
                    if(line1[idx][4] == 8) {
                      if(line1[idx][5] > Math.PI) {
                        line1[idx][5] -= Math.PI;
                      }
                      else {
                        line1[idx][5] += Math.PI;
                      }
                    }
                    // case 1: <--line0--0 + <--line1--0
                    line1.reverse();
                  }
                  // case 1: <--line0--0 + <--line1--0

                  // line0.head == line1.tail
                  if(line0[0] == line1[line1.length-1]) {
                    // remove line1.tail
                    line1.pop();
                  }

                  let p0 = line0[0];
                  let p1 = line1[line1.length-1];
                  if(p0 != branch && p1 != branch && (p0[2] < getDist(p0, branch) || p1[2] < getDist(p1, branch))) {
                    const midx = (p0[0] + p1[0]) / 2;
                    const midy = (p0[1] + p1[1]) / 2;
                    //const midz = (p0[2] + p1[2]) / 2;
                    const midz = (p0[2] + getDist(p0, p1) + p1[2]) / 2;
                    let normal = getTangentAngleRad(p0, p1);
                    if(iscw([branch, p0, p1])) {
                      normal += M_1PI;
                    }
                    branch[8] = [midx, midy, midz, 1, 8, normal-M_PI2];
                  }

                  line0.unshift(...line1);
                }
              }
              
              // update originalline0
              originalline0.length = 0;
              originalline0.push(...line0);
              
              // remove originalline1
              if(originalline0 != originalline1) {
                polylines = polylines.filter(pl => pl != originalline1);
              }

              // decrease walks
              if(branch0[3] > 1) {
                branch0[3]--;
              }
              // set type
              if(branch0[3] > 2)
                branch0[4] = 4;
              else if(branch0[3] > 0)
                branch0[4] = branch0[3];
              
              // decrease walks
              if(branch1[3] > 1) {
                branch1[3]--;
              }
              // set type
              if(branch1[3] > 2)
                branch1[4] = 4;
              else if(branch1[3] > 0)
                branch1[4] = branch1[3];
            }
          }
          //console.log('0: polylines:', polylines.slice().map(pl => pl.slice()));
        }
      }
      
      //console.log('1: polylines:', polylines.slice().map(pl => pl.slice()));
      
      retry = window.paramreduceintersects.retry1;
      while(retry--) {
        let remainingbranches = branches.filter(branch => polylines.some(pl => (branch == pl[0] || branch == pl[pl.length-1])));
        for(const branch of remainingbranches) {
          let radius = branch[2];

          // 2. find lines associated with branch
          //let lines = polylines.filter(pl => (branch == pl[0] || branch == pl[pl.length-1]));
          let lines = polylines.filter(pl => (fp = pl[0], lp = pl[pl.length-1], branch == fp || branch == lp || (remainingbranches.indexOf(fp) > -1 && getDist(branch, fp) * 0.7071067811865476 < radius) || (remainingbranches.indexOf(lp) > -1 && getDist(branch, lp) * 0.7071067811865476 < radius)));

          let lineinfos = [];
          for(let originalline of lines) {
            let line = window.paramreduceintersects.sim1? simplifyDist(originalline.slice(), window.paramreduceintersects.sim1tol) : originalline.slice();
            if(line.length < 2) {
              continue;
            }
            let fp = line[0];
            let lp = line[line.length-1];
            let loop = fp == lp;
            
            let branch0, branch1;
            if(branch == fp || (remainingbranches.indexOf(fp) > -1 && getDist(branch, fp) * 0.7071067811865476  < radius)) {
              branch0 = fp;
            }
            if(branch == lp || (remainingbranches.indexOf(lp) > -1 && getDist(branch, lp) * 0.7071067811865476  < radius)) {
              branch1 = lp;
            }
            //if(branch0 && branch1 && branch0 != branch1) {
            //  let underdomain = originalline.every(p => getDist(branch, p) <= radius);
            //  if(underdomain) {
            //    polylines = polylines.filter(pl => pl != originalline);
            //    continue;
            //  }
            //}
            
            if(branch0) {
              let outidx = line.findIndex(p => getDist(branch, p) >= radius);
              if(outidx > -1) {
                if(window.paramreduceintersects.fnmax1) {
                  for(let i = outidx - 1; i > 0; i--) {
                    if(line[i][7]) {
                      const p = line[i]
                      while(line.length > 2 && p != line[0]) {
                        line.shift();
                      }
                      break;
                    }
                  }
                }
                else {
                  for(let i = 0, ii = outidx; i < ii; i++) {
                    if(line[i][7]) {
                      const p = line[i]
                      while(line.length > 2 && p != line[0]) {
                        line.shift();
                      }
                      break;
                    }
                  }
                }
              }
              
              if(!line[0] || !line[1]) {
                continue;
              }

              let tangent = getTangentAngleRad(line[0], line[1]);
              let revdirection = false;
              lineinfos.push([tangent, line, revdirection, loop, branch0, originalline]);
            }
            
            if(branch1) {
              let outidx = line.findLastIndex(p => getDist(branch, p) >= radius);
              if(outidx > -1) {
                if(window.paramreduceintersects.fnmax1) {
                  for(let i = outidx + 1, ii = line.length - 1; i < ii; i++) {
                    if(line[i][7]) {
                      const p = line[i]
                      while(line.length > 2 && p != line[line.length-1]) {
                        line.pop();
                      }
                      break;
                    }
                  }
                }
                else {
                  for(let i = line.length - 1, ii = outidx; i > ii; i--) {
                    if(line[i][7]) {
                      const p = line[i]
                      while(line.length > 2 && p != line[line.length-1]) {
                        line.pop();
                      }
                      break;
                    }
                  }
                }
              }

              if(!line[line.length-1] || !line[line.length-2]) {
                continue;
              }

              let tangent = getTangentAngleRad(line[line.length-1], line[line.length-2]);
              let revdirection = true;
              lineinfos.push([tangent, line, revdirection, loop, branch1, originalline]);
            }           
          }
          
          if(lineinfos.length < 2) {
            continue;
          }

          // matchscore: [diffAngleRad, lineinfo0, lineinfo1]
          let matchscores = [];
          for(let i = 0, ii = lineinfos.length; i < ii; i++) {
            let lineinfo0 = lineinfos[i];
            for(let j = i+1; j < ii; j++) {
              let lineinfo1 = lineinfos[j];
              //let diff = diffAngleRad(lineinfo0[0], lineinfo1[0]) * R2D;
              let diff = lineinfo0[5] == lineinfo1[5]? 0 : (diffAngleRad(lineinfo0[0], lineinfo1[0]) * R2D);
              matchscores.push([diff, lineinfo0, lineinfo1]); 
            }
          }

          let matchscoreIdx = d3.maxIndex(matchscores, v => v[0]);
          if(matchscoreIdx > -1) {
            let matchscore = matchscores[matchscoreIdx];
            let [diff, lineinfo0, lineinfo1] = matchscore;
            //console.log('1:', diff, lineinfo0, lineinfo1);
            
            // diff1 > 120 Deg.
            if(diff > window.paramreduceintersects.diff1) {
              retry = 1;

              let [tangent0, line0, revdirection0, loop0, branch0, originalline0] = lineinfo0;
              
              let [tangent1, line1, revdirection1, loop1, branch1, originalline1] = lineinfo1;

              if(originalline0 == originalline1) {
                if(line0[0] == line0[line0.length-1]) {
                  line0.pop();
                }
                if(line0[0] == branch) {
                  line0.push(line0.shift());
                }
                line0.push(line0[0]);
              }
              else {
                ////trim tail
                //if(loop0) {
                //  if(!revdirection0) {
                //    if(branch == line0[line0.length-1]) {
                //      const p0 = line0.pop();
                //      const p1 = line0[line0.length-1];
                //      if(isCirInts(p0, p1)) {
                //        const midx = (p0[0] + p1[0]) / 2;
                //        const midy = (p0[1] + p1[1]) / 2;
                //        const midz = (p0[2] + p1[2]) / 2;
                //        line0.push([midx, midy, midz, -1, 8]);
                //      }
                //      else {
                //        const theta = getTangentAngleRad(p0, p1);
                //        const hr = p0[2] * 0.5;
                //        const dist = getDist(p0, p1);
                //        const t = (dist-hr) / dist;
                //        const interr = p0[2] * (1 - t) + p1[2] * t;
                //        line0.push([hr * Math.cos(theta) + p0[0], hr * Math.sin(theta) + p0[1], interr, -1, 8]);
                //      }
                //    }
                //  }
                //  else {
                //    if(branch == line0[0]) {
                //      const p0 = line0.shift();
                //      const p1 = line0[0];
                //      if(isCirInts(p0, p1)) {
                //        const midx = (p0[0] + p1[0]) / 2;
                //        const midy = (p0[1] + p1[1]) / 2;
                //        const midz = (p0[2] + p1[2]) / 2;
                //        line0.unshift([midx, midy, midz, -1, 8]);
                //      }
                //      else {
                //        const theta = getTangentAngleRad(p0, p1);
                //        const hr = p0[2] * 0.5;
                //        const dist = getDist(p0, p1);
                //        const t = (dist-hr) / dist;
                //        const interr = p0[2] * (1 - t) + p1[2] * t;
                //        line0.unshift([hr * Math.cos(theta) + p0[0], hr * Math.sin(theta) + p0[1], interr, -1, 8]);
                //      }
                //    }
                //  }
                //}
                //
                ////trim tail
                //if(loop1) {
                //  if(!revdirection1) {
                //    if(branch == line1[line1.length-1]) {
                //      const p0 = line1.pop();
                //      const p1 = line1[line1.length-1];
                //      if(isCirInts(p0, p1)) {
                //        let midx = (p0[0] + p1[0]) / 2;
                //        let midy = (p0[1] + p1[1]) / 2;
                //        let midz = (p0[2] + p1[2]) / 2;
                //        line1.push([midx, midy, midz, 1, 1]);
                //      }
                //      else {
                //        const theta = getTangentAngleRad(p0, p1);
                //        const hr = p0[2] * 0.5;
                //        const dist = getDist(p0, p1);
                //        const t = (dist-hr) / dist;
                //        const interr = p0[2] * (1 - t) + p1[2] * t;
                //        line1.push([hr * Math.cos(theta) + p0[0], hr * Math.sin(theta) + p0[1], interr, -1, 8]);
                //      }
                //    }
                //  }
                //  else {
                //    if(branch == line1[0]) {
                //      const p0 = line1.shift();
                //      const p1 = line1[0];
                //      if(isCirInts(p0, p1)) {
                //        let midx = (p0[0] + p1[0]) / 2;
                //        let midy = (p0[1] + p1[1]) / 2;
                //        let midz = (p0[2] + p1[2]) / 2;
                //        line1.unshift([midx, midy, midz, 1, 1]);
                //      }
                //      else {
                //        const theta = getTangentAngleRad(p0, p1);
                //        const hr = p0[2] * 0.5;
                //        const dist = getDist(p0, p1);
                //        const t = (dist-hr) / dist;
                //        const interr = p0[2] * (1 - t) + p1[2] * t;
                //        line1.unshift([hr * Math.cos(theta) + p0[0], hr * Math.sin(theta) + p0[1], interr, -1, 8]);
                //      }
                //    }
                //  }
                //}

                // ?--line0--? + ?--line1--?
                // 4 cases lines join
                // 1. <--line0--0 + <--line1--0, !rev +  rev, OK
                // 2. <--line0--0 + 0--line1-->, !rev + !rev, reversed line1 to case 1
                // 3. 0--line0--> + 0--line1-->,  rev + !rev, OK
                // 4. 0--line0--> + <--line1--0,  rev +  rev, reversed line1 to case 3
                //
                if(revdirection0) {
                  // case 3,4: 0--line0--> + ?--line1--?
                  if(revdirection1) {
                    // case 4: 0--line0--> + <--line1--0
                    const idx = 0;
                    if(line1[idx][4] == 8) {
                      if(line1[idx][5] > Math.PI) {
                        line1[idx][5] -= Math.PI;
                      }
                      else {
                        line1[idx][5] += Math.PI;
                      }
                    }
                    // case 3: 0--line0--> + 0--line1-->
                    line1.reverse();
                  }
                  // case 3: 0--line0--> + 0--line1-->

                  // line0.tail == line1.head
                  if(line0[line0.length-1] == line1[0]) {
                    // remove line1.head
                    line1.shift();
                  }

                  if(line0[line0.length-1] == branch0 && !branch0[8] && isCirInts(line0[line0.length-1], line0[line0.length-2])) {
                    // remove line0.tail
                    line0.pop();
                  }
                  if(line1[0] == branch1 && !branch1[8] && isCirInts(line1[0], line1[1])) {
                    // remove line1.head
                    line1.shift();
                  }
                  
                  //const p0 = line0[line0.length-1];
                  //const p1 = line1[0];
                  //const midx = (p0[0] + p1[0]) / 2;
                  //const midy = (p0[1] + p1[1]) / 2;
                  //const midz = (p0[2] + p1[2]) / 2;
                  //line0.push([midx, midy, midz, -1, 1]);

                  line0.push(...line1);
                }
                else {
                  // case 1,2: <--line0--0 + ?--line1--?
                  if(!revdirection1) {
                    // case 2: <--line0--0 + 0--line1-->
                    const idx = line1.length-1;
                    if(line1[idx][4] == 8) {
                      if(line1[idx][5] > Math.PI) {
                        line1[idx][5] -= Math.PI;
                      }
                      else {
                        line1[idx][5] += Math.PI;
                      }
                    }
                    // case 1: <--line0--0 + <--line1--0
                    line1.reverse();
                  }
                  // case 1: <--line0--0 + <--line1--0

                  // line0.head == line1.tail
                  if(line0[0] == line1[line1.length-1]) {
                    // remove line1.tail
                    line1.pop();
                  }
                  
                  if(line0[0] == branch0 && !branch0[8] && isCirInts(line0[0], line0[1])) {
                    // remove line0.head
                    line0.shift();
                  }
                  if(line1[line1.length-1] == branch1 && !branch1[8] && isCirInts(line1[line1.length-1], line1[line1.length-2])) {
                    // remove line1.tail
                    line1.pop();
                  }

                  //const p0 = line0[0];
                  //const p1 = line1[line1.length-1];
                  //const midx = (p0[0] + p1[0]) / 2;
                  //const midy = (p0[1] + p1[1]) / 2;
                  //const midz = (p0[2] + p1[2]) / 2;
                  //line0.unshift([midx, midy, midz, -1, 1]);
                  
                  line0.unshift(...line1);
                }
              }
              
              // update originalline0
              originalline0.length = 0;
              originalline0.push(...line0);
              
              // remove originalline1
              if(originalline0 != originalline1) {
                polylines = polylines.filter(pl => pl != originalline1);
              }

              // decrease walks
              if(branch0[3] > 1) {
                branch0[3]--;
              }
              // set type
              if(branch0[3] > 2)
                branch0[4] = 4;
              else if(branch0[3] > 0)
                branch0[4] = branch0[3];
              
              // decrease walks
              if(branch1[3] > 1) {
                branch1[3]--;
              }
              // set type
              if(branch1[3] > 2)
                branch1[4] = 4;
              else if(branch1[3] > 0)
                branch1[4] = branch1[3];
            }
          }
          //console.log('1: polylines:', polylines.slice().map(pl => pl.slice()));
        }
      }
      
      //console.log('2: polylines:', polylines.slice().map(pl => pl.slice()));
      
      // restore type
      for(const branch of branches) {
        branch[3] = -1;
        branch[4] = 4;
        
        //let [x0, y0, radius] = branch;
        //
        //// find lines associated with branch
        //let lines = polylines.filter(pl => (branch == pl[0] || branch == pl[pl.length-1]));
        //let underdomains = lines.filter(pl => pl.every(p => ([x1, y1] = p, dx = x1-x0, dy = y1-y0, Math.hypot(dx, dy) <= radius)));
        //
        //for(const underdomain of underdomains) {
        //  underdomain.length = 0;
        //}
        //polylines = polylines.filter(v => v.length);
      }
      
      // find and remove short inter-links (line-head and line-tail is branch and intersect all points)
      let interlinks = polylines.filter(pl => pl[0][4] == 4 && pl[pl.length-1][4] == 4 && pl[0] != pl[pl.length-1]);
      for(const line of interlinks) {
        const branch = line[0];
        const radius = branch[2];
        const underdomain = line.slice(1).every(p => getDist(branch, p) <= radius);
        if(underdomain) {
          polylines = polylines.filter(pl => pl != line);
        }
      }
      
      let lineinfos = [];
      for(const line of polylines) {
        if(line.length < 2) {
          continue;
        }
        
        let fp = line[0];
        let lp = line[line.length-1];
        let loop = fp == lp;

        if(loop) {
          continue;
        }

        if(fp[4] == 4) {
          if(!line[0] || !line[1]) {
            continue;
          }
          
          let tangent = getTangentAngleRad(line[0], line[1]);
          let revdirection = false;
          lineinfos.push([tangent, line, revdirection]);
        }
        if(lp[4] == 4) {
          if(!line[line.length-1] || !line[line.length-2]) {
            continue;
          }

          let tangent = getTangentAngleRad(line[line.length-1], line[line.length-2]);
          let revdirection = true;
          lineinfos.push([tangent, line, revdirection]);
        }
      }
      
      for(const lineinfo of lineinfos) {
        let [tangent, line, revdirection] = lineinfo;
        let p0 = revdirection? line[line.length-1] : line[0];
        
        //// JS-Prapakorn-Italic, JS-Prapakorn-Normal, JS-Puchong-Normal: น
        //let theta = tangent + M_1PI;
        //let length = p0[2] * 0.3;
        //let radius = p0[2] * 0.9;
        //let p1 = [length * Math.cos(theta) + p0[0], length * Math.sin(theta) + p0[1], radius, -1, 8];
        ////length = p0[2] * 0.7071067811865476;
        ////radius = p0[2] * 1;
        ////let p2 = [length * Math.cos(theta) + p0[0], length * Math.sin(theta) + p0[1], radius, -1, 8];
        //
        //// fixed: Poller-One-Regular: t
        //// fixed: Lumanosimo-Regular: u
        //let t0;
        //let findt0 = false;
        //const points = polylines.filter(polyline => polyline != line).flat();
        //if(!(points.indexOf(p0) > -1)) {
        //  for(const polyline of centerline.multipolylines[idx]) {
        //    if(d3.polygonContains(polyline, p1)) {
        //      findt0 = !polyline.slice(1, -1).filter(p => p != p0).some(p => points.indexOf(p) > -1);
        //      break;
        //    }
        //  }
        //}
        //
        //if(findt0) {
        //  const pi = revdirection? line[line.length-2] : line[1];
        //  if(pi) {
        //    t0 = maketerm(p0, pi, 2.5, polygons);
        //    if(t0) {
        //      // fixed r
        //      let difftheta = diffAngleRad(theta, t0[5]);
        //      t0[2] = t0[2] / Math.cos(difftheta);
        //    }
        //  }
        //}
        
        let clone = p0.slice();
        clone[3] = -1;
        clone[4] = 8;
        if(!revdirection) {
          line.shift();
          line.unshift(clone);
          //if(t0) {
          //  line.unshift(t0);
          //}
          //else {
          //  //line.unshift(p1);
          //  if(p0[8] && getDist(p0, p0[8]) > p0[8][2]) {
          //    line.unshift([...p0[8], -1, 8]);
          //  }
          //}
          if(p0[8]) {// && getDist(p0, p0[8]) > p0[8][2]) {
            const p2 = p0[8];
            const midx = (p0[0] + p2[0]) / 2;
            const midy = (p0[1] + p2[1]) / 2;
            const midz = (p0[2] + p2[2]) / 2;
            const p1 = [midx, midy, midz, 1, 8];
            line.unshift(p1);
            line.unshift(p2);
          }
        }
        else {
          line.pop();
          line.push(clone);
          //if(t0) {
          //  line.push(t0);
          //}
          //else {
          //  //line.push(p1);
          //  if(p0[8] && getDist(p0, p0[8]) > p0[8][2]) {
          //    line.push([...p0[8], -1, 8]);
          //  }
          //}
          if(p0[8]) {// && getDist(p0, p0[8]) > p0[8][2]) {
            const p2 = p0[8];
            const midx = (p0[0] + p2[0]) / 2;
            const midy = (p0[1] + p2[1]) / 2;
            const midz = (p0[2] + p2[2]) / 2;
            const p1 = [midx, midy, midz, 1, 8];
            line.push(p1);
            line.push(p2);
          }
        }
      }

      return polylines;
    });

    // fixed: corners
    window.enablefixedcorners == undefined && (window.enablefixedcorners = true);
    centerline.fixedcorners = centerline.reduceintersects.map((polylines, idx) => {
      if(!window.enablefixedcorners) {
        return polylines;
      }
      
      let polygons = centerline.coordinates[idx];
      
      polylines = polylines.map(polyline => {
        const fp = polyline[0];
        const lp = polyline[polyline.length-1];
        const isloop = polyline.length > 3 && ((fp == lp) || (fp[0] == lp[0] && fp[1] == lp[1] && fp[2] == lp[2]));
        let addpoints = [];
        if(isloop) {
          //polyline.push(polyline[1].slice(), polyline[2].slice());
          const pf1 = polyline[1].slice();
          const pl1 = polyline[polyline.length-2].slice();
          polyline.unshift(pl1);
          polyline.push(pf1);
          addpoints.push(pl1, pf1);
        }
        for(const time of [0, 1]) {
          let removepoints = [];
          let fixedcorner = [];
          let angles = [];
          for(let i = 0, j = 1, k = 2, kk = polyline.length; k < kk; i++, j++, k++) {
            angles[j] = getAbsAngleRad(polyline[i], polyline[j], polyline[k]);
          }
          for(let i = 0, j = 1, ii = polyline.length-1, iii = ii-1; i <= ii; i++, j++) {
            let pi = polyline[i];
            let pj = polyline[j];
            if(i >= 1 && i <= iii && pi[4] != 8 && !(pi[4] == 16 && pj[4] == 16)) {
              let ph = polyline[i-1];
              if(pi[7] && pi[7].length == 1) {
                if(pj) {
                  let edgepointtype = 0;
                  //let error0 = pi[7][pi[7].length-1] && pi[7][pi[7].length-1][0];
                  //let edgepoint0 = pi[7][pi[7].length-1] && pi[7][pi[7].length-1][1];
                  let angledeg = 0;
                  let [error0, edgepoint0] = pi[7][pi[7].length-1];
                  if(edgepoint0 && edgepoint0[7] && edgepoint0[7][0] && edgepoint0[7][0][2]) {
                    angledeg = edgepoint0[7][0][2];
                  }                  
                  let tangent0 = getTangentAngleRad(pi, edgepoint0);
                  let edgepoint1;
                  let errors = pj[7];
                  let angle = M_1PI - ((M_1PI - angles[i]) + (M_1PI - angles[j]));
                  let diffangle = 0;
                  if(time == 0 && (angle <= 150*D2R) && errors && errors.length == 1 && getDist(pi, pj) < Math.min(pi[2], pj[2])) {
                    edgepoint1 = errors[0] && errors[0][1];
                    if(edgepoint1) {
                      let tangent1 = getTangentAngleRad(pj, edgepoint1);
                      diffangle = diffAngleRad(tangent0, tangent1);
                      if(diffangle > (150 * D2R)) {
                        let diff_tangent_normal = R2D * diffAngleRad(meanAngleRad([getTangentAngleRad(polyline[i-1], pi), getTangentAngleRad(pi, pj)]), getTangentAngleRad(edgepoint0, edgepoint0));
                        if(diff_tangent_normal > 30 && diff_tangent_normal < 150) {
                          edgepointtype = 1;
                          //if(time == 1) {
                          //  continue;
                          //}
                        }
                      }
                      else {
                        edgepoint1 = undefined;
                      }
                    }
                  }

                  if(time == 1 && (angles[i] <= 160*D2R) && (getDist(pi, ph) > (Math.max(pi[2], ph[2])*0.6)) && (getDist(pi, pj) > (Math.max(pi[2], pj[2])*0.6)) && angledeg > 0 && error0 && edgepoint0 && !edgepoint1 && pi != removepoints[removepoints.length-1]) {
                    const actualnormal = meanAngleRad([getTangentAngleRad(polyline[i-1], pi), getTangentAngleRad(pi, polyline[i+1])]) + M_PI2;
                    const theta = getTangentAngleRad(edgepoint0, pi);
                    let diffangle = diffAngleRad(actualnormal, theta);
                    if(diffangle > M_PI2) {
                      diffangle = M_1PI - diffangle;
                    }
                    if(diffangle < 36*D2R) {
                      //let linelength = pi[2] * (1/Math.cos(diffangle));
                      //edgepoint1 = [linelength * Math.cos(theta) + pi[0], linelength * Math.sin(theta) + pi[1], (linelength + Math.hypot(edgepoint0[0]-pi[0], edgepoint0[1]-pi[1])) / 2];
                      const linelength = getDist(edgepoint0, pi);
                      edgepoint1 = [linelength * Math.cos(theta) + pi[0], linelength * Math.sin(theta) + pi[1], linelength];
                      hitpolygon(pi, edgepoint1, polygons);
                    }
                  }

                  if(edgepoint0 && edgepoint1) {
                    const p0 = edgepoint0;
                    const p1 = edgepoint1;
                    const midx = (p0[0] + p1[0]) / 2;
                    const midy = (p0[1] + p1[1]) / 2;
                    const dx = p0[0] - p1[0];
                    const dy = p0[1] - p1[1];
                    const radius = Math.hypot(dx, dy) / 2;
                    //const avr_tangent = getTangentAngleRad(polyline[i-1], polyline[j]);
                    //const normal = getTangentAngleRad(p0, p1);
                    //let tangent = normal + (normal < avr_tangent ? -M_PI2 : M_PI2);
                    const actualtangent = getTangentAngleRad(polyline[i-1], polyline[i]);
                    const normal = getTangentAngleRad(p0, p1);
                    let tangent = normal - M_PI2;
                    let diff = diffAngleRad(tangent, actualtangent);
                    if(diff > (90 * D2R)) {
                      tangent -= M_1PI;
                    }
                    
                    if(edgepointtype == 1) {
                      //lineIntersect(x0, y0, x1, y1, x2, y2, x3, y3)
                      let startnextidx = i;
                      let itest = i-2;
                      while(++itest < ii) {
                        //if(lineIntersect(p0[0], p0[1], p1[0], p1[1], polyline[itest][0], polyline[itest][1], polyline[itest+1][0], polyline[itest+1][1])) {
                        //  //console.log('i:', i, 'itest:', itest);
                        //  startnextidx = itest;
                        //  break;
                        //}
                        let nexttangent = getTangentAngleRad([midx, midy], polyline[itest])
                        let diff = diffAngleRad(tangent, actualtangent);
                        if(diff < (90 * D2R)) {
                          //console.log('i:', i, 'itest:', itest);
                          startnextidx = itest;
                          break;
                        }
                      }
                      
                      removepoints.push(pi);
                      removepoints.push(pj);
                      //polyline[i] = pi.slice();
                      //pi = polyline[i];
                      pi = pi.slice();
                      pi[0] = midx;
                      pi[1] = midy;
                      pi[2] = radius;
                      pi[3] = 1;
                      pi[4] = 16;
                      pi[5] = tangent;
                      pi[6] = tangent;
                      //pi[7] = [...pi[7], ...pj[7]];
                      //console.log('diffangle:', diffangle * R2D);
                      //if(diffangle > (150 * D2R)) {
                      //  removepoints.push(pj);
                      //}
                      if((startnextidx - i) > 1) {
                        i = startnextidx;
                      }
                    }
                    else {
                      removepoints.push(pi);
                      //polyline[i] = pi.slice();
                      //pi = polyline[i];
                      pi = pi.slice();
                      pi[0] = midx;
                      pi[1] = midy;
                      pi[2] = radius;
                      pi[3] = 1;
                      pi[4] = 16;
                      pi[5] = tangent;
                      pi[6] = tangent;
                    }
                  }
                }
              }
            }
            fixedcorner.push(pi);
          }
          polyline = fixedcorner.filter(p => removepoints.indexOf(p) == -1);
          /*
          for(let h = 0, i = 1, j = 2, len = polyline.length, jj = len-1; j < len; h++, i++, j++) {
            let ph = polyline[h];
            let pi = polyline[i];
            let pj = polyline[j];
            let turndeg = getTurnAngleDeg(ph, pi, pj);
            //console.log('turndeg:', turndeg, i, pi, pi[4], pi[5]);
            if((time == 1) && (turndeg < -150 || turndeg > 150)) {
              // swap points
              if(j == jj) {
                if(ph[5]) {
                  ph[5] = ph[5] + M_1PI;
                }
                if(ph[6]) {
                  ph[6] = ph[6] + M_1PI;
                }
                if(pi[5]) {
                  pi[5] = pi[5] + M_1PI;
                }
                if(pi[6]) {
                  pi[6] = pi[6] + M_1PI;
                }
                polyline[h] = pi;
                polyline[i] = ph;
              }
              else {
                if(pi[5]) {
                  pi[5] = pi[5] + M_1PI;
                }
                if(pi[6]) {
                  pi[6] = pi[6] + M_1PI;
                }
                if(pj[5]) {
                  pj[5] = pj[5] + M_1PI;
                }
                if(pj[6]) {
                  pj[6] = pj[6] + M_1PI;
                }
                polyline[i] = pj;
                polyline[j] = pi;
              }
            }
          }
          */
        }

        let segments = [];
        let prev = 0;
        for(let i = 1, ii = polyline.length-1; i < ii; i++) {
          const point = polyline[i];
          if(point[4] == 16) {
            segments.push(polyline.slice(prev, i+1));
            prev = i;
          }
        }
        segments.push(polyline.slice(prev));
        
        polyline = segments.map((polyline, idx) => {
          if(polyline[0][4] == 16) {
            const pfirst = polyline[0];
            const limit = polyline.length / 2 + 0.5 | 0;
            const radius = pfirst[2];
            const mindist = radius * DEFAULT_MARGIN_DISTANCE_FACTOR;
            let dist = mindist;
            let startnextidx = 1;
            while(startnextidx < limit) {
              dist = getDist(pfirst, polyline[startnextidx]);
              if(mindist < dist) {
                break;
              }
              startnextidx++;
            }
            if(startnextidx > 1) {
              polyline = [pfirst, ...polyline.slice(startnextidx)];
              //polyline[1][4] = 8;
            }
          }
        
          if(polyline[polyline.length-1][4] == 16) {
            const plast = polyline[polyline.length - 1];
            const limit = polyline.length / 2 | 0;
            const radius = plast[2];
            const mindist = radius * DEFAULT_MARGIN_DISTANCE_FACTOR;
            let dist = mindist;
            let startnextidx = polyline.length - 2;
            while(startnextidx > limit) {
              dist = getDist(plast, polyline[startnextidx]);
              if(mindist < dist) {
                break;
              }
              startnextidx--;
            }
            if(startnextidx < (polyline.length - 2)) {
              polyline = [...polyline.slice(0, startnextidx+1), plast];
              //polyline[polyline.length - 2][4] = 8;
            }
          }
        
          if(idx > 0) {
            polyline.shift();
          }
        
          return polyline;
        }).flat();

        for(const p of polyline) {
          if(p[4] == 16) {
            p[4] = 8;
          }
        }
        
        if(isloop) {
          //if(polyline[0] != polyline[polyline.length-1]) {
          //  polyline.pop();
          //}
          //if(polyline[0] != polyline[polyline.length-1]) {
          //  polyline.pop();
          //}
          for(const addpoint of addpoints) {
            let addindex = polyline.indexOf(addpoint);
            if(addindex > -1) {
              polyline[addindex] = null;
            }
          }
          polyline = polyline.filter(v => v);
          
          const fp = polyline[0];
          const lp = polyline[polyline.length-1];
          if(!((fp == lp) || (fp[0] == lp[0] && fp[1] == lp[1] && fp[2] == lp[2]))) {
            //polyline[0][4] = 1;
            polyline.unshift(polyline[polyline.length-1]);
          }

          let idx8 = polyline.findIndex(v => v[4] == 8);
          if(idx8 > -1) {
            polyline.pop();
            for(let i = 0; i < idx8; i++) {
              polyline.push(polyline.shift());
            }
            polyline.push(polyline[0]);
          }
        }

        return polyline;
      });

      //!window.interpolate2interpolate2 && (window.interpolate2 = [false, 0]);
      //polylines = polylines.map(polyline => interpolatepoints(polyline, ...window.interpolate2));
      
      return polylines;
    });
    
    const calculatetangents = (polyline, avr) => {
      // ex.1
      // poly:    [0,1]
      // tangent:  [0]
      // avr:      []
      // avr:     [tangent[0], ...avr, tangent[tangent.length-1]]

      // ex.2
      // poly:    [0,1,2]
      // tangent:  [0,1]
      // avr:       [0]
      // avr:     [tangent[0], ...avr, tangent[tangent.length-1]]

      // ex.3
      // poly:    [0,1,2,3]
      // tangent:  [0,1,2]
      // avr:       [0,1]
      // avr:     [tangent[0], ...avr, tangent[tangent.length-1]]
      
      avr = !!avr;

      let actualtangentanglerads = [];
      for(let i = 0, j = 1, jj = polyline.length; j < jj; i++, j++) {
        actualtangentanglerads.push(getTangentAngleRad(polyline[i], polyline[j]));
      }
      let actualavrtangentanglerads = [];
      for(let i = 0, j = 1, jj = actualtangentanglerads.length; j < jj; i++, j++) {
        actualavrtangentanglerads.push( meanAngleRad([actualtangentanglerads[i], actualtangentanglerads[j]]) );
      }
      actualavrtangentanglerads = [actualtangentanglerads[0], ...actualavrtangentanglerads, actualtangentanglerads[actualtangentanglerads.length-1]];
      let avrtangentanglerads = actualavrtangentanglerads.slice();

      for(let i = 0, ii = polyline.length, fi = 2, li = ii-3; i < ii; i++) {
        // actual terminal
        if(polyline[i][4] == 8) {
          avrtangentanglerads[i] = polyline[i][5];
        }
      }
          
      //window.avrtimes == undefined && (window.avrtimes = 0);
      //if(polyline.length > 2) {
      //  for(let i = 0, ii = window.avrtimes; i < ii; i++) {
      //    let avrtangentanglerads2 = avrtangentanglerads.map(v => v);
      //    for(let i = 0, j = 1, k = 2, kk = avrtangentanglerads.length; k < kk; i++, j++, k++) {
      //      //if(polyline[j][4] != 16) {
      //        avrtangentanglerads2[j] = meanAngleRad([avrtangentanglerads[i], avrtangentanglerads[j], avrtangentanglerads[k]]);
      //      //}
      //    }
      //    avrtangentanglerads = avrtangentanglerads2;
      //  }
      //}
      
      if(avr) {
        let avrtangentanglerads2 = avrtangentanglerads.map(v => v);
        for(let i = 0, j = 1, k = 2, kk = avrtangentanglerads.length; k < kk; i++, j++, k++) {
          if(polyline[j][4] != 16 || (polyline[i][4] != 8 || polyline[i][4] != 16) || (polyline[k][4] != 8 || polyline[k][4] != 16)) {
            avrtangentanglerads2[j] = meanAngleRad([avrtangentanglerads[i], avrtangentanglerads[j], avrtangentanglerads[k]]);
          }
        }
        avrtangentanglerads = avrtangentanglerads2;
      }

      // reset tangents
      for(let i = 0, ii = polyline.length; i < ii; i++) {
        let [x, y, z, walks, type, tangent] = polyline[i];
        if(type != 8) {
          polyline[i][5] = null;
          polyline[i][6] = null;
        }
      }

      // set tangents
      for(let i = 0, ii = polyline.length; i < ii; i++) {
        let [x, y, z, walks, type, tangent, actualtangent] = polyline[i];
        if(type != 8) {
          if(tangent == null) {
            polyline[i][5] = avrtangentanglerads[i];
          }
          else {
            polyline[i][5] = meanAngleRad([tangent, avrtangentanglerads[i]]);
          }
          if(actualtangent == null) {
            polyline[i][6] = actualavrtangentanglerads[i];
          }
          else {
            polyline[i][6] = meanAngleRad([actualtangent, actualavrtangentanglerads[i]]);
          }
        }
      }
      
      const fp = polyline[0];
      const lp = polyline[polyline.length-1];
      const isloop = polyline.length > 3 && ((fp == lp) || (fp[0] == lp[0] && fp[1] == lp[1] && fp[2] == lp[2]));
      if(isloop) {
        fp[5] = lp[5] = meanAngleRad([fp[5], lp[5]]);
        fp[6] = lp[6] = meanAngleRad([fp[6], lp[6]]);
      }
    }

    centerline.normals = centerline.fixedcorners.map((polylines, traceidx) => {
      let polygons = centerline.coordinates[traceidx];
      let branches = [...new Set(centerline.traces[traceidx].flat().filter(v => v[4] == 4))];
      //let pointerrors = [...new Set(centerline.centers[traceidx].flat().filter(v => v[2] > 0 && v[7]))];
      
      // fixed terminal tangent from fixed cornners
      polylines = polylines.map((polyline, polyidx) => {
        //polyline = ridge_simplify(polyline, DEFAULT_RIDGE_SIMPLIFY, true, false, false, !option0);
        //polyline = ridge_simplify(polyline, DEFAULT_RIDGE_SIMPLIFY, true, false, false, true, option0);
        //polyline = ridge_simplify(polyline, DEFAULT_RIDGE_SIMPLIFY, true, false, false, true, false);
        if(polyline.length < 2) {
          //console.debug('! traceidx:', traceidx, 'polyidx:', polyidx, 'polyline.length:', polyline.length);
          return null;
        }
        
        let segments = [];
        let prev = 0;
        for(let i = 1, ii = polyline.length-1; i < ii; i++) {
          const point = polyline[i];
          // actual terminal: point[5]
          if(point[4] == 8 && point[5]) {
            segments.push(polyline.slice(prev, i+1));
            prev = i;
          }
        }
        segments.push(polyline.slice(prev));
        for(let polyline of segments) {
          //fixed terminal tangent
          if(polyline[0][4] == 8) {
            if(diffAngleRad(polyline[0][5], getTangentAngleRad(polyline[0], polyline[1])) > (90 * D2R)) {
              let [x, y, z, walks, type, tangent, actualtangent, errors] = polyline[0];
              polyline[0] = [x, y, z, walks, type, tangent + M_1PI];
              if(actualtangent) {
                polyline[0][6] = actualtangent + Math.PI;
              }
              if(errors) {
                polyline[0][7] = errors;
              }
            }
          }
          if(polyline[polyline.length-1][4] == 8) {
            if(diffAngleRad(polyline[polyline.length-1][5], getTangentAngleRad(polyline[polyline.length-2], polyline[polyline.length-1])) > (90 * D2R)) {
              let [x, y, z, walks, type, tangent, actualtangent, errors] = polyline[polyline.length-1];
              polyline[polyline.length-1] = [x, y, z, walks, type, tangent + Math.PI];
              if(actualtangent) {
                polyline[polyline.length-1][6] = actualtangent + M_1PI;
              }
              if(errors) {
                polyline[polyline.length-1][7] = errors;
              }
            }
          }
        }
        return segments;
      }).filter(v => v).flat();
      
      //if(istext && polylines.length) {
      //  let fpolyidx = 0;
      //  let lpolyidx = polylines.length - 1;
      //  let fpoly = polylines[fpolyidx];
      //  let lpoly = polylines[lpolyidx];
      //  let fpointidx = 0;
      //  let lpointidx = lpoly.length - 1;
      //  let fpoint = fpoly[fpointidx];
      //  let lpoint = lpoly[lpointidx];
      //
      //  if(lpoint[0] < fpoint[0]) {
      //    // inners flip
      //    for(const poly of polylines) {
      //      poly.reverse();
      //      if(poly[0][4] == 8) {
      //        poly[0][5] += M_1PI;
      //        if(poly[0][6]) {
      //          poly[0][6] += M_1PI;
      //        }
      //      }
      //      if(poly[poly.length-1][4] == 8) {
      //        poly[poly.length-1][5] += M_1PI;
      //        if(poly[poly.length-1][6]) {
      //          poly[poly.length-1][6] += M_1PI;
      //        }
      //      }
      //    }
      //    // self flip
      //    polylines.reverse();
      //  }
      //}
      
      if(polylines[0] && polylines[0][0] && polylines[0][0][4] == 2 && polylines.length > 1) {
        let firstsegment = polylines[0];
        let firstpoint = firstsegment[0];
        let lastsegment = polylines.find(polyline => polyline[polyline.length-1] == firstpoint);
        if(lastsegment && lastsegment != firstsegment) {
          firstpoint[3] = firstpoint[4] = 1;
          lastsegment.pop();
          firstsegment.unshift(...lastsegment);
          polylines = polylines.filter(polyline => polyline != lastsegment);
        }
      }

      return polylines.map((polyline, polyidx) => {

        polyline = ridge_simplify(polyline, DEFAULT_RIDGE_SIMPLIFY, true, false, false, true, false);
        if(polyline.length < 2) {
          //console.debug('! traceidx:', traceidx, 'polyidx:', polyidx, 'polyline.length:', polyline.length);
          return null;
        }
        
        window.pointdistfactor == undefined && (window.pointdistfactor = 1.5);
        let point = polyline[0];
        let points = [point];
        let prvpoint = point;
        for(let dx = 0, dy = 0, dist = 0, i = 1, len = polyline.length; i < len; i++) {
          point = polyline[i];
          //if(!(prvpoint[4] == 8 && prvpoint[5]) && !(point[4] == 8 && point[5])) {
          if(!((prvpoint[4] == 8 && prvpoint[5]) && (point[4] == 8 && point[5]))) {
            dx = point[0] - prvpoint[0];
            dy = point[1] - prvpoint[1];
            dist = Math.hypot(dx, dy);
            //const pointdist = Math.max(prvpoint[2], point[2]) * 3;
            const pointdist = (prvpoint[2] + point[2]) * 0.7071067811865476 * pointdistfactor;
            if(dist > pointdist && !(prvpoint[7] && point[7] && isCirInts(prvpoint, point))) {
              let x0 = prvpoint[0];
              let y0 = prvpoint[1];
              let r0 = prvpoint[2];
              let addinter0 = true;
              if((prvpoint[4] == 8 && prvpoint[5])) {
                let diffangledeg = diffAngleDeg(prvpoint[5] * R2D, getTangentAngleDeg(prvpoint, point));
                r0 = r0 * Math.cos(diffangledeg * D2R);
                addinter0 = false;
              }
              let x1 = point[0];
              let y1 = point[1];
              let r1 = point[2];
              let addinter1 = true;
              if((point[4] == 8 && point[5])) {
                let diffangledeg = diffAngleDeg(point[5] * R2D, getTangentAngleDeg(prvpoint, point));
                r1 = r1 * Math.cos(diffangledeg * D2R);
                addinter1 = false;
              }
              let interpolate = d3.interpolateNumberArray([x0, y0, r0], [x1, y1, r1]);
              if(addinter0) {
                const inter0 = interpolate((r0*0.7071067811865476)/dist);
                points.push([...inter0, -1, 1]);
              }
              if(addinter1) {
                const inter1 = interpolate((dist-(r1*0.7071067811865476))/dist);
                points.push([...inter1, -1, 1]);
              }
            }
            else if(dist < (prvpoint[2] + point[2]) && (Math.max(prvpoint[2], point[2]) / Math.min(prvpoint[2], point[2])) > 1.5) {
              const midx = (prvpoint[0] + point[0]) / 2;
              const midy = (prvpoint[1] + point[1]) / 2;
              const midz = (prvpoint[2] + point[2]) / 2;
              points.push([midx, midy, midz, 1, 1]);
            }
          }
          points.push(point);
          prvpoint = point;
        }
        polyline = points;
        
        for(let i = 0, ii = polyline.length; i < ii; i++) {
          // actual terminal and round-cap
          if(polyline[i][4] == 8) {
            // round-cap
            if(!polyline[i][5]) {
              polyline[i] = polyline[i].slice();
              // change to round-cap type
              polyline[i][4] = 16;
            }
          }
          else if(polyline[i][4] == 4) {
            // clone branch
            polyline[i] = polyline[i].slice();
          }
        }
        
        calculatetangents(polyline, false);
        
        window.avrtimes == undefined && (window.avrtimes = 4);
        window.branchfactor == undefined && (window.branchfactor = 1/3);
        
        if(polyline.length > 2) {
          for(let avrtime = 0; avrtime < avrtimes; avrtime++) {
            for(let i = 0, ii = polyline.length, iii = polyline.length-1; i < ii; i++) {
              const center = polyline[i];
              let [x, y, z, walks, type, tangent, actualtangent, errors] = center;

              if(type != 8 ) {
                if(i > 0 && i < iii) {
                  let difftheta = diffAngleRad(actualtangent, tangent);
                  if(difftheta > M_PI3) {
                    difftheta = M_PI3;
                  }

                  let newz = z / Math.cos(difftheta);
                  if(newz < 0) {
                    tangent -= M_1PI;
                    difftheta = diffAngleRad(actualtangent, tangent);
                    if(difftheta > M_PI3) {
                      difftheta = M_PI3;
                    }
                    newz = z / Math.cos(difftheta);
                  }
                  z = newz;
                }
              }

              // normal = tangent + 90Deg
              let theta = tangent + M_PI2;
              let p0 = [z * Math.cos(theta) + x, z * Math.sin(theta) + y];
              theta += M_1PI;
              let p1 = [z * Math.cos(theta) + x, z * Math.sin(theta) + y];
              
              let enable = type != 8 && walks > 0;
              if(enable) {
                enable = !errors;
              }
              if(enable) {
                enable = !branches.some(branch => getDist(branch, center) <= (branch[2] * branchfactor));
              }
              
              if(enable) {
                const hit0 = hitpolygon(center, p0, polygons);
                const hit1 = hitpolygon(center, p1, polygons);
                
                if(hit0 && hit1) {
                  const midx = (p0[0] + p1[0]) / 2;
                  const midy = (p0[1] + p1[1]) / 2;
                  const radius = getDist(p0, p1) / 2;
                  
                  center[0] = midx;
                  center[1] = midy;
                  center[2] = radius;
                }
              }
            }
            
            calculatetangents(polyline, true);
            //polyline = ridge_simplify(polyline, DEFAULT_RIDGE_SIMPLIFY, true, false, false, true, false);
          }
        }

        let segments = [];
        let prev = 0;
        //for(let i = 1, ii = polyline.length-1; i < ii; i++) {
        //  const point = polyline[i];
        //  if(point[4] == 8) {
        //    segments.push(polyline.slice(prev, i+1));
        //    prev = i;
        //  }
        //}
        segments.push(polyline.slice(prev));
        
        let normalsegments = [];
        for(let polyline of segments) {
          let p0s = [];
          let p1s = [];
          let p2s = [];

          for(let i = 0, ii = polyline.length, iii = polyline.length-1; i < ii; i++) {
            const center = polyline[i];
            let [x, y, z, walks, type, tangent, actualtangent, errors] = center;

            if(type != 8 ) {
              if(i > 0 && i < iii) {
                let difftheta = diffAngleRad(actualtangent, tangent);
                if(difftheta > M_PI3) {
                  difftheta = M_PI3;
                }
                let newz = z / Math.cos(difftheta);
                if(newz < 0) {
                  tangent -= M_1PI;
                  difftheta = diffAngleRad(actualtangent, tangent);
                  if(difftheta > M_PI3) {
                    difftheta = M_PI3;
                  }
                  newz = z / Math.cos(difftheta);
                }
                z = newz;
              }
            }

            // normal = tangent + 90Deg
            let theta = tangent + M_PI2;
            let p0 = [z * Math.cos(theta) + x, z * Math.sin(theta) + y];
            theta += M_1PI;
            let p1 = [z * Math.cos(theta) + x, z * Math.sin(theta) + y];

            let enable = type != 8 && walks > 0;
            if(enable) {
              enable = !branches.some(branch => getDist(branch, center) <= (branch[2] * branchfactor));
            }
            
            if(enable) {
              hitpolygon(center, p0, polygons);
              hitpolygon(center, p1, polygons);
            }

            p0s.push(p0);
            p1s.push(p1);
            p2s.push(center);
          }
          normalsegments.push([p0s, p1s, p2s]);
        }

        return normalsegments;
      }).filter(v => v);
    });

    //centerline.pessatincolumnds = centerline.normals.map((polylines) => {
    //  return polylines.map((normalsegments) => {
    //    let lines = [];
    //    for(const polyline of normalsegments) {
    //      let [p0s, p1s] = polyline;
    //      lines.push(d3linecurveCardinal( p0s ) + ' ');
    //      lines.push(d3linecurveCardinal( p1s ) + ' ');
    //    }
    //    return lines;
    //  }).flat();
    //});
  }

  return centerline;
}


// ---- PES port overrides (appended; not part of the vendored core) ----
USE_WORKER = false; // no straight-skeleton-v2/worker.js here; SkeletonBuilder runs inline
// Handles for the TS driver (top-level const/function of a classic script are
// not reachable as window.* from module code; export them explicitly).
globalThis.__pesSatinCore = {
  apiWorkerMakeMultipolygon,
  apiWorkerGetCenterline,
  apiWorkerGetSatinColumnDS,
  simplifyDouglasPeucker,
  ringarea,
  fnAddMiterJoinToTriangle,
  findNearestBrotherColorIndex,
  hexToRgb,
};
