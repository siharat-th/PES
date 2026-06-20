//
//  pesStitchBlock.cpp
//  pesEngine
//
//  Created by SIHARAT THAMMAYA on 3/7/2562 BE.
//  Copyright © 2562 SIHARAT THAMMAYA. All rights reserved.
//

#include "pesStitchBlock.hpp"
#include <algorithm>

//const int32_t bytesqr_table[256] = {
//  0,     1,     4,     9,     16,    25,    36,    49,    64,    81,    100,   121,   144,   169,   196,   225,
//  256,   289,   324,   361,   400,   441,   484,   529,   576,   625,   676,   729,   784,   841,   900,   961,
//  1024,  1089,  1156,  1225,  1296,  1369,  1444,  1521,  1600,  1681,  1764,  1849,  1936,  2025,  2116,  2209,
//  2304,  2401,  2500,  2601,  2704,  2809,  2916,  3025,  3136,  3249,  3364,  3481,  3600,  3721,  3844,  3969,
//  4096,  4225,  4356,  4489,  4624,  4761,  4900,  5041,  5184,  5329,  5476,  5625,  5776,  5929,  6084,  6241,
//  6400,  6561,  6724,  6889,  7056,  7225,  7396,  7569,  7744,  7921,  8100,  8281,  8464,  8649,  8836,  9025,
//  9216,  9409,  9604,  9801,  10000, 10201, 10404, 10609, 10816, 11025, 11236, 11449, 11664, 11881, 12100, 12321,
//  12544, 12769, 12996, 13225, 13456, 13689, 13924, 14161, 14400, 14641, 14884, 15129, 15376, 15625, 15876, 16129,
//  16384, 16641, 16900, 17161, 17424, 17689, 17956, 18225, 18496, 18769, 19044, 19321, 19600, 19881, 20164, 20449,
//  20736, 21025, 21316, 21609, 21904, 22201, 22500, 22801, 23104, 23409, 23716, 24025, 24336, 24649, 24964, 25281,
//  25600, 25921, 26244, 26569, 26896, 27225, 27556, 27889, 28224, 28561, 28900, 29241, 29584, 29929, 30276, 30625, 
//  30976, 31329, 31684, 32041, 32400, 32761, 33124, 33489, 33856, 34225, 34596, 34969, 35344, 35721, 36100, 36481,
//  36864, 37249, 37636, 38025, 38416, 38809, 39204, 39601, 40000, 40401, 40804, 41209, 41616, 42025, 42436, 42849,
//  43264, 43681, 44100, 44521, 44944, 45369, 45796, 46225, 46656, 47089, 47524, 47961, 48400, 48841, 49284, 49729,
//  50176, 50625, 51076, 51529, 51984, 52441, 52900, 53361, 53824, 54289, 54756, 55225, 55696, 56169, 56644, 57121,
//  57600, 58081, 58564, 59049, 59536, 60025, 60516, 61009, 61504, 62001, 62500, 63001, 63504, 64009, 64516, 65025
//};


pesStitchBlock::pesStitchBlock(){
}

pesStitchBlock::~pesStitchBlock(){
    if(polyline.size())
        polyline.clear();
    if(types.size())
        types.clear();
}


void pesStitchBlock::addStitch(float x, float y){
    addStitch(pesVec2f(x,y));
}

// if length >64 make a fake jump isJump=true, flags=NORMAL [OR-Path = 0x80]
void pesStitchBlock::addStitch(const pesVec2f & v){
    //    polyline.addVertex(v);
    if(polyline.size()){
        auto l = polyline.getVertices().back();
        auto t = types.back();
        if(t==NORMAL_STITCH){
            if(l.x==v.x && l.y==v.y)
                return;
        }
    }
    polyline.addVertex(v.x, v.y);
    types.push_back(NORMAL_STITCH);
}

// jump Inside Block mean jump between segments that same color  [OR-Path = 0x90]
void pesStitchBlock::addJump(float x, float y, bool jumpInBlock){
    addJump(pesVec2f(x,y), jumpInBlock);
}

// jump Between Block mean jump then stop for change thread color [OR-Path = 0xA0]
void pesStitchBlock::addJump(const pesVec2f & stitch, bool jumpInBlock){
    //    polyline.addVertex(stitch);
    polyline.addVertex(stitch.x, stitch.y);
    int type = jumpInBlock ? JUMP_STITCH : TRIM_STITCH;
    types.push_back(type);
}


void pesStitchBlock::addStitch(const pesVec2f & stitch, uint8_t type){
    addStitch(stitch.x, stitch.y, type);
}

void pesStitchBlock::addStitch(float x, float y, uint8_t type) {
    polyline.addVertex(x, y);
    types.push_back(type);
}

void pesStitchBlock::addStitchRelative(float x, float y, uint8_t type) {
    pesVec2f v = polyline.getVertices().back();
    addStitch(v.x+x, v.y+y, type);
}

void pesStitchBlock::addStitchRelative(const pesVec2f& stitch, uint8_t type) {
    addStitchRelative(stitch.x, stitch.y, type);
}

bool pesStitchBlock::isFakeJump(int index) {
    if (index >= 0 && index < (int)types.size()) {
        return types[index] & FAKEJUMP_STITCH;
    }
    return false;
}

bool pesStitchBlock::lastIsJump() {
    return (types.back()==FAKEJUMP_STITCH || types.back()==JUMP_STITCH || types.back()==TRIM_STITCH);
}

bool pesStitchBlock::lastIsEnd(){
    if(types.size() != polyline.size()){
        types.resize(polyline.size());
    }
    return types.back()==END_STITCH;
}

void pesStitchBlock::removeLast(){
    if(polyline.size() && polyline.size()==types.size()){
        polyline.getVertices().erase(polyline.getVertices().end()-1);
        types.erase(types.end()-1);
    }
    else{
//        ofLogError("pesStitchBlock", "removeLast error %s", "block empty or size miss match");
    }
}

void pesStitchBlock::clear(){
    polyline.clear();
    types.clear();
}

void pesStitchBlock::setNearestColor(const pesColor & srcColor){
    ////double currentClosestValue = 9999999;
    //int32_t currentClosestValue = INT32_MAX;
    //int32_t closestIndex = -1;
    //const int32_t red = srcColor.r;
    //const int32_t green = srcColor.g;
    //const int32_t blue = srcColor.b;
    //int32_t deltaRed;
    //int32_t deltaBlue;
    //int32_t deltaGreen;
    //int32_t dist;
    //for (int32_t i = 1; i <= 64; i++)
    //{
    //    //int deltaRed;
    //    //int deltaBlue;
    //    //int deltaGreen;
    //    //double dist;
    //    const pesColor c = pesColor(pesGetBrotherColor(i));
    //    
    //    deltaRed = abs(red - c.r);
    //    deltaBlue = abs(green - c.g);
    //    deltaGreen = abs(blue - c.b);
    //    
    //    // Actually calculating the square root (as the given formula suggests) is a waste of resources.
    //    //dist = sqrt((double)(deltaRed * deltaRed) + (deltaBlue * deltaBlue) + (deltaGreen * deltaGreen));
    //    //dist = deltaRed * deltaRed + deltaBlue * deltaBlue + deltaGreen * deltaGreen;
    //    dist = deltaRed + deltaBlue + deltaGreen;
    //    if (dist == 0) {
    //        closestIndex = i;
    //        break;
    //    }
    //    else if(dist <= currentClosestValue)
    //    {
    //        currentClosestValue = dist;
    //        closestIndex = i;
    //    }
    //}

    int closestIndex = pesGetNearestBrotherColorRGBfIndex(srcColor);
    //int closestIndex = pesGetNearestBrotherColorHSLfIndex(srcColor);
    this->closestIndex = closestIndex;
    setColorFromIndex(closestIndex);
}

void pesStitchBlock::setColorFromIndex(int index){
    if (index == 65) {
        index = closestIndex;
    }
    colorIndex = (int)pesClamp(index, 1, 64);
    color = pesGetBrotherColor(colorIndex);
}

void pesStitchBlock::scale(float s){
    for (auto& point : polyline) {
        point *= s;
    }
    polyline.flagHasChanged();
}

void pesStitchBlock::scale(float sx, float sy){
    for(auto it=polyline.begin(); it!=polyline.end(); ++it){
        it->x*=sx;
        it->y*=sy;
    }
    polyline.flagHasChanged();
}

void pesStitchBlock::rotate(float degree){
    pesVec2f pivot = polyline.getBoundingBox().getCenter();
    for(auto it=polyline.begin(); it!=polyline.end(); ++it){
        it->rotate(degree, pivot);
    }
    polyline.flagHasChanged();
}

void pesStitchBlock::rotateAround(float degree, const pesVec2f & pivot){
    for(auto it=polyline.begin(); it!=polyline.end(); ++it){
        it->rotate(degree, pivot);
    }
    polyline.flagHasChanged();
}

void pesStitchBlock::translate(float tx, float ty){
    for(auto it=polyline.begin(); it!=polyline.end(); ++it){
        it->x+=tx;
        it->y+=ty;
    }
    polyline.flagHasChanged();
}

void pesStitchBlock::translate(const pesVec2f & t){
    for(auto it=polyline.begin(); it!=polyline.end(); ++it){
        it->x+=t.x;
        it->y+=t.y;
    }
    polyline.flagHasChanged();
}

size_t  pesStitchBlock::size() const{
    return polyline.size();
}

int pesStitchBlock::getTotalStitches(){
    return (int)size();
}

bool pesStitchBlock::isJump(unsigned int i){
    if(i>=types.size()){
        return false;
    }
    return types[i] & (JUMP_STITCH | TRIM_STITCH);
}

int pesStitchBlock::getTotalJumps(){
    if(types.size()!=polyline.size())
        types.resize(polyline.size());
    return (int)std::count_if(types.begin(), types.end(), [](uint8_t type){ return type & (JUMP_STITCH | TRIM_STITCH); });
}

int pesStitchBlock::getTotalWithType(uint8_t type){
    if(types.size()!=polyline.size()) types.resize(polyline.size());
    return (int)std::count(types.begin(), types.end(), type);
}
