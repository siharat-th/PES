//
//  pesVec2f.hpp
//  pesEngine
//
//  Created by SIHARAT THAMMAYA on 2/7/2562 BE.
//  Copyright © 2562 SIHARAT THAMMAYA. All rights reserved.
//

#ifndef pesVec2f_hpp
#define pesVec2f_hpp

#include <stdio.h>

class pesVec3f;

class pesVec2f {
public:
    /// \cond INTERNAL
    static const int DIM = 2;
    /// \endcond
    
    float x;
    float y;
    
    pesVec2f();
    pesVec2f(float x, float y);
    pesVec2f( const pesVec3f& vec );
    
    void set(float x, float y);
    void set(const pesVec2f& vec);
    
    pesVec2f& operator+=(const pesVec2f& vec);
    pesVec2f& operator-=(const pesVec2f& vec);
    bool      operator==(const pesVec2f& vec) const;
    bool      operator!=( const pesVec2f& vec ) const;
    bool      match(const pesVec2f& vec, float tolerance = 0.0001f) const;
    
    pesVec2f  operator+(const pesVec2f& vec) const;
    pesVec2f  operator-(const pesVec2f& vec) const;
    pesVec2f  operator*(const pesVec2f& vec) const;
    pesVec2f  operator*(const float f) const;
    pesVec2f&  operator*=(const float f);
    pesVec2f  operator/(const float f) const;
    pesVec2f& operator/=(const float f);
    
    float distance(const pesVec2f& pnt) const;
    float squareDistance(const pesVec2f& pnt) const;
    pesVec2f   getMiddle(const pesVec2f& pnt) const;
    pesVec2f& normalize();
    
    float length() const;
    float lengthSquared() const;
    
    // returns The angle in degrees (-180...180)
    float angle(const pesVec2f& vec) const;
    // returns The angle in radians (-PI...PI)
    float angleRad(const pesVec2f& vec) const;
    
    float dot(const pesVec2f& vec) const;
    
    pesVec2f& translate(float tx, float ty);
    pesVec2f& translate(const pesVec2f& vec);
    
    pesVec2f& rotate(float degree);
    // rotate around `pivot` rather than around the origin
    pesVec2f& rotate(float degree, const pesVec2f& pivot);
    
    pesVec2f& rotateRad(float rad);
    pesVec2f& rotateRad(float rad, const pesVec2f& pivot);
    
    pesVec2f& scale(const float length);
    
    // Perpendicular normalized vector.
    pesVec2f perpendiculared() const;
    
    pesVec2f getPerpendicular() const;
    
    pesVec2f& perpendicular();
    
    pesVec2f   getInterpolated(const pesVec2f& pnt, float p) const;
    pesVec2f&  interpolate(const pesVec2f& pnt, float p);

    inline float cross( const pesVec2f& vec ) {
        return x * vec.y - y * vec.x;
    }
    
};

#endif /* pesVec2f_hpp */
