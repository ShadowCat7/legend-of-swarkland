#include "string.hpp"

void String::decode(const ByteBuffer & bytes, bool * output_ok) {
    bool ok = true;
    for (int i = 0; i < bytes.length(); i += 1) {
        uint8_t byte1 = *((uint8_t*)&bytes[i]);
        if ((0x80 & byte1) == 0) {
            append(byte1);
        } else if ((0xe0 & byte1) == 0xc0) {
            if (++i >= bytes.length()) {
                ok = false;
                append(0xfffd);
                break;
            }
            uint8_t byte2 = *((uint8_t*)&bytes[i]);
            if ((byte2 & 0xc0) != 0x80) {
                ok = false;
                append(0xfffd);
                continue;
            }

            uint32_t bits_byte1 = (byte1 & 0x1f);
            uint32_t bits_byte2 = (byte2 & 0x3f);
            append(bits_byte2 | (bits_byte1 << 6));
        } else if ((0xf0 & byte1) == 0xe0) {
            if (++i >= bytes.length()) {
                ok = false;
                append(0xfffd);
                break;
            }
            uint8_t byte2 = *((uint8_t*)&bytes[i]);
            if ((byte2 & 0xc0) != 0x80) {
                ok = false;
                append(0xfffd);
                continue;
            }

            if (++i >= bytes.length()) {
                ok = false;
                append(0xfffd);
                break;
            }
            uint8_t byte3 = *((uint8_t*)&bytes[i]);
            if ((byte3 & 0xc0) != 0x80) {
                ok = false;
                append(0xfffd);
                continue;
            }

            uint32_t bits_byte1 = (byte1 & 0xf);
            uint32_t bits_byte2 = (byte2 & 0x3f);
            uint32_t bits_byte3 = (byte3 & 0x3f);
            append(bits_byte3 | (bits_byte2 << 6) | (bits_byte1 << 12));
        } else if ((0xf8 & byte1) == 0xf0) {
            if (++i >= bytes.length()) {
                ok = false;
                append(0xfffd);
                break;
            }
            uint8_t byte2 = *((uint8_t*)&bytes[i]);
            if ((byte2 & 0xc0) != 0x80) {
                ok = false;
                append(0xfffd);
                continue;
            }

            if (++i >= bytes.length()) {
                ok = false;
                append(0xfffd);
                break;
            }
            uint8_t byte3 = *((uint8_t*)&bytes[i]);
            if ((byte3 & 0xc0) != 0x80) {
                ok = false;
                append(0xfffd);
                continue;
            }

            if (++i >= bytes.length()) {
                ok = false;
                append(0xfffd);
                break;
            }
            uint8_t byte4 = *((uint8_t*)&bytes[i]);
            if ((byte3 & 0xc0) != 0x80) {
                ok = false;
                append(0xfffd);
                continue;
            }

            uint32_t bits_byte1 = (byte1 & 0x7);
            uint32_t bits_byte2 = (byte2 & 0x3f);
            uint32_t bits_byte3 = (byte3 & 0x3f);
            uint32_t bits_byte4 = (byte4 & 0x3f);
            append(bits_byte4 | (bits_byte3 << 6) | (bits_byte2 << 12) | (bits_byte1 << 18));
        } else {
            ok = false;
            append(0xfffd);
        }
    }
    *output_ok = ok;
}

void String::encode(ByteBuffer * output_bytes) const {
    for (int i = 0; i < _chars.length(); i += 1) {
        uint32_t codepoint = _chars[i];
        if (codepoint <= 0x7f) {
            // 00000000 00000000 00000000 0xxxxxxx
            unsigned char bytes[1] = {
                (unsigned char)(codepoint),
            };
            output_bytes->append((char*)bytes, 1);
        } else if (codepoint <= 0x7ff) {
            unsigned char bytes[2] = {
                // 00000000 00000000 00000xxx xx000000
                (unsigned char)(0xc0 | (codepoint >> 6)),
                // 00000000 00000000 00000000 00xxxxxx
                (unsigned char)(0x80 | (codepoint & 0x3f)),
            };
            output_bytes->append((char*)bytes, 2);
        } else if (codepoint <= 0xffff) {
            unsigned char bytes[3] = {
                // 00000000 00000000 xxxx0000 00000000
                (unsigned char)(0xe0 | (codepoint >> 12)),
                // 00000000 00000000 0000xxxx xx000000
                (unsigned char)(0x80 | ((codepoint >> 6) & 0x3f)),
                // 00000000 00000000 00000000 00xxxxxx
                (unsigned char)(0x80 | (codepoint & 0x3f)),
            };
            output_bytes->append((char*)bytes, 3);
        } else if (codepoint <= 0x1fffff) {
            unsigned char bytes[4] = {
                // 00000000 000xxx00 00000000 00000000
                (unsigned char)(0xf0 | (codepoint >> 18)),
                // 00000000 000000xx xxxx0000 00000000
                (unsigned char)(0x80 | ((codepoint >> 12) & 0x3f)),
                // 00000000 00000000 0000xxxx xx000000
                (unsigned char)(0x80 | ((codepoint >> 6) & 0x3f)),
                // 00000000 00000000 00000000 00xxxxxx
                (unsigned char)(0x80 | (codepoint & 0x3f)),
            };
            output_bytes->append((char*)bytes, 4);
        } else {
            panic("codepoint out of UTF8 range");
        }
    }
}
