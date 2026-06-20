//
//  UnicodeHelper.c
//  PES3
//
//  Created by SIHARAT THAMMAYA on 9/11/2561 BE.
//  Copyright © 2561 Jimmy Software Co., Ltd. All rights reserved.
//

#include "UnicodeHelper.h"

// return true if c is thai letter
bool th_isThaiLetter(uint32_t c)
{
    return c>=THAI_KO_KAI && c<=THAI_KHOMUT;
}

// return true if c is thai consonant
bool th_isConsonant(uint32_t c)
{
    return c>=THAI_KO_KAI && c<=THAI_HO_NOKHUK; // ก ข ... ฮ
}

// return true if c is thai leading vowel
bool th_isLeadingvowel(uint32_t c)
{
    return c>=THAI_SARA_E && c<=THAI_SARA_AI_MAIMALAI; // เ แ โ ใ ไ
}

// return true if c is thai above vowel
bool th_isAboveVowel(uint32_t c)
{
    return c>=THAI_SARA_I && c<=THAI_SARA_UEE;  //
}

// return true if c is thai lower vowel
bool th_isBelowVowel(uint32_t c)
{
    return c>=THAI_SARA_U && c<=THAI_PHINTHU;  //
}

// return true if c is thai tone
bool th_isTone(uint32_t c)
{
    return c>=THAI_MAI_EK && c<=THAI_MAI_CHATTAWA; //
}

bool th_isNormalConsonantAndDiacritic(uint32_t c)
{
    return (c>=THAI_KO_KAI && c<=THAI_CHO_CHOE) || (c>=THAI_THO_NANGMONTHO && c<=THAI_BO_BAIMAI) ||
            c==THAI_PHO_PHUNG || c==THAI_PHO_PHAN || (c>=THAI_PHO_SAMPHAO && c<=THAI_PAIYANNOI);
}

bool th_isAscenderConsonant(uint32_t c)            // Ascender consonants (ป, ฝ, ฟ)
{
    return c==THAI_PO_PLA || c==THAI_FO_FA || c==THAI_FO_FAN;
}

bool th_isDescenderConsonant(uint32_t c)           // Strict Descender consonants (ฎ, ฏ)
{
    return c==THAI_DO_CHADA || c==THAI_TO_PATAK;
}

bool th_isRemovableDescenderConsonant(uint32_t c)  // Removable descender consonants (ญ, ฐ)
{
    return c==THAI_YO_YING || c==THAI_THO_THAN;
}

bool th_isAboveVowelAndDiacritic(uint32_t c)       // (◌ั, ◌ิ, ◌ี, ◌ึ, ◌ื, ◌็, ◌ํ)
{
    return c==THAI_MAI_HAN_AKAT || th_isAboveVowel(c) || c==THAI_MAITAIKHU || c==THAI_NIKHAHIT;
}

bool th_isBelowVowelAndDiacritic(uint32_t c)       // (◌ุ, ◌ู, ◌ฺ)
{
    return th_isBelowVowel(c);
}

bool th_isToneAndDiacritic(uint32_t c)             // Above tone/diacritic ( ่, ้, ๊, ๋, ์, ◌๎)
{
    return th_isTone(c) || c==THAI_THANTHAKHAT || c==THAI_YAMAKKAN;
}


