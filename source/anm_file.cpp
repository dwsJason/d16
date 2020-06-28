//
// C++ Decoder
// For ANM, Deluxe Animation File Format
// 
//  This link should be good, at least until Jason Scott passes away
//  after that the way back machine should have a copy
// 
//  http://www.textfiles.com/programming/FORMATS/animfile.txt
//
#include "anm_file.h"
#include <stdio.h>

// If these structs are the wrong size, there's an issue with type sizes, and
// your compiler
static_assert(sizeof(ANM_Header)==2816, "ANM_Header is supposed to be 2816 bytes");

//------------------------------------------------------------------------------
// Load in a FanFile constructor
//
AnmFile::AnmFile(const char *pFilePath)
	: m_widthPixels(0)
	, m_heightPixels(0)
	, m_numColors( 0 )
{

	m_pal.iNumColors = 0;
	m_pal.pColors = nullptr;

	LoadFromFile(pFilePath);
}
//------------------------------------------------------------------------------

AnmFile::~AnmFile()
{
	if (m_pal.pColors)
	{
		delete[] m_pal.pColors;
		m_pal.pColors = nullptr;
	}
	// Free Up the memory
	for (int idx = 0; idx < m_pPixelMaps.size(); ++idx)
	{
		delete[] m_pPixelMaps[idx];
		m_pPixelMaps[ idx ] = nullptr;
	}
}

//------------------------------------------------------------------------------

void AnmFile::LoadFromFile(const char* pFilePath)
{
	// Free any existing memory
	if (m_pal.pColors)
	{
		delete[] m_pal.pColors;
		m_pal.pColors = nullptr;
	}
	// Free Up the memory
	for (int idx = 0; idx < m_pPixelMaps.size(); ++idx)
	{
		delete[] m_pPixelMaps[idx];
		m_pPixelMaps[ idx ] = nullptr;
	}
	m_pPixelMaps.clear();

	//--------------------------------------------------------------------------

	std::vector<unsigned char> bytes;

	//--------------------------------------------------------------------------
	// Read the file into memory
	FILE* pFile = nullptr;
	errno_t err = fopen_s(&pFile, pFilePath, "rb");

	if (0==err)
	{
		fseek(pFile, 0, SEEK_END);
		size_t length = ftell(pFile);	// get file size
		fseek(pFile, 0, SEEK_SET);

		bytes.resize( length );			// make sure buffer is large enough

		// Read in the file
		fread(&bytes[0], sizeof(unsigned char), bytes.size(), pFile);
		fclose(pFile);
	}

	if (bytes.size())
	{
		size_t file_offset = 0;	// File Cursor

		ANM_Header* pHeader = (ANM_Header*)&bytes[0];

		if (pHeader->IsValid())
		{
			// Looks Valid

			// I don't support palette animation, just just grab
			// the palette from the header


			// Now grab first frame of animation

			// loop through and grab the rest of the frames
		}
	}

}

//------------------------------------------------------------------------------

//------------------------------------------------------------------------------

