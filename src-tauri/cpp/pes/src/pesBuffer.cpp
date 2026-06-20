//
//  pesBuffer.cpp
//  pesEngine
//
//  Created by SIHARAT THAMMAYA on 11/7/2562 BE.
//  Copyright © 2562 SIHARAT THAMMAYA. All rights reserved.
//

#include "pesBuffer.hpp"
#include <cstring>
#include <fstream>

using namespace std;

//--------------------------------------------------
pesBuffer::pesBuffer() : currentLine(end(), end()) {}

//--------------------------------------------------
pesBuffer::pesBuffer(const char* _buffer, std::size_t size)
        : buffer(_buffer, _buffer + size), currentLine(end(), end()) {}

//--------------------------------------------------
pesBuffer::pesBuffer(const string& text)
        : buffer(text.begin(), text.end()), currentLine(end(), end()) {}

//--------------------------------------------------
pesBuffer::pesBuffer(istream& stream, size_t ioBlockSize) : currentLine(end(), end()) {
    set(stream, ioBlockSize);
}

//--------------------------------------------------
bool pesBuffer::set(istream& stream, size_t ioBlockSize) {
    if (stream.bad()) {
        clear();
        return false;
    } else {
        buffer.clear();
    }

    vector<char> aux_buffer(ioBlockSize);
    while (stream.good()) {
        stream.read(&aux_buffer[0], ioBlockSize);
        append(aux_buffer.data(), (size_t)stream.gcount());
    }
    return true;
}

//--------------------------------------------------
void pesBuffer::setall(char mem) { buffer.assign(buffer.size(), mem); }

//--------------------------------------------------
bool pesBuffer::writeTo(ostream& stream) const {
    if (stream.bad()) {
        return false;
    }
    stream.write(buffer.data(), buffer.size());
    return stream.good();
}

//--------------------------------------------------
void pesBuffer::set(const char* _buffer, std::size_t _size) {
    buffer.assign(_buffer, _buffer + _size);
}

//--------------------------------------------------
void pesBuffer::set(const string& text) { set(text.c_str(), text.size()); }

//--------------------------------------------------
void pesBuffer::append(const string& _buffer) { append(_buffer.c_str(), _buffer.size()); }

//--------------------------------------------------
void pesBuffer::append(const char* _buffer, std::size_t _size) {
    buffer.insert(buffer.end(), _buffer, _buffer + _size);
}

void pesBuffer::append(int _ivalue) {
    const char* _buffer = (const char*)&_ivalue;
    append(_buffer, 4);
}

void pesBuffer::append(unsigned int _ivalue) {
    const char* _buffer = (const char*)&_ivalue;
    append(_buffer, 4);
}

void pesBuffer::append(short _svalue) {
    const char* _buffer = (const char*)&_svalue;
    append(_buffer, 2);
}

void pesBuffer::append(unsigned short _svalue) {
    const char* _buffer = (const char*)&_svalue;
    append(_buffer, 2);
}

void pesBuffer::append(float _fvalue) {
    union {
        float f32;
        unsigned int u32;
    } float_int_u;
    float_int_u.f32 = _fvalue;
    append(float_int_u.u32);
}

//--------------------------------------------------
void pesBuffer::appendU32(uint32_t u32) {
    uint8_t buf[4];
    buf[0] = u32 & 0xFF;
    buf[1] = (u32 & 0xFF00) >> 8;
    buf[2] = (u32 & 0xFF0000) >> 16;
    buf[3] = (u32 & 0xFF000000) >> 24;

    append((const char*)buf, 4);
}

void pesBuffer::appendU16(uint16_t u16) {
    uint8_t buf[2];
    buf[0] = u16 & 0xFF;
    buf[1] = (u16 & 0xFF00) >> 8;

    append((const char*)&buf[0], 1);
    append((const char*)&buf[1], 1);
}

void pesBuffer::appendS16(int16_t s16) {
    int16_t buf[2];
    buf[0] = s16 & 0xFF;
    buf[1] = (s16 & 0xFF00) >> 8;

    append((const char*)&buf[0], 1);
    append((const char*)&buf[1], 1);
}

void pesBuffer::appendU8(uint8_t u8) { append((const char*)&u8, 1); }

void pesBuffer::appendU8(uint8_t u8, int num) {
    for (int i = 0; i < num; i++) {
        append((const char*)&u8, 1);
    }
}

void pesBuffer::appendString(const char* str) {
    int len = (int)strlen(str);

    append((const char*)str, len);
}

void pesBuffer::appendPESString(const char* str) {
    unsigned char buf[257];

    int len = (int)strlen(str);
    if (len > 255) len = 255;

    buf[0] = len;
    memcpy(&buf[1], str, len);

    append((const char*)buf, len + 1);
}

void pesBuffer::appendPESString2(const char* str) {
    unsigned char buf[258];

    int len = (int)strlen(str);
    if (len > 255) len = 255;

    buf[0] = len & 0xFF;
    buf[1] = (len & 0xFF00) >> 8;
    memcpy(&buf[2], str, len);

    append((const char*)buf, len + 2);
}

//--------------------------------------------------
void pesBuffer::reserve(size_t size) { buffer.reserve(size); }

//--------------------------------------------------
void pesBuffer::clear() { buffer.clear(); }

//--------------------------------------------------
void pesBuffer::allocate(std::size_t _size) { resize(_size); }

//--------------------------------------------------
void pesBuffer::resize(std::size_t _size) { buffer.resize(_size); }

//--------------------------------------------------
char* pesBuffer::getData() { return buffer.data(); }

//--------------------------------------------------
const char* pesBuffer::getData() const { return buffer.data(); }

//--------------------------------------------------
string pesBuffer::getText() const {
    if (buffer.empty()) {
        return "";
    }
    return std::string(buffer.begin(), buffer.end());
}

//--------------------------------------------------
pesBuffer::operator string() const { return getText(); }

//--------------------------------------------------
pesBuffer& pesBuffer::operator=(const string& text) {
    set(text);
    return *this;
}

//--------------------------------------------------
std::size_t pesBuffer::size() const { return buffer.size(); }

//--------------------------------------------------
vector<char>::iterator pesBuffer::begin() { return buffer.begin(); }

//--------------------------------------------------
vector<char>::iterator pesBuffer::end() { return buffer.end(); }

//--------------------------------------------------
vector<char>::const_iterator pesBuffer::begin() const { return buffer.begin(); }

//--------------------------------------------------
vector<char>::const_iterator pesBuffer::end() const { return buffer.end(); }

//--------------------------------------------------
vector<char>::reverse_iterator pesBuffer::rbegin() { return buffer.rbegin(); }

//--------------------------------------------------
vector<char>::reverse_iterator pesBuffer::rend() { return buffer.rend(); }

//--------------------------------------------------
vector<char>::const_reverse_iterator pesBuffer::rbegin() const { return buffer.rbegin(); }

//--------------------------------------------------
vector<char>::const_reverse_iterator pesBuffer::rend() const { return buffer.rend(); }

//--------------------------------------------------
pesBuffer::Line::Line(vector<char>::iterator _begin, vector<char>::iterator _end)
        : _current(_begin), _begin(_begin), _end(_end) {
    if (_begin == _end) {
        line = "";
        return;
    }

    bool lineEndWasCR = false;

    while (_current != _end && *_current != '\n') {
        if (*_current == '\r') {
            lineEndWasCR = true;
            break;
        } else if (*_current == 0 && _current + 1 == _end) {
            break;
        } else {
            _current++;
        }
    }

    line = string(_begin, _current);

    if (_current != _end) {
        _current++;
    }

    if (lineEndWasCR && _current != _end && *_current == '\n') {
        _current++;
    }
}

//--------------------------------------------------
const string& pesBuffer::Line::operator*() const { return line; }

//--------------------------------------------------
const string* pesBuffer::Line::operator->() const { return &line; }

//--------------------------------------------------
const string& pesBuffer::Line::asString() const { return line; }

//--------------------------------------------------
pesBuffer::Line& pesBuffer::Line::operator++() {
    *this = Line(_current, _end);
    return *this;
}

//--------------------------------------------------
pesBuffer::Line pesBuffer::Line::operator++(int) {
    Line tmp(*this);
    operator++();
    return tmp;
}

//--------------------------------------------------
bool pesBuffer::Line::operator!=(Line const& rhs) const {
    return rhs._begin != _begin || rhs._end != _end;
}

//--------------------------------------------------
bool pesBuffer::Line::operator==(Line const& rhs) const {
    return rhs._begin == _begin && rhs._end == _end;
}

bool pesBuffer::Line::empty() const { return _begin == _end; }

//--------------------------------------------------
pesBuffer::Lines::Lines(vector<char>::iterator begin, vector<char>::iterator end)
        : _begin(begin), _end(end) {}

//--------------------------------------------------
pesBuffer::Line pesBuffer::Lines::begin() { return Line(_begin, _end); }

//--------------------------------------------------
pesBuffer::Line pesBuffer::Lines::end() { return Line(_end, _end); }

//--------------------------------------------------
pesBuffer::Lines pesBuffer::getLines() { return pesBuffer::Lines(begin(), end()); }

//--------------------------------------------------
ostream& operator<<(ostream& ostr, const pesBuffer& buf) {
    buf.writeTo(ostr);
    return ostr;
}

//--------------------------------------------------
istream& operator>>(istream& istr, pesBuffer& buf) {
    buf.set(istr);
    return istr;
}

pesBuffer pesBufferFromFile(const string& path, bool binary) {
    ios_base::openmode binary_mode = binary ? ios::binary : (ios_base::openmode)0;
    fstream fs;
    fs.open(path.c_str(), ios::in | binary_mode);
    return pesBuffer(fs);
}

bool pesBufferToFile(const string& path, pesBuffer& buffer, bool binary) {
    ios_base::openmode binary_mode = binary ? ios::binary : (ios_base::openmode)0;
    fstream fs;
    fs.open(path.c_str(), ios::out | binary_mode);
    return buffer.writeTo(fs);
}
