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
#include <vector>
#include <thread>
#include "PPUVoltages.h"
#include "PPUTimings.h"

enum PPUDotType {
    // first 512 entries are exclusively for the 9-bit PPU pixel format: "eeellcccc".
    // these additional entries are bitshifted to avoid collisions
    sync_level = (1 << 9),
    blank_level = (2 << 9),
    colorburst = (3 << 9)
};

const uint8_t PaletteLUT_2C04[5][64] = {
    {},
    {
        0x35,0x23,0x16,0x22,0x1C,0x09,0x1D,0x15,0x20,0x00,0x27,0x05,0x04,0x28,0x08,0x20,
        0x21,0x3E,0x1F,0x29,0x3C,0x32,0x36,0x12,0x3F,0x2B,0x2E,0x1E,0x3D,0x2D,0x24,0x01,
        0x0E,0x31,0x33,0x2A,0x2C,0x0C,0x1B,0x14,0x2E,0x07,0x34,0x06,0x13,0x02,0x26,0x2E,
        0x2E,0x19,0x10,0x0A,0x39,0x03,0x37,0x17,0x0F,0x11,0x0B,0x0D,0x38,0x25,0x18,0x3A
    },
    {
        0x2E,0x27,0x18,0x39,0x3A,0x25,0x1C,0x31,0x16,0x13,0x38,0x34,0x20,0x23,0x3C,0x0B,
        0x0F,0x21,0x06,0x3D,0x1B,0x29,0x1E,0x22,0x1D,0x24,0x0E,0x2B,0x32,0x08,0x2E,0x03,
        0x04,0x36,0x26,0x33,0x11,0x1F,0x10,0x02,0x14,0x3F,0x00,0x09,0x12,0x2E,0x28,0x20,
        0x3E,0x0D,0x2A,0x17,0x0C,0x01,0x15,0x19,0x2E,0x2C,0x07,0x37,0x35,0x05,0x0A,0x2D
    },
    {
        0x14,0x25,0x3A,0x10,0x0B,0x20,0x31,0x09,0x01,0x2E,0x36,0x08,0x15,0x3D,0x3E,0x3C,
        0x22,0x1C,0x05,0x12,0x19,0x18,0x17,0x1B,0x00,0x03,0x2E,0x02,0x16,0x06,0x34,0x35,
        0x23,0x0F,0x0E,0x37,0x0D,0x27,0x26,0x20,0x29,0x04,0x21,0x24,0x11,0x2D,0x2E,0x1F,
        0x2C,0x1E,0x39,0x33,0x07,0x2A,0x28,0x1D,0x0A,0x2E,0x32,0x38,0x13,0x2B,0x3F,0x0C
    },
    {
        0x18,0x03,0x1C,0x28,0x2E,0x35,0x01,0x17,0x10,0x1F,0x2A,0x0E,0x36,0x37,0x0B,0x39,
        0x25,0x1E,0x12,0x34,0x2E,0x1D,0x06,0x26,0x3E,0x1B,0x22,0x19,0x04,0x2E,0x3A,0x21,
        0x05,0x0A,0x07,0x02,0x13,0x14,0x00,0x15,0x0C,0x3D,0x11,0x0F,0x0D,0x38,0x2D,0x24,
        0x33,0x20,0x08,0x16,0x3F,0x2B,0x20,0x3C,0x2E,0x27,0x23,0x31,0x29,0x32,0x2C,0x09
    }
};

class NES_CVBS
{
private:
    PPUTimings PPURasterTimings = {};

    // Filter settings
    int PPUType = 0;                // 0 = 2C02, 1 = 2C07, 2 = UA6538
    int PPU2C04Rev = 0;             // for generating the LUT to unscramble 2C04 palettes
    bool PPUSyncEnable = false;     // enable sync and colorburst emulation
    bool PPUFullFrameInput = false; // input buffer includes the entire 283x242 "visible portion". only available in NTSC
    int PPUThreadCount = 0;         // enables multithreading when thread count > 1.

    // image settings
    double BrightnessDelta = 0.0;
    double ContrastDelta = 0.0;
    double HueDelta = 0.0;
    double SaturationDelta = 0.0;

    // voltage LUT for any given color, in mV
    // low/high, no emphasis/emphasis, $xy color
    // 0x40 == sync, 0x41 = colorburst
    uint16_t SignalLevelLUT[2][2][66] = {};
    
    // 2C04 unscrambling LUT
    const uint8_t* PPU2C04LUT = nullptr;

    // input PPU frame buffer, can be 256x240 or 283x242
    uint16_t* PPURawFrameBuffer = nullptr;


    void InitializeSignalLevelLUT(double brightness_delta, double contrast_delta, CompositeOutputLevel ppu_voltages);

    void InitializeDecoder(double hue_delta, double saturation_delta);

    // Initializes the raw field buffer
    void InitializeField();

    // places the input PPU buffer into the raw field
    void EmplaceField();

    // helper functions for the two functions above
    void WritePixelsIn(uint16_t length, PPUDotType* raw_field_buffer, uint16_t& pixel_index, uint16_t& scanline_index, uint16_t& pixel_threshold, PPUDotType pixel, uint16_t** ppu_buffer = nullptr);
    bool ScanlineIsIn(uint16_t length, uint16_t& scanline, uint16_t& scanline_threshold);

    void EncodeField(int dot_phase, int line_start, int line_end, bool skip_dot);
    void DecodeField(uint32_t* rgb_buffer, int dot_phase, int line_start, int line_end, bool skip_dot);

public:
    uint16_t FieldBufferWidth = 0;
    uint16_t FieldBufferHeight = 0;
    uint16_t SignalBufferWidth = 0;
    uint16_t SignalBufferHeight = 0;
    // entire raw PPU pixel field is stored here, for encoding later
    PPUDotType* RawFieldBuffer = nullptr;

    // a single composite field is stored here for color decoding
    uint16_t* SignalFieldBuffer = nullptr;
    void FilterFrame(uint16_t* ppu_buffer, uint32_t* rgb_buffer, int dot_phase, bool skip_dot);

    void ApplySettings(double brightness_delta, double contrast_delta, double hue_delta, double saturation_delta);

    NES_CVBS(int ppu_type, int ppu_2c04_rev, bool ppu_sync_enable, bool ppu_full_frame_input, int ppu_thread_count);
    ~NES_CVBS();
};
