//
//  pesUtility.hpp
//  pesEngine
//
//  Created by SIHARAT THAMMAYA on 11/7/2562 BE.
//  Copyright © 2562 SIHARAT THAMMAYA. All rights reserved.
//

#ifndef pesUtility_hpp
#define pesUtility_hpp

#include <stdio.h>
#include <string>
#include <cstdlib>
#include <algorithm>
#include <sstream>
#include <iomanip>
#include "pesPath.hpp"
#include "pesBuffer.hpp"
#include "pesMath.hpp"
#include "json.hpp"

template<class T, class BoolFunction>
void pesRemove(std::vector<T>& values, BoolFunction shouldErase) {
    values.erase(remove_if(values.begin(), values.end(), shouldErase), values.end());
}

template<class T, class BoolFunction>
void pesSort(std::vector<T>& values, BoolFunction compare) {
    sort(values.begin(), values.end(), compare);
}

template<typename Type>
typename std::enable_if<std::is_floating_point<Type>::value, bool>::type pesIsFloatEqual(const Type& a, const Type& b)
{
    return std::abs(a - b) <= std::numeric_limits<Type>::epsilon() * std::abs(a);
}


// MARK: - std::string Utility
std::vector<std::string> pesSplitString(const std::string& source, const std::string& delimiter, bool ignoreEmpty = false, bool trim = false);
std::string pesJoinString(const std::vector<std::string>& stringElements, const std::string& delimiter);
void pesStringReplace(std::string& input, const std::string& searchStr, const std::string& replaceStr);

bool pesIsStringInString(const std::string& haystack, const std::string& needle);
bool pesIsStringEndsWith(std::string const & value, std::string const & ending);
bool pesIsStringStartsWith(std::string const & value, std::string const & starting);

// Check how many times a string contains another string.
std::size_t pesStringTimesInString(const std::string& haystack, const std::string& needle);
bool pesStringIsDigits(const std::string &str);

std::string pesTrimFront(const std::string & src);
std::string pesTrimBack(const std::string & src);
std::string pesTrim(const std::string & src);

std::string pesVAArgsToString(const char * format, ...);
std::string pesVAArgsToString(const char * format, va_list args);

int pesToInt(const std::string& intString);
float pesToFloat(const std::string& floatString);

template <class T>
std::string pesToString(const T& value) {
    std::ostringstream out;
    out << value;
    return out.str();
}

// Like sprintf "%4f" format, in this example precision=4
template <class T>
std::string pesToString(const T& value, int precision) {
    std::ostringstream out;
    out << std::fixed << std::setprecision(precision) << value;
    return out.str();
}

/// Like sprintf "% 4d" or "% 4f" format, in this example width=4, fill=' '
template <class T>
std::string pesToString(const T& value, int width, char fill) {
    std::ostringstream out;
    out << std::fixed << std::setfill(fill) << std::setw(width) << value;
    return out.str();
}

/// Like sprintf "%04.2d" or "%04.2f" format, in this example precision=2, width=4, fill='0'
template <class T>
std::string pesToString(const T& value, int precision, int width, char fill) {
    std::ostringstream out;
    out << std::fixed << std::setfill(fill) << std::setw(width) << std::setprecision(precision) << value;
    return out.str();
}

// Convert a std::vector of values to a comma-delimited std::string.
template<class T>
std::string pesToString(const std::vector<T>& values) {
    std::stringstream out;
    int n = values.size();
    out << "{";
    
    if (n > 0) {
        for (int i = 0; i < n - 1; i++) {
            out << values[i] << ", ";
        }
        out << values[n - 1];
    }
    
    out << "}";
    return out.str();
}

template <class T>
std::string pesToHex(const T& value) {
    std::ostringstream out;
    // pretend that the value is a bunch of bytes
    unsigned char* valuePtr = (unsigned char*)&value;
    
    // the number of bytes is determined by the datatype
    int numBytes = sizeof(T);
    
    // the bytes are stored backwards (least significant first)
    for (int i = numBytes - 1; i >= 0; i--) {
        // print each byte out as a 2-character wide hex value
        out << std::setfill('0') << std::setw(2) << std::hex << (int)valuePtr[i];
    }
    
    return out.str();
}

//template <class T>
//std::string pesToHex(const T& value);
//
//template <>
//std::string pesToHex(const std::string& value);
//
///// Converts a std::string (e.g., `"abc"`) to its equivalent hexadecimal
///// representation (e.g., `"616263"`).
//std::string pesToHex(const char* value);


// MARK: - Loader/Parser Utility
pesPath pesLoadPASFromBuffer(pesBuffer buffer); // Tile Pattern(for Embossed fill)
pesPath pesLoadPMFFromBuffer(pesBuffer buffer); // Motif Pattern(for motif line/fill)

#endif /* pesUtility_hpp */
