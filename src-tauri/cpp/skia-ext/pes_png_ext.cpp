// pesEncodePngWithDpi — replaces the fork's SkPngEncoder::Encode(...,ppm)
// overload (which just injected a pHYs chunk via png_set_pHYs). We encode
// with the stock 3-arg SkPngEncoder::Encode, then splice a pHYs chunk right
// after IHDR ourselves, so Skia's PNG encoder stays unpatched.
#include "skia-ext/pes_skia_ext.h"

#include "include/core/SkData.h"
#include "include/core/SkPixmap.h"
#include "include/core/SkStream.h"
#include "include/encode/SkPngEncoder.h"

#include <cstdint>
#include <cstring>

namespace {

// PNG/zlib CRC-32 (poly 0xEDB88320).
uint32_t crc32_png(const uint8_t* data, size_t len) {
    static uint32_t table[256];
    static bool init = false;
    if (!init) {
        for (uint32_t n = 0; n < 256; ++n) {
            uint32_t c = n;
            for (int k = 0; k < 8; ++k) {
                c = (c & 1) ? (0xEDB88320u ^ (c >> 1)) : (c >> 1);
            }
            table[n] = c;
        }
        init = true;
    }
    uint32_t c = 0xFFFFFFFFu;
    for (size_t i = 0; i < len; ++i) {
        c = table[(c ^ data[i]) & 0xFF] ^ (c >> 8);
    }
    return c ^ 0xFFFFFFFFu;
}

void put_be32(uint8_t* p, uint32_t v) {
    p[0] = (uint8_t)(v >> 24);
    p[1] = (uint8_t)(v >> 16);
    p[2] = (uint8_t)(v >> 8);
    p[3] = (uint8_t)v;
}

}  // namespace

namespace pes_skia {

bool encodePngWithDpi(SkWStream* dst, const SkPixmap& src, uint32_t ppm) {
    SkDynamicMemoryWStream tmp;
    if (!SkPngEncoder::Encode(&tmp, src, SkPngEncoder::Options())) {
        return false;
    }
    sk_sp<SkData> png = tmp.detachAsData();
    const uint8_t* bytes = png->bytes();
    const size_t size = png->size();

    // 8-byte signature + IHDR chunk (4 len + 4 type + 13 data + 4 crc = 25).
    const size_t kInsertAt = 8 + 25;
    if (ppm == 0 || size < kInsertAt) {
        return dst->write(bytes, size);
    }

    // pHYs: length 9, type "pHYs", x/y pixels-per-unit, unit=1 (metre), crc.
    uint8_t phys[4 + 4 + 9 + 4];
    put_be32(phys, 9);
    std::memcpy(phys + 4, "pHYs", 4);
    put_be32(phys + 8, ppm);
    put_be32(phys + 12, ppm);
    phys[16] = 1;
    put_be32(phys + 17, crc32_png(phys + 4, 4 + 9));  // crc over type+data

    bool ok = dst->write(bytes, kInsertAt);
    ok = ok && dst->write(phys, sizeof(phys));
    ok = ok && dst->write(bytes + kInsertAt, size - kInsertAt);
    return ok;
}

}  // namespace pes_skia
