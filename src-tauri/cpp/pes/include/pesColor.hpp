//
//  pesColor.hpp
//  pesEngine
//
//  Created by SIHARAT THAMMAYA on 2/7/2562 BE.
//  Copyright © 2562 SIHARAT THAMMAYA. All rights reserved.
//

#ifndef pesColor_hpp
#define pesColor_hpp

#include <stdio.h>
#include <stdint.h>
#include "pesVec3f.h"

pesVec3f RGBfToHSLf(const pesVec3f& RGBf);
pesVec3f RGBToHSLf(uint8_t r, uint8_t g, uint8_t b);
pesVec3f HSLfToRGBf(const pesVec3f& HSLf);

extern const pesVec3f pesPaletteBrotherColorRGBf[66];
extern const pesVec3f pesPaletteBrotherColorHSLf[66];

class pesColor{
public:
    pesColor();
    pesColor(unsigned char r, unsigned char g, unsigned char b, unsigned char a=0xff);
    pesColor(const pesColor&);
    pesColor(const pesVec3f& RGBf) { set(RGBf.x * 255.0f, RGBf.y * 255.0f, RGBf.z * 255.0f); }

    //pesColor(const pesColor&) = default;
    pesColor(pesColor&&) = default;
    pesColor& operator=(const pesColor&) = default;
    pesColor& operator=(pesColor&&) = default;
    pesColor& operator=(const pesVec3f& RGBf) {
        this->set(RGBf.x * 255.0f, RGBf.y * 255.0f, RGBf.z * 255.0f);
        return *this;
    };
    
    void set(unsigned char red, unsigned char green, unsigned char blue, unsigned char alpha=0xff);
    void setHex(int hexColor, float alpha=0xff);
    int getHex() const;
    void setHexARGB(uint32_t hexColor);
    uint32_t getHexARGB() const;

    pesVec3f toRGBf() const;
    pesVec3f toHSLf() const;

    static pesColor fromRGBf(const pesVec3f& RGBf) { return pesColor(RGBf); }
    static pesColor fromHSLf(const pesVec3f& HSLf) { return pesColor(HSLfToRGBf(HSLf)); }
    
    bool isEqual(pesColor other);
    
    unsigned char r, g, b, a;
};

typedef struct pesBrotherThread_
{
    pesColor color;
    const char* description;
    const char* catalogNumber;
} pesBrotherThread;

pesColor pesGetBrotherColor(int index);
const char* pesGetBrotherColorName(int index);
const char* pesGetBrotherCatalogNumber(int index);
const char* pesGetBrotherColorNameWithCatalogNumber(int index);
pesBrotherThread pesGetBrotherThread(int index);
int pesGetNearestBrotherColorIndex( const pesColor & srcColor );
int pesGetNearestBrotherColorRGBfIndex(const pesColor& srcColor);
int pesGetNearestBrotherColorHSLfIndex(const pesColor& srcColor);

#endif /* pesColor_hpp */
