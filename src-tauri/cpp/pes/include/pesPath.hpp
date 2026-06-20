//
//  pesPath.hpp
//  pesEngine
//
//  Created by SIHARAT THAMMAYA on 2/7/2562 BE.
//  Copyright © 2562 SIHARAT THAMMAYA. All rights reserved.
//

#ifndef pesPath_hpp
#define pesPath_hpp

#include <stdio.h>
#include "pesPolyline.hpp"
#include "pesColor.hpp"

class pesPath{
public:
    pesPath();
    
    void append(const pesPath & path);

    void clear();
    void close();
    
    void lineTo(float x, float y);
    
    void moveTo(float x, float y);
    
    void curveTo(float x, float y);
    
    void bezierTo(const pesVec2f & cp1, const pesVec2f & cp2, const pesVec2f & p);
    
    void quadBezierTo(const pesVec2f & cp1, const pesVec2f & cp2, const pesVec2f & p);
    
    void arc(const pesVec2f & centre, float radiusX, float radiusY, float angleBegin, float angleEnd);
    void arcNegative(const pesVec2f & centre, float radiusX, float radiusY, float angleBegin, float angleEnd);
    
    void rectangle(const pesRectangle & r);
    void rectangle(const pesVec2f & p,float w,float h);
    void rectangle(float x,float y,float w,float h);
    
    void circle(float x, float y, float radius);
    void circle(const pesVec2f & p, float radius);
    
    void simplify(float tol, bool remake = false);
    void scale(float x, float y);
    void translate(float x, float y);
    void flagShapeChanged();
    bool hasChanged();
    
    void newSubPath();

    void setFilled(bool hasFill);
    bool isFill();
    bool isFill() const { return bFill; }
    bool isStroke() const { return hasOutline() && strokeColor.a!=0; }
    
    void setStrokeWidth(float width); // default 0
    void setColor( const pesColor& color );
    void setHexColor( int hex );
    void setFillColor(const pesColor & color);
    void setFillHexColor(int hex);
    void setFillHexARGBColor(uint32_t hex);
    void setStrokeColor(const pesColor& color);
    void setStrokeHexColor(int hex);
    void setStrokeHexARGBColor(uint32_t hex);
    void setUseShapeColor(bool useColor);
    bool getUseShapeColor() const;
    pesColor getFillColor() const;
    int getFillHexColor() const;
    uint32_t getFillHexARGBColor() const;
    pesColor getStrokeColor() const;
    int getStrokeHexColor() const;
    uint32_t getStrokeHexARGBColor() const;
    float getStrokeWidth() const;
    pesPolyline & lastPolyline();
    std::vector<pesPath> getSubPath() const;
    
    bool hasOutline() const { return strokeWidth>0; }
    
    enum Mode{
        COMMANDS,
        POLYLINES
    };
    
    void setMode(Mode mode);
    Mode getMode();
    
    struct Command{
        enum Type{
            _moveTo = 0,
            _lineTo = 1,
            _curveTo = 2,
            _bezierTo = 3,
            _quadBezierTo = 4,
            _arc = 5,
            _arcNegative = 6,
            _close = 7
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
    
    std::vector<Command> & getCommands();
    const std::vector<Command> & getCommands() const;
    const std::vector<pesPolyline> & getOutline() const;
    
    bool inside(float x, float y) const;
    
    void addCommand(const Command & command);
    
    void rotate(float degree);
    void rotateAround(float degree, const pesVec2f& pivot);
    
    pesRectangle getBoundingBox() const;
    
    void setCurveResolution(int curveResolution);
    int getCurveResolution() const;
    
    void setCircleResolution(int circleResolution);
    int getCircleResolution() const;
    
    bool  bFill;

    bool  bVisible;
    int flags;
    
    int roleType;
    float roleOffset;
    
    int fillType;
    int fillSubtype;
    float fillAngle;
    float fillSize;
    float fillDensity;
    float fillSpacing;
    float fillPitch;
    float fillPullCompensate;

    int strokeType;
    int strokeSubtype;
    float strokeOffset;
    float strokeSize;
    float strokeDensity;
    float strokeSpacing;
    float strokePitch;
    float strokePullCompensate;

    std::string path_id;
    std::string group_id;


    enum {
        FILL_TYPE_DEFAULT,
        FILL_TYPE_SMART_SATIN
    };

    enum {
        FLAG_NONE,
        FLAG_SHOW_POLOYLINE_DOTS = 1
    };

    int _fillColorIndex;
    int _strokeColorIndex;

    // 0: fill-rule="nonzero"   (Skia & SVG)
    // 1: fill-rule="evenodd"   (Skia & SVG)
    // 2: InverseWinding        (Skia)
    // 3: InverseEvenOdd        (Skia)
    // Initial: "nonzero"
    int fillRule{0};

private:
    void generatePolylinesFromCommands();
    
    std::vector<Command> commands;
    pesColor strokeColor;
    pesColor fillColor;
    float strokeWidth;
//    bool  bFill;
    bool  bUseShapeColor;
    
    std::vector<pesPolyline>  polylines;
    
    bool  bHasChanged;
    int   prevCurveRes;
    int   curveResolution;
    int   circleResolution;
    bool  bNeedsPolylinesGeneration;
    
    Mode  mode;
};

#endif /* pesPath_hpp */
