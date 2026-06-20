//
//  pesEffect.hpp
//  pesEngine
//
//  Created by SIHARAT THAMMAYA on 1/10/2562 BE.
//  Copyright © 2562 SIHARAT THAMMAYA. All rights reserved.
//

#ifndef pesEffect_hpp
#define pesEffect_hpp

#include <stdio.h>
#include "pesData.hpp"
#include <memory>

class pesEffect{
public:
    pesEffect()
    :applyPaths_cb(nullptr)
    ,applyStitches_cb(nullptr)
    ,bItalic(false)
    ,bCreateBorder(false)
    ,borderGap(0)
    ,borderGapY(0)
    {
    }
    
    std::function<void(std::vector<pesPath> & paths)> applyPaths_cb;
    std::function<void(std::shared_ptr<pesData> data)> applyStitches_cb;
    void applyPaths(std::vector<pesPath> & paths);
    void applyStitches(std::shared_ptr<pesData> data);
    
    pesRectangle getBoundingBox(const std::vector<pesPath> & paths);
    
    pesPath createBorderPaths(const pesPolyline &outline);
    std::vector<pesVec2f> createBorderStitches(const pesPolyline &outline);
    
    static pesEffect CreateEffect(pesData::Parameter param);
    
    bool bItalic;
    bool bCreateBorder;
    int borderGap;
    int borderGapY;
};

class pesEffectNormal : public pesEffect{
public:
    pesEffectNormal();
    
//    void applyPaths(vector<pesPath> & paths){
//        pesEffect::applyPaths(paths);
//    }
//
//    void applyStitches(shared_ptr<pesData> data){
//        pesEffect::applyStitches(data);
//    }
//
//    bool bItalic;
//    bool bCreateBorder;
//    int borderGap;
//    int borderGapY;
};

class pesEffectArchTop : public pesEffect{
public:
    pesEffectArchTop();
    
    float angle = -1;
    float radius = -1; // note: -1 mean calculate auto radius
    //int borderGap;
    //int borderGapY;
};

class pesEffectArchBottom : public pesEffect{
public:
    pesEffectArchBottom();
    
    float angle = -1;
    float radius = -1; // note: -1 mean calculate auto radius
    //int borderGap;
    //int borderGapY;
};

class pesEffectItalic : public pesEffect{
public:
    pesEffectItalic();
};

class pesEffectSineWave : public pesEffect{
public:
    pesEffectSineWave();
    
    int magnitude = 150;
};

class pesEffectChevron : public pesEffect{
public:
    pesEffectChevron(bool bChevronTop=false);
};

class pesEffectCircle : public pesEffect{
public:
    pesEffectCircle();
};

class pesEffectSlant : public pesEffect{
public:
    pesEffectSlant(bool bSlantUp=true);
    
    int angle = 30;
};

class pesEffectFadeRight : public pesEffect{
public:
    pesEffectFadeRight();
};

class pesEffectFadeLeft : public pesEffect{
public:
    pesEffectFadeLeft();
};

class pesEffectFadeUp : public pesEffect{
public:
    pesEffectFadeUp();
    
    int slantFactor = 10; // unit percent
};

class pesEffectFadeDown : public pesEffect{
public:
    pesEffectFadeDown();
    
    int slantFactor = 10; // unit percent
};

class pesEffectTriangleUp : public pesEffect{
public:
    pesEffectTriangleUp();
};

class pesEffectTriangleDown : public pesEffect{
public:
    pesEffectTriangleDown();
};

class pesEffectInflate : public pesEffect{
public:
    pesEffectInflate();
};


#endif /* pesEffect_hpp */
