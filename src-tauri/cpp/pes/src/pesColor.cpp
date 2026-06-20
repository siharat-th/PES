//
//  pesColor.cpp
//  pesEngine
//
//  Created by SIHARAT THAMMAYA on 2/7/2562 BE.
//  Copyright © 2562 SIHARAT THAMMAYA. All rights reserved.
//

#include "pesColor.hpp"
#include <stdio.h>
#include <stdlib.h>
#include <float.h>
#include <algorithm>
#include <iostream>
#include <cmath>

// MARK: - pesBrotherThread
const pesBrotherThread brother_thread_table[] = {
    {pesColor(168, 168, 168), "Default",         "005"}, /* Index  0 */
    {pesColor( 14,  31, 124), "Prussian Blue",   "007"}, /* Index  1 */
    {pesColor( 10,  85, 163), "Blue",            "405"}, /* Index  2 */
    {pesColor( 48, 135, 119), "Teal Green",      "534"}, /* Index  3 */ /* TODO: Verify RGB value is correct */
    {pesColor( 75, 107, 175), "Cornflower Blue", "070"}, /* Index  4 */
    {pesColor(237,  23,  31), "Red",             "800"}, /* Index  5 */
    {pesColor(209,  92,   0), "Reddish Brown",   "337"}, /* Index  6 */
    {pesColor(145,  54, 151), "Magenta",         "620"}, /* Index  7 */
    {pesColor(228, 154, 203), "Light Lilac",     "810"}, /* Index  8 */
    {pesColor(145,  95, 172), "Lilac",           "612"}, /* Index  9 */
    {pesColor(157, 214, 125), "Mint Green",      "515"}, /* Index 10 */ /* TODO: Verify RGB value is correct */
    {pesColor(232, 169,   0), "Deep Gold",       "214"}, /* Index 11 */
    {pesColor(254, 186,  53), "Orange",          "208"}, /* Index 12 */
    {pesColor(255, 255,   0), "Yellow",          "205"}, /* Index 13 */
    {pesColor(112, 188,  31), "Lime Green",      "513"}, /* Index 14 */
    {pesColor(186, 152,   0), "Brass",           "328"}, /* Index 15 */
    {pesColor(168, 168, 168), "Silver",          "005"}, /* Index 16 */
    {pesColor(123, 111,   0), "Russet Brown",    "337"}, /* Index 17 */ /* TODO: Verify RGB value is correct */
    {pesColor(255, 255, 179), "Cream Brown",     "010"}, /* Index 18 */
    {pesColor( 95, 101, 121), "Pewter",          "704"}, /* Index 19 */
    {pesColor(  0,   0,   0), "Black",           "900"}, /* Index 20 */
    {pesColor( 11,  61, 145), "Ultramarine",     "406"}, /* Index 21 */
    {pesColor(119,   1, 118), "Royal Purple",    "869"}, /* Index 22 */
    {pesColor( 41,  49,  51), "Dark Gray",       "707"}, /* Index 23 */
    {pesColor( 42,  19,   1), "Dark Brown",      "058"}, /* Index 24 */
    {pesColor(246,  74, 138), "Deep Rose",       "086"}, /* Index 25 */
    {pesColor(178, 118,  36), "Light Brown",     "323"}, /* Index 26 */
    {pesColor(252, 187, 196), "Salmon Pink",     "079"}, /* Index 27 */ /* TODO: Verify RGB value is correct */
    {pesColor(254,  55,  15), "Vermillion",      "030"}, /* Index 28 */
    {pesColor(240, 240, 240), "White",           "001"}, /* Index 29 */
    {pesColor(106,  28, 138), "Violet",          "613"}, /* Index 30 */
    {pesColor(168, 221, 196), "Seacrest",        "542"}, /* Index 31 */
    {pesColor( 37, 132, 187), "Sky Blue",        "019"}, /* Index 32 */
    {pesColor(254, 179,  67), "Pumpkin",         "126"}, /* Index 33 */
    {pesColor(240, 231, 101), "Cream Yellow",    "812"}, /* Index 34 */
    {pesColor(208, 166,  96), "Khaki",           "348"}, /* Index 35 */
    {pesColor(209,  84,   0), "Clay Brown",      "339"}, /* Index 36 */
    {pesColor(102, 186,  73), "Leaf Green",      "509"}, /* Index 37 */
    {pesColor( 19,  74,  70), "Peacock Blue",    "415"}, /* Index 38 */
    {pesColor(110, 123, 119), "Gray",            "817"}, /* Index 39 */
    {pesColor(216, 202, 198), "Warm Gray",       "399"}, /* Index 40 */ /* TODO: Verify RGB value is correct */
    {pesColor( 67,  86,   7), "Dark Olive",      "517"}, /* Index 41 */
    {pesColor(240, 225, 198), "Linen",           "307"}, /* Index 42 */
    {pesColor(249, 147, 188), "Pink",            "085"}, /* Index 43 */
    {pesColor(  0,  56,  34), "Deep Green",      "808"}, /* Index 44 */
    {pesColor(178, 175, 212), "Lavender",        "804"}, /* Index 45 */
    {pesColor(104, 106, 176), "Wisteria Violet", "607"}, /* Index 46 */
    {pesColor(239, 227, 185), "Beige",           "843"}, /* Index 47 */
    {pesColor(247,  56, 102), "Carmine",         "807"}, /* Index 48 */
    {pesColor(181,  76, 100), "Amber Red",       "333"}, /* Index 49 */ /* TODO: Verify RGB value is correct */
    {pesColor( 19,  43,  26), "Olive Green",     "519"}, /* Index 50 */
    {pesColor(199,   1,  85), "Dark Fuschia",    "107"}, /* Index 51 */ /* TODO: Verify RGB value is correct */
    {pesColor(254, 158,  50), "Tangerine",       "209"}, /* Index 52 */
    {pesColor(168, 222, 235), "Light Blue",      "017"}, /* Index 53 */
    {pesColor(  0, 103,  26), "Emerald Green",   "507"}, /* Index 54 */ /* TODO: Verify RGB value is correct */
    {pesColor( 78,  41, 144), "Purple",          "614"}, /* Index 55 */
    {pesColor( 47, 126,  32), "Moss Green",      "515"}, /* Index 56 */
    {pesColor(254, 227, 197), "Flesh Pink",      "124"}, /* Index 57 */ /* TODO: Verify RGB value is correct */
    {pesColor(255, 217,  17), "Harvest Gold",    "206"}, /* Index 58 */
    {pesColor(  9,  91, 166), "Electric Blue",   "420"}, /* Index 59 */
    {pesColor(240, 249, 112), "Lemon Yellow",    "202"}, /* Index 60 */
    {pesColor(227, 243,  91), "Fresh Green",     "027"}, /* Index 61 */
    {pesColor(160, 160, 160), "Applique material","x9"}, /* Index 62 */ /* TODO: Verify RGB value is correct */
    {pesColor(160, 160, 160), "Applique position","x8"}, /* Index 63 */ /* TODO: Verify RGB value is correct */
    {pesColor(160, 160, 160), "Applique",         "x7"}, /* Index 64 */
    {pesColor(  0,   0,   0), "Original color",  "---"}  /* Index 65 */
};


const pesVec3f pesPaletteBrotherColorRGBf[66] = {
    brother_thread_table[ 0].color.toRGBf(),
    brother_thread_table[ 1].color.toRGBf(),
    brother_thread_table[ 2].color.toRGBf(),
    brother_thread_table[ 3].color.toRGBf(),
    brother_thread_table[ 4].color.toRGBf(),
    brother_thread_table[ 5].color.toRGBf(),
    brother_thread_table[ 6].color.toRGBf(),
    brother_thread_table[ 7].color.toRGBf(),
    brother_thread_table[ 8].color.toRGBf(),
    brother_thread_table[ 9].color.toRGBf(),
    brother_thread_table[10].color.toRGBf(),
    brother_thread_table[11].color.toRGBf(),
    brother_thread_table[12].color.toRGBf(),
    brother_thread_table[13].color.toRGBf(),
    brother_thread_table[14].color.toRGBf(),
    brother_thread_table[15].color.toRGBf(),
    brother_thread_table[16].color.toRGBf(),
    brother_thread_table[17].color.toRGBf(),
    brother_thread_table[18].color.toRGBf(),
    brother_thread_table[19].color.toRGBf(),
    brother_thread_table[20].color.toRGBf(),
    brother_thread_table[21].color.toRGBf(),
    brother_thread_table[22].color.toRGBf(),
    brother_thread_table[23].color.toRGBf(),
    brother_thread_table[24].color.toRGBf(),
    brother_thread_table[25].color.toRGBf(),
    brother_thread_table[26].color.toRGBf(),
    brother_thread_table[27].color.toRGBf(),
    brother_thread_table[28].color.toRGBf(),
    brother_thread_table[29].color.toRGBf(),
    brother_thread_table[30].color.toRGBf(),
    brother_thread_table[31].color.toRGBf(),
    brother_thread_table[32].color.toRGBf(),
    brother_thread_table[33].color.toRGBf(),
    brother_thread_table[34].color.toRGBf(),
    brother_thread_table[35].color.toRGBf(),
    brother_thread_table[36].color.toRGBf(),
    brother_thread_table[37].color.toRGBf(),
    brother_thread_table[38].color.toRGBf(),
    brother_thread_table[39].color.toRGBf(),
    brother_thread_table[40].color.toRGBf(),
    brother_thread_table[41].color.toRGBf(),
    brother_thread_table[42].color.toRGBf(),
    brother_thread_table[43].color.toRGBf(),
    brother_thread_table[44].color.toRGBf(),
    brother_thread_table[45].color.toRGBf(),
    brother_thread_table[46].color.toRGBf(),
    brother_thread_table[47].color.toRGBf(),
    brother_thread_table[48].color.toRGBf(),
    brother_thread_table[49].color.toRGBf(),
    brother_thread_table[50].color.toRGBf(),
    brother_thread_table[51].color.toRGBf(),
    brother_thread_table[52].color.toRGBf(),
    brother_thread_table[53].color.toRGBf(),
    brother_thread_table[54].color.toRGBf(),
    brother_thread_table[55].color.toRGBf(),
    brother_thread_table[56].color.toRGBf(),
    brother_thread_table[57].color.toRGBf(),
    brother_thread_table[58].color.toRGBf(),
    brother_thread_table[59].color.toRGBf(),
    brother_thread_table[60].color.toRGBf(),
    brother_thread_table[61].color.toRGBf(),
    brother_thread_table[62].color.toRGBf(),
    brother_thread_table[63].color.toRGBf(),
    brother_thread_table[64].color.toRGBf(),
    brother_thread_table[65].color.toRGBf()
};

const pesVec3f pesPaletteBrotherColorHSLf[66] = {
    RGBfToHSLf(pesPaletteBrotherColorRGBf[ 0]),
    RGBfToHSLf(pesPaletteBrotherColorRGBf[ 1]),
    RGBfToHSLf(pesPaletteBrotherColorRGBf[ 2]),
    RGBfToHSLf(pesPaletteBrotherColorRGBf[ 3]),
    RGBfToHSLf(pesPaletteBrotherColorRGBf[ 4]),
    RGBfToHSLf(pesPaletteBrotherColorRGBf[ 5]),
    RGBfToHSLf(pesPaletteBrotherColorRGBf[ 6]),
    RGBfToHSLf(pesPaletteBrotherColorRGBf[ 7]),
    RGBfToHSLf(pesPaletteBrotherColorRGBf[ 8]),
    RGBfToHSLf(pesPaletteBrotherColorRGBf[ 9]),
    RGBfToHSLf(pesPaletteBrotherColorRGBf[10]),
    RGBfToHSLf(pesPaletteBrotherColorRGBf[11]),
    RGBfToHSLf(pesPaletteBrotherColorRGBf[12]),
    RGBfToHSLf(pesPaletteBrotherColorRGBf[13]),
    RGBfToHSLf(pesPaletteBrotherColorRGBf[14]),
    RGBfToHSLf(pesPaletteBrotherColorRGBf[15]),
    RGBfToHSLf(pesPaletteBrotherColorRGBf[16]),
    RGBfToHSLf(pesPaletteBrotherColorRGBf[17]),
    RGBfToHSLf(pesPaletteBrotherColorRGBf[18]),
    RGBfToHSLf(pesPaletteBrotherColorRGBf[19]),
    RGBfToHSLf(pesPaletteBrotherColorRGBf[20]),
    RGBfToHSLf(pesPaletteBrotherColorRGBf[21]),
    RGBfToHSLf(pesPaletteBrotherColorRGBf[22]),
    RGBfToHSLf(pesPaletteBrotherColorRGBf[23]),
    RGBfToHSLf(pesPaletteBrotherColorRGBf[24]),
    RGBfToHSLf(pesPaletteBrotherColorRGBf[25]),
    RGBfToHSLf(pesPaletteBrotherColorRGBf[26]),
    RGBfToHSLf(pesPaletteBrotherColorRGBf[27]),
    RGBfToHSLf(pesPaletteBrotherColorRGBf[28]),
    RGBfToHSLf(pesPaletteBrotherColorRGBf[29]),
    RGBfToHSLf(pesPaletteBrotherColorRGBf[30]),
    RGBfToHSLf(pesPaletteBrotherColorRGBf[31]),
    RGBfToHSLf(pesPaletteBrotherColorRGBf[32]),
    RGBfToHSLf(pesPaletteBrotherColorRGBf[33]),
    RGBfToHSLf(pesPaletteBrotherColorRGBf[34]),
    RGBfToHSLf(pesPaletteBrotherColorRGBf[35]),
    RGBfToHSLf(pesPaletteBrotherColorRGBf[36]),
    RGBfToHSLf(pesPaletteBrotherColorRGBf[37]),
    RGBfToHSLf(pesPaletteBrotherColorRGBf[38]),
    RGBfToHSLf(pesPaletteBrotherColorRGBf[39]),
    RGBfToHSLf(pesPaletteBrotherColorRGBf[40]),
    RGBfToHSLf(pesPaletteBrotherColorRGBf[41]),
    RGBfToHSLf(pesPaletteBrotherColorRGBf[42]),
    RGBfToHSLf(pesPaletteBrotherColorRGBf[43]),
    RGBfToHSLf(pesPaletteBrotherColorRGBf[44]),
    RGBfToHSLf(pesPaletteBrotherColorRGBf[45]),
    RGBfToHSLf(pesPaletteBrotherColorRGBf[46]),
    RGBfToHSLf(pesPaletteBrotherColorRGBf[47]),
    RGBfToHSLf(pesPaletteBrotherColorRGBf[48]),
    RGBfToHSLf(pesPaletteBrotherColorRGBf[49]),
    RGBfToHSLf(pesPaletteBrotherColorRGBf[50]),
    RGBfToHSLf(pesPaletteBrotherColorRGBf[51]),
    RGBfToHSLf(pesPaletteBrotherColorRGBf[52]),
    RGBfToHSLf(pesPaletteBrotherColorRGBf[53]),
    RGBfToHSLf(pesPaletteBrotherColorRGBf[54]),
    RGBfToHSLf(pesPaletteBrotherColorRGBf[55]),
    RGBfToHSLf(pesPaletteBrotherColorRGBf[56]),
    RGBfToHSLf(pesPaletteBrotherColorRGBf[57]),
    RGBfToHSLf(pesPaletteBrotherColorRGBf[58]),
    RGBfToHSLf(pesPaletteBrotherColorRGBf[59]),
    RGBfToHSLf(pesPaletteBrotherColorRGBf[60]),
    RGBfToHSLf(pesPaletteBrotherColorRGBf[61]),
    RGBfToHSLf(pesPaletteBrotherColorRGBf[62]),
    RGBfToHSLf(pesPaletteBrotherColorRGBf[63]),
    RGBfToHSLf(pesPaletteBrotherColorRGBf[64]),
    RGBfToHSLf(pesPaletteBrotherColorRGBf[65])
};

//const float C1P3 = 1.0f / 3.0f;
//const float C2P3 = 2.0f / 3.0f;

pesVec3f RGBfToHSLf(const pesVec3f& rgb) {
    pesVec3f hsl(0, 0, 0);
    auto& h = hsl.x;
    auto& s = hsl.y;
    auto& l = hsl.z;

    const auto& r = rgb.x;
    const auto& g = rgb.y;
    const auto& b = rgb.z;

    //float min = std::min(std::min(r, g), b);
    //float max = std::max(std::max(r, g), b);
    //float delta = max - min;
    //l = (max + min) / 2;
    //if (delta == 0) {
    //    h = s = 0.0f;
    //} 
    //else {
    //    s = (l <= 0.5) ? (delta / (max + min)) : (delta / (2 - max - min));
    //    if (r == max) {
    //        h = ((g - b) / 6) / delta;
    //    } 
    //    else if (g == max) {
    //        h = C1P3 + ((b - r) / 6) / delta;
    //    } 
    //    else {
    //        h = C2P3 + ((r - g) / 6) / delta;
    //    }
    //    if (h < 0) h += 1;
    //    if (h > 1) h -= 1;
    //}

    float mx = std::max(std::max(r, g), b), 
        mn = std::min(std::min(r, g), b), 
        d = mx - mn,
        invd = 1.0f / d, 
        g_lt_b = g < b ? 6.0f : 0.0f;
    h = (1 / 6.0f) * mx == mn ? 0.0f
        : mx == r             ? invd * (g - b) + g_lt_b
        : mx == g             ? invd * (b - r) + 2.0f
                              : invd * (r - g) + 4.0f;
    float sum = mx + mn;
    l = sum * 0.5f;
    s = mx == mn ? 0.0f : (d / (l > 0.5f ? (2.0f - sum) : sum));
    h /= 6.0f;
    return hsl;
}

pesVec3f RGBToHSLf(uint8_t r, uint8_t g, uint8_t b) {
    return RGBfToHSLf(pesVec3f(r / 255.0f, g / 255.0f, b / 255.0f));
}

float hue_to_rgb(float hue, float s, float l){
    float x = s * (1.0f - std::fabs(l + l - 1.0f));
    float q = std::fabs(6.0f * (hue - std::floor(hue)) - 3.0f) - 1.0f;
    return x * (std::max(0.0f, std::min(q, 1.0f)) - 0.5f) + l;
}

pesVec3f HSLfToRGBf(const pesVec3f& hsl) {
    //auto HueToRGB = [](float v1, float v2, float vH) -> float {
    //    if (vH < 0) vH += 1;
    //    if (vH > 1) vH -= 1;
    //    if ((6 * vH) < 1) return (v1 + (v2 - v1) * 6 * vH);
    //    if ((2 * vH) < 1) return v2;
    //    if ((3 * vH) < 2) return (v1 + (v2 - v1) * (C2P3 - vH) * 6);
    //    return v1;
    //};

    const auto& h = hsl.x;
    const auto& s = hsl.y;
    const auto& l = hsl.z;

    pesVec3f rgb(0, 0, 0);
    auto& r = rgb.x;
    auto& g = rgb.y;
    auto& b = rgb.z;

    //if (s == 0) {
    //    r = g = b = l;
    //} 
    //else {
    //    float v2 = (l < 0.5f) ? (l * (1 + s)) : ((l + s) - (l * s));
    //    float v1 = 2 * l - v2;
    //    r = HueToRGB(v1, v2, h + C1P3);
    //    g = HueToRGB(v1, v2, h);
    //    b = HueToRGB(v1, v2, h - C1P3);
    //}

    float x = s * (1.0f - abs(l + l - 1.0f));

//    auto hue_to_rgb = [&, l = l](auto hue) { // Compiler error on Mac don't know why (Pom)
//        auto q = abs(6.0f * (hue - floor(hue)) - 3.0f) - 1.0f;
//        return x * (std::max(0.0f, std::min(q, 1.0f)) - 0.5f) + l;
//    };

    r = hue_to_rgb(h + 0 / 3.0f, s, l);
    g = hue_to_rgb(h + 2 / 3.0f, s, l);
    b = hue_to_rgb(h + 1 / 3.0f, s, l);

    return rgb;
}

//struct Color {
//    float r, g, b, a;
//    explicit operator bool() const { return r && g && b && a; }
//};
//struct HSLA {
//    float h, s, l, a;
//    explicit operator bool() const { return h && s && l && a; }
//};
//HSLA to_hsla(Color c) {
//    float mx = std::max(std::max(c.r, c.g), c.b), 
//        mn = std::min(std::min(c.r, c.g), c.b),
//        d = mx - mn,
//        invd = 1.0f / d,
//        g_lt_b = c.g < c.b? 6.0f : 0.0f;
//    float h = (1 / 6.0f) * (mx == mn)? 0.0f : (mx == c.r)? invd * (c.g - c.b) + g_lt_b : (mx == c.g)? invd * (c.b - c.r) + 2.0f: invd * (c.r - c.g) + 4.0f;
//    float sum = mx + mn, l = sum * 0.5f, s = (mx == mn)? 0.0f: d / (l > 0.5f)? 2.0f - sum: sum;
//    return {h, s, l, c.a};
//}
//Color to_rgba(HSLA c) {
//    auto [h, s, l, a] = c;
//    float x = s * (1.0f - abs(l + l - 1.0f));
//    auto hue_to_rgb = [&, l = l](auto hue) {
//        auto q = abs(6.0f * (hue - floor(hue)) - 3.0f) - 1.0f;
//        return x * (std::max(0.0f, std::min(q, 1.0f)) - 0.5f) + l;
//    };
//    return {
//            hue_to_rgb(h + 0 / 3.0f),
//            hue_to_rgb(h + 2 / 3.0f),
//            hue_to_rgb(h + 1 / 3.0f),
//            c.a,
//    };
//}

pesColor::pesColor(){
    set(0xff, 0xff, 0xff, 0xff);
}

pesColor::pesColor(unsigned char r, unsigned char g, unsigned char b, unsigned char a){
    set(r, g, b, a);
}

pesColor::pesColor(const pesColor& c){
    set(c.r, c.g, c.b, c.a); 
}

void pesColor::set(unsigned char red,
                   unsigned char green,
                   unsigned char blue,
                   unsigned char alpha) {
    r = red;
    g = green;
    b = blue;
    a = alpha;
}

void pesColor::setHex(int hexColor, float alpha){
    r = (hexColor >> 16) & 0xff;
    g = (hexColor >> 8) & 0xff;
    b = (hexColor >> 0) & 0xff;
    a = alpha;
}

int pesColor::getHex() const {
    return
    ((0xff & (unsigned char) r) << 16) |
    ((0xff & (unsigned char) g) << 8) |
    ((0xff & (unsigned char) b));
}

void pesColor::setHexARGB(uint32_t hexColor) {
    if (hexColor > uint32_t(0xffffff)) {
        a = hexColor >> 24;
    }
    r = hexColor << 8 >> 24;
    g = hexColor << 16 >> 24;
    b = hexColor & 0xff;
}

uint32_t pesColor::getHexARGB() const {
    //return ((a << 8 | r) << 8 | g) << 8 | b;
    return (a << 24) | (r << 16) | (g << 8) | b;
}

pesVec3f pesColor::toRGBf() const {
    return pesVec3f(r / 255.0f, g / 255.0f, b / 255.0f);
}

pesVec3f pesColor::toHSLf() const {
    return RGBfToHSLf(toRGBf());
}

bool pesColor::isEqual(pesColor other){
    return other.r==r && other.g==g && other.b==b && other.a==a;
}


// int pesGetNearestBrotherColorIndex(const pesColor& srcColor) {
//     double currentClosestValue = 9999999;
//     int closestIndex = -1;
//     int red = srcColor.r;
//     int green = srcColor.g;
//     int blue = srcColor.b;
//     for (int i = 1; i <= 61; i++) {
//         int deltaRed;
//         int deltaBlue;
//         int deltaGreen;
//         double dist;
//         pesColor c = pesColor(pesGetBrotherColor(i));

//         deltaRed = red - c.r;
//         deltaBlue = green - c.g;
//         deltaGreen = blue - c.b;

//         dist = sqrt((double)(deltaRed * deltaRed) + (deltaBlue * deltaBlue) +
//                     (deltaGreen * deltaGreen));
//         if (dist <= currentClosestValue) {
//             currentClosestValue = dist;
//             closestIndex = i;
//         }
//     }

//     return (closestIndex);
// }
// Cleaned by Github Copilot - GPT 4o
int pesGetNearestBrotherColorIndex(const pesColor& srcColor) {
    double currentClosestValue = std::numeric_limits<double>::max();
    int closestIndex = -1;
    int red = srcColor.r;
    int green = srcColor.g;
    int blue = srcColor.b;

    for (int i = 1; i <= 61; i++) {
        pesColor c = pesColor(pesGetBrotherColor(i));

        int deltaRed = red - c.r;
        int deltaGreen = green - c.g;
        int deltaBlue = blue - c.b;

        double dist = sqrt(static_cast<double>(deltaRed * deltaRed) +
                           static_cast<double>(deltaGreen * deltaGreen) +
                           static_cast<double>(deltaBlue * deltaBlue));

        if (dist <= currentClosestValue) {
            currentClosestValue = dist;
            closestIndex = i;
        }
    }

    return closestIndex;
}

// int pesGetNearestBrotherColorRGBfIndex(const pesColor& srcColor) {
//     auto dist = FLT_MAX, mindist = FLT_MAX;
//     int closestIndex = 65;
//     auto srcRGBf = srcColor.toRGBf();
//     for (int i = 1; i <= 62; i++) {
//         dist = srcRGBf.distance(pesPaletteBrotherColorRGBf[i]);
//         if (dist < mindist) {
//             mindist = dist;
//             closestIndex = i;
//         }
//     }
//     return closestIndex;
// }
// Cleaned by Github Copilot - GPT 4o
int pesGetNearestBrotherColorRGBfIndex(const pesColor& srcColor) {
    float mindist = std::numeric_limits<float>::max();
    int closestIndex = 65;
    auto srcRGBf = srcColor.toRGBf();
    for (int i = 1; i <= 62; i++) {
        float dist = srcRGBf.distance(pesPaletteBrotherColorRGBf[i]);
        if (dist < mindist) {
            mindist = dist;
            closestIndex = i;
        }
    }
    return closestIndex;
}

// int pesGetNearestBrotherColorHSLfIndex(const pesColor& srcColor) {
//     auto mindist = FLT_MAX;
//     int closestIndex = 65;
//     auto srcHSLf = srcColor.toHSLf();
//     srcHSLf.x = srcHSLf.x;
//     srcHSLf.y = srcHSLf.y;
//     srcHSLf.z = srcHSLf.z;
//     for (int i = 1; i <= 62; i++) {
//         //float dist = srcHLSf.distance(pesPaletteBrotherColorHSLf[i]);    
//         auto HSLf = pesPaletteBrotherColorHSLf[i];
//         HSLf.x = HSLf.x;
//         HSLf.y = HSLf.y;
//         HSLf.z = HSLf.z;
//         float dist = srcHSLf.distance(HSLf);
//         if (dist < mindist) {
//             mindist = dist;
//             closestIndex = i;
//         }
//     }
//     return closestIndex;
// }
// Cleaned by Github Copilot - GPT 4o
int pesGetNearestBrotherColorHSLfIndex(const pesColor& srcColor) {
    float mindist = FLT_MAX;
    int closestIndex = 65;
    auto srcHSLf = srcColor.toHSLf();
    for (int i = 1; i <= 62; i++) {
        float dist = srcHSLf.distance(pesPaletteBrotherColorHSLf[i]);
        if (dist < mindist) {
            mindist = dist;
            closestIndex = i;
        }
    }
    return closestIndex;
}

pesColor pesGetBrotherColor(int index){
    if(index<1 || index>65)
        index = 0;
    pesBrotherThread pesThread = pesGetBrotherThread(index);
    return pesThread.color;
}

const char* pesGetBrotherColorName(int index){
    if(index<1 || index>65)
        index = 0;
    pesBrotherThread pesThread = pesGetBrotherThread(index);
    return pesThread.description;
}

const char* pesGetBrotherCatalogNumber(int index){
    if(index<1 || index>65)
        index = 0;
    pesBrotherThread pesThread = pesGetBrotherThread(index);
    return pesThread.catalogNumber;
}

static char __brother_thread_name[66][32];
const char* pesGetBrotherColorNameWithCatalogNumber(int index){
    pesBrotherThread pesThread = pesGetBrotherThread(index);
    if (index < 1 || index>65)
        index = 0;
    if(pesThread.catalogNumber[0]!='x'){
        sprintf(__brother_thread_name[index], "%s (%s)", pesThread.description, pesThread.catalogNumber);
        return __brother_thread_name[index];
    }
    return pesThread.description;
}

pesBrotherThread pesGetBrotherThread(int index){
    if(index<1 || index>65)
        index = 0;
    return brother_thread_table[index];
}
