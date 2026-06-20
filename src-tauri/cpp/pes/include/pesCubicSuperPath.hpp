//
//  pesCubicSuperPath.hpp
//  pesEngine
//
//  Created by SIHARAT THAMMAYA on 1/10/2562 BE.
//  Copyright © 2562 SIHARAT THAMMAYA. All rights reserved.
//

#ifndef pesCubicSuperPath_hpp
#define pesCubicSuperPath_hpp

#include <stdio.h>
#include "pesPath.hpp"

class pesCubicSuperPath{
    
public:
//    enum struct KnotType{
    enum KnotType{
        KNOT_TYPE_CURVE,
        KNOT_TYPE_CORNER
    };
    
    struct Knot{
        KnotType type;
        pesVec2f p;
        
        Knot(KnotType _type, pesVec2f _p):type(_type), p(_p){}
    };
    
    pesCubicSuperPath();
    ~pesCubicSuperPath();
    
    void reset();
    void addCurvePoint(const pesVec2f & p);
    void addCornerPoint(const pesVec2f & p);
    void removeLastPoint();
    void calculateCSP();
    
    pesPath path;
    std::vector<Knot> knots;
};

#endif /* pesCubicSuperPath_hpp */
