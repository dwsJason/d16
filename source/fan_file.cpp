//
// C++ Encoder/Decoder
// For FAN, Foenix Animation File Format
// A realtime compressed bitmap style animations, used for cutscenes
// and stuff like that
// 
// Potentially, a file saved as single frame, could server as a generic
// compressed bitmap / tilecatalog format 
//
//
//  $$TODO, bring down a text file copy of the spec into the project
// 
//  https://docs.google.com/document/d/1dqJH3cplxpecKXj-rUa170ZjljcrYAxmUSj14xomeOc/edit
//
#include "fan_file.h"

#include "memstream.h"	// my memory based serializer

#include <stdio.h>

FanFile::FanFile(int iWidthPixels, int iHeightPixels, int iNumColors)
	: m_widthPixels( iWidthPixels )
	, m_heightPixels( iHeightPixels )
	, m_numColors( iNumColors )
{

	m_pal.iNumColors = iNumColors;
	m_pal.pColors = new FAN_Color[ iNumColors ];
}

FanFile::~FanFile()
{
	delete[] m_pal.pColors;

	// Free Up the memory
	for (int idx = 0; idx < m_pPixelMaps.size(); ++idx)
	{
		delete[] m_pPixelMaps[idx];
		m_pPixelMaps[ idx ] = nullptr;
	}
}

//------------------------------------------------------------------------------

void FanFile::SetPalette( const FAN_Palette& palette )
{
	// copy in the colors
	for (int idx = 0; idx < palette.iNumColors; ++idx)
	{
		m_pal.pColors[idx] = palette.pColors[idx];
	}

}

//------------------------------------------------------------------------------
//
// Make a Copy of the image data
//
void FanFile::AddImages( const std::vector<unsigned char*>& pPixelMaps )
{
	int numPixels = m_widthPixels * m_heightPixels;

	for (int idx = 0; idx < pPixelMaps.size(); ++idx)
	{
		unsigned char* pPixels = new unsigned char[ numPixels ];
		memcpy(pPixels, pPixelMaps[ idx ], numPixels);
		m_pPixelMaps.push_back( pPixels );
	}
}

//------------------------------------------------------------------------------
//
// Save to File
//
void FanFile::SaveToFile(const char* pFilenamePath)
{
	// Actually, going to serialize to memory, then will save that to file
}

//------------------------------------------------------------------------------

