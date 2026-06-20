//
//  pesUtility.cpp
//  pesEngine
//
//  Created by SIHARAT THAMMAYA on 11/7/2562 BE.
//  Copyright © 2562 SIHARAT THAMMAYA. All rights reserved.
//

#include "pesUtility.hpp"
#include <cctype>
#include <sstream>
#include <iomanip>

using namespace std;

// MARK: - String Utility
vector<string> pesSplitString(const string& source, const string& delimiter, bool ignoreEmpty, bool trim){
    vector<string> result;
    
    if (delimiter.empty()) {
        result.push_back(source);
        return result;
    }
    
    string::const_iterator substart = source.begin(), subend;
    
    while (true) {
        subend = search(substart, source.end(), delimiter.begin(), delimiter.end());
        string sub(substart, subend);
        
        if (trim) {
            sub = pesTrim(sub);
        }
        
        if (!ignoreEmpty || !sub.empty()) {
            result.push_back(sub);
        }
        
        if (subend == source.end()) {
            break;
        }
        
        substart = subend + delimiter.size();
    }
    
    return result;
}

string pesJoinString(const vector<string>& stringElements, const string& delimiter){
    string str;
    
    if (stringElements.empty()) {
        return str;
    }
    
    auto numStrings = stringElements.size();
    string::size_type strSize = delimiter.size() * (numStrings - 1);
    
    for (const string &s : stringElements) {
        strSize += s.size();
    }
    
    str.reserve(strSize);
    str += stringElements[0];
    
    for (decltype(numStrings) i = 1; i < numStrings; ++i) {
        str += delimiter;
        str += stringElements[i];
    }
    
    return str;
}

void pesStringReplace(string& input, const string& searchStr, const string& replaceStr){
    auto pos = input.find(searchStr);
    
    while (pos != std::string::npos) {
        input.replace(pos, searchStr.size(), replaceStr);
        pos += replaceStr.size();
        std::string nextfind(input.begin() + pos, input.end());
        auto nextpos = nextfind.find(searchStr);
        
        if (nextpos == std::string::npos) {
            break;
        }
        
        pos += nextpos;
    }
}

bool pesIsStringInString(const string& haystack, const string& needle){
    return haystack.find(needle) != std::string::npos;
}

bool pesIsStringEndsWith(std::string const & value, std::string const & ending){
    if (ending.size() > value.size()) return false;
    return std::equal(ending.rbegin(), ending.rend(), value.rbegin());
}

bool pesIsStringStartsWith(std::string const & value, std::string const & starting){
    if (starting.size() > value.size()) return false;
    return !value.compare(0, starting.length(), starting);
}

// Check how many times a string contains another string.
std::size_t pesStringTimesInString(const string& haystack, const string& needle){
    const size_t step = needle.size();
    
    size_t count(0);
    size_t pos(0);
    
    while ((pos = haystack.find(needle, pos)) != std::string::npos) {
        pos += step;
        ++count;
    }
    
    return count;
}

bool pesStringIsDigits(const std::string &str)
{
    return std::all_of(str.begin(), str.end(), ::isdigit); // C++11
}

string pesTrimFront(const string & src){
    auto dst = src;
    dst.erase(dst.begin(), std::find_if_not(dst.begin(), dst.end(), [&](char & c) {return ::isspace(c); }));
    return dst;
}
string pesTrimBack(const string & src){
    auto dst = src;
    dst.erase(std::find_if_not(dst.rbegin(), dst.rend(), [&](char & c) {return ::isspace(c); }).base(), dst.end());
    return dst;
}
string pesTrim(const string & src){
    return pesTrimFront(pesTrimBack(src));
}

string pesVAArgsToString(const char * format, ...) {
    // variadic args to string:
    // http://www.codeproject.com/KB/string/string_format.aspx
    char aux_buffer[10000];
    string retStr("");
    
    if (nullptr != format) {
        va_list marker;
        
        // initialize variable arguments
        va_start(marker, format);
        
        // Get formatted string length adding one for nullptr
        size_t len = vsprintf(aux_buffer, format, marker) + 1;
        
        // Reset variable arguments
        va_end(marker);
        
        if (len > 0) {
            va_list args;
            
            // initialize variable arguments
            va_start(args, format);
            
            // Create a char vector to hold the formatted string.
            vector<char> buffer(len, '\0');
            vsprintf(&buffer[0], format, args);
            retStr = &buffer[0];
            va_end(args);
        }
        
    }
    
    return retStr;
}

string pesVAArgsToString(const char * format, va_list args) {
    // variadic args to string:
    // http://www.codeproject.com/KB/string/string_format.aspx
    char aux_buffer[10000];
    string retStr("");
    
    if (nullptr != format) {
        
        // Get formatted string length adding one for nullptr
        vsprintf(aux_buffer, format, args);
        retStr = aux_buffer;
        
    }
    
    return retStr;
}

int pesToInt(const string& intString){
    int x = 0;
    istringstream cur(intString);
    cur >> x;
    return x;
}

float pesToFloat(const string& floatString) {
    float x = 0;
    istringstream cur(floatString);
    cur >> x;
    return x;
}


// MARK: - Loader/Parser Utility
pesPath pesLoadPASFromBuffer(pesBuffer buff){
    pesPath path;
    if (buff.size()) {
        for (pesBuffer::Line it = buff.getLines().begin(), end = buff.getLines().end(); it != end; ++it) {
            std::string line = *it;
            if (line.empty() == false) {
                line = pesTrim(line);
//                GUI_LogVerbose() << line.c_str();
                
                int numTile = 0;
                if(pesIsStringInString(line, "@TILE")){
                    std::string str(line);
                    pesStringReplace(str, "@TILE", "");
                    str = pesTrim(str);
                    if(pesStringIsDigits(str)){
                        numTile = pesToInt(str);
//                        GUI_LogVerbose("numTile") << numTile;
                    }
                    
                    if(numTile>0){
                        while(++it!=end){
                            std::string tile_line = *it;
                            if(pesIsStringInString(tile_line, "@TILELINE")){
                                bool backslashAtEnd = false;
                                while(pesIsStringEndsWith(tile_line, "\\")){
                                    it++;
                                    tile_line.append(*it);
                                    backslashAtEnd = true;
                                }
                                
                                if(backslashAtEnd)
                                    pesStringReplace(tile_line, "\\", "");
//                                GUI_LogVerbose() << tile_line.c_str();
                                pesStringReplace(tile_line, "@TILELINE", "");
                                auto sep = pesSplitString(tile_line, " ", true, true);
                                if(sep.size()>2){
                                    // [0] TILELINE_INDEX
                                    // [1] NUM_LINE
                                    // [2..N] DATA (x,y pair integer range between 0 to 1024)
//                                    int pathIndex = pesToInt(sep[0]);
                                    int numTileLine = pesToInt(sep[1]);
                                    if((numTileLine+1)*2==sep.size()){
                                        for(int i=0; i<numTileLine; i++){
                                            int idx = 2 + i*2;
                                            int x = pesToInt(sep[idx]);
                                            int y = pesToInt(sep[idx+1]);
//                                            GUI_LogVerbose("XY") << x << "," << y;
                                            if(i==0)
                                                path.moveTo((float)x, (float)y);
                                            else
                                                path.lineTo((float)x, (float)y);
                                        }
                                    }
                                    else{
//                                        GUI_LogError("LoadPattern") << "TileLine size miss match";
                                    }
                                }
                            }
                        }
                        if(it==end)
                            break;
                    }
                }
                
            }
        }
    }
    
    return path;
}

pesPath pesLoadPMFFromBuffer(pesBuffer buff){
    pesPath path;
    if (buff.size()) {
        for (pesBuffer::Line it = buff.getLines().begin(), end = buff.getLines().end(); it != end; ++it) {
            std::string line = *it;
            if (line.empty() == false) {
                line = pesTrim(line);
//                GUI_LogVerbose() << line.c_str();
                
                int version = 0;
                if(pesIsStringInString(line, "@VERSION")){
                    std::string str(line);
                    pesStringReplace(str, "@VERSION", "");
                    str = pesTrim(str);
                    if(pesStringIsDigits(str)){
                        version = pesToInt(str);
//                        GUI_LogVerbose("version") << version;
                    }
                    
                    if(version>=10000){
                        while(++it!=end){
                            std::string tile_line = *it;
                            if(pesIsStringInString(tile_line, "@LINE")){
                                bool backslashAtEnd = false;
                                while(pesIsStringEndsWith(tile_line, "\\")){
                                    it++;
                                    tile_line.append(*it);
                                    backslashAtEnd = true;
                                }
                                
                                if(backslashAtEnd)
                                    pesStringReplace(tile_line, "\\", "");
//                                GUI_LogVerbose() << tile_line.c_str();
                                pesStringReplace(tile_line, "@LINE", "");
                                auto sep = pesSplitString(tile_line, " ", true, true);
                                if(sep.size()>2){
                                    // [0] NUM_LINE
                                    // [1..N] DATA (x,y pair integer range between 0 to 1024)
                                    int numLine = pesToInt(sep[0]);
                                    if((numLine*2+1)==sep.size()){
                                        for(int i=0; i<numLine; i++){
                                            int idx = 1 + i*2;
                                            int x = pesToInt(sep[idx]);
                                            int y = pesToInt(sep[idx+1]);
//                                            GUI_LogVerbose("XY") << x << "," << y;
                                            if(i==0)
                                                path.moveTo((float)x, (float)y);
                                            else
                                                path.lineTo((float)x, (float)y);
                                        }
                                    }
                                    else{
//                                        GUI_LogError("LoadPattern") << "Line size miss match";
                                    }
                                }
                            }
                        }
                        if(it==end)
                            break;
                    }
                }
                
            }
        }
    }
    
    return path;
}
