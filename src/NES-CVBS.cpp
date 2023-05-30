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

#include "NES-CVBS.h"
#include <iostream>

void NES_CVBS::FilterFrame(uint16_t* ppu_buffer, uint32_t* rgb_buffer, int ppu_phase)
{
    PPURawFrameBuffer = ppu_buffer;
    bool odd_field = false;
    // TODO: multithreading
    {
        //EmplaceField();
        //EncodeField(ppu_phase, 0, 240, odd_field);
        //DecodeField(rgb_buffer, ppu_phase, 0, 240);
    }
}

NES_CVBS::NES_CVBS(FilterSettings filter_settings)
{
    Settings = filter_settings;

    CompositeOutputLevel ppu_voltages = {};
    switch (Settings.ppu_type) {
    case 0:
        ppu_voltages = NES_2C02;
        PPURasterTimings = PPU2C02Timings;
        break;
    case 1:
        ppu_voltages = NES_2C07;
        PPURasterTimings = PPU2C07Timings;
        break;
    case 2:
        ppu_voltages = NES_UA6538;
        PPURasterTimings = PPUUA6538Timings;
        break;
    }

    FieldBufferWidth = Settings.sync_enable ? PPURasterTimings.field_width : PPURasterTimings.visible_width;
    FieldBufferHeight = Settings.sync_enable ? PPURasterTimings.field_height : PPURasterTimings.visible_height;

    RawFieldBuffer.resize(size_t(FieldBufferWidth * FieldBufferHeight));
    SignalFieldBuffer.resize(size_t(FieldBufferWidth * FieldBufferHeight * PPURasterTimings.samples_per_pixel));

    InitializeField();

    for (int emph = 0; emph < 2; emph++) {
        for (int color = 0; color < 0x40; color++) {
            int hue = color & 0x0F;
            int luma = color & 0xF0;
            double high = ppu_voltages.signal[luma][0][emph];
            double low = ppu_voltages.signal[luma][1][emph];
            double blank = ppu_voltages.signal[1][1][0];

            if (hue == 0x0D) high = low;
            else if (hue == 0x00) low = high;
            else if (hue >= 0x0E) high = low = blank;

            ColorLevelLUT[0][emph][color] = uint16_t(high * 1000);
            ColorLevelLUT[1][emph][color] = uint16_t(low * 1000);
        }

        SyncLevelLUT[emph] = uint16_t(ppu_voltages.sync[emph] * 1000);
        ColorburstLevelLUT[emph] = uint16_t(ppu_voltages.colorburst[emph] * 1000);
    }

    PPU2C04LUT = PaletteLUT_2C04[Settings.ppu_2C04_rev];
}

NES_CVBS::~NES_CVBS()
{
}

void NES_CVBS::InitializeField()
{
    auto write_pixels_in = [&](uint16_t length, std::vector<PPUDotType>& raw_field_buffer, uint16_t& pixel_index, uint16_t& scanline_index, uint16_t& pixel_threshold, PPUDotType pixel) {
        pixel_threshold += length;
        while (pixel_index < pixel_threshold) {
            raw_field_buffer[size_t((scanline_index * FieldBufferWidth) + pixel_index)] = pixel;
            pixel_index++;
        }
    };

    auto scanline_is_in = [&](uint16_t length, uint16_t& scanline, uint16_t& scanline_threshold, bool reset = false) {
        scanline_threshold += length;
        bool result = scanline < scanline_threshold;
        if (reset) scanline_threshold = 0;
        return result;
    };

    for (uint16_t scanline = 0; scanline < FieldBufferHeight; scanline++) {
        uint16_t pixel = 0;
        uint16_t pixel_threshold = 0;
        uint16_t scanline_threshold = 0;

        // horizontal sync, back porch, and colorburst
        if (Settings.sync_enable) {
            write_pixels_in(PPURasterTimings.horizontal_sync, RawFieldBuffer, pixel, scanline, pixel_threshold, sync_level);
            write_pixels_in(PPURasterTimings.back_porch_first, RawFieldBuffer, pixel, scanline, pixel_threshold, blank_level);
            write_pixels_in(PPURasterTimings.colorburst, RawFieldBuffer, pixel, scanline, pixel_threshold, colorburst);
            write_pixels_in(PPURasterTimings.back_porch_second, RawFieldBuffer, pixel, scanline, pixel_threshold, blank_level);
        }

        if (scanline_is_in(PPURasterTimings.active_scanlines, scanline, scanline_threshold)) {
            write_pixels_in(PPURasterTimings.gray_pulse, RawFieldBuffer, pixel, scanline, pixel_threshold, PPUDotType((0x25) & 0x30));
            write_pixels_in(PPURasterTimings.border_left, RawFieldBuffer, pixel, scanline, pixel_threshold, PPUDotType(0x10));
            write_pixels_in(PPURasterTimings.active_pixels, RawFieldBuffer, pixel, scanline, pixel_threshold, PPUDotType(0x25));
            write_pixels_in(PPURasterTimings.border_right, RawFieldBuffer, pixel, scanline, pixel_threshold, PPUDotType(0x10));
        }
        else if (scanline_is_in(PPURasterTimings.postrender_scanlines, scanline, scanline_threshold)) {
            write_pixels_in(PPURasterTimings.gray_pulse, RawFieldBuffer, pixel, scanline, pixel_threshold, PPUDotType((0x25) & 0x30));
            write_pixels_in(PPURasterTimings.border_bottom, RawFieldBuffer, pixel, scanline, pixel_threshold, PPUDotType(0x10));
        }
        else if (Settings.sync_enable && scanline_is_in(PPURasterTimings.postrender_blank_scanlines, scanline, scanline_threshold))
            write_pixels_in(PPURasterTimings.vblank, RawFieldBuffer, pixel, scanline, pixel_threshold, blank_level);
        else if (Settings.sync_enable && scanline_is_in(PPURasterTimings.vertical_sync_scanlines, scanline, scanline_threshold)) {
            // it's rewind time! overwrite hsync, back porch and colorburst with vblank pulse
            pixel = 0;
            pixel_threshold = 0;
            write_pixels_in(PPURasterTimings.blank_pulse, RawFieldBuffer, pixel, scanline, pixel_threshold, sync_level);
            write_pixels_in(PPURasterTimings.sync_seperator, RawFieldBuffer, pixel, scanline, pixel_threshold, blank_level);
        }
        else if (Settings.sync_enable && scanline_is_in(PPURasterTimings.prerender_blanking_scanlines, scanline, scanline_threshold)) {
            write_pixels_in(PPURasterTimings.vblank, RawFieldBuffer, pixel, scanline, pixel_threshold, blank_level);
        }

        // front porch is the only constant in life
        if (Settings.sync_enable)
            write_pixels_in(PPURasterTimings.front_porch, RawFieldBuffer, pixel, scanline, pixel_threshold, blank_level);
    }
}

void NES_CVBS::EmplaceField()
{
    auto write_pixels_in = [&](uint16_t length, std::vector<PPUDotType>& raw_field_buffer, uint16_t& pixel_index, uint16_t& scanline_index, uint16_t& pixel_threshold, PPUDotType pixel) {
        pixel_threshold += length;
        while (pixel_index < pixel_threshold) {
            raw_field_buffer[size_t((scanline_index * FieldBufferWidth) + pixel_index)] = pixel;
            pixel_index++;
        }
    };

    auto scanline_is_in = [&](uint16_t length, uint16_t& scanline, uint16_t& scanline_threshold) {
        scanline_threshold += length;
        return scanline < scanline_threshold;
    };

    for (uint16_t scanline = 0; scanline < FieldBufferHeight; scanline++) {
        uint16_t pixel = 0;
        uint16_t pixel_threshold = 0;
        uint16_t scanline_threshold = 0;

        if (Settings.sync_enable &&
            (!scanline_is_in(PPURasterTimings.vertical_sync_scanlines, scanline, scanline_threshold) ||
                scanline_is_in(PPURasterTimings.prerender_blanking_scanlines, scanline, scanline_threshold))) {
            scanline_threshold = 0;
            write_pixels_in(PPURasterTimings.horizontal_sync, RawFieldBuffer, pixel, scanline, pixel_threshold, sync_level);
            write_pixels_in(PPURasterTimings.back_porch_first, RawFieldBuffer, pixel, scanline, pixel_threshold, blank_level);
            write_pixels_in(PPURasterTimings.colorburst, RawFieldBuffer, pixel, scanline, pixel_threshold, colorburst);
            write_pixels_in(PPURasterTimings.back_porch_second, RawFieldBuffer, pixel, scanline, pixel_threshold, blank_level);
        }
        else scanline_threshold = 0;

        if (scanline_is_in(PPURasterTimings.active_scanlines, scanline, scanline_threshold)) {
            // TODO: proper gray pulse emulation
            write_pixels_in(PPURasterTimings.gray_pulse, RawFieldBuffer, pixel, scanline, pixel_threshold, (blank_level));
            // TODO: border color emulation
            // Skip one dot in odd fields
            write_pixels_in(PPURasterTimings.border_left, RawFieldBuffer, pixel, scanline, pixel_threshold, blank_level);
            write_pixels_in(PPURasterTimings.active_pixels, RawFieldBuffer, pixel, scanline, pixel_threshold, PPUDotType(*PPURawFrameBuffer++));
            write_pixels_in(PPURasterTimings.border_right, RawFieldBuffer, pixel, scanline, pixel_threshold, blank_level);
        }
        else if (scanline_is_in(PPURasterTimings.postrender_scanlines, scanline, scanline_threshold)) {
            write_pixels_in(PPURasterTimings.gray_pulse, RawFieldBuffer, pixel, scanline, pixel_threshold, (blank_level));
            write_pixels_in(PPURasterTimings.border_bottom, RawFieldBuffer, pixel, scanline, pixel_threshold, blank_level);
        }
        else if (Settings.sync_enable && scanline_is_in(PPURasterTimings.postrender_blank_scanlines, scanline, scanline_threshold))
            write_pixels_in(PPURasterTimings.vblank, RawFieldBuffer, pixel, scanline, pixel_threshold, blank_level);
        else if (Settings.sync_enable && scanline_is_in(PPURasterTimings.vertical_sync_scanlines, scanline, scanline_threshold)) {
            write_pixels_in(PPURasterTimings.vblank, RawFieldBuffer, pixel, scanline, pixel_threshold, sync_level);
            write_pixels_in(PPURasterTimings.sync_seperator, RawFieldBuffer, pixel, scanline, pixel_threshold, blank_level);
        }

        // front porch is the only constant in life
        if (Settings.sync_enable)
            write_pixels_in(PPURasterTimings.front_porch, RawFieldBuffer, pixel, scanline, pixel_threshold, blank_level);
    }
}

void NES_CVBS::EncodeField(int ppu_phase, int line_start, int line_end, bool odd_field)
{
    //for (PPUDotType i : RawFieldBuffer)
    //    std::cout << i << ' ';
}

void NES_CVBS::EncodeFullField(int ppu_phase, int line_start, int line_end)
{
}

void NES_CVBS::DecodeField(uint32_t* rgb_buffer, int ppu_phase, int line_start, int line_end)
{
    if (Settings.sync_enable) DecodeFullField(ppu_phase);
}

void NES_CVBS::DecodeFullField(int& ppu_phase)
{
}
