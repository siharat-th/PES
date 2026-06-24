//
//  pesData.cpp
//  pesEngine
//
//  Created by SIHARAT THAMMAYA on 3/7/2562 BE.
//  Copyright © 2562 SIHARAT THAMMAYA. All rights reserved.
//
#include <stdio.h>      /* printf, scanf, puts, NULL */
#include <stdlib.h>     /* srand, rand */
#include <time.h>       /* time */
#include "pesData.hpp"
#include "pesAutoBranch.hpp"
#include "pesSatinColumn.hpp"
#include "pesUtility.hpp"
#include "pesSatinOutline.hpp"
#include "pesEMBFill.hpp"
#include "pesSVG.hpp"
#include "pesDocument.hpp"
#include "include/core/SkPath.h"
#include "include/core/SkPathBuilder.h"
#include "include/core/SkPathUtils.h"
#include "include/core/SkImage.h"
#include "include/codec/SkCodec.h"
#include "include/pathops/SkPathOps.h"
#include "skia-ext/pes_skia_ext.h"
#include "skia-ext/pes_skpath_compat.h"


#include "pesPathUtility.hpp"
#include <cfloat>
#include <map>
#include <src/core/SkPathEnums.h>
#include "src/core/SkPathPriv.h"

bool PES_SORTING_BY_AREA = true;

pesData::pesData():
totalStitches(0),
totalJump(0),
bound(0,0,0,0),
strokebound(0,0,0,0),
document(NULL),
centerx(0), 
centery(0), 
byteLength(0),
numColorChange(0),
minx(0),
miny(0),
maxx(0),
maxy(0),
showPathBoundIndex(-1),
showPathIndex(-1)
{
    srand(static_cast<unsigned int>(time(nullptr)));
    // fillBlocks.clear();
    // strokeBlocks.clear();
    // colors.clear();
}

pesData::~pesData(){
    clear();
}

void pesData::clear(){
    fillBlocks.clear();
    strokeBlocks.clear();
    colors.clear();
    for( auto &p : paths ) {
        p.clear();
    }
    paths.clear();
}

bool pesData::readPECStitch(const unsigned char* pbyte, unsigned int* colors){
    int colorIndex = 0;
    pesStitchBlock curblock;
    curblock.setColorFromIndex(colors[colorIndex]);
    pesVec2f prev(0,0);
    bool stitchBlockDone = false;
    const unsigned char* pstitch = pbyte;
    int ptr = 0;
    
    int count = 0;
    while(!stitchBlockDone){
        int val1 = (int)pbyte[ptr++];
        int val2 = (int)pbyte[ptr++];
        uint8_t stitchType = NORMAL_STITCH;
        
        if (val1 == 0xFF && val2 == 0x00) {
            // EOF
            stitchBlockDone = true;
            if(curblock.lastIsJump()){
                curblock.removeLast();
            }
            fillBlocks.push_back(curblock);
            curblock.clear();
            break;
        }
        else if (val1 == 0xFE && val2 == 0xB0) {
            // Color switch, start new block
            fillBlocks.push_back(curblock);
            curblock.clear();
            
            ptr++;
            colorIndex++;
            curblock.setColorFromIndex(colors[colorIndex]);
        }
        else{
            bool jumpStitch = false;
            int deltaX = 0;
            int deltaY = 0;
            if ((val1 & 0x80) == 0x80) {
                jumpStitch = true;
                if (val1 & 0x20) stitchType = TRIM_STITCH;
                else if (val1 & 0x10) stitchType = JUMP_STITCH;
                else if (val1 & 0x80) stitchType = FAKEJUMP_STITCH;
                //stitchType = (val1 >> 8) & (FAKEJUMP_STITCH | TRIM_STITCH | JUMP_STITCH);
                
                int _h = ((val1 & 0xF) * 256);
                int _l = val2;
                deltaX = _h + _l;
                
                if (((int)deltaX & 0x800) == 0x800) {
                    deltaX = (int)((int)deltaX | 0xFFFFF000);
                }
                
                val2 = pbyte[ptr++];
            } else {
                deltaX = (unsigned char)(val1 & 0x7f);
                
                if (deltaX > 63) {
                    deltaX -= 128;
                }
            }
            
            if ((val2 & 0x80) == 0x80) {
                jumpStitch = true;
                
                if (val2 & 0x20) stitchType = TRIM_STITCH;
                else if (val2 & 0x10) stitchType = JUMP_STITCH;
                else if (val2 & 0x80) stitchType = FAKEJUMP_STITCH;
                //stitchType = (val2 >> 8) & (FAKEJUMP_STITCH | TRIM_STITCH | JUMP_STITCH);
                
                int _h = ((val2 & 0xF) * 256);
                int _l = pbyte[ptr++];
                deltaY = _h + _l;
                
                if (((int)deltaY & 0x800) == 0x800) {
                    deltaY = (int)((int)deltaY | 0xFFFFF000);
                }
            } else {
                deltaY = (unsigned char)(val2 & 0x7f);
                
                if (deltaY > 63) {
                    deltaY -= 128;
                }
            }
            
            int x = prev.x + deltaX;
            int y = prev.y + deltaY;
            
            pesVec2f newStitch(x,y);
            curblock.addStitch(newStitch, stitchType);
            if(stitchType==FAKEJUMP_STITCH || stitchType==JUMP_STITCH || stitchType==TRIM_STITCH){
                totalJump++;
            }
            
            count++;
            prev = newStitch;
        }
    }
    
    pstitch = NULL;
    return count>0;
}

void addFirstJump(pesBuffer & stitches, int x, int y);
void translate(pesBuffer & stitches, int x, int y);
void translateLastPoint(pesBuffer & stitches, int x, int y);
void appendStitches(pesBuffer & stitches, const pesBuffer & newStitches, float space);

bool pesData::loadPES2TextFromBuffer(const pesBuffer & fontbuff, const pesBuffer & configbuff, string textIn, bool center){
//    parameter.setType(OBJECT_TYPE_PES2_TEXT);
    parameter.type = OBJECT_TYPE_PES2_TEXT;
    parameter.text = textIn;
    clear();
    bool bLoadOK = false;
    if(fontbuff.size() && configbuff.size()){
        // load config
        std::map<std::string, int> config;
        pesBuffer & configBuffer = const_cast<pesBuffer&>(configbuff);
        auto lines = configBuffer.getLines();
        for (auto it = lines.begin(); it != lines.end(); it++) {
            string line = (*it);
            size_t ks = line.find("<key>");
            size_t ke = line.find("</key>");
            if (ks < line.size() && ke < line.size()) {
                string key = line.substr(ks + 5, ke - ks - 5);
                
                it++;
                string line2 = (*it);
                size_t vs = line2.find("<integer>");
                size_t ve = line2.find("</integer>");
                if (vs < line2.size() && ve < line2.size()) {
                    string value = line2.substr(vs + 9, ve - vs - 9);
                    config[key] = atoi(value.c_str());
                }
            }
        }
        int nInterCharacter = config["InterCharacter"];
        //int nLowerGap = config["LowerGap"];
        //int nUpperGap = config["UpperGap"];
        //int nFishTail = config["FishTail"];
        //int nElephantTail = config["ElephantTail"];
        //int nOwlTail = config["OwlTail"];
        //int nLineHeight = config["LineHeight"];
        //int nUpperVovelGap = config["UpperVovelGap"];
        //int nSpaceGap = config["SpaceGap"];
        //int nBaseGap = config["BaseGap"];
        //int nKorKai = config["__KorKai"];
        //int nZero = config["__ZeroHeight"];
        //int nAHeight = config["__AHeight"];
        //int nIHeight = config["__IHeight"];
        //int nTHeight = config["__THeight"];
        //int nKiteX = config["KiteX"];
        //int nKiteY = config["KiteY"];
        
        // load font
        std::map<char, pesBuffer> fontStitches;
        size_t packSize = fontbuff.size();
        unsigned char *pPack = (unsigned char*)fontbuff.getData();
        int ptr = 0;
        while ((ptr + 6) < (int)packSize) {
            int charID = (pPack[ptr + 3] << 24 & 0xFF000000) | (pPack[ptr + 2] << 16 & 0xFF0000) | (pPack[ptr + 1] << 8 & 0xFF00) | (pPack[ptr] & 0xFF);
            
            ptr += 4;
            int l = pPack[ptr++];
            int h = pPack[ptr++];
            uint16_t stitchLength = ((h << 8) & 0xFF00) | (l & 0xFF);
            if (ptr + stitchLength <= (int)packSize) {
                pesBuffer stitch;
                stitch.set((const char*)&pPack[ptr], stitchLength);
                fontStitches[charID] = stitch;
                ptr += stitchLength;
            } else {
//                GUI_LogError("load fontPack") << "Error out of length!";
                return bLoadOK;
            }
        }
        
        // creat stitch font from text
        int extraLetterSpace = parameter.extraLetterSpace;
        int extraSpace = parameter.extraSpace;
        
        int index = 0;
        int lastch = 0;
        int ch = 0;
        int textLen = 0;
        static unsigned char text[256 + 5];
        while (index < (int)textIn.length() && textLen < 255) {
            int chx = (unsigned char)textIn[index];
            if ((chx & 0xF0) == 0xF0) {
//                GUI_LogError("Unicode ERROR");
                return bLoadOK;
            }
            if ((chx & 0xE0) == 0xE0) {
                //GUI_Log( "Unicode Thai\n" );
                ch = (chx & 0x0F) << 12;
                index++;
                chx = (unsigned char)textIn[index];
                ch += (chx & 0x3F) << 6;
                index++;
                chx = (unsigned char)textIn[index];
                ch += (chx & 0x3F);
            } else {
                ch = chx;
            }
            
            if (ch >= 0xE01) {
                ch = ch - (0xE01 - 0xA1);
            }
            
            if (ch == 0xD3) {
                if (lastch >= 0xE8 && lastch <= 0xEB) {
                    text[textLen - 1] = 0;
                    textLen--;
                    
                    chx = 0xED;
                    text[textLen] = chx;
                    textLen++;
                    
                    chx = lastch;
                    text[textLen] = chx;
                    textLen++;
                } else {
                    chx = 0xED;
                    text[textLen] = chx;
                    textLen++;
                }
                chx = 0xD2;
                ch = 0xD2;
            }
            if (ch >= 0xD8 && ch <= 0xDA) { // lower vowel
                if (lastch == 0xAD) { // ญ
                    int cx = 158;
                    bool exist = fontStitches.find(ch) != fontStitches.end();
                    if (exist) {
                        text[textLen - 1] = 0;
                        textLen--;
                        text[textLen] = (unsigned char)cx;
                        textLen++;
                    }
                } else if (lastch == 0xB0) { // ฐ
                    int cx = 159;
                    bool exist = fontStitches.find(ch) != fontStitches.end();
                    if (exist) {
                        text[textLen - 1] = 0;
                        textLen--;
                        text[textLen] = (unsigned char)cx;
                        textLen++;
                    }
                }
            }
            
            text[textLen++] = ch;
            lastch = ch;
            index++;
            
        }// end while (index < (int)textIn.length() && textLen < 255)
        
        text[textLen] = 0;
        index = 0;
        while (index < textLen) {
            ch = text[index];
            bool bExist = fontStitches.find(ch) != fontStitches.end();
            if (bExist) {
                break;
            }
            index++;
        }
        index++;
        
        pesBuffer pinnBuff;
        pinnBuff.append("#PINN");
        pinnBuff.appendU8(0);
        pinnBuff.appendU8(11);
        {
            pesBuffer charStitch;
            charStitch.set(fontStitches[ch].getData(), fontStitches[ch].size());
            size_t idx = charStitch.size() - 8; // last 8 bytes are boundary data, we need only stitch data
            charStitch.resize(idx);
            pinnBuff.append(charStitch);
        }
        
        lastch = ch;
        int lastLastCh = 0;
        
        for (int i = index; i < textLen; i++) {
            ch = text[i];
            
            bool bExist = fontStitches.find(ch) != fontStitches.end();
            if (bExist) {
                pesBuffer newStitchBuff;
                newStitchBuff.set(fontStitches[ch].getData(), fontStitches[ch].size());
                size_t idx = newStitchBuff.size() - 8; // last 8 bytes are boundary data, we need only stitch data
                newStitchBuff.resize(idx);
                
                if (lastch == 0xBB || lastch == 0xBD || lastch == 0xBF) {
                    if (ch == 0xD1 ||
                        (ch >= 0xD4 && ch <= 0xD7) ||
                        (ch >= 0xE7 && ch <= 0xEE))
                    {
                        int tx = config["FishTail"];
                        ::translate(newStitchBuff, -tx, 0);
                        translateLastPoint(newStitchBuff, tx, 0);
                    }
                }
                
                if (lastch == 0xAA || lastch == 0xAB || lastch == 0xC8 || lastch == 0xCA) {
                    if (ch == 0xD1 ||
                        (ch >= 0xD4 && ch <= 0xD7) ||
                        (ch >= 0xE7 && ch <= 0xEE)) {
                        int tx = config["ElephantTail"];
                        ::translate(newStitchBuff, -tx, 0);
                        translateLastPoint(newStitchBuff, tx, 0);
                    }
                }
                if (lastch == 0xCE) // ฮ
                {
                    if (ch == 0xD1 ||
                        (ch >= 0xD4 && ch <= 0xD7) ||
                        (ch >= 0xE7 && ch <= 0xEE)) {
                        int tx = config["OwlTail"];
                        ::translate(newStitchBuff, -tx, 0);
                        translateLastPoint(newStitchBuff, tx, 0);
                    }
                }
                if (lastch == 0xCC) { // ฬ
                    if (ch == 0xD1 ||
                        (ch >= 0xD4 && ch <= 0xD7) ||
                        (ch >= 0xE7 && ch <= 0xEE)) {
                        int ty = config["KiteY"];
                        ::translate(newStitchBuff, 0, -ty);
                        translateLastPoint(newStitchBuff, 0, ty);
                    }
                }
                if ((lastch == 0xD1) || ((lastch >= 0xD4) && (lastch <= 0xD7)) || (lastch == 0xED)) { // upper vovel
                    if (ch >= 0xE8 && ch <= 0xEC) { // toner mark
                        if (lastLastCh == 0xBB || lastLastCh == 0xBD || lastLastCh == 0xBF)
                        {
                            int tx = config["FishTail"];
                            int ty = config["UpperVovelGap"];
                            ::translate(newStitchBuff, -tx, -ty);
                            translateLastPoint(newStitchBuff, tx, ty);
                        }
                        else if (lastLastCh == 0xAA || lastLastCh == 0xAB || lastLastCh == 0xC8 || lastLastCh == 0xCA)
                        {
                            int tx = config["ElephantTail"];
                            int ty = config["UpperVovelGap"];
                            ::translate(newStitchBuff, -tx, -ty);
                            translateLastPoint(newStitchBuff, tx, ty);
                        }
                        else if (lastLastCh == 0xCE) // ฮ
                        {
                            int tx = config["OwlTail"];
                            int ty = config["UpperVovelGap"];
                            ::translate(newStitchBuff, -tx, -ty);
                            translateLastPoint(newStitchBuff, tx, ty);
                        }
                        else if (lastLastCh == 0xCC) // ฬ
                        {
                            int ty = config["KiteY"] + config["UpperVovelGap"];
                            ::translate(newStitchBuff, 0, -ty);
                            translateLastPoint(newStitchBuff, 0, ty);
                        }
                        else
                        {
                            int ty = config["UpperVovelGap"];
                            ::translate(newStitchBuff, 0, -ty);
                            translateLastPoint(newStitchBuff, 0, ty);
                        }
                    }
                } else if ((lastch >= 0xD8) && (lastch <= 0xDA)) { // lower vovel
                    if (ch >= 0xE8 && ch <= 0xEC) { // toner mark
                        if (lastLastCh == 0xBB || lastLastCh == 0xBD || lastLastCh == 0xBF) { // ป ฝ พ
                            int tx = config["FishTail"];
                            ::translate(newStitchBuff, -tx, 0);
                            translateLastPoint(newStitchBuff, tx, 0);
                        } else if (lastLastCh == 0xAA || lastLastCh == 0xAB || lastLastCh == 0xC8 || lastLastCh == 0xCA) {
                            int tx = config["ElephantTail"];
                            ::translate(newStitchBuff, -tx, 0);
                            translateLastPoint(newStitchBuff, tx, 0);
                        } else if (lastLastCh == 0xCE) // ฮ
                        {
                            int tx = config["OwlTail"];
                            ::translate(newStitchBuff, -tx, 0);
                            translateLastPoint(newStitchBuff, tx, 0);
                        } else if (lastLastCh == 0xCC) // ฬ
                        {
                            int ty = config["KiteY"];
                            ::translate(newStitchBuff, 0, -ty);
                            translateLastPoint(newStitchBuff, 0, ty);
                        }
                    }
                }
                if ((lastch == 0xC4) || (lastch == 0xC6)) {
                    if (ch >= 0xD8 && ch <= 0xDA) {
                        int tx = config["FishTail"];
                        ::translate(newStitchBuff, -tx, 0);
                        translateLastPoint(newStitchBuff, tx, 0);
                    }
                }
                if ((lastch >= 0xAD) && (lastch <= 0xB0)) {
                    if (ch >= 0xD8 && ch <= 0xDA) {
                        int ty = config["BaseGap"];
                        ::translate(newStitchBuff, 0, ty);
                        translateLastPoint(newStitchBuff, 0, -ty);
                    }
                }
                
                
                if (ch >= 0xD4 && ch <= 0xDA)
                {
                    appendStitches(pinnBuff, newStitchBuff, 0);
                }
                else if (ch >= 0xE7 && ch <= 0xEE)
                {
                    appendStitches(pinnBuff, newStitchBuff, 0);
                }
                else if (ch == 0xD1)
                {
                    ::translate(newStitchBuff, -extraLetterSpace, 0);
                    appendStitches(pinnBuff, newStitchBuff, (float)(config["InterCharacter"] + nInterCharacter + extraLetterSpace));
                } else
                {
                    appendStitches(pinnBuff, newStitchBuff, (float)(config["InterCharacter"] + nInterCharacter + extraLetterSpace));
                }
                
                newStitchBuff.clear();
                
            } else {
                if (ch == 0x20)
                {
                    translateLastPoint(pinnBuff, config["SpaceGap"] + extraSpace, 0);
                }
            }
            lastLastCh = lastch;
            lastch = ch;
            
        }// end for(int i = index; i < textLen; i++)
        
        bLoadOK = loadPINNFromBuffer(pinnBuff, center, true);
    }
    
    return bLoadOK;
}

bool pesData::loadPESFromBuffer(const pesBuffer & pesbuff, bool center){
//    parameter.setType(ObjectType::OBJECT_TYPE_PES);
    parameter.type = ObjectType::OBJECT_TYPE_PES;
    clear();
    bool bLoadOK = false;
    long length = pesbuff.size();
    if (length <= 6) {
        return bLoadOK;
    }
    size_t found = pesbuff.getText().find("#PES", 0, 4);
    if (found != 0) {
        return bLoadOK;
    }
    
    const unsigned char* pbyte = (const unsigned char*)pesbuff.getData();
    const unsigned char* pheader = pbyte;
    // header #PES0001 => skip 8 byte

    pheader+=8;
    unsigned int pecoffset = *(int*)pheader;
    unsigned int stitchoffset = pecoffset + 532;

    if(length<=stitchoffset){
        return bLoadOK;
    }
    
    // pec
    //SkDebugf( "Reading PEC\n" );
    const unsigned char* ppec = &pbyte[pecoffset];
    //if( ppec[0] == 0 && ppec[1] == 0 ) {
    //    SkDebugf( "PEC Header OK\n" );
    //}


    unsigned int numcolor = ppec[48] + 1;
    unsigned int *colors = (unsigned int *)malloc(sizeof(unsigned int) * numcolor);
    for(size_t i=0; i<numcolor; i++){
        colors[i] = ppec[49+i];
    }
    
    // find gfxoffset (last section of pes file)
    unsigned int gfxoffset = ppec[514] + ((int)ppec[515] << 8) + ((int)ppec[516] << 16) + 513 + pecoffset;
    if(length<=gfxoffset){
//        ofLogError("loadPES") << "GFX data too small";
        return bLoadOK;
    }
    
    bLoadOK = readPECStitch(&pbyte[stitchoffset], colors);
    if(bLoadOK){
        recalculate();
        //SkDebugf( "Load PES %i\n", center );
        if(center){
            if(bound.getCenter().length()){
                float tx = -bound.getCenter().x;
                float ty = -bound.getCenter().y;
                for(auto &b:fillBlocks){
                    b.translate(tx, ty);
                }
                calcBoundingBox();
            }
        }
    }
    
    free(colors);
    colors = NULL;
    
    return bLoadOK;
}

static char decode_xxx(unsigned char inputByte){
    if(inputByte >= 0x80)
        return (char) ((-~inputByte) - 1);
    return ((char) inputByte);
}

bool pesData::loadXXXFromBuffer(const pesBuffer & xxxbuff, bool center){
    parameter.type = ObjectType::OBJECT_TYPE_PES;
    clear();
    bool bLoadOK = false;
    
    return bLoadOK;
}

// pom : PES5 Import DST
void decode_dst_ternary(unsigned char b[3], int *x, int *y){
    *x = 0;
    *y = 0;
    if (b[0] & 0x01) {
        *x += 1;
    }
    if (b[0] & 0x02) {
        *x -= 1;
    }
    if (b[0] & 0x04) {
        *x += 9;
    }
    if (b[0] & 0x08) {
        *x -= 9;
    }
    if (b[0] & 0x80) {
        *y += 1;
    }
    if (b[0] & 0x40) {
        *y -= 1;
    }
    if (b[0] & 0x20) {
        *y += 9;
    }
    if (b[0] & 0x10) {
        *y -= 9;
    }
    if (b[1] & 0x01) {
        *x += 3;
    }
    if (b[1] & 0x02) {
        *x -= 3;
    }
    if (b[1] & 0x04) {
        *x += 27;
    }
    if (b[1] & 0x08) {
        *x -= 27;
    }
    if (b[1] & 0x80) {
        *y += 3;
    }
    if (b[1] & 0x40) {
        *y -= 3;
    }
    if (b[1] & 0x20) {
        *y += 27;
    }
    if (b[1] & 0x10) {
        *y -= 27;
    }
    if (b[2] & 0x04) {
        *x += 81;
    }
    if (b[2] & 0x08) {
        *x -= 81;
    }
    if (b[2] & 0x20) {
        *y += 81;
    }
    if (b[2] & 0x10) {
        *y -= 81;
    }
}

static int decode_dst_flags(unsigned char b2){
//    int returnCode = 0;
//    if(b2 == 0xF3)
//    {
//        return END_STITCH;
//    }
//    if(b2 & 0x80)
//    {
//        returnCode |= JUMP_STITCH;
//    }
//    if(b2 & 0x40)
//    {
//        returnCode |= STOP_STITCH;
//    }
//    return returnCode;
    
    if (b2 == 0xF3) {
        return END_STITCH;
    }
    if (b2 & 0x40) {
        return STOP_STITCH;
    }
    if (b2 & 0x80) {
        return JUMP_STITCH;
    }
    return NORMAL_STITCH;
}

bool pesData::loadDSTFromBuffer(const pesBuffer & dstbuff, bool center){
    parameter.type = ObjectType::OBJECT_TYPE_PES;
    clear();
    bool bLoadOK = false;
    
    if(dstbuff.size()){
        pesStitchBlock currentBlock;
        pesVec2f prevStitch(0, 0);
        bool stitchBlockDone = false;
        const unsigned char* pbyte;
        pbyte = (const unsigned char*)dstbuff.getData() + 512;
        int ptr = 0;
        while (!stitchBlockDone) {
            stitchBlockDone = false;
            int x, y, flags;
            unsigned char b[3];
            b[0] = pbyte[ptr++];
            b[1] = pbyte[ptr++];
            b[2] = pbyte[ptr++];
            decode_dst_ternary(b, &x, &y);
            flags = decode_dst_flags(b[2]);
            if(flags==END_STITCH){
                stitchBlockDone = true;
                fillBlocks.push_back(currentBlock);
                currentBlock.clear();
                break;
            }
            else if(flags==STOP_STITCH){
                fillBlocks.push_back(currentBlock);
                currentBlock.clear();
            }
            else{
                float newX = prevStitch.x + (x/1.0);
                float newY = prevStitch.y + (y/1.0);
                
                currentBlock.addStitch(pesVec2f(newX, newY), flags);
                
                prevStitch = currentBlock.polyline.getVertices().back();
            }
        }
        
        pbyte = NULL;
        bLoadOK = true;
        
        // DST Initial Color
        for(int i=0; i<fillBlocks.size(); i++){
            fillBlocks[i].setColorFromIndex(i+1);
        }
        
        verticalFlip();
        recalculate();
        
        if(center){
            pesVec2f center = bound.getCenter();
            if(center.length()){
//                translate(-center.x, -center.y);
                float tx = -center.x;
                float ty = -center.y;
                for(auto &b:fillBlocks){
                    b.translate(tx, ty);
                }
                calcBoundingBox();
            }
        }
    }

    return bLoadOK;
}

bool pesData::loadPINNFromBuffer(const pesBuffer & pinnbuff, bool center, bool isPINNFont){
    parameter.type = isPINNFont ? OBJECT_TYPE_PES2_TEXT : ObjectType::OBJECT_TYPE_PES;
//    parameter.setType(type);
    clear();
    bool bLoadOK = false;
    if(pinnbuff.size()){
        // header #PINN skip 5 byte
        int numColorChange = pinnbuff.getData()[5];
//        int numColor = numColorChange + 1;
        std::vector<int> colors;
        for (int i = 0; i <= numColorChange; i++) {
            colors.push_back(pinnbuff.getData()[5 + i + 1]);
        }
        
        uint8_t END[2];
        END[0] = *(pinnbuff.end() - 2);
        END[1] = *(pinnbuff.end() - 1);
        pesBuffer fixBuff;
        bool fix = false;
        if(END[0]!=0xFF || END[1]!=0x00){ // missing end of stitch block
//            ofLogError("loadPINNFromBuffer") << "missing end of stitch block [0xFF, 0x00]";
//            ofLogNotice("Last two", "%02x %02x", END[0], END[1]);
            fixBuff = pinnbuff;
            if(END[1]==0xFF)
                fixBuff.appendU8(0x00);
            else{
                fixBuff.appendU8(0xFF);
                fixBuff.appendU8(0x00);
            }
            fix = true;
        }
        
        int colorIndex = 0;
        pesStitchBlock currentBlock;
        if(colors.size()==0){
            colors.push_back(0x20); // default color=32
        }
        currentBlock.setColorFromIndex(colors[colorIndex]);
        pesVec2f prevStitch(0, 0);
//        bool jump=true;
        
        bool stitchBlockDone = false;
        const unsigned char* pbyte;
        if(fix)
            pbyte = (const unsigned char*)fixBuff.getData() + (5 + colors.size() + 1);
        else
            pbyte = (const unsigned char*)pinnbuff.getData() + (5 + colors.size() + 1);
        int ptr = 0;
        while (!stitchBlockDone) {
            stitchBlockDone = false;
            int val1 = (int)pbyte[ptr++];
            int val2 = (int)pbyte[ptr++];
            
            
            uint8_t stitchType = NORMAL_STITCH;
            if (val1 == 0xFF && val2 == 0x00) {
                // EOF
                stitchBlockDone = true;
                if (isPINNFont && currentBlock.lastIsJump()) // remove last trail for PINNFont
                    currentBlock.polyline.getVertices().erase(currentBlock.polyline.getVertices().end() - 1);
                fillBlocks.push_back(currentBlock);
                currentBlock.clear();
                break;
            } else if (val1 == 0xFE && val2 == 0xB0) {
                // color switch, start new block
                // int flags = STOP_STITCH;
                fillBlocks.push_back(currentBlock);
                
                ptr++; // skip 1 byte.
                colorIndex++;
                currentBlock.clear();
                currentBlock.setColorFromIndex(colors[colorIndex]);
            } else {
                
                bool jumpStitch = false;
                int deltaX = 0;
                int deltaY = 0;
                
                int extraBits1 = 0x00;
                if ((val1 & 0x80) == 0x80) {
                    jumpStitch = true;
                    extraBits1 = val1 & 0x70;
                    if (val1 & 0x20) stitchType = TRIM_STITCH;
                    else if (val1 & 0x10) stitchType = JUMP_STITCH;
                    else if (val1 & 0x80) stitchType = FAKEJUMP_STITCH;
                    //stitchType = (val1 >> 8) & (FAKEJUMP_STITCH | TRIM_STITCH | JUMP_STITCH);

                    deltaX = ((val1 & 0xF) * 256) + val2;
                    if (((int)deltaX & 0x800) == 0x800) {
                        deltaX = (int)((int)deltaX | 0xFFFFF000);
                    }
                    val2 = pbyte[ptr++];
                } else {
                    deltaX = (unsigned char)(val1 & 0x7f);
                    if (deltaX > 63) {
                        deltaX -= 128;
                    }
                }
                
                int extraBits2 = 0x00;
                if ((val2 & 0x80) == 0x80) {
                    jumpStitch = true;
                    extraBits2 = val2 & 0x70;
                    if (val2 & 0x20) stitchType = TRIM_STITCH;
                    else if (val2 & 0x10) stitchType = JUMP_STITCH;
                    else if (val2 & 0x80) stitchType = FAKEJUMP_STITCH;
                    //stitchType = (val2 >> 8) & (FAKEJUMP_STITCH | TRIM_STITCH | JUMP_STITCH);

                    int _h = ((val2 & 0xF) * 256);
                    int _l = pbyte[ptr++];
                    deltaY = _h + _l;
                    if (((int)deltaY & 0x800) == 0x800) {
                        deltaY = (int)((int)deltaY | 0xFFFFF000);
                    }
                } else {
                    deltaY = (unsigned char)(val2 & 0x7f);
                    if (deltaY > 63) {
                        deltaY -= 128;
                    }
                }
                
                int x = prevStitch.x + deltaX;
                int y = prevStitch.y + deltaY;
                
                currentBlock.addStitch(pesVec2f(x,y), stitchType);
                
                prevStitch = currentBlock.polyline.getVertices().back();
            }
        }
        pbyte = NULL;
        bLoadOK = true;
        
        recalculate();
        
        if(center){
            pesVec2f center = bound.getCenter();
            if(center.length()){
//                translate(-center.x, -center.y);
                float tx = -center.x;
                float ty = -center.y;
                for(auto &b:fillBlocks){
                    b.translate(tx, ty);
                }
                calcBoundingBox();
            }
        }
    }
    else{
        bLoadOK = false;
//        ofLogError("PESData::loadPINN()") << "Error data is empty";
    }
    
    return bLoadOK;
}

bool pesData::loadSVGFromBuffer(const pesBuffer & svgbuff, bool center){
    parameter.type = OBJECT_TYPE_SCALABLE_SVG_FILE;
    parameter.useColorFromPicker = false;
    parameter.fillType = FillType::FILL_TYPE_NONE;
    parameter.strokeType = StrokeType::STROKE_TYPE_NONE;

    // center == true: import svg
    // center == false: load svg from ppes
    if (center) {
        parameter.fillColorIndex = 65;
        parameter.colorIndex = 65;
    }
    clear();
    bool bLoadOK = false;
    
    if(svgbuff.size()){
        pesSVG pessvg;
        pessvg.loadFromBuffer(svgbuff);
        paths = pessvg.getPaths();

        //experimental clip viewbox
        //clip overflow viewbox
        //int x = pessvg.getX();
        //int y = pessvg.getY();
        //int w = pessvg.getWidth();
        //int h = pessvg.getHeight();        
        //bool hassize = w > 0 && h > 0;
        //if (hassize) {
        //    auto skpclip = SkPath().addRect(x, y, x + w, y + h);
        //
        //    for (auto& p : paths) {
        //        auto skp = toSk(p);
        //        if (Op(skpclip, skp, kIntersect_SkPathOp, &skp)) {
        //            AsWinding(skp, &skp);
        //            p.clear();
        //            p.append(toPes(skp));
        //            p.fillRule = (int)skp.getFillType();
        //        }            
        //    }
        //}

        bool hasstroke = false;

        // Jimmy - experiment
        for( int i=0; i<paths.size(); i++ ) {
            pesColor cs = paths[i].getStrokeColor();
            pesColor cf = paths[i].getFillColor();
            float sw = paths[i].getStrokeWidth();
            bool iss = paths[i].isStroke();
            bool isf = paths[i].isFill();

            hasstroke |= iss;

            /*
            SkDebugf( "%i. Stroke: %i(%0.2f) (%i,%i,%i,%i)  Fill: %i (%i,%i,%i,%i)\n", 
                i, iss, sw, 
                (int)cs.r, (int)cs.g, (int)cs.b, (int)cs.a,
                isf,
                (int)cf.r, (int)cf.g, (int)cf.b, (int)cf.a
                );
            */

            //// Fix Winding 
            //// test-fill: datas/svgs/80_s/80s_01.svg
            //// test fill-hole: DINOSOUR-DINOSOUR2.svg
            //if (!(parameter.text == "Avatar" || parameter.text == "Tag") && isf) {
            //    SkPaint p;
            //    p.setStyle(SkPaint::kStroke_Style);
            //    p.setStrokeCap(SkPaint::Cap::kButt_Cap);
            //    p.setStrokeJoin(SkPaint::Join::kMiter_Join);
            //    p.setStrokeWidth(0.2f);
            //
            //    auto& ppath = paths[i];
            //    auto skp = toSk(ppath);
            //    auto sim = SkPath(skp);
            //    if (Simplify(sim, &sim)) {
            //        if (skpathutils::FillPathWithPaint(skp, p, &skp)) {
            //            if (Op(skp, sim, kUnion_SkPathOp, &skp)) {
            //                //if (AsWindingPes(skp, &skp, true)) {
            //                if (AsWinding(skp, &skp)) {
            //                    ppath.clear();
            //                    ppath.append(toPes(skp));
            //                    ppath.fillRule = (int)skp.getFillType();
            //                }
            //            }
            //        }
            //    }
            //}
        }

        // center == true: import svg
        // center == false: load svg from ppes
        if (center) {
            // stroke not found, auto overide fill path
            if (!hasstroke) {
                for (auto& p : paths) {
                    p.setStrokeColor(p.getFillColor());
                    p.setStrokeWidth(0.2f);
                }
            }
        }
        
        if(paths.size()){




            recalculate();
            
            if(center){
                pesVec2f c = bound.getCenter();
                if(c.length()){
                    translate(-c.x, -c.y);
                }
            }
            
            bLoadOK = true;
        }
    }
    
    return bLoadOK;
}

bool pesData::loadBackgroundFromBuffer(const pesBuffer & buff, bool center){
//    parameter.setType(OBJECT_TYPE_SCALABLE_SVG_FILE);
    parameter.type = OBJECT_TYPE_BACKGROUND;
    parameter.useColorFromPicker = false;
    parameter.fillType = FillType::FILL_TYPE_NONE;
    parameter.strokeType = StrokeType::STROKE_TYPE_NONE;
    parameter.text = "Background";
    parameter.locked = true;
    clear();
    bool bLoadOK = false;
    
    if(buff.size()){
        parameter.backgroundBuffer = pesBuffer(buff.getData(), buff.size());
        sk_sp<SkData> encoded = SkData::MakeWithCopy(buff.getData(), buff.size());
        parameter.textureBackground = SkImages::DeferredFromEncodedData(encoded);
        //SkDebugf("image size: %i, %i\n",
        //            parameter.textureBackground->width(),
        //            parameter.textureBackground->height());  

        calcBoundingBox();
        //SkDebugf( "Bound: %0.2f %0.2f %0.2f %0.2f\n", bound.getMinX(), bound.getMinY(), 
        //    bound.getMaxX(), bound.getMaxY() );
        bLoadOK = true;      
    }
    
    return bLoadOK;
}

bool pesData::loadPPEFFromSVGBuffer(const pesBuffer & svgbuff, bool center){
//    parameter.setType(OBJECT_TYPE_SCALABLE_PPEF_TEXT);
    parameter.type = OBJECT_TYPE_SCALABLE_PPEF_TEXT;
    parameter.isPPEFPath = true;
    parameter.fillType = FillType::FILL_TYPE_SATIN_COLUMN;
    parameter.strokeType = StrokeType::STROKE_TYPE_NONE;
    clear();
    bool bLoadOK = false;
    
    if(svgbuff.size()){
        pesSVG pessvg;
        pessvg.loadFromBuffer(svgbuff);
        paths = pessvg.getPaths();
        
        if(paths.size()){
            recalculate();
            
            if(center){
                pesVec2f c = bound.getCenter();
                if(c.length()){
                    translate(-c.x, -c.y);
                }
            }
            
            bLoadOK = true;
        }
    }
    
    return bLoadOK;
}

bool pesData::loadTTFFromSVGBuffer(const pesBuffer & svgbuff, bool center){
//    parameter.setType(OBJECT_TYPE_SCALABLE_TTF_TEXT);
    parameter.type = OBJECT_TYPE_SCALABLE_TTF_TEXT;
    parameter.isTTFPath = true;
    parameter.fillType = FillType::FILL_TYPE_NONE;
    parameter.strokeType = StrokeType::STROKE_TYPE_NONE;
    
    clear();
    bool bLoadOK = false;
    
    if(svgbuff.size()){
        pesSVG pessvg;
        pessvg.loadFromBuffer(svgbuff);
        paths = pessvg.getPaths();
        
        if(paths.size()){
            recalculate();
            
            if(center){
                pesVec2f c = bound.getCenter();
                if(c.length()){
                    translate(-c.x, -c.y);
                }
            }
            
            bLoadOK = true;
        }
    }
    
    return bLoadOK;
}

bool pesData::loadSatinColumnFromSVGBuffer(const pesBuffer & svgbuff){
    parameter.type = OBJECT_TYPE_SCALABLE_SATINCOLUMN;
    parameter.isSatinColumnPath = true;
    parameter.fillType = FillType::FILL_TYPE_SATIN_COLUMN;
    parameter.strokeType = StrokeType::STROKE_TYPE_NONE;
    
    clear();
    bool bLoadOK = false;
    
    if(svgbuff.size()){
        pesSVG pessvg;
        pessvg.loadFromBuffer(svgbuff);
        paths = pessvg.getPaths();
        
        if(paths.size()){
            recalculate();
            bLoadOK = true;
        }
    }
    
    return bLoadOK;
}

bool pesData::isSameType(const pesData & other){
    return parameter.type == other.parameter.type;
}

bool pesData::isScalable(){
    return pesInRange(parameter.type, OBJECT_TYPE_SCALABLE_SVG_FILE, OBJECT_TYPE_SCALABLE_PPEF_TEXT_V2);
}

bool pesData::canMerge(const pesData & other){
    return isSameType(other);
}

bool pesData::merge(const pesData & newData){    
    bool mergeSuccess = false;
    if(canMerge(newData)){
        if(isScalable()){
            paths.insert(paths.end(), newData.paths.begin(), newData.paths.end());
            if(parameter.type == OBJECT_TYPE_SCALABLE_PPEF_TEXT){
                applyPPEFFill();
            }
            else if(parameter.type == OBJECT_TYPE_SCALABLE_PPEF_TEXT_V2){
                applyPPEF_V2_Fill();
            }
            else{
                if (parameter.type == OBJECT_TYPE_SCALABLE_SVG_FILE) {
                    parameter.text = "Merge-Paths";
                }
                applyFill();
            }
            applyStroke();
            recalculate();
            mergeSuccess = true;
        }
        else{
            fillBlocks.insert(fillBlocks.end(), newData.fillBlocks.begin(), newData.fillBlocks.end());
            strokeBlocks.insert(strokeBlocks.end(), newData.strokeBlocks.begin(), newData.strokeBlocks.end());
            recalculate();
            mergeSuccess = true;
        }
    }
    return mergeSuccess;
}

void pesData::calcBoundingBox(){
    if(isScalable()){
        if(totalStitches>0){
            calcStitchBoudingBox();
        }
        else{
            calcScalableBoundingBox();
        }
    }
    else{
        if(parameter.type==pesData::OBJECT_TYPE_BACKGROUND){
            int s = 10;
            // int w = parameter.textureBackground->width();
            // int h = parameter.textureBackground->height();
            // int w = pesGetDocument()->getBackgroundWidth() * s;
            // int h = pesGetDocument()->getBackgroundHeight() * s;
            // int w = parameter.textureBackground->width() * s;
            // int h = parameter.textureBackground->height() *s;
            // bound.setFromCenter(pesVec2f(0,0), w, h);

            if(document!=NULL){
                if(parameter.text=="Background"){
                    int w = parameter.textureBackground->width();
                    int h = parameter.textureBackground->height();
                    bound.setFromCenter(pesVec2f(0,0), w, h);
                }
                else{
                    int w = document->getBackgroundWidth() * s;
                    int h = document->getBackgroundHeight() * s;
                    bound.setFromCenter(pesVec2f(0,0), w, h);
                }
            }
            else{
                int w = parameter.textureBackground->width() * s;
                int h = parameter.textureBackground->height() *s;
                bound.setFromCenter(pesVec2f(0,0), w, h);
            }

            // SkDebugf("texture w=%d\n", parameter.textureBackground->width());
            // SkDebugf("document w=%d\n", document->getBackgroundWidth());
            // SkDebugf("pesGetDocument w=%d\n", pesGetDocument()->getBackgroundWidth());

            // if(this->document == pesGetDocument()){
            //     SkDebugf("document==pesGetDocument\n");
            // }
            // else{
            //     SkDebugf("document!=pesGetDocument\n");
            //     SkDebugf("document=%x , pesGetDocument=%x\n", this->document, pesGetDocument());
            // }
        }
        else{
            calcStitchBoudingBox();
        }
    }
    
    minx = (int)bound.getMinX();
    maxx = (int)bound.getMaxX();
    miny = (int)bound.getMinY();
    maxy = (int)bound.getMaxY();
    centerx = (int)bound.getCenter().x;
    centery = (int)bound.getCenter().y;
}

void pesData::calcStitchBoudingBox(){
//    bound.set(0, 0, 0, 0);
    bound.set(0,0,0,0);
    for(auto & block:fillBlocks){
        auto b = block.polyline.getBoundingBox();
        
        if(bound.isEmpty()){
            if(b.isEmpty()==false){
                bound.set(b);
            }
        }
        else{
            if(b.isEmpty()==false){
                bound.growToInclude(b);
            }
        }
    }
    for(auto & block:strokeBlocks){
        auto b = block.polyline.getBoundingBox();
        if(bound.isEmpty()){
            if(b.isEmpty()==false){
                bound.set(b);
            }
        }
        else{
            if(b.isEmpty()==false){
                bound.growToInclude(b);
            }
        }
    }
}

void pesData::calcScalableBoundingBox(){
//    bound.set(0, 0, 0, 0);
//    for(auto & p:paths){
//        if(bound.isEmpty()){
//            bound.set(p.getBoundingBox());
//        }
//        else{
//            bound.growToInclude(p.getBoundingBox());
//        }
//    }
    
    bound.set(0, 0, 0, 0);
    strokebound.set(0, 0, 0, 0);

    if (paths.size()) {
        const float pes_scale = std::min(
            abs(this->parameter.ppefScaleX), 
            abs(this->parameter.ppefScaleY));

        for (const auto& p : paths) {
            if (p.bVisible == false) {
                continue;
            }
            auto bb = p.getBoundingBox();
            if(bound.isEmpty()){
                bound.set(bb);
            }
            else{
                bound.growToInclude(bb);
            }

            if (p.isStroke()) {
                auto strokewidth = abs(p.getStrokeWidth());
                if (p.isFill() && p.getCommands().size() > 2) {
                    strokewidth = abs(strokewidth * pes_scale);
                }

                if (strokewidth > 0) {
                    const auto radius = SkScalarHalf(strokewidth);
                    bb.set(bb.x - radius,
                           bb.y - radius,
                           bb.width + radius * 2.0f,
                           bb.height + radius * 2.0f);
                }

                if (strokebound.isEmpty()) {
                    strokebound.set(bb);
                } 
                else {
                    strokebound.growToInclude(bb);
                }
            }
        }
    }
}

pesRectangle pesData::getScalableBoundingBox() const{
//    pesRectangle svgbound(0, 0, 0, 0);
//    for(auto & p:paths){
//        if(svgbound.isEmpty()){
//            svgbound.set(p.getBoundingBox());
//        }
//        else{
//            svgbound.growToInclude(p.getBoundingBox());
//        }
//    }
//    return svgbound;
    
    pesRectangle svgbound;
    if(paths.size()<1)
        return pesRectangle(0, 0, 0, 0);
    else{
        svgbound.set(paths[0].getBoundingBox());
        for(auto & p:paths){
            svgbound.growToInclude(p.getBoundingBox());
        }
    }
    return svgbound;
}

void pesData::recalculate(){
    totalStitches = totalJump = 0;
    numColorChange = 0;
    colors.clear();
    int prevColor = -1;
    for(auto & block:fillBlocks){
        totalStitches += block.getTotalStitches();
        totalJump += block.getTotalJumps();
        if(block.size() && block.colorIndex!=prevColor){
            colors.push_back(block.colorIndex);
            prevColor = block.colorIndex;
        }
    }
    
    for(auto & block:strokeBlocks){
        totalStitches += block.getTotalStitches();
        totalJump += block.getTotalJumps();
        if(block.size() && block.colorIndex!=prevColor){
            colors.push_back(block.colorIndex);
            prevColor = block.colorIndex;
        }
    }
    numColorChange = (int)colors.size() - 1;
    if(numColorChange<0)
        numColorChange = 0;
    
    calcBoundingBox();
}

void pesData::translate(float tx, float ty){
    if(isScalable()){
        for(auto& p:paths){
            p.translate(tx, ty);
        }
    }
    
    for(auto & block:fillBlocks){
        block.translate(tx, ty);
    }
    
    for(auto & block:strokeBlocks){
        block.translate(tx, ty);
    }
    
    calcBoundingBox();
}

void pesData::scale(float sx, float sy){
    if(isScalable()){
//        // (ppef save cumulative scale)
//        if(parameter.isPPEFPath){
//            parameter.ppefScaleX *= sx;
//            parameter.ppefScaleY *= sy;
//        }
        
        // (ppef&ttf save cumulative scale)
        //if(parameter.isPPEFPath || parameter.isTTFPath){
            parameter.ppefScaleX *= sx;
            parameter.ppefScaleY *= sy;
        //}
        
        pesVec2f oldMin(bound.getMinX(), bound.getMinY());

        for(auto &p:paths){
            p.scale(sx, sy);
        }
        
        if(parameter.isPPEFPath){
            fillBlocks.clear();
            strokeBlocks.clear();
            if(parameter.type==pesData::OBJECT_TYPE_SCALABLE_PPEF_TEXT_V2)
                applyPPEF_V2_Fill();
            else
                applyPPEFFill();
        }
        else{
            if(parameter.type==pesData::OBJECT_TYPE_SCALABLE_SVG_FILE) {
                // Do not re-create stitch if we never create it yet.
                if( fillBlocks.size() || strokeBlocks.size() ) {
                    fillBlocks.clear();
                    strokeBlocks.clear();
                    applyFill();
                    applyStroke();
                }
            }
            else {
                fillBlocks.clear();
                strokeBlocks.clear();
                applyFill();
                applyStroke();
            }
        }
        recalculate();
        
        pesVec2f newMin(bound.getMinX(), bound.getMinY());
        pesVec2f d = oldMin - newMin;
        if(d.length()){
            translate(d.x, d.y);
        }
    }
}

void pesData::scaleStitches(float sx, float sy){
    sx = pesClamp(sx, 0.9, 1.1);
    sy = pesClamp(sy, 0.9, 1.1);
    for(auto & block:fillBlocks){
        block.scale(sx, sy);
    }
    for(auto & block:strokeBlocks){
        block.scale(sx, sy);
    }
    recalculate();
}

void pesData::rotate(float degree){
    if(isScalable()){
        for(auto & p:paths){
            p.rotateAround(degree, bound.getCenter());
        }
    }
    
    for(auto & block:fillBlocks){
        block.rotateAround(degree, bound.getCenter());
    }
    
    for(auto & block:strokeBlocks){
        block.rotateAround(degree, bound.getCenter());
    }
    
    calcBoundingBox();
}

static pesPath CreateDefaultPattern(){ // net1.pas
    pesPath p;
    p.moveTo(768, 1024); //0
    p.lineTo(1024, 768);
    p.moveTo(512,1024); //1
    p.lineTo(1024, 512);
    p.moveTo(512, 768); //2
    p.lineTo(1024, 256);
    p.moveTo(512, 512); //3
    p.lineTo(1024,0);
    p.moveTo(512, 256); //4
    p.lineTo(768, 0);
    p.moveTo(512, 0); //5
    p.lineTo(640, 128);
    p.moveTo(640, 384); //6
    p.lineTo(256, 0);
    p.moveTo(640, 640); //7
    p.lineTo(0, 0);
    p.moveTo(640, 896); //8
    p.lineTo(0, 256);
    p.moveTo(512, 1024); //9
    p.lineTo(0, 512);
    p.moveTo(0, 768); //10
    p.lineTo(256, 1024);
    p.moveTo(0, 1024); //11
    p.lineTo(128, 896);
    p.moveTo(0, 768); //12
    p.lineTo(128, 640);
    p.moveTo(0, 512); //13
    p.lineTo(128, 384);
    p.moveTo(0, 256); //14
    p.lineTo(128, 128);
    return p;
}

static pesPath CreateDefaultMotif(){ //motif014.pmf
    pesPath p;
    p.moveTo(0, 1024);
    p.lineTo(512, 1024);
    p.lineTo(512, 800);
    p.lineTo(896,800);
    p.lineTo(672,512);
    p.lineTo(800,512);
    p.lineTo(608,288);
    p.lineTo(736,288);
    p.lineTo(512,0);
    p.lineTo(288,288);
    p.lineTo(416,288);
    p.lineTo(224,512);
    p.lineTo(352,512);
    p.lineTo(128,800);
    p.lineTo(512,800);
    p.lineTo(512,1024);
    p.lineTo(1024,1024);
    
    return p;
}

// Experiment
float pesData::perimeter_distance(pesVec2f p1, pesVec2f p2, const pesPolyline & outline){
    //    # how far around the perimeter (and in what direction) do I need to go
    //    # to get from p1 to p2?
    
    unsigned int nearestIndex_p1, nearestIndex_p2;
    outline.getClosestPoint(p1, &nearestIndex_p1);
    outline.getClosestPoint(p2, &nearestIndex_p2);

    float fpos_p1 = outline.getLengthAtIndex(nearestIndex_p1);
    float fpos_p2 = outline.getLengthAtIndex(nearestIndex_p2);

    float distance = fpos_p2 - fpos_p1;
    float outline_length = outline.getPerimeter();
    
    if (fabs(distance) > outline_length / 2.0){
        //    # if we'd have to go more than halfway around, it's faster to go
        //    # the other way
        if (distance < 0)
            return distance + outline_length;
        else if (distance > 0)
            return distance - outline_length;
        else
            //    # this ought not happen, but just for completeness, return 0 if
            //    # p1 and p2 are the same point
            return 0;
    }
    else
        return distance;
}

void pesData::connectPointsOnOutline(pesVec2f last, pesVec2f p1, pesVec2f p2, const pesPolyline & outline, int fillBlockIndex){
    const int ppmm = 10;
    const float running_stitch_length = 1.0f*ppmm; // 1mm
    const float TRESHOLD = 2.0 * ppmm;
    
    unsigned int nearestIndex;
    outline.getClosestPoint(p1, &nearestIndex);
    float fpos = outline.getLengthAtIndex(nearestIndex);
    float distance = perimeter_distance(p1, p2, outline);
    int numStitches = abs(int(distance / running_stitch_length));
    float direction = copysign(1.0f, distance);
    float one_stitch = running_stitch_length * direction;
    
    float outline_length = outline.getPerimeter();
    for(int i=0; i<numStitches; i++){
        
        fpos = fpos + one_stitch;
        if(fpos>outline_length){
            fpos = fpos - outline_length;
        }
        else if(fpos<0.0){
            fpos = outline_length - fpos;
        }
        
        pesVec2f stitch = outline.getPointAtLength(fpos);
        
        if(i==0){
            float dist = last.distance(stitch);
            if(dist>=TRESHOLD){
                fillBlocks[fillBlockIndex].addJump(stitch);
            }
            else{
                fillBlocks[fillBlockIndex].addStitch(stitch);
            }
        }
        else{
            fillBlocks[fillBlockIndex].addStitch(stitch);
        }
    }
}

void pesData::findClosestPointsBetweenOutlines(const pesPolyline & outline1, const pesPolyline & outline2,
                                      pesVec2f & closest1, pesVec2f & closest2){
    float dist = INFINITY;
    for(int first=0; first<outline1.size(); first++){
        pesVec2f vFirst = outline1[first];
        for(int second=0; second<outline2.size(); second++){
            pesVec2f vSecond = outline2[second];
            float d = vFirst.distance(vSecond);
            if(d<dist){
                dist = d;
                closest1.set(vFirst);  // exit point
                closest2.set(vSecond); // enter point
            }
        }
    }
}

void pesData::connectPointsBetweenOutlines(pesVec2f p1, const pesPolyline & outline1, int fillBlockIndex1,
                                           pesVec2f p2, const pesPolyline & outline2, int fillBlockIndex2){
    pesVec2f exitPoint, enterPoint;
    findClosestPointsBetweenOutlines(outline1, outline2, exitPoint, enterPoint);
    
    pesVec2f last1 = fillBlocks[fillBlockIndex1].polyline.getVertices().back();
    connectPointsOnOutline(last1, p1, exitPoint, outline1, fillBlockIndex1);
    
    pesVec2f last2 = fillBlocks[fillBlockIndex1].polyline.getVertices().back();
    connectPointsOnOutline(last2, enterPoint, p2, outline2, fillBlockIndex2);
}

void pesData::connectPointsOnOutline(pesVec2f last, pesVec2f p1, pesVec2f p2, const pesPolyline & outline, pesStitchBlock & block){
    const int ppmm = 10;
    const float running_stitch_length = 1.0f*ppmm; // 1mm
    const float TRESHOLD = 2.0 * ppmm;
    
    unsigned int nearestIndex;
    outline.getClosestPoint(p1, &nearestIndex);
    float fpos = outline.getLengthAtIndex(nearestIndex);
    float distance = perimeter_distance(p1, p2, outline);
    int numStitches = abs(int(distance / running_stitch_length));
    float direction = copysign(1.0f, distance);
    float one_stitch = running_stitch_length * direction;
    
    float outline_length = outline.getPerimeter();
    for(int i=0; i<numStitches; i++){
        
        fpos = fpos + one_stitch;
        if(fpos>outline_length){
            fpos = fpos - outline_length;
        }
        else if(fpos<0.0){
            fpos = outline_length - fpos;
        }
        
        pesVec2f stitch = outline.getPointAtLength(fpos);
        
        if(i==0){
            float dist = last.distance(stitch);
            if(dist>=TRESHOLD){
                block.addJump(stitch);
            }
            else{
                block.addStitch(stitch);
            }
        }
        else{
            block.addStitch(stitch);
        }
    }
}
void pesData::connectPointsBetweenOutlines(pesVec2f p1, const pesPolyline & outline1, pesVec2f p2, const pesPolyline & outline2, pesStitchBlock & block){
    pesVec2f exitPoint, enterPoint;
    findClosestPointsBetweenOutlines(outline1, outline2, exitPoint, enterPoint);
    
    pesVec2f last1 = block.polyline.getVertices().back();
    connectPointsOnOutline(last1, p1, exitPoint, outline1, block);
    
    pesVec2f last2 = block.polyline.getVertices().back();
    connectPointsOnOutline(last2, enterPoint, p2, outline2, block);
}


// END Experiment


#define ENABLE_EMB_FILL_CACHE 0

#if ENABLE_EMB_FILL_CACHE
static std::shared_ptr<std::map<std::string, pesEMBFill>> __emb_fill_cache = std::make_shared<std::map<std::string, pesEMBFill>>();
#endif

const int __cw = 1;
const int __ccw= -1;

pesPath pesData::unitePaths( pesPath &tpath, pesPath &npath ) {
    bool isFill = tpath.isFill();
    bool isStroke = tpath.isStroke();
    float strokeWidth = tpath.getStrokeWidth();
    pesColor strokeColor = tpath.getStrokeColor();
    pesColor fillColor = tpath.getFillColor();
    std::string path_id = tpath.path_id;
    std::string group_id = tpath.group_id;
    int fillType = tpath.fillType;
    float fillAngle = tpath.fillAngle;
    int roleType = tpath.roleType;
    float roleOffset = tpath.roleOffset;

    SkPath path1 = toSk(tpath);
    SkPath path2 = toSk(npath);

    Op(path1, path2, SkPathOp::kUnion_SkPathOp, &path1);
    Simplify(path1, &path1);

    pesPath path = toPes( path1 );
    path.setStrokeColor( strokeColor );
    path.setFillColor( fillColor );
    path.setStrokeWidth( strokeWidth );
    path.setFilled( isFill );  
    path.path_id = path_id;
    path.group_id = group_id;
    path.fillType = fillType;
    path.fillAngle = fillAngle;
    path.roleType = roleType;
    path.roleOffset = roleOffset;
    return path;    
}

SkPath pesData::SkiaPathStroke( SkPath &skPath, float value ) {
    SkPaint paint;
    paint.setStyle( SkPaint::Style::kStroke_Style );
    paint.setStrokeWidth( value * 2);
    paint.setStrokeCap( SkPaint::Cap::kButt_Cap);
    paint.setStrokeJoin( SkPaint::Join::kBevel_Join);
    SkPath newPath = skpathutils::FillPathWithPaint(skPath, paint);
    newPath.setFillType( SkPathFillType::kEvenOdd );
    return newPath;
}

pesPath pesData::PathStrokeToFill( pesPath &spath, float scale ) { 
    //SkDebugf( "Stroke Width: %0.2f, %0.2f, %0.2f\n", spath.getStrokeWidth(), scale, spath.getStrokeWidth() * scale );
    SkPath _sPath = toSk( spath );
    SkPath _tpath = SkiaPathStroke( _sPath, spath.getStrokeWidth() * scale / 3 );
    pesPath tpath = toPes( _tpath );

    tpath.setStrokeColor( pesColor( 0, 0, 0, 0 ) );
    tpath.setFillColor( spath.getStrokeColor() );
    tpath.setStrokeWidth( 0);
    tpath.setFilled( true );
    tpath.fillType = spath.fillType;
    tpath.group_id = spath.group_id;
    tpath.path_id = spath.path_id;

    return tpath;
}

pesPath pesData::PathInset( pesPath &tpath, float value ) {
    bool isFill = tpath.isFill();
    bool isStroke = tpath.isStroke();
    float strokeWidth = tpath.getStrokeWidth();
    pesColor strokeColor = tpath.getStrokeColor();
    pesColor fillColor = tpath.getFillColor();
    int fillType = tpath.fillType;
    float fillAngle = tpath.fillAngle;
    int roleType = tpath.roleType;
    float roleOffset = tpath.roleOffset;    
    std::string group_id = tpath.group_id;
    std::string path_id = tpath.path_id;

    SkPath skPath = toSk(tpath);

    // Do inset  
    SkPath outlinePath = SkiaPathStroke( skPath, value ); 
    Simplify(outlinePath, &outlinePath);
    Op(outlinePath, skPath, SkPathOp::kReverseDifference_SkPathOp, &outlinePath);

    pesPath path = toPes( outlinePath );

    path.setStrokeColor( strokeColor );
    path.setFillColor( fillColor );
    path.setStrokeWidth( strokeWidth );
    path.setFilled( isFill );
    path.fillType = fillType;
    path.fillAngle = fillAngle;
    path.roleType = roleType;
    path.roleOffset = roleOffset;    
    path.group_id = group_id;
    path.path_id = path_id;

    return path;    
}

pesPath pesData::PathOutset( pesPath &tpath, float value ) {
    bool isFill = tpath.isFill();
    bool isStroke = tpath.isStroke();
    float strokeWidth = tpath.getStrokeWidth();
    pesColor strokeColor = tpath.getStrokeColor();
    pesColor fillColor = tpath.getFillColor();
    int fillType = tpath.fillType;
    float fillAngle = tpath.fillAngle;
    int roleType = tpath.roleType;
    float roleOffset = tpath.roleOffset;    
    std::string group_id = tpath.group_id;
    std::string path_id = tpath.path_id;

    SkPath skPath = toSk(tpath);

    // Do Outset  
    SkPath outlinePath = SkiaPathStroke( skPath, value ); 
    Simplify(outlinePath, &outlinePath);
    Op(outlinePath, skPath, SkPathOp::kDifference_SkPathOp, &outlinePath);
    //Simplify(outlinePath, &outlinePath);

    pesPath path = toPes( outlinePath );

    path.setStrokeColor( strokeColor );
    path.setFillColor( fillColor );
    path.setStrokeWidth( strokeWidth );
    path.setFilled( isFill );
    path.fillType = fillType;
    path.fillAngle = fillAngle;
    path.roleType = roleType;
    path.roleOffset = roleOffset;    
    path.group_id = group_id;
    path.path_id = path_id;

    return path;        
}

struct ShapeInfo {
    pesPath path;
    pesPath outline;
    pesVec2f cxy{0, 0};
    bool bWorked{false};
    int id{-1};
    int members{0};

    ShapeInfo(pesPath& p) : path(p) {
        int points = 0;
        for (const auto& c : p.getCommands()) {
            switch (c.type) {
                default:
                    continue;

                case pesPath::Command::Type::_moveTo:
                case pesPath::Command::Type::_lineTo:
                case pesPath::Command::Type::_quadBezierTo:
                case pesPath::Command::Type::_bezierTo:
                    cxy += c.to;
                    points++;
                    break;
            }
        }
        if (points) {
            cxy /= points;
        }
    }

    ShapeInfo* nextshape(std::vector<ShapeInfo>& shapes) {
        return nextshape(shapes, this->cxy, this);
    }

    static ShapeInfo* nextshape(std::vector<ShapeInfo>& shapes,
                                pesVec2f& from,
                                ShapeInfo* instance = nullptr) {
        // pesVec2f* from = &this->cxy;
        ShapeInfo* next = nullptr;
        auto mindist = SK_ScalarMax;
        for (auto& s : shapes) {
            if (instance == &s || s.bWorked) {
                continue;
            }
            auto d = from.distance(s.cxy);
            if (mindist > d) {
                mindist = d;
                next = &s;
            }
        }
        if (next) {
            next->bWorked = true;
        }
        return next;
    }

    pesPolyline getOutline(float tol = 0.2f) {
        auto outline = this->outline.getOutline().front();
        addClosePointIfNeed(outline);
        outline.simplify(tol);
        return outline;
    }

    static void sortPositionsAroundPointInCwOrder(std::vector<ShapeInfo>& shapes,
                                                  const pesVec2f& center) {
        pesSort(shapes, [&](const auto& s0, const auto& s1) -> bool {
            return center.angleRad(s0.cxy) < center.angleRad(s1.cxy);
        });
    }

    static int getSubPathsSize(const SkPath& path) {
        int movecount = 0;
        for (int verb = 0, verbs = path.countVerbs(); verb < verbs; verb++) {
            if (pes_skia::getVerb(path, verb) == SkPath::Verb::kMove_Verb) {
                movecount++;
            }
        }
        return movecount;
    }

    static void addClosePointIfNeed(pesPolyline& polyline) {
        const auto& first = polyline.getVertices().front();
        const auto& last = polyline.getVertices().back();
        if (last.x != first.x || last.y != first.y) {
            polyline.addVertex(first);
        }
    }

    static pesPath makeOutline(const pesPath& p) {
        //SkPath skp = toSk(p);
        //AsWinding(skp, &skp);
        //auto pp = toPes(skp);
        //skp.reset();
        ////skp.setFillType((SkPathFillType)pp.fillRule);
        PesPath skp;
        skp.setFillType(SkPathFillType::kWinding);
        for (const auto& subpp : p.getSubPath()) {
            auto subskp = toSk(subpp);
            if (SkPathFirstDirection::kCCW == SkPathPriv::ComputeFirstDirection(subskp)) {
                skp.reverseAddPath(subskp);
            } else {
                skp.addPath(subskp);
            }
        }
        //Op(skp, SkPath(), kUnion_SkPathOp, &skp);
        //SkDebugf("++size:%d\n", getSubPathsSize(skp));
        skp.simplify();
        //SkDebugf("--size:%d\n", getSubPathsSize(skp));

        //skp.setFillType((SkPathFillType)0);
        auto pp = toPes(skp);
        skp.reset();
        skp.setFillType(SkPathFillType::kWinding);
        for (const auto& subpp : pp.getSubPath()) {
            auto subskp = toSk(subpp);
            if (SkPathFirstDirection::kCCW == SkPathPriv::ComputeFirstDirection(subskp)) {
                skp.reverseAddPath(subskp);
            } else {
                skp.addPath(subskp);
            }
        }

        //Op(skp, SkPath(), kUnion_SkPathOp, &skp);
        //SkDebugf("++size:%d\n", getSubPathsSize(skp));
        skp.simplify();
        //SkDebugf("--size:%d\n", getSubPathsSize(skp));

        //skp.setFillType(SkPathFillType::kWinding);

        pp = toPes(skp);
        skp.reset();
        return pp;
    }

    // use makeSingle always right if...
    // - fillRule is invert
    // use makeSingle may be wrong if...
    // - outline contour more than one
    static void makeSingle(const pesRectangle& bound, pesPath& p, std::vector<pesPath>& subpaths, std::vector<ShapeInfo>& shapeinfos) {
        shapeinfos.clear();
        int shapeid = 0;
        int fillRule = p.fillRule;
        bool isInverse = fillRule & 2;

        auto& shapeinfo = shapeinfos.emplace_back(p);
        if (isInverse) {
            //pesRectangle bound = shapeinfo.path.getBoundingBox();
            const auto rect = SkRect::MakeXYWH(bound.x, bound.y, bound.width, bound.height);
            shapeinfo.outline = toPes(PesPath().addRect(rect));
        } 
        else {
            if (subpaths.size() < 2) {
                shapeinfo.outline = shapeinfo.path;
            }
            else {
                auto ppoutline = makeOutline(p);
                auto ppoutlines = ppoutline.getSubPath();
                pesSort(ppoutlines, [&](const pesPath& p0, const auto& p1) -> bool {
                    return p0.getBoundingBox().getArea() > p1.getBoundingBox().getArea();
                });
                shapeinfo.outline = ppoutlines.front();
            }
        }
        shapeinfo.id = shapeid++;
        shapeinfo.members = (int)subpaths.size();
    }
    
    static void makeGroups(const pesRectangle& bound, pesPath& p, std::vector<pesPath>& subpaths, std::vector<ShapeInfo>& shapeinfos) {
#ifndef __EMSCRIPTEN__
        //SkAutoTime skautotime("ShapeInfo::makeGroups");
#endif

        if (subpaths.size() < 2) {
            makeSingle(bound, p, subpaths, shapeinfos);
            return;
        }

        shapeinfos.clear();
        int shapeid = 0;
        int fillRule = p.fillRule;
        bool isInverse = fillRule & 2;

        if (isInverse) {
            auto& shapeinfo = shapeinfos.emplace_back(p);
            pesRectangle bound = shapeinfo.path.getBoundingBox();
            const auto rect = SkRect::MakeXYWH(bound.x, bound.y, bound.width, bound.height);
            shapeinfo.outline = toPes(PesPath().addRect(rect));
            shapeinfo.id = shapeid++;
            shapeinfo.members = (int)subpaths.size();
            return;
        }

        auto ppoutline = makeOutline(p);

        auto ppoutlines = ppoutline.getSubPath();

        // sort outlines from bigger to smaller
        //for (auto& path : ppoutlines) {
        //    path.simplify(0.4f);
        //}
        //pesSort(ppoutlines, [&](const auto& p0, const auto& p1) -> bool {
        //    return fabs(p0.getOutline()[0].getArea()) > fabs(p1.getOutline()[0].getArea());
        //});

        struct SubpathsInfo {
            SubpathsInfo(pesPath& pp, pesVec2f& v0, pesVec2f& v1, pesVec2f& v2) {
                x0 = v0.x;
                y0 = v0.y;
                x1 = v1.x;
                y1 = v1.y;
                x2 = v2.x;
                y2 = v2.y;
                ppp = &pp;
            }
            float x0{0};
            float y0{0};
            float x1{0};
            float y1{0};
            float x2{0};
            float y2{0};
            pesPath* ppp{nullptr};
        };

        std::vector<SubpathsInfo> subpathsinfos;
        for (auto& pp : subpaths) {
            auto& cmd = pp.getCommands();
            if (cmd.size() > 3 && cmd.back().type == pesPath::Command::Type::_close) {
                auto& cmd = pp.getCommands();
                pesVec2f point0 = cmd[0].to;
                pesVec2f point1 = cmd[cmd.size() / 2].to;
                for (int i = cmd.size() - 2, ii = 1; i > ii; i--) {
                    if (point0 != cmd[i].to) {
                        pesVec2f point2 = cmd[i].to;
                        subpathsinfos.emplace_back(SubpathsInfo(pp, point0, point1, point2));
                        break;
                    }
                }
            }
        }

        int nSum = 0;
        int nGroup = -1;
        int nIntersect = 0;
        for (auto& ppoutline : ppoutlines) {
            if (subpathsinfos.empty()) {
                break;
            }

            nGroup++;

            ppoutline.fillRule = 0;
            auto skpgroup = toSk(ppoutline);

            nIntersect = 0;
            pesPath ppIntersects;
            ppIntersects.fillRule = fillRule;

            // find and keep subpaths that intersect with skpgroup.
            for (auto iter = subpathsinfos.begin(); iter != subpathsinfos.end();) {
                auto& node = *iter;
                if (skpgroup.contains(node.x0, node.y0)) {
                    nIntersect++;
                    ppIntersects.append(*node.ppp);
                    iter = subpathsinfos.erase(iter);
                    continue;
                }
                ++iter;
            }

            if (nIntersect == 0) {
                for (auto iter = subpathsinfos.begin(); iter != subpathsinfos.end();) {
                    auto& node = *iter;
                    if (skpgroup.contains(node.x1, node.y1)) {
                        nIntersect++;
                        ppIntersects.append(*node.ppp);
                        iter = subpathsinfos.erase(iter);
                        continue;
                    }
                    ++iter;
                }
            }

            if (nIntersect == 0) {
                for (auto iter = subpathsinfos.begin(); iter != subpathsinfos.end();) {
                    auto& node = *iter;
                    if (skpgroup.contains(node.x2, node.y2)) {
                        nIntersect++;
                        ppIntersects.append(*node.ppp);
                        iter = subpathsinfos.erase(iter);
                        continue;
                    }
                    ++iter;
                }
            }

            if (nIntersect) {
                nSum += nIntersect;
                auto& shapeinfo = shapeinfos.emplace_back(ppIntersects);
                shapeinfo.outline = ppoutline;
                shapeinfo.id = shapeid++;
                shapeinfo.members = nIntersect;
            }
            //SkDebugf("group: %d, members: %d\n", nGroup, nIntersect);
        }

        if (ppoutlines.size() == shapeinfos.size() && subpaths.size() != nSum) {
            nGroup = -1;
            for (auto& ppoutline : ppoutlines) {
                if (subpathsinfos.empty()) {
                    break;
                }

                nGroup++;

                ppoutline.fillRule = 0;
                auto skpgroup = toSk(ppoutline);

                // find and keep subpaths that intersect with skpgroup.
                for (auto iter = subpathsinfos.begin(); iter != subpathsinfos.end();) {
                    auto& node = *iter;
                    if (skpgroup.contains(node.x1, node.y1)) {
                        shapeinfos[nGroup].path.append(*node.ppp);
                        shapeinfos[nGroup].members++;
                        nSum++;
                        iter = subpathsinfos.erase(iter);
                        continue;
                    }
                    ++iter;
                }
            }
        }

        if (ppoutlines.size() == shapeinfos.size() && subpaths.size() != nSum) {
            nGroup = -1;
            for (auto& ppoutline : ppoutlines) {
                if (subpathsinfos.empty()) {
                    break;
                }

                nGroup++;

                ppoutline.fillRule = 0;
                auto skpgroup = toSk(ppoutline);

                // find and keep subpaths that intersect with skpgroup.
                for (auto iter = subpathsinfos.begin(); iter != subpathsinfos.end();) {
                    auto& node = *iter;
                    if (skpgroup.contains(node.x2, node.y2)) {
                        shapeinfos[nGroup].path.append(*node.ppp);
                        shapeinfos[nGroup].members++;
                        nSum++;
                        iter = subpathsinfos.erase(iter);
                        continue;
                    }
                    ++iter;
                }
            }
        }

#ifndef __EMSCRIPTEN__
        if (subpaths.size() != nSum) {
            SkDebugf("groups:%lu, shapeinfos:%lu, subpaths:%lu, nSum:%d\n", ppoutlines.size(), shapeinfos.size(), subpaths.size(), nSum);
        }
#endif

    }

    void printdebug() { SkDebugf("id: %d, cxy: [%g, %g]\n", id, cxy.x, cxy.y); }
};

pesPath joinSatincolumnToOutline(pesPath& path0, pesPath& path1) {
    auto get_total_signed_area = [](const pesVec2f& vect0, const pesVec2f& vect1) -> float {
        return (vect0.y - vect1.y) * (vect0.x + vect1.x);
    };
    
    auto path0cmds = path0.getCommands();
    auto path1cmds = path1.getCommands();

    auto& vec0 = path0cmds[0].to;
    auto& vec1 = path0cmds[1].to;
    auto& vec2 = path1cmds[1].to;
    auto& vec3 = path1cmds[0].to;
    auto area = get_total_signed_area(vec0, vec1);
    area += get_total_signed_area(vec1, vec2);
    area += get_total_signed_area(vec2, vec3);
    area += get_total_signed_area(vec3, vec0);
    area = -area;

    //  cw: then path0 + rev.path1
    // ccw: then path1 + rev.path0
    bool iscw = area > 0;
    auto path = iscw ? path0 : path1;
    auto& revcmds = iscw ? path1cmds : path0cmds;
    std::reverse(revcmds.begin(), revcmds.end());
    bool needclose = true;
    const auto& jump = revcmds[0].to;
    path.lineTo(jump.x, jump.y);
    for (int m = 0, n = 1, nn = (int)revcmds.size(); n < nn; m++, n++) {
        auto cmd = revcmds[m];
        switch (cmd.type) {
            case pesPath::Command::Type::_moveTo:
                path.close();
                needclose = false;
                break;

            default:
                cmd.to = revcmds[n].to;
                std::swap(cmd.cp1, cmd.cp2);
                path.addCommand(cmd);
                break;
        }
    }
    if (needclose) {
        path.close();
    }
    return path;
}

struct SatinColumnInfo {
    pesPath p0;
    pesPath p1;

    float ff0x{0};
    float ff0y{0};
    float ff1x{0};
    float ff1y{0};

    float fl0x{0};
    float fl0y{0};
    float fl1x{0};
    float fl1y{0};

    float fx{0};
    float fy{0};

    int idx{-1};
    int gidx{-1};
    float area{0};
    
    int priority{0};
    SkPath skpoutline;
    
    std::vector<pesPolyline> slicepatches;
    pesPolyline underlaypatch;
    
    SatinColumnInfo(pesPath& pp0,
                    pesPath& pp1,
                    const int nidx,
                    const float pull_compensation_mm) {

        p0.append(pp0);
        p1.append(pp1);
        idx = nidx;

        if (pull_compensation_mm != 0) {
            const float pullCompensate = pull_compensation_mm * 10;
            auto& p0cmds = p0.getCommands();
            auto& p1cmds = p1.getCommands();
            pesPolyline centerline;
            for (int m = 0, mm = (int)p0cmds.size(); m < mm; m++) {
                centerline.addVertex(p0cmds[m].to.getMiddle(p1cmds[m].to));
            }
            // mockup next middle, required by lastidx
            int lastidx = centerline.size() - 1;
            auto last = centerline[lastidx];
            centerline.addVertex(last + (last - centerline[lastidx - 1]).normalize() * 10);

            const float C90DIR  = 90  * DEG_TO_RAD;
            const float C45DIR  = 45  * DEG_TO_RAD;
            const float C135DIR = 135 * DEG_TO_RAD;

            pesVec2f prevd0, prevd1;
            
            for (int i = 0, j = 1, jj = (int)centerline.size(), jjj = jj - 1; j < jj; i++, j++) {
                auto& p0cmd = p0cmds[i];
                auto& p1cmd = p1cmds[i];
                auto& side0 = p0cmd.to;
                auto& side1 = p1cmd.to;

                const auto& midi = centerline[i];
                const auto& midj = centerline[j];
                auto d0 =  midj - midi;
                auto d1 = side1 - midi;
                auto dot = d0.dot(d1);
                float angle = dot == 0 ? C90DIR : acosf(dot / d0.length() / d1.length());
                //if (angle < C45DIR) angle += C45DIR;
                //angle = abs(fmod(angle - C45DIR, C90DIR) + C45DIR);
                if (i > 0 && i < jjj) {
                    if (angle < C45DIR) {
                        angle = C45DIR;
                    } 
                    else if (angle > C135DIR) {
                        angle = C135DIR;
                    }
                }

                // clean code
                float rscale = 1.0f / cosf(C90DIR - angle);

                //auto newside0 = side0 + (side0 - side1).normalize() * pullCompensate * rscale;
                //auto newside1 = side1 + (side1 - side0).normalize() * pullCompensate * rscale;
                //
                //d0 = newside0 - side0;
                //d1 = newside1 - side1;
                //
                //side0 = newside0;
                //side1 = newside1;

                const auto pull = d1.normalize() * pullCompensate * rscale;
                d0 = -pull;
                d1 = pull;
                side0 += d0;
                side1 += d1;

                if (p0cmd.type == pesPath::Command::Type::_bezierTo ||
                    p0cmd.type == pesPath::Command::Type::_quadBezierTo) {
                    //p0cmd.cp1 += d0;
                    if (i > 0) {
                        p0cmd.cp1 += prevd0;
                    }
                    p0cmd.cp2 += d0;
                }
                if (p1cmd.type == pesPath::Command::Type::_bezierTo ||
                    p1cmd.type == pesPath::Command::Type::_quadBezierTo) {
                    //p1cmd.cp1 += d1;
                    if (i > 0) {
                        p1cmd.cp1 += prevd1;
                    }
                    p1cmd.cp2 += d1;
                }
                prevd0 = d0;
                prevd1 = d1;
            }
        }

        auto& p0cmds = p0.getCommands();
        auto& p1cmds = p1.getCommands();
        if (p0cmds.size() > 1 && p0cmds.size() == p1cmds.size()) {
            auto fidx = 0;
            auto lidx = p0cmds.size() - 1;

            auto vf0 = p0cmds[fidx].to;
            auto vf1 = p1cmds[fidx].to;
            ff0x = vf0.x;
            ff0y = vf0.y;
            ff1x = vf1.x;
            ff1y = vf1.y;

            auto vl0 = p0cmds[lidx].to;
            auto vl1 = p1cmds[lidx].to;
            fl0x = vl0.x;
            fl0y = vl0.y;
            fl1x = vl1.x;
            fl1y = vl1.y;

            if (p0cmds.size() == 2) {
                auto vmid0 = p0cmds[0].to.getMiddle(p0cmds[1].to);
                auto vmid1 = p1cmds[0].to.getMiddle(p1cmds[1].to);
                auto vmid01 = vmid0.getMiddle(vmid1);
                fx = vmid01.x;
                fy = vmid01.y;
            }
            else if (p0cmds.size() == 3) {
                auto vmid0 = p0cmds[1].to;
                auto vmid1 = p1cmds[1].to;
                auto vmid01 = vmid0.getMiddle(vmid1);
                fx = vmid01.x;
                fy = vmid01.y;
            } 
            else {
                auto vmid0 = p0cmds[1].to.getMiddle(p0cmds[2].to);
                auto vmid1 = p1cmds[1].to.getMiddle(p1cmds[2].to);
                auto vmid01 = vmid0.getMiddle(vmid1);
                fx = vmid01.x;
                fy = vmid01.y;
            }
        }

        auto outline = joinSatincolumnToOutline(p0, p1);
        outline.simplify(0.2);
        area = outline.getOutline()[0].getArea();
        skpoutline = toSk(outline);
    }
};

struct SatinColumnInfoGroup {
    int idx{-1};
    SkPath skpoutline;
    bool bgeted{false};
    bool bskiped{false};
    std::vector<std::shared_ptr<SatinColumnInfo>> scInfos;

    SatinColumnInfoGroup(int nidx, SkPath& skp) {
        idx = nidx;
        skpoutline = skp;
    }
};


// Current version
void pesData::applyFill() {
    // (SkAutoTime profiling removed: dropped from Skia M150)

    fillBlocks.clear();
    
    pesPolyline prevOutline;
    pesVec2f prevStitch;
    int prevFillBlocksIndex = -1;
    
    if(paths.size()){
        pesRectangle bound = getScalableBoundingBox();
        int numBlock = (int)paths.size();
        fillBlocks.resize(numBlock);

        int blockIndex = 0;
        for (const auto& p : paths) {
            if (p.bVisible && p.isFill()) {
                fillBlocks[blockIndex].clear();
                fillBlocks[blockIndex++].setNearestColor(p.getFillColor());
                //fillBlocks[blockIndex++].setColorFromIndex(pesGetNearestBrotherColorHSLfIndex(p.getFillColor()));
                //fillBlocks[blockIndex++].setColorFromIndex(pesGetNearestBrotherColorRGBfIndex(p.getFillColor()));
            }
        }
        fillBlocks.resize(blockIndex);
        blockIndex = 0;

        pesEMBFillPreset::setDefaultPreset((pesEMBFillPreset::Preset)parameter.fill.presetIndex);
        pesEMBFill::Preset preset = pesEMBFillPreset::getDefaultPreset();

        // -------------------------------------------------------
        // Fill Dynamic
        // -------------------------------------------------------
        if(parameter.fillType==FillType::FILL_TYPE_DYNAMIC){
            const bool bUniteCutter = true;

            // build new paths stack only fill path and visible
            std::vector<pesPath>_ppaths;
            for( auto p: paths ) {
                if( !p.bVisible ) {
                    continue;
                }
                if( p.isFill() ) {
                    pesPath _p = pesPath(p);
                    _ppaths.push_back( _p );
                }
            }

            // Cut Fill
            if( 1 ) {
                for( int pathIndex = _ppaths.size()-1; pathIndex >= 0; pathIndex-- ) {                   
                    if( _ppaths[pathIndex].roleType != 2 && _ppaths[pathIndex].roleType != 3 ) { // not cutter
                        continue;
                    }
                    if( _ppaths[pathIndex].isStroke() )
                        continue;
                    // cutter
                    pesPath epath0 = _ppaths[pathIndex];

                    // Unite cutter in same group
                    if( bUniteCutter ) {
                        SkPath _epath0 = toSk( epath0 );
                            
                        SkOpBuilder builder;
                        builder.add(_epath0, kUnion_SkPathOp);
                        
                        int pi = pathIndex-1;
                        for( ; pi >= 0; pi-- ) {
                            if( _ppaths[pi].isStroke() )
                                continue;
                            if( _ppaths[pi].group_id == epath0.group_id ) {
                                SkPath _path = toSk( _ppaths[pi] );
                                builder.add(_path, kUnion_SkPathOp);
                            }
                            else {
                                break;
                            }
                        }
                        pathIndex = pi+1;

                        pes_skia::resolveBuilder(builder,&_epath0); 
                        epath0 = toPes(_epath0);
                        //epath0.setFillColor( _ppaths[pathIndex].getFillColor() );                        
                        //_ppaths[pathIndex].clear();
                        //_ppaths[pathIndex] = epath0;                       
                    }

                    pesPath epath;
                    if( epath0.roleOffset < 0 ) {
                        epath = PathInset( epath0, 0-epath0.roleOffset ); //
                    }
                    else if( epath0.roleOffset > 0 ) {
                        epath = PathOutset( epath0, epath0.roleOffset ); //
                    }
                    else {
                        epath = pesPath(epath0);
                    }
                    //SkDebugf( "Cutter: %s %0.2f\n", epath.path_id.c_str(), epath.roleOffset );
                    SkPath eraserPath = toSk(epath);
                    eraserPath.setFillType( SkPathFillType::kWinding );
                    //Simplify( eraserPath, &eraserPath );

                    for( int ip=pathIndex-1; ip>=0; ip-- ) {
                        pesPath &tpath = _ppaths[ip];
                        if( tpath.roleType != 1  && tpath.roleType != 3 ) // not cutable
                            continue; 
                        if( tpath.isStroke() )
                            continue;

                        // Cuttable underneath
                        bool isFill = tpath.isFill();
                        bool isStroke = tpath.isStroke();
                        float strokeWidth = tpath.getStrokeWidth();
                        pesColor strokeColor = tpath.getStrokeColor();
                        pesColor fillColor = tpath.getFillColor();
                        int fillType = tpath.fillType;
                        float fillAngle = tpath.fillAngle;
                        int roleType = tpath.roleType;
                        float roleOffset = tpath.roleOffset;
                        std::string path_id = tpath.path_id;
                        std::string group_id = tpath.group_id;

                        SkPath targetPath = toSk(tpath);
                        targetPath.setFillType( SkPathFillType::kWinding );

                        SkOpBuilder builder;
                        if( epath0.roleOffset < 0 ) {
                            SkPath e0 = toSk(epath0);
                            e0.setFillType( SkPathFillType::kEvenOdd );
                            SkPath o0 = SkiaPathStroke( targetPath, (0-epath0.roleOffset) );
                            builder.add(e0, kUnion_SkPathOp);
                            builder.add(o0, kIntersect_SkPathOp);
                            builder.add(eraserPath, kUnion_SkPathOp);
                            builder.add(targetPath, kReverseDifference_SkPathOp);
                        }
                        else {
                            //Op(eraserPath, targetPath, SkPathOp::kReverseDifference_SkPathOp, &targetPath);
                            builder.add(eraserPath, kUnion_SkPathOp);
                            builder.add(targetPath, kReverseDifference_SkPathOp);
                        }
                        pes_skia::resolveBuilder(builder,&targetPath);                                                  

                        pesPath path = toPes( targetPath );
                        path.setStrokeColor( strokeColor );
                        path.setFillColor( fillColor );
                        path.setStrokeWidth( strokeWidth );
                        path.setFilled( isFill );
                        path.fillType = fillType;
                        path.fillAngle = fillAngle;
                        path.roleType = roleType;
                        path.roleOffset = roleOffset;
                        path.path_id = path_id;
                        path.group_id = group_id;

                        _ppaths[ip].clear();
                        _ppaths[ip] = path;                            
                    }
                }
            }        
            // Cut Self if unite cutter
            if( bUniteCutter ) {
                for( int pathIndex = _ppaths.size()-1; pathIndex >= 0; pathIndex-- ) {                   
                    if( _ppaths[pathIndex].roleType != 2 && _ppaths[pathIndex].roleType != 3 ) { // not cutter
                        continue;
                    }
                    if( _ppaths[pathIndex].isStroke() )
                        continue;
                    // cutter
                    pesPath &epath0 = _ppaths[pathIndex]; // ChatGPT
                    SkPath e0 = toSk(epath0);

                    pesPath epath;
                    if( epath0.roleOffset < 0 ) {
                        epath = PathInset( epath0, 0-epath0.roleOffset ); //
                    }
                    else if( epath0.roleOffset > 0 ) {
                        epath = PathOutset( epath0, epath0.roleOffset ); //
                    }
                    else {
                        epath = pesPath(epath0);
                    }
                    //SkDebugf( "Cutter: %s %0.2f\n", epath.path_id.c_str(), epath.roleOffset );
                    SkPath eraserPath = toSk(epath);
                    eraserPath.setFillType( SkPathFillType::kWinding );
                    Simplify( eraserPath, &eraserPath );

                    for( int ip=pathIndex-1; ip>=0; ip-- ) {
                        pesPath &tpath = _ppaths[ip];
                        if( tpath.group_id != epath.group_id ) 
                            continue;
                        if( tpath.roleType != 1  && tpath.roleType != 3 ) // not cutable
                            continue; 
                        if( tpath.isStroke() )
                            continue;

                        // Cuttable underneath
                        bool isFill = tpath.isFill();
                        bool isStroke = tpath.isStroke();
                        float strokeWidth = tpath.getStrokeWidth();
                        pesColor strokeColor = tpath.getStrokeColor();
                        pesColor fillColor = tpath.getFillColor();
                        int fillType = tpath.fillType;
                        float fillAngle = tpath.fillAngle;
                        int roleType = tpath.roleType;
                        float roleOffset = tpath.roleOffset;
                        std::string path_id = tpath.path_id;
                        std::string group_id = tpath.group_id;

                        SkPath targetPath = toSk(tpath);
                        targetPath.setFillType( SkPathFillType::kWinding );
                        Simplify( targetPath, &targetPath );
                        //SkDebugf( "--- Cut fill %s\n", tpath.path_id.c_str() );

                        SkOpBuilder builder;
                        if( epath0.roleOffset < 0 ) {
                            SkPath o0 = SkiaPathStroke( targetPath, (0-epath0.roleOffset) );
                            builder.add(e0, kUnion_SkPathOp);
                            builder.add(o0, kIntersect_SkPathOp);
                            builder.add(eraserPath, kUnion_SkPathOp);
                            builder.add(targetPath, kReverseDifference_SkPathOp);
                        }
                        else {
                            //Op(eraserPath, targetPath, SkPathOp::kReverseDifference_SkPathOp, &targetPath);
                            builder.add(eraserPath, kUnion_SkPathOp);
                            builder.add(targetPath, kReverseDifference_SkPathOp);
                        }
                        pes_skia::resolveBuilder(builder,&targetPath);                        

                        pesPath path = toPes( targetPath );
                        path.setStrokeColor( strokeColor );
                        path.setFillColor( fillColor );
                        path.setStrokeWidth( strokeWidth );
                        path.setFilled( isFill );
                        path.fillType = fillType;
                        path.fillAngle = fillAngle;
                        path.roleType = roleType;
                        path.roleOffset = roleOffset;
                        path.path_id = path_id;
                        path.group_id = group_id;

                        _ppaths[ip].clear();
                        _ppaths[ip] = path;                            
                    }
                    
                }
            }      

            //fillBlocks.resize(numBlock);
            fillBlocks.resize(_ppaths.size());
            for (size_t i = 0; i < _ppaths.size(); i++) {
                fillBlocks[i].setNearestColor(_ppaths[i].getFillColor());
                _ppaths[i]._fillColorIndex = fillBlocks[i].colorIndex;
            }               

            std::vector<pesPath>sorted_ppaths;
            #define SORT_FILL true
            if( SORT_FILL ) {
                std::vector<int>colors;
                for (int ic = 0; ic < fillBlocks.size(); ic++) {
                    const auto& block = fillBlocks[ic];
                    bool duplicate = false;
                    for (auto c : colors) {
                        if (block.colorIndex == c) {
                            duplicate = true;
                            break;
                        }
                    }
                    if(!duplicate){
                        colors.push_back(block.colorIndex);
                    }
                }  
                //std::reverse( colors.begin(), colors.end() );

                for( int c: colors ) {
                    for( auto p: _ppaths ) {
                        if( p._fillColorIndex == c ) {
                            sorted_ppaths.push_back( p );
                        }
                    }
                }          
                for (size_t i = 0; i < sorted_ppaths.size(); i++) {
                    //fillBlocks[i].setNearestColor(sorted_ppaths[i].getFillColor());
                    fillBlocks[i].setColorFromIndex(sorted_ppaths[i]._fillColorIndex);
                }                  
            }
            else {
                sorted_ppaths = _ppaths;
            }

            // -------------------------------------------------------
            // Paths OK
            int pindex = 0;
            for(auto p: sorted_ppaths){
                vector<pesPath> subpath = p.getSubPath();
                if(parameter.underlayFill.underlay){
                    for(auto & path : subpath){
                        const float pes_one_mm = 10.0;
                        float compensate = fabs(parameter.underlayFill.shrinkSize * pes_one_mm) * -1; // alway negative
                        auto offsets = pesClipper::getOffsets(path.getSubPath(), compensate, ClipperLib::jtSquare, ClipperLib::etClosedPolygon);
                        path.clear();
                        for (auto& offset : offsets) {
                            offset.simplify();
                            auto& vertices = offset.getVertices();
                            path.moveTo(vertices[0].x, vertices[0].y);
                            for (int i = 1; i < vertices.size(); i++) {
                                path.lineTo(vertices[i].x, vertices[i].y);
                            }
                            path.close();
                        }
                    }
                }
                
                
                
                if(subpath.size()){
                    int first_outer_dir = subpath[0].getOutline()[0].getArea()>=0 ? __cw : __ccw;

                    #define SORT_SUBPATH true
                    if( SORT_SUBPATH ){
                        pesSort(subpath, [](const pesPath & p0, const pesPath & p1)->bool{
                            //if(fabs(p0.getOutline()[0].getArea())>fabs(p1.getOutline()[0].getArea()))
                            if( p0.getOutline()[0].getCentroid2D().x < p1.getOutline()[0].getCentroid2D().x )
                                return true;
                            return false;
                        });
                    }


                    //for(int shell=0; shell<(int)subpath.size(); shell++){
                    //    pesEMBFill epf;
                    //    std::string hashkey;
                    //
                    //    float shell_area = subpath[shell].getOutline()[0].getArea();
                    //    // Ignore small area (eye hilight is around 50)
                    //    if( shell_area >= -15 && shell_area <= 15 ) {
                    //        continue;
                    //    }
                    //
                    //    int shell_dir = subpath[shell].getOutline()[0].getArea()>=0 ? __cw : __ccw;
                    //    if(shell_dir==first_outer_dir){
                    //        pesPath shell_and_hole = subpath[shell];
                    //        // Forming single shell & hole
                    //        for(int hole=0; hole<(int)subpath.size(); hole++){
                    //            int hole_dir = subpath[hole].getOutline()[0].getArea()>=0 ? __cw : __ccw;
                    //            if(hole_dir!=first_outer_dir){
                    //                shell_and_hole.append(subpath[hole]);
                    //            }
                    //        }

/////////////////// start >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>

                    //already check above
                    //if (!p.isFill()) {
                    //    continue;
                    //}

                    const auto bound = subpath[0].getBoundingBox();
                    const auto vLeftTop = pesVec2f(bound.getMinX(), bound.getMinY());
                    const auto cxy = bound.getCenter();

                    std::vector<ShapeInfo> shapeinfos;

                    bool bGroups = true;
                    if (bGroups) {
                        // std::vector<int> orders;
                        // GetWindingContourOrdered(skp, &orders);
                        //bool isColorReduction = parameter.text == "Color-Reduction";
                        ShapeInfo::makeGroups(bound, p, subpath, shapeinfos);
                    }
                    else {
                        //shapeinfos.emplace_back(p).id = 0;
                        ShapeInfo::makeSingle(bound, p, subpath, shapeinfos);
                    }

                    if (!shapeinfos.size()) {
                        //SkDebugf("pindex: %d, shapeinfos.size: %lu\n", pindex, shapeinfos.size());
                        pindex++;
                        continue;
                    }

                    ShapeInfo::sortPositionsAroundPointInCwOrder(shapeinfos, cxy);

                    ShapeInfo* shape = nullptr;
                    pesVec2f from = vLeftTop;
                    while ((shape = ShapeInfo::nextshape(shapeinfos, from, shape)) != nullptr) {
                        from = shape->cxy;
                        auto& shell_and_hole = shape->path;

                        pesEMBFill epf;

                        // mock if above
                        if (true) {

/////////////////// end <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<

                            //shell_and_hole.simplify(0.3f);

                            float density = parameter.fill.density;
                            float sewDirection = p.fillAngle;
                            float pullCompensate = parameter.pullCompensate;
                            bool fillFading = parameter.fillFading;
                            
                            // experiment dynamic tolerance
                            //const float density2x = density * 2.0f;
                            //const auto mintolerance = 1.0f / density2x;
                            //shell_and_hole.simplify(mintolerance);
                            //auto outline = shape->getOutline(mintolerance);
                            auto outline = shape->getOutline(0.2f);

                            if( sewDirection == 0 || sewDirection == -1 ) {
                                // Calculate sew direction
                                //const pesPolyline &outline = subpath[shell].getOutline()[0];
                                //const pesPolyline& outline = shell_and_hole.getOutline()[0];
                                const std::vector<pesVec2f>& overties = outline.getVertices();
                                pesVec2f cen = outline.getCentroid2D(); // .getBoundingBox().getCenter();
                                pesVec2f far;

                                float fdist = -1;
                                for (int o = 0; o < overties.size(); o++) {
                                    float cdist = cen.distance(overties[o]);
                                    if( cdist > fdist ) {
                                        far = overties[o];
                                        fdist = cdist;
                                    }
                                }
                                pesVec2f far0 = far - cen;
                                far0.y = 0 - far0.y;
                                pesVec2f rt( 0, 1.0 );

                                // Set Sew Direction
                                sewDirection = far0.angle( rt );
                            }
                    
                            // Create single fill patch for one shell & hole
                            vector<pesPolyline> patches;
                            vector<bool> jumps;

#if ENABLE_EMB_FILL_CACHE
                            std::string hashkey = pesVAArgsToString("r=%d-t=%d-d=%.2f-s=%d-n=%d-p=%.2f-a=%.2f-v=%d-cx=%.2f-cy=%.2f-cm=%.2f", shell_and_hole.fillRule, parameter.fillType, density, sewDirection, shell_and_hole.getOutline().size(), outline.getPerimeter(), outline.getArea(), outline.getVertices().size(), outline.getCentroid2D().x, outline.getCentroid2D().y, parameter.pullCompensate);
                            auto it = __emb_fill_cache.get()->find(hashkey);
                            if(it!=__emb_fill_cache.get()->end()){
                                epf = (*it).second;
                                //SkAutoTime skautotime("FILL_TYPE_DYNAMIC epf.toPatches2");
                                patches = epf.toPatches2(preset, jumps);
                            }
                            else{
                                epf.setPesPath(shell_and_hole);
                                epf.setOutline(outline);
                                epf.createSections2(density, sewDirection, pullCompensate);
                                //SkAutoTime skautotime("FILL_TYPE_DYNAMIC epf.toPatches2");
                                patches = epf.toPatches2(preset, jumps);
                            }
                            __emb_fill_cache.get()->emplace(hashkey, epf);
#else
                            epf.setPesPath(shell_and_hole);
                            epf.setOutline(outline);
                            epf.createSections2(density, sewDirection, pullCompensate);
                            // SkAutoTime skautotime("FILL_TYPE_DYNAMIC epf.toPatches2");
                            patches = epf.toPatches2(preset, jumps);
#endif

                            int blockIndex = pindex;

                            // Fill all patch
                            pesColor col = p.getFillColor();
                            if( col.a != 0 && patches.size()){
                                vector<pesPolyline> u_patches;
                                vector<bool> u_jumps;
                                bool insertUnderlay = parameter.fill.underlay;  // true;
                                if (insertUnderlay) {
                                    float area = outline.getArea();
                                    if (fabs(area) < 250) {
                                        insertUnderlay = false;
                                    }
                                }

                                if (insertUnderlay) {
                                    float u_compensate = 0;
                                    pesEMBFill u_epf;
                                    u_epf.setPesPath(shell_and_hole);
                                    u_epf.setOutline(outline);
                                    u_epf.createSections(parameter.underlayFill.density,
                                                         sewDirection + 90,
                                                         u_compensate);
                                    u_patches = u_epf.toPatches(preset, u_jumps);
                                }
                                
                                // Experiment
                                //#define EXPERIMENT_CONNECT true
                                //bool EXPERIMENT_CONNECT = parameter.type != OBJECT_TYPE_SCALABLE_SVG_FILE;
                                bool EXPERIMENT_CONNECT = true;
                                if( EXPERIMENT_CONNECT ) {
                                    if(prevFillBlocksIndex!=-1){
                                        pesVec2f p1 = prevStitch;
                                        pesPolyline & outline1 = prevOutline;
                                        int fillBlockIndex1 = prevFillBlocksIndex;
                                        pesVec2f p2 = patches[0][0];
                                        if(insertUnderlay && u_patches.size()){
                                            p2 = u_patches[0][0];
                                        }
                                        pesPolyline outline2 = outline;
                                        outline1.flagHasChanged();
                                        outline2.flagHasChanged();
                                        int fillBlockIndex2 = blockIndex;
                                        connectPointsBetweenOutlines(p1, outline1, fillBlockIndex1, p2, outline2, fillBlockIndex2);
                                    }
                                }
                                // End Experiment
                                
                                // UNDERLAY Experiment
//                              bool insertUnderlay = true;
                                if(insertUnderlay){
//                                    vector<pesPolyline> u_patches;
//                                    vector<bool> u_jumps;
//                                    pesEMBFill u_epf;
//                                    u_epf.setPesPath(shell_and_hole);
//                                    u_epf.createSections(parameter.underlayFill.density, sewDirection+90, compensate);
//                                    u_patches = u_epf.toPatches(preset, jumps);
                                    
                                    if(u_patches.size()){
                                        // from underlay connect to fill
                                        pesVec2f p1 = u_patches.back().getVertices().back();
                                        pesPolyline outline1 = outline;
                                        int fillBlockIndex1 = blockIndex;
                                        pesVec2f p2 = patches[0][0];
                                        pesPolyline outline2 = outline;
                                        outline1.flagHasChanged();
                                        outline2.flagHasChanged();
                                        int fillBlockIndex2 = blockIndex;
                                        pesVec2f u_p1 = outline1.getClosestPoint(p1);
                                        pesVec2f u_p2 = outline1.getClosestPoint(p2);
                                        connectPointsOnOutline(p1, u_p1, p2, outline1, blockIndex);
                                        // end connect
                                    }
                                    
                                    int u_size = (int)u_patches.size();
                                    int u_jumpIndex = 0;
                                    for(int i=0; i<u_size;i++){
                                        int ss = (int)u_patches[i].size();
                                        for(int j=0; j<ss; j++){
                                            pesVec2f s(u_patches[i][j]);
                                            int type = NORMAL_STITCH;
                                            
                                            if(u_jumps.size()>u_jumpIndex && u_jumps[u_jumpIndex++]){
                                                type = JUMP_STITCH;
                                            }
                                            fillBlocks[blockIndex].addStitch(s, type);
                                        }
                                        //patch.addVertices(patches[i].getVertices());
                                        u_patches[i].clear();
                                    }
                                }
                                // END UNDERLAY Experiment
                                
                                //size_t pathchsize = 0;
                                //for (const auto& pathch : patches) {
                                //    pathchsize += pathch.size();
                                //}
                                //SkDebugf("pathchsize: %lu, jumps.size: %lu\n",
                                //         pathchsize,
                                //         jumps.size());

                                int size = (int)patches.size();
                                int jumpIndex = 0;
                                for(int i=0; i<size;i++){
                                    int ss = (int)patches[i].size();
                                    for(int j=0; j<ss; j++){
                                        pesVec2f s(patches[i][j]);
                                        int type = NORMAL_STITCH;
                                        
                                        if(jumps.size()>jumpIndex && jumps[jumpIndex++]){
                                            type = JUMP_STITCH;
                                        }
                                        fillBlocks[blockIndex].addStitch(s, type);
                                    }
                                    //patch.addVertices(patches[i].getVertices());
                                    patches[i].clear();
                                }
                                
                                //if(patch.getVertices().size())
                                //    lastStitch = patch.getVertices().back();
                                
//                                // Insert Underlay
//                                bool insertUnderlay = parameter.fill.underlay;
//                                int underlaySewDirection = sewDirection + 90;
//               
//                                if(insertUnderlay){
//                                    pesData data = *this;
//                                    data.paths = sorted_ppaths;
//                                    data.parameter.fillType = FILL_TYPE_NORMAL;
//                                    data.parameter.patternFill.underlay = false;
//                                    data.parameter.fill.underlay = false;
//                                    data.parameter.fill.sewDirection = underlaySewDirection;
//                                    data.parameter.fill.density = data.parameter.underlayFill.density;
//                                    data.parameter.underlayFill.underlay = true;
//                                    data.applyFill();
//                                    auto & underlay = data.fillBlocks[blockIndex].polyline.getVertices();
//                                    if(underlay.size()){
////                                        auto & outlines = data.parameter.underlayFill.outlines;
////                                        pesVec2f p1 = underlay.back();
////                                        pesPolyline outline1 = outlines[0];
////                                        pesVec2f p2 = patches[0][0];
////                                        pesPolyline outline2 = shell_and_hole.getOutline()[0];
////                                        connectPointsBetweenOutlines(p1, outline1, p2, outline2, data.fillBlocks[blockIndex]);
////                                        underlay = data.fillBlocks[blockIndex].polyline.getVertices();
//                                        auto & vertices = fillBlocks[blockIndex].polyline.getVertices();
//                                        vertices.insert(vertices.begin(), underlay.begin(), underlay.end());
//                                        fillBlocks[blockIndex].types[0] = JUMP_STITCH;
//                                    }
//                                }

                                if( EXPERIMENT_CONNECT ) {                                
                                    // Experiment                                
                                    prevOutline = outline;
                                    prevStitch = fillBlocks[blockIndex].polyline.getVertices().back();
                                    prevFillBlocksIndex = blockIndex;
                                    // End Experiment
                                }
                            }
                        }
                    }
                }
                pindex++;
            } // end for()
        } 
        // -------------------------------------------------------
        /*       
        else if(parameter.fillType==FillType::FILL_TYPE_NORMAL || parameter.fillType==FillType::FILL_TYPE_PATTERN){
            pesPath pattPath;
            if(parameter.fillType==FillType::FILL_TYPE_PATTERN){
//                std::string filepath = guiVAArgsToString("Pattern/%s.pas", parameter.patternFill.patternName.c_str());
//                LoadPattern(filepath);

                // TODO: Need load patternBuffer outside pesEngine
                pattPath = pesLoadPASFromBuffer(parameter.patternFill.patternBuffer);
                if(pattPath.getCommands().size()<2){
                    pattPath = CreateDefaultPattern();
                }
                const float originalSize = 1024;
                float realSize = parameter.patternFill.patternSize * 10;
                float scale = realSize/originalSize;
                pattPath.scale(scale, scale);
            }

            int pindex = 0;
            for(auto p: paths){
//                fillBlocks[pindex].clear();
                vector<pesPath> subpath = p.getSubPath();
                if(parameter.underlayFill.underlay){
                    for(auto & path : subpath){
                        const float pes_one_mm = 10.0;
                        float compensate = fabs(parameter.underlayFill.shrinkSize * pes_one_mm) * -1; // alway negative
                        auto offsets = pesClipper::getOffsets(path.getSubPath(), compensate, ClipperLib::jtSquare, ClipperLib::etClosedPolygon);
                        path.clear();
                        for (auto& offset : offsets) {
                            offset.simplify();
                            auto& vertices = offset.getVertices();
                            path.moveTo(vertices[0].x, vertices[0].y);
                            for (int i = 1; i < vertices.size(); i++) {
                                path.lineTo(vertices[i].x, vertices[i].y);
                            }
                            path.close();
                        }
                    }
                }
                
                
                
                if(subpath.size()){
                    bool keepOrder = (parameter.type == pesData::OBJECT_TYPE_SCALABLE_TTF_TEXT);
                    if(keepOrder==false && PES_SORTING_BY_AREA==true){
                        pesSort(subpath, [](const pesPath & p0, const pesPath & p1)->bool{
                            if(fabs(p0.getOutline()[0].getArea())>fabs(p1.getOutline()[0].getArea()))
                                return true;
                            return false;
                        });
                    }

                    int first_outer_dir = subpath[0].getOutline()[0].getArea()>=0 ? __cw : __ccw;
                    for(int shell=0; shell<(int)subpath.size(); shell++){
                        pesEMBFill epf;
                        std::string hashkey;

                        int shell_dir = subpath[shell].getOutline()[0].getArea()>=0 ? __cw : __ccw;
                        if(shell_dir==first_outer_dir){
                            pesPath shell_and_hole = subpath[shell];
                            for(int hole=shell+1; hole<(int)subpath.size(); hole++){
                                int hole_dir = subpath[hole].getOutline()[0].getArea()>=0 ? __cw : __ccw;
                                if(hole_dir!=first_outer_dir){
                                    shell_and_hole.append(subpath[hole]);
                                }
                            }

                            shell_and_hole.simplify(0.3f);
                            if(parameter.underlayFill.underlay){
                                if(shell_and_hole.getOutline()[0].getArea()<2500){
                                    continue;
                                }
                            }

                            float density = 2.5f;
                            int sewDirection = 0;
                            if(parameter.fillType==FillType::FILL_TYPE_PATTERN){
                                density = parameter.patternFill.density;
                                sewDirection = parameter.patternFill.sewDirection;
                            }
                            else if(parameter.fillType==FillType::FILL_TYPE_NORMAL){
                                density = parameter.fill.density;
                                sewDirection = parameter.fill.sewDirection;
                            }

                            // SkDebugf("density=%.2f dir=%.2f\n", density, sewDirection);

                            hashkey = pesVAArgsToString("t=%d-d=%.2f-s=%d-n=%d-p=%.2f-a=%.2f-v=%d-cx=%.2f-cy=%.2f-fade=%d-pull=%.2f", parameter.fillType, density, sewDirection, shell_and_hole.getOutline().size(), shell_and_hole.getOutline()[0].getPerimeter(), shell_and_hole.getOutline()[0].getArea(), shell_and_hole.getOutline()[0].getVertices().size(), shell_and_hole.getOutline()[0].getCentroid2D().x, shell_and_hole.getOutline()[0].getCentroid2D().y,
                                parameter.fillFading,
                                parameter.pullCompensate);


                            vector<pesPolyline> patches;
                            vector<bool> jumps;
                            auto it = __emb_fill_cache.get()->find(hashkey);
                            if(it!=__emb_fill_cache.get()->end()){
                                epf = (*it).second;
                                if(parameter.fillType==FillType::FILL_TYPE_NORMAL)
                                {
                                    patches = epf.toPatches(preset, jumps, parameter.fillFading);
                                }
                                
                                else if(parameter.fillType==FillType::FILL_TYPE_PATTERN){
                                    epf.createTileGrid(parameter.patternFill.patternSize);
                                    patches = epf.toPatches(pattPath, jumps);
                                }
                                
                            }
                            else{
                                epf.setPesPath(shell_and_hole);

                                float compensate = parameter.pullCompensate;
                                if(parameter.fillType==FillType::FILL_TYPE_NORMAL)
                                {
                                    epf.createSections(parameter.fill.density, (float)parameter.fill.sewDirection, compensate, parameter.fillFading);
                                    patches = epf.toPatches(preset, jumps, parameter.fillFading);
                                }
                                
                                else if(parameter.fillType==FillType::FILL_TYPE_PATTERN){
                                    epf.createTileGrid(parameter.patternFill.patternSize);
                                    epf.createSections(parameter.patternFill.density, (float)parameter.patternFill.sewDirection, compensate, parameter.fillFading);
                                    patches = epf.toPatches(pattPath, jumps);
                                }
                                
                            }
                            __emb_fill_cache.get()->emplace(hashkey, epf);

                            int blockIndex = pindex;

                            pesColor col = p.getFillColor();
                            if( col.a != 0 && patches.size()){
                                // Experiment
                            
                                if(prevFillBlocksIndex!=-1){
                                    pesVec2f p1 = prevStitch;
                                    pesPolyline & outline1 = prevOutline;
                                    int fillBlockIndex1 = prevFillBlocksIndex;
                                    pesVec2f p2 = patches[0][0];
                                    pesPolyline outline2 = shell_and_hole.getOutline()[0];
                                    int fillBlockIndex2 = blockIndex;
                                    connectPointsBetweenOutlines(p1, outline1, fillBlockIndex1, p2, outline2, fillBlockIndex2);
                                }
                                // End Experiment
                                
                                int size = (int)patches.size();
                                int jumpIndex = 0;
                                for(int i=0; i<size;i++){
                                    int ss = (int)patches[i].size();
                                    for(int j=0; j<ss; j++){
                                        pesVec2f s(patches[i][j]);
                                        int type = NORMAL_STITCH;
                                        if(jumps.size()>jumpIndex && jumps[jumpIndex++]){
                                            type = JUMP_STITCH;
                                        }
                                        fillBlocks[blockIndex].addStitch(s, type);
                                    }
                                    //patch.addVertices(patches[i].getVertices());
                                    patches[i].clear();
                                }

                                //if(patch.getVertices().size())
                                //    lastStitch = patch.getVertices().back();
                                
                                // Insert Underlay
                                bool insertUnderlay = false;
                                int underlaySewDirection = 90;
                                if(parameter.fillType==FILL_TYPE_NORMAL)
                                {
                                    insertUnderlay = parameter.fill.underlay;
                                    underlaySewDirection = parameter.fill.sewDirection + 90;
                                }
                                else if(parameter.fillType==FILL_TYPE_PATTERN){
                                    insertUnderlay = parameter.patternFill.underlay;
                                    underlaySewDirection = parameter.patternFill.sewDirection + 90;
                                }
                                
                                if(insertUnderlay){
                                    pesData data = *this;
                                    data.parameter.fillType = FILL_TYPE_NORMAL;
                                    data.parameter.patternFill.underlay = false;
                                    data.parameter.fill.underlay = false;
                                    data.parameter.fill.sewDirection = underlaySewDirection;
                                    data.parameter.fill.density = data.parameter.underlayFill.density;
                                    data.parameter.underlayFill.underlay = true;
                                    data.applyFill();
//                                    fillBlocks[blockIndex].polyline.insert(fillBlocks[blockIndex].polyline.begin(), data.fillBlocks[blockIndex].polyline);
                                    auto & vertices = fillBlocks[blockIndex].polyline.getVertices();
                                    auto & underlay = data.fillBlocks[blockIndex].polyline.getVertices();
                                    vertices.insert(vertices.begin(), underlay.begin(), underlay.end());
                                }
                                
                                
                                // Experiment
                                prevOutline = shell_and_hole.getOutline()[0];
                                prevStitch = fillBlocks[blockIndex].polyline.getVertices().back();
                                prevFillBlocksIndex = blockIndex;
                                // End Experiment
                                
                                if(parameter.underlayFill.underlay){
                                    parameter.underlayFill.outlines.push_back(shell_and_hole.getOutline()[0]);
                                }
                            }
                        }
                    }
                }
                pindex++;
            } // end for()
        } 
        */

        else if (parameter.fillType == FillType::FILL_TYPE_NORMAL || parameter.fillType == FillType::FILL_TYPE_PATTERN) {
            pesPath pattPath;
            if(parameter.fillType==FillType::FILL_TYPE_PATTERN){
                // TODO: Need load patternBuffer outside pesEngine
                pattPath = pesLoadPASFromBuffer(parameter.patternFill.patternBuffer);
                if(pattPath.getCommands().size()<2){
                    pattPath = CreateDefaultPattern();
                }
                const float originalSize = 1024;
                float realSize = parameter.patternFill.patternSize * 10;
                float scale = realSize/originalSize;
                pattPath.scale(scale, scale);
            }

            int pindex = 0;
            for (auto& p : paths) {
                if (!p.bVisible) {
                    continue;
                }
                if (!p.isFill()) {
                    continue;
                }

                vector<pesPath> subpath = p.getSubPath();

                if (subpath.size()) {
                    const auto bound = subpath[0].getBoundingBox();
                    const auto vLeftTop = pesVec2f(bound.getMinX(), bound.getMinY());
                    const auto cxy = bound.getCenter();

                    std::vector<ShapeInfo> shapeinfos;

                    bool bGroups = true;
                    if (bGroups) {
                        //bool isColorReduction = parameter.text == "Color-Reduction";
                        ShapeInfo::makeGroups(bound, p, subpath, shapeinfos);
                    }
                    else {
                        //shapeinfos.emplace_back(p).id = 0;
                        ShapeInfo::makeSingle(bound, p, subpath, shapeinfos);
                    }

                    if (!shapeinfos.size()) {
                        //SkDebugf("pindex: %d, shapeinfos.size: %lu\n", pindex, shapeinfos.size());
                        pindex++;
                        continue;
                    }

                    ShapeInfo::sortPositionsAroundPointInCwOrder(shapeinfos, cxy);

                    ShapeInfo* shape = nullptr;
                    pesVec2f from = vLeftTop;
                    while ((shape = ShapeInfo::nextshape(shapeinfos, from, shape)) != nullptr) {
                        from = shape->cxy;
                        auto& shell_and_hole = shape->path;
                        //auto& outline = shape->outline;
                            
                        pesEMBFill epf;
                        //shell_and_hole.simplify(0.3f);

                        float density = 2.5f;
                        float sewDirection = 0;
                        float pullCompensate = parameter.pullCompensate;
                        bool fillFading = parameter.fillFading;
                        if (parameter.fillType == FillType::FILL_TYPE_NORMAL) {
                            density = parameter.fill.density;
                            sewDirection = parameter.fill.sewDirection;
                        }
                        else if (parameter.fillType == FillType::FILL_TYPE_PATTERN) {
                            density = parameter.patternFill.density;
                            sewDirection = parameter.patternFill.sewDirection;
                        }

                        //// experiment dynamic tolerance
                        //const float density2x = density * 2.0f;
                        //const auto mintolerance = 1.0f / density2x;
                        ////shell_and_hole.simplify(mintolerance);
                        //auto outline = shape->getOutline(mintolerance);
                        auto outline = shape->getOutline(0.2f);

                        vector<pesPolyline> patches;
                        vector<bool> jumps;
                        epf.setPesPath(shell_and_hole);
                        epf.setOutline(outline);

                        if (parameter.fillType == FillType::FILL_TYPE_NORMAL) {
                            epf.createSections2(density, sewDirection, pullCompensate, fillFading);
                            //SkAutoTime skautotime("FILL_TYPE_NORMAL epf.toPatches2");
                            patches = epf.toPatches2(preset, jumps, fillFading);
                        }
                        else if (parameter.fillType == FillType::FILL_TYPE_PATTERN) {
                            epf.createTileGrid(parameter.patternFill.patternSize);
                            epf.createSections2(density, sewDirection, pullCompensate, fillFading);
                            //SkAutoTime skautotime("FILL_TYPE_PATTERN epf.toPatches2");
                            patches = epf.toPatches2(pattPath, jumps, fillFading);
                        }
                                
                        int blockIndex = pindex;

                        pesColor col = p.getFillColor();
                        if (col.a != 0 && patches.size()) {
                            //bool EXPERIMENT_CONNECT = parameter.type != OBJECT_TYPE_SCALABLE_SVG_FILE;
                            bool EXPERIMENT_CONNECT = true;
                            if (EXPERIMENT_CONNECT) {
                                // Experiment
                                if (prevFillBlocksIndex != -1) {
                                    pesVec2f p1 = prevStitch;
                                    pesPolyline& outline1 = prevOutline;
                                    int fillBlockIndex1 = prevFillBlocksIndex;
                                    pesVec2f p2 = patches[0][0];
                                    pesPolyline outline2 = outline;  // shell_and_hole.getOutline()[0];
                                    int fillBlockIndex2 = blockIndex;
                                    connectPointsBetweenOutlines(p1, outline1, fillBlockIndex1, p2, outline2, fillBlockIndex2);
                                }
                                // End Experiment
                            }
                                
                            int jumpIndex = 0;
                            for (int i = 0, ii = (int)patches.size(); i < ii; i++) {
                                for (int j = 0, jj = (int)patches[i].size(); j < jj; j++) {
                                    pesVec2f &patche = patches[i][j];
                                    int type = NORMAL_STITCH;
                                    if (jumps.size() > jumpIndex && jumps[jumpIndex++]) {
                                        type = JUMP_STITCH;
                                    }
                                    fillBlocks[blockIndex].addStitch(patche, type);
                                }
                                patches[i].clear();
                            }
                                            
                            if (EXPERIMENT_CONNECT) {
                                // Experiment
                                prevOutline = outline;  // shell_and_hole.getOutline()[0];
                                prevStitch = fillBlocks[blockIndex].polyline.getVertices().back();
                                prevFillBlocksIndex = blockIndex;
                                // End Experiment
                            }
                        }
                    }
                }
                pindex++;
            } // end for()
        }
        else if (parameter.fillType == FillType::FILL_TYPE_NORMAL2) {
            int totalFillJumps2 = 0;
            int totalFillStitches2 = 0;

            int pindex = 0;
            for (auto& p : paths) {
                if (!p.bVisible) { continue; }
                if (!p.isFill()) { continue; }

                vector<pesPath> subpath = p.getSubPath();
                if (subpath.size()) {
                    const auto bound = subpath[0].getBoundingBox();
                    const auto vLeftTop = pesVec2f(bound.getMinX(), bound.getMinY());
                    const auto cxy = bound.getCenter();

                    std::vector<ShapeInfo> shapeinfos;
                    ShapeInfo::makeGroups(bound, p, subpath, shapeinfos);

                    if (!shapeinfos.size()) {
                        pindex++;
                        continue;
                    }

                    ShapeInfo::sortPositionsAroundPointInCwOrder(shapeinfos, cxy);

                    ShapeInfo* shape = nullptr;
                    pesVec2f from = vLeftTop;
                    while ((shape = ShapeInfo::nextshape(shapeinfos, from, shape)) != nullptr) {
                        from = shape->cxy;
                        auto& shell_and_hole = shape->path;

                        pesEMBFill epf;

                        float density = parameter.fill.density;
                        float sewDirection = parameter.fill.sewDirection;
                        float pullCompensate = parameter.pullCompensate;
                        bool fillFading = parameter.fillFading;

                        auto outline = shape->getOutline(0.2f);

                        epf.setPesPath(shell_and_hole);
                        epf.setOutline(outline);
                        epf.createSections3(density, sewDirection, pullCompensate, fillFading);

                        pesStitchBlock sblock = epf.toStitchBlock3(preset, fillFading);

                        int blockIndex = pindex;
                        pesColor col = p.getFillColor();
                        if (col.a != 0 && sblock.size() > 0) {
                            // Between shapes: JUMP (no connectPointsBetweenOutlines)
                            // Each shape's fill starts with its own JUMP from toStitchBlock3

                            // Copy stitches from sblock to fillBlocks
                            for (size_t i = 0; i < sblock.size(); i++) {
                                pesVec2f stitch = sblock.polyline.getVertices()[i];
                                uint8_t type = sblock.types[i];
                                // Force JUMP at start of each shape (except first which already has JUMP)
                                if (i == 0 && prevFillBlocksIndex != -1) {
                                    fillBlocks[blockIndex].addJump(stitch);
                                } else {
                                    fillBlocks[blockIndex].addStitch(stitch, type);
                                }
                            }

                            prevOutline = outline;
                            prevStitch = fillBlocks[blockIndex].polyline.getVertices().back();
                            prevFillBlocksIndex = blockIndex;
                        }
                    }
                }
                pindex++;
            }

            // Count jumps and stitches for NORMAL2
            for (auto& fb : fillBlocks) {
                totalFillJumps2 += fb.getTotalJumps();
                totalFillStitches2 += fb.getTotalStitches();
            }

            // Auto-compare: also run NORMAL algorithm and print comparison
            {
                int totalFillJumps1 = 0;
                int totalFillStitches1 = 0;
                int pidx = 0;
                for (auto& p : paths) {
                    if (!p.bVisible || !p.isFill()) { pidx++; continue; }
                    vector<pesPath> subpath = p.getSubPath();
                    if (subpath.size()) {
                        const auto bnd = subpath[0].getBoundingBox();
                        const auto vlt = pesVec2f(bnd.getMinX(), bnd.getMinY());
                        const auto cxy2 = bnd.getCenter();
                        std::vector<ShapeInfo> sinfos;
                        ShapeInfo::makeGroups(bnd, p, subpath, sinfos);
                        if (!sinfos.size()) { pidx++; continue; }
                        ShapeInfo::sortPositionsAroundPointInCwOrder(sinfos, cxy2);
                        ShapeInfo* sh = nullptr;
                        pesVec2f frm = vlt;
                        while ((sh = ShapeInfo::nextshape(sinfos, frm, sh)) != nullptr) {
                            frm = sh->cxy;
                            pesEMBFill epf;
                            float dens = parameter.fill.density;
                            float sdir = parameter.fill.sewDirection;
                            auto outl = sh->getOutline(0.2f);
                            epf.setPesPath(sh->path);
                            epf.setOutline(outl);
                            epf.createSections2(dens, sdir, parameter.pullCompensate, parameter.fillFading);
                            vector<bool> jmps;
                            vector<pesPolyline> ptchs = epf.toPatches2(preset, jmps, parameter.fillFading);
                            int jc = 0;
                            for (bool j : jmps) { if (j) jc++; }
                            totalFillJumps1 += jc;
                            totalFillStitches1 += (int)jmps.size() - jc;
                        }
                    }
                    pidx++;
                }
                SkDebugf("[Fill Compare] NORMAL:  jumps=%d, stitches=%d\n", totalFillJumps1, totalFillStitches1);
                SkDebugf("[Fill Compare] NORMAL2: jumps=%d, stitches=%d\n", totalFillJumps2, totalFillStitches2);
            }
        }
        else if(parameter.fillType==FillType::FILL_TYPE_MOTIF){
            float pes_one_mm = 10;
            float size = parameter.motifFill.size * pes_one_mm;
            float space = parameter.motifFill.spacing * pes_one_mm;
            //float rowspace = parameter.motifFill.rowSpacing * pes_one_mm;
            float oneMotif = size+space;
            //float rowSpacing = size+rowspace;
            float pitch = 0.2f * pes_one_mm;

            // TODO: Need load patternBuffer outside pesEngine
            pesPath pattPath = pesLoadPMFFromBuffer(parameter.motifFill.patternBuffer);
            if(pattPath.getCommands().size()<2){
                pattPath = CreateDefaultMotif();
            }
            const float originalSize = 1024;
            float realSize = size;
            float scale = realSize/originalSize;
            pattPath.scale(scale, scale);
            pesVec2f right(1,0);

            int pindex = 0;
            for(auto p: paths){
                vector<pesPath> subpath = p.getSubPath();
                if(subpath.size()){
                    bool keepOrder = (parameter.type == pesData::OBJECT_TYPE_SCALABLE_TTF_TEXT);
                    if(keepOrder==false && PES_SORTING_BY_AREA==true){
                        pesSort(subpath, [](const pesPath & p0, const pesPath & p1)->bool{
                            if(fabs(p0.getOutline()[0].getArea())>fabs(p1.getOutline()[0].getArea()))
                                return true;
                            return false;
                        });
                    }

                    int first_outer_dir = subpath[0].getOutline()[0].getArea()>=0 ? __cw : __ccw;
                    for(int shell=0; shell<(int)subpath.size(); shell++){
                        int shell_dir = subpath[shell].getOutline()[0].getArea()>=0 ? __cw : __ccw;
                        if(shell_dir==first_outer_dir){
                            pesPath shell_and_hole = subpath[shell];
                            for(int hole=shell+1; hole<(int)subpath.size(); hole++){
                                int hole_dir = subpath[hole].getOutline()[0].getArea()>=0 ? __cw : __ccw;
                                if(hole_dir!=first_outer_dir){
                                    shell_and_hole.append(subpath[hole]);
                                }
                            }

                            shell_and_hole.simplify(0.3f);
                            pesEMBFill epf(shell_and_hole);
                            vector<pesPolyline> patches;


                            deque<vector<pesEMBFill::Line>> sections = epf.createMFSections(parameter.motifFill.size+parameter.motifFill.rowSpacing, 0);

                            pesVec2f _lastStitch(0,0);
                            //float max_stitch_length = 1 * 10.0;
                            int blockIndex = pindex;
                            while(sections.size()){
                                if(_lastStitch.length()>0){
                                    pesVec2f start_corner;
                                    int section_index = epf.find_nearest_section(sections, _lastStitch, start_corner);
                                    vector<bool> jumps;
                                    pesPolyline patch = epf.connect_points(_lastStitch, start_corner, jumps);
                                    for(auto s:patch.getVertices()){
                                        fillBlocks[blockIndex].addStitch(s);
                                    }

                                    vector<pesEMBFill::Line> section = sections.at(section_index);
                                    if(start_corner!=section[0].start && start_corner!=section[0].end){
                                        std::reverse(section.begin(), section.end());
                                    }

                                    if(start_corner!=section[0].start){
                                        for(int i=0; i<(int)section.size(); i++){
                                            std::swap(section[i].start, section[i].end);
                                        }
                                    }

                                    //bool first_segment = true;
                                    bool swap = false;
                                    pesVec2f last_end(0,0);
                                    for(auto segment : section){
                                        pesVec2f beg = segment.start;
                                        pesVec2f end = segment.end;
                                        if(swap){
                                            std::swap(beg, end);
                                        }
                                        swap=!swap;

                                        pesPolyline p;
                                        p.addVertex(beg);
                                        p.addVertex(end);
                                        float l = p.getPerimeter();
                                        int numMotif = (int)(l/oneMotif);
                                        pesPolyline resample = p.getResampledByCount2(numMotif);

                                        pesVec2f p0 = resample[0];
                                        int loopCount = resample.isClosed() ? (int)resample.size()+1 : (int)resample.size();
                                        for(int i=1; i<loopCount; i++){
                                            int wrapedIndex = resample.getWrappedIndex(i);
                                            pesVec2f p1 = resample[wrapedIndex];
                                            float seg_len = p1.distance(p0);
                                            if (seg_len == 0.0)
                                                continue;

                                            pesVec2f along = (p1-p0).normalize();
                                            float angle = right.angle(along);
                                            pesPath motif = pattPath;
                                            motif.rotate(angle);

                                            pesVec2f ms = motif.getOutline().front().getVertices().front();
                                            pesVec2f d = p0 - ms;
                                            bool __motif_align_vcenter = true;
                                            if(__motif_align_vcenter){
                                                pesVec2f start(0, size/2);
                                                start.rotate(angle);
                                                d = p0 - start;
                                            }
                                            motif.translate(d.x, d.y);

                                            auto vertices = motif.getOutline().front().getVertices();
                                            for(int j=0; j<(int)vertices.size(); j++){
//                                                Stitch s((int)round(vertices[j].x), (int)round(vertices[j].y));
                                                pesVec2f s(vertices[j]);
                                                int type = NORMAL_STITCH;
                                                if(j==0 && i==1){
                                                    type = JUMP_STITCH;
                                                }
                                                fillBlocks[blockIndex].addStitch(s, type);
                                            }
                                            pesVec2f last = vertices.back();
                                            float dist = last.distance(p1);
                                            if(dist>pitch){
                                                along = (p1-last).normalize();
                                                float run_len = pitch;
                                                while (run_len<dist) {
                                                    //pesVec2f stitch = p0 + along * run_len;
                                                    //pesVec2f s(stitch);
//                                                    fillBlocks[blockIndex].addStitch(s);
                                                    run_len+=pitch;
                                                }
                                            }
                                            p0 = p1;
                                        } // end for(int i=1; i<loopCount; ...)

                                        pesVec2f last(fillBlocks[blockIndex].polyline.getVertices().back());
                                        if(p0.distance(last)>0.1* pes_one_mm){
                                            pesVec2f s(p0);
                                            fillBlocks[blockIndex].addStitch(s);
                                        }
                                    } // end for(auto segment : section)

                                    sections.erase(sections.begin()+section_index);
                                }
                                else{
                                    vector<pesEMBFill::Line> section = sections.front();
                                    //bool first_segment = true;
                                    bool swap = false;
                                    pesVec2f last_end(0,0);
                                    for(auto segment : section){
                                        pesVec2f beg = segment.start;
                                        pesVec2f end = segment.end;
                                        if(swap){
                                            std::swap(beg, end);
                                        }
                                        swap=!swap;

                                        pesPolyline p;
                                        p.addVertex(beg);
                                        p.addVertex(end);
                                        float l = p.getPerimeter();
                                        int numMotif = (int)(l/oneMotif);
                                        pesPolyline resample = p.getResampledByCount2(numMotif);

                                        pesVec2f p0 = resample[0];
                                        int loopCount = resample.isClosed() ? (int)resample.size()+1 : (int)resample.size();
                                        for(int i=1; i<loopCount; i++){
                                            int wrapedIndex = resample.getWrappedIndex(i);
                                            pesVec2f p1 = resample[wrapedIndex];
                                            float seg_len = p1.distance(p0);
                                            if (seg_len == 0.0)
                                                continue;

                                            pesVec2f along = (p1-p0).normalize();
                                            float angle = right.angle(along);
                                            pesPath motif = pattPath;
                                            motif.rotate(angle);

                                            pesVec2f ms = motif.getOutline().front().getVertices().front();
                                            pesVec2f d = p0 - ms;
                                            bool __motif_align_vcenter = true;
                                            if(__motif_align_vcenter){
                                                pesVec2f start(0, size/2);
                                                start.rotate(angle);
                                                d = p0 - start;
                                            }
                                            motif.translate(d.x, d.y);

                                            auto vertices = motif.getOutline().front().getVertices();
                                            for(int j=0; j<(int)vertices.size(); j++){
                                                pesVec2f s(vertices[j]);
                                                int type = NORMAL_STITCH;
                                                if(j==0 && i==1){
                                                   type = JUMP_STITCH;
                                                }
                                                fillBlocks[blockIndex].addStitch(s, type);
                                            }
                                            pesVec2f last = vertices.back();
                                            float dist = last.distance(p1);
                                            if(dist>pitch){
                                                along = (p1-last).normalize();
                                                float run_len = pitch;
                                                while (run_len<dist) {
                                                    //pesVec2f stitch = p0 + along * run_len;
                                                    //pesVec2f s(stitch);
//                                                    fillBlocks[blockIndex].addStitch(s);
                                                    run_len+=pitch;
                                                }
                                            }

                                            p0 = p1;
                                        } // end for(int i=1; i<loopCount; i++)

                                        pesVec2f last( fillBlocks[blockIndex].polyline.getVertices().back() );
                                        if(p0.distance(last)>0.1* pes_one_mm){
                                            pesVec2f s(p0);
                                            fillBlocks[blockIndex].addStitch(s);
                                        }

                                    } // end for(auto segment : section)

                                    sections.pop_front();
                                }// end if(_lastStitch.length()>0)...else

                                pesVec2f last(fillBlocks[blockIndex].polyline.getVertices().back());
                                _lastStitch = last;
                            } // end while(sections.size())

                        } // end if(shell_dir==first_outer_dir)

                    } // end for(int shell=0; ...)

                } // end if(subpath.size())

                pindex++;
            } // end for(auto p: paths)

        }// end if(motif fill)
        else if(parameter.fillType==FillType::FILL_TYPE_SATIN_COLUMN){
            int numBlock = (int)paths.size();
            fillBlocks.resize(numBlock);

            for (size_t i = 0; i < paths.size(); i++) {
                fillBlocks[blockIndex].clear();
                fillBlocks[i].setNearestColor(paths[i].getFillColor());
            }

//            if(fillBlocks.size()) fillBlocks.clear();
//            fillBlocks.resize(1);

            int ppmm = 10;
            float satinDensity = parameter.fill.density;
            float zigzagSpacing = 1.0f * ppmm / satinDensity;
            pesSatinColumn::parameter_t param;
            param.satinUnderlay = false;
            param.satinZigzagUnderlay = false;
            param.pixels_per_mm = ppmm;
            param.zigzag_spacing_mm = zigzagSpacing;
            param.pull_compensation_mm = parameter.pullCompensate;

            int colorIndex = parameter.fillColorIndex + 0;
            fillBlocks[0].setColorFromIndex(colorIndex);

            std::vector<pesPolyline> patches;
            std::vector<pesPolyline> underlayPatches;

            // Enable/Disable here
            // bool bEnableMakeSatinNoneOverlap = satinDensity > 2.0; // Noom
            bool bEnableMakeSatinNoneOverlap = parameter.autoSatinFill.noneOverlap;  // pom

            std::vector<std::shared_ptr<SatinColumnInfo>> scInfos;
            std::vector<std::shared_ptr<SatinColumnInfoGroup>> scInfogroups;
            
            for (int idx = 0, i = 0, j = 1, size = (int)paths.size(); j < size; idx++, i += 2, j += 2) {
                auto info = std::make_shared<SatinColumnInfo>(paths[i], paths[j], idx, param.pull_compensation_mm);
                scInfos.push_back(info);
            }

            PesPath skp;
            std::vector<SkPath> skpgroups;
            for (int i = 0, ii = (int)scInfos.size(); i < ii; i++) {
                skp.addPath(scInfos[i]->skpoutline);
            }
            skp.simplify();
            skp.setFillType(SkPathFillType::kWinding);

            auto pp = toPes(skp);
            std::vector<SkPath> skps;
            auto pps = pp.getSubPath();
            for (int i = 0, ii = pps.size(); i < ii; i++) {
                skps.push_back(toSk(pps[i]));
            }

            for (int i = 0, ii = skps.size(); i < ii; i++) {
                auto& skp = skps[i];
                if (SkPathFirstDirection::kCCW == SkPathPriv::ComputeFirstDirection(skp)) {
                    PesPath skrev;
                    skrev.reverseAddPath(skp);
                    skps[i] = skrev;
                }
            }

            skp = SkPath();
            for (int i = 0, ii = skps.size(); i < ii; i++) {
                skp.addPath(skps[i]);
            }

            skp.simplify();
            skp.setFillType(SkPathFillType::kWinding);

            pp = toPes(skp);
            skps.clear();
            pps = pp.getSubPath();
            for (int i = 0, ii = pps.size(); i < ii; i++) {
                auto skp = toSk(pps[i]);
                if (SkPathFirstDirection::kCCW == SkPathPriv::ComputeFirstDirection(skp)) {
                    PesPath skrev;
                    skrev.reverseAddPath(skp);
                    skp = skrev;
                }
                skpgroups.push_back(skp);
            }

            //SkDebugf("skpgroups: %d\n", skpgroups.size());

            for (int i = 0, ii = skpgroups.size(); i < ii; i++) {
                auto& skpgroup = skpgroups[i];
                auto infogroup = std::make_shared<SatinColumnInfoGroup>(i, skpgroup);
                scInfogroups.push_back(infogroup);

                for (auto& scInfo : scInfos) {
                    if (scInfo->gidx == -1) {
                        if (skpgroup.contains(scInfo->fx, scInfo->fy)) {
                            scInfo->gidx = i;
                            infogroup->scInfos.push_back(scInfo);
                        }
                    }
                }

                if (infogroup->scInfos.size() > 0) {
                    if (infogroup->scInfos.size() > 1) {
                        for (auto& scInfo : infogroup->scInfos) {
                            for (auto& scInfoOther : infogroup->scInfos) {
                                if (scInfo != scInfoOther) {
                                    auto& skother = scInfoOther->skpoutline;
                                    if (skother.contains(scInfo->ff0x, scInfo->ff0y) &&
                                        skother.contains(scInfo->ff1x, scInfo->ff1y)) {
                                        scInfo->priority++;
                                    }
                                    if (skother.contains(scInfo->fl0x, scInfo->fl0y) &&
                                        skother.contains(scInfo->fl1x, scInfo->fl1y)) {
                                        scInfo->priority++;
                                    }
                                }
                            }
                        }

                        pesSort(infogroup->scInfos, [](const auto& info0, const auto& info1) -> bool {
                            if (info0->priority == info1->priority)
                                return fabs(info0->area) < fabs(info1->area);
                            return info0->priority > info1->priority;
                        });
                    }
                }
            }

            // ready doSatin
            for (auto& scInfogroup : scInfogroups) {
                scInfogroup->bgeted = false;
            }
            for (auto& scInfo : scInfos) {
                if (scInfo->gidx != -1) {
                    auto& scInfogroup = scInfogroups[scInfo->gidx];
                    if (!scInfogroup->bgeted) {
                        // make outline (remainingArea) from remaining objects
                        auto& scInfos = scInfogroup->scInfos;
                        for (int i = 0, ii = (int)scInfos.size(); i < ii; i++) {
                            SkPath remainingArea;
                            if (bEnableMakeSatinNoneOverlap) {
                                PesPath ra;
                                for (int j = i + 1; j < ii; j++) {
                                    ra.addPath(scInfos[j]->skpoutline);
                                }
                                remainingArea = ra;
                                //if (i + 1 < ii) {
                                //    Simplify(remainingArea, &remainingArea);
                                //    AsWinding(remainingArea, &remainingArea);
                                //    remainingArea.setFillType(SkPathFillType::kWinding);
                                //}
                            }

                            auto& scInfo = scInfos[i];
                            pesSatinColumn satin(scInfo->p0, scInfo->p1);
                            satin.param = param;
                            satin.doUnderlay(scInfo->underlaypatch);

                            scInfogroup->bskiped |=
                                    satin.doSatin3(scInfo->slicepatches,
                                                    remainingArea,
                                                    scInfo->skpoutline,
                                                    bEnableMakeSatinNoneOverlap);
                        }

                        // now, not required
                        //if (!scInfogroup->bskiped) {
                        //    pesSort(scInfogroup->scInfos, [](const auto& info0, const auto& info1) -> bool {
                        //        return info0->idx < info1->idx;
                        //    });
                        //}

                        scInfogroup->bgeted = true;
                    }
                }
                else {
                    pesSatinColumn satin(scInfo->p0, scInfo->p1);
                    satin.param = param;
                    satin.doUnderlay(scInfo->underlaypatch);
                    auto remainingArea = SkPath();
                    bool bskiped = satin.doSatin3(scInfo->slicepatches,
                                                    remainingArea,
                                                    scInfo->skpoutline,
                                                    bEnableMakeSatinNoneOverlap);
                }
            }

            // get all results
            for (auto& scInfo : scInfos) {
                underlayPatches.push_back(scInfo->underlaypatch);

                pesPolyline patch;
                for (auto& slice : scInfo->slicepatches) {
                    if (slice.size() > 0) {
                        for (auto& point : slice) {
                            patch.addVertex(point);
                        }
                    }
                }
                if (patch.size() > 0) {
                    patches.push_back(patch);
                }
            }

            vector<pesNearestBranchingResult> results;
            pesFindNearestEnterAndExit(patches, results, true, false);
            if(results.size()){
                // not used
                //float runpitch = 0.6 * param.pixels_per_mm;
                const float underlayRunpitch = 2.5 * param.pixels_per_mm;
                int blockIndex = 0;
                for(auto rs:results){
                    int enterIndex = rs.enterIndex;
                    int exitIndex = rs.exitIndex;
                    int pIndex = rs.patchesIndex;
                    
                    if(parameter.fill.underlay){
                        // Build underlay center walk
                        pesPolyline centerWalk;
                        for(int ip=0; ip<patches[pIndex].size()-1; ip++){
                            pesVec2f mid = patches[pIndex][ip].getMiddle(patches[pIndex][ip+1]);
                            centerWalk.addVertex(mid);
                        }
                        
                        float perimeter = centerWalk.getPerimeter();
                        
                        // not used
                        //if(perimeter/runpitch<3) {
                        //    runpitch = perimeter / 3;
                        //    #ifndef __EMSCRIPTEN__
                        //    SkDebugf("new runpitch=%.2f\n", runpitch);
                        //    #endif
                        //}
                        
                        int num_points = (int)round(centerWalk.getPerimeter() / underlayRunpitch);

                        // fixed: centerWalk size < 2
                        // fixed: num_points == 0 && centerWalk size == 1, result is always a triple points at x:0, y:0
                        if (centerWalk.size() > 0) {
                            if (num_points < 2) {
                                if (centerWalk.size() > 1) {
                                    pesPolyline poly;
                                    poly.addVertex(centerWalk.getPointAtPercent(0.5f));
                                    centerWalk = poly;
                                }
                            } 
                            else {
                                centerWalk = centerWalk.getResampledByCount2(num_points);
                            }
                        
                            // from satin exit point walk bak to underlay starting point
                            if(fillBlocks[blockIndex].size()){
                                auto last = fillBlocks[blockIndex].polyline.getVertices().back();
                                // safety: assign default value
                                unsigned int exitNearestIndex = 0;
                                centerWalk.getClosestPoint(fillBlocks[blockIndex].polyline.getVertices().back(), &exitNearestIndex );
                                for(int i=exitNearestIndex; i>=0; --i){
                                    if(i==exitNearestIndex){
                                        fillBlocks[blockIndex].addJump(centerWalk[i], false);
                                    }
                                    else{
                                        fillBlocks[blockIndex].addStitch(centerWalk[i]);
                                    }
                                }
                            }
                        
                            // Insert Underlay
                            for(int i=0; i<underlayPatches[pIndex].size(); i++){
                                if(i==0){
                                    fillBlocks[blockIndex].addJump(underlayPatches[pIndex][i], false);
                                }
                                else{
                                    fillBlocks[blockIndex].addStitch(underlayPatches[pIndex][i]);
                                }
                            }
                        
                            // from underlay walk back to satin
                            // safety: assign default value
                            unsigned int nearestIndex = 0, enterNearestIndex = 0;
                            if(fillBlocks[blockIndex].polyline.size()){
                                centerWalk.getClosestPoint(fillBlocks[blockIndex].polyline.getVertices().back(), &nearestIndex );
                            }
                            else{
                                nearestIndex = 0;
                            }
                            centerWalk.getClosestPoint(patches[pIndex][enterIndex], &enterNearestIndex );
                            if(nearestIndex < enterNearestIndex){
                                for(int i=nearestIndex; i<=enterNearestIndex; i++){
                                    fillBlocks[blockIndex].addStitch(centerWalk[i]);
                                }
                            }
                            else if(nearestIndex > enterNearestIndex){
                                for(int i=nearestIndex; i>enterNearestIndex; i--){
                                    fillBlocks[blockIndex].addStitch(centerWalk[i]);
                                }
                            }
                        }
                    }

                    // Satin
                    fillBlocks[blockIndex].addJump(patches[pIndex][enterIndex]);
                    if(enterIndex>0){
                        pesPolyline runstitches;
                        // walk-back to [0](firstIndex)
                        for (int j = enterIndex; j >= 1; j--) {
                            if(patches[pIndex][j].match(patches[pIndex][j-1]))
                                continue;
                            //pesVec2f mid = patches[pIndex][j].getMiddle(patches[pIndex][j-1]);
                            //pesVec2f last = fillBlocks[blockIndex].polyline.getVertices().back();
                            //if(last.distance(mid)>=runpitch)
                            //    fillBlocks[blockIndex].addStitch(mid);
                            if (((j-1) % 4) == 0) runstitches.addVertex(patches[pIndex][j].getMiddle(patches[pIndex][j-1]));
                        }
#ifndef __EMSCRIPTEN__
                        int b = runstitches.size();
                        runstitches.simplify(1);
                        int a = runstitches.size();
                        SkDebugf("\n-->> runstitch: %d - %d ==> %d\n", b, b - a, a);
#else
                        runstitches.simplify(1);
#endif
                        for (const auto& runstitch : runstitches) {
                            fillBlocks[blockIndex].addStitch(runstitch);
                        }
                    }

                    // do satin zigzag for every segments from firstIndex to exitIndex
                    fillBlocks[blockIndex].addStitch(patches[pIndex][0]);
                    int cnt = (int)patches[pIndex].size();
                    for(int j=1; j<=exitIndex; j++){
                        fillBlocks[blockIndex].addStitch( patches[pIndex][j]);
                    }

                    // if exitIndex < lastIndex
                    if(exitIndex<cnt-1){
                        pesPolyline runstitches;
                        vector<pesVec2f> satin;
                        // walk-forward from exitIndex to lastIndex
                        for (int j = exitIndex; j < cnt - 1; j++) {
                            if(patches[pIndex][j].match(patches[pIndex][j-1]))
                                continue;
                            //pesVec2f mid = patches[pIndex][j].getMiddle(patches[pIndex][j+1]);
                            //pesVec2f last = fillBlocks[blockIndex].polyline.getVertices().back();
                            //if (last.distance(mid) >= runpitch)
                            //    fillBlocks[blockIndex].addStitch(mid);                            
                            if ((j % 4) == 0) runstitches.addVertex(patches[pIndex][j].getMiddle(patches[pIndex][j+1]));
                            satin.push_back(patches[pIndex][j]);
                        }
#ifndef __EMSCRIPTEN__
                        int b = runstitches.size();
                        runstitches.simplify(1);
                        int a = runstitches.size();
                        SkDebugf("--<< runstitch: %d - %d ==> %d\n", b, b - a, a);
#else
                        runstitches.simplify(1);
#endif
                        for (const auto& runstitch : runstitches) {
                            fillBlocks[blockIndex].addStitch(runstitch);
                        }
                        satin.push_back(patches[pIndex].getVertices().back());

                        // do satin zigzag for remain segments from lastIndex back to exitIndex
                        // by reverse satin list
                        std::reverse(satin.begin(), satin.end());

                        // then add to stitchBlocks
                        //int nums = (int)satin.size();
                        //for(int j=0; j<nums; j++){
                        //    fillBlocks[blockIndex].addStitch(satin[j]);
                        //}
                        for (const auto& s : satin) {
                            fillBlocks[blockIndex].addStitch(s);
                        }
                    }
                }

            } // end if(results.size())
        } // end if(FillType::FILL_TYPE_SATIN_COLUMN)
        
        else if(parameter.fillType==FillType::FILL_TYPE_CROSS_STITCH){
//            applyCrossStitchFill();
            applyCrossStitchFill2();
        }

    } // end if(paths.size())

    bool isOverideColors = false;
    if (parameter.text == "Avatar" || parameter.text == "Tag") {
        isOverideColors = parameter.useColorFromPicker;
    } 
    else {
        isOverideColors = parameter.fillColorIndex != 65;
    }

    if (isOverideColors) {
        for (auto& f : fillBlocks) {
            f.setColorFromIndex(parameter.fillColorIndex);
        }
    }

    recalculate();
}

void pesData::applyCrossStitchFill2(){
    int numBlock = (int)paths.size();
    pesPolyline prevOutline;
    pesVec2f prevStitch;
    int prevFillBlocksIndex = -1;
    pesEMBFillPreset::setDefaultPreset((pesEMBFillPreset::Preset)parameter.fill.presetIndex);
    pesEMBFill::Preset preset = pesEMBFillPreset::getDefaultPreset();
    
    // -------------------------------------------------------
    // Fill Dynamic
    // -------------------------------------------------------
//    if(parameter.fillType==FillType::FILL_TYPE_DYNAMIC){
    if( 1 ){
        const bool bUniteCutter = true;

        // build new paths stack only fill path and visible
        std::vector<pesPath>_ppaths;
        for( auto p: paths ) {
            if( !p.bVisible ) {
                continue;
            }
            if( p.isFill() ) {
                pesPath _p = pesPath(p);
                _ppaths.push_back( _p );
            }
        }

        // Cut Fill
        if( 1 ) {
            for( int pathIndex = _ppaths.size()-1; pathIndex >= 0; pathIndex-- ) {
                if( _ppaths[pathIndex].roleType != 2 && _ppaths[pathIndex].roleType != 3 ) { // not cutter
                    continue;
                }
                if( _ppaths[pathIndex].isStroke() )
                    continue;
                // cutter
                pesPath epath0 = _ppaths[pathIndex];

                // Unite cutter in same group
                if( bUniteCutter ) {
                    SkPath _epath0 = toSk( epath0 );
                        
                    SkOpBuilder builder;
                    builder.add(_epath0, kUnion_SkPathOp);
                    
                    int pi = pathIndex-1;
                    for( ; pi >= 0; pi-- ) {
                        if( _ppaths[pi].isStroke() )
                            continue;
                        if( _ppaths[pi].group_id == epath0.group_id ) {
                            SkPath _path = toSk( _ppaths[pi] );
                            builder.add(_path, kUnion_SkPathOp);
                        }
                        else {
                            break;
                        }
                    }
                    pathIndex = pi+1;

                    pes_skia::resolveBuilder(builder,&_epath0);
                    epath0 = toPes(_epath0);
                    //epath0.setFillColor( _ppaths[pathIndex].getFillColor() );
                    //_ppaths[pathIndex].clear();
                    //_ppaths[pathIndex] = epath0;
                }

                pesPath epath;
                if( epath0.roleOffset < 0 ) {
                    epath = PathInset( epath0, 0-epath0.roleOffset ); //
                }
                else if( epath0.roleOffset > 0 ) {
                    epath = PathOutset( epath0, epath0.roleOffset ); //
                }
                else {
                    epath = pesPath(epath0);
                }
                //SkDebugf( "Cutter: %s %0.2f\n", epath.path_id.c_str(), epath.roleOffset );
                SkPath eraserPath = toSk(epath);
                eraserPath.setFillType( SkPathFillType::kWinding );
                //Simplify( eraserPath, &eraserPath );

                for( int ip=pathIndex-1; ip>=0; ip-- ) {
                    pesPath &tpath = _ppaths[ip];
                    if( tpath.roleType != 1  && tpath.roleType != 3 ) // not cutable
                        continue;
                    if( tpath.isStroke() )
                        continue;

                    // Cuttable underneath
                    bool isFill = tpath.isFill();
                    bool isStroke = tpath.isStroke();
                    float strokeWidth = tpath.getStrokeWidth();
                    pesColor strokeColor = tpath.getStrokeColor();
                    pesColor fillColor = tpath.getFillColor();
                    int fillType = tpath.fillType;
                    float fillAngle = tpath.fillAngle;
                    int roleType = tpath.roleType;
                    float roleOffset = tpath.roleOffset;
                    std::string path_id = tpath.path_id;
                    std::string group_id = tpath.group_id;

                    SkPath targetPath = toSk(tpath);
                    targetPath.setFillType( SkPathFillType::kWinding );

                    SkOpBuilder builder;
                    if( epath0.roleOffset < 0 ) {
                        SkPath e0 = toSk(epath0);
                        e0.setFillType( SkPathFillType::kEvenOdd );
                        SkPath o0 = SkiaPathStroke( targetPath, (0-epath0.roleOffset) );
                        builder.add(e0, kUnion_SkPathOp);
                        builder.add(o0, kIntersect_SkPathOp);
                        builder.add(eraserPath, kUnion_SkPathOp);
                        builder.add(targetPath, kReverseDifference_SkPathOp);
                    }
                    else {
                        //Op(eraserPath, targetPath, SkPathOp::kReverseDifference_SkPathOp, &targetPath);
                        builder.add(eraserPath, kUnion_SkPathOp);
                        builder.add(targetPath, kReverseDifference_SkPathOp);
                    }
                    pes_skia::resolveBuilder(builder,&targetPath);

                    pesPath path = toPes( targetPath );
                    path.setStrokeColor( strokeColor );
                    path.setFillColor( fillColor );
                    path.setStrokeWidth( strokeWidth );
                    path.setFilled( isFill );
                    path.fillType = fillType;
                    path.fillAngle = fillAngle;
                    path.roleType = roleType;
                    path.roleOffset = roleOffset;
                    path.path_id = path_id;
                    path.group_id = group_id;

                    _ppaths[ip].clear();
                    _ppaths[ip] = path;
                }
            }
        }
        
        
        // Cut Self if unite cutter
        if( bUniteCutter ) {
            for( int pathIndex = _ppaths.size()-1; pathIndex >= 0; pathIndex-- ) {
                if( _ppaths[pathIndex].roleType != 2 && _ppaths[pathIndex].roleType != 3 ) { // not cutter
                    continue;
                }
                if( _ppaths[pathIndex].isStroke() )
                    continue;
                // cutter
                pesPath &epath0 = _ppaths[pathIndex]; // ChatGPT
                SkPath e0 = toSk(epath0);

                pesPath epath;
                if( epath0.roleOffset < 0 ) {
                    epath = PathInset( epath0, 0-epath0.roleOffset ); //
                }
                else if( epath0.roleOffset > 0 ) {
                    epath = PathOutset( epath0, epath0.roleOffset ); //
                }
                else {
                    epath = pesPath(epath0);
                }
                //SkDebugf( "Cutter: %s %0.2f\n", epath.path_id.c_str(), epath.roleOffset );
                SkPath eraserPath = toSk(epath);
                eraserPath.setFillType( SkPathFillType::kWinding );
                Simplify( eraserPath, &eraserPath );

                for( int ip=pathIndex-1; ip>=0; ip-- ) {
                    pesPath &tpath = _ppaths[ip];
                    if( tpath.group_id != epath.group_id )
                        continue;
                    if( tpath.roleType != 1  && tpath.roleType != 3 ) // not cutable
                        continue;
                    if( tpath.isStroke() )
                        continue;

                    // Cuttable underneath
                    bool isFill = tpath.isFill();
                    bool isStroke = tpath.isStroke();
                    float strokeWidth = tpath.getStrokeWidth();
                    pesColor strokeColor = tpath.getStrokeColor();
                    pesColor fillColor = tpath.getFillColor();
                    int fillType = tpath.fillType;
                    float fillAngle = tpath.fillAngle;
                    int roleType = tpath.roleType;
                    float roleOffset = tpath.roleOffset;
                    std::string path_id = tpath.path_id;
                    std::string group_id = tpath.group_id;

                    SkPath targetPath = toSk(tpath);
                    targetPath.setFillType( SkPathFillType::kWinding );
                    Simplify( targetPath, &targetPath );
                    //SkDebugf( "--- Cut fill %s\n", tpath.path_id.c_str() );

                    SkOpBuilder builder;
                    if( epath0.roleOffset < 0 ) {
                        SkPath o0 = SkiaPathStroke( targetPath, (0-epath0.roleOffset) );
                        builder.add(e0, kUnion_SkPathOp);
                        builder.add(o0, kIntersect_SkPathOp);
                        builder.add(eraserPath, kUnion_SkPathOp);
                        builder.add(targetPath, kReverseDifference_SkPathOp);
                    }
                    else {
                        //Op(eraserPath, targetPath, SkPathOp::kReverseDifference_SkPathOp, &targetPath);
                        builder.add(eraserPath, kUnion_SkPathOp);
                        builder.add(targetPath, kReverseDifference_SkPathOp);
                    }
                    pes_skia::resolveBuilder(builder,&targetPath);

                    pesPath path = toPes( targetPath );
                    path.setStrokeColor( strokeColor );
                    path.setFillColor( fillColor );
                    path.setStrokeWidth( strokeWidth );
                    path.setFilled( isFill );
                    path.fillType = fillType;
                    path.fillAngle = fillAngle;
                    path.roleType = roleType;
                    path.roleOffset = roleOffset;
                    path.path_id = path_id;
                    path.group_id = group_id;

                    _ppaths[ip].clear();
                    _ppaths[ip] = path;
                }
                
            }
        }

        //fillBlocks.resize(numBlock);
        fillBlocks.resize(_ppaths.size());
        for (size_t i = 0; i < _ppaths.size(); i++) {
            fillBlocks[i].setNearestColor(_ppaths[i].getFillColor());
            _ppaths[i]._fillColorIndex = fillBlocks[i].colorIndex;
        }

        std::vector<pesPath>sorted_ppaths;
        #define SORT_FILL true
        if( SORT_FILL ) {
            std::vector<int>colors;
            for (int ic = 0; ic < fillBlocks.size(); ic++) {
                auto& block = fillBlocks[ic];
                bool duplicate = false;
                for(auto c : colors){
                    if(block.colorIndex==c){
                        duplicate = true;
                        break;
                    }
                }
                if(!duplicate){
                    colors.push_back(block.colorIndex);
                }
            }
            //std::reverse( colors.begin(), colors.end() );

            for( int c: colors ) {
                for( auto p: _ppaths ) {
                    if( p._fillColorIndex == c ) {
                        sorted_ppaths.push_back( p );
                    }
                }
            }
            for(size_t i=0; i<sorted_ppaths.size(); i++)
            {
                fillBlocks[i].setNearestColor(sorted_ppaths[i].getFillColor());
            }
        }
        else {
            sorted_ppaths = _ppaths;
        }

        // -------------------------------------------------------
        // Paths OK
        // -------------------------------------------------------
        float pes_one_mm = 10;
        parameter.motifFill.size = 1.8;
        float size = parameter.motifFill.size * pes_one_mm;
        float space = parameter.motifFill.spacing * pes_one_mm;
        float oneMotif = size+space;
        float pitch = 0.2f * pes_one_mm;
        // Cross Stitch Preset Pattern (+Double Running)
        pesPath pattPath;
        pattPath.moveTo(0, 0);
        pattPath.lineTo(1024, 1024);
        pattPath.lineTo(0, 0);
        pattPath.lineTo(512, 512);
        pattPath.lineTo(1024, 0);
        pattPath.lineTo(0, 1024);
        pattPath.lineTo(1024, 0);

        if(pattPath.getCommands().size()<2){
            pattPath = CreateDefaultMotif();
        }
        const float originalSize = 1024;
        float realSize = size;
        float scale = realSize/originalSize;
        pattPath.scale(scale, scale);
        pesVec2f right(1,0);
        
        int pindex = 0;
        for(auto p: sorted_ppaths){
            vector<pesPath> subpath = p.getSubPath();
            if(subpath.size()){
                int first_outer_dir = subpath[0].getOutline()[0].getArea()>=0 ? __cw : __ccw;

                #define SORT_SUBPATH true
                if( SORT_SUBPATH ){
                    pesSort(subpath, [](const pesPath & p0, const pesPath & p1)->bool{
                        //if(fabs(p0.getOutline()[0].getArea())>fabs(p1.getOutline()[0].getArea()))
                        if( p0.getOutline()[0].getCentroid2D().x < p1.getOutline()[0].getCentroid2D().x )
                            return true;
                        return false;
                    });
                }

                for(int shell=0; shell<(int)subpath.size(); shell++){
                    pesEMBFill epf;
                    std::string hashkey;

                    float shell_area = subpath[shell].getOutline()[0].getArea();
                    // Ignore small area (eye hilight is around 50)
                    if( shell_area >= -15 && shell_area <= 15 ) {
                        continue;
                    }

                    int shell_dir = subpath[shell].getOutline()[0].getArea()>=0 ? __cw : __ccw;
                    if(shell_dir==first_outer_dir){
                        pesPath shell_and_hole = subpath[shell];
                        // Forming single shell & hole
                        for(int hole=0; hole<(int)subpath.size(); hole++){
                            int hole_dir = subpath[hole].getOutline()[0].getArea()>=0 ? __cw : __ccw;
                            if(hole_dir!=first_outer_dir){
                                shell_and_hole.append(subpath[hole]);
                            }
                        }
                        shell_and_hole.simplify(0.3f);

                        float density = parameter.fill.density;
                        int sewDirection = 0;
                        float compensate = parameter.pullCompensate;
                        pesEMBFill epf(shell_and_hole);
                        vector<pesPolyline> patches;
                        deque<vector<pesEMBFill::Line>> sections = epf.createCrossStitchSections(parameter.motifFill.size+parameter.motifFill.rowSpacing);
                        pesVec2f _lastStitch(0,0);
                        int blockIndex = pindex;
                        while(sections.size()){
                            if(_lastStitch.length()>0){
                                pesVec2f start_corner;
                                int section_index = epf.find_nearest_section(sections, _lastStitch, start_corner);
                                vector<bool> jumps;
                                pesPolyline patch = epf.connect_points(_lastStitch, start_corner, jumps);
    //                            for(auto s:patch.getVertices()){
    //                                fillBlocks[blockIndex].addStitch(s);
    //                            }
                                if(patch.getVertices().size())
                                    fillBlocks[blockIndex].addJump(patch.getVertices().back());

                                vector<pesEMBFill::Line> section = sections.at(section_index);
                                if(start_corner!=section[0].start && start_corner!=section[0].end){
                                    std::reverse(section.begin(), section.end());
                                }

                                if(start_corner!=section[0].start){
                                    for(int i=0; i<(int)section.size(); i++){
                                        std::swap(section[i].start, section[i].end);
                                    }
                                }

                                //bool first_segment = true;
                                bool swap = false;
                                pesVec2f last_end(0,0);
                                for(auto segment : section){
                                    pesVec2f beg = segment.start;
                                    pesVec2f end = segment.end;
                                    if(swap){
                                        std::swap(beg, end);
                                    }
                                    swap=!swap;

                                    pesPolyline p;
                                    p.addVertex(beg);
                                    p.addVertex(end);
                                    float l = p.getPerimeter();
                                    int numMotif = (int)(l/oneMotif);
                                    pesPolyline resample = p.getResampledByCount2(numMotif);

                                    pesVec2f p0 = resample[0];
                                    int loopCount = resample.isClosed() ? (int)resample.size()+1 : (int)resample.size();
                                    for(int i=1; i<loopCount; i++){
                                        int wrapedIndex = resample.getWrappedIndex(i);
                                        pesVec2f p1 = resample[wrapedIndex];
                                        float seg_len = p1.distance(p0);
                                        if (seg_len == 0.0)
                                            continue;

                                        pesVec2f along = (p1-p0).normalize();
                                        float angle = right.angle(along);
                                        pesPath motif = pattPath;
                                        motif.rotate(angle);

                                        pesVec2f ms = motif.getOutline().front().getVertices().front();
                                        pesVec2f d = p0 - ms;
                                        bool __motif_align_vcenter = true;
                                        if(__motif_align_vcenter){
                                            pesVec2f start(0, size/2);
                                            start.rotate(angle);
                                            d = p0 - start;
                                        }
                                        motif.translate(d.x, d.y);

                                        auto vertices = motif.getOutline().front().getVertices();
                                        for(int j=0; j<(int)vertices.size(); j++){
    //                                                Stitch s((int)round(vertices[j].x), (int)round(vertices[j].y));
                                            pesVec2f s(vertices[j]);
                                            int type = NORMAL_STITCH;
                                            if(j==0 && i==1){
                                                type = JUMP_STITCH;
                                            }
                                            fillBlocks[blockIndex].addStitch(s, type);
                                        }
                                        pesVec2f last = vertices.back();
                                        float dist = last.distance(p1);
                                        if(dist>pitch){
                                            along = (p1-last).normalize();
                                            float run_len = pitch;
                                            while (run_len<dist) {
                                                //pesVec2f stitch = p0 + along * run_len;
                                                //pesVec2f s(stitch);
    //                                                    fillBlocks[blockIndex].addStitch(s);
                                                run_len+=pitch;
                                            }
                                        }
                                        p0 = p1;
                                    } // end for(int i=1; i<loopCount; ...)

                                    if(fillBlocks[blockIndex].polyline.getVertices().size()){
                                        pesVec2f last(fillBlocks[blockIndex].polyline.getVertices().back());
                                        if(p0.distance(last)>0.1* pes_one_mm){
                                            pesVec2f s(p0);
                                            fillBlocks[blockIndex].addStitch(s);
                                        }
                                    }
                                } // end for(auto segment : section)

                                sections.erase(sections.begin()+section_index);
                            }
                            else{
                                vector<pesEMBFill::Line> section = sections.front();
                                //bool first_segment = true;
                                bool swap = false;
                                pesVec2f last_end(0,0);
                                for(auto segment : section){
                                    pesVec2f beg = segment.start;
                                    pesVec2f end = segment.end;
                                    if(swap){
                                        std::swap(beg, end);
                                    }
                                    swap=!swap;

                                    pesPolyline p;
                                    p.addVertex(beg);
                                    p.addVertex(end);
                                    float l = p.getPerimeter();
                                    int numMotif = (int)(l/oneMotif);
                                    pesPolyline resample = p.getResampledByCount2(numMotif);

                                    pesVec2f p0 = resample[0];
                                    int loopCount = resample.isClosed() ? (int)resample.size()+1 : (int)resample.size();
                                    for(int i=1; i<loopCount; i++){
                                        int wrapedIndex = resample.getWrappedIndex(i);
                                        pesVec2f p1 = resample[wrapedIndex];
                                        float seg_len = p1.distance(p0);
                                        if (seg_len == 0.0)
                                            continue;

                                        pesVec2f along = (p1-p0).normalize();
                                        float angle = right.angle(along);
                                        pesPath motif = pattPath;
                                        motif.rotate(angle);

                                        pesVec2f ms = motif.getOutline().front().getVertices().front();
                                        pesVec2f d = p0 - ms;
                                        bool __motif_align_vcenter = true;
                                        if(__motif_align_vcenter){
                                            pesVec2f start(0, size/2);
                                            start.rotate(angle);
                                            d = p0 - start;
                                        }
                                        motif.translate(d.x, d.y);

                                        auto vertices = motif.getOutline().front().getVertices();
                                        for(int j=0; j<(int)vertices.size(); j++){
                                            pesVec2f s(vertices[j]);
                                            int type = NORMAL_STITCH;
                                            if(j==0 && i==1){
                                               type = JUMP_STITCH;
                                            }
                                            fillBlocks[blockIndex].addStitch(s, type);
                                        }
                                        pesVec2f last = vertices.back();
                                        float dist = last.distance(p1);
                                        if(dist>pitch){
                                            along = (p1-last).normalize();
                                            float run_len = pitch;
                                            while (run_len<dist) {
                                                //pesVec2f stitch = p0 + along * run_len;
                                                //pesVec2f s(stitch);
    //                                                    fillBlocks[blockIndex].addStitch(s);
                                                run_len+=pitch;
                                            }
                                        }

                                        p0 = p1;
                                    } // end for(int i=1; i<loopCount; i++)

                                    if(fillBlocks[blockIndex].polyline.getVertices().size()){
                                        pesVec2f last( fillBlocks[blockIndex].polyline.getVertices().back() );
                                        if(p0.distance(last)>0.1* pes_one_mm){
                                            pesVec2f s(p0);
                                            fillBlocks[blockIndex].addStitch(s);
                                        }
                                    }

                                } // end for(auto segment : section)

                                sections.pop_front();
                            }// end if(_lastStitch.length()>0)...else
                            
                            if(fillBlocks[blockIndex].polyline.getVertices().size()){
                                pesVec2f last(fillBlocks[blockIndex].polyline.getVertices().back());
                                _lastStitch = last;
                            }
                        } // end while(sections.size())
                        
                    }
                }
            }
            pindex++;
        } // end for()
    }
    // -------------------------------------------------------
}

void pesData::applyCrossStitchFill(){
    float pes_one_mm = 10;
    parameter.motifFill.size = 1.8;
    float size = parameter.motifFill.size * pes_one_mm;
    float space = parameter.motifFill.spacing * pes_one_mm;
    //float rowspace = parameter.motifFill.rowSpacing * pes_one_mm;
    float oneMotif = size+space;
    //float rowSpacing = size+rowspace;
//    float pitch = 0.2f * pes_one_mm;
    float pitch = 0.2f * pes_one_mm;

    // TODO: Need load patternBuffer outside pesEngine
//    pesPath pattPath = pesLoadPMFFromBuffer(parameter.motifFill.patternBuffer);
    
//    // Cross Stitch Preset Pattern
//    pesPath pattPath;
//    pattPath.moveTo(0, 0);
//    pattPath.lineTo(1024, 1024);
//    pattPath.lineTo(512, 512);
//    pattPath.lineTo(0, 1024);
//    pattPath.lineTo(1024, 0);
    
    // Cross Stitch Preset Pattern (+Double Running)
    pesPath pattPath;
    pattPath.moveTo(0, 0);
    pattPath.lineTo(1024, 1024);
    pattPath.lineTo(0, 0);
    pattPath.lineTo(512, 512);
    pattPath.lineTo(1024, 0);
    pattPath.lineTo(0, 1024);
    pattPath.lineTo(1024, 0);
    
    if(pattPath.getCommands().size()<2){
        pattPath = CreateDefaultMotif();
    }
    const float originalSize = 1024;
    float realSize = size;
    float scale = realSize/originalSize;
    pattPath.scale(scale, scale);
    pesVec2f right(1,0);

    int pindex = 0;
    for(auto p: paths){
        vector<pesPath> subpath = p.getSubPath();
        if(subpath.size()){
            bool keepOrder = (parameter.type == pesData::OBJECT_TYPE_SCALABLE_TTF_TEXT);
            if(keepOrder==false && PES_SORTING_BY_AREA==true){
                pesSort(subpath, [](const pesPath & p0, const pesPath & p1)->bool{
                    if(fabs(p0.getOutline()[0].getArea())>fabs(p1.getOutline()[0].getArea()))
                        return true;
                    return false;
                });
            }

            int first_outer_dir = subpath[0].getOutline()[0].getArea()>=0 ? __cw : __ccw;
            for(int shell=0; shell<(int)subpath.size(); shell++){
                int shell_dir = subpath[shell].getOutline()[0].getArea()>=0 ? __cw : __ccw;
                if(shell_dir==first_outer_dir){
                    pesPath shell_and_hole = subpath[shell];
                    for(int hole=shell+1; hole<(int)subpath.size(); hole++){
                        int hole_dir = subpath[hole].getOutline()[0].getArea()>=0 ? __cw : __ccw;
                        if(hole_dir!=first_outer_dir){
                            shell_and_hole.append(subpath[hole]);
                        }
                    }

                    shell_and_hole.simplify(0.3f);
                    pesEMBFill epf(shell_and_hole);
                    vector<pesPolyline> patches;


//                    deque<vector<pesEMBFill::Line>> sections = epf.createMFSections(parameter.motifFill.size+parameter.motifFill.rowSpacing, 0);
                    
                    deque<vector<pesEMBFill::Line>> sections = epf.createCrossStitchSections(parameter.motifFill.size+parameter.motifFill.rowSpacing);

                    pesVec2f _lastStitch(0,0);
                    //float max_stitch_length = 1 * 10.0;
                    int blockIndex = pindex;
                    while(sections.size()){
                        if(_lastStitch.length()>0){
                            pesVec2f start_corner;
                            int section_index = epf.find_nearest_section(sections, _lastStitch, start_corner);
                            vector<bool> jumps;
                            pesPolyline patch = epf.connect_points(_lastStitch, start_corner, jumps);
//                            for(auto s:patch.getVertices()){
//                                fillBlocks[blockIndex].addStitch(s);
//                            }
                            if(patch.getVertices().size())
                                fillBlocks[blockIndex].addJump(patch.getVertices().back());

                            vector<pesEMBFill::Line> section = sections.at(section_index);
                            if(start_corner!=section[0].start && start_corner!=section[0].end){
                                std::reverse(section.begin(), section.end());
                            }

                            if(start_corner!=section[0].start){
                                for(int i=0; i<(int)section.size(); i++){
                                    std::swap(section[i].start, section[i].end);
                                }
                            }

                            //bool first_segment = true;
                            bool swap = false;
                            pesVec2f last_end(0,0);
                            for(auto segment : section){
                                pesVec2f beg = segment.start;
                                pesVec2f end = segment.end;
                                if(swap){
                                    std::swap(beg, end);
                                }
                                swap=!swap;

                                pesPolyline p;
                                p.addVertex(beg);
                                p.addVertex(end);
                                float l = p.getPerimeter();
                                int numMotif = (int)(l/oneMotif);
                                pesPolyline resample = p.getResampledByCount2(numMotif);

                                pesVec2f p0 = resample[0];
                                int loopCount = resample.isClosed() ? (int)resample.size()+1 : (int)resample.size();
                                for(int i=1; i<loopCount; i++){
                                    int wrapedIndex = resample.getWrappedIndex(i);
                                    pesVec2f p1 = resample[wrapedIndex];
                                    float seg_len = p1.distance(p0);
                                    if (seg_len == 0.0)
                                        continue;

                                    pesVec2f along = (p1-p0).normalize();
                                    float angle = right.angle(along);
                                    pesPath motif = pattPath;
                                    motif.rotate(angle);

                                    pesVec2f ms = motif.getOutline().front().getVertices().front();
                                    pesVec2f d = p0 - ms;
                                    bool __motif_align_vcenter = true;
                                    if(__motif_align_vcenter){
                                        pesVec2f start(0, size/2);
                                        start.rotate(angle);
                                        d = p0 - start;
                                    }
                                    motif.translate(d.x, d.y);

                                    auto vertices = motif.getOutline().front().getVertices();
                                    for(int j=0; j<(int)vertices.size(); j++){
//                                                Stitch s((int)round(vertices[j].x), (int)round(vertices[j].y));
                                        pesVec2f s(vertices[j]);
                                        int type = NORMAL_STITCH;
                                        if(j==0 && i==1){
                                            type = JUMP_STITCH;
                                        }
                                        fillBlocks[blockIndex].addStitch(s, type);
                                    }
                                    pesVec2f last = vertices.back();
                                    float dist = last.distance(p1);
                                    if(dist>pitch){
                                        along = (p1-last).normalize();
                                        float run_len = pitch;
                                        while (run_len<dist) {
                                            //pesVec2f stitch = p0 + along * run_len;
                                            //pesVec2f s(stitch);
//                                                    fillBlocks[blockIndex].addStitch(s);
                                            run_len+=pitch;
                                        }
                                    }
                                    p0 = p1;
                                } // end for(int i=1; i<loopCount; ...)

                                if(fillBlocks[blockIndex].polyline.getVertices().size()){
                                    pesVec2f last(fillBlocks[blockIndex].polyline.getVertices().back());
                                    if(p0.distance(last)>0.1* pes_one_mm){
                                        pesVec2f s(p0);
                                        fillBlocks[blockIndex].addStitch(s);
                                    }
                                }
                            } // end for(auto segment : section)

                            sections.erase(sections.begin()+section_index);
                        }
                        else{
                            vector<pesEMBFill::Line> section = sections.front();
                            //bool first_segment = true;
                            bool swap = false;
                            pesVec2f last_end(0,0);
                            for(auto segment : section){
                                pesVec2f beg = segment.start;
                                pesVec2f end = segment.end;
                                if(swap){
                                    std::swap(beg, end);
                                }
                                swap=!swap;

                                pesPolyline p;
                                p.addVertex(beg);
                                p.addVertex(end);
                                float l = p.getPerimeter();
                                int numMotif = (int)(l/oneMotif);
                                pesPolyline resample = p.getResampledByCount2(numMotif);

                                pesVec2f p0 = resample[0];
                                int loopCount = resample.isClosed() ? (int)resample.size()+1 : (int)resample.size();
                                for(int i=1; i<loopCount; i++){
                                    int wrapedIndex = resample.getWrappedIndex(i);
                                    pesVec2f p1 = resample[wrapedIndex];
                                    float seg_len = p1.distance(p0);
                                    if (seg_len == 0.0)
                                        continue;

                                    pesVec2f along = (p1-p0).normalize();
                                    float angle = right.angle(along);
                                    pesPath motif = pattPath;
                                    motif.rotate(angle);

                                    pesVec2f ms = motif.getOutline().front().getVertices().front();
                                    pesVec2f d = p0 - ms;
                                    bool __motif_align_vcenter = true;
                                    if(__motif_align_vcenter){
                                        pesVec2f start(0, size/2);
                                        start.rotate(angle);
                                        d = p0 - start;
                                    }
                                    motif.translate(d.x, d.y);

                                    auto vertices = motif.getOutline().front().getVertices();
                                    for(int j=0; j<(int)vertices.size(); j++){
                                        pesVec2f s(vertices[j]);
                                        int type = NORMAL_STITCH;
                                        if(j==0 && i==1){
                                           type = JUMP_STITCH;
                                        }
                                        fillBlocks[blockIndex].addStitch(s, type);
                                    }
                                    pesVec2f last = vertices.back();
                                    float dist = last.distance(p1);
                                    if(dist>pitch){
                                        along = (p1-last).normalize();
                                        float run_len = pitch;
                                        while (run_len<dist) {
                                            //pesVec2f stitch = p0 + along * run_len;
                                            //pesVec2f s(stitch);
//                                                    fillBlocks[blockIndex].addStitch(s);
                                            run_len+=pitch;
                                        }
                                    }

                                    p0 = p1;
                                } // end for(int i=1; i<loopCount; i++)

                                if(fillBlocks[blockIndex].polyline.getVertices().size()){
                                    pesVec2f last( fillBlocks[blockIndex].polyline.getVertices().back() );
                                    if(p0.distance(last)>0.1* pes_one_mm){
                                        pesVec2f s(p0);
                                        fillBlocks[blockIndex].addStitch(s);
                                    }
                                }

                            } // end for(auto segment : section)

                            sections.pop_front();
                        }// end if(_lastStitch.length()>0)...else

                        if(fillBlocks[blockIndex].polyline.getVertices().size()){
                            pesVec2f last(fillBlocks[blockIndex].polyline.getVertices().back());
                            _lastStitch = last;
                        }
                    } // end while(sections.size())

                } // end if(shell_dir==first_outer_dir)

            } // end for(int shell=0; ...)

        } // end if(subpath.size())

        pindex++;
    } // end for(auto p: paths)
}

void pesData::applyPPEFFill(){
    fillBlocks.clear();
    if(parameter.isPPEFPath && paths.size()){
        int numBlock = 1;
        if(parameter.border && (parameter.shapeIndex<=6)){
            numBlock = 2;
        }
        fillBlocks.resize(numBlock);
        int ppmm = 10;
        float satinDensity = parameter.fill.density;
        float zigzagSpacing = 1.0f * ppmm / satinDensity;
        pesSatinColumn::parameter_t param;
        param.satinUnderlay = false;
        param.satinZigzagUnderlay = false;
        param.pixels_per_mm = ppmm;
        param.zigzag_spacing_mm = zigzagSpacing;
        param.pull_compensation_mm = parameter.pullCompensate;
        
        int colorIndex = parameter.fillColorIndex + 0;
        fillBlocks[0].setColorFromIndex(colorIndex);
        if(numBlock==2){
            fillBlocks[1].setColorFromIndex(parameter.borderColorIndex);
        }
        
        std::vector<pesPolyline> patches;
        std::vector<pesPolyline> underlayPatches;
        for(auto path : paths){
            int s = (int)path.getOutline().size();
            if(s && s%2==0){
                auto  coms = path.getCommands();
                int pathIndex = -1;
                vector<pesPath> csp;
                csp.resize(2);
                
                for(size_t i=0; i<coms.size(); i++){
                    if(coms[i].type==pesPath::Command::Type::_moveTo){
                        pathIndex++;
                        if(pathIndex>1 && pathIndex%2==0){
                            
                            pesSatinColumn satin(csp);
                            satin.param = param;
                            patches.emplace_back();
                            underlayPatches.emplace_back();
                            satin.doUnderlay(underlayPatches.back());
                            satin.doSatin(param.zigzag_spacing_mm, patches.back());
                            
                            csp[0].clear();
                            csp[1].clear();
                        }
                    }
                    
                    if(pathIndex%2==0){ // event
                        csp[0].addCommand(coms[i]);
                    }
                    else{ // odd
                        csp[1].addCommand(coms[i]);
                    }
                }
                
                if(csp[0].getCommands().size() && csp[1].getCommands().size()){
                    pesSatinColumn satin(csp);
                    satin.param = param;
                    patches.emplace_back();
                    underlayPatches.emplace_back();
                    satin.doUnderlay(underlayPatches.back());
                    satin.doSatin(param.zigzag_spacing_mm, patches.back());
                }
            }
            else{
//                GUI_LogError("PESData::applyPPEFFill()") << "invalid block size";
            }
        } // end for(auto path : paths)
        
        vector<pesNearestBranchingResult> results;
        pesFindNearestEnterAndExit(patches, results, true, false);
        int num = (int)patches.size();
        if(results.size()){
            float runpitch = 1.5 * param.pixels_per_mm;
            const float underlayRunpitch = 2.5 * param.pixels_per_mm;
            for(auto rs:results){
                int enterIndex = rs.enterIndex;
                int exitIndex = rs.exitIndex;
                int pIndex = rs.patchesIndex;
                int blockIndex = 0;
                if(numBlock==2){
                    if(parameter.shapeIndex==3){
                        // circle border
                        if(pIndex>=num-2)
                            blockIndex=1;
                    }
                    else{
                        // normal border
                        if(pIndex==num-1)
                            blockIndex=1;
                    }
                }
                
                // Build underlay center walk
                pesPolyline centerWalk;
                for(int ip=0; ip<patches[pIndex].size()-1; ip++){
                    pesVec2f mid = patches[pIndex][ip].getMiddle(patches[pIndex][ip+1]);
                    centerWalk.addVertex(mid);
                }
//                SkDebugf("center walk distance=%.2f\n", centerWalk.getPerimeter());
                float perimeter = centerWalk.getPerimeter();
                if(perimeter/runpitch<3){
                    runpitch = perimeter / 3;
#ifndef __EMSCRIPTEN__
                    SkDebugf("new runpitch=%.2f\n", runpitch);
#endif
                }
                
                if(parameter.fill.underlay){
                    int num_points = (int)round(centerWalk.getPerimeter() / underlayRunpitch);
                    
                    // fixed: centerWalk size < 2
                    // fixed: num_points == 0 && centerWalk size == 1, result is always a triple
                    // points at x:0, y:0
                    if (centerWalk.size() > 0) {
                        if (num_points < 2) {
                            if (centerWalk.size() > 1) {
                                pesPolyline poly;
                                poly.addVertex(centerWalk.getPointAtPercent(0.5f));
                                centerWalk = poly;
                            }
                        } else {
                            centerWalk = centerWalk.getResampledByCount2(num_points);
                        }

                        // from satin exit point walk bak to underlay starting point
                        if (fillBlocks[blockIndex].size()) {
                            auto last = fillBlocks[blockIndex].polyline.getVertices().back();
                            // safety: assign default value
                            unsigned int exitNearestIndex = 0;
                            centerWalk.getClosestPoint( fillBlocks[blockIndex].polyline.getVertices().back(), &exitNearestIndex);
                            for (int i = exitNearestIndex; i >= 0; --i) {
                                if (i == exitNearestIndex) {
                                    fillBlocks[blockIndex].addJump(centerWalk[i], false);
                                } 
                                else {
                                    fillBlocks[blockIndex].addStitch(centerWalk[i]);
                                }
                            }
                        }

                        // Insert Underlay
                        for (int i = 0; i < underlayPatches[pIndex].size(); i++) {
                            if (i == 0) {
                                fillBlocks[blockIndex].addJump(underlayPatches[pIndex][i], false);
                            } 
                            else {
                                fillBlocks[blockIndex].addStitch(underlayPatches[pIndex][i]);
                            }
                        }

                        // from underlay walk back to satin
                        // safety: assign default value
                        unsigned int nearestIndex = 0, enterNearestIndex = 0;
                        if (fillBlocks[blockIndex].polyline.size()) {
                            centerWalk.getClosestPoint(fillBlocks[blockIndex].polyline.getVertices().back(), &nearestIndex);
                        } 
                        else {
                            nearestIndex = 0;
                        }
                        centerWalk.getClosestPoint(patches[pIndex][enterIndex], &enterNearestIndex);
                        if (nearestIndex < enterNearestIndex) {
                            for (int i = nearestIndex; i <= enterNearestIndex; i++) {
                                fillBlocks[blockIndex].addStitch(centerWalk[i]);
                            }
                        } 
                        else if (nearestIndex > enterNearestIndex) {
                            for (int i = nearestIndex; i > enterNearestIndex; i--) {
                                fillBlocks[blockIndex].addStitch(centerWalk[i]);
                            }
                        }
                    }
                }
                
                // Satin
                fillBlocks[blockIndex].addJump(patches[pIndex][enterIndex], false);
                if(enterIndex>0){
                    // walk-back to [0](firstIndex)
                    for(int j=enterIndex; j>=1; j--){
                        if(patches[pIndex][j].match(patches[pIndex][j-1]))
                            continue;
                        pesVec2f mid = patches[pIndex][j].getMiddle(patches[pIndex][j-1]);
                        auto last = fillBlocks[blockIndex].polyline.getVertices().back();
                        if(last.distance(mid)>=runpitch)
                            fillBlocks[blockIndex].addStitch(mid);
                    }
                }
                
                // do satin zigzag for every segments from firstIndex to exitIndex
                if(enterIndex!=0){
                    fillBlocks[blockIndex].addStitch(patches[pIndex][0]);
                }
                int cnt = (int)patches[pIndex].size();
                for(int j=1; j<=exitIndex; j++){
                    fillBlocks[blockIndex].addStitch( patches[pIndex][j]);
                }
                
                // if exitIndex < lastIndex
                if(exitIndex<cnt-1){
                    vector<pesVec2f> satin;
                    // walk-forward from exitIndex to lastIndex
                    for(int j=exitIndex; j<cnt-1; j++){
                        if(patches[pIndex][j].match(patches[pIndex][j-1]))
                            continue;
                        pesVec2f mid = patches[pIndex][j].getMiddle(patches[pIndex][j+1]);
                        auto last = fillBlocks[blockIndex].polyline.getVertices().back();
                        if(last.distance(mid)>=runpitch)
                            fillBlocks[blockIndex].addStitch(mid);
                        
                        satin.push_back(patches[pIndex][j]);
                    }
                    satin.push_back(patches[pIndex].getVertices().back());
                    
                    // do satin zigzag for remain segments from lastIndex back to exitIndex
                    // by reverse satin list
                    std::reverse(satin.begin(), satin.end());
                    
                    // then add to stitchBlocks
                    int numsatin = (int)satin.size();
                    for(int j=0; j<numsatin; j++){
                        fillBlocks[blockIndex].addStitch(satin[j]);
                    }
                    
                    // fixed
                    pesVec2f exitPos = patches[pIndex][exitIndex];
                    pesVec2f lastPos = fillBlocks[blockIndex].polyline.getVertices().back();
                    if(!exitPos.match(lastPos)){
                        fillBlocks[blockIndex].addStitch(exitPos);
                    }
                }
                
            } // end for(auto rs:results)
        } // end results.size()
    }
    
    recalculate();
}

void pesData::applyPPEF_V2_Fill(){
    fillBlocks.clear();
    if(parameter.isPPEFPath && paths.size()){
        int numSegments = 0;
        for(auto path : paths){
            int s = (int)path.getOutline().size();
            numSegments += s/2;
        }
        int numBlocks = numSegments;
        if(parameter.border && (parameter.shapeIndex<=6)){
            numBlocks = numSegments + 1;
        }
        fillBlocks.resize(numBlocks);
        int ppmm = 10;
        float satinDensity = parameter.fill.density;
        float zigzagSpacing = 1.0f * ppmm / satinDensity;
        pesSatinColumn::parameter_t param;
        param.satinUnderlay = false;
        param.satinZigzagUnderlay = false;
        param.pixels_per_mm = ppmm;
        param.zigzag_spacing_mm = zigzagSpacing;
        param.pull_compensation_mm = parameter.pullCompensate;
        
//        int colorIndex = parameter.fillColorIndex + 0;
//        fillBlocks[0].setColorFromIndex(colorIndex);
        for(int i=0; i<fillBlocks.size(); i++){
            int colorIndex = 0x20;
            if(parameter.colors.size()>i){
                colorIndex = parameter.colors[i];
            }
            fillBlocks[i].setColorFromIndex(colorIndex);
        }
        if(numBlocks==numSegments + 1){
            fillBlocks.back().setColorFromIndex(parameter.borderColorIndex);
        }
        
        std::vector<pesPolyline> patches;
        std::vector<pesPolyline> underlayPatches;
        for(auto path : paths){
            int s = (int)path.getOutline().size();
            if(s && s%2==0){
                auto  coms = path.getCommands();
                int pathIndex = -1;
                vector<pesPath> csp;
                csp.resize(2);
                
                for(size_t i=0; i<coms.size(); i++){
                    if(coms[i].type==pesPath::Command::Type::_moveTo){
                        pathIndex++;
                        if(pathIndex>1 && pathIndex%2==0){
                            
                            pesSatinColumn satin(csp);
                            satin.param = param;
                            patches.emplace_back();
                            underlayPatches.emplace_back();
                            satin.doUnderlay(underlayPatches.back());
                            satin.doSatin(param.zigzag_spacing_mm, patches.back());
                            
                            csp[0].clear();
                            csp[1].clear();
                        }
                    }
                    
                    if(pathIndex%2==0){ // event
                        csp[0].addCommand(coms[i]);
                    }
                    else{ // odd
                        csp[1].addCommand(coms[i]);
                    }
                }
                
                if(csp[0].getCommands().size() && csp[1].getCommands().size()){
                    pesSatinColumn satin(csp);
                    satin.param = param;
                    patches.emplace_back();
                    underlayPatches.emplace_back();
                    satin.doUnderlay(underlayPatches.back());
                    satin.doSatin(param.zigzag_spacing_mm, patches.back());
                }
            }
            else{
//                GUI_LogError("PESData::applyPPEFFill()") << "invalid block size";
            }
        } // end for(auto path : paths)
        
        vector<pesNearestBranchingResult> results;
        pesFindNearestEnterAndExit(patches, results, true, false);
        int num = (int)patches.size();
        if(results.size()){
            const float runpitch = 1.5 * param.pixels_per_mm;
            const float underlayRunpitch = 2.5 * param.pixels_per_mm;
            for(auto rs:results){
                int enterIndex = rs.enterIndex;
                int exitIndex = rs.exitIndex;
                int pIndex = rs.patchesIndex;
//                int blockIndex = 0;
                int blockIndex = rs.patchesIndex;
//                if(numBlock==2){
                if(numBlocks==numSegments + 1){
                    if(parameter.shapeIndex==3){
                        // circle border
                        if(pIndex>=num-2)
                            blockIndex = numBlocks -1; //1;
                    }
                    else{
                        // normal border
                        if(pIndex==num-1)
                            blockIndex = numBlocks -1; //1;
                    }
                }
                
                if(parameter.fill.underlay){
                    // Build underlay center walk
                    pesPolyline centerWalk;
                    for(int ip=0; ip<patches[pIndex].size()-1; ip++){
                        pesVec2f mid = patches[pIndex][ip].getMiddle(patches[pIndex][ip+1]);
                        centerWalk.addVertex(mid);
                    }
                    int num_points = (int)round(centerWalk.getPerimeter() / underlayRunpitch);
                    centerWalk = centerWalk.getResampledByCount2(num_points);
                    
                    // from satin exit point walk bak to underlay starting point
                    if(fillBlocks[blockIndex].size()){
                        auto last = fillBlocks[blockIndex].polyline.getVertices().back();
                        unsigned int exitNearestIndex;
                        centerWalk.getClosestPoint(fillBlocks[blockIndex].polyline.getVertices().back(), &exitNearestIndex );
                        for(int i=exitNearestIndex; i>=0; --i){
                            if(i==exitNearestIndex){
                                fillBlocks[blockIndex].addJump(centerWalk[i], false);
                            }
                            else{
                                fillBlocks[blockIndex].addStitch(centerWalk[i]);
                            }
                        }
                    }
                    
                    // Insert Underlay
                    for(int i=0; i<underlayPatches[pIndex].size(); i++){
                        if(i==0){
                            fillBlocks[blockIndex].addJump(underlayPatches[pIndex][i], false);
                        }
                        else{
                            fillBlocks[blockIndex].addStitch(underlayPatches[pIndex][i]);
                        }
                    }
                    
                    // from underlay walk back to satin
                    unsigned int nearestIndex, enterNearestIndex;
                    if(fillBlocks[blockIndex].polyline.size()){
                        centerWalk.getClosestPoint(fillBlocks[blockIndex].polyline.getVertices().back(), &nearestIndex );
                    }
                    else{
                        nearestIndex = 0;
                    }
                    centerWalk.getClosestPoint(patches[pIndex][enterIndex], &enterNearestIndex );
                    if(nearestIndex < enterNearestIndex){
                        for(int i=nearestIndex; i<=enterNearestIndex; i++){
                            fillBlocks[blockIndex].addStitch(centerWalk[i]);
                        }
                    }
                    else if(nearestIndex > enterNearestIndex){
                        for(int i=nearestIndex; i>enterNearestIndex; i--){
                            fillBlocks[blockIndex].addStitch(centerWalk[i]);
                        }
                    }
                }
                
                // Satin
                fillBlocks[blockIndex].addJump(patches[pIndex][enterIndex], false);
                if(enterIndex>0){
                    // walk-back to [0](firstIndex)
                    for(int j=enterIndex; j>=1; j--){
                        if(patches[pIndex][j].match(patches[pIndex][j-1]))
                            continue;
                        pesVec2f mid = patches[pIndex][j].getMiddle(patches[pIndex][j-1]);
                        auto last = fillBlocks[blockIndex].polyline.getVertices().back();
                        if(last.distance(mid)>=runpitch)
                            fillBlocks[blockIndex].addStitch(mid);
                    }
                }
                
                // do satin zigzag for every segments from firstIndex to exitIndex
                fillBlocks[blockIndex].addStitch(patches[pIndex][0]);
                int cnt = (int)patches[pIndex].size();
                for(int j=1; j<=exitIndex; j++){
                    fillBlocks[blockIndex].addStitch( patches[pIndex][j]);
                }
                
                // if exitIndex < lastIndex
                if(exitIndex<cnt-1){
                    vector<pesVec2f> satin;
                    // walk-forward from exitIndex to lastIndex
                    for(int j=exitIndex; j<cnt-1; j++){
                        if(patches[pIndex][j].match(patches[pIndex][j-1]))
                            continue;
                        pesVec2f mid = patches[pIndex][j].getMiddle(patches[pIndex][j+1]);
                        auto last = fillBlocks[blockIndex].polyline.getVertices().back();
                        if(last.distance(mid)>=runpitch)
                            fillBlocks[blockIndex].addStitch(mid);
                        
                        satin.push_back(patches[pIndex][j]);
                    }
                    satin.push_back(patches[pIndex].getVertices().back());
                    
                    // do satin zigzag for remain segments from lastIndex back to exitIndex
                    // by reverse satin list
                    std::reverse(satin.begin(), satin.end());
                    
                    // then add to stitchBlocks
                    int numsatin = (int)satin.size();
                    for(int j=0; j<numsatin; j++){
                        fillBlocks[blockIndex].addStitch(satin[j]);
                    }
                    
                    // fixed
                    pesVec2f exitPos = patches[pIndex][exitIndex];
                    pesVec2f lastPos = fillBlocks[blockIndex].polyline.getVertices().back();
                    if(!exitPos.match(lastPos)){
                        fillBlocks[blockIndex].addStitch(exitPos);
                    }
                }
                
            } // end for(auto rs:results)
        } // end results.size()
    }
    
    recalculate();
}

int pesFindBiggestContourIndex(pesPath *p) {
    int numOutline = (int)p->getOutline().size();
    int biggestIndex = -1;
    float biggestArea = 0;
    // find the biggest contour
    for(int i=0; i<numOutline; i++){
        float absoluteArea = fabs(p->getOutline()[i].getArea());
        if( absoluteArea > biggestArea){
            biggestIndex = i;
            biggestArea = absoluteArea;
        }
    }
    return biggestIndex;
}

int pesFindBiggestContourIndex(const vector<pesPolyline> & contours) {
    int numOutline = (int)contours.size();
    int biggestIndex = -1;
    float biggestArea = 0;
    // find the biggest contour
    for(int i=0; i<numOutline; i++){
        float absoluteArea = fabs(contours[i].getArea());
        if( absoluteArea > biggestArea){
            biggestIndex = i;
            biggestArea = absoluteArea;
        }
    }
    return biggestIndex;
}

void pesData::applyStroke(){
    strokeBlocks.clear();
    int numBlock =  (int)paths.size();
    int blockIndex = 0;
    //int colorIndex = parameter.colorIndex;
    
    strokeBlocks.resize(numBlock);
    for (const auto& p : paths) {
        //if (p.bVisible && p.isStroke()) {
            strokeBlocks[blockIndex].clear();
            strokeBlocks[blockIndex++].setNearestColor(p.getStrokeColor());
            //strokeBlocks[blockIndex++].setColorFromIndex(pesGetNearestBrotherColorHSLfIndex(p.getStrokeColor()));
            //strokeBlocks[blockIndex++].setColorFromIndex(pesGetNearestBrotherColorRGBfIndex(p.getStrokeColor()));
        //}
    }
    strokeBlocks.resize(blockIndex);
    blockIndex = 0;
    const float pes_one_mm = 10.0;
    
    if(parameter.strokeType==StrokeType::STROKE_TYPE_NONE){
        // do nothing?
    }
    else if((int)parameter.strokeType==StrokeType::STROKE_TYPE_DYNAMIC) {
        const float oneStitch = parameter.strokeRunPitch * pes_one_mm;
        int nRound = 1;
        bool beanStitch = false;
        
        vector<pesPolyline> stroke;
        int prev_colorIndex = strokeBlocks[blockIndex].colorIndex;
        float lastW = 0;
        if( paths.size() ) {
            // scales, not only x-axis
            lastW = abs(paths[0].getStrokeWidth()) * std::min(abs(parameter.ppefScaleX), abs(parameter.ppefScaleY));
        }

        int strokeBlockIndex = blockIndex;
        
        float zigzagWidth = lastW;
        float zigzagSpacing = pes_one_mm / parameter.strokeDensity;   

        // Get all of end points
        vector<pesVec2f>endPoints;
        for(size_t i=0; i<paths.size(); i++){
            if( paths[i].bVisible && paths[i].isStroke() ) {
                auto & outlines = paths[i].getOutline();
                for( auto outline: outlines ) {
                    auto vs = outline.getVertices();
                    if( vs.size() < 3 ) {
                        continue;
                    }
                    endPoints.push_back( vs[0] );
                    endPoints.push_back( vs[vs.size()-1] );
                }
            }
        }

        vector<vector<pesPolyline>> stroke_stack;
        vector<int> color_stack;
        vector<float> width_stack;
        vector<bool> gen_stack;
        vector<int> blockindex_stack;
        
        // Group stroke outline by stroke color
        for(size_t i=0; i<paths.size(); i++){
            if( paths[i].bVisible ) {
                int cur_colorIndex = strokeBlocks[blockIndex].colorIndex;
                // scales, not only x-axis
                float w = abs(paths[i].getStrokeWidth()) * std::min(abs(parameter.ppefScaleX), abs(parameter.ppefScaleY)); 
                pesColor c = paths[i].getStrokeColor();

                if(paths[i].isStroke()){
                    // Create last stroke
                    if(cur_colorIndex != prev_colorIndex || lastW != w){
                        //SkDebugf( "StrokeWidth: %i %0.2f\n", (int)i, lastW );
                        stroke_stack.push_back( stroke );
                        color_stack.push_back( prev_colorIndex );
                        blockindex_stack.push_back( strokeBlockIndex );
                        width_stack.push_back( lastW );
                        gen_stack.push_back( false );

                        stroke.clear();
                        prev_colorIndex = cur_colorIndex;
                        lastW = w;

                        strokeBlockIndex = blockIndex;
                    }

                    // Process new stroke
                    auto & outline = paths[i].getOutline();
                    std::string sgroup = paths[i].group_id;

                    // Not cuttable, just insert ?
                    if( paths[i].roleType != 1 && paths[i].roleType != 3 ) { // not cuttable
                        stroke.insert(stroke.end(), outline.begin(), outline.end());
                    }
                    else {
                        //SkDebugf( "To cut line: %s\n", sgroup.c_str() );
                        std::vector<pesPolyline> new_outline;

                        for( pesPolyline polyline : outline ) {
                            std::vector<pesVec2f>vertices = polyline.getVertices();
                            std::vector<bool>b;
                            b.resize(polyline.getVertices().size());
                            for( int bi=0; bi<b.size(); bi++ ) {
                                b[bi] = 1;
                            }

                            // Cut by upper cutters
                            for( int u=i+1; u<paths.size(); u++ ) {
                                pesPath upath = paths[u];
                                if( upath.bVisible == false ) {
                                    continue;
                                }
                                if( upath.group_id == sgroup ) {
                                    continue;
                                }
                                if( upath.roleType != 2 && upath.roleType != 3 ) {
                                    continue;
                                }
                                if( upath.isFill() == false ) {
                                    //continue;
                                    SkPath _upath = toSk( upath );
                                    _upath.setFillType( SkPathFillType::kEvenOdd );
                                    _upath = SkiaPathStroke( _upath, upath.getStrokeWidth()/2 ); // Jimmy - 0.9973 (/2)
                                    upath = toPes( _upath );
                                }
                                else {
                                    if( paths[i].roleOffset < 0 ) {
                                        upath = PathInset( upath, 0-paths[i].roleOffset/2 ); //
                                    }
                                    else if( paths[i].roleOffset > 0 ) {
                                        upath = PathOutset( upath, paths[i].roleOffset/2 ); //
                                    }
                                }

                                SkPath skp = toSk( upath );
                                skp.setFillType( SkPathFillType::kEvenOdd );
                                for( int vi=0; vi<vertices.size(); vi++ ) {
                                    pesVec2f v = vertices[vi];
                                    if( skp.contains( v.x, v.y ) ) {
                                        b[vi] = 0;
                                    }
                                }
                            }
                            // Generate cutted line
                            pesPolyline _outline;
                            for( int bi=0; bi<b.size(); bi++ ) {
                                if( b[bi] ) {
                                    if( bi > 0 && !b[bi-1] ) {
                                        // ออกจาก cut area
                                        _outline.addVertex( (vertices[bi-1] + vertices[bi]) / 2 ); // Jimmy - 0.9973 Jeab
                                    }
                                    _outline.addVertex( vertices[bi] );
                                }
                                else {
                                    if( bi > 0 && b[bi-1] ) {
                                        // กำลังจะเข้า cut area
                                        _outline.addVertex( (vertices[bi] + vertices[bi-1]) / 2 ); // Jimmy - 0.9973 Jeab
                                        //_outline.addVertex( vertices[bi] );
                                    }
                                    if( _outline.getVertices().size() > 2 ) {
                                        new_outline.push_back( pesPolyline(_outline) );
                                    }
                                    _outline.clear();
                                }
                            }
                            if( _outline.getVertices().size() > 2 ) {
                                new_outline.push_back( pesPolyline(_outline) );
                            }
                            _outline.clear();
                        }

                        if( new_outline.size() ) {
                            stroke.insert(stroke.end(), new_outline.begin(), new_outline.end());
                        }

                        vector<pesPolyline> stroke2;
                        // stroke is ready.
                        for( int _st=0; _st < stroke.size(); _st++ ) {
                            pesPolyline _stroke = stroke[_st];

                            vector<pesVec2f>&vs = _stroke.getVertices();
                            vector<pesVec2f>vs2;

                            for( int _vt=0; _vt < vs.size(); _vt++ ) { // start at 2
                                pesVec2f _v = vs[_vt];
                                vs2.push_back( _v );

                                for( int _e=0; _e<endPoints.size(); _e++ ) {
                                    if( _v.distance(endPoints[_e]) < 5 ) {
                                        if( vs2.size() > 5 ) {
                                            stroke2.push_back( vs2 );
                                            vs2.clear();
                                            vs2.push_back( _v );
                                        }
                                    }
                                }
                            }
                            if( vs2.size() > 1 ) {
                                stroke2.push_back( vs2 );
                                vs2.clear();
                            }
                        }
                        stroke = stroke2;
                    }
                }
            }

            blockIndex++;
        }
        if(stroke.size()){
            stroke_stack.push_back( stroke );
            color_stack.push_back( prev_colorIndex );
            blockindex_stack.push_back( strokeBlockIndex );
            width_stack.push_back( lastW );
            gen_stack.push_back( false );
        }

        for( int _s=0; _s<stroke_stack.size(); _s++ ) {
            if( gen_stack[_s] == false ) {
                float w = width_stack[_s];
                int c = color_stack[_s];

                vector<pesPolyline> _stroke = stroke_stack[_s];
                vector<float> z;
                for( int zi=0; zi<stroke_stack[_s].size(); zi++ ) {
                    z.push_back( width_stack[_s] );
                }
                
                for( int _ss=_s+1; _ss<stroke_stack.size(); _ss++ ) {
                    if( gen_stack[_ss] == false ) {
                        if( c == color_stack[_ss] && w == width_stack[_ss] ) {
                            _stroke.insert( _stroke.end(), stroke_stack[_ss].begin(), stroke_stack[_ss].end() );
                            for( int zi=0; zi<stroke_stack[_ss].size(); zi++ ) {
                                z.push_back( width_stack[_ss] );
                            }
                            gen_stack[_ss] = true;
                        }
                    }
                }
                if( w >= 4 ) {
                    zigzagWidth = w;
                    //SkDebugf( "--- Satin: %0.2f\n", zigzagWidth );
                    doStrokeSatin(_stroke, z, 0, zigzagSpacing, blockindex_stack[_s]);
                    z.clear();
                }
                else {
                    nRound = (int)w;
                    if( nRound == 0 ) {
                        nRound = 1;
                    } 
                    if( nRound > 2 ) {
                        nRound = 2;
                    }               
                    //SkDebugf( "--- Runing: %i\n", nRound );
                    doStrokeRunning(_stroke, oneStitch, blockindex_stack[_s], nRound, false);
                }
                gen_stack[_s] = true;
            }
        }        
    }
    else if(parameter.strokeType>=StrokeType::STROKE_TYPE_RUNNING && parameter.strokeType<=StrokeType::STROKE_TYPE_RUNNING_BEAN){
        const float oneStitch = parameter.strokeRunPitch * pes_one_mm;
        int nRound = 1;
        bool beanStitch = false;
        if(parameter.strokeType==StrokeType::STROKE_TYPE_RUNNING_DOUBLE){
            nRound = 2;
        }
        else if(parameter.strokeType==StrokeType::STROKE_TYPE_RUNNING_TRIPLE){
            nRound = 3;
        }
        else if(parameter.strokeType==StrokeType::STROKE_TYPE_RUNNING_BEAN){
            beanStitch = true;
        }
        
        vector<pesPolyline> stroke;
        int prev_colorIndex = strokeBlocks[blockIndex].colorIndex;
        int strokeBlockIndex = blockIndex;
        
        // Group stroke outline by stroke color
        for(size_t i=0; i<paths.size(); i++){
            int cur_colorIndex = strokeBlocks[blockIndex].colorIndex;
            if(paths[i].isStroke()){
                if(cur_colorIndex!=prev_colorIndex){
                    doStrokeRunning(stroke, oneStitch, strokeBlockIndex, nRound, beanStitch);
                    stroke.clear();
                    prev_colorIndex = cur_colorIndex;
                    strokeBlockIndex = blockIndex;
                }
                auto & outline = paths[i].getOutline();
                stroke.insert(stroke.end(), outline.begin(), outline.end());
            }
            blockIndex++;
        }
        if(stroke.size()){
            doStrokeRunning(stroke, oneStitch, strokeBlockIndex, nRound, beanStitch);
        }
    }
    else if(parameter.strokeType==StrokeType::STROKE_TYPE_SATIN){
        float zigzagWidth = parameter.strokeWidth * pes_one_mm;
        float zigzagSpacing = pes_one_mm / parameter.strokeDensity;
        
        vector<pesPolyline> stroke;
        vector<float> z;
        int prev_colorIndex = strokeBlocks[blockIndex].colorIndex;
        int strokeBlockIndex = blockIndex;
        for(size_t i=0; i<paths.size(); i++){
            int cur_colorIndex = strokeBlocks[blockIndex].colorIndex;
            if(paths[i].isStroke()){
                if(cur_colorIndex!=prev_colorIndex){
                    doStrokeSatin(stroke, z, 0, zigzagSpacing, strokeBlockIndex);
                    
                    stroke.clear();
                    z.clear();
                    prev_colorIndex = cur_colorIndex;
                    strokeBlockIndex = blockIndex;
                }
                auto & outline = paths[i].getOutline();
                stroke.insert(stroke.end(), outline.begin(), outline.end());
                for( int zi=0; zi<outline.size(); zi++ ) {
                    z.push_back( zigzagWidth );
                }
            }
            blockIndex++;
        }
        if(stroke.size()){
            doStrokeSatin(stroke, z, 0, zigzagSpacing, strokeBlockIndex);
        }
    }
    else if(parameter.strokeType==StrokeType::STROKE_TYPE_SATIN_OUTER || parameter.strokeType==StrokeType::STROKE_TYPE_SATIN_INNER){
        float zigzagWidth = parameter.strokeWidth * pes_one_mm;
        float zigzagSpacing = pes_one_mm / parameter.strokeDensity;
        float inset = 0;
        if(parameter.strokeType==StrokeType::STROKE_TYPE_SATIN_OUTER){
            inset = zigzagWidth*0.5;
        }
        else if(parameter.strokeType==StrokeType::STROKE_TYPE_SATIN_INNER){
            inset = -zigzagWidth*0.5;
        }
        
        vector<pesPolyline> stroke;
        vector<float> z;
        int prev_colorIndex = strokeBlocks[blockIndex].colorIndex;
        int strokeBlockIndex = blockIndex;
        for(size_t i=0; i<paths.size(); i++){
            int cur_colorIndex = strokeBlocks[blockIndex].colorIndex;
            if(paths[i].isStroke()){
                if(cur_colorIndex!=prev_colorIndex){
                    // find path direction(cw or ccw)
                    int biggestIndex = pesFindBiggestContourIndex(stroke);
                    bool first_outer_is_ccw = false;
                    if(biggestIndex!=-1){
                        first_outer_is_ccw = stroke[biggestIndex].isCounterClockWise();
                    }
                    //float fInset = first_outer_is_ccw ? -inset : inset;
                    
                    doStrokeSatin(stroke, z, inset, zigzagSpacing, strokeBlockIndex);
                    
                    stroke.clear();
                    z.clear();
                    prev_colorIndex = cur_colorIndex;
                    strokeBlockIndex = blockIndex;
                }
                auto & outline = paths[i].getOutline();
                stroke.insert(stroke.end(), outline.begin(), outline.end());
                for( int zi=0; zi<outline.size(); zi++ ) {
                    z.push_back( zigzagWidth );
                }                
            }
            blockIndex++;
        }
        if(stroke.size()){
            // find path direction(cw or ccw)
            int biggestIndex = pesFindBiggestContourIndex(stroke);
            bool first_outer_is_ccw = false;
            if(biggestIndex!=-1){
                first_outer_is_ccw = stroke[biggestIndex].isCounterClockWise();
            }
            //float fInset = first_outer_is_ccw ? -inset : inset;
            doStrokeSatin(stroke, z, inset, zigzagSpacing, strokeBlockIndex);
        }
    }
    else if(parameter.strokeType==StrokeType::STROKE_TYPE_MOTIF){
        float size = parameter.motif.size * pes_one_mm;
        float space = parameter.motif.spacing * pes_one_mm;
        float pitch = parameter.motif.runPitch * pes_one_mm;
        float oneMotif = size+space;
        
        bool __motif_align_vcenter = true;
//        std::string filepath = guiVAArgsToString("Pattern/%s.pmf", parameter.motif.patternName.c_str());
//        GUI_Path pattPath = LoadPattern(filepath);
        
        // TODO: need load patternBuffer from outside pesEngine
        pesPath pattPath = pesLoadPMFFromBuffer(parameter.motif.patternBuffer);
        if(pattPath.getCommands().size()<2){
            pattPath = CreateDefaultMotif();
        }
        const float originalSize = 1024;
        float realSize = size;
        float scale = realSize/originalSize;
        pattPath.scale(scale, scale);
        pesVec2f right(1,0);
        
        for(auto path : paths){
//            strokeBlocks[blockIndex].clear();
//            pesColor col = path->getStrokeColor();
//            strokeBlocks[blockIndex].setNearestColor(col);
            
            if(path.isStroke()){
                int numOutline = (int)path.getOutline().size();
                int biggestIndex = -1;
                float biggestArea = 0;
                // find the biggest contour
                for(int i=0; i<numOutline; i++){
                    float absoluteArea = fabs(path.getOutline()[i].getArea());
                    if( absoluteArea > biggestArea){
                        biggestIndex = i;
                        biggestArea = absoluteArea;
                    }
                }
                
                bool first_outer_is_ccw = false;
                if(biggestIndex!=-1){
                    float area = path.getOutline()[biggestIndex].getArea();
                    if(area<0.0){
                        // if area is negative path is counter clock wise
                        first_outer_is_ccw = true;
                    }
                }
                
                for(auto outline : path.getOutline()){
                    float l = outline.getPerimeter();
                    int numMotif = (int)(l/oneMotif);
                    pesPolyline resample = outline.getResampledByCount2(numMotif);
                    if(first_outer_is_ccw){
                        std::reverse(resample.begin(), resample.end());
                        resample.flagHasChanged();
                    }
                    
                    if(outline.isClosed()){
                        resample.close();
                    }
                    
                    //                patches.push_back(doMotif(resample, pitch));
                    
                    pesVec2f p0 = resample[0];
                    int loopCount = resample.isClosed() ? resample.size()+1 : resample.size();
                    for(int i=1; i<loopCount; i++){
                        int wrapedIndex = resample.getWrappedIndex(i);
                        pesVec2f p1 = resample[wrapedIndex];
                        float seg_len = p1.distance(p0);
                        if (seg_len == 0.0)
                            continue;
                        
                        pesVec2f along = (p1-p0).normalize();
                        float angle = right.angle(along);
                        pesPath motif = pattPath;
                        motif.rotate(angle);
                        
                        pesVec2f ms = motif.getOutline().front().getVertices().front();
                        pesVec2f d = p0 - ms;
                        if(__motif_align_vcenter){
                            pesVec2f start(0, size/2);
                            start.rotate(angle);
                            d = p0 - start;
                        }
                        motif.translate(d.x, d.y);
                        
                        auto vertices = motif.getOutline().front().getVertices();
                        for(int j=0; j<(int)vertices.size(); j++){
                            pesVec2f s(vertices[j]);
                            int type = NORMAL_STITCH;
                            if(j==0 && i==1){
                                type = JUMP_STITCH;
                            }
                            strokeBlocks[blockIndex].addStitch(s, type);
                        }
                        pesVec2f last = vertices.back();
                        float dist = last.distance(p1);
                        if(dist>pitch){
                            along = (p1-last).normalize();
                            float run_len = pitch;
                            while (run_len<dist) {
                                pesVec2f stitch = p0 + along * run_len;
                                pesVec2f s(stitch);
                                strokeBlocks[blockIndex].addStitch(s);
                                run_len+=pitch;
                            }
                        }
                        
                        p0 = p1;
                    }
                    
                    if(strokeBlocks[blockIndex].polyline.size()){
                        pesVec2f last(strokeBlocks[blockIndex].polyline.getVertices().back());
                        if(p0.distance(last)>0.1* pes_one_mm){
                            pesVec2f s(p0);
                            strokeBlocks[blockIndex].addStitch(s);
                        }
                    }
                }
            }
            blockIndex++;
        } // end for(auto path : paths))
        
        pattPath.clear();
    }

    bool isOverideColors = false;
    if (parameter.text == "Avatar" || parameter.text == "Tag") {
        isOverideColors = parameter.useColorFromPicker;
    } 
    else {
        isOverideColors = parameter.colorIndex != 65;
    }

    if (isOverideColors) {
        for (auto& block : strokeBlocks) {
            block.setColorFromIndex(parameter.colorIndex);
        }
    }
    
    recalculate();
}

void pesApplyStrokeToPatches(vector<pesPolyline> strokePoly, vector<pesPolyline> & patches, float onePitch){
    if(patches.size())
        patches.clear();
    
    for(auto & o:strokePoly){
        float perimeter = fabs(o.getPerimeter());
        if(perimeter>0)
        {
            int numStitch = (int)ceil(perimeter/onePitch);
            pesPolyline rs = o.getResampledByCount2(numStitch);
            patches.push_back(rs);
        }
    }
}

float pesGetPerimeterDistance(const pesPolyline & contour, int index1, int index2){
    float fpos_p1 = contour.getLengthAtIndex(index1);
    float fpos_p2 = contour.getLengthAtIndex(index2);
    float distance = fpos_p2 - fpos_p1;
    float outline_length = contour.getPerimeter();
    
    if (fabs(distance) > outline_length / 2.0 && contour.isClosed()){
        //    # if we'd have to go more than halfway around, it's faster to go
        //    # the other way
        if (distance < 0)
            return distance + outline_length;
        else if (distance > 0)
            return distance - outline_length;
        else
            //    # this ought not happen, but just for completeness, return 0 if
            //    # p1 and p0 are the same point
            return 0;
    }
    else
        return distance;
}

void pesData::doStrokeRunning(const vector<pesPolyline> &stroke, float oneStitch, int strokeBlockIndex, int nRound, bool doBean){
    nRound = CLAMP(nRound, 1, 3);
    
    const float min_dist = 2; // unicorn
    vector<pesPolyline> patches;
    bool bClipperOffset = parameter.strokeRunningInset>=0.05 || parameter.strokeRunningInset<=-0.05;
    if(bClipperOffset==false){
        pesApplyStrokeToPatches(stroke, patches, oneStitch);
    }
    else{
        vector<pesPolyline> closedContour = stroke;
        pesRemove(closedContour, [](pesPolyline & p)->bool{
            return p.isClosed()==false;
        });
        
        const float pes_one_mm = 10.0;
        const float compensate = parameter.strokeRunningInset * pes_one_mm;
        vector<pesPolyline> offsets = pesClipper::getOffsets(closedContour, compensate, ClipperLib::JoinType(parameter.strokeJoinType), ClipperLib::etClosedPolygon);
        if(parameter.isTTFPath){
            // sort left to right
            pesVec2f pmin(bound.getMinX(), bound.getMinY());
            pesSort(offsets, [=](pesPolyline & a, pesPolyline & b)->bool{
                if(pmin.distance(a.getBoundingBox().getPosition()) < pmin.distance(b.getBoundingBox().getPosition())) return true;
                else return false;
            });
        }
        pesApplyStrokeToPatches(offsets, patches, oneStitch);
    }
    
    bool bKeepOrder = parameter.isTTFPath;
    if(patches.size()){
        vector<pesNearestBranchingResult> results;
        pesFindNearestEnterAndExit(patches, results, bKeepOrder);
        if(results.size()){
            for(auto rs:results){
                int enterIndex = rs.enterIndex;
                int exitIndex = rs.exitIndex;
                int pIndex = rs.patchesIndex;
                
                strokeBlocks[strokeBlockIndex].addJump(patches[pIndex][enterIndex]);
                bool isClosed = patches[pIndex].isClosed();
                if(isClosed){
                    int direction = -1;
                    float distance = pesGetPerimeterDistance(patches[pIndex], enterIndex, exitIndex);
                    if(enterIndex!=exitIndex && distance!=0){
                        // walk from enter point to exit point
                        if(distance!=0){
                            direction = (int)copysign(1.0f, distance);
                            int curIndex = enterIndex;
                            while(curIndex!=exitIndex){
                                curIndex = patches[pIndex].getWrappedIndex(curIndex+direction);
                                pesVec2f next = patches[pIndex][curIndex];
                                pesVec2f last = strokeBlocks[strokeBlockIndex].polyline.getVertices().back();
                                if(last.distance(next)>=min_dist) {
                                    strokeBlocks[strokeBlockIndex].addStitch(next);
                                }
                            }
                        }
                    }
                    
                    // do running stitch & bean stitch
                    int num = (int)patches[pIndex].size();
                    direction*=-1;
                    for(int i=0; i<=num; i++){
                        int curIndex = patches[pIndex].getWrappedIndex(exitIndex + direction*i);
                        //strokeBlocks[strokeBlockIndex].addStitch( patches[pIndex][curIndex]);
                        pesVec2f next = patches[pIndex][curIndex];
                        pesVec2f last = strokeBlocks[strokeBlockIndex].polyline.getVertices().back();
                        if(last.distance(next)>=min_dist) {
                            strokeBlocks[strokeBlockIndex].addStitch(next);
                        }                        
                        if(i>0 && doBean){
                            int prevIndex = patches[pIndex].getWrappedIndex(exitIndex + direction*(i-1));
                            //strokeBlocks[strokeBlockIndex].addStitch( patches[pIndex][prevIndex]);
                            pesVec2f next = patches[pIndex][prevIndex];
                            pesVec2f last = strokeBlocks[strokeBlockIndex].polyline.getVertices().back();
                            if(last.distance(next)>=min_dist) {
                                strokeBlocks[strokeBlockIndex].addStitch(next);
                            }                              
                            //strokeBlocks[strokeBlockIndex].addStitch( patches[pIndex][curIndex]);
                            next = patches[pIndex][curIndex];
                            last = strokeBlocks[strokeBlockIndex].polyline.getVertices().back();
                            if(last.distance(next)>=min_dist) {
                                strokeBlocks[strokeBlockIndex].addStitch(next);
                            }                              
                        }
                    }
                    
                    // do running double & triple stitch
                    for(int round=1; round<nRound; round++){
                        direction*=-1;
                        patches[pIndex].grow(-1);
                        for(int i=0; i<=num; i++){
                            int curIndex = patches[pIndex].getWrappedIndex(exitIndex + direction*i);                            
                            //strokeBlocks[strokeBlockIndex].addStitch( patches[pIndex][curIndex]);
                            pesVec2f next = patches[pIndex][curIndex];
                            pesVec2f last = strokeBlocks[strokeBlockIndex].polyline.getVertices().back();
                            if(last.distance(next)>=min_dist) {
                                strokeBlocks[strokeBlockIndex].addStitch(next);
                            }                               
                        }
                    }
                }
                else{
                    if(enterIndex>0){
                        // walk-back to [0](firstIndex)
                        for(int j=enterIndex; j>=1; j--){
                            pesVec2f next = patches[pIndex][j-1];
                            pesVec2f last = strokeBlocks[strokeBlockIndex].polyline.getVertices().back();
                            if(last.distance(next)>=min_dist) {
                                strokeBlocks[strokeBlockIndex].addStitch(next);
                            }
                        }
                    }
                    
                    if(nRound>1){
                        int size = (int)patches[pIndex].size();
                        for(int j=1; j<size; j++){
                            //strokeBlocks[strokeBlockIndex].addStitch( patches[pIndex][j]);
                            pesVec2f next = patches[pIndex][j];
                            pesVec2f last = strokeBlocks[strokeBlockIndex].polyline.getVertices().back();
                            if(last.distance(next)>=min_dist) {
                                strokeBlocks[strokeBlockIndex].addStitch(next);
                            }                              
                        }
                        patches[pIndex].grow(-1);
                        
                        if(nRound==2){
                            for(auto it=patches[pIndex].rbegin(); it!=patches[pIndex].rend(); it++){
                                //strokeBlocks[strokeBlockIndex].addStitch(*it);
                                pesVec2f next = *it;
                                pesVec2f last = strokeBlocks[strokeBlockIndex].polyline.getVertices().back();
                                if(last.distance(next)>=min_dist) {
                                    strokeBlocks[strokeBlockIndex].addStitch(next);
                                }                                  
                            }
                            
                            for(int j=1; j<=exitIndex; j++){
                                //strokeBlocks[strokeBlockIndex].addStitch( patches[pIndex][j]);
                                pesVec2f next = patches[pIndex][j];
                                pesVec2f last = strokeBlocks[strokeBlockIndex].polyline.getVertices().back();
                                if(last.distance(next)>=min_dist) {
                                    strokeBlocks[strokeBlockIndex].addStitch(next);
                                }                                 
                            }
                            
                            continue;
                        }
                    }
                    
                    if(nRound>2){
                        for(auto it=patches[pIndex].rbegin(); it!=patches[pIndex].rend(); it++){
                            //strokeBlocks[strokeBlockIndex].addStitch(*it);
                            pesVec2f next = *it;
                            pesVec2f last = strokeBlocks[strokeBlockIndex].polyline.getVertices().back();
                            if(last.distance(next)>=min_dist) {
                                strokeBlocks[strokeBlockIndex].addStitch(next);
                            }                             
                        }
                        patches[pIndex].grow(-1);
                    }
                    
                    // do running stitch for every segments from firstIndex to exitIndex
                    //strokeBlocks[strokeBlockIndex].addStitch(patches[pIndex][0]);
                    pesVec2f next = patches[pIndex][0];
                    pesVec2f last = strokeBlocks[strokeBlockIndex].polyline.getVertices().back();
                    if(last.distance(next)>=min_dist) {
                        strokeBlocks[strokeBlockIndex].addStitch(next);
                    }                     
                    int cnt = (int)patches[pIndex].size();
                    for(int j=1; j<=exitIndex; j++){
                        //strokeBlocks[strokeBlockIndex].addStitch( patches[pIndex][j]);
                        pesVec2f next = patches[pIndex][j];
                        pesVec2f last = strokeBlocks[strokeBlockIndex].polyline.getVertices().back();
                        if(last.distance(next)>=min_dist) {
                            strokeBlocks[strokeBlockIndex].addStitch(next);
                        }                         
                        if(doBean){
                            //strokeBlocks[strokeBlockIndex].addStitch( patches[pIndex][j-1]);
                            pesVec2f next = patches[pIndex][j-1];
                            pesVec2f last = strokeBlocks[strokeBlockIndex].polyline.getVertices().back();
                            if(last.distance(next)>=min_dist) {
                                strokeBlocks[strokeBlockIndex].addStitch(next);
                            }                             
                            strokeBlocks[strokeBlockIndex].addStitch( patches[pIndex][j]);
                            next = patches[pIndex][j];
                            last = strokeBlocks[strokeBlockIndex].polyline.getVertices().back();
                            if(last.distance(next)>=min_dist) {
                                strokeBlocks[strokeBlockIndex].addStitch(next);
                            }                             
                        }
                    }
                    
                    // if exitIndex < lastIndex
                    if(exitIndex<cnt-1){
                        vector<pesVec2f> tmpRunning;
                        // walk-forward from exitIndex to lastIndex
                        for(int j=exitIndex; j<cnt-1; j++){
                            pesVec2f next = patches[pIndex][j+1];
                            pesVec2f last = strokeBlocks[strokeBlockIndex].polyline.getVertices().back();
                            if(last.distance(next)>=min_dist) {
                                strokeBlocks[strokeBlockIndex].addStitch(next);
                            }
                            
                            tmpRunning.push_back(patches[pIndex][j]);
                        }
                        tmpRunning.push_back(patches[pIndex].getVertices().back());
                        
                        // do running for remain segments from lastIndex back to exitIndex
                        // by reverse list
                        std::reverse(tmpRunning.begin(), tmpRunning.end());
                        
                        // then add to stitchBlocks
                        int nums = (int)tmpRunning.size();
                        for(int j=0; j<nums; j++){
                            //strokeBlocks[strokeBlockIndex].addStitch(tmpRunning[j]);
                            pesVec2f next = tmpRunning[j];
                            pesVec2f last = strokeBlocks[strokeBlockIndex].polyline.getVertices().back();
                            if(last.distance(next)>=min_dist) {
                                strokeBlocks[strokeBlockIndex].addStitch(next);
                            }                            
                            if(doBean){
                                if(j>0){
                                    //strokeBlocks[strokeBlockIndex].addStitch(tmpRunning[j-1]);
                                    pesVec2f next = tmpRunning[j-1];
                                    pesVec2f last = strokeBlocks[strokeBlockIndex].polyline.getVertices().back();
                                    if(last.distance(next)>=min_dist) {
                                        strokeBlocks[strokeBlockIndex].addStitch(next);
                                    }                                      
                                    strokeBlocks[strokeBlockIndex].addStitch(tmpRunning[j]);
                                    next = tmpRunning[j];
                                    last = strokeBlocks[strokeBlockIndex].polyline.getVertices().back();
                                    if(last.distance(next)>=min_dist) {
                                        strokeBlocks[strokeBlockIndex].addStitch(next);
                                    }                                      
                                }
                            }
                        }
                    }  
                }
            }
        }
    }
}

void pesData::doStrokeSatin(vector<pesPolyline> stroke, const std::vector<float>  strokeWidth, float inset, float spacing, int blockIndex){
    const float runpitch = 1.5 * 10;
    const float min_dist = runpitch;

    if(inset!=0){
        pesRemove(stroke, [](pesPolyline & p)->bool{
            return p.isClosed()==false;
        });
        stroke = pesClipper::getOffsets(stroke, inset, ClipperLib::jtSquare, ClipperLib::etClosedPolygon);
    }
    
    vector<pesPolyline> patches;
    for(int _st=0; _st<stroke.size(); _st++ ){
        pesPolyline &outline = stroke[_st];
        if( outline.getPerimeter() < 5 ) {
            continue;
        }
        pesSatinOutline result;
        pesSatinOutlineGenerate(outline, strokeWidth[_st], 0, result);
        auto stitches = pesSatinOutlineRenderStitches(result, spacing);
        patches.emplace_back();
        patches.back().addVertices(stitches);
        if(outline.isClosed()){
            patches.back().setClosed(true);
        }
    }
    
    bool bKeepOrder = parameter.isTTFPath;
    if(patches.size()){
        vector<pesNearestBranchingResult> results;
        // pesFindNearestEnterAndExit(patches, results, parameter.strokeType, bKeepOrder); // Jimmy - I think it's wrong params
        pesFindNearestEnterAndExit(patches, results, bKeepOrder);

        if(results.size()){
            for(auto rs:results){
                int enterIndex = rs.enterIndex;
                int exitIndex = rs.exitIndex;
                int pIndex = rs.patchesIndex;
                
                // first point
                strokeBlocks[blockIndex].addJump(patches[pIndex][enterIndex]);
                bool isClosed = patches[pIndex].isClosed();
                if(isClosed){
                    int direction = -1;
                    float distance = pesGetPerimeterDistance(patches[pIndex], enterIndex, exitIndex);
                    if(enterIndex!=exitIndex && distance!=0){
                        // walk from enter point to exit point
                        direction = (int)copysign(1.0f, distance);
                        int curIndex = enterIndex;
                        while(curIndex!=exitIndex){
                            int prevIndex = curIndex;
                            curIndex = patches[pIndex].getWrappedIndex(curIndex+direction);
                            pesVec2f mid = patches[pIndex][curIndex].getMiddle(patches[pIndex][prevIndex]);
                            pesVec2f last = strokeBlocks[blockIndex].polyline.getVertices().back();
                            if(last.distance(mid)>=min_dist)
                                strokeBlocks[blockIndex].addStitch(mid);
                        }
                    }
                    
                    // do satin zigzag
                    int num = (int)patches[pIndex].size();
                    direction*=-1;
                    for(int i=0; i<=num; i++){
                        int curIndex = patches[pIndex].getWrappedIndex(exitIndex + direction*i);
                        strokeBlocks[blockIndex].addStitch( patches[pIndex][curIndex]);
                    }
                }
                else{
                    if(enterIndex>0){
                        // walk-back to [0](firstIndex)
                        for(int j=enterIndex; j>=1; j--){
                            pesVec2f mid = patches[pIndex][j].getMiddle(patches[pIndex][j-1]);
                            pesVec2f last = strokeBlocks[blockIndex].polyline.getVertices().back();
                            if(last.distance(mid)>=min_dist)
                                strokeBlocks[blockIndex].addStitch(mid);
                        }
                    }
                    
                    // do satin zigzag for every segments from firstIndex to exitIndex
                    strokeBlocks[blockIndex].addStitch(patches[pIndex][0]);
                    int cnt = (int)patches[pIndex].size();
                    for(int j=1; j<=exitIndex; j++){
                        strokeBlocks[blockIndex].addStitch( patches[pIndex][j]);
                    }
                    
                    // if exitIndex < lastIndex
                    if(exitIndex<cnt-1){
                        vector<pesVec2f> satin;
                        // walk-forward from exitIndex to lastIndex
                        for(int j=exitIndex; j<cnt-1; j++){
                            pesVec2f mid = patches[pIndex][j].getMiddle(patches[pIndex][j+1]);
                            pesVec2f last = strokeBlocks[blockIndex].polyline.getVertices().back();
                            if(last.distance(mid)>=min_dist)
                                strokeBlocks[blockIndex].addStitch(mid);
                            
                            satin.push_back(patches[pIndex][j]);
                        }
                        satin.push_back(patches[pIndex].getVertices().back());
                        
                        // do satin zigzag for remain segments from lastIndex back to exitIndex
                        // by reverse satin list
                        std::reverse(satin.begin(), satin.end());
                        
                        // then add to stitchBlocks
                        int nums = (int)satin.size();
                        for(int j=0; j<nums; j++){
                            strokeBlocks[blockIndex].addStitch(satin[j]);
                        }
                    }
                }// end if( isClosed )
            }
        }
    }
}

void pesData::getStitchBlockList(pesStitchBlockList& blocks, bool bJoinBlocksWithColorIndex) {
    if(blocks.size()){
        blocks.clear();
    }
    if (bJoinBlocksWithColorIndex) {
        int prevColor = -1;
        for (const auto& b : fillBlocks) {
            if (b.size()) {
                int color = b.colorIndex;
                if (color != prevColor) {
                    prevColor = color;
                    blocks.push_back(b);
                } else {
                    auto& back = blocks.back();
                    back.polyline.addVertices(b.polyline.getVertices());
                    back.types.insert(back.types.end(), b.types.begin(), b.types.end());
                }
            }
        }
        for (const auto& b : strokeBlocks) {
            if (b.size()) {
                int color = b.colorIndex;
                if (color != prevColor) {
                    prevColor = color;
                    blocks.push_back(b);
                } else {
                    auto& back = blocks.back();
                    back.polyline.addVertices(b.polyline.getVertices());
                    back.types.insert(back.types.end(), b.types.begin(), b.types.end());
                }
            }
        }
        return;
    } else {
        if (fillBlocks.size()) {
            blocks.insert(blocks.end(), fillBlocks.begin(), fillBlocks.end());
        }
        if (strokeBlocks.size()) {
            blocks.insert(blocks.end(), strokeBlocks.begin(), strokeBlocks.end());
        }
    }
    pesRemove(blocks, [](pesStitchBlock & block){
        return block.size() < 1;
    });
}

unsigned int pesData::getTotalStitches() const{
    return totalStitches;
}

unsigned int pesData::getTotalJump() const{
    return totalJump;
}

//pesRectangle pesData::getBoundingBox() const{
//    return bound;
//}

pesRectangle pesData::getBoundingBox(){
    recalculate();
    const auto ocxy = bound.getCenter();
    //if (SkScalarNearlyZero(bound.width)) {
    if (bound.width > -2.0f && bound.width < 2.0f) {
        if (bound.width < 0.0f) {
            bound.x = ocxy.x + 1.0f;
            bound.width = -2.0f;
        } 
        else {
            bound.x = ocxy.x - 1.0f;
            bound.width = 2.0f;
        }
    }
    //if (SkScalarNearlyZero(bound.height)) {
    if (bound.height > -2.0f && bound.height < 2.0f) {
        if (bound.height < 0.0f) {
            bound.y = ocxy.y + 1.0f;
            bound.height = -2.0f;
        } else {
            bound.y = ocxy.y - 1.0f;
            bound.height = 2.0f;
        }
    }
    return bound;
}

pesRectangle pesData::getStrokeBoundingBox() { 
    recalculate();
    return strokebound;
}

pesRectangle pesData::getRotatedBoundingBox(){
    if(parameter.rotateDegree==0)
        return getBoundingBox();
    
    pesRectangle b = getBoundingBox();
    pesVec2f pivot = b.getCenter();
    pesVec2f p[4];
    p[0] = pesVec2f(b.x, b.y).rotate(parameter.rotateDegree, pivot);
    p[1] = pesVec2f(b.x+b.width, b.y).rotate(parameter.rotateDegree, pivot);
    p[2] = pesVec2f(b.x+b.width, b.y+b.height).rotate(parameter.rotateDegree, pivot);
    p[3] = pesVec2f(b.x, b.y+b.height).rotate(parameter.rotateDegree, pivot);
    
    float minx = FLT_MAX;
    float miny = FLT_MAX;
    float maxx = -FLT_MAX;
    float maxy = -FLT_MAX;
    
    for(int i=0; i<4; i++){
        minx = std::min(minx, p[i].x);
        miny = std::min(miny, p[i].y);
        maxx = std::max(maxx, p[i].x);
        maxy = std::max(maxy, p[i].y);
    }
    return pesRectangle(minx, miny, maxx-minx, maxy-miny);
}

pesBuffer pesData::getSVGBuffer(bool forScanNcut) const {
    //SkDebugf( "getSVGBuffer\n" );
    pesRectangle svgbound = getScalableBoundingBox();
    pesSVG svg;
    /*
    for( int i=0; i<paths.size(); i++ ) {
        pesColor c = paths[i].getStrokeColor();
    }
    */
    svg.addPaths(paths);

    if(forScanNcut){
        svg.setPos(svgbound.getPosition());
        svg.setViewbox(svgbound.getMinX(), svgbound.getMinY(), bound.width, bound.height);
        svg.setSize(svgbound.width/10, svgbound.height/10, "mm");
    }

    return svg.toString();
}

void pesData::orderByColor(){
    std::vector<int> colors;
    //for(auto & block :fillBlocks){ // Jimmy - i reverse order to preserve apperance of upper thread
    for( int i=0; i<fillBlocks.size(); i++ ) { 
    //for( int i=fillBlocks.size()-1; i>=0; i-- ) {
        auto &block = fillBlocks[i];
        bool duplicate = false;
        for(auto c : colors){
            if(block.colorIndex==c){
                duplicate = true;
                break;
            }
        }
        if(!duplicate){
            colors.push_back(block.colorIndex);
        }
    }
    //std::reverse(colors.begin(), colors.end());
    
    int numColor = (int)colors.size();
    pesStitchBlockList ordered_fillBlocks;
    int prevColor = -1;
    for(int i=0; i<numColor; i++){
        for(auto & block : fillBlocks){
            if(colors[i]==block.colorIndex){
                if(ordered_fillBlocks.size() && ordered_fillBlocks.back().colorIndex==colors[i]){
                    ordered_fillBlocks.back().polyline.insert(
                            block.polyline.getVertices(),
                            (int)ordered_fillBlocks.back().polyline.size());
                    ordered_fillBlocks.back().types.insert(
                            ordered_fillBlocks.back().types.end(), block.types.begin(),
                            block.types.end());
                }
                else 
                    ordered_fillBlocks.push_back(block);
            }
        }
    }
    fillBlocks.clear();
    fillBlocks = ordered_fillBlocks;
    recalculate();
    int nums = (int)fillBlocks.size();
}

//pom
int _getNearestBrotherColorIndex(vector<int> colorList, const pesColor& srcColor){
    auto mindist = FLT_MAX;
    int closestIndex = 65;
    auto srcHSLf = srcColor.toHSLf();
    srcHSLf.x = srcHSLf.x;
    srcHSLf.y = srcHSLf.y;
    srcHSLf.z = srcHSLf.z;
    for (int i = 0; i < colorList.size(); i++) {
        auto HSLf = pesPaletteBrotherColorHSLf[colorList[i]];
        HSLf.x = HSLf.x;
        HSLf.y = HSLf.y;
        HSLf.z = HSLf.z;
        float dist = srcHSLf.distance(HSLf);
        if (dist < mindist) {
            mindist = dist;
            closestIndex = colorList[i];
        }
    }
    return closestIndex;
}

// pom
void pesData::reduceColor(int numColor){
    recalculate();
    std::set<int> colorSet;
    for(auto c : colors){
        colorSet.insert(c);
    }
    if(colorSet.size()<=numColor) return;
    
    // sort by stitch count (descending)
    pesStitchBlockList blocks;
    getStitchBlockList(blocks);
    pesSort(blocks, [](pesStitchBlock& p0, pesStitchBlock& p1) -> bool {
        return p0.getTotalStitches() > p1.getTotalStitches();
    });
    
    // color list priority with stitch count
    vector<int> colorList;
    int index = 0;
    do{
        int cc = blocks[index].colorIndex;
        bool duplicate = false;
        for(auto c : colorList){
            if(cc == c){
                duplicate = true;
                break;
            }
        }
        if(!duplicate){
            colorList.push_back(cc);
        }
        index++;
    }while(colorList.size()<numColor && index<blocks.size());
    
    // reduce color
    for(auto & block : fillBlocks){
        bool found = false;
        for(auto c : colorList){
            if(block.colorIndex == c){
                found = true;
                break;
            }
        }
        if(!found){
            int nearestIndex = _getNearestBrotherColorIndex(colorList, block.color);
            block.setColorFromIndex(nearestIndex);
        }
    }
    
    for(auto & block : strokeBlocks){
        bool found = false;
        for(auto c : colorList){
            if(block.colorIndex == c){
                found = true;
                break;
            }
        }
        if(!found){
            int nearestIndex = _getNearestBrotherColorIndex(colorList, block.color);
            block.setColorFromIndex(nearestIndex);
        }
    }
    
//    if(reOrderByColor){ // Do we need to order by color?
//        orderByColor();
//    }
}

struct RGBColor{
    int r, g, b;
};

// Function to calculate Euclidean distance between two colors
double euclideanDistance(const RGBColor& color1, const RGBColor& color2) {
    return sqrt(pow(color1.r - color2.r, 2) + pow(color1.g - color2.g, 2) + pow(color1.b - color2.b, 2));
}

vector<RGBColor> quantizeColors(const vector<RGBColor>& image, int k) {
    // Initialize k random colors as centroids
    vector<RGBColor> centroids(k);
    for (int i = 0; i < k; ++i) {
        centroids[i] = image[rand() % image.size()];
    }

    // Assign each pixel to the nearest centroid
    vector<int> labels(image.size());
    for (size_t i = 0; i < image.size(); ++i) {
        double min_dist = numeric_limits<double>::max();
        int closest_centroid = 0;
        for (int j = 0; j < k; ++j) {
            double dist = euclideanDistance(image[i], centroids[j]);
            if (dist < min_dist) {
                min_dist = dist;
                closest_centroid = j;
            }
        }
        labels[i] = closest_centroid;
    }

    // Update centroids by taking the mean of each cluster
    vector<int> count(k, 0);
    vector<RGBColor> new_centroids(k);
    for (size_t i = 0; i < image.size(); ++i) {
        int cluster = labels[i];
        new_centroids[cluster].r += image[i].r;
        new_centroids[cluster].g += image[i].g;
        new_centroids[cluster].b += image[i].b;
        count[cluster]++;
    }

    for (int i = 0; i < k; ++i) {
        if (count[i] > 0) {
            new_centroids[i].r /= count[i];
            new_centroids[i].g /= count[i];
            new_centroids[i].b /= count[i];
        }
    }

    return new_centroids;
}

void pesData::quantizeColor(int numColor){
    recalculate();
    std::set<int> colorSet;
    for(auto c : colors){
        colorSet.insert(c);
    }
    if(colorSet.size()<=numColor) return;

    // List of original colors
    std::vector<RGBColor> originalColors;
    for(auto c : colorSet){
        pesColor bc = pesGetBrotherColor(c);
        originalColors.push_back({bc.r, bc.g, bc.b});
    }
    
    // Perform color quantization
    int k = numColor;
    vector<RGBColor> quantized_colors = quantizeColors(originalColors, k);
    
    // Brother color index
    vector<int> quantizedColorIndex;
    for(auto c : quantized_colors){
        int nearestIndex = pesGetNearestBrotherColorHSLfIndex(pesColor(c.r, c.g, c.b));
        quantizedColorIndex.push_back(nearestIndex);
    }
    
    // reduce color
    for(auto & block : fillBlocks){
        bool found = false;
        for(auto c : quantizedColorIndex){
            if(block.colorIndex == c){
                found = true;
                break;
            }
        }
        if(!found){
            int nearestIndex = _getNearestBrotherColorIndex(quantizedColorIndex, block.color);
            block.setColorFromIndex(nearestIndex);
        }
    }
    
    for(auto & block : strokeBlocks){
        bool found = false;
        for(auto c : quantizedColorIndex){
            if(block.colorIndex == c){
                found = true;
                break;
            }
        }
        if(!found){
            int nearestIndex = _getNearestBrotherColorIndex(quantizedColorIndex, block.color);
            block.setColorFromIndex(nearestIndex);
        }
    }
}

bool pesData::canFlip(){
    return parameter.type == OBJECT_TYPE_PES;
}

void pesData::verticalFlip(){
    float min = getBoundingBox().getMinY();
    float max = getBoundingBox().getMaxY();
    for(auto & block : fillBlocks){
        auto & vertices = block.polyline.getVertices();
        for(auto & v : vertices){
            v.y = pesMap(v.y, min, max, max, min);
        }
    }
    // if(parameter.rotateDegree>180){
    //     parameter.rotateDegree = 540 - parameter.rotateDegree;
    // }
    // else{
    //     parameter.rotateDegree = 180 - parameter.rotateDegree;
    // }
}

void pesData::horizontalFlip(){
    float min = getBoundingBox().getMinX();
    float max = getBoundingBox().getMaxX();
    for(auto & block : fillBlocks){
        auto & vertices = block.polyline.getVertices();
        for(auto & v : vertices){
            v.x = pesMap(v.x, min, max, max, min);
        }
    }
    // parameter.rotateDegree = 360 - parameter.rotateDegree;
}

void pesData::savePaths(){
    savedPaths.clear();
    savedPaths = paths;
}

void addFirstJump(pesBuffer & stitches, int x, int y){
    unsigned long length1 = (unsigned long)stitches.size();
    stitches.resize(length1 + 4);
    unsigned char *pNewStiches = (unsigned char*)stitches.getData();
    memmove(&pNewStiches[4], pNewStiches, length1);
    
    unsigned long nJumpX = 4;
    unsigned long nJumpY = 6;
    unsigned char val1 = pNewStiches[nJumpX];
    unsigned char val2 = pNewStiches[nJumpX + 1];
    int dx = 0;
    int dy = 0;
    int deltaX = 0;
    int deltaY = 0;
    
    if ((val1 & 0x80) == 0x80) {
        deltaX = ((val1 & 0xF) * 256) + val2;
        if (((int)deltaX & 0x800) == 0x800) {
            deltaX = (int)((int)deltaX | 0xFFFFF000);
        }
        val2 = pNewStiches[nJumpY];
        
        pNewStiches[nJumpX] = ((dx >> 8) & 0x0F) | 0x80;
        pNewStiches[nJumpX + 1] = (dx & 0xFF);
    } else {
        deltaX = (int)val1;
        if (deltaX > 63) {
            deltaX -= 128;
        }
        nJumpY = 1;
        
        if (dx < 0)
            dx += 128;
        pNewStiches[nJumpX] = dx;
    }
    
    if ((val2 & 0x80) == 0x80) {
        int _h = ((val2 & 0xF) * 256);
        int _l = pNewStiches[nJumpY + 1];
        deltaY = _h + _l;
        if (((int)deltaY & 0x800) == 0x800) {
            deltaY = (int)((int)deltaY | 0xFFFFF000);
        }
        
        pNewStiches[nJumpY] = ((dy >> 8) & 0x0F) | 0x80;
        pNewStiches[nJumpY + 1] = (dy & 0xFF);
    } else {
        deltaY = (int)val2;
        if (deltaY > 63) {
            deltaY -= 128;
        }
        
        if (dy < 0)
            dy += 128;
        pNewStiches[nJumpX] = dy;
    }
    
    
    nJumpX = 0;
    nJumpY = 2;
    
    dx = (int)(deltaX + x);
    dy = (int)(deltaY + y);
    
    pNewStiches[nJumpX] = ((dx >> 8) & 0x0F) | 0x80;
    pNewStiches[nJumpX + 1] = (dx & 0xFF);
    
    pNewStiches[nJumpY] = ((dy >> 8) & 0x0F) | 0x80;
    pNewStiches[nJumpY + 1] = (dy & 0xFF);
}

void translate(pesBuffer & stitches, int x, int y){
    size_t length1 = stitches.size();
    unsigned char *pNewStiches = (unsigned char *)stitches.getData();
    unsigned long nJumpX = 0;
    unsigned long nJumpY = 2;
    unsigned char val1 = pNewStiches[nJumpX];
    unsigned char val2 = pNewStiches[nJumpX + 1];
    unsigned char val3 = pNewStiches[nJumpY];
    
    if (((val1 & 0x80) != 0x80) || ((val3 & 0x80) != 0x80)) {
        addFirstJump(stitches, 0, 0);
        pNewStiches = NULL;
        length1 = stitches.size();
        pNewStiches = NULL;
        pNewStiches = (unsigned char *)stitches.getData();
        
        val1 = pNewStiches[nJumpX];
        val2 = pNewStiches[nJumpX + 1];
    }
    
    int deltaX = 0;
    int deltaY = 0;
    if ((val1 & 0x80) == 0x80) {
        deltaX = ((val1 & 0xF) * 256) + val2;
        if (((int)deltaX & 0x800) == 0x800) {
            deltaX = (int)((int)deltaX | 0xFFFFF000);
        }
        val2 = pNewStiches[nJumpY];
        
        deltaX += x;
        int dx = (int)deltaX;
        
        pNewStiches[nJumpX] = ((dx >> 8) & 0x0F) | 0x80;
        pNewStiches[nJumpX + 1] = (dx & 0xFF);
        
    } else {
        deltaX = (int)val1;
        if (deltaX > 63) {
            deltaX -= 128;
        }
        nJumpY = 1;
        
        deltaX += x;
        if (deltaX < 0)
            deltaX += 128;
        pNewStiches[nJumpX] = deltaX;
    }
    
    if ((val2 & 0x80) == 0x80) {
        int _h = ((val2 & 0xF) * 256);
        int _l = pNewStiches[nJumpY + 1];
        deltaY = _h + _l;
        if (((int)deltaY & 0x800) == 0x800) {
            deltaY = (int)((int)deltaY | 0xFFFFF000);
        }
        
        deltaY += y;
        
        int dy = (int)deltaY;
        
        pNewStiches[nJumpY] = ((dy >> 8) & 0x0F) | 0x80;
        pNewStiches[nJumpY + 1] = (dy & 0xFF);
    } else {
        deltaY = (int)val2;
        if (deltaY > 63) {
            deltaY -= 128;
        }
        
        deltaY += y;
        if (deltaY < 0)
            deltaY += 128;
        pNewStiches[nJumpY] = deltaY;
    }
    pNewStiches = NULL;
}

void translateLastPoint(pesBuffer & stitches, int x, int y){
    unsigned char *pNewStiches = (unsigned char *)stitches.getData();
    //    unsigned long pJumpX = (unsigned long)stitches.size()+1 - 6;
    //    unsigned long pJumpY = (unsigned long)stitches.size()+1 - 4;
    unsigned long pJumpX = (unsigned long)stitches.size() - 6;
    unsigned long pJumpY = (unsigned long)stitches.size() - 4;
    unsigned char val1 = pNewStiches[pJumpX];
    unsigned char val2 = pNewStiches[pJumpX + 1];
    
    int deltaX = 0;
    int deltaY = 0;
    
    if ((val1 & 0x80) == 0x80) {
        deltaX = ((val1 & 0xF) * 256) + val2;
        if (((int)deltaX & 0x800) == 0x800) {
            deltaX = (int)((int)deltaX | 0xFFFFF000);
        }
        val2 = pNewStiches[pJumpY];
    } else {
//        GUI_LogError("translateLastPoint") << "val1(jumpX) invalid data";
        return;
    }
    
    if ((val2 & 0x80) == 0x80) {
        int _h = ((val2 & 0xF) * 256);
        int _l = pNewStiches[pJumpY + 1];
        deltaY = _h + _l;
        if (((int)deltaY & 0x800) == 0x800) {
            deltaY = (int)((int)deltaY | 0xFFFFF000);
        }
    } else {
//        GUI_LogError("translateLastPoint") << "val2(jumpY) invalid data";
        return;
    }
    deltaX += x;
    deltaY += y;
    
//    float lastX = (float)x;
//    float lastY = (float)y;
    
    int dx = (int)deltaX;
    int dy = (int)deltaY;
    
    pNewStiches[pJumpX] = ((dx >> 8) & 0x0F) | 0x80;
    pNewStiches[pJumpX + 1] = (dx & 0xFF);
    
    pNewStiches[pJumpY] = ((dy >> 8) & 0x0F) | 0x80;
    pNewStiches[pJumpY + 1] = (dy & 0xFF);
    
    pNewStiches = NULL;
}

void appendStitches(pesBuffer & stitches, const pesBuffer & newStitches, float space)
{
    //    size_t length1 = stitches.size() + 1;
    //    size_t length2 = newStitches.size() + 1;
    //    size_t nLastStitch = stitches.size() + 1 - 2;
    //    size_t nJumpX = stitches.size() + 1 - 6;
    //    size_t nJumpY = stitches.size() + 1 - 4;
    size_t length1 = stitches.size();
    size_t length2 = newStitches.size();
    size_t nLastStitch = stitches.size() - 2;
    size_t nJumpX = stitches.size() - 6;
    size_t nJumpY = stitches.size() - 4;
    stitches.resize(length1 + length2);
    unsigned char *pNewStitches = (unsigned char *)stitches.getData();
    
    if (pNewStitches[nLastStitch] == 255 && pNewStitches[nLastStitch + 1] == 0) {
        unsigned char val1 = pNewStitches[nJumpX];
        unsigned char val2 = pNewStitches[nJumpX + 1];
        
        int deltaX = 0;
        int deltaY = 0;
        
        if ((val1 & 0x80) == 0x80) {
            deltaX = ((val1 & 0xF) * 256) + val2;
            if ((deltaX & 0x800) == 0x800) {
                deltaX = deltaX | 0xFFFFF000;
            }
            val2 = pNewStitches[nJumpY];
        } else {
//            GUI_LogError("appendStitches") << "val1 error";
            return;
        }
        
        if ((val2 & 0x80) == 0x80) {
            int h = ((val2 & 0xF) * 256);
            int l = pNewStitches[nJumpY + 1];
            deltaY = h + l;
            if ((deltaY & 0x800) == 0x800) {
                deltaY = deltaY | 0xFFFFF000;
            }
        } else {
//            GUI_LogError("appendStitches") << "val2 error";
            return;
        }
        
        int oldDeltaX = deltaX;
        int oldDeltaY = deltaY;
        
        memcpy(&pNewStitches[nJumpX], newStitches.getData(), newStitches.size());
        
        val1 = pNewStitches[nJumpX];
        val2 = pNewStitches[nJumpX + 1];
        
        deltaX = 0;
        deltaY = 0;
        
        if ((val1 & 0x80) == 0x80) {
            deltaX = ((val1 & 0xF) * 256) + val2;
            if (((int)deltaX & 0x800) == 0x800) {
                deltaX = (int)((int)deltaX | 0xFFFFF000);
            }
            val2 = pNewStitches[nJumpY];
            
            deltaX += (int)(oldDeltaX + space);
            int dx = (int)deltaX;
            
            // pom
            unsigned char orPart = 0x80;
            orPart |= 0x10;
            
            pNewStitches[nJumpX] = ((dx >> 8) & 0x0F) | orPart;
            pNewStitches[nJumpX + 1] = (dx & 0xFF);
        } else {
            deltaX = (int)val1;
            if (deltaX > 63) {
                deltaX -= 128;
            }
            nJumpY = nJumpX + 1;
            
            deltaX += (int)(oldDeltaX + space);
            if (deltaX < 0)
                deltaX += 128;
            pNewStitches[nJumpX] = deltaX;
        }
        
        if ((val2 & 0x80) == 0x80) {
            int _h = ((val2 & 0xF) * 256);
            int _l = pNewStitches[nJumpY + 1];
            deltaY = _h + _l;
            if (((int)deltaY & 0x800) == 0x800) {
                deltaY = (int)((int)deltaY | 0xFFFFF000);
            }
            
            deltaY += oldDeltaY;
            int dy = (int)deltaY;
            
            // pom
            unsigned char orPart = 0x80;
            orPart |= 0x10;
            
            pNewStitches[nJumpY] = ((dy >> 8) & 0x0F) | orPart;
            pNewStitches[nJumpY + 1] = (dy & 0xFF);
            
        } else {
            deltaY = (int)val2;
            if (deltaY > 63) {
                deltaY -= 128;
            }
            
            deltaY += oldDeltaY;
            if (deltaY < 0)
                deltaY += 128;
            pNewStitches[nJumpY] = deltaY;
        }
        
        stitches.resize(length1 + length2 - 6);
        
    } // end if(pNewStitches[nLastStitch] == 0xFF && pNewStitches[nLastStitch + 1] == 0x00)
    
    pNewStitches = NULL;
}

