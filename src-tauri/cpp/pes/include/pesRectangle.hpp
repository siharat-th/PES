//
//  pesRectangle.hpp
//  pesEngine
//
//  Created by SIHARAT THAMMAYA on 2/7/2562 BE.
//  Copyright © 2562 SIHARAT THAMMAYA. All rights reserved.
//

#ifndef pesRectangle_hpp
#define pesRectangle_hpp

#include <stdio.h>
#include "pesVec2f.hpp"

class pesRectangle{
public:
    pesRectangle();
    pesRectangle(float px, float py, float w, float h);
    pesRectangle(const pesRectangle & r);
    
    virtual ~pesRectangle();

    //pesRectangle(const pesRectangle&) = default;
    pesRectangle(pesRectangle&&) = default;
    pesRectangle& operator=(const pesRectangle&) = default;
    pesRectangle& operator=(pesRectangle&&) = default;    
    
    void set(float px, float py, float w, float h);
    void set(const pesVec2f& p, float w, float h);
    void set(const pesRectangle & rect);
    
    void setFromCenter(float px, float py, float w, float h);
    void setFromCenter(const pesVec2f& p, float w, float h);
    
    void growToInclude(float px, float py);
    void growToInclude(const pesVec2f& p);
    void growToInclude(const pesRectangle& rect);
    
    pesVec2f getPosition();
    pesVec2f getCenter() const;
    
    float getMinX() const;
    float getMaxX() const;
    float getMinY() const;
    float getMaxY() const;
    
    bool intersects(const pesRectangle& rect) const;
    bool inside(float px, float py) const;
    
    void translate(float dx, float dy);
    void scale(float sX, float sY);
    void scaleFromCenter(float sX, float sY);
    
    pesRectangle getIntersection(const pesRectangle& rect) const;
    pesRectangle getUnion(const pesRectangle& rect) const;
    
    float getArea() const;
    float getPerimeter() const;
    float getAspectRatio() const;
    
    bool isEmpty() const;
    
    float x;
    float y;
    float width;
    float height;
    
    void scaleInTo(const pesRectangle& targetRect);
    bool intersects(const pesVec2f& p0, const pesVec2f& p1) const;
};

#endif /* pesRectangle_hpp */
