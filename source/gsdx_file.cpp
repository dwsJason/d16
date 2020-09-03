//
// C++ Encoder
// For GSdx, a tool for pre-compiling scroll code on the IIgs
// 
// File Format summary here from Brutal Deluxe
//
//
#include "gsdx_file.h"

#include <assert.h>
#include <map>
#include <stdio.h>
#include <string>

#include "SDL_Surface.h"

#include "bctypes.h"

//------------------------------------------------------------------------------

GSDXFile::GSDXFile(SDL_Surface* pImage)
	: m_pBaseImage( pImage )
{
}

GSDXFile::~GSDXFile()
{
}

//------------------------------------------------------------------------------
//
// Generate Machine Code for a DeltaY / Vertical Scroll, and save to a file
// 
void GSDXFile::GenerateDeltaY( int deltaY, const char* pFilePath )
{
static unsigned char blank_line[320] = {0};

	assert(nullptr != pFilePath);
	assert(deltaY <= 2);
	assert(deltaY >= -2);
	assert(deltaY != 0);

	unsigned char* c1PixelData = GetC1StylePixels( m_pBaseImage );

	std::vector<unsigned char> code;
	std::vector<unsigned char> table;

	code.push_back(0x6B);  // RTL


	if (deltaY > 0)
	{
		// Scrolling from the bottom of the screen / up to the top
		if (1 == deltaY)
		{
			// from blank -> to first line
			// from last line -> to next line
			// from last line -> to blank

			int height = m_pBaseImage->h;

			CompileLine(blank_line, c1PixelData+0, code, table);

			for (int idx = 0; idx < (height-1); ++idx)
			{
				CompileLine(&c1PixelData[ idx * 160 ], &c1PixelData[ (idx+1) * 160 ], code, table);
			}

			CompileLine(c1PixelData + ((height-1)*160), blank_line, code, table);

		}
		else if (2 == deltaY)
		{
			int height = m_pBaseImage->h;

			CompileLine(blank_line, c1PixelData+0, code, table);
			CompileLine(blank_line, c1PixelData+1, code, table);

			for (int idx = 0; idx < (height-2); ++idx)
			{
				CompileLine(&c1PixelData[ idx * 160 ], &c1PixelData[ (idx+2) * 160 ], code, table);
			}

			CompileLine(c1PixelData + ((height-2)*160), blank_line, code, table);
			CompileLine(c1PixelData + ((height-1)*160), blank_line, code, table);
		}
		else
		{
			// unsupported scroll speed
			assert(0);
		}
	}
	else
	{
		// Scrolling from the top of the screen / down to the bottom
		if (-1 == deltaY)
		{
			// from blank -> to first line
			// from last line -> to next line
			// from last line -> to blank

			int height = m_pBaseImage->h;

			CompileLine(c1PixelData+0, blank_line, code, table);

			for (int idx = 0; idx < (height-1); ++idx)
			{
				CompileLine(&c1PixelData[ (idx+1) * 160 ],&c1PixelData[ idx * 160 ],
							code, table);
			}

			CompileLine(blank_line,c1PixelData + ((height-1)*160), code, table);
		}
		else if (-2 == deltaY)
		{
			int height = m_pBaseImage->h;

			CompileLine(c1PixelData+0, blank_line, code, table);
			CompileLine(c1PixelData+1, blank_line, code, table);

			for (int idx = 0; idx < (height-2); ++idx)
			{
				CompileLine(&c1PixelData[ (idx+2) * 160 ], &c1PixelData[ idx * 160 ],
							code, table);
			}

			CompileLine(blank_line, c1PixelData + ((height-2)*160), code, table);
			CompileLine(blank_line, c1PixelData + ((height-1)*160), code, table);
		}
		else
		{
			// unsupported scroll speed
			assert(0);
		}
	}

	// Add an extra entry, so we know how long it is
	table.push_back(code.size() & 0xFF);
	table.push_back((code.size() >> 8) & 0xFF);

	std::string bin = pFilePath;
	bin += ".bin";

	// Save the Code Wad
	FILE *file = fopen(bin.c_str(), "wb");
	fwrite(&code[0], 1, code.size(), file);
	fclose(file);

	// Save the Table Wad
	bin+=".table";
	file = fopen(bin.c_str(), "wb");
	fwrite(&table[0], 1, table.size(), file);
	fclose(file);

}

//------------------------------------------------------------------------------
//
//  Return back an array of GS Nibbled Color Indexes
//
unsigned char* GSDXFile::GetC1StylePixels( SDL_Surface* pImage )
{
	// For now, we only support if it's already color index
	assert(SDL_PIXELFORMAT_INDEX8 == pImage->format->format);
	// For now, we only support images that are 320 pixels wide
	assert(pImage->w = 320);

	if( SDL_MUSTLOCK(pImage) )
		SDL_LockSurface(pImage);


	int width       = (pImage->w);
	int width_bytes = (width + 1) / 2;
	int height      = (pImage->h);

	unsigned char* pResult = new unsigned char[ width_bytes * height ];

	for (int pixel_y = 0; pixel_y < height; ++pixel_y)
	{
		Uint8* pSourcePixel = (Uint8*)pImage->pixels;
		pSourcePixel += (pixel_y * pImage->pitch);

		unsigned char* pDestPixel = pResult + (width_bytes * pixel_y);

		for (int pixel_x = 0; pixel_x < width; pixel_x+=2)
		{
			unsigned char colorIndex0 = (unsigned char)pSourcePixel[ pixel_x + 0 ];
			colorIndex0 <<= 4;

			unsigned char colorIndex1 = (unsigned char)pSourcePixel[ pixel_x + 1 ];
			colorIndex1  &= 0xF;

			// Combine the pixel nibbles, and store
			pDestPixel[ pixel_x >> 1 ] = (colorIndex0 | colorIndex1);
		}
	}

	if( SDL_MUSTLOCK(pImage) )
		SDL_UnlockSurface(pImage);

	return pResult;
}

//------------------------------------------------------------------------------
//
//  Version 1 Simple Diff
//  A line is 160 bytes
//
void GSDXFile::CompileLine(unsigned char* pDest, unsigned char* pSource,
					 std::vector<unsigned char>& code,
					 std::vector<unsigned char>& table )
{
	std::map<u8, std::vector<u8>>  byte_map;
	std::map<u16, std::vector<u8>> short_map;

	// The address is just an offset into the data here, currently limited to 64K
	table.push_back(code.size() & 0xFF);
	table.push_back((code.size() >> 8) & 0xFF);

	for (int idx = 0; idx < 160; ++idx)
	{
		// While no differences, skip!
		if (pDest[idx] == pSource[idx])
			continue;

		if ((idx == 159 ) || (pDest[ idx+1 ] == pSource[ idx+ 1]))
		{
			// 8 Bit Difference
			u8 pixel = pSource[ idx ];
			std::vector<u8>& offsets = byte_map[ pixel ];
			offsets.push_back( (u8)idx );
		}
		else
		{
			// 16 Bit Difference
			u16 pixel = pSource[idx + 1];
			pixel <<= 8;
			pixel |= pSource[idx];
			std::vector<u8>& offsets = short_map[ pixel ];
			offsets.push_back( (u8)idx );
		}
	}

	// Data Collected, not generate the executable code
	for (const std::pair<u16, std::vector<u8>>& keypair : short_map)
	{
		#if 0
        // Accessing KEY from element
        std::string word = element.first;
        // Accessing VALUE from element.
        int count = element.second;
		#endif
		// Output the LDA #$1234
		u16 pixels = keypair.first;
		code.push_back(0xA9);                   // LDA #$XXXX, opcode
		code.push_back((u8)(pixels & 0xFF));	// LSB
		code.push_back((u8)(pixels >> 8));      // MSB

		const std::vector<u8>& addresses = keypair.second;

		if (0 == pixels)
		{
			for (int addressIndex = 0; addressIndex < addresses.size(); ++addressIndex)
			{
				code.push_back(0x64);  // STZ DP opcode
				code.push_back(addresses[ addressIndex ]); // address
			}
		}
		else
		{
			for (int addressIndex = 0; addressIndex < addresses.size(); ++addressIndex)
			{
				code.push_back(0x85); // STA DP opcode
				code.push_back(addresses[ addressIndex ]); // address
			}
		}
	}

	for (const std::pair<u8, std::vector<u8>>& keypair : byte_map)
	{
		#if 0
        // Accessing KEY from element
        std::string word = element.first;
        // Accessing VALUE from element.
        int count = element.second;
		#endif
		// Output the LDX #$12
		u16 pixels = keypair.first;
		code.push_back(0xA2);                   // LDX #$XX, opcode
		code.push_back((u8)(pixels & 0xFF));	// LSB

		const std::vector<u8>& addresses = keypair.second;

		if (0 == pixels)
		{
			for (int addressIndex = 0; addressIndex < addresses.size(); ++addressIndex)
			{
				code.push_back(0x84);  // STY DP opcode
				code.push_back(addresses[ addressIndex ]); // address
			}
		}
		else
		{
			for (int addressIndex = 0; addressIndex < addresses.size(); ++addressIndex)
			{
				code.push_back(0x86); // STX DP opcode
				code.push_back(addresses[ addressIndex ]); // address
			}
		}

	}

	//code.push_back(0x60);  // RTS opcode
	code.push_back(0x7B); // TDC
	code.push_back(0x69); // ADC #$1234
	code.push_back(0xA0); // #160
	code.push_back(0x00);
	code.push_back(0x5B); // TCD

}
//------------------------------------------------------------------------------

