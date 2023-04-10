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

// NES composite output voltages, in volt units
struct CompositeOutputLevel{
	double sync[2];				// sync / blank
	double colorburst[2];		// hi / lo
	double signal[4][2][2];		// $0x-$3x, $x0/$xD, no emphasis/emphasis
};

// https://forums.nesdev.org/viewtopic.php?p=159266#p159266
CompositeOutputLevel lidnariq_NESCPU07_2C02G_NES001;
lidnariq_NESCPU07_2C02G_NES001.sync = { 0.048, 0.312 };
lidnariq_NESCPU07_2C02G_NES001.colorburst = { 0.524, 0.148 };
lidnariq_NESCPU07_2C02G_NES001.signal = {
	{
		{ 0.616, 0.500 },
		{ 0.228, 0.192 }
	},
	{
		{ 0.840, 0.676 },
		{ 0.312, 0.256 }
	},
	{
		{ 1.100, 0.896 },
		{ 0.552, 0.448 }
	},
	{
		{ 1.100, 0.896 },
		{ 0.880, 0.712 }
	}
};
