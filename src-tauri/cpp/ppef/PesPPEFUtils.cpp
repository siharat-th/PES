//
//  PesPPEFUtils.cpp
//  sources
//
//  Created by SIHARAT THAMMAYA on 23/9/2565 BE.
//

#include "PesPPEFUtils.hpp"
#include "PesUnicodeUtils.hpp"

PPEF_Reader::PPEF_Reader(const char * filePath)
:db(filePath, SQLite::OPEN_READONLY)
{
}

PPEF_Reader::~PPEF_Reader(){
    
}

void PPEF_Reader::readPPEFConfig(){
        SQLite::Statement query(db, "SELECT name, value FROM Config");
        while (query.executeStep()) {
            std::string name = query.getColumn(0);
            if(name=="BaseGap")
                config.BaseGap = query.getColumn(1).getInt();
            else if(name=="ElephantTail")
                config.ElephantTail = query.getColumn(1).getInt();
            else if(name=="FishTail")
                config.FishTail = query.getColumn(1).getInt();
            else if(name=="KiteY")
                config.KiteY = query.getColumn(1).getInt();
            else if(name=="LetterSpace")
                config.LetterSpace = query.getColumn(1).getInt();
            else if(name=="LineHeight")
                config.LineHeight = query.getColumn(1).getInt();
            else if(name=="LowerGap")
                config.LowerGap = query.getColumn(1).getInt();
            else if(name=="OwlTail")
                config.OwlTail = query.getColumn(1).getInt();
            else if(name=="SpaceGap")
                config.SpaceGap = query.getColumn(1).getInt();
            else if(name=="UpperGap")
                config.UpperGap = query.getColumn(1).getInt();
            else if(name=="UpperVovelGap")
                config.UpperVovelGap = query.getColumn(1).getInt();
        }
        query.reset();
}

std::vector<pesPath> PPEF_Reader::getStringAsShapes(const std::string & utf8, float scale, int extraLetterSpace, int extraSpace){
    std::vector<pesPath> shapes;
    
    // Check for Thai language
    bool bThaiAlphabet = false;
    for(auto c : UTF8Iterator(utf8)){
        if(th_isThaiLetter(c)){
            bThaiAlphabet = true;
            break;
        }
    }
    
    std::vector<uint32_t> uniString;
    uint32_t prev = -1;
    for(auto unichar: UTF8Iterator(utf8)){
        
        if(bThaiAlphabet){
            // If input utf8-string contain Thai aphabet.
            if(unichar==THAI_SARA_AM) // Decompose THAI_SARA_AM to THAI_NIKHAHIT + THAI_SARA_AA
            {
                if(prev!=-1 && th_isToneAndDiacritic(prev)){
                    uniString.back() = THAI_NIKHAHIT;
                    uniString.push_back(prev);
                    uniString.push_back(THAI_SARA_AA);
                }
                else{
                    uniString.push_back(THAI_NIKHAHIT);
                    uniString.push_back(THAI_SARA_AA);
                }
            }
            else
            {
                if(prev!=-1){
                    if(th_isBelowVowelAndDiacritic(unichar)){ // Substitue THAI_THO_THAN & THAI_YO_YING when followed by Lower vowel
                        if(prev==THAI_THO_THAN)
                            uniString.back() = THAI_THO_THAN_NONE_TAIL;
                        else if(prev==THAI_YO_YING)
                            uniString.back() = THAI_YO_YING_NONE_TAIL;
                    }
                }
                uniString.push_back(unichar);
            }
            prev = unichar;
        }
        else{
            uniString.push_back(unichar);
        }
    }
    
    const static int baseline = 150;
    int xstart = 0;
    int ystart = -baseline;
//    try{
        SQLite::Statement query(db, "SELECT data FROM Data WHERE unicode = ?");
        int num = uniString.size();
        
        uint32_t ch = uniString.front();
        uint32_t lastch = ch;
        int lastLastCh = 0;
        
        for(int i=0; i<num; i++){
            float xmin = 0;
            float advance = 0;
            float xoffset = 0;
            float yoffset = 0;
            pesPath chshape;
            ch = uniString[i];
            query.bind(1, ch);
            
            bool bExist = false;
            if(query.executeStep()){
                bExist = true;
                SQLite::Column colBlob = query.getColumn(0);
                const void* blob = colBlob.getBlob(); // Get a pointer to the bytes.
                std::size_t size = colBlob.getBytes(); // Get the number of bytes.
                pesBuffer buffer(static_cast<const char*>(blob), size); // Create a buffer with those bytes.
                if(buffer.size()){
                    int offset;
                    char* c = buffer.getData();
                    memcpy(&xmin, c, sizeof(float)); // read xmin
                    c+=sizeof(float);
                    memcpy(&advance, c, sizeof(float)); // read advance
                    c+=sizeof(float);
                    memcpy(&offset, c, sizeof(int)); // read data offset(jump to address)
                    
                    c = buffer.getData();
                    int ptr = offset;
                    int bufferSize = (int)buffer.size();
                    while(ptr<bufferSize){
                        pesPath::Command com(pesPath::Command::_moveTo);
//                        size_t byte_read = sizeof(GUI_Path::Command) - sizeof(string);
                        size_t byte_read = sizeof(pesPath::Command);
                        memcpy(&com, &c[ptr], byte_read );
                        chshape.addCommand(com);
                        ptr+=byte_read;
                    }
                }
            }
            
            if(bExist){
                
                int extSpace = (ch == 0x20) ? extraSpace : extraLetterSpace;
                
                if(bThaiAlphabet){
                    if( th_isToneAndDiacritic(ch) ) { // ญี่ปุ่น
                        if( th_isBelowVowel(lastch)) {
                            lastch = lastLastCh;
                            lastLastCh = 0;
                        }
                    }
                    
                    if(th_isAboveVowelAndDiacritic(ch) || th_isToneAndDiacritic(ch)){
                        if(th_isAscenderConsonant(lastch)){ // (ป, ฝ, ฟ)
                            xoffset -= config.FishTail;
                        }
                        else if(lastch==THAI_CHO_CHANG || lastch==THAI_SO_SO || // (ช ซ ศ ส)
                                lastch==THAI_SO_SALA || lastch==THAI_SO_SUA)
                        {
                            xoffset -= config.ElephantTail;
                        }
                        else if(lastch==THAI_LO_CHULA){ // ฬ
                            yoffset -= config.KiteY;
                        }
                        else if(lastch==THAI_HO_NOKHUK){ // ฮ
                            xoffset -= config.OwlTail;
                        }
                    }
                    
                    if(th_isAboveVowelAndDiacritic(lastch)){
                        if(th_isToneAndDiacritic(ch)){
                            if(th_isAscenderConsonant(lastLastCh)){ // (ป, ฝ, ฟ)
                                xoffset -= config.FishTail;
                            }
                            else if(lastLastCh==THAI_CHO_CHANG || lastLastCh==THAI_SO_SO || // (ช ซ ศ ส)
                                    lastLastCh==THAI_SO_SALA || lastLastCh==THAI_SO_SUA)
                            {
                                xoffset -= config.ElephantTail;
                            }
                            else if(lastLastCh==THAI_LO_CHULA){ // ฬ
                                yoffset -= config.KiteY;
                            }
                            else if(lastLastCh==THAI_HO_NOKHUK){ // ฮ
                                xoffset -= config.OwlTail;
                            }
                            yoffset -= config.UpperVovelGap;
                        }
                    }
                    
                    if(th_isBelowVowelAndDiacritic(ch)){
                        if(lastch==THAI_RU || lastch==THAI_LU){
                            xoffset -= config.FishTail;
                        }
                        else if(lastch==THAI_DO_CHADA || lastch==THAI_TO_PATAK){
                            yoffset += config.BaseGap;
                        }
                    }
                    
                    if(th_isAboveVowelAndDiacritic(ch) || th_isBelowVowelAndDiacritic(ch) || th_isToneAndDiacritic(ch)){
                        xoffset -= (config.LetterSpace + extSpace);
                    }
                } // end if(bThaiAlphabet)
                else{

                }
            
                float tx = (float)(xstart - xmin + xoffset);
                float ty = (float)(ystart + yoffset);
                chshape.translate(tx, ty);
                
                xstart += advance;
                bool bZeroAdvance = advance==0;
                if(bZeroAdvance==false){
                    xstart += (config.LetterSpace + extSpace);
                }
                
                if(chshape.getCommands().size()){
                    chshape.setFilled(false);
                    chshape.setStrokeWidth(2);
                    chshape.setStrokeHexColor(0x0000ff);
                    shapes.push_back(chshape);
                }
                
            } // end if(bExist)
            
            if(i>0){
                lastLastCh = lastch;
                lastch = ch;
            }
            query.reset();
        }
//    }
//    catch (const std::exception& e)
//    {
//        GUI_LogError("Query DB") << "SQLite exception: " << e.what();
//    }
    
    if(scale!=1.0){
        for(pesPath & s : shapes){
            s.scale(scale, scale);
        }
    }
    
    return shapes;
}

//std::vector<pesPath> getStringAsShapesAndColors(const std::string & str, std::vector<int8_t> & colors,float scale=1/3.0);
std::vector<pesPath> PPEF_Reader::getStringAsShapesAndColors(const std::string & utf8, std::vector<uint8_t> & colors,
                                                             float scale, int extraLetterSpace, int extraSpace){
    std::vector<pesPath> shapes;
    colors.clear();
    
    // Check for Thai language
    bool bThaiAlphabet = false;
    for(auto c : UTF8Iterator(utf8)){
        if(th_isThaiLetter(c)){
            bThaiAlphabet = true;
            break;
        }
    }
    
    std::vector<uint32_t> uniString;
    uint32_t prev = -1;
    for(auto unichar: UTF8Iterator(utf8)){
        
        if(bThaiAlphabet){
            // If input utf8-string contain Thai aphabet.
            if(unichar==THAI_SARA_AM) // Decompose THAI_SARA_AM to THAI_NIKHAHIT + THAI_SARA_AA
            {
                if(prev!=-1 && th_isToneAndDiacritic(prev)){
                    uniString.back() = THAI_NIKHAHIT;
                    uniString.push_back(prev);
                    uniString.push_back(THAI_SARA_AA);
                }
                else{
                    uniString.push_back(THAI_NIKHAHIT);
                    uniString.push_back(THAI_SARA_AA);
                }
            }
            else
            {
                if(prev!=-1){
                    if(th_isBelowVowelAndDiacritic(unichar)){ // Substitue THAI_THO_THAN & THAI_YO_YING when followed by Lower vowel
                        if(prev==THAI_THO_THAN)
                            uniString.back() = THAI_THO_THAN_NONE_TAIL;
                        else if(prev==THAI_YO_YING)
                            uniString.back() = THAI_YO_YING_NONE_TAIL;
                    }
                }
                uniString.push_back(unichar);
            }
            prev = unichar;
        }
        else{
            uniString.push_back(unichar);
        }
    }
    
    const static int baseline = 150;
    int xstart = 0;
    int ystart = -baseline;
//    try{
        SQLite::Statement query(db, "SELECT data, color FROM Data WHERE unicode = ?");
        int num = uniString.size();
        
        uint32_t ch = uniString.front();
        uint32_t lastch = ch;
        int lastLastCh = 0;
        
        for(int i=0; i<num; i++){
            float xmin = 0;
            float advance = 0;
            float xoffset = 0;
            float yoffset = 0;
            pesPath chshape;
            ch = uniString[i];
            query.bind(1, ch);
            
            bool bExist = false;
            if(query.executeStep()){
                // colors(PPEF2)
                SQLite::Column colorBlob = query.getColumn(1);
                std::size_t colorSize = colorBlob.getBytes();
                pesBuffer colorBuffer(static_cast<const char*>(colorBlob), colorSize);
                int cntColor = 0;
                if(colorBuffer.size()){
                    char* c = colorBuffer.getData();
                    int offset = 0;
                    while(offset<colorBuffer.size()){
                        uint8_t color;
                        memcpy(&color, &c[offset], sizeof(uint8_t)); // read colorIndex(1-64)
                        offset+=sizeof(uint8_t);
                        colors.push_back(color);
                        cntColor++;
                    }
                }
                // end colors(PPEF2)
                bExist = true;
                SQLite::Column colBlob = query.getColumn(0);
                const void* blob = colBlob.getBlob(); // Get a pointer to the bytes.
                std::size_t size = colBlob.getBytes(); // Get the number of bytes.
                pesBuffer buffer(static_cast<const char*>(blob), size); // Create a buffer with those bytes.
                if(buffer.size()){
                    int offset;
                    char* c = buffer.getData();
                    memcpy(&xmin, c, sizeof(float)); // read xmin
                    c+=sizeof(float);
                    memcpy(&advance, c, sizeof(float)); // read advance
                    c+=sizeof(float);
                    memcpy(&offset, c, sizeof(int)); // read data offset(jump to address)
                    
                    c = buffer.getData();
                    int ptr = offset;
                    int bufferSize = (int)buffer.size();
                    int cntMoveTo = 0;
                    while(ptr<bufferSize){
                        pesPath::Command com(pesPath::Command::_moveTo);
//                        size_t byte_read = sizeof(GUI_Path::Command) - sizeof(string);
                        size_t byte_read = sizeof(pesPath::Command);
                        memcpy(&com, &c[ptr], byte_read );
                        if(com.type==pesPath::Command::_moveTo)
                            cntMoveTo++;
                        chshape.addCommand(com);
                        ptr+=byte_read;
                    }
                    
                    int cntSatinColumn = cntMoveTo / 2;
                    while(cntColor<cntSatinColumn){
                        colors.push_back(0x20);
                        cntColor++;
                    }
                }
            }
            
            if(bExist){
                
                int extSpace = (ch == 0x20) ? extraSpace : extraLetterSpace;
                
                if(bThaiAlphabet){
                    if( th_isToneAndDiacritic(ch) ) { // ญี่ปุ่น
                        if( th_isBelowVowel(lastch)) {
                            lastch = lastLastCh;
                            lastLastCh = 0;
                        }
                    }
                    
                    if(th_isAboveVowelAndDiacritic(ch) || th_isToneAndDiacritic(ch)){
                        if(th_isAscenderConsonant(lastch)){ // (ป, ฝ, ฟ)
                            xoffset -= config.FishTail;
                        }
                        else if(lastch==THAI_CHO_CHANG || lastch==THAI_SO_SO || // (ช ซ ศ ส)
                                lastch==THAI_SO_SALA || lastch==THAI_SO_SUA)
                        {
                            xoffset -= config.ElephantTail;
                        }
                        else if(lastch==THAI_LO_CHULA){ // ฬ
                            yoffset -= config.KiteY;
                        }
                        else if(lastch==THAI_HO_NOKHUK){ // ฮ
                            xoffset -= config.OwlTail;
                        }
                    }
                    
                    if(th_isAboveVowelAndDiacritic(lastch)){
                        if(th_isToneAndDiacritic(ch)){
                            if(th_isAscenderConsonant(lastLastCh)){ // (ป, ฝ, ฟ)
                                xoffset -= config.FishTail;
                            }
                            else if(lastLastCh==THAI_CHO_CHANG || lastLastCh==THAI_SO_SO || // (ช ซ ศ ส)
                                    lastLastCh==THAI_SO_SALA || lastLastCh==THAI_SO_SUA)
                            {
                                xoffset -= config.ElephantTail;
                            }
                            else if(lastLastCh==THAI_LO_CHULA){ // ฬ
                                yoffset -= config.KiteY;
                            }
                            else if(lastLastCh==THAI_HO_NOKHUK){ // ฮ
                                xoffset -= config.OwlTail;
                            }
                            yoffset -= config.UpperVovelGap;
                        }
                    }
                    
                    if(th_isBelowVowelAndDiacritic(ch)){
                        if(lastch==THAI_RU || lastch==THAI_LU){
                            xoffset -= config.FishTail;
                        }
                        else if(lastch==THAI_DO_CHADA || lastch==THAI_TO_PATAK){
                            yoffset += config.BaseGap;
                        }
                    }
                    
                    if(th_isAboveVowelAndDiacritic(ch) || th_isBelowVowelAndDiacritic(ch) || th_isToneAndDiacritic(ch)){
                        xoffset -= (config.LetterSpace + extSpace);
                    }
                } // end if(bThaiAlphabet)
                else{

                }
            
                float tx = (float)(xstart - xmin + xoffset);
                float ty = (float)(ystart + yoffset);
                chshape.translate(tx, ty);
                
                xstart += advance;
                bool bZeroAdvance = advance==0;
                if(bZeroAdvance==false){
                    xstart += (config.LetterSpace + extSpace);
                }
                
                if(chshape.getCommands().size()){
                    chshape.setFilled(false);
                    chshape.setStrokeWidth(2);
                    chshape.setStrokeHexColor(0x0000ff);
                    shapes.push_back(chshape);
                }
                
            } // end if(bExist)
            
            if(i>0){
                lastLastCh = lastch;
                lastch = ch;
            }
            query.reset();
        }
//    }
//    catch (const std::exception& e)
//    {
//        GUI_LogError("Query DB") << "SQLite exception: " << e.what();
//    }
    
    if(scale!=1.0){
        for(pesPath & s : shapes){
            s.scale(scale, scale);
        }
    }
    
    return shapes;
}
