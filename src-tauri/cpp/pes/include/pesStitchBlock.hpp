//
//  pesStitchBlock.hpp
//  pesEngine
//
//  Created by SIHARAT THAMMAYA on 3/7/2562 BE.
//  Copyright © 2562 SIHARAT THAMMAYA. All rights reserved.
//

#ifndef pesStitchBlock_hpp
#define pesStitchBlock_hpp

#include <stdio.h>
#include "pesPolyline.hpp"
#include "pesColor.hpp"

// Machine codes for stitch flags
#define NORMAL_STITCH   0  // stitch to (xx, yy)
#define JUMP_STITCH     1  // move to(xx, yy)
#define TRIM_STITCH     2  // trim + move to(xx, yy)
#define STOP_STITCH     4  // pause machine for thread change
#define SEQUIN_STITCH   8  // sequin (not in brother machine)
#define END_STITCH      16 // end of program

#define FAKEJUMP_STITCH 8  // fake-jump (use for pes-stitchType)

class pesStitchBlock{
public:
    pesStitchBlock();
    virtual ~pesStitchBlock();
    
    // add normal stitch (length -64 to 63)
    void addStitch(float x, float y);
    
    // if length >64 make a fake jump isJump=true, flags=NORMAL [OR-Path = 0x80]
    void addStitch(const pesVec2f & v);
    
    // jump Inside Block mean jump between segments that same color  [OR-Path = 0x90]
    void addJump(float x, float y, bool jumpInBlock=true);
    
    // jump Between Block mean jump then stop for change thread color [OR-Path = 0xA0]
    void addJump(const pesVec2f & stitch, bool jumpInBlock=true);
    
    void addStitch(float x, float y, uint8_t type);
    void addStitch(const pesVec2f& stitch, uint8_t type);
    
    void addStitchRelative(float x, float y, uint8_t type);
    void addStitchRelative(const pesVec2f& stitch, uint8_t type);
    
    void scale(float s);
    void scale(float sx, float sy);
    void rotate(float degree);
    void rotateAround(float degree,  const pesVec2f & pivot);
    void translate(float tx, float ty);
    void translate(const pesVec2f & t);
    
    void removeLast();
    
    bool isFakeJump(int index);
    bool lastIsJump();
    void clear();
    
    bool lastIsEnd();
    bool isJump(unsigned int i);
    
    size_t size() const;
    int getTotalStitches();
    int getTotalJumps();
    int getTotalWithType(uint8_t type);
    
    void setNearestColor(const pesColor & srcColor);
    void setColorFromIndex(int index);
    
    pesColor getColor(){return color;}
    pesPolyline getPolyline(){return polyline;}
    
    int closestIndex{0};
    int colorIndex{0};
    pesColor color;
    
    std::vector<uint8_t> types;
    pesPolyline polyline;
};

typedef std::vector<pesStitchBlock> pesStitchBlockList;

#endif /* pesStitchBlock_hpp */
