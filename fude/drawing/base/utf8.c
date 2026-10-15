// Copyright (c) 2026 Borja Vazquez Cuesta. All rights reserved.

#include "drawing/base/utf8.h"

// ===========================================================================
// See utf8.h.
//
// The decoder takes a sequence only when it is well formed: a lead byte, then as many continuation bytes (10xxxxxx)
// as it says, holding a code point in Unicode's range written in its shortest form and not a surrogate. Anything else
// (a byte out of place, an overlong form such as C0 80, a surrogate's three bytes, a code point past U+10FFFF) is
// one malformed byte, read as U+FFFD, so two spellings of one character never compare unequal and no NUL hides in
// a longer form.
// ===========================================================================

// Decodes the code point at _s; *_len gets its length in bytes (0 at the end, 1 for a malformed byte, given U+FFFD).
u32 fude_utf8_decode(const c8* _s, u32* _len) {
    const u8* _u = (const u8*)_s;
    if(_u[0] == 0) {
        *_len = 0;
        return 0;
    }
    if(_u[0] < 0x80u) {
        *_len = 1;
        return _u[0];
    }
    // The bytes after a lead byte must be continuation bytes (10xxxxxx). A character cut short (its lead byte, then
    // plain text) is one malformed byte, so the letters after it are read as themselves, not swallowed into it. A
    // continuation byte is never 0, so the end of the string stops a sequence too.
    // A form longer than the code point needs, a surrogate (U+D800 to U+DFFF, which UTF-8 never holds) and a code
    // point past U+10FFFF are malformed too.
    #define FUDE_UTF8_MORE(_b) (((_b) & 0xC0u) == 0x80u)
    if((_u[0] & 0xE0u) == 0xC0u && FUDE_UTF8_MORE(_u[1])) {
        const u32 _cp = ((u32)(_u[0] & 0x1Fu) << 6) | (u32)(_u[1] & 0x3Fu);
        if(_cp >= 0x80u) {
            *_len = 2;
            return _cp;
        }
    } else if((_u[0] & 0xF0u) == 0xE0u && FUDE_UTF8_MORE(_u[1]) && FUDE_UTF8_MORE(_u[2])) {
        const u32 _cp = ((u32)(_u[0] & 0x0Fu) << 12) | ((u32)(_u[1] & 0x3Fu) << 6) | (u32)(_u[2] & 0x3Fu);
        if(_cp >= 0x800u && (_cp < 0xD800u || _cp > 0xDFFFu)) {
            *_len = 3;
            return _cp;
        }
    } else if((_u[0] & 0xF8u) == 0xF0u && FUDE_UTF8_MORE(_u[1]) && FUDE_UTF8_MORE(_u[2]) && FUDE_UTF8_MORE(_u[3])) {
        const u32 _cp = ((u32)(_u[0] & 0x07u) << 18) | ((u32)(_u[1] & 0x3Fu) << 12) | ((u32)(_u[2] & 0x3Fu) << 6) | (u32)(_u[3] & 0x3Fu);
        if(_cp >= 0x10000u && _cp <= 0x10FFFFu) {
            *_len = 4;
            return _cp;
        }
    }
    #undef FUDE_UTF8_MORE
    *_len = 1;   // A malformed byte: skip it on its own.
    return 0xFFFDu;
}

// Decodes the code point at *_s and moves *_s past it; returns 0, without moving, at the end.
u32 fude_utf8_next(const c8** _s) {
    u32       _len = 0;
    const u32 _cp  = fude_utf8_decode(*_s, &_len);
    *_s += _len;
    return _cp;
}

// Encodes _cp as 1 to 4 bytes of UTF-8 into _out, followed by a NUL. Returns the bytes written, without the NUL.
u32 fude_utf8_put(u32 _cp, c8* _out) {
    if(_cp < 0x80u) {
        _out[0] = (c8)_cp;
        _out[1] = 0;
        return 1u;
    }
    if(_cp < 0x800u) {
        _out[0] = (c8)(0xC0u | (_cp >> 6));
        _out[1] = (c8)(0x80u | (_cp & 0x3Fu));
        _out[2] = 0;
        return 2u;
    }
    if(_cp < 0x10000u) {
        _out[0] = (c8)(0xE0u | (_cp >> 12));
        _out[1] = (c8)(0x80u | ((_cp >> 6) & 0x3Fu));
        _out[2] = (c8)(0x80u | (_cp & 0x3Fu));
        _out[3] = 0;
        return 3u;
    }
    _out[0] = (c8)(0xF0u | (_cp >> 18));
    _out[1] = (c8)(0x80u | ((_cp >> 12) & 0x3Fu));
    _out[2] = (c8)(0x80u | ((_cp >> 6) & 0x3Fu));
    _out[3] = (c8)(0x80u | (_cp & 0x3Fu));
    _out[4] = 0;
    return 4u;
}

// Returns how many code points _s holds, counting every byte that is not a continuation byte.
u32 fude_utf8_count(const c8* _s) {
    u32 _n = 0;
    for(; *_s != 0; _s++) {
        _n += ((u8)*_s & 0xC0u) != 0x80u ? 1u : 0u;   // Count it unless it is a continuation byte (10xxxxxx).
    }
    return _n;
}

// Returns whether _cp is a Han character: the main CJK block (U+4E00 to U+9FFF), Extension A (U+3400 to U+4DBF), the
// compatibility forms (U+F900 to U+FAFF), or the supplementary planes' extensions (U+20000 to U+2FFFF).
b8 fude_utf8_is_han(u32 _cp) {
    return (_cp >= 0x4E00u && _cp <= 0x9FFFu) || (_cp >= 0x3400u && _cp <= 0x4DBFu) || (_cp >= 0xF900u && _cp <= 0xFAFFu) ||
           (_cp >= 0x20000u && _cp <= 0x2FFFFu);
}

// Each accented Latin letter's base letter (from Unicode's decompositions, in lower case), and the IPA vowels a
// romanization writes (ɛ ɔ ʉ ə and their capitals). The table must stay sorted by code point: it is binary-searched.
typedef struct { u16 codepoint; c8 base; } fude_utf8_latin;
static const fude_utf8_latin FUDE_UTF8_LATIN[] = {
    { 0x00C0, 'a' }, { 0x00C1, 'a' }, { 0x00C2, 'a' }, { 0x00C3, 'a' }, { 0x00C4, 'a' }, { 0x00C5, 'a' },
    { 0x00C7, 'c' }, { 0x00C8, 'e' }, { 0x00C9, 'e' }, { 0x00CA, 'e' }, { 0x00CB, 'e' }, { 0x00CC, 'i' },
    { 0x00CD, 'i' }, { 0x00CE, 'i' }, { 0x00CF, 'i' }, { 0x00D1, 'n' }, { 0x00D2, 'o' }, { 0x00D3, 'o' },
    { 0x00D4, 'o' }, { 0x00D5, 'o' }, { 0x00D6, 'o' }, { 0x00D9, 'u' }, { 0x00DA, 'u' }, { 0x00DB, 'u' },
    { 0x00DC, 'u' }, { 0x00DD, 'y' }, { 0x00E0, 'a' }, { 0x00E1, 'a' }, { 0x00E2, 'a' }, { 0x00E3, 'a' },
    { 0x00E4, 'a' }, { 0x00E5, 'a' }, { 0x00E7, 'c' }, { 0x00E8, 'e' }, { 0x00E9, 'e' }, { 0x00EA, 'e' },
    { 0x00EB, 'e' }, { 0x00EC, 'i' }, { 0x00ED, 'i' }, { 0x00EE, 'i' }, { 0x00EF, 'i' }, { 0x00F1, 'n' },
    { 0x00F2, 'o' }, { 0x00F3, 'o' }, { 0x00F4, 'o' }, { 0x00F5, 'o' }, { 0x00F6, 'o' }, { 0x00F9, 'u' },
    { 0x00FA, 'u' }, { 0x00FB, 'u' }, { 0x00FC, 'u' }, { 0x00FD, 'y' }, { 0x00FF, 'y' }, { 0x0100, 'a' },
    { 0x0101, 'a' }, { 0x0102, 'a' }, { 0x0103, 'a' }, { 0x0104, 'a' }, { 0x0105, 'a' }, { 0x0106, 'c' },
    { 0x0107, 'c' }, { 0x0108, 'c' }, { 0x0109, 'c' }, { 0x010A, 'c' }, { 0x010B, 'c' }, { 0x010C, 'c' },
    { 0x010D, 'c' }, { 0x010E, 'd' }, { 0x010F, 'd' }, { 0x0112, 'e' }, { 0x0113, 'e' }, { 0x0114, 'e' },
    { 0x0115, 'e' }, { 0x0116, 'e' }, { 0x0117, 'e' }, { 0x0118, 'e' }, { 0x0119, 'e' }, { 0x011A, 'e' },
    { 0x011B, 'e' }, { 0x011C, 'g' }, { 0x011D, 'g' }, { 0x011E, 'g' }, { 0x011F, 'g' }, { 0x0120, 'g' },
    { 0x0121, 'g' }, { 0x0122, 'g' }, { 0x0123, 'g' }, { 0x0124, 'h' }, { 0x0125, 'h' }, { 0x0128, 'i' },
    { 0x0129, 'i' }, { 0x012A, 'i' }, { 0x012B, 'i' }, { 0x012C, 'i' }, { 0x012D, 'i' }, { 0x012E, 'i' },
    { 0x012F, 'i' }, { 0x0130, 'i' }, { 0x0134, 'j' }, { 0x0135, 'j' }, { 0x0136, 'k' }, { 0x0137, 'k' },
    { 0x0139, 'l' }, { 0x013A, 'l' }, { 0x013B, 'l' }, { 0x013C, 'l' }, { 0x013D, 'l' }, { 0x013E, 'l' },
    { 0x0143, 'n' }, { 0x0144, 'n' }, { 0x0145, 'n' }, { 0x0146, 'n' }, { 0x0147, 'n' }, { 0x0148, 'n' },
    { 0x014C, 'o' }, { 0x014D, 'o' }, { 0x014E, 'o' }, { 0x014F, 'o' }, { 0x0150, 'o' }, { 0x0151, 'o' },
    { 0x0154, 'r' }, { 0x0155, 'r' }, { 0x0156, 'r' }, { 0x0157, 'r' }, { 0x0158, 'r' }, { 0x0159, 'r' },
    { 0x015A, 's' }, { 0x015B, 's' }, { 0x015C, 's' }, { 0x015D, 's' }, { 0x015E, 's' }, { 0x015F, 's' },
    { 0x0160, 's' }, { 0x0161, 's' }, { 0x0162, 't' }, { 0x0163, 't' }, { 0x0164, 't' }, { 0x0165, 't' },
    { 0x0168, 'u' }, { 0x0169, 'u' }, { 0x016A, 'u' }, { 0x016B, 'u' }, { 0x016C, 'u' }, { 0x016D, 'u' },
    { 0x016E, 'u' }, { 0x016F, 'u' }, { 0x0170, 'u' }, { 0x0171, 'u' }, { 0x0172, 'u' }, { 0x0173, 'u' },
    { 0x0174, 'w' }, { 0x0175, 'w' }, { 0x0176, 'y' }, { 0x0177, 'y' }, { 0x0178, 'y' }, { 0x0179, 'z' },
    { 0x017A, 'z' }, { 0x017B, 'z' }, { 0x017C, 'z' }, { 0x017D, 'z' }, { 0x017E, 'z' }, { 0x0186, 'o' },
    { 0x018F, 'e' }, { 0x0190, 'e' }, { 0x01A0, 'o' }, { 0x01A1, 'o' }, { 0x01AF, 'u' }, { 0x01B0, 'u' },
    { 0x01CD, 'a' }, { 0x01CE, 'a' }, { 0x01CF, 'i' }, { 0x01D0, 'i' }, { 0x01D1, 'o' }, { 0x01D2, 'o' },
    { 0x01D3, 'u' }, { 0x01D4, 'u' }, { 0x01D5, 'u' }, { 0x01D6, 'u' }, { 0x01D7, 'u' }, { 0x01D8, 'u' },
    { 0x01D9, 'u' }, { 0x01DA, 'u' }, { 0x01DB, 'u' }, { 0x01DC, 'u' }, { 0x01DE, 'a' }, { 0x01DF, 'a' },
    { 0x01E0, 'a' }, { 0x01E1, 'a' }, { 0x01E6, 'g' }, { 0x01E7, 'g' }, { 0x01E8, 'k' }, { 0x01E9, 'k' },
    { 0x01EA, 'o' }, { 0x01EB, 'o' }, { 0x01EC, 'o' }, { 0x01ED, 'o' }, { 0x01F0, 'j' }, { 0x01F4, 'g' },
    { 0x01F5, 'g' }, { 0x01F8, 'n' }, { 0x01F9, 'n' }, { 0x01FA, 'a' }, { 0x01FB, 'a' }, { 0x0200, 'a' },
    { 0x0201, 'a' }, { 0x0202, 'a' }, { 0x0203, 'a' }, { 0x0204, 'e' }, { 0x0205, 'e' }, { 0x0206, 'e' },
    { 0x0207, 'e' }, { 0x0208, 'i' }, { 0x0209, 'i' }, { 0x020A, 'i' }, { 0x020B, 'i' }, { 0x020C, 'o' },
    { 0x020D, 'o' }, { 0x020E, 'o' }, { 0x020F, 'o' }, { 0x0210, 'r' }, { 0x0211, 'r' }, { 0x0212, 'r' },
    { 0x0213, 'r' }, { 0x0214, 'u' }, { 0x0215, 'u' }, { 0x0216, 'u' }, { 0x0217, 'u' }, { 0x0218, 's' },
    { 0x0219, 's' }, { 0x021A, 't' }, { 0x021B, 't' }, { 0x021E, 'h' }, { 0x021F, 'h' }, { 0x0226, 'a' },
    { 0x0227, 'a' }, { 0x0228, 'e' }, { 0x0229, 'e' }, { 0x022A, 'o' }, { 0x022B, 'o' }, { 0x022C, 'o' },
    { 0x022D, 'o' }, { 0x022E, 'o' }, { 0x022F, 'o' }, { 0x0230, 'o' }, { 0x0231, 'o' }, { 0x0232, 'y' },
    { 0x0233, 'y' }, { 0x0244, 'u' }, { 0x0254, 'o' }, { 0x0259, 'e' }, { 0x025B, 'e' }, { 0x0289, 'u' },
    { 0x1E00, 'a' }, { 0x1E01, 'a' }, { 0x1E02, 'b' }, { 0x1E03, 'b' }, { 0x1E04, 'b' }, { 0x1E05, 'b' },
    { 0x1E06, 'b' }, { 0x1E07, 'b' }, { 0x1E08, 'c' }, { 0x1E09, 'c' }, { 0x1E0A, 'd' }, { 0x1E0B, 'd' },
    { 0x1E0C, 'd' }, { 0x1E0D, 'd' }, { 0x1E0E, 'd' }, { 0x1E0F, 'd' }, { 0x1E10, 'd' }, { 0x1E11, 'd' },
    { 0x1E12, 'd' }, { 0x1E13, 'd' }, { 0x1E14, 'e' }, { 0x1E15, 'e' }, { 0x1E16, 'e' }, { 0x1E17, 'e' },
    { 0x1E18, 'e' }, { 0x1E19, 'e' }, { 0x1E1A, 'e' }, { 0x1E1B, 'e' }, { 0x1E1C, 'e' }, { 0x1E1D, 'e' },
    { 0x1E1E, 'f' }, { 0x1E1F, 'f' }, { 0x1E20, 'g' }, { 0x1E21, 'g' }, { 0x1E22, 'h' }, { 0x1E23, 'h' },
    { 0x1E24, 'h' }, { 0x1E25, 'h' }, { 0x1E26, 'h' }, { 0x1E27, 'h' }, { 0x1E28, 'h' }, { 0x1E29, 'h' },
    { 0x1E2A, 'h' }, { 0x1E2B, 'h' }, { 0x1E2C, 'i' }, { 0x1E2D, 'i' }, { 0x1E2E, 'i' }, { 0x1E2F, 'i' },
    { 0x1E30, 'k' }, { 0x1E31, 'k' }, { 0x1E32, 'k' }, { 0x1E33, 'k' }, { 0x1E34, 'k' }, { 0x1E35, 'k' },
    { 0x1E36, 'l' }, { 0x1E37, 'l' }, { 0x1E38, 'l' }, { 0x1E39, 'l' }, { 0x1E3A, 'l' }, { 0x1E3B, 'l' },
    { 0x1E3C, 'l' }, { 0x1E3D, 'l' }, { 0x1E3E, 'm' }, { 0x1E3F, 'm' }, { 0x1E40, 'm' }, { 0x1E41, 'm' },
    { 0x1E42, 'm' }, { 0x1E43, 'm' }, { 0x1E44, 'n' }, { 0x1E45, 'n' }, { 0x1E46, 'n' }, { 0x1E47, 'n' },
    { 0x1E48, 'n' }, { 0x1E49, 'n' }, { 0x1E4A, 'n' }, { 0x1E4B, 'n' }, { 0x1E4C, 'o' }, { 0x1E4D, 'o' },
    { 0x1E4E, 'o' }, { 0x1E4F, 'o' }, { 0x1E50, 'o' }, { 0x1E51, 'o' }, { 0x1E52, 'o' }, { 0x1E53, 'o' },
    { 0x1E54, 'p' }, { 0x1E55, 'p' }, { 0x1E56, 'p' }, { 0x1E57, 'p' }, { 0x1E58, 'r' }, { 0x1E59, 'r' },
    { 0x1E5A, 'r' }, { 0x1E5B, 'r' }, { 0x1E5C, 'r' }, { 0x1E5D, 'r' }, { 0x1E5E, 'r' }, { 0x1E5F, 'r' },
    { 0x1E60, 's' }, { 0x1E61, 's' }, { 0x1E62, 's' }, { 0x1E63, 's' }, { 0x1E64, 's' }, { 0x1E65, 's' },
    { 0x1E66, 's' }, { 0x1E67, 's' }, { 0x1E68, 's' }, { 0x1E69, 's' }, { 0x1E6A, 't' }, { 0x1E6B, 't' },
    { 0x1E6C, 't' }, { 0x1E6D, 't' }, { 0x1E6E, 't' }, { 0x1E6F, 't' }, { 0x1E70, 't' }, { 0x1E71, 't' },
    { 0x1E72, 'u' }, { 0x1E73, 'u' }, { 0x1E74, 'u' }, { 0x1E75, 'u' }, { 0x1E76, 'u' }, { 0x1E77, 'u' },
    { 0x1E78, 'u' }, { 0x1E79, 'u' }, { 0x1E7A, 'u' }, { 0x1E7B, 'u' }, { 0x1E7C, 'v' }, { 0x1E7D, 'v' },
    { 0x1E7E, 'v' }, { 0x1E7F, 'v' }, { 0x1E80, 'w' }, { 0x1E81, 'w' }, { 0x1E82, 'w' }, { 0x1E83, 'w' },
    { 0x1E84, 'w' }, { 0x1E85, 'w' }, { 0x1E86, 'w' }, { 0x1E87, 'w' }, { 0x1E88, 'w' }, { 0x1E89, 'w' },
    { 0x1E8A, 'x' }, { 0x1E8B, 'x' }, { 0x1E8C, 'x' }, { 0x1E8D, 'x' }, { 0x1E8E, 'y' }, { 0x1E8F, 'y' },
    { 0x1E90, 'z' }, { 0x1E91, 'z' }, { 0x1E92, 'z' }, { 0x1E93, 'z' }, { 0x1E94, 'z' }, { 0x1E95, 'z' },
    { 0x1E96, 'h' }, { 0x1E97, 't' }, { 0x1E98, 'w' }, { 0x1E99, 'y' }, { 0x1EA0, 'a' }, { 0x1EA1, 'a' },
    { 0x1EA2, 'a' }, { 0x1EA3, 'a' }, { 0x1EA4, 'a' }, { 0x1EA5, 'a' }, { 0x1EA6, 'a' }, { 0x1EA7, 'a' },
    { 0x1EA8, 'a' }, { 0x1EA9, 'a' }, { 0x1EAA, 'a' }, { 0x1EAB, 'a' }, { 0x1EAC, 'a' }, { 0x1EAD, 'a' },
    { 0x1EAE, 'a' }, { 0x1EAF, 'a' }, { 0x1EB0, 'a' }, { 0x1EB1, 'a' }, { 0x1EB2, 'a' }, { 0x1EB3, 'a' },
    { 0x1EB4, 'a' }, { 0x1EB5, 'a' }, { 0x1EB6, 'a' }, { 0x1EB7, 'a' }, { 0x1EB8, 'e' }, { 0x1EB9, 'e' },
    { 0x1EBA, 'e' }, { 0x1EBB, 'e' }, { 0x1EBC, 'e' }, { 0x1EBD, 'e' }, { 0x1EBE, 'e' }, { 0x1EBF, 'e' },
    { 0x1EC0, 'e' }, { 0x1EC1, 'e' }, { 0x1EC2, 'e' }, { 0x1EC3, 'e' }, { 0x1EC4, 'e' }, { 0x1EC5, 'e' },
    { 0x1EC6, 'e' }, { 0x1EC7, 'e' }, { 0x1EC8, 'i' }, { 0x1EC9, 'i' }, { 0x1ECA, 'i' }, { 0x1ECB, 'i' },
    { 0x1ECC, 'o' }, { 0x1ECD, 'o' }, { 0x1ECE, 'o' }, { 0x1ECF, 'o' }, { 0x1ED0, 'o' }, { 0x1ED1, 'o' },
    { 0x1ED2, 'o' }, { 0x1ED3, 'o' }, { 0x1ED4, 'o' }, { 0x1ED5, 'o' }, { 0x1ED6, 'o' }, { 0x1ED7, 'o' },
    { 0x1ED8, 'o' }, { 0x1ED9, 'o' }, { 0x1EDA, 'o' }, { 0x1EDB, 'o' }, { 0x1EDC, 'o' }, { 0x1EDD, 'o' },
    { 0x1EDE, 'o' }, { 0x1EDF, 'o' }, { 0x1EE0, 'o' }, { 0x1EE1, 'o' }, { 0x1EE2, 'o' }, { 0x1EE3, 'o' },
    { 0x1EE4, 'u' }, { 0x1EE5, 'u' }, { 0x1EE6, 'u' }, { 0x1EE7, 'u' }, { 0x1EE8, 'u' }, { 0x1EE9, 'u' },
    { 0x1EEA, 'u' }, { 0x1EEB, 'u' }, { 0x1EEC, 'u' }, { 0x1EED, 'u' }, { 0x1EEE, 'u' }, { 0x1EEF, 'u' },
    { 0x1EF0, 'u' }, { 0x1EF1, 'u' }, { 0x1EF2, 'y' }, { 0x1EF3, 'y' }, { 0x1EF4, 'y' }, { 0x1EF5, 'y' },
    { 0x1EF6, 'y' }, { 0x1EF7, 'y' }, { 0x1EF8, 'y' }, { 0x1EF9, 'y' },
};

// Returns a letter as a romanization search compares it: ASCII capitals in lower case, an accented letter as its base
// (looked up in FUDE_UTF8_LATIN), 0 for a combining accent or a modifier letter, anything else as it is.
u32 fude_utf8_latin_base(u32 _cp) {
    if(_cp >= 'A' && _cp <= 'Z') {
        return _cp + ('a' - 'A');
    }
    if((_cp >= 0x0300u && _cp <= 0x036Fu) || (_cp >= 0x02B9u && _cp <= 0x02C1u)) {
        return 0u;   // A combining accent, or a modifier letter (U+02B9 to U+02C1: ʹ ʻ ʼ ʾ ʿ ˀ ˁ and their kin).
    }
    u32 _lo = 0, _hi = (u32)(sizeof(FUDE_UTF8_LATIN) / sizeof(FUDE_UTF8_LATIN[0]));
    while(_lo < _hi) {
        const u32 _mid = (_lo + _hi) / 2u;
        if(FUDE_UTF8_LATIN[_mid].codepoint == _cp) {
            return (u32)(u8)FUDE_UTF8_LATIN[_mid].base;
        }
        if(FUDE_UTF8_LATIN[_mid].codepoint < _cp) { _lo = _mid + 1u; } else { _hi = _mid; }
    }
    return _cp;
}

// Returns a character as a search compares it (see the header): a full-width form or the ideographic space as its
// ASCII, then through fude_utf8_latin_base.
u32 fude_utf8_search_fold(u32 _cp) {
    if(_cp >= 0xFF01u && _cp <= 0xFF5Eu) {
        _cp -= 0xFF01u - 0x21u;   // The full-width forms of ! to ~ run in the same order as ASCII's.
    } else if(_cp == 0x3000u) {
        _cp = ' ';
    }
    return fude_utf8_latin_base(_cp);
}

// Reads the next character of *_s that a search compares, folded, moving *_s past it and past the characters folded
// away before it. Returns 0 at the end.
RDE_INTERNAL u32 fude_utf8_next_folded(const c8** _s) {
    for(;;) {
        const u32 _cp = fude_utf8_next(_s);
        if(_cp == 0u) {
            return 0u;
        }
        const u32 _folded = fude_utf8_search_fold(_cp);
        if(_folded != 0u) {
            return _folded;
        }
    }
}

// Looks for _needle in _hay, a character at a time, comparing them folded (see the header). A match starts on a
// character that is compared, never on an accent left out.
i32 fude_utf8_find(const c8* _hay, const c8* _needle, usize _from, usize* _end) {
    const c8* _first = _needle;
    if(fude_utf8_next_folded(&_first) == 0u) {
        return -1;   // Nothing to compare: an empty needle, or one of accents alone.
    }
    for(usize _i = _from; _hay[_i] != 0;) {
        u32       _len  = 0;
        const u32 _here = fude_utf8_search_fold(fude_utf8_decode(&_hay[_i], &_len));
        const c8* _h    = &_hay[_i];
        const c8* _n    = _needle;
        u32       _want = fude_utf8_next_folded(&_n);
        b8        _same = _here != 0u;
        while(_same && _want != 0u) {
            _same = fude_utf8_next_folded(&_h) == _want;
            _want = fude_utf8_next_folded(&_n);
        }
        if(_same) {
            // The accents after its last letter belong to that letter, so they are part of the match.
            for(const c8* _after = _h; *_after != 0 && fude_utf8_search_fold(fude_utf8_next(&_after)) == 0u;) {
                _h = _after;
            }
            *_end = (usize)(_h - _hay);
            return (i32)_i;
        }
        _i += _len;
    }
    return -1;
}

// Copies at most _len bytes of _s into _out, NUL-terminated, cutting before a whole character when they do not fit.
void fude_utf8_copy(c8* _out, usize _size, const c8* _s, usize _len) {
    if(_size == 0) {
        return;   // There is no room even for the NUL, so nothing is written.
    }
    usize _n = _len < _size - 1u ? _len : _size - 1u;
    while(_n > 0 && _n < _len && ((u8)_s[_n] & 0xC0u) == 0x80u) {
        _n--;   // The byte after the cut continues a character, so move the cut back to that character's start.
    }
    if(_n > 0) {
        rde_memcpy(_out, _s, _n);
    }
    _out[_n] = 0;
}

// Copies the whole of _s into _out as fude_utf8_copy does; NULL copies as "".
void fude_utf8_set(c8* _out, usize _size, const c8* _s) {
    fude_utf8_copy(_out, _size, _s != NULL ? _s : "", _s != NULL ? rde_strlen(_s) : 0u);
}
