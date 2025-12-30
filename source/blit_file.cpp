//
// C++ Encoder
//

#include "blit_file.h"

// compiled data class
#include "compiled.h"

#include <stdio.h>

//------------------------------------------------------------------------------

static const char* FixExtension( const std::string& originalName, std::string extension)
{
	// The idea here is so I don't leak memory, this gets resized (NOT THREAD SAFE)
	// the pointer remains useful after we leave the function, at least temporarily
	static std::string result;

	// Step 1, remove the extension from the original
	result = originalName;

	size_t offset = result.find_last_of(".");

	if (offset != result.npos)
	{
		result.resize(offset);
	}

	result = result+extension;

	return result.c_str();
}

//------------------------------------------------------------------------------

static void SaveHunk(const std::vector<u8> hunk, std::string filenamePath)
{
	//-------------------------------------------------------------------------
	// Create the file and write it
	FILE* pFile = nullptr;
	errno_t err = fopen_s(&pFile, filenamePath.c_str(), "wb");

	if (0==err)
	{
		fwrite(&hunk[0], sizeof(u8), hunk.size(), pFile);
		fclose(pFile);
	}

}


//------------------------------------------------------------------------------

BLITFile::BLITFile(const std::vector<SDL_Surface*>& pSurfaces, const std::vector<ImVec4>& targetColors)
{
	bool bShiftPixels = true;
	bool bShiftRows = true;

	m_targetColors = targetColors;

	std::vector<unsigned char*> c1Frames;

	for (int index = 0; index < pSurfaces.size(); ++index)
	{
		c1Frames.push_back(CreateC1Data(pSurfaces[index]));

		if (bShiftPixels && m_widthPixels == 640)
		{
			for (int shifts = 0; shifts < 3; ++shifts)
			{
				// generate pixel shifted frames
				unsigned char *pSrc = c1Frames[ c1Frames.size() - 1 ];
				unsigned char *pImage = new unsigned char[ m_frameSize ];

				memcpy(pImage, pSrc, m_frameSize);

				for (int y = 0; y < 200; ++y)
				{
					u8* pPixels  = pImage  + (160 * y);
					u8* pPixels2 = pPixels + 0x8000;

					u8 neighbor  = 0;
					u8 neighbor2 = 0;

					for (int x = 0; x < 160; ++x)
					{
						u8 pixel = pPixels[x];

						pPixels[x] = (pixel>>2) | neighbor;
						neighbor = pixel<<6;

						if (400 == m_heightPixels)
						{
							pixel = pPixels2[x];

							pPixels2[x] = (pixel>>2) | neighbor2;
							neighbor2 = pixel<<6;
						}
					}
				}

				c1Frames.push_back(pImage);
			}
		}
		else if (bShiftPixels && m_widthPixels == 320)
		{
			// generate pixel shifted frames
			unsigned char *pSrc = c1Frames[ c1Frames.size() - 1 ];
			unsigned char *pImage = new unsigned char[ m_frameSize ];

			memcpy(pImage, pSrc, m_frameSize);

			for (int y = 0; y < 200; ++y)
			{
				u8* pPixels  = pImage  + (160 * y);
				u8* pPixels2 = pPixels + 0x8000;

				u8 neighbor  = 0;
				u8 neighbor2 = 0;

				for (int x = 0; x < 160; ++x)
				{
					u8 pixel = pPixels[x];

					pPixels[x] = (pixel>>4) | neighbor;
					neighbor = pixel<<4;

					if (400 == m_heightPixels)
					{
						pixel = pPixels2[x];

						pPixels2[x] = (pixel>>4) | neighbor2;
						neighbor2 = pixel<<4;
					}
				}
			}

			c1Frames.push_back(pImage);
		}

		if (bShiftRows && m_heightPixels == 400)
		{
			// this is only needed for 400 line mode
			// Default we have 1 image to shift down
			int numShifts = 1;
			// if bShiftPixels && width is 320, we have 2 images to shift down
			if (bShiftPixels && 320 == m_widthPixels)
			{
				numShifts = 2;
			}
			// if bShiftPixels && width is 640, we have 4 images to shift down
			if (bShiftPixels && 640 == m_widthPixels)
			{
				numShifts = 4;
			}

			// generate pixel shifted frames
			for (int shifts = 0; shifts < numShifts; ++shifts)
			{
				unsigned char *pSrc = c1Frames[c1Frames.size() - numShifts];
				unsigned char *pImage = new unsigned char[ m_frameSize ];

				// initial copy
				memcpy(pImage, pSrc, m_frameSize);

				// copying page 0 to page 1, moves it a whole line
				memcpy(pImage+0x8000, pSrc, 200*160);
				
				// first page, first row needs erased
				memset(pImage, 0, 160);

				// copy 199 lines from page 1 to page 0
				memcpy(pImage+160, pSrc+0x8000, 199*160);

				c1Frames.push_back(pImage);
			}
		}
	}

	AddImages(c1Frames);
}

//------------------------------------------------------------------------------

BLITFile::~BLITFile()
{
	// Free Up the memory
	for (int idx = 0; idx < m_pC1PixelMaps.size(); ++idx)
	{
		delete[] m_pC1PixelMaps[idx];
		m_pC1PixelMaps[ idx ] = nullptr;
	}
}

//------------------------------------------------------------------------------
//
// Append a copy of raw image data into the class
//
void BLITFile::AddImages( const std::vector<unsigned char*>& pFrameBytes )
{
	for (int idx = 0; idx < pFrameBytes.size(); ++idx)
	{
		unsigned char* pPixels = new unsigned char[ m_frameSize ];
		memcpy(pPixels, pFrameBytes[ idx ], m_frameSize );
		m_pC1PixelMaps.push_back( pPixels );
	}
}


//-----------------------------------------------------------------------------

static void AddLine(std::vector<u8>& output, char* pLine)
{
	if (pLine)
	{
		for (int idx = 0; true; ++idx)
		{
			if (pLine[idx])
			{
				output.push_back(pLine[idx]);
			}
			else
			{
				break;
			}
		}
	}
}

//-----------------------------------------------------------------------------
//
// basically we're going to print out source code
//
void BLITFile::SaveToFile(const char* pFilenamePath, const std::string& filename)
{
	std::string baseName = FixExtension(filename,"");

	// We're not going to even try encoding an empty file
	if (m_pC1PixelMaps.size() < 1)
	{
		return;
	}

	unsigned char* pBlankC1 = new unsigned char[m_frameSize];

	memcpy(pBlankC1, m_pC1PixelMaps[0], m_frameSize);
	memset(pBlankC1, 0, 0x7D00);
	if (m_frameSize > 0x8000)
	{
		// double frame / interlaced VOC image
		memset(pBlankC1+0x8000, 0, 0x7D00);
	}

	//-------------------------------------------------------------------------
	std::vector<CCompiledData*> m_pCompiledDrawData;
	std::vector<CCompiledData*> m_pCompiledEraseData;

	for (int index = 0; index < m_pC1PixelMaps.size(); ++index)
	{
		CCompiledData* pDrawData = new CCompiledData(m_pC1PixelMaps[index], pBlankC1, m_frameSize);
		m_pCompiledDrawData.push_back(pDrawData);

		CCompiledData* pEraseData = new CCompiledData(pBlankC1, m_pC1PixelMaps[index], m_frameSize);
		m_pCompiledEraseData.push_back(pEraseData);
	}

	//-------------------------------------------------------------------------
	// Collect Compiled Data into output buffer

	char temp[256];
	std::vector<u8> output;

	// Leave space in the first bank for dispatch code
	int bytes = 256;
	int total_bytes = 0;
	int chunk_no = 1;

	// Header Comments
	AddLine(output, "*\n"
					"* Compiled Sprite - Generated by D16\n"
					"*\n"
					"\n");

	// Jump Table
	sprintf(temp, "%s_jumptable\n", baseName.c_str());
	AddLine(output, temp);

	int numFrames = (int) m_pCompiledDrawData.size();
	int lineCount = 0;

	for (int idx = 0; idx < numFrames; ++idx)
	{
		if (0 == lineCount)
		{
			AddLine(output, "\tadrl\t");
			sprintf(temp, "%sdraw%d,%serase%d", baseName.c_str(), idx, baseName.c_str(),idx);
			bytes += 8;
		}
		else
		{
			sprintf(temp, ",%sdraw%d,%serase%d", baseName.c_str(), idx, baseName.c_str(),idx);
			bytes += 8;
		}

		AddLine(output, temp);

		lineCount++;
		if (lineCount >= 4)
		{
			lineCount = 0;
			AddLine(output, "\n");
		}
	}

	AddLine(output, "\n");

	// Draw/Erase functions

	for (int idx = 0; idx < numFrames; ++idx)
	{
		std::vector<u8> hunk;
		int hunk_size = 0;

		sprintf(temp, "\n%sdraw%d ent\n\tmx %%00\n", baseName.c_str(), idx);
		AddLine(hunk, temp);

		hunk_size = m_pCompiledDrawData[idx]->ExportBlit(hunk);

		if (bytes+hunk_size < 65535 )
		{
			output.insert(std::end(output), std::begin(hunk), std::end(hunk));
			bytes+=hunk_size;
		}
		else
		{
			AddLine(output, "\n");

			sprintf(temp,"\n* Hunk Bytes = %d, $%X\n", bytes, bytes);
			AddLine(output, temp);

			SaveHunk(output, pFilenamePath);

			char ext[256];
			sprintf(ext, ".%d", chunk_no);
			// setup next filename
			pFilenamePath = FixExtension(pFilenamePath, ext);

			// reset stuff
			total_bytes += bytes;
			bytes = hunk_size;
			chunk_no++;
			output = hunk;
		}

		hunk.resize(0);

		sprintf(temp, "\n%serase%d ent\n\tmx %%00\n", baseName.c_str(), idx);
		AddLine(hunk, temp);

		hunk_size = m_pCompiledEraseData[idx]->ExportBlit(hunk);

		if (bytes+hunk_size < 65535 )
		{
			output.insert(std::end(output), std::begin(hunk), std::end(hunk));
			bytes+=hunk_size;
		}
		else
		{
			AddLine(output, "\n");

			sprintf(temp,"\n* Hunk Bytes = %d, $%X\n", bytes, bytes);
			AddLine(output, temp);

			SaveHunk(output, pFilenamePath);

			char ext[256];
			sprintf(ext, ".%d", chunk_no);
			// setup next filename
			pFilenamePath = FixExtension(pFilenamePath, ext);

			// reset stuff
			total_bytes += bytes;
			bytes = hunk_size;
			chunk_no++;
			output = hunk;
		}
	}

	AddLine(output, "\n");

	sprintf(temp,"\n* Hunk Bytes  = %d, $%X\n", bytes, bytes);
	AddLine(output, temp);
	total_bytes += bytes;
	sprintf(temp,"* Total Bytes = %d, $%X\n", total_bytes, total_bytes);

	AddLine(output, temp);

//-----------------------------------------------------------------------------

	if (bytes)
	{
		SaveHunk(output, pFilenamePath);
		bytes = 0;
	}
}

//-----------------------------------------------------------------------------

Uint32 BLITFile::SDL_GetPixel(SDL_Surface* pSurface, int x, int y)
{
	Uint32 color = 0;

	if (pSurface)
	{
		// Keep x and y legitimate
		if (x < 0) x = 0;
		if (x >= pSurface->w) x = pSurface->w-1;
		if (y < 0) y = 0;
		if (y >= pSurface->h) y = pSurface->h-1;

		if (pSurface->flags & SDL_PREALLOC)
		{
			// This is only true if I allocated the pixels
			// which means this has to be 8 bit indexed
			Uint8* pPixel = (Uint8*)pSurface->pixels;
			pPixel += (y * pSurface->pitch) + x;

			int index = *pPixel;

			color = *((Uint32*)&pSurface->format->palette->colors[ index ]);

		}
		else
		{
			// Better be 32 bit per pixel

			if( SDL_MUSTLOCK(pSurface) )
				SDL_LockSurface(pSurface);

			int BytesPerPixel = pSurface->format->BytesPerPixel;

			Uint8 * pixel = (Uint8*)pSurface->pixels;
			pixel += (y * pSurface->pitch) + (x * BytesPerPixel);

			//
			//color = *((Uint32*)pixel);

			color  =  (Uint32)pixel[0];
			color |= ((Uint32)pixel[1]) << 8;
			color |= ((Uint32)pixel[2]) << 16;

			if( SDL_MUSTLOCK(pSurface) )
				SDL_UnlockSurface(pSurface);
		}
	}

	color |= 0xFF000000;  // We don't support Alpha
	return color;
}

//-----------------------------------------------------------------------------
// Just return the index, that has the closest match to the passed in color
Uint32 BLITFile::ClosestIndex(Uint32* pClut, Uint32 uColor, Uint32 uNumColors)
{
	int closestIndex = 0;
	long closestDistance;
	Uint32 color = pClut[0];
	int red,green,blue;
	int targetRed, targetGreen, targetBlue;
	int deltaRed, deltaGreen, deltaBlue;

	targetRed   = (uColor >> 0) & 0xFF;
	targetGreen = (uColor >> 8) & 0xFF;
	targetBlue  = (uColor >>16) & 0xFF;

	red   = (color >> 0) & 0xFF;
	green = (color >> 8) & 0xFF;
	blue  = (color >>16) & 0xFF;

	deltaRed   = red - targetRed;
	deltaGreen = green - targetGreen;
	deltaBlue  = blue - targetBlue;

	closestDistance = (deltaRed * deltaRed) + (deltaGreen * deltaGreen) + (deltaBlue * deltaBlue);

	for (unsigned int idx = 1; idx < uNumColors; ++idx)
	{
		color = pClut[ idx ];

		red   = (color >> 0) & 0xFF;
		green = (color >> 8) & 0xFF;
		blue  = (color >>16) & 0xFF;

		deltaRed   = red - targetRed;
		deltaGreen = green - targetGreen;
		deltaBlue  = blue - targetBlue;

		long distance = (deltaRed * deltaRed) + (deltaGreen * deltaGreen) + (deltaBlue * deltaBlue);

		if (distance < closestDistance)
		{
			closestDistance = distance;
			closestIndex = idx;
		}
	}

	return closestIndex;
}
//-----------------------------------------------------------------------------

unsigned char* BLITFile::CreateC1Data(SDL_Surface* pImage)
{
	int c1_size = 0x8000;
	bool b400LineMode = false;
	int LINE_TOTAL = 200;

	if (400 == pImage->h)
	{
		// stacked C1, for 400 line mode
		c1_size = 0x10000;
		b400LineMode = true;
		LINE_TOTAL = 400;
	}

	m_frameSize = c1_size;
	m_widthPixels  = pImage->w;
	m_heightPixels = pImage->h;

	unsigned char* c1data = new unsigned char[c1_size];

// Copy of the C1 memory
	memset(c1data, 0, c1_size );

// Get a copy of the clut
	Uint32 pClut[ 16 ];

	for (int idx = 0; idx < m_targetColors.size(); ++idx)
	{
		const ImVec4& floatColor = m_targetColors[ idx ];

		float red   = floatColor.x * 255.0f;
		float green = floatColor.y * 255.0f;
		float blue  = floatColor.z * 255.0f;

		Uint32 color = 0xFF000000;   					// A = 1.0
		color       |= (((Uint32)blue)&0xFF)  << 16;
		color       |= (((Uint32)green)&0xFF) << 8;
		color       |= (((Uint32)red)&0xFF)   << 0;
		
		pClut[idx] = color;
	}

	if (640 == pImage->w)
	{
		//$$JGA this kind of sucks, but it's what I can do for now
		for (int index = 4; index < m_targetColors.size(); ++index)
		{
			m_targetColors[ index ] = m_targetColors[ index & 3 ];
			pClut[ index ] = pClut[ index & 3 ];
		}

		// 640 mode
		// convert pixel data into 2 bit indices
		for (int y = 0; y < LINE_TOTAL; ++y)
		{
			int offset = 0;
			int use_y = y;

			if (b400LineMode)
			{
				if (y & 0x1)
				{
					offset = 0x8000;
				}
				use_y >>= 1;
			}

			for (int x = 0; x < 640; x+=4)
			{
				Uint32 pixel0 = SDL_GetPixel(pImage, x, y);
				Uint32 index0 = ClosestIndex(pClut+8, pixel0, 4);
				Uint32 pixel1 = SDL_GetPixel(pImage, x+1, y);
				Uint32 index1 = ClosestIndex(pClut+12, pixel1, 4);
				Uint32 pixel2 = SDL_GetPixel(pImage, x+2, y);
				Uint32 index2 = ClosestIndex(pClut+0, pixel2, 4);
				Uint32 pixel3 = SDL_GetPixel(pImage, x+3, y);
				Uint32 index3 = ClosestIndex(pClut+4, pixel3, 4);

				c1data[ offset + (use_y * 160) + (x>>2) ] = (unsigned char) (index3 | (index2<<2) | (index1<<4) | (index0<<6));
			}
		}

		// put the lines into 640 mode
		for (int idx = 0x7D00; idx < 0x7DC8; ++idx)
		{
			c1data[idx] = 0x80;
			if (b400LineMode)
			{
				c1data[idx + 0x8000] = 0x80;
			}
		}

		// Color Data, just doing a floor conversion

		Uint16* pPal = (Uint16*)(&c1data[ 0x7E00 ]);
		for (int idx = 0; idx < 16; ++idx)
		{
			Uint32 sourceColor = pClut[ idx ];
			Uint16 targetColor = (Uint16)(((sourceColor>>4) & 0xF) << 8); // Red

			targetColor |= (Uint16) (((sourceColor>>12) & 0xF) << 4); // Green
			targetColor |= (Uint16) (((sourceColor>>20) & 0xF) << 0); // Blue

			pPal[ idx ] = targetColor;
			if (b400LineMode)
			{
				pPal[ 0x4000 + idx ] = targetColor;
			}
		}

	}
	else
	{
		// 320 mode
		// Nibblized pixel data
		for (int y = 0; y < LINE_TOTAL; ++y)
		{
			int offset = 0;
			int use_y = y;

			if (b400LineMode)
			{
				if (y & 0x1)
				{
					offset = 0x8000;
				}
				use_y >>= 1;
			}

			for (int x = 0; x < 320; x+=2)
			{
				Uint32 pixel0 = SDL_GetPixel(pImage, x, y);
				Uint32 index0 = ClosestIndex(pClut, pixel0);
				Uint32 pixel1 = SDL_GetPixel(pImage, x+1, y);
				Uint32 index1 = ClosestIndex(pClut, pixel1);

				c1data[ offset + (use_y * 160) + (x>>1) ] = (unsigned char) (index1 | (index0<<4));
			}
		}

		// Color Data, just doing a floor conversion

		Uint16* pPal = (Uint16*)(&c1data[ 0x7E00 ]);
		for (int idx = 0; idx < 16; ++idx)
		{
			Uint32 sourceColor = pClut[ idx ];
			Uint16 targetColor = (Uint16)(((sourceColor>>4) & 0xF) << 8); // Red

			targetColor |= (Uint16) (((sourceColor>>12) & 0xF) << 4); // Green
			targetColor |= (Uint16) (((sourceColor>>20) & 0xF) << 0); // Blue

			pPal[ idx ] = targetColor;
			if (b400LineMode)
			{
				pPal[ 0x4000 + idx ] = targetColor;
			}
		}
	}

	return c1data;
}

//-----------------------------------------------------------------------------

