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

// demo program, takes in an indexed .bmp

#include "main.h"

int main(int argc, char* argv[])
{
	NES_CVBS* nes_filter = new NES_CVBS(0, 0, true, false, 8);

	uint16_t* ppu_frame_input = new uint16_t[256 * 240]{};
	for (int pixel = 0; pixel < (256 * 240); pixel++) ppu_frame_input[pixel] = 0x03;
	uint32_t* rgb_frame_output = new uint32_t[256 * 240]{};

	nes_filter->FilterFrame(ppu_frame_input, rgb_frame_output, 0, true);

	std::vector<uint8_t> buffer_stretch;

	for (int i = 0; i < (nes_filter->SignalBufferWidth * nes_filter->SignalBufferHeight); i++) {
		buffer_stretch.push_back(uint8_t((nes_filter->SignalFieldBuffer[i] >> 8) & 0x00FF));
		buffer_stretch.push_back(uint8_t(nes_filter->SignalFieldBuffer[i] & 0x00FF));
	}

	export_png("test.png", buffer_stretch, uint32_t(nes_filter->SignalBufferWidth), nes_filter->SignalBufferHeight);

	// done, claygo
	delete nes_filter;
	delete[] ppu_frame_input;
	delete[] rgb_frame_output;

	return 0;
}
