//
//  pesMath.hpp
//  pesEngine
//
//  Created by SIHARAT THAMMAYA on 2/7/2562 BE.
//  Copyright © 2562 SIHARAT THAMMAYA. All rights reserved.
//

#ifndef pesMath_hpp
#define pesMath_hpp

#include <stdio.h>
#include <cmath>
#include <optional>
#include "pesVec2f.hpp"



#ifndef PI
#define PI 3.14159265358979323846
#endif

#ifndef TWO_PI
#define TWO_PI 6.28318530717958647693
#endif

#ifndef M_TWO_PI
#define M_TWO_PI 6.28318530717958647693
#endif

#ifndef FOUR_PI
#define FOUR_PI 12.56637061435917295385
#endif

#ifndef HALF_PI
#define HALF_PI 1.57079632679489661923
#endif

#ifndef DEG_TO_RAD
#define DEG_TO_RAD (PI / 180.0)
#endif

#ifndef RAD_TO_DEG
#define RAD_TO_DEG (180.0 / PI)
#endif

#ifndef MIN
#define MIN(x,y) (((x) < (y)) ? (x) : (y))
#endif

#ifndef MAX
#define MAX(x,y) (((x) > (y)) ? (x) : (y))
#endif

#ifndef CLAMP
#define CLAMP(val,min,max) ((val) < (min) ? (min) : ((val > max) ? (max) : (val)))
#endif

#ifndef ABS
#define ABS(x) (((x) < 0) ? -(x) : (x))
#endif

float pesMap(float value, float inputMin, float inputMax, float outputMin, float outputMax, bool clamp = false);
bool  pesInRange(int t, int min, int max);
bool  pesInRange(float t, float min, float max);

float pesDist(float x1, float y1, float x2, float y2);
float pesDistSquared(float x1, float y1, float x2, float y2);

float pesWrap(float value, float from, float to);
float pesClamp(float value, float min, float max);
float pesLerp(float start, float stop, float amt);

bool pesLineCircleIntersec(pesVec2f v1, pesVec2f v2, pesVec2f center, float radius);
bool pesLineSegmentIntersection(const pesVec2f& line1Start, const pesVec2f& line1End, const pesVec2f& line2Start, const pesVec2f& line2End, pesVec2f& intersection);
// Suggest by Github Copilot - GPT 4o
std::optional<pesVec2f> pesLineSegmentIntersection(const pesVec2f& p1, const pesVec2f& p2, const pesVec2f& q1, const pesVec2f& q2);
#endif /* pesMath_hpp */
