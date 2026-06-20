//
//  pesEMBClassify.hpp
//  pesEngine
//
//  Created by SIHARAT THAMMAYA on 19/2/2563 BE.
//  Copyright © 2563 SIHARAT THAMMAYA. All rights reserved.
//

#ifndef pesEMBClassify_hpp
#define pesEMBClassify_hpp

#include <stdio.h>
#include "pesEngine.h"

enum pesEMBClassifyType{
    CLASSIFY_TYPE_UNKNOW=0,
    CLASSIFY_TYPE_SATIN,
    CLASSIFY_TYPE_RUNNING,
    CLASSIFY_TYPE_FILLED
};

typedef struct pesEMBClassifyResult_ {
    pesEMBClassifyType type = CLASSIFY_TYPE_UNKNOW;
    std::vector<pesPath> paths;
    int firstIndex = -1;
    int lastIndex = -1;
}pesEMBClassifyResult;


class pesEMBClassify{
public:
    static std::vector<pesEMBClassifyResult> classifySatin(const pesStitchBlock & block);
    static std::vector<pesEMBClassifyResult> classifyUnknow(const pesStitchBlock & block, const std::vector<pesEMBClassifyResult> & satinResults);
    static std::vector<pesEMBClassifyResult> classifyRunning(const pesStitchBlock & block, const std::vector<pesEMBClassifyResult> & unknowResults);
    static std::vector<pesEMBClassifyResult> classifyFilled(const pesStitchBlock & block, const pesEMBClassifyResult & unknowResult);
    
    static std::vector<pesEMBClassifyResult> classify(const pesStitchBlock & block);
};

// C-Wrapper
std::vector<pesEMBClassifyResult> pes_classify(const pesStitchBlock & block);

#ifdef PES_ENGINE_TEST
void pes_classify_runtest();
#endif

#endif /* pesEMBClassify_hpp */
