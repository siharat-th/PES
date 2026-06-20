//
//  pesSatinOutline.hpp
//  pesEngine
//
//  Created by SIHARAT THAMMAYA on 3/7/2562 BE.
//  Copyright © 2562 SIHARAT THAMMAYA. All rights reserved.
//

#ifndef pesSatinOutline_hpp
#define pesSatinOutline_hpp

#include <stdio.h>
#include "pesPolyline.hpp"

typedef struct pesSatinOutline_ {
    int length;
    std::vector<pesVec2f> side1;
    std::vector<pesVec2f> side2;
} pesSatinOutline;

void pesSatinOutlineGenerate(pesPolyline &contour, float zigzagWidth, float inset, pesSatinOutline& result);
void pesSatinOutlineGenerate(pesPolyline &contour, float zigzagWidth, pesSatinOutline& result);
std::vector<pesVec2f> pesSatinOutlineRenderStitches(const pesSatinOutline& result, float zigzagSpacing);

#endif /* pesSatinOutline_hpp */
