//
//  pesPathUtility.hpp
//  pesEngine
//
//  Created by SIHARAT THAMMAYA on 30/9/2562 BE.
//  Copyright © 2562 SIHARAT THAMMAYA. All rights reserved.
//

#ifndef pesPathUtility_hpp
#define pesPathUtility_hpp

#include <stdio.h>
#include "pesPath.hpp"
#include "include/core/SkPath.h"

pesPath toPes(const SkPath& path);
SkPath toSk(const pesPath& path);

#endif /* pesPathUtility_hpp */
