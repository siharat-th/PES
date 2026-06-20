//
//  pesPolyline.cpp
//  pesEngine
//
//  Created by SIHARAT THAMMAYA on 2/7/2562 BE.
//  Copyright © 2562 SIHARAT THAMMAYA. All rights reserved.
//
#include "include/core/SkCanvas.h"
#include "pesPolyline.hpp"
#include <float.h>

using namespace std;

pesPolyline::pesPolyline() {
    setRightVector();
    clear();
}

pesPolyline::pesPolyline(const vector<pesVec2f>& points) {
    setRightVector();
    clear();
    addVertices(points);
}

pesPolyline pesPolyline::fromRect(const pesRectangle& r) {
    pesPolyline p;
    p.addVertex(pesVec2f(static_cast<float>(r.x), static_cast<float>(r.y)));
    p.addVertex(pesVec2f(static_cast<float>(r.x + r.width), static_cast<float>(r.y)));
    p.addVertex(pesVec2f(static_cast<float>(r.x + r.width), static_cast<float>(r.y + r.height)));
    p.addVertex(pesVec2f(static_cast<float>(r.x), static_cast<float>(r.y + r.height)));
    p.close();
    return p;
}

void pesPolyline::clear() {
    setClosed(false);
    points.clear();
    curveVertices.clear();
    flagHasChanged();
}

void pesPolyline::close() { setClosed(true); }

void pesPolyline::setClosed(bool tf) {
    bClosed = tf;
    flagHasChanged();
}

bool pesPolyline::isClosed() const { return bClosed; }

void pesPolyline::addVertex(const pesVec2f& p) {
    curveVertices.clear();
    points.push_back(p);
    flagHasChanged();
}

void pesPolyline::addVertices(const pesVec2f* verts, int numverts) {
    curveVertices.clear();
    points.insert(points.end(), verts, verts + numverts);
    flagHasChanged();
}

void pesPolyline::addVertices(const vector<pesVec2f>& verts) {
    curveVertices.clear();
    points.insert(points.end(), verts.begin(), verts.end());
    flagHasChanged();
}

void pesPolyline::addVertex(float x, float y) { addVertex(pesVec2f(x, y)); }

void pesPolyline::insert(const pesVec2f& p, int index) {
    curveVertices.clear();
    points.insert(points.begin() + index, p);
    flagHasChanged();
}

void pesPolyline::insert(const vector<pesVec2f>& verts, int index) {
    curveVertices.clear();
    points.insert(points.begin() + index, verts.begin(), verts.end());
    flagHasChanged();
}

void pesPolyline::resize(size_t size) {
    points.resize(size);
    flagHasChanged();
}

size_t pesPolyline::size() const { return points.size(); }

const pesVec2f& pesPolyline::operator[](int index) const { return points[index]; }

pesVec2f& pesPolyline::operator[](int index) {
    flagHasChanged();
    return points[index];
}

vector<pesVec2f>::iterator pesPolyline::begin() { return points.begin(); }

//--------------------------------------------------
vector<pesVec2f>::iterator pesPolyline::end() { return points.end(); }

//--------------------------------------------------
vector<pesVec2f>::const_iterator pesPolyline::begin() const { return points.begin(); }

//--------------------------------------------------
vector<pesVec2f>::const_iterator pesPolyline::end() const { return points.end(); }

//--------------------------------------------------
vector<pesVec2f>::reverse_iterator pesPolyline::rbegin() { return points.rbegin(); }

//--------------------------------------------------
vector<pesVec2f>::reverse_iterator pesPolyline::rend() { return points.rend(); }

//--------------------------------------------------
vector<pesVec2f>::const_reverse_iterator pesPolyline::rbegin() const { return points.rbegin(); }

//--------------------------------------------------
vector<pesVec2f>::const_reverse_iterator pesPolyline::rend() const { return points.rend(); }

bool pesPolyline::hasChanged() {
    if (bHasChanged) {
        bHasChanged = false;
        return true;
    } else {
        return false;
    }
}

void pesPolyline::flagHasChanged() {
    bHasChanged = true;
    bCacheIsDirty = true;
}

vector<pesVec2f>& pesPolyline::getVertices() {
    flagHasChanged();
    return points;
}

const vector<pesVec2f>& pesPolyline::getVertices() const { return points; }

void pesPolyline::setCircleResolution(int res) {
    if (res > 1 && res != (int)circlePoints.size()) {
        circlePoints.resize(res);

        float angle = 0.0f;
        const float angleAdder = (float)(M_TWO_PI / res);
        for (int i = 0; i < res; i++) {
            circlePoints[i].x = cos(angle);
            circlePoints[i].y = sin(angle);
            angle += angleAdder;
        }
    }
}

float pesPolyline::wrapAngle(float angleRadians) {
    return pesWrap(angleRadians, 0.0f, (float)TWO_PI);
}

void pesPolyline::lineTo(float x, float y) { addVertex(pesVec2f(x, y)); }

void pesPolyline::lineTo(const pesVec2f& p) { addVertex(p); }

// void pesPolyline::bezierTo(const pesVec2f& cp1,
//                            const pesVec2f& cp2,
//                            const pesVec2f& to,
//                            int curveResolution) {
//     // if, and only if poly vertices has points, we can make a bezier
//     // from the last point
//     curveVertices.clear();

//     // the resolultion with which we computer this bezier
//     // is arbitrary, can we possibly make it dynamic?

//     if (size() > 0) {
//         float x0 = points[size() - 1].x;
//         float y0 = points[size() - 1].y;

//         float ax, bx, cx;
//         float ay, by, cy;
//         float t, t2, t3;
//         float x, y;

//         // polynomial coefficients
//         cx = 3.0f * (cp1.x - x0);
//         bx = 3.0f * (cp2.x - cp1.x) - cx;
//         ax = to.x - x0 - cx - bx;

//         cy = 3.0f * (cp1.y - y0);
//         by = 3.0f * (cp2.y - cp1.y) - cy;
//         ay = to.y - y0 - cy - by;

//         for (int i = 1; i <= curveResolution; i++) {
//             t = (float)i / (float)(curveResolution);
//             t2 = t * t;
//             t3 = t2 * t;
//             x = (ax * t3) + (bx * t2) + (cx * t) + x0;
//             y = (ay * t3) + (by * t2) + (cy * t) + y0;
//             points.emplace_back(x, y);
//         }
//     }
//     flagHasChanged();
// }
// GPT
void pesPolyline::bezierTo(const pesVec2f& cp1, const pesVec2f& cp2, const pesVec2f& to, int curveResolution) {
    // Clear curve vertices
    curveVertices.clear();

    // Ensure there are points to create a bezier curve from the last point
    if (points.empty()) {
        return;
    }

    // Get the last point
    float x0 = points.back().x;
    float y0 = points.back().y;

    // Polynomial coefficients
    float cx = 3.0f * (cp1.x - x0);
    float bx = 3.0f * (cp2.x - cp1.x) - cx;
    float ax = to.x - x0 - cx - bx;

    float cy = 3.0f * (cp1.y - y0);
    float by = 3.0f * (cp2.y - cp1.y) - cy;
    float ay = to.y - y0 - cy - by;

    // Calculate bezier points
    for (int i = 1; i <= curveResolution; ++i) {
        float t = static_cast<float>(i) / static_cast<float>(curveResolution);
        float t2 = t * t;
        float t3 = t2 * t;
        float x = (ax * t3) + (bx * t2) + (cx * t) + x0;
        float y = (ay * t3) + (by * t2) + (cy * t) + y0;
        points.emplace_back(x, y);
    }

    // Flag that the shape has changed
    flagHasChanged();
}

void pesPolyline::quadBezierTo(const pesVec2f& cp1,
                               const pesVec2f& cp2,
                               const pesVec2f& cp3,
                               int curveResolution) {
    curveVertices.clear();
    for (int i = 0; i <= curveResolution; i++) {
        double t = (double)i / (double)(curveResolution);
        double a = (1.0 - t) * (1.0 - t);
        double b = 2.0 * t * (1.0 - t);
        double c = t * t;
        double x = a * cp1.x + b * cp2.x + c * cp3.x;
        double y = a * cp1.y + b * cp2.y + c * cp3.y;
        points.emplace_back(x, y);
    }
    flagHasChanged();
}

void pesPolyline::curveTo(float x, float y, int curveResolution) {
    if (curveVertices.empty()) {
        if (!points.empty()) {
            pesVec2f last = points.back();
            curveVertices.push_back(last);
            curveVertices.push_back(last);  // double start point
        } else {
            curveVertices.push_back(pesVec2f(x, y));
        }
    }
    curveVertices.push_back(pesVec2f(x, y));

    if (curveVertices.size() == 4) {
        float x0 = curveVertices[0].x;
        float y0 = curveVertices[0].y;
        float x1 = curveVertices[1].x;
        float y1 = curveVertices[1].y;
        float x2 = curveVertices[2].x;
        float y2 = curveVertices[2].y;
        float x3 = curveVertices[3].x;
        float y3 = curveVertices[3].y;

        for (int i = 1; i <= curveResolution; i++) {
            float t = (float)i / (float)(curveResolution);
            t = pesClamp(t, 0.0, 1.0);
            float t2 = t * t;
            float t3 = t2 * t;

            float x = 0.5f * ((2.0f * x1) + (-x0 + x2) * t + (2.0f * x0 - 5.0f * x1 + 4 * x2 - x3) * t2 +
                        (-x0 + 3.0f * x1 - 3.0f * x2 + x3) * t3);

            float y = 0.5f * ((2.0f * y1) + (-y0 + y2) * t + (2.0f * y0 - 5.0f * y1 + 4 * y2 - y3) * t2 +
                        (-y0 + 3.0f * y1 - 3.0f * y2 + y3) * t3);

            points.emplace_back(x, y);
        }
        curveVertices.pop_front();
    }
    flagHasChanged();
}

void pesPolyline::curveTo(const pesVec2f& p, int curveResolution) {
    curveTo(p.x, p.y, curveResolution);
}

void pesPolyline::arc(const pesVec2f& center,
                      float radiusX,
                      float radiusY,
                      float angleBegin,
                      float angleEnd,
                      int circleResolution,
                      bool bClockwise) {
    bool clockwise = bClockwise;

    if (circleResolution <= 1) circleResolution = 2;
    setCircleResolution(circleResolution);
    points.reserve(points.size() + circleResolution);

    const float epsilon = 0.0001f;

    const size_t nCirclePoints = circlePoints.size();
    float segmentArcSize = (float)(M_TWO_PI / (float)nCirclePoints);

    // convert angles to radians and wrap them into the range 0-M_TWO_PI and
    float angleBeginRad = wrapAngle(angleBegin * DEG_TO_RAD);
    float angleEndRad = wrapAngle(angleEnd * DEG_TO_RAD);

    while (angleBeginRad >= angleEndRad) angleEndRad += (float)M_TWO_PI;

    // determine the directional angle delta
    float d = clockwise ? angleEndRad - angleBeginRad : angleBeginRad - angleEndRad;
    // find the shortest angle delta, clockwise delta direction yeilds POSITIVE values
    float deltaAngle = atan2(sin(d), cos(d));

    // establish the remaining angle that we have to work through
    float remainingAngle = deltaAngle;

    // if the delta angle is in the CCW direction OR the start and stop angles are
    // effectively the same adjust the remaining angle to be a be a full rotation
    if (deltaAngle < 0 || abs(deltaAngle) < epsilon) remainingAngle += (float)M_TWO_PI;

    pesVec2f radii(radiusX, radiusY);
    pesVec2f point;

    int currentLUTIndex = 0;
    bool isFirstPoint = true;  // special case for the first point

    while (remainingAngle > 0) {
        if (isFirstPoint) {
            // TODO: should this be the exact point on the circle or
            // should it be an intersecting point on the line that connects two
            // surrounding LUT points?
            //
            // get the EXACT first point requested (for points that
            // don't fall precisely on a LUT entry)
            point = pesVec2f(cos(angleBeginRad), sin(angleBeginRad));
            // set up the get any in between points from the LUT
            float ratio = (float)(angleBeginRad / M_TWO_PI * (float)nCirclePoints);
            currentLUTIndex = clockwise ? (int)ceil(ratio) : (int)floor(ratio);
            float lutAngleAtIndex = currentLUTIndex * segmentArcSize;
            // the angle between the beginning angle and the next angle in the LUT table
            float d = clockwise ? (lutAngleAtIndex - angleBeginRad)
                                : (angleBeginRad - lutAngleAtIndex);
            float firstPointDelta =
                    atan2(sin(d), cos(d));  // negative is in the clockwise direction

            // if the are "equal", get the next one CCW
            if (abs(firstPointDelta) < epsilon) {
                currentLUTIndex = clockwise ? (currentLUTIndex + 1) : (currentLUTIndex - 1);
                firstPointDelta = segmentArcSize;  // we start at the next lut point
            }

            // start counting from the offset
            remainingAngle -= firstPointDelta;
            isFirstPoint = false;
        } else {
            point = pesVec2f(circlePoints[currentLUTIndex].x, circlePoints[currentLUTIndex].y);
            if (clockwise) {
                currentLUTIndex++;                 // go to the next LUT point
                remainingAngle -= segmentArcSize;  // account for next point
                // if the angle overshoots, then the while loop will fail next time
            } else {
                currentLUTIndex--;                 // go to the next LUT point
                remainingAngle -= segmentArcSize;  // account for next point
                // if the angle overshoots, then the while loop will fail next time
            }
        }

        // keep the current lut index in range
        if (clockwise) {
            currentLUTIndex = currentLUTIndex % nCirclePoints;
        } else {
            if (currentLUTIndex < 0) currentLUTIndex = (int)nCirclePoints + currentLUTIndex;
        }

        // add the point to the poly line
        point = point * radii + center;
        points.push_back(point);

        // if the next LUT point moves us past the end angle then
        // add a a point a the exact end angle and call it finished
        if (remainingAngle < epsilon) {
            point = pesVec2f(cos(angleEndRad), sin(angleEndRad));
            point = point * radii + center;
            points.push_back(point);
            remainingAngle = 0;  // call it finished, the next while loop test will fail
        }
    }
    flagHasChanged();
}

void pesPolyline::arcNegative(const pesVec2f& center,
                              float radiusX,
                              float radiusY,
                              float angleBegin,
                              float angleEnd,
                              int circleResolution) {
    arc(center, radiusX, radiusY, angleBegin, angleEnd, circleResolution, false);
}

//----------------------------------------------------------
float pesPolyline::getPerimeter() const {
    if (points.size() < 2) {
        return 0;
    } else {
        updateCache();
        if (lengths.size()) {
            return lengths.back();
        }

        pesRectangle bbox = getBoundingBox();
        float l = sqrt(bbox.height * bbox.height + bbox.width * bbox.width);
        return l;
    }
}

//----------------------------------------------------------
float pesPolyline::getArea() const {
    updateCache();
    return area;
}

//----------------------------------------------------------
pesVec2f pesPolyline::getCentroid2D() const {
    updateCache();
    return centroid2D;
}

//----------------------------------------------------------
pesRectangle pesPolyline::getBoundingBox() const {
    //    updateCache();
    // bounding box
    bool first = true;
    for (const auto& point : points) {
        if (first) {
            first = false;
            bound.set(point, 0, 0);
        } else {
            bound.growToInclude(point);
        }
    }

    return bound;
}

//----------------------------------------------------------
pesPolyline pesPolyline::getSmoothed(int smoothingSize, float smoothingShape) const {
    int n = static_cast<int>(size());
    smoothingSize = static_cast<int>(pesClamp(static_cast<float>(smoothingSize), 0.0f, static_cast<float>(n)));
    smoothingShape = pesClamp(smoothingShape, 0.0f, 1.0f);

    // precompute weights and normalization
    vector<float> weights(smoothingSize);
    // side weights
    for (int i = 1; i < smoothingSize; ++i) {
        weights[i] = pesMap(static_cast<float>(i), 0.0f, static_cast<float>(smoothingSize), 1.0f, smoothingShape);
    }

    // make a copy of this polyline
    pesPolyline result = *this;

    for (int i = 0; i < n; i++) {
        float sum = 1.0f;  // center weight
        for (int j = 1; j < smoothingSize; j++) {
            pesVec2f cur;
            int leftPosition = i - j;
            int rightPosition = i + j;
            if (leftPosition < 0 && bClosed) {
                leftPosition += n;
            }
            if (leftPosition >= 0) {
                cur += points[leftPosition];
                sum += weights[j];
            }
            if (rightPosition >= n && bClosed) {
                rightPosition -= n;
            }
            if (rightPosition < n) {
                cur += points[rightPosition];
                sum += weights[j];
            }
            result[i] += cur * weights[j];
        }
        result[i] /= sum;
    }

    return result;
}

//----------------------------------------------------------
pesPolyline pesPolyline::getResampledBySpacing(float spacing) const {
    if (spacing == 0 || size() == 0) return *this;
    pesPolyline poly;
    float totalLength = getPerimeter();
    for (float f = 0; f < totalLength; f += spacing) {
        pesVec2f p = getPointAtLength(f);
        poly.lineTo(p.x, p.y);
    }

    if (!isClosed()) {
        if (poly.size() > 0) poly.points.back() = points.back();
        poly.setClosed(false);
    } else {
        poly.setClosed(true);
    }

    return poly;
}

//----------------------------------------------------------
pesPolyline pesPolyline::getResampledByCount(int count) const {
    float perimeter = getPerimeter();
    if (count < 2) {
        count = 2;
    }
    return pesPolyline::getResampledBySpacing(perimeter / (count - 1));
}

//----------------------------------------------------------
// pom--v2
pesPolyline pesPolyline::getResampledByCount2(int count) const {
    if (size() == 0) return *this;

    if (count < 2) {
        count = 2;
    }

    pesPolyline poly;
    float fstep = 1.0f / count;
    for (int i = 0; i <= count; i++) {
        float percent = fstep * i;
        if (percent > 1.0) percent = 1.0;
        pesVec2f p = getPointAtPercent(percent);
        poly.lineTo(p.x, p.y);
    }

    if (!isClosed()) {
        //        if(poly.size() > 0) poly.points.back() = points.back();
        poly.setClosed(false);
    } else {
        poly.setClosed(true);
    }

    return poly;
}

//----------------------------------------------------------
// http://local.wasp.uwa.edu.au/~pbourke/geometry/pointline/
static pesVec2f pes_getClosestPointUtil(const pesVec2f& p1,
                                        const pesVec2f& p2,
                                        const pesVec2f& p3,
                                        float* normalizedPosition) {
    // if p1 is coincident with p2, there is no line
    if (p1 == p2) {
        if (normalizedPosition != nullptr) {
            *normalizedPosition = 0;
        }
        return p1;
    }

    float u = (p3.x - p1.x) * (p2.x - p1.x);
    u += (p3.y - p1.y) * (p2.y - p1.y);
    // perfect place for fast inverse sqrt...
    float len = (p2 - p1).length();
    u /= (len * len);

    // clamp u
    if (u > 1) {
        u = 1;
    } else if (u < 0) {
        u = 0;
    }
    if (normalizedPosition != nullptr) {
        *normalizedPosition = u;
    }
    return p1.getInterpolated(p2, u);
}

//----------------------------------------------------------
// a much faster but less accurate version would check distances to vertices first,
// which assumes vertices are evenly spaced
pesVec2f pesPolyline::getClosestPoint(const pesVec2f& target, unsigned int* nearestIndex) const {
    const pesPolyline& polyline = *this;

    if (polyline.size() < 2) {
        if (nearestIndex != nullptr) {
            *nearestIndex = 0;
        }
        return target;
    }

    float distance = 0;
    pesVec2f nearestPoint;
    unsigned int nearest = 0;
    float normalizedPosition = 0;
    unsigned int lastPosition = (int)polyline.size() - 1;
    if (polyline.isClosed()) {
        lastPosition++;
    }
    for (int i = 0; i < (int)lastPosition; i++) {
        bool repeatNext = i == (int)(polyline.size() - 1);

        const pesVec2f& cur = polyline[i];
        const pesVec2f& next = repeatNext ? polyline[0] : polyline[i + 1];

        float curNormalizedPosition = 0;
        pesVec2f curNearestPoint =
                pes_getClosestPointUtil(cur, next, target, &curNormalizedPosition);
        float curDistance = curNearestPoint.distance(target);
        if (i == 0 || curDistance < distance) {
            distance = curDistance;
            nearest = i;
            nearestPoint = curNearestPoint;
            normalizedPosition = curNormalizedPosition;
        }
    }

    if (nearestIndex != nullptr) {
        if (normalizedPosition > .5) {
            nearest++;
            if (nearest == polyline.size()) {
                nearest = 0;
            }
        }
        *nearestIndex = nearest;
    }

    return nearestPoint;
}

//--------------------------------------------------
bool pesPolyline::inside(const pesVec2f& p, const pesPolyline& polyline) {
    return pesPolyline::inside(p.x, p.y, polyline);
}

//--------------------------------------------------
// bool pesPolyline::inside(float x, float y, const pesPolyline& polyline) {
//     pesRectangle bbox = polyline.getBoundingBox();
//     if (bbox.isEmpty() == false) {
//         if (bbox.inside(x, y) == false) return false;
//     }

//     int counter = 0;
//     int i;
//     double xinters;
//     pesVec2f p1, p2;

//     int N = (int)polyline.size();

//     p1 = polyline[0];
//     for (i = 1; i <= N; i++) {
//         p2 = polyline[i % N];
//         if (y > MIN(p1.y, p2.y)) {
//             if (y <= MAX(p1.y, p2.y)) {
//                 if (x <= MAX(p1.x, p2.x)) {
//                     if (p1.y != p2.y) {
//                         xinters = (y - p1.y) * (p2.x - p1.x) / (p2.y - p1.y) + p1.x;
//                         if (p1.x == p2.x || x <= xinters) counter++;
//                     }
//                 }
//             }
//         }
//         p1 = p2;
//     }

//     if (counter % 2 == 0)
//         return false;
//     else
//         return true;
// }
// GPT
bool pesPolyline::inside(float x, float y, const pesPolyline& polyline) {
    pesRectangle bbox = polyline.getBoundingBox();
    if (!bbox.isEmpty() && !bbox.inside(x, y)) {
        return false;
    }

    int counter = 0;
    double xinters;
    pesVec2f p1 = polyline[0];
    int N = static_cast<int>(polyline.size());

    for (int i = 1; i <= N; ++i) {
        pesVec2f p2 = polyline[i % N];
        if (y > std::min(p1.y, p2.y) && y <= std::max(p1.y, p2.y) && x <= std::max(p1.x, p2.x)) {
            if (p1.y != p2.y) {
                xinters = (y - p1.y) * (p2.x - p1.x) / (p2.y - p1.y) + p1.x;
                if (p1.x == p2.x || x <= xinters) {
                    counter++;
                }
            }
        }
        p1 = p2;
    }

    return counter % 2 != 0;
}

//--------------------------------------------------
bool pesPolyline::inside(float x, float y) const { return pesPolyline::inside(x, y, *this); }

//--------------------------------------------------
bool pesPolyline::inside(const pesVec2f& p) const { return pesPolyline::inside(p, *this); }

struct pesSegment {
    pesVec2f P0;
    pesVec2f P1;
};

// static void pes_simplifyDP(float tol, pesVec2f* v, int j, int k, int* mk) {
//     if (k <= j + 1)  // there is nothing to simplify
//         return;

//     // check for adequate approximation by segment S from v[j] to v[k]
//     int maxi = j;                 // index of vertex farthest from S
//     float maxd2 = 0;              // distance squared of farthest vertex
//     float tol2 = tol * tol;       // tolerance squared
//     pesSegment S = {v[j], v[k]};  // segment from v[j] to v[k]
//     auto u = S.P1 - S.P0;         // segment direction vector
//     double cu = u.dot(u);         // segment length squared

//     // test each vertex v[i] for max distance from S
//     // compute using the Feb 2001 Algorithm's dist_ofPoint_to_Segment()
//     // Note: this works in any dimension (2D, 3D, ...)
//     pesVec2f w;
//     pesVec2f Pb;       // base of perpendicular from v[i] to S
//     float b, cw, dv2;  // dv2 = distance v[i] to S squared

//     for (int i = j + 1; i < k; i++) {
//         // compute distance squared
//         w = v[i] - S.P0;
//         cw = w.dot(u);
//         if (cw <= 0)
//             dv2 = (v[i] - S.P0).lengthSquared();
//         else if (cu <= cw)
//             dv2 = (v[i] - S.P1).lengthSquared();
//         else {
//             b = (float)(cw / cu);
//             Pb = S.P0 + u * b;
//             dv2 = (v[i] - Pb).lengthSquared();
//         }
//         // test with current max distance squared
//         if (dv2 <= maxd2) continue;

//         // v[i] is a new max vertex
//         maxi = i;
//         maxd2 = dv2;
//     }
//     if (maxd2 > tol2)  // error is worse than the tolerance
//     {
//         // split the polyline at the farthest vertex from S
//         mk[maxi] = 1;  // mark v[maxi] for the simplified polyline
//         // recursively simplify the two subpolylines at v[maxi]
//         pes_simplifyDP(tol, v, j, maxi, mk);  // polyline v[j] to v[maxi]
//         pes_simplifyDP(tol, v, maxi, k, mk);  // polyline v[maxi] to v[k]
//     }
//     // else the approximation is OK, so ignore intermediate vertices
// }
// GPT
static void pes_simplifyDP(float tol, const pesVec2f* v, int j, int k, int* mk) {
    if (k <= j + 1) {  // there is nothing to simplify
        return;
    }

    // Check for adequate approximation by segment S from v[j] to v[k]
    int maxi = j;                 // Index of vertex farthest from S
    float maxd2 = 0;              // Distance squared of farthest vertex
    float tol2 = tol * tol;       // Tolerance squared
    pesSegment S = {v[j], v[k]};  // Segment from v[j] to v[k]
    pesVec2f u = S.P1 - S.P0;     // Segment direction vector
    float cu = u.dot(u);          // Segment length squared

    // Test each vertex v[i] for max distance from S
    // Compute using the Feb 2001 Algorithm's dist_ofPoint_to_Segment()
    // Note: this works in any dimension (2D, 3D, ...)
    for (int i = j + 1; i < k; ++i) {
        pesVec2f w = v[i] - S.P0;
        float cw = w.dot(u);
        float dv2;

        if (cw <= 0) {
            dv2 = w.lengthSquared();
        } else if (cu <= cw) {
            dv2 = (v[i] - S.P1).lengthSquared();
        } else {
            float b = cw / cu;
            pesVec2f Pb = S.P0 + u * b;
            dv2 = (v[i] - Pb).lengthSquared();
        }

        // Test with current max distance squared
        if (dv2 > maxd2) {
            // v[i] is a new max vertex
            maxi = i;
            maxd2 = dv2;
        }
    }

    if (maxd2 > tol2) {  // error is worse than the tolerance
        // split the polyline at the farthest vertex from S
        mk[maxi] = 1;  // mark v[maxi] for the simplified polyline
        // recursively simplify the two subpolylines at v[maxi]
        pes_simplifyDP(tol, v, j, maxi, mk);  // polyline v[j] to v[maxi]
        pes_simplifyDP(tol, v, maxi, k, mk);  // polyline v[maxi] to v[k]
    }
}

// void pesPolyline::simplify(float tol) {
//     if (points.size() < 2) return;
//     int n = (int)size();

//     if (n == 0) {
//         return;
//     }

//     vector<pesVec2f> sV;
//     sV.resize(n);

//     int i, k, m, pv;         // misc counters
//     float tol2 = tol * tol;  // tolerance squared
//     vector<pesVec2f> vt;
//     vector<int> mk;
//     vt.resize(n);
//     mk.resize(n, 0);

//     // STAGE 1.  Vertex Reduction within tolerance of prior vertex cluster
//     vt[0] = points[0];  // start at the beginning
//     for (i = k = 1, pv = 0; i < n; i++) {
//         pesVec2f diff = points[i] - points[pv];
//         if (diff.lengthSquared() < tol2) continue;

//         vt[k++] = points[i];
//         pv = i;
//     }
//     if (pv < n - 1) vt[k++] = points[n - 1];  // finish at the end

//     // STAGE 2.  Douglas-Peucker polyline simplification
//     mk[0] = mk[k - 1] = 1;  // mark the first and last vertices
//     pes_simplifyDP(tol, &vt[0], 0, k - 1, &mk[0]);

//     // copy marked vertices to the output simplified polyline
//     for (i = m = 0; i < k; i++) {
//         if (mk[i]) sV[m++] = vt[i];
//     }

//     // get rid of the unused points
//     if (m < (int)sV.size()) {
//         points.assign(sV.begin(), sV.begin() + m);
//     } else {
//         points = sV;
//     }
// }
// GPT
void pesPolyline::simplify(float tol) {
    if (points.size() < 2) return;

    int n = static_cast<int>(points.size());
    if (n == 0) return;

    vector<pesVec2f> sV(n);
    vector<pesVec2f> vt(n);
    vector<int> mk(n, 0);

    float tol2 = tol * tol;  // tolerance squared

    // STAGE 1. Vertex Reduction within tolerance of prior vertex cluster
    vt[0] = points[0];  // start at the beginning
    int k = 1;
    int pv = 0;
    for (int i = 1; i < n; ++i) {
        pesVec2f diff = points[i] - points[pv];
        if (diff.lengthSquared() < tol2) continue;

        vt[k++] = points[i];
        pv = i;
    }
    if (pv < n - 1) vt[k++] = points[n - 1];  // finish at the end

    // STAGE 2. Douglas-Peucker polyline simplification
    mk[0] = mk[k - 1] = 1;  // mark the first and last vertices
    pes_simplifyDP(tol, vt.data(), 0, k - 1, mk.data());

    // Copy marked vertices to the output simplified polyline
    int m = 0;
    for (int i = 0; i < k; ++i) {
        if (mk[i]) sV[m++] = vt[i];
    }

    // Get rid of the unused points
    sV.resize(m);
    points = std::move(sV);

    flagHasChanged();
}

//--------------------------------------------------
void pesPolyline::setRightVector(pesVec3f v) {
    rightVector = v;
    flagHasChanged();
}

//--------------------------------------------------
pesVec3f pesPolyline::getRightVector() const { return rightVector; }

//--------------------------------------------------
float pesPolyline::getIndexAtLength(float length) const {
    if (points.size() < 2) return 0;
    updateCache();

    float totalLength = getPerimeter();
    length = pesClamp(length, 0, totalLength);

    int lastPointIndex = isClosed() ? static_cast<int>(points.size()) : static_cast<int>(points.size()) - 1;

    int i1 = static_cast<int>(pesClamp(floor(length / totalLength * lastPointIndex), 0.0f, lengths.size() - 2.0f));
    int leftLimit = 0;
    int rightLimit = lastPointIndex;

    if (lengths.empty()) {
        return 0;
    }
    for (int iterations = 0; iterations < 32; iterations++) {  // limit iterations
        i1 = (int)pesClamp((float)i1, 0.0f, lengths.size() - 1.0f);
        float distAt1 = lengths[i1];
        if (distAt1 <= length) {  // if Length at i1 is less than desired Length (this is good)
            float distAt2 = lengths[i1 + 1];
            if (distAt2 >= length) {
                float t = pesMap(length, distAt1, distAt2, 0, 1);
                return i1 + t;
            } else {
                leftLimit = i1;
            }
        } else {
            rightLimit = i1;
        }
        i1 = (leftLimit + rightLimit) / 2;
    }
    return 0;
}

//--------------------------------------------------
float pesPolyline::getIndexAtPercent(float f) const { return getIndexAtLength(f * getPerimeter()); }

//--------------------------------------------------
float pesPolyline::getLengthAtIndex(int index) const {
    if (points.size() < 2) return 0;
    updateCache(); // Jimmy***
    return lengths[getWrappedIndex(index)];
}

//--------------------------------------------------
float pesPolyline::getLengthAtIndexInterpolated(float findex) const {
    if (points.size() < 2) return 0;
    updateCache();
    int i1, i2;
    float t;
    getInterpolationParams(findex, i1, i2, t);
    return pesLerp(getLengthAtIndex(i1), getLengthAtIndex(i2), t);
}

//--------------------------------------------------
pesVec2f pesPolyline::getPointAtLength(float f) const {
    if (points.size() < 2) return pesVec2f();
    updateCache();
    return getPointAtIndexInterpolated(getIndexAtLength(f));
}

//--------------------------------------------------
pesVec2f pesPolyline::getPointAtPercent(float f) const {
    float length = getPerimeter();
    return getPointAtLength(f * length);
}

//--------------------------------------------------
pesVec2f pesPolyline::getPointAtIndexInterpolated(float findex) const {
    if (points.size() < 2) return pesVec2f();
    int i1, i2;
    float t;
    getInterpolationParams(findex, i1, i2, t);
    pesVec2f leftPoint(points[i1]);
    pesVec2f rightPoint(points[i2]);
    return leftPoint.getInterpolated(rightPoint, t);
}

//--------------------------------------------------
float pesPolyline::getAngleAtIndex(int index) const {
    if (points.size() < 2) return 0;
    updateCache();
    return angles[getWrappedIndex(index)];
}

//--------------------------------------------------
float pesPolyline::getAngleAtIndexInterpolated(float findex) const {
    if (points.size() < 2) return 0;
    int i1, i2;
    float t;
    getInterpolationParams(findex, i1, i2, t);
    return pesLerp(getAngleAtIndex(i1), getAngleAtIndex(i2), t);
}

//--------------------------------------------------
pesVec3f pesPolyline::getRotationAtIndex(int index) const {
    if (points.size() < 2) return pesVec3f();
    updateCache();
    return rotations[getWrappedIndex(index)];
}

//--------------------------------------------------
pesVec3f pesPolyline::getRotationAtIndexInterpolated(float findex) const {
    if (points.size() < 2) return pesVec3f();
    int i1, i2;
    float t;
    getInterpolationParams(findex, i1, i2, t);
    return getRotationAtIndex(i1).getInterpolated(getRotationAtIndex(i2), t);
}

//--------------------------------------------------
pesVec3f pesPolyline::getTangentAtIndex(int index) const {
    if (points.size() < 2) return pesVec3f();
    updateCache();
    return tangents[getWrappedIndex(index)];
}

//--------------------------------------------------
pesVec3f pesPolyline::getTangentAtIndexInterpolated(float findex) const {
    if (points.size() < 2) return pesVec3f();
    int i1, i2;
    float t;
    getInterpolationParams(findex, i1, i2, t);
    return getTangentAtIndex(i1).getInterpolated(getTangentAtIndex(i2), t);
}

//--------------------------------------------------
pesVec3f pesPolyline::getNormalAtIndex(int index) const {
    if (points.size() < 2) return pesVec3f();
    updateCache();
    return normals[getWrappedIndex(index)];
}

//--------------------------------------------------
pesVec3f pesPolyline::getNormalAtIndexInterpolated(float findex) const {
    if (points.size() < 2) return pesVec3f();
    int i1, i2;
    float t;
    getInterpolationParams(findex, i1, i2, t);
    return getNormalAtIndex(i1).getInterpolated(getNormalAtIndex(i2), t);
}

//--------------------------------------------------
void pesPolyline::calcData(
        int index, pesVec3f& tangent, float& angle, pesVec3f& rotation, pesVec3f& normal) const {
    int i1 = getWrappedIndex(index - 1);
    int i2 = getWrappedIndex(index);
    int i3 = getWrappedIndex(index + 1);

    const pesVec3f& p1 = points[i1];
    const pesVec3f& p2 = points[i2];
    const pesVec3f& p3 = points[i3];

    pesVec3f v1(p1 - p2);  // vector to previous point
    pesVec3f v2(p3 - p2);  // vector to next point
    v1.normalize();
    v2.normalize();

    tangent = (v2 - v1);
    tangent.normalize();

    rotation = v1.getCrossed(v2);
    angle = 180 - (acos(pesClamp(v1.x * v2.x + v1.y * v2.y + v1.z * v2.z, -1, 1)) * RAD_TO_DEG);

    normal = rightVector.getCrossed(tangent);
    normal.normalize();
}

//--------------------------------------------------
// int pesPolyline::getWrappedIndex(int index) const {
//     if (points.empty()) return 0;

//     if (index < 0) return isClosed() ? (int)(index + points.size()) % points.size() : 0;
//     if (index > int(points.size()) - 1)
//         return isClosed() ? index % (int)points.size() : (int)points.size() - 1;
//     return index;
// }
// GPT
int pesPolyline::getWrappedIndex(int index) const {
    if (points.empty()) return 0;

    int size = static_cast<int>(points.size());
    if (index < 0) {
        return isClosed() ? (index + size) % size : 0;
    }
    if (index >= size) {
        return isClosed() ? index % size : size - 1;
    }
    return index;
}

//--------------------------------------------------
void pesPolyline::getInterpolationParams(float findex, int& i1, int& i2, float& t) const {
    i1 = static_cast<int>(floor(findex));
    t = findex - i1;
    i1 = getWrappedIndex(i1);
    i2 = getWrappedIndex(i1 + 1);
}

//--------------------------------------------------
void pesPolyline::updateCache(bool bForceUpdate) const {
    if (bCacheIsDirty || bForceUpdate) {
        lengths.clear();
        angles.clear();
        rotations.clear();
        normals.clear();
        tangents.clear();
        area = 0;
        bound.set(0, 0, 0, 0);
        centroid2D.set(0, 0);
        bCacheIsDirty = false;

        if (points.size() < 2) return;

        // area
        for (int i = 0; i < (int)points.size() - 1; i++) {
            area += points[i].x * points[i + 1].y - points[i + 1].x * points[i].y;
        }
        area += points[points.size() - 1].x * points[0].y -
                points[0].x * points[points.size() - 1].y;
        area *= 0.5;

        //        // bounding box
        //        for(size_t i = 0; i < size(); i++) {
        //            if(i == 0) {
        //                bound.set(points[i],0,0);
        //            } else {
        //                bound.growToInclude(points[i]);
        //            }
        //        }

        if (fabsf(area) < FLT_EPSILON) {
            centroid2D = getBoundingBox().getCenter();
        } else {
            // centroid
            // TODO: doesn't seem to work on all concave shapes
            for (int i = 0; i < (int)points.size() - 1; i++) {
                centroid2D.x += (points[i].x + points[i + 1].x) *
                                (points[i].x * points[i + 1].y - points[i + 1].x * points[i].y);
                centroid2D.y += (points[i].y + points[i + 1].y) *
                                (points[i].x * points[i + 1].y - points[i + 1].x * points[i].y);
            }
            centroid2D.x += (points[points.size() - 1].x + points[0].x) *
                            (points[points.size() - 1].x * points[0].y -
                             points[0].x * points[points.size() - 1].y);
            centroid2D.y += (points[points.size() - 1].y + points[0].y) *
                            (points[points.size() - 1].x * points[0].y -
                             points[0].x * points[points.size() - 1].y);

            centroid2D.x /= (6 * area);
            centroid2D.y /= (6 * area);
        }

        // per vertex cache
        lengths.resize(points.size());
        tangents.resize(points.size());
        angles.resize(points.size());
        normals.resize(points.size());
        rotations.resize(points.size());

        float angle;
        pesVec3f rotation;
        pesVec3f normal;
        pesVec3f tangent;

        float length = 0;
        for (int i = 0; i < (int)points.size(); i++) {
            lengths[i] = length;

            calcData(i, tangent, angle, rotation, normal);
            tangents[i] = tangent;
            angles[i] = angle;
            rotations[i] = rotation;
            normals[i] = normal;

            length += points[i].distance(points[getWrappedIndex(i + 1)]);
        }

        if (isClosed()) lengths.push_back(length);
    }
}

pesPolyline pesPolyline::getShrink(float compensate) const { return getGrow(-compensate); }

pesPolyline pesPolyline::getGrow(float compensate) const {
    if (isCounterClockWise()) compensate *= -1;

    vector<pesVec2f> outer;
    outer.reserve(points.size());

    for (const auto& point : points) {
        pesVec3f n = getNormalAtIndex(&point - &points[0]);
        pesVec2f normal(n.x, n.y);
        outer.push_back(point + normal * compensate);
    }

    pesPolyline poly(outer);
    if (isClosed()) {
        poly.close();
    }

    return poly;
}

void pesPolyline::shrink(float compensate) { grow(-compensate); }

void pesPolyline::grow(float compensate) {
    if (isCounterClockWise()) compensate *= -1;

    for (auto& point : points) {
        pesVec3f n = getNormalAtIndex(&point - &points[0]);
        pesVec2f normal(n.x, n.y);
        point += normal * compensate;
    }

    flagHasChanged();
}

int pesPolyline::getTotalDistance(){
    int sum = 0;
    for (size_t i = 0; i < points.size() - 1; ++i) {
        sum += points[i].distance(points[i + 1]);
    }
    return sum;
}

bool pesPolyline::isClockWise() const { return getArea() > 0; }

bool pesPolyline::isCounterClockWise() const { return getArea() < 0; }
