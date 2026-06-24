//
//  PesPPEFUtils.hpp
//  sources
//
//  Created by SIHARAT THAMMAYA on 23/9/2565 BE.
//

#ifndef PesPPEFUtils_hpp
#define PesPPEFUtils_hpp

#include <stdio.h>
#include "pesEngine.h"
#include "SQLiteCpp/SQLiteCpp.h"

class PPEF_Reader{
public:
    PPEF_Reader(const char * filePath);
    ~PPEF_Reader();
    void readPPEFConfig();
    
    // Note: PPEF define a hight of Latin Capital Letter "M" with 300 pt.(from baseline to topmost)
    // scale with 1/3 will change it height to 100 pt.(1 cm. in PES)
//    vector<GUI_Path *> getStringAsShapes(const string & str, float scale=1/3.0, PPEF_Effect* effect=nullptr, int extraLetterSpace=0, int extraSpace=0);
    std::vector<pesPath> getStringAsShapes(const std::string & str, float scale=1/3.0, int extraLetterSpace=0, int extraSpace=0);
    std::vector<pesPath> getStringAsShapesAndColors(const std::string & str, std::vector<uint8_t> & colors,float scale=1/3.0, int extraLetterSpace=0, int extraSpace=0);
    
private:
    SQLite::Database db;
    
    struct{
        int LetterSpace = 0;
        int LineHeight = 764;
        int SpaceGap = 150;
        int LowerGap = 25;
        int BaseGap = 120;
        int UpperGap = 16;
        int UpperVovelGap = 145;
        int FishTail = 65;
        int ElephantTail = 25;
        int OwlTail = 25;
        int KiteY = 50;
    }config;
};

#endif /* PesPPEFUtils_hpp */
