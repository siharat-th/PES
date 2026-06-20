//
//  pesRectangle.cpp
//  pesEngine
//
//  Created by SIHARAT THAMMAYA on 2/7/2562 BE.
//  Copyright © 2562 SIHARAT THAMMAYA. All rights reserved.
//

#include "pesRectangle.hpp"
#include "pesMath.hpp"
#include <float.h>

#if defined(_MSC_VER)
#include <limits>
#endif

using namespace std;

pesRectangle::pesRectangle(){
    set(0,0,0,0);
}

//----------------------------------------------------------
pesRectangle::~pesRectangle(){}

//----------------------------------------------------------
pesRectangle::pesRectangle(float px, float py, float w, float h){
    set(px,py,w,h);
}

pesRectangle::pesRectangle(const pesRectangle & r){
    set(r.x, r.y, r.width, r.height);
}

void pesRectangle::set(float px, float py, float w, float h) {
    x        = px;
    y        = py;
    width    = w;
    height    = h;
}

void pesRectangle::set(const pesVec2f& p, float w, float h) {
    set(p.x, p.y, w, h);
}

void pesRectangle::set(const pesRectangle & rect){
    set(rect.x, rect.y, rect.width, rect.height);
}

//----------------------------------------------------------
void pesRectangle::setFromCenter(float px, float py, float w, float h) {
    set(px - w*0.5f, py - h*0.5f, w, h);
}

//----------------------------------------------------------
void pesRectangle::setFromCenter(const pesVec2f& p, float w, float h) {
    setFromCenter(p.x, p.y, w, h);
}

//----------------------------------------------------------
void pesRectangle::growToInclude(float px, float py) {
    growToInclude(pesVec2f(px,py));
}

//----------------------------------------------------------
void pesRectangle::growToInclude(const pesVec2f& p) {
    float x0 = MIN(getMinX(),p.x);
    float x1 = MAX(getMaxX(),p.x);
    float y0 = MIN(getMinY(),p.y);
    float y1 = MAX(getMaxY(),p.y);
    float w = x1 - x0;
    float h = y1 - y0;
    set(x0,y0,w,h);
}

void pesRectangle::growToInclude(const pesRectangle& rect){
    float x0 = MIN(getMinX(),rect.getMinX());
    float x1 = MAX(getMaxX(),rect.getMaxX());
    float y0 = MIN(getMinY(),rect.getMinY());
    float y1 = MAX(getMaxY(),rect.getMaxY());
    float w = x1 - x0;
    float h = y1 - y0;
    set(x0,y0,w,h);
}

float pesRectangle::getMinX() const {
    return MIN(x, x + width);  // - width
}

//----------------------------------------------------------
float pesRectangle::getMaxX() const {
    return MAX(x, x + width);  // - width
}

//----------------------------------------------------------
float pesRectangle::getMinY() const{
    return MIN(y, y + height);  // - height
}

//----------------------------------------------------------
float pesRectangle::getMaxY() const {
    return MAX(y, y + height);  // - height
}

pesVec2f pesRectangle::getPosition() {
    return pesVec2f(x, y);
}

//----------------------------------------------------------
pesVec2f pesRectangle::getCenter() const {
    return pesVec2f(x + width * 0.5f, y + height * 0.5f);
}

bool pesRectangle::intersects(const pesRectangle& rect) const{
    return (getMinX() < rect.getMaxX() && getMaxX() > rect.getMinX() &&
            getMinY() < rect.getMaxY() && getMaxY() > rect.getMinY());
}

bool pesRectangle::inside(float px, float py) const{
    return px > getMinX() && py > getMinY() && px < getMaxX() && py < getMaxY();
}

void pesRectangle::translate(float dx, float dy) {
    x += dx;
    y += dy;
}

void pesRectangle::scale(float sX, float sY){
    width  *= sX;
    height *= sY;
}

void pesRectangle::scaleFromCenter(float sX, float sY){
    if(sX == 1.0f && sY == 1.0f) return; // nothing to do
    
    float newWidth  = width  * sX;
    float newHeight = height * sY;
    
    pesVec2f center = getCenter();
    
    x = center.x - newWidth  / 2.0f;
    y = center.y - newHeight / 2.0f;
    
    width  = newWidth;
    height = newHeight;
}

pesRectangle pesRectangle::getIntersection(const pesRectangle& rect) const{
    float x0 = MAX(getMinX(),rect.getMinX());
    float x1 = MIN(getMaxX(),rect.getMaxX());
    
    float w = x1 - x0;
    if(w < 0.0f) return pesRectangle(0,0,0,0); // short circuit if needed
    
    float y0 = MAX(getMinY(),rect.getMinY());
    float y1 = MIN(getMaxY(),rect.getMaxY());
    
    float h = y1 - y0;
    if(h < 0.0f) return pesRectangle(0,0,0,0);  // short circuit if needed
    
    return pesRectangle(x0,y0,w,h);
}

pesRectangle pesRectangle::getUnion(const pesRectangle& rect) const{
    pesRectangle united = *this;
    united.growToInclude(rect);
    return united;
}

float pesRectangle::getArea() const{
    return fabs(width) * fabs(height);
}

float pesRectangle::getPerimeter() const{
    return 2.0f * fabs(width) + 2.0f * fabs(height);
}

float pesRectangle::getAspectRatio() const{
    return fabs(width) / fabs(height);
}

// void pesRectangle::scaleInTo(const pesRectangle& targetRect){
//     float tw = targetRect.width; // target width
//     float th = targetRect.height;  // target height
//     float sw = width;   // subject width
//     float sh = height;  // subject height
//     if(fabs(sw) >= FLT_EPSILON || fabs(sh) >= FLT_EPSILON) {
//         float wRatio = fabs(tw) / fabs(sw);
//         float hRatio = fabs(th) / fabs(sh);
//         float min = (MIN(wRatio,hRatio));
//         scaleFromCenter(min, min);
//     }
// }
// GPT
void pesRectangle::scaleInTo(const pesRectangle& targetRect) {
    float tw = targetRect.width;  // target width
    float th = targetRect.height; // target height
    float sw = width;             // subject width
    float sh = height;            // subject height

    if (fabs(sw) < FLT_EPSILON || fabs(sh) < FLT_EPSILON) {
        return;
    }

    float wRatio = fabs(tw) / fabs(sw);
    float hRatio = fabs(th) / fabs(sh);
    float minRatio = MIN(wRatio, hRatio);
    scaleFromCenter(minRatio, minRatio);
}

template<typename Type>
typename std::enable_if<std::is_floating_point<Type>::value, bool>::type pesIsFloatEqual(const Type& a, const Type& b)
{
    return std::abs(a - b) <= std::numeric_limits<Type>::epsilon() * std::abs(a);
}

bool pesRectangle::isEmpty() const {
//    return fabs(width)<=FLT_EPSILON && fabs(height)<=FLT_EPSILON;
    return pesIsFloatEqual(width, 0.0f) && pesIsFloatEqual(height, 0.0f);
}

bool pesRectangle::intersects(const pesVec2f& p0, const pesVec2f& p1) const {
    // check for a line intersection
    pesVec2f p;
    
    pesVec2f topLeft(getMinX(), getMinY());
    pesVec2f topRight(getMaxX(), getMinY());
    pesVec2f bottomRight(getMaxX(), getMaxY());
    pesVec2f bottomLeft(getMinX(), getMaxY());
    
    return inside(p0.x, p0.y) || // check end inside
    inside(p1.x, p1.y) || // check end inside
    pesLineSegmentIntersection(p0, p1, topLeft,     topRight,    p) || // cross top
    pesLineSegmentIntersection(p0, p1, topRight,    bottomRight, p) || // cross right
    pesLineSegmentIntersection(p0, p1, bottomRight, bottomLeft,  p) || // cross bottom
    pesLineSegmentIntersection(p0, p1, bottomLeft,  topLeft,     p);   // cross left
}
