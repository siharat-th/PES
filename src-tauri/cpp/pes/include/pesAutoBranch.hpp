//
//  pesAutoBranch.hpp
//  pesEngine
//
//  Created by SIHARAT THAMMAYA on 9/7/2562 BE.
//  Copyright © 2562 SIHARAT THAMMAYA. All rights reserved.
//

#ifndef pesAutoBranch_hpp
#define pesAutoBranch_hpp

#include <stdio.h>
#include <algorithm>
#include "pesPath.hpp"

struct pesNearestBranchingResult{
    int enterIndex=-1;
    int exitIndex=-1;
    int patchesIndex=0;
};

void pesFindNearestEnterAndExit(
                             std::vector<pesPolyline> & patches,
                             std::vector<pesNearestBranchingResult> & results,
                             bool bKeepPatchesOrder=false,
                             bool bAccurateClosestPoint=true);

#endif /* pesAutoBranch_hpp */
