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
#include "src/NES-CVBS.h"
#include "src/lodepng/lodepng.h"
#include "SDL.h"
#include <iostream>

void export_png(const char* filename, std::vector<uint8_t>& image, unsigned width, unsigned height) {
	std::vector<unsigned char> png;
	lodepng::State state; //optionally customize this one
	state.info_raw.bitdepth = 16;
	state.info_raw.colortype = LCT_GREY;
	state.info_png.color.bitdepth = 16;
	state.info_png.color.colortype = LCT_GREY;
	state.encoder.auto_convert = 0;
	unsigned error = lodepng::encode(png, image, width, height, state);
	if (!error) lodepng::save_file(png, filename);

	//if there's an error, display it
	if (error) std::cout << "encoder error " << error << ": " << lodepng_error_text(error) << std::endl;
}

int main(int argc, char* argv[])
{
	FilterSettings cvbs_settings;

	NES_CVBS nes_filter(cvbs_settings);

	uint16_t ppu_frame_input[256 * 240] = {};
	uint32_t* rgb_frame_output = nullptr;

	nes_filter.FilterFrame(ppu_frame_input, rgb_frame_output, 0);

	std::vector<uint8_t> buffer_stretch;
	buffer_stretch.resize(size_t(nes_filter.RawFieldBuffer.size() * 2));

	for (int i = 0; i < nes_filter.RawFieldBuffer.size(); i++) {
		buffer_stretch.at(size_t(i * 2)) = uint8_t((nes_filter.RawFieldBuffer.at(i) >> 8) & 0x00FF);
		buffer_stretch.at(size_t((i * 2) + 1)) = uint8_t(nes_filter.RawFieldBuffer.at(i) & 0x00FF);
	}

	export_png("test.png", buffer_stretch, nes_filter.FieldBufferWidth, nes_filter.FieldBufferHeight);

	return 0;
}
