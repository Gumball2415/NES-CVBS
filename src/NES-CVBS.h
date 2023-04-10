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

#include "PPUVoltages.h"

#define ACTIVE_WIDTH 256
#define ACTIVE_HEIGHT 240

// NES scanline timings, described in PPU pixel/dot/cycle durations
// https://www.nesdev.org/wiki/NTSC_video#Scanline_Timing
// NES frame timings, in scanline lengths
// https://www.nesdev.org/wiki/File:Ppu.svg
struct PPUScanline {
	// common throughout most of the scanlines
	uint16_t short_sync = 25;
	uint16_t first_back_porch = 4;
	uint16_t colorburst = 15;
	uint16_t second_back_porch = 5;
	uint16_t front_porch = 9;
	
	// visible scanlines (0-239)
	uint16_t active_scanlines = ACTIVE_HEIGHT;
	uint16_t gray_pulse = 1;
	uint16_t border_left = 15;
	uint16_t active = ACTIVE_WIDTH;
	uint16_t border_right = 11
	
	// postrender scanlines (240-241)
	uint16_t postrender_scanlines = 2;
	uint16_t border_bottom = 282;
	
	// postrender blanking scanlines (242-244)
	uint16_t postrender_blank_scanlines = 3;
	uint16_t blank = 297;		// blank from colorburst until hsync
	
	// vertical sync scanlines (245-247)
	uint16_t vertical_sync_scanlines = 3;
	uint16_t long_sync = 318;
	uint16_t sync_seperator = 23;
	
	// prerender blanking scanlines (248-261)
	uint16_t prerender_blanking_scanlines = 4;
	// same as postrender blanking, but with a skipped dot on colorburst
	bool dot_skip = true;		// only on odd frames
};

enum PPUDotType {
	// first 512 entries are exclusively for the 9-bit PPU pixel format: "eeellcccc".
	sync_level = 512,
	blank_level,
	colorburst
};
