//
//  pesPolyline.hpp
//  pesEngine
//
//  Created by SIHARAT THAMMAYA on 2/7/2562 BE.
//  Copyright © 2562 SIHARAT THAMMAYA. All rights reserved.
//

#ifndef pesPolyline_hpp
#define pesPolyline_hpp

#include <stdio.h>
#include <deque>
#include <vector>
#include "pesRectangle.hpp"
#include "pesVec3f.h"

class pesPolyline {
public:
    pesPolyline();
    pesPolyline(const std::vector<pesVec2f>& points);

    static pesPolyline fromRect(const pesRectangle& rect);

    void clear();
    void close();
    void setClosed(bool tf);
    bool isClosed() const;

    void addVertex(const pesVec2f& point);
    void addVertex(float x, float y);
    void addVertices(const pesVec2f* verts, int numverts);
    void addVertices(const std::vector<pesVec2f>& points);

    void insert(const pesVec2f& point, int index);
    void insert(const std::vector<pesVec2f>& points, int index);

    void resize(size_t size);
    size_t size() const;

    const pesVec2f& operator[](int index) const;
    pesVec2f& operator[](int index);

    std::vector<pesVec2f>& getVertices();
    const std::vector<pesVec2f>& getVertices() const;

    std::vector<pesVec2f>::iterator begin();
    std::vector<pesVec2f>::const_iterator begin() const;
    std::vector<pesVec2f>::reverse_iterator rbegin();
    std::vector<pesVec2f>::const_reverse_iterator rbegin() const;
    std::vector<pesVec2f>::iterator end();
    std::vector<pesVec2f>::const_iterator end() const;
    std::vector<pesVec2f>::reverse_iterator rend();
    std::vector<pesVec2f>::const_reverse_iterator rend() const;

    void lineTo(float x, float y);
    void lineTo(const pesVec2f& p);

    void curveTo(float x, float y, int curveResolution = 20);
    void curveTo(const pesVec2f& p, int curveResolution = 20);

    void bezierTo(const pesVec2f& cp1,
                  const pesVec2f& cp2,
                  const pesVec2f& p,
                  int curveResolution = 20);

    void quadBezierTo(const pesVec2f& cp1,
                      const pesVec2f& cp2,
                      const pesVec2f& p,
                      int curveResolution = 20);

    void arc(const pesVec2f& centre,
             float radiusX,
             float radiusY,
             float angleBegin,
             float angleEnd,
             int circleResolution = 20,
             bool bClockwise = true);
    void arcNegative(const pesVec2f& centre,
                     float radiusX,
                     float radiusY,
                     float angleBegin,
                     float angleEnd,
                     int circleResolution = 20);

    bool hasChanged();
    void flagHasChanged();

    void simplify(float tol = 0.2);

    pesPolyline getSmoothed(int smoothingSize, float smoothingShape = 0) const;

    pesPolyline getResampledBySpacing(float spacing) const;
    pesPolyline getResampledByCount(int count) const;
    pesPolyline getResampledByCount2(int count) const;

    static bool inside(float x, float y, const pesPolyline& polyline);
    bool inside(float x, float y) const;
    static bool inside(const pesVec2f& p, const pesPolyline& polyline);
    bool inside(const pesVec2f& p) const;

    pesRectangle getBoundingBox() const;
    float getPerimeter() const;
    float getArea() const;
    pesVec2f getCentroid2D() const;
    pesVec2f getClosestPoint(const pesVec2f& target, unsigned int* nearestIndex = nullptr) const;

    float getIndexAtLength(float f) const;
    float getIndexAtPercent(float f) const;
    float getLengthAtIndex(int index) const;
    float getLengthAtIndexInterpolated(float findex) const;
    pesVec2f getPointAtLength(float f) const;
    pesVec2f getPointAtPercent(float f) const;
    pesVec2f getPointAtIndexInterpolated(float findex) const;
    float getAngleAtIndex(int index) const;
    float getAngleAtIndexInterpolated(float findex) const;
    pesVec3f getRotationAtIndex(int index) const;
    pesVec3f getRotationAtIndexInterpolated(float findex) const;
    pesVec3f getTangentAtIndex(int index) const;
    pesVec3f getTangentAtIndexInterpolated(float findex) const;
    pesVec3f getNormalAtIndex(int index) const;
    pesVec3f getNormalAtIndexInterpolated(float findex) const;
    int getWrappedIndex(int index) const;
    void setRightVector(pesVec3f v = pesVec3f(0, 0, -1));
    pesVec3f getRightVector() const;

    pesPolyline getShrink(float compensate) const;
    pesPolyline getGrow(float compensate) const;
    void shrink(float compensate);
    void grow(float compensate);
    
    int getTotalDistance();

    bool isClockWise() const;
    bool isCounterClockWise() const;

private:
    void setCircleResolution(int res);
    float wrapAngle(float angleRad);

    std::vector<pesVec2f> points;
    std::deque<pesVec2f> curveVertices;
    std::vector<pesVec2f> circlePoints;
    pesVec3f rightVector;

    // cache
    mutable std::vector<float> lengths;
    mutable std::vector<pesVec3f> tangents;  // tangent at vertex, stored per point
    mutable std::vector<pesVec3f> normals;   //
    mutable std::vector<pesVec3f>
            rotations;  // rotation between adjacent segments, stored per point (cross product)
    mutable std::vector<float> angles;  // angle (degrees) between adjacent segments, stored per
                                        // point (asin(cross product))
    mutable pesVec2f centroid2D;
    mutable float area;
    mutable pesRectangle bound;

    bool bClosed;
    bool bHasChanged;
    mutable bool bCacheIsDirty;  // used only internally, no public API to read

    void updateCache(bool bForceUpdate = false) const;

    // given an interpolated index (e.g. 5.75) return neighboring indices and interolation factor
    // (e.g. 5, 6, 0.75)
    void getInterpolationParams(float findex, int& i1, int& i2, float& t) const;
    void calcData(
            int index, pesVec3f& tangent, float& angle, pesVec3f& rotation, pesVec3f& normal) const;
};

#endif /* pesPolyline_hpp */
