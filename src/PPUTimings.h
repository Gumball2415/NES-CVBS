/*
NES-CVBS
Copyright (c) 2023 Persune

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
*/

#include <cstdint>

// NES scanline timings, described in PPU pixel/dot/cycle durations
// NES frame timings, in scanline lengths
// https://www.nesdev.org/wiki/File:Ppu.svg
// https://www.nesdev.org/wiki/NTSC_video#Scanline_Timing
struct PPUTimings {
    uint16_t field_width;
    uint16_t field_height;
    uint16_t visible_width;
    uint16_t visible_height;

    // common throughout most of the scanlines
    uint16_t horizontal_sync;
    uint16_t back_porch_first;
    uint16_t colorburst;
    uint16_t back_porch_second;
    uint16_t front_porch;

    // visible scanlines (0-239)
    uint16_t active_scanlines;
    uint16_t gray_pulse;
    uint16_t border_left;
    uint16_t active_pixels;
    uint16_t border_right;

    // postrender scanlines (240-241)
    uint16_t postrender_scanlines;
    uint16_t border_bottom;

    // postrender blanking scanlines (242-244)
    uint16_t postrender_blank_scanlines;
    uint16_t vblank;

    // vertical sync scanlines (245-247)
    uint16_t vertical_sync_scanlines;
    uint16_t blank_pulse;
    uint16_t sync_seperator;

    // prerender blanking scanlines (248-261)
    uint16_t prerender_blanking_scanlines;
    // a dot is skipped on odd frames at (340, 261) supposedly to limit the dot crawl patterns
    bool dot_skip;

    uint16_t samples_per_pixel;
    uint8_t colorburst_phase;
};

const PPUTimings PPU2C02Timings = {
    341, 262,
    283, 242,
    // hsync frontporch, backporch, and colorburst
    25, 4, 15, 5, 9,

    // visible scanlines (0-239)
    240, 1, 15, 256, 11,

    // postrender scanlines (240-241)
    2, 282,

    // postrender blanking scanlines (242-244)
    3, 283,

    // vertical sync scanlines (245-247)
    3, 318, 14,

    // prerender blanking scanlines (248-261)
    14,

    true,
    uint16_t((21477272 * 2) / (21477272 / 4)),  // color generator clock / PPU clock
    0x08
};

const PPUTimings PPU2C07Timings = {
    341, 312,
    256, 240,
    // hsync frontporch, backporch, and colorburst
    25, 4, 15, 5, 9,

    // visible scanlines (0-239)
    240, 0, 18, 256, 9,

    // postrender scanlines (not present)
    0, 283,

    // postrender blanking scanlines (240-269)
    30, 283,

    // vertical sync scanlines (245-247)
    3, 320, 12,

    // prerender blanking scanlines (248-261)
    39,

    false,
    uint16_t((26601712 * 2) / (26601712 / 5)),    // color generator clock / PPU clock
    0x07
};

// Dendy timings seem to have the same timings as the 2C07
const PPUTimings PPUUA6538Timings = PPU2C07Timings;
