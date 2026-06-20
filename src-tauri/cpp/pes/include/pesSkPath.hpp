//
//  pesSkPath.hpp
//  pesEngine
//
//  Created by SIHARAT THAMMAYA on 2/7/2562 BE.
//  Copyright © 2562 SIHARAT THAMMAYA. All rights reserved.
//

#ifndef pesSkPath_hpp
#define pesSkPath_hpp

#include <stdio.h>
#include "pesPolyline.hpp"
#include "pesColor.hpp"
#include "include/core/SkCanvas.h"
#include "include/core/SkPath.h"

using namespace std;

class pesSkPath{
public:
    pesSkPath();
    
    void append(const pesSkPath & path);

    void clear();
    void close();
    
    void lineTo(float x, float y);
    
    void moveTo(float x, float y);
    
//    void curveTo(float x, float y);
    
    void bezierTo(const pesVec2f & cp1, const pesVec2f & cp2, const pesVec2f & p);
    
    void quadBezierTo(const pesVec2f & cp1, const pesVec2f & cp2, const pesVec2f & p);
    
    void arc(const pesVec2f & centre, float radiusX, float radiusY, float angleBegin, float angleEnd);
    void arcNegative(const pesVec2f & centre, float radiusX, float radiusY, float angleBegin, float angleEnd);
    
    void rectangle(const pesRectangle & r);
    void rectangle(const pesVec2f & p,float w,float h);
    void rectangle(float x,float y,float w,float h);
    
    void circle(float x, float y, float radius);
    void circle(const pesVec2f & p, float radius);
    
    void simplify(float tol);
    void scale(float x, float y);
    void translate(float x, float y);
    void flagShapeChanged();
    bool hasChanged();
    
//    void newSubPath();

    void setFilled(bool hasFill);
    bool isFill();
    bool isFill() const { return bFill; }
    bool isStroke() const { return hasOutline() && strokeColor.a!=0; }
    
    void setStrokeWidth(float width); // default 0
    void setColor( const pesColor& color );
    void setHexColor( int hex );
    void setFillColor(const pesColor & color);
    void setFillHexColor(int hex);
    void setStrokeColor(const pesColor & color);
    void setStrokeHexColor( int hex );
    void setUseShapeColor(bool useColor);
    bool getUseShapeColor() const;
    pesColor getFillColor() const;
    pesColor getStrokeColor() const;
    float getStrokeWidth() const;
//    pesPolyline & lastPolyline();
//    vector<pesSkPath> getSubPath();
    
    bool hasOutline() const { return strokeWidth>0; }
    
    enum Mode{
        COMMANDS,
        POLYLINES
    };
    
    void setMode(Mode mode);
    Mode getMode();
    
    struct Command{
        enum Type{
            moveTo = 0,
            lineTo = 1,
            curveTo = 2,
            bezierTo = 3,
            quadBezierTo = 4,
            arc = 5,
            arcNegative = 6,
            close = 7
        };
        
        /// for close
        Command(Type type);
        
        // for close, lineTo and curveTo
        Command(Type type , const pesVec3f & p);
        
        /// for bezierTo
        Command(Type type , const pesVec3f & p, const pesVec3f & cp1, const pesVec3f & cp2);
        
        ///for arc
        Command(Type type , const pesVec3f & centre, float radiusX, float radiusY, float angleBegin, float angleEnd);
        
        
        Type type;
        pesVec3f to;
        pesVec3f cp1, cp2;
        float radiusX, radiusY, angleBegin, angleEnd;
    };
    
//    vector<Command> & getCommands();
//    const vector<Command> & getCommands() const;
//    const vector<pesPolyline> & getOutline() const;
    
//    bool inside(float x, float y) const;
    
//    void addCommand(const Command & command);
    
    void rotate(float degree);
    void rotateAround(float degree, const pesVec2f& pivot);
    
    pesRectangle getBoundingBox() const;
    
//    void setCurveResolution(int curveResolution);
//    int getCurveResolution() const;
//
//    void setCircleResolution(int circleResolution);
//    int getCircleResolution() const;
    
    bool  bFill;
    
private:
//    void generatePolylinesFromCommands();
    
    vector<Command> commands;
    pesColor strokeColor;
    pesColor fillColor;
    float strokeWidth;
    bool  bUseShapeColor;
    
    std::vector<pesPolyline>  polylines;
    
    bool  bHasChanged;
    int   prevCurveRes;
    int   curveResolution;
    int   circleResolution;
    bool  bNeedsPolylinesGeneration;
    
    Mode  mode;
    
    // pom
    SkPath skpath;
};

#endif /* pesSkPath_hpp */
