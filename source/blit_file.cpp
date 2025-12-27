//
// C++ Encoder
//

#include "blit_file.h"

#include <stdio.h>

//------------------------------------------------------------------------------

BLITFile::BLITFile(const std::vector<SDL_Surface*>& pSurfaces)
{
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

//------------------------------------------------------------------------------
//
// Compress / Serialize a new GSLA File
//
void BLITFile::SaveToFile(const char* pFilenamePath)
{
	// We're not going to even try encoding an empty file
	if (m_pC1PixelMaps.size() < 1)
	{
		return;
	}
}

//------------------------------------------------------------------------------

