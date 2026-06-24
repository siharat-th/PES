//
//  PesUnicodeUtils.hpp
//  sources
//
//  Created by SIHARAT THAMMAYA on 23/9/2565 BE.
//

#ifndef PesUnicodeUtils_hpp
#define PesUnicodeUtils_hpp

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string>
#include "utf8cpp/include/utf8.h"

#define THAI_KO_KAI             0xe01u
#define THAI_KHO_KHAI           0xe02u
#define THAI_KHO_KHUAT          0xe03u
#define THAI_KHO_KHWAI          0xe04u
#define THAI_KHO_KHON           0xe05u
#define THAI_KHO_RAKHANG        0xe06u
#define THAI_NGO_NGU            0xe07u
#define THAI_CHO_CHAN           0xe08u
#define THAI_CHO_CHING          0xe09u
#define THAI_CHO_CHANG          0xe0au
#define THAI_SO_SO              0xe0bu
#define THAI_CHO_CHOE           0xe0cu
#define THAI_YO_YING            0xe0du
#define THAI_DO_CHADA           0xe0eu
#define THAI_TO_PATAK           0xe0fu
#define THAI_THO_THAN           0xe10u
#define THAI_THO_NANGMONTHO     0xe11u
#define THAI_THO_PHUTHAO        0xe12u
#define THAI_NO_NEN             0xe13u
#define THAI_DO_DEK             0xe14u
#define THAI_TO_TAO             0xe15u
#define THAI_THO_THUNG          0xe16u
#define THAI_THO_THAHAN         0xe17u
#define THAI_THO_THONG          0xe18u
#define THAI_NO_NU              0xe19u
#define THAI_BO_BAIMAI          0xe1au
#define THAI_PO_PLA             0xe1bu
#define THAI_PHO_PHUNG          0xe1cu
#define THAI_FO_FA              0xe1du
#define THAI_PHO_PHAN           0xe1eu
#define THAI_FO_FAN             0xe1fu
#define THAI_PHO_SAMPHAO        0xe20u
#define THAI_MO_MA              0xe21u
#define THAI_YO_YAK             0xe22u
#define THAI_RO_RUA             0xe23u
#define THAI_RU                 0xe24u
#define THAI_LO_LING            0xe25u
#define THAI_LU                 0xe26u
#define THAI_WO_WAEN            0xe27u
#define THAI_SO_SALA            0xe28u
#define THAI_SO_RUSI            0xe29u
#define THAI_SO_SUA             0xe2au
#define THAI_HO_HIP             0xe2bu
#define THAI_LO_CHULA           0xe2cu
#define THAI_O_ANG              0xe2du
#define THAI_HO_NOKHUK          0xe2eu

#define THAI_PAIYANNOI          0xe2fu

#define THAI_SARA_A             0xe30u
#define THAI_MAI_HAN_AKAT       0xe31u
#define THAI_SARA_AA            0xe32u
#define THAI_SARA_AM            0xe33u
#define THAI_SARA_I             0xe34u
#define THAI_SARA_II            0xe35u
#define THAI_SARA_UE            0xe36u
#define THAI_SARA_UEE           0xe37u
#define THAI_SARA_U             0xe38u
#define THAI_SARA_UU            0xe39u
#define THAI_PHINTHU            0xe3au

#define THAI_SYMBOL_BAHT        0xe3fu

#define THAI_SARA_E             0xe40u
#define THAI_SARA_AE            0xe41u
#define THAI_SARA_O             0xe42u
#define THAI_SARA_AI_MAIMUAN    0xe43u
#define THAI_SARA_AI_MAIMALAI   0xe44u
#define THAI_LAKKHANGYAO        0xe45u
#define THAI_MAIYAMOK           0xe46u
#define THAI_MAITAIKHU          0xe47u

#define THAI_MAI_EK             0xe48u
#define THAI_MAI_THO            0xe49u
#define THAI_MAI_TRI            0xe4au
#define THAI_MAI_CHATTAWA       0xe4bu

#define THAI_THANTHAKHAT        0xe4cu
#define THAI_NIKHAHIT           0xe4du
#define THAI_YAMAKKAN           0xe4eu

#define THAI_FONGMAN            0xe4fu

#define THAI_DIGIT_ZERO         0xe50u
#define THAI_DIGIT_ONE          0xe51u
#define THAI_DIGIT_TWO          0xe52u
#define THAI_DIGIT_THREE        0xe53u
#define THAI_DIGIT_FOUR         0xe54u
#define THAI_DIGIT_FIVE         0xe55u
#define THAI_DIGIT_SIX          0xe56u
#define THAI_DIGIT_SEVEN        0xe57u
#define THAI_DIGIT_EIGHT        0xe58u
#define THAI_DIGIT_NINE         0xe59u

#define THAI_ANGKHANKHU         0xe5au
#define THAI_KHOMUT             0xe5bu

// Win PUA
#define THAI_THO_THAN_NONE_TAIL           0xf700u
#define THAI_YO_YING_NONE_TAIL            0xf70fu

bool th_isThaiLetter(uint32_t c);
bool th_isConsonant(uint32_t c);
bool th_isTone(uint32_t c);
bool th_isLeadingvowel(uint32_t c);
bool th_isAboveVowel(uint32_t c);
bool th_isBelowVowel(uint32_t c);

bool th_isNormalConsonantAndDiacritic(uint32_t c);
bool th_isAscenderConsonant(uint32_t c);            // Ascender consonants (ป, ฝ, ฟ)
bool th_isDescenderConsonant(uint32_t c);           // Strict Descender consonants (ฎ, ฏ)
bool th_isRemovableDescenderConsonant(uint32_t c);  // Removable descender consonants (ญ, ฐ)
bool th_isAboveVowelAndDiacritic(uint32_t c);       // (◌ั, ◌ิ, ◌ี, ◌ึ, ◌ื, ◌็, ◌ํ)
bool th_isBelowVowelAndDiacritic(uint32_t c);       // (◌ุ, ◌ู, ◌ฺ)
bool th_isToneAndDiacritic(uint32_t c);             // Above tone/diacritic ( ่, ้, ๊, ๋, ์, ◌๎)

class UTF8Iterator{
public:
    UTF8Iterator(const std::string & str);
    utf8::iterator<std::string::const_iterator> begin() const;
    utf8::iterator<std::string::const_iterator> end() const;
    utf8::iterator<std::string::const_reverse_iterator> rbegin() const;
    utf8::iterator<std::string::const_reverse_iterator> rend() const;
    
private:
    std::string src_valid;
};

#endif /* PesUnicodeUtils_hpp */
