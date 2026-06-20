//
//  PES_PESEncoder.hpp
//  PES
//
//  Created by Panutat Tejasen on 10/12/2560 BE.
//  Copyright © 2560 Jimmy Software Co., Ltd. All rights reserved.
//

#ifndef pesEncoder_hpp
#define pesEncoder_hpp

#include <stdio.h>
#include "pesBuffer.hpp"
#include "pesData.hpp"

#define pes_OPTION_NONE           0
#define pes_OPTION_PES5_NO_TAIL   1
#define pes_OPTION_NOT_CENTER_PEC 2
#define pes_OPTION_PEPE           0x8000000

pesBuffer pesCreatePES1Buffer(pesData *pPesData, int option = pes_OPTION_NONE, pesVec2f* pHoopSize = nullptr);
pesBuffer pesCreatePES5Buffer(pesData *pPesData, int option = pes_OPTION_NONE, pesVec2f* pHoopSize = nullptr);
pesBuffer pesCreatePINNBuffer(pesData *pPesData, int option = pes_OPTION_NONE);

pesBuffer pesCreatePES1DefaultHeaderBuffer();
pesBuffer pesCreatePECDefaultHeaderBuffer();
pesBuffer pesCreateThumbnailDefaultBuffer();

pesBuffer pesCreatePECStitch(pesData *pPesData, int dx = 0, int dy = 0);

// pom (DST)
pesBuffer pesCreateDSTStitch(pesData *pPesData, int *totalStitches=NULL);
pesBuffer pesCreateDSTBuffer(pesData *pPesData);

// pom (EXP)
pesBuffer pesCreateEXPBuffer(pesData *pPesData);

// pom (XXX)
pesBuffer pesCreateXXXBuffer(pesData *pPesData);

// pom (JEF) janome
pesBuffer pesCreateJEFBuffer(pesData *pPesData);

// pom (gcode) GRBL
pesBuffer pesCreateGcodeBuffer(pesData *pPesData);

// pom (XXX) singer
pesBuffer pesCreateXXXBuffer(pesData *pPesData);

// pom (DXF) autocad file for cameo
pesBuffer pesCreateDXFBuffer(pesData *pPesData);

#endif /* PES_PESEncoder_hpp */
