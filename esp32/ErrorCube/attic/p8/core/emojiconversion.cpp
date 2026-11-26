//
//  ZEPTO-8 — Fantasy console emulator
//
//  Copyright © 2016—2020 Sam Hocevar <sam@hocevar.net>
//
//  This program is free software. It comes without any warranty, to
//  the extent permitted by applicable law. You can redistribute it
//  and/or modify it under the terms of the Do What the Fuck You Want
//  to Public License, Version 2, as published by the WTFPL Task Force.
//  See http://www.wtfpl.net/ for more details.
//
// ERROR-CUBE: the same conversions without <regex>, <codecvt> and <map> (they cost the ESP32 build ~100 KB).

#include <cstdint>
#include <cstring>
#include <string>
#include <string_view>

#include "emojiconversion.h"

// where each of the 256 glyphs' UTF-8 starts in utf8_chars, and how long it is (small: internal RAM is precious)
static const char* chars = nullptr;
static uint16_t g_off[256];
static uint8_t g_len[256];
static bool ready = false;
static uint8_t multibyte_start[32];       // a bit for each lead byte
static inline std::string_view glyph(int i) { return std::string_view(chars + g_off[i], g_len[i]); }
static inline bool isStart(uint8_t c) { return multibyte_start[c >> 3] & (1 << (c & 7)); }

static void init()
{
    if (ready) return;
    ready = true;
    // The complete PICO-8 charmap, from 0 to 255. We cannot just store
    // codepoints because some emoji glyphs are combinations of several
    // codepoints, e.g. ⬇️ is U+2B07 (down arrow) + U+FE0F (variation
    // selector-16).
    static char const utf8_chars[] =
        "\0¹²³⁴⁵⁶⁷⁸\t\nᵇᶜ\rᵉᶠ▮■□⁙⁘‖◀▶「」¥•、。゛゜"
        " !\"#$%&'()*+,-./0123456789:;<=>?@ABCDEFGHIJKLMNO"
        "PQRSTUVWXYZ[\\]^_`abcdefghijklmnopqrstuvwxyz{|}~○"
        "█▒🐱⬇️░✽●♥☉웃⌂⬅️😐♪🅾️◆…➡️★⧗⬆️ˇ∧❎▤▥あいうえおか"
        "きくけこさしすせそたちつてとなにぬねのはひふへほまみむめもやゆよ"
        "らりるれろわをんっゃゅょアイウエオカキクケコサシスセソタチツテト"
        "ナニヌネノハヒフヘホマミムメモヤユヨラリルレロワヲンッャュョ◜◝";
    char const *p8 = utf8_chars;
    chars = utf8_chars;
    for (int i = 0; i < 256; ++i)
    {
        uint8_t c = (uint8_t)*p8;
        size_t len8 = c < 0x80 ? 1 : c < 0xe0 ? 2 : c < 0xf0 ? 3 : 4;
        // an emoji: the glyph and U+FE0F (EF B8 8F)
        if ((uint8_t)p8[len8] == 0xef && (uint8_t)p8[len8 + 1] == 0xb8 && (uint8_t)p8[len8 + 2] == 0x8f) len8 += 3;
        g_off[i] = (uint16_t)(p8 - utf8_chars); g_len[i] = (uint8_t)len8;
        if (len8 > 1) multibyte_start[c >> 3] |= 1 << (c & 7);
        p8 += len8;
    }
}

std::string charset::utf8_to_pico8(std::string const &str)
{
    init();
    std::string ret;
    ret.reserve(str.size());
    size_t n = str.size();
    for (size_t p = 0; p < n; )
    {
        uint8_t c = (uint8_t)str[p];
        if (isStart(c))
        {
            // the longest glyph that starts here
            int best = -1; size_t bestLen = 1;
            for (int i = 0; i < 256; ++i)
            {
                std::string_view g = glyph(i);
                if (g.size() > bestLen && (uint8_t)g[0] == c && p + g.size() <= n && !memcmp(str.data() + p, g.data(), g.size()))
                    { best = i; bestLen = g.size(); }
            }
            if (best >= 0) { ret += (char)best; p += bestLen; continue; }
        }
        ret += str[p++];
    }
    return ret;
}

std::string charset::pico8_to_utf8(std::string const &str)
{
    init();
    std::string ret;
    for (uint8_t ch : str)
        ret += std::string(glyph(ch));
    return ret;
}

// Map uppercase letters to PICO-8 glyphs: A..Z are the glyphs 128..153 (█ ▒ 🐱 ⬇️ ...)
std::string charset::upper_to_emoji(std::string str)
{
    init();
    std::string newstring;
    for (char c : str)
    {
        if (c >= 'A' && c <= 'Z') newstring += std::string(glyph(128 + (c - 'A')));
        else newstring += c;
    }
    return charset::utf8_to_pico8(newstring);
}
