//
//  pesVec2f.cpp
//  pesEngine
//
//  Created by SIHARAT THAMMAYA on 2/7/2562 BE.
//  Copyright © 2562 SIHARAT THAMMAYA. All rights reserved.
//

#include "pesVec2f.hpp"
#include <cmath>
#include "pesMath.hpp"
#include "pesVec3f.h"

using namespace std;

pesVec2f::pesVec2f() : x(0), y(0) {
}

pesVec2f::pesVec2f(float _x, float _y) : x(_x), y(_y) {
}

pesVec2f::pesVec2f( const pesVec3f& vec ) {
    x = vec.x;
    y = vec.y;
}

void pesVec2f::set(float _x, float _y) {
    x = _x;
    y = _y;
}

void pesVec2f::set(const pesVec2f& vec){
    set(vec.x, vec.y);
}

pesVec2f pesVec2f::operator+(const pesVec2f& vec) const {
    return pesVec2f(x + vec.x, y + vec.y);
}

pesVec2f pesVec2f::operator-(const pesVec2f& vec) const {
    return pesVec2f(x - vec.x, y - vec.y);
}

pesVec2f pesVec2f::operator*(const pesVec2f& vec) const {
    return pesVec2f(x*vec.x, y*vec.y);
}

pesVec2f pesVec2f::operator*(const float f) const {
    return pesVec2f(x*f, y*f);
}

pesVec2f& pesVec2f::operator*=(const float f){
    x*=f;
    y*=f;
    return *this;
}

pesVec2f pesVec2f::operator/(const float f) const {
    return pesVec2f(x / f, y / f);
}

pesVec2f& pesVec2f::operator/=(const float f) {
    if (f == 0) return *this;
    
    x /= f;
    y /= f;
    
    return *this;
}

float pesVec2f::distance(const pesVec2f& pnt) const {
    float vx = x - pnt.x;
    float vy = y - pnt.y;
    
    return (float)sqrt(vx*vx + vy*vy);
}

float pesVec2f::squareDistance(const pesVec2f& pnt) const {
    float vx = x - pnt.x;
    float vy = y - pnt.y;
    
    return vx*vx + vy*vy;
}

pesVec2f pesVec2f::getMiddle(const pesVec2f& pnt) const {
    return pesVec2f((x + pnt.x) / 2.0f, (y + pnt.y) / 2.0f);
}

pesVec2f& pesVec2f::normalize() {
    float length = (float)sqrt(x*x + y*y);
    
    if (length > 0) {
        x /= length;
        y /= length;
    }
    
    return *this;
}

float pesVec2f::length() const {
    return (float)sqrt(x*x + y*y);
}

float pesVec2f::lengthSquared() const {
    return (float)(x*x + y*y);
}

// returns The angle in degrees (-180...180)
float pesVec2f::angle(const pesVec2f& vec) const {
    return (float)(atan2(x*vec.y - y*vec.x, x*vec.x + y*vec.y)*RAD_TO_DEG);
}

// returns The angle in radians (-PI...PI)
float pesVec2f::angleRad(const pesVec2f& vec) const {
    return atan2(x*vec.y - y*vec.x, x*vec.x + y*vec.y);
}

float pesVec2f::dot(const pesVec2f& vec) const {
    return x*vec.x + y*vec.y;
}

pesVec2f& pesVec2f::translate(float tx, float ty){
    x += tx;
    y += ty;
    return *this;
}

pesVec2f& pesVec2f::translate(const pesVec2f& vec){
    x += vec.x;
    y += vec.y;
    return *this;
}

pesVec2f& pesVec2f::rotate(float degree) {
    float a = (float)(degree * DEG_TO_RAD);
    float xrot = x*cos(a) - y*sin(a);
    y = x*sin(a) + y*cos(a);
    x = xrot;
    return *this;
}

// rotate around `pivot` rather than around the origin
pesVec2f& pesVec2f::rotate(float degree, const pesVec2f& pivot) {
    float a = (float)(degree * DEG_TO_RAD);
    float xrot = ((x - pivot.x)*cos(a) - (y - pivot.y)*sin(a)) + pivot.x;
    y = ((x - pivot.x)*sin(a) + (y - pivot.y)*cos(a)) + pivot.y;
    x = xrot;
    return *this;
}

pesVec2f& pesVec2f::rotateRad(float rad) {
    float a = rad;
    float xrot = x*cos(a) - y*sin(a);
    y = x*sin(a) + y*cos(a);
    x = xrot;
    return *this;
}

pesVec2f& pesVec2f::rotateRad(float rad, const pesVec2f& pivot) {
    float a = rad;
    float xrot = ((x - pivot.x)*cos(a) - (y - pivot.y)*sin(a)) + pivot.x;
    y = ((x - pivot.x)*sin(a) + (y - pivot.y)*cos(a)) + pivot.y;
    x = xrot;
    return *this;
}

pesVec2f& pesVec2f::scale(const float length) {
    float l = (float)sqrt(x*x + y*y);
    if (l > 0) {
        x = (x / l)*length;
        y = (y / l)*length;
    }
    
    return *this;
}

pesVec2f& pesVec2f::operator+=(const pesVec2f& vec) {
    x += vec.x;
    y += vec.y;
    
    return *this;
}

pesVec2f& pesVec2f::operator-=(const pesVec2f& vec) {
    x -= vec.x;
    y -= vec.y;
    
    return *this;
}

bool pesVec2f::operator==(const pesVec2f& vec) const {
    return (x == vec.x) && (y == vec.y);
}

bool pesVec2f::operator!=( const pesVec2f& vec ) const {
    return (x != vec.x) || (y != vec.y);
}

bool pesVec2f::match(const pesVec2f& vec, float tolerance) const {
    return (fabs(x - vec.x) < tolerance)
    && (fabs(y - vec.y) < tolerance);
}

// Perpendicular normalized vector.
pesVec2f pesVec2f::perpendiculared() const {
    return getPerpendicular();
}

pesVec2f pesVec2f::getPerpendicular() const {
    float length = (float)sqrt(x*x + y*y);
    
    if (length > 0)
        return pesVec2f(-(y / length), x / length);
    else
        return pesVec2f();
}

pesVec2f& pesVec2f::perpendicular() {
    float length = (float)sqrt(x*x + y*y);
    
    if (length > 0) {
        float _x = x;
        x = -(y / length);
        y = _x / length;
    }
    
    return *this;
}

pesVec2f pesVec2f::getInterpolated(const pesVec2f& pnt, float p) const {
    return pesVec2f(x*(1 - p) + pnt.x*p, y*(1 - p) + pnt.y*p);
}

pesVec2f& pesVec2f::interpolate(const pesVec2f& pnt, float p) {
    x = x*(1 - p) + pnt.x*p;
    y = y*(1 - p) + pnt.y*p;
    
    return *this;
}
