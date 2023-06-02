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
#include <thread>
#include <iostream>

void NES_CVBS::FilterFrame(uint16_t* ppu_buffer, uint32_t* rgb_buffer, int dot_phase, bool skip_dot)
{
    PPURawFrameBuffer = ppu_buffer;
    // place the input frame inside the raw field buffer
    EmplaceField();

    if (PPUThreadCount > 1) {
        int dot_phase_buffer = dot_phase;
        unsigned int scanline_index = 0;

        // split the work into n number of threads
        unsigned int field_chunk_size = (FieldBufferHeight / PPUThreadCount),
            field_chunk_size_remainder = 0;
        // if the thread count doesn't divide the field evenly, relegate the nth thread to the remaining area
        if (FieldBufferHeight % PPUThreadCount) {
            field_chunk_size_remainder = FieldBufferHeight - (field_chunk_size * (PPUThreadCount - 1));
        }

        std::vector<std::thread> thread_vector;
        for (int thread_number = 0; thread_number < PPUThreadCount; thread_number++) {
            if (field_chunk_size_remainder && (thread_number == (PPUThreadCount - 1))){
                thread_vector.emplace_back([&, scanline_index]() {
                    EncodeField(dot_phase_buffer, scanline_index, scanline_index + field_chunk_size_remainder, skip_dot);
                    DecodeField(rgb_buffer, dot_phase_buffer, scanline_index, scanline_index + field_chunk_size_remainder, skip_dot);
                    });
                scanline_index += field_chunk_size_remainder;
            }
            else {
                thread_vector.emplace_back([&, scanline_index]() {
                    EncodeField(dot_phase_buffer, scanline_index, scanline_index + field_chunk_size, skip_dot);
                    DecodeField(rgb_buffer, dot_phase_buffer, scanline_index, scanline_index + field_chunk_size, skip_dot);
                    });
                scanline_index += field_chunk_size;
            }
        }
        for (auto& thread : thread_vector)
            thread.join();
    }
    else {
        EncodeField(dot_phase, 0, FieldBufferHeight, skip_dot);
        DecodeField(rgb_buffer, dot_phase, 0, FieldBufferHeight, skip_dot);
    }
}

void NES_CVBS::ApplySettings(double brightness_delta, double contrast_delta, double hue_delta, double saturation_delta)
{
    BrightnessDelta = brightness_delta;
    ContrastDelta = contrast_delta;
    HueDelta = hue_delta;
    SaturationDelta = saturation_delta;

    CompositeOutputLevel ppu_voltages;
    switch (PPUType) {
    case 1:
        ppu_voltages = NES_2C07;
        PPURasterTimings = PPU2C07Timings;
        break;
    case 2:
        ppu_voltages = NES_UA6538;
        PPURasterTimings = PPUUA6538Timings;
        break;
    default:
        ppu_voltages = NES_2C02;
        PPURasterTimings = PPU2C02Timings;
        break;
    }

    FieldBufferWidth = PPUSyncEnable ? PPURasterTimings.field_width : PPURasterTimings.visible_width;
    FieldBufferHeight = PPUSyncEnable ? PPURasterTimings.field_height : PPURasterTimings.visible_height;
    SignalBufferWidth = FieldBufferWidth * PPURasterTimings.samples_per_pixel;
    SignalBufferHeight = FieldBufferHeight;

    InitializeSignalLevelLUT(BrightnessDelta, ContrastDelta, ppu_voltages);

    InitializeDecoder(HueDelta, SaturationDelta);

    delete[] RawFieldBuffer;
    delete[] SignalFieldBuffer;
    RawFieldBuffer = new PPUDotType[FieldBufferWidth * FieldBufferHeight];
    SignalFieldBuffer = new uint16_t[SignalBufferWidth * SignalBufferHeight];
    InitializeField();

    PPU2C04LUT = PaletteLUT_2C04[PPU2C04Rev];
}

NES_CVBS::NES_CVBS(int ppu_type, int ppu_2c04_rev, bool ppu_sync_enable, bool ppu_full_frame_input, int ppu_thread_count)
{
    PPUType = ppu_type;
    PPU2C04Rev = ppu_2c04_rev;
    PPUSyncEnable = ppu_sync_enable;
    PPUFullFrameInput = ppu_full_frame_input;
    PPUThreadCount = ppu_thread_count;

    ApplySettings(BrightnessDelta, ContrastDelta, HueDelta, SaturationDelta);
}

NES_CVBS::~NES_CVBS()
{
    delete[] RawFieldBuffer;
    delete[] SignalFieldBuffer;
}

void NES_CVBS::InitializeSignalLevelLUT(double brightness_delta, double contrast_delta, CompositeOutputLevel ppu_voltages)
{
    auto voltage_normalize = [&](double voltage, double black_point, double white_point) {
        // black point and white point potentially could be used for normalizing
        return uint16_t((((voltage - black_point) / (white_point - black_point) * (contrast_delta + 1)) + brightness_delta) * 0xFFFF);
        // for now, convert to integer mV
        //return uint16_t(voltage * 1000);
    };

    for (int emph = 0; emph < 2; emph++) {
        double blank = ppu_voltages.sync[0];
        double white = ppu_voltages.signal[3][1][0];
        for (int color = 0; color < 0x40; color++) {
            int hue = color & 0x0F;
            int luma = (color >> 4) & 0x03;
            double low = ppu_voltages.signal[luma][0][emph];
            double high = ppu_voltages.signal[luma][1][emph];

            if (hue == 0x00) low = high;
            else if (hue == 0x0D) high = low;
            else if (hue >= 0x0E) high = low = ppu_voltages.signal[1][0][0];

            SignalLevelLUT[0][emph][color] = voltage_normalize(low, blank, white);
            SignalLevelLUT[1][emph][color] = voltage_normalize(high, blank, white);
        }

        SignalLevelLUT[0][emph][0x40] = voltage_normalize(ppu_voltages.sync[0], blank, white);
        SignalLevelLUT[1][emph][0x40] = voltage_normalize(ppu_voltages.sync[1], blank, white);
        SignalLevelLUT[0][emph][0x41] = voltage_normalize(ppu_voltages.colorburst[0], blank, white);
        SignalLevelLUT[1][emph][0x41] = voltage_normalize(ppu_voltages.colorburst[1], blank, white);
    }
}

void NES_CVBS::InitializeDecoder(double hue_delta, double saturation_delta)
{
}

void NES_CVBS::InitializeField()
{
    for (uint16_t scanline = 0; scanline < FieldBufferHeight; scanline++) {
        uint16_t pixel = 0;
        uint16_t pixel_threshold = 0;
        uint16_t scanline_threshold = 0;

        // horizontal sync, back porch, and colorburst
        if (PPUSyncEnable) {
            WritePixelsIn(PPURasterTimings.horizontal_sync, RawFieldBuffer, pixel, scanline, pixel_threshold, sync_level);
            WritePixelsIn(PPURasterTimings.back_porch_first, RawFieldBuffer, pixel, scanline, pixel_threshold, blank_level);
            WritePixelsIn(PPURasterTimings.colorburst, RawFieldBuffer, pixel, scanline, pixel_threshold, colorburst);
            WritePixelsIn(PPURasterTimings.back_porch_second, RawFieldBuffer, pixel, scanline, pixel_threshold, blank_level);
        }

        if (ScanlineIsIn(PPURasterTimings.active_scanlines, scanline, scanline_threshold)) {
            WritePixelsIn(PPURasterTimings.gray_pulse, RawFieldBuffer, pixel, scanline, pixel_threshold, blank_level);
            WritePixelsIn(PPURasterTimings.border_left, RawFieldBuffer, pixel, scanline, pixel_threshold, blank_level);
            WritePixelsIn(PPURasterTimings.active_pixels, RawFieldBuffer, pixel, scanline, pixel_threshold, blank_level);
            WritePixelsIn(PPURasterTimings.border_right, RawFieldBuffer, pixel, scanline, pixel_threshold, blank_level);
        }
        else if (ScanlineIsIn(PPURasterTimings.postrender_scanlines, scanline, scanline_threshold)) {
            WritePixelsIn(PPURasterTimings.gray_pulse, RawFieldBuffer, pixel, scanline, pixel_threshold, blank_level);
            WritePixelsIn(PPURasterTimings.border_bottom, RawFieldBuffer, pixel, scanline, pixel_threshold, blank_level);
        }
        else if (PPUSyncEnable && ScanlineIsIn(PPURasterTimings.postrender_blank_scanlines, scanline, scanline_threshold))
            WritePixelsIn(PPURasterTimings.vblank, RawFieldBuffer, pixel, scanline, pixel_threshold, blank_level);
        else if (PPUSyncEnable && ScanlineIsIn(PPURasterTimings.vertical_sync_scanlines, scanline, scanline_threshold)) {
            // it's rewind time! overwrite hsync, back porch and colorburst with vblank pulse
            pixel = 0;
            pixel_threshold = 0;
            WritePixelsIn(PPURasterTimings.blank_pulse, RawFieldBuffer, pixel, scanline, pixel_threshold, sync_level);
            WritePixelsIn(PPURasterTimings.sync_seperator, RawFieldBuffer, pixel, scanline, pixel_threshold, blank_level);
        }
        else if (PPUSyncEnable && ScanlineIsIn(PPURasterTimings.prerender_blanking_scanlines, scanline, scanline_threshold)) {
            WritePixelsIn(PPURasterTimings.vblank, RawFieldBuffer, pixel, scanline, pixel_threshold, blank_level);
        }

        // front porch is the only constant in life
        if (PPUSyncEnable)
            WritePixelsIn(PPURasterTimings.front_porch, RawFieldBuffer, pixel, scanline, pixel_threshold, blank_level);
    }
}

void NES_CVBS::EmplaceField()
{
    uint16_t pixel_offset = 0;
    uint16_t visible_scanline = PPURasterTimings.active_scanlines + PPURasterTimings.postrender_scanlines;
    if (PPUSyncEnable) pixel_offset += PPURasterTimings.horizontal_sync +
        PPURasterTimings.back_porch_first +
        PPURasterTimings.colorburst +
        PPURasterTimings.back_porch_second;
    uint16_t pixel_index = 0, pixel_threshold = 0, scanline_threshold = 0;
    if (PPUFullFrameInput && PPUType == 0) {
        for (uint16_t scanline = 0; scanline < visible_scanline; scanline++) {
            pixel_index = pixel_offset;
            pixel_threshold = pixel_index;
            scanline_threshold = 0;
            if (ScanlineIsIn(PPURasterTimings.active_scanlines, scanline, scanline_threshold)) {
                WritePixelsIn(PPURasterTimings.gray_pulse, RawFieldBuffer, pixel_index, scanline, pixel_threshold, blank_level, &PPURawFrameBuffer);
                WritePixelsIn(PPURasterTimings.border_left, RawFieldBuffer, pixel_index, scanline, pixel_threshold, blank_level, &PPURawFrameBuffer);
                WritePixelsIn(PPURasterTimings.active_pixels, RawFieldBuffer, pixel_index, scanline, pixel_threshold, blank_level, &PPURawFrameBuffer);
                WritePixelsIn(PPURasterTimings.border_right, RawFieldBuffer, pixel_index, scanline, pixel_threshold, blank_level, &PPURawFrameBuffer);
            }
            else {
                WritePixelsIn(PPURasterTimings.gray_pulse, RawFieldBuffer, pixel_index, scanline, pixel_threshold, blank_level, &PPURawFrameBuffer);
                WritePixelsIn(PPURasterTimings.border_bottom, RawFieldBuffer, pixel_index, scanline, pixel_threshold, blank_level, &PPURawFrameBuffer);
            }

        }
    }
    else {
        for (uint16_t scanline = 0; scanline < PPURasterTimings.active_scanlines; scanline++) {
            pixel_index = pixel_offset + PPURasterTimings.gray_pulse + PPURasterTimings.border_left;
            pixel_threshold = pixel_index;
            scanline_threshold = 0;
            WritePixelsIn(PPURasterTimings.active_pixels, RawFieldBuffer, pixel_index, scanline, pixel_threshold, blank_level, &PPURawFrameBuffer);
        }
    }
}

void NES_CVBS::WritePixelsIn(uint16_t length, PPUDotType* raw_field_buffer, uint16_t& pixel_index, uint16_t& scanline_index, uint16_t& pixel_threshold, PPUDotType pixel, uint16_t** ppu_buffer)
{
    if (length <= 0) return;
    pixel_threshold += length;
    if (ppu_buffer != nullptr) {
        while (pixel_index < pixel_threshold) {
            // pointer of a pointer is icky, but i don't know any other way to advance the PPU buffer pointer here
            raw_field_buffer[size_t((scanline_index * FieldBufferWidth) + pixel_index)] = PPUDotType(*(*ppu_buffer)++);
            pixel_index++;
        }
    }
    else {
        while (pixel_index < pixel_threshold) {
            raw_field_buffer[size_t((scanline_index * FieldBufferWidth) + pixel_index)] = pixel;
            pixel_index++;
        }
    }
}

bool NES_CVBS::ScanlineIsIn(uint16_t length, uint16_t& scanline, uint16_t& scanline_threshold)
{
    scanline_threshold += length;
    return scanline < scanline_threshold;
}

void NES_CVBS::EncodeField(int dot_phase, int line_start, int line_end, bool skip_dot)
{
    // TODO: find a more efficient method to determine color wave phase
    static auto in_phase = [&](uint16_t phase, uint8_t hue) {
        return (hue + phase) % 12 < 6;
    };

    static auto in_emphasis_phase = [&](uint16_t phase, uint8_t emphasis) {
        return (emphasis & 0b001) && in_phase(phase, 0xC) ||
            (emphasis & 0b010) && in_phase(phase, 0x4) ||
            (emphasis & 0b100) && in_phase(phase, 0x8);
    };
    // PAL phase swing amount
    static int phase_swing_delta = 3;

    // amount of color generator clocks within a given pixel
    static int phase_pixel_delta = PPURasterTimings.samples_per_pixel;
    static int phase_pixel_delta_negative = 12 - PPURasterTimings.samples_per_pixel;

    // skip a dot on odd rendered frames.
    // we can't really alter the size of the signal buffer, so instead we'll shift the phase by -1 pixel_index
    // we'll skip over this pixel in the decoder
    bool dot_jump = skip_dot && (PPUType == 0);

    // determine polarity of waveform
    bool wave_toggle = false, emphasis_toggle = false;

    // color generator phase
    uint16_t phase = (((dot_phase + line_start) % 3) * 4) % 12;

    // if we haven't skipped a dot yet, shift phase to "previous" dot phase, before dot skipped
    if (dot_jump && line_start == 0)
        phase = (phase + phase_pixel_delta) % 12;

    for (int scanline = line_start; scanline < line_end; scanline++) {
        // on PAL, alternate phase on every other scanline
        bool phase_alternate = ((scanline) & 0b1) && (PPUType >= 1);
        if (phase_alternate) phase = (phase + phase_swing_delta) % 12;

        for (int pixel_index = 0; pixel_index < FieldBufferWidth; pixel_index++) {
            if (pixel_index == 63 && scanline == 0 && dot_jump)
                phase = (phase + phase_pixel_delta_negative) % 12;
            PPUDotType pixel = RawFieldBuffer[size_t((scanline * FieldBufferWidth) + pixel_index)];
            uint8_t color = pixel & 0x3F;
            uint8_t hue = pixel & 0x0F;
            uint8_t emphasis = (pixel >> 6) & 7;

            if (pixel == sync_level || pixel == blank_level) {
                hue = PPURasterTimings.colorburst_phase;
                color = 0x40;
            }
            else if (pixel == colorburst){
                hue = PPURasterTimings.colorburst_phase;
                color = 0x41;
            }

            int signal_index = 0;
            while (signal_index < phase_pixel_delta) {
                wave_toggle = in_phase(phase, hue);
                emphasis_toggle = in_emphasis_phase(phase, emphasis);

                if (pixel == sync_level) wave_toggle = 0;
                else if (pixel == blank_level) wave_toggle = 1;

                SignalFieldBuffer[size_t((scanline * FieldBufferWidth * phase_pixel_delta) +
                    (pixel_index * phase_pixel_delta) +
                    signal_index++)] = SignalLevelLUT[wave_toggle][emphasis_toggle][color];

                phase = (phase + 1) % 12;
            }
        }
        if (phase_alternate) phase = (phase + (12 - phase_swing_delta)) % 12;
    }
}

void NES_CVBS::DecodeField(uint32_t* rgb_buffer, int dot_phase, int line_start, int line_end, bool skip_dot)
{
}
