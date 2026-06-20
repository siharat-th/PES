//
//  pesBuffer.hpp
//  pesEngine
//
//  Created by SIHARAT THAMMAYA on 11/7/2562 BE.
//  Copyright © 2562 SIHARAT THAMMAYA. All rights reserved.
//

#ifndef pesBuffer_hpp
#define pesBuffer_hpp

#include <stdio.h>
#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <iostream>
#include <string>
#include <vector>

// using namespace std;

class pesBuffer {
public:
    pesBuffer();
    pesBuffer(const char* buffer, std::size_t size);
    pesBuffer(const std::string& text);
    pesBuffer(std::istream& stream, size_t ioBlockSize = 1024);

    void set(const char* _buffer, std::size_t _size);
    void set(const std::string& text);
    bool set(std::istream& stream, size_t ioBlockSize = 1024);
    void setall(char mem);
    void append(const std::string& _buffer);
    void append(const char* _buffer, std::size_t _size);

    void appendU32(uint32_t n);
    void appendU16(uint16_t n);
    void appendU8(uint8_t n);
    void appendU8(uint8_t u8, int num);

    void appendS16(int16_t n);

    void appendString(const char* str);
    void appendPESString(const char* str);
    void appendPESString2(const char* str);

    // pom
    // MARK: Little-endian
    void append(int _ivalue);
    void append(unsigned int _ivalue);
    void append(short _svalue);
    void append(unsigned short _svalue);
    void append(float _fvalue);

    void reserve(size_t size);

    bool writeTo(std::ostream& stream) const;

    void clear();

    void allocate(std::size_t _size);
    void resize(std::size_t _size);

    char* getData();
    const char* getData() const;

    std::string getText() const;
    operator std::string() const;  // cast to std::string, to use a buffer as a std::string
    pesBuffer& operator=(const std::string& text);

    std::size_t size() const;

    friend std::ostream& operator<<(std::ostream& ostr, const pesBuffer& buf);
    friend std::istream& operator>>(std::istream& istr, pesBuffer& buf);

    std::vector<char>::iterator begin();
    std::vector<char>::iterator end();
    std::vector<char>::const_iterator begin() const;
    std::vector<char>::const_iterator end() const;
    std::vector<char>::reverse_iterator rbegin();
    std::vector<char>::reverse_iterator rend();
    std::vector<char>::const_reverse_iterator rbegin() const;
    std::vector<char>::const_reverse_iterator rend() const;

    // deplicated in C++17 fixed by ChatGPT :)
    //struct Line : public std::iterator<std::forward_iterator_tag, Line> {
    //struct Line : public std::iterator<std::forward_iterator_tag, Line, ptrdiff_t, Line*, Line&> {        
    struct Line : public std::iterator_traits<Line> {        
        Line(std::vector<char>::iterator _begin, std::vector<char>::iterator _end);
        const std::string& operator*() const;
        const std::string* operator->() const;
        const std::string& asString() const;
        Line& operator++();
        Line operator++(int);
        bool operator!=(Line const& rhs) const;
        bool operator==(Line const& rhs) const;
        bool empty() const;

    private:
        std::string line;
        std::vector<char>::iterator _current, _begin, _end;
    };

    struct Lines {
        Lines(std::vector<char>::iterator begin, std::vector<char>::iterator end);
        Line begin();
        Line end();

    private:
        std::vector<char>::iterator _begin, _end;
    };

    Lines getLines();

private:
    std::vector<char> buffer;
    Line currentLine;
};

#endif /* pesBuffer_hpp */
