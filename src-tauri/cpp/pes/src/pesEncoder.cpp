//
//  PES_PESEncoder.cpp
//  PES
//
//  Created by Panutat Tejasen on 10/12/2560 BE.
//  Copyright © 2560 Jimmy Software Co., Ltd. All rights reserved.
//
#include "pesEncoder.hpp"
#include <assert.h>
#include "pesColor.hpp"
#include "pesDocument.hpp"
#include "pesUtility.hpp"

extern const char* pes5threadCode[65];
extern const char pes5extraCode[];

const char pes5extraCode[] =
        "\x00\x00\x2E\x3A\xC7\x74\xEE\x44\x62\x4B\x29\x95\x22\x45\x62\x4B\x29\x95\x22\x29\xAE\x3A"
        "\x27\x95\xEE\x10\xA2\x4A\x29\x95\x22\x11\x22\x4A\x29\x95\x22\x11\x2E\x3A\xC9\x74\x2E\x11";

// pom
const char* pes5threadCode[65] = {
        "\x01\x1C\x8E\x0F\x00\x00\x00\x22\x91\x08\x00\x00\x00\x22\x11\x08\x00\x00\x00\x22\x11\x04"
        "\x00\x00\x00\x22\x11\x04\x00\x00\x00\x22\x11\x04\x00\x00\x00\x22\x11\x02\x00\x00\x00\x22"
        "\x11\x02\x00\x00\x00\x1C\x0E\x02\x00\x0E\x1F\x7C",  // Pussian Blue
        "\x02\x30\x8E\x0F\x00\x00\x00\x28\x91\x00\x00\x00\x00\x28\x91\x00\x00\x00\x00\x24\x91\x07"
        "\x00\x00\x00\x24\x91\x08\x00\x00\x00\x22\x11\x08\x00\x00\x00\x7E\x91\x08\x00\x00\x00\x20"
        "\x91\x08\x00\x00\x00\x20\x0E\x07\x00\x0A\x55\xA3",
        "\x03\x3E\x0E\x0C\x00\x00\x00\x02\x11\x0A\x00\x00\x00\x02\x10\x0A\x00\x00\x00\x1E\x10\x09"
        "\x00\x00\x00\x22\x0C\x09\x00\x00\x00\x20\x90\x08\x00\x00\x00\x22\x91\x1F\x00\x00\x00\x22"
        "\x11\x08\x00\x00\x00\x1C\x0E\x08\x00\x00\x87\x77",
        "\x04\x1C\x1F\x07\x00\x00\x00\x22\x91\x08\x00\x00\x00\x22\x90\x08\x00\x00\x00\x22\x88\x08"
        "\x00\x00\x00\x22\x88\x08\x00\x00\x00\x22\x88\x08\x00\x00\x00\x22\x84\x08\x00\x00\x00\x22"
        "\x84\x08\x00\x00\x00\x1C\x04\x07\x00\x4B\x6B\xAF",
        "\x05\x1C\x0E\x07\x00\x00\x00\x22\x91\x08\x00\x00\x00\x22\x91\x08\x00\x00\x00\x22\x91\x08"
        "\x00\x00\x00\x1C\x91\x08\x00\x00\x00\x22\x91\x08\x00\x00\x00\x22\x91\x08\x00\x00\x00\x22"
        "\x91\x08\x00\x00\x00\x1C\x0E\x07\x00\xED\x17\x1F",
        "\x06\x1C\x8E\x0F\x00\x00\x00\x22\x91\x08\x00\x00\x00\x20\x10\x08\x00\x00\x00\x20\x10\x04"
        "\x00\x00\x00\x18\x0C\x04\x00\x00\x00\x20\x10\x04\x00\x00\x00\x22\x11\x02\x00\x00\x00\x22"
        "\x11\x02\x00\x00\x00\x1C\x0E\x02\x00\xD1\x5C\x00",
        "\x07\x1C\x0E\x07\x00\x00\x00\x22\x91\x08\x00\x00\x00\x02\x91\x08\x00\x00\x00\x1E\x90\x08"
        "\x00\x00\x00\x22\x88\x08\x00\x00\x00\x22\x84\x08\x00\x00\x00\x22\x82\x08\x00\x00\x00\x22"
        "\x81\x08\x00\x00\x00\x1C\x1F\x07\x00\x91\x36\x97",
        "\x08\x1C\x08\x07\x00\x00\x00\x22\x8E\x08\x00\x00\x00\x22\x88\x08\x00\x00\x00\x22\x88\x08"
        "\x00\x00\x00\x1C\x88\x08\x00\x00\x00\x22\x88\x08\x00\x00\x00\x22\x88\x08\x00\x00\x00\x22"
        "\x88\x08\x00\x00\x00\x1C\x08\x07\x00\xE4\x9A\xCB",
        "\x09\x1C\x08\x07\x00\x00\x00\x22\x8E\x08\x00\x00\x00\x02\x88\x08\x00\x00\x00\x1E\x08\x08"
        "\x00\x00\x00\x22\x08\x04\x00\x00\x00\x22\x08\x02\x00\x00\x00\x22\x08\x01\x00\x00\x00\x22"
        "\x88\x00\x00\x00\x00\x1C\x88\x0F\x00\x91\x5F\xAC",
        "\x0A\x3E\x0E\x07\x00\x00\x00\x02\x91\x08\x00\x00\x00\x02\x91\x08\x00\x00\x00\x1E\x11\x08"
        "\x00\x00\x00\x22\x11\x04\x00\x00\x00\x20\x11\x02\x00\x00\x00\x22\x11\x01\x00\x00\x00\x22"
        "\x91\x00\x00\x00\x00\x1C\x8E\x0F\x00\x9E\xD6\x7D",
        "\x0B\x1C\x08\x0C\x00\x00\x00\x22\x0E\x0A\x00\x00\x00\x22\x08\x0A\x00\x00\x00\x20\x08\x09"
        "\x00\x00\x00\x10\x08\x09\x00\x00\x00\x08\x88\x08\x00\x00\x00\x04\x88\x1F\x00\x00\x00\x02"
        "\x08\x08\x00\x00\x00\x3E\x08\x08\x00\xE8\xA9\x00",
        "\x0C\x1C\x0E\x07\x00\x00\x00\x22\x91\x08\x00\x00\x00\x22\x91\x08\x00\x00\x00\x20\x91\x08"
        "\x00\x00\x00\x10\x11\x07\x00\x00\x00\x08\x91\x08\x00\x00\x00\x04\x91\x08\x00\x00\x00\x02"
        "\x91\x08\x00\x00\x00\x3E\x0E\x07\x00\xFE\xBA\x35",
        "\x0D\x1C\x8E\x0F\x00\x00\x00\x22\x91\x00\x00\x00\x00\x22\x91\x00\x00\x00\x00\x20\x91\x07"
        "\x00\x00\x00\x10\x91\x08\x00\x00\x00\x08\x11\x08\x00\x00\x00\x04\x91\x08\x00\x00\x00\x02"
        "\x91\x08\x00\x00\x00\x3E\x0E\x07\x00\xFF\xFF\x00",
        "\x0E\x3E\x08\x07\x00\x00\x00\x02\x8E\x08\x00\x00\x00\x02\x08\x08\x00\x00\x00\x1E\x08\x08"
        "\x00\x00\x00\x22\x08\x06\x00\x00\x00\x20\x08\x08\x00\x00\x00\x22\x88\x08\x00\x00\x00\x22"
        "\x88\x08\x00\x00\x00\x1C\x08\x07\x00\x70\xBC\x1F",
        "\x0F\x1C\x0E\x07\x00\x00\x00\x22\x91\x08\x00\x00\x00\x20\x91\x08\x00\x00\x00\x20\x90\x08"
        "\x00\x00\x00\x18\x08\x07\x00\x00\x00\x20\x84\x08\x00\x00\x00\x22\x82\x08\x00\x00\x00\x22"
        "\x81\x08\x00\x00\x00\x1C\x1F\x07\x00\xBA\x98\x00",
        "\x10\x1C\x8E\x0F\x00\x00\x00\x22\x91\x00\x00\x00\x00\x22\x91\x00\x00\x00\x00\x22\x91\x07"
        "\x00\x00\x00\x22\x91\x08\x00\x00\x00\x22\x11\x08\x00\x00\x00\x22\x91\x08\x00\x00\x00\x22"
        "\x91\x08\x00\x00\x00\x1C\x0E\x07\x00\xA8\xA8\xA8",
        "\x11\x1C\x0E\x07\x00\x00\x00\x22\x91\x08\x00\x00\x00\x20\x90\x08\x00\x00\x00\x20\x90\x08"
        "\x00\x00\x00\x18\x8C\x08\x00\x00\x00\x20\x90\x08\x00\x00\x00\x22\x91\x08\x00\x00\x00\x22"
        "\x91\x08\x00\x00\x00\x1C\x0E\x07\x00\x7D\x6F\x00",
        "\x12\x1C\x08\x07\x00\x00\x00\x22\x8E\x08\x00\x00\x00\x22\x88\x08\x00\x00\x00\x22\x88\x08"
        "\x00\x00\x00\x22\x88\x08\x00\x00\x00\x22\x88\x08\x00\x00\x00\x22\x88\x08\x00\x00\x00\x22"
        "\x88\x08\x00\x00\x00\x1C\x08\x07\x00\xFF\xFF\xB3",
        "\x13\x3E\x0E\x0C\x00\x00\x00\x22\x11\x0A\x00\x00\x00\x20\x11\x0A\x00\x00\x00\x10\x11\x09"
        "\x00\x00\x00\x10\x11\x09\x00\x00\x00\x10\x91\x08\x00\x00\x00\x08\x91\x1F\x00\x00\x00\x08"
        "\x11\x08\x00\x00\x00\x08\x0E\x08\x00\x4F\x55\x56",
        "\x14\x1C\x0E\x07\x00\x00\x00\x22\x91\x08\x00\x00\x00\x22\x91\x08\x00\x00\x00\x22\x91\x08"
        "\x00\x00\x00\x22\x91\x08\x00\x00\x00\x3C\x91\x08\x00\x00\x00\x20\x91\x08\x00\x00\x00\x22"
        "\x91\x08\x00\x00\x00\x1C\x0E\x07\x00\x00\x00\x00",
        "\x15\x30\x0E\x07\x00\x00\x00\x28\x91\x08\x00\x00\x00\x28\x91\x00\x00\x00\x00\x24\x91\x07"
        "\x00\x00\x00\x24\x91\x08\x00\x00\x00\x22\x91\x08\x00\x00\x00\x7E\x91\x08\x00\x00\x00\x20"
        "\x91\x08\x00\x00\x00\x20\x0E\x07\x00\x0B\x3D\x91",
        "\x16\x1C\x0E\x07\x00\x00\x00\x22\x91\x08\x00\x00\x00\x22\x81\x08\x00\x00\x00\x22\x8F\x08"
        "\x00\x00\x00\x1C\x91\x08\x00\x00\x00\x22\x11\x0F\x00\x00\x00\x22\x11\x08\x00\x00\x00\x22"
        "\x91\x08\x00\x00\x00\x1C\x0E\x07\x00\x77\x01\x76",
        "\x17\x3E\x8E\x0F\x00\x00\x00\x22\x91\x08\x00\x00\x00\x20\x11\x08\x00\x00\x00\x10\x11\x04"
        "\x00\x00\x00\x10\x11\x04\x00\x00\x00\x10\x11\x04\x00\x00\x00\x08\x11\x02\x00\x00\x00\x08"
        "\x11\x02\x00\x00\x00\x08\x0E\x02\x00\x29\x31\x33",
        "\x18\x1C\x1F\x07\x00\x00\x00\x22\x81\x08\x00\x00\x00\x22\x81\x08\x00\x00\x00\x22\x8F\x08"
        "\x00\x00\x00\x22\x11\x07\x00\x00\x00\x22\x90\x08\x00\x00\x00\x22\x91\x08\x00\x00\x00\x22"
        "\x91\x08\x00\x00\x00\x1C\x0E\x07\x00\x2A\x13\x01",
        "\x19\x1C\x0E\x07\x00\x00\x00\x22\x91\x08\x00\x00\x00\x22\x91\x00\x00\x00\x00\x22\x91\x07"
        "\x00\x00\x00\x22\x8E\x08\x00\x00\x00\x22\x91\x08\x00\x00\x00\x22\x91\x08\x00\x00\x00\x22"
        "\x91\x08\x00\x00\x00\x1C\x0E\x07\x00\xF6\x4A\x8A",
        "\x1A\x1C\x0E\x07\x00\x00\x00\x22\x91\x08\x00\x00\x00\x20\x11\x08\x00\x00\x00\x20\x10\x08"
        "\x00\x00\x00\x18\x08\x06\x00\x00\x00\x20\x04\x08\x00\x00\x00\x22\x82\x08\x00\x00\x00\x22"
        "\x81\x08\x00\x00\x00\x1C\x1F\x07\x00\xB2\x76\x24",
        "\x1B\x1C\x1F\x07\x00\x00\x00\x22\x91\x08\x00\x00\x00\x22\x90\x08\x00\x00\x00\x22\x88\x08"
        "\x00\x00\x00\x22\x88\x08\x00\x00\x00\x22\x08\x0F\x00\x00\x00\x22\x04\x08\x00\x00\x00\x22"
        "\x84\x08\x00\x00\x00\x1C\x04\x07\x00\xFC\xBB\xC4",
        "\x1C\x1C\x0E\x07\x00\x00\x00\x22\x91\x08\x00\x00\x00\x22\x90\x08\x00\x00\x00\x22\x90\x08"
        "\x00\x00\x00\x22\x8C\x08\x00\x00\x00\x22\x90\x08\x00\x00\x00\x22\x91\x08\x00\x00\x00\x22"
        "\x91\x08\x00\x00\x00\x1C\x0E\x07\x00\xFE\x37\x0F",
        "\x1D\x1C\x0E\x04\x00\x00\x00\x22\x11\x07\x00\x00\x00\x22\x11\x04\x00\x00\x00\x22\x11\x04"
        "\x00\x00\x00\x22\x11\x04\x00\x00\x00\x22\x11\x04\x00\x00\x00\x22\x11\x04\x00\x00\x00\x22"
        "\x11\x04\x00\x00\x00\x1C\x0E\x04\x00\xF0\xF0\xF0",
        "\x1E\x1C\x08\x07\x00\x00\x00\x22\x8E\x08\x00\x00\x00\x02\x08\x08\x00\x00\x00\x1E\x08\x08"
        "\x00\x00\x00\x22\x08\x06\x00\x00\x00\x22\x08\x08\x00\x00\x00\x22\x88\x08\x00\x00\x00\x22"
        "\x88\x08\x00\x00\x00\x1C\x08\x07\x00\x6A\x1C\x8A",
        "\x1F\x3E\x18\x07\x00\x00\x00\x02\x94\x08\x00\x00\x00\x02\x94\x08\x00\x00\x00\x1E\x12\x08"
        "\x00\x00\x00\x22\x12\x04\x00\x00\x00\x20\x11\x02\x00\x00\x00\x22\x3F\x01\x00\x00\x00\x22"
        "\x90\x00\x00\x00\x00\x1C\x90\x0F\x00\xA8\xDD\xC4",
        "\x20\x1C\x08\x07\x00\x00\x00\x22\x8E\x08\x00\x00\x00\x22\x88\x08\x00\x00\x00\x22\x88\x08"
        "\x00\x00\x00\x22\x88\x08\x00\x00\x00\x22\x08\x0F\x00\x00\x00\x22\x08\x08\x00\x00\x00\x22"
        "\x88\x08\x00\x00\x00\x1C\x08\x07\x00\x25\x84\xBB",
        "\x21\x10\x0E\x07\x00\x00\x00\x1C\x91\x08\x00\x00\x00\x10\x91\x00\x00\x00\x00\x10\x90\x07"
        "\x00\x00\x00\x10\x88\x08\x00\x00\x00\x10\x84\x08\x00\x00\x00\x10\x82\x08\x00\x00\x00\x10"
        "\x81\x08\x00\x00\x00\x10\x1F\x07\x00\xFE\xB3\x43",
        "\x22\x1C\x08\x07\x00\x00\x00\x22\x8E\x08\x00\x00\x00\x22\x88\x08\x00\x00\x00\x22\x08\x08"
        "\x00\x00\x00\x1C\x08\x04\x00\x00\x00\x22\x08\x02\x00\x00\x00\x22\x08\x01\x00\x00\x00\x22"
        "\x88\x00\x00\x00\x00\x1C\x88\x0F\x00\xFF\xF0\x8D",
        "\x23\x1C\x18\x07\x00\x00\x00\x22\x94\x08\x00\x00\x00\x20\x94\x08\x00\x00\x00\x20\x92\x08"
        "\x00\x00\x00\x18\x12\x07\x00\x00\x00\x20\x91\x08\x00\x00\x00\x22\xBF\x08\x00\x00\x00\x22"
        "\x90\x08\x00\x00\x00\x1C\x10\x07\x00\xD0\xA6\x60",
        "\x24\x1C\x0E\x07\x00\x00\x00\x22\x91\x08\x00\x00\x00\x20\x90\x08\x00\x00\x00\x20\x90\x08"
        "\x00\x00\x00\x18\x8C\x08\x00\x00\x00\x20\x10\x0F\x00\x00\x00\x22\x11\x08\x00\x00\x00\x22"
        "\x91\x08\x00\x00\x00\x1C\x0E\x07\x00\xD1\x54\x00",
        "\x25\x3E\x0E\x07\x00\x00\x00\x02\x91\x08\x00\x00\x00\x02\x91\x08\x00\x00\x00\x1E\x91\x08"
        "\x00\x00\x00\x22\x91\x08\x00\x00\x00\x20\x11\x0F\x00\x00\x00\x22\x11\x08\x00\x00\x00\x22"
        "\x91\x08\x00\x00\x00\x1C\x0E\x07\x00\x66\xBA\x49",
        "\x26\x30\x88\x0F\x00\x00\x00\x28\x8E\x00\x00\x00\x00\x28\x88\x00\x00\x00\x00\x24\x88\x07"
        "\x00\x00\x00\x24\x88\x08\x00\x00\x00\x22\x08\x08\x00\x00\x00\x7E\x88\x08\x00\x00\x00\x20"
        "\x88\x08\x00\x00\x00\x20\x08\x07\x00\x13\x4A\x46",
        "\x27\x1C\x88\x0F\x00\x00\x00\x22\x8E\x08\x00\x00\x00\x22\x08\x08\x00\x00\x00\x22\x08\x04"
        "\x00\x00\x00\x1C\x08\x04\x00\x00\x00\x22\x08\x04\x00\x00\x00\x22\x08\x02\x00\x00\x00\x22"
        "\x08\x02\x00\x00\x00\x1C\x08\x02\x00\x87\x87\x87",
        "\x28\x1C\x0E\x07\x00\x00\x00\x22\x91\x08\x00\x00\x00\x20\x91\x08\x00\x00\x00\x20\x91\x08"
        "\x00\x00\x00\x18\x91\x08\x00\x00\x00\x20\x1E\x0F\x00\x00\x00\x22\x10\x08\x00\x00\x00\x22"
        "\x91\x08\x00\x00\x00\x1C\x0E\x07\x00\xD8\xCC\xC6",
        "\x29\x3E\x88\x0F\x00\x00\x00\x02\x8E\x08\x00\x00\x00\x02\x08\x08\x00\x00\x00\x1E\x08\x04"
        "\x00\x00\x00\x22\x08\x04\x00\x00\x00\x20\x08\x04\x00\x00\x00\x22\x08\x02\x00\x00\x00\x22"
        "\x08\x02\x00\x00\x00\x1C\x08\x02\x00\x43\x56\x07",
        "\x2A\x1C\x8E\x0F\x00\x00\x00\x22\x91\x08\x00\x00\x00\x20\x11\x08\x00\x00\x00\x20\x11\x04"
        "\x00\x00\x00\x18\x11\x04\x00\x00\x00\x20\x11\x04\x00\x00\x00\x22\x11\x02\x00\x00\x00\x22"
        "\x11\x02\x00\x00\x00\x1C\x0E\x02\x00\xFE\xE3\xC5",
        "\x2B\x1C\x8E\x0F\x00\x00\x00\x22\x91\x00\x00\x00\x00\x22\x91\x00\x00\x00\x00\x22\x91\x07"
        "\x00\x00\x00\x22\x8E\x08\x00\x00\x00\x22\x11\x08\x00\x00\x00\x22\x91\x08\x00\x00\x00\x22"
        "\x91\x08\x00\x00\x00\x1C\x0E\x07\x00\xF9\x93\xBC",
        "\x2C\x1C\x0E\x07\x00\x00\x00\x22\x91\x08\x00\x00\x00\x22\x91\x08\x00\x00\x00\x22\x91\x08"
        "\x00\x00\x00\x1C\x11\x07\x00\x00\x00\x22\x91\x08\x00\x00\x00\x22\x91\x08\x00\x00\x00\x22"
        "\x91\x08\x00\x00\x00\x1C\x0E\x07\x00\x00\x38\x22",
        "\x2D\x1C\x0E\x0C\x00\x00\x00\x22\x11\x0A\x00\x00\x00\x22\x11\x0A\x00\x00\x00\x22\x11\x09"
        "\x00\x00\x00\x1C\x11\x09\x00\x00\x00\x22\x91\x08\x00\x00\x00\x22\x91\x1F\x00\x00\x00\x22"
        "\x11\x08\x00\x00\x00\x1C\x0E\x08\x00\xB2\xAF\xD4",
        "\x2E\x1C\x8E\x0F\x00\x00\x00\x22\x91\x08\x00\x00\x00\x02\x11\x08\x00\x00\x00\x1E\x11\x04"
        "\x00\x00\x00\x22\x11\x04\x00\x00\x00\x22\x11\x04\x00\x00\x00\x22\x11\x02\x00\x00\x00\x22"
        "\x11\x02\x00\x00\x00\x1C\x0E\x02\x00\x68\x6A\xB0",
        "\x2F\x1C\x18\x07\x00\x00\x00\x22\x94\x08\x00\x00\x00\x22\x14\x08\x00\x00\x00\x22\x12\x08"
        "\x00\x00\x00\x1C\x12\x06\x00\x00\x00\x22\x11\x08\x00\x00\x00\x22\xBF\x08\x00\x00\x00\x22"
        "\x90\x08\x00\x00\x00\x1C\x10\x07\x00\xEF\xE3\xB9",
        "\x30\x1C\x8E\x0F\x00\x00\x00\x22\x91\x08\x00\x00\x00\x22\x11\x08\x00\x00\x00\x22\x11\x04"
        "\x00\x00\x00\x1C\x11\x04\x00\x00\x00\x22\x11\x04\x00\x00\x00\x22\x11\x02\x00\x00\x00\x22"
        "\x11\x02\x00\x00\x00\x1C\x0E\x02\x00\xF7\x38\x66",
        "\x31\x1C\x0E\x07\x00\x00\x00\x22\x91\x08\x00\x00\x00\x20\x10\x08\x00\x00\x00\x20\x10\x08"
        "\x00\x00\x00\x18\x0C\x06\x00\x00\x00\x20\x10\x08\x00\x00\x00\x22\x91\x08\x00\x00\x00\x22"
        "\x91\x08\x00\x00\x00\x1C\x0E\x07\x00\xB5\x4C\x64",
        "\x32\x3E\x08\x07\x00\x00\x00\x02\x8E\x08\x00\x00\x00\x02\x88\x08\x00\x00\x00\x1E\x88\x08"
        "\x00\x00\x00\x22\x88\x08\x00\x00\x00\x20\x08\x0F\x00\x00\x00\x22\x08\x08\x00\x00\x00\x22"
        "\x88\x08\x00\x00\x00\x1C\x08\x07\x00\x13\x2B\x1A",
        "\x33\x10\x8E\x0F\x00\x00\x00\x1C\x91\x08\x00\x00\x00\x10\x11\x08\x00\x00\x00\x10\x11\x04"
        "\x00\x00\x00\x10\x11\x04\x00\x00\x00\x10\x11\x04\x00\x00\x00\x10\x11\x02\x00\x00\x00\x10"
        "\x11\x02\x00\x00\x00\x10\x0E\x02\x00\xC7\x01\x56",
        "\x34\x1C\x0E\x07\x00\x00\x00\x22\x91\x08\x00\x00\x00\x22\x91\x08\x00\x00\x00\x20\x91\x08"
        "\x00\x00\x00\x10\x91\x08\x00\x00\x00\x08\x11\x0F\x00\x00\x00\x04\x11\x08\x00\x00\x00\x02"
        "\x91\x08\x00\x00\x00\x3E\x0E\x07\x00\xFE\x9E\x32",
        "\x35\x1C\x88\x0F\x00\x00\x00\x22\x8E\x08\x00\x00\x00\x22\x08\x08\x00\x00\x00\x22\x08\x04"
        "\x00\x00\x00\x22\x08\x04\x00\x00\x00\x22\x08\x04\x00\x00\x00\x22\x08\x02\x00\x00\x00\x22"
        "\x08\x02\x00\x00\x00\x1C\x08\x02\x00\xA8\xDE\xEB",
        "\x36\x3E\x8E\x0F\x00\x00\x00\x02\x91\x08\x00\x00\x00\x02\x11\x08\x00\x00\x00\x1E\x11\x04"
        "\x00\x00\x00\x22\x11\x04\x00\x00\x00\x20\x11\x04\x00\x00\x00\x22\x11\x02\x00\x00\x00\x22"
        "\x11\x02\x00\x00\x00\x1C\x0E\x02\x00\x00\x67\x3E",
        "\x37\x1C\x08\x0C\x00\x00\x00\x22\x0E\x0A\x00\x00\x00\x02\x08\x0A\x00\x00\x00\x1E\x08\x09"
        "\x00\x00\x00\x22\x08\x09\x00\x00\x00\x22\x88\x08\x00\x00\x00\x22\x88\x1F\x00\x00\x00\x22"
        "\x08\x08\x00\x00\x00\x1C\x08\x08\x00\x4E\x29\x90",
        "\x38\x3E\x88\x0F\x00\x00\x00\x02\x8E\x00\x00\x00\x00\x02\x88\x00\x00\x00\x00\x1E\x88\x07"
        "\x00\x00\x00\x22\x88\x08\x00\x00\x00\x20\x08\x08\x00\x00\x00\x22\x88\x08\x00\x00\x00\x22"
        "\x88\x08\x00\x00\x00\x1C\x08\x07\x00\x2F\x7E\x20",
        "\x39\x10\x0E\x0C\x00\x00\x00\x1C\x11\x0A\x00\x00\x00\x10\x11\x0A\x00\x00\x00\x10\x10\x09"
        "\x00\x00\x00\x10\x08\x09\x00\x00\x00\x10\x84\x08\x00\x00\x00\x10\x82\x1F\x00\x00\x00\x10"
        "\x01\x08\x00\x00\x00\x10\x1F\x08\x00\xFD\xD9\xDE",
        "\x3A\x1C\x0E\x07\x00\x00\x00\x22\x91\x08\x00\x00\x00\x22\x91\x00\x00\x00\x00\x20\x91\x07"
        "\x00\x00\x00\x10\x91\x08\x00\x00\x00\x08\x91\x08\x00\x00\x00\x04\x91\x08\x00\x00\x00\x02"
        "\x91\x08\x00\x00\x00\x3E\x0E\x07\x00\xFF\xD9\x11",
        "\x3B\x30\x0E\x07\x00\x00\x00\x28\x91\x08\x00\x00\x00\x28\x91\x08\x00\x00\x00\x24\x90\x08"
        "\x00\x00\x00\x24\x88\x08\x00\x00\x00\x22\x84\x08\x00\x00\x00\x7E\x82\x08\x00\x00\x00\x20"
        "\x81\x08\x00\x00\x00\x20\x1F\x07\x00\x09\x5B\xA6",
        "\x3C\x1C\x0E\x07\x00\x00\x00\x22\x91\x08\x00\x00\x00\x22\x91\x08\x00\x00\x00\x20\x11\x08"
        "\x00\x00\x00\x10\x11\x04\x00\x00\x00\x08\x11\x02\x00\x00\x00\x04\x11\x01\x00\x00\x00\x02"
        "\x91\x00\x00\x00\x00\x3E\x8E\x0F\x00\xF0\xF9\x70",
        "\x3D\x1C\x8E\x0F\x00\x00\x00\x22\x91\x08\x00\x00\x00\x22\x11\x08\x00\x00\x00\x22\x10\x04"
        "\x00\x00\x00\x22\x08\x04\x00\x00\x00\x22\x04\x04\x00\x00\x00\x22\x02\x02\x00\x00\x00\x22"
        "\x01\x02\x00\x00\x00\x1C\x1F\x02\x00\xE3\xF3\x5B",
        "\x3E\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00"
        "\x00\x00\x00\x8E\xE3\x38\x8E\xE3\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00"
        "\x00\x00\x00\x00\x00\x00\x00\x00\x00\xFF\xC8\x64",
        "\x3F\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00"
        "\x00\x00\x00\x8E\xE3\x38\x8E\xE3\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00"
        "\x00\x00\x00\x00\x00\x00\x00\x00\x00\xFF\xC8\x96",
        "\x40\x00\x00\x00\x00\x00\x00\x20\x20\x20\x20\x20\x20\x50\x50\x50\x50\x50\x10\x88\x88\x88"
        "\x88\x88\x08\x04\x05\x05\x05\x05\x05\x02\x02\x02\x02\x02\x02\x00\x00\x00\x00\x00\x00\x00"
        "\x00\x00\x00\x00\x00\x00\x00\x00\x00\xFF\xC8\xC8",  // 64 (Applique)
        "\x40\x00\x00\x00\x00\x00\x00\x20\x20\x20\x20\x20\x20\x50\x50\x50\x50\x50\x10\x88\x88\x88"
        "\x88\x88\x08\x04\x05\x05\x05\x05\x05\x02\x02\x02\x02\x02\x02\x00\x00\x00\x00\x00\x00\x00"
        "\x00\x00\x00\x00\x00\x00\x00\x00\x00\xFF\xC8\xC8",  // 64 (Applique)
};

const float SHORT_JUMP_THRESHOLD_MM = 2;  // Note 0 = Non-Optimized

void PES_encodeMatrix(pesBuffer& buffer, float f1, float f2, float f3, float f4, float f5,
                      float f6) {
    float matrix[6] = {f1, f2, f3, f4, f5, f6};

    for (int i = 0; i < 6; i++) {
        buffer.append((const char*)&matrix[i], 4);
    }
}

void PES_encodeIdentityMatrix(pesBuffer& buffer) {
    float matrix[6] = {1.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f};

    for (int i = 0; i < 6; i++) {
        buffer.append((const char*)&matrix[i], 4);
    }
}

void PES_encodeNumString(pesBuffer& buffer, int n) {
    char str[4];
    sprintf(str, "%i", n);

    buffer.appendPESString(str);
}

void PES_encodeColor(pesBuffer& buffer, pesData* pPesData, int index) {
    int pecIndex = pPesData->colors[index];

    const char* strNumber = pesGetBrotherCatalogNumber(pecIndex);
    pesColor color = pesGetBrotherColor(pecIndex);

    buffer.appendPESString(strNumber);  // PE Design confirmed

    buffer.appendU8(color.r);
    buffer.appendU8(color.g);
    buffer.appendU8(color.b);

    buffer.appendU8(0);     // Unknown value
    buffer.appendU8(0x0A);  // Unknown value
    buffer.appendU8(0);     // Unknown value
    buffer.appendU8(0);     // Unknown value
    buffer.appendU8(0);     // Unknown value

    PES_encodeNumString(buffer, pecIndex);  // PE Design confirmed

    buffer.appendPESString("EMBROIDERY");  // PE Design confirmed

    buffer.appendU8(0);  // Unknown value
}

void PES_encodeHoopSize(pesBuffer& buffer, pesData* pPesData, pesVec2f& hoopSize) {
    //pesVec2f hoopSize = pesGetDocument()->getHoopSizeInMM();
    buffer.appendU16((uint16_t)hoopSize.x);
    buffer.appendU16((uint16_t)hoopSize.y);
}

void PES_encodeColorList(pesBuffer& buffer, pesData* pPesData, int version) {
    int numColor = (int)(pPesData->colors.size());
    buffer.appendU16(numColor);

    for (int i = 0; i < numColor; i++) {
        PES_encodeColor(buffer, pPesData, i);
    }
}

void PES_encodeBoundary(pesBuffer& buffer, pesData* pPesData, int translateX, int translateY) {
    int minx = pPesData->getBoundingBox().getMinX() + translateX;
    int miny = pPesData->getBoundingBox().getMinY() + translateY;
    int maxx = pPesData->getBoundingBox().getMaxX() + translateX;
    int maxy = pPesData->getBoundingBox().getMaxY() + translateY;

    buffer.appendU16(minx);
    buffer.appendU16(miny);
    buffer.appendU16(maxx);
    buffer.appendU16(maxy);
}

void PES_encodeBlockGeometry(pesBuffer& buffer, pesData* pPesData, int translatex, int translatey) {
    PES_encodeBoundary(buffer, pPesData, translatex, translatey);
    PES_encodeBoundary(buffer, pPesData, translatex, translatey);

    PES_encodeMatrix(buffer, 1.0f, 0.0f, 0.0f, 1.0f, (float)-translatex, (float)-translatey);
}

// more info : https://github.com/frno7/libpes
void PES_encodePES5Header(pesBuffer& buffer, pesData* pPesData, pesVec2f& hoopSize) {
    buffer.append("#PES");  // PES File ID
    buffer.append("0050");  // PES version

    // PEC section offset stub
    buffer.appendU32(0);  // PEC offset

    buffer.appendU16(1);    // Unknown value "Hoop Size Indicator"
    buffer.appendU8(0x30);  // Unknown value
    buffer.appendU8(0x31);  // Unknown value 0x31 | 0x32

    // Description
    buffer.appendPESString("");  // Design String
    buffer.appendPESString("");  // Catagory String
    buffer.appendPESString("");  // Author String
    buffer.appendPESString("");  // Keywords String
    buffer.appendPESString("");  // Comments String

    buffer.appendU16(0);  // Unknown value "Optimize Hoop Change"

    // Hoop size
    PES_encodeHoopSize(buffer, pPesData, hoopSize);  // Hoop size

    buffer.appendU16(0);    // Unknown value Hoop Rotation, 90° 1 = rotate.
    buffer.appendU16(7);    // Unknown value Design Page Background Color 7 (typical)
    buffer.appendU16(19);   // Unknown value Design Page Foreground Color 19 (typical)
    buffer.appendU16(1);    // Unknown value 0 | 1 "Show Grid"
    buffer.appendU16(1);    // Unknown value "With Axes" 1 (typical)
    buffer.appendU16(0);    // Unknown value 0 | 1 "Snap To Grid"
    buffer.appendU16(100);  // Unknown value "Grid Interval" 100(typical)
    buffer.appendU8(1);     // Unknown value
    buffer.appendU8(0);     // Unknown value
    buffer.appendU8(0);     // Unknown value "OptimizeEntryExitPoints" 0(typical)
    buffer.appendU8(0);     // Unknown value
    buffer.appendU8(0);     // Unknown value "fromImageStringLength" 0(typical)

    // Affine transform
    PES_encodeIdentityMatrix(buffer);  // Affine Transform

    buffer.appendU8(0, 6);  // Unknown value [Number of programmable fill patterns,
                            // Number of programmable Motif,
                            // Number of feather patterns] 0(typical)(3x2=6)

    // Color List
    PES_encodeColorList(buffer, pPesData, 5);

    buffer.appendU16(1);  // Unknown value The number of Objects, this is the number of objects
                          // within the PES document 1(typical)
}

void PES_encodePES1DefaultHeader(pesBuffer& buffer) {
    buffer.append("#PES");  // PES File ID
    buffer.append("0001");  // PES version

    buffer.appendU32(0);  // PEC offset

    buffer.appendU16(0);  // Hoop size 10x10
    buffer.appendU16(1);  // Use existing design area
    buffer.appendU16(1);  // CSewSeg segment block count
}

void PES_encodePES1Header(pesBuffer& buffer, pesData* pPesData, pesVec2f& hoopSize) {
    buffer.append("#PES");  // PES File ID
    buffer.append("0001");  // PES version

    // PEC section offset stub
    buffer.appendU32(0);  // PEC offset

    //pesVec2f hoopSize = pesGetDocument()->getHoopSizeInMM();
    if (hoopSize.x <= 100 && hoopSize.y <= 100) {
        buffer.appendU16(0);
    } else {
        buffer.appendU16(1);
    }

    buffer.appendU16(1);  // Use existing design area

    buffer.appendU16(1);  // CSewSeg segment block count
}

void PES_encodeCSewSegHeader(pesBuffer& buffer, pesData* pPesData, pesVec2f& hoopSize) {
    int hw = (int)hoopSize.x * 10;
    int hh = (int)hoopSize.y * 10;

    int centerx = pPesData->centerx;
    int centery = pPesData->centery;

    int minx = pPesData->getBoundingBox().getMinX();
    int miny = pPesData->getBoundingBox().getMinY();
    int maxx = pPesData->getBoundingBox().getMaxX();
    int maxy = pPesData->getBoundingBox().getMaxY();

    int w = maxx - minx;
    int h = maxy - miny;

    int translatex = hw - 1000 - centerx;  // if frame bigger than 10 x 10 cm need to compensate
    int translatey = hh - 1000 - centery;  //

    PES_encodeBlockGeometry(buffer, pPesData, translatex, translatey);  // Block Geometry

    buffer.appendU16(1);  // Unknown value

    int leftbottomx = minx - centerx;  // leftbottom in local coordinate of pattern
    int leftbottomy = maxy - centery;

    buffer.appendS16(leftbottomx + hw);  // leftboottom in hoop coordinate (hoopsize * 2 --> center is at hw, hh)
    buffer.appendS16(leftbottomy + hh);  // this will used in calculate boundary in PE design

    buffer.appendU16(w);  // CSewSeg width
    buffer.appendU16(h);  // CSewSeg height

    buffer.appendU8(0, 8);  // Unknown value
}

void PES_encodeDefaultCSewSegHeader(pesBuffer& buffer) {
    buffer.appendU16(0);  // minx
    buffer.appendU16(0);  // miny
    buffer.appendU16(0);  // maxx
    buffer.appendU16(0);  // maxy

    buffer.appendU16(0);  // minx
    buffer.appendU16(0);  // miny
    buffer.appendU16(0);  // maxx
    buffer.appendU16(0);  // maxy

    PES_encodeMatrix(buffer, 1.0, 0.0, 0.0, 1.0, 0.0, 0.0);

    buffer.appendU16(1);  // Unknown value

    buffer.appendS16(0);  // leftbottom in hoop coordinate (hoopsize * 2 --> center is at hw, hh)
    buffer.appendS16(0);  // this will used in calculate boundary in PE design

    buffer.appendU16(0);  // CSewSeg width
    buffer.appendU16(0);  // CSewSeg height

    buffer.appendU8(0, 8);  // Unknown value

    buffer.appendU16(0);  // CSewSeg block count ? -> move to encodeCSewSeg
}

void PES_encodeCEmbOne(pesBuffer& buffer, pesData* pPesData, pesVec2f& hoopSize) {
    buffer.appendU32(0x0000FFFF);        // Start new block
    buffer.appendPESString2("CEmbOne");  // CEmbOne section ID

    PES_encodeCSewSegHeader(buffer, pPesData, hoopSize);
}

void PES_encodeDefaultCEmbOne(pesBuffer& buffer) {
    buffer.appendU32(0x0000FFFF);        // Start new block
    buffer.appendPESString2("CEmbOne");  // CEmbOne section ID

    PES_encodeDefaultCSewSegHeader(buffer);
}

int PES_getThreadIndex(pesData* pPesData, int pecColorIndex) {
    int numColor = (int)(pPesData->colors.size());

    for (int i = 0; i < numColor; i++) {
        int pecIndex = pPesData->colors[i];
        if (pecIndex == pecColorIndex) return i;
    }
    return 0;
}

int PES_encodeCSewSegSegmentBlock(pesBuffer& buffer, pesData* pPesData, pesVec2f& hoopSize, int version) {
    //pesVec2f hoopSize = pesGetDocument()->getHoopSizeInMM();
    int hw = (int)hoopSize.x * 10;
    int hh = (int)hoopSize.y * 10;

    pesRectangle bound = pPesData->getBoundingBox();

    int centerx = bound.getCenter().x;
    int centery = bound.getCenter().y;

    int translatex = hw - centerx;
    int translatey = hh - centery;

    int minx = bound.getMinX();
    int miny = bound.getMinY();
    int maxx = bound.getMaxX();
    int maxy = bound.getMaxY();

    int w = maxx - minx;
    int h = maxy - miny;

    pesBuffer* pBuf = NULL;
    int stitchType = -1;
    int threadIndex = -1;
    int numCoord = 0;
    bool isJump;

    int nBlockCount = 0;
    pesBuffer sewSegColor;

    pesStitchBlockList blocks;
    pPesData->getStitchBlockList(blocks);

    for (int i = 0; i < (int)blocks.size(); i++) {
        pesStitchBlock block = blocks[i];
        threadIndex = PES_getThreadIndex(pPesData, block.colorIndex);  // + 1; // Jimmy - check this

        int nStitch = (int)block.size();
        isJump = (block.types[0] == JUMP_STITCH || block.types[0] == TRIM_STITCH);
        if (i == 0) {
            isJump = 0;  // Jimmy
        }
        numCoord = 0;

        sewSegColor.appendU16(nBlockCount);
        if (version < 5) {
            sewSegColor.appendU16(block.colorIndex);
        } else {
            sewSegColor.appendU16(threadIndex);  // -1 // Jimmy - check this
        }

        int lastStitchX = 0;
        int lastStitchY = 0;

        for (int j = 0; j < nStitch; j++) {
            pesVec2f stitch = block.polyline[j];
            if (pBuf == NULL || block.isJump(j) != isJump || numCoord >= 1000) {
                if (numCoord != 0 && pBuf != NULL) {
                    stitchType = isJump ? 1 : 0;
                    buffer.appendU16(stitchType);
                    if (version < 5) {
                        buffer.appendU16(block.colorIndex);
                    } else {
                        buffer.appendU16(threadIndex);  // -1 // Jimmy - check this
                    }
                    buffer.appendU16(numCoord);
                    buffer.append(*pBuf);
                    if ((i < (int)blocks.size() - 1) || (j < nStitch - 1)) buffer.appendU16(0x8003);

                    pBuf->clear();
                    nBlockCount++;
                    numCoord = 0;
                    isJump = block.isJump(j);
                    pBuf->appendS16(lastStitchX + translatex);
                    pBuf->appendS16(lastStitchY + translatey);
                    numCoord = 1;
                } else {
                    pBuf = new pesBuffer();
                    pBuf->appendS16(0 + translatex);
                    pBuf->appendS16(0 + translatey);
                    numCoord = 1;
                }
            }
            pBuf->appendS16(stitch.x + translatex);
            pBuf->appendS16(stitch.y + translatey);
            lastStitchX = stitch.x;
            lastStitchY = stitch.y;

            numCoord++;
        }
        if (numCoord) {
            stitchType = isJump ? 1 : 0;
            buffer.appendU16(stitchType);
            if (version < 5) {
                buffer.appendU16(block.colorIndex);
            } else {
                buffer.appendU16(threadIndex);  // -1 // Jimmy - check this
            }
            buffer.appendU16(numCoord);
            buffer.append(*pBuf);
            pBuf->clear();
            nBlockCount++;
            if (i < (int)(blocks.size()) - 1) buffer.appendU16(0x8003);
        }
    }
    int numColor = (int)(pPesData->colors.size());
    buffer.appendU16(numColor);
    buffer.append(sewSegColor);

    return nBlockCount;
}

void PES_encodeCSewSeg(pesBuffer& buffer, pesData* pPesData, pesVec2f& hoopSize, int version) {
    pesBuffer bufferCSewSegBlock;
    int BlockCount = PES_encodeCSewSegSegmentBlock(bufferCSewSegBlock, pPesData, hoopSize, version);

    buffer.appendU16(BlockCount);  // CSewSeg block count ?
    buffer.appendU32(0x0000FFFF);  // Start new block

    buffer.appendPESString2("CSewSeg");  // CSewSeg section ID

    buffer.append(bufferCSewSegBlock);
}

static void pecEncodeJump(pesBuffer& buff, int x, int types) {
    int outputVal = abs(x) & 0x7FF;
    unsigned int orPart = 0x80;

    if (types & TRIM_STITCH) {
        orPart |= 0x20;  // orPart=0xA0
                         //        GUI_LogNotice("TRIM_STITCH");
    }
    if (types & JUMP_STITCH) {
        orPart |= 0x10;  // orPart=0x90
                         //        GUI_LogNotice("JUMP_STITCH");
    }
    // otherwise orPart=0x80 (fake jump stitch)

    if (x < 0) {
        outputVal = x + 0x1000 & 0x7FF;
        outputVal |= 0x800;
    }

    unsigned char jump[2] = {0};
    jump[0] = (unsigned char)(((outputVal >> 8) & 0x0F) | orPart);
    jump[1] = (unsigned char)(outputVal & 0xFF);

    buff.append((const char*)jump, 2);
}

static void pecEncodeStop(pesBuffer& buff, unsigned char val) {
    unsigned char stop[3] = {0xFE, 0xB0, val};
    buff.append((const char*)stop, 3);
}

pesBuffer pesCreatePECStitch(pesData* pPesData, int _dx, int _dy) {
    //    GUI_LogNotice("PES_createPECStitch");
    // build relative stich&encode
    double thisX = 0.0;
    double thisY = 0.0;
    unsigned char stopCode = 2;
    const int unit_per_mm = 10;
    float trimAfter = pesGetDocument()->getTrimAtLength() * unit_per_mm;

    pesBuffer stitches;
    pesStitchBlockList blocks;
    pPesData->getStitchBlockList(blocks);
    for (int i = 0; i < (int)blocks.size(); i++) {
        pesStitchBlock block = blocks[i];
        if (i > 0) {
            // write change color(start new block) 0xFE 0xE0 0xNN<-use stopCode
            pecEncodeStop(stitches, stopCode);
            if (stopCode == (unsigned char)2) {
                stopCode = (unsigned char)1;
            } else {
                stopCode = (unsigned char)2;
            }
        }
        for (int j = 0; j < (int)block.size(); j++) {
            pesVec2f stitch = block.polyline[j];
            stitch.x += _dx;
            stitch.y += _dy;

            if (i == 0 && j == 0) {
                if (block.types[j] == TRIM_STITCH) block.types[j] = JUMP_STITCH;
            } else if (i > 0 && j == 0) {
                block.types[j] = TRIM_STITCH;
            }

            int deltaX = (int)round(stitch.x - thisX);
            int deltaY = (int)round(stitch.y - thisY);
            thisX += (double)deltaX;
            thisY += (double)deltaY;

            // when 2 or more pes are merged into may we can found end flag before last stitches
            // so we just skip this stitch
            if (block.types[j] & STOP_STITCH || block.types[j] & END_STITCH) {
                continue;
            } else if (deltaX < 63 && deltaX > -64 && deltaY < 63 && deltaY > -64 &&
                       (!(block.isJump(j)))) {
                // normal stitch x y pair in to 2 bytes
                unsigned char delta[2];
                delta[0] = (deltaX < 0) ? (unsigned char)(deltaX + 0x80) : (unsigned char)deltaX;
                delta[1] = (deltaY < 0) ? (unsigned char)(deltaY + 0x80) : (unsigned char)deltaY;
                stitches.append((const char*)delta, 2);
            } else {
                // this can be jump, trim or fake jump (hi length stitch)
                // MARK: Incase deltaX/deltaY >2047 or <-2048 make double jump or multiple jump
                float len = sqrt(deltaX * deltaX + deltaY * deltaY);
                if (len >= trimAfter) {
                    if (block.types[j] == JUMP_STITCH) block.types[j] = TRIM_STITCH;
                }

                if (abs(deltaX) > 2047 || abs(deltaY) > 2047) {
                    double _max = (double)MAX((float)abs(deltaX), (float)abs(deltaY));
                    int nround = (int)ceil((_max + 1) / 2047);
                    for (int i = 0; i < nround; i++) {
                        float stepX = (deltaX / (float)nround);
                        float stepY = (deltaY / (float)nround);
                        pecEncodeJump(stitches, (int)round(stepX), block.types[j]);
                        pecEncodeJump(stitches, (int)round(stepY), block.types[j]);
                    }
                } else {
                    pecEncodeJump(stitches, deltaX, block.types[j]);
                    pecEncodeJump(stitches, deltaY, block.types[j]);
                }
            }
        }
    }
    // end of stitch data
    const unsigned char endofstitch[1] = {0xFF};
    stitches.append((const char*)endofstitch, 1);

    return stitches;
}

void PES_clearImage(unsigned char image[][48]) { memset(image, 0, 48 * 38); }

void PES_writeImage(pesBuffer& buff, unsigned char image[][48]) {
    // 38rows * 48columns
    for (int i = 0; i < 38; i++) {
        for (int j = 0; j < 6; j++) {  // 6*8=48 (8mean 8bits in each byte)
            int offset = j * 8;
            unsigned char output = 0;
            output |= (unsigned char)(image[i][offset] != 0);
            output |= (unsigned char)(image[i][offset + 1] != (unsigned char)0) << 1;
            output |= (unsigned char)(image[i][offset + 2] != (unsigned char)0) << 2;
            output |= (unsigned char)(image[i][offset + 3] != (unsigned char)0) << 3;
            output |= (unsigned char)(image[i][offset + 4] != (unsigned char)0) << 4;
            output |= (unsigned char)(image[i][offset + 5] != (unsigned char)0) << 5;
            output |= (unsigned char)(image[i][offset + 6] != (unsigned char)0) << 6;
            output |= (unsigned char)(image[i][offset + 7] != (unsigned char)0) << 7;
            buff.append((const char*)&output, 1);
        }
    }
}

void PES_createPecImageBorder(unsigned char image[38][48]) {
    unsigned char border[38][49] = {"000000000000000000000000000000000000000000000000",
                                    "000011111111111111111111111111111111111111110000",
                                    "000100000000000000000000000000000000000000001000",
                                    "001000000000000000000000000000000000000000000100",
                                    "010000000000000000000000000000000000000000000010",
                                    "010000000000000000000000000000000000000000000010",
                                    "010000000000000000000000000000000000000000000010",
                                    "010000000000000000000000000000000000000000000010",
                                    "010000000000000000000000000000000000000000000010",
                                    "010000000000000000000000000000000000000000000010",
                                    "010000000000000000000000000000000000000000000010",
                                    "010000000000000000000000000000000000000000000010",
                                    "010000000000000000000000000000000000000000000010",
                                    "010000000000000000000000000000000000000000000010",
                                    "010000000000000000000000000000000000000000000010",
                                    "010000000000000000000000000000000000000000000010",
                                    "010000000000000000000000000000000000000000000010",
                                    "010000000000000000000000000000000000000000000010",
                                    "010000000000000000000000000000000000000000000010",
                                    "010000000000000000000000000000000000000000000010",
                                    "010000000000000000000000000000000000000000000010",
                                    "010000000000000000000000000000000000000000000010",
                                    "010000000000000000000000000000000000000000000010",
                                    "010000000000000000000000000000000000000000000010",
                                    "010000000000000000000000000000000000000000000010",
                                    "010000000000000000000000000000000000000000000010",
                                    "010000000000000000000000000000000000000000000010",
                                    "010000000000000000000000000000000000000000000010",
                                    "010000000000000000000000000000000000000000000010",
                                    "010000000000000000000000000000000000000000000010",
                                    "010000000000000000000000000000000000000000000010",
                                    "010000000000000000000000000000000000000000000010",
                                    "010000000000000000000000000000000000000000000010",
                                    "010000000000000000000000000000000000000000000010",
                                    "001000000000000000000000000000000000000000000100",
                                    "000100000000000000000000000000000000000000001000",
                                    "000011111111111111111111111111111111111111110000",
                                    "000000000000000000000000000000000000000000000000"};
    for (int y = 0; y < 38; y++) {
        for (int x = 0; x < 48; x++) {
            image[y][x] = border[y][x] - '0';
        }
    }
}

void PES_encodePECGraphics(pesBuffer& buffer, pesData* pPesData) {
    int maxh = 32;
    int maxw = 42;

    float sx = pPesData->getBoundingBox().width > maxw
                       ? (float)maxw / pPesData->getBoundingBox().width
                       : 1.0f;
    float sy = pPesData->getBoundingBox().height > maxh
                       ? (float)maxh / pPesData->getBoundingBox().height
                       : 1.0f;
    float scale = MIN(sx, sy);

    int w = (int)(pPesData->getBoundingBox().width * scale - 1);
    int h = (int)(pPesData->getBoundingBox().height * scale - 1);
    int x = (int)((48 - w) / 2.0f);
    int y = (int)((38 - h) / 2.0f);
    int left = x;
    int right = x + w;
    int top = y;
    int bottom = y + h;

    unsigned char image[38][48];

    // PES_clearImage(image);
    PES_createPecImageBorder(image);
    // writing all colors
    pesStitchBlockList blocks;
    pPesData->getStitchBlockList(blocks);
    for (int i = 0; i < (int)blocks.size(); i++) {
        pesStitchBlock block = blocks[i];
        for (int j = 0; j < (int)block.size(); j++) {
            pesVec2f stitch = block.polyline[j];
            int x = (int)roundf(pesMap((float)stitch.x, (float)pPesData->minx,
                                       (float)pPesData->maxx, (float)left, (float)right));
            int y = (int)roundf(pesMap((float)stitch.y, (float)pPesData->miny,
                                       (float)pPesData->maxy, (float)top, (float)bottom));
            image[y][x] = 1;
            if (j > 0) {
                pesVec2f start((float)block.polyline[j - 1].x, (float)block.polyline[j - 1].y);
                pesVec2f end((float)block.polyline[j].x, (float)block.polyline[j].y);
                //                auto vv = ZigZagger().getPointsOnLine(start, end, 4.0);
                //                if (vv.size() > 0) {
                //                    for (auto med : vv) {
                //                        if (med.size() > 0) {
                //                            for (auto m : med) {
                //                                int x = (int)roundf(pesMap((float)m.x,
                //                                (float)pPesData->minx, (float)pPesData->maxx,
                //                                (float)left, (float)right)); int y =
                //                                (int)roundf(pesMap((float)m.y,
                //                                (float)pPesData->miny, (float)pPesData->maxy,
                //                                (float)top, (float)bottom)); image[y][x] = 1;
                //                            }
                //                        }
                //                    }
                //                }

                // faster than ZigZagger().getPointsOnLine()
                start.x = (int)roundf(pesMap((float)start.x, (float)pPesData->minx,
                                             (float)pPesData->maxx, (float)left, (float)right));
                start.y = (int)roundf(pesMap((float)start.y, (float)pPesData->miny,
                                             (float)pPesData->maxy, (float)top, (float)bottom));
                end.x = (int)roundf(pesMap((float)end.x, (float)pPesData->minx,
                                           (float)pPesData->maxx, (float)left, (float)right));
                end.y = (int)roundf(pesMap((float)end.y, (float)pPesData->miny,
                                           (float)pPesData->maxy, (float)top, (float)bottom));
                float dist = start.distance(end);
                if (dist >= 1) {
                    pesPolyline p;
                    p.addVertex(start);
                    p.addVertex(end);
                    int count = (int)ceilf(p.getPerimeter() / dist);
                    std::vector<pesVec2f> vertices = p.getResampledByCount2(count).getVertices();
                    for (auto v : vertices) {
                        int x = (int)roundf(v.x);
                        int y = (int)roundf(v.y);
                        image[y][x] = 1;
                    }
                }
            }
        }
    }
    PES_writeImage(buffer, image);

    // writing each color(block)
    for (int i = 0; i < (int)blocks.size(); i++) {
        // PES_clearImage(image);
        PES_createPecImageBorder(image);
        pesStitchBlock block = blocks[i];
        for (int j = 0; j < (int)block.size(); j++) {
            pesVec2f stitch = block.polyline[j];
            int x = (int)roundf(pesMap((float)stitch.x, (float)pPesData->minx,
                                       (float)pPesData->maxx, (float)left, (float)right));
            int y = (int)roundf(pesMap((float)stitch.y, (float)pPesData->miny,
                                       (float)pPesData->maxy, (float)top, (float)bottom));
            image[y][x] = 1;
            if (j > 0) {
                if (!block.isJump(j)) {
                    pesVec2f start((float)block.polyline[j - 1].x, (float)block.polyline[j - 1].y);
                    pesVec2f end((float)block.polyline[j].x, (float)block.polyline[j].y);
                    //                    auto vv = ZigZagger().getPointsOnLine(start, end, 4.0);
                    //                    if (vv.size() > 0) {
                    //                        for (auto med : vv) {
                    //                            if (med.size() > 0) {
                    //                                for (auto m : med) {
                    //                                    int x = (int)roundf(pesMap((float)m.x,
                    //                                    (float)pPesData->minx,
                    //                                    (float)pPesData->maxx, (float)left,
                    //                                    (float)right)); int y =
                    //                                    (int)roundf(pesMap((float)m.y,
                    //                                    (float)pPesData->miny,
                    //                                    (float)pPesData->maxy, (float)top,
                    //                                    (float)bottom)); image[y][x] = 1;
                    //                                }
                    //                            }
                    //                        }
                    //                    }

                    // faster than ZigZagger().getPointsOnLine()
                    start.x = (int)roundf(pesMap((float)start.x, (float)pPesData->minx,
                                                 (float)pPesData->maxx, (float)left, (float)right));
                    start.y = (int)roundf(pesMap((float)start.y, (float)pPesData->miny,
                                                 (float)pPesData->maxy, (float)top, (float)bottom));
                    end.x = (int)roundf(pesMap((float)end.x, (float)pPesData->minx,
                                               (float)pPesData->maxx, (float)left, (float)right));
                    end.y = (int)roundf(pesMap((float)end.y, (float)pPesData->miny,
                                               (float)pPesData->maxy, (float)top, (float)bottom));
                    float dist = start.distance(end);
                    if (dist >= 1) {
                        pesPolyline p;
                        p.addVertex(start);
                        p.addVertex(end);
                        int count = (int)ceilf(p.getPerimeter() / dist);
                        std::vector<pesVec2f> vertices = p.getResampledByCount2(count).getVertices();
                        for (auto v : vertices) {
                            int x = (int)roundf(v.x);
                            int y = (int)roundf(v.y);
                            image[y][x] = 1;
                        }
                    }
                }
            }
        }
        PES_writeImage(buffer, image);
    }
}

void PES_encodePEC(pesBuffer& pes, pesData* pPesData, bool centerPec) {
    pesBuffer buffer;
    buffer.appendString("LA:");     // PEC section ID

    buffer.appendU8(0x20, 16);      // padding with space (total 19 bytes)
    buffer.appendU8('\r');          // Carriage return

    buffer.appendU8(0x20, 11);      // Unknown
    buffer.appendU8(0xFF);          // Unknown (0x20 | 0xFF)
    buffer.appendU16(0x00FF);       // Unknown

    buffer.appendU8(6);             // Thumbnail with in bytes (48 pixels = 6 bytes)
    buffer.appendU8(38);            // Thumbnail height in lines

    buffer.appendU8(0x20, 12);      // Unknown

    int numColor = (int)(pPesData->colors.size());
    buffer.appendU8(numColor - 1);  // Number of color - 1

    for (int i = 0; i < numColor; i++) {
        int pecIndex = pPesData->colors[i];
        buffer.appendU8(pecIndex);
    }
    buffer.appendU8(0x20, 463 - numColor);  // Unknown

    int n = (int)buffer.size();
    assert(n == 512);

    int centerx = pPesData->centerx;
    int centery = pPesData->centery;

    int minx = pPesData->minx;
    int miny = pPesData->miny;
    int maxx = pPesData->maxx;
    int maxy = pPesData->maxy;

    int cx = centerPec ? 0 - centerx : 0;
    int cy = centerPec ? 0 - centery : 0;
    pesBuffer pecStitches = pesCreatePECStitch(pPesData, cx, cy);  //-centerx, -centery );

    minx -= centerx;
    miny -= centery;
    maxx -= centerx;
    maxy -= centery;
    centerx = 0;
    centery = 0;

    size_t pecOffset = pes.size();
    size_t stitchOffset = pecOffset + 532;
    size_t gfxOffset = stitchOffset + pecStitches.size();

    size_t gof = 20 + pecStitches.size();

    buffer.appendU16(0x0000);         // Unknown
    buffer.appendU16((uint16_t)gof);  // Offset to thumbnail

    buffer.appendU16(0x3100);  // Unknown
    buffer.appendU16(0xF0FF);  // Unknown

    int w = maxx - minx;
    int h = maxy - miny;
    SkDebugf( "W: %i H: %i\n", w, h );

    buffer.appendU16(w);  // Width
    buffer.appendU16(h);  // Width

    buffer.appendU16(0x01E0);  // Unknown
    buffer.appendU16(0x01B0);  // Unknown

    int w2 = 0 - minx;  // w/2-centerx;
    int h2 = 0 - miny;  // h/2-centery;

    // printf("w,h,cx,cy: %i, %i, %i, %i, %i, %i \n", w, h, centerx, centery, w2, h2);

    unsigned char wh = ((w2 & 0xFF00) >> 8);
    unsigned char wl = w2 & 0xFF;

    unsigned char hh = ((h2 & 0xFF00) >> 8);
    unsigned char hl = h2 & 0xFF;

    if (0 && ((hh | wh) & 0x80)) {
        buffer.appendU8(0x90);
        buffer.appendU8(0x00);
        buffer.appendU8(0x90);
        buffer.appendU8(0x00);
    } else {
        buffer.appendU8(((w2 & 0xFF00) >> 8) + 0x90);  // Thumbnail position for preview
        buffer.appendU8(w2 & 0xFF);                    //
        buffer.appendU8(((h2 & 0xFF00) >> 8) + 0x90);  //
        buffer.appendU8(h2 & 0xFF);                    //
    }

    buffer.append(pecStitches);  // PEC stitches list
    pecStitches.clear();

    PES_encodePECGraphics(buffer, pPesData);

    pes.append(buffer);
    buffer.clear();
}

pesBuffer pesCreateThumbnailDefaultBuffer() {
    unsigned char image[38][48];

    PES_createPecImageBorder(image);

    pesBuffer buffer;
    PES_writeImage(buffer, image);

    return buffer;
}

void PES_encodeDefaultPEC(pesBuffer& pes) {
    pesBuffer buffer;
    buffer.appendString("LA:");  // PEC section ID

    buffer.appendU8(0x20, 16);  // Label
    buffer.appendU8('\r');      // Carriage return
    buffer.appendU8(0x20, 11);  // Unknown
    buffer.appendU8(0x20);      // Unknown (0x20 | 0xFF)
    buffer.appendU16(0x00FF);   // Unknown

    buffer.appendU8(6);   // Thumbnail with in bytes (48 pixels = 6 bytes)
    buffer.appendU8(38);  // Thumbnail height in lines

    buffer.appendU8(0x20, 12);  // Unknown

    int numColor = 1;
    buffer.appendU8(numColor - 1);  // Number of color - 1

    for (int i = 0; i < numColor; i++) {
        int pecIndex = 0;
        buffer.appendU8(pecIndex);
    }
    buffer.appendU8(0x20, 463 - numColor);  // Unknown

    int n = (int)buffer.size();
    assert(n == 512);

    size_t pecOffset = pes.size();
    size_t stitchOffset = pecOffset + 532;
    size_t gfxOffset = stitchOffset + 0;

    size_t gof = 20 + 0;

    buffer.appendU16(0x0000);         // Unknown
    buffer.appendU16((uint16_t)gof);  // Offset to thumbnail

    buffer.appendU16(0x3100);  // Unknown
    buffer.appendU16(0xF0FF);  // Unknown

    int minx = 0;
    int miny = 0;
    int maxx = 0;
    int maxy = 0;

    int centerx = 0;
    int centery = 0;

    int w = maxx - minx;
    int h = maxy - miny;

    buffer.appendU16(w);  // Width
    buffer.appendU16(h);  // Width

    buffer.appendU16(0x01E0);  // Unknown
    buffer.appendU16(0x01B0);  // Unknown

    int w2 = w / 2 - centerx;
    int h2 = h / 2 - centery;
    buffer.appendU8(((w2 & 0xFF00) >> 8) + 0x90);  // Thumbnail position for preview
    buffer.appendU8(w2 & 0xFF);                    //
    buffer.appendU8(((h2 & 0xFF00) >> 8) + 0x90);  //
    buffer.appendU8(h2 & 0xFF);                    //

    pes.append(buffer);
    buffer.clear();
}

void PES_encodePES5Extra(pesBuffer& pes, pesData* pPesData) {
    pesBuffer buffer;

    int numColor = (int)(pPesData->colors.size());
    buffer.appendU8(numColor - 1);
    for (int i = 0; i < numColor; i++) {
        int pecIndex = pPesData->colors[i];
        buffer.appendU8(pecIndex);
    }
    buffer.appendU8(0x20, 127 - numColor);

    for (int i = 0; i < numColor; i++) {
        int pecIndex = pPesData->colors[i];

        buffer.appendU8(0x00, 12);  // 12

        pesBuffer b1(&pes5threadCode[pecIndex - 1][1], 52);
        buffer.append(b1);  // 52

        buffer.appendU8(0x00, 24);  // 24

        pesBuffer b2(pes5extraCode, 44);
        buffer.append(b2);  // 44

        buffer.appendU8(0x00, 12);  // 12
    }

    for (int i = 0; i < numColor; i++) {
        int pecIndex = pPesData->colors[i];
        pesColor color = pesGetBrotherColor(pecIndex);
        buffer.appendU8(pes5threadCode[pecIndex - 1][53]);  // RGB
        buffer.appendU8(pes5threadCode[pecIndex - 1][54]);
        buffer.appendU8(pes5threadCode[pecIndex - 1][55]);
    }

    pes.append(buffer);
}

pesBuffer pesCreatePES5Buffer(pesData* pPesData, int option, pesVec2f* pHoopSize) {
    // Header ------------------------------
    pesVec2f hoopSize = pHoopSize == nullptr ? pesGetDocument()->getHoopSizeInMM() : *pHoopSize;

    pesBuffer pes = pesBuffer();

    PES_encodePES5Header(pes, pPesData, hoopSize);
    PES_encodeCEmbOne(pes, pPesData, hoopSize);
    PES_encodeCSewSeg(pes, pPesData, hoopSize, 5);

    pes.appendU8(0x00, 8);  // Wilcom has only this chunk before

    int numColor = (int)(pPesData->colors.size());
    for (int i = 0; i < numColor; i++) {
        pes.appendU32(0);
        pes.appendU32(i);
    }

    int pecOffset = (int)pes.size();
    unsigned char* pPes = (unsigned char*)pes.getData();
    // PEC offset
    pPes[8] = (pecOffset & 0xFF);
    pPes[9] = (pecOffset & 0xFF00) >> 8;
    pPes[10] = (pecOffset & 0xFF0000) >> 16;
    pPes[11] = (pecOffset & 0xFF000000) >> 24;

    pes.set((char*)pPes, pecOffset);

    bool centerPec = (option & pes_OPTION_NOT_CENTER_PEC) ? false : true;
    SkDebugf( "Center PEC:%i %i\n", option, centerPec );
    PES_encodePEC(pes, pPesData, centerPec);

    if ((option & pes_OPTION_PES5_NO_TAIL) == 0) {
        PES_encodePES5Extra(pes, pPesData);
    }

    return pes;
}

pesBuffer pesCreatePINNBuffer(pesData* pPesData, int option) {
    pesBuffer pinnBuff;
    if (pPesData == NULL) {
        //        ofLogError("PES_CreatePINNBuffer") << "pes no data";
        return pinnBuff;
    }

    bool center = (option & pes_OPTION_NOT_CENTER_PEC) ? false : true;
    if (center) {
        pesVec2f vc = pPesData->getBoundingBox().getCenter();
        if (vc.length()) {
            pPesData->translate(-vc.x, -vc.y);
        }
    }

    std::vector<uint8_t> colors = pPesData->colors;
    char numColor = colors.size();
    char numColorChange = numColor - 1;
    pinnBuff.append("#PINN");
    pinnBuff.appendU8(numColorChange);
    pinnBuff.append((const char*)colors.data(), numColor);
    pinnBuff.append(pesCreatePECStitch(pPesData));

    return pinnBuff;
}

pesBuffer pesCreatePES1Buffer(pesData* pPesData, int option, pesVec2f* pHoopSize) {
    // Header ------------------------------
    pesVec2f hoopSize = pHoopSize == nullptr ? pesGetDocument()->getHoopSizeInMM() : *pHoopSize;

    pesBuffer pes = pesBuffer();

    PES_encodePES1Header(pes, pPesData, hoopSize);
    PES_encodeCEmbOne(pes, pPesData, hoopSize);
    PES_encodeCSewSeg(pes, pPesData, hoopSize, 1);

    pes.appendU8(0x00, 4);

    int pecOffset = (int)pes.size();
    unsigned char* pPes = (unsigned char*)pes.getData();
    pPes[8] = (pecOffset & 0xFF);
    pPes[9] = (pecOffset & 0xFF00) >> 8;
    pPes[10] = (pecOffset & 0xFF0000) >> 16;
    pPes[11] = (pecOffset & 0xFF000000) >> 24;
    pes.set((char*)pPes, pecOffset);

    bool centerPec = (option & pes_OPTION_NOT_CENTER_PEC) ? false : true;
    PES_encodePEC(pes, pPesData, centerPec);

    return pes;
}

pesBuffer pesCreatePES1DefaultHeaderBuffer() {
    pesBuffer pes = pesBuffer();

    PES_encodePES1DefaultHeader(pes);
    PES_encodeDefaultCEmbOne(pes);

    pes.appendU8(0x00, 4);

    int pecOffset = (int)pes.size();
    unsigned char* pPes = (unsigned char*)pes.getData();
    pPes[8] = (pecOffset & 0xFF);
    pPes[9] = (pecOffset & 0xFF00) >> 8;
    pPes[10] = (pecOffset & 0xFF0000) >> 16;
    pPes[11] = (pecOffset & 0xFF000000) >> 24;
    pes.set((char*)pPes, pecOffset);

    return pes;
}

pesBuffer pesCreatePECDefaultHeaderBuffer() {
    pesBuffer pec = pesBuffer();

    PES_encodeDefaultPEC(pec);

    return pec;
}

// pom (DST)

int pes_roundDouble(double src) {
    if (src < 0.0) return (int)ceil(src - 0.5);
    return (int)floor(src + 0.5);
}

unsigned char PES_dstSetBit(int pos) { return (unsigned char)(1 << pos); }

void PES_dstEncodeRecord(pesBuffer& buff, int x, int y, int flag) {
    char b0, b1, b2;
    b0 = b1 = b2 = 0;

    /* cannot encode values > +121 or < -121. */
    //    if(x > 121 || x < -121) GUI_LogError("dst_encode_record", "x is not in valid range
    //    [-121,121] , x = %d\n", x); if(y > 121 || y < -121) GUI_LogError("dst_encode_record", "y
    //    is not in valid range [-121,121] , y = %d\n", y);

    if (x >= +41) {
        b2 += PES_dstSetBit(2);
        x -= 81;
    }
    if (x <= -41) {
        b2 += PES_dstSetBit(3);
        x += 81;
    }
    if (x >= +14) {
        b1 += PES_dstSetBit(2);
        x -= 27;
    }
    if (x <= -14) {
        b1 += PES_dstSetBit(3);
        x += 27;
    }
    if (x >= +5) {
        b0 += PES_dstSetBit(2);
        x -= 9;
    }
    if (x <= -5) {
        b0 += PES_dstSetBit(3);
        x += 9;
    }
    if (x >= +2) {
        b1 += PES_dstSetBit(0);
        x -= 3;
    }
    if (x <= -2) {
        b1 += PES_dstSetBit(1);
        x += 3;
    }
    if (x >= +1) {
        b0 += PES_dstSetBit(0);
        x -= 1;
    }
    if (x <= -1) {
        b0 += PES_dstSetBit(1);
        x += 1;
    }
    //    if(x !=   0) { GUI_LogError("dst_encode_record", "x should be zero yet x = %d\n", x); }
    if (y >= +41) {
        b2 += PES_dstSetBit(5);
        y -= 81;
    }
    if (y <= -41) {
        b2 += PES_dstSetBit(4);
        y += 81;
    }
    if (y >= +14) {
        b1 += PES_dstSetBit(5);
        y -= 27;
    }
    if (y <= -14) {
        b1 += PES_dstSetBit(4);
        y += 27;
    }
    if (y >= +5) {
        b0 += PES_dstSetBit(5);
        y -= 9;
    }
    if (y <= -5) {
        b0 += PES_dstSetBit(4);
        y += 9;
    }
    if (y >= +2) {
        b1 += PES_dstSetBit(7);
        y -= 3;
    }
    if (y <= -2) {
        b1 += PES_dstSetBit(6);
        y += 3;
    }
    if (y >= +1) {
        b0 += PES_dstSetBit(7);
        y -= 1;
    }
    if (y <= -1) {
        b0 += PES_dstSetBit(6);
        y += 1;
    }
    //    if(y !=   0) { GUI_LogError("dst_encode_record", "y should be zero yet y = %d\n", y); }

    b2 |= (char)3;

    if (flag & END_STITCH) {
        b2 = (char)-13;
        b0 = b1 = (char)0;
    }

    if (flag & (JUMP_STITCH | TRIM_STITCH)) {
        b2 = (char)(b2 | 0x83);
    }
    if (flag & STOP_STITCH) {
        b2 = (char)(b2 | 0xC3);
    }

    buff.appendU8((unsigned char)b0);
    buff.appendU8((unsigned char)b1);
    buff.appendU8((unsigned char)b2);
}

pesBuffer pesCreateDSTStitch(pesData* pPesData, int* totalStitches) {
    // build relative stich&encode
    double thisX = 0.0;
    double thisY = 0.0;
    int count = 0;
    pesBuffer stitches;

    // Machine Home position
    const int pixel_per_mm = 10;
    const float jmp_tol = SHORT_JUMP_THRESHOLD_MM * pixel_per_mm;
    pesStitchBlockList blocks;
    pPesData->getStitchBlockList(blocks);

    for (int i = 0; i < (int)blocks.size(); i++) {
        pesStitchBlock block = blocks[i];
        if (i > 0) {
            // write change color(start new block)
            // PES_dstEncodeRecord(stitches, 0, 0, STOP_STITCH);
            PES_dstEncodeRecord(stitches, 0, 0, TRIM_STITCH);
            count++;
            PES_dstEncodeRecord(stitches, 0, 0, STOP_STITCH);
            count++;
        }
        for (int j = 0; j < (int)block.size(); j++) {
            pesVec2f stitch = block.polyline[j];
            int type = block.types[j];

            int deltaX = pes_roundDouble(stitch.x - thisX);
            int deltaY = pes_roundDouble(stitch.y - thisY);
            thisX += (double)deltaX;
            thisY += (double)deltaY;

            // when 2 or more pes are merged into may we can found end flag before last stitches
            // so we just skip this stitch
            if (type == STOP_STITCH || type == END_STITCH) {
                continue;
            } else if (deltaX <= 121 && deltaX >= -121 && deltaY <= 121 && deltaY >= -121) {
                // dst max stitch length = 121 both normal & jump
                pesVec2f step(deltaX, deltaY);
                if (step.length() <= jmp_tol) {
                    type = NORMAL_STITCH;
                }
                PES_dstEncodeRecord(stitches, deltaX, deltaY, type);
                count++;
            } else {
                // MARK: Incase deltaX/deltaY >121 or <-121 make double jump or multiple jump
                double _max = (double)MAX((float)abs(deltaX), (float)abs(deltaY));
                int nround = (int)ceil((_max + 1) / 121);
                for (int i = 0; i < nround; i++) {
                    float stepX = (deltaX / (float)nround);
                    float stepY = (deltaY / (float)nround);
                    PES_dstEncodeRecord(stitches, pes_roundDouble(stepX), pes_roundDouble(stepY),
                                        type);
                    count++;
                }
            }
        }
    }
    // end of stitch data
    PES_dstEncodeRecord(stitches, 0, 0, END_STITCH);
    //    count++;
    if (totalStitches != NULL) {
        *totalStitches = count;
    }
    return stitches;
}

pesBuffer pesCreateDSTBuffer(pesData* pPesData) {
    pesBuffer buff;
    pesStitchBlockList blocks;
    pPesData->getStitchBlockList(blocks);

    if (blocks.empty()) {
        //        GUI_LogError("pesCreateDSTBuffer", "Fail stitch block are empty");
        return buff;
    }

    // Machine Home position
    //    const int pixel_per_mm = 10;

    pPesData->recalculate();
    int totalStitches;
    const pesBuffer& stitchBuff = pesCreateDSTStitch(pPesData, &totalStitches);
    int xx, yy;  // , dx, dy, flags;
    int co = 1, st = 0;
    int ax, ay, mx, my;
    const char* pd = 0;

    xx = yy = 0;
    co = 1;
    co = (int)pPesData->colors.size();
    st = 0;
    st = totalStitches;

#ifdef __WIN32__
    buff.append("LA:Untitled        \x0d");
#else
    buff.append(pesVAArgsToString("LA:%-16s\x0d", "Untitled"));
#endif
    buff.append(pesVAArgsToString("ST:%7d\x0d", st));
    buff.append(pesVAArgsToString("CO:%3d\x0d",
                                  co - 1));  // number of color changes, not number of colors!
    buff.append(pesVAArgsToString("+X:%5d\x0d", (int)abs(pPesData->maxx)));
    buff.append(pesVAArgsToString("-X:%5d\x0d", (int)abs(pPesData->minx)));
    buff.append(pesVAArgsToString("+Y:%5d\x0d", (int)abs(pPesData->maxy)));
    buff.append(pesVAArgsToString("-Y:%5d\x0d", (int)abs(pPesData->miny)));

    ax = ay = mx = my = 0;
    pd = 0;
    if (pd == 0 || strlen(pd) != 6) {
        // pd is not valid, so fill in a default consisting of "******"
        pd = "******";
    }
    buff.append(pesVAArgsToString("AX:+%5d\x0d", ax));
    buff.append(pesVAArgsToString("AY:+%5d\x0d", ay));
    buff.append(pesVAArgsToString("MX:+%5d\x0d", mx));
    buff.append(pesVAArgsToString("MY:+%5d\x0d", my));
    buff.append(pesVAArgsToString("PD:%6s\x0d", pd));
    buff.appendU8(0x1a);  // 0x1a is the code for end of section.

    // fill 0x20 padding to size 512
    int currentBufferSize = buff.size();
    int numOfPadding = 512 - currentBufferSize;
    buff.appendU8(0x20, numOfPadding);

    buff.append(stitchBuff);

    buff.appendU8(0xa1);
    buff.appendU8(0, 2);

    return buff;
}

// pom : PES5 Import DST
void decode_tajima_ternary(unsigned char b[3], int *x, int *y){
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

int decode_tajima_record_flags(unsigned char b2){
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

// end pom (DST)

void PES_expEncode(pesBuffer& buff, char dx, char dy, int flag) {
    if (flag == STOP_STITCH) {
        buff.appendU8(0x80);
        buff.appendU8(1);
        buff.appendU8(dx);
        buff.appendU8(dy);
    } else if (flag == TRIM_STITCH || flag == JUMP_STITCH) {
        //        buff.appendU8(0x80);
        //        buff.appendU8(2);
        //        buff.appendU8(dx);
        //        buff.appendU8(dy);
        buff.appendU8(0x80);
        buff.appendU8(4);
        buff.appendU8(dx);
        buff.appendU8(dy);
    } else if (flag == END_STITCH) {
        //        buff.appendU8(0x80);
        //        buff.appendU8(0x10);
        //        buff.appendU8(0);
        //        buff.appendU8(0);
    } else {
        buff.appendU8(dx);
        buff.appendU8(dy);
    }
}

pesBuffer pesCreateEXPBuffer(pesData* pPesData) {
    // build relative stich&encode
    double thisX = 0.0;
    double thisY = 0.0;
    pesBuffer stitches;

    const int unit_per_mm = 10;
    const float jmp_tol = SHORT_JUMP_THRESHOLD_MM * unit_per_mm;
    pesStitchBlockList blocks;
    pPesData->getStitchBlockList(blocks);
    for (int i = 0; i < (int)blocks.size(); i++) {
        pesStitchBlock block = blocks[i];
        if (i > 0) {
            // write change color(start new block)
            // PES_dstEncodeRecord(stitches, 0, 0, TRIM_STITCH);
            PES_expEncode(stitches, 0, 0, STOP_STITCH);
        }
        for (int j = 0; j < (int)block.size(); j++) {
            pesVec2f stitch = block.polyline[j];
            int type = block.types[j];

            int deltaX = pes_roundDouble(stitch.x - thisX);
            int deltaY = pes_roundDouble(stitch.y - thisY);
            thisX += (double)deltaX;
            thisY += (double)deltaY;

            // when 2 or more pes are merged into may we can found end flag before last stitches
            // so we just skip this stitch
            if (type == STOP_STITCH || type == END_STITCH) {
                continue;
            } else if (deltaX <= 127 && deltaX >= -127 && deltaY <= 127 && deltaY >= -127) {
                // dst max stitch length = 127 both normal & jump
                pesVec2f step(deltaX, deltaY);
                if (step.length() <= jmp_tol) {
                    type = NORMAL_STITCH;
                }
                PES_expEncode(stitches, deltaX, deltaY, type);
            } else {
                // MARK: Incase deltaX/deltaY >127 or <-127 make double jump or multiple jump
                double _max = (double)MAX((float)abs(deltaX), (float)abs(deltaY));
                int nround = (int)ceil((_max + 1) / 127);
                for (int i = 0; i < nround; i++) {
                    float stepX = (deltaX / (float)nround);
                    float stepY = (deltaY / (float)nround);
                    PES_expEncode(stitches, pes_roundDouble(stepX), pes_roundDouble(stepY), type);
                }
                //                if(nround>1){
                //                    // sereies of jump, set anchor point with 0,0
                //                    PES_expEncode(stitches, 0, 0, NORMAL_STITCH);
                //                }
            }
        }
    }
    // end of stitch data
    PES_expEncode(stitches, 0, 0, END_STITCH);
    //    embFile_printf(file, "\x1a");

    return stitches;
}

// MARK - Janome(JEF)
typedef struct JEFThread_ {
    pesColor color;
    const char* description;
    const char* catalogNumber;
} jefThread_t;

static const jefThread_t jefThreads[] = {{{0, 0, 0}, "Black", ""},
                                         {{0, 0, 0}, "Black", ""},
                                         {{255, 255, 255}, "White", ""},
                                         {{255, 255, 23}, "Yellow", ""},
                                         {{250, 160, 96}, "Orange", ""},
                                         {{92, 118, 73}, "Olive Green", ""},
                                         {{64, 192, 48}, "Green", ""},
                                         {{101, 194, 200}, "Sky", ""},
                                         {{172, 128, 190}, "Purple", ""},
                                         {{245, 188, 203}, "Pink", ""},
                                         {{255, 0, 0}, "Red", ""},
                                         {{192, 128, 0}, "Brown", ""},
                                         {{0, 0, 240}, "Blue", ""},
                                         {{228, 195, 93}, "Gold", ""},
                                         {{165, 42, 42}, "Dark Brown", ""},
                                         {{213, 176, 212}, "Pale Violet", ""},
                                         {{252, 242, 148}, "Pale Yellow", ""},
                                         {{240, 208, 192}, "Pale Pink", ""},
                                         {{255, 192, 0}, "Peach", ""},
                                         {{201, 164, 128}, "Beige", ""},
                                         {{155, 61, 75}, "Wine Red", ""},
                                         {{160, 184, 204}, "Pale Sky", ""},
                                         {{127, 194, 28}, "Yellow Green", ""},
                                         {{185, 185, 185}, "Silver Grey", ""},
                                         {{160, 160, 160}, "Grey", ""},
                                         {{152, 214, 189}, "Pale Aqua", ""},
                                         {{184, 240, 240}, "Baby Blue", ""},
                                         {{54, 139, 160}, "Powder Blue", ""},
                                         {{79, 131, 171}, "Bright Blue", ""},
                                         {{56, 106, 145}, "Slate Blue", ""},
                                         {{0, 32, 107}, "Nave Blue", ""},
                                         {{229, 197, 202}, "Salmon Pink", ""},
                                         {{249, 103, 107}, "Coral", ""},
                                         {{227, 49, 31}, "Burnt Orange", ""},
                                         {{226, 161, 136}, "Cinnamon", ""},
                                         {{181, 148, 116}, "Umber", ""},
                                         {{228, 207, 153}, "Blonde", ""},
                                         {{225, 203, 0}, "Sunflower", ""},
                                         {{225, 173, 212}, "Orchid Pink", ""},
                                         {{195, 0, 126}, "Peony Purple", ""},
                                         {{128, 0, 75}, "Burgundy", ""},
                                         {{160, 96, 176}, "Royal Purple", ""},
                                         {{192, 64, 32}, "Cardinal Red", ""},
                                         {{202, 224, 192}, "Opal Green", ""},
                                         {{137, 152, 86}, "Moss Green", ""},
                                         {{0, 170, 0}, "Meadow Green", ""},
                                         {{33, 138, 33}, "Dark Green", ""},
                                         {{93, 174, 148}, "Aquamarine", ""},
                                         {{76, 191, 143}, "Emerald Green", ""},
                                         {{0, 119, 114}, "Peacock Green", ""},
                                         {{112, 112, 112}, "Dark Grey", ""},
                                         {{242, 255, 255}, "Ivory White", ""},
                                         {{177, 88, 24}, "Hazel", ""},
                                         {{203, 138, 7}, "Toast", ""},
                                         {{247, 146, 123}, "Salmon", ""},
                                         {{152, 105, 45}, "Cocoa Brown", ""},
                                         {{162, 113, 72}, "Sienna", ""},
                                         {{123, 85, 74}, "Sepia", ""},
                                         {{79, 57, 70}, "Dark Sepia", ""},
                                         {{82, 58, 151}, "Violet Blue", ""},
                                         {{0, 0, 160}, "Blue Ink", ""},
                                         {{0, 150, 222}, "Solar Blue", ""},
                                         {{178, 221, 83}, "Green Dust", ""},
                                         {{250, 143, 187}, "Crimson", ""},
                                         {{222, 100, 158}, "Floral Pink", ""},
                                         {{181, 80, 102}, "Wine", ""},
                                         {{94, 87, 71}, "Olive Drab", ""},
                                         {{76, 136, 31}, "Meadow", ""},
                                         {{228, 220, 121}, "Mustard", ""},
                                         {{203, 138, 26}, "Yellow Ochre", ""},
                                         {{198, 170, 66}, "Old Gold", ""},
                                         {{236, 176, 44}, "Honeydew", ""},
                                         {{248, 128, 64}, "Tangerine", ""},
                                         {{255, 229, 5}, "Canary Yellow", ""},
                                         {{250, 122, 122}, "Vermillion", ""},
                                         {{107, 224, 0}, "Bright Green", ""},
                                         {{56, 108, 174}, "Ocean Blue", ""},
                                         {{227, 196, 180}, "Beige Grey", ""},
                                         {{227, 172, 129}, "Bamboo", ""}};

#define JEF_HOOP_110X110 0
#define JEF_HOOP_50X50 1
#define JEF_HOOP_140X200 2
#define JEF_HOOP_126X110 3
#define JEF_HOOP_200X200 4

static int jefGetHoopSize(int width, int height) {
    //    if(width <  500 && height <  500) { return JEF_HOOP_50X50; }
    //    if(width < 1260 && height < 1100) { return JEF_HOOP_126X110; }
    //    if(width < 1400 && height < 2000) { return JEF_HOOP_140X200; }
    //    if(width < 2000 && height < 2000) { return JEF_HOOP_200X200; }
    //    return ((int) JEF_HOOP_110X110);

    if (width < 500 && height < 500) {
        return JEF_HOOP_50X50;
    }
    if (width < 1100 && height < 1100) {
        return JEF_HOOP_110X110;
    }
    if (width < 1260 && height < 1100) {
        return JEF_HOOP_126X110;
    }
    if (width < 1400 && height < 2000) {
        return JEF_HOOP_140X200;
    }
    if (width < 2000 && height < 2000) {
        return JEF_HOOP_200X200;
    }
    return ((int)JEF_HOOP_110X110);
}

void PES_jefEncode(pesBuffer& buff, char dx, char dy, int flag) {
    if (flag == STOP_STITCH) {
        buff.appendU8(0x80);
        buff.appendU8(1);
        buff.appendU8(dx);
        buff.appendU8(dy);
    } else if (flag == TRIM_STITCH) {
        buff.appendU8(0x80);
        buff.appendU8(2);
        buff.appendU8(dx);
        buff.appendU8(dy);
    } else if (flag == JUMP_STITCH) {
        buff.appendU8(0x80);
        buff.appendU8(4);
        buff.appendU8(dx);
        buff.appendU8(dy);
    } else if (flag == END_STITCH) {
        buff.appendU8(0x80);
        buff.appendU8(0x10);
        //        buff.appendU8(0);
        //        buff.appendU8(0);
    } else {
        buff.appendU8(dx);
        buff.appendU8(dy);
    }
}

pesBuffer PES_CreateJEFStitchBuffer(pesData* pPesData, int& stitchCnt, int& jumpAndStopCnt) {
    pesBuffer buffer;
    double thisX = 0;
    double thisY = 0;
    pesStitchBlockList blocks;
    pPesData->getStitchBlockList(blocks);

    for (int i = 0; i < (int)blocks.size(); i++) {
        pesStitchBlock block = blocks[i];
        if (i > 0) {
            // write change color(start new block)
            // PES_dstEncodeRecord(stitches, 0, 0, TRIM_STITCH);
            PES_jefEncode(buffer, 0, 0, STOP_STITCH);
            jumpAndStopCnt++;
        }
        for (int j = 0; j < (int)block.size(); j++) {
            pesVec2f stitch = block.polyline[j];
            int type = block.types[j];

            int deltaX = pes_roundDouble(stitch.x - thisX);
            int deltaY = pes_roundDouble(stitch.y - thisY);
            thisX += (double)deltaX;
            thisY += (double)deltaY;

            // when 2 or more pes are merged into may we can found end flag before last stitches
            // so we just skip this stitch
            if (type == STOP_STITCH || type == END_STITCH) {
                continue;
            } else if (deltaX <= 127 && deltaX >= -127 && deltaY <= 127 && deltaY >= -127) {
                // dst max stitch length = 127 both normal & jump
                PES_jefEncode(buffer, deltaX, deltaY, type);
                if (type == JUMP_STITCH || type == TRIM_STITCH) {
                    jumpAndStopCnt++;
                } else {
                    stitchCnt++;
                }
            } else {
                // MARK: Incase deltaX/deltaY >127 or <-127 make double jump or multiple jump
                double _max = (double)MAX((float)abs(deltaX), (float)abs(deltaY));
                int nround = (int)ceil((_max + 1) / 127);
                for (int i = 0; i < nround; i++) {
                    float stepX = (deltaX / (float)nround);
                    float stepY = (deltaY / (float)nround);
                    PES_jefEncode(buffer, pes_roundDouble(stepX), pes_roundDouble(stepY), type);
                    if (type == JUMP_STITCH || type == TRIM_STITCH) {
                        jumpAndStopCnt++;
                    } else {
                        stitchCnt++;
                    }
                }
            }
        }
    }
    // end of stitch data
    PES_jefEncode(buffer, 0, 0, END_STITCH);

    return buffer;
}

int PES_jefFindNearestColorIndex(pesColor srcColor) {
    double currentClosestValue = 9999999;
    int closestIndex = -1;
    int red = srcColor.r;
    int green = srcColor.g;
    int blue = srcColor.b;
    for (int i = 1; i < 79; i++) {
        int deltaRed;
        int deltaBlue;
        int deltaGreen;
        double dist;
        pesColor c = jefThreads[i].color;

        deltaRed = red - c.r;
        deltaBlue = green - c.g;
        deltaGreen = blue - c.b;

        dist = sqrt((double)(deltaRed * deltaRed) + (deltaBlue * deltaBlue) +
                    (deltaGreen * deltaGreen));
        if (dist <= currentClosestValue) {
            currentClosestValue = dist;
            closestIndex = i;
        }
    }
    return closestIndex;
}

pesBuffer pesCreateJEFBuffer(pesData* pPesData) {
    // Make sure embroidery is center to(0,0)
    if (pPesData->centerx != 0 || pPesData->centery != 0) {
        pPesData->translate(-pPesData->centerx, -pPesData->centery);
        pPesData->recalculate();
    }

    // #1 write color table size
    pesBuffer buffer;
    int numColor = (int)(pPesData->colors.size());
    buffer.appendU32(0x74 + (numColor * 8));
    buffer.appendU32(0x0A);  // Unknow

    // #2 write date-time
    std::time_t t = std::time(0);
    std::tm* now = std::localtime(&t);
    int year = now->tm_year + 1900;
    int month = now->tm_mon + 1;
    int day = now->tm_mday;
    int hour = now->tm_hour;
    int minute = now->tm_min;
    int sec = now->tm_sec;
    std::string date =
            pesVAArgsToString("%04d%02d%02d%02d%02d%02d", year, month, day, hour, minute, sec);
    buffer.append(date);

    buffer.appendU8(0);  // Version letter. 12000: m, 11000: n, 10000v3: o, 10000 v2.2 p, 9000 q,
                         // mc350, r, mc200 s, mb4 t
    buffer.appendU8(0);  // Janome's software leaves this as 00 00 at times

    buffer.appendU32(numColor);

    // #3 write total stitches count
    int stitchCount = 0;
    int jumpAndStopCount = 0;
    pesBuffer stitches = PES_CreateJEFStitchBuffer(pPesData, stitchCount, jumpAndStopCount);
    //    buffer.appendU32(stitchCount + jumpAndStopCount); // total stitches ??
    int totalStitchCount = stitchCount + jumpAndStopCount + jumpAndStopCount + 1;
    buffer.appendU32(totalStitchCount);

    // #4 write hoop size
    int designWidth = pPesData->getBoundingBox().width;
    int designHeight = pPesData->getBoundingBox().height;
    int hoopSizeIndex = jefGetHoopSize(designWidth, designHeight);
    buffer.appendU32(hoopSizeIndex);

    //     #5 write hoop padding
    //    /* Distance from default 110 x 110 Hoop */
    //    if(min(550 - designWidth / 2, 550 - designHeight / 2) >= 0)
    //    {
    //        buffer.append((int) max(-1, (1100 - designWidth) / 2));  /* left */
    //        buffer.append((int)max(-1, (1100 - designWidth) / 2));  /* right */
    //        buffer.append((int) max(-1, (1100 - designHeight) / 2)); /* top */
    //        buffer.append((int) max(-1, (1100 - designHeight) / 2)); /* bottom */
    //    }
    //    else
    //    {
    //        buffer.append((int)-1);
    //        buffer.append((int)-1);
    //        buffer.append((int)-1);
    //        buffer.append((int)-1);
    //    }
    //    /* Distance from default 50 x 50 Hoop */
    //    if(min((500 - designWidth) / 2, (500 - designHeight) / 2) >= 0)
    //    {
    //        buffer.append((int) max(-1, (500 - designWidth) / 2));  /* left */
    //        buffer.append((int)max(-1, (500 - designWidth) / 2));  /* right */
    //        buffer.append((int) max(-1, (500 - designHeight) / 2)); /* top */
    //        buffer.append((int) max(-1, (500 - designHeight) / 2)); /* bottom */
    //    }
    //    else
    //    {
    //        buffer.append(-1);
    //        buffer.append(-1);
    //        buffer.append(-1);
    //        buffer.append(-1);
    //    }
    //    /* Distance from default 140 x 200 Hoop */
    //    buffer.append((int)max(-1, ((1400 - designWidth) / 2)));   /* left */
    //    buffer.append((int)max(-1, ((2000 - designHeight) / 2))); /* top */
    //    buffer.append((int)max(-1, ((1400 - designWidth) / 2)));   /* right */
    //    buffer.append((int)max(-1, ((2000 - designHeight) / 2))); /* bottom */
    //    /* repeated Distance from default 140 x 200 Hoop */
    //    /* TODO: Actually should be distance to custom hoop */
    //    buffer.append((int)max(-1, ((1400 - designWidth) / 2)));  /* left */
    //    buffer.append((int)max(-1, ((2000 - designHeight) / 2))); /* top */
    //    buffer.append((int)max(-1, ((1400 - designWidth) / 2)));  /* right */
    //    buffer.append((int)max(-1, ((2000 - designHeight) / 2))); /* bottom */
    //
    //    buffer.appendU32(0); // ??
    //    buffer.appendU32(0); // ??
    //    buffer.appendU32(0); // ??
    //    buffer.appendU32(0); // ??

    // #5 write hoop padding
    // Distances from center of hoop.
    buffer.appendU32(designWidth / 2);   // -x
    buffer.appendU32(designHeight / 2);  // -y
    buffer.appendU32(designWidth / 2);   // +x
    buffer.appendU32(designHeight / 2);  // +y

    int hoopWidth, hoopHeight;
    // Distance from default 110 x 110 Hoop, or -1,-1,-1,-1 if does not fit.
    hoopWidth = 1100;
    hoopHeight = 1100;
    if (designWidth < hoopWidth && designHeight < hoopHeight) {
        buffer.appendU32((hoopWidth - designWidth) / 2);
        buffer.appendU32((hoopHeight - designHeight) / 2);
        buffer.appendU32((hoopWidth - designWidth) / 2);
        buffer.appendU32((hoopHeight - designHeight) / 2);
    } else {
        buffer.append((int)-1);
        buffer.append((int)-1);
        buffer.append((int)-1);
        buffer.append((int)-1);
    }
    // Distance from default 50 x 50 Hoop, or -1,-1,-1,-1 if does not fit.
    hoopWidth = 500;
    hoopHeight = 500;
    if (designWidth < hoopWidth && designHeight < hoopHeight) {
        buffer.appendU32((hoopWidth - designWidth) / 2);
        buffer.appendU32((hoopHeight - designHeight) / 2);
        buffer.appendU32((hoopWidth - designWidth) / 2);
        buffer.appendU32((hoopHeight - designHeight) / 2);
    } else {
        buffer.append((int)-1);
        buffer.append((int)-1);
        buffer.append((int)-1);
        buffer.append((int)-1);
    }
    // Distance from default 140 x 200 Hoop, or -1,-1,-1,-1 if does not fit.
    hoopWidth = 1400;
    hoopHeight = 2000;
    if (designWidth < hoopWidth && designHeight < hoopHeight) {
        buffer.appendU32((hoopWidth - designWidth) / 2);
        buffer.appendU32((hoopHeight - designHeight) / 2);
        buffer.appendU32((hoopWidth - designWidth) / 2);
        buffer.appendU32((hoopHeight - designHeight) / 2);
    } else {
        buffer.append((int)-1);
        buffer.append((int)-1);
        buffer.append((int)-1);
        buffer.append((int)-1);
    }
    // Distance from custom hoop, or -1,-1,-1,-1 if does not fit.
    buffer.append((int)-1);
    buffer.append((int)-1);
    buffer.append((int)-1);
    buffer.append((int)-1);

    // #6 write color
    for (int i = 0; i < numColor; i++) {
        // find nearest color (pes -> jef)
        pesColor pesColor = pesGetBrotherColor(pPesData->colors[i]);
        int jefColorIndex = PES_jefFindNearestColorIndex(pesColor);
        buffer.appendU32(jefColorIndex);
    }
    for (int i = 0; i < numColor; i++) {
        buffer.appendU32(0x0D);
    }

    // #7 write stitches data
    buffer.append(stitches);

    return buffer;
}

// MARK: gcode
#include "pesGcode.hpp"

pesBuffer pesCreateGcodeBuffer(pesData* pPesData) {
    const int unit_per_mm = 10;
    const float jmp_tol = SHORT_JUMP_THRESHOLD_MM * unit_per_mm;

    // information
    int stitchCount = 0;
    int jumpCount = 0;
    int longJumpCount = 0;  // jump w/ dist>10 mm
    int numColorChange = 0;

    PES_Gcode gcode;
    gcode.resetToZero();
    pesStitchBlockList blocks;
    pPesData->getStitchBlockList(blocks);
    bool hasStitch = blocks.size() && blocks.front().size();
    if (hasStitch) {
        pesVec2f last;
        last.x = last.y = 0;
        for (int i = 0; i < (int)blocks.size(); i++) {
            pesStitchBlock block = blocks[i];
            bool machine_stop = false;
            if (i > 0) {
                // change color(Machine Stop)
                machine_stop = true;
            }
            for (int j = 0; j < (int)block.size(); j++) {
                pesVec2f stitch = block.polyline[j];
                int type = block.types[j];
                if (type == STOP_STITCH || type == END_STITCH) {
                    continue;
                }

                // unit mm x 10
                pesVec2f cur(stitch.x, stitch.y);
                pesVec2f prev(last.x, last.y);
                float dist = cur.distance(prev);
                if (type == JUMP_STITCH || type == TRIM_STITCH) {
                    if (dist <= jmp_tol) {
                        type = NORMAL_STITCH;
                    }
                }
                // end unit mm x 10

                // unit mm
                cur = cur * 0.1;
                if (type == JUMP_STITCH || type == TRIM_STITCH) {
                    if (machine_stop && j == 0) {
                        machine_stop = false;
                        gcode.stopAt(cur.x, cur.y);
                        longJumpCount++;
                        numColorChange++;
                    } else {
                        int spd = dist > 100 ? PES_GCODE_FEED_SPD : PES_GCODE_RAPID_SPD;
                        gcode.moveTo(cur.x, cur.y, spd);
                        if (spd == PES_GCODE_FEED_SPD) {
                            longJumpCount++;
                        }
                    }
                    jumpCount++;
                } else if (type == NORMAL_STITCH) {
                    prev = prev * 0.1;
                    dist = cur.distance(prev);
                    if (dist > 10) {
                        int nround = (int)ceil(dist / 10.0);
                        float step = dist / nround;
                        pesVec2f dir = cur - prev;
                        dir.normalize();
                        for (int i = 0; i < nround; i++) {
                            pesVec2f to = prev + dir * (i + 1);
                            gcode.needleTo(to.x, to.y);
                            stitchCount++;
                        }
                    } else {
                        gcode.needleTo(cur.x, cur.y);
                        stitchCount++;
                    }
                }
                // end unit mm

                last = stitch;
            }
        }
    }
    gcode.end();

    //    GUI_LogNotice("stitchCount") << stitchCount;
    //    GUI_LogNotice("jumpCount") << jumpCount;
    //    GUI_LogNotice("longJumpCount") << longJumpCount;
    //    GUI_LogNotice("numColorChange(Machine stop)") << numColorChange;

    pesBuffer buffer;
    string ENDLINE = "\n";
    for (auto c : gcode.getCommands()) {
        buffer.append(c.toString() + ENDLINE);
    }
    //    GUI_LogNotice(buffer.getText());
    return buffer;
}

// MARK: XXX
void PES_xxxEncodeStop(pesBuffer & buff, char colorIndex){
    buff.appendU8(0x7F);
    buff.appendU8(colorIndex + 8);
}

void PES_xxxEncode(pesBuffer & buff, char dx, char dy, int flag){
    if(flag==JUMP_STITCH || flag==TRIM_STITCH){
        buff.appendU8(0x7E);
        buff.appendU16(dx);
        buff.appendU16(dy);
    }
    else if(flag==STOP_STITCH){
    }
    else if(flag==END_STITCH){
    }
    else{
        buff.appendU8(dx);
        buff.appendU8(dy);
    }
}

pesBuffer PES_CreateXXXStitchBuffer(pesData *pPesData, int & stitchCnt){
    pesBuffer buffer;
    double thisX = 0;
    double thisY = 0;
    
    pesStitchBlockList blocks;
    pPesData->getStitchBlockList(blocks);
    
    for (int i = 0; i < (int)blocks.size(); i++) {
        pesStitchBlock block = blocks[i];
        if (i > 0) {
            // write change color(start new block)
            char colorIndex = i;
            PES_xxxEncodeStop(buffer, colorIndex);
//            jumpAndStopCnt++;
        }
        for (int j = 0; j < (int)block.size(); j++) {
            auto stitch = block.polyline[j];
            int type = block.types[j];
            
            int deltaX = pes_roundDouble(stitch.x - thisX);
            int deltaY = pes_roundDouble(stitch.y - thisY);
            thisX += (double)deltaX;
            thisY += (double)deltaY;
            
            // when 2 or more pes are merged into may we can found end flag before last stitches
            // so we just skip this stitch
            if (type==STOP_STITCH || type==END_STITCH) {
                continue;
            } else if (deltaX <= 124 && deltaX >= -124 && deltaY <= 124 && deltaY >= -124) {
                // dst max stitch length = 127 both normal & jump
                PES_xxxEncode(buffer, deltaX, deltaY, type);
                if(type==JUMP_STITCH || type==TRIM_STITCH){
//                    jumpAndStopCnt++;
                }
                else{
                    stitchCnt++;
                }
            } else {
                // MARK: Incase deltaX/deltaY >124 or <-124 make double jump or multiple jump
                double _max = (double)MAX((float)abs(deltaX), (float)abs(deltaY));
                int nround = (int)ceil((_max + 1) / 124);
                for (int i = 0; i < nround; i++) {
                    float stepX = (deltaX / (float)nround);
                    float stepY = (deltaY / (float)nround);
                    PES_xxxEncode(buffer, pes_roundDouble(stepX), pes_roundDouble(stepY), type);
                    if(type==JUMP_STITCH || type==TRIM_STITCH){
//                        jumpAndStopCnt++;
                    }
                    else{
                        stitchCnt++;
                    }
                }
            }
        }
    }
    
    return buffer;
}

pesBuffer pesCreateXXXBuffer(pesData *pPesData){
    int stitchCnt;
    // stitch data
    pesBuffer stitches = PES_CreateXXXStitchBuffer(pPesData, stitchCnt);
    
    pesBuffer header;
    header.appendU8(0x00, 0x17);
    header.appendU32(stitchCnt);
    header.appendU8(0x00, 0x0C);
    header.appendU16((unsigned short)pPesData->colors.size());
    header.appendS16(0x0000);
    // boundary
    auto b = pPesData->getBoundingBox();
    header.appendS16((short)b.width);
    header.appendS16((short)b.height);
    header.appendS16((short)(b.width/2.0));
    header.appendS16((short)(b.height/2.0));
    header.appendS16((short)(b.width/2.0));
    header.appendS16((short)(b.height/2.0));
    header.appendU8(0x00, 0xC5);
    // offset
    int offset = int(stitches.size() + header.size() + 4);
    header.appendU32(offset);
    
    // final buffer
    pesBuffer buffer;
    buffer.append(header);
    buffer.append(stitches);
    buffer.appendU8(0x7F); // is this really correct?
    buffer.appendU8(0x7F);
    buffer.appendU8(0x03);
    buffer.appendU8(0x14);
    buffer.appendU8(0x00);
    buffer.appendU8(0x00);
    // colors
    int numColor = (int)pPesData->colors.size();
    for(int i=0; i<numColor; i++){
        pesColor color = pesGetBrotherColor(pPesData->colors[i]);
        buffer.appendU8(0x00);
        buffer.appendU8(color.r);
        buffer.appendU8(color.g);
        buffer.appendU8(color.b);
    }
    for(int i=0; i< 22-numColor; i++){
        buffer.appendU32(0x10000000);
    }
    // eof
    buffer.appendU8(0x00);
    buffer.appendU8(0x01);
    
    return buffer;
}

pesBuffer pesCreateDXFBuffer(pesData *pPesData){
    pesBuffer buffer;
    // source develop on other git and not complete yet
    return buffer;
}
