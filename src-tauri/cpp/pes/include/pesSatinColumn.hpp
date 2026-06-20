//
//  pesSatinColumn.hpp
//  pesEngine
//
//  Created by SIHARAT THAMMAYA on 3/7/2562 BE.
//  Copyright © 2562 SIHARAT THAMMAYA. All rights reserved.
//

#ifndef pesSatinColumn_hpp
#define pesSatinColumn_hpp

#include <stdio.h>
#include "include/core/SkPath.h"
#include "pesPolyline.hpp"
#include "pesPath.hpp"

#include "pesCubicSuperPath.hpp"

class pesSatinColumn{
public:
//    pesSatinColumn(const vector<pesPath *> & paths);
    pesSatinColumn(const std::vector<pesPath> & paths);
    pesSatinColumn(const pesPath& path0, const pesPath& path1);
    ~pesSatinColumn();

    void doSatin(float zigzagSpace, pesPolyline & patch);
    void doSatin2(float zigzagSpace, pesPolyline & patch);
    bool doSatin3(std::vector<pesPolyline>& patchs,
                  SkPath& remainingArea,
                  SkPath& selfArea,
                  bool bEnableMakeSatinNoneOverlap);

    void doUnderlay(pesPolyline & patch);
    
    struct parameter_t{
        unsigned int pixels_per_mm = 10;
        
        float zigzag_spacing_mm = 0.4f;
        float pull_compensation_mm = 0.0f;
        
        bool satinUnderlay = true;
        bool satinZigzagUnderlay = true;
        
        float satin_underlay_inset_mm = 0.4f;
        
        float satin_zigzag_underlay_spacing_mm = 5.0f;
        float satin_zigzag_underlay_inset_mm = 0.2f;
        
        float satin_underlay_max_stitch_length_mm = 5.0f;
        
        bool satinAutoShorten = true;
        float satin_auto_shorten_tolerance_mm = 0.3f; //0.11;//0.15;
        float satin_shorten_offset_mm = 0.3f;
        
    }param;
    
private:
    std::vector<pesPolyline> segments[2];
    void preparedata(const pesPath& path0, const pesPath& path1);
};

class pesSatinColumnGenerator{
public:
    pesSatinColumnGenerator();
    ~pesSatinColumnGenerator();
    
    void reset();
    void addCurvePoint(const pesVec2f & p);
    void addCornerPoint(const pesVec2f & p);
    void calculateCSP();
    std::vector<pesPath> getSatinColumnPaths();
    
private:
    int cnt;
    pesCubicSuperPath csp[2];
};

#endif /* pesSatinColumn_hpp */
